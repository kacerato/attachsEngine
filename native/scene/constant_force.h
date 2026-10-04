#pragma once
#include "scene/components.h"
#include <array>

namespace ae::scene {
// Continuous force, not an impulse and not a transform writer. Local vectors
// follow the solver's current quaternion and never inherit object scale.
class ConstantForce final : public ComponentValue {
public:

#include "scene/generated/constant_force_ConstantForce_fields0.inc"

#include "scene/generated/constant_force_ConstantForce_fields1.inc"

#include "scene/generated/constant_force_ConstantForce_fields2.inc"

#include "scene/generated/constant_force_ConstantForce_fields3.inc"

#include "scene/generated/constant_force_ConstantForce_fields4.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<ConstantForce>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {
      const float v=p.read(*this);
      if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out<<enabled;for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>enabled)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    return valid();
  }
};
#include "scene/generated/constant_force_constantForceNumbers.inc"
#include "scene/generated/constant_force_constantForceBooleans.inc"
inline constexpr std::array<ComponentTriple,4> constantForceTriples{{
  {"force","Força mundo",{"force_x","force_y","force_z"}},
  {"relative_force","Força local",{"relative_force_x","relative_force_y","relative_force_z"}},
  {"torque","Torque mundo",{"torque_x","torque_y","torque_z"}},
  {"relative_torque","Torque local",{"relative_torque_x","relative_torque_y","relative_torque_z"}}
}};
inline const ComponentType ConstantForce::descriptor{
  "astra.physics.constant_force",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ConstantForce>();},
  constantForceNumbers,constantForceBooleans,{},nullptr,false,{},constantForceTriples
};
}
