#include "platform/android/android_launch_options.h"
#include <android/native_activity.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
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

bool readUnsignedLaunchOption(ANativeActivity *activity, const char *option, u32 &value) {
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
  jint decoded = -1;
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
    jmethodID getInt = env->GetMethodID(intentClass, "getIntExtra", "(Ljava/lang/String;I)I");
    if (getInt == nullptr) return false;
    jstring key = env->NewStringUTF(option);
    if (key == nullptr) return false;
    decoded = env->CallIntMethod(intent, getInt, key, static_cast<jint>(-1));
    return !env->ExceptionCheck();
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();
  if (!ok || decoded < 0) return false;
  value = static_cast<u32>(decoded);
  return true;
}

bool readStringLaunchOption(ANativeActivity *activity, const char *option, char *buffer,
                            usize bufferSize) {
  if (activity == nullptr || activity->vm == nullptr || option == nullptr || buffer == nullptr ||
      bufferSize == 0) return false;
  JNIEnv *env = nullptr;
  bool attachedHere = false;
  const jint result = activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attachedHere = true;
  } else if (result != JNI_OK) {
    return false;
  }
  bool decoded = false;
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
    jmethodID getString = env->GetMethodID(intentClass, "getStringExtra",
        "(Ljava/lang/String;)Ljava/lang/String;");
    if (getString == nullptr) return false;
    jstring key = env->NewStringUTF(option);
    if (key == nullptr) return false;
    auto value = static_cast<jstring>(env->CallObjectMethod(intent, getString, key));
    if (env->ExceptionCheck() || value == nullptr) return false;
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return false;
    const usize length = std::strlen(chars);
    if (length >= bufferSize) {
      env->ReleaseStringUTFChars(value, chars);
      return false;
    }
    std::memcpy(buffer, chars, length);
    buffer[length] = '\0';
    env->ReleaseStringUTFChars(value, chars);
    decoded = true;
    return true;
  }();
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (localFrame) env->PopLocalFrame(nullptr);
  if (attachedHere) activity->vm->DetachCurrentThread();
  return ok && decoded;
}

} // namespace ae::platform::android
