#include "ui/gui_document.h"
#include "ui/gui_images.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>

namespace ae::ui {
bool guiInputKind(GuiKind k) noexcept {return k==GuiKind::Joystick||k==GuiKind::ActionButton||k==GuiKind::LookArea;}
namespace {
bool assetPath(std::string_view p) {
  if(p.empty())return true;
  if(p.size()>1024||p.front()=='/'||p.find_first_of("\\:\0",0,3)!=std::string_view::npos)return false;
  for(usize a=0;a<p.size();) {
    const auto b=p.find('/',a);const auto part=p.substr(a,b==std::string_view::npos?b:b-a);
    if(part.empty()||part=="."||part=="..")return false;
    if(b==std::string_view::npos)break;
    a=b+1;
  }
  return true;
}
UiPoint center(const GuiPlacement &p,const GuiControlState &s){return {p.bounds.x+s.origin.x*p.bounds.width,p.bounds.y+s.origin.y*p.bounds.height};}
}
bool validGuiControl(const GuiControl &c,std::string &error) {
  if(static_cast<u32>(c.mode)>2||static_cast<u32>(c.axis)>2||static_cast<u32>(c.gate)>1||c.action.size()>48){error="Invalid UI input enum/action";return false;}
  for(unsigned char v:c.action)if(v<32||v=='"'||v=='\\'){error="Invalid UI action identifier";return false;}
  for(float v:{c.inputRadius,c.baseRadius,c.knobRadius,c.deadzone,c.outerDeadzone,c.exponent,c.sensitivity,c.returnSeconds})if(!std::isfinite(v)){error="Nonfinite UI input property";return false;}
  if(c.inputRadius<1||c.inputRadius>8192||c.baseRadius<0||c.baseRadius>8192||c.knobRadius<0||c.knobRadius>8192||c.deadzone<0||c.outerDeadzone<0||c.deadzone+c.outerDeadzone>=1||c.exponent<.1f||c.exponent>8||c.sensitivity<=0||c.sensitivity>1000||c.returnSeconds<0||c.returnSeconds>10||!assetPath(c.baseImage)||!assetPath(c.knobImage)){error="Invalid UI input domain/resource";return false;}
  return true;
}
void writeGuiControl(std::ostream &s,const GuiControl &c) {
  s<<std::quoted(c.action)<<' '<<static_cast<u32>(c.mode)<<' '<<static_cast<u32>(c.axis)<<' '<<static_cast<u32>(c.gate)<<' '<<c.inputRadius<<' '<<c.baseRadius<<' '<<c.knobRadius<<' '<<c.deadzone<<' '<<c.outerDeadzone<<' '<<c.exponent<<' '<<c.sensitivity<<' '<<c.returnSeconds<<' '<<c.showBase<<' '<<c.showKnob<<' '<<std::quoted(c.baseImage)<<' '<<std::quoted(c.knobImage);
}
bool readGuiControl(std::istream &s,GuiControl &c) {
  u32 mode,axis,gate,base,knob;
  if(!(s>>std::quoted(c.action)>>mode>>axis>>gate>>c.inputRadius>>c.baseRadius>>c.knobRadius>>c.deadzone>>c.outerDeadzone>>c.exponent>>c.sensitivity>>c.returnSeconds>>base>>knob>>std::quoted(c.baseImage)>>std::quoted(c.knobImage))||mode>2||axis>2||gate>1||base>1||knob>1)return false;
  c.mode=static_cast<GuiStickMode>(mode);c.axis=static_cast<GuiStickAxis>(axis);c.gate=static_cast<GuiStickGate>(gate);c.showBase=base!=0;c.showKnob=knob!=0;return true;
}
const GuiControlState *GuiRuntime::controlState(GuiId id) const {for(const auto &s:controls_)if(s.node==id)return &s;return nullptr;}
u32 GuiRuntime::takeControlPresses(GuiId id){for(auto &s:controls_)if(s.node==id){const auto n=s.presses;s.presses=0;return n;}return 0;}
UiPoint GuiRuntime::takeLookDelta(GuiId id){for(auto &s:controls_)if(s.node==id){const auto v=s.value;s.value={};return v;}return {};}
void GuiRuntime::cancelPointers() noexcept {
  captures_.clear();motionDirty_=true;
  for(auto &s:controls_){++s.cancellation;s.down=false;s.value={};s.knob={};s.presses=0;s.returnTime=0;s.origin={.5f,.5f};}
}
void GuiRuntime::reconcileControls() {
  std::erase_if(controls_,[&](const auto &s){const auto *n=document_.find(s.node);return !n||!guiInputKind(n->kind);});
  for(const auto &n:document_.nodes())if(guiInputKind(n.kind)) {
    auto at=std::find_if(controls_.begin(),controls_.end(),[&](const auto &s){return s.node==n.id;});
    if(at==controls_.end()){GuiControlState s;s.node=n.id;s.config=n.control;controls_.push_back(std::move(s));at=std::prev(controls_.end());}
    const auto *p=placement(n.id);
    if(at->config!=n.control||!p||!p->enabled||p->opacity<=0) {
      std::erase_if(captures_,[&](const auto &c){return c.node==n.id;});
      const auto cancellation=at->cancellation+1;*at=GuiControlState{};at->cancellation=cancellation;at->node=n.id;at->config=n.control;
    }
  }
}
void GuiRuntime::advanceControls(double seconds) {
  for(auto &s:controls_)if(!s.down&&s.returnTime>0) {
    s.returnTime=std::max(0.f,s.returnTime-static_cast<float>(seconds));
    const float t=s.config.returnSeconds>0?s.returnTime/s.config.returnSeconds:0;
    s.knob={s.returnFrom.x*t*t,s.returnFrom.y*t*t};if(s.returnTime==0)s.origin={.5f,.5f};
  }
}
bool GuiRuntime::controlPointer(const UiPointerEvent &e) {
  auto capture=std::find_if(captures_.begin(),captures_.end(),[&](const auto &c){return c.pointer==e.pointerId&&c.device==e.device;});
  if(capture==captures_.end())return true;
  const auto id=capture->node;const auto *n=document_.find(id);const auto *p=placement(id);
  auto state=std::find_if(controls_.begin(),controls_.end(),[&](const auto &s){return s.node==id;});
  if(!n||!p||!p->enabled||state==controls_.end())return true;
  auto &s=*state;const auto &c=n->control;
  if(e.phase==UiPointerPhase::Cancel||e.phase==UiPointerPhase::Up) {
    s.down=false;
    if(n->kind!=GuiKind::LookArea||e.phase==UiPointerPhase::Cancel)s.value={};
    s.returnFrom=s.knob;s.returnTime=c.returnSeconds;
    if(e.phase==UiPointerPhase::Cancel){++s.cancellation;s.presses=0;s.returnTime=0;}
    if(s.returnTime==0){s.knob={};s.origin={.5f,.5f};}
    const bool notify=e.phase==UiPointerPhase::Up&&(n->kind==GuiKind::ActionButton||n->interaction.clickable)&&p->bounds.contains(e.position)&&p->clip.contains(e.position);
    captures_.erase(capture);motionDirty_=true;
    if(notify)click(id);
    return true;
  }
  if(e.phase==UiPointerPhase::Down) {
    s.down=true;s.pointer=e.pointerId;s.device=e.device;s.last=e.position;s.returnTime=0;s.origin={.5f,.5f};
    if(n->kind==GuiKind::Joystick&&c.mode!=GuiStickMode::Fixed)s.origin={(e.position.x-p->bounds.x)/std::max(1.f,p->bounds.width),(e.position.y-p->bounds.y)/std::max(1.f,p->bounds.height)};
    if(n->kind==GuiKind::ActionButton)s.presses=std::min(32u,s.presses+1);
  }
  if(n->kind==GuiKind::ActionButton){s.value={1,0};return true;}
  if(n->kind==GuiKind::LookArea) {
    s.value.x=std::clamp(s.value.x+(e.position.x-s.last.x)/std::max(1.f,p->bounds.width)*c.sensitivity,-1000.f,1000.f);
    s.value.y=std::clamp(s.value.y+(e.position.y-s.last.y)/std::max(1.f,p->bounds.height)*c.sensitivity,-1000.f,1000.f);
    s.last=e.position;return true;
  }
  const float radius=c.inputRadius*p->scale;auto origin=center(*p,s);UiPoint delta{e.position.x-origin.x,e.position.y-origin.y};const float distance=std::hypot(delta.x,delta.y);
  if(c.mode==GuiStickMode::Dynamic&&distance>radius) {
    origin={e.position.x-delta.x/distance*radius,e.position.y-delta.y/distance*radius};
    s.origin={(origin.x-p->bounds.x)/std::max(1.f,p->bounds.width),(origin.y-p->bounds.y)/std::max(1.f,p->bounds.height)};delta={e.position.x-origin.x,e.position.y-origin.y};
  }
  UiPoint raw{delta.x/std::max(1.f,radius),-delta.y/std::max(1.f,radius)};
  if(c.axis==GuiStickAxis::Horizontal)raw.y=0;
  if(c.axis==GuiStickAxis::Vertical)raw.x=0;
  const float length=c.gate==GuiStickGate::Circle?std::hypot(raw.x,raw.y):std::max(std::abs(raw.x),std::abs(raw.y));
  if(length>1){raw.x/=length;raw.y/=length;}
  s.knob={raw.x,-raw.y};
  const float amount=std::min(length,1.f),scaled=std::pow(std::clamp((amount-c.deadzone)/(1-c.deadzone-c.outerDeadzone),0.f,1.f),c.exponent)*c.sensitivity;
  s.value=amount>0?UiPoint{raw.x/amount*scaled,raw.y/amount*scaled}:UiPoint{};return true;
}
void GuiRuntime::drawControl(UiDrawList &list,const GuiNode &n,const GuiPlacement &p) const {
  const auto *state=controlState(n.id);GuiControlState idle;const auto &s=state?*state:idle;const auto &c=n.control;
  if(n.background>>24) {
    UiColor tint=0;for(u32 shift=0;shift<32;shift+=8)tint|=(((n.background>>shift)&255)*((p.tint>>shift)&255)/255)<<shift;
    const float opacity=p.enabled||n.transitions.enabled?p.opacity:p.opacity*.45f;
    tint=(tint&0xFFFFFF)|(static_cast<u32>(std::lround((tint>>24)*opacity))<<24);list.addRect(p.bounds,tint,n.radius*p.scale);
  }
  if(c.mode!=GuiStickMode::Fixed&&!s.down&&s.returnTime==0)return;
  const auto origin=center(p,s);
  auto draw=[&](bool show,float radius,std::string_view path,UiPoint at,UiColor tint) {
    if(!show||radius<=0)return;
    radius*=p.scale;const UiRect r{at.x-radius,at.y-radius,radius*2,radius*2};const float alpha=p.enabled||n.transitions.enabled?p.opacity:p.opacity*.45f;
    UiColor multiplied=0;for(u32 shift=0;shift<32;shift+=8)multiplied|=(((tint>>shift)&255)*((p.tint>>shift)&255)/255)<<shift;tint=multiplied;
    tint=(tint&0xFFFFFF)|(static_cast<u32>(std::lround((tint>>24)*alpha))<<24);
    if(path.empty())list.addRect(r,tint,radius);
    else if(const auto *image=images_?images_->find(path):nullptr;image&&image->error.empty())list.addGuiImage(r,image->texels,tint,radius);
    else {list.addBorder(r,0xFFFF7755,2*p.scale,radius);UiTypeStyle type{};type.size=12*p.scale;list.addText(r,"Imagem ausente",0xFFFFAA88,type);}
  };
  draw(c.showBase,c.baseRadius,c.baseImage,origin,n.foreground);const float travel=std::max(0.f,c.baseRadius-c.knobRadius)*p.scale;
  draw(c.showKnob,c.knobRadius,c.knobImage,{origin.x+s.knob.x*travel,origin.y+s.knob.y*travel},n.accent);
}
}
