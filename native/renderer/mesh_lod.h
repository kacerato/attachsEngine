#pragma once
#include "core/base.h"

namespace ae::renderer {
// LOD na própria malha (S3) — o Mesh LOD da Unity 6.2 e o LOD de importação da
// Godot 4: cada nível é outra faixa do buffer de índices sobre o MESMO buffer de
// vértices do desenho, sem objeto filho nem recurso separado. O nível 0 é o
// próprio desenho; `level` começa em 1 e cresce com a simplificação.
//
// `geometricError` é o desvio máximo do nível em relação à malha original, em
// unidades do espaço da malha (a escala do objeto multiplica na seleção). É a
// mesma grandeza que `MapDrawRecord::geometricError` do pacote de mapa, e por
// isso a seleção usa `computeScreenSpaceError`/`selectLodLevel` e o orçamento
// em pixels do painel Qualidade, sem uma segunda regra.
inline constexpr u32 MeshLodMaximumLevels = 4; // igual a LodMaximumLevelsPerGroup

struct MeshLodLevel {
  u32 draw = 0;       // desenho (nível 0) a que este nível pertence
  u32 level = 1;      // 1..MeshLodMaximumLevels-1
  u32 firstIndex = 0; // no mesmo buffer de índices; índices locais ao desenho
  u32 indexCount = 0;
  float geometricError = 0;
};
} // namespace ae::renderer
