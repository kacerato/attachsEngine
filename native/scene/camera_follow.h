#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>

namespace ae::scene {
// Owns position only. Orbit rotates its authored offset by the camera's world
// orientation; CameraLook/animation still own orientation, without feedback
// from the target's facing. Existing v1 archives retain their world offset.
class CameraFollow final : public ComponentValue {
public:
  u64 target=0;

#include "scene/generated/camera_follow_CameraFollow_fields0.inc"

  float offset[3]{0,2,-5};

#include "scene/generated/camera_follow_CameraFollow_fields2.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<CameraFollow>(*this);}
  bool valid() const override {
    if(target>std::numeric_limits<u32>::max()) return false;
    for(const auto &p:descriptor.numbers) {
      const float value=p.read(*this);
      if(!std::isfinite(value)||value<p.minimum||value>p.maximum) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out<<target<<' '<<enabled;
    for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
    out<<' '<<orbit;
  }
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2 || !(in>>target>>enabled)) return false;
    orbit=false;pivotHeight=0;
    for(u32 i=0;i<(version==1?4:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version==2&&!(in>>orbit))return false;
    return valid();
  }
};
#include "scene/generated/camera_follow_cameraFollowNumbers.inc"
#include "scene/generated/camera_follow_cameraFollowBooleans.inc"
inline constexpr std::array<ComponentObjectReference,1> cameraFollowReferences{{
  {"target","Alvo","",ObjectReferenceScope::OtherNonDescendant,"Escolher objeto",
   [](const ComponentValue &v){return static_cast<const CameraFollow &>(v).target;},
   [](ComponentValue &v,u64 id){static_cast<CameraFollow &>(v).target=id;},{"Posição"},
   [](const ComponentValue &v){return static_cast<const CameraFollow &>(v).enabled;}}
}};
inline constexpr std::array<ComponentTriple,1> cameraFollowTriples{{
  {"offset","Deslocamento",{"offset_x","offset_y","offset_z"}}
}};
inline const ComponentType CameraFollow::descriptor{
  "astra.camera.follow",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<CameraFollow>();},
  cameraFollowNumbers,cameraFollowBooleans,{},nullptr,false,cameraFollowReferences,cameraFollowTriples
};
}
