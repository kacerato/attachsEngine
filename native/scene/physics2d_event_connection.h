#pragma once
#include "scene/components.h"
#include <array>
#include <limits>

namespace ae::scene {
class PhysicsEventConnection2D final : public ComponentValue {
public:
  bool enabled=true;
  u32 event=0,action=0; // trigger enter/stay/exit, contact enter/stay/exit; disconnected/activate/deactivate/toggle
  u64 receiver=0,otherFilter=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PhysicsEventConnection2D>(*this);}
  bool valid() const override {return event<=5&&action<=3&&receiver<=std::numeric_limits<u32>::max()&&otherFilter<=std::numeric_limits<u32>::max();}
  void write(std::ostream &out) const override {out<<enabled<<' '<<event<<' '<<action<<' '<<receiver<<' '<<otherFilter;}
  bool read(std::istream &in,u32 version) override {return version==1&&static_cast<bool>(in>>enabled>>event>>action>>receiver>>otherFilter)&&valid();}
};
inline constexpr std::array<ComponentEnumOption,6> physics2DConnectionEvents{{
  {0,"Entrada sensor"},{1,"Perm. sensor"},{2,"Saída sensor"},
  {3,"Entrada contato"},{4,"Perm. contato"},{5,"Saída contato"}
}};
inline constexpr std::array<ComponentEnumOption,4> physics2DConnectionActions{{
  {0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"}
}};
inline constexpr std::array<ComponentBoolean,1> physics2DConnectionBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).enabled;},
   [](ComponentValue &v,bool on){static_cast<PhysicsEventConnection2D&>(v).enabled=on;},{"Conexão"}}
}};
inline constexpr std::array<ComponentEnum,2> physics2DConnectionEnums{{
  {"event","Evento",physics2DConnectionEvents,[](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).event;},
   [](ComponentValue &v,u32 value){static_cast<PhysicsEventConnection2D&>(v).event=value;},
   {"Conexão","","Sensor exige um Colisor 2D sensor; contato exige uma forma sólida no emissor"}},
  {"action","Ação",physics2DConnectionActions,[](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).action;},
   [](ComponentValue &v,u32 value){static_cast<PhysicsEventConnection2D&>(v).action=value;},
   {"Conexão","","Executada antes do callback C#; permanência pode disparar a cada passo físico"}}
}};
inline constexpr std::array<ComponentObjectReference,2> physics2DConnectionReferences{{
  {"receiver","Receptor","",ObjectReferenceScope::Any,"Escolher objeto",
   [](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).receiver;},
   [](ComponentValue &v,u64 value){static_cast<PhysicsEventConnection2D&>(v).receiver=value;},
   {"Conexão","","Objeto cuja ativação será alterada",[](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).action!=0;}},
   [](const ComponentValue &v){const auto &c=static_cast<const PhysicsEventConnection2D&>(v);return c.enabled&&c.action!=0;}},
  {"other_filter","Outro objeto","",ObjectReferenceScope::Any,"Qualquer objeto",
   [](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).otherFilter;},
   [](ComponentValue &v,u64 value){static_cast<PhysicsEventConnection2D&>(v).otherFilter=value;},
   {"Filtro","","Opcional: somente eventos com este outro objeto; vazio aceita todos",[](const ComponentValue &v){return static_cast<const PhysicsEventConnection2D&>(v).action!=0;}}}
}};
inline const ComponentType PhysicsEventConnection2D::descriptor{
  "astra.physics2d.event_connection",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PhysicsEventConnection2D>();},
  {},physics2DConnectionBooleans,physics2DConnectionEnums,nullptr,true,physics2DConnectionReferences
};
}
