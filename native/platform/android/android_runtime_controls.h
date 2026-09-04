#pragma once

#include "core/base.h"

namespace ae::platform::android {

// Immutable snapshot published by the Android editor overlay. This is a
// platform input contract: renderer and water modules remain unaware of JNI,
// widgets and Activity lifecycle.
struct AndroidRuntimeControls final {
  u64 revision = 0;
  float renderScale = 1.0f;
  u32 shadowQuality = 3;
  bool dynamicResolution = true;
  float bloomIntensity = 0.08f;
  float sharpen = 0.12f;
  float waveHeight = 1.0f;
  float waveSpeed = 1.0f;
  float waveSteepness = 1.0f;
  float microWaves = 1.0f;
  float surfaceOpacity = 0.72f;
  float absorption = 1.0f;
  float foam = 0.65f;
  float interactionStrength = 0.65f;
};

AndroidRuntimeControls runtimeControlsSnapshot() noexcept;

} // namespace ae::platform::android
