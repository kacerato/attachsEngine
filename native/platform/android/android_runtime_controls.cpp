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
  std::atomic<float> microWaves{1.6f};
  std::atomic<float> surfaceOpacity{0.72f};
  std::atomic<float> absorption{1.0f};
  std::atomic<float> foam{1.05f};
  std::atomic<float> interactionStrength{0.65f};
  std::atomic<float> bodyRippleGain{0.75f};
  std::atomic<float> waterRoughness{0.22f};
  std::atomic<float> waterTurbidity{0.10f};
  std::atomic<float> waterIor{1.333f};
  std::atomic<float> waveDirectionDegrees{0.0f};
  std::atomic<float> foamCompression{.8f},foamGrowth{4},foamDecay{.5f};
  std::atomic<float> specularAntialiasing{.5f},contactFoamWidth{1.35f};
  std::atomic<float> foamElevation{.14f},foamCoverage{1.35f};
  std::atomic<float> microDisplacement{.10f},microWavelength{.85f};
  std::atomic<float> wakeStrength{1.2f},wakeMinimumSpeed{.1f},wakeSpacing{2.0f};
  std::atomic<float> wakeWidthScale{.22f},wakeMaximumImpulse{1.2f};
  std::atomic<float> solidLodError{0},foliageLodError{0},lodTransition{0};
  std::atomic<float> fluidDensity{1400};
  std::atomic<bool> waterPaused{false};
  std::atomic<float> swellLength{1}, directionalSpread{1}, crossSwell{0};
  std::atomic<float> waterLevel{0}, longWaveAmplitude{0}, longWaveLength{320};
  std::atomic<float> spectralWindSpeed{10}, spectralFetch{100000}, spectralDepth{20};
  std::atomic<float> spectralSwell{.8f}, spectralSpread{.2f}, spectralDamping{.1f};
  std::atomic<float> crossWindSpeed{0}, crossDirectionDegrees{65}, crossFetch{100000};
  std::atomic<float> crossSwellShape{1}, crossSpread{.1f}, crossWeight{.35f};
  std::array<std::atomic<float>,3> cascadeDisplacement{{1,1,1}};
  std::array<std::atomic<float>,3> cascadeChoppiness{{1,1,1}};
} controls;
std::atomic<ae::u32> waterProviderStatus{0};

float bounded(float value, float minimum, float maximum, float fallback) {
  return value >= minimum && value <= maximum ? value : fallback;
}
}

namespace ae::platform::android {
void publishWaterProviderStatus(u32 status) noexcept {
  waterProviderStatus.store(status,std::memory_order_release);
}

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
    snapshot.solidLodError=controls.solidLodError.load(std::memory_order_relaxed);
    snapshot.foliageLodError=controls.foliageLodError.load(std::memory_order_relaxed);
    snapshot.lodTransition=controls.lodTransition.load(std::memory_order_relaxed);
    snapshot.waveHeight = controls.waveHeight.load(std::memory_order_relaxed);
    snapshot.waveSpeed = controls.waveSpeed.load(std::memory_order_relaxed);
    snapshot.waveSteepness = controls.waveSteepness.load(std::memory_order_relaxed);
    snapshot.microWaves = controls.microWaves.load(std::memory_order_relaxed);
    snapshot.surfaceOpacity = controls.surfaceOpacity.load(std::memory_order_relaxed);
    snapshot.absorption = controls.absorption.load(std::memory_order_relaxed);
    snapshot.foam = controls.foam.load(std::memory_order_relaxed);
    snapshot.interactionStrength = controls.interactionStrength.load(std::memory_order_relaxed);
    snapshot.bodyRippleGain = controls.bodyRippleGain.load(std::memory_order_relaxed);
    snapshot.waterRoughness = controls.waterRoughness.load(std::memory_order_relaxed);
    snapshot.waterTurbidity = controls.waterTurbidity.load(std::memory_order_relaxed);
    snapshot.waterIor = controls.waterIor.load(std::memory_order_relaxed);
    snapshot.waveDirectionDegrees = controls.waveDirectionDegrees.load(std::memory_order_relaxed);
    snapshot.foamCompression=controls.foamCompression.load(std::memory_order_relaxed);
    snapshot.foamGrowth=controls.foamGrowth.load(std::memory_order_relaxed);
    snapshot.foamDecay=controls.foamDecay.load(std::memory_order_relaxed);
    snapshot.specularAntialiasing=controls.specularAntialiasing.load(std::memory_order_relaxed);
    snapshot.contactFoamWidth=controls.contactFoamWidth.load(std::memory_order_relaxed);
    snapshot.foamElevation=controls.foamElevation.load(std::memory_order_relaxed);
    snapshot.foamCoverage=controls.foamCoverage.load(std::memory_order_relaxed);
    snapshot.microDisplacement=controls.microDisplacement.load(std::memory_order_relaxed);
    snapshot.microWavelength=controls.microWavelength.load(std::memory_order_relaxed);
    snapshot.wakeStrength=controls.wakeStrength.load(std::memory_order_relaxed);
    snapshot.wakeMinimumSpeed=controls.wakeMinimumSpeed.load(std::memory_order_relaxed);
    snapshot.wakeSpacing=controls.wakeSpacing.load(std::memory_order_relaxed);
    snapshot.wakeWidthScale=controls.wakeWidthScale.load(std::memory_order_relaxed);
    snapshot.wakeMaximumImpulse=controls.wakeMaximumImpulse.load(std::memory_order_relaxed);
    snapshot.fluidDensity=controls.fluidDensity.load(std::memory_order_relaxed);
    snapshot.waterPaused=controls.waterPaused.load(std::memory_order_relaxed);
    snapshot.swellLength=controls.swellLength.load(std::memory_order_relaxed);
    snapshot.directionalSpread=controls.directionalSpread.load(std::memory_order_relaxed);
    snapshot.crossSwellShape=controls.crossSwellShape.load(std::memory_order_relaxed);
    snapshot.waterLevel=controls.waterLevel.load(std::memory_order_relaxed);
    snapshot.longWaveAmplitude=controls.longWaveAmplitude.load(std::memory_order_relaxed);
    snapshot.longWaveLength=controls.longWaveLength.load(std::memory_order_relaxed);
    snapshot.spectralWindSpeed=controls.spectralWindSpeed.load(std::memory_order_relaxed);
    snapshot.spectralFetch=controls.spectralFetch.load(std::memory_order_relaxed);
    snapshot.spectralDepth=controls.spectralDepth.load(std::memory_order_relaxed);
    snapshot.spectralSwell=controls.spectralSwell.load(std::memory_order_relaxed);
    snapshot.spectralSpread=controls.spectralSpread.load(std::memory_order_relaxed);
    snapshot.spectralDamping=controls.spectralDamping.load(std::memory_order_relaxed);
    snapshot.crossWindSpeed=controls.crossWindSpeed.load(std::memory_order_relaxed);
    snapshot.crossDirectionDegrees=controls.crossDirectionDegrees.load(std::memory_order_relaxed);
    snapshot.crossFetch=controls.crossFetch.load(std::memory_order_relaxed);
    snapshot.crossSwell=controls.crossSwell.load(std::memory_order_relaxed);
    snapshot.crossSpread=controls.crossSpread.load(std::memory_order_relaxed);
    snapshot.crossWeight=controls.crossWeight.load(std::memory_order_relaxed);
    for(ae::usize index=0;index<3;++index) {
      snapshot.cascadeDisplacement[index]=controls.cascadeDisplacement[index].load(std::memory_order_relaxed);
      snapshot.cascadeChoppiness[index]=controls.cascadeChoppiness[index].load(std::memory_order_relaxed);
    }
    const u64 after = controls.revision.load(std::memory_order_acquire);
    if (before == after) { snapshot.revision = after; return snapshot; }
  }
}

} // namespace ae::platform::android

extern "C" JNIEXPORT jint JNICALL
Java_dev_aether_editor_AetherActivity_nativeWaterProviderStatus(JNIEnv *,jclass) {
  return static_cast<jint>(waterProviderStatus.load(std::memory_order_acquire));
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_AetherActivity_nativeApplyVisibilityControls(
    JNIEnv *,jclass,jfloat solid,jfloat foliage,jfloat transition) {
  controls.revision.fetch_add(1,std::memory_order_acq_rel);
  controls.solidLodError.store(bounded(solid,0,16,0),std::memory_order_relaxed);
  controls.foliageLodError.store(bounded(foliage,0,128,0),std::memory_order_relaxed);
  controls.lodTransition.store(bounded(transition,0,.99f,0),std::memory_order_relaxed);
  controls.revision.fetch_add(1,std::memory_order_release);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_aether_editor_AetherActivity_nativeApplyControls(
    JNIEnv *, jclass, jfloat renderScale, jint shadowQuality, jboolean dynamicResolution,
    jfloat bloomIntensity, jfloat sharpen, jfloat waveHeight, jfloat waveSpeed,
    jfloat waveSteepness, jfloat microWaves, jfloat surfaceOpacity, jfloat absorption,
    jfloat foam, jfloat interactionStrength, jfloat bodyRippleGain, jfloat waterRoughness,
    jfloat waterTurbidity, jfloat waterIor, jfloat waveDirectionDegrees,
    jfloat foamCompression,jfloat foamGrowth,jfloat foamDecay,
    jfloat specularAntialiasing,jfloat contactFoamWidth,
    jfloat foamElevation,jfloat foamCoverage,jfloat microDisplacement,jfloat microWavelength,
    jfloat wakeStrength,jfloat wakeMinimumSpeed,jfloat wakeSpacing,jfloat wakeWidthScale,
    jfloat wakeMaximumImpulse,
    jfloat fluidDensity,jboolean waterPaused,
    jfloat swellLength,jfloat directionalSpread,jfloat crossSwell,
    jfloat waterLevel,jfloat longWaveAmplitude,jfloat longWaveLength,
    jfloat spectralWindSpeed,jfloat spectralFetchKm,jfloat spectralDepth,
    jfloat spectralSwell,jfloat spectralSpread,jfloat spectralDamping,
    jfloat crossWindSpeed,jfloat crossDirectionDegrees,jfloat crossFetchKm,
    jfloat spectralCrossSwell,jfloat crossSpread,jfloat crossWeight,
    jfloat cascade0Displacement,jfloat cascade1Displacement,jfloat cascade2Displacement,
    jfloat cascade0Choppiness,jfloat cascade1Choppiness,jfloat cascade2Choppiness) {
  controls.revision.fetch_add(1, std::memory_order_acq_rel);
  controls.renderScale.store(bounded(renderScale, 0.5f, 1.0f, 1.0f), std::memory_order_relaxed);
  controls.shadowQuality.store(static_cast<ae::u32>(std::clamp(shadowQuality, 0, 3)),
                               std::memory_order_relaxed);
  controls.dynamicResolution.store(dynamicResolution == JNI_TRUE, std::memory_order_relaxed);
  controls.bloomIntensity.store(bounded(bloomIntensity, 0.0f, 1.5f, 0.08f),
                                std::memory_order_relaxed);
  controls.sharpen.store(bounded(sharpen, 0.0f, 1.0f, 0.12f), std::memory_order_relaxed);
  controls.waveHeight.store(bounded(waveHeight, 0.0f, 12.0f, 1.0f), std::memory_order_relaxed);
  controls.waveSpeed.store(bounded(waveSpeed, 0.0f, 3.0f, 1.0f), std::memory_order_relaxed);
  controls.waveSteepness.store(bounded(waveSteepness, 0.0f, 2.0f, 1.0f),
                               std::memory_order_relaxed);
  controls.microWaves.store(bounded(microWaves, 0.0f, 4.0f, 1.6f), std::memory_order_relaxed);
  controls.surfaceOpacity.store(bounded(surfaceOpacity, 0.05f, 1.0f, 0.72f),
                                std::memory_order_relaxed);
  controls.absorption.store(bounded(absorption, 0.1f, 4.0f, 1.0f), std::memory_order_relaxed);
  controls.foam.store(bounded(foam, 0.0f, 3.0f, 1.05f), std::memory_order_relaxed);
  controls.interactionStrength.store(bounded(interactionStrength, 0.0f, 2.0f, 0.65f),
                                     std::memory_order_relaxed);
  controls.bodyRippleGain.store(bounded(bodyRippleGain, 0.0f, 4.0f, 0.75f),
                                std::memory_order_relaxed);
  controls.waterRoughness.store(bounded(waterRoughness, 0.025f, 1.0f, 0.22f), std::memory_order_relaxed);
  controls.waterTurbidity.store(bounded(waterTurbidity, 0.0f, 1.0f, 0.10f), std::memory_order_relaxed);
  controls.waterIor.store(bounded(waterIor, 1.0f, 2.0f, 1.333f), std::memory_order_relaxed);
  controls.waveDirectionDegrees.store(bounded(waveDirectionDegrees, -180.0f, 180.0f, 0.0f), std::memory_order_relaxed);
  controls.foamCompression.store(bounded(foamCompression,0,2,.8f),std::memory_order_relaxed);
  controls.foamGrowth.store(bounded(foamGrowth,0,100,4),std::memory_order_relaxed);
  controls.foamDecay.store(bounded(foamDecay,0,100,.5f),std::memory_order_relaxed);
  controls.specularAntialiasing.store(bounded(specularAntialiasing,0,1,.5f),std::memory_order_relaxed);
  controls.contactFoamWidth.store(bounded(contactFoamWidth,0,10,1.35f),std::memory_order_relaxed);
  controls.foamElevation.store(bounded(foamElevation,0,1,.14f),std::memory_order_relaxed);
  controls.foamCoverage.store(bounded(foamCoverage,0,4,1.35f),std::memory_order_relaxed);
  controls.microDisplacement.store(bounded(microDisplacement,0,1,.10f),std::memory_order_relaxed);
  controls.microWavelength.store(bounded(microWavelength,.2f,8,.85f),std::memory_order_relaxed);
  controls.wakeStrength.store(bounded(wakeStrength,0,8,1.2f),std::memory_order_relaxed);
  controls.wakeMinimumSpeed.store(bounded(wakeMinimumSpeed,0,10,.1f),std::memory_order_relaxed);
  controls.wakeSpacing.store(bounded(wakeSpacing,.5f,100,2),std::memory_order_relaxed);
  controls.wakeWidthScale.store(bounded(wakeWidthScale,.05f,2,.22f),std::memory_order_relaxed);
  controls.wakeMaximumImpulse.store(bounded(wakeMaximumImpulse,.05f,5,1.2f),std::memory_order_relaxed);
  controls.fluidDensity.store(bounded(fluidDensity,500,2000,1400),std::memory_order_relaxed);
  controls.waterPaused.store(waterPaused==JNI_TRUE,std::memory_order_relaxed);
  controls.swellLength.store(bounded(swellLength,.5f,4,1),std::memory_order_relaxed);
  controls.directionalSpread.store(bounded(directionalSpread,0,2,1),std::memory_order_relaxed);
  controls.crossSwell.store(bounded(crossSwell,0,2,0),std::memory_order_relaxed);
  controls.waterLevel.store(bounded(waterLevel,-20,40,0),std::memory_order_relaxed);
  controls.longWaveAmplitude.store(bounded(longWaveAmplitude,0,20,0),std::memory_order_relaxed);
  controls.longWaveLength.store(bounded(longWaveLength,120,2000,320),std::memory_order_relaxed);
  controls.spectralWindSpeed.store(bounded(spectralWindSpeed,0,40,10),std::memory_order_relaxed);
  controls.spectralFetch.store(bounded(spectralFetchKm,.1f,2000,100)*1000,std::memory_order_relaxed);
  controls.spectralDepth.store(bounded(spectralDepth,1,500,20),std::memory_order_relaxed);
  controls.spectralSwell.store(bounded(spectralSwell,0,2,.8f),std::memory_order_relaxed);
  controls.spectralSpread.store(bounded(spectralSpread,0,1,.2f),std::memory_order_relaxed);
  controls.spectralDamping.store(bounded(spectralDamping,0,2,.1f),std::memory_order_relaxed);
  controls.crossWindSpeed.store(bounded(crossWindSpeed,0,40,0),std::memory_order_relaxed);
  controls.crossDirectionDegrees.store(bounded(crossDirectionDegrees,-180,180,65),std::memory_order_relaxed);
  controls.crossFetch.store(bounded(crossFetchKm,.1f,2000,100)*1000,std::memory_order_relaxed);
  controls.crossSwellShape.store(bounded(spectralCrossSwell,0,2,1),std::memory_order_relaxed);
  controls.crossSpread.store(bounded(crossSpread,0,1,.1f),std::memory_order_relaxed);
  controls.crossWeight.store(bounded(crossWeight,0,1,.35f),std::memory_order_relaxed);
  const float displacement[]={cascade0Displacement,cascade1Displacement,cascade2Displacement};
  const float choppiness[]={cascade0Choppiness,cascade1Choppiness,cascade2Choppiness};
  for(ae::usize index=0;index<3;++index) {
    controls.cascadeDisplacement[index].store(bounded(displacement[index],0,3,1),std::memory_order_relaxed);
    controls.cascadeChoppiness[index].store(bounded(choppiness[index],0,4,1),std::memory_order_relaxed);
  }
  controls.revision.fetch_add(1, std::memory_order_release);
}
