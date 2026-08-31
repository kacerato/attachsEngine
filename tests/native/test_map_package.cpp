#include "harness.h"
#include "renderer/map_package.h"
#include "renderer/static_collision_mesh.h"

#include <array>
#include <cstring>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
void setWord(std::vector<u8> &bytes, usize offset, u32 value) {
  for (u32 shift = 0; shift < 4; ++shift) bytes[offset + shift] = static_cast<u8>(value >> (shift * 8));
}
void setWide(std::vector<u8> &bytes, usize offset, u64 value) {
  setWord(bytes, offset, static_cast<u32>(value));
  setWord(bytes, offset + 4, static_cast<u32>(value >> 32));
}
void setFloat(std::vector<u8> &bytes, usize offset, float value) {
  u32 encoded = 0;
  std::memcpy(&encoded, &value, sizeof(value));
  setWord(bytes, offset, encoded);
}
std::vector<u8> packageFixture(u32 version = MapPackageVersion,
                               u32 vertexStride = MapVertexStride) {
  std::vector<u8> bytes(144 + 16 + 80 + 96 + 80 + 12);
  setWord(bytes, 0, MapPackageMagic); setWord(bytes, 4, version);
  setWord(bytes, 8, MapPackageHeaderSize); setWord(bytes, 12, vertexStride);
  for (usize offset : {16u, 20u, 24u, 28u}) setWord(bytes, offset, 1);
  setWord(bytes, 32, 3); setWide(bytes, 40, 144); setWide(bytes, 48, 160);
  setWide(bytes, 56, 240); setWide(bytes, 64, 336); setWide(bytes, 72, 416);
  setFloat(bytes, 124, .1f); setFloat(bytes, 128, 1000); setWord(bytes, 132, 1);
  auto *material = reinterpret_cast<MapMaterialRecord *>(bytes.data() + 160);
  for (u32 &texture : material->textureIndices) texture = InvalidMapTexture;
  auto *draw = reinterpret_cast<MapDrawRecord *>(bytes.data() + 240);
  draw->indexCount = 3;
  return bytes;
}
}

AE_TEST(Map_package_decodes_bounded_versioned_sections) {
  auto bytes = packageFixture();
  MapPackageView view;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, view), "valid fixture");
  AE_EXPECT_EQ(view.header.version, MapPackageVersion, "current version");
  AE_EXPECT_EQ(view.header.vertexStride, MapVertexStride, "packed vertex stride");
  AE_EXPECT_EQ(view.header.vertexCount, 1u, "vertex count");
  AE_EXPECT_EQ(view.draws[0].indexCount, 3u, "draw count");
  AE_EXPECT_TRUE(view.contentFingerprint != 0, "package identity");
  MapPackageView same;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, same), "same fixture");
  AE_EXPECT_EQ(view.contentFingerprint, same.contentFingerprint, "stable package identity");
  bytes[36] = 1; // reserved header byte remains a valid package but changes exact content.
  MapPackageView changed;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, changed), "changed valid fixture");
  AE_EXPECT_TRUE(view.contentFingerprint != changed.contentFingerprint, "content change identity");
}

AE_TEST(Map_package_rejects_version_ranges_and_material_references) {
  auto bytes = packageFixture();
  MapPackageView view;
  setWord(bytes, 4, MapPackageVersion + 1);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "future version");
  setWord(bytes, 4, MapPackageVersion);
  reinterpret_cast<MapDrawRecord *>(bytes.data() + 240)->materialIndex = 2;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "invalid material");
}

AE_TEST(Map_package_keeps_legacy_v1_readable_and_rejects_mixed_layouts) {
  MapPackageView view;
  auto legacy = packageFixture(1, MapVertexStrideV1);
  AE_EXPECT_TRUE(decodeMapPackage(legacy, view), "legacy v1 package");
  AE_EXPECT_EQ(view.header.version, 1u, "legacy version");
  AE_EXPECT_EQ(view.header.vertexStride, MapVertexStrideV1, "legacy float stride");
  auto mixed = packageFixture(1, MapVertexStride);
  AE_EXPECT_TRUE(!decodeMapPackage(mixed, view), "v1 cannot claim packed v2 layout");
}

AE_TEST(Static_collision_transforms_world_includes_blend_and_skips_cutout_cards) {
  std::array<CollisionVertex, 9> vertices{{
      {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
      {0.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 1.0f},
      {0.0f, 4.0f, 0.0f}, {1.0f, 4.0f, 0.0f}, {0.0f, 4.0f, 1.0f},
  }};
  const std::array<u32, 9> indices{0, 1, 2, 3, 4, 5, 6, 7, 8};
  std::array<MapMaterialRecord, 3> materials{};
  materials[1].flags = MapMaterialBlend;
  materials[2].flags = MapMaterialAlphaMask;
  std::array<MapDrawRecord, 3> draws{};
  for (MapDrawRecord &draw : draws) {
    draw.indexCount = 3;
    draw.model[0] = draw.model[5] = draw.model[10] = draw.model[15] = 1.0f;
  }
  draws[0].model[12] = 10.0f;
  draws[0].model[13] = 20.0f;
  draws[0].model[14] = 30.0f;
  draws[1].firstIndex = 3;
  draws[1].materialIndex = 1;
  draws[2].firstIndex = 6;
  draws[2].materialIndex = 2;

  MapPackageView package{};
  package.header.vertexCount = static_cast<u32>(vertices.size());
  package.header.vertexStride = sizeof(CollisionVertex);
  package.header.indexCount = static_cast<u32>(indices.size());
  package.materials = materials;
  package.draws = draws;
  package.vertices = {reinterpret_cast<const u8 *>(vertices.data()), sizeof(vertices)};
  package.indices = indices;

  StaticCollisionMesh collision;
  AE_EXPECT_TRUE(buildStaticCollisionMesh(package, collision), "world collision build");
  AE_EXPECT_EQ(collision.vertices.size(), 6u, "blend included and cutout card excluded");
  AE_EXPECT_EQ(collision.indices.size(), 6u, "opaque and blend triangles");
  AE_EXPECT_TRUE(collision.vertices[0].x == 10.0f && collision.vertices[0].y == 20.0f &&
                     collision.vertices[0].z == 30.0f,
                 "column-major world translation applied");
  AE_EXPECT_EQ(collision.indices[0], 0u, "first index");
  AE_EXPECT_EQ(collision.indices[1], 2u, "winding reversed");
  AE_EXPECT_EQ(collision.indices[2], 1u, "winding reversed");
}
