#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class CameraProjection : u32 { Perspective, Orthographic };
class Camera final : public ComponentValue {
public:

#include "scene/generated/camera_Camera_fields0.inc"

#include "scene/generated/camera_Camera_fields1.inc"

  CameraProjection projection=CameraProjection::Perspective;

#include "scene/generated/camera_Camera_fields2.inc"

  u32 environmentMask=~0u;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Camera>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const auto v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    const bool maskValid=environmentMask==~0u || (environmentMask && !(environmentMask&(environmentMask-1)) && environmentMask<=128u);
    return farPlane>nearPlane && priority==std::floor(priority) && static_cast<u32>(projection)<=1 && maskValid;
  }
  void write(std::ostream &out) const override {out<<enabled<<' ';for(const auto &p:descriptor.numbers) out<<p.read(*this)<<' ';out<<static_cast<u32>(projection)<<' '<<environmentMask<<' ';}
  bool read(std::istream &in,u32 version) override {
    if((version<1||version>3)||!(in>>enabled>>verticalFov>>nearPlane>>farPlane>>priority)) return false;
    projection=CameraProjection::Perspective;orthographicHalfHeight=5;environmentMask=~0u;
    if(version==2) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    if(version==3) {u32 mode=0;if(!(in>>orthographicHalfHeight>>mode>>environmentMask)||mode>1) return false;projection=static_cast<CameraProjection>(mode);}
    return true;
  }
};
#include "scene/generated/camera_cameraNumbers.inc"
#include "scene/generated/camera_cameraBooleans.inc"
inline constexpr std::array<ComponentEnumOption,2> cameraProjectionOptions{{{0,"Perspectiva"},{1,"Ortográfica"}}};
inline constexpr std::array<ComponentEnumOption,9> cameraEnvironmentMaskOptions{{
  {~0u,"Todos os ambientes"},{1u,"Ambiente 0"},{2u,"Ambiente 1"},{4u,"Ambiente 2"},{8u,"Ambiente 3"},
  {16u,"Ambiente 4"},{32u,"Ambiente 5"},{64u,"Ambiente 6"},{128u,"Ambiente 7"}
}};
#include "scene/generated/camera_cameraEnums.inc"
inline const ComponentType Camera::descriptor{
  "astra.camera",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Camera>();},cameraNumbers,cameraBooleans,cameraEnums
};
}
