#pragma once
#include "scene/components.h"
#include <array>

namespace ae::scene {
enum class PhysicsFieldKind : u32 { Gravity, Wind, Drag, Radial };
// Authoring only. Membership and forces are evaluated from the live solver COM.
class PhysicsFieldProperties : public ComponentValue {
public:
  bool enabled=true,wakeBodies=true,replaceWorldGravity=true;
  u32 shape=0,falloff=0,affectedLayer=0; // box/sphere; uniform/linear/smooth; all or layer+1
  float halfExtents[3]{3,3,3},radius=3,offset[3]{};
  float vector[3]{},coefficient=1,linearDrag=1,angularDrag=1,acceleration=-9.81f,tangentialAcceleration=0;
  bool valid() const override {
    if(shape>1||falloff>2||affectedLayer>32)return false;
    for(const auto &p:type().numbers){const auto n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
    return true;
  }
  void write(std::ostream &out) const override {
    for(const auto &p:type().numbers)out<<p.read(*this)<<' ';
    for(const auto &p:type().booleans)out<<p.read(*this)<<' ';
    for(const auto &p:type().enums)out<<p.read(*this)<<' ';
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1)return false;
    for(const auto &p:type().numbers)if(!(in>>*p.write(*this)))return false;
    for(const auto &p:type().booleans){bool v;if(!(in>>v))return false;p.write(*this,v);}
    for(const auto &p:type().enums){u32 v;if(!(in>>v))return false;p.write(*this,v);}
    return valid();
  }
};
template<PhysicsFieldKind Kind> class PhysicsField final : public PhysicsFieldProperties {
public:
  PhysicsField(){if constexpr(Kind==PhysicsFieldKind::Gravity)vector[1]=-9.81f;else if constexpr(Kind==PhysicsFieldKind::Wind)vector[0]=5;}
  static const ComponentType descriptor;
  const ComponentType &type() const override{return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<PhysicsField>(*this);}
};
using GravityField=PhysicsField<PhysicsFieldKind::Gravity>;
using WindField=PhysicsField<PhysicsFieldKind::Wind>;
using DragField=PhysicsField<PhysicsFieldKind::Drag>;
using RadialField=PhysicsField<PhysicsFieldKind::Radial>;
inline bool fieldBox(const ComponentValue &v){return static_cast<const PhysicsFieldProperties&>(v).shape==0;}
inline bool fieldSphere(const ComponentValue &v){return !fieldBox(v);}
#define AE_FIELD_NUM(id,label,field,lo,hi,group,unit,visible) ComponentNumber{label,lo,hi,.1f,[](const ComponentValue&v)->const float&{return static_cast<const PhysicsFieldProperties&>(v).field;},[](ComponentValue&v){return &static_cast<PhysicsFieldProperties&>(v).field;},id,{group,unit,nullptr,visible}}
inline constexpr std::array<ComponentNumber,7> fieldVolumeNumbers{{
  AE_FIELD_NUM("half_x","Meia extensão X",halfExtents[0],.001f,10000,"Volume","m",fieldBox),
  AE_FIELD_NUM("half_y","Meia extensão Y",halfExtents[1],.001f,10000,"Volume","m",fieldBox),
  AE_FIELD_NUM("half_z","Meia extensão Z",halfExtents[2],.001f,10000,"Volume","m",fieldBox),
  AE_FIELD_NUM("radius","Raio",radius,.001f,10000,"Volume","m",fieldSphere),
  AE_FIELD_NUM("offset_x","Centro X",offset[0],-10000,10000,"Volume","m",nullptr),
  AE_FIELD_NUM("offset_y","Centro Y",offset[1],-10000,10000,"Volume","m",nullptr),
  AE_FIELD_NUM("offset_z","Centro Z",offset[2],-10000,10000,"Volume","m",nullptr)
}};
template<PhysicsFieldKind K> inline constexpr auto fieldNumbers=[] {
  constexpr usize extra=K==PhysicsFieldKind::Gravity?3:K==PhysicsFieldKind::Wind?4:2;
  std::array<ComponentNumber,7+extra> out{};std::copy(fieldVolumeNumbers.begin(),fieldVolumeNumbers.end(),out.begin());
  if constexpr(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind){
    out[7]=AE_FIELD_NUM("vector_x","Vetor X",vector[0],-10000,10000,"Efeito",(K==PhysicsFieldKind::Gravity?"m/s²":"m/s"),nullptr);
    out[8]=AE_FIELD_NUM("vector_y","Vetor Y",vector[1],-10000,10000,"Efeito",(K==PhysicsFieldKind::Gravity?"m/s²":"m/s"),nullptr);
    out[9]=AE_FIELD_NUM("vector_z","Vetor Z",vector[2],-10000,10000,"Efeito",(K==PhysicsFieldKind::Gravity?"m/s²":"m/s"),nullptr);
    if constexpr(K==PhysicsFieldKind::Wind)out[10]=AE_FIELD_NUM("coefficient","Acoplamento",coefficient,0,10000,"Efeito","kg/s",nullptr);
  }else if constexpr(K==PhysicsFieldKind::Drag){
    out[7]=AE_FIELD_NUM("linear_drag","Arrasto linear",linearDrag,0,1000,"Efeito","1/s",nullptr);
    out[8]=AE_FIELD_NUM("angular_drag","Arrasto angular",angularDrag,0,1000,"Efeito","1/s",nullptr);
  }else{
    out[7]=AE_FIELD_NUM("acceleration","Aceleração radial",acceleration,-10000,10000,"Efeito","m/s²",nullptr);
    out[8]=AE_FIELD_NUM("tangential_acceleration","Aceleração tangencial",tangentialAcceleration,-10000,10000,"Efeito","m/s²",nullptr);
  }
  return out;
}();
#undef AE_FIELD_NUM
template<PhysicsFieldKind K> inline constexpr auto fieldBooleans=[] {
  std::array<ComponentBoolean,K==PhysicsFieldKind::Gravity?3:2> out{};
  out[0]={"enabled","Ativo",[](const ComponentValue&v){return static_cast<const PhysicsFieldProperties&>(v).enabled;},[](ComponentValue&v,bool n){static_cast<PhysicsFieldProperties&>(v).enabled=n;},{"Efeito"}};
  out[1]={"wake_bodies","Acordar corpos",[](const ComponentValue&v){return static_cast<const PhysicsFieldProperties&>(v).wakeBodies;},[](ComponentValue&v,bool n){static_cast<PhysicsFieldProperties&>(v).wakeBodies=n;},{"Alcance","","Desligado: corpos em repouso permanecem dormindo"}};
  if constexpr(K==PhysicsFieldKind::Gravity)out[2]={"replace_world_gravity","Substituir gravidade do mundo",[](const ComponentValue&v){return static_cast<const PhysicsFieldProperties&>(v).replaceWorldGravity;},[](ComponentValue&v,bool n){static_cast<PhysicsFieldProperties&>(v).replaceWorldGravity=n;},{"Efeito","","Sobrepostos somam vetores e cancelam a gravidade padrão uma única vez"}};
  return out;
}();
inline constexpr std::array<ComponentEnumOption,2> fieldShapes{{{0,"Caixa"},{1,"Esfera"}}};
inline constexpr std::array<ComponentEnumOption,3> fieldFalloffs{{{0,"Uniforme"},{1,"Linear"},{2,"Suave"}}};
inline constexpr std::array<const char*,33> fieldLayerNames{{"Todas","Camada 0","Camada 1","Camada 2","Camada 3","Camada 4","Camada 5","Camada 6","Camada 7","Camada 8","Camada 9","Camada 10","Camada 11","Camada 12","Camada 13","Camada 14","Camada 15","Camada 16","Camada 17","Camada 18","Camada 19","Camada 20","Camada 21","Camada 22","Camada 23","Camada 24","Camada 25","Camada 26","Camada 27","Camada 28","Camada 29","Camada 30","Camada 31"}};
inline constexpr auto fieldLayers=[]{std::array<ComponentEnumOption,33> out{};for(u32 n=0;n<33;++n)out[n]={n,fieldLayerNames[n]};return out;}();
inline constexpr std::array<ComponentEnum,3> fieldEnums{{
#define AE_FIELD_ENUM(id,label,field,options,group,help) {id,label,options,[](const ComponentValue&v){return static_cast<const PhysicsFieldProperties&>(v).field;},[](ComponentValue&v,u32 n){static_cast<PhysicsFieldProperties&>(v).field=n;},{group,"",help}}
  AE_FIELD_ENUM("shape","Forma",shape,fieldShapes,"Volume","Escala e orientação são as do objeto e de seus pais"),
  AE_FIELD_ENUM("falloff","Queda de influência",falloff,fieldFalloffs,"Alcance","Centro vale 1; Linear e Suave chegam a zero na borda"),
  AE_FIELD_ENUM("affected_layer","Camada afetada",affectedLayer,fieldLayers,"Alcance","Todas ou uma camada física do projeto; não altera a matriz de colisão")
#undef AE_FIELD_ENUM
}};
template<PhysicsFieldKind K> inline constexpr auto fieldTriples=[] {
  std::array<ComponentTriple,(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind)?3:2> out{};
  out[0]={"half_extents","Meias XYZ",{"half_x","half_y","half_z"}};
  out[1]={"offset","Centro local",{"offset_x","offset_y","offset_z"}};
  if constexpr(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind)out[2]={"vector",K==PhysicsFieldKind::Gravity?"Gravidade local":"Vento local",{"vector_x","vector_y","vector_z"}};
  return out;
}();
#define AE_FIELD_TYPE(T,K,id) template<> inline const ComponentType T::descriptor{id,1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<T>();},fieldNumbers<PhysicsFieldKind::K>,fieldBooleans<PhysicsFieldKind::K>,fieldEnums,nullptr,false,{},fieldTriples<PhysicsFieldKind::K>};
AE_FIELD_TYPE(GravityField,Gravity,"astra.physics.field.gravity")
AE_FIELD_TYPE(WindField,Wind,"astra.physics.field.wind")
AE_FIELD_TYPE(DragField,Drag,"astra.physics.field.drag")
AE_FIELD_TYPE(RadialField,Radial,"astra.physics.field.radial")
#undef AE_FIELD_TYPE
inline int physicsFieldKind(const ComponentValue &v){
  const ComponentType *types[]{&GravityField::descriptor,&WindField::descriptor,&DragField::descriptor,&RadialField::descriptor};
  for(int n=0;n<4;++n)if(v.type().id==types[n]->id)return n;
  return -1;
}
}
