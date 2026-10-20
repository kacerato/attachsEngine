#include "renderer/scene_environment.h"

#include <array>

namespace ae::renderer {
namespace {
float length3(const float value[3]) {
  return std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
}

void transformPoint(const float matrix[16], const float point[3], float out[3]) {
  for (u32 row=0;row<3;++row)
    out[row]=matrix[row]*point[0]+matrix[4+row]*point[1]+matrix[8+row]*point[2]+matrix[12+row];
}

void transformVector(const float matrix[16], const float vector[3], float out[3]) {
  for (u32 row=0;row<3;++row)
    out[row]=matrix[row]*vector[0]+matrix[4+row]*vector[1]+matrix[8+row]*vector[2];
}

float distanceOutside(const SceneEnvironmentVolume &volume,const float camera[3]) {
  if(volume.shape==EnvironmentVolumeShape::Global) return 0;
  float local[3];transformPoint(volume.worldToLocal,camera,local);
  float deltaLocal[3]{};
  if(volume.shape==EnvironmentVolumeShape::Box) {
    for(u32 axis=0;axis<3;++axis) {
      const float half=volume.boxSize[axis]*.5f;
      deltaLocal[axis]=local[axis]-std::clamp(local[axis],-half,half);
    }
  } else {
    const float radius=length3(local);
    if(radius<=volume.sphereRadius) return 0;
    const float scale=1.0f-volume.sphereRadius/radius;
    for(u32 axis=0;axis<3;++axis) deltaLocal[axis]=local[axis]*scale;
  }
  float deltaWorld[3];transformVector(volume.localToWorld,deltaLocal,deltaWorld);
  return length3(deltaWorld);
}

float volumeInfluence(const SceneEnvironmentVolume &volume,const float camera[3]) {
  if(volume.weight<=0) return 0;
  const float distance=distanceOutside(volume,camera);
  if(distance<=0) return volume.weight;
  if(volume.blendDistance<=0 || distance>=volume.blendDistance) return 0;
  return volume.weight*(1.0f-distance/volume.blendDistance);
}

float mix(float from,float to,float amount) {return from+(to-from)*amount;}
void mixColor(float out[3],const float target[3],float amount) {
  for(u32 channel=0;channel<3;++channel) out[channel]=mix(out[channel],target[channel],amount);
}
// A gradação liga a LUT por igualdade exata com o neutro: com peso cheio o
// resultado tem de ser o alvo bit a bit, não from+(to-from).
void mixGrading(ColorGrading &out,const ColorGrading &target,float amount) {
  if(amount>=1.0f) {out=target;return;}
  out.temperature=mix(out.temperature,target.temperature,amount);
  out.tint=mix(out.tint,target.tint,amount);
  mixColor(out.liftColor,target.liftColor,amount);out.lift=mix(out.lift,target.lift,amount);
  mixColor(out.gammaColor,target.gammaColor,amount);out.gamma=mix(out.gamma,target.gamma,amount);
  mixColor(out.gainColor,target.gainColor,amount);out.gain=mix(out.gain,target.gain,amount);
  mixColor(out.shadowsColor,target.shadowsColor,amount);out.shadows=mix(out.shadows,target.shadows,amount);
  mixColor(out.midtonesColor,target.midtonesColor,amount);out.midtones=mix(out.midtones,target.midtones,amount);
  mixColor(out.highlightsColor,target.highlightsColor,amount);
  out.highlights=mix(out.highlights,target.highlights,amount);
  out.shadowsStart=mix(out.shadowsStart,target.shadowsStart,amount);
  out.shadowsEnd=mix(out.shadowsEnd,target.shadowsEnd,amount);
  out.highlightsStart=mix(out.highlightsStart,target.highlightsStart,amount);
  out.highlightsEnd=mix(out.highlightsEnd,target.highlightsEnd,amount);
  out.vibrance=mix(out.vibrance,target.vibrance,amount);
}

// Nuvens entram e saem pela opacidade, como a neblina pela densidade: um volume
// sem nuvens sobre um nublado deixa o céu abrir aos poucos, sem salto de forma.
// Os demais campos mudam junto (a forma de quem chega é a que vale), e o
// interruptor fica ligado enquanto alguém ainda contribui.
void mixClouds(CloudLayer &out,const CloudLayer &target,float amount) {
  if(amount>=1.0f) {out=target;if(!out.enabled) out.opacity=0.0f;return;}
  const float from=out.enabled?out.opacity:0.0f;
  const float to=target.enabled?target.opacity:0.0f;
  if(!out.enabled && target.enabled) {
    // A forma vem inteira de quem chega; só a opacidade cresce.
    const float opacity=out.opacity;out=target;out.opacity=opacity;
  } else if(target.enabled) {
    out.coverage=mix(out.coverage,target.coverage,amount);
    out.cirrus=mix(out.cirrus,target.cirrus,amount);
    out.altitudeMeters=mix(out.altitudeMeters,target.altitudeMeters,amount);
    out.sizeMeters=mix(out.sizeMeters,target.sizeMeters,amount);
    mixColor(out.tint,target.tint,amount);
    out.exposureEv=mix(out.exposureEv,target.exposureEv,amount);
    out.windSpeedKmh=mix(out.windSpeedKmh,target.windSpeedKmh,amount);
    out.density=mix(out.density,target.density,amount);
    out.ambientDimmer=mix(out.ambientDimmer,target.ambientDimmer,amount);
    out.shadowMultiplier=mix(out.shadowMultiplier,target.shadowMultiplier,amount);
    if(amount>=.5f) {out.windOrientationDegrees=target.windOrientationDegrees;out.castShadows=target.castShadows;}
  }
  out.opacity=mix(from,to,amount);
  out.enabled=out.opacity>0.0f;
}

void blend(SceneEnvironment &result,const SceneEnvironment &target,u32 overrides,float amount) {
  if(overrides&EnvironmentOverrideSky) {
    mixColor(result.skyZenith,target.skyZenith,amount);
    mixColor(result.skyHorizon,target.skyHorizon,amount);
    mixColor(result.ground,target.ground,amount);
    result.atmosphere=mix(result.atmosphere,target.atmosphere,amount);
    result.sunDiskDegrees=mix(result.sunDiskDegrees,target.sunDiskDegrees,amount);
    result.sunDiskIntensity=mix(result.sunDiskIntensity,target.sunDiskIntensity,amount);
    result.sunGlare=mix(result.sunGlare,target.sunGlare,amount);
    result.moonPhase=mix(result.moonPhase,target.moonPhase,amount);
    result.moonSizeDegrees=mix(result.moonSizeDegrees,target.moonSizeDegrees,amount);
    result.moonIntensity=mix(result.moonIntensity,target.moonIntensity,amount);
    result.starIntensity=mix(result.starIntensity,target.starIntensity,amount);
    result.physicalSkyIntensity=mix(result.physicalSkyIntensity,target.physicalSkyIntensity,amount);
    result.airDensity=mix(result.airDensity,target.airDensity,amount);
    result.aerosolDensity=mix(result.aerosolDensity,target.aerosolDensity,amount);
    result.aerosolAnisotropy=mix(result.aerosolAnisotropy,target.aerosolAnisotropy,amount);
    result.planetRadiusKm=mix(result.planetRadiusKm,target.planetRadiusKm,amount);
    result.observerHeightKm=mix(result.observerHeightKm,target.observerHeightKm,amount);
    result.rayleighScaleHeightKm=mix(result.rayleighScaleHeightKm,target.rayleighScaleHeightKm,amount);
    result.aerosolScaleHeightKm=mix(result.aerosolScaleHeightKm,target.aerosolScaleHeightKm,amount);
    result.atmosphereHeightKm=mix(result.atmosphereHeightKm,target.atmosphereHeightKm,amount);
    result.groundAlbedo=mix(result.groundAlbedo,target.groundAlbedo,amount);
    if(amount>=.5f) {
      result.sky=target.sky;
      // Resources are selected discretely with the sky group. Never take an
      // HDRI from a volume overriding only fog/post, nor blend radiance using
      // the rotation of a different, nonresident panorama.
      result.environmentMap=target.environmentMap;
      result.hdriRotationDegrees=target.hdriRotationDegrees;
      result.hdriExposureEv=target.hdriExposureEv;
      result.physicalAtmosphereHighQuality=target.physicalAtmosphereHighQuality;
    }
  }
  if(overrides&EnvironmentOverrideFog) {
    mixColor(result.fogColor,target.fogColor,amount);
    result.fogLightEnergy=mix(result.fogLightEnergy,target.fogLightEnergy,amount);
    result.fogDensity=mix(result.fogDensity,target.fog?target.fogDensity:0.0f,amount);
    result.fogStart=mix(result.fogStart,target.fogStart,amount);
    result.fogBaseHeight=mix(result.fogBaseHeight,target.fogBaseHeight,amount);
    result.fogHeightFalloff=mix(result.fogHeightFalloff,target.fogHeightFalloff,amount);
    if(amount>=.5f) result.fog=target.fog;
  }
  if(overrides&EnvironmentOverrideClouds) mixClouds(result.clouds,target.clouds,amount);
  if(overrides&EnvironmentOverrideIndirect) {
    result.indirectDiffuse=mix(result.indirectDiffuse,target.indirectDiffuse,amount);
    result.indirectSpecular=mix(result.indirectSpecular,target.indirectSpecular,amount);
  }
  // Cada override de pós é independente, como no Volume da Unity. Escolhas
  // discretas (modo, curva, chaves) trocam no meio da mistura.
  const bool discrete=amount>=.5f;
  if(overrides&EnvironmentOverridePost) {
    if(discrete) {result.post=target.post;result.toneMapper=target.toneMapper;}
  }
  if(overrides&EnvironmentOverrideExposure) {
    result.fixedExposure=mix(result.fixedExposure,target.fixedExposure,amount);
    result.exposureCompensation=mix(result.exposureCompensation,target.exposureCompensation,amount);
    result.exposureLimitMin=mix(result.exposureLimitMin,target.exposureLimitMin,amount);
    result.exposureLimitMax=mix(result.exposureLimitMax,target.exposureLimitMax,amount);
    result.exposureHistogramLow=mix(result.exposureHistogramLow,target.exposureHistogramLow,amount);
    result.exposureHistogramHigh=mix(result.exposureHistogramHigh,target.exposureHistogramHigh,amount);
    result.exposureTargetMidGrey=mix(result.exposureTargetMidGrey,target.exposureTargetMidGrey,amount);
    result.exposureSpeedDarkToLight=mix(result.exposureSpeedDarkToLight,target.exposureSpeedDarkToLight,amount);
    result.exposureSpeedLightToDark=mix(result.exposureSpeedLightToDark,target.exposureSpeedLightToDark,amount);
    if(discrete) {
      result.exposureMode=target.exposureMode;result.exposureMetering=target.exposureMetering;
      result.exposureAdaptation=target.exposureAdaptation;
    }
  }
  if(overrides&EnvironmentOverrideBloom) {
    result.bloomThreshold=mix(result.bloomThreshold,target.bloomThreshold,amount);
    result.bloomIntensity=mix(result.bloomIntensity,target.bloom?target.bloomIntensity:0.0f,amount);
    if(discrete) result.bloom=target.bloom;
  }
  if(overrides&EnvironmentOverrideVignette) {
    result.vignetteIntensity=mix(result.vignetteIntensity,target.vignette?target.vignetteIntensity:0.0f,amount);
    if(discrete) result.vignette=target.vignette;
  }
  if(overrides&EnvironmentOverrideColorAdjustments) {
    result.contrast=mix(result.contrast,target.contrast,amount);
    result.saturation=mix(result.saturation,target.saturation,amount);
  }
  if(overrides&EnvironmentOverrideColorGrading) mixGrading(result.grading,target.grading,amount);
  if(overrides&EnvironmentOverrideFilmGrain) {
    result.filmGrainIntensity=mix(result.filmGrainIntensity,
        target.filmGrain?target.filmGrainIntensity:0.0f,amount);
    if(discrete) result.filmGrain=target.filmGrain;
  }
  if(overrides&EnvironmentOverrideAmbientOcclusion) {
    result.ambientOcclusionRadius=mix(result.ambientOcclusionRadius,target.ambientOcclusionRadius,amount);
    result.ambientOcclusionIntensity=mix(result.ambientOcclusionIntensity,
        target.ambientOcclusion?target.ambientOcclusionIntensity:0.0f,amount);
    result.ambientOcclusionPower=mix(result.ambientOcclusionPower,target.ambientOcclusionPower,amount);
    result.ambientOcclusionBias=mix(result.ambientOcclusionBias,target.ambientOcclusionBias,amount);
    if(discrete) result.ambientOcclusion=target.ambientOcclusion;
  }
}
} // namespace

SceneEnvironment defaultSceneViewEnvironment() {
  SceneEnvironment result{};
  result.active=true;
  result.post=true;
  result.bloom=true;
  result.vignette=true;
  result.ambientOcclusion=true;
  result.exposureCompensation=-0.15f;
  result.bloomThreshold=1.15f;
  result.bloomIntensity=0.08f;
  result.contrast=1.06f;
  result.saturation=1.04f;
  result.vignetteIntensity=0.08f;
  result.ambientOcclusionRadius=0.85f;
  result.ambientOcclusionIntensity=0.85f;
  result.ambientOcclusionPower=1.35f;
  return result;
}

float sceneEnvironmentVolumeInfluence(const SceneEnvironmentVolume &volume,const float cameraPosition[3]) {
  return volume.valid()?std::clamp(volumeInfluence(volume,cameraPosition),0.0f,1.0f):0.0f;
}

SceneEnvironment resolveSceneEnvironment(std::span<const SceneEnvironmentVolume> volumes,
                                         const float cameraPosition[3],u32 layerMask,
                                         SceneEnvironmentBlendReport *report) {
  if(report) *report={};
  std::vector<const SceneEnvironmentVolume*> ordered;
  ordered.reserve(volumes.size());
  for(const auto &volume:volumes) {
    if(!volume.valid() || !volume.environment.active || !(layerMask&(1u<<volume.layer))) continue;
    ordered.push_back(&volume);if(report) ++report->considered;
  }
  std::stable_sort(ordered.begin(),ordered.end(),[](const auto *a,const auto *b) {
    if(a->environment.priority!=b->environment.priority)
      return a->environment.priority<b->environment.priority;
    // Menor identidade era o desempate do ambiente global v1; aplicá-la por
    // último preserva esse contrato ao migrar para mistura de volumes.
    return a->stableId>b->stableId;
  });
  SceneEnvironment result{};
  bool initialized=false;
  for(const auto *volume:ordered) {
    const float amount=sceneEnvironmentVolumeInfluence(*volume,cameraPosition);
    if(amount<=0) continue;
    if(report) {
      ++report->contributing;
      if(amount>report->dominantInfluence ||
         (amount==report->dominantInfluence && (!report->dominant || volume->stableId<report->dominant))) {
        report->dominant=volume->stableId;report->dominantInfluence=amount;
      }
    }
    if(!initialized) {
      // Sem perfil global, grupos não sobrescritos herdam os padrões do mundo;
      // nunca vazam valores desmarcados só porque este foi o primeiro volume.
      result=SceneEnvironment{};result.active=true;initialized=true;
      blend(result,volume->environment,volume->overrides,amount);
    } else blend(result,volume->environment,volume->overrides,amount);
  }
  result.active=initialized;
  return result;
}
} // namespace ae::renderer
