#pragma once

struct ANativeActivity;
struct ANativeWindow;

namespace ae::platform::android {

// O Android limpa flags imersivas quando o foco muda; por isso esta função é
// segura para ser chamada tanto na inicialização quanto em GAINED_FOCUS.
bool applyImmersiveLandscapeWindow(ANativeActivity *activity);

// Solicita a cadência da superfície, em vez de aceitar o override de 60 Hz que
// Android 15+ pode aplicar mesmo num painel de 90/120 Hz. O compositor ainda é
// a autoridade final e escolhe a taxa compatível com o display/estado térmico.
bool requestRenderFrameRate(ANativeWindow *window, float framesPerSecond);

} // namespace ae::platform::android
