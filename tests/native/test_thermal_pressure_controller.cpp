#include "renderer/thermal_pressure_controller.h"
#include "harness.h"

using namespace ae::renderer;

AE_TEST(thermal_pressure_piora_imediatamente_por_headroom_ou_status) {
  ThermalPressureController controller;
  controller.reset();
  auto update = controller.observe({0, 0.86f, true, true});
  AE_EXPECT_EQ(update.pressure, ThermalPressure::Light, "headroom antecipa pressao leve");
  AE_EXPECT_TRUE(update.changed, "piora e imediata");
  update = controller.observe({3, 0.70f, true, true});
  AE_EXPECT_EQ(update.pressure, ThermalPressure::Severe, "status severo tem autoridade");
}

AE_TEST(thermal_pressure_recupera_um_nivel_por_streak) {
  ThermalPressureController controller;
  controller.reset();
  controller.observe({3, 1.0f, true, true});
  for (int sample = 0; sample < 2; ++sample) {
    const auto update = controller.observe({0, 0.60f, true, true});
    AE_EXPECT_EQ(update.pressure, ThermalPressure::Severe, "nao recupera cedo");
  }
  auto update = controller.observe({0, 0.60f, true, true});
  AE_EXPECT_EQ(update.pressure, ThermalPressure::Light, "recupera somente um nivel");
  for (int sample = 0; sample < 3; ++sample)
    update = controller.observe({0, 0.60f, true, true});
  AE_EXPECT_EQ(update.pressure, ThermalPressure::None, "segunda streak recupera o restante");
}

AE_TEST(thermal_pressure_amostra_invalida_ou_morna_nao_finge_recuperacao) {
  ThermalPressureController controller;
  controller.reset();
  controller.observe({1, 0.90f, true, true});
  for (int sample = 0; sample < 10; ++sample)
    controller.observe({-1, 0.0f, false, false});
  AE_EXPECT_EQ(controller.pressure(), ThermalPressure::Light, "ausencia de dado preserva estado");
  for (int sample = 0; sample < 10; ++sample)
    controller.observe({0, 0.80f, true, true});
  AE_EXPECT_EQ(controller.pressure(), ThermalPressure::Light, "headroom sem margem nao recupera");
}

AE_TEST(thermal_pressure_status_funciona_quando_headroom_nao_existe) {
  ThermalPressureController controller;
  controller.reset();
  controller.observe({2, 0.0f, true, false});
  AE_EXPECT_EQ(controller.pressure(), ThermalPressure::Light, "fallback de status");
  for (int sample = 0; sample < 3; ++sample)
    controller.observe({0, 0.0f, true, false});
  AE_EXPECT_EQ(controller.pressure(), ThermalPressure::None, "fallback tambem recupera com histerese");
}
