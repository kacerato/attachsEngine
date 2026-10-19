#include "profiler/telemetry_recorder.h"

namespace ae::profiler {

TelemetryRecorder::TelemetryRecorder() {
  statistics_.configure(WarmupMilliseconds, WindowFrames);
}

void TelemetryRecorder::reset() {
  statistics_.reset();
  identityValid_ = false;
}

bool TelemetryRecorder::record(FrameCounters now, const RenderPhaseTimings &phases,
                               const TelemetryWindowContext &context, std::string &outLine) {
  const bool changed = identityValid_ &&
      (scene_ != context.scene || mode_ != context.mode || tier_ != context.tier);
  if (changed) statistics_.reset();
  if (!identityValid_ || changed) {
    scene_.assign(context.scene);
    mode_.assign(context.mode);
    tier_.assign(context.tier);
    identityValid_ = true;
  }
  // Uma amostra inválida já reiniciou a janela dentro de FrameStatistics.
  if (statistics_.record(now, phases) != FrameSampleResult::WindowReady) return false;
  FrameProfileSummary summary{};
  if (!statistics_.summarize(summary)) return false;
  TelemetryWindowContext window = context;
  window.sequence = sequence_;
  outLine = telemetryWindowJson(summary, window);
  if (outLine.empty()) return false;
  ++sequence_;
  return true;
}

} // namespace ae::profiler
