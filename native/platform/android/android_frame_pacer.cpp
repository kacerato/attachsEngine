#include "platform/android/android_frame_pacer.h"

#include <android/choreographer.h>
#include <android/log.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
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

void AndroidFramePacer::onFrame(long, void *context) {
  auto &pacer = *static_cast<AndroidFramePacer *>(context);
  pacer.callbackScheduled_.store(false, std::memory_order_release);
  if (!pacer.active_.load(std::memory_order_acquire)) return;
  // Coalesce VSYNCs if rendering was longer than one refresh interval. This
  // prevents catch-up bursts without losing input or lowering image quality.
  pacer.frameReady_.store(true, std::memory_order_release);
  pacer.schedule();
}

} // namespace ae::platform::android
