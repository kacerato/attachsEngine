#pragma once
#include "scene/components.h"
#include <array>
#include <algorithm>
namespace ae::scene {
class CameraLook final : public ComponentValue {
public:

#include "scene/generated/camera_look_CameraLook_fields0.inc"

#include "scene/generated/camera_look_CameraLook_fields1.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<CameraLook>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<enabled;}
  bool read(std::istream &in,u32 version) override {
    if(version<1 || version>2) return false;
    enabled=true;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    if(version>=2 && !(in>>enabled)) return false;
    return valid();
  }
};
#include "scene/generated/camera_look_cameraLookNumbers.inc"
#include "scene/generated/camera_look_cameraLookBooleans.inc"
inline const ComponentType CameraLook::descriptor{
  "astra.camera.look",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<CameraLook>();},cameraLookNumbers,cameraLookBooleans
};
// Delta normalizado pela viewport. O rig pai orienta a câmera no mundo; aqui
// só X/Y locais mudam, preservando o roll autorado.
inline bool applyCameraLookRotation(float rotationDegrees[3],const CameraLook &settings,float x,float y) {
  if(!rotationDegrees||!settings.valid()||!std::isfinite(x)||!std::isfinite(y)) return false;
  for(u32 i=0;i<3;++i) if(!std::isfinite(rotationDegrees[i])) return false;
  if(!settings.enabled) return true;
  const float yaw=std::remainder(rotationDegrees[1]+x*settings.yawSensitivity,360.0f);
  const float pitch=std::clamp(rotationDegrees[0]+y*settings.pitchSensitivity,-settings.pitchLimit,settings.pitchLimit);
  if(!std::isfinite(yaw)||!std::isfinite(pitch)) return false;
  rotationDegrees[0]=pitch;rotationDegrees[1]=yaw;
  return true;
}
}
