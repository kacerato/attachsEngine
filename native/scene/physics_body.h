#pragma once
#include "scene/collider.h"
namespace ae::scene {
enum class BodyMotion : u32 { Static=0, Kinematic=1, Dynamic=2 };
class PhysicsBody final : public ComponentValue {
public:
  BodyMotion motion=BodyMotion::Static;
  float mass=1,friction=.5f,restitution=0;
  float velocityX=0,velocityY=0,velocityZ=0;
  float angularX=0,angularY=0,angularZ=0,linearDamping=.05f,angularDamping=.05f,gravityFactor=1;
  bool sensor=false,allowSleep=true;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PhysicsBody>(*this);}
  bool valid() const override {
    if(static_cast<u32>(motion)>2) return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {out<<static_cast<u32>(motion);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);out<<' '<<sensor<<' '<<allowSleep;}
  bool read(std::istream &in,u32 version) override {
    u32 mode=0;if((version!=2&&version!=3) || !(in>>mode) || mode>2) return false;
    motion=static_cast<BodyMotion>(mode);
    for(usize i=0;i<(version==2?6:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=3 && !(in>>sensor>>allowSleep)) return false;
    return true;
  }
};
// Os grupos seguem a leitura do Rigidbody da Unity: o que descreve o corpo, o
// que freia o corpo e o que só vale no instante em que o Play começa. O
// inspetor deriva as abas desta declaração — não há uma segunda lista de
// seções na interface.
inline bool bodySimulates(const ComponentValue &v) {return static_cast<const PhysicsBody&>(v).motion!=BodyMotion::Static;}
inline bool bodyIsDynamic(const ComponentValue &v) {return static_cast<const PhysicsBody&>(v).motion==BodyMotion::Dynamic;}
inline constexpr std::array<ComponentNumber,12> physicsBodyNumbers{{
#define AE_PHYS_NUMBER(id,label,field,lo,hi,group,unit,visible) {label,lo,hi,.1f,[](const ComponentValue &v)->const float&{return static_cast<const PhysicsBody&>(v).field;},[](ComponentValue &v)->float*{return &static_cast<PhysicsBody&>(v).field;},id,{group,unit,nullptr,visible}}
  AE_PHYS_NUMBER("mass","Massa kg",mass,.01f,1000000,"Corpo","kg",bodyIsDynamic),
  AE_PHYS_NUMBER("friction","Atrito",friction,0,1,"Corpo","",nullptr),
  AE_PHYS_NUMBER("restitution","Restituição",restitution,0,1,"Corpo","",nullptr),
  AE_PHYS_NUMBER("velocity_x","Velocidade inicial X",velocityX,-1000,1000,"Início","m/s",bodySimulates),
  AE_PHYS_NUMBER("velocity_y","Velocidade inicial Y",velocityY,-1000,1000,"Início","m/s",bodySimulates),
  AE_PHYS_NUMBER("velocity_z","Velocidade inicial Z",velocityZ,-1000,1000,"Início","m/s",bodySimulates),
  AE_PHYS_NUMBER("angular_x","Giro inicial X · rad/s",angularX,-1000,1000,"Início","rad/s",bodySimulates),
  AE_PHYS_NUMBER("angular_y","Giro inicial Y · rad/s",angularY,-1000,1000,"Início","rad/s",bodySimulates),
  AE_PHYS_NUMBER("angular_z","Giro inicial Z · rad/s",angularZ,-1000,1000,"Início","rad/s",bodySimulates),
  AE_PHYS_NUMBER("linear_damping","Amortecimento linear",linearDamping,0,10,"Amortecimento","",bodyIsDynamic),
  AE_PHYS_NUMBER("angular_damping","Amortecimento angular",angularDamping,0,10,"Amortecimento","",bodyIsDynamic),
  AE_PHYS_NUMBER("gravity_factor","Multiplicador da gravidade",gravityFactor,-100,100,"Amortecimento","",bodyIsDynamic)
#undef AE_PHYS_NUMBER
}};
inline constexpr std::array<ComponentTriple,2> physicsBodyTriples{{
  {"velocity","Velocidade inicial",{"velocity_x","velocity_y","velocity_z"}},
  {"angular_velocity","Giro inicial",{"angular_x","angular_y","angular_z"}}
}};
inline constexpr std::array<ComponentEnumOption,3> bodyMotionOptions{{{0,"Estático"},{1,"Cinemático"},{2,"Dinâmico"}}};
inline constexpr std::array<ComponentEnum,1> physicsBodyEnums{{
  {"motion","Movimento",bodyMotionOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const PhysicsBody&>(v).motion);},
    [](ComponentValue &v,u32 value){static_cast<PhysicsBody&>(v).motion=static_cast<BodyMotion>(value);},{"Corpo"}}
}};
inline bool migratePhysicsBody(std::istream &in,u32 version,Components &components) {
  // V1 stored an implicit box in the body; never replace an explicit collider.
  if(version==2) {PhysicsBody body;if(!body.read(in,2)||!body.valid()) return false;return components.edit(PhysicsBody::descriptor)&&components.replace(body);}
  if(version!=1 || components.find(Collider::descriptor)) return false;
  u32 dynamic=0;PhysicsBody body;Collider collider;
  if(!(in>>dynamic>>body.mass>>collider.halfX>>collider.halfY>>collider.halfZ>>body.friction>>body.restitution) || dynamic>1) return false;
  body.motion=dynamic?BodyMotion::Dynamic:BodyMotion::Static;
  if(!body.valid() || !collider.valid()) return false;
  return components.edit(PhysicsBody::descriptor) && components.replace(body) &&
         components.edit(Collider::descriptor) && components.replace(collider);
}
inline constexpr std::array<ComponentBoolean,2> physicsBodyBooleans{{
  {"sensor","Sensor sem resposta",[](const ComponentValue &v){return static_cast<const PhysicsBody&>(v).sensor;},[](ComponentValue &v,bool b){static_cast<PhysicsBody&>(v).sensor=b;},{"Corpo"}},
  {"allow_sleep","Permitir repouso",[](const ComponentValue &v){return static_cast<const PhysicsBody&>(v).allowSleep;},[](ComponentValue &v,bool b){static_cast<PhysicsBody&>(v).allowSleep=b;},{"Corpo","",nullptr,bodySimulates}}
}};
inline const ComponentType PhysicsBody::descriptor{
  "astra.physics.body",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PhysicsBody>();},
  physicsBodyNumbers,physicsBodyBooleans,physicsBodyEnums,migratePhysicsBody,false,{},physicsBodyTriples
};
}
