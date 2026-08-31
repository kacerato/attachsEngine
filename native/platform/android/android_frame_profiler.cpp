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
        "[FrameProfileContext] {\"schemaVersion\":3,\"pid\":%d,\"epoch\":%u,"
        "\"scene\":\"%s\",\"content_fingerprint\":\"%016llx\",\"target_fps\":%u,"
        "\"gpu_isolation\":\"%s\","
        "\"camera_locked\":%s,\"camera_mode\":\"%s\","
        "\"camera_route_fingerprint\":\"%016llx\",\"camera_route_frame_ordinal\":%llu,"
        "\"camera_route_tick_count\":%llu,"
        "\"camera_pose\":[%.6f,%.6f,%.6f,%.6f,%.6f],"
        "\"draws\":%u,\"materials\":%u,\"textures\":%u,\"triangles\":%u,"
        "\"package_version\":%u,\"render_draws\":%u,\"lod_groups\":%u,"
        "\"hzb_enabled\":%s,\"lod_enabled\":%s,"
        "\"visible_draws\":%u,\"culled_draws\":%u,\"submitted_draw_calls\":%u,"
        "\"visible_triangles\":%llu,\"submitted_triangles\":%llu,"
        "\"hzb_tested_draws\":%u,\"hzb_occluded_draws\":%u,\"hzb_revived_draws\":%u,"
        "\"hzb_motion_skipped_draws\":%u,\"hzb_budget_skipped_draws\":%u,"
        "\"instances\":%u,\"width\":%u,\"height\":%u}",
        getpid(), epoch_, scene, static_cast<unsigned long long>(context.contentFingerprint),
        context.targetFps, context.gpuIsolation != nullptr ? context.gpuIsolation : "full",
        context.cameraLocked ? "true" : "false",
        context.cameraMode != nullptr ? context.cameraMode : "free",
        static_cast<unsigned long long>(context.cameraRouteFingerprint),
        static_cast<unsigned long long>(context.cameraRouteFrameOrdinal),
        static_cast<unsigned long long>(context.cameraRouteTickCount),
        context.cameraPosition[0], context.cameraPosition[1], context.cameraPosition[2],
        context.cameraYaw, context.cameraPitch, context.drawCount, context.materialCount,
        context.textureCount, context.triangleCount, context.packageVersion,
        context.renderDrawCount, context.lodGroupCount,
        context.hzbEnabled ? "true" : "false", context.lodEnabled ? "true" : "false",
        context.visibleDrawCount,
        context.culledDrawCount, context.submittedDrawCallCount,
        static_cast<unsigned long long>(context.visibleTriangleCount),
        static_cast<unsigned long long>(context.submittedTriangleCount),
        context.hzbTestedDrawCount, context.hzbOccludedDrawCount, context.hzbRevivedDrawCount,
        context.hzbSkippedCameraMotionDrawCount,
        context.hzbSkippedBudgetDrawCount,
        instances, width, height);
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
      "{\"schemaVersion\":4,\"pid\":%d,\"epoch\":%u,\"window\":%llu,\"build\":\"%s\","
      "\"instances\":%u,\"width\":%u,\"height\":%u,\"frames\":%u,\"elapsed_ms\":%.6f,"
      "\"present_fps\":%.6f,\"warmup_samples\":%u,\"warmup_process_cpu_max_ms\":%.6f,"
      "\"route_frame\":%llu,\"visible_draws\":%u,\"visible_triangles\":%llu",
      getpid(), epoch_, static_cast<unsigned long long>(++window_), build, instances, width, height,
      summary.samples, summary.elapsedMs, summary.presentFps, summary.warmupSamples,
      summary.warmupProcessCpuMaxMs,
      static_cast<unsigned long long>(context.cameraRouteFrameOrdinal),
      context.visibleDrawCount,
      static_cast<unsigned long long>(context.visibleTriangleCount));
  if (used < 0 || static_cast<size_t>(used) >= sizeof(json)) return;
  // Vetor compacto [mean,p50,p95,p99,max] por métrica. Cada entrada do Logcat é
  // truncada em silêncio perto de 1023 bytes, e a janela v3 já ocupava 937 com
  // apenas três passes — por isso as classes de passe saem num registro próprio
  // logo abaixo, em vez de caber "quase sempre" nesta linha.
  const auto appendMetrics = [&](char *buffer, size_t capacity, int offset, u32 first,
                                 u32 last) -> int {
    for (u32 metric = first; metric < last; ++metric) {
      const auto &d = summary.metrics[metric];
      const int written = std::snprintf(buffer + offset, capacity - static_cast<size_t>(offset),
          ",\"%s\":[%.4f,%.4f,%.4f,%.4f,%.4f]",
          profiler::frameMetricName(metric), d.mean, d.p50, d.p95, d.p99, d.maximum);
      if (written < 0 || static_cast<size_t>(written) >= capacity - static_cast<size_t>(offset))
        return -1;
      offset += written;
    }
    return offset;
  };
  const auto closeJson = [](char *buffer, size_t capacity, int offset) -> bool {
    if (offset < 0 || static_cast<size_t>(offset) + 2 > capacity) return false;
    buffer[offset++] = '}';
    buffer[offset] = '\0';
    return true;
  };
  used = appendMetrics(json, sizeof(json), used, 0, profiler::FrameLevelMetricCount);
  if (!closeJson(json, sizeof(json), used)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Buffer de relatório insuficiente.");
    return;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[FrameProfile] %s", json);

  // Regiões de GPU do frame, pareadas à janela acima por pid/epoch/window. O
  // consumidor exige o par: uma janela sem suas regiões é captura incompleta,
  // não uma janela cujos passes custaram zero.
  char passes[1024];
  int passesUsed = std::snprintf(passes, sizeof(passes),
      "{\"schemaVersion\":4,\"pid\":%d,\"epoch\":%u,\"window\":%llu",
      getpid(), epoch_, static_cast<unsigned long long>(window_));
  if (passesUsed < 0 || static_cast<size_t>(passesUsed) >= sizeof(passes)) return;
  passesUsed = appendMetrics(passes, sizeof(passes), passesUsed,
                             profiler::FrameLevelMetricCount, profiler::FrameMetricCount);
  if (!closeJson(passes, sizeof(passes), passesUsed)) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag,
                        "[FrameProfilePasses] Buffer de relatório insuficiente.");
    return;
  }
  __android_log_print(ANDROID_LOG_INFO, LogTag, "[FrameProfilePasses] %s", passes);
}

} // namespace ae::platform::android
