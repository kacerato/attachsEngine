#pragma once

#include "core/base.h"
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
  double gpuGeometryMs = 0;
  double gpuBackgroundMs = 0;
  double gpuTransparentMs = 0;
};

enum class FrameMetric : u32 {
  Interval, ProcessCpu, ThreadCpu, AcquireWall, InteropWall, RecordSubmitWall, PresentWall,
  GpuFrame, GpuGeometry, GpuBackground, GpuTransparent, Count
};
constexpr u32 FrameMetricCount = static_cast<u32>(FrameMetric::Count);
constexpr const char *FrameMetricNames[] = {
    "interval_ms", "process_cpu_ms", "thread_cpu_ms", "acquire_wall_ms",
    "interop_wall_ms", "record_submit_wall_ms", "present_wall_ms", "gpu_frame_ms",
    "gpu_geometry_ms", "gpu_background_ms", "gpu_transparent_ms"};

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
