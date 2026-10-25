#pragma once
#include "scene/components.h"
#include "resources/asset_registry.h"
#include <array>
#include <cmath>

namespace ae::scene {
// Configurações de iluminação da CENA — o equivalente da janela Lighting da
// Unity 6 (Window › Rendering › Lighting), não de um Volume.
//
// Fonte do sol (Sun Source): a Luz direcional que é a luz principal da cena, a
// que projeta pelas cascatas e que o céu procedural desenha como disco. Vazio
// mantém a regra da Unity para "None": a direcional mais brilhante — com o sol
// de Hora do dia na frente, para que a noite não devolva o dia a outra
// direcional. Uma referência que deixou de servir (objeto apagado, luz
// desligada ou não direcional) também cai na regra automática.
//
// v2 (G3c): Lightmapping Settings do Lighting Settings asset da Unity, na ordem
// da aba Scene: Baked Global Illumination, Lightmap Resolution, Lightmap
// Padding, Max Lightmap Size, Direct/Indirect/Environment Samples, Bounces,
// Filtering (denoiser), Compress Lightmaps, Directional Mode, Indirect Intensity
// e Albedo Boost. `lightingData` é o recurso .aelightmap que o Generate
// Lighting gravou (o LightingData da Unity); Clear Baked Data o desvincula.
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightingSettings.html
class LightingSettings final : public ComponentValue {
public:
  u64 sunSource = 0;
  bool bakedGlobalIllumination = true;
  float lightmapResolution = 40.0f;
  float lightmapPadding = 2.0f;
  u32 maxLightmapSize = 2; // índice em LightmapMaxSizeSteps (256..4096): 1024
  float directSamples = 32.0f;
  float indirectSamples = 512.0f;
  float environmentSamples = 256.0f;
  u32 bounces = 2;          // 0..4
  bool denoise = true;
  bool compressLightmaps = true;
  u32 directionalMode = 0;  // 0 Non-Directional
  float indirectIntensity = 1.0f;
  float albedoBoost = 1.0f;
  resources::AssetGuid lightingData{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<LightingSettings>(*this); }
  bool valid() const override;
  void write(std::ostream &out) const override;
  bool read(std::istream &in, u32 version) override;
};
inline bool lightingBakedGi(const ComponentValue &v) { return static_cast<const LightingSettings &>(v).bakedGlobalIllumination; }
inline constexpr const char *kLightmapCapability = "render.gi.baked";
inline constexpr const char *kLightmapConsumer = "renderer/lightmap_baker.cpp → renderer/lightmap_data.h → rhi/shaders/lightmap_lighting.glsl";
inline constexpr std::array<ComponentNumber, 7> lightingSettingsNumbers{{
#define AE_LIGHTING_NUMBER(id, label, field, lo, hi, step, unit, help) \
  {label, lo, hi, step, [](const ComponentValue &v) -> const float & { return static_cast<const LightingSettings &>(v).field; }, \
   [](ComponentValue &v) -> float * { return &static_cast<LightingSettings &>(v).field; }, id, \
   {"Lightmaps", unit, help, lightingBakedGi, nullptr, kLightmapCapability, kLightmapConsumer, Invalidate::Probes}}
  AE_LIGHTING_NUMBER("lightmap_resolution", "Resolução do lightmap", lightmapResolution, .1f, 400, 1, "texels/m",
                     "Texels por metro de superfície (Lightmap Resolution; Unity: 40)"),
  AE_LIGHTING_NUMBER("lightmap_padding", "Espaçamento do lightmap", lightmapPadding, 0, 16, 1, "texels",
                     "Texels entre objetos na página (Lightmap Padding)"),
  AE_LIGHTING_NUMBER("direct_samples", "Amostras diretas", directSamples, 1, 4096, 1, "",
                     "Raios de sombra por texel das luzes com raio de sombra (Direct Samples)"),
  AE_LIGHTING_NUMBER("indirect_samples", "Amostras indiretas", indirectSamples, 8, 8192, 8, "",
                     "Raios por texel em cada rebote (Indirect Samples)"),
  AE_LIGHTING_NUMBER("environment_samples", "Amostras do ambiente", environmentSamples, 8, 8192, 8, "",
                     "Raios por texel para a visibilidade do céu (Environment Samples)"),
  AE_LIGHTING_NUMBER("indirect_intensity", "Intensidade indireta", indirectIntensity, 0, 5, .05f, "",
                     "Multiplica a luz indireta e o céu assados (Indirect Intensity)"),
  AE_LIGHTING_NUMBER("albedo_boost", "Reforço de albedo", albedoBoost, 1, 10, .05f, "",
                     "Clareia a cor que rebate (Albedo Boost)")
#undef AE_LIGHTING_NUMBER
}};
inline constexpr std::array<ComponentBoolean, 3> lightingSettingsBooleans{{
  {"baked_global_illumination", "Iluminação global assada",
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).bakedGlobalIllumination; },
   [](ComponentValue &v, bool next) { static_cast<LightingSettings &>(v).bakedGlobalIllumination = next; },
   {"Lightmaps", "", "Liga o Generate Lighting de lightmaps (Baked Global Illumination)", nullptr, nullptr, kLightmapCapability,
    kLightmapConsumer, Invalidate::Probes}},
  {"denoise", "Remover ruído",
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).denoise; },
   [](ComponentValue &v, bool next) { static_cast<LightingSettings &>(v).denoise = next; },
   {"Lightmaps", "", "Filtro guiado por normal e posição depois do bake (Filtering)", lightingBakedGi, nullptr, kLightmapCapability,
    kLightmapConsumer, Invalidate::Probes}},
  {"compress_lightmaps", "Comprimir lightmaps",
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).compressLightmaps; },
   [](ComponentValue &v, bool next) { static_cast<LightingSettings &>(v).compressLightmaps = next; },
   {"Lightmaps", "", "8 bits por canal com faixa por página; desligado usa meia precisão (Compress Lightmaps)", lightingBakedGi,
    nullptr, kLightmapCapability, kLightmapConsumer, Invalidate::Probes}}
}};
inline constexpr std::array<ComponentEnumOption, 5> lightmapMaxSizeOptions{{
  {0, "256"}, {1, "512"}, {2, "1024"}, {3, "2048"}, {4, "4096"}
}};
inline constexpr std::array<ComponentEnumOption, 5> lightmapBounceOptions{{
  {0, "Nenhum"}, {1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}
}};
inline constexpr std::array<ComponentEnumOption, 1> lightmapDirectionalOptions{{{0, "Não direcional"}}};
inline constexpr std::array<ComponentEnum, 3> lightingSettingsEnums{{
  {"max_lightmap_size", "Tamanho máximo do lightmap", lightmapMaxSizeOptions,
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).maxLightmapSize; },
   [](ComponentValue &v, u32 value) { static_cast<LightingSettings &>(v).maxLightmapSize = value; },
   {"Lightmaps", "px", "Lado máximo de cada página (Max Lightmap Size)", lightingBakedGi, nullptr, kLightmapCapability,
    kLightmapConsumer, Invalidate::Probes}},
  {"bounces", "Rebotes", lightmapBounceOptions,
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).bounces; },
   [](ComponentValue &v, u32 value) { static_cast<LightingSettings &>(v).bounces = value; },
   {"Lightmaps", "", "Quantas vezes a luz rebate entre superfícies (Bounces)", lightingBakedGi, nullptr, kLightmapCapability,
    kLightmapConsumer, Invalidate::Probes}},
  {"directional_mode", "Modo direcional", lightmapDirectionalOptions,
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).directionalMode; },
   [](ComponentValue &v, u32 value) { static_cast<LightingSettings &>(v).directionalMode = value; },
   {"Lightmaps", "", "Não direcional guarda só a irradiância (Directional Mode › Non-Directional)", lightingBakedGi, nullptr,
    kLightmapCapability, kLightmapConsumer, Invalidate::Probes}}
}};
inline constexpr std::array<ComponentObjectReference, 1> lightingSettingsReferences{{
  {"sun_source", "Fonte do sol", "astra.render.light", ObjectReferenceScope::Any, "Automática (a direcional mais brilhante)",
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).sunSource; },
   [](ComponentValue &v, u64 id) { static_cast<LightingSettings &>(v).sunSource = id; },
   {"Ambiente", "", "A luz direcional que é o sol da cena: projeta as cascatas e aparece no céu", nullptr, nullptr,
    "render.light.sun_source", "runtime/scene_lights.cpp → renderer/punctual_lights.h", Invalidate::LightCluster},
   nullptr, nullptr, true}
}};
inline constexpr std::array<ComponentResourceBinding, 1> lightingSettingsResources{{
  {"lighting_data", "Dados de iluminação", resources::AssetType::Lightmap,
   [](const ComponentValue &) { return 1u; },
   [](const ComponentValue &v, u32) { return static_cast<const LightingSettings &>(v).lightingData; },
   [](ComponentValue &v, u32, resources::AssetGuid value) { static_cast<LightingSettings &>(v).lightingData = value; return true; },
   {"Lightmaps", "", "Escrito pelo Gerar iluminação; Limpar desvincula", nullptr, nullptr, kLightmapCapability, kLightmapConsumer,
    Invalidate::Probes}}
}};
inline bool LightingSettings::valid() const {
  for (const auto &p : lightingSettingsNumbers) {
    const float v = p.read(*this);
    if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
  }
  return maxLightmapSize <= 4 && bounces <= 4 && directionalMode == 0;
}
inline void LightingSettings::write(std::ostream &out) const {
  out << sunSource << ' ' << bakedGlobalIllumination << ' ' << maxLightmapSize << ' ' << bounces << ' ' << denoise << ' '
      << compressLightmaps << ' ' << directionalMode << ' ' << lightingData.high << ' ' << lightingData.low;
  for (const auto &p : lightingSettingsNumbers) out << ' ' << p.read(*this);
}
inline bool LightingSettings::read(std::istream &in, u32 version) {
  if (version < 1 || version > 2 || !(in >> sunSource)) return false;
  if (version == 1) return true; // v1: só a Fonte do sol; o resto nos padrões da Unity
  LightingSettings value;
  value.sunSource = sunSource;
  if (!(in >> value.bakedGlobalIllumination >> value.maxLightmapSize >> value.bounces >> value.denoise >>
        value.compressLightmaps >> value.directionalMode >> value.lightingData.high >> value.lightingData.low)) return false;
  for (const auto &p : lightingSettingsNumbers) if (!(in >> *p.write(value))) return false;
  if (!value.valid()) return false;
  sunSource = value.sunSource; bakedGlobalIllumination = value.bakedGlobalIllumination;
  lightmapResolution = value.lightmapResolution; lightmapPadding = value.lightmapPadding;
  maxLightmapSize = value.maxLightmapSize; directSamples = value.directSamples; indirectSamples = value.indirectSamples;
  environmentSamples = value.environmentSamples; bounces = value.bounces; denoise = value.denoise;
  compressLightmaps = value.compressLightmaps; directionalMode = value.directionalMode;
  indirectIntensity = value.indirectIntensity; albedoBoost = value.albedoBoost; lightingData = value.lightingData;
  return true;
}
inline const ComponentType LightingSettings::descriptor{
  "astra.render.lighting_settings", 2,
  []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LightingSettings>(); },
  lightingSettingsNumbers, lightingSettingsBooleans, lightingSettingsEnums, nullptr,
  false, lightingSettingsReferences, {}, lightingSettingsResources
};
} // namespace ae::scene
