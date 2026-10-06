#include "platform/android/android_frame_profiler.h"
#include "platform/android/android_launch_options.h"
#include "platform/process_memory.h"
#include <android/log.h>
#include <android/native_activity.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <time.h>
#include <unistd.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr u32 ProfileSchemaVersion = 8;

bool readClock(clockid_t clock, u64 &out) {
  timespec value{};
  if (clock_gettime(clock, &value) != 0) return false;
  out = static_cast<u64>(value.tv_sec) * 1'000'000'000ULL + static_cast<u64>(value.tv_nsec);
  return true;
}

double ratioOrUnavailable(u64 numerator, u64 denominator) {
  return denominator > 0 ? static_cast<double>(numerator) / static_cast<double>(denominator)
                         : -1.0;
}

const char *memoryPressureName(const platform::SystemMemorySnapshot &system,
                               const rhi::DeviceMemorySnapshot &device,
                               double &outSystemAvailableRatio,
                               double &outDriverUsageRatio,
                               double &outEngineClassRatio) {
  outSystemAvailableRatio = system.valid
                                ? ratioOrUnavailable(system.availableBytes, system.totalBytes)
                                : -1.0;
  outDriverUsageRatio = device.driverBudgetAvailable
                            ? ratioOrUnavailable(device.deviceLocalUsageBytes,
                                                 device.deviceLocalBudgetBytes)
                            : -1.0;
  outEngineClassRatio = -1.0;
  for (const auto &entry : device.allocationBudget.entries) {
    if (entry.limitBytes == 0 || entry.limitBytes == std::numeric_limits<u64>::max()) continue;
    outEngineClassRatio = std::max(outEngineClassRatio,
                                   ratioOrUnavailable(entry.usedBytes, entry.limitBytes));
  }
  const bool hasSignal = outSystemAvailableRatio >= 0.0 || outDriverUsageRatio >= 0.0 ||
                         outEngineClassRatio >= 0.0;
  if (!hasSignal) return "unknown";
  if ((outSystemAvailableRatio >= 0.0 && outSystemAvailableRatio <= 0.05) ||
      outDriverUsageRatio >= 0.90 || outEngineClassRatio >= 0.90)
    return "critical";
  if ((outSystemAvailableRatio >= 0.0 && outSystemAvailableRatio <= 0.10) ||
      outDriverUsageRatio >= 0.75 || outEngineClassRatio >= 0.75)
    return "warning";
  return "normal";
}
} // namespace

bool readFrameProfilingOption(ANativeActivity *activity) {
  return readBooleanLaunchOption(activity, "aether.profile_frames");
}

void AndroidFrameProfiler::record(const profiler::RenderPhaseTimings &phases,
                                  const FrameProfileContext &context,
                                  u32 instances, u32 width, u32 height) {
  if (!enabled_) return;
  const char *scene = context.sceneId != nullptr ? context.sceneId : "unknown";
  // Reopening publishes a different render scene after the first frames.
  // Never attribute its windows to the loading context. Camera motion, visible
  // draws and DRS vary normally and must not restart a window every frame.
  const bool identityChanged = identityValid_ &&
      (std::strcmp(identityScene_, scene) != 0 ||
       identityFingerprint_ != context.contentFingerprint || identityInstances_ != instances ||
       identityRenderDraws_ != context.renderDrawCount || identityWidth_ != width ||
       identityHeight_ != height || identityPostUiFused_ != context.postUiFused ||
       (context.cameraLocked && (identityCameraPosition_[0]!=context.cameraPosition[0] ||
        identityCameraPosition_[1]!=context.cameraPosition[1] || identityCameraPosition_[2]!=context.cameraPosition[2] ||
        identityCameraYaw_!=context.cameraYaw || identityCameraPitch_!=context.cameraPitch)));
  if (identityChanged) reset();
  identityValid_ = true;
  identityScene_ = scene;
  identityFingerprint_ = context.contentFingerprint;
  identityInstances_ = instances;
  identityRenderDraws_ = context.renderDrawCount;
  identityWidth_ = width;
  identityHeight_ = height;
  identityPostUiFused_ = context.postUiFused;
  std::copy(context.cameraPosition,context.cameraPosition+3,identityCameraPosition_);
  identityCameraYaw_=context.cameraYaw;identityCameraPitch_=context.cameraPitch;
  profiler::FrameCounters counters{};
  if (!readClock(CLOCK_MONOTONIC, counters.wallNs) ||
      !readClock(CLOCK_PROCESS_CPUTIME_ID, counters.processCpuNs) ||
      !readClock(CLOCK_THREAD_CPUTIME_ID, counters.threadCpuNs)) {
    enabled_ = false;
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] clock_gettime falhou; coleta desativada.");
    return;
  }
  if (contextPending_) {
    // log_print formats into a 1024-byte buffer on this Android runtime.
    // Preserve the whole context in one log_write record (below Logcat's
    // payload limit), including the flags that identify an A/B capture.
    char contextJson[3072];
    const int contextLength = std::snprintf(contextJson, sizeof(contextJson),
        "[FrameProfileContext] {\"schemaVersion\":%u,\"pid\":%d,\"epoch\":%u,"
        "\"scene\":\"%s\",\"content_fingerprint\":\"%016llx\",\"target_fps\":%u,"
        "\"gpu_isolation\":\"%s\","
        "\"camera_locked\":%s,\"camera_mode\":\"%s\","
        "\"camera_route_fingerprint\":\"%016llx\",\"camera_route_frame_ordinal\":%llu,"
        "\"camera_route_tick_count\":%llu,"
        "\"camera_pose\":[%.6f,%.6f,%.6f,%.6f,%.6f],"
        "\"draws\":%u,\"materials\":%u,\"textures\":%u,\"triangles\":%u,"
        "\"package_version\":%u,\"render_draws\":%u,\"lod_groups\":%u,"
        "\"hzb_enabled\":%s,\"lod_enabled\":%s,\"post_ui_fused\":%s,\"spatial_chunks\":%u,"
        "\"opaque_no_clip\":%s,\"full_detail_sampling\":%s,\"point_lighting_specialization\":%s,\"scene_reuse_enabled\":%s,"
        "\"rendering_quality_locked\":%s,\"shadow_filter_taps\":%u,"
        "\"lod_error_px\":%.3f,\"coverage_lod_error_px\":%.3f,"
        "\"render_scale\":%.3f,\"render_width\":%u,\"render_height\":%u,"
        "\"visible_draws\":%u,\"culled_draws\":%u,\"submitted_draw_calls\":%u,"
        "\"visible_triangles\":%llu,\"submitted_triangles\":%llu,"
        "\"hzb_tested_draws\":%u,\"hzb_occluded_draws\":%u,\"hzb_revived_draws\":%u,"
        "\"hzb_motion_skipped_draws\":%u,\"hzb_budget_skipped_draws\":%u,"
        "\"instances\":%u,\"width\":%u,\"height\":%u}",
        ProfileSchemaVersion, getpid(), epoch_, scene,
        static_cast<unsigned long long>(context.contentFingerprint),
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
        context.postUiFused ? "true" : "false",
        context.spatialChunks,
        context.opaqueNoClip ? "true" : "false",
        context.fullDetailSampling ? "true" : "false",
        context.pointLightingSpecialization ? "true" : "false",
        context.sceneReuseEnabled?"true":"false",
        context.renderingQualityLocked?"true":"false",context.shadowFilterTaps,
        context.lodPixelErrorBudget, context.coverageLodPixelErrorBudget,
        context.renderScale, context.renderWidth, context.renderHeight,
        context.visibleDrawCount,
        context.culledDrawCount, context.submittedDrawCallCount,
        static_cast<unsigned long long>(context.visibleTriangleCount),
        static_cast<unsigned long long>(context.submittedTriangleCount),
        context.hzbTestedDrawCount, context.hzbOccludedDrawCount, context.hzbRevivedDrawCount,
        context.hzbSkippedCameraMotionDrawCount,
        context.hzbSkippedBudgetDrawCount,
        instances, width, height);
    if (contextLength > 0 && static_cast<usize>(contextLength) < sizeof(contextJson)) {
      __android_log_write(ANDROID_LOG_INFO, LogTag, contextJson);
    } else {
      __android_log_write(ANDROID_LOG_ERROR, LogTag, "[FrameProfileContext] contexto excede limite; coleta rejeitada.");
    }
    contextPending_ = false;
  }
  if (phases.gpuFrameMs > 0.0) {
    ++attributionSampleFrames_;
    if (profiler::gpuPassAttributionCollapsed(phases.gpuPassMs, phases.gpuFrameMs)) {
      ++collapsedAttributionFrames_;
    }
  }
  const auto result = statistics_.record(counters, phases);
  if (result == profiler::FrameSampleResult::Invalid) {
    reset();
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Amostra inválida; janela descartada.");
    return;
  }
  if (result == profiler::FrameSampleResult::Collecting ||
      result == profiler::FrameSampleResult::WindowReady) {
    const float scale = std::isfinite(context.renderScale)
                            ? std::clamp(context.renderScale, 0.0f, 1.0f)
                            : 0.0f;
    if (renderScaleSamples_ == 0) {
      renderScaleMinimum_ = scale;
      renderScaleMaximum_ = scale;
    } else {
      renderScaleMinimum_ = std::min(renderScaleMinimum_, scale);
      renderScaleMaximum_ = std::max(renderScaleMaximum_, scale);
    }
    renderScaleLast_ = scale;
    ++renderScaleSamples_;
    if(context.sceneReused) ++sceneReusedFrames_;
    if(context.opaqueNoClip && !context.sceneReused) ++opaqueNoClipFrames_;
    if(context.pointLightingSpecialization && !context.sceneReused) ++pointLightingFrames_;
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
      "{\"schemaVersion\":%u,\"pid\":%d,\"epoch\":%u,\"window\":%llu,\"build\":\"%s\","
      "\"instances\":%u,\"width\":%u,\"height\":%u,\"frames\":%u,\"elapsed_ms\":%.6f,"
      "\"present_fps\":%.6f,\"warmup_samples\":%u,\"warmup_process_cpu_max_ms\":%.6f,"
      "\"route_frame\":%llu,\"visible_draws\":%u,\"visible_triangles\":%llu,\"scene_reused_frames\":%u,\"opaque_no_clip_frames\":%u,\"point_lighting_frames\":%u",
      ProfileSchemaVersion, getpid(), epoch_, static_cast<unsigned long long>(++window_), build,
      instances, width, height,
      summary.samples, summary.elapsedMs, summary.presentFps, summary.warmupSamples,
      summary.warmupProcessCpuMaxMs,
      static_cast<unsigned long long>(context.cameraRouteFrameOrdinal),
      context.visibleDrawCount,
      static_cast<unsigned long long>(context.visibleTriangleCount),sceneReusedFrames_,opaqueNoClipFrames_,pointLightingFrames_);
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
  char passes[900];
  // attribution diz se a divisao abaixo significa alguma coisa neste hardware.
  // Numa GPU TBDR os timestamps internos ao render pass podem resolver todos no
  // fim do tile: as regioes existem, mas o hardware nao as separa.
  const char *attribution =
      attributionSampleFrames_ != 0 &&
              collapsedAttributionFrames_ * 2 >= attributionSampleFrames_
          ? "tile-deferred"
          : "resolved";
  constexpr u32 PassesPerRecord=5;
  constexpr u32 PartCount=(GpuPassClassCount+PassesPerRecord-1)/PassesPerRecord;
  for(u32 part=0;part<PartCount;++part) {
    int passesUsed=std::snprintf(passes,sizeof(passes),
        "{\"schemaVersion\":%u,\"pid\":%d,\"epoch\":%u,\"window\":%llu,"
        "\"part\":%u,\"parts\":%u,\"attribution\":\"%s\","
        "\"collapsed_frames\":%u,\"attribution_samples\":%u",
        ProfileSchemaVersion,getpid(),epoch_,static_cast<unsigned long long>(window_),
        part,PartCount,attribution,collapsedAttributionFrames_,attributionSampleFrames_);
    if(passesUsed<0||static_cast<size_t>(passesUsed)>=sizeof(passes)) return;
    const u32 first=profiler::FrameLevelMetricCount+part*PassesPerRecord;
    const u32 last=std::min(first+PassesPerRecord,profiler::FrameMetricCount);
    passesUsed=appendMetrics(passes,sizeof(passes),passesUsed,first,last);
    if(!closeJson(passes,sizeof(passes),passesUsed)) {
      __android_log_print(ANDROID_LOG_ERROR,LogTag,
                          "[FrameProfilePasses] Parte excedeu limite seguro do Logcat.");
      return;
    }
    __android_log_print(ANDROID_LOG_INFO,LogTag,"[FrameProfilePasses] %s",passes);
  }

  // Registro separado para manter cada entrada abaixo do limite do Logcat. A
  // classificação compara p95 com budgets; ela nunca preenche CPU ociosa nem
  // reduz resolução por uma espera normal de VSYNC.
  const profiler::FramePressureResult pressure =
      profiler::classifyFramePressure(summary, context.frameBudget);
  __android_log_print(
      ANDROID_LOG_INFO, LogTag,
      "[FrameProfilePressure] {\"schemaVersion\":%u,\"pid\":%d,\"epoch\":%u,"
      "\"window\":%llu,\"classification\":\"%s\",\"cpu_p95_ratio\":%.4f,"
      "\"gpu_p95_ratio\":%.4f,\"interval_p95_ratio\":%.4f,"
      "\"presentation_wait_p95_ms\":%.4f,\"frame_budget_ms\":%.4f,"
      "\"cpu_budget_ms\":%.4f,\"gpu_budget_ms\":%.4f,"
      "\"render_scale_min\":%.3f,\"render_scale_max\":%.3f,"
      "\"render_scale_end\":%.3f,\"adpf\":%s,\"adpf_gpu_work\":%s,\"game_mode\":%d,"
      "\"sustained_supported\":%s,\"sustained_enabled\":%s,"
      "\"thermal_api\":%s,\"thermal_status\":%d,"
      "\"thermal_headroom_valid\":%s,\"thermal_headroom\":%.3f,"
      "\"thermal_pressure\":\"%s\"}",
      ProfileSchemaVersion, getpid(), epoch_, static_cast<unsigned long long>(window_),
      profiler::framePressureKindName(pressure.kind), pressure.cpuP95BudgetRatio,
      pressure.gpuP95BudgetRatio, pressure.intervalP95BudgetRatio,
      pressure.presentationWaitP95Ms,
      static_cast<double>(context.frameBudget.frameIntervalMs),
      static_cast<double>(context.frameBudget.cpuLaneBudgetMs),
      static_cast<double>(context.frameBudget.gpuLaneBudgetMs),
      static_cast<double>(renderScaleMinimum_), static_cast<double>(renderScaleMaximum_),
      static_cast<double>(renderScaleLast_), context.adpfAvailable ? "true" : "false",
      context.adpfGpuWorkAvailable ? "true" : "false", context.gameMode,
      context.sustainedPerformanceSupported ? "true" : "false",
      context.sustainedPerformanceEnabled ? "true" : "false",
      context.thermalApiAvailable ? "true" : "false", context.thermalStatus,
      context.thermalHeadroomValid ? "true" : "false",
      static_cast<double>(context.thermalHeadroom),
      context.thermalPressure != nullptr ? context.thermalPressure : "none");

  // RAM e memória gráfica compartilham a mesma chave pid/epoch/window das
  // métricas temporais. Arrays compactos mantêm a entrada inteira abaixo do
  // limite do Logcat sem omitir classes: ram=[virtual,rss,pico,anon,file,
  // shmem,swap], system=[total,disponível], heap=[tamanho,budget,uso driver],
  // engine=[uso,pico], classes=[uso,pico,limite] em MemoryClass order.
  platform::ProcessMemorySnapshot processMemory{};
  platform::SystemMemorySnapshot systemMemory{};
  platform::readProcessMemorySnapshot(processMemory);
  platform::readSystemMemorySnapshot(systemMemory);
  double systemAvailableRatio = -1.0;
  double driverUsageRatio = -1.0;
  double engineClassRatio = -1.0;
  const char *memoryPressure = memoryPressureName(
      systemMemory, context.deviceMemory, systemAvailableRatio, driverUsageRatio,
      engineClassRatio);
  const auto &allocation = context.deviceMemory.allocationBudget;
  const auto &buffer = allocation.entries[static_cast<usize>(rhi::MemoryClass::Buffer)];
  const auto &texture = allocation.entries[static_cast<usize>(rhi::MemoryClass::Texture)];
  const auto &target = allocation.entries[static_cast<usize>(rhi::MemoryClass::RenderTarget)];
  const auto &staging = allocation.entries[static_cast<usize>(rhi::MemoryClass::Staging)];
  __android_log_print(
      ANDROID_LOG_INFO, LogTag,
      "[FrameProfileMemory] {\"schemaVersion\":%u,\"pid\":%d,\"epoch\":%u,"
      "\"window\":%llu,\"classification\":\"%s\",\"ram_valid\":%s,"
      "\"ram_bytes\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu],"
      "\"system_ram_valid\":%s,\"system_ram_bytes\":[%llu,%llu],"
      "\"gpu_budget_supported\":%s,\"gpu_unified\":%s,"
      "\"gpu_heap_bytes\":[%llu,%llu,%llu],\"gpu_engine_bytes\":[%llu,%llu],"
      "\"gpu_classes\":[[%llu,%llu,%llu],[%llu,%llu,%llu],"
      "[%llu,%llu,%llu],[%llu,%llu,%llu]],\"ratios\":[%.6f,%.6f,%.6f]}",
      ProfileSchemaVersion, getpid(), epoch_, static_cast<unsigned long long>(window_),
      memoryPressure, processMemory.valid ? "true" : "false",
      static_cast<unsigned long long>(processMemory.virtualBytes),
      static_cast<unsigned long long>(processMemory.residentBytes),
      static_cast<unsigned long long>(processMemory.peakResidentBytes),
      static_cast<unsigned long long>(processMemory.anonymousBytes),
      static_cast<unsigned long long>(processMemory.fileBytes),
      static_cast<unsigned long long>(processMemory.sharedBytes),
      static_cast<unsigned long long>(processMemory.swapBytes),
      systemMemory.valid ? "true" : "false",
      static_cast<unsigned long long>(systemMemory.totalBytes),
      static_cast<unsigned long long>(systemMemory.availableBytes),
      context.deviceMemory.driverBudgetAvailable ? "true" : "false",
      context.deviceMemory.unifiedMemory ? "true" : "false",
      static_cast<unsigned long long>(context.deviceMemory.deviceLocalHeapBytes),
      static_cast<unsigned long long>(context.deviceMemory.deviceLocalBudgetBytes),
      static_cast<unsigned long long>(context.deviceMemory.deviceLocalUsageBytes),
      static_cast<unsigned long long>(allocation.totalUsedBytes()),
      static_cast<unsigned long long>(allocation.totalPeakBytes()),
      static_cast<unsigned long long>(buffer.usedBytes),
      static_cast<unsigned long long>(buffer.peakBytes),
      static_cast<unsigned long long>(buffer.limitBytes),
      static_cast<unsigned long long>(texture.usedBytes),
      static_cast<unsigned long long>(texture.peakBytes),
      static_cast<unsigned long long>(texture.limitBytes),
      static_cast<unsigned long long>(target.usedBytes),
      static_cast<unsigned long long>(target.peakBytes),
      static_cast<unsigned long long>(target.limitBytes),
      static_cast<unsigned long long>(staging.usedBytes),
      static_cast<unsigned long long>(staging.peakBytes),
      static_cast<unsigned long long>(staging.limitBytes),
      systemAvailableRatio, driverUsageRatio, engineClassRatio);
  collapsedAttributionFrames_ = 0;
  attributionSampleFrames_ = 0;
  renderScaleSamples_ = 0;
  sceneReusedFrames_=0;
  opaqueNoClipFrames_=0;
  pointLightingFrames_=0;
}

} // namespace ae::platform::android
