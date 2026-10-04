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
// Sombra de luz LOCAL (pontual e spot) é autoral aqui desde a versão 3, porque
// agora existe passe: o atlas em quadtree de `renderer/shadow_atlas.h`. Os
// nomes e os padrões são os do Light Inspector da Unity — Shadow Type,
// Resolution, Strength, Bias, Normal Bias e Near Plane.
//
// A direcional continua sem estes campos: a sombra dela vem das cascatas do
// sol, com política própria em Ambiente, e um segundo conjunto de controles
// para a mesma sombra seria dois donos para um valor só. A matriz real de
// capacidades está em `renderer/punctual_lights.h`.
class Light final : public ComponentValue {
public:
  LightKind kind = LightKind::Point;

#include "scene/generated/light_Light_fields0.inc"

  LightUnit unit = LightUnit::LuxLumen;

#include "scene/generated/light_Light_fields1.inc"

  float color[3]{1, 1, 1};

#include "scene/generated/light_Light_fields3.inc"

#include "scene/generated/light_Light_fields4.inc"

#include "scene/generated/light_Light_fields5.inc"

#include "scene/generated/light_Light_fields6.inc"

  // Sombra local. `shadowMode` 0 nenhuma, 1 dura, 2 suave; `shadowResolution`
  // 0 automática (a política escolhe pelo tamanho na tela) e 1..4 Baixa a
  // Muito alta.
  u32 shadowMode = 0;
  u32 shadowResolution = 0;

#include "scene/generated/light_Light_fields7.inc"

#include "scene/generated/light_Light_fields8.inc"

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Light>(*this); }
  bool valid() const override {
    if (static_cast<u32>(kind) > 2 || static_cast<u32>(unit) > 2) return false;
    if (shadowMode > 2 || shadowResolution > 4) return false;
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
        << ' ' << useColorTemperature << ' ' << shadowMode << ' ' << shadowResolution;
    for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 mode = 0, serializedUnit = 0;
    if ((version < 1 || version > 3) || !(in >> mode) || mode > 2 || !(in >> enabled)) return false;
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
    // Cena v2 é anterior à sombra local: ela abre sem sombra, que é exatamente
    // o que tinha quando foi salva.
    shadowMode = 0;shadowResolution = 0;shadowStrength = 1;
    shadowBias = .05f;shadowNormalBias = .4f;shadowNearPlane = .2f;
    if (version >= 3 && (!(in >> shadowMode) || shadowMode > 2 || !(in >> shadowResolution) || shadowResolution > 4))
      return false;
    // A v2 gravou só os oito números da época (cor, temperatura, intensidade,
    // alcance e cone); os quatro de sombra vieram na v3, no fim da lista.
    const usize stored = version >= 3 ? descriptor.numbers.size() : 8u;
    for (usize i = 0; i < stored; ++i) if (!(in >> *descriptor.numbers[i].write(*this))) return false;
    return true;
  }
};
inline bool lightHasRange(const ComponentValue &v) {return static_cast<const Light &>(v).kind!=LightKind::Directional;}
inline bool lightHasCone(const ComponentValue &v) {return static_cast<const Light &>(v).kind==LightKind::Spot;}
inline bool lightUsesTemperature(const ComponentValue &v) {return static_cast<const Light &>(v).useColorTemperature;}
// Sombra local só existe para pontual e spot; a direcional projeta pelas
// cascatas do sol, com política em Ambiente.
inline bool lightHasLocalShadow(const ComponentValue &v) {
  return static_cast<const Light &>(v).kind!=LightKind::Directional;
}
inline bool lightCastsLocalShadow(const ComponentValue &v) {
  const auto &light=static_cast<const Light &>(v);
  return light.kind!=LightKind::Directional && light.shadowMode!=0;
}
#include "scene/generated/light_lightNumbers.inc"
inline constexpr std::array<ComponentEnumOption, 3> lightKindOptions{{
  {0, "Direcional"}, {1, "Pontual"}, {2, "Spot"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightUnitOptions{{
  {0, "Interna (legada)"}, {1, "Lux / candela"}, {2, "Lux / lúmen"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightShadowModeOptions{{
  {0, "Nenhuma"}, {1, "Dura"}, {2, "Suave"}
}};
inline constexpr std::array<ComponentEnumOption, 5> lightShadowResolutionOptions{{
  {0, "Automática"}, {1, "Baixa"}, {2, "Média"}, {3, "Alta"}, {4, "Muito alta"}
}};
inline constexpr std::array<ComponentEnum, 4> lightEnums{{
  {"kind", "Modalidade", lightKindOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).kind); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).kind = static_cast<LightKind>(value); },
    {"Geral","","Tipo de emissão da luz"}},
  {"unit", "Unidade", lightUnitOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).unit); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).unit = static_cast<LightUnit>(value); },
    {"Emissão","","Direcional usa lux; luz local usa candela ou lúmen",nullptr,nullptr,
     "render.light.photometric","scene/light_units.h → irradiância linear",Invalidate::LightCluster}},
  {"shadow_mode", "Sombra", lightShadowModeOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).shadowMode; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).shadowMode = value; },
    {"Sombra","","Projeção de sombra desta luz no atlas local",lightHasLocalShadow,nullptr,
     "render.shadow.punctual","renderer/shadow_atlas.h → atlas local",Invalidate::LightCluster}},
  {"shadow_resolution", "Resolução da sombra", lightShadowResolutionOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).shadowResolution; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).shadowResolution = value; },
    {"Sombra","","Automática escolhe pelo tamanho da luz na tela",lightCastsLocalShadow,nullptr,
     "render.shadow.punctual","renderer/shadow_atlas.h → atlas local",Invalidate::LightCluster}}
}};
#include "scene/generated/light_lightBooleans.inc"
inline constexpr std::array<ComponentTriple,1> lightTriples{{
  {"color","Cor linear",{"color.r","color.g","color.b"},ComponentTripleKind::LinearColor}
}};
inline const ComponentType Light::descriptor{
  "astra.render.light", 3, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Light>(); },
  lightNumbers, lightBooleans, lightEnums, nullptr, false, {}, lightTriples
};
}
