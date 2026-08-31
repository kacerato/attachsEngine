#include "platform/android/android_window.h"

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/window.h>
#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";

bool clearJavaException(JNIEnv *environment, const char *operation) {
  if (!environment->ExceptionCheck()) return false;
  environment->ExceptionClear();
  __android_log_print(ANDROID_LOG_ERROR, LogTag,
                      "Falha Java ao configurar janela imersiva: %s.", operation);
  return true;
}
} // namespace

bool applyImmersiveLandscapeWindow(ANativeActivity *activity) {
  if (activity == nullptr || activity->vm == nullptr || activity->clazz == nullptr) return false;

  ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_FULLSCREEN, 0);

  JNIEnv *environment = nullptr;
  bool attachedHere = false;
  const jint environmentResult = activity->vm->GetEnv(
      reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (environmentResult == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return false;
    attachedHere = true;
  } else if (environmentResult != JNI_OK) {
    return false;
  }

  bool success = false;
  jclass activityClass = environment->GetObjectClass(activity->clazz);
  if (activityClass != nullptr && !clearJavaException(environment, "obter Activity")) {
    jmethodID getWindow = environment->GetMethodID(
        activityClass, "getWindow", "()Landroid/view/Window;");
    if (getWindow != nullptr && !clearJavaException(environment, "localizar Window")) {
      jobject window = environment->CallObjectMethod(activity->clazz, getWindow);
      if (window != nullptr && !clearJavaException(environment, "obter Window")) {
        jclass windowClass = environment->GetObjectClass(window);
        jmethodID getDecorView = windowClass == nullptr
                                    ? nullptr
                                    : environment->GetMethodID(
                                          windowClass, "getDecorView", "()Landroid/view/View;");
        if (getDecorView != nullptr && !clearJavaException(environment, "localizar DecorView")) {
          jobject decorView = environment->CallObjectMethod(window, getDecorView);
          if (decorView != nullptr && !clearJavaException(environment, "obter DecorView")) {
            jclass viewClass = environment->GetObjectClass(decorView);
            jmethodID setVisibility = viewClass == nullptr
                                          ? nullptr
                                          : environment->GetMethodID(
                                                viewClass, "setSystemUiVisibility", "(I)V");
            if (setVisibility != nullptr &&
                !clearJavaException(environment, "localizar flags de sistema")) {
              constexpr jint LayoutStable = 0x00000100;
              constexpr jint LayoutHideNavigation = 0x00000200;
              constexpr jint LayoutFullscreen = 0x00000400;
              constexpr jint HideNavigation = 0x00000002;
              constexpr jint Fullscreen = 0x00000004;
              constexpr jint ImmersiveSticky = 0x00001000;
              environment->CallVoidMethod(
                  decorView, setVisibility,
                  LayoutStable | LayoutHideNavigation | LayoutFullscreen |
                      HideNavigation | Fullscreen | ImmersiveSticky);
              success = !clearJavaException(environment, "aplicar flags de sistema");
            }
            if (viewClass != nullptr) environment->DeleteLocalRef(viewClass);
            environment->DeleteLocalRef(decorView);
          }
        }
        if (windowClass != nullptr) environment->DeleteLocalRef(windowClass);
        environment->DeleteLocalRef(window);
      }
    }
  }
  if (activityClass != nullptr) environment->DeleteLocalRef(activityClass);

  if (attachedHere) activity->vm->DetachCurrentThread();
  return success;
}

float queryMaximumDisplayRefreshRate(ANativeActivity *activity,
                                     float fallbackFramesPerSecond) {
  const float safeFallback = std::isfinite(fallbackFramesPerSecond) &&
                                     fallbackFramesPerSecond > 0.0f
                                 ? fallbackFramesPerSecond
                                 : 60.0f;
  if (activity == nullptr || activity->vm == nullptr || activity->clazz == nullptr) {
    return safeFallback;
  }

  JNIEnv *environment = nullptr;
  bool attachedHere = false;
  const jint environmentResult = activity->vm->GetEnv(
      reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (environmentResult == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) {
      return safeFallback;
    }
    attachedHere = true;
  } else if (environmentResult != JNI_OK) {
    return safeFallback;
  }

  float maximumRefreshRate = 0.0f;
  const bool localFrame = environment->PushLocalFrame(16) == JNI_OK;
  const bool success = localFrame && [&]() {
    jclass activityClass = environment->GetObjectClass(activity->clazz);
    if (activityClass == nullptr) return false;
    jmethodID getWindowManager = environment->GetMethodID(
        activityClass, "getWindowManager", "()Landroid/view/WindowManager;");
    if (getWindowManager == nullptr) return false;
    jobject windowManager = environment->CallObjectMethod(activity->clazz, getWindowManager);
    if (environment->ExceptionCheck() || windowManager == nullptr) return false;

    jclass windowManagerClass = environment->GetObjectClass(windowManager);
    if (windowManagerClass == nullptr) return false;
    jmethodID getDefaultDisplay = environment->GetMethodID(
        windowManagerClass, "getDefaultDisplay", "()Landroid/view/Display;");
    if (getDefaultDisplay == nullptr) return false;
    jobject display = environment->CallObjectMethod(windowManager, getDefaultDisplay);
    if (environment->ExceptionCheck() || display == nullptr) return false;

    jclass displayClass = environment->GetObjectClass(display);
    if (displayClass == nullptr) return false;
    jmethodID getSupportedModes = environment->GetMethodID(
        displayClass, "getSupportedModes", "()[Landroid/view/Display$Mode;");
    if (getSupportedModes == nullptr) return false;
    auto modes = static_cast<jobjectArray>(
        environment->CallObjectMethod(display, getSupportedModes));
    if (environment->ExceptionCheck() || modes == nullptr) return false;

    jclass modeClass = environment->FindClass("android/view/Display$Mode");
    if (modeClass == nullptr) return false;
    jmethodID getRefreshRate = environment->GetMethodID(modeClass, "getRefreshRate", "()F");
    if (getRefreshRate == nullptr) return false;

    const jsize modeCount = environment->GetArrayLength(modes);
    for (jsize index = 0; index < modeCount; ++index) {
      jobject mode = environment->GetObjectArrayElement(modes, index);
      if (mode == nullptr) continue;
      const float refreshRate = environment->CallFloatMethod(mode, getRefreshRate);
      environment->DeleteLocalRef(mode);
      if (environment->ExceptionCheck()) return false;
      if (std::isfinite(refreshRate) && refreshRate > 0.0f) {
        maximumRefreshRate = std::max(maximumRefreshRate, refreshRate);
      }
    }
    return maximumRefreshRate > 0.0f;
  }();

  if (environment->ExceptionCheck()) environment->ExceptionClear();
  if (localFrame) environment->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();

  const float resolvedRefreshRate = success ? maximumRefreshRate : safeFallback;
  __android_log_print(success ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, LogTag,
                      "[FramePolicy] painel máximo=%.2f Hz%s.",
                      static_cast<double>(resolvedRefreshRate),
                      success ? "" : " (fallback)");
  return resolvedRefreshRate;
}

int queryDisplayRotation(ANativeActivity *activity, int fallbackRotation) {
  if (activity == nullptr || activity->vm == nullptr || activity->clazz == nullptr) {
    return fallbackRotation;
  }
  JNIEnv *environment = nullptr;
  bool attachedHere = false;
  const jint environmentResult = activity->vm->GetEnv(
      reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (environmentResult == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) {
      return fallbackRotation;
    }
    attachedHere = true;
  } else if (environmentResult != JNI_OK) {
    return fallbackRotation;
  }

  int rotation = fallbackRotation;
  const bool localFrame = environment->PushLocalFrame(12) == JNI_OK;
  if (localFrame) {
    jclass activityClass = environment->GetObjectClass(activity->clazz);
    jmethodID getWindowManager = activityClass == nullptr ? nullptr : environment->GetMethodID(
        activityClass, "getWindowManager", "()Landroid/view/WindowManager;");
    jobject windowManager = getWindowManager == nullptr ? nullptr :
        environment->CallObjectMethod(activity->clazz, getWindowManager);
    jclass windowManagerClass = windowManager == nullptr ? nullptr :
        environment->GetObjectClass(windowManager);
    jmethodID getDefaultDisplay = windowManagerClass == nullptr ? nullptr :
        environment->GetMethodID(windowManagerClass, "getDefaultDisplay",
                                 "()Landroid/view/Display;");
    jobject display = getDefaultDisplay == nullptr ? nullptr :
        environment->CallObjectMethod(windowManager, getDefaultDisplay);
    jclass displayClass = display == nullptr ? nullptr : environment->GetObjectClass(display);
    jmethodID getRotation = displayClass == nullptr ? nullptr :
        environment->GetMethodID(displayClass, "getRotation", "()I");
    if (!environment->ExceptionCheck() && getRotation != nullptr) {
      const jint queried = environment->CallIntMethod(display, getRotation);
      if (!environment->ExceptionCheck() && queried >= 0 && queried <= 3) rotation = queried;
    }
    if (environment->ExceptionCheck()) environment->ExceptionClear();
    environment->PopLocalFrame(nullptr);
  }
  if (attachedHere) activity->vm->DetachCurrentThread();
  return rotation;
}

bool requestRenderFrameRate(ANativeWindow *window, float framesPerSecond) {
  if (window == nullptr || !std::isfinite(framesPerSecond) || framesPerSecond <= 0.0f) return false;

  // minSdk 26: resolver em runtime mantém o APK compatível. A função existe a
  // partir da API 30; abaixo disso o Choreographer/swapchain permanecem válidos.
  using SetFrameRate = int32_t (*)(ANativeWindow *, float, int8_t);
  // O linker por namespace do Android não garante que símbolos de uma
  // dependência apareçam em RTLD_DEFAULT. Abrir libandroid explicitamente é
  // necessário em alguns OEMs, embora a função exista no nível de API.
  static void *libandroid = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  auto setFrameRate = libandroid == nullptr ? nullptr : reinterpret_cast<SetFrameRate>(
      dlsym(libandroid, "ANativeWindow_setFrameRate"));
  if (setFrameRate == nullptr) {
    const char *error = dlerror();
    __android_log_print(ANDROID_LOG_INFO, LogTag,
                        "[FramePolicy] Surface.setFrameRate indisponível: %s.",
                        error != nullptr ? error : "símbolo ausente");
    return false;
  }
  constexpr int8_t CompatibilityDefault = 0;
  const int32_t result = setFrameRate(window, framesPerSecond, CompatibilityDefault);
  __android_log_print(result == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, LogTag,
                      "[FramePolicy] solicitação da surface=%.0f Hz resultado=%d.",
                      static_cast<double>(framesPerSecond), result);
  return result == 0;
}

} // namespace ae::platform::android
