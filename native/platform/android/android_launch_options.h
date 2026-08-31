#pragma once
#include "core/base.h"

struct ANativeActivity;
namespace ae::platform::android {
bool readBooleanLaunchOption(ANativeActivity *activity, const char *option);
bool readFloatLaunchOption(ANativeActivity *activity, const char *option, float &value);
bool readUnsignedLaunchOption(ANativeActivity *activity, const char *option, u32 &value);
}
