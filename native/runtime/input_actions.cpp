#include "runtime/input_actions.h"

#include <algorithm>
#include <cmath>

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

} // namespace

bool operator==(const InputBinding &a, const InputBinding &b) {
  return a.source == b.source && a.code == b.code && a.negativeCode == b.negativeCode &&
         a.axis == b.axis && a.scale == b.scale && a.invert == b.invert;
}
bool operator==(const InputAction &a, const InputAction &b) {
  return a.id == b.id && a.kind == b.kind && a.deadzone == b.deadzone && a.sensitivity == b.sensitivity &&
         a.context == b.context && a.bindings == b.bindings;
}

bool InputBinding::valid() const {
  if (static_cast<u32>(source) > static_cast<u32>(InputSource::GamepadButton)) return false;
  if (axis > 7) return false;
  if (!std::isfinite(scale) || std::abs(scale) > 1000) return false;
  return true;
}

bool InputAction::valid() const {
  if (!validIdentifier(id, false) || !validIdentifier(context, true)) return false;
  if (static_cast<u32>(kind) > static_cast<u32>(ActionKind::Axis2D)) return false;
  if (!std::isfinite(deadzone) || deadzone < 0 || deadzone >= 1) return false;
  if (!std::isfinite(sensitivity) || sensitivity <= 0 || sensitivity > 1000) return false;
  if (bindings.size() > InputActionMap::kMaximumBindings) return false;
  for (const auto &binding : bindings) if (!binding.valid()) return false;
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
  out << ' ' << actions_.size() << ' ' << std::quoted(move_) << ' ' << std::quoted(look_) << ' ' << std::quoted(jump_);
  for (const auto &action : actions_) {
    out << ' ' << std::quoted(action.id) << ' ' << static_cast<u32>(action.kind) << ' ' << action.deadzone
        << ' ' << action.sensitivity << ' ' << std::quoted(action.context) << ' ' << action.bindings.size();
    for (const auto &binding : action.bindings)
      out << ' ' << static_cast<u32>(binding.source) << ' ' << binding.code << ' ' << binding.negativeCode
          << ' ' << binding.axis << ' ' << binding.scale << ' ' << binding.invert;
  }
}

bool InputActionMap::read(std::istream &in) {
  u32 count = 0;
  InputActionMap candidate;
  candidate.actions_.clear();
  if (!(in >> count) || count > kMaximumActions) return false;
  if (!(in >> std::quoted(candidate.move_) >> std::quoted(candidate.look_) >> std::quoted(candidate.jump_))) return false;
  for (u32 index = 0; index < count; ++index) {
    InputAction action;
    u32 kind = 0, bindings = 0;
    if (!(in >> std::quoted(action.id) >> kind >> action.deadzone >> action.sensitivity >>
          std::quoted(action.context) >> bindings) || bindings > kMaximumBindings) return false;
    action.kind = static_cast<ActionKind>(kind);
    for (u32 slot = 0; slot < bindings; ++slot) {
      InputBinding binding;
      u32 source = 0;
      bool invert = false;
      if (!(in >> source >> binding.code >> binding.negativeCode >> binding.axis >> binding.scale >> invert)) return false;
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
  map_ = map;
  values_.assign(map_.actions().size(), Value{});
}

void InputService::setContextEnabled(std::string_view context, bool enabled) {
  if (context.empty()) return;
  const auto found = std::find(disabledContexts_.begin(), disabledContexts_.end(), context);
  if (enabled) { if (found != disabledContexts_.end()) disabledContexts_.erase(found); }
  else if (found == disabledContexts_.end()) disabledContexts_.emplace_back(context);
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
  if (!focus_) submit(InputDeviceState{});
}

void InputService::reset() {
  for (auto &value : values_) value = Value{};
}

void InputService::submit(const InputDeviceState &state) {
  values_.resize(map_.actions().size());
  const InputDeviceState silent{};
  const InputDeviceState &source = focus_ ? state : silent;
  for (usize index = 0; index < map_.actions().size(); ++index) {
    const auto &action = map_.actions()[index];
    auto &value = values_[index];
    value.wasDown = value.down;
    if (!contextEnabled(action.context)) {
      value.x = value.y = 0;
      value.down = false;
      continue;
    }
    value.x = evaluate(action, source, 0);
    value.y = action.kind == ActionKind::Axis2D ? evaluate(action, source, 1) : 0;
    value.down = action.kind == ActionKind::Button ? value.x > .5f
                                                   : std::abs(value.x) + std::abs(value.y) > 0;
  }
}

float InputService::evaluate(const InputAction &action, const InputDeviceState &state, u32 axis) const {
  float accumulated = 0;
  for (const auto &binding : action.bindings) {
    if (binding.axis != axis) continue;
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
      case InputSource::None: continue;
    }
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
  return found && found->down && !found->wasDown;
}
bool InputService::justReleased(std::string_view action) const {
  const auto *found = value(action);
  return found && !found->down && found->wasDown;
}

} // namespace ae::runtime
