#include "resources/mesh_derived.h"
#include "meshoptimizer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::resources {

bool validCollisionMeshRecipe(const CollisionMeshRecipe &recipe) noexcept {
  return recipe.source.valid() && recipe.trianglePercent >= 5 && recipe.trianglePercent <= 90 &&
         std::isfinite(recipe.maximumError) && recipe.maximumError >= 0.001f && recipe.maximumError <= 0.25f &&
         (!recipe.identity.valid() || recipe.identity != recipe.source);
}

AssetGuid collisionMeshGuid(const CollisionMeshRecipe &recipe) {
  if (!validCollisionMeshRecipe(recipe)) return {};
  if (recipe.identity.valid()) return recipe.identity;
  return assetGuidFromSeed("collision-mesh:v2:" + recipe.source.text());
}

bool buildCollisionMesh(std::span<const u8> vertices, std::span<const u32> indices,
                        const renderer::MapDrawRecord &source,
                        const CollisionMeshRecipe &recipe, CollisionMeshBuild &out,
                        std::string &diagnostic) {
  out = {};
  diagnostic.clear();
  if (!validCollisionMeshRecipe(recipe)) { diagnostic = "Parâmetros da malha física inválidos."; return false; }
  if (vertices.size() % renderer::MapVertexStride || source.indexCount < 3 || source.indexCount % 3 ||
      static_cast<u64>(source.firstIndex) + source.indexCount > indices.size()) {
    diagnostic = "Geometria fonte inválida para simplificação."; return false;
  }
  const auto sourceIndices = indices.subspan(source.firstIndex, source.indexCount);
  u32 vertexCount = 0;
  for (const auto index : sourceIndices) vertexCount = std::max(vertexCount, index + 1);
  const u64 totalVertices = vertices.size() / renderer::MapVertexStride;
  if (!vertexCount || static_cast<u64>(source.vertexOffset) + vertexCount > totalVertices) {
    diagnostic = "Índices da malha saem do buffer de vértices."; return false;
  }
  const usize targetTriangles = std::max<usize>(1, (source.indexCount / 3) * recipe.trianglePercent / 100);
  const usize targetIndices = targetTriangles * 3;
  out.indices.resize(source.indexCount);
  const auto *positions = reinterpret_cast<const float *>(vertices.data() +
      static_cast<usize>(source.vertexOffset) * renderer::MapVertexStride);
  const usize written = meshopt_simplify(out.indices.data(), sourceIndices.data(), sourceIndices.size(),
      positions, vertexCount, renderer::MapVertexStride, targetIndices, recipe.maximumError, 0, &out.resultingError);
  if (written < 3 || written % 3 || written > source.indexCount) {
    out = {}; diagnostic = "O simplificador não produziu triângulos válidos."; return false;
  }
  out.indices.resize(written);
  return true;
}

} // namespace ae::resources
