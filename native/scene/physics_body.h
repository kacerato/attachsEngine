#pragma once
#include "scene/collider.h"
namespace ae::scene {
enum class BodyMotion : u32 { Static=0, Kinematic=1, Dynamic=2 };
class PhysicsBody final : public ComponentValue {
public:
  BodyMotion motion=BodyMotion::Static;

#include "scene/generated/physics_body_PhysicsBody_fields0.inc"

#include "scene/generated/physics_body_PhysicsBody_fields1.inc"

#include "scene/generated/physics_body_PhysicsBody_fields2.inc"

#include "scene/generated/physics_body_PhysicsBody_fields3.inc"

  bool freezePosition[3]{},freezeRotation[3]{},continuousCollision=false;
  // Material físico: GUID do recurso compartilhado e combinação por par. Atrito
  // e restituição acima são a cópia local, usada também quando o recurso falta.
  u32 frictionCombine=0,restitutionCombine=0;
  resources::AssetGuid material{};

#include "scene/generated/physics_body_PhysicsBody_fields5.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PhysicsBody>(*this);}
  bool valid() const override {
    if(static_cast<u32>(motion)>2||frictionCombine>4||restitutionCombine>4) return false;
    if(motion==BodyMotion::Dynamic&&freezePosition[0]&&freezePosition[1]&&freezePosition[2]&&freezeRotation[0]&&freezeRotation[1]&&freezeRotation[2])return false;
    if(solverVelocitySteps!=std::floor(solverVelocitySteps))return false;
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return true;
  }
  void write(std::ostream &out) const override {out<<static_cast<u32>(motion);for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);out<<' '<<sensor<<' '<<allowSleep;for(bool n:freezePosition)out<<' '<<n;for(bool n:freezeRotation)out<<' '<<n;out<<' '<<continuousCollision;
    out<<' '<<frictionCombine<<' '<<restitutionCombine<<' '<<(material.valid()?material.text():"-");}
  bool read(std::istream &in,u32 version) override {
    u32 mode=0;if((version<2||version>5) || !(in>>mode) || mode>2) return false;
    motion=static_cast<BodyMotion>(mode);
    for(usize i=0;i<(version==2?6:version==3?12:descriptor.numbers.size());++i) if(!(in>>*descriptor.numbers[i].write(*this))) return false;
    if(version>=3 && !(in>>sensor>>allowSleep)) return false;
    if(version>=4){for(auto &n:freezePosition)if(!(in>>n))return false;for(auto &n:freezeRotation)if(!(in>>n))return false;if(!(in>>continuousCollision))return false;}
    frictionCombine=restitutionCombine=0;material={};
    if(version>=5) {
      std::string guid;if(!(in>>frictionCombine>>restitutionCombine>>guid)) return false;
      if(guid!="-"&&!resources::AssetGuid::parse(guid,material)) return false;
    }
    return true;
  }
};
// Os grupos seguem a leitura do Rigidbody da Unity: o que descreve o corpo, o
// que freia o corpo e o que só vale no instante em que o Play começa. O
// inspetor deriva as abas desta declaração — não há uma segunda lista de
// seções na interface.
inline bool bodySimulates(const ComponentValue &v) {return static_cast<const PhysicsBody&>(v).motion!=BodyMotion::Static;}
inline bool bodyIsDynamic(const ComponentValue &v) {return static_cast<const PhysicsBody&>(v).motion==BodyMotion::Dynamic;}
#include "scene/generated/physics_body_physicsBodyNumbers.inc"
inline constexpr std::array<ComponentTriple,2> physicsBodyTriples{{
  {"velocity","Velocidade inicial",{"velocity_x","velocity_y","velocity_z"}},
  {"angular_velocity","Giro inicial",{"angular_x","angular_y","angular_z"}}
}};
inline constexpr std::array<ComponentEnumOption,3> bodyMotionOptions{{{0,"Estático"},{1,"Cinemático"},{2,"Dinâmico"}}};
inline constexpr std::array<ComponentEnumOption,5> physicsCombineOptions{{{0,"Padrão do motor"},{1,"Média"},{2,"Mínimo"},{3,"Multiplicar"},{4,"Máximo"}}};
#include "scene/generated/physics_body_physicsBodyEnums.inc"
inline bool migratePhysicsBody(std::istream &in,u32 version,Components &components) {
  // V1 stored an implicit box in the body; never replace an explicit collider.
  if(version==2||version==3) {PhysicsBody body;if(!body.read(in,version)||!body.valid()) return false;return components.edit(PhysicsBody::descriptor)&&components.replace(body);}
  if(version!=1 || components.find(Collider::descriptor)) return false;
  u32 dynamic=0;PhysicsBody body;Collider collider;
  if(!(in>>dynamic>>body.mass>>collider.halfX>>collider.halfY>>collider.halfZ>>body.friction>>body.restitution) || dynamic>1) return false;
  body.motion=dynamic?BodyMotion::Dynamic:BodyMotion::Static;
  if(!body.valid() || !collider.valid()) return false;
  return components.edit(PhysicsBody::descriptor) && components.replace(body) &&
         components.edit(Collider::descriptor) && components.replace(collider);
}
#include "scene/generated/physics_body_physicsBodyBooleans.inc"
inline const std::array<ComponentResourceBinding,1> physicsBodyResources{{
  {"material","Material físico",resources::AssetType::PhysicsMaterial,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return static_cast<const PhysicsBody&>(v).material;},
   [](ComponentValue &v,u32 slot,resources::AssetGuid g){if(slot) return false;static_cast<PhysicsBody&>(v).material=g;return true;},
   {"Material","","Recurso compartilhado; escolher copia atrito, restituição e combinação para este corpo"}}
}};
inline const ComponentType PhysicsBody::descriptor{
  "astra.physics.body",5,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PhysicsBody>();},
  physicsBodyNumbers,physicsBodyBooleans,physicsBodyEnums,migratePhysicsBody,false,{},physicsBodyTriples,physicsBodyResources
};
}
