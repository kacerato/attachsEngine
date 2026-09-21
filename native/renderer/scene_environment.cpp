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

void blend(SceneEnvironment &result,const SceneEnvironment &target,u32 overrides,float amount) {
  if(overrides&EnvironmentOverrideSky) {
    mixColor(result.skyZenith,target.skyZenith,amount);
    mixColor(result.skyHorizon,target.skyHorizon,amount);
    mixColor(result.ground,target.ground,amount);
    result.atmosphere=mix(result.atmosphere,target.atmosphere,amount);
    result.sunDiskDegrees=mix(result.sunDiskDegrees,target.sunDiskDegrees,amount);
    result.sunDiskIntensity=mix(result.sunDiskIntensity,target.sunDiskIntensity,amount);
    if(amount>=.5f) result.sky=target.sky;
  }
  if(overrides&EnvironmentOverrideFog) {
    mixColor(result.fogColor,target.fogColor,amount);
    result.fogDensity=mix(result.fogDensity,target.fog?target.fogDensity:0.0f,amount);
    result.fogStart=mix(result.fogStart,target.fogStart,amount);
    if(amount>=.5f) result.fog=target.fog;
  }
  if(overrides&EnvironmentOverrideIndirect) {
    result.indirectDiffuse=mix(result.indirectDiffuse,target.indirectDiffuse,amount);
    result.indirectSpecular=mix(result.indirectSpecular,target.indirectSpecular,amount);
  }
  if(overrides&EnvironmentOverridePost) {
    result.exposureEv=mix(result.exposureEv,target.exposureEv,amount);
    result.bloomThreshold=mix(result.bloomThreshold,target.bloomThreshold,amount);
    result.bloomIntensity=mix(result.bloomIntensity,target.bloom?target.bloomIntensity:0.0f,amount);
    result.contrast=mix(result.contrast,target.contrast,amount);
    result.saturation=mix(result.saturation,target.saturation,amount);
    result.vignetteIntensity=mix(result.vignetteIntensity,target.vignette?target.vignetteIntensity:0.0f,amount);
    result.filmGrainIntensity=mix(result.filmGrainIntensity,
        target.filmGrain?target.filmGrainIntensity:0.0f,amount);
    result.ambientOcclusionRadius=mix(result.ambientOcclusionRadius,target.ambientOcclusionRadius,amount);
    result.ambientOcclusionIntensity=mix(result.ambientOcclusionIntensity,
        target.ambientOcclusion?target.ambientOcclusionIntensity:0.0f,amount);
    result.ambientOcclusionPower=mix(result.ambientOcclusionPower,target.ambientOcclusionPower,amount);
    result.ambientOcclusionBias=mix(result.ambientOcclusionBias,target.ambientOcclusionBias,amount);
    if(amount>=.5f) {
      result.post=target.post;result.bloom=target.bloom;result.vignette=target.vignette;
      result.filmGrain=target.filmGrain;
      result.ambientOcclusion=target.ambientOcclusion;
      result.toneMapper=target.toneMapper;
    }
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
  result.exposureEv=-0.15f;
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
