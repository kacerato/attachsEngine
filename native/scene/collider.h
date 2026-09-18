#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
// Mesh é o Mesh Collider da Unity: a forma vem da malha do MeshRenderer DESTE
// objeto (todos os slots), como o sharedMesh que a Unity preenche a partir do
// MeshFilter. Com `convex` a física usa o casco convexo e aceita qualquer corpo;
// sem ele usa os triângulos exatos e só aceita corpo estático ou cinemático.
enum class ColliderShape : u32 { Box=0, Sphere=1, Capsule=2, Mesh=3 };
// Primitive shapes are independent of visual geometry and body motion. Capsule
// local Y halfHeight excludes the hemispherical ends; dimensions are in local
// scene units.
class Collider final : public ComponentValue {
public:
  ColliderShape shape=ColliderShape::Box;
  float halfX=.5f,halfY=.5f,halfZ=.5f,radius=.5f,halfHeight=.5f;
  float centerX=0,centerY=0,centerZ=0;
  float rotationX=0,rotationY=0,rotationZ=0;
  u64 owner=0; // zero explicitly means the body on this object
  bool enabled=true;
  bool convex=false; // só vale para Mesh
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider>(*this);}
  bool valid() const override {
    if(static_cast<u32>(shape)>3 || owner>std::numeric_limits<u32>::max()) return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);out<<' '<<owner<<' '<<enabled<<' '<<convex;}
  bool read(std::istream &in,u32 version) override {
    // Versão 4 acrescenta a forma Malha e o Convexo; as anteriores só conhecem
    // as três primitivas e leem Convexo desligado, que era o único estado.
    u32 kind=0;if((version<1||version>4) || !(in>>kind) || kind>(version>=4?3u:2u)) return false;
    shape=static_cast<ColliderShape>(kind);
    centerX=centerY=centerZ=rotationX=rotationY=rotationZ=0;owner=0;enabled=true;convex=false;
    for(usize i=0;i<(version==1?5:version==2?8:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=3 && !(in>>owner>>enabled)) return false;
    if(version>=4 && !(in>>convex)) return false;
    return true;
  }
};
inline bool colliderIsBox(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Box;}
inline bool colliderHasRadius(const ComponentValue &v) {const auto s=static_cast<const Collider&>(v).shape;return s==ColliderShape::Sphere||s==ColliderShape::Capsule;}
inline bool colliderIsCapsule(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Capsule;}
inline bool colliderIsMesh(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Mesh;}
// A malha já está no referencial do objeto: como no Mesh Collider da Unity, não
// há centro nem rotação próprios — a pose é a do objeto.
inline bool colliderIsPrimitive(const ComponentValue &v) {return !colliderIsMesh(v);}
inline constexpr std::array<ComponentNumber,11> colliderNumbers{{
#define AE_COLLIDER_NUMBER(id,label,field,visible) {label,.01f,10000,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Collider&>(v).field;},id,{"Forma","",nullptr,visible}}
  AE_COLLIDER_NUMBER("half_x","Meia extensão X",halfX,colliderIsBox),
  AE_COLLIDER_NUMBER("half_y","Meia extensão Y",halfY,colliderIsBox),
  AE_COLLIDER_NUMBER("half_z","Meia extensão Z",halfZ,colliderIsBox),
  AE_COLLIDER_NUMBER("radius","Raio",radius,colliderHasRadius),
  AE_COLLIDER_NUMBER("half_height","Meia altura cilíndrica",halfHeight,colliderIsCapsule),
#undef AE_COLLIDER_NUMBER
#define AE_COLLIDER_CENTER(id,label,field,unit) {label,-10000000,10000000,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<Collider&>(v).field;},id,{"Pose",unit,nullptr,colliderIsPrimitive}}
  AE_COLLIDER_CENTER("center_x","Centro X",centerX,""),
  AE_COLLIDER_CENTER("center_y","Centro Y",centerY,""),
  AE_COLLIDER_CENTER("center_z","Centro Z",centerZ,""),
  AE_COLLIDER_CENTER("rotation_x","Rotação local X",rotationX,"°"),
  AE_COLLIDER_CENTER("rotation_y","Rotação local Y",rotationY,"°"),
  AE_COLLIDER_CENTER("rotation_z","Rotação local Z",rotationZ,"°")
#undef AE_COLLIDER_CENTER
}};
inline constexpr std::array<ComponentEnumOption,4> colliderShapeOptions{{{0,"Caixa"},{1,"Esfera"},{2,"Cápsula"},{3,"Malha"}}};
inline constexpr std::array<ComponentEnum,1> colliderEnums{{
  {"shape","Forma",colliderShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Collider&>(v).shape);},
    [](ComponentValue &v,u32 value){static_cast<Collider&>(v).shape=static_cast<ColliderShape>(value);},{"Forma"}}
}};
inline constexpr std::array<ComponentBoolean,2> colliderBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const Collider&>(v).enabled;},[](ComponentValue &v,bool enabled){static_cast<Collider&>(v).enabled=enabled;}},
  {"convex","Convexo",[](const ComponentValue &v){return static_cast<const Collider&>(v).convex;},[](ComponentValue &v,bool value){static_cast<Collider&>(v).convex=value;},
    {"Forma","","Casco convexo da malha: aceita corpo dinâmico. Desligado usa os triângulos exatos e só vale em corpo estático ou cinemático.",colliderIsMesh}}
}};
inline constexpr std::array<ComponentObjectReference,1> colliderReferences{{
  {"owner","Corpo proprietário","astra.physics.body",ObjectReferenceScope::SelfOrAncestor,"Neste objeto",
    [](const ComponentValue &v){return static_cast<const Collider&>(v).owner;},[](ComponentValue &v,u64 id){static_cast<Collider&>(v).owner=id;},{"Vínculo"}}
}};
inline constexpr std::array<ComponentTriple,3> colliderTriples{{
  {"half_extents","Meia extensão",{"half_x","half_y","half_z"}},
  {"center","Centro",{"center_x","center_y","center_z"}},
  {"rotation","Rotação",{"rotation_x","rotation_y","rotation_z"}}
}};
inline const ComponentType Collider::descriptor{
  "astra.physics.collider",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider>();},colliderNumbers,colliderBooleans,colliderEnums,nullptr,true,colliderReferences,colliderTriples
};
}
