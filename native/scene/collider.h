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
  bool weldVertices=true; // cooking da malha não convexa
  bool optimizeCooking=true; // árvore mais cara de cozinhar, mais rápida em jogo
  float hullTolerance=.001f; // unidades locais, só no casco convexo
  float activeEdgeAngle=5.f; // graus, só na malha triangular
  // Vazio herda as malhas visuais do objeto. Válido escolhe um único recurso
  // de malha para a forma física (por exemplo, uma versão simplificada).
  resources::AssetGuid collisionMesh{};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider>(*this);}
  bool valid() const override {
    if(static_cast<u32>(shape)>3 || owner>std::numeric_limits<u32>::max()) return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {
    out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
    out<<' '<<owner<<' '<<enabled<<' '<<convex<<' '<<(collisionMesh.valid()?collisionMesh.text():std::string("-"))
       <<' '<<weldVertices<<' '<<optimizeCooking;
  }
  bool read(std::istream &in,u32 version) override {
    // v4 acrescentou Malha/Convexo; v5 separa a malha física da visual; v6
    // acrescenta cooking autoral sem reinterpretar os dados antigos.
    u32 kind=0;if((version<1||version>6) || !(in>>kind) || kind>(version>=4?3u:2u)) return false;
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
    return true;
  }
};
inline bool colliderIsBox(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Box;}
inline bool colliderHasRadius(const ComponentValue &v) {const auto s=static_cast<const Collider&>(v).shape;return s==ColliderShape::Sphere||s==ColliderShape::Capsule;}
inline bool colliderIsCapsule(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Capsule;}
inline bool colliderIsMesh(const ComponentValue &v) {return static_cast<const Collider&>(v).shape==ColliderShape::Mesh;}
inline bool colliderIsConvexMesh(const ComponentValue &v) {const auto &c=static_cast<const Collider&>(v);return c.shape==ColliderShape::Mesh&&c.convex;}
inline bool colliderIsTriangleMesh(const ComponentValue &v) {const auto &c=static_cast<const Collider&>(v);return c.shape==ColliderShape::Mesh&&!c.convex;}
// A malha já está no referencial do objeto: como no Mesh Collider da Unity, não
// há centro nem rotação próprios — a pose é a do objeto.
inline bool colliderIsPrimitive(const ComponentValue &v) {return !colliderIsMesh(v);}
inline constexpr std::array<ComponentNumber,13> colliderNumbers{{
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
  AE_COLLIDER_CENTER("rotation_z","Rotação local Z",rotationZ,"°"),
#undef AE_COLLIDER_CENTER
  {"Tolerância do casco",.00001f,1.f,.001f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).hullTolerance;},
   [](ComponentValue &v)->float*{return &static_cast<Collider&>(v).hullTolerance;},"hull_tolerance",
   {"Cozimento","u","Pontos até esta distância podem ficar fora do casco; valores maiores geram cascos mais simples.",colliderIsConvexMesh}},
  {"Ângulo de aresta ativa",0.f,90.f,1.f,[](const ComponentValue &v)->const float&{return static_cast<const Collider&>(v).activeEdgeAngle;},
   [](ComponentValue &v)->float*{return &static_cast<Collider&>(v).activeEdgeAngle;},"active_edge_angle",
   {"Cozimento","°","Separa arestas de contato em superfícies com mudança de normal acima deste ângulo.",colliderIsTriangleMesh}}
}};
inline constexpr std::array<ComponentEnumOption,4> colliderShapeOptions{{{0,"Caixa"},{1,"Esfera"},{2,"Cápsula"},{3,"Malha"}}};
inline constexpr std::array<ComponentEnum,1> colliderEnums{{
  {"shape","Forma",colliderShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Collider&>(v).shape);},
    [](ComponentValue &v,u32 value){static_cast<Collider&>(v).shape=static_cast<ColliderShape>(value);},{"Forma"}}
}};
inline constexpr std::array<ComponentBoolean,4> colliderBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return static_cast<const Collider&>(v).enabled;},[](ComponentValue &v,bool enabled){static_cast<Collider&>(v).enabled=enabled;}},
  {"convex","Convexo",[](const ComponentValue &v){return static_cast<const Collider&>(v).convex;},[](ComponentValue &v,bool value){static_cast<Collider&>(v).convex=value;},
    {"Forma","","Casco convexo da malha: aceita corpo dinâmico. Desligado usa os triângulos exatos e só vale em corpo estático ou cinemático.",colliderIsMesh}},
  {"weld_vertices","Soldar vértices iguais",[](const ComponentValue &v){return static_cast<const Collider&>(v).weldVertices;},
    [](ComponentValue &v,bool value){static_cast<Collider&>(v).weldVertices=value;},
    {"Cozimento","","Compartilha vértices coincidentes antes de criar a malha física, reduzindo costuras internas.",colliderIsTriangleMesh}},
  {"optimize_cooking","Otimizar para o jogo",[](const ComponentValue &v){return static_cast<const Collider&>(v).optimizeCooking;},
    [](ComponentValue &v,bool value){static_cast<Collider&>(v).optimizeCooking=value;},
    {"Cozimento","","Constrói uma árvore de busca mais eficiente; desligue para cozinhar mais rápido durante iterações.",colliderIsTriangleMesh}}
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
  "astra.physics.collider",6,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider>();},colliderNumbers,colliderBooleans,colliderEnums,nullptr,true,colliderReferences,colliderTriples,colliderResources
};
}
