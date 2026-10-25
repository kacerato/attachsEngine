#pragma once
#include "scene/components.h"
#include "scene/light_defaults.h"
#include "scene/light_units.h"
#include "scene/rendering_layers.h"
#include <array>
namespace ae::scene {
// Luz anexável a qualquer objeto: a pose vem da Transformação, como em qualquer
// outro componente. `+Z` local é a direção de emissão de direcional e spot — a
// mesma convenção de frente que a Câmera desta engine usa, para que girar um
// objeto de luz e um de câmera do mesmo jeito aponte os dois para o mesmo lado.
//
// O componente segue o Light Inspector da Unity 6 (URP), na mesma ordem:
// Geral (Tipo), Emissão (Cor, Temperatura, Intensidade, Unidade), Volume
// (Alcance, Spot Angle, Inner Spot Angle), Sombra (Shadow Type, Resolution,
// Strength, Bias, Normal Bias, Near Plane) e Renderização (Culling Mask,
// Rendering Layers). Fonte da verdade de cada valor:
//
//  - `color` é sRGB desde a v4, como o Color da Unity num projeto linear: o
//    número no Inspector é o do seletor. O runtime converte para linear antes
//    de multiplicar pela intensidade. Cenas até a v3 guardavam linear e são
//    convertidas na leitura, sem mudança de aparência.
//  - `spotAngle`/`innerSpotAngle` são aberturas TOTAIS em graus desde a v4
//    (Spot Angle / Inner Spot Angle). Até a v3 eram meio-ângulos; a leitura
//    dobra.
//  - A SOMBRA É DA LUZ, inclusive a do sol (v4). Cascatas, distância e
//    resolução das cascatas continuam na política do aparelho, que é o teto,
//    como o URP Asset na Unity. Desvio e Desvio na normal escalam o da
//    política: os padrões (0,05 e 0,4) reproduzem exatamente o que o aparelho
//    já usava. Cenas até a v3 abrem com a direcional em sombra suave — o que
//    elas tinham, sem controle.
//  - Modo (Realtime/Mixed/Baked, `lightmapMode`) tem efeito desde o G3c: em
//    Assado a luz direta e o rebote vão para o lightmap e a luz sai do tempo
//    real nos desenhos que recebem lightmap (os que se movem continuam com
//    ela); em Misto só o rebote é assado e a direta continua em tempo real; em
//    Tempo real nada é assado. O sol da Hora do dia é sempre tempo real.
//  - Cookie e luz de Área não existem no shader; não há campo para eles.
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

  // `shadowMode` 0 nenhuma, 1 dura, 2 suave; `shadowResolution` 0 automática
  // (a política escolhe pelo tamanho na tela) e 1..4 Baixa a Muito alta. A
  // resolução só vale para luz local: a do sol é a das cascatas da política.
  u32 shadowMode = 0;
  u32 shadowResolution = 0;

#include "scene/generated/light_Light_fields7.inc"

#include "scene/generated/light_Light_fields8.inc"

  // 0 Tempo real, 1 Misto, 2 Assado (ver acima).
  u32 lightmapMode = 0;
  // Camadas de objeto que esta luz alcança e camadas de renderização dela.
  u32 cullingMask = kAllObjectLayers;
  u32 renderingLayers = kDefaultRenderingLayers;

  float outerHalfAngle() const noexcept { return spotAngle * .5f; }
  float innerHalfAngle() const noexcept { return innerSpotAngle * .5f; }
  bool castsShadow() const noexcept { return shadowMode != 0; }

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Light>(*this); }
  bool valid() const override {
    if (static_cast<u32>(kind) > 2 || static_cast<u32>(unit) > 2) return false;
    if (shadowMode > 2 || shadowResolution > 4 || lightmapMode > 2) return false;
    if ((renderingLayers & ~kAllRenderingLayers) != 0) return false;
    for (const auto &p : descriptor.numbers) {
      const float v = p.read(*this);
      if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
    }
    // Um cone interno maior que o externo produziria uma janela invertida e,
    // no shader, uma borda dura no lugar errado. Recusa na autoria.
    return spotAngle >= innerSpotAngle;
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(kind) << ' ' << enabled << ' ' << static_cast<u32>(unit)
        << ' ' << useColorTemperature << ' ' << shadowMode << ' ' << shadowResolution
        << ' ' << lightmapMode << ' ' << cullingMask << ' ' << renderingLayers;
    for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 mode = 0, serializedUnit = 0;
    if ((version < 1 || version > 4) || !(in >> mode) || mode > 2 || !(in >> enabled)) return false;
    kind = static_cast<LightKind>(mode);
    lightmapMode = 0; cullingMask = kAllObjectLayers; renderingLayers = kDefaultRenderingLayers;
    if (version >= 4) {
      if (!(in >> serializedUnit) || serializedUnit > 2 || !(in >> useColorTemperature) ||
          !(in >> shadowMode) || shadowMode > 2 || !(in >> shadowResolution) || shadowResolution > 4 ||
          !(in >> lightmapMode) || lightmapMode > 2 || !(in >> cullingMask) || !(in >> renderingLayers) ||
          (renderingLayers & ~kAllRenderingLayers) != 0) return false;
      unit = static_cast<LightUnit>(serializedUnit);
      for (const auto &p : descriptor.numbers) if (!(in >> *p.write(*this))) return false;
      return true;
    }
    // Até a v3: cor linear, meio-ângulos e nenhuma sombra autoral no sol.
    float linear[3]{}, innerHalf = 0, outerHalf = 0;
    shadowMode = 0; shadowResolution = 0; shadowStrength = 1;
    shadowBias = .05f; shadowNormalBias = .4f; shadowNearPlane = .2f;
    if (version == 1) {
      unit = LightUnit::Engine;
      useColorTemperature = false;
      colorTemperature = 6500;
      if (!(in >> linear[0] >> linear[1] >> linear[2] >> intensity >> range >> innerHalf >> outerHalf)) return false;
    } else {
      if (!(in >> serializedUnit) || serializedUnit > 2 || !(in >> useColorTemperature)) return false;
      unit = static_cast<LightUnit>(serializedUnit);
      // A v2 é anterior à sombra local: abre sem sombra, que é o que tinha.
      if (version >= 3 && (!(in >> shadowMode) || shadowMode > 2 || !(in >> shadowResolution) || shadowResolution > 4))
        return false;
      // A ordem da época: cor, temperatura, intensidade, alcance, meio-cone
      // interno e externo; a v3 acrescentou os quatro números de sombra.
      if (!(in >> linear[0] >> linear[1] >> linear[2] >> colorTemperature >> intensity >> range >> innerHalf >> outerHalf))
        return false;
      if (version >= 3 && !(in >> shadowStrength >> shadowBias >> shadowNormalBias >> shadowNearPlane)) return false;
    }
    for (u32 channel = 0; channel < 3; ++channel) color[channel] = linearChannelToSrgb(linear[channel]);
    innerSpotAngle = innerHalf * 2.0f;
    spotAngle = outerHalf * 2.0f;
    // O sol já projetava pelas cascatas, com o filtro da política: suave.
    if (kind == LightKind::Directional) shadowMode = 2;
    return true;
  }
};
inline bool lightHasRange(const ComponentValue &v) {return static_cast<const Light &>(v).kind!=LightKind::Directional;}
inline bool lightHasCone(const ComponentValue &v) {return static_cast<const Light &>(v).kind==LightKind::Spot;}
inline bool lightUsesTemperature(const ComponentValue &v) {return static_cast<const Light &>(v).useColorTemperature;}
// Resolução e plano próximo são de mapa local; a direcional usa as cascatas.
inline bool lightHasLocalShadow(const ComponentValue &v) {
  return static_cast<const Light &>(v).kind!=LightKind::Directional;
}
inline bool lightCastsLocalShadow(const ComponentValue &v) {
  const auto &light=static_cast<const Light &>(v);
  return light.kind!=LightKind::Directional && light.shadowMode!=0;
}
inline bool lightCastsShadow(const ComponentValue &v) {return static_cast<const Light &>(v).shadowMode!=0;}
#include "scene/generated/light_lightNumbers.inc"
inline constexpr std::array<ComponentEnumOption, 3> lightKindOptions{{
  {0, "Direcional"}, {1, "Pontual"}, {2, "Spot"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightUnitOptions{{
  {0, "Relativa (sem unidade)"}, {1, "Lux / candela"}, {2, "Lux / lúmen"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightModeOptions{{
  {0, "Tempo real"}, {1, "Misto"}, {2, "Assado"}
}};
inline constexpr std::array<ComponentEnumOption, 3> lightShadowModeOptions{{
  {0, "Sem sombra"}, {1, "Sombra dura"}, {2, "Sombra suave"}
}};
inline constexpr std::array<ComponentEnumOption, 5> lightShadowResolutionOptions{{
  {0, "Automática"}, {1, "Baixa"}, {2, "Média"}, {3, "Alta"}, {4, "Muito alta"}
}};
inline constexpr std::array<ComponentEnum, 7> lightEnums{{
  {"kind", "Tipo", lightKindOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).kind); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).kind = static_cast<LightKind>(value); },
    {"Geral","","Direcional ilumina a cena inteira numa direção; pontual e spot partem de um ponto"},true},
  {"unit", "Unidade", lightUnitOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).unit); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).unit = static_cast<LightUnit>(value); },
    {"Emissão","","Relativa é a intensidade da URP; lux, candela e lúmen são as unidades físicas da HDRP",nullptr,nullptr,
     "render.light.photometric","scene/light_units.h → irradiância linear",Invalidate::LightCluster}},
  // G3c: Mode do Light Inspector (Realtime/Mixed/Baked).
  {"mode", "Modo", lightModeOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).lightmapMode; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).lightmapMode = value; },
    {"Geral","","Tempo real ilumina a cada quadro; Misto assa só o rebote; Assado grava luz e rebote no lightmap",nullptr,nullptr,
     "render.light.baked","renderer/lightmap_baker.cpp + renderer/punctual_lights.h",Invalidate::LightCluster|Invalidate::Probes},true},
  {"shadow_mode", "Tipo de sombra", lightShadowModeOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).shadowMode; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).shadowMode = value; },
    {"Sombra","","Dura usa uma busca; suave filtra a borda. A política do aparelho pode desligar a sombra",nullptr,nullptr,
     "render.shadow.punctual","cascatas do sol + renderer/shadow_atlas.h",Invalidate::LightCluster}},
  {"shadow_resolution", "Resolução da sombra", lightShadowResolutionOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).shadowResolution; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).shadowResolution = value; },
    {"Sombra","","Automática escolhe pelo tamanho da luz na tela",lightCastsLocalShadow,nullptr,
     "render.shadow.punctual","renderer/shadow_atlas.h → atlas local",Invalidate::LightCluster}},
  {"culling_mask", "Máscara de culling", objectLayerOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).cullingMask; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).cullingMask = value; },
    {"Renderização","","Camadas de objeto que esta luz ilumina",nullptr,nullptr,
     "render.light.layers","renderer/punctual_lights.h → máscara por desenho",Invalidate::LightCluster},false,true},
  {"rendering_layers", "Camadas de renderização", renderingLayerOptions,
    [](const ComponentValue &v) { return static_cast<const Light &>(v).renderingLayers; },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).renderingLayers = value; },
    {"Renderização","","Ilumina só renderers com uma camada de renderização em comum",nullptr,nullptr,
     "render.light.layers","renderer/punctual_lights.h → máscara por desenho",Invalidate::LightCluster},false,true}
}};
#include "scene/generated/light_lightBooleans.inc"
inline constexpr std::array<ComponentTriple,1> lightTriples{{
  {"color","Cor",{"color.r","color.g","color.b"},ComponentTripleKind::SrgbColor}
}};
inline const ComponentType Light::descriptor{
  "astra.render.light", 4, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Light>(); },
  lightNumbers, lightBooleans, lightEnums, nullptr, false, {}, lightTriples
};

// Valores da tabela única de padrões aplicados a uma luz recém-criada.
inline void applyLightDefault(Light &light, LightKind kind) {
  const auto &value = lightDefault(kind);
  light.kind = value.kind; light.unit = value.unit; light.intensity = value.intensity;
  light.range = value.range; light.spotAngle = value.spotAngle; light.innerSpotAngle = value.innerSpotAngle;
  light.shadowMode = value.shadowMode;
}
}
