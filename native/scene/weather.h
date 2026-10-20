#pragma once
#include "scene/components.h"
#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {
// G4c: clima, do jeito prático da Unity. A Unity não tem um componente de clima;
// o que se faz é um script de clima que (1) muda o Cloud Layer de um Volume
// (cobertura, densidade, tom) e (2) liga e dosa Particle Systems de chuva e neve
// presos à câmera, com Simulation Space World. Este componente é esse script:
// vive no objeto de um Ambiente (o Volume da Astra) e ESCREVE propriedades
// visíveis -- as nuvens e a neblina desse Ambiente, a taxa de emissão dos dois
// sistemas de partículas e a posição deles sobre a câmera da cena. Nada fica
// escondido: o Inspector do Ambiente e o das partículas mostram o que o clima
// escreveu. Em Play a mudança de tipo ou intensidade passa pela Transição; o
// relâmpago (Tempestade) só existe em Play. A matemática está em
// runtime/scene_weather.cpp.
enum class WeatherKind : u32 { Clear = 0, Cloudy = 1, Rain = 2, Storm = 3, Snow = 4 };

class Weather final : public ComponentValue {
public:
  WeatherKind kind = WeatherKind::Rain;
  u64 rain = 0, snow = 0;

#include "scene/generated/weather_Weather_fields0.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Weather>(*this); }
  bool valid() const override {
    if (static_cast<u32>(kind) > 4 || rain > std::numeric_limits<u32>::max() || snow > std::numeric_limits<u32>::max())
      return false;
    for (const auto &p : descriptor.numbers) {
      const float v = p.read(*this);
      if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(kind) << ' ' << rain << ' ' << snow;
    for (const auto &p : descriptor.booleans) out << ' ' << p.read(*this);
    for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 value = 0;
    if (version != 1 || !(in >> value >> rain >> snow) || value > 4) return false;
    kind = static_cast<WeatherKind>(value);
    for (const auto &p : descriptor.booleans) {
      bool flag = false;
      if (!(in >> flag)) return false;
      p.write(*this, flag);
    }
    for (const auto &p : descriptor.numbers) if (!(in >> *p.write(*this))) return false;
    return valid();
  }
};
inline bool weatherHasRain(const ComponentValue &v) {
  const auto kind = static_cast<const Weather &>(v).kind;
  return kind == WeatherKind::Rain || kind == WeatherKind::Storm;
}
inline bool weatherHasSnow(const ComponentValue &v) { return static_cast<const Weather &>(v).kind == WeatherKind::Snow; }
inline bool weatherIsStorm(const ComponentValue &v) { return static_cast<const Weather &>(v).kind == WeatherKind::Storm; }
inline bool weatherHasLightning(const ComponentValue &v) {
  const auto &weather = static_cast<const Weather &>(v);
  return weather.kind == WeatherKind::Storm && weather.lightning;
}
inline bool weatherFollowsCamera(const ComponentValue &v) { return static_cast<const Weather &>(v).followCamera; }
#include "scene/generated/weather_weatherNumbers.inc"
#include "scene/generated/weather_weatherBooleans.inc"
inline constexpr std::array<ComponentEnumOption, 5> weatherKinds{{
  {0, "Limpo"}, {1, "Nublado"}, {2, "Chuva"}, {3, "Tempestade"}, {4, "Neve"}
}};
#include "scene/generated/weather_weatherEnums.inc"
inline constexpr std::array<ComponentObjectReference, 2> weatherReferences{{
  {"rain", "Chuva", "astra.effects.particle_system", ObjectReferenceScope::Other, "Escolher partículas",
   [](const ComponentValue &v) { return static_cast<const Weather &>(v).rain; },
   [](ComponentValue &v, u64 id) { static_cast<Weather &>(v).rain = id; },
   {"Precipitação", "", "Partículas de chuva: o clima dosa a emissão e as leva sobre a câmera", nullptr, nullptr,
    "render.environment.weather", "runtime/scene_weather.cpp → runtime/scene_particles.cpp"}},
  {"snow", "Neve", "astra.effects.particle_system", ObjectReferenceScope::Other, "Escolher partículas",
   [](const ComponentValue &v) { return static_cast<const Weather &>(v).snow; },
   [](ComponentValue &v, u64 id) { static_cast<Weather &>(v).snow = id; },
   {"Precipitação", "", "Partículas de neve: o clima dosa a emissão e as leva sobre a câmera", nullptr, nullptr,
    "render.environment.weather", "runtime/scene_weather.cpp → runtime/scene_particles.cpp"}},
}};
inline const ComponentType Weather::descriptor{
  "astra.environment.weather", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Weather>(); },
  weatherNumbers, weatherBooleans, weatherEnums, nullptr, false, weatherReferences
};
} // namespace ae::scene
