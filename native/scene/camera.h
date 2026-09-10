#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
class Camera final : public ComponentValue {
public:
  bool enabled=true;
  float verticalFov=60,nearPlane=.1f,farPlane=2000,priority=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Camera>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return farPlane>nearPlane && priority==std::floor(priority);
  }
  void write(std::ostream &out) const override {out<<enabled<<' ';for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';}
  bool read(std::istream &in,u32 version) override {
    if(version!=1||!(in>>enabled)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    return true;
  }
};
inline constexpr std::array<ComponentNumber,4> cameraNumbers{{
#define AE_CAMERA_NUMBER(id,label,field,lo,hi,step) {label,lo,hi,step,[](const ComponentValue &v)->const float&{return static_cast<const Camera&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Camera&>(v).field;},id}
  AE_CAMERA_NUMBER("vertical_fov","Campo vertical · graus",verticalFov,1,170,1),
  AE_CAMERA_NUMBER("near_plane","Plano próximo · m",nearPlane,.001f,10000,.01f),
  AE_CAMERA_NUMBER("far_plane","Plano distante · m",farPlane,.01f,1000000,10),
  AE_CAMERA_NUMBER("priority","Prioridade",priority,-10000,10000,1)
#undef AE_CAMERA_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> cameraBooleans{{
  {"enabled","Usar no Play",[](const ComponentValue &v){return static_cast<const Camera&>(v).enabled;},[](ComponentValue &v,bool b){static_cast<Camera&>(v).enabled=b;}}
}};
inline const ComponentType Camera::descriptor{
  "astra.camera",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Camera>();},cameraNumbers,cameraBooleans
};
}
