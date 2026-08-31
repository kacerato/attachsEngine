#include "profiler/frame_statistics.h"
#include <algorithm>
#include <cmath>

namespace ae::profiler {

bool gpuPassAttributionCollapsed(const std::array<double, GpuPassClassCount> &passMs,
                                 double frameMs) {
  // Frame sem tempo medido não afirma nada sobre atribuição.
  if (!(frameMs > 0.0)) return false;
  const auto intraPass = [](GpuPassClass pass) {
    // Tudo que é gravado dentro do render pass principal. HZB fica de fora: é
    // uma cadeia de passes própria e é legitimamente medida em separado.
    return pass != GpuPassClass::Hzb;
  };
  double dominant = 0.0;
  double others = 0.0;
  for (u32 index = 0; index < GpuPassClassCount; ++index) {
    const auto pass = static_cast<GpuPassClass>(index);
    if (!intraPass(pass)) continue;
    const double value = passMs[index];
    if (value > dominant) {
      others += dominant;
      dominant = value;
    } else {
      others += value;
    }
  }
  // Assinatura: uma região sozinha responde por quase todo o frame e as demais
  // somadas são ruído. 0,98/0,01 são folgados de propósito -- a assinatura real
  // medida foi 0,9993 contra 0,00008, e um limiar apertado transformaria uma
  // separação legítima porém desigual em falso positivo.
  return dominant >= frameMs * 0.98 && others <= dominant * 0.01;
}

bool FrameStatistics::configure(u32 warmupMilliseconds, u32 windowFrames) {
  if (windowFrames == 0 || windowFrames > Capacity) return false;
  warmupNs_ = static_cast<u64>(warmupMilliseconds) * 1'000'000;
  windowFrames_ = windowFrames;
  reset();
  return true;
}

void FrameStatistics::reset() {
  hasPrevious_ = false;
  warmedUp_ = false;
  count_ = 0;
  warmupSamples_ = 0;
  warmupProcessCpuMaxMs_ = 0;
}

FrameSampleResult FrameStatistics::record(FrameCounters now, const RenderPhaseTimings &phases) {
  const double phaseValues[] = {phases.acquireMs, phases.interopMs, phases.recordSubmitMs,
                                phases.presentMs, phases.gpuFrameMs};
  const auto rejectsValue = [](double value) { return !std::isfinite(value) || value < 0; };
  for (double value : phaseValues) {
    if (rejectsValue(value)) {
      reset();
      return FrameSampleResult::Invalid;
    }
  }
  for (double value : phases.gpuPassMs) {
    if (rejectsValue(value)) {
      reset();
      return FrameSampleResult::Invalid;
    }
  }
  if (!hasPrevious_) {
    previous_ = now;
    startedNs_ = now.wallNs;
    hasPrevious_ = true;
    warmedUp_ = warmupNs_ == 0;
    return FrameSampleResult::WarmingUp;
  }
  if (now.wallNs <= previous_.wallNs || now.processCpuNs < previous_.processCpuNs ||
      now.threadCpuNs < previous_.threadCpuNs) {
    reset();
    return FrameSampleResult::Invalid;
  }
  const double wallMs = static_cast<double>(now.wallNs - previous_.wallNs) / 1e6;
  const double processMs = static_cast<double>(now.processCpuNs - previous_.processCpuNs) / 1e6;
  const double threadMs = static_cast<double>(now.threadCpuNs - previous_.threadCpuNs) / 1e6;
  previous_ = now;
  if (!warmedUp_) {
    ++warmupSamples_;
    warmupProcessCpuMaxMs_ = std::max(warmupProcessCpuMaxMs_, processMs);
    warmedUp_ = now.wallNs - startedNs_ >= warmupNs_;
    return FrameSampleResult::WarmingUp;
  }
  if (count_ == windowFrames_) count_ = 0;
  const double frameValues[] = {wallMs, processMs, threadMs, phases.acquireMs, phases.interopMs,
                                phases.recordSubmitMs, phases.presentMs, phases.gpuFrameMs};
  static_assert(sizeof(frameValues) / sizeof(frameValues[0]) == FrameLevelMetricCount,
                "frameValues precisa acompanhar FrameMetric.");
  for (u32 metric = 0; metric < FrameLevelMetricCount; ++metric)
    samples_[metric][count_] = frameValues[metric];
  for (u32 pass = 0; pass < GpuPassClassCount; ++pass)
    samples_[FrameLevelMetricCount + pass][count_] = phases.gpuPassMs[pass];
  ++count_;
  return count_ == windowFrames_ ? FrameSampleResult::WindowReady : FrameSampleResult::Collecting;
}

bool FrameStatistics::summarize(FrameProfileSummary &out) const {
  if (count_ != windowFrames_) return false;
  out = {};
  out.samples = count_;
  out.warmupSamples = warmupSamples_;
  out.warmupProcessCpuMaxMs = warmupProcessCpuMaxMs_;
  std::array<double, Capacity> sorted{};
  for (u32 metric = 0; metric < FrameMetricCount; ++metric) {
    double sum = 0;
    for (u32 i = 0; i < count_; ++i) {
      sorted[i] = samples_[metric][i];
      sum += sorted[i];
    }
    std::sort(sorted.begin(), sorted.begin() + count_);
    auto quantile = [&](u32 percentile) { return sorted[(count_ * percentile + 99) / 100 - 1]; };
    out.metrics[metric] = {sum / count_, quantile(50), quantile(95), quantile(99), sorted[count_ - 1]};
    if (metric == static_cast<u32>(FrameMetric::Interval)) out.elapsedMs = sum;
  }
  out.presentFps = 1000.0 * count_ / out.elapsedMs;
  return true;
}

} // namespace ae::profiler
