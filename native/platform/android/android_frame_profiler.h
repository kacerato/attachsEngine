#pragma once
#include "core/frame_policy.h"
#include "profiler/frame_pressure.h"
#include "profiler/frame_statistics.h"
#include "rhi/memory_budget.h"

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
  // Publicado na linha FrameProfilePressure, não duplicado no contexto longo.
  // O profiler recebe o budget resolvido em vez de reconstruí-lo pelo FPS.
  FrameBudget frameBudget{};
  bool adpfAvailable = false;
  bool adpfGpuWorkAvailable = false;
  i32 gameMode = 0;
  bool sustainedPerformanceSupported = false;
  bool sustainedPerformanceEnabled = false;
  bool thermalApiAvailable = false;
  bool thermalHeadroomValid = false;
  float thermalHeadroom = -1.0f;
  i32 thermalStatus = -1;
  const char *thermalPressure = "none";
  const char *gpuIsolation = "full";
  bool cameraLocked = false;
  // "free" | "locked" | "route" -- route implies a CameraRoutePlayer is
  // actively driving the camera this epoch, distinct from a single static
  // locked pose. See ae::platform::CameraRouteMode for the recorder/player
  // side of this contract.
  const char *cameraMode = "free";
  u64 cameraRouteFingerprint = 0;
  u64 cameraRouteFrameOrdinal = 0;
  u64 cameraRouteTickCount = 0;
  float cameraPosition[3]{};
  float cameraYaw = 0;
  float cameraPitch = 0;
  u32 drawCount = 0;
  u32 materialCount = 0;
  u32 textureCount = 0;
  u32 triangleCount = 0;
  u32 packageVersion = 0;
  u32 renderDrawCount = 0;
  u32 lodGroupCount = 0;
  bool hzbEnabled = false;
  bool lodEnabled = false;
  float lodPixelErrorBudget = 0.0f;
  float coverageLodPixelErrorBudget = 0.0f;
  float renderScale = 1.0f;
  u32 renderWidth = 0;
  u32 renderHeight = 0;
  u32 visibleDrawCount = 0;
  u32 culledDrawCount = 0;
  u32 submittedDrawCallCount = 0;
  u64 visibleTriangleCount = 0;
  u64 submittedTriangleCount = 0;
  // HZB occlusion stage (see renderer::VisibilityTelemetry); zero on every
  // frame HZB occlusion is disabled. hzbOccludedDrawCount is already counted
  // inside culledDrawCount above, not additional to it.
  u32 hzbTestedDrawCount = 0;
  u32 hzbOccludedDrawCount = 0;
  u32 hzbRevivedDrawCount = 0;
  u32 hzbSkippedCameraMotionDrawCount = 0;
  u32 hzbSkippedBudgetDrawCount = 0;
  // Snapshot do allocator/driver no fechamento da janela. Em Android UMA,
  // deviceMemory.unifiedMemory impede interpretar o heap Vulkan como VRAM
  // fisicamente adicional à RAM do sistema.
  rhi::DeviceMemorySnapshot deviceMemory{};
};

class AndroidFrameProfiler final {
public:
  void setEnabled(bool enabled) { enabled_ = enabled; reset(); }
  bool enabled() const { return enabled_; }
  void reset() {
    statistics_.reset();
    ++epoch_;
    contextPending_ = true;
    collapsedAttributionFrames_ = 0;
    attributionSampleFrames_ = 0;
    renderScaleSamples_ = 0;
    renderScaleMinimum_ = 1.0f;
    renderScaleMaximum_ = 1.0f;
    renderScaleLast_ = 1.0f;
  }
  void record(const profiler::RenderPhaseTimings &phases, const FrameProfileContext &context,
              u32 instances, u32 width, u32 height);

private:
  bool enabled_ = false;
  u32 epoch_ = 0;
  u64 window_ = 0;
  bool contextPending_ = true;
  // Frames da janela corrente em que os timestamps por região colapsaram (ver
  // profiler::gpuPassAttributionCollapsed). Publicado junto das regiões para
  // que ninguém otimize em cima de uma divisão que o hardware não fez.
  u32 collapsedAttributionFrames_ = 0;
  u32 attributionSampleFrames_ = 0;
  u32 renderScaleSamples_ = 0;
  float renderScaleMinimum_ = 1.0f;
  float renderScaleMaximum_ = 1.0f;
  float renderScaleLast_ = 1.0f;
  profiler::FrameStatistics statistics_{};
};

} // namespace ae::platform::android
