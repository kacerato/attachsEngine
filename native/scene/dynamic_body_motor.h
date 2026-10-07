#pragma once
#include "scene/components.h"
#include "scene/motor_control.h"
#include <array>
namespace ae::scene {
// Forces on the existing rigid body, never a second transform authority.
class DynamicBodyMotor final : public ComponentValue {
public:
  MotorControlPolicy control{};
  bool enabled=true,inheritPlatformVelocity=true,automaticSupport=true;
  float speed=6,acceleration=24,braking=36,airControl=.25f,jumpSpeed=6;
  float probeHeight=1,probeDistance=.15f,supportRadius=.3f,maxSlopeDegrees=50;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<DynamicBodyMotor>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
    return u32(control.source)<=5;
  }
  void write(std::ostream &out) const override {out<<enabled<<' '<<inheritPlatformVelocity<<' '<<automaticSupport;for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);out<<' '<<u32(control.source);}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>3||!(in>>enabled>>inheritPlatformVelocity))return false;
    control={};
    // Existing scenes retain their authored probes. New motors follow the
    // solver geometry, including compound offsets and world scale.
    automaticSupport=false;
    if(version>=2&&!(in>>automaticSupport))return false;
    for(u32 i=0;i<(version==3?descriptor.numbers.size():9);++i)if(!(in>>*descriptor.numbers[i].write(*this)))return false;
    if(version==3){u32 source=0;if(!(in>>source)||source>5)return false;control.source=MotorControlSource(source);}
    return valid();
  }
};
inline bool dynamicMotorManualSupport(const ComponentValue &v){return !static_cast<const DynamicBodyMotor&>(v).automaticSupport;}
#include "scene/generated/dynamic_body_motor_dynamicMotorNumbers.inc"
#include "scene/generated/dynamic_body_motor_dynamicMotorBooleans.inc"
#include "scene/generated/dynamic_body_motor_dynamicMotorEnums.inc"
inline const ComponentType DynamicBodyMotor::descriptor{
  "astra.physics.dynamic_motor",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<DynamicBodyMotor>();},dynamicMotorNumbers,dynamicMotorBooleans,dynamicMotorEnums
};
}
