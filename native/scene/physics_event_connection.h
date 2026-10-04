#pragma once
#include "scene/components.h"
#include <array>
#include <limits>

namespace ae::scene {
class PhysicsEventConnection3D final : public ComponentValue {
public:

#include "scene/generated/physics_event_connection_PhysicsEventConnection3D_fields0.inc"

  u32 event=0,action=0; // trigger enter/stay/exit, contact enter/stay/exit; disconnected/activate/deactivate/toggle
  u64 receiver=0,otherFilter=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PhysicsEventConnection3D>(*this);}
  bool valid() const override {return event<=5&&action<=3&&receiver<=std::numeric_limits<u32>::max()&&otherFilter<=std::numeric_limits<u32>::max();}
  void write(std::ostream &out) const override {out<<enabled<<' '<<event<<' '<<action<<' '<<receiver<<' '<<otherFilter;}
  bool read(std::istream &in,u32 version) override {return version==1&&static_cast<bool>(in>>enabled>>event>>action>>receiver>>otherFilter)&&valid();}
};
inline constexpr std::array<ComponentEnumOption,6> physicsConnectionEvents{{
  {0,"Entrada sensor"},{1,"Perm. sensor"},{2,"Saída sensor"},
  {3,"Entrada contato"},{4,"Perm. contato"},{5,"Saída contato"}
}};
inline constexpr std::array<ComponentEnumOption,4> physicsConnectionActions{{
  {0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"}
}};
#include "scene/generated/physics_event_connection_physicsConnectionBooleans.inc"
#include "scene/generated/physics_event_connection_physicsConnectionEnums.inc"
inline constexpr std::array<ComponentObjectReference,2> physicsConnectionReferences{{
  {"receiver","Receptor","",ObjectReferenceScope::Any,"Escolher objeto",
   [](const ComponentValue &v){return static_cast<const PhysicsEventConnection3D&>(v).receiver;},
   [](ComponentValue &v,u64 value){static_cast<PhysicsEventConnection3D&>(v).receiver=value;},
   {"Conexão","","Objeto cuja ativação será alterada",[](const ComponentValue &v){return static_cast<const PhysicsEventConnection3D&>(v).action!=0;}},
   [](const ComponentValue &v){const auto &c=static_cast<const PhysicsEventConnection3D&>(v);return c.enabled&&c.action!=0;}},
  {"other_filter","Outro objeto","",ObjectReferenceScope::Any,"Qualquer objeto",
   [](const ComponentValue &v){return static_cast<const PhysicsEventConnection3D&>(v).otherFilter;},
   [](ComponentValue &v,u64 value){static_cast<PhysicsEventConnection3D&>(v).otherFilter=value;},
   {"Filtro","","Opcional: somente eventos com este outro objeto; vazio aceita todos",[](const ComponentValue &v){return static_cast<const PhysicsEventConnection3D&>(v).action!=0;}}}
}};
inline const ComponentType PhysicsEventConnection3D::descriptor{
  "astra.physics.event_connection",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PhysicsEventConnection3D>();},
  {},physicsConnectionBooleans,physicsConnectionEnums,nullptr,true,physicsConnectionReferences
};
}
