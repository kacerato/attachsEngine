#include "harness.h"
#include "renderer/map_package.h"

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
std::vector<u8> packageFixture() {
  std::vector<u8> bytes(144 + 16 + 80 + 96 + 80 + 12);
  setWord(bytes, 0, MapPackageMagic); setWord(bytes, 4, MapPackageVersion);
  setWord(bytes, 8, MapPackageHeaderSize); setWord(bytes, 12, MapVertexStride);
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
  AE_EXPECT_EQ(view.header.vertexCount, 1u, "vertex count");
  AE_EXPECT_EQ(view.draws[0].indexCount, 3u, "draw count");
}

AE_TEST(Map_package_rejects_version_ranges_and_material_references) {
  auto bytes = packageFixture();
  MapPackageView view;
  setWord(bytes, 4, 2);
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "future version");
  setWord(bytes, 4, 1);
  reinterpret_cast<MapDrawRecord *>(bytes.data() + 240)->materialIndex = 2;
  AE_EXPECT_TRUE(!decodeMapPackage(bytes, view), "invalid material");
}
