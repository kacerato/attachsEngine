#include "harness.h"
#include "core/sha256.h"
#include "renderer/environment_map.h"
#include "resources/environment_map_asset.h"
#include "resources/image_decode.h"

#include <bit>
#include <cmath>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
void setWord(std::vector<u8> &bytes, usize offset, u32 value) {
  for (u32 byte = 0; byte < 4; ++byte)
    bytes[offset + byte] = static_cast<u8>(value >> (byte * 8u));
}

void setScalar(std::vector<u8> &bytes, usize offset, float value) {
  setWord(bytes, offset, std::bit_cast<u32>(value));
}

std::vector<u8> environmentFixture(u32 version) {
  const usize bytes = version == 1 ? 80 : version == 2 ? 144 : version == 3 ? 176 : 320;
  std::vector<u8> result(bytes);
  setWord(result, 0, EnvironmentResourceMagic);
  setWord(result, 4, version);
  setWord(result, 8, static_cast<u32>(bytes));
  return result;
}

std::vector<u8> constantRadianceHdr(u32 width=4,u32 height=2) {
  const std::string header="#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y "+std::to_string(height)+
                           " +X "+std::to_string(width)+"\n";
  std::vector<u8> result(header.begin(),header.end());
  // Width < 8 selects Radiance's legacy flat RGBE stream in stb_image.
  for(u32 pixel=0;pixel<width*height;++pixel)
    result.insert(result.end(),{128,128,128,129}); // linear RGB = 1
  return result;
}
bool alwaysCancelled(void *) { return true; }
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

AE_TEST(environment_map_v4_decodes_valid_diffuse_irradiance_without_inventing_legacy_data) {
  auto bytes = environmentFixture(4);
  constexpr usize trailer = 144;
  constexpr usize sh = 176;
  setWord(bytes, trailer, static_cast<u32>(EnvironmentProjection::Octahedral));
  setWord(bytes, trailer + 4, 256);
  setWord(bytes, trailer + 8, 256);
  setWord(bytes, trailer + 12, 9);
  setWord(bytes, trailer + 16, 128);
  setWord(bytes, trailer + 20, 128);
  setWord(bytes, trailer + 24, 1);
  setWord(bytes, trailer + 28, EnvironmentMapPrefilteredGgx |
                                    EnvironmentMapSplitSumBrdf |
                                    EnvironmentMapDiffuseIrradianceSh9);
  for (usize coefficient = 0; coefficient < DiffuseIrradianceShCoefficientCount;
       ++coefficient)
    for (usize channel = 0; channel < 3; ++channel)
      setScalar(bytes, sh + (coefficient * 4 + channel) * sizeof(float),
                static_cast<float>(coefficient * 3 + channel) * 0.125f);

  EnvironmentMapDescription description{};
  AE_EXPECT_TRUE(decodeEnvironmentMapDescription(bytes, description), "AEEN v4 válido");
  AE_EXPECT_TRUE(description.hasDiffuseIrradianceSh(), "SH9 declarado é reconhecido");
  AE_EXPECT_TRUE(std::abs(description.diffuseIrradianceSh[8][2] - 3.25f) < 1.0e-6f,
                 "coeficientes HDR preservados");

  setScalar(bytes, sh + 3 * sizeof(float), 1.0f);
  AE_EXPECT_TRUE(!decodeEnvironmentMapDescription(bytes, description),
                 "padding std140 não pode carregar dado ambíguo");
}

AE_TEST(diffuse_irradiance_sh_evaluator_matches_constant_and_upper_hemisphere) {
  constexpr float pi = 3.14159265358979323846f;
  constexpr float y00 = 0.2820947918f;
  constexpr float y10 = 0.4886025119f;
  DiffuseIrradianceSh9 constant{};
  const float radiance[3]{0.25f, 1.0f, 2.0f};
  for (usize channel = 0; channel < 3; ++channel)
    constant[0][channel] = 4.0f * pi * y00 * pi * radiance[channel];
  const std::array<std::array<float, 3>, 3> directions{{
      {0, 1, 0}, {1, 0, 0}, {0, -1, 0}}};
  for (const auto &direction : directions) {
    float irradiance[3]{};
    AE_EXPECT_TRUE(evaluateDiffuseIrradianceSh(constant, direction.data(), irradiance),
                   "avalia direção válida");
    for (usize channel = 0; channel < 3; ++channel)
      AE_EXPECT_TRUE(std::abs(irradiance[channel] - pi * radiance[channel]) < 2.0e-5f,
                     "radiância constante produz irradiância pi vezes radiância");
  }

  DiffuseIrradianceSh9 hemisphere{};
  hemisphere[0][0] = 2.0f * pi * y00 * pi;
  hemisphere[1][0] = pi * y10 * (2.0f * pi / 3.0f);
  float up[3]{}, side[3]{}, down[3]{};
  const float upDirection[3]{0, 1, 0}, sideDirection[3]{1, 0, 0}, downDirection[3]{0, -1, 0};
  AE_EXPECT_TRUE(evaluateDiffuseIrradianceSh(hemisphere, upDirection, up), "hemisfério acima");
  AE_EXPECT_TRUE(evaluateDiffuseIrradianceSh(hemisphere, sideDirection, side), "hemisfério lateral");
  AE_EXPECT_TRUE(evaluateDiffuseIrradianceSh(hemisphere, downDirection, down), "hemisfério abaixo");
  AE_EXPECT_TRUE(std::abs(up[0] - pi) < 2.0e-5f, "normal para cima recebe pi");
  AE_EXPECT_TRUE(std::abs(side[0] - pi * 0.5f) < 2.0e-5f, "normal lateral recebe pi/2");
  AE_EXPECT_TRUE(std::abs(down[0]) < 2.0e-5f, "normal para baixo não vê hemisfério superior");
}

AE_TEST(radiance_hdr_decoder_is_linear_bounded_and_does_not_claim_openexr) {
  const auto hdr=constantRadianceHdr();
  resources::DecodedHdrImage image;std::string diagnostic;
  AE_EXPECT_TRUE(resources::detectImageContainer(hdr)==resources::ImageContainer::RadianceHdr,
                 "assinatura Radiance RGBE reconhecida");
  AE_EXPECT_TRUE(resources::decodeRadianceHdrRgba32f(hdr,{},image,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(image.width==4&&image.height==2&&image.rgba.size()==32&&
                 std::abs(image.rgba[0]-1.0f)<1e-6f&&image.rgba[3]==1.0f,
                 "RGBE vira radiância linear RGBA32F");
  const std::array<u8,4> exr{0x76,0x2f,0x31,0x01};
  AE_EXPECT_TRUE(!resources::decodeRadianceHdrRgba32f(exr,{},image,diagnostic)&&
                 diagnostic.find("OpenEXR")!=std::string::npos,
                 "OpenEXR é recusado com contrato explícito");
}

AE_TEST(environment_map_import_derives_hdr_payloads_sh_and_versioned_cache) {
  resources::EnvironmentMapImportSettings settings;
  settings.panoramaWidth=64;settings.specularSize=16;settings.brdfSize=16;
  settings.specularSamples=16;settings.brdfSamples=32;
  resources::EnvironmentMapImportLimits limits;limits.maximumOutputBytes=2ull<<20;
  resources::EnvironmentMapImportSettings restoredSettings;
  AE_EXPECT_TRUE(resources::readEnvironmentMapImportSettings(
      resources::writeEnvironmentMapImportSettings(settings),restoredSettings)&&
      restoredSettings.panoramaWidth==settings.panoramaWidth&&
      resources::environmentMapCacheRelativePath(std::string(64,'a')).ends_with(".aemc"),
      "receita autoral e caminho de cache são determinísticos");
  renderer::SharedEnvironmentMap imported;std::string diagnostic;
  const auto hdr=constantRadianceHdr();
  AE_EXPECT_TRUE(resources::importRadianceEnvironmentMap(hdr,settings,limits,{},imported,diagnostic),
                 diagnostic.c_str());
  AE_EXPECT_TRUE(imported&&imported->valid()&&imported->panorama.width==64&&
                 imported->specular.levels==5&&imported->brdf.levels==1,
                 "panorama linear, GGX octahedral e BRDF foram derivados");
  const float up[3]{0,1,0};float irradiance[3]{};
  AE_EXPECT_TRUE(evaluateDiffuseIrradianceSh(imported->description.diffuseIrradianceSh,up,irradiance)&&
                 std::abs(irradiance[0]-3.14159265f)<.02f&&
                 std::abs(irradiance[1]-irradiance[0])<.001f,
                 "HDR constante produz SH9 de irradiância pi sem cor sintética");
  std::vector<u8> cache;
  AE_EXPECT_TRUE(resources::writeEnvironmentMapCache(*imported,cache),"cache serializado");
  renderer::SharedEnvironmentMap restored;
  AE_EXPECT_TRUE(resources::readEnvironmentMapCache(cache,imported->cacheKey,limits,restored)&&
                 restored&&restored->sourceHash==imported->sourceHash&&
                 restored->specular.texels==imported->specular.texels,
                 "cache versionado preserva identidade e payload HDR");
  cache.back()^=1;
  AE_EXPECT_TRUE(!resources::readEnvironmentMapCache(cache,imported->cacheKey,limits,restored),
                 "cache corrompido é recusado inteiro");
  AE_EXPECT_TRUE(resources::writeEnvironmentMapCache(*imported,cache),"cache refeito para validar chave");
  AE_EXPECT_TRUE(!resources::readEnvironmentMapCache(cache,"chave-errada",limits,restored),
                 "cache de outra receita é recusado");
  // Estrutura íntegra e checksum recalculado ainda não autorizam half Inf/NaN.
  constexpr usize firstPanoramaHalf=316;
  cache[firstPanoramaHalf]=0;cache[firstPanoramaHalf+1]=0x7c;
  const auto checksum=Sha256::hex(std::span<const u8>(cache).first(cache.size()-64));
  std::copy(checksum.begin(),checksum.end(),cache.end()-64);
  AE_EXPECT_TRUE(!resources::readEnvironmentMapCache(cache,imported->cacheKey,limits,restored),
                 "cache numericamente inválido é recusado mesmo com checksum correto");
}

AE_TEST(environment_map_import_cancels_before_allocating_derived_payloads) {
  resources::EnvironmentMapImportSettings settings;settings.panoramaWidth=64;
  settings.specularSize=16;settings.brdfSize=16;settings.specularSamples=16;settings.brdfSamples=32;
  renderer::SharedEnvironmentMap output;std::string diagnostic;
  const resources::EnvironmentMapCancel cancel{alwaysCancelled,nullptr};
  AE_EXPECT_TRUE(!resources::importRadianceEnvironmentMap(constantRadianceHdr(),settings,{},cancel,
                                                           output,diagnostic)&&!output&&
                 diagnostic.find("cancelada")!=std::string::npos,
                 "cancelamento não publica recurso parcial");
}

AE_TEST(material_feature_variant_is_compact_and_round_trips_texture_bits) {
  for (u32 variant = 0; variant < MaterialFeatureVariantCount; ++variant) {
    const u32 mask = materialFeatureMaskForVariant(variant);
    AE_EXPECT_EQ(materialFeatureVariant(mask), variant, "chave de variante reversível");
  }
  AE_EXPECT_EQ(materialFeatureVariant(0xffffffffu), 7u, "flags alheias não expandem a chave");
}
