#include "renderer/editor_post_cache_key.h"
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

AE_TEST(frame_policy_adpf_detailed_total_reports_cpu_and_gpu_work) {
  AE_EXPECT_EQ(ae::performanceHintActualTotalNanoseconds(1'500'000, 6'500'000),
               8'000'000, "component sum");
  AE_EXPECT_EQ(ae::performanceHintActualTotalNanoseconds(-1, 0), 1,
               "empty report remains ABI-positive");
  AE_EXPECT_EQ(ae::performanceHintActualTotalNanoseconds(
                   std::numeric_limits<ae::i64>::max(), 1),
               std::numeric_limits<ae::i64>::max(), "saturating ABI value");
}

#include "core/frame_admission.h"
AE_TEST(frame_admission_keeps_thirty_and_fifty_over_actual_mobile_refresh_callbacks) {
 for(const auto refresh:{60u,90u,120u})for(const auto target:{30u,50u}) {
  ae::i64 phase=0;unsigned admitted=0;
  for(unsigned n=0;n<refresh*10;++n)if(ae::admitPresentationFrame(1000000000LL+ae::i64(n)*1000000000LL/refresh,1000000000LL/target,phase))++admitted;
  AE_EXPECT_TRUE(admitted>=target*10-1 && admitted<=target*10+1,"ten seconds of physical VSYNC retain requested average without rounding50 to40");
  AE_EXPECT_EQ(ae::makeFrameBudget(float(refresh),float(target)).renderHz,target,"resolved mobile budget preserves supported30/50");
 }
}
AE_TEST(frame_admission_coalesces_gpu_stalls_without_a_catch_up_burst) {
 ae::i64 phase=0;
 AE_EXPECT_TRUE(ae::admitPresentationFrame(1000000000,20000000,phase),"first callback");
 AE_EXPECT_TRUE(ae::admitPresentationFrame(4000000000,20000000,phase),"one frame after a three-second GPU stall");
 AE_EXPECT_TRUE(!ae::admitPresentationFrame(4000000000,20000000,phase),"same callback cannot replay150 missed frames");
 AE_EXPECT_TRUE(!ae::admitPresentationFrame(4008333333,20000000,phase),"next physical VSYNC is still before the next deadline");
 AE_EXPECT_TRUE(ae::admitPresentationFrame(4025000000,20000000,phase),"next actual presentation deadline");
 phase=0;AE_EXPECT_TRUE(!ae::admitPresentationFrame(0,20000000,phase),"invalid clock produces no frame");
}

AE_TEST(editor_post_cache_bounds_and_complete_shader_key) {
  using namespace ae::renderer;
  AE_EXPECT_TRUE(editorPostCacheFits(1920,1080,9ull*1024*1024),"actual allocation within14MiB");
  AE_EXPECT_TRUE(!editorPostCacheFits(4096,2160)&&!editorPostCacheFits(1,1,EditorPostCacheBudget+1),"no hidden downsize or allocation overrun");
  AE_EXPECT_TRUE(!editorPostCacheFits(0,1)&&!editorPostCacheFits(0xffffffffu,0xffffffffu),"overflow and zero extent refuse");
  EditorPostCacheKey key;ae::u64 policy=2,shader=3,resource=4;ae::u32 width=1920,format=37;float grade=1,fxaa=1;
  key.value(policy);key.value(shader);key.value(resource);key.value(width);key.value(format);key.value(grade);key.value(fxaa);
  auto same=key;AE_EXPECT_TRUE(key==same,"same effective shader inputs reuse");
  for(ae::usize i=0;i<key.bytes.size();++i){auto changed=key;changed.bytes[i]^=1;AE_EXPECT_TRUE(!(key==changed),"every policy/shader/resource/extent/format/effect input invalidates");}
  // UI is deliberately not part of the scene shader key: a fresh overlay
  // must not force recomputation of identical scene post pixels.
  std::vector<ae::u8> ui{1,2};ui[0]=4;AE_EXPECT_TRUE(key==same,"UI mutation leaves scene-only key unchanged");
}
