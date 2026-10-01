#pragma once
#include "scene/components.h"
#include <array>

namespace ae::scene {
// Continuous force, not an impulse and not a transform writer. Local vectors
// follow the solver's current quaternion and never inherit object scale.
class ConstantForce final : public ComponentValue {
public:
  float forceX=0,forceY=0,forceZ=0;
  float relativeForceX=0,relativeForceY=0,relativeForceZ=0;
  float torqueX=0,torqueY=0,torqueZ=0;
  float relativeTorqueX=0,relativeTorqueY=0,relativeTorqueZ=0;
  bool enabled=true;
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
inline constexpr std::array<ComponentNumber,12> constantForceNumbers{{
#define AE_FORCE_NUMBER(id,label,field,group,unit) {label,-10000000,10000000,1.f,[](const ComponentValue &v)->const float&{return static_cast<const ConstantForce&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<ConstantForce&>(v).field;},id,{group,unit,"Aplicado a cada passo físico; não multiplique pelo tempo."}}
  AE_FORCE_NUMBER("force_x","Força X",forceX,"Força mundo","N"),
  AE_FORCE_NUMBER("force_y","Força Y",forceY,"Força mundo","N"),
  AE_FORCE_NUMBER("force_z","Força Z",forceZ,"Força mundo","N"),
  AE_FORCE_NUMBER("relative_force_x","Força local X",relativeForceX,"Força local","N"),
  AE_FORCE_NUMBER("relative_force_y","Força local Y",relativeForceY,"Força local","N"),
  AE_FORCE_NUMBER("relative_force_z","Força local Z",relativeForceZ,"Força local","N"),
  AE_FORCE_NUMBER("torque_x","Torque X",torqueX,"Torque mundo","N·m"),
  AE_FORCE_NUMBER("torque_y","Torque Y",torqueY,"Torque mundo","N·m"),
  AE_FORCE_NUMBER("torque_z","Torque Z",torqueZ,"Torque mundo","N·m"),
  AE_FORCE_NUMBER("relative_torque_x","Torque local X",relativeTorqueX,"Torque local","N·m"),
  AE_FORCE_NUMBER("relative_torque_y","Torque local Y",relativeTorqueY,"Torque local","N·m"),
  AE_FORCE_NUMBER("relative_torque_z","Torque local Z",relativeTorqueZ,"Torque local","N·m")
#undef AE_FORCE_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> constantForceBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const ConstantForce&>(v).enabled;},
   [](ComponentValue &v,bool b){static_cast<ConstantForce&>(v).enabled=b;},
   {"Força mundo","","Requer Corpo físico dinâmico no mesmo objeto; desligado permite configurar sem aplicar."}}
}};
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
