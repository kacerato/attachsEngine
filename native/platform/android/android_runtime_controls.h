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
  // Ganho corpo->grade. Independente da onda analítica de toque: um autor
  // pode querer interação tátil forte sem fazer cada casco abrir uma cratera.
  float bodyRippleGain = 0.75f;
  float waterRoughness = 0.22f;
  float waterTurbidity = 0.10f;
  float waterIor = 1.333f;
  float waveDirectionDegrees = 0.0f;
  float foamCompression=.8f, foamGrowth=4, foamDecay=.5f;
  float specularAntialiasing=.5f, contactFoamWidth=1.35f;
  float fluidDensity=1400;
  bool waterPaused=false;
  float swellLength=1, directionalSpread=1, crossSwell=0;
  float waterLevel=0, longWaveAmplitude=0, longWaveLength=320;
};

AndroidRuntimeControls runtimeControlsSnapshot() noexcept;
void publishWaterProviderStatus(u32 status) noexcept;

} // namespace ae::platform::android
