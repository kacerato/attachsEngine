#pragma once
#include "scene/components.h"
#include <array>
#include <algorithm>
namespace ae::scene {
class CameraLook final : public ComponentValue {
public:
  float yawSensitivity=300,pitchSensitivity=195,pitchLimit=83;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<CameraLook>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';}
  bool read(std::istream &in,u32 version) override {
    if(version!=1) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    return true;
  }
};
inline constexpr std::array<ComponentNumber,3> cameraLookNumbers{{
#define AE_LOOK_NUMBER(id,label,field,lo,hi,group) {label,lo,hi,1,[](const ComponentValue &v)->const float&{return static_cast<const CameraLook&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<CameraLook&>(v).field;},id,{group,"°"}}
  AE_LOOK_NUMBER("yaw_sensitivity","Sensibilidade horizontal graus/tela",yawSensitivity,0,720,"Sensibilidade"),
  AE_LOOK_NUMBER("pitch_sensitivity","Sensibilidade vertical graus/tela",pitchSensitivity,0,720,"Sensibilidade"),
  AE_LOOK_NUMBER("pitch_limit","Limite vertical graus",pitchLimit,1,89,"Limites")
#undef AE_LOOK_NUMBER
}};
inline const ComponentType CameraLook::descriptor{
  "astra.camera.look",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<CameraLook>();},cameraLookNumbers
};
}
