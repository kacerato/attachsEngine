#pragma once

struct ANativeActivity;
struct ANativeWindow;

namespace ae::platform::android {

// O Android limpa flags imersivas quando o foco muda; por isso esta função é
// segura para ser chamada tanto na inicialização quanto em GAINED_FOCUS.
bool applyImmersiveLandscapeWindow(ANativeActivity *activity);

// Consulta a maior taxa anunciada pelos modos físicos do display principal.
// É uma capability, não uma estimativa de desempenho; a política global ainda
// pode escolher uma meta menor e o compositor pode reduzi-la por energia/térmica.
float queryMaximumDisplayRefreshRate(ANativeActivity *activity,
                                     float fallbackFramesPerSecond = 60.0f);

// Android Display.getRotation(): 0, 1, 2 or 3 quarter-turns from the natural
// device orientation. Used only at configuration boundaries; touch handling
// stays free of JNI and per-event platform queries.
int queryDisplayRotation(ANativeActivity *activity, int fallbackRotation = -1);

// Solicita a cadência da superfície, em vez de aceitar o override de 60 Hz que
// Android 15+ pode aplicar mesmo num painel de 90/120 Hz. O compositor ainda é
// a autoridade final e escolhe a taxa compatível com o display/estado térmico.
bool requestRenderFrameRate(ANativeWindow *window, float framesPerSecond);

} // namespace ae::platform::android
