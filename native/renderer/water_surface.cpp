#include "renderer/water_surface.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
constexpr float Pi = 3.14159265358979323846f;
bool finite(float value) { return std::isfinite(value); }
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
WaterVec3 normalized(WaterVec3 v) {
  const float length = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
  return length > 1.0e-8f ? WaterVec3{v.x/length,v.y/length,v.z/length}
                          : WaterVec3{0.0f,1.0f,0.0f};
}
}

WaterProfile defaultOceanWaterProfile() noexcept {
  WaterProfile profile{};
  profile.waveCount = 5;
  // This fallback targets the 3 m validation grid. Every wavelength retains
  // at least five vertices per cycle, avoiding sub-Nyquist vertex shimmer.
  // Short capillary detail belongs in a filtered normal spectrum, not geometry.
  profile.waves[0] = {{0.9701425f, 0.2425356f}, 0.42f, 46.0f, 0.82f, 0.42f, 0.0f};
  profile.waves[1] = {{0.4472136f, 0.8944272f}, 0.24f, 31.0f, 1.05f, 0.36f, 1.1f};
  profile.waves[2] = {{-0.8320503f, 0.5547002f}, 0.14f, 22.0f, 1.32f, 0.28f, 2.4f};
  profile.waves[3] = {{0.1961161f, -0.9805807f}, 0.075f, 17.0f, 1.55f, 0.18f, 0.7f};
  profile.waves[4] = {{-0.7071068f, -0.7071068f}, 0.045f, 15.0f, 1.78f, 0.12f, 1.8f};
  return profile;
}

WaterValidationError validateWaterProfile(const WaterProfile &p) noexcept {
  if (p.schemaVersion != WaterProfileVersion) return WaterValidationError::Schema;
  if (static_cast<u32>(p.domain) > static_cast<u32>(WaterDomain::RiverSpline))
    return WaterValidationError::Domain;
  if (static_cast<u32>(p.reflection) > static_cast<u32>(WaterReflection::Planar))
    return WaterValidationError::Reflection;
  if (p.waveCount > MaximumWaterWaves) return WaterValidationError::WaveCount;
  for (u32 i=0;i<p.waveCount;++i) {
    const auto &w=p.waves[i];
    const float directionLength=w.direction.x*w.direction.x+w.direction.y*w.direction.y;
    if (!finite(directionLength) || directionLength < 0.999f || directionLength > 1.001f ||
        !finite(w.amplitude) || w.amplitude < 0.0f || w.amplitude > 20.0f ||
        !finite(w.wavelength) || w.wavelength < 0.05f || w.wavelength > 10000.0f ||
        !finite(w.speed) || std::abs(w.speed) > 100.0f || !finite(w.steepness) ||
        w.steepness < 0.0f || w.steepness > 1.0f || !finite(w.phase))
      return WaterValidationError::Wave;
  }
  const float optical[]={p.deepColor.x,p.deepColor.y,p.deepColor.z,p.shallowColor.x,
      p.shallowColor.y,p.shallowColor.z,p.absorption.x,p.absorption.y,p.absorption.z,
      p.refractiveIndex,p.roughness,p.turbidity,p.foamThreshold,p.foamDecay};
  for(float value:optical) if(!finite(value) || value<0.0f) return WaterValidationError::Optical;
  if(p.refractiveIndex<1.0f || p.refractiveIndex>2.0f || p.roughness>1.0f ||
     p.turbidity>1.0f || p.foamThreshold>1.0f || p.foamDecay>20.0f ||
     !finite(p.surfaceOpacity) || p.surfaceOpacity < 0.0f || p.surfaceOpacity > 1.0f ||
     !finite(p.microWaveStrength) || p.microWaveStrength < 0.0f ||
     p.microWaveStrength > 4.0f)
    return WaterValidationError::Optical;
  if(!finite(p.maximumDistance) || p.maximumDistance<=0.0f) return WaterValidationError::Distance;
  if(!finite(p.basePatchSize) || p.basePatchSize<1.0f || p.basePatchSize>1024.0f ||
     p.clipmapLevels<1 || p.clipmapLevels>MaximumWaterClipmapLevels)
    return WaterValidationError::Clipmap;
  return WaterValidationError::None;
}

WaterSample sampleWaterSurface(const WaterProfile &p, WaterVec2 position,
                               float timeSeconds) noexcept {
  WaterSample result{};
  if(validateWaterProfile(p)!=WaterValidationError::None || !finite(position.x) ||
     !finite(position.y) || !finite(timeSeconds)) return result;
  float dx=0.0f,dz=0.0f,dyDt=0.0f,breaking=0.0f,orbitalX=0.0f,orbitalZ=0.0f;
  for(u32 i=0;i<p.waveCount;++i) {
    const auto &wave=p.waves[i];
    const float k=2.0f*Pi/wave.wavelength;
    const float angle=k*(wave.direction.x*position.x+wave.direction.y*position.y)-
                      wave.speed*timeSeconds+wave.phase;
    const float sine=std::sin(angle),cosine=std::cos(angle);
    // A bounded second harmonic sharpens crests without horizontal folding.
    // The same function and derivative are evaluated in the vertex shader.
    const float crest = 0.25f * wave.steepness;
    const float normalization = 1.0f + crest;
    const float shape = (sine + crest * (2.0f*sine*sine-1.0f)) / normalization;
    const float derivative = cosine * (1.0f+4.0f*crest*sine) / normalization;
    const float elevation=wave.amplitude*shape;
    result.height+=elevation;
    const float slope=wave.amplitude*k*derivative;
    dx+=slope*wave.direction.x; dz+=slope*wave.direction.y;
    dyDt-=wave.amplitude*wave.speed*derivative;
    // Linear deep-water theory puts horizontal orbital velocity in phase with
    // the elevation of the same component: u = omega * eta along the direction.
    // Drag on a floating body needs this; using only the vertical component
    // leaves a hull motionless in a swell that should be pushing it.
    orbitalX+=wave.speed*elevation*wave.direction.x;
    orbitalZ+=wave.speed*elevation*wave.direction.y;
    breaking+=std::abs(wave.amplitude*k*wave.steepness*sine);
  }
  result.normal=normalized({-dx,1.0f,-dz});
  result.velocity={orbitalX,dyDt,orbitalZ};
  result.breaking=clamp01(breaking);
  return result;
}

float maximumWaterDisplacement(const WaterProfile &p) noexcept {
  if (validateWaterProfile(p) != WaterValidationError::None) return 0.0f;
  float extent = 0.0f;
  for (u32 index = 0; index < p.waveCount; ++index) extent += p.waves[index].amplitude;
  return extent;
}

bool WaterInteractionField::addImpulse(const WaterImpulse &impulse) noexcept {
  const float values[]{impulse.center.x, impulse.center.y, impulse.startTime,
      impulse.amplitude, impulse.wavelength, impulse.speed, impulse.decay, impulse.duration};
  for (float value : values) if (!finite(value)) return false;
  if (impulse.amplitude == 0.0f || impulse.wavelength < 0.1f || impulse.wavelength > 100.0f ||
      impulse.speed <= 0.0f || impulse.speed > 100.0f || impulse.decay < 0.0f ||
      impulse.decay > 20.0f || impulse.duration <= 0.0f || impulse.duration > 60.0f) return false;
  impulses_[next_] = impulse;
  next_ = (next_ + 1) % MaximumWaterInteractions;
  return true;
}

WaterSample WaterInteractionField::sample(WaterVec2 position, float timeSeconds) const noexcept {
  WaterSample result{};
  if (!finite(position.x) || !finite(position.y) || !finite(timeSeconds)) return result;
  float slopeX = 0.0f, slopeZ = 0.0f, breaking = 0.0f;
  for (const WaterImpulse &impulse : impulses_) {
    const float age = timeSeconds - impulse.startTime;
    if (impulse.amplitude == 0.0f || age < 0.0f || age > impulse.duration) continue;
    const float dx = position.x - impulse.center.x;
    const float dz = position.y - impulse.center.y;
    const float distance = std::sqrt(dx * dx + dz * dz);
    const float waveNumber = 2.0f * Pi / impulse.wavelength;
    const float front = impulse.speed * age;
    const float radial = distance - front;
    const float width = std::max(0.35f, impulse.wavelength * 0.55f);
    const float gaussian = std::exp(-(radial * radial) / (width * width));
    const float temporal = std::exp(-impulse.decay * age);
    const float angle = waveNumber * radial;
    const float envelope = impulse.amplitude * gaussian * temporal;
    result.height += envelope * std::sin(angle);
    if (distance > 1.0e-5f) {
      const float derivative = envelope *
          (waveNumber * std::cos(angle) - 2.0f * radial / (width * width) * std::sin(angle));
      slopeX += derivative * dx / distance;
      slopeZ += derivative * dz / distance;
      breaking += std::abs(derivative) * 0.25f;
    }
  }
  result.normal = normalized({-slopeX, 1.0f, -slopeZ});
  result.breaking = clamp01(breaking);
  return result;
}

u32 WaterInteractionField::activeCount(float timeSeconds) const noexcept {
  if (!finite(timeSeconds)) return 0;
  u32 count = 0;
  for (const WaterImpulse &impulse : impulses_) {
    const float age = timeSeconds - impulse.startTime;
    if (impulse.amplitude != 0.0f && age >= 0.0f && age <= impulse.duration) ++count;
  }
  return count;
}

float waterCoverage(const WaterExclusionVolume &v, WaterVec2 p) noexcept {
  if(!finite(p.x)||!finite(p.y)||!finite(v.center.x)||!finite(v.center.y)||
     !finite(v.halfExtent.x)||!finite(v.halfExtent.y)||v.halfExtent.x<=0.0f||
     v.halfExtent.y<=0.0f||!finite(v.rotationRadians)||!finite(v.feather)||v.feather<0.0f)
    return 1.0f;
  const float c=std::cos(v.rotationRadians),s=std::sin(v.rotationRadians);
  const float px=p.x-v.center.x,pz=p.y-v.center.y;
  const float x=std::abs(c*px+s*pz),z=std::abs(-s*px+c*pz);
  float signedDistance=0.0f;
  if(v.shape==WaterExclusionShape::Circle)
    signedDistance=std::sqrt(x*x+z*z)-v.halfExtent.x;
  else if(v.shape==WaterExclusionShape::Box) {
    const float qx=x-v.halfExtent.x,qz=z-v.halfExtent.y;
    signedDistance=std::sqrt(std::max(qx,0.0f)*std::max(qx,0.0f)+
                             std::max(qz,0.0f)*std::max(qz,0.0f))+
                   std::min(std::max(qx,qz),0.0f);
  } else return 1.0f;
  if(v.feather<=0.0f) return signedDistance>=0.0f?1.0f:0.0f;
  const float t=clamp01(signedDistance/v.feather);
  return t*t*(3.0f-2.0f*t);
}

bool planWaterClipmap(const WaterProfile &p, WaterVec2 camera,
                      WaterPatchPlan &plan) noexcept {
  plan={};
  if(validateWaterProfile(p)!=WaterValidationError::None || !finite(camera.x)||!finite(camera.y))
    return false;
  for(u32 level=0;level<p.clipmapLevels;++level) {
    const float size=p.basePatchSize*static_cast<float>(1u<<level);
    const float centerX=std::floor(camera.x/size)*size;
    const float centerZ=std::floor(camera.y/size)*size;
    for(int z=-2;z<2;++z) for(int x=-2;x<2;++x) {
      if(level>0 && x>=-1 && x<=0 && z>=-1 && z<=0) continue;
      if(level==0 && !(x>=-1 && x<=0 && z>=-1 && z<=0)) continue;
      if(plan.count>=plan.patches.size()) return false;
      const float originX=centerX+static_cast<float>(x)*size;
      const float originZ=centerZ+static_cast<float>(z)*size;
      const float farthest=std::hypot(originX-camera.x,originZ-camera.y);
      if(farthest>p.maximumDistance+size*2.0f) continue;
      plan.patches[plan.count++]={{originX,originZ},size,
                                  std::max(0.25f,size*0.03f),level};
    }
  }
  return plan.count!=0;
}

ResolvedWaterPipeline resolveWaterPipeline(const WaterProfile &p,
                                           const WaterCapabilities &c) noexcept {
  ResolvedWaterPipeline result{};
  result.reflection=p.reflection;
  if(p.reflection==WaterReflection::ScreenSpace && (!c.sampledSceneColor||!c.sampledSceneDepth)) {
    result.reflection=WaterReflection::Environment; result.usedFallback=true;
  }
  if(p.reflection==WaterReflection::Planar && !c.sampledSceneColor) {
    result.reflection=WaterReflection::Environment; result.usedFallback=true;
  }
  result.refraction=c.sampledSceneColor&&c.sampledSceneDepth;
  result.spectralSimulation=c.computeShaders&&c.halfFloatStorage;
  result.tessellation=c.tessellation;
  return result;
}

} // namespace ae::renderer
