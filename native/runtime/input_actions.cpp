#include "runtime/input_actions.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <limits>
#include <locale>
#include <charconv>

namespace ae::runtime {
namespace {

bool validIdentifier(std::string_view value, bool allowEmpty) {
  if (value.empty()) return allowEmpty;
  if (value.size() > 48) return false;
  for (const unsigned char c : value)
    if (c < 32 || c == '"' || c == '\\') return false;
  return true;
}

float applyDeadzone(float value, float deadzone) {
  const float magnitude = std::abs(value);
  if (magnitude <= deadzone) return 0;
  // Rescalonar o que sobra evita o salto que um corte puro produz na borda da
  // zona morta: o eixo parte de zero em vez de pular para o valor da borda.
  const float scaled = (magnitude - deadzone) / std::max(1e-4f, 1 - deadzone);
  return value < 0 ? -scaled : scaled;
}
u32 sourceGroup(InputSource source) {
  switch(source) {
    case InputSource::TouchMove:case InputSource::TouchLook:case InputSource::TouchButton:return InputTouch;
    case InputSource::Key:case InputSource::MouseButton:case InputSource::MouseAxis:return InputKeyboardMouse;
    case InputSource::GamepadAxis:case InputSource::GamepadButton:return InputGamepad;
    default:return 0;
  }
}

} // namespace

bool operator==(const InputBinding &a, const InputBinding &b) {
  return a.source == b.source && a.code == b.code && a.negativeCode == b.negativeCode &&
         a.axis == b.axis && a.scale == b.scale && a.invert == b.invert;
}
bool operator==(const InputAction &a, const InputAction &b) {
  return a.id == b.id && a.kind == b.kind && a.deadzone == b.deadzone && a.sensitivity == b.sensitivity &&
         a.context == b.context && a.bindings == b.bindings && a.enabled==b.enabled &&
         a.interaction==b.interaction && a.duration==b.duration && a.deviceGroups==b.deviceGroups;
}

bool InputBinding::valid() const {
  if (static_cast<u32>(source) > static_cast<u32>(InputSource::MouseAxis)) return false;
  if (axis > 7) return false;
  if(source==InputSource::MouseButton && code>=5) return false;
  if(source==InputSource::MouseAxis && code>=4) return false;
  if (source == InputSource::TouchButton && code >= 32) return false;
  if (!std::isfinite(scale) || std::abs(scale) > 1000) return false;
  return true;
}

bool InputAction::valid() const {
  if (!validIdentifier(id, false) || !validIdentifier(context, true)) return false;
  if (static_cast<u32>(kind) > static_cast<u32>(ActionKind::Axis2D)) return false;
  if(static_cast<u32>(interaction)>static_cast<u32>(InputInteraction::Tap) || (deviceGroups&~InputAllDevices) ||
     !std::isfinite(duration) || duration<.01f || duration>60 ||
     (kind!=ActionKind::Button && interaction!=InputInteraction::Press))return false;
  if (!std::isfinite(deadzone) || deadzone < 0 || deadzone >= 1) return false;
  if (!std::isfinite(sensitivity) || sensitivity <= 0 || sensitivity > 1000) return false;
  if (bindings.size() > InputActionMap::kMaximumBindings) return false;
  for (const auto &binding : bindings)
    if (!binding.valid() || binding.axis > (kind == ActionKind::Axis2D ? 1u : 0u)) return false;
  return true;
}

InputActionMap::InputActionMap() {
  InputAction move;
  move.id = "Mover";
  move.kind = ActionKind::Axis2D;
  move.bindings = {{InputSource::TouchMove, 0, 0, 0, 1, false}, {InputSource::TouchMove, 0, 0, 1, 1, false}};
  InputAction look;
  look.id = "Olhar";
  look.kind = ActionKind::Axis2D;
  look.deadzone = 0;
  look.bindings = {{InputSource::TouchLook, 0, 0, 0, 1, false}, {InputSource::TouchLook, 0, 0, 1, 1, false}};
  InputAction jump;
  jump.id = "Saltar";
  jump.kind = ActionKind::Button;
  jump.deadzone = 0;
  jump.bindings = {{InputSource::TouchButton, 0, 0, 0, 1, false}};
  actions_ = {std::move(move), std::move(look), std::move(jump)};
  move_ = "Mover";
  look_ = "Olhar";
  jump_ = "Saltar";
}

const InputAction *InputActionMap::find(std::string_view id) const {
  for (const auto &action : actions_) if (action.id == id) return &action;
  return nullptr;
}

bool InputActionMap::add(const InputAction &action) {
  if (!action.valid() || actions_.size() >= kMaximumActions || find(action.id)) return false;
  actions_.push_back(action);
  return true;
}

bool InputActionMap::remove(std::string_view id) {
  // Remover a ação que cumpre um papel deixaria o papel apontando para o vazio;
  // o papel é limpo junto, e quem configurar outro depois decide qual será.
  for (auto i = actions_.begin(); i != actions_.end(); ++i) if (i->id == id) {
    if (move_ == id) move_.clear();
    if (look_ == id) look_.clear();
    if (jump_ == id) jump_.clear();
    actions_.erase(i);
    return true;
  }
  return false;
}

bool InputActionMap::rename(std::string_view id, std::string_view renamed) {
  if (!validIdentifier(renamed, false) || find(renamed)) return false;
  for (auto &action : actions_) if (action.id == id) {
    action.id = std::string(renamed);
    // Os papéis acompanham o nome novo: renomear não pode desconectar o
    // personagem do próprio controle.
    if (move_ == id) move_ = action.id;
    if (look_ == id) look_ = action.id;
    if (jump_ == id) jump_ = action.id;
    return true;
  }
  return false;
}

bool InputActionMap::replace(std::string_view id,const InputAction &candidate) {
  if(candidate.id!=id || !candidate.valid()) return false;
  for(auto &action:actions_) if(action.id==id) {action=candidate;return true;}
  return false;
}

bool InputActionMap::assignRole(std::string &role, std::string_view id) {
  if (id.empty()) { role.clear(); return true; }
  if (!find(id)) return false;
  role = std::string(id);
  return true;
}
bool InputActionMap::setMoveAction(std::string_view id) { return assignRole(move_, id); }
bool InputActionMap::setLookAction(std::string_view id) { return assignRole(look_, id); }
bool InputActionMap::setJumpAction(std::string_view id) { return assignRole(jump_, id); }

bool InputActionMap::valid() const {
  if (actions_.size() > kMaximumActions) return false;
  for (usize i = 0; i < actions_.size(); ++i) {
    if (!actions_[i].valid()) return false;
    for (usize j = 0; j < i; ++j) if (actions_[j].id == actions_[i].id) return false;
  }
  for (const auto *role : {&move_, &look_, &jump_})
    if (!role->empty() && !find(*role)) return false;
  return true;
}

bool InputActionMap::isDefault() const {
  const InputActionMap defaults;
  return *this == defaults;
}

void InputActionMap::write(std::ostream &out) const {
  const auto precision=out.precision();out.precision(std::numeric_limits<float>::max_digits10);
  out << " ASTRA_ACTION_MAP 2 " << actions_.size() << ' ' << std::quoted(move_) << ' ' << std::quoted(look_) << ' ' << std::quoted(jump_);
  for (const auto &action : actions_) {
    out << ' ' << std::quoted(action.id) << ' ' << static_cast<u32>(action.kind) << ' ' << action.deadzone
        << ' ' << action.sensitivity << ' ' << std::quoted(action.context) << ' ' << action.bindings.size()
        << ' ' << action.enabled << ' ' << static_cast<u32>(action.interaction) << ' ' << action.duration << ' ' << action.deviceGroups;
    for (const auto &binding : action.bindings)
      out << ' ' << static_cast<u32>(binding.source) << ' ' << binding.code << ' ' << binding.negativeCode
          << ' ' << binding.axis << ' ' << binding.scale << ' ' << binding.invert;
  }
  out.precision(precision);
}

bool InputActionMap::read(std::istream &in,u32 maximumSource,bool allowVersioned) {
  u32 count = 0;
  InputActionMap candidate;
  candidate.actions_.clear();
  std::string token;u32 version=1;
  if(!(in>>token))return false;
  if(token=="ASTRA_ACTION_MAP") {
    if(!allowVersioned || !(in>>version>>count) || version!=2)return false;
  } else {
    const auto parsed=std::from_chars(token.data(),token.data()+token.size(),count);
    if(parsed.ec!=std::errc{} || parsed.ptr!=token.data()+token.size())return false;
  }
  if(count>kMaximumActions)return false;
  if (!(in >> std::quoted(candidate.move_) >> std::quoted(candidate.look_) >> std::quoted(candidate.jump_))) return false;
  for (u32 index = 0; index < count; ++index) {
    InputAction action;
    u32 kind = 0, bindings = 0;
    if (!(in >> std::quoted(action.id) >> kind >> action.deadzone >> action.sensitivity >>
          std::quoted(action.context) >> bindings) || bindings > kMaximumBindings) return false;
    action.kind = static_cast<ActionKind>(kind);
    if(version==2) {
      u32 enabled=0,interaction=0;
      if(!(in>>enabled>>interaction>>action.duration>>action.deviceGroups) || enabled>1)return false;
      action.enabled=enabled!=0;action.interaction=static_cast<InputInteraction>(interaction);
    }
    for (u32 slot = 0; slot < bindings; ++slot) {
      InputBinding binding;
      u32 source = 0;
      bool invert = false;
      if (!(in >> source >> binding.code >> binding.negativeCode >> binding.axis >> binding.scale >> invert)) return false;
      if(source>maximumSource) return false;
      binding.source = static_cast<InputSource>(source);
      binding.invert = invert;
      action.bindings.push_back(binding);
    }
    if (!action.valid()) return false;
    candidate.actions_.push_back(std::move(action));
  }
  if (!candidate.valid()) return false;
  *this = std::move(candidate);
  return true;
}

// ---------------------------------------------------------------- serviço

void InputService::setMap(const InputActionMap &map) {
  if (!map.valid()) return;
  cancelBindingCapture();captureStatus_=InputCaptureStatus::Idle;captureReleaseGate_=false;
  map_ = authoredMap_ = map;disabledContexts_.clear();
  values_.assign(map_.actions().size(), Value{});
  enabledOverrides_.assign(map_.actions().size(),-1);deviceGroups_=InputAllDevices;
}

bool InputService::binding(std::string_view action,u32 index,InputBinding &out,bool authored) const {
  const auto *value=(authored?authoredMap_:map_).find(action);
  if(!value || index>=value->bindings.size())return false;
  out=value->bindings[index];return true;
}
bool InputService::overrideBinding(std::string_view action,u32 index,const InputBinding &bindingValue) {
  if(captureStatus_==InputCaptureStatus::Waiting)return false;
  const auto *value=map_.find(action);
  if(!value || index>=value->bindings.size() || !bindingValue.valid())return false;
  auto candidate=*value;candidate.bindings[index]=bindingValue;
  if(!map_.replace(action,candidate))return false;
  reset();return true;
}
bool InputService::removeOverride(std::string_view action,u32 index) {
  InputBinding authored;return binding(action,index,authored,true)&&overrideBinding(action,index,authored);
}
void InputService::removeAllOverrides() {cancelBindingCapture();captureReleaseGate_=false;map_=authoredMap_;reset();}
std::string InputService::exportProfile() const {
  std::ostringstream out;out.imbue(std::locale::classic());out.precision(std::numeric_limits<float>::max_digits10);
  out<<"ASTRA_INPUT_PROFILE 2 ";authoredMap_.write(out);map_.write(out);return out.str();
}
bool InputService::importProfile(std::string_view profile) {
  if(captureStatus_==InputCaptureStatus::Waiting||profile.empty()||profile.size()>262144)return false;
  std::istringstream in{std::string(profile)};in.imbue(std::locale::classic());std::string header;u32 version=0;
  InputActionMap original,candidate;
  if(!(in>>header>>version)||header!="ASTRA_INPUT_PROFILE"||(version!=1&&version!=2)||
     !original.read(in,static_cast<u32>(InputSource::MouseAxis),version==2)||
     !candidate.read(in,static_cast<u32>(InputSource::MouseAxis),version==2))return false;
  in>>std::ws;if(!in.eof()||!(original==authoredMap_))return false;
  // Only binding values may differ. Action identity, ordering, contexts, roles,
  // cardinality and processing settings remain the authored contract.
  auto reconstructed=authoredMap_;
  for(const auto &action:candidate.actions()) {
    const auto *base=authoredMap_.find(action.id);
    if(!base || base->bindings.size()!=action.bindings.size())return false;
    auto expected=*base;expected.bindings=action.bindings;
    if(!(expected==action)||!reconstructed.replace(action.id,expected))return false;
  }
  if(!(reconstructed==candidate))return false;
  map_=std::move(candidate);reset();return true;
}

void InputService::setContextEnabled(std::string_view context, bool enabled) {
  if (context.empty()) return;
  const auto found = std::find(disabledContexts_.begin(), disabledContexts_.end(), context);
  if (enabled) { if (found != disabledContexts_.end()) disabledContexts_.erase(found); }
  else if (found == disabledContexts_.end()) disabledContexts_.emplace_back(context);
  if(!enabled)for(usize i=0;i<map_.actions().size()&&i<values_.size();++i)
    if(map_.actions()[i].context==context){const bool down=values_[i].down;values_[i]={};values_[i].wasDown=down;values_[i].phase=InputPhase::Disabled;}
}

bool InputService::contextEnabled(std::string_view context) const {
  if (context.empty()) return true;
  return std::find(disabledContexts_.begin(), disabledContexts_.end(), context) == disabledContexts_.end();
}

void InputService::setGameplayFocus(bool focused) {
  if (focus_ == focused) return;
  focus_ = focused;
  // Perder o foco solta os botões imediatamente: um botão que continuasse
  // "pressionado" porque o dedo saiu para a interface ficaria preso até o
  // próximo toque no mesmo lugar.
  if (!focus_) {cancelBindingCapture();captureReleaseGate_=false;submit(InputDeviceState{});}
}

void InputService::reset() {
  for (auto &value : values_) value = Value{};
}

bool InputService::beginBindingCapture(std::string_view action,u32 index,InputSource source,bool negative,u32 cancelKey) {
  const auto *value=map_.find(action);
  if(captureStatus_==InputCaptureStatus::Waiting||!focus_||!value||index>=value->bindings.size())return false;
  if(source!=InputSource::Key&&source!=InputSource::GamepadButton&&source!=InputSource::GamepadAxis&&source!=InputSource::MouseButton)return false;
  if(negative&&(source!=InputSource::Key||value->kind==ActionKind::Button))return false;
  if(negative&&value->bindings[index].source!=InputSource::Key)return false;
  captureAction_=action;captureIndex_=index;captureSource_=source;captureNegative_=negative;captureCancelKey_=cancelKey;
  captureSeed_=false;captureReleaseGate_=false;capturePrevious_={};captureAxisNeutral_.fill(false);captureStatus_=InputCaptureStatus::Waiting;reset();return true;
}
void InputService::cancelBindingCapture() noexcept {
  if(captureStatus_==InputCaptureStatus::Waiting)captureStatus_=InputCaptureStatus::Cancelled;
}
void InputService::capture(const InputDeviceState &state) {
  if(captureStatus_!=InputCaptureStatus::Waiting)return;
  if(!captureSeed_){capturePrevious_=state;captureSeed_=true;return;}
  const auto contains=[](const std::vector<u32> &keys,u32 code){return std::find(keys.begin(),keys.end(),code)!=keys.end();};
  if(captureCancelKey_&&contains(state.keys,captureCancelKey_)&&!contains(capturePrevious_.keys,captureCancelKey_)) {
    captureNegative_=false;captureAccepted_={InputSource::Key,captureCancelKey_};captureReleaseGate_=true;
    cancelBindingCapture();return;
  }
  const auto *action=map_.find(captureAction_);
  if(!action||captureIndex_>=action->bindings.size()){cancelBindingCapture();return;}
  InputBinding candidate=action->bindings[captureIndex_];bool found=false;
  if(captureSource_==InputSource::Key||captureSource_==InputSource::GamepadButton) {
    const auto &keys=captureSource_==InputSource::Key?state.keys:state.gamepadButtons;
    const auto &previous=captureSource_==InputSource::Key?capturePrevious_.keys:capturePrevious_.gamepadButtons;
    for(auto code:keys)if(!contains(previous,code)) {
      if(captureNegative_){if(code==candidate.code)continue;candidate.negativeCode=code;}
      else {candidate.code=code;if(candidate.negativeCode==code)candidate.negativeCode=0;}
      found=true;break;
    }
  } else if(captureSource_==InputSource::MouseButton) {
    const auto pressed=state.mouseButtons&~capturePrevious_.mouseButtons;
    for(u32 i=0;i<5;++i)if(pressed&(1u<<i)){candidate.code=i;found=true;break;}
  } else for(u32 i=0;i<state.gamepadAxes.size();++i) {
    const float value=state.gamepadAxes[i];if(!std::isfinite(value))continue;
    if(std::abs(value)<.2f)captureAxisNeutral_[i]=true;
    if(captureAxisNeutral_[i]&&std::abs(value)>.65f){candidate.code=i;candidate.invert=value<0;found=true;break;}
  }
  capturePrevious_=state;
  if(!found)return;
  candidate.source=captureSource_;if(!captureNegative_&&captureSource_!=InputSource::Key)candidate.negativeCode=0;
  auto changed=*action;changed.bindings[captureIndex_]=candidate;
  if(!candidate.valid()||!map_.replace(captureAction_,changed))return;
  captureAccepted_=candidate;captureReleaseGate_=true;captureStatus_=InputCaptureStatus::Completed;reset();
}

bool InputService::setActionEnabled(std::string_view name,bool enabled) {
  for(usize i=0;i<map_.actions().size();++i)if(map_.actions()[i].id==name) {
    enabledOverrides_.resize(map_.actions().size(),-1);
    if(actionEnabled(name)!=enabled && i<values_.size()) {
      const bool down=values_[i].down;values_[i]={};values_[i].wasDown=down;
      values_[i].phase=enabled?InputPhase::Waiting:InputPhase::Disabled;
    }
    enabledOverrides_[i]=enabled?1:0;return true;
  }
  return false;
}
bool InputService::restoreActionEnabled(std::string_view name) {
  const auto *action=map_.find(name);if(!action)return false;
  if(!setActionEnabled(name,action->enabled))return false;
  for(usize i=0;i<map_.actions().size();++i)if(map_.actions()[i].id==name){enabledOverrides_[i]=-1;return true;}
  return false;
}
bool InputService::actionEnabled(std::string_view name) const {
  for(usize i=0;i<map_.actions().size();++i)if(map_.actions()[i].id==name)
    return i<enabledOverrides_.size()&&enabledOverrides_[i]>=0?enabledOverrides_[i]!=0:map_.actions()[i].enabled;
  return false;
}
bool InputService::setDeviceGroups(u32 groups) {
  if(groups&~InputAllDevices)return false;
  if(groups!=deviceGroups_){deviceGroups_=groups;reset();}return true;
}
InputPhase InputService::phase(std::string_view name) const {const auto*a=map_.find(name);const auto*v=value(name);return a&&v&&actionEnabled(name)&&contextEnabled(a->context)&&(a->deviceGroups&deviceGroups_)?v->phase:InputPhase::Disabled;}
float InputService::progress(std::string_view name) const {
  const auto*a=map_.find(name);const auto*v=value(name);if(!a||!v)return 0;
  if(v->phase==InputPhase::Performed)return 1;
  return v->phase==InputPhase::Started?static_cast<float>(std::min(v->elapsed/a->duration,1.0)):0;
}
float InputService::elapsed(std::string_view name) const {const auto*v=value(name);return v?static_cast<float>(v->elapsed):0;}

void InputService::submit(const InputDeviceState &state,double unscaledElapsed) {
  // A malformed clock aborts pending interactions. Never turn NaN / negative
  // time into a hidden successful timeout.
  if(!std::isfinite(unscaledElapsed)||unscaledElapsed<0){reset();return;}
  const bool capturing=captureStatus_==InputCaptureStatus::Waiting||captureReleaseGate_;
  if(captureReleaseGate_) {
    const auto code=captureNegative_?captureAccepted_.negativeCode:captureAccepted_.code;
    const auto has=[&](const std::vector<u32> &keys){return std::find(keys.begin(),keys.end(),code)!=keys.end();};
    const bool held=captureAccepted_.source==InputSource::Key?has(state.keys):
      captureAccepted_.source==InputSource::GamepadButton?has(state.gamepadButtons):
      captureAccepted_.source==InputSource::MouseButton?(state.mouseButtons&(1u<<code))!=0:
      std::abs(state.gamepadAxes[code])>.2f;
    if(!held)captureReleaseGate_=false;
  }
  if(capturing)capture(state);
  values_.resize(map_.actions().size());
  for (usize index = 0; index < map_.actions().size(); ++index) {
    const auto &action = map_.actions()[index];
    auto &value = values_[index];
    const bool enabled=index<enabledOverrides_.size()&&enabledOverrides_[index]>=0?enabledOverrides_[index]!=0:action.enabled;
    const bool deviceCanceled=std::any_of(action.bindings.begin(),action.bindings.end(),[&](const auto&binding){
      const auto group=sourceGroup(binding.source);
      return (group&action.deviceGroups&deviceGroups_) &&
          ((group&state.canceledDeviceGroups) || (state.canceledSources&(1u<<static_cast<u32>(binding.source))));
    });
    value.wasDown = value.down;
    value.performedPulse=false;
    if (!focus_||capturing||!enabled||!contextEnabled(action.context)||!(action.deviceGroups&deviceGroups_) ||
        deviceCanceled) {
      const bool previous=value.wasDown,pending=value.physicalDown;
      value={};value.wasDown=previous;
      value.phase=!enabled||!(action.deviceGroups&deviceGroups_)?InputPhase::Disabled:
          pending||previous?InputPhase::Canceled:InputPhase::Waiting;
      continue;
    }
    value.x = evaluate(action, state, 0);
    value.y = action.kind == ActionKind::Axis2D ? evaluate(action, state, 1) : 0;
    const bool physical=action.kind==ActionKind::Button?value.x>.5f:std::abs(value.x)+std::abs(value.y)>0;
    const bool started=physical&&!value.physicalDown,released=!physical&&value.physicalDown;
    if(started){value.elapsed=0;value.phase=InputPhase::Started;}
    else if(value.physicalDown)value.elapsed=std::min(value.elapsed+unscaledElapsed,60.0);
    if(action.interaction==InputInteraction::Press) {
      value.down=physical;value.phase=physical?InputPhase::Performed:InputPhase::Waiting;
      value.performedPulse=physical&&!value.wasDown;
      if(!physical)value.elapsed=0;
    } else if(action.interaction==InputInteraction::Hold) {
      value.down=physical&&value.elapsed>=action.duration;
      if(value.down){value.phase=InputPhase::Performed;value.performedPulse=!value.wasDown;}
      else if(released)value.phase=InputPhase::Canceled;
      else if(!physical)value.phase=InputPhase::Waiting;
    } else {
      value.down=false;
      if(value.phase!=InputPhase::Canceled && value.physicalDown && value.elapsed>action.duration)value.phase=InputPhase::Canceled;
      if(released && value.phase==InputPhase::Started && value.elapsed<=action.duration) {
        value.down=true;value.performedPulse=true;value.phase=InputPhase::Performed;
      } else if(!physical&&!released)value.phase=InputPhase::Waiting;
    }
    value.physicalDown=physical;
    if(action.kind==ActionKind::Button){value.x=value.down?1.f:0.f;value.y=0;}
  }
}

float InputService::evaluate(const InputAction &action, const InputDeviceState &state, u32 axis) const {
  float accumulated = 0;
  for (const auto &binding : action.bindings) {
    if (binding.axis != axis || !(sourceGroup(binding.source)&action.deviceGroups&deviceGroups_)) continue;
    float raw = 0;
    switch (binding.source) {
      case InputSource::TouchMove: raw = axis == 0 ? state.moveX : state.moveY; break;
      case InputSource::TouchLook: raw = axis == 0 ? state.lookX : state.lookY; break;
      case InputSource::TouchButton: raw = (state.touchButtons & (1u << binding.code)) ? 1.0f : 0.0f; break;
      case InputSource::Key: {
        const auto down = [&](u32 code) {
          return code != 0 && std::find(state.keys.begin(), state.keys.end(), code) != state.keys.end();
        };
        raw = (down(binding.code) ? 1.0f : 0.0f) - (down(binding.negativeCode) ? 1.0f : 0.0f);
        break;
      }
      case InputSource::GamepadAxis:
        raw = binding.code < state.gamepadAxes.size() ? state.gamepadAxes[binding.code] : 0.0f;
        break;
      case InputSource::GamepadButton: {
        const auto &buttons = state.gamepadButtons;
        raw = std::find(buttons.begin(), buttons.end(), binding.code) != buttons.end() ? 1.0f : 0.0f;
        break;
      }
      case InputSource::MouseButton: raw=(state.mouseButtons&(1u<<binding.code))?1.f:0.f;break;
      case InputSource::MouseAxis: raw=state.mouseAxes[binding.code];break;
      case InputSource::None: continue;
    }
    if(!std::isfinite(raw))continue;
    if (binding.invert) raw = -raw;
    accumulated += raw * binding.scale;
  }
  accumulated = std::clamp(accumulated, -1.0f, 1.0f);
  if (action.kind != ActionKind::Button) accumulated = applyDeadzone(accumulated, action.deadzone);
  return std::clamp(accumulated * action.sensitivity, -1000.0f, 1000.0f);
}

const InputService::Value *InputService::value(std::string_view action) const {
  const auto &actions = map_.actions();
  for (usize index = 0; index < actions.size() && index < values_.size(); ++index)
    if (actions[index].id == action) return &values_[index];
  return nullptr;
}

float InputService::axis(std::string_view action) const {
  const auto *found = value(action);
  return found ? found->x : 0.0f;
}

void InputService::axis2(std::string_view action, float out[2]) const {
  const auto *found = value(action);
  out[0] = found ? found->x : 0.0f;
  out[1] = found ? found->y : 0.0f;
}

bool InputService::pressed(std::string_view action) const {
  const auto *found = value(action);
  return found && found->down;
}
bool InputService::justPressed(std::string_view action) const {
  const auto *found = value(action);
  return found && found->performedPulse;
}
bool InputService::justReleased(std::string_view action) const {
  const auto *found = value(action);
  return found && !found->down && found->wasDown;
}

} // namespace ae::runtime
