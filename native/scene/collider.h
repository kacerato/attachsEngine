#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class ColliderShape : u32 { Box=0, Sphere=1, Capsule=2 };
// Independent of visual geometry and body motion. Capsule local Y halfHeight
// excludes the hemispherical ends; dimensions are in local scene units.
class Collider final : public ComponentValue {
public:
  ColliderShape shape=ColliderShape::Box;
  float halfX=.5f,halfY=.5f,halfZ=.5f,radius=.5f,halfHeight=.5f;
  float centerX=0,centerY=0,centerZ=0;
  float rotationX=0,rotationY=0,rotationZ=0;
  u64 owner=0; // zero explicitly means the body on this object
  bool enabled=true;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider>(*this);}
  bool valid() const override {
    if(static_cast<u32>(shape)>2 || owner>std::numeric_limits<u32>::max()) return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);out<<' '<<owner<<' '<<enabled;}
  bool read(std::istream &in,u32 version) override {
    u32 kind=0;if((version<1||version>3) || !(in>>kind) || kind>2) return false;
    shape=static_cast<ColliderShape>(kind);
    centerX=centerY=centerZ=rotationX=rotationY=rotationZ=0;owner=0;enabled=true;
    for(usize i=0;i<(version==1?5:version==2?8:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=3 && !(in>>owner>>enabled)) return false;
    return true;
  }
};
inline constexpr std::array<ComponentNumber,11> colliderNumbers{{
#define AE_COLLIDER_NUMBER(id,label,field) {label,.01f,10000,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Collider&>(v).field;},id}
  AE_COLLIDER_NUMBER("half_x","Meia extensão X",halfX),
  AE_COLLIDER_NUMBER("half_y","Meia extensão Y",halfY),
  AE_COLLIDER_NUMBER("half_z","Meia extensão Z",halfZ),
  AE_COLLIDER_NUMBER("radius","Raio",radius),
  AE_COLLIDER_NUMBER("half_height","Meia altura cilíndrica",halfHeight),
#undef AE_COLLIDER_NUMBER
#define AE_COLLIDER_CENTER(id,label,field) {label,-10000000,10000000,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Collider&>(v).field;},id}
  AE_COLLIDER_CENTER("center_x","Centro X",centerX),
  AE_COLLIDER_CENTER("center_y","Centro Y",centerY),
  AE_COLLIDER_CENTER("center_z","Centro Z",centerZ),
  AE_COLLIDER_CENTER("rotation_x","Rotação local X · graus",rotationX),
  AE_COLLIDER_CENTER("rotation_y","Rotação local Y · graus",rotationY),
  AE_COLLIDER_CENTER("rotation_z","Rotação local Z · graus",rotationZ)
#undef AE_COLLIDER_CENTER
}};
inline constexpr std::array<ComponentEnumOption,3> colliderShapeOptions{{{0,"Caixa"},{1,"Esfera"},{2,"Cápsula"}}};
inline constexpr std::array<ComponentEnum,1> colliderEnums{{
  {"shape","Forma",colliderShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Collider&>(v).shape);},
    [](ComponentValue &v,u32 value){static_cast<Collider&>(v).shape=static_cast<ColliderShape>(value);}}
}};
inline constexpr std::array<ComponentBoolean,1> colliderBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const Collider&>(v).enabled;},[](ComponentValue &v,bool enabled){static_cast<Collider&>(v).enabled=enabled;}}
}};
inline constexpr std::array<ComponentObjectReference,1> colliderReferences{{
  {"owner","Corpo proprietário","astra.physics.body",ObjectReferenceScope::SelfOrAncestor,"Neste objeto",
    [](const ComponentValue &v){return static_cast<const Collider&>(v).owner;},[](ComponentValue &v,u64 id){static_cast<Collider&>(v).owner=id;}}
}};
inline const ComponentType Collider::descriptor{
  "astra.physics.collider",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider>();},colliderNumbers,colliderBooleans,colliderEnums,nullptr,true,colliderReferences
};
}
