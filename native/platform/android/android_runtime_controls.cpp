#include "platform/android/android_runtime_controls.h"

#include <jni.h>
#include <algorithm>
#include <atomic>

namespace {
struct SharedControls final {
  std::atomic<ae::u64> revision{0};
  std::atomic<float> renderScale{1.0f};
  std::atomic<ae::u32> shadowQuality{3};
  std::atomic<bool> dynamicResolution{true};
  std::atomic<float> bloomIntensity{0.08f};
  std::atomic<float> sharpen{0.12f};
  std::atomic<float> waveHeight{1.0f};
  std::atomic<float> waveSpeed{1.0f};
  std::atomic<float> waveSteepness{1.0f};
  std::atomic<float> microWaves{1.0f};
  std::atomic<float> surfaceOpacity{0.72f};
  std::atomic<float> absorption{1.0f};
  std::atomic<float> foam{0.65f};
  std::atomic<float> interactionStrength{0.65f};
} controls;

float bounded(float value, float minimum, float maximum, float fallback) {
  return value >= minimum && value <= maximum ? value : fallback;
}
}

namespace ae::platform::android {

AndroidRuntimeControls runtimeControlsSnapshot() noexcept {
  AndroidRuntimeControls snapshot{};
  // Single UI writer, render-thread readers. An odd revision means the writer
  // is changing several independent axes; retry rather than observe a hybrid.
  for (;;) {
    const u64 before = controls.revision.load(std::memory_order_acquire);
    if ((before & 1u) != 0) continue;
    snapshot.renderScale = controls.renderScale.load(std::memory_order_relaxed);
    snapshot.shadowQuality = controls.shadowQuality.load(std::memory_order_relaxed);
    snapshot.dynamicResolution = controls.dynamicResolution.load(std::memory_order_relaxed);
    snapshot.bloomIntensity = controls.bloomIntensity.load(std::memory_order_relaxed);
    snapshot.sharpen = controls.sharpen.load(std::memory_order_relaxed);
    snapshot.waveHeight = controls.waveHeight.load(std::memory_order_relaxed);
    snapshot.waveSpeed = controls.waveSpeed.load(std::memory_order_relaxed);
    snapshot.waveSteepness = controls.waveSteepness.load(std::memory_order_relaxed);
    snapshot.microWaves = controls.microWaves.load(std::memory_order_relaxed);
    snapshot.surfaceOpacity = controls.surfaceOpacity.load(std::memory_order_relaxed);
    snapshot.absorption = controls.absorption.load(std::memory_order_relaxed);
    snapshot.foam = controls.foam.load(std::memory_order_relaxed);
    snapshot.interactionStrength = controls.interactionStrength.load(std::memory_order_relaxed);
    const u64 after = controls.revision.load(std::memory_order_acquire);
    if (before == after) { snapshot.revision = after; return snapshot; }
  }
}

} // namespace ae::platform::android

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_AetherActivity_nativeApplyControls(
    JNIEnv *, jclass, jfloat renderScale, jint shadowQuality, jboolean dynamicResolution,
    jfloat bloomIntensity, jfloat sharpen, jfloat waveHeight, jfloat waveSpeed,
    jfloat waveSteepness, jfloat microWaves, jfloat surfaceOpacity, jfloat absorption,
    jfloat foam, jfloat interactionStrength) {
  controls.revision.fetch_add(1, std::memory_order_acq_rel);
  controls.renderScale.store(bounded(renderScale, 0.5f, 1.0f, 1.0f), std::memory_order_relaxed);
  controls.shadowQuality.store(static_cast<ae::u32>(std::clamp(shadowQuality, 0, 3)),
                               std::memory_order_relaxed);
  controls.dynamicResolution.store(dynamicResolution == JNI_TRUE, std::memory_order_relaxed);
  controls.bloomIntensity.store(bounded(bloomIntensity, 0.0f, 1.5f, 0.08f),
                                std::memory_order_relaxed);
  controls.sharpen.store(bounded(sharpen, 0.0f, 1.0f, 0.12f), std::memory_order_relaxed);
  controls.waveHeight.store(bounded(waveHeight, 0.0f, 3.0f, 1.0f), std::memory_order_relaxed);
  controls.waveSpeed.store(bounded(waveSpeed, 0.0f, 3.0f, 1.0f), std::memory_order_relaxed);
  controls.waveSteepness.store(bounded(waveSteepness, 0.0f, 2.0f, 1.0f),
                               std::memory_order_relaxed);
  controls.microWaves.store(bounded(microWaves, 0.0f, 3.0f, 1.0f), std::memory_order_relaxed);
  controls.surfaceOpacity.store(bounded(surfaceOpacity, 0.05f, 1.0f, 0.72f),
                                std::memory_order_relaxed);
  controls.absorption.store(bounded(absorption, 0.1f, 4.0f, 1.0f), std::memory_order_relaxed);
  controls.foam.store(bounded(foam, 0.0f, 2.0f, 0.65f), std::memory_order_relaxed);
  controls.interactionStrength.store(bounded(interactionStrength, 0.0f, 2.0f, 0.65f),
                                     std::memory_order_relaxed);
  controls.revision.fetch_add(1, std::memory_order_release);
}
