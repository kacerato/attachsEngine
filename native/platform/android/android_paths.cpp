#include "platform/android/android_paths.h"

#include <android/log.h>
#include <android/native_activity.h>
#include <cstring>

namespace ae::platform::android {

namespace {
constexpr const char *LogTag = "Aether.Android";

bool clearJavaException(JNIEnv *environment, const char *operation) {
  if (!environment->ExceptionCheck()) return false;
  environment->ExceptionClear();
  __android_log_print(ANDROID_LOG_ERROR, LogTag, "Falha Java ao resolver caminho: %s.", operation);
  return true;
}
} // namespace

bool getNativeLibraryDir(ANativeActivity *activity, char *outBuffer, int outBufferSize) {
  if (activity == nullptr || activity->vm == nullptr || activity->clazz == nullptr ||
      outBuffer == nullptr || outBufferSize <= 0) {
    return false;
  }

  JNIEnv *environment = nullptr;
  bool attachedHere = false;
  const jint environmentResult =
      activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6);
  if (environmentResult == JNI_EDETACHED) {
    if (activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return false;
    attachedHere = true;
  } else if (environmentResult != JNI_OK) {
    return false;
  }

  bool success = false;
  // Context.getApplicationInfo().nativeLibraryDir — três chamadas encadeadas,
  // limpando exceção e referência local a cada passo (mesmo padrão de
  // android_window.cpp).
  jclass activityClass = environment->GetObjectClass(activity->clazz);
  if (activityClass != nullptr && !clearJavaException(environment, "obter classe da Activity")) {
    jmethodID getApplicationInfo = environment->GetMethodID(
        activityClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    if (getApplicationInfo != nullptr &&
        !clearJavaException(environment, "localizar getApplicationInfo")) {
      jobject applicationInfo = environment->CallObjectMethod(activity->clazz, getApplicationInfo);
      if (applicationInfo != nullptr && !clearJavaException(environment, "obter ApplicationInfo")) {
        jclass applicationInfoClass = environment->GetObjectClass(applicationInfo);
        jfieldID nativeLibraryDirField =
            applicationInfoClass == nullptr
                ? nullptr
                : environment->GetFieldID(applicationInfoClass, "nativeLibraryDir",
                                          "Ljava/lang/String;");
        if (nativeLibraryDirField != nullptr &&
            !clearJavaException(environment, "localizar campo nativeLibraryDir")) {
          auto nativeLibraryDir = static_cast<jstring>(
              environment->GetObjectField(applicationInfo, nativeLibraryDirField));
          if (nativeLibraryDir != nullptr && !clearJavaException(environment, "ler nativeLibraryDir")) {
            const char *chars = environment->GetStringUTFChars(nativeLibraryDir, nullptr);
            if (chars != nullptr) {
              const size_t length = std::strlen(chars);
              if (length < static_cast<size_t>(outBufferSize)) {
                std::memcpy(outBuffer, chars, length + 1);
                success = true;
              } else {
                __android_log_print(ANDROID_LOG_ERROR, LogTag,
                                    "nativeLibraryDir excede o buffer fornecido (%zu >= %d).",
                                    length, outBufferSize);
              }
              environment->ReleaseStringUTFChars(nativeLibraryDir, chars);
            }
            environment->DeleteLocalRef(nativeLibraryDir);
          }
        }
        if (applicationInfoClass != nullptr) environment->DeleteLocalRef(applicationInfoClass);
        environment->DeleteLocalRef(applicationInfo);
      }
    }
  }
  if (activityClass != nullptr) environment->DeleteLocalRef(activityClass);

  if (attachedHere) activity->vm->DetachCurrentThread();
  return success;
}

} // namespace ae::platform::android
