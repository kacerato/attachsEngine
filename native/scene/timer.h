#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>

namespace ae::scene {
// Tempo autorado. A contagem pertence ao mundo de Play e nunca é serializada.
// Várias instâncias no mesmo objeto são distinguidas por instanceId().
class Timer final : public ComponentValue {
public:

#include "scene/generated/timer_Timer_fields0.inc"

#include "scene/generated/timer_Timer_fields1.inc"

#include "scene/generated/timer_Timer_fields2.inc"

#include "scene/generated/timer_Timer_fields3.inc"

#include "scene/generated/timer_Timer_fields4.inc"
 // Compatibility default: v1-v3 began when enabled.
  // A typed persistent timeout connection. Runtime changes activeSelf through GameWorld.
  u32 elapsedAction=0; // 0 disconnected, 1 activate, 2 deactivate, 3 toggle
  u64 elapsedTarget=0;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Timer>(*this);}
  bool valid() const override {return elapsedAction<=3 && elapsedTarget<=std::numeric_limits<u32>::max() && std::isfinite(intervalSeconds)&&intervalSeconds>=0.05f&&intervalSeconds<=3600.0f;}
  void write(std::ostream &out) const override {out<<intervalSeconds<<' '<<repeat<<' '<<enabled<<' '<<ignoreTimeScale<<' '<<elapsedAction<<' '<<elapsedTarget<<' '<<autoStart;}
  bool read(std::istream &in,u32 version) override {
    if((version<1 || version>4) || !(in>>intervalSeconds>>repeat>>enabled)) return false;
    ignoreTimeScale=false;
    if(version>=2 && !(in>>ignoreTimeScale)) return false;
    elapsedAction=0;elapsedTarget=0;
    if(version>=3 && !(in>>elapsedAction>>elapsedTarget)) return false;
    autoStart=true;
    if(version>=4 && !(in>>autoStart)) return false;
    return valid();
  }
};
#include "scene/generated/timer_timerNumbers.inc"
#include "scene/generated/timer_timerBooleans.inc"
inline constexpr std::array<ComponentEnumOption,4> timerActions{{
  {0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"}
}};
#include "scene/generated/timer_timerEnums.inc"
inline constexpr std::array<ComponentObjectReference,1> timerReferences{{
  {"elapsed_target","Receptor","",ObjectReferenceScope::Any,"Escolher objeto",
   [](const ComponentValue &v){return static_cast<const Timer &>(v).elapsedTarget;},
   [](ComponentValue &v,u64 target){static_cast<Timer &>(v).elapsedTarget=target;},
   {"Conexão","","Alvo da ação; usa referência persistente remapeada por clone/prefab",
    [](const ComponentValue &v){return static_cast<const Timer &>(v).elapsedAction!=0;},nullptr,{},"runtime/scene_timers.h -> GameWorld::setActive"},
   [](const ComponentValue &v){const auto &timer=static_cast<const Timer &>(v);return timer.enabled && timer.elapsedAction!=0;}}
}};
// Operações sobre o agendador de Play (runtime/scene_timers.h). Intervalo zero
// em start mantém o valor autorado; positivo altera só o intervalo em execução.
inline constexpr std::array<ComponentParameter,1> timerStartParameters{{
  {"interval","Intervalo",ComponentValueKind::Number,"s"}
}};
inline constexpr std::array<ComponentParameter,1> timerElapsedPayload{{
  {"count","Disparos",ComponentValueKind::Integer}
}};
inline constexpr std::array<ComponentMethod,6> timerMethods{{
  {"start","Iniciar","Reinicia a contagem; intervalo zero usa o autorado",timerStartParameters},
  {"stop","Parar","Interrompe e zera a contagem"},
  {"pause","Pausar","Congela a contagem sem perder o restante"},
  {"resume","Retomar","Continua a contagem pausada"},
  {"remaining","Restante","Segundos até o próximo disparo; zero quando parado",{},ComponentValueKind::Number},
  {"running","Em execução","Verdadeiro enquanto conta, inclusive pausado",{},ComponentValueKind::Boolean},
}};
inline constexpr std::array<ComponentEvent,1> timerEvents{{
  {"elapsed","Disparou","Um evento por quadro; Disparos conta intervalos vencidos no quadro",timerElapsedPayload}
}};
inline const ComponentType Timer::descriptor{
  "astra.time.timer",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Timer>();},
  timerNumbers,timerBooleans,timerEnums,nullptr,true,timerReferences,{},{},{},{},{},timerMethods,timerEvents
};
}
