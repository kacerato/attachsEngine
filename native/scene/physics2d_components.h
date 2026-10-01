#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class Body2DMotion : u32 { Static, Kinematic, Dynamic };
enum class Collider2DShape : u32 { Box, Circle, Capsule };
class Body2D final : public ComponentValue {
public:
 Body2DMotion motion=Body2DMotion::Dynamic;
 bool fixedRotation=false,allowSleep=true;
 float mass=1.f;
 float gravityScale=1.f;
 float velocityX=0.f;
 float velocityY=0.f;
 float angularVelocityDegrees=0.f;
 float linearDamping=0.05f;
 float angularDamping=0.05f;
 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Body2D>(*this);}
 bool valid() const override {if(static_cast<u32>(motion)>2)return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &out) const override {out<<static_cast<u32>(motion);for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);out<<' '<<fixedRotation;out<<' '<<allowSleep;}
 bool read(std::istream &in,u32 version) override {u32 n;if(version!=1||!(in>>n)||n>2)return false;motion=static_cast<Body2DMotion>(n);for(const auto &p:descriptor.numbers)if(!(in>>*p.write(*this)))return false;if(!(in>>fixedRotation))return false;if(!(in>>allowSleep))return false;return valid();}
};
inline bool body2dDynamic(const ComponentValue &v){return static_cast<const Body2D&>(v).motion==Body2DMotion::Dynamic;}
inline bool body2dMoves(const ComponentValue &v){return static_cast<const Body2D&>(v).motion!=Body2DMotion::Static;}
inline constexpr std::array<ComponentNumber,7> Body2DNumbers{{
 {"Massa",0.001f,100000,0.1f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).mass;},[](ComponentValue &v){return &static_cast<Body2D&>(v).mass;},"mass",{"Corpo","kg" ,nullptr,body2dDynamic}},
 {"Escala gravidade",-100,100,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).gravityScale;},[](ComponentValue &v){return &static_cast<Body2D&>(v).gravityScale;},"gravity_scale",{"Corpo","" ,nullptr,body2dDynamic}},
 {"Velocidade X",-10000,10000,0.1f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).velocityX;},[](ComponentValue &v){return &static_cast<Body2D&>(v).velocityX;},"velocity_x",{"Movimento","m/s" ,nullptr,body2dMoves}},
 {"Velocidade Y",-10000,10000,0.1f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).velocityY;},[](ComponentValue &v){return &static_cast<Body2D&>(v).velocityY;},"velocity_y",{"Movimento","m/s" ,nullptr,body2dMoves}},
 {"Velocidade angular",-36000,36000,1.f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).angularVelocityDegrees;},[](ComponentValue &v){return &static_cast<Body2D&>(v).angularVelocityDegrees;},"angular_velocity_degrees",{"Movimento","graus/s" ,nullptr,[](const ComponentValue &v){const auto &c=static_cast<const Body2D&>(v);return c.motion==Body2DMotion::Kinematic||(c.motion==Body2DMotion::Dynamic&&!c.fixedRotation);}}},
 {"Arrasto linear",0,100,0.01f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).linearDamping;},[](ComponentValue &v){return &static_cast<Body2D&>(v).linearDamping;},"linear_damping",{"Movimento","" ,nullptr,body2dDynamic}},
 {"Arrasto angular",0,100,0.01f,[](const ComponentValue &v)->const float&{return static_cast<const Body2D&>(v).angularDamping;},[](ComponentValue &v){return &static_cast<Body2D&>(v).angularDamping;},"angular_damping",{"Movimento","" ,nullptr,body2dDynamic}},
}};
inline constexpr std::array<ComponentBoolean,2> Body2DBooleans{{
 {"fixed_rotation","Fixar rotação",[](const ComponentValue &v){return static_cast<const Body2D&>(v).fixedRotation;},[](ComponentValue &v,bool n){static_cast<Body2D&>(v).fixedRotation=n;},{"Movimento","",nullptr,body2dDynamic}},
 {"allow_sleep","Permitir repouso",[](const ComponentValue &v){return static_cast<const Body2D&>(v).allowSleep;},[](ComponentValue &v,bool n){static_cast<Body2D&>(v).allowSleep=n;},{"Movimento","",nullptr,body2dDynamic}},
}};
inline constexpr std::array<ComponentEnumOption,3> Body2DOptions{{{0,"Estático"},{1,"Cinemático"},{2,"Dinâmico"}}};
inline constexpr std::array<ComponentEnum,1> Body2DEnums{{{"motion","Movimento",Body2DOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Body2D&>(v).motion);},[](ComponentValue &v,u32 n){static_cast<Body2D&>(v).motion=static_cast<Body2DMotion>(n);},{"Corpo"}}}};
inline const ComponentType Body2D::descriptor{"astra.physics2d.body",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Body2D>();},Body2DNumbers,Body2DBooleans,Body2DEnums,nullptr,false};
class Collider2D final : public ComponentValue {
public:
 Collider2DShape shape=Collider2DShape::Box;
 bool sensor=false;
 float halfX=0.5f;
 float halfY=0.5f;
 float radius=0.5f;
 float capsuleHalfLength=0.5f;
 float offsetX=0.f;
 float offsetY=0.f;
 float friction=0.5f;
 float restitution=0.f;
 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Collider2D>(*this);}
 bool valid() const override {if(static_cast<u32>(shape)>2)return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &out) const override {out<<static_cast<u32>(shape);for(const auto &p:descriptor.numbers)out<<' '<<p.read(*this);out<<' '<<sensor;}
 bool read(std::istream &in,u32 version) override {u32 n;if(version!=1||!(in>>n)||n>2)return false;shape=static_cast<Collider2DShape>(n);for(const auto &p:descriptor.numbers)if(!(in>>*p.write(*this)))return false;if(!(in>>sensor))return false;return valid();}
};
inline constexpr std::array<ComponentNumber,8> Collider2DNumbers{{
 {"Meia largura",0.005f,500,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).halfX;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).halfX;},"half_x",{"Forma","m",nullptr,[](const ComponentValue &v){return static_cast<const Collider2D&>(v).shape==Collider2DShape::Box;}}},
 {"Meia altura",0.005f,500,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).halfY;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).halfY;},"half_y",{"Forma","m",nullptr,[](const ComponentValue &v){return static_cast<const Collider2D&>(v).shape==Collider2DShape::Box;}}},
 {"Raio",0.005f,500,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).radius;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).radius;},"radius",{"Forma","m",nullptr,[](const ComponentValue &v){return static_cast<const Collider2D&>(v).shape!=Collider2DShape::Box;}}},
 {"Meia distância centros",0.005f,500,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).capsuleHalfLength;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).capsuleHalfLength;},"capsule_half_length",{"Forma","m",nullptr,[](const ComponentValue &v){return static_cast<const Collider2D&>(v).shape==Collider2DShape::Capsule;}}},
 {"Centro X",-1000,1000,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).offsetX;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).offsetX;},"offset_x",{"Forma","m"}},
 {"Centro Y",-1000,1000,0.05f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).offsetY;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).offsetY;},"offset_y",{"Forma","m"}},
 {"Atrito",0,1,0.01f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).friction;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).friction;},"friction",{"Contato","",nullptr,[](const ComponentValue &v){return !static_cast<const Collider2D&>(v).sensor;}}},
 {"Restituição",0,1,0.01f,[](const ComponentValue &v)->const float&{return static_cast<const Collider2D&>(v).restitution;},[](ComponentValue &v){return &static_cast<Collider2D&>(v).restitution;},"restitution",{"Contato","",nullptr,[](const ComponentValue &v){return !static_cast<const Collider2D&>(v).sensor;}}},
}};
inline constexpr std::array<ComponentBoolean,1> Collider2DBooleans{{
 {"sensor","Sensor sem resposta",[](const ComponentValue &v){return static_cast<const Collider2D&>(v).sensor;},[](ComponentValue &v,bool n){static_cast<Collider2D&>(v).sensor=n;},{"Contato"}},
}};
inline constexpr std::array<ComponentEnumOption,3> Collider2DOptions{{{0,"Caixa"},{1,"Círculo"},{2,"Cápsula Y"}}};
inline constexpr std::array<ComponentEnum,1> Collider2DEnums{{{"shape","Forma",Collider2DOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Collider2D&>(v).shape);},[](ComponentValue &v,u32 n){static_cast<Collider2D&>(v).shape=static_cast<Collider2DShape>(n);},{"Forma"}}}};
inline const ComponentType Collider2D::descriptor{"astra.physics2d.collider",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Collider2D>();},Collider2DNumbers,Collider2DBooleans,Collider2DEnums,nullptr,true};

class ConstantForce2D final : public ComponentValue {
public:
 bool enabled=true;float forceX=0,forceY=0,relativeForceX=0,relativeForceY=0,torque=0;
 static const ComponentType descriptor;
 const ComponentType &type() const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<ConstantForce2D>(*this);}
 bool valid() const override{for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream &o)const override{o<<enabled;for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream &i,u32 v)override{if(v!=1||!(i>>enabled))return false;for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
inline constexpr std::array<ComponentNumber,5> force2DNumbers{{
#define AE_FORCE2D(id,label,field,unit) {label,-1000000,1000000,.1f,[](const ComponentValue &v)->const float&{return static_cast<const ConstantForce2D&>(v).field;},[](ComponentValue &v){return &static_cast<ConstantForce2D&>(v).field;},id,{"Propulsão",unit}}
 AE_FORCE2D("force_x","Força X",forceX,"N"),AE_FORCE2D("force_y","Força Y",forceY,"N"),
 AE_FORCE2D("relative_force_x","Força local X",relativeForceX,"N"),AE_FORCE2D("relative_force_y","Força local Y",relativeForceY,"N"),AE_FORCE2D("torque","Torque",torque,"N m")
#undef AE_FORCE2D
}};
inline constexpr std::array<ComponentBoolean,1> force2DBooleans{{{"enabled","Ativo",[](const ComponentValue &v){return static_cast<const ConstantForce2D&>(v).enabled;},[](ComponentValue &v,bool n){static_cast<ConstantForce2D&>(v).enabled=n;},{"Propulsão"}}}};
inline const ComponentType ConstantForce2D::descriptor{"astra.physics2d.constant-force",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ConstantForce2D>();},force2DNumbers,force2DBooleans};
enum class Joint2DKind:u32{Weld,Revolute,Prismatic,Distance};
class Joint2D final:public ComponentValue {
public:
 Joint2DKind kind=Joint2DKind::Weld;u64 target=0;
 bool enabled=true,worldAnchor=false,collideConnected=false,limitEnabled=false,motorEnabled=false,springEnabled=false;
 float anchorAX=0,anchorAY=0,anchorBX=0,anchorBY=0,referenceAngleDegrees=0,axisAngleDegrees=0;
 float length=1,minLength=.005f,maxLength=100,lowerLimit=-1,upperLimit=1;
 float motorSpeed=1,motorAngularSpeedDegrees=90,maxMotorForce=100,maxMotorTorque=100;
 float springHertz=5,springDamping=.7f,springTargetTranslation=0,springTargetAngleDegrees=0;
 float linearHertz=0,angularHertz=0,linearDampingRatio=1,angularDampingRatio=1;
 static const ComponentType descriptor;
 const ComponentType &type()const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone()const override{return std::make_unique<Joint2D>(*this);}
 bool valid()const override{if(static_cast<u32>(kind)>3||target>std::numeric_limits<u32>::max())return false;for(const auto &p:descriptor.numbers){float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return !limitEnabled||((kind!=Joint2DKind::Distance||minLength<=maxLength)&&((kind!=Joint2DKind::Revolute&&kind!=Joint2DKind::Prismatic)||lowerLimit<=upperLimit));}
 void write(std::ostream &o)const override{o<<static_cast<u32>(kind)<<' '<<target<<' '<<enabled<<' '<<worldAnchor<<' '<<collideConnected<<' '<<limitEnabled<<' '<<motorEnabled<<' '<<springEnabled;for(const auto &p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream &i,u32 v)override{u32 n;if(v!=1||!(i>>n>>target>>enabled>>worldAnchor>>collideConnected>>limitEnabled>>motorEnabled>>springEnabled)||n>3)return false;kind=static_cast<Joint2DKind>(n);for(const auto &p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
inline constexpr std::array<ComponentNumber,23> joint2DNumbers{{
#define AE_J2D(id,label,field,lo,hi,group,unit,show) {label,lo,hi,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Joint2D&>(v).field;},[](ComponentValue &v){return &static_cast<Joint2D&>(v).field;},id,{group,unit,nullptr,[](const ComponentValue &v){const auto &c=static_cast<const Joint2D&>(v);(void)c;return show;}}}
 AE_J2D("anchor_a_x","Anchor A X",anchorAX,-1000,1000,"Anchors","m",true),
 AE_J2D("anchor_a_y","Anchor A Y",anchorAY,-1000,1000,"Anchors","m",true),
 AE_J2D("anchor_b_x","Anchor B X",anchorBX,-1000,1000,"Anchors","m",true),
 AE_J2D("anchor_b_y","Anchor B Y",anchorBY,-1000,1000,"Anchors","m",true),
 AE_J2D("reference_angle_degrees","Ângulo referência",referenceAngleDegrees,-180,180,"Ajustes","graus",c.kind!=Joint2DKind::Distance),
 AE_J2D("axis_angle_degrees","Eixo local A",axisAngleDegrees,-180,180,"Ajustes","graus",c.kind==Joint2DKind::Prismatic),
 AE_J2D("length","Comprimento repouso",length,.005f,1000,"Ajustes","m",c.kind==Joint2DKind::Distance),
 AE_J2D("min_length","Comprimento mínimo",minLength,.005f,1000,"Ajustes","m",c.kind==Joint2DKind::Distance&&c.springEnabled&&c.limitEnabled),
 AE_J2D("max_length","Comprimento máximo",maxLength,.005f,1000,"Ajustes","m",c.kind==Joint2DKind::Distance&&c.springEnabled&&c.limitEnabled),
 AE_J2D("lower_limit","Limite inferior",lowerLimit,-178,178,"Ajustes","",(c.kind==Joint2DKind::Revolute||c.kind==Joint2DKind::Prismatic)&&c.limitEnabled),
 AE_J2D("upper_limit","Limite superior",upperLimit,-178,178,"Ajustes","",(c.kind==Joint2DKind::Revolute||c.kind==Joint2DKind::Prismatic)&&c.limitEnabled),
 AE_J2D("motor_speed","Velocidade motor",motorSpeed,-1000,1000,"Ajustes","m/s",(c.kind==Joint2DKind::Prismatic||(c.kind==Joint2DKind::Distance&&c.springEnabled))&&c.motorEnabled),
 AE_J2D("motor_angular_speed_degrees","Motor angular",motorAngularSpeedDegrees,-36000,36000,"Ajustes","graus/s",c.kind==Joint2DKind::Revolute&&c.motorEnabled),
 AE_J2D("max_motor_force","Força máxima motor",maxMotorForce,0,1000000,"Ajustes","N",(c.kind==Joint2DKind::Prismatic||(c.kind==Joint2DKind::Distance&&c.springEnabled))&&c.motorEnabled),
 AE_J2D("max_motor_torque","Torque máximo motor",maxMotorTorque,0,1000000,"Ajustes","N m",c.kind==Joint2DKind::Revolute&&c.motorEnabled),
 AE_J2D("spring_hertz","Frequência mola",springHertz,0,120,"Ajustes","Hz",c.kind!=Joint2DKind::Weld&&c.springEnabled),
 AE_J2D("spring_damping","Razão amortecimento",springDamping,0,10,"Ajustes","",c.kind!=Joint2DKind::Weld&&c.springEnabled&&c.springHertz>0),
 AE_J2D("spring_target_translation","Translação alvo",springTargetTranslation,-1000,1000,"Ajustes","m",c.kind==Joint2DKind::Prismatic&&c.springEnabled&&c.springHertz>0),
 AE_J2D("spring_target_angle_degrees","Ângulo alvo",springTargetAngleDegrees,-180,180,"Ajustes","graus",c.kind==Joint2DKind::Revolute&&c.springEnabled&&c.springHertz>0),
 AE_J2D("linear_hertz","Mola linear weld",linearHertz,0,120,"Ajustes","Hz",c.kind==Joint2DKind::Weld),
 AE_J2D("angular_hertz","Mola angular weld",angularHertz,0,120,"Ajustes","Hz",c.kind==Joint2DKind::Weld),
 AE_J2D("linear_damping_ratio","Amortecimento linear weld",linearDampingRatio,0,10,"Ajustes","",c.kind==Joint2DKind::Weld&&c.linearHertz>0),
 AE_J2D("angular_damping_ratio","Amortecimento angular weld",angularDampingRatio,0,10,"Ajustes","",c.kind==Joint2DKind::Weld&&c.angularHertz>0)
#undef AE_J2D
}};
inline constexpr std::array<ComponentBoolean,6> joint2DBooleans{{
#define AE_JBOOL(id,label,field,group,show) {id,label,[](const ComponentValue &v){return static_cast<const Joint2D&>(v).field;},[](ComponentValue &v,bool n){static_cast<Joint2D&>(v).field=n;},{group,"",nullptr,[](const ComponentValue &v){const auto &c=static_cast<const Joint2D&>(v);(void)c;return show;}}}
 AE_JBOOL("enabled","Ativo",enabled,"Conexão",true),AE_JBOOL("world_anchor","Conectar ao mundo",worldAnchor,"Conexão",true),AE_JBOOL("collide_connected","Colidir conectados",collideConnected,"Conexão",true),
 AE_JBOOL("limit_enabled","Limites",limitEnabled,"Ajustes",c.kind==Joint2DKind::Revolute||c.kind==Joint2DKind::Prismatic||(c.kind==Joint2DKind::Distance&&c.springEnabled)),
 AE_JBOOL("motor_enabled","Motor",motorEnabled,"Ajustes",c.kind==Joint2DKind::Revolute||c.kind==Joint2DKind::Prismatic||(c.kind==Joint2DKind::Distance&&c.springEnabled)),
 AE_JBOOL("spring_enabled","Mola",springEnabled,"Ajustes",c.kind!=Joint2DKind::Weld)
#undef AE_JBOOL
}};
inline constexpr std::array<ComponentEnumOption,4> joint2DOptions{{{0,"Fixed / Weld"},{1,"Revolute"},{2,"Prismatic"},{3,"Distance"}}};
inline constexpr std::array<ComponentEnum,1> joint2DEnums{{{"kind","Modo",joint2DOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Joint2D&>(v).kind);},[](ComponentValue &v,u32 n){static_cast<Joint2D&>(v).kind=static_cast<Joint2DKind>(n);},{"Conexão"}}}};
inline constexpr std::array<ComponentObjectReference,1> joint2DReferences{{{"target","Corpo conectado","astra.physics2d.body",ObjectReferenceScope::Other,"Escolher Body2D",[](const ComponentValue &v){return static_cast<const Joint2D&>(v).target;},[](ComponentValue &v,u64 id){static_cast<Joint2D&>(v).target=id;},{"Conexão","",nullptr,[](const ComponentValue &v){return !static_cast<const Joint2D&>(v).worldAnchor;}},[](const ComponentValue &v){const auto &c=static_cast<const Joint2D&>(v);return c.enabled&&!c.worldAnchor;}}}};
inline const ComponentType Joint2D::descriptor{"astra.physics2d.joint",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Joint2D>();},joint2DNumbers,joint2DBooleans,joint2DEnums,nullptr,true,joint2DReferences};
}
