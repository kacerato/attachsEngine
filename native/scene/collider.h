#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
// Mesh é o Mesh Collider da Unity. Sem uma malha de colisão explícita, a forma
// vem da malha do MeshRenderer DESTE objeto (todos os slots), como o
// sharedMesh que a Unity preenche a partir do MeshFilter. Um recurso explícito
// separa a geometria física da visual sem duplicar nem alterar a fonte.
// Com `convex` a física usa o casco convexo e aceita qualquer corpo; sem ele
// usa os triângulos exatos e só aceita corpo estático ou cinemático.
enum class ColliderShape : u32 { Box=0, Sphere=1, Capsule=2, Mesh=3, Cylinder=4 };
// Primitive shapes are independent of visual geometry and body motion. Capsule
// local Y halfHeight excludes the hemispherical ends; dimensions are in local
// scene units.
class Collider final : public ComponentValue {
public:
  ColliderShape shape=ColliderShape::Box;

#include "scene/generated/collider_Collider_fields0.inc"

#include "scene/generated/collider_Collider_fields1.inc"

#include "scene/generated/collider_Collider_fields2.inc"

  u64 owner=0; // zero explicitly means the body on this object

#include "scene/generated/collider_Collider_fields3.inc"

#include "scene/generated/collider_Collider_fields4.inc"
 // só vale para Mesh

#include "scene/generated/collider_Collider_fields5.inc"
 // cooking da malha não convexa

#include "scene/generated/collider_Collider_fields6.inc"
 // árvore mais cara de cozinhar, mais rápida em jogo

#include "scene/generated/collider_Collider_fields7.inc"
 // unidades locais, só no casco convexo

#include "scene/generated/collider_Collider_fields8.inc"
 // graus, só na malha triangular
  // Vazio herda as malhas visuais do objeto. Válido escolhe um único recurso
  // de malha para a forma física (por exemplo, uma versão simplificada).
  resources::AssetGuid collisionMesh{};
  // Opt-in preserves old scenes whose hidden center/rotation were ignored.
#include "scene/generated/collider_Collider_fields9.inc"
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider>(*this);}
  bool valid() const override {
    if(static_cast<u32>(shape)>4 || owner>std::numeric_limits<u32>::max()) return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {
    out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
    out<<' '<<owner<<' '<<enabled<<' '<<convex<<' '<<(collisionMesh.valid()?collisionMesh.text():std::string("-"))
       <<' '<<weldVertices<<' '<<optimizeCooking<<' '<<meshLocalPose;
  }
  bool read(std::istream &in,u32 version) override {
    // v4 acrescentou Malha/Convexo; v5 separa a malha física da visual; v6
    // acrescenta cooking autoral sem reinterpretar os dados antigos.
    u32 kind=0;if((version<1||version>8) || !(in>>kind) || kind>(version>=7?4u:version>=4?3u:2u)) return false;
    shape=static_cast<ColliderShape>(kind);
    centerX=centerY=centerZ=rotationX=rotationY=rotationZ=0;owner=0;enabled=true;convex=false;collisionMesh={};
    weldVertices=true;optimizeCooking=true;hullTolerance=.001f;activeEdgeAngle=5.f;
    const usize numberCount=version==1?5:version==2?8:version<6?11:descriptor.numbers.size();
    for(usize i=0;i<numberCount;++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=3 && !(in>>owner>>enabled)) return false;
    if(version>=4 && !(in>>convex)) return false;
    if(version>=5) {
      std::string guid;if(!(in>>guid) || (guid!="-"&&!resources::AssetGuid::parse(guid,collisionMesh))) return false;
    }
    if(version>=6 && !(in>>weldVertices>>optimizeCooking)) return false;
    meshLocalPose=false;if(version>=8 && !(in>>meshLocalPose))return false;
    return true;
  }
};
inline bool colliderIsBox(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Box;}
inline bool colliderHasRadius(const ComponentValue &v) {const auto s=static_cast<const Collider&>(v).shape;return s==ColliderShape::Sphere||s==ColliderShape::Capsule||s==ColliderShape::Cylinder;}
inline bool colliderIsCapsule(const ComponentValue &v) {const auto s=static_cast<const Collider&>(v).shape;return s==ColliderShape::Capsule||s==ColliderShape::Cylinder;}
inline bool colliderIsMesh(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Mesh;}
inline bool colliderIsConvexMesh(const ComponentValue &v) {const auto &c=static_cast<const Collider&>(v);return c.shape==ColliderShape::Mesh&&c.convex;}
inline bool colliderIsTriangleMesh(const ComponentValue &v) {const auto &c=static_cast<const Collider&>(v);return c.shape==ColliderShape::Mesh&&!c.convex;}
// A malha já está no referencial do objeto: como no Mesh Collider da Unity, não
// há centro nem rotação próprios — a pose é a do objeto.
inline bool colliderIsPrimitive(const ComponentValue &v) {return !colliderIsMesh(v);}
inline bool colliderHasLocalPose(const ComponentValue &v) {const auto &c=static_cast<const Collider&>(v);return c.shape!=ColliderShape::Mesh||c.meshLocalPose;}
#include "scene/generated/collider_colliderNumbers.inc"
inline constexpr std::array<ComponentEnumOption,5> colliderShapeOptions{{{0,"Caixa"},{1,"Esfera"},{2,"Cápsula"},{3,"Malha"},{4,"Cilindro"}}};
#include "scene/generated/collider_colliderEnums.inc"
#include "scene/generated/collider_colliderBooleans.inc"
inline constexpr std::array<ComponentObjectReference,1> colliderReferences{{
  {"owner","Corpo proprietário","astra.physics.body",ObjectReferenceScope::SelfOrAncestor,"Neste objeto",
    [](const ComponentValue &v){return static_cast<const Collider&>(v).owner;},[](ComponentValue &v,u64 id){static_cast<Collider&>(v).owner=id;},{"Vínculo"}}
}};
inline constexpr std::array<ComponentTriple,3> colliderTriples{{
  {"half_extents","Meia extensão",{"half_x","half_y","half_z"}},
  {"center","Centro",{"center_x","center_y","center_z"}},
  {"rotation","Rotação",{"rotation_x","rotation_y","rotation_z"}}
}};
// O endereço continua existindo quando a forma não é Malha: o valor fica
// inativo e escondido, mas ainda participa de dependências, presets e reparo.
inline u32 colliderMeshResourceSlots(const ComponentValue &) {return 1;}
inline constexpr std::array<ComponentResourceBinding,1> colliderResources{{
  {"collision_mesh","Malha de colisão",resources::AssetType::Mesh,colliderMeshResourceSlots,
   [](const ComponentValue &v,u32){return static_cast<const Collider&>(v).collisionMesh;},
   [](ComponentValue &v,u32 slot,resources::AssetGuid value){
     if(slot) return false;
     static_cast<Collider&>(v).collisionMesh=value;
     return true;
   },
   {"Forma","","Vazio usa a malha visual; escolha uma malha simplificada para a física",colliderIsMesh},true}
}};
inline const ComponentType Collider::descriptor{
  "astra.physics.collider",8,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider>();},colliderNumbers,colliderBooleans,colliderEnums,nullptr,true,colliderReferences,colliderTriples,colliderResources
};
}
