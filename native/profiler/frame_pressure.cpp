#include "profiler/frame_pressure.h"

#include <algorithm>
#include <cmath>

namespace ae::profiler {
namespace {
const Distribution &metric(const FrameProfileSummary &summary, FrameMetric value) {
  return summary.metrics[static_cast<u32>(value)];
}
} // namespace

FramePressureResult classifyFramePressure(const FrameProfileSummary &summary,
                                          const FrameBudget &budget,
                                          const FramePressurePolicy &policy) {
  FramePressureResult result{};
  const double cpuBudget = budget.cpuLaneBudgetMs;
  const double gpuBudget = budget.gpuLaneBudgetMs;
  const double intervalBudget = budget.frameIntervalMs;
  if (summary.samples == 0 || !(cpuBudget > 0.0) || !(gpuBudget > 0.0) ||
      !(intervalBudget > 0.0) || !std::isfinite(policy.lanePressureRatio) ||
      policy.lanePressureRatio <= 0.0 || !std::isfinite(policy.lateIntervalRatio) ||
      policy.lateIntervalRatio <= 0.0) {
    return result;
  }

  // Thread CPU é a trilha crítica do render thread. Process CPU continua no
  // relatório como soma diagnóstica de todas as threads, mas pode exceder o
  // wall time quando o futuro Job System paralelizar trabalho e, portanto, não
  // deve ser comparado diretamente ao budget de uma única trilha.
  const double cpuP95 = metric(summary, FrameMetric::ThreadCpu).p95;
  const double gpuP95 = metric(summary, FrameMetric::GpuFrame).p95;
  const double intervalP95 = metric(summary, FrameMetric::Interval).p95;
  if (!std::isfinite(cpuP95) || cpuP95 < 0.0 || !std::isfinite(gpuP95) ||
      gpuP95 <= 0.0 || !std::isfinite(intervalP95) || intervalP95 <= 0.0) {
    return result;
  }

  result.cpuP95BudgetRatio = cpuP95 / cpuBudget;
  result.gpuP95BudgetRatio = gpuP95 / gpuBudget;
  result.intervalP95BudgetRatio = intervalP95 / intervalBudget;
  result.presentationWaitP95Ms =
      metric(summary, FrameMetric::AcquireWall).p95 +
      metric(summary, FrameMetric::PresentWall).p95;

  const bool cpuPressure = result.cpuP95BudgetRatio >= policy.lanePressureRatio;
  const bool gpuPressure = result.gpuP95BudgetRatio >= policy.lanePressureRatio;
  if (cpuPressure && gpuPressure) result.kind = FramePressureKind::Mixed;
  else if (cpuPressure) result.kind = FramePressureKind::Cpu;
  else if (gpuPressure) result.kind = FramePressureKind::Gpu;
  else if (result.intervalP95BudgetRatio >= policy.lateIntervalRatio &&
           result.presentationWaitP95Ms > budget.compositorReserveMs)
    result.kind = FramePressureKind::Presentation;
  else
    result.kind = FramePressureKind::WithinBudget;
  return result;
}

const char *framePressureKindName(FramePressureKind kind) {
  switch (kind) {
    case FramePressureKind::WithinBudget: return "within-budget";
    case FramePressureKind::Cpu: return "cpu";
    case FramePressureKind::Gpu: return "gpu";
    case FramePressureKind::Mixed: return "mixed";
    case FramePressureKind::Presentation: return "presentation";
    default: return "unknown";
  }
}

} // namespace ae::profiler
