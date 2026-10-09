// Conexão de evento: "quando ESTE objeto emitir o evento X, faça Y".
//
// Mora no objeto emissor, como o UnityEvent mora no componente que o dispara e
// as conexões Godot ficam salvas no nó de origem. O evento e o método vêm dos
// descritores declarados nos tipos (ComponentEvent/ComponentMethod); este
// arquivo só lhes dá uma identidade PERSISTENTE, porque o índice de um evento
// dentro do tipo não é contrato de arquivo.
//
// Valores das tabelas abaixo são gravados na cena: acrescentar no fim, nunca
// renumerar nem reutilizar. `auditEventConnectionCatalog` (runtime) exige que
// todo evento declarado e todo método acionável tenham uma linha aqui.
//
// Referências: Unity 6000.0 UnityEvent
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html
// e Godot 4.5 signals
// https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html
#pragma once
#include "scene/components.h"

#include <array>
#include <cmath>
#include <limits>
#include <string_view>

namespace ae::scene {

struct EventConnectionEventKey { u32 value; std::string_view type; std::string_view event; };
struct EventConnectionMethodKey { u32 value; std::string_view type; std::string_view method; };

inline constexpr std::array<EventConnectionEventKey,29> eventConnectionEventKeys{{
  {1,"astra.time.timer","elapsed"},
  {2,"astra.tween.transform","completed"},
  {3,"astra.physics.collider","trigger_enter"},
  {4,"astra.physics.collider","trigger_exit"},
  {5,"astra.physics.collider","collision_enter"},
  {6,"astra.physics.collider","collision_exit"},
  {7,"astra.physics2d.collider","trigger_enter"},
  {8,"astra.physics2d.collider","trigger_exit"},
  {9,"astra.physics2d.collider","collision_enter"},
  {10,"astra.physics2d.collider","collision_exit"},
  {11,"astra.tween.sequence","step_started"},
  {12,"astra.tween.sequence","completed"},
  {13,"astra.tween.property","completed"},
  {14,"astra.physics.joint","broken"},
  {15,"astra.physics.character","collider_hit"},
  {16,"astra.physics.collider","trigger_stay"},
  {17,"astra.camera.brain","camera_activated"},
  {18,"astra.camera.brain","camera_cut"},
  {19,"astra.camera.brain","blend_finished"},
  {20,"astra.camera.virtual","activated"},
  {21,"astra.camera.virtual","deactivated"},
  {22,"astra.animation.animator","state_entered"},
  {23,"astra.animation.animator","state_event"},
  {24,"astra.animation.animator","machine_entered"},{25,"astra.animation.animator","machine_exited"},{26,"astra.animation.animator","transition_interrupted"},
  {27,"astra.navigation.agent","destination_reached"},{28,"astra.navigation.agent","path_failed"},{29,"astra.navigation.agent","link_entered"},
}};
inline constexpr std::array<ComponentEnumOption,30> eventConnectionEvents{{
  {0,"Nenhum"},
  {1,"Timer disparou"},{2,"Tween concluiu"},
  {3,"Sensor 3D: entrou"},{4,"Sensor 3D: saiu"},{5,"Colisão 3D: começou"},{6,"Colisão 3D: terminou"},
  {7,"Sensor 2D: entrou"},{8,"Sensor 2D: saiu"},{9,"Colisão 2D: começou"},{10,"Colisão 2D: terminou"},
  {11,"Sequência: etapa começou"},{12,"Sequência concluiu"},
  {13,"Tween de propriedade concluiu"},
  {14,"Junta quebrou"},
  {15,"Personagem bateu num colisor"},
  {16,"Sensor 3D: dentro"},
  {17,"Cérebro: câmera ativada"},{18,"Cérebro: corte de câmera"},{19,"Cérebro: transição concluída"},
  {20,"Câmera virtual entrou ao vivo"},{21,"Câmera virtual saiu do ar"},
  {22,"Animator: entrou num estado"},{23,"Animator: evento do estado"},
  {24,"Animator: entrou num grupo"},{25,"Animator: saiu de um grupo"},{26,"Animator: mistura interrompida"},
  {27,"Agente: chegou"},{28,"Agente: caminho falhou"},{29,"Agente: entrou num link"},
}};
inline constexpr std::array<EventConnectionMethodKey,31> eventConnectionMethodKeys{{
  {1,"astra.audio.source","play"},
  {2,"astra.audio.source","stop"},
  {3,"astra.audio.source","pause"},
  {4,"astra.audio.source","resume"},
  {5,"astra.audio.source","seek"},
  {6,"astra.time.timer","start"},
  {7,"astra.time.timer","stop"},
  {8,"astra.time.timer","pause"},
  {9,"astra.time.timer","resume"},
  {10,"astra.tween.transform","restart"},
  {11,"astra.tween.transform","cancel"},
  {12,"astra.tween.transform","pause"},
  {13,"astra.tween.transform","resume"},
  {14,"astra.path.follow","restart"},
  {15,"astra.path.follow","stop"},
  {16,"astra.tween.sequence","play"},
  {17,"astra.tween.sequence","cancel"},
  {18,"astra.tween.sequence","pause"},
  {19,"astra.tween.sequence","resume"},
  {20,"astra.tween.property","restart"},
  {21,"astra.tween.property","cancel"},
  {22,"astra.tween.property","pause"},
  {23,"astra.tween.property","resume"},
  {24,"astra.physics.raycast","update"},
  {25,"astra.physics.shapecast","update"},
  {26,"astra.camera.virtual","prioritize"},
  {27,"astra.camera.virtual","snap"},
  {28,"astra.audio.snapshot","transition_to"},
  {29,"astra.audio.snapshot","apply"},
  {30,"astra.navigation.agent","stop"},
  {31,"astra.navigation.agent","resume"},
}};
inline constexpr std::array<ComponentEnumOption,32> eventConnectionMethods{{
  {0,"Nenhum"},
  {1,"Áudio: tocar"},{2,"Áudio: parar"},{3,"Áudio: pausar"},{4,"Áudio: retomar"},{5,"Áudio: posicionar"},
  {6,"Timer: iniciar"},{7,"Timer: parar"},{8,"Timer: pausar"},{9,"Timer: retomar"},
  {10,"Tween: reiniciar"},{11,"Tween: cancelar"},{12,"Tween: pausar"},{13,"Tween: retomar"},
  {14,"Percurso: reiniciar"},{15,"Percurso: parar"},
  {16,"Sequência: tocar"},{17,"Sequência: cancelar"},{18,"Sequência: pausar"},{19,"Sequência: retomar"},
  {20,"Tween de propriedade: reiniciar"},{21,"Tween de propriedade: cancelar"},{22,"Tween de propriedade: pausar"},{23,"Tween de propriedade: retomar"},
  {24,"Raio: atualizar agora"},{25,"Varredura: atualizar agora"},
  {26,"Câmera virtual: priorizar"},{27,"Câmera virtual: encaixar"},
  {28,"Snapshot: transicionar"},{29,"Snapshot: aplicar"},
  {30,"Agente: parar"},{31,"Agente: retomar"},
}};
inline constexpr std::array<ComponentEnumOption,5> eventConnectionActions{{
  {0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"},{4,"Chamar método"},
}};
inline constexpr u32 kEventConnectionCallMethod=4;

inline const EventConnectionEventKey *findEventConnectionEvent(u32 value) {
  for(const auto &key:eventConnectionEventKeys) if(key.value==value) return &key;
  return nullptr;
}
inline const EventConnectionEventKey *findEventConnectionEvent(std::string_view type,std::string_view event) {
  for(const auto &key:eventConnectionEventKeys) if(key.type==type && key.event==event) return &key;
  return nullptr;
}
inline const EventConnectionMethodKey *findEventConnectionMethod(u32 value) {
  for(const auto &key:eventConnectionMethodKeys) if(key.value==value) return &key;
  return nullptr;
}
// Eventos que carregam o outro objeto (contatos) aceitam filtro por ele.
inline bool eventConnectionCarriesObject(u32 event) {return (event>=3 && event<=10) || (event>=15 && event<=21);}
// Métodos com um número de argumento: timer.start (intervalo; zero usa o autorado)
// audio.seek (segundos) e snapshot.transition_to (duração; negativo usa a autorada).
inline bool eventConnectionTakesNumber(u32 method) {return method==5 || method==6 || method==28;}

class EventConnection final : public ComponentValue {
public:
  bool enabled=true,once=false;
  u32 event=0,action=0,method=0;
  float argument=0;
  u64 receiver=0,otherFilter=0;   // receptor zero: este próprio objeto
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<EventConnection>(*this);}
  bool valid() const override {
    return (event==0||findEventConnectionEvent(event)) && action<=kEventConnectionCallMethod &&
           (method==0||findEventConnectionMethod(method)) && std::isfinite(argument) && argument>=0 && argument<=3600 &&
           receiver<=std::numeric_limits<u32>::max() && otherFilter<=std::numeric_limits<u32>::max();
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<event<<' '<<action<<' '<<method<<' '<<argument<<' '<<receiver<<' '<<otherFilter<<' '<<once;
  }
  bool read(std::istream &in,u32 version) override {
    return version==1 && static_cast<bool>(in>>enabled>>event>>action>>method>>argument>>receiver>>otherFilter>>once) && valid();
  }
};
inline const EventConnection &eventConnection(const ComponentValue &v) {return static_cast<const EventConnection &>(v);}
inline EventConnection &eventConnection(ComponentValue &v) {return static_cast<EventConnection &>(v);}

inline constexpr std::array<ComponentBoolean,2> eventConnectionBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return eventConnection(v).enabled;},[](ComponentValue &v,bool b){eventConnection(v).enabled=b;},
   {"Conexão","","Desligada não reage a eventos; a configuração é preservada"}},
  {"once","Uma vez",[](const ComponentValue &v){return eventConnection(v).once;},[](ComponentValue &v,bool b){eventConnection(v).once=b;},
   {"Conexão","","Reage só ao primeiro evento de cada execução de Play",[](const ComponentValue &v){return eventConnection(v).action!=0;}}},
}};
inline constexpr std::array<ComponentEnum,3> eventConnectionEnums{{
  {"event","Evento",eventConnectionEvents,[](const ComponentValue &v){return eventConnection(v).event;},[](ComponentValue &v,u32 x){eventConnection(v).event=x;},
   {"Quando","","Emitido por um componente deste objeto; sem o componente, a conexão não dispara"}},
  {"action","Ação",eventConnectionActions,[](const ComponentValue &v){return eventConnection(v).action;},[](ComponentValue &v,u32 x){eventConnection(v).action=x;},
   {"Então","","Executada no ponto seguro seguinte ao evento, antes do próximo despacho de scripts"}},
  {"method","Método",eventConnectionMethods,[](const ComponentValue &v){return eventConnection(v).method;},[](ComponentValue &v,u32 x){eventConnection(v).method=x;},
   {"Então","","Chamado no primeiro componente do tipo correspondente no receptor",
    [](const ComponentValue &v){return eventConnection(v).action==kEventConnectionCallMethod;}}},
}};
inline constexpr std::array<ComponentNumber,1> eventConnectionNumbers{{
  {"Valor",0,3600,.05f,[](const ComponentValue &v)->const float &{return eventConnection(v).argument;},
   [](ComponentValue &v)->float *{return &eventConnection(v).argument;},"argument",
   {"Então","s","Timer: intervalo (zero usa o autorado). Áudio: posição do cursor",
    [](const ComponentValue &v){const auto &c=eventConnection(v);return c.action==kEventConnectionCallMethod&&eventConnectionTakesNumber(c.method);}}},
}};
inline constexpr std::array<ComponentObjectReference,2> eventConnectionReferences{{
  {"receiver","Receptor","",ObjectReferenceScope::Any,"Este objeto",
   [](const ComponentValue &v){return eventConnection(v).receiver;},
   [](ComponentValue &v,u64 x){eventConnection(v).receiver=x;},
   {"Então","","Objeto ativado ou dono do componente chamado; vazio usa o próprio emissor",
    [](const ComponentValue &v){return eventConnection(v).action!=0;}}},
  {"other_filter","Outro objeto","",ObjectReferenceScope::Any,"Qualquer objeto",
   [](const ComponentValue &v){return eventConnection(v).otherFilter;},
   [](ComponentValue &v,u64 x){eventConnection(v).otherFilter=x;},
   {"Quando","","Opcional: só contatos com este outro objeto",
    [](const ComponentValue &v){return eventConnectionCarriesObject(eventConnection(v).event);}}},
}};
inline const ComponentType EventConnection::descriptor{
  "astra.logic.event_connection",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<EventConnection>();},
  eventConnectionNumbers,eventConnectionBooleans,eventConnectionEnums,nullptr,true,eventConnectionReferences
};

} // namespace ae::scene
