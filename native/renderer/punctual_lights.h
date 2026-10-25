#pragma once
#include "core/base.h"
#include "core/engine_capability.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string_view>

namespace ae::renderer {

// Orçamento de luzes adicionais por quadro — tudo que não é o sol principal:
// pontuais, spots e as direcionais além do sol (lua, preenchimento).
//
// O bloco uniforme do quadro é um UBO std140; cada luz custa 64 bytes. Dezesseis
// é o teto do bloco (o primeiro banco de oito fica onde sempre esteve, o
// segundo foi acrescentado no fim, sem mover offset nenhum) e cabe com folga
// no mínimo garantido pelo Vulkan (16 KiB). A política do aparelho decide
// quantas o quadro aceita (`LightBudgetSettings`, como Additional Lights no
// URP Asset) e cada desenho soma no máximo oito — o Per Object Limit da URP —
// escolhidas pela máscara por desenho abaixo. O número está aqui, e não
// espalhado entre shader e renderer, porque os dois precisam concordar.
inline constexpr u32 MaximumPunctualLights = 16;
inline constexpr u32 PunctualLightBankSize = 8;
inline constexpr u32 MaximumLightsPerObject = 8;

// A forma que o shader lê. Quatro vec4 por luz, std140.
//
// Direcional adicional: `positionRange.w` negativo. O fragmento usa então
// `-directionOffset.xyz` como direção para a luz, sem janela de alcance nem
// cone; a sombra fica sempre em -1 (só o sol principal tem cascatas, como a
// luz principal da URP).
struct PunctualLight {
  float positionRange[4]{};   // xyz posição de mundo, w alcance em metros (<0: direcional)
  float colorIntensity[4]{};  // rgb cor * intensidade, w escala do cone
  float directionOffset[4]{}; // xyz direção de emissão, w deslocamento do cone
  // Sombra local (G6-B): x é o primeiro tile desta luz no atlas e -1 significa
  // "sem sombra neste quadro"; y é quantos tiles (1 no spot, 6 no pontual);
  // z é a força (o Strength do Light Inspector) e w o desvio ao longo da
  // normal, em texels do mapa. O renderer preenche isto DEPOIS de montar o
  // atlas — antes dele não existe tile a apontar.
  float shadow[4]{-1, 0, 1, 1};
  bool directional() const noexcept { return positionRange[3] < 0.0f; }
};
static_assert(sizeof(PunctualLight) == 64);

// Validate the uploaded records, after local shadow allocation. This is a
// compiled shader contract, so a spot, a directional, a valid shadow tile or
// malformed cone must immediately return to the general lighting pipeline.
inline bool unshadowedPointLighting(std::span<const PunctualLight> lights) noexcept {
  for (const auto &light : lights)
    if (light.colorIntensity[3] != 0.0f || light.directionOffset[3] != 1.0f ||
        !(light.shadow[0] < -0.5f) || !(light.positionRange[3] >= 0.0f)) return false;
  return true;
}

// Máscaras autorais de cada vaga aceita: Culling Mask (camadas de objeto) e
// Rendering Layers da Luz. O renderer guarda uma por vaga junto do bloco.
struct PunctualLightFilter {
  u32 cullingMask = ~0u;
  u32 renderingLayers = 1u;
  // G3c: luz em modo Assado. A luz dela já está no lightmap de quem o recebe;
  // nesses desenhos o tempo real a pula (os que se movem continuam com ela).
  bool baked = false;
};
// O lado do desenho: camada do objeto e Rendering Layer Mask do renderer.
struct DrawLightingKey {
  u32 objectLayer = 0;
  u32 renderingLayers = 1u;
  bool receiveShadows = true;
  // PK-6: Probes do Mesh Renderer (valores da Unity) e o Static do objeto, que
  // decide se o desenho entra na captura de uma sonda Assada.
  u8 lightProbes = 1;      // 0 Desligado, 1 Misturar sondas
  u8 reflectionProbes = 1; // 0 Desligado, 1 Misturar, 2 Misturar e céu, 3 Simples
  bool isStatic = false;
  // G3c: lightmap assado deste slot (página + 1; 0 sem lightmap) e o
  // lightmapScaleOffset da Unity (uv = uv2 * xy + zw).
  u16 lightmapPage = 0;
  float lightmapScaleOffset[4]{1, 1, 0, 0};
  bool lightmapped() const noexcept { return lightmapPage != 0; }
};
inline constexpr bool lightFilterAccepts(const PunctualLightFilter &filter, const DrawLightingKey &key) noexcept {
  const u32 layerBit = key.objectLayer < 32 ? 1u << key.objectLayer : 0u;
  return (filter.cullingMask & layerBit) != 0 && (filter.renderingLayers & key.renderingLayers) != 0 &&
         !(filter.baked && key.lightmapPage != 0);
}

namespace detail {
inline float recordLuminance(const PunctualLight &light) noexcept {
  return 0.2126f * light.colorIntensity[0] + 0.7152f * light.colorIntensity[1] + 0.0722f * light.colorIntensity[2];
}
} // namespace detail

// Máscara por desenho das luzes aceitas que podem alcançá-lo: bit i quando a
// esfera de influência da luz i (posição, alcance) toca a esfera do desenho e
// as máscaras autorais (Culling Mask e Rendering Layers) aceitam o objeto.
// Conservadora para spot (o cone fica dentro da esfera); direcional adicional
// alcança tudo. O fragmento pula as luzes fora da máscara antes até da
// distância -- medido na Vila em S, o laço das luzes pontuais era 15% do passe
// opaco.
//
// Mais de `perObjectLimit` candidatas: ficam as que mais iluminam o centro do
// desenho (direcionais primeiro), como o Per Object Limit da URP. Empate fica
// com a vaga menor, que é a de maior influência no quadro.
inline u32 punctualLightMask(const float center[3], float radius, std::span<const PunctualLight> lights,
                             std::span<const PunctualLightFilter> filters, const DrawLightingKey &key,
                             u32 perObjectLimit = MaximumLightsPerObject, bool sphereTest = true) noexcept {
  u32 mask = 0, count = 0;
  float score[MaximumPunctualLights]{};
  const u32 total = static_cast<u32>(std::min<usize>(lights.size(), MaximumPunctualLights));
  for (u32 i = 0; i < total; ++i) {
    if (i < filters.size() && !lightFilterAccepts(filters[i], key)) continue;
    const auto &light = lights[i];
    if (light.directional()) {
      mask |= 1u << i; ++count;
      score[i] = 1e30f;
      continue;
    }
    const float *position = light.positionRange;
    const float dx = position[0] - center[0], dy = position[1] - center[1], dz = position[2] - center[2];
    const float squared = dx * dx + dy * dy + dz * dz;
    const float reach = std::max(position[3], 0.0f) + std::max(radius, 0.0f);
    // Raio não finito (desenho sem limites conhecidos) mantém a luz.
    if (!sphereTest || !std::isfinite(reach) || squared <= reach * reach) {
      mask |= 1u << i; ++count;
      const float gap = std::max(std::sqrt(squared) - (std::isfinite(radius) ? std::max(radius, 0.0f) : 0.0f), 0.1f);
      score[i] = detail::recordLuminance(light) / (gap * gap);
    }
  }
  const u32 limit = std::min(perObjectLimit, MaximumLightsPerObject);
  while (count > limit) {
    u32 weakest = MaximumPunctualLights;
    for (u32 i = 0; i < total; ++i)
      if ((mask >> i) & 1u)
        if (weakest == MaximumPunctualLights || score[i] < score[weakest] ||
            (score[i] == score[weakest] && i > weakest)) weakest = i;
    mask &= ~(1u << weakest); --count;
  }
  return mask;
}
// Forma sem máscaras autorais: todas as luzes aceitam o desenho.
inline u32 punctualLightMask(const float center[3], float radius,
                             std::span<const PunctualLight> lights) noexcept {
  return punctualLightMask(center, radius, lights, {}, DrawLightingKey{}, MaximumLightsPerObject, true);
}

// Codificação em GpuMeshInstance::normalColumns[11] (coluna 2 .w): zero é
// "sem máscara" (todas as luzes, recebe sombra, recebe o sol), valor de
// buildNormalMatrix. Com máscara: bit 16 marca a palavra válida, bits 0..15
// são as luzes, bit 17 "não recebe sombra" (Receive Shadows desligado) e bit 18
// "o sol não ilumina" (Culling Mask/Rendering Layers do sol recusam o objeto).
// Inteiros até 2^19 são exatos em float.
inline constexpr u32 DrawLightingMaskValid = 1u << 16;
inline constexpr u32 DrawLightingNoShadows = 1u << 17;
inline constexpr u32 DrawLightingNoSun = 1u << 18;
inline float encodeDrawLighting(u32 mask, bool receiveShadows, bool sunReaches) noexcept {
  return static_cast<float>(DrawLightingMaskValid | (mask & 0xffffu) |
                            (receiveShadows ? 0u : DrawLightingNoShadows) | (sunReaches ? 0u : DrawLightingNoSun));
}
inline float encodePunctualLightMask(u32 mask) noexcept { return encodeDrawLighting(mask, true, true); }

enum class LightModality : u32 { Directional = 0, Point = 1, Spot = 2 };

// Sombra pedida pelo autor, por luz. É o Shadow Type do Light Inspector da
// Unity (No Shadows / Hard Shadows / Soft Shadows) com a resolução, a força e
// os desvios que ela também expõe. Vale também para o sol (v4 da Luz): nele,
// resolução e plano próximo são das cascatas da política.
struct SceneLightShadow {
  // 0 nenhuma, 1 dura, 2 suave.
  u8 mode = 0;
  // 0 automática (a política decide pelo tamanho na tela), 1..4 Baixa..Muito alta.
  u8 resolution = 0;
  float strength = 1;
  float bias = .05f, normalBias = .4f;
  float nearPlane = .2f;
  bool casts() const noexcept { return mode != 0; }
};
// Os padrões autorais que reproduzem exatamente o desvio da política: o Desvio
// e o Desvio na normal da Luz escalam o do aparelho por estes divisores.
inline constexpr float ReferenceShadowBias = .05f;
inline constexpr float ReferenceShadowNormalBias = .4f;

// V-2: o corpo que a Hora do dia faz a direcional seguir.
enum class CelestialLightBody : u8 { None = 0, Sun = 1, Moon = 2 };

// O que a extração produz, antes de qualquer política de orçamento.
struct SceneLight {
  u64 objectId = 0;
  LightModality modality = LightModality::Point;
  float position[3]{};
  float direction[3]{0, -1, 0};
  float color[3]{1, 1, 1}; // linear
  float intensity = 1;
  float range = 10;
  float innerAngle = 20; // meio-ângulo, graus (o Inspector mostra o total)
  float outerAngle = 35;
  SceneLightShadow shadow{};
  // Intensidade em unidade física (lux, candela, lúmen) em vez da escala
  // interna legada. Só a física pede que céu e ambiente acompanhem o sol.
  bool photometric = false;
  // G4: fase da lua ditada por TimeOfDay no sol; negativo usa a do Ambiente.
  float moonPhase = -1.0f;
  // Culling Mask e Rendering Layers da Luz.
  PunctualLightFilter filter{};
  // A Fonte do sol das Configurações de iluminação aponta para esta luz.
  bool sunSource = false;
  // V-2: Sol ou Lua da Hora do dia. O céu precisa do sol verdadeiro mesmo
  // quando a luz principal da noite é a Lua: `celestialToSun` é a direção do
  // chão ao sol daquela hora, e `celestialSphere` (linhas mundo -> esfera das
  // estrelas, renderer/time_of_day.h) gira as estrelas com a hora.
  CelestialLightBody celestialBody = CelestialLightBody::None;
  // G3c: 0 Tempo real, 1 Misto, 2 Assado (o astro da Hora do dia é sempre 0).
  u8 lightmapMode = 0;
  float celestialToSun[3]{0, 1, 0};
  // G4d: o ar que a Hora do dia já pôs na cor e na intensidade do sol
  // (TimeOfDayState::atmosphereTint/atmosphereDimming); o céu físico divide.
  float atmosphereTint[3]{1, 1, 1};
  float atmosphereDimming = 1.0f;
  float celestialSphere[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
};

// Nenhuma luz some em silêncio: o que não coube é contado, por modalidade, e
// quem chama publica isso no console. `directionalAccepted` conta o sol
// principal e as direcionais adicionais que entraram no quadro.
struct LightBudgetReport {
  u32 punctualAccepted = 0;
  u32 punctualDropped = 0;
  u32 directionalAccepted = 0;
  u32 directionalDropped = 0;
  bool complete() const noexcept { return punctualDropped == 0 && directionalDropped == 0; }
};

// A matriz real de capacidades, publicada em vez de prometida. `shadows` é o
// que EXISTE em shader e passe, não o que seria desejável.
struct LightCapability {
  LightModality modality;
  std::string_view name;
  bool lit;     // há consumidor gráfico de iluminação
  bool shadows; // há passe de sombra implementado
};
inline constexpr std::array<LightCapability, 3> lightCapabilities{{
  {LightModality::Directional, "Direcional", true, true},
  {LightModality::Point, "Pontual", true, true},
  {LightModality::Spot, "Spot", true, true}
}};
inline constexpr bool lightCastsShadow(LightModality modality) {
  for (const auto &entry : lightCapabilities)
    if (entry.modality == modality) return entry.shadows;
  return false;
}

// Esta tabela e o registro de capacidades do motor (`core/engine_capability.h`)
// respondem à MESMA pergunta. Enquanto eram duas listas independentes, implementar
// o atlas de sombra local e esquecer de atualizar uma delas deixaria o inspetor
// escondendo um controle que o renderer já sabe desenhar — ou oferecendo um que
// ele não sabe. O compilador recusa a divergência.
namespace detail {
inline constexpr bool capabilityImplemented(std::string_view id) {
  const auto *entry = core::findEngineCapability(id);
  return entry && entry->state == core::CapabilityState::Implemented;
}
} // namespace detail
static_assert(lightCastsShadow(LightModality::Directional) ==
              detail::capabilityImplemented("render.shadow.directional"));
static_assert(lightCastsShadow(LightModality::Point) ==
              detail::capabilityImplemented("render.shadow.punctual"));
static_assert(lightCastsShadow(LightModality::Spot) ==
              detail::capabilityImplemented("render.shadow.punctual"));
static_assert(detail::capabilityImplemented("render.light.additional_directional"));
static_assert(detail::capabilityImplemented("render.light.layers"));

inline bool finiteVector(const float v[3]) {
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

// Uma luz sem energia não é erro de autoria — é uma luz apagada pelo valor. Ela
// não ocupa vaga do orçamento, senão desligar uma luz por intensidade zero
// apagaria outra que caberia.
inline bool lightContributes(const SceneLight &light) {
  if (!finiteVector(light.position) || !finiteVector(light.direction) || !finiteVector(light.color)) return false;
  if (!std::isfinite(light.intensity) || !std::isfinite(light.range)) return false;
  if (light.intensity <= 0) return false;
  if (light.color[0] <= 0 && light.color[1] <= 0 && light.color[2] <= 0) return false;
  if (light.modality != LightModality::Directional && light.range <= 0) return false;
  return true;
}

namespace detail {
inline float influence(const SceneLight &light, const float camera[3]) {
  // Direcional adicional ilumina a cena inteira: entra antes de qualquer
  // lâmpada, na ordem da própria intensidade.
  if (light.modality == LightModality::Directional) return 1e20f + light.intensity;
  const float dx = light.position[0] - camera[0];
  const float dy = light.position[1] - camera[1];
  const float dz = light.position[2] - camera[2];
  const float distance = dx * dx + dy * dy + dz * dz;
  // Fora do alcance a luz não ilumina nada em volta da câmera, mas ainda pode
  // iluminar o que a câmera vê. Por isso a ordenação é contínua, não um corte:
  // o brilho relativo cai com a distância e o alcance, sem degrau.
  const float reach = light.range > 0 ? light.range : 1;
  return light.intensity * reach * reach / (reach * reach + distance);
}
inline void normalized(const float source[3], float out[3]) {
  const float length = std::sqrt(source[0] * source[0] + source[1] * source[1] + source[2] * source[2]);
  if (!(length > 1e-6f)) { out[0] = 0; out[1] = -1; out[2] = 0; return; }
  for (u32 i = 0; i < 3; ++i) out[i] = source[i] / length;
}
inline bool directionalCandidate(const SceneLight &light) {
  // An authored, enabled directional light at zero energy explicitly suppresses
  // the renderer's default sun. Local lights at zero energy still consume no slots.
  // Filtering through lightContributes here resurrected daylight in dark interiors.
  return light.modality == LightModality::Directional && finiteVector(light.position) &&
         finiteVector(light.direction) && finiteVector(light.color) &&
         std::isfinite(light.intensity) && light.intensity >= 0;
}
} // namespace detail

// A luz direcional que é o sol da cena — a luz principal, com cascatas — ou
// nullptr quando a cena não tem nenhuma.
//
// Como a Unity: a Fonte do sol das Configurações de iluminação manda quando
// aponta para uma direcional ativa; senão ("None"), a mais brilhante. Na regra
// automática, o sol movido por TimeOfDay vence mesmo à noite (intensidade
// zero), senão a direcional antiga reacenderia o dia.
inline const SceneLight *selectDirectionalLightBeforeNight(std::span<const SceneLight> lights) {
  for (const auto &light : lights)
    if (light.sunSource && detail::directionalCandidate(light)) return &light;
  const SceneLight *chosen = nullptr;
  for (const auto &light : lights) {
    if (!detail::directionalCandidate(light)) continue;
    const bool timed = light.moonPhase >= 0.0f, chosenTimed = chosen && chosen->moonPhase >= 0.0f;
    if (chosen && timed != chosenTimed) {
      if (timed) chosen = &light;
      continue;
    }
    if (!chosen || light.intensity > chosen->intensity ||
        (light.intensity == chosen->intensity && light.objectId < chosen->objectId))
      chosen = &light;
  }
  return chosen;
}

// V-2: troca de sombra do ciclo do HDRP. Com o sol da Hora do dia apagado (ou
// sem nenhum sol), a Lua mais forte acesa vira a luz principal: ela ganha as
// cascatas e o sombreamento direto; o sol volta a mandar quando acende.
inline const SceneLight *selectDirectionalLight(std::span<const SceneLight> lights) {
  const SceneLight *chosen = selectDirectionalLightBeforeNight(lights);
  if (chosen && chosen->celestialBody == CelestialLightBody::Moon) chosen = nullptr;
  if (chosen && chosen->intensity > 0.0f) return chosen;
  const SceneLight *moon = nullptr;
  for (const auto &light : lights)
    if (light.celestialBody == CelestialLightBody::Moon && detail::directionalCandidate(light) &&
        light.intensity > 0.0f && (!moon || light.intensity > moon->intensity)) moon = &light;
  if (moon) return moon;
  return chosen ? chosen : selectDirectionalLightBeforeNight(lights);
}

// Escolhe as luzes adicionais do quadro: pontuais, spots e as direcionais além
// do sol principal. Determinístico: maior influência primeiro e, em empate,
// menor `objectId` — dois quadros com a mesma cena produzem a mesma lista, sem
// cintilação por ordem de iteração. `out.size()` é o orçamento do quadro.
inline u32 selectPunctualLights(std::span<const SceneLight> lights, const float camera[3],
                                std::span<PunctualLight> out, LightBudgetReport &report,
                                std::span<u32> outSources = {}) {
  report = {};
  if (!camera) return 0;
  const SceneLight *sun = selectDirectionalLight(lights);
  if (sun) ++report.directionalAccepted;
  std::array<u32, 64> ordered{};
  u32 candidates = 0, droppedDirectional = 0, droppedPunctual = 0;
  for (u32 i = 0; i < lights.size(); ++i) {
    const auto &light = lights[i];
    if (&light == sun || !lightContributes(light)) continue;
    if (candidates == ordered.size()) {
      ++(light.modality == LightModality::Directional ? droppedDirectional : droppedPunctual);
      continue;
    }
    ordered[candidates++] = i;
  }
  // Inserção: no máximo 64 candidatos e, na prática, um punhado. Ordenação
  // estável sem alocar e sem std::sort num caminho de quadro.
  for (u32 i = 1; i < candidates; ++i) {
    const u32 value = ordered[i];
    const float key = detail::influence(lights[value], camera);
    u32 j = i;
    while (j > 0) {
      const float other = detail::influence(lights[ordered[j - 1]], camera);
      if (other > key || (other == key && lights[ordered[j - 1]].objectId <= lights[value].objectId)) break;
      ordered[j] = ordered[j - 1];
      --j;
    }
    ordered[j] = value;
  }
  const u32 capacity = static_cast<u32>(std::min<usize>(out.size(), MaximumPunctualLights));
  const u32 accepted = candidates < capacity ? candidates : capacity;
  for (u32 i = 0; i < candidates; ++i) {
    const bool directional = lights[ordered[i]].modality == LightModality::Directional;
    if (i < accepted) ++(directional ? report.directionalAccepted : report.punctualAccepted);
    else ++(directional ? droppedDirectional : droppedPunctual);
  }
  report.directionalDropped = droppedDirectional;
  report.punctualDropped = droppedPunctual;
  for (u32 i = 0; i < accepted; ++i) {
    const auto &light = lights[ordered[i]];
    // De qual luz da cena veio esta vaga: sem isso o renderer não tem como
    // reatar a sombra autorada e as máscaras ao slot que o shader lê.
    if (i < outSources.size()) outSources[i] = ordered[i];
    auto &target = out[i];
    // O bloco é memória persistente: sem isto a vaga herdaria o tile de sombra
    // de outra luz do quadro anterior. O renderer escreve a sombra depois.
    target.shadow[0] = -1; target.shadow[1] = 0; target.shadow[2] = 1; target.shadow[3] = 1;
    for (u32 axis = 0; axis < 3; ++axis) target.positionRange[axis] = light.position[axis];
    target.positionRange[3] = light.modality == LightModality::Directional ? -1.0f : light.range;
    for (u32 axis = 0; axis < 3; ++axis)
      target.colorIntensity[axis] = light.color[axis] * light.intensity;
    detail::normalized(light.direction, target.directionOffset);
    if (light.modality == LightModality::Spot) {
      // Janela suave do cone, pré-calculada como no glTF: o fragmento só faz
      // `clamp(cos * scale + offset, 0, 1)`, sem trigonometria por pixel.
      const float toRadians = 0.0174532925199433f;
      const float outer = std::cos(std::fmin(std::fmax(light.outerAngle, 0.f), 89.5f) * toRadians);
      const float inner = std::cos(std::fmin(std::fmax(light.innerAngle, 0.f), 89.5f) * toRadians);
      const float span = inner - outer > 1e-4f ? inner - outer : 1e-4f;
      target.colorIntensity[3] = 1.f / span;
      target.directionOffset[3] = -outer / span;
    } else {
      // Pontual e direcional: a mesma conta precisa render 1 em qualquer
      // direção, para que o fragmento não tenha um desvio por tipo de luz.
      target.colorIntensity[3] = 0;
      target.directionOffset[3] = 1;
    }
  }
  return accepted;
}

} // namespace ae::renderer
