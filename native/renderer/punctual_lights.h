#pragma once
#include "core/base.h"
#include "core/engine_capability.h"
#include <array>
#include <cmath>
#include <span>
#include <string_view>

namespace ae::renderer {

// Orçamento de luzes pontuais/spot por quadro.
//
// O bloco uniforme do quadro é um UBO std140 e já carrega ambiente, sombras e
// água; cada luz custa 48 bytes. Oito é o teto: cabe com folga no mínimo
// garantido pelo Vulkan (16 KiB) e é o que o laço do fragmento percorre sem
// desmontar o registro em GPUs móveis. O número está aqui, e não espalhado
// entre shader e renderer, porque os dois precisam concordar exatamente.
inline constexpr u32 MaximumPunctualLights = 8;

// A forma que o shader lê. Quatro vec4 por luz, std140.
struct PunctualLight {
  float positionRange[4]{};   // xyz posição de mundo, w alcance em metros
  float colorIntensity[4]{};  // rgb cor * intensidade, w escala do cone
  float directionOffset[4]{}; // xyz direção de emissão, w deslocamento do cone
  // Sombra local (G6-B): x é o primeiro tile desta luz no atlas e -1 significa
  // "sem sombra neste quadro"; y é quantos tiles (1 no spot, 6 no pontual);
  // z é a força (o Strength do Light Inspector) e w o desvio ao longo da
  // normal, em texels do mapa. O renderer preenche isto DEPOIS de montar o
  // atlas — antes dele não existe tile a apontar.
  float shadow[4]{-1, 0, 1, 1};
};
static_assert(sizeof(PunctualLight) == 64);

enum class LightModality : u32 { Directional = 0, Point = 1, Spot = 2 };

// Sombra pedida pelo autor, por luz. É o Shadow Type do Light Inspector da
// Unity (No Shadows / Hard Shadows / Soft Shadows) com a resolução, a força e
// os desvios que ela também expõe.
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

// O que a extração produz, antes de qualquer política de orçamento.
struct SceneLight {
  u64 objectId = 0;
  LightModality modality = LightModality::Point;
  float position[3]{};
  float direction[3]{0, -1, 0};
  float color[3]{1, 1, 1};
  float intensity = 1;
  float range = 10;
  float innerAngle = 20; // meio-ângulo, graus
  float outerAngle = 35;
  SceneLightShadow shadow{};
  // Intensidade em unidade física (lux, candela, lúmen) em vez da escala
  // interna legada. Só a física pede que céu e ambiente acompanhem o sol.
  bool photometric = false;
};

// Nenhuma luz some em silêncio: o que não coube é contado, por modalidade, e
// quem chama publica isso no console.
struct LightBudgetReport {
  u32 punctualAccepted = 0;
  u32 punctualDropped = 0;
  u32 directionalAccepted = 0;
  u32 directionalDropped = 0;
  bool complete() const noexcept { return punctualDropped == 0 && directionalDropped == 0; }
};

// A matriz real de capacidades, publicada em vez de prometida. `shadows` é o
// que EXISTE em shader e passe, não o que seria desejável: só o sol direcional
// tem passe de profundidade e cascatas. Enquanto um cubemap de sombra ou um
// atlas de spot não existir, a interface não oferece o interruptor.
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
} // namespace detail

// Escolhe as luzes pontuais/spot do quadro. Determinístico: maior influência
// primeiro e, em empate, menor `objectId` — dois quadros com a mesma cena
// produzem a mesma lista, sem cintilação por ordem de iteração.
inline u32 selectPunctualLights(std::span<const SceneLight> lights, const float camera[3],
                                std::span<PunctualLight> out, LightBudgetReport &report,
                                std::span<u32> outSources = {}) {
  report = {};
  if (out.empty() || !camera) return 0;
  std::array<u32, 64> ordered{};
  u32 candidates = 0;
  for (u32 i = 0; i < lights.size(); ++i) {
    const auto &light = lights[i];
    if (light.modality == LightModality::Directional) {
      if (!lightContributes(light)) continue;
      if (report.directionalAccepted == 0) ++report.directionalAccepted;
      else ++report.directionalDropped;
      continue;
    }
    if (!lightContributes(light)) continue;
    if (candidates == ordered.size()) { ++report.punctualDropped; continue; }
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
  const u32 accepted = candidates < out.size() ? candidates : static_cast<u32>(out.size());
  report.punctualAccepted = accepted;
  report.punctualDropped += candidates - accepted;
  for (u32 i = 0; i < accepted; ++i) {
    const auto &light = lights[ordered[i]];
    // De qual luz da cena veio esta vaga: sem isso o renderer não tem como
    // reatar a sombra autorada ao slot que o shader lê.
    if (i < outSources.size()) outSources[i] = ordered[i];
    auto &target = out[i];
    // O bloco é memória persistente: sem isto a vaga herdaria o tile de sombra
    // de outra luz do quadro anterior. O renderer escreve a sombra depois.
    target.shadow[0] = -1; target.shadow[1] = 0; target.shadow[2] = 1; target.shadow[3] = 1;
    for (u32 axis = 0; axis < 3; ++axis) target.positionRange[axis] = light.position[axis];
    target.positionRange[3] = light.range;
    for (u32 axis = 0; axis < 3; ++axis)
      target.colorIntensity[axis] = light.color[axis] * light.intensity;
    detail::normalized(light.direction, target.directionOffset);
    if (light.modality == LightModality::Spot) {
      // Janela suave do cone, pré-calculada como no glTF: o fragmento só faz
      // `clamp(cos * scale + offset, 0, 1)`, sem trigonometria por pixel.
      const float toRadians = 0.0174532925199433f;
      const float outer = std::cos(std::fmin(std::fmax(light.outerAngle, 0.f), 89.f) * toRadians);
      const float inner = std::cos(std::fmin(std::fmax(light.innerAngle, 0.f), 89.f) * toRadians);
      const float span = inner - outer > 1e-4f ? inner - outer : 1e-4f;
      target.colorIntensity[3] = 1.f / span;
      target.directionOffset[3] = -outer / span;
    } else {
      // Pontual: a mesma conta precisa render 1 em qualquer direção, para que o
      // fragmento não tenha um desvio por tipo de luz.
      target.colorIntensity[3] = 0;
      target.directionOffset[3] = 1;
    }
  }
  return accepted;
}

// A luz direcional que o sol do ambiente passa a usar, ou nullptr quando a cena
// não tem nenhuma. O consumidor é o sol existente, com cascatas: é por isso que
// a segunda direcional é excedente e não uma segunda sombra.
inline const SceneLight *selectDirectionalLight(std::span<const SceneLight> lights) {
  const SceneLight *chosen = nullptr;
  for (const auto &light : lights) {
    if (light.modality != LightModality::Directional || !lightContributes(light)) continue;
    if (!chosen || light.intensity > chosen->intensity ||
        (light.intensity == chosen->intensity && light.objectId < chosen->objectId))
      chosen = &light;
  }
  return chosen;
}

} // namespace ae::renderer
