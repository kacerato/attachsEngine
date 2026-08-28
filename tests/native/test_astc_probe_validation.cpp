#include "harness.h"
#include "platform/astc_probe_validation.h"
#include "profiler/gpu_timestamp.h"
#include <array>
#include <cmath>
#include <limits>

AE_TEST(GpuTimestamp_rejeita_medicao_ausente_em_vez_de_zero_ms) {
  double ms = 0;
  AE_EXPECT_TRUE(!ae::profiler::gpuTimestampMilliseconds(0, 1, 0, 1, ms), "fila sem timestamps");
  AE_EXPECT_TRUE(std::isnan(ms), "ausencia nao e tempo zero");
  AE_EXPECT_TRUE(!ae::profiler::gpuTimestampMilliseconds(1, 1, 64, 1, ms), "delta nulo");
  AE_EXPECT_TRUE(!ae::profiler::gpuTimestampMilliseconds(0, 1, 65, 1, ms), "bits invalidos");
  for (double period : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    AE_EXPECT_TRUE(!ae::profiler::gpuTimestampMilliseconds(0, 1, 64, period, ms), "periodo invalido");
}

AE_TEST(GpuTimestamp_aceita_wrap_e_mascara_bits_superiores) {
  double ms;
  AE_EXPECT_TRUE(ae::profiler::gpuTimestampMilliseconds(250, 259, 8, 1.0e6, ms), "wrap 8 bits");
  AE_EXPECT_EQ(ms, 9.0, "delta modular");
  AE_EXPECT_TRUE(ae::profiler::gpuTimestampMilliseconds(UINT64_MAX - 1, 1, 64, 1.0e6, ms), "wrap 64 bits");
  AE_EXPECT_EQ(ms, 3.0, "sem shift indefinido em 64 bits");
}

AE_TEST(Astc_corpus_inclui_variacao_dentro_do_bloco_e_bordas_impares) {
  std::array<ae::u8, 13 * 7 * 4> pixels{};
  AE_EXPECT_TRUE(ae::platform::fillAstcProbePattern(pixels, 13, 7), "dimensoes impares");
  AE_EXPECT_TRUE(pixels[4 * 4] != pixels[5 * 4], "rampa intrabloco");
  AE_EXPECT_TRUE(pixels[8 * 4] != pixels[9 * 4], "checker intrabloco");
  AE_EXPECT_TRUE(!ae::platform::fillAstcProbePattern(pixels, 0, 7), "dimensao zero");
  AE_EXPECT_TRUE(!ae::platform::fillAstcProbePattern(pixels, UINT32_MAX, UINT32_MAX), "overflow");
}

AE_TEST(Astc_comparacao_verifica_ultimo_texel_e_alpha) {
  std::array<ae::u8, 64> source{}, decoded{};
  decoded.back() = 255;
  ae::u32 difference;
  AE_EXPECT_TRUE(ae::platform::compareAstcProbePixels(source, decoded, difference), "comparacao valida");
  AE_EXPECT_EQ(difference, 255u, "nao pular texels nem alpha");
  AE_EXPECT_TRUE(!ae::platform::compareAstcProbePixels({}, {}, difference), "vazio nao passa");
  AE_EXPECT_TRUE(!ae::platform::compareAstcProbePixels(source, {}, difference), "tamanhos diferentes");
}
