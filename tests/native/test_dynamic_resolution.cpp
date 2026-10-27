#include "harness.h"
#include "renderer/dynamic_resolution.h"
#include "renderer/post_sample_bounds.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace ae;
using namespace ae::renderer;
using namespace ae::test;

namespace {
DynamicResolutionSettings settings() {
  DynamicResolutionSettings value{};
  value.enabled = true;
  value.minimumScale = 0.70f;
  value.maximumScale = 1.0f;
  value.decreaseStep = 0.10f;
  value.increaseStep = 0.05f;
  value.overloadFrames = 3;
  value.recoveryFrames = 5;
  value.recoveryHeadroomRatio = 0.70f;
  return value;
}
}

AE_TEST(dynamic_resolution_cede_rapido_e_respeita_o_piso) {
  DynamicResolutionController controller;
  controller.reset(settings(), 8.0f);
  AE_EXPECT_TRUE(!controller.observe(8.5f).changed, "primeiro pico nao oscila escala");
  AE_EXPECT_TRUE(!controller.observe(8.5f).changed, "segundo pico ainda acumula");
  AE_EXPECT_TRUE(controller.observe(8.5f).changed, "sobrecarga sustentada reduz");
  AE_EXPECT_EQ(controller.scale(), 0.90f, "excesso pequeno desce o degrau minimo");
  for (u32 index = 0; index < 60; ++index) controller.observe(12.0f);
  AE_EXPECT_EQ(controller.scale(), 0.70f, "sobrecarga longa para no piso, sem atravessar");
}

AE_TEST(dynamic_resolution_recupera_so_com_headroom_sustentado) {
  DynamicResolutionController controller;
  controller.reset(settings(), 8.0f);
  for (u32 index = 0; index < 3; ++index) controller.observe(8.5f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "precondicao reduzida");
  for (u32 index = 0; index < 20; ++index) controller.observe(6.0f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "abaixo do budget mas dentro da histerese nao sobe");
  for (u32 index = 0; index < 4; ++index) controller.observe(5.0f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "folga ainda curta (mediana precisa virar)");
  for (u32 index = 0; index < 3; ++index) controller.observe(5.0f);
  AE_EXPECT_EQ(controller.scale(), 0.95f, "headroom sustentado recupera um degrau");
}

AE_TEST(dynamic_resolution_pico_isolado_nao_derruba_e_oscilacao_no_limite_desce) {
  DynamicResolutionController controller;
  controller.reset(settings(), 8.0f);
  for (u32 index = 0; index < 10; ++index) controller.observe(7.0f);
  controller.observe(30.0f);
  for (u32 index = 0; index < 10; ++index) controller.observe(7.0f);
  AE_EXPECT_EQ(controller.scale(), 1.0f, "um quadro de 30 ms (compilacao, upload) nao muda a escala");

  // 8,6 / 7,9 ms num budget de 8: a media passa do budget, mas nenhum par de
  // quadros seguidos fica acima. A contagem de quadros seguidos nunca descia.
  controller.reset(settings(), 8.0f);
  bool dropped = false;
  for (u32 index = 0; index < 12 && !dropped; ++index)
    dropped = controller.observe(index % 2 == 0 ? 8.6f : 7.9f).changed;
  AE_EXPECT_TRUE(dropped, "GPU oscilando em volta do budget desce");
}

AE_TEST(dynamic_resolution_espera_as_amostras_da_escala_velha) {
  DynamicResolutionController controller;
  controller.reset(settings(), 8.0f);
  for (u32 index = 0; index < 3; ++index) controller.observe(8.5f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "primeira descida");
  // Os 3 quadros em voo ainda medem a escala cheia: nao contam.
  for (u32 index = 0; index < 3; ++index)
    AE_EXPECT_TRUE(!controller.observe(8.5f).changed, "amostra da escala velha descartada");
  AE_EXPECT_EQ(controller.scale(), 0.90f, "o mesmo excesso nao corta duas vezes");
  for (u32 index = 0; index < 3; ++index) controller.observe(7.2f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "na escala nova o quadro cabe e fica");
}

AE_TEST(dynamic_resolution_degrau_proporcional_ao_excesso) {
  DynamicResolutionController controller;
  auto config = settings();
  config.minimumScale = 0.5f;
  controller.reset(config, 8.0f);
  for (u32 index = 0; index < 3; ++index) controller.observe(16.0f);
  // Area ~ escala^2: 16 ms num budget de 8 pediria ~0,69; o teto do degrau segura em 0,85.
  AE_EXPECT_TRUE(std::fabs(controller.scale() - 0.85f) < 1.0e-4f,
                 "excesso de 2x desce o teto do degrau");
  AE_EXPECT_EQ(controller.decisionGpuMilliseconds(), 16.0f, "a troca guarda a mediana que a decidiu");
  const float before = controller.scale();
  for (u32 index = 0; index < 3; ++index) controller.observe(10.5f);
  for (u32 index = 0; index < 3; ++index) controller.observe(10.5f);
  const float fitted = before * std::sqrt(0.95f * 8.0f / 10.5f);
  AE_EXPECT_TRUE(std::fabs(controller.scale() - fitted) < 1.0e-4f,
                 "excesso medio desce o que a area pede");
}

AE_TEST(dynamic_resolution_reconfiguracao_termica_nao_salta_para_o_teto) {
  DynamicResolutionSettings settings{};
  settings.enabled = true;
  settings.minimumScale = 0.5f;
  settings.maximumScale = 1.0f;
  settings.decreaseStep = 0.25f;
  settings.overloadFrames = 1;
  DynamicResolutionController controller;
  controller.reset(settings, 7.0f);
  for (u32 index = 0; index < 12; ++index) controller.observe(9.0f);
  AE_EXPECT_EQ(controller.scale(), 0.5f, "carga levou a escala ao piso");

  settings.maximumScale = 0.75f;
  controller.reconfigure(settings, 7.0f);
  AE_EXPECT_EQ(controller.scale(), 0.5f,
               "pressao termica preserva a escala ja reduzida em vez de subir");

  settings.minimumScale = 0.6f;
  controller.reconfigure(settings, 7.0f);
  AE_EXPECT_EQ(controller.scale(), 0.6f, "novos limites ainda sao respeitados");
}

AE_TEST(dynamic_resolution_ignora_amostra_invalida_e_modo_desligado) {
  DynamicResolutionController controller;
  auto config = settings();
  config.enabled = false;
  controller.reset(config, 8.0f);
  for (u32 index = 0; index < 10; ++index) controller.observe(20.0f);
  AE_EXPECT_EQ(controller.scale(), 1.0f, "desligado preserva maximo");
  config.enabled = true;
  controller.reset(config, 8.0f);
  controller.observe(std::numeric_limits<float>::quiet_NaN());
  controller.observe(0.0f);
  AE_EXPECT_EQ(controller.scale(), 1.0f, "telemetria ausente nao vira degradacao");
}

AE_TEST(dynamic_resolution_extent_alinha_sem_estourar) {
  AE_EXPECT_EQ(scaledRenderExtent(2772, 1.0f), 2772u, "escala cheia preserva o painel");
  AE_EXPECT_EQ(scaledRenderExtent(1280, 0.75f), 960u, "escala conhecida");
  AE_EXPECT_EQ(scaledRenderExtent(0, 0.75f), 1u, "zero falha seguro");
  AE_EXPECT_EQ(scaledRenderExtent(100, std::numeric_limits<float>::quiet_NaN()), 100u,
               "escala invalida usa cheio");
}

namespace {
// Linha da tela lendo o bloom em 1/2 como o passe principal: uv da cena presa
// com meio texel da cena (clampSourceUv) e, com a correção, também ao canto
// ativo do alvo em 1/2. Fora do canto, o alvo guarda um quadro antigo (1000).
float brightestBloomRead(u32 target, float scale, bool halfClamp) {
  const u32 internal = scaledRenderExtent(target, scale);
  const u32 allocation = (target + 1) / 2;
  std::vector<float> half(allocation, 1000.0f);
  for (u32 i = 0; i < halfActiveExtent(internal) && i < allocation; ++i) half[i] = 1.0f;
  const auto texel = [&](int i) { return half[static_cast<usize>(std::clamp(i, 0, static_cast<int>(allocation) - 1))]; };
  const float active = static_cast<float>(internal) / static_cast<float>(target);
  float brightest = 0.0f;
  for (u32 x = 0; x < target; ++x) {
    float uv = (static_cast<float>(x) + 0.5f) / static_cast<float>(target) * active;
    uv = std::clamp(uv, 0.5f / static_cast<float>(target), active - 0.5f / static_cast<float>(target));
    if (halfClamp) uv = clampHalfUv(uv, internal, allocation);
    const float p = uv * static_cast<float>(allocation) - 0.5f, base = std::floor(p), f = p - base;
    const float value = texel(static_cast<int>(base)) * (1.0f - f) + texel(static_cast<int>(base) + 1) * f;
    brightest = std::max(brightest, value);
  }
  return brightest;
}
}

AE_TEST(dynamic_resolution_borda_da_tela_nao_le_texel_fora_do_alvo_em_meia) {
  // 2772 x 1280 do POCO: as duas dimensões, nas escalas que o nível C usa.
  for (u32 target : {2772u, 1280u}) {
    for (float scale : {1.0f, 0.85f, 0.7f, 0.55f}) {
      AE_EXPECT_EQ(brightestBloomRead(target, scale, true), 1.0f,
                   "com o limite do alvo em 1/2 a borda nao muda com a escala");
    }
  }
  AE_EXPECT_TRUE(brightestBloomRead(1280, 0.55f, false) > 1.5f,
                 "o limite antigo (meio texel da cena) puxa o texel nao escrito para a borda");
  AE_EXPECT_EQ(halfActiveExtent(705), 353u, "ceil(interna/2), como o renderer grava o nivel");
}

AE_TEST(post_primeira_linha_da_vista_nao_mistura_o_fundo) {
  // Coluna de 1280 texels; a vista começa em fração (101,6 logo abaixo da barra)
  // e termina em 1280. Texels com centro dentro valem 1 (cena); os de fora, o
  // fundo da câmera (0 de dia, 8 no céu da noite). Cada pixel lê a cena no
  // próprio centro, preso aos limites, com bilinear.
  for (float start : {101.6f, 100.0f, 100.4f, 0.0f}) {
    for (float background : {0.0f, 8.0f}) {
      std::vector<float> column(1280);
      for (u32 i = 0; i < 1280; ++i) column[i] = static_cast<float>(i) + 0.5f >= start ? 1.0f : background;
      const auto read = [&](float texel, bool newBounds) {
        const float lower = newBounds ? firstDrawnTexelCenter(start) : start + 0.5f;
        const float upper = newBounds ? lastDrawnTexelCenter(1280.0f) : 1280.0f - 0.5f;
        const float p = std::clamp(texel, lower, upper) - 0.5f, base = std::floor(p), f = p - base;
        const auto at = [&](int i) { return column[static_cast<usize>(std::clamp(i, 0, 1279))]; };
        return at(static_cast<int>(base)) * (1.0f - f) + at(static_cast<int>(base) + 1) * f;
      };
      for (u32 row = static_cast<u32>(std::ceil(start - 0.5f)); row < 1280; ++row)
        AE_EXPECT_TRUE(std::fabs(read(static_cast<float>(row) + 0.5f, true) - 1.0f) < 1.0e-5f,
                       "toda linha desenhada le so a cena, sem o fundo da camera");
      if (start == 101.6f)
        AE_EXPECT_TRUE(std::fabs(read(101.5f, false) - 1.0f) > .3f,
                       "o limite antigo (borda + meio texel) misturava o fundo na primeira linha");
    }
  }
}
