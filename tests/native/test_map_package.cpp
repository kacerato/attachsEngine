#include "harness.h"
#include "renderer/map_package.h"
#include "renderer/static_collision_mesh.h"

#include <array>
#include <cstring>
#include <limits>
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
usize alignUp16(usize value) { return (value + 15) & ~static_cast<usize>(15); }

// Builds a one-texture/one-material/one-draw/one-vertex/three-index fixture
// for the given version, computing section offsets from the version's actual
// on-disk draw record stride (96 bytes for v1/v2, 108 for v3) instead of
// hardcoding them -- hardcoded offsets would silently go stale the next time
// MapDrawRecord's on-disk layout changes. Deliberately writes the draw
// record field-by-field via setWord/setFloat rather than
// reinterpret_cast<MapDrawRecord*>: that struct is v3-shaped in memory, and
// casting it onto a v1/v2-strided buffer location would write 12 bytes past
// the real record -- exactly the bug decodeMapPackage's own per-version
// unpack exists to avoid.
std::vector<u8> packageFixture(u32 version = MapPackageVersion,
                               u32 vertexStride = MapVertexStride,
                               u32 lodLevel = 0, float geometricError = 0.0f, u32 lodGroupId = 0) {
  const u32 drawStride = version >= 3 ? MapDrawRecordStride : MapDrawRecordStrideV1V2;
  const usize textureOffset = 144;
  const usize materialOffset = textureOffset + 16;
  const usize drawOffset = materialOffset + 80;
  const usize vertexOffset = alignUp16(drawOffset + drawStride);
  const usize indexOffset = alignUp16(vertexOffset + vertexStride);
  const usize totalSize = indexOffset + 12; // Three u32 indices.

  std::vector<u8> bytes(totalSize);
  setWord(bytes, 0, MapPackageMagic); setWord(bytes, 4, version);
  setWord(bytes, 8, MapPackageHeaderSize); setWord(bytes, 12, vertexStride);
  for (usize offset : {16u, 20u, 24u, 28u}) setWord(bytes, offset, 1);
  setWord(bytes, 32, 3);
  setWide(bytes, 40, textureOffset); setWide(bytes, 48, materialOffset);
  setWide(bytes, 56, drawOffset); setWide(bytes, 64, vertexOffset); setWide(bytes, 72, indexOffset);
  setFloat(bytes, 124, .1f); setFloat(bytes, 128, 1000); setWord(bytes, 132, 1);
  auto *material = reinterpret_cast<MapMaterialRecord *>(bytes.data() + materialOffset);
  for (u32 &texture : material->textureIndices) texture = InvalidMapTexture;
  setWord(bytes, drawOffset + 4, 3); // indexCount.
  if (version >= 3) {
    setWord(bytes, drawOffset + 96, lodLevel);
    setFloat(bytes, drawOffset + 100, geometricError);
    setWord(bytes, drawOffset + 104, lodGroupId);
  }
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
  AE_EXPECT_EQ(view.draws[0].lodLevel, 0u, "v3 lodLevel round-trips");
  AE_EXPECT_TRUE(view.draws[0].geometricError == 0.0f, "v3 geometricError round-trips");
  AE_EXPECT_EQ(view.draws[0].lodGroupId, 0u, "v3 lodGroupId round-trips");
  AE_EXPECT_TRUE(view.contentFingerprint != 0, "package identity");
  MapPackageView same;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, same), "same fixture");
  AE_EXPECT_EQ(view.contentFingerprint, same.contentFingerprint, "stable package identity");
  bytes[36] = 1; // reserved header byte remains a valid package but changes exact content.
  MapPackageView changed;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, changed), "changed valid fixture");
  AE_EXPECT_TRUE(view.contentFingerprint != changed.contentFingerprint, "content change identity");
}

AE_TEST(Map_camera_water_rejects_incompatible_or_invalid_vertex_contract) {
  auto bytes = packageFixture();
  MapPackageView view;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, view), "base fixture");
  const usize vertex = static_cast<usize>(view.header.vertexOffset);
  auto *material = reinterpret_cast<MapMaterialRecord *>(bytes.data() + 160);
  material->flags = MapMaterialWaterCameraGrid;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "requires water shading");
  material->flags |= MapMaterialWater;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "zero grid metadata rejected");
  setFloat(bytes, vertex + 36, 3.0f);
  setFloat(bytes, vertex + 40, 8000.0f);
  AE_EXPECT_TRUE(decodeMapPackage(bytes, view), "valid camera grid");
  setFloat(bytes, vertex + 36, std::numeric_limits<float>::quiet_NaN());
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "nonfinite spacing rejected");
}

AE_TEST(Map_package_validates_multiview_layout_and_preserves_legacy_impostors) {
  auto bytes = packageFixture();
  auto *material = reinterpret_cast<MapMaterialRecord *>(bytes.data() + 160);
  material->flags = MapMaterialImpostor | MapMaterialAlphaMask;
  MapPackageView view;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, view), "legacy zero-metadata impostor remains valid");
  material->reserved = 4u | (5u << 4u) | (4u << 8u) | (2u << 12u);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "multiview requires replacement normal");
  material->flags |= MapMaterialNormalMap;
  material->textureIndices[0] = material->textureIndices[1] = 0;
  AE_EXPECT_TRUE(decodeMapPackage(bytes, view), "valid eight-view material");
  material->reserved &= ~(15u << 12u);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "zero rows rejected before GPU use");
  material->reserved |= 3u << 12u;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "non-power-of-two rows unsupported");
  material->reserved = 0x10000u;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "truncated push-constant metadata rejected");
}

AE_TEST(Map_package_rejects_version_ranges_and_material_references) {
  auto bytes = packageFixture();
  MapPackageView view;
  setWord(bytes, 4, MapPackageVersion + 1);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "future version");
  setWord(bytes, 4, MapPackageVersion);
  // drawOffset is 240 regardless of version (materialOffset(160) + one
  // 80-byte MapMaterialRecord) -- only vertexOffset/indexOffset shift with
  // the draw record's on-disk stride. materialIndex sits at the fourth u32
  // field (offset +12), a position unchanged since v1.
  setWord(bytes, 240 + 12, 2);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "invalid material");
}

AE_TEST(Map_package_rejects_non_finite_or_negative_geometric_error) {
  MapPackageView view;
  auto nanError = packageFixture(MapPackageVersion, MapVertexStride, 0,
                                 std::numeric_limits<float>::quiet_NaN(), 0);
  AE_EXPECT_TRUE(!decodeMapPackage(nanError, view), "NaN geometricError rejected");
  auto negativeError = packageFixture(MapPackageVersion, MapVertexStride, 0, -1.0f, 0);
  AE_EXPECT_TRUE(!decodeMapPackage(negativeError, view), "negative geometricError rejected");
}

AE_TEST(Map_package_rejects_invalid_lod_contract_fields) {
  MapPackageView view;
  AE_EXPECT_TRUE(!decodeMapPackage(
      packageFixture(MapPackageVersion, MapVertexStride, MapMaximumLodLevels, 1.0f, 0), view),
      "LOD level above the package contract is rejected");
  AE_EXPECT_TRUE(!decodeMapPackage(
      packageFixture(MapPackageVersion, MapVertexStride, 1, 1.0f, 1), view),
      "LOD group outside draw count is rejected");
  AE_EXPECT_TRUE(!decodeMapPackage(
      packageFixture(MapPackageVersion, MapVertexStride, 0, 0.5f, 0), view),
      "finest LOD must have zero geometric error");
}

AE_TEST(Map_package_accepts_the_baked_impostor_level_above_the_cooker_levels) {
  // O cooker gera tres niveis de simplificacao; o quarto e o impostor que
  // tools/bake-foliage-impostors.py emenda depois. Sem este caso, subir
  // MapMaximumLodLevels ficaria coberto so pelo teste de rejeicao acima, que
  // passa igual se o limite estiver alto demais.
  MapPackageView view;
  AE_EXPECT_TRUE(decodeMapPackage(
      packageFixture(MapPackageVersion, MapVertexStride, MapMaximumLodLevels - 1, 16.0f, 0), view),
      "coarsest legal level decodes");
  AE_EXPECT_EQ(view.draws[0].lodLevel, MapMaximumLodLevels - 1, "impostor level round-trips");
  AE_EXPECT_TRUE(view.draws[0].geometricError == 16.0f, "impostor geometric error round-trips");
}

AE_TEST(Map_package_keeps_legacy_v1_readable_and_rejects_mixed_layouts) {
  MapPackageView view;
  auto legacy = packageFixture(1, MapVertexStrideV1);
  AE_EXPECT_TRUE(decodeMapPackage(legacy, view), "legacy v1 package");
  AE_EXPECT_EQ(view.header.version, 1u, "legacy version");
  AE_EXPECT_EQ(view.header.vertexStride, MapVertexStrideV1, "legacy float stride");
  AE_EXPECT_EQ(view.draws[0].lodLevel, 0u, "legacy package defaults to LOD level 0");
  AE_EXPECT_TRUE(view.draws[0].geometricError == 0.0f, "legacy package has zero geometric error");
  AE_EXPECT_EQ(view.draws[0].lodGroupId, 0u, "legacy package's only draw is its own trivial LOD group");
  auto mixed = packageFixture(1, MapVertexStride);
  AE_EXPECT_TRUE(!decodeMapPackage(mixed, view), "v1 cannot claim packed v2 layout");
}

AE_TEST(Map_package_v2_also_defaults_lod_fields_to_no_lod) {
  MapPackageView view;
  auto v2 = packageFixture(2, MapVertexStride);
  AE_EXPECT_TRUE(decodeMapPackage(v2, view), "v2 package (no LOD fields on disk)");
  AE_EXPECT_EQ(view.header.version, 2u, "v2 version");
  AE_EXPECT_EQ(view.draws[0].lodLevel, 0u, "v2 package defaults to LOD level 0");
  AE_EXPECT_TRUE(view.draws[0].geometricError == 0.0f, "v2 package has zero geometric error");
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
  package.draws.assign(draws.begin(), draws.end());
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

  for (auto &material : materials) material.flags = MapMaterialWater;
  AE_EXPECT_TRUE(buildStaticCollisionMesh(package, collision),
                 "render-only water package is a valid empty collision world");
  AE_EXPECT_TRUE(collision.empty(), "water uses analytic queries, not static triangles");
}

AE_TEST(Static_collision_uses_only_authoritative_lod_zero_geometry) {
  std::array<CollisionVertex, 6> vertices{{
      {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
      {0.0f, 0.1f, 0.0f}, {1.0f, 0.1f, 0.0f}, {0.0f, 0.1f, 1.0f},
  }};
  const std::array<u32, 6> indices{0, 1, 2, 3, 4, 5};
  std::array<MapMaterialRecord, 1> materials{};
  std::array<MapDrawRecord, 2> draws{};
  for (MapDrawRecord &draw : draws) {
    draw.indexCount = 3;
    draw.model[0] = draw.model[5] = draw.model[10] = draw.model[15] = 1.0f;
  }
  draws[1].firstIndex = 3;
  draws[1].lodLevel = 1;
  draws[1].geometricError = 1.0f;
  draws[1].lodGroupId = 0;

  MapPackageView package{};
  package.header.vertexCount = static_cast<u32>(vertices.size());
  package.header.vertexStride = sizeof(CollisionVertex);
  package.header.indexCount = static_cast<u32>(indices.size());
  package.materials = materials;
  package.draws.assign(draws.begin(), draws.end());
  package.vertices = {reinterpret_cast<const u8 *>(vertices.data()), sizeof(vertices)};
  package.indices = indices;

  StaticCollisionMesh collision;
  AE_EXPECT_TRUE(buildStaticCollisionMesh(package, collision), "LOD collision build");
  AE_EXPECT_EQ(collision.vertices.size(), 3u, "coarse render vertices excluded");
  AE_EXPECT_EQ(collision.indices.size(), 3u, "coarse render triangle excluded");
}
