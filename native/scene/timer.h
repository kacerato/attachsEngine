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
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<Timer>(*this);}
  bool valid() const override {return std::isfinite(intervalSeconds)&&intervalSeconds>=0.05f&&intervalSeconds<=3600.0f;}
  void write(std::ostream &out) const override {out<<intervalSeconds<<' '<<repeat<<' '<<enabled;}
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>intervalSeconds>>repeat>>enabled)) return false;
    return valid();
  }
};
inline constexpr std::array<ComponentNumber,1> timerNumbers{{
  {"Intervalo",0.05f,3600.0f,0.05f,
   [](const ComponentValue &value)->const float &{return static_cast<const Timer &>(value).intervalSeconds;},
   [](ComponentValue &value)->float *{return &static_cast<Timer &>(value).intervalSeconds;},
   "interval_seconds",{"Disparo","s","Tempo entre disparos; uma vez quando Repetir está desligado"}}
}};
inline constexpr std::array<ComponentBoolean,2> timerBooleans{{
  {"repeat","Repetir",[](const ComponentValue &value){return static_cast<const Timer &>(value).repeat;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).repeat=on;},{"Disparo"}},
  {"enabled","Ativo",[](const ComponentValue &value){return static_cast<const Timer &>(value).enabled;},
   [](ComponentValue &value,bool on){static_cast<Timer &>(value).enabled=on;},{"Disparo"}}
}};
inline const ComponentType Timer::descriptor{
  "astra.time.timer",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Timer>();},
  timerNumbers,timerBooleans,{},nullptr,true
};
}
