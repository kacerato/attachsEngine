#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>

namespace ae::scene {
// Acompanha a posição de outro objeto. A rotação permanece com CameraLook,
// animação ou autoria; este componente possui somente a posição no Play.
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
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>target>>enabled)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
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
  "astra.camera.follow",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<CameraFollow>();},
  cameraFollowNumbers,cameraFollowBooleans,{},nullptr,false,cameraFollowReferences,cameraFollowTriples
};
}
