#include "harness.h"
#include "renderer/dynamic_resolution.h"

#include <cmath>
#include <limits>

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
