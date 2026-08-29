#include "platform/android/android_frame_pacer.h"

#include <android/choreographer.h>
#include <android/log.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
}

void AndroidFramePacer::setTargetFrameRate(unsigned framesPerSecond) {
  const unsigned safeRate = framesPerSecond == 0 ? 60u : framesPerSecond;
  targetIntervalNanos_.store(1'000'000'000LL / safeRate, std::memory_order_relaxed);
}

bool AndroidFramePacer::initialize() {
  choreographer_ = AChoreographer_getInstance();
  if (choreographer_ == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[FramePacer] Choreographer indisponível; usando throttle da swapchain.");
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag,
                      "[FramePacer] VSYNC do Android controla a admissão de frames.");
  return true;
}

void AndroidFramePacer::start() {
  if (choreographer_ == nullptr) return;
  active_.store(true, std::memory_order_release);
  frameReady_.store(true, std::memory_order_release);
  lastFrameNanos_.store(0, std::memory_order_relaxed);
  measuredIntervalNanos_.store(0, std::memory_order_relaxed);
  measuredIntervals_.store(0, std::memory_order_relaxed);
  lastAdmittedNanos_.store(0, std::memory_order_relaxed);
  schedule();
}

void AndroidFramePacer::stop() {
  active_.store(false, std::memory_order_release);
  frameReady_.store(false, std::memory_order_release);
  // A callback already queued cannot be cancelled by the NDK API. It observes
  // active_ and deliberately does not enqueue a successor.
}

void AndroidFramePacer::schedule() {
  if (!active_.load(std::memory_order_acquire)) return;
  bool expected = false;
  if (!callbackScheduled_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
  AChoreographer_postFrameCallback(choreographer_, &AndroidFramePacer::onFrame, this);
}

void AndroidFramePacer::onFrame(long frameTimeNanos, void *context) {
  auto &pacer = *static_cast<AndroidFramePacer *>(context);
  pacer.callbackScheduled_.store(false, std::memory_order_release);
  if (!pacer.active_.load(std::memory_order_acquire)) return;
  const std::int64_t previous = pacer.lastFrameNanos_.exchange(
      static_cast<std::int64_t>(frameTimeNanos), std::memory_order_relaxed);
  if (previous > 0) {
    const std::int64_t interval = static_cast<std::int64_t>(frameTimeNanos) - previous;
    if (interval >= 4'000'000 && interval <= 40'000'000) {
      pacer.measuredIntervalNanos_.fetch_add(interval, std::memory_order_relaxed);
      const unsigned samples = pacer.measuredIntervals_.fetch_add(1, std::memory_order_relaxed) + 1;
      if (samples == 120) {
        const double average = static_cast<double>(
            pacer.measuredIntervalNanos_.load(std::memory_order_relaxed)) / samples;
        __android_log_print(ANDROID_LOG_INFO, LogTag,
                            "[FramePacer] cadência VSYNC observada=%.2f Hz (120 intervalos).",
                            1'000'000'000.0 / average);
      }
    }
  }
  // Choreographer segue o refresh físico (120 Hz neste aparelho) mesmo quando
  // a Surface vota em 60 Hz. Admitir todos esses callbacks renderizaria frames
  // que o compositor descarta. Filtramos pela meta global, sem busy-wait.
  const std::int64_t target = pacer.targetIntervalNanos_.load(std::memory_order_relaxed);
  const std::int64_t lastAdmitted = pacer.lastAdmittedNanos_.load(std::memory_order_relaxed);
  constexpr std::int64_t SchedulingToleranceNanos = 500'000;
  if (lastAdmitted == 0 || static_cast<std::int64_t>(frameTimeNanos) - lastAdmitted +
                               SchedulingToleranceNanos >= target) {
    pacer.lastAdmittedNanos_.store(static_cast<std::int64_t>(frameTimeNanos),
                                   std::memory_order_relaxed);
    // Coalesce callbacks if rendering was longer than the target interval.
    pacer.frameReady_.store(true, std::memory_order_release);
  }
  pacer.schedule();
}

} // namespace ae::platform::android
