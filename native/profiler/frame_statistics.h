#pragma once

#include "core/base.h"
#include "core/gpu_pass_class.h"
#include <array>

namespace ae::profiler {

// Cumulative clocks sampled after successful present. Process CPU includes all
// process threads; neither CPU clock includes time spent blocked off-CPU.
struct FrameCounters {
  u64 wallNs = 0;
  u64 processCpuNs = 0;
  u64 threadCpuNs = 0;
};

struct RenderPhaseTimings {
  double acquireMs = 0;
  double interopMs = 0;
  double recordSubmitMs = 0;
  double presentMs = 0;
  double gpuFrameMs = 0;
  // Indexado por GpuPassClass. Um array em vez de campos nomeados porque a
  // lista de regiões do frame cresce (sombras, pós) e cada campo novo exigia
  // tocar quatro arquivos que podiam divergir em silêncio.
  std::array<double, GpuPassClassCount> gpuPassMs{};
};

// Métricas de nível de frame. As classes de passe vêm logo depois, na ordem de
// GpuPassClass, formando um espaço de índices único — ver frameMetricName.
enum class FrameMetric : u32 {
  Interval, ProcessCpu, ThreadCpu, AcquireWall, InteropWall, RecordSubmitWall, PresentWall,
  GpuFrame, Count
};
constexpr u32 FrameLevelMetricCount = static_cast<u32>(FrameMetric::Count);
constexpr u32 FrameMetricCount = FrameLevelMetricCount + GpuPassClassCount;
constexpr const char *FrameLevelMetricNames[] = {
    "interval_ms", "process_cpu_ms", "thread_cpu_ms", "acquire_wall_ms",
    "interop_wall_ms", "record_submit_wall_ms", "present_wall_ms", "gpu_frame_ms"};
static_assert(sizeof(FrameLevelMetricNames) / sizeof(FrameLevelMetricNames[0]) ==
                  FrameLevelMetricCount,
              "FrameLevelMetricNames precisa de uma entrada por métrica de frame.");

// Nome único para qualquer índice de métrica. Não duplica a lista de classes de
// passe: a tabela canônica é a de core/gpu_pass_class.h.
constexpr const char *frameMetricName(u32 metric) {
  return metric < FrameLevelMetricCount
             ? FrameLevelMetricNames[metric]
             : GpuPassClassMetricNames[metric - FrameLevelMetricCount];
}

// Índice da métrica de uma classe de passe dentro do espaço acima.
constexpr u32 frameMetricIndex(GpuPassClass pass) {
  return FrameLevelMetricCount + static_cast<u32>(pass);
}

struct Distribution {
  double mean = 0, p50 = 0, p95 = 0, p99 = 0, maximum = 0;
};

struct FrameProfileSummary {
  u32 samples = 0;
  u32 warmupSamples = 0;
  double warmupProcessCpuMaxMs = 0;
  double elapsedMs = 0;
  double presentFps = 0;
  std::array<Distribution, FrameMetricCount> metrics{};
};

enum class FrameSampleResult { WarmingUp, Collecting, WindowReady, Invalid };

// Bounded storage, no heap allocation in record/summarize. Single-thread owned.
// Quantiles use nearest-rank; never average percentiles from different windows.
class FrameStatistics final {
public:
  static constexpr u32 Capacity = 600;
  bool configure(u32 warmupMilliseconds, u32 windowFrames);
  void reset();
  FrameSampleResult record(FrameCounters now, const RenderPhaseTimings &phases);
  bool summarize(FrameProfileSummary &out) const;

private:
  u64 warmupNs_ = 5'000'000'000ULL;
  u32 windowFrames_ = Capacity;
  bool hasPrevious_ = false;
  bool warmedUp_ = false;
  FrameCounters previous_{};
  u64 startedNs_ = 0;
  u32 count_ = 0;
  u32 warmupSamples_ = 0;
  double warmupProcessCpuMaxMs_ = 0;
  std::array<std::array<double, Capacity>, FrameMetricCount> samples_{};
};

} // namespace ae::profiler
