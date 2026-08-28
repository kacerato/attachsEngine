#pragma once

#include <atomic>

struct AChoreographer;

namespace ae::platform::android {

// Android-specific frame clock. Rendering is admitted by display VSYNC instead
// of a busy loop that relies on vkAcquireNextImageKHR to throttle the CPU.
// The class owns no rendering state and can later pace any viewport/runtime.
class AndroidFramePacer final {
public:
  bool initialize();
  void start();
  void stop();
  bool available() const { return choreographer_ != nullptr; }
  bool consumeFrame() { return frameReady_.exchange(false, std::memory_order_acq_rel); }

private:
  static void onFrame(long frameTimeNanos, void *context);
  void schedule();

  AChoreographer *choreographer_ = nullptr;
  std::atomic<bool> active_{false};
  std::atomic<bool> callbackScheduled_{false};
  std::atomic<bool> frameReady_{false};
};

} // namespace ae::platform::android
