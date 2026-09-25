#include "resources/mesh_lod_build.h"
#include "meshoptimizer.h"

#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {
constexpr u32 CacheSize = 16;
}

bool buildMeshLods(std::span<const u8> vertices, std::vector<u32> &indices,
                   std::span<const renderer::MapDrawRecord> draws, std::span<const u8> deformed,
                   const MeshLodSettings &settings, std::vector<renderer::MeshLodLevel> &out,
                   MeshLodReport &report, std::string &diagnostic) {
  out.clear();
  report = {};
  diagnostic.clear();
  const u64 vertexCount = vertices.size() / renderer::MapVertexStride;
  // Toda faixa é conferida antes de qualquer escrita: falhar no meio deixaria
  // metade das faixas reordenadas.
  std::vector<u32> localCount(draws.size(), 0);
  for (usize d = 0; d < draws.size(); ++d) {
    const auto &draw = draws[d];
    if (u64(draw.firstIndex) + draw.indexCount > indices.size()) {
      diagnostic = "Desenho com índices fora do buffer; LOD não gerado.";
      return false;
    }
    u32 highest = 0;
    for (u32 i = 0; i < draw.indexCount; ++i) highest = std::max(highest, indices[draw.firstIndex + i]);
    if (draw.indexCount && u64(draw.vertexOffset) + highest >= vertexCount) {
      diagnostic = "Desenho com vértices fora do buffer; LOD não gerado.";
      return false;
    }
    localCount[d] = draw.indexCount ? highest + 1 : 0;
  }
  if (!settings.generate && !settings.optimizeOrder) return true;
  const u32 maximumLevels = std::clamp(settings.maximumLevels, 1u, renderer::MeshLodMaximumLevels);
  double missesBefore = 0, missesAfter = 0, triangles = 0;
  std::vector<u32> scratch, level0;
  for (usize d = 0; d < draws.size(); ++d) {
    const auto &draw = draws[d];
    const u32 count = draw.indexCount - draw.indexCount % 3;
    if (count < 3) continue;
    const u32 localVertices = localCount[d];
    u32 *range = indices.data() + draw.firstIndex;
    const float *positions = reinterpret_cast<const float *>(vertices.data() + u64(draw.vertexOffset) * renderer::MapVertexStride);
    const double drawTriangles = count / 3.0;
    const auto before = meshopt_analyzeVertexCache(range, count, localVertices, CacheSize, 0, 0);
    missesBefore += before.acmr * drawTriangles;
    if (settings.optimizeOrder) {
      scratch.resize(count);
      meshopt_optimizeVertexCache(scratch.data(), range, count, localVertices);
      std::copy(scratch.begin(), scratch.end(), range);
    }
    missesAfter += meshopt_analyzeVertexCache(range, count, localVertices, CacheSize, 0, 0).acmr * drawTriangles;
    triangles += drawTriangles;
    report.sourceTriangles += count / 3;
    if (!settings.generate || maximumLevels < 2 || report.budgetReached) continue;
    if (d < deformed.size() && deformed[d]) {++report.skippedDeformed; continue;}
    if (count / 3 < settings.minimumTriangles) {++report.skippedSmall; continue;}
    // Cópia do nível 0: acrescentar níveis realoca `indices`.
    level0.assign(range, range + count);
    const float scale = meshopt_simplifyScale(positions, localVertices, renderer::MapVertexStride);
    u32 previous = count;
    float previousError = 0;
    bool any = false;
    for (u32 level = 1; level < maximumLevels; ++level) {
      const u32 target = std::max<u32>(settings.stopTriangles * 3, (previous / 2) / 3 * 3);
      scratch.resize(count);
      float relative = 0;
      const u32 produced = static_cast<u32>(meshopt_simplify(scratch.data(), level0.data(), count, positions, localVertices,
                                                             renderer::MapVertexStride, target,
                                                             settings.maximumRelativeError, meshopt_SimplifyLockBorder,
                                                             &relative));
      // Sem redução real (bordas presas ou teto de erro): o nível seguinte
      // seria o mesmo desenho com outro nome.
      if (produced < 3 || produced > previous * 85 / 100) break;
      if (report.addedIndices + produced > settings.maximumAddedIndices) {report.budgetReached = true; break;}
      scratch.resize(produced);
      if (settings.optimizeOrder) {
        std::vector<u32> ordered(produced);
        meshopt_optimizeVertexCache(ordered.data(), scratch.data(), produced, localVertices);
        scratch.swap(ordered);
      }
      renderer::MeshLodLevel lod;
      lod.draw = static_cast<u32>(d);
      lod.level = level;
      lod.firstIndex = static_cast<u32>(indices.size());
      lod.indexCount = produced;
      // Erro absoluto no espaço da malha, nunca menor que o do nível anterior:
      // a seleção pressupõe erros não decrescentes.
      lod.geometricError = std::max(previousError, relative * scale);
      indices.insert(indices.end(), scratch.begin(), scratch.end());
      range = indices.data() + draw.firstIndex;
      previousError = lod.geometricError;
      out.push_back(lod);
      report.addedIndices += produced;
      report.lodTriangles += produced / 3;
      ++report.levels;
      any = true;
      previous = produced;
      if (produced / 3 < settings.stopTriangles * 2) break;
    }
    report.drawsWithLods += any;
  }
  if (triangles > 0) {
    report.acmrBefore = static_cast<float>(missesBefore / triangles);
    report.acmrAfter = static_cast<float>(missesAfter / triangles);
  }
  return true;
}
} // namespace ae::resources
