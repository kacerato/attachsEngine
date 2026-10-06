#pragma once
#include "core/base.h"

#include <android/native_activity.h>

namespace ae::platform::android {
// Vibração única pelo serviço do sistema (VibrationEffect.createOneShot, API 26).
// Falso quando o aparelho não tem vibrador, a permissão falta ou o JNI falha:
// quem chama recusa a operação em vez de fingir que vibrou.
bool vibrateOnce(ANativeActivity *activity,u32 milliseconds,float amplitude);
}
