#pragma once
#include "scene/components.h"
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
// Sem transição suave nesta versão: a troca é direta, como Fade Mode = None na
// Unity. O cross-fade por dithering do pacote de mapa continua sendo do pacote.
inline constexpr u32 LodGroupMaximumLevels = 4;

class LodGroup final : public ComponentValue {
public:
  u32 levelCount = 3;
  // Porcentagem da altura da vista. Os padrões são os da Unity para três níveis.
  std::array<float, LodGroupMaximumLevels> transitions{60, 30, 10, 5};
  std::array<u64, LodGroupMaximumLevels> levels{};
  // Tamanho do grupo em unidades locais (LODGroup.size): a maior extensão dos
  // limites do nível mais detalhado. Multiplicado pela maior escala global.
  float size = 1;
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
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out << levelCount << ' ' << size;
    for (u32 i = 0; i < LodGroupMaximumLevels; ++i) out << ' ' << transitions[i] << ' ' << levels[i];
  }
  bool read(std::istream &in, u32 version) override {
    if (version != 1 || !(in >> levelCount >> size)) return false;
    for (u32 i = 0; i < LodGroupMaximumLevels; ++i)
      if (!(in >> transitions[i] >> levels[i])) return false;
    return true;
  }
};

inline bool lodHasLevel1(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 1; }
inline bool lodHasLevel2(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 2; }
inline bool lodHasLevel3(const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount > 3; }

inline constexpr std::array<ComponentNumber, 5> lodGroupNumbers{{
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
   {"Limites", "", "Maior extensão do nível mais detalhado; Recalcular mede a malha"}}
}};
inline constexpr std::array<ComponentEnumOption, 4> lodGroupLevelOptions{{{1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}}};
inline constexpr std::array<ComponentEnum, 1> lodGroupEnums{{
  {"level_count", "Níveis", lodGroupLevelOptions,
   [](const ComponentValue &v) { return static_cast<const LodGroup &>(v).levelCount; },
   [](ComponentValue &v, u32 value) { static_cast<LodGroup &>(v).levelCount = value; }, {"Níveis"}}
}};
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
  "astra.render.lod_group", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LodGroup>(); },
  lodGroupNumbers, {}, lodGroupEnums, nullptr, false, lodGroupReferences
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
// nível (Culled).
inline u32 selectLodGroupLevel(const LodGroup &group, float relativeHeight) {
  for (u32 i = 0; i < group.levelCount && i < LodGroupMaximumLevels; ++i)
    if (relativeHeight * 100 >= group.transitions[i]) return i;
  return group.levelCount;
}
} // namespace ae::scene
