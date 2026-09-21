#pragma once
#include "scene/components.h"
#include "scene/light_units.h"
#include <array>
namespace ae::scene {
// Luz anexável a qualquer objeto: a pose vem da Transformação, como em qualquer
// outro componente. `+Z` local é a direção de emissão de direcional e spot — a
// mesma convenção de frente que a Câmera desta engine usa, para que girar um
// objeto de luz e um de câmera do mesmo jeito aponte os dois para o mesmo lado.
//
// Unidades, escritas aqui porque o shader depende delas:
//  - `color` é RGB linear, não sRGB. O inspetor mostra o mesmo número que o
//    shader multiplica.
//  - Cenas v1 usam LightUnit::Engine e preservam exatamente a escala anterior.
//  - LuxCandela usa lux na direcional e candela na pontual/spot, como glTF.
//  - LuxLumen usa lux na direcional e fluxo em lúmen na pontual/spot.
//  - `innerAngle`/`outerAngle` são meio-ângulos em graus. Entre eles a
//    atenuação é suave; fora de `outerAngle` é zero.
//
// Sombra NÃO é propriedade desta lista de propósito. O passe de sombra
// implementado é o de cascatas do sol, que só existe para a modalidade
// direcional; oferecer um interruptor de sombra em pontual ou spot seria um
// botão sem shader atrás. A matriz real de capacidades está em
// `renderer/punctual_lights.h`.
class Light final : public ComponentValue {
public:
  LightKind kind = LightKind::Point;
  bool enabled = true;
  LightUnit unit = LightUnit::LuxLumen;
  bool useColorTemperature = false;
  float color[3]{1, 1, 1};
  float colorTemperature = 6500;
  float intensity = 1000;
  float range = 10;
  float innerAngle = 20, outerAngle = 35;
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Light>(*this); }
  bool valid() const override {
    if (static_cast<u32>(kind) > 2 || static_cast<u32>(unit) > 2) return false;
    for (const auto &p : descriptor.numbers) {
      const float v = p.read(*this);
      if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
    }
    // Um cone interno maior que o externo produziria uma janela invertida e,
    // no shader, uma borda dura no lugar errado. Recusa na autoria.
    return outerAngle >= innerAngle;
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(kind) << ' ' << enabled << ' ' << static_cast<u32>(unit)
        << ' ' << useColorTemperature;
    for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 mode = 0, serializedUnit = 0;
    if ((version != 1 && version != 2) || !(in >> mode) || mode > 2 || !(in >> enabled)) return false;
    kind = static_cast<LightKind>(mode);
    if (version == 1) {
      unit = LightUnit::Engine;
      useColorTemperature = false;
      colorTemperature = 6500;
      if (!(in >> color[0] >> color[1] >> color[2] >> intensity >> range >> innerAngle >> outerAngle)) return false;
      return true;
    }
    if (!(in >> serializedUnit) || serializedUnit > 2 || !(in >> useColorTemperature)) return false;
    unit = static_cast<LightUnit>(serializedUnit);
    for (const auto &p : descriptor.numbers) if (!(in >> *p.write(*this))) return false;
    return true;
  }
};
inline bool lightHasRange(const ComponentValue &v) {return static_cast<const Light &>(v).kind!=LightKind::Directional;}
inline bool lightHasCone(const ComponentValue &v) {return static_cast<const Light &>(v).kind==LightKind::Spot;}
inline bool lightUsesTemperature(const ComponentValue &v) {return static_cast<const Light &>(v).useColorTemperature;}
inline constexpr std::array<ComponentNumber, 8> lightNumbers{{
#define AE_LIGHT_NUMBER(id, label, field, lo, hi, step, group, unit, visible) {label, lo, hi, step, [](const ComponentValue &v) -> const float & {return static_cast<const Light &>(v).field;}, [](ComponentValue &v) -> float * {return &static_cast<Light &>(v).field;}, id,{group,unit,nullptr,visible}}
  AE_LIGHT_NUMBER("color.r", "Cor R", color[0], 0, 1, .01f,"Emissão","",nullptr),
  AE_LIGHT_NUMBER("color.g", "Cor G", color[1], 0, 1, .01f,"Emissão","",nullptr),
  AE_LIGHT_NUMBER("color.b", "Cor B", color[2], 0, 1, .01f,"Emissão","",nullptr),
  {"Temperatura",1667,25000,50,[](const ComponentValue &v)->const float&{return static_cast<const Light&>(v).colorTemperature;},
    [](ComponentValue &v)->float*{return &static_cast<Light&>(v).colorTemperature;},"color_temperature",
    {"Emissão","K","Multiplicada pela cor linear; D65 é 6500 K",lightUsesTemperature,nullptr,
     "render.light.temperature","scene/light_units.h → RGB linear",Invalidate::LightCluster}},
  AE_LIGHT_NUMBER("intensity", "Intensidade", intensity, 0, 1000000, 1.f,"Emissão","",nullptr),
  AE_LIGHT_NUMBER("range", "Alcance", range, .01f, 1000, .1f,"Volume","m",lightHasRange),
  AE_LIGHT_NUMBER("inner_angle", "Meio-cone interno", innerAngle, 0, 89, 1,"Volume","°",lightHasCone),
  AE_LIGHT_NUMBER("outer_angle", "Meio-cone externo", outerAngle, 0, 89, 1,"Volume","°",lightHasCone)
#undef AE_LIGHT_NUMBER
}};
inline constexpr std::array<ComponentEnumOption, 3> lightKindOptions{{
  {0, "Direcional"}, {1, "Pontual"}, {2, "Spot"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightUnitOptions{{
  {0, "Interna (legada)"}, {1, "Lux / candela"}, {2, "Lux / lúmen"}
}};
inline constexpr std::array<ComponentEnum, 2> lightEnums{{
  {"kind", "Modalidade", lightKindOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).kind); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).kind = static_cast<LightKind>(value); },
    {"Geral","","Tipo de emissão da luz"}},
  {"unit", "Unidade", lightUnitOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).unit); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).unit = static_cast<LightUnit>(value); },
    {"Emissão","","Direcional usa lux; luz local usa candela ou lúmen",nullptr,nullptr,
     "render.light.photometric","scene/light_units.h → irradiância linear",Invalidate::LightCluster}}
}};
inline constexpr std::array<ComponentBoolean, 2> lightBooleans{{
  {"enabled", "Acesa", [](const ComponentValue &v) { return static_cast<const Light &>(v).enabled; },
   [](ComponentValue &v, bool b) { static_cast<Light &>(v).enabled = b; },
   {"Geral","","Ativa a contribuição desta luz"}},
  {"use_color_temperature", "Filtro por temperatura", [](const ComponentValue &v) { return static_cast<const Light &>(v).useColorTemperature; },
   [](ComponentValue &v, bool b) { static_cast<Light &>(v).useColorTemperature = b; },
   {"Emissão","","Multiplica a cor pela temperatura de corpo negro",nullptr,nullptr,
    "render.light.temperature","scene/light_units.h → RGB linear",Invalidate::LightCluster}}
}};
inline constexpr std::array<ComponentTriple,1> lightTriples{{
  {"color","Cor linear",{"color.r","color.g","color.b"},ComponentTripleKind::LinearColor}
}};
inline const ComponentType Light::descriptor{
  "astra.render.light", 2, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Light>(); },
  lightNumbers, lightBooleans, lightEnums, nullptr, false, {}, lightTriples
};
}
