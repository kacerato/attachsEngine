#include "harness.h"
#include "renderer/spatial_render_chunks.h"

#include <array>

using namespace ae;
using namespace ae::renderer;

namespace {
MapPackageView fixture(std::array<float, 36> &vertices, std::array<u32, 12> &indices,
                       std::array<MapMaterialRecord, 1> &materials,
                       std::array<MapDrawRecord, 1> &draws) {
  for (u32 triangle = 0; triangle < 4; ++triangle) {
    const float x = static_cast<float>(triangle) * 100.0f;
    vertices[triangle * 9 + 0] = x; vertices[triangle * 9 + 1] = 0;
    vertices[triangle * 9 + 2] = 0; vertices[triangle * 9 + 3] = x + 1;
    vertices[triangle * 9 + 4] = 0; vertices[triangle * 9 + 5] = 0;
    vertices[triangle * 9 + 6] = x; vertices[triangle * 9 + 7] = 1;
    vertices[triangle * 9 + 8] = 0;
    indices[triangle * 3 + 0] = triangle * 3;
    indices[triangle * 3 + 1] = triangle * 3 + 1;
    indices[triangle * 3 + 2] = triangle * 3 + 2;
  }
  draws[0].indexCount = static_cast<u32>(indices.size());
  draws[0].model[0] = draws[0].model[5] = draws[0].model[10] = draws[0].model[15] = 1;
  MapPackageView package{};
  package.header.vertexCount = 12;
  package.header.vertexStride = 3 * sizeof(float);
  package.header.indexCount = static_cast<u32>(indices.size());
  package.header.triangleCount = 4;
  package.materials = materials;
  package.draws.assign(draws.begin(), draws.end());
  package.vertices = {reinterpret_cast<const u8 *>(vertices.data()), sizeof(vertices)};
  package.indices = indices;
  return package;
}
}

AE_TEST(Spatial_render_chunks_preserve_triangles_and_split_distant_geometry) {
  std::array<float, 36> vertices{}; std::array<u32, 12> indices{};
  std::array<MapMaterialRecord, 1> materials{}; std::array<MapDrawRecord, 1> draws{};
  const MapPackageView package = fixture(vertices, indices, materials, draws);
  SpatialRenderChunks chunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {2, 2}, chunks), "chunk build");
  AE_EXPECT_EQ(chunks.draws.size(), 2u, "two bounded chunks");
  AE_EXPECT_EQ(chunks.indices.size(), indices.size(), "all indices preserved");
  AE_EXPECT_EQ(chunks.draws[0].indexCount, 6u, "first triangle budget");
  AE_EXPECT_EQ(chunks.draws[1].indexCount, 6u, "second triangle budget");
  AE_EXPECT_TRUE(chunks.draws[0].boundsCenter[0] < chunks.draws[1].boundsCenter[0],
                 "spatially ordered bounds");
}

AE_TEST(Spatial_render_chunks_keep_blended_primitive_order_intact) {
  std::array<float, 36> vertices{}; std::array<u32, 12> indices{};
  std::array<MapMaterialRecord, 1> materials{}; std::array<MapDrawRecord, 1> draws{};
  MapPackageView package = fixture(vertices, indices, materials, draws);
  materials[0].flags = MapMaterialBlend;
  SpatialRenderChunks chunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {1, 1}, chunks), "blend build");
  AE_EXPECT_EQ(chunks.draws.size(), 1u, "blend remains one draw");
  for (u32 index = 0; index < indices.size(); ++index)
    AE_EXPECT_EQ(chunks.indices[index], indices[index], "blend order preserved");
}


AE_TEST(Spatial_render_chunks_use_finer_global_budget_for_alpha_coverage) {
  std::array<float, 36> vertices{}; std::array<u32, 12> indices{};
  std::array<MapMaterialRecord, 1> materials{}; std::array<MapDrawRecord, 1> draws{};
  MapPackageView package = fixture(vertices, indices, materials, draws);
  materials[0].flags = MapMaterialAlphaMask;
  SpatialRenderChunks chunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {8, 1}, chunks), "coverage build");
  AE_EXPECT_EQ(chunks.draws.size(), 4u, "coverage usa orçamento fino por classe de material");
  for (const MapDrawRecord &draw : chunks.draws)
    AE_EXPECT_EQ(draw.indexCount, 3u, "cada chunk respeita orçamento coverage");
}
