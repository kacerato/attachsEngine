#include "harness.h"
#include "renderer/spatial_render_chunks.h"

#include <array>
#include <cmath>

using namespace ae;
using namespace ae::renderer;

AE_TEST(Spatial_chunks_bound_shader_rotations_under_nonuniform_scale) {
  std::array<float, 12> vertices{-2,-3,0, 2,-3,0, 2,3,0, -2,3,0};
  std::array<u32, 6> indices{0,1,2,0,2,3};
  std::array<MapMaterialRecord, 1> materials{};
  materials[0].flags = MapMaterialAlphaMask | MapMaterialImpostor;
  MapDrawRecord draw{};
  draw.indexCount = 6;
  draw.model[0]=1; draw.model[5]=2; draw.model[10]=5; draw.model[15]=1;
  draw.model[12]=50;
  MapPackageView package{};
  package.header.vertexCount=4; package.header.vertexStride=12;
  package.header.triangleCount=2;
  package.vertices={reinterpret_cast<const u8*>(vertices.data()), sizeof(vertices)};
  package.indices=indices; package.materials=materials; package.draws.push_back(draw);
  SpatialRenderChunks chunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {}, chunks), "rotating bounds built");
  for (u32 step=0; step<64; ++step) {
    const float angle=static_cast<float>(step)*6.2831853f/64.0f;
    for (u32 vertex=0; vertex<4; ++vertex) {
      const float x=vertices[vertex*3]*std::cos(angle);
      const float y=vertices[vertex*3+1]*2;
      const float z=-vertices[vertex*3]*std::sin(angle)*5;
      AE_EXPECT_TRUE(std::sqrt(x*x+y*y+z*z)<=chunks.draws[0].boundsRadius+0.0001f,
                     "all shader orientations stay inside cull bounds");
    }
  }
  AE_EXPECT_EQ(chunks.draws[0].boundsCenter[0], 50.0f, "world-space rotation centre");
}

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

AE_TEST(Spatial_camera_water_stays_contiguous_but_finite_water_splits) {
  std::array<float, 36> vertices{}; std::array<u32, 12> indices{};
  std::array<MapMaterialRecord, 1> materials{}; std::array<MapDrawRecord, 1> draws{};
  MapPackageView package = fixture(vertices, indices, materials, draws);
  materials[0].flags = MapMaterialWater | MapMaterialWaterCameraGrid;
  SpatialRenderChunks chunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {1, 1}, chunks), "camera water build");
  AE_EXPECT_EQ(chunks.draws.size(), 1u, "camera-relative geometry stays one draw");
  for (u32 index = 0; index < indices.size(); ++index)
    AE_EXPECT_EQ(chunks.indices[index], indices[index], "grid triangle order preserved");
  materials[0].flags = MapMaterialWater;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {1, 1}, chunks), "finite water build");
  AE_EXPECT_EQ(chunks.draws.size(), 4u, "finite water retains spatial culling granularity");
}

AE_TEST(Spatial_water_chunks_include_shader_displacement_in_bounds) {
  std::array<float, 36> vertices{}; std::array<u32, 12> indices{};
  std::array<MapMaterialRecord, 1> materials{}; std::array<MapDrawRecord, 1> draws{};
  MapPackageView package = fixture(vertices, indices, materials, draws);
  materials[0].flags = MapMaterialWater;
  SpatialRenderChunks staticChunks, animatedChunks;
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {2, 2, 0.0f}, staticChunks),
                 "static water bounds");
  AE_EXPECT_TRUE(buildSpatialRenderChunks(package, {2, 2, 1.75f}, animatedChunks),
                 "animated water bounds");
  AE_EXPECT_EQ(animatedChunks.draws.size(), staticChunks.draws.size(),
               "allowance does not change spatial partition");
  for (u32 index = 0; index < animatedChunks.draws.size(); ++index)
    AE_EXPECT_TRUE(std::abs(animatedChunks.draws[index].boundsRadius -
                            staticChunks.draws[index].boundsRadius - 1.75f) < 1.0e-5f,
                   "each chunk encloses the complete wave envelope");
}
