#include "platform/android/android_frame_profiler.h"
#include "platform/android/android_launch_options.h"
#include <android/log.h>
#include <android/native_activity.h>
#include <cstdio>
#include <time.h>
#include <unistd.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";

bool readClock(clockid_t clock, u64 &out) {
  timespec value{};
  if (clock_gettime(clock, &value) != 0) return false;
  out = static_cast<u64>(value.tv_sec) * 1'000'000'000ULL + static_cast<u64>(value.tv_nsec);
  return true;
}
} // namespace

bool readFrameProfilingOption(ANativeActivity *activity) {
  return readBooleanLaunchOption(activity, "aether.profile_frames");
}

void AndroidFrameProfiler::record(const profiler::RenderPhaseTimings &phases,
                                  const FrameProfileContext &context,
                                  u32 instances, u32 width, u32 height) {
  if (!enabled_) return;
  profiler::FrameCounters counters{};
  if (!readClock(CLOCK_MONOTONIC, counters.wallNs) ||
      !readClock(CLOCK_PROCESS_CPUTIME_ID, counters.processCpuNs) ||
      !readClock(CLOCK_THREAD_CPUTIME_ID, counters.threadCpuNs)) {
    enabled_ = false;
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] clock_gettime falhou; coleta desativada.");
    return;
  }
  if (contextPending_) {
    const char *scene = context.sceneId != nullptr ? context.sceneId : "unknown";
    __android_log_print(ANDROID_LOG_INFO, LogTag,
        "[FrameProfileContext] {\"schemaVersion\":2,\"pid\":%d,\"epoch\":%u,"
        "\"scene\":\"%s\",\"content_fingerprint\":\"%016llx\",\"target_fps\":%u,"
        "\"gpu_isolation\":\"%s\","
        "\"camera_locked\":%s,\"camera_pose\":[%.6f,%.6f,%.6f,%.6f,%.6f],"
        "\"draws\":%u,\"materials\":%u,\"textures\":%u,\"triangles\":%u,"
        "\"visible_draws\":%u,\"culled_draws\":%u,\"submitted_draw_calls\":%u,"
        "\"visible_triangles\":%llu,\"submitted_triangles\":%llu,"
        "\"instances\":%u,\"width\":%u,\"height\":%u}",
        getpid(), epoch_, scene, static_cast<unsigned long long>(context.contentFingerprint),
        context.targetFps, context.gpuIsolation != nullptr ? context.gpuIsolation : "full",
        context.cameraLocked ? "true" : "false",
        context.cameraPosition[0], context.cameraPosition[1], context.cameraPosition[2],
        context.cameraYaw, context.cameraPitch, context.drawCount, context.materialCount,
        context.textureCount, context.triangleCount, context.visibleDrawCount,
        context.culledDrawCount, context.submittedDrawCallCount,
        static_cast<unsigned long long>(context.visibleTriangleCount),
        static_cast<unsigned long long>(context.submittedTriangleCount), instances, width, height);
    contextPending_ = false;
  }
  const auto result = statistics_.record(counters, phases);
  if (result == profiler::FrameSampleResult::Invalid) {
    reset();
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Amostra inválida; janela descartada.");
    return;
  }
  if (result != profiler::FrameSampleResult::WindowReady) return;
  profiler::FrameProfileSummary summary{};
  if (!statistics_.summarize(summary)) return;
#if defined(NDEBUG)
  constexpr const char *build = "optimized";
#else
  constexpr const char *build = "debug";
#endif
  char json[3072];
  int used = std::snprintf(json, sizeof(json),
      "{\"schemaVersion\":3,\"pid\":%d,\"epoch\":%u,\"window\":%llu,\"build\":\"%s\","
      "\"instances\":%u,\"width\":%u,\"height\":%u,\"frames\":%u,\"elapsed_ms\":%.6f,"
      "\"present_fps\":%.6f,\"warmup_samples\":%u,\"warmup_process_cpu_max_ms\":%.6f",
      getpid(), epoch_, static_cast<unsigned long long>(++window_), build, instances, width, height,
      summary.samples, summary.elapsedMs, summary.presentFps, summary.warmupSamples,
      summary.warmupProcessCpuMaxMs);
  if (used < 0 || static_cast<size_t>(used) >= sizeof(json)) return;
  for (u32 metric = 0; metric < profiler::FrameMetricCount; ++metric) {
    const auto &d = summary.metrics[metric];
    // Schema v3 usa vetor compacto [mean,p50,p95,p99,max]. O formato anterior
    // excederia o limite de 1023 bytes do Logcat ao adicionar tempos por passe.
    const int written = std::snprintf(json + used, sizeof(json) - used,
        ",\"%s\":[%.4f,%.4f,%.4f,%.4f,%.4f]",
        profiler::FrameMetricNames[metric], d.mean, d.p50, d.p95, d.p99, d.maximum);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(json) - used) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Buffer de relatório insuficiente.");
      return;
    }
    used += written;
  }
  if (static_cast<size_t>(used) + 2 > sizeof(json)) return;
  json[used++] = '}';
  json[used] = '\0';
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[FrameProfile] %s", json);
}

} // namespace ae::platform::android
