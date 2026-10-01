#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>

namespace ae::scene {
// Tempo autorado. A contagem pertence ao mundo de Play e nunca é serializada.
// Várias instâncias no mesmo objeto são distinguidas por instanceId().
class Timer final : public ComponentValue {
public:
  float intervalSeconds=1.0f;
  bool repeat=true;
  bool enabled=true;
  bool ignoreTimeScale=false;
  bool autoStart=true; // Compatibility default: v1-v3 began when enabled.
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
inline constexpr std::array<ComponentNumber,1> timerNumbers{{
  {"Intervalo",0.05f,3600.0f,0.05f,
   [](const ComponentValue &value)->const float &{return static_cast<const Timer &>(value).intervalSeconds;},
   [](ComponentValue &value)->float *{return &static_cast<Timer &>(value).intervalSeconds;},
   "interval_seconds",{"Disparo","s","Tempo entre disparos; uma vez quando Repetir está desligado"}}
}};
inline constexpr std::array<ComponentBoolean,4> timerBooleans{{
  {"auto_start","Iniciar automaticamente",[](const ComponentValue &value){return static_cast<const Timer &>(value).autoStart;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).autoStart=on;},{"Disparo","","Inicia ao entrar em Play ou criar a instância; desligado exige Start. Alteração não reinicia timer já criado"}},
  {"repeat","Repetir",[](const ComponentValue &value){return static_cast<const Timer &>(value).repeat;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).repeat=on;},{"Disparo"}},
  {"enabled","Ativo",[](const ComponentValue &value){return static_cast<const Timer &>(value).enabled;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).enabled=on;},{"Disparo"}},
  {"ignore_time_scale","Ignorar escala de tempo",[](const ComponentValue &value){return static_cast<const Timer &>(value).ignoreTimeScale;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).ignoreTimeScale=on;},{"Disparo","","Usa o intervalo não escalado aceito do quadro; a pausa do editor interrompe ambos os relógios"}}
}};
inline constexpr std::array<ComponentEnumOption,4> timerActions{{
  {0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"}
}};
inline constexpr std::array<ComponentEnum,1> timerEnums{{
  {"elapsed_action","Ao disparar",timerActions,
   [](const ComponentValue &v){return static_cast<const Timer &>(v).elapsedAction;},
   [](ComponentValue &v,u32 action){static_cast<Timer &>(v).elapsedAction=action;},
   {"Conexão","","Executa uma ação no alvo antes de entregar TimerElapsed; alternar preserva a paridade dos disparos agregados",nullptr,nullptr,{},"runtime/scene_timers.h -> GameWorld::setActive"}}
}};
inline constexpr std::array<ComponentObjectReference,1> timerReferences{{
  {"elapsed_target","Receptor","",ObjectReferenceScope::Any,"Escolher objeto",
   [](const ComponentValue &v){return static_cast<const Timer &>(v).elapsedTarget;},
   [](ComponentValue &v,u64 target){static_cast<Timer &>(v).elapsedTarget=target;},
   {"Conexão","","Alvo da ação; usa referência persistente remapeada por clone/prefab",
    [](const ComponentValue &v){return static_cast<const Timer &>(v).elapsedAction!=0;},nullptr,{},"runtime/scene_timers.h -> GameWorld::setActive"},
   [](const ComponentValue &v){const auto &timer=static_cast<const Timer &>(v);return timer.enabled && timer.elapsedAction!=0;}}
}};
inline const ComponentType Timer::descriptor{
  "astra.time.timer",4,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Timer>();},
  timerNumbers,timerBooleans,timerEnums,nullptr,true,timerReferences
};
}
