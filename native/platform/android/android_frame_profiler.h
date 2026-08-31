#pragma once
#include "profiler/frame_statistics.h"

struct ANativeActivity;

namespace ae::platform::android {

bool readFrameProfilingOption(ANativeActivity *activity);

// Identity emitted separately from compact frame windows so Logcat's entry
// limit cannot truncate either record. Strings are stable literals owned by
// the caller; record() consumes them synchronously and retains no pointer.
struct FrameProfileContext final {
  const char *sceneId = "unknown";
  u64 contentFingerprint = 0;
  u32 targetFps = 0;
  const char *gpuIsolation = "full";
  bool cameraLocked = false;
  float cameraPosition[3]{};
  float cameraYaw = 0;
  float cameraPitch = 0;
  u32 drawCount = 0;
  u32 materialCount = 0;
  u32 textureCount = 0;
  u32 triangleCount = 0;
  u32 visibleDrawCount = 0;
  u32 culledDrawCount = 0;
  u32 submittedDrawCallCount = 0;
  u64 visibleTriangleCount = 0;
  u64 submittedTriangleCount = 0;
};

class AndroidFrameProfiler final {
public:
  void setEnabled(bool enabled) { enabled_ = enabled; reset(); }
  bool enabled() const { return enabled_; }
  void reset() { statistics_.reset(); ++epoch_; contextPending_ = true; }
  void record(const profiler::RenderPhaseTimings &phases, const FrameProfileContext &context,
              u32 instances, u32 width, u32 height);

private:
  bool enabled_ = false;
  u32 epoch_ = 0;
  u64 window_ = 0;
  bool contextPending_ = true;
  profiler::FrameStatistics statistics_{};
};

} // namespace ae::platform::android
