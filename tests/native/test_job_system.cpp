#include "harness.h"
#include "core/job_system.h"

#include <atomic>
#include <vector>

using namespace ae;
using namespace ae::test;

AE_TEST(JobSystem_executa_todos_os_jobs_submetidos) {
  JobSystem js(4);
  std::atomic<int> counter{0};
  constexpr int kJobs = 200;
  for (int i = 0; i < kJobs; ++i) {
    js.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
  }
  js.waitIdle();
  AE_EXPECT_TRUE(counter.load() == kJobs, "todos os jobs submetidos precisam ter rodado exatamente uma vez");
}

AE_TEST(JobSystem_job_pode_submeter_outro_job_dependente) {
  JobSystem js(2);
  std::atomic<int> stage{0};
  js.submit([&js, &stage] {
    stage.store(1, std::memory_order_release);
    js.submit([&stage] { stage.store(2, std::memory_order_release); });
  });
  js.waitIdle();
  AE_EXPECT_TRUE(stage.load() == 2, "waitIdle precisa esperar tambem jobs submetidos por outros jobs");
}

AE_TEST(JobSystem_funciona_com_uma_unica_thread) {
  JobSystem js(1);
  std::atomic<int> counter{0};
  for (int i = 0; i < 32; ++i) {
    js.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
  }
  js.waitIdle();
  AE_EXPECT_TRUE(counter.load() == 32, "com um unico worker os jobs ainda devem todos rodar (sem roubo, so fila local)");
}

AE_TEST(JobSystem_stress_muitos_jobs_muitas_threads) {
  JobSystem js(8);
  std::atomic<i64> sum{0};
  constexpr int kJobs = 5000;
  for (int i = 0; i < kJobs; ++i) {
    js.submit([&sum] { sum.fetch_add(1, std::memory_order_relaxed); });
  }
  js.waitIdle();
  AE_EXPECT_TRUE(sum.load() == kJobs, "sob stress com work-stealing nenhum job pode ser perdido ou duplicado");
}
