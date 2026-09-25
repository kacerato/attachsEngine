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
  bool enabled=true;
  float offset[3]{0,2,-5};
  float dampingSeconds=.2f;
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
inline constexpr std::array<ComponentNumber,4> cameraFollowNumbers{{
#define AE_FOLLOW_NUMBER(id,label,member,lo,hi,step,group,unit) \
  {label,lo,hi,step,[](const ComponentValue &v)->const float &{return static_cast<const CameraFollow &>(v).member;}, \
   [](ComponentValue &v)->float *{return &static_cast<CameraFollow &>(v).member;},id,{group,unit}}
  AE_FOLLOW_NUMBER("offset_x","Deslocamento X",offset[0],-10000,10000,.1f,"Posição","m"),
  AE_FOLLOW_NUMBER("offset_y","Deslocamento Y",offset[1],-10000,10000,.1f,"Posição","m"),
  AE_FOLLOW_NUMBER("offset_z","Deslocamento Z",offset[2],-10000,10000,.1f,"Posição","m"),
  AE_FOLLOW_NUMBER("damping_seconds","Amortecimento",dampingSeconds,0,30,.05f,"Resposta","s")
#undef AE_FOLLOW_NUMBER
}};
inline constexpr std::array<ComponentBoolean,1> cameraFollowBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const CameraFollow &>(v).enabled;},
   [](ComponentValue &v,bool on){static_cast<CameraFollow &>(v).enabled=on;},{"Resposta"}}
}};
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
