#pragma once
#include "core/base.h"

struct ANativeActivity;
namespace ae::platform::android {
bool readBooleanLaunchOption(ANativeActivity *activity, const char *option, bool defaultValue = false);
bool readFloatLaunchOption(ANativeActivity *activity, const char *option, float &value);
bool readUnsignedLaunchOption(ANativeActivity *activity, const char *option, u32 &value);
// Copies the string extra into buffer (bufferSize includes the trailing NUL).
// Fails closed on a missing extra or a value that would not fit -- callers
// never receive a silently truncated path/id.
bool readStringLaunchOption(ANativeActivity *activity, const char *option, char *buffer,
                            usize bufferSize);
}
