#pragma once
#include "scene/components.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {
// LOD Group da Unity, na mesma forma: cada nível aponta para um objeto ABAIXO
// deste (LOD0, LOD1... como filhos), e a transição de um nível é a altura
// relativa na tela — tamanho do grupo projetado sobre a altura da vista —
// abaixo da qual ele deixa de ser usado. Abaixo da transição do último nível
// o grupo inteiro some (Culled).
//
// O nível escolhido fica visível com toda a sua subárvore; os outros níveis
// escondem as deles. Um objeto que aparece em mais de um nível (a base comum,
// por exemplo) fica visível sempre que um desses níveis está ativo.
//
// Fade Mode como na Unity: None troca direto; Cross Fade desenha os dois níveis
// com cobertura complementar (o mesmo dither do LOD do pacote de mapa), numa
// faixa no fim de cada nível — Fade Transition Width, proporção do comprimento
// do nível — ou, com Animate Cross-fading, por tempo a cada troca.
inline constexpr u32 LodGroupMaximumLevels = 4;
enum class LodFadeMode : u32 { None = 0, CrossFade = 1 };
// LODGroup.crossFadeAnimationDuration da Unity (estático, 0,5 s por padrão).
inline constexpr float LodCrossFadeAnimationSeconds = .5f;

class LodGroup final : public ComponentValue {
public:

#include "scene/generated/lod_group_LodGroup_fields0.inc"

  u32 levelCount = 3;
  // Porcentagem da altura da vista. Os padrões são os da Unity para três níveis.
  std::array<float, LodGroupMaximumLevels> transitions{60, 30, 10, 5};
  std::array<u64, LodGroupMaximumLevels> levels{};
  // Tamanho do grupo em unidades locais (LODGroup.size): a maior extensão dos
  // limites do nível mais detalhado. Multiplicado pela maior escala global.
  float size = 1;
  LodFadeMode fadeMode = LodFadeMode::None;

#include "scene/generated/lod_group_LodGroup_fields2.inc"

  // Proporção (0..1) do comprimento de cada nível em que ele cruza com o
  // próximo. A documentação da Unity não fixa um padrão; 0,2 é o da Astra.
  std::array<float, LodGroupMaximumLevels> fadeWidths{.2f, .2f, .2f, .2f};
  // ForceLOD da Unity: estado de EXECUÇÃO. 0 é a seleção automática, n força o
  // LOD n-1. Nunca é gravado (write/read ignoram) e não aparece no Inspector:
  // só um script no Play escreve, e o Play parte sempre do automático.
  u32 forcedLevel = 0;
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<LodGroup>(*this); }
  bool valid() const override {
    if (levelCount < 1 || levelCount > LodGroupMaximumLevels) return false;
    if (!std::isfinite(size) || size <= 0) return false;
    for (u32 i = 0; i < levelCount; ++i) {
      const float t = transitions[i];
      if (!std::isfinite(t) || t <= 0 || t > 100) return false;
      // Estritamente decrescente: dois níveis na mesma altura nunca seriam escolhidos.
      if (i && t >= transitions[i - 1]) return false;
      if (levels[i] > std::numeric_limits<u32>::max()) return false;
      if (!std::isfinite(fadeWidths[i]) || fadeWidths[i] < 0 || fadeWidths[i] > 1) return false;
    }
    return static_cast<u32>(fadeMode) <= 1 && forcedLevel <= levelCount;
  }
  void write(std::ostream &out) const override {
    out << levelCount << ' ' << size;
    for (u32 i = 0; i < LodGroupMaximumLevels; ++i) out << ' ' << transitions[i] << ' ' << levels[i];
    out << ' ' << static_cast<u32>(fadeMode) << ' ' << animateCrossFading;
    for (const float width : fadeWidths) out << ' ' << width;
    out << ' ' << enabled;
  }
  bool read(std::istream &in, u32 version) override {
    // v1 não tinha Fade Mode: lê como None, que era o único comportamento.
    if (version < 1 || version > 3 || !(in >> levelCount >> size)) return false;
    for (u32 i = 0; i < LodGroupMaximumLevels; ++i)
      if (!(in >> transitions[i] >> levels[i])) return false;
    fadeMode = LodFadeMode::None;
    animateCrossFading = false;
    fadeWidths = {.2f, .2f, .2f, .2f};
    forcedLevel = 0;
    enabled = true;
    if (version >= 2) {
      u32 mode = 0;
      if (!(in >> mode >> animateCrossFading) || mode > 1) return false;
      fadeMode = static_cast<LodFadeMode>(mode);
      for (float &width : fadeWidths) if (!(in >> width)) return false;
    }
    if (version >= 3 && !(in >> enabled)) return false;
    return valid();
  }
};

inline bool lodHasLevel1(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 1; }
inline bool lodHasLevel2(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 2; }
inline bool lodHasLevel3(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 3; }
inline bool lodCrossFades(const ComponentValue &v) { return static_cast<const LodGroup &>(v).fadeMode == LodFadeMode::CrossFade; }
// Fade Transition Width só vale sem animação, como na Unity.
inline bool lodFadeWidthVisible(const ComponentValue &v) {
  const auto &g = static_cast<const LodGroup &>(v);
  return g.fadeMode == LodFadeMode::CrossFade && !g.animateCrossFading;
}
inline bool lodFadeWidth1(const ComponentValue &v) { return lodFadeWidthVisible(v) && lodHasLevel1(v); }
inline bool lodFadeWidth2(const ComponentValue &v) { return lodFadeWidthVisible(v) && lodHasLevel2(v); }
inline bool lodFadeWidth3(const ComponentValue &v) { return lodFadeWidthVisible(v) && lodHasLevel3(v); }
inline bool lodNever(const ComponentValue &) { return false; }

inline constexpr std::array<ComponentNumber, 9> lodGroupNumbers{{
#define AE_LOD_TRANSITION(i, visible)                                                                         \
  {"Transição LOD " #i, .1f, 100, .5f,                                                                        \
   [](const ComponentValue &v) -> const float & { return static_cast<const LodGroup &>(v).transitions[i]; }, \
   [](ComponentValue &v) -> float * { return &static_cast<LodGroup &>(v).transitions[i]; },                  \
   "transition_" #i,                                                                                         \
   {"Níveis", "%", "Altura na tela abaixo da qual este nível deixa de ser usado", visible}}
  AE_LOD_TRANSITION(0, nullptr),
  AE_LOD_TRANSITION(1, lodHasLevel1),
  AE_LOD_TRANSITION(2, lodHasLevel2),
  AE_LOD_TRANSITION(3, lodHasLevel3),
#undef AE_LOD_TRANSITION
  {"Tamanho", .001f, 1000000, .1f,
   [](const ComponentValue &v) -> const float & { return static_cast<const LodGroup &>(v).size; },
   [](ComponentValue &v) -> float * { return &static_cast<LodGroup &>(v).size; }, "size",
   {"Limites", "", "Maior extensão do nível mais detalhado; Recalcular mede a malha"}},
#define AE_LOD_FADE(i, visible)                                                                              \
  {"Largura do fade LOD " #i, 0, 1, .05f,                                                                   \
   [](const ComponentValue &v) -> const float & { return static_cast<const LodGroup &>(v).fadeWidths[i]; }, \
   [](ComponentValue &v) -> float * { return &static_cast<LodGroup &>(v).fadeWidths[i]; },                 \
   "fade_width_" #i, {"Fade", "", "Proporção do nível em que ele cruza com o próximo", visible}}
  AE_LOD_FADE(0, lodFadeWidthVisible),
  AE_LOD_FADE(1, lodFadeWidth1),
  AE_LOD_FADE(2, lodFadeWidth2),
  AE_LOD_FADE(3, lodFadeWidth3)
#undef AE_LOD_FADE
}};
inline constexpr std::array<ComponentEnumOption, 4> lodGroupLevelOptions{{{1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}}};
inline constexpr std::array<ComponentEnumOption, 2> lodGroupFadeOptions{{{0, "Nenhum"}, {1, "Cross Fade"}}};
inline constexpr std::array<ComponentEnumOption, 5> lodGroupForceOptions{{
  {0, "Automático"}, {1, "LOD 0"}, {2, "LOD 1"}, {3, "LOD 2"}, {4, "LOD 3"}}};
inline constexpr std::array<ComponentEnum, 3> lodGroupEnums{{
  {"level_count", "Níveis", lodGroupLevelOptions,
   [](const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount; },
   [](ComponentValue &v, u32 value) { static_cast<LodGroup &>(v).levelCount = value; }, {"Níveis"}},
  {"fade_mode", "Fade Mode", lodGroupFadeOptions,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const LodGroup &>(v).fadeMode); },
   [](ComponentValue &v, u32 value) { static_cast<LodGroup &>(v).fadeMode = static_cast<LodFadeMode>(value); }, {"Fade"}},
  // Só por script (LODGroup.ForceLOD); invisível no Inspector e fora do arquivo.
  {"force_level", "Forçar nível", lodGroupForceOptions,
   [](const ComponentValue &v) { return static_cast<const LodGroup &>(v).forcedLevel; },
   [](ComponentValue &v, u32 value) { static_cast<LodGroup &>(v).forcedLevel = value; },
   {"Execução", "", "Estado de execução: 0 automático, n força o LOD n-1", lodNever}}
}};
#include "scene/generated/lod_group_lodGroupBooleans.inc"
inline constexpr std::array<ComponentObjectReference, 4> lodGroupReferences{{
#define AE_LOD_LEVEL(i, visible)                                                                               \
  {"level_" #i, "Objetos LOD " #i, "", ObjectReferenceScope::Descendant, "Nenhum",                            \
   [](const ComponentValue &v) { return static_cast<const LodGroup &>(v).levels[i]; },                       \
   [](ComponentValue &v, u64 id) { static_cast<LodGroup &>(v).levels[i] = id; }, {"Níveis", "", nullptr, visible}}
  AE_LOD_LEVEL(0, nullptr),
  AE_LOD_LEVEL(1, lodHasLevel1),
  AE_LOD_LEVEL(2, lodHasLevel2),
  AE_LOD_LEVEL(3, lodHasLevel3)
#undef AE_LOD_LEVEL
}};
inline const ComponentType LodGroup::descriptor{
  "astra.render.lod_group", 3, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LodGroup>(); },
  lodGroupNumbers, lodGroupBooleans, lodGroupEnums, nullptr, false, lodGroupReferences
};

// Altura relativa na tela, a métrica da Unity: o tamanho do grupo em mundo
// sobre a altura que a vista enxerga àquela distância. Ortográfica não depende
// da distância. Entrada degenerada devolve infinito — o nível mais detalhado,
// o mesmo lado seguro da seleção por erro do pacote de mapa.
inline float lodRelativeHeight(float worldSize, float distance, float fovYRadians, float orthographicHalfHeight) {
  if (!std::isfinite(worldSize) || worldSize <= 0) return std::numeric_limits<float>::infinity();
  if (orthographicHalfHeight > 0 && std::isfinite(orthographicHalfHeight)) return worldSize / (2 * orthographicHalfHeight);
  if (!std::isfinite(distance) || !std::isfinite(fovYRadians) || fovYRadians <= 0 || fovYRadians >= 3.14159f)
    return std::numeric_limits<float>::infinity();
  if (distance <= 0) return std::numeric_limits<float>::infinity();
  return worldSize / (2 * distance * std::tan(fovYRadians * .5f));
}
// Índice do nível a usar, ou `levelCount` quando o grupo está abaixo do último
// nível (Culled). ForceLOD vence a altura.
inline u32 selectLodGroupLevel(const LodGroup &group, float relativeHeight) {
  if (group.forcedLevel) return std::min(group.forcedLevel - 1, group.levelCount - 1);
  for (u32 i = 0; i < group.levelCount && i < LodGroupMaximumLevels; ++i)
    if (relativeHeight * 100 >= group.transitions[i]) return i;
  return group.levelCount;
}
// Cross Fade sem animação: quanto o nível `level` já cedeu ao seguinte (0..1),
// pela posição da altura dentro da faixa no fim dele. A faixa é a proporção
// `fadeWidths[level]` do comprimento do nível — de 100% ao limite no LOD 0, do
// limite anterior ao seu nos outros. Zero fora da faixa.
inline float lodGroupFadeFactor(const LodGroup &group, u32 level, float relativeHeight) {
  if (group.fadeMode != LodFadeMode::CrossFade || group.animateCrossFading || group.forcedLevel ||
      level >= group.levelCount || !std::isfinite(relativeHeight))
    return 0;
  const float bottom = group.transitions[level], top = level ? group.transitions[level - 1] : 100.0f;
  const float zone = group.fadeWidths[level] * std::max(0.0f, top - bottom);
  const float height = relativeHeight * 100;
  if (zone <= 0 || height < bottom || height >= bottom + zone) return 0;
  return std::clamp(1 - (height - bottom) / zone, 0.0f, 1.0f);
}
} // namespace ae::scene
