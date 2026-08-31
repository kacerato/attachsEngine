#include "platform/android/android_performance.h"

#include <android/api-level.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <dlfcn.h>
#include <unistd.h>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "Aether.Android";
constexpr i32 Android13Api = 33;
constexpr i32 Android7Api = 24;
constexpr i32 GameModeUnsupported = 0;
constexpr i32 GameStateModeNone = 1;
constexpr i32 GameStateModeGameplayUninterruptible = 3;

template <typename Function>
Function loadSymbol(void *library, const char *name) {
  return reinterpret_cast<Function>(dlsym(library, name));
}

bool attach(ANativeActivity *activity, JNIEnv *&environment, bool &attachedHere) {
  if (activity == nullptr || activity->vm == nullptr) return false;
  attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (result == JNI_OK) return true;
  if (result != JNI_EDETACHED ||
      activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return false;
  attachedHere = true;
  return true;
}
} // namespace

AndroidPerformance::~AndroidPerformance() { shutdown(); }

bool AndroidPerformance::initialize(ANativeActivity *activity,
                                    const AndroidPerformancePolicy &policy) {
  shutdown();
  activity_ = activity;
  targetFrameDurationNs_ = policy.targetFrameDurationNs > 0
                               ? policy.targetFrameDurationNs : 16'666'667;
  preferSustainedPerformance_ = policy.preferSustainedPerformance;
  sustainedPerformanceSupported_ = querySustainedPerformanceSupport();
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[PerformancePolicy] sustained requested=%s supported=%s.",
      preferSustainedPerformance_ ? "true" : "false",
      sustainedPerformanceSupported_ ? "true" : "false");

  // Querying Game Mode does not opt out of OEM interventions. In particular we
  // intentionally do not declare game_mode_config, because doing so would reset
  // vendor interventions that currently help the connected Xiaomi.
  publishGameState(false, true);

  if (android_get_device_api_level() < Android13Api) {
    __android_log_print(ANDROID_LOG_INFO, LogTag,
                        "[PerformancePolicy] ADPF indisponível antes da API 33; fallback ativo.");
    return false;
  }
  androidLibrary_ = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  if (androidLibrary_ == nullptr) return false;
  const auto releaseHintApi = [this]() {
    if (hintSession_ != nullptr && closeSession_ != nullptr) closeSession_(hintSession_);
    hintSession_ = nullptr;
    hintManager_ = nullptr;
    getManager_ = nullptr;
    createSession_ = nullptr;
    preferredRate_ = nullptr;
    updateTarget_ = nullptr;
    reportActual_ = nullptr;
    closeSession_ = nullptr;
    if (androidLibrary_ != nullptr) dlclose(androidLibrary_);
    androidLibrary_ = nullptr;
    preferredUpdateRateNs_ = 0;
  };
  getManager_ = loadSymbol<GetManagerFn>(androidLibrary_, "APerformanceHint_getManager");
  createSession_ = loadSymbol<CreateSessionFn>(androidLibrary_, "APerformanceHint_createSession");
  preferredRate_ = loadSymbol<PreferredRateFn>(
      androidLibrary_, "APerformanceHint_getPreferredUpdateRateNanos");
  updateTarget_ = loadSymbol<UpdateTargetFn>(
      androidLibrary_, "APerformanceHint_updateTargetWorkDuration");
  reportActual_ = loadSymbol<ReportActualFn>(
      androidLibrary_, "APerformanceHint_reportActualWorkDuration");
  closeSession_ = loadSymbol<CloseSessionFn>(androidLibrary_, "APerformanceHint_closeSession");
  if (getManager_ == nullptr || createSession_ == nullptr || preferredRate_ == nullptr ||
      updateTarget_ == nullptr || reportActual_ == nullptr || closeSession_ == nullptr) {
    // Game State remains useful even if a vendor image omits one optional
    // native symbol. Do not clear activity_ by calling the full shutdown.
    releaseHintApi();
    return false;
  }
  hintManager_ = getManager_();
  if (hintManager_ == nullptr) {
    releaseHintApi();
    return false;
  }
  const i32 renderThread = static_cast<i32>(gettid());
  hintSession_ = createSession_(hintManager_, &renderThread, 1, targetFrameDurationNs_);
  if (hintSession_ == nullptr) {
    releaseHintApi();
    return false;
  }
  preferredUpdateRateNs_ = preferredRate_(hintManager_);
  __android_log_print(ANDROID_LOG_INFO, LogTag,
      "[PerformancePolicy] ADPF ativo thread=%d target_ms=%.3f update_ms=%.3f game_mode=%d.",
      renderThread, static_cast<double>(targetFrameDurationNs_) / 1e6,
      static_cast<double>(preferredUpdateRateNs_) / 1e6, gameMode_);
  return true;
}

void AndroidPerformance::shutdown() {
  if (sustainedPerformanceEnabled_) setSustainedPerformance(false);
  if (hintSession_ != nullptr && closeSession_ != nullptr) closeSession_(hintSession_);
  hintSession_ = nullptr;
  hintManager_ = nullptr;
  getManager_ = nullptr;
  createSession_ = nullptr;
  preferredRate_ = nullptr;
  updateTarget_ = nullptr;
  reportActual_ = nullptr;
  closeSession_ = nullptr;
  if (androidLibrary_ != nullptr) dlclose(androidLibrary_);
  androidLibrary_ = nullptr;
  activity_ = nullptr;
  targetFrameDurationNs_ = 0;
  preferredUpdateRateNs_ = 0;
  reportFailures_ = 0;
  preferSustainedPerformance_ = true;
  sustainedPerformanceSupported_ = false;
  sustainedPerformanceEnabled_ = false;
}

void AndroidPerformance::setActive(bool active, bool loading) {
  setSustainedPerformance(active && preferSustainedPerformance_);
  publishGameState(active, loading);
}

void AndroidPerformance::updateTargetFrameDuration(i64 targetFrameDurationNs) {
  if (targetFrameDurationNs <= 0 || targetFrameDurationNs == targetFrameDurationNs_) return;
  targetFrameDurationNs_ = targetFrameDurationNs;
  if (hintSession_ != nullptr && updateTarget_ != nullptr)
    updateTarget_(hintSession_, targetFrameDurationNs_);
}

void AndroidPerformance::reportFrameDuration(i64 actualFrameDurationNs) {
  if (hintSession_ == nullptr || reportActual_ == nullptr || actualFrameDurationNs <= 0) return;
  const int result = reportActual_(hintSession_, actualFrameDurationNs);
  if (result == 0) {
    reportFailures_ = 0;
    return;
  }
  if (++reportFailures_ == 3) {
    __android_log_print(ANDROID_LOG_WARN, LogTag,
                        "[PerformancePolicy] ADPF report falhou repetidamente: errno=%d.", result);
  }
}

bool AndroidPerformance::publishGameState(bool active, bool loading) {
  if (activity_ == nullptr || android_get_device_api_level() < Android13Api) return false;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  if (!attach(activity_, env, attachedHere)) return false;
  const bool localFrame = env->PushLocalFrame(16) == JNI_OK;
  const bool ok = localFrame && [&]() {
    jclass activityClass = env->GetObjectClass(activity_->clazz);
    jmethodID getService = activityClass == nullptr ? nullptr : env->GetMethodID(
        activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("game");
    if (getService == nullptr || serviceName == nullptr) return false;
    jobject manager = env->CallObjectMethod(activity_->clazz, getService, serviceName);
    if (env->ExceptionCheck() || manager == nullptr) return false;
    jclass managerClass = env->GetObjectClass(manager);
    jmethodID getMode = managerClass == nullptr ? nullptr : env->GetMethodID(
        managerClass, "getGameMode", "()I");
    jmethodID setState = managerClass == nullptr ? nullptr : env->GetMethodID(
        managerClass, "setGameState", "(Landroid/app/GameState;)V");
    if (getMode == nullptr || setState == nullptr) return false;
    gameMode_ = env->CallIntMethod(manager, getMode);
    if (env->ExceptionCheck()) return false;
    jclass stateClass = env->FindClass("android/app/GameState");
    jmethodID constructor = stateClass == nullptr ? nullptr : env->GetMethodID(stateClass, "<init>", "(ZI)V");
    if (constructor == nullptr) return false;
    const i32 mode = active ? GameStateModeGameplayUninterruptible : GameStateModeNone;
    jobject state = env->NewObject(stateClass, constructor, loading ? JNI_TRUE : JNI_FALSE, mode);
    if (state == nullptr) return false;
    env->CallVoidMethod(manager, setState, state);
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity_->vm->DetachCurrentThread();
  if (!ok) gameMode_ = GameModeUnsupported;
  return ok;
}

bool AndroidPerformance::querySustainedPerformanceSupport() {
  if (activity_ == nullptr || android_get_device_api_level() < Android7Api) return false;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  if (!attach(activity_, env, attachedHere)) return false;
  const bool localFrame = env->PushLocalFrame(12) == JNI_OK;
  const bool supported = localFrame && [&]() {
    jclass activityClass = env->GetObjectClass(activity_->clazz);
    jmethodID getService = activityClass == nullptr ? nullptr : env->GetMethodID(
        activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("power");
    if (getService == nullptr || serviceName == nullptr) return false;
    jobject manager = env->CallObjectMethod(activity_->clazz, getService, serviceName);
    if (env->ExceptionCheck() || manager == nullptr) return false;
    jclass managerClass = env->GetObjectClass(manager);
    jmethodID isSupported = managerClass == nullptr ? nullptr : env->GetMethodID(
        managerClass, "isSustainedPerformanceModeSupported", "()Z");
    if (isSupported == nullptr) return false;
    const bool result = env->CallBooleanMethod(manager, isSupported) == JNI_TRUE;
    return !env->ExceptionCheck() && result;
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity_->vm->DetachCurrentThread();
  return supported;
}

bool AndroidPerformance::setSustainedPerformance(bool enabled) {
  if (!sustainedPerformanceSupported_ || activity_ == nullptr ||
      sustainedPerformanceEnabled_ == enabled) return sustainedPerformanceEnabled_ == enabled;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  if (!attach(activity_, env, attachedHere)) return false;
  const bool localFrame = env->PushLocalFrame(8) == JNI_OK;
  const bool ok = localFrame && [&]() {
    jclass activityClass = env->GetObjectClass(activity_->clazz);
    jmethodID getWindow = activityClass == nullptr ? nullptr : env->GetMethodID(
        activityClass, "getWindow", "()Landroid/view/Window;");
    if (getWindow == nullptr) return false;
    jobject window = env->CallObjectMethod(activity_->clazz, getWindow);
    if (env->ExceptionCheck() || window == nullptr) return false;
    jclass windowClass = env->GetObjectClass(window);
    jmethodID setMode = windowClass == nullptr ? nullptr : env->GetMethodID(
        windowClass, "setSustainedPerformanceMode", "(Z)V");
    if (setMode == nullptr) return false;
    env->CallVoidMethod(window, setMode, enabled ? JNI_TRUE : JNI_FALSE);
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity_->vm->DetachCurrentThread();
  if (ok) {
    sustainedPerformanceEnabled_ = enabled;
    __android_log_print(ANDROID_LOG_INFO, LogTag,
                        "[PerformancePolicy] sustained enabled=%s.",
                        enabled ? "true" : "false");
  }
  return ok;
}

} // namespace ae::platform::android
