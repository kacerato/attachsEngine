#include "platform/android/android_launch_options.h"
#include <android/native_activity.h>
#include <android/log.h>
#include <cmath>
#include <limits>

namespace ae::platform::android {
namespace { constexpr const char *LogTag = "Aether.Android"; }
// Leitura somente no lançamento; nenhuma referência JNI é retida no loop.
bool readBooleanLaunchOption(ANativeActivity *activity, const char *option) {
  if (activity == nullptr || activity->vm == nullptr || option == nullptr) return false;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LaunchOptions] Falha ao anexar JNI.");
      return false;
    }
    attachedHere = true;
  } else if (result != JNI_OK) {
    __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LaunchOptions] JNI indisponível.");
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
    jstring key = env->NewStringUTF(option);
    if (key == nullptr) return false;
    enabled = env->CallBooleanMethod(intent, getBoolean, key, JNI_FALSE) == JNI_TRUE;
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();
  if (!ok) __android_log_print(ANDROID_LOG_ERROR, LogTag, "[LaunchOptions] Falha ao ler opção de lançamento.");
  return ok && enabled;
}

bool readFloatLaunchOption(ANativeActivity *activity, const char *option, float &value) {
  if (activity == nullptr || activity->vm == nullptr || option == nullptr) return false;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attachedHere = true;
  } else if (result != JNI_OK) {
    return false;
  }
  float decoded = std::numeric_limits<float>::quiet_NaN();
  const bool localFrame = env->PushLocalFrame(8) == JNI_OK;
  const bool ok = localFrame && [&]() {
    jclass activityClass = env->GetObjectClass(activity->clazz);
    if (activityClass == nullptr) return false;
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    if (getIntent == nullptr) return false;
    jobject intent = env->CallObjectMethod(activity->clazz, getIntent);
    if (env->ExceptionCheck() || intent == nullptr) return false;
    jclass intentClass = env->GetObjectClass(intent);
    if (intentClass == nullptr) return false;
    jmethodID getFloat = env->GetMethodID(intentClass, "getFloatExtra", "(Ljava/lang/String;F)F");
    if (getFloat == nullptr) return false;
    jstring key = env->NewStringUTF(option);
    if (key == nullptr) return false;
    jvalue arguments[2]{};
    arguments[0].l = key;
    arguments[1].f = std::numeric_limits<float>::quiet_NaN();
    decoded = env->CallFloatMethodA(intent, getFloat, arguments);
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();
  if (!ok || !std::isfinite(decoded)) return false;
  value = decoded;
  return true;
}

} // namespace ae::platform::android
