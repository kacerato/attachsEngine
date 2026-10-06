#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class Body2DMotion : u32 { Static, Kinematic, Dynamic };
enum class Collider2DShape : u32 { Box, Circle, Capsule };
class Body2D final : public ComponentValue {
public:
 Body2DMotion motion=Body2DMotion::Dynamic;

#include "scene/generated/physics2d_components_Body2D_fields0.inc"

#include "scene/generated/physics2d_components_Body2D_fields1.inc"

#include "scene/generated/physics2d_components_Body2D_fields2.inc"

#include "scene/generated/physics2d_components_Body2D_fields3.inc"

#include "scene/generated/physics2d_components_Body2D_fields4.inc"

#include "scene/generated/physics2d_components_Body2D_fields5.inc"

#include "scene/generated/physics2d_components_Body2D_fields6.inc"

#include "scene/generated/physics2d_components_Body2D_fields7.inc"

 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Body2D>(*this);}
 bool valid() const override {if(static_cast<u32>(motion)>2)return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &out) const override {out<<static_cast<u32>(motion);for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);out<<' '<<fixedRotation;out<<' '<<allowSleep;}
 bool read(std::istream &in,u32 version) override {u32 n;if(version!=1||!(in>>n)||n>2)return false;motion=static_cast<Body2DMotion>(n);for(const auto &p:descriptor.numbers)if(!(in>>*p.write(*this)))return false;if(!(in>>fixedRotation))return false;if(!(in>>allowSleep))return false;return valid();}
};
inline bool body2dDynamic(const ComponentValue &v){return static_cast<const Body2D&>(v).motion==Body2DMotion::Dynamic;}
inline bool body2dMoves(const ComponentValue &v){return static_cast<const Body2D&>(v).motion!=Body2DMotion::Static;}
#include "scene/generated/physics2d_components_Body2DNumbers.inc"
#include "scene/generated/physics2d_components_Body2DBooleans.inc"
inline constexpr std::array<ComponentEnumOption,3> Body2DOptions{{{0,"Estático"},{1,"Cinemático"},{2,"Dinâmico"}}};
#include "scene/generated/physics2d_components_Body2DEnums.inc"
inline const ComponentType Body2D::descriptor{"astra.physics2d.body",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Body2D>();},Body2DNumbers,Body2DBooleans,Body2DEnums,nullptr,false};
class Collider2D final : public ComponentValue {
public:
 Collider2DShape shape=Collider2DShape::Box;

#include "scene/generated/physics2d_components_Collider2D_fields0.inc"

#include "scene/generated/physics2d_components_Collider2D_fields1.inc"

#include "scene/generated/physics2d_components_Collider2D_fields2.inc"

#include "scene/generated/physics2d_components_Collider2D_fields3.inc"

#include "scene/generated/physics2d_components_Collider2D_fields4.inc"

#include "scene/generated/physics2d_components_Collider2D_fields5.inc"

#include "scene/generated/physics2d_components_Collider2D_fields6.inc"

#include "scene/generated/physics2d_components_Collider2D_fields7.inc"

#include "scene/generated/physics2d_components_Collider2D_fields8.inc"

 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider2D>(*this);}
 bool valid() const override {if(static_cast<u32>(shape)>2)return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &out) const override {out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);out<<' '<<sensor;}
 bool read(std::istream &in,u32 version) override {u32 n;if(version!=1||!(in>>n)||n>2)return false;shape=static_cast<Collider2DShape>(n);for(const auto &p:descriptor.numbers)if(!(in>>*p.write(*this)))return false;if(!(in>>sensor))return false;return valid();}
};
#include "scene/generated/physics2d_components_Collider2DNumbers.inc"
#include "scene/generated/physics2d_components_Collider2DBooleans.inc"
inline constexpr std::array<ComponentEnumOption,3> Collider2DOptions{{{0,"Caixa"},{1,"Círculo"},{2,"Cápsula Y"}}};
#include "scene/generated/physics2d_components_Collider2DEnums.inc"
// Acontecimentos do Box2D. A instância é o colisor que tocou: o solver 2D
// informa a forma, ao contrário do 3D.
inline constexpr std::array<ComponentParameter,1> physics2DOtherPayload{{{"other","Outro objeto",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentEvent,4> physics2DContactEvents{{
  {"trigger_enter","Sensor: entrou","Outro corpo começou a sobrepor este sensor",physics2DOtherPayload},
  {"trigger_exit","Sensor: saiu","Outro corpo deixou de sobrepor este sensor",physics2DOtherPayload},
  {"collision_enter","Colisão: começou","Contato sólido começou; entregue aos dois colisores",physics2DOtherPayload},
  {"collision_exit","Colisão: terminou","Contato sólido terminou; entregue aos dois colisores",physics2DOtherPayload},
}};
inline const ComponentType Collider2D::descriptor{"astra.physics2d.collider",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider2D>();},Collider2DNumbers,Collider2DBooleans,Collider2DEnums,nullptr,true,{},{},{},{},{},{},{},physics2DContactEvents};

class ConstantForce2D final : public ComponentValue {
public:

#include "scene/generated/physics2d_components_ConstantForce2D_fields0.inc"

#include "scene/generated/physics2d_components_ConstantForce2D_fields1.inc"

 static const ComponentType descriptor;
 const ComponentType &type() const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<ConstantForce2D>(*this);}
 bool valid() const override{for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &o)const override{o<<enabled;for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream &i,u32 v)override{if(v!=1||!(i>>enabled))return false;for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
#include "scene/generated/physics2d_components_force2DNumbers.inc"
#include "scene/generated/physics2d_components_force2DBooleans.inc"
inline const ComponentType ConstantForce2D::descriptor{"astra.physics2d.constant-force",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ConstantForce2D>();},force2DNumbers,force2DBooleans};
enum class Joint2DKind:u32{Weld,Revolute,Prismatic,Distance};
class Joint2D final:public ComponentValue {
public:
 Joint2DKind kind=Joint2DKind::Weld;u64 target=0;

#include "scene/generated/physics2d_components_Joint2D_fields0.inc"

#include "scene/generated/physics2d_components_Joint2D_fields1.inc"

#include "scene/generated/physics2d_components_Joint2D_fields2.inc"

#include "scene/generated/physics2d_components_Joint2D_fields3.inc"

#include "scene/generated/physics2d_components_Joint2D_fields4.inc"

#include "scene/generated/physics2d_components_Joint2D_fields5.inc"

 static const ComponentType descriptor;
 const ComponentType &type()const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone()const override{return std::make_unique<Joint2D>(*this);}
 bool valid()const override{if(static_cast<u32>(kind)>3||target>std::numeric_limits<u32>::max())return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return !limitEnabled||((kind!=Joint2DKind::Distance||minLength<=maxLength)&&((kind!=Joint2DKind::Revolute&&kind!=Joint2DKind::Prismatic)||lowerLimit<=upperLimit));}
 void write(std::ostream &o)const override{o<<static_cast<u32>(kind)<<' '<<target<<' '<<enabled<<' '<<worldAnchor<<' '<<collideConnected<<' '<<limitEnabled<<' '<<motorEnabled<<' '<<springEnabled;for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream &i,u32 v)override{u32 n;if(v!=1||!(i>>n>>target>>enabled>>worldAnchor>>collideConnected>>limitEnabled>>motorEnabled>>springEnabled)||n>3)return false;kind=static_cast<Joint2DKind>(n);for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
#include "scene/generated/physics2d_components_joint2DNumbers.inc"
#include "scene/generated/physics2d_components_joint2DBooleans.inc"
inline constexpr std::array<ComponentEnumOption,4> joint2DOptions{{{0,"Fixed / Weld"},{1,"Revolute"},{2,"Prismatic"},{3,"Distance"}}};
#include "scene/generated/physics2d_components_joint2DEnums.inc"
inline constexpr std::array<ComponentObjectReference,1> joint2DReferences{{{"target","Corpo conectado","astra.physics2d.body",ObjectReferenceScope::Other,"Escolher Body2D",[](const ComponentValue &v){return static_cast<const Joint2D&>(v).target;},[](ComponentValue &v,u64 id){static_cast<Joint2D&>(v).target=id;},{"Conexão","",nullptr,[](const ComponentValue &v){return !static_cast<const Joint2D&>(v).worldAnchor;}},[](const ComponentValue &v){const auto &c=static_cast<const Joint2D&>(v);return c.enabled&&!c.worldAnchor;}}}};
inline const ComponentType Joint2D::descriptor{"astra.physics2d.joint",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Joint2D>();},joint2DNumbers,joint2DBooleans,joint2DEnums,nullptr,true,joint2DReferences};
}
