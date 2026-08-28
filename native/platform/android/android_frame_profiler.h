#pragma once
#include "profiler/frame_statistics.h"

struct ANativeActivity;

namespace ae::platform::android {

bool readFrameProfilingOption(ANativeActivity *activity);

class AndroidFrameProfiler final {
public:
  void setEnabled(bool enabled) { enabled_ = enabled; reset(); }
  bool enabled() const { return enabled_; }
  void reset() { statistics_.reset(); ++epoch_; }
  void record(const profiler::RenderPhaseTimings &phases, u32 instances, u32 width, u32 height);

private:
  bool enabled_ = false;
  u32 epoch_ = 0;
  u64 window_ = 0;
  profiler::FrameStatistics statistics_{};
};

} // namespace ae::platform::android
