#pragma once

struct ANativeActivity;

namespace ae::platform::android {

// O Android limpa flags imersivas quando o foco muda; por isso esta função é
// segura para ser chamada tanto na inicialização quanto em GAINED_FOCUS.
bool applyImmersiveLandscapeWindow(ANativeActivity *activity);

} // namespace ae::platform::android
