#pragma once
#include "scene/motor_control.h"
#include <algorithm>
#include <cmath>
namespace ae::runtime {
using MotorControlSource=scene::MotorControlSource;
inline bool validMotorControlInput(MotorControlSource source,float right,float forward,float yaw) {
  const auto n=u32(source);return n>=1&&n<=5&&std::isfinite(right)&&std::isfinite(forward)&&std::isfinite(yaw)&&std::abs(right)<=1&&std::abs(forward)<=1;
}
struct MotorControlSnapshot {
  MotorControlSource source=MotorControlSource::None;
  u32 candidates=0;bool focused=true,measured=false,jump=false;
  float right=0,forward=0,yaw=0,priority=0;
};
// Five fixed channels per motor: no frame allocations or opaque source registry.
// UI/hardware snapshots release neutral input; script/AI zero is an explicit claim.
class MotorControlState {
  struct Intent {float right=0,forward=0,yaw=0;bool active=false,jump=false;};
  std::array<Intent,6> inputs_{};
  MotorControlSnapshot last_{};
public:
  bool submit(MotorControlSource source,float right,float forward,float yaw,bool jump=false) {
    const auto n=u32(source);if(!validMotorControlInput(source,right,forward,yaw))return false;
    auto &i=inputs_[n];i.right=right;i.forward=forward;i.yaw=yaw;i.jump|=jump;
    i.active=n>=4||right!=0||forward!=0||i.jump;return true;
  }
  bool queueJump(MotorControlSource source){const auto n=u32(source);if(n<1||n>5||inputs_[n].jump)return false;inputs_[n].jump=inputs_[n].active=true;return true;}
  bool cancel(MotorControlSource source){const auto n=u32(source);if(n<1||n>5)return false;inputs_[n]={};return true;}
  void clear(){inputs_={};last_={};}
  void beginScriptFrame(){for(u32 n=4;n<=5;++n){auto &i=inputs_[n];i.right=i.forward=i.yaw=0;i.active=i.jump;}}
  MotorControlSnapshot resolve(const scene::MotorControlPolicy &policy,bool focused) {
    last_={};last_.focused=focused;last_.measured=true;
    if(!focused){inputs_={};return last_;}
    u32 selected=0;float priority=-1;
    for(u32 n=1;n<=5;++n)if(inputs_[n].active){last_.candidates|=1u<<n;
      const auto p=policy.priority(MotorControlSource(n));
      // Stable tie break: gamepad > keyboard > UI and AI > script when equal.
      if((policy.source==MotorControlSource::None&&p>=priority)||u32(policy.source)==n){selected=n;priority=p;}
    }
    if(policy.source!=MotorControlSource::None){selected=u32(policy.source);priority=policy.priority(policy.source);}
    last_.source=MotorControlSource(selected);last_.priority=std::max(0.f,priority);
    if(selected){auto &i=inputs_[selected];last_.right=i.right;last_.forward=i.forward;last_.yaw=i.yaw;last_.jump=i.jump;}
    // Losing jump pulses cannot fire later when another source relinquishes ownership.
    for(auto &i:inputs_){i.jump=false;if(i.right==0&&i.forward==0&&!(&i==&inputs_[4]||&i==&inputs_[5]))i.active=false;}
    return last_;
  }
  MotorControlSnapshot snapshot(bool focused) const {auto out=last_;out.focused=focused;if(!focused){out.source=MotorControlSource::None;out.candidates=0;out.right=out.forward=out.yaw=0;out.jump=false;}return out;}
};
}
