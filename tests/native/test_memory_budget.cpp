#include "harness.h"
#include "rhi/memory_budget.h"

using namespace ae;
using namespace ae::rhi;
using namespace ae::test;

AE_TEST(MemoryBudget_distribui_total_sem_perder_bytes) {
  const MemoryBudgetConfig config = MemoryBudgetConfig::fromTotalBytes(1000);
  u64 total = 0;
  for (u64 limit : config.limits) total += limit;
  AE_EXPECT_EQ(total, 1000u, "as cotas por classe devem somar exatamente o total");
  AE_EXPECT_EQ(config.limits[static_cast<usize>(MemoryClass::Texture)], 500u,
               "texturas devem receber metade do orçamento móvel inicial");
}

AE_TEST(MemoryBudget_rejeita_estouro_sem_corromper_estado) {
  MemoryBudgetConfig config{};
  config.limits.fill(100);
  MemoryBudgetTracker tracker(config);
  AE_EXPECT_TRUE(tracker.tryReserve(MemoryClass::Buffer, 80), "primeira reserva deveria caber");
  AE_EXPECT_TRUE(!tracker.tryReserve(MemoryClass::Buffer, 21), "reserva acima do limite deve falhar");
  const MemoryBudgetSnapshot snapshot = tracker.snapshot();
  AE_EXPECT_EQ(snapshot.entries[static_cast<usize>(MemoryClass::Buffer)].usedBytes, 80u,
               "falha de reserva nao pode alterar o uso atual");
}

AE_TEST(MemoryBudget_release_preserva_pico_e_detecta_underflow) {
  MemoryBudgetConfig config{};
  config.limits.fill(100);
  MemoryBudgetTracker tracker(config);
  AE_EXPECT_TRUE(tracker.tryReserve(MemoryClass::Texture, 70), "reserva deveria caber");
  AE_EXPECT_TRUE(tracker.release(MemoryClass::Texture, 20), "release valido deveria funcionar");
  AE_EXPECT_TRUE(!tracker.release(MemoryClass::Texture, 60), "release maior que uso deve falhar");
  const MemoryBudgetEntry entry = tracker.snapshot().entries[static_cast<usize>(MemoryClass::Texture)];
  AE_EXPECT_EQ(entry.usedBytes, 50u, "underflow rejeitado nao pode alterar uso");
  AE_EXPECT_EQ(entry.peakBytes, 70u, "release nao deve apagar o pico historico");
}
