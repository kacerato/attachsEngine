// Consultas físicas persistentes (bloco F, F043): Raio, Varredura de forma e
// Braço de mola. São componentes autorados que consultam o mundo físico a
// cada quadro de Play (runtime/scene_physics_queries.h) e guardam o último
// resultado; scripts e Conexões de evento leem pelos métodos.
//
// Referências: Godot 4.5 RayCast3D, ShapeCast3D e SpringArm3D
// https://docs.godotengine.org/en/4.5/classes/class_raycast3d.html
// https://docs.godotengine.org/en/4.5/classes/class_shapecast3d.html
// https://docs.godotengine.org/en/4.5/classes/class_springarm3d.html
#pragma once
#include "scene/components.h"
#include "scene/physics_field.h"

#include <array>
#include <cmath>

namespace ae::scene {

// Filtro comum: camada (0 todas, n camada n−1), o próprio corpo, sensores e tipos de corpo.
struct PhysicsQueryFilterFields {
  u32 layer=0;
  bool enabled=true,excludeSelf=true,includeSensors=false,includeStatic=true,includeDynamic=true;
  bool valid() const {return layer<=32;}
  void write(std::ostream &out) const {out<<enabled<<' '<<layer<<' '<<excludeSelf<<' '<<includeSensors<<' '<<includeStatic<<' '<<includeDynamic;}
  bool read(std::istream &in) {return static_cast<bool>(in>>enabled>>layer>>excludeSelf>>includeSensors>>includeStatic>>includeDynamic);}
};

class RayCast final : public ComponentValue {
public:
  PhysicsQueryFilterFields filter;
  float target[3]{0,-1,0};       // local: direção e alcance
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<RayCast>(*this);}
  bool valid() const override {
    const float length=std::sqrt(target[0]*target[0]+target[1]*target[1]+target[2]*target[2]);
    return filter.valid() && std::isfinite(length) && length>1e-4f && length<=10000;
  }
  void write(std::ostream &out) const override {filter.write(out);out<<' '<<target[0]<<' '<<target[1]<<' '<<target[2];}
  bool read(std::istream &in,u32 version) override {return version==1 && filter.read(in) && static_cast<bool>(in>>target[0]>>target[1]>>target[2]) && valid();}
};

enum class ShapeCastShape : u32 {Sphere=0,Box=1,Capsule=2,Cylinder=3};
class ShapeCast final : public ComponentValue {
public:
  PhysicsQueryFilterFields filter;
  ShapeCastShape shape=ShapeCastShape::Sphere;
  float radius=.5f,halfHeight=.5f,halfExtent[3]{.5f,.5f,.5f};
  float target[3]{0,-1,0};
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<ShapeCast>(*this);}
  bool valid() const override {
    if(!filter.valid()||static_cast<u32>(shape)>3) return false;
    for(const float v:{radius,halfHeight,halfExtent[0],halfExtent[1],halfExtent[2]}) if(!std::isfinite(v)||v<.001f||v>1000) return false;
    for(const float v:target) if(!std::isfinite(v)||std::abs(v)>10000) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    filter.write(out);out<<' '<<static_cast<u32>(shape)<<' '<<radius<<' '<<halfHeight;
    for(const float v:halfExtent) out<<' '<<v;
    for(const float v:target) out<<' '<<v;
  }
  bool read(std::istream &in,u32 version) override {
    u32 kind=0;if(version!=1||!filter.read(in)||!(in>>kind>>radius>>halfHeight)) return false;
    shape=static_cast<ShapeCastShape>(kind);
    for(float &v:halfExtent) if(!(in>>v)) return false;
    for(float &v:target) if(!(in>>v)) return false;
    return valid();
  }
};

class SpringArm final : public ComponentValue {
public:
  PhysicsQueryFilterFields filter;
  float length=3,margin=.1f,radius=0;   // raio zero: raio de luz; maior: esfera
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<SpringArm>(*this);}
  bool valid() const override {
    return filter.valid() && std::isfinite(length) && length>.001f && length<=1000 && std::isfinite(margin) && margin>=0 && margin<=10 &&
           std::isfinite(radius) && radius>=0 && radius<=100;
  }
  void write(std::ostream &out) const override {filter.write(out);out<<' '<<length<<' '<<margin<<' '<<radius;}
  bool read(std::istream &in,u32 version) override {return version==1 && filter.read(in) && static_cast<bool>(in>>length>>margin>>radius) && valid();}
};

namespace physics_query_detail {
template<class T> PhysicsQueryFilterFields &filterOf(ComponentValue &v) {return static_cast<T&>(v).filter;}
template<class T> const PhysicsQueryFilterFields &filterOf(const ComponentValue &v) {return static_cast<const T&>(v).filter;}
template<class T> std::array<ComponentBoolean,5> booleans() {
  return {{
    {"enabled","Ativo",[](const ComponentValue &v){return filterOf<T>(v).enabled;},[](ComponentValue &v,bool b){filterOf<T>(v).enabled=b;},{"Consulta","","Desligado não consulta e mantém o último resultado como vazio"}},
    {"exclude_self","Ignorar o próprio corpo",[](const ComponentValue &v){return filterOf<T>(v).excludeSelf;},[](ComponentValue &v,bool b){filterOf<T>(v).excludeSelf=b;},{"Filtro","","Ignora o corpo deste objeto ou do ancestral mais próximo com Corpo físico"}},
    {"include_sensors","Incluir sensores",[](const ComponentValue &v){return filterOf<T>(v).includeSensors;},[](ComponentValue &v,bool b){filterOf<T>(v).includeSensors=b;},{"Filtro","","Sensores ficam de fora por padrão (collide_with_areas do Godot)"}},
    {"include_static","Incluir estáticos",[](const ComponentValue &v){return filterOf<T>(v).includeStatic;},[](ComponentValue &v,bool b){filterOf<T>(v).includeStatic=b;},{"Filtro"}},
    {"include_dynamic","Incluir dinâmicos",[](const ComponentValue &v){return filterOf<T>(v).includeDynamic;},[](ComponentValue &v,bool b){filterOf<T>(v).includeDynamic=b;},{"Filtro"}},
  }};
}
template<class T> ComponentEnum layer() {
  return {"layer","Camada",fieldLayers,[](const ComponentValue &v){return filterOf<T>(v).layer;},[](ComponentValue &v,u32 x){filterOf<T>(v).layer=x;},
          {"Filtro","","Todas ou só uma camada física do projeto"}};
}
} // namespace physics_query_detail

inline constexpr std::array<ComponentMethod,6> physicsQueryMethods{{
  {"colliding","Acertando","Verdadeiro quando a última consulta encontrou algo",{},ComponentValueKind::Boolean},
  {"collider","Objeto atingido","Dono do corpo atingido; vazio sem acerto",{},ComponentValueKind::Object},
  {"point","Ponto","Ponto do acerto no mundo",{},ComponentValueKind::Vector3},
  {"normal","Normal","Normal da superfície atingida",{},ComponentValueKind::Vector3},
  {"distance","Distância","Distância do acerto ao início, em metros",{},ComponentValueKind::Number},
  {"update","Atualizar agora","Refaz a consulta neste instante (force_raycast_update do Godot)"},
}};

inline const std::array<ComponentBoolean,5> rayCastBooleans=physics_query_detail::booleans<RayCast>();
inline const std::array<ComponentEnum,1> rayCastEnums{physics_query_detail::layer<RayCast>()};
inline constexpr std::array<ComponentNumber,3> rayCastNumbers{{
  {"Alvo X",-10000,10000,.1f,[](const ComponentValue &v)->const float &{return static_cast<const RayCast&>(v).target[0];},[](ComponentValue &v)->float *{return &static_cast<RayCast&>(v).target[0];},"target_x",{"Consulta","m"}},
  {"Alvo Y",-10000,10000,.1f,[](const ComponentValue &v)->const float &{return static_cast<const RayCast&>(v).target[1];},[](ComponentValue &v)->float *{return &static_cast<RayCast&>(v).target[1];},"target_y",{"Consulta","m"}},
  {"Alvo Z",-10000,10000,.1f,[](const ComponentValue &v)->const float &{return static_cast<const RayCast&>(v).target[2];},[](ComponentValue &v)->float *{return &static_cast<RayCast&>(v).target[2];},"target_z",{"Consulta","m"}},
}};
inline constexpr std::array<ComponentTriple,1> rayCastTriples{{{"target","Alvo local",{"target_x","target_y","target_z"}}}};
inline const ComponentType RayCast::descriptor{
  "astra.physics.raycast",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<RayCast>();},
  rayCastNumbers,rayCastBooleans,rayCastEnums,nullptr,true,{},rayCastTriples,{},{},{},{},physicsQueryMethods};

inline bool shapeCastBox(const ComponentValue &v) {return static_cast<const ShapeCast&>(v).shape==ShapeCastShape::Box;}
inline bool shapeCastRound(const ComponentValue &v) {return static_cast<const ShapeCast&>(v).shape!=ShapeCastShape::Box;}
inline bool shapeCastTall(const ComponentValue &v) {const auto s=static_cast<const ShapeCast&>(v).shape;return s==ShapeCastShape::Capsule||s==ShapeCastShape::Cylinder;}
inline constexpr std::array<ComponentEnumOption,4> shapeCastShapes{{{0,"Esfera"},{1,"Caixa"},{2,"Cápsula"},{3,"Cilindro"}}};
inline const std::array<ComponentBoolean,5> shapeCastBooleans=physics_query_detail::booleans<ShapeCast>();
inline const std::array<ComponentEnum,2> shapeCastEnums{
  physics_query_detail::layer<ShapeCast>(),
  ComponentEnum{"shape","Forma",shapeCastShapes,[](const ComponentValue &v){return static_cast<u32>(static_cast<const ShapeCast&>(v).shape);},
   [](ComponentValue &v,u32 x){static_cast<ShapeCast&>(v).shape=static_cast<ShapeCastShape>(x);},{"Forma"}}};
#define AE_SHAPE_NUMBER(LABEL,MIN,MAX,MEMBER,ID,GROUP,UNIT,VISIBLE) \
  ComponentNumber{LABEL,MIN,MAX,.05f,[](const ComponentValue &v)->const float &{return static_cast<const ShapeCast&>(v).MEMBER;}, \
   [](ComponentValue &v)->float *{return &static_cast<ShapeCast&>(v).MEMBER;},ID,{GROUP,UNIT,nullptr,VISIBLE}}
inline constexpr std::array<ComponentNumber,8> shapeCastNumbers{{
  AE_SHAPE_NUMBER("Raio",.001f,1000,radius,"radius","Forma","m",shapeCastRound),
  AE_SHAPE_NUMBER("Meia altura",.001f,1000,halfHeight,"half_height","Forma","m",shapeCastTall),
  AE_SHAPE_NUMBER("Meia extensão X",.001f,1000,halfExtent[0],"half_x","Forma","m",shapeCastBox),
  AE_SHAPE_NUMBER("Meia extensão Y",.001f,1000,halfExtent[1],"half_y","Forma","m",shapeCastBox),
  AE_SHAPE_NUMBER("Meia extensão Z",.001f,1000,halfExtent[2],"half_z","Forma","m",shapeCastBox),
  AE_SHAPE_NUMBER("Alvo X",-10000,10000,target[0],"target_x","Consulta","m",nullptr),
  AE_SHAPE_NUMBER("Alvo Y",-10000,10000,target[1],"target_y","Consulta","m",nullptr),
  AE_SHAPE_NUMBER("Alvo Z",-10000,10000,target[2],"target_z","Consulta","m",nullptr),
}};
#undef AE_SHAPE_NUMBER
inline constexpr std::array<ComponentTriple,2> shapeCastTriples{{
  {"half_extents","Meia extensão",{"half_x","half_y","half_z"}},{"target","Alvo local",{"target_x","target_y","target_z"}}}};
inline const ComponentType ShapeCast::descriptor{
  "astra.physics.shapecast",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ShapeCast>();},
  shapeCastNumbers,shapeCastBooleans,shapeCastEnums,nullptr,true,{},shapeCastTriples,{},{},{},{},physicsQueryMethods};

inline const std::array<ComponentBoolean,5> springArmBooleans=physics_query_detail::booleans<SpringArm>();
inline const std::array<ComponentEnum,1> springArmEnums{physics_query_detail::layer<SpringArm>()};
inline constexpr std::array<ComponentNumber,3> springArmNumbers{{
  {"Comprimento",.001f,1000,.1f,[](const ComponentValue &v)->const float &{return static_cast<const SpringArm&>(v).length;},
   [](ComponentValue &v)->float *{return &static_cast<SpringArm&>(v).length;},"length",{"Braço","m","Distância livre máxima ao longo de +Z local"}},
  {"Margem",0,10,.05f,[](const ComponentValue &v)->const float &{return static_cast<const SpringArm&>(v).margin;},
   [](ComponentValue &v)->float *{return &static_cast<SpringArm&>(v).margin;},"margin",{"Braço","m","Folga entre o filho e a superfície atingida"}},
  {"Raio da esfera",0,100,.05f,[](const ComponentValue &v)->const float &{return static_cast<const SpringArm&>(v).radius;},
   [](ComponentValue &v)->float *{return &static_cast<SpringArm&>(v).radius;},"radius",{"Braço","m","Zero usa raio de luz; maior varre uma esfera (evita a câmera atravessar quinas)"}},
}};
inline constexpr std::array<ComponentMethod,1> springArmMethods{{
  {"hit_length","Comprimento atual","Distância livre do último quadro, já descontada a margem",{},ComponentValueKind::Number},
}};
inline const ComponentType SpringArm::descriptor{
  "astra.physics.spring_arm",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<SpringArm>();},
  springArmNumbers,springArmBooleans,springArmEnums,nullptr,false,{},{},{},{},{},{},springArmMethods};

} // namespace ae::scene
