#include "harness.h"
#include "resources/import_cache.h"
#include <algorithm>
#include <cstring>
#include <limits>

using namespace ae;
namespace {
resources::GltfImport controlCacheFixture() {
  resources::GltfImport model;
  model.nodes.resize(1);
  model.draws.resize(1);
  model.draws[0].indexCount = 3;
  model.drawNodes = {0};
  model.drawSkins = {-1};
  model.drawMorphs = {-1};
  model.vertices.resize(renderer::MapVertexStride * 3);
  model.indices = {0, 1, 2};
  model.names = {"terrain"};
  model.keys = {"terrain"};
  model.materials.resize(2);
  model.materials[0].flags|=renderer::MapMaterialTerrain;
  for (auto &m : model.materials)
    for (auto &i : m.textureIndices)
      i = renderer::InvalidMapTexture;
  auto texture = std::make_shared<renderer::AuthoringTexture>();
  texture->width = texture->height = 9;
  texture->levels = 1;
  texture->format = renderer::AuthoringTextureRgba8;
  texture->srgb = false;
  texture->samplerFlags = renderer::encodeGltfTextureSampler(9729, 9729);
  texture->mipChain.resize(9 * 9 * 4, 0);
  for (usize i = 0; i < texture->mipChain.size(); i += 4)
    texture->mipChain[i] = 255;
  model.textures.push_back(texture);
  model.textureBytes = texture->mipChain.size();
  model.terrainControls.resize(2);
  model.terrainControls[0].textureIndex = 0;
  model.terrainControls[0].uvScale[0] = .125f;
  model.terrainControls[0].uvScale[1] = .25f;
  model.terrainControls[0].uvOffset[0] = .5f;
  model.terrainControls[0].uvOffset[1] = .5f;
  return model;
}
void patch32(std::vector<u8> &bytes, usize offset, u32 value) {
  std::memcpy(bytes.data() + offset, &value, 4);
}
usize controlTail(const std::vector<u8> &bytes) {
  constexpr char marker[] = "ASTRATC1";
  auto it = std::search(bytes.begin(), bytes.end(), marker, marker + 8);
  return static_cast<usize>(it - bytes.begin());
}
} // namespace
AE_TEST(terrain_control_cache_roundtrip_and_legacy_tail_compatibility) {
  auto model = controlCacheFixture();
  std::vector<u8> bytes, again;
  AE_EXPECT_TRUE(resources::writeImportCache(model, "terrain-control", bytes),
                 "write control cache");
  resources::GltfImport restored;
  AE_EXPECT_TRUE(resources::readImportCache(bytes, "terrain-control", restored),
                 "read control cache");
  AE_EXPECT_EQ(restored.terrainControls.size(), usize(2),
               "bindings remain aligned with neutral material");
  AE_EXPECT_EQ(restored.terrainControls[0].textureIndex, 0u,
               "control texture reference");
  AE_EXPECT_EQ(restored.terrainControls[0].uvScale[1], .25f,
               "local mapping scale preserved");
  AE_EXPECT_EQ(restored.terrainControls[1].textureIndex,
               renderer::InvalidMapTexture, "neutral binding preserved");
  AE_EXPECT_EQ(restored.textures[0]->mipChain, model.textures[0]->mipChain,
               "full control texels preserved");
  AE_EXPECT_TRUE(
      resources::writeImportCache(restored, "terrain-control", again) &&
          again == bytes,
      "stable cache binary roundtrip");
  auto tail = controlTail(bytes);
  AE_EXPECT_TRUE(tail < bytes.size(), "versioned tail present");
  bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(tail),
              bytes.end() - 8);
  AE_EXPECT_TRUE(
      resources::readImportCache(bytes, "terrain-control", restored) &&
          restored.terrainControls.empty(),
      "old cache without terrain tail is still readable");
  AE_EXPECT_TRUE(resources::ImportCacheImporterRevision >= 8,
                 "new importer revision prevents old terrain cache replay");
}
AE_TEST(terrain_control_cache_rejects_malformed_bindings_and_texture_contract) {
  auto model = controlCacheFixture();
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeImportCache(model, "terrain-control", bytes),
                 "valid seed cache");
  const auto tail = controlTail(bytes);
  AE_EXPECT_TRUE(tail + 56 < bytes.size(), "tail bounds");
  resources::GltfImport out;
  for (const auto [offset, value] :
       std::vector<std::pair<usize, u32>>{{tail + 8, 2},
                                          {tail + 12, 1},
                                          {tail + 16, 99},
                                          {tail + 20, 0x7fc00000u},
                                          {tail + 24, 0xbf800000u},
                                          {tail + 28, 0x7f800000u},
                                          {tail + 40, 0x3f800000u}}) {
    auto corrupt = bytes;
    patch32(corrupt, offset, value);
    AE_EXPECT_TRUE(
        !resources::readImportCache(corrupt, "terrain-control", out) &&
            out.terrainControls.empty() && out.draws.empty(),
        "malformed control tail fails closed");
  }
  auto truncated = bytes;
  truncated.erase(truncated.end() - 10, truncated.end() - 8);
  AE_EXPECT_TRUE(!resources::readImportCache(truncated, "terrain-control", out),
                 "truncated binding rejected");
  auto bad = model;
  bad.materials[0].flags=0;
  AE_EXPECT_TRUE(!resources::writeImportCache(bad,"terrain-control",truncated),"control texture requires an actual terrain material");
  bad=model;
  bad.terrainControls.resize(1);
  AE_EXPECT_TRUE(
      !resources::writeImportCache(bad, "terrain-control", truncated) &&
          truncated.empty(),
      "unaligned binding count refused on write");
  const u32 metadata[] = {
      9, 9, 1, renderer::AuthoringTextureRgba8, model.textures[0]->samplerFlags,
      0};
  const auto *start = reinterpret_cast<const u8 *>(metadata);
  auto pos =
      std::search(bytes.begin(), bytes.end(), start, start + sizeof metadata);
  AE_EXPECT_TRUE(pos != bytes.end(), "locate control texture metadata");
  auto textureAt = static_cast<usize>(pos - bytes.begin());
  for (const auto [offset, value] : std::vector<std::pair<usize, u32>>{
           {textureAt + 16, model.textures[0]->samplerFlags |
                                renderer::AuthoringTextureRepeatU},
           {textureAt + 16, model.textures[0]->samplerFlags | (1u << 23)},
           {textureAt + 16, model.textures[0]->samplerFlags |
                                renderer::TextureSamplerMipEnabled},
           {textureAt + 20, 1},
           {textureAt, 8}}) {
    auto corrupt = bytes;
    patch32(corrupt, offset, value);
    AE_EXPECT_TRUE(
        !resources::readImportCache(corrupt, "terrain-control", out) &&
            out.draws.empty(),
        "invalid control dimensions/filter/color-space flags rejected");
  }
}
