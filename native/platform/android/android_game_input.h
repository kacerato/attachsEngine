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
  void key(int device,bool gamepad,u32 code,bool down) {
    int &owner=gamepad?gamepadDevice_:keyboardDevice_;
    auto &pressed=gamepad?buttons_:keys_;
    if(owner!=device) {owner=device;pressed.clear();if(gamepad) axes_.fill(0);}
    const auto found=std::find(pressed.begin(),pressed.end(),code);
    if(down && found==pressed.end()) pressed.push_back(code);
    else if(!down && found!=pressed.end()) pressed.erase(found);
  }
  void axes(int device,const std::array<float,8> &values) {
    if(gamepadDevice_!=device) {gamepadDevice_=device;buttons_.clear();}
    for(usize i=0;i<values.size();++i)
      axes_[i]=std::isfinite(values[i])?std::clamp(values[i],i==4||i==5?0.0f:-1.0f,1.0f):0.0f;
  }
  void disconnect(int device) {
    if(keyboardDevice_==device) {keyboardDevice_=-1;keys_.clear();}
    if(gamepadDevice_==device) {gamepadDevice_=-1;buttons_.clear();axes_.fill(0);}
  }
  void clear() {keyboardDevice_=gamepadDevice_=-1;keys_.clear();buttons_.clear();axes_.fill(0);}
  int keyboardDevice() const noexcept {return keyboardDevice_;}
  int gamepadDevice() const noexcept {return gamepadDevice_;}
  bool active() const noexcept {
    return !keys_.empty()||!buttons_.empty()||
           std::any_of(axes_.begin(),axes_.end(),[](float value){return value!=0;});
  }
  runtime::InputDeviceState snapshot() const {
    runtime::InputDeviceState state;
    state.keys=keys_;state.gamepadButtons=buttons_;state.gamepadAxes=axes_;
    return state;
  }
private:
  int keyboardDevice_=-1,gamepadDevice_=-1;
  std::vector<u32> keys_,buttons_;
  std::array<float,8> axes_{};
};

} // namespace ae::platform::android
