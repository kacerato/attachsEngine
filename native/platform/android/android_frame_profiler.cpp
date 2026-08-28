#include "platform/android/android_frame_profiler.h"
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

// Read once at launch; no JNI calls or Activity references retained per frame.
bool readFrameProfilingOption(ANativeActivity *activity) {
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Falha ao anexar JNI.");
      return false;
    }
    attachedHere = true;
  } else if (result != JNI_OK) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] JNI indisponível.");
    return false;
  }
  bool enabled = false;
  const bool localFrame = env->PushLocalFrame(8) == JNI_OK;
  const bool ok = localFrame && [&]() {
    jclass activityClass = env->GetObjectClass(activity->clazz);
    if (activityClass == nullptr) return false;
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    if (getIntent == nullptr) return false;
    jobject intent = env->CallObjectMethod(activity->clazz, getIntent);
    if (env->ExceptionCheck()) return false;
    if (intent == nullptr) return true;
    jclass intentClass = env->GetObjectClass(intent);
    if (intentClass == nullptr) return false;
    jmethodID getBoolean = env->GetMethodID(intentClass, "getBooleanExtra", "(Ljava/lang/String;Z)Z");
    if (getBoolean == nullptr) return false;
    jstring key = env->NewStringUTF("aether.profile_frames");
    if (key == nullptr) return false;
    enabled = env->CallBooleanMethod(intent, getBoolean, key, JNI_FALSE) == JNI_TRUE;
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();
  if (!ok) __android_log_print(ANDROID_LOG_ERROR, LogTag, "[FrameProfile] Falha ao ler opção de lançamento.");
  return ok && enabled;
}

void AndroidFrameProfiler::record(const profiler::RenderPhaseTimings &phases,
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
      "{\"schemaVersion\":1,\"pid\":%d,\"epoch\":%u,\"window\":%llu,\"build\":\"%s\","
      "\"instances\":%u,\"width\":%u,\"height\":%u,\"frames\":%u,\"elapsed_ms\":%.6f,"
      "\"present_fps\":%.6f,\"warmup_samples\":%u,\"warmup_process_cpu_max_ms\":%.6f",
      getpid(), epoch_, static_cast<unsigned long long>(++window_), build, instances, width, height,
      summary.samples, summary.elapsedMs, summary.presentFps, summary.warmupSamples,
      summary.warmupProcessCpuMaxMs);
  if (used < 0 || static_cast<size_t>(used) >= sizeof(json)) return;
  for (u32 metric = 0; metric < profiler::FrameMetricCount; ++metric) {
    const auto &d = summary.metrics[metric];
    const int written = std::snprintf(json + used, sizeof(json) - used,
        ",\"%s\":{\"mean\":%.6f,\"p50\":%.6f,\"p95\":%.6f,\"p99\":%.6f,\"max\":%.6f}",
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
