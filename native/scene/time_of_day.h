#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>

namespace ae::scene {
// G4: hora do dia. Vive no objeto da luz direcional (o sol) e, quando ativo,
// substitui a rotação dela pela posição solar da hora, data e latitude, além de
// esquentar a cor perto do horizonte e apagar o sol à noite. Também dita a fase
// da lua do céu. Em Play a hora avança pela Duração do dia; a autoral nunca é
// reescrita, então parar o Play volta ao mesmo céu. A matemática está em
// renderer/time_of_day.cpp.
class TimeOfDay final : public ComponentValue {
public:
#include "scene/generated/time_of_day_TimeOfDay_fields0.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TimeOfDay>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {
      const float v=p.read(*this);
      if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out<<enabled;for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>enabled)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    return valid();
  }
};
#include "scene/generated/time_of_day_timeOfDayNumbers.inc"
#include "scene/generated/time_of_day_timeOfDayBooleans.inc"
inline const ComponentType TimeOfDay::descriptor{
  "astra.render.time_of_day",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TimeOfDay>();},
  timeOfDayNumbers,timeOfDayBooleans
};
}
