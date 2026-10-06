#include "platform/android/android_haptics.h"

#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <cmath>

namespace ae::platform::android {
namespace {
constexpr const char *LogTag = "AetherHaptics";
}

bool vibrateOnce(ANativeActivity *activity,u32 milliseconds,float amplitude) {
  if(!activity || !activity->vm || milliseconds==0 || !std::isfinite(amplitude)) return false;
  JNIEnv *env=nullptr;bool attachedHere=false;
  const jint state=activity->vm->GetEnv(reinterpret_cast<void **>(&env),JNI_VERSION_1_6);
  if(state==JNI_EDETACHED) {
    if(activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK) return false;
    attachedHere=true;
  } else if(state!=JNI_OK) return false;
  bool vibrated=false;
  if(env->PushLocalFrame(16)==JNI_OK) {
    vibrated=[&]() {
      jclass contextClass=env->FindClass("android/content/Context");
      if(!contextClass) return false;
      jfieldID serviceField=env->GetStaticFieldID(contextClass,"VIBRATOR_SERVICE","Ljava/lang/String;");
      if(!serviceField) return false;
      jobject serviceName=env->GetStaticObjectField(contextClass,serviceField);
      jmethodID getService=env->GetMethodID(contextClass,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
      if(!serviceName || !getService) return false;
      jobject vibrator=env->CallObjectMethod(activity->clazz,getService,serviceName);
      if(env->ExceptionCheck() || !vibrator) return false;
      jclass vibratorClass=env->FindClass("android/os/Vibrator");
      if(!vibratorClass) return false;
      jmethodID hasVibrator=env->GetMethodID(vibratorClass,"hasVibrator","()Z");
      if(!hasVibrator || !env->CallBooleanMethod(vibrator,hasVibrator) || env->ExceptionCheck()) return false;
      jclass effectClass=env->FindClass("android/os/VibrationEffect");
      if(!effectClass) return false;
      jmethodID createOneShot=env->GetStaticMethodID(effectClass,"createOneShot","(JI)Landroid/os/VibrationEffect;");
      if(!createOneShot) return false;
      // Amplitude 1..255; zero pede a padrão do aparelho (DEFAULT_AMPLITUDE = -1).
      const jint level=amplitude<=0?-1:static_cast<jint>(std::clamp(std::lround(amplitude*255.f),1l,255l));
      jobject effect=env->CallStaticObjectMethod(effectClass,createOneShot,static_cast<jlong>(milliseconds),level);
      if(env->ExceptionCheck() || !effect) return false;
      jmethodID vibrate=env->GetMethodID(vibratorClass,"vibrate","(Landroid/os/VibrationEffect;)V");
      if(!vibrate) return false;
      env->CallVoidMethod(vibrator,vibrate,effect);
      return !env->ExceptionCheck();
    }();
    if(env->ExceptionCheck()) {
      env->ExceptionClear();
      __android_log_print(ANDROID_LOG_WARN,LogTag,"Vibração recusada pelo sistema");
      vibrated=false;
    }
    env->PopLocalFrame(nullptr);
  }
  if(attachedHere) activity->vm->DetachCurrentThread();
  return vibrated;
}
}
