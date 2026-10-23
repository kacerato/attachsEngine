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
  // V-2: a direcional que faz o papel de lua, como a "Moon" que o gerenciador
  // de ciclo do HDRP gira junto com o "Sun". A mesma hora, data e latitude a
  // levam até a lua; de noite ela vira a luz principal, com sombra.
  u64 moonLight = 0;

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
    out<<' '<<moonLight;
  }
  // v1 não tinha a Lua.
  bool read(std::istream &in,u32 version) override {
    if((version!=1&&version!=2) || !(in>>enabled)) return false;
    for(const auto &p:descriptor.numbers) if(!(in>>*p.write(*this))) return false;
    moonLight=0;
    if(version==2&&!(in>>moonLight)) return false;
    return valid();
  }
};
#include "scene/generated/time_of_day_timeOfDayNumbers.inc"
#include "scene/generated/time_of_day_timeOfDayBooleans.inc"
inline constexpr std::array<ComponentObjectReference, 1> timeOfDayReferences{{
  {"moon_light", "Lua", "astra.render.light", ObjectReferenceScope::Any, "Nenhuma (noite sem luar)",
   [](const ComponentValue &v) { return static_cast<const TimeOfDay &>(v).moonLight; },
   [](ComponentValue &v, u64 id) { static_cast<TimeOfDay &>(v).moonLight = id; },
   {"Hora do dia", "", "Luz direcional girada até a lua desta hora; à noite ilumina com a fase e projeta a sombra",
    nullptr, nullptr, "render.environment.time_of_day", "runtime/scene_lights.cpp → renderer/time_of_day.cpp",
    Invalidate::LightCluster},
   nullptr, nullptr, true}
}};
// G7: acontecimentos do relógio no Play (runtime/scene_time_of_day.cpp), como
// os eventos de hora do ciclo de dia do plano G4. Também viram eventos da
// Conexão de evento (scene/event_connection.h), sem script.
inline constexpr std::array<ComponentParameter,1> timeOfDayHourPayload{{{"hour","Hora",ComponentValueKind::Integer,"h"}}};
inline constexpr std::array<ComponentParameter,1> timeOfDayDayPayload{{{"day","Dia do ano",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentEvent,4> timeOfDayEvents{{
  {"hour_changed","Hora mudou","A hora inteira mudou: passou o tempo ou um script escreveu outra hora",timeOfDayHourPayload},
  {"sunrise","Nascer do sol","O sol subiu acima do horizonte"},
  {"sunset","Pôr do sol","O sol desceu abaixo do horizonte"},
  {"day_changed","Dia mudou","A hora passou de 24 h; o Dia do ano avança um",timeOfDayDayPayload},
}};
inline const ComponentType TimeOfDay::descriptor{
  "astra.render.time_of_day",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TimeOfDay>();},
  timeOfDayNumbers,timeOfDayBooleans,{},nullptr,false,timeOfDayReferences,{},{},{},{},{},{},timeOfDayEvents
};
}
