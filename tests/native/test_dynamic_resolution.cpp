#include "harness.h"
#include "renderer/dynamic_resolution.h"

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
  AE_EXPECT_TRUE(!controller.observe(10.0f).changed, "primeiro pico nao oscila escala");
  AE_EXPECT_TRUE(!controller.observe(10.0f).changed, "segundo pico ainda acumula");
  AE_EXPECT_TRUE(controller.observe(10.0f).changed, "sobrecarga sustentada reduz");
  AE_EXPECT_EQ(controller.scale(), 0.90f, "degrau configurado");
  for (u32 index = 0; index < 30; ++index) controller.observe(12.0f);
  AE_EXPECT_TRUE(controller.scale() >= 0.70f, "nunca atravessa o piso");
}

AE_TEST(dynamic_resolution_recupera_so_com_headroom_sustentado) {
  DynamicResolutionController controller;
  controller.reset(settings(), 8.0f);
  for (u32 index = 0; index < 3; ++index) controller.observe(10.0f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "precondicao reduzida");
  for (u32 index = 0; index < 5; ++index) controller.observe(6.0f);
  AE_EXPECT_EQ(controller.scale(), 0.90f, "abaixo do budget mas sem headroom nao sobe");
  for (u32 index = 0; index < 5; ++index) controller.observe(5.0f);
  AE_EXPECT_EQ(controller.scale(), 0.95f, "headroom sustentado recupera um degrau");
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
