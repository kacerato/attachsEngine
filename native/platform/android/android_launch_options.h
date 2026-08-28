#pragma once
struct ANativeActivity;
namespace ae::platform::android {
bool readBooleanLaunchOption(ANativeActivity *activity, const char *option);
}
