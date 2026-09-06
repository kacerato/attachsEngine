#pragma once

#include "core/base.h"

#include <array>

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
  // Zero inherits the project policy; display-pixel budgets are independent
  // from resolution scale and do not shorten world visibility.
  float solidLodError = 0, foliageLodError = 0, lodTransition = 0;
  float waveHeight = 1.0f;
  float waveSpeed = 1.0f;
  float waveSteepness = 1.0f;
  float microWaves = 1.6f;
  float surfaceOpacity = 0.72f;
  float absorption = 1.0f;
  float foam = 1.05f;
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
  float foamElevation=.14f, foamCoverage=1.35f;
  float microDisplacement=.10f, microWavelength=.85f;
  float wakeStrength=1.2f, wakeMinimumSpeed=.1f, wakeSpacing=2.0f;
  float wakeWidthScale=.22f, wakeMaximumImpulse=1.2f;
  float fluidDensity=1400;
  bool waterPaused=false;
  float swellLength=1, directionalSpread=1, crossSwell=0;
  float waterLevel=0, longWaveAmplitude=0, longWaveLength=320;
  // Espectro TMA/JONSWAP. Fetch usa metros no contrato nativo; a UI mostra km.
  float spectralWindSpeed=10, spectralFetch=100000, spectralDepth=20;
  float spectralSwell=.8f, spectralSpread=.2f, spectralDamping=.1f;
  float crossWindSpeed=0, crossDirectionDegrees=65, crossFetch=100000;
  float crossSwellShape=1, crossSpread=.1f, crossWeight=.35f;
  std::array<float,3> cascadeDisplacement{1,1,1};
  std::array<float,3> cascadeChoppiness{1,1,1};
};

AndroidRuntimeControls runtimeControlsSnapshot() noexcept;
void publishWaterProviderStatus(u32 status) noexcept;

} // namespace ae::platform::android
