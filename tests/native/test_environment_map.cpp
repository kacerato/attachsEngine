#include "harness.h"
#include "renderer/environment_map.h"

#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
void setWord(std::vector<u8> &bytes, usize offset, u32 value) {
  for (u32 byte = 0; byte < 4; ++byte)
    bytes[offset + byte] = static_cast<u8>(value >> (byte * 8u));
}

std::vector<u8> environmentFixture(u32 version) {
  const usize bytes = version == 1 ? 80 : version == 2 ? 144 : 176;
  std::vector<u8> result(bytes);
  setWord(result, 0, EnvironmentResourceMagic);
  setWord(result, 4, version);
  setWord(result, 8, static_cast<u32>(bytes));
  return result;
}
}

AE_TEST(environment_map_legacy_versions_decode_to_explicit_fallback) {
  for (u32 version : {1u, 2u}) {
    EnvironmentMapDescription description{};
    AE_EXPECT_TRUE(decodeEnvironmentMapDescription(environmentFixture(version), description),
                   "AEEN legado continua legível");
    AE_EXPECT_TRUE(!description.hasPrefilteredSpecular(), "legado não inventa radiância cozida");
    AE_EXPECT_TRUE(!description.hasSplitSumBrdf(), "legado não inventa BRDF LUT");
  }
}

AE_TEST(environment_map_v3_validates_prefiltered_resources) {
  auto bytes = environmentFixture(3);
  constexpr usize trailer = 144;
  setWord(bytes, trailer, static_cast<u32>(EnvironmentProjection::Octahedral));
  setWord(bytes, trailer + 4, 256);
  setWord(bytes, trailer + 8, 256);
  setWord(bytes, trailer + 12, 9);
  setWord(bytes, trailer + 16, 128);
  setWord(bytes, trailer + 20, 128);
  setWord(bytes, trailer + 24, 1);
  setWord(bytes, trailer + 28, EnvironmentMapPrefilteredGgx | EnvironmentMapSplitSumBrdf);
  EnvironmentMapDescription description{};
  AE_EXPECT_TRUE(decodeEnvironmentMapDescription(bytes, description), "AEEN v3 válido");
  AE_EXPECT_TRUE(description.hasPrefilteredSpecular(), "radiância octahedral reconhecida");
  AE_EXPECT_TRUE(description.hasSplitSumBrdf(), "split-sum reconhecido");
  setWord(bytes, trailer + 12, 0);
  AE_EXPECT_TRUE(!decodeEnvironmentMapDescription(bytes, description), "flag sem mips é rejeitada");
}

AE_TEST(material_feature_variant_is_compact_and_round_trips_texture_bits) {
  for (u32 variant = 0; variant < MaterialFeatureVariantCount; ++variant) {
    const u32 mask = materialFeatureMaskForVariant(variant);
    AE_EXPECT_EQ(materialFeatureVariant(mask), variant, "chave de variante reversível");
  }
  AE_EXPECT_EQ(materialFeatureVariant(0xffffffffu), 7u, "flags alheias não expandem a chave");
}
