#include "profiler/frame_pressure.h"
#include "harness.h"

namespace {
ae::profiler::FrameProfileSummary summary(double intervalP95, double threadCpuP95,
                                          double gpuP95, double acquireP95 = 0.0,
                                          double presentP95 = 0.0) {
  using namespace ae::profiler;
  FrameProfileSummary value{};
  value.samples = 600;
  value.metrics[static_cast<ae::u32>(FrameMetric::Interval)].p95 = intervalP95;
  value.metrics[static_cast<ae::u32>(FrameMetric::ThreadCpu)].p95 = threadCpuP95;
  value.metrics[static_cast<ae::u32>(FrameMetric::GpuFrame)].p95 = gpuP95;
  value.metrics[static_cast<ae::u32>(FrameMetric::AcquireWall)].p95 = acquireP95;
  value.metrics[static_cast<ae::u32>(FrameMetric::PresentWall)].p95 = presentP95;
  return value;
}
} // namespace

AE_TEST(frame_pressure_classifica_gpu_sem_exigir_cpu_artificialmente_ocupada) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f);
  const auto result = ae::profiler::classifyFramePressure(summary(10.0, 1.4, 8.2), budget);
  AE_EXPECT_EQ(result.kind, ae::profiler::FramePressureKind::Gpu, "GPU excedeu sua margem");
  AE_EXPECT_TRUE(result.cpuP95BudgetRatio < 0.5, "CPU ociosa nao e defeito");
}

AE_TEST(frame_pressure_distingue_cpu_misto_e_dentro_do_budget) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f);
  AE_EXPECT_EQ(ae::profiler::classifyFramePressure(summary(9.0, 6.2, 4.0), budget).kind,
               ae::profiler::FramePressureKind::Cpu, "pressao CPU");
  AE_EXPECT_EQ(ae::profiler::classifyFramePressure(summary(10.0, 6.2, 7.0), budget).kind,
               ae::profiler::FramePressureKind::Mixed, "pressao nas duas trilhas");
  AE_EXPECT_EQ(ae::profiler::classifyFramePressure(summary(8.2, 2.0, 5.0), budget).kind,
               ae::profiler::FramePressureKind::WithinBudget, "folga nas duas trilhas");
}

AE_TEST(frame_pressure_nao_chama_vsync_normal_de_gargalo) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f);
  const auto normal = ae::profiler::classifyFramePressure(summary(8.3, 1.0, 4.0, 2.0), budget);
  AE_EXPECT_EQ(normal.kind, ae::profiler::FramePressureKind::WithinBudget,
               "espera normal sem atraso permanece saudavel");
  const auto late = ae::profiler::classifyFramePressure(summary(10.0, 1.0, 4.0, 2.0), budget);
  AE_EXPECT_EQ(late.kind, ae::profiler::FramePressureKind::Presentation,
               "atraso sem pressao CPU/GPU aponta apresentacao");
}

AE_TEST(frame_pressure_sem_timestamp_gpu_permanece_desconhecida) {
  const auto budget = ae::makeFrameBudget(120.0f, 120.0f);
  AE_EXPECT_EQ(ae::profiler::classifyFramePressure(summary(10.0, 1.0, 0.0), budget).kind,
               ae::profiler::FramePressureKind::Unknown,
               "nao inventa classificacao sem evidencia GPU");
}
