#include "core/frame_policy.h"
#include "harness.h"

AE_TEST(frame_policy_seleciona_60_90_120_sem_ultrapassar_painel) {
  AE_EXPECT_EQ(ae::makeFrameBudget(144.0f, static_cast<float>(ae::DefaultMaximumRenderHz)).renderHz,
               120u, "auto global usa 120 em painel de alta cadencia");
  AE_EXPECT_EQ(ae::makeFrameBudget(120.0f, 120.0f).renderHz, 120u, "painel 120");
  AE_EXPECT_EQ(ae::makeFrameBudget(90.0f, 120.0f).renderHz, 90u, "painel 90");
  AE_EXPECT_EQ(ae::makeFrameBudget(75.0f, 120.0f).renderHz, 60u, "painel intermediario");
  AE_EXPECT_EQ(ae::makeFrameBudget(120.0f, 60.0f).renderHz, 60u, "cap do projeto");
}

AE_TEST(frame_policy_tick_e_render_sao_independentes) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f, 60);
  AE_EXPECT_EQ(budget.renderHz, 120u, "apresentacao em 120 Hz");
  AE_EXPECT_EQ(budget.simulationHz, 60u, "fixed tick continua em 60 Hz");
  AE_EXPECT_TRUE(budget.frameIntervalMs > 8.3f && budget.frameIntervalMs < 8.4f,
                 "budget de 120 Hz e 8,33 ms");
  AE_EXPECT_TRUE(budget.cpuLaneBudgetMs < budget.frameIntervalMs &&
                 budget.gpuLaneBudgetMs < budget.frameIntervalMs,
                 "CPU/GPU preservam margem do compositor");
}

AE_TEST(frame_policy_entrada_invalida_tem_fallback_seguro) {
  const auto budget = ae::makeFrameBudget(0.0f, -1.0f, 0);
  AE_EXPECT_EQ(budget.renderHz, 60u, "fallback de apresentacao");
  AE_EXPECT_EQ(budget.simulationHz, 1u, "tick nunca pode ser zero");
}

AE_TEST(frame_policy_adpf_usa_intervalo_e_override_diagnostico_seguro) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f, 60);
  const ae::i64 inherited = ae::performanceHintTargetNanoseconds(budget);
  const ae::i64 fullInterval = ae::performanceHintTargetNanoseconds(budget, 1.0f);
  const ae::i64 diagnostic = ae::performanceHintTargetNanoseconds(budget, 0.75f);

  AE_EXPECT_TRUE(inherited >= 8'333'000 && inherited <= 8'334'000,
                 "ADPF herda o intervalo completo de apresentacao");
  AE_EXPECT_TRUE(fullInterval >= 8'333'000 && fullInterval <= 8'334'000,
                 "override 1.0 reproduz o contrato antigo");
  AE_EXPECT_TRUE(diagnostic >= 6'249'000 && diagnostic <= 6'251'000,
                 "override de profiling preserva unidade e precisao");
  AE_EXPECT_EQ(ae::performanceHintTargetNanoseconds(budget, 0.49f), inherited,
               "override agressivo demais falha para o intervalo global");
  AE_EXPECT_EQ(ae::performanceHintTargetNanoseconds(budget, 1.01f), inherited,
               "override acima do intervalo falha para o intervalo global");
}
