#pragma once
#include "core/base.h"
#include "renderer/map_package.h"
#include "renderer/mesh_lod.h"
#include <span>
#include <string>
#include <vector>

namespace ae::resources {
// Geração de LOD e ordem de índices na importação (S3), com meshoptimizer.
//
// Regras do gerador de Mesh LOD da Unity 6.2: começa só com 256 triângulos ou
// mais; cada nível tem cerca de metade dos índices do anterior; para quando o
// limite de níveis chega, quando sobram menos de 64 triângulos ou quando a
// simplificação já não reduz; não cria vértices. Cada nível é simplificado a
// partir do nível 0 (o erro não se acumula) com as bordas presas, para dois
// desenhos vizinhos não abrirem fresta entre si.
//
// "Otimizar ordem" é o Optimize Mesh > Polygon Order da Unity: reordena os
// triângulos de cada faixa para o cache de vértices da GPU (Forsyth/Tom), sem
// mudar a malha. O ACMR (vértices processados por triângulo) antes e depois é
// medido e relatado — o ganho só é afirmado por essa medida.
struct MeshLodSettings {
  bool generate = false;
  bool optimizeOrder = false;
  u32 maximumLevels = renderer::MeshLodMaximumLevels; // conta o nível 0
  u32 minimumTriangles = 256;
  u32 stopTriangles = 64;
  // Teto do erro relativo à extensão da malha (1% = .01). Acima disso o nível
  // não seria escolhido perto de nada útil e só custaria memória.
  float maximumRelativeError = .1f;
  // Índices que os níveis extras podem acrescentar ao arquivo inteiro.
  u64 maximumAddedIndices = 12ull << 20;
};

struct MeshLodReport {
  u32 drawsWithLods = 0, levels = 0, skippedSmall = 0, skippedDeformed = 0;
  u64 sourceTriangles = 0, lodTriangles = 0, addedIndices = 0;
  bool budgetReached = false;
  // ACMR médio ponderado por triângulo (cache FIFO de 16 vértices).
  float acmrBefore = 0, acmrAfter = 0;
};

// `deformed[d]` != 0 marca desenho com skin ou blend shapes: sem LOD (o gerador
// da Unity também ignora pesos e deformações) e ordem só reordenada, que não
// muda nada para o skin. Os níveis são acrescentados ao fim de `indices`.
// Falha fechada: desenho com faixa fora do buffer devolve falso sem alterar nada.
bool buildMeshLods(std::span<const u8> vertices, std::vector<u32> &indices,
                   std::span<const renderer::MapDrawRecord> draws, std::span<const u8> deformed,
                   const MeshLodSettings &settings, std::vector<renderer::MeshLodLevel> &out,
                   MeshLodReport &report, std::string &diagnostic);
} // namespace ae::resources
