#pragma once

#include "runtime/input_actions.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ae::platform::android {

// Android key codes are kept as binding codes. Axis slots are logical controls:
// left X/forward, right X/up, left/right trigger, D-pad X/up.
class AndroidGameInputState final {
public:
  static constexpr u32 mousePointerId(u32 id) noexcept {return 0x80000000u|(id&0x7fffffffu);}
  void key(int device,bool gamepad,u32 code,bool down) {
    int &owner=gamepad?gamepadDevice_:keyboardDevice_;
    auto &pressed=gamepad?buttons_:keys_;
    auto &latched=gamepad?buttonsPressed_:keysPressed_;
    if(owner!=device) {
      if(owner!=-1)canceledSources_|=gamepad?gamepadSources:keySource;
      owner=device;pressed.clear();latched.clear();if(gamepad) axes_.fill(0);
    }
    const auto found=std::find(pressed.begin(),pressed.end(),code);
    if(down && found==pressed.end()) {pressed.push_back(code);if(std::find(latched.begin(),latched.end(),code)==latched.end())latched.push_back(code);}
    else if(!down && found!=pressed.end()) pressed.erase(found);
  }
  void axes(int device,const std::array<float,8> &values) {
    if(gamepadDevice_!=device) {if(gamepadDevice_!=-1)canceledSources_|=gamepadSources;gamepadDevice_=device;buttons_.clear();buttonsPressed_.clear();}
    for(usize i=0;i<values.size();++i)
      axes_[i]=std::isfinite(values[i])?std::clamp(values[i],i==4||i==5?0.0f:-1.0f,1.0f):0.0f;
  }
  void mouse(int device,float x,float y,float width,float height,u32 buttons,float scrollX=0,float scrollY=0,bool motion=true) {
    if(mouseDevice_!=device) {if(mouseDevice_!=-1)canceledSources_|=mouseSources;clearMouse();mouseDevice_=device;}
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) {clearMouse();return;}
    buttons&=31u;
    // Retain a quick click until one snapshot has observed it, even when Android
    // delivers down and up between frames. Several clicks in a frame are one pulse.
    mousePressed_|=buttons&~mouseButtons_;mouseButtons_=buttons;
    if(mousePositionKnown_ && motion) {
      mouseAxes_[0]=std::clamp(mouseAxes_[0]+(x-mouseX_)/width,-1.f,1.f);
      mouseAxes_[1]=std::clamp(mouseAxes_[1]+(y-mouseY_)/height,-1.f,1.f);
    }
    mouseX_=x;mouseY_=y;mousePositionKnown_=true;
    if(std::isfinite(scrollX))mouseAxes_[2]=std::clamp(mouseAxes_[2]+scrollX,-1.f,1.f);
    if(std::isfinite(scrollY))mouseAxes_[3]=std::clamp(mouseAxes_[3]+scrollY,-1.f,1.f);
  }
  void finishFrame() noexcept {mouseAxes_.fill(0);mousePressed_=0;keysPressed_.clear();buttonsPressed_.clear();canceledSources_=0;}
  void clearMouse() noexcept {if(mouseDevice_!=-1)canceledSources_|=mouseSources;mouseDevice_=-1;mouseButtons_=mousePressed_=0;mouseAxes_.fill(0);mousePositionKnown_=false;}
  int mouseDevice() const noexcept {return mouseDevice_;}
  void disconnect(int device) {
    if(mouseDevice_==device)canceledSources_|=mouseSources;
    if(keyboardDevice_==device)canceledSources_|=keySource;
    if(gamepadDevice_==device)canceledSources_|=gamepadSources;
    if(mouseDevice_==device) clearMouse();
    if(keyboardDevice_==device) {keyboardDevice_=-1;keys_.clear();keysPressed_.clear();}
    if(gamepadDevice_==device) {gamepadDevice_=-1;buttons_.clear();buttonsPressed_.clear();axes_.fill(0);}
  }
  void clear() {if(mouseDevice_!=-1)canceledSources_|=mouseSources;if(keyboardDevice_!=-1)canceledSources_|=keySource;if(gamepadDevice_!=-1)canceledSources_|=gamepadSources;clearMouse();keyboardDevice_=gamepadDevice_=-1;keys_.clear();buttons_.clear();keysPressed_.clear();buttonsPressed_.clear();axes_.fill(0);}
  int keyboardDevice() const noexcept {return keyboardDevice_;}
  int gamepadDevice() const noexcept {return gamepadDevice_;}
  bool active() const noexcept {
    return canceledSources_||mouseButtons_||mousePressed_||std::any_of(mouseAxes_.begin(),mouseAxes_.end(),[](float value){return value!=0;})||!keys_.empty()||!buttons_.empty()||!keysPressed_.empty()||!buttonsPressed_.empty()||
           std::any_of(axes_.begin(),axes_.end(),[](float value){return value!=0;});
  }
  runtime::InputDeviceState snapshot() const {
    runtime::InputDeviceState state;
    state.keys=keys_;state.gamepadButtons=buttons_;state.gamepadAxes=axes_;
    for(auto code:keysPressed_)if(std::find(state.keys.begin(),state.keys.end(),code)==state.keys.end())state.keys.push_back(code);
    for(auto code:buttonsPressed_)if(std::find(state.gamepadButtons.begin(),state.gamepadButtons.end(),code)==state.gamepadButtons.end())state.gamepadButtons.push_back(code);
    state.mouseButtons=mouseButtons_|mousePressed_;state.mouseAxes=mouseAxes_;
    state.canceledSources=canceledSources_;
    return state;
  }
private:
  int keyboardDevice_=-1,gamepadDevice_=-1,mouseDevice_=-1;
  u32 mouseButtons_=0,mousePressed_=0;bool mousePositionKnown_=false;
  static constexpr u32 keySource=1u<<static_cast<u32>(runtime::InputSource::Key);
  static constexpr u32 mouseSources=(1u<<static_cast<u32>(runtime::InputSource::MouseButton))|(1u<<static_cast<u32>(runtime::InputSource::MouseAxis));
  static constexpr u32 gamepadSources=(1u<<static_cast<u32>(runtime::InputSource::GamepadButton))|(1u<<static_cast<u32>(runtime::InputSource::GamepadAxis));
  u32 canceledSources_=0;
  float mouseX_=0,mouseY_=0;std::array<float,4> mouseAxes_{};
  std::vector<u32> keys_,buttons_,keysPressed_,buttonsPressed_;
  std::array<float,8> axes_{};
};

} // namespace ae::platform::android
