#include "platform/android/android_window.h"

#include <android/log.h>
#include <android/native_activity.h>
#include <android/window.h>

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

} // namespace ae::platform::android
