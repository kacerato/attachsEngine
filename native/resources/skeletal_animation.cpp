#include "resources/skeletal_animation.h"
#include "core/rotation_math.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <limits>
#include <unordered_set>

namespace ae::resources {
namespace {

bool invert(const float m[16], float out[16]) {
  float inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const float determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-20f) return false;
  for (u32 i = 0; i < 16; ++i) out[i] = inv[i] / determinant;
  return true;
}

void multiply(const float a[16], const float b[16], float out[16]) {
  float result[16];
  for (u32 column = 0; column < 4; ++column)
    for (u32 row = 0; row < 4; ++row) {
      float sum = 0;
      for (u32 k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[column * 4 + k];
      result[column * 4 + row] = sum;
    }
  std::copy(result, result + 16, out);
}

} // namespace

float wrapAnimationTime(float time, float duration, AnimationWrapMode mode, bool &finished) {
  finished = false;
  if (!std::isfinite(time)) time = 0;
  if (!(duration > 0)) {
    finished = mode == AnimationWrapMode::Once;
    return 0;
  }
  switch (mode) {
    case AnimationWrapMode::Loop: {
      float local = std::fmod(time, duration);
      return local < 0 ? local + duration : local;
    }
    case AnimationWrapMode::PingPong: {
      float local = std::fmod(std::fabs(time), 2 * duration);
      return local > duration ? 2 * duration - local : local;
    }
    case AnimationWrapMode::ClampForever: return std::clamp(time, 0.0f, duration);
    case AnimationWrapMode::Once:
    default:
      if (time >= duration) {
        finished = true;
        return duration;
      }
      return std::max(time, 0.0f);
  }
}

bool validAnimationChannel(const AnimationChannel &channel) {
  if(static_cast<u8>(channel.path)>3||static_cast<u8>(channel.interpolation)>2)return false;
  if(static_cast<u8>(channel.rotationMode)>2||(channel.path!=AnimationPath::Rotation&&channel.rotationMode!=AnimationRotationMode::Quaternion))return false;
  if(channel.path==AnimationPath::Weights?(!channel.weightCount||channel.weightCount>MaximumMorphTargets):channel.weightCount!=0)return false;
  if(!channel.layerSources.empty()) {
    if(channel.layerSources.size()>32||!channel.curves.empty()||!channel.times.empty()||!channel.values.empty()||channel.rotationMode!=AnimationRotationMode::Quaternion)return false;
    for(const auto &source:channel.layerSources)
      if(!source.layerSources.empty()||source.node!=channel.node||source.path!=channel.path||source.weightCount!=channel.weightCount||
         !std::isfinite(source.layerWeight)||source.layerWeight<0||source.layerWeight>1||!std::isfinite(source.layerReferenceTime)||
         (source.layerReferenceTime<0&&source.layerReferenceTime!=-1)||!validAnimationChannel(source))return false;
    return !channel.layerSources.front().layerAdditive;
  }
  if(!channel.curves.empty()) {
    if(!channel.times.empty()||!channel.values.empty()||channel.curves.size()!=channel.curveComponents())return false;
    for(const auto &curve:channel.curves)if(!validAnimationCurve(curve))return false;
    if(channel.path==AnimationPath::Rotation&&channel.rotationMode!=AnimationRotationMode::Euler) {
      const auto &keys=channel.curves[0].keys;
      for(u32 c=1;c<4;++c) {
        if(channel.curves[c].keys.size()!=keys.size())return false;
        for(usize i=0;i<keys.size();++i)if(channel.curves[c].keys[i].time!=keys[i].time)return false;
      }
      double cumulative=0;float previous[4]{};
      if(channel.rotationMode==AnimationRotationMode::ProgressiveQuaternion&&channel.curves[4].keys.size()!=keys.size())return false;
      for(usize i=0;i<keys.size();++i) {
        float q[4];for(u32 c=0;c<4;++c)q[c]=channel.curves[c].keys[i].value;
        if(!normalizeRotationQuaternion(q))return false;
        if(channel.rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
          for(u32 c=0;c<4;++c) {
            const auto &key=channel.curves[c].keys[i];
            if(key.incoming!=AnimationTangentMode::Linear||key.outgoing!=AnimationTangentMode::Linear||key.broken||
               key.weightedIn||key.weightedOut||key.inSlope!=0||key.outSlope!=0)return false;
          }
          double travel=0;if(i) {
            float authored[4];for(u32 c=0;c<4;++c)authored[c]=channel.curves[c].keys[i].value;
            travel=rotationDistanceDegrees(previous,authored);cumulative+=travel;
          }
          const auto &progress=channel.curves[4].keys[i];
          if(progress.time!=keys[i].time||std::abs(progress.value-cumulative)>std::max(.00001,cumulative*1e-6)||
             (i&&progress.value<channel.curves[4].keys[i-1].value))return false;
          if(i&&travel>0&&progress.value==channel.curves[4].keys[i-1].value)return false;
        }
        for(u32 c=0;c<4;++c)previous[c]=channel.curves[c].keys[i].value;
      }
    }
    return true;
  }
  if(channel.rotationMode!=AnimationRotationMode::Quaternion)return false;
  const usize keys = channel.times.size();
  if (!keys) return false;
  if (channel.path == AnimationPath::Weights && (!channel.weightCount || channel.weightCount > MaximumMorphTargets))
    return false;
  if (channel.path != AnimationPath::Weights && channel.weightCount) return false;
  for (usize i = 0; i < keys; ++i) {
    if (!std::isfinite(channel.times[i]) || channel.times[i] < 0) return false;
    if (i && channel.times[i] < channel.times[i - 1]) return false;
  }
  const usize perKey = channel.components() * (channel.interpolation == AnimationInterpolation::CubicSpline ? 3u : 1u);
  if (channel.values.size() != keys * perKey) return false;
  for (const float value : channel.values)
    if (!std::isfinite(value)) return false;
  if (channel.path == AnimationPath::Rotation) {
    const usize offset = channel.interpolation == AnimationInterpolation::CubicSpline ? 4 : 0;
    for (usize i = 0; i < keys; ++i) {
      float pose[4];std::copy_n(channel.values.data()+i*perKey+offset,4,pose);
      if(!normalizeRotationQuaternion(pose))return false;
    }
  }
  return true;
}

bool sampleAnimationChannel(const AnimationChannel &channel, float time, float out[4]) {
  if (channel.components() > 4) return false;
  return sampleAnimationChannel(channel, time, std::span<float>(out, 4));
}

bool sampleAnimationChannel(const AnimationChannel &channel, float time, std::span<float> out) {
  return validAnimationChannel(channel)&&sampleValidatedAnimationChannel(channel,time,out);
}
bool sampleValidatedAnimationChannel(const AnimationChannel &channel,float time,std::span<float> out) {
  if(!std::isfinite(time)||out.size()<channel.components())return false;
  const u32 components = channel.components();
  if(!channel.layerSources.empty()) {
    // Fixed storage, one level only, no allocations or key scans in the hot path.
    float result[MaximumMorphTargets]{},value[MaximumMorphTargets]{},reference[MaximumMorphTargets]{};
    const auto &base=channel.layerSources.front();
    if(!sampleValidatedAnimationChannel(base,0,{result,components}))return false;
    for(const auto &source:channel.layerSources) {
      const float weight=source.layerWeight;if(weight==0)continue;
      if(!sampleValidatedAnimationChannel(source,time,{value,components}))return false;
      if(source.layerAdditive) {
        std::fill_n(reference,components,channel.path==AnimationPath::Scale?1.f:0.f);
        if(channel.path==AnimationPath::Rotation)reference[3]=1;
        if(source.layerReferenceTime>=0&&!sampleValidatedAnimationChannel(source,source.layerReferenceTime,{reference,components}))return false;
        if(channel.path==AnimationPath::Rotation) {
          if(!normalizeRotationQuaternion(reference)||!normalizeRotationQuaternion(value))return false;
          float delta[4],inverse[]{-reference[0],-reference[1],-reference[2],reference[3]},identity[]{0,0,0,1},scaled[4],next[4];
          const auto multiply=[](const float *a,const float *b,float *q) {
            const float r[]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
              a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};std::copy_n(r,4,q);
          };
          multiply(inverse,value,delta);
          if(!interpolateRotationArc(identity,delta,weight,scaled))return false;
          multiply(result,scaled,next);std::copy_n(next,4,result);
          if(!normalizeRotationQuaternion(result))return false;
        } else for(u32 c=0;c<components;++c) {
          if(channel.path==AnimationPath::Scale) {
            if(std::abs(reference[c])<1e-8f)return false;
            result[c]*=1+(value[c]/reference[c]-1)*weight;
          } else result[c]+=(value[c]-reference[c])*weight;
        }
      } else if(channel.path==AnimationPath::Rotation) {
        float next[4];if(!interpolateRotationArc(result,value,weight,next))return false;std::copy_n(next,4,result);
      } else for(u32 c=0;c<components;++c)result[c]=static_cast<float>(double(result[c])+(double(value[c])-result[c])*weight);
      for(u32 c=0;c<components;++c)if(!std::isfinite(result[c]))return false;
    }
    std::copy_n(result,components,out.begin());return true;
  }
  if(!channel.curves.empty()) {
    if(channel.path==AnimationPath::Rotation&&channel.rotationMode==AnimationRotationMode::Euler) {
      float angles[3];for(u32 c=0;c<3;++c) {
        AnimationCurveSample value;if(!sampleValidatedAnimationCurve(channel.curves[c],time,value))return false;
        angles[c]=static_cast<float>(value.value);
      }
      rotationQuaternionXYZ(angles,out.data());return normalizeRotationQuaternion(out.data());
    }
    if(channel.path==AnimationPath::Rotation&&channel.rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
      const auto &keys=channel.curves[0].keys;usize i=0;
      if(time>=keys.back().time)i=keys.size()-1;
      else if(time>keys.front().time)i=static_cast<usize>(std::upper_bound(keys.begin(),keys.end(),time,[](float t,const auto &k){return t<k.time;})-keys.begin())-1;
      float a[4];for(u32 c=0;c<4;++c)a[c]=channel.curves[c].keys[i].value;
      if(!normalizeRotationQuaternion(a))return false;
      if(i+1==keys.size()||time<=keys.front().time) {std::copy(a,a+4,out.begin());return true;}
      float b[4];for(u32 c=0;c<4;++c)b[c]=channel.curves[c].keys[i+1].value;
      if(!normalizeRotationQuaternion(b))return false;
      const auto &progress=channel.curves[4];const double length=double(progress.keys[i+1].value)-progress.keys[i].value;
      // Equal orientations have a flat, non-editable progress segment.
      if(length<=0) {std::copy(a,a+4,out.begin());return true;}
      AnimationCurveSample value;if(!sampleValidatedAnimationCurve(progress,time,value))return false;
      const double alpha=(value.value-progress.keys[i].value)/length;
      if(!std::isfinite(alpha)||std::abs(alpha)>1e30)return false;
      return interpolateRotationArc(a,b,alpha,out.data());
    }
    if(channel.path==AnimationPath::Rotation) {
      const auto &keys=channel.curves[0].keys;
      if(keys.size()>1&&time>keys.front().time&&time<keys.back().time) {
        const auto upper=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const auto &k){return t<k.time;});
        const usize i=static_cast<usize>(upper-keys.begin())-1;bool linear=true;
        float a[4],b[4];
        for(u32 c=0;c<4;++c) {
          const auto &ka=channel.curves[c].keys[i],&kb=channel.curves[c].keys[i+1];a[c]=ka.value;b[c]=kb.value;
          linear=linear&&ka.outgoing==AnimationTangentMode::Linear&&kb.incoming==AnimationTangentMode::Linear;
        }
        if(linear) {
          // Authored API values can be finite but large enough to overflow a
          // float dot product. Never turn that into the legacy identity fallback.
          if(!normalizeRotationQuaternion(a)||!normalizeRotationQuaternion(b))return false;
          return interpolateRotationArc(a,b,(time-keys[i].time)/(keys[i+1].time-keys[i].time),out.data());
        }
      }
    }
    for(u32 c=0;c<components;++c) {
      AnimationCurveSample value;if(!sampleValidatedAnimationCurve(channel.curves[c],time,value))return false;
      out[c]=static_cast<float>(value.value);
      if(!std::isfinite(out[c]))return false;
    }
    if(channel.path==AnimationPath::Rotation) {
      if(!normalizeRotationQuaternion(out.data()))return false;
    }
    return true;
  }
  const bool cubic = channel.interpolation == AnimationInterpolation::CubicSpline;
  const usize keys = channel.times.size();
  const auto value = [&](usize key, u32 part) -> const float * {
    // CUBICSPLINE: part 0 tangente de entrada, 1 valor, 2 tangente de saída.
    return channel.values.data() + (cubic ? (key * 3 + part) : key) * components;
  };
  const auto emit = [&](const float *source) {
    for (u32 i = 0; i < components; ++i) out[i] = source[i];
    if (channel.path == AnimationPath::Rotation) return normalizeRotationQuaternion(out.data());
    return true;
  };
  if (keys == 1 || time <= channel.times.front()) return emit(value(0, 1));
  if (time >= channel.times.back()) return emit(value(keys - 1, 1));
  const auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
  const usize next = static_cast<usize>(upper - channel.times.begin());
  const usize previous = next - 1;
  const float t0 = channel.times[previous], t1 = channel.times[next];
  const float span = t1 - t0;
  if (channel.interpolation == AnimationInterpolation::Step || !(span > 0)) return emit(value(previous, 1));
  const float s = (time - t0) / span;
  if (channel.interpolation == AnimationInterpolation::Linear) {
    const float *a = value(previous, 1), *b = value(next, 1);
    if (channel.path == AnimationPath::Rotation) {
      return interpolateRotationArc(a,b,s,out.data());
    }
    for (u32 i = 0; i < components; ++i) {
      out[i] = static_cast<float>(double(a[i])+(double(b[i])-a[i])*s);
      if(!std::isfinite(out[i]))return false;
    }
    return true;
  }
  // Hermite cúbico da especificação: v(s) = (2s³-3s²+1)·v0 + (s³-2s²+s)·Δt·b0 +
  // (-2s³+3s²)·v1 + (s³-s²)·Δt·a1, com b0 a tangente de saída de v0 e a1 a de
  // entrada de v1.
  const float s2 = s * s, s3 = s2 * s;
  const float h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
  const float *v0 = value(previous, 1), *b0 = value(previous, 2), *v1 = value(next, 1), *a1 = value(next, 0);
  for (u32 i = 0; i < components; ++i) {
    out[i] = static_cast<float>(double(h00)*v0[i]+double(h10)*span*b0[i]+double(h01)*v1[i]+double(h11)*span*a1[i]);
    if(!std::isfinite(out[i]))return false;
  }
  if (channel.path == AnimationPath::Rotation) return normalizeRotationQuaternion(out.data());
  return true;
}

bool validMorphTargetSet(const MorphTargetSet &set) {
  if (!set.targetCount || set.targetCount > MaximumMorphTargets || !set.vertexCount) return false;
  if (set.deltas.size() != usize(set.targetCount) * set.vertexCount * MorphDeltaStride) return false;
  if (set.defaultWeights.size() != set.targetCount || set.maximumDisplacement.size() != set.targetCount) return false;
  if (!set.names.empty() && set.names.size() != set.targetCount) return false;
  for (const float value : set.deltas) if (!std::isfinite(value)) return false;
  for (const float value : set.defaultWeights) if (!std::isfinite(value)) return false;
  for (const float value : set.maximumDisplacement) if (!std::isfinite(value) || value < 0) return false;
  return true;
}

AssetGuid animationClipGuid(const AssetGuid &source, std::string_view name, u32 ordinal) {
  return assetGuidFromSeed("clipe:" + source.text() + ":" + std::string(name) + ":" + std::to_string(ordinal));
}

std::vector<AssetGuid> animationClipGuids(const AssetGuid &source, std::span<const AnimationClip> clips) {
  std::vector<AssetGuid> out;
  for (usize c = 0; c < clips.size(); ++c) {
    u32 ordinal = 0;
    for (usize k = 0; k < c; ++k) if (clips[k].name == clips[c].name) ++ordinal;
    out.push_back(animationClipGuid(source, clips[c].name, ordinal));
  }
  return out;
}

std::string animationClipDisplayName(const AnimationClip &clip, u32 index) {
  return clip.name.empty() ? "Clipe " + std::to_string(index + 1) : clip.name;
}

float morphBoundsExpansion(const MorphTargetSet &set, std::span<const float> weights) {
  float expansion = 0;
  for (u32 t = 0; t < set.targetCount && t < weights.size() && t < set.maximumDisplacement.size(); ++t)
    expansion += std::fabs(weights[t]) * set.maximumDisplacement[t];
  return std::isfinite(expansion) ? expansion : 0.0f;
}

bool deformPositions(std::span<float> positions, std::span<const u8> influences, std::span<const float> palette,
                     u32 influenceLimit, const MorphTargetSet *morph, std::span<const float> weights) {
  const usize vertices = positions.size() / 3;
  if (positions.size() % 3) return false;
  if (morph) {
    if (!validMorphTargetSet(*morph) || morph->vertexCount != vertices) return false;
    for (u32 t = 0; t < morph->targetCount && t < weights.size(); ++t) {
      const float w = weights[t];
      if (w == 0 || !std::isfinite(w)) continue;
      for (usize v = 0; v < vertices; ++v) {
        const float *delta = morph->deltas.data() + (v * morph->targetCount + t) * MorphDeltaStride;
        for (u32 axis = 0; axis < 3; ++axis) positions[v * 3 + axis] += w * delta[axis];
      }
    }
  }
  if (palette.empty()) return true;
  if (palette.size() % 16 || influences.size() != vertices * SkinInfluenceStride) return false;
  const usize joints = palette.size() / 16;
  for (usize v = 0; v < vertices; ++v) {
    u16 packed[8];
    std::copy(influences.data() + v * SkinInfluenceStride, influences.data() + (v + 1) * SkinInfluenceStride,
              reinterpret_cast<u8 *>(packed));
    float m[12]{}, total = 0;
    for (u32 k = 0; k < 4 && k < influenceLimit; ++k) {
      const float w = packed[4 + k] / 65535.0f;
      if (w <= 0 || packed[k] >= joints) continue;
      const float *joint = palette.data() + usize(packed[k]) * 16;
      for (u32 c = 0; c < 4; ++c)
        for (u32 r = 0; r < 3; ++r) m[c * 3 + r] += w * joint[c * 4 + r];
      total += w;
    }
    if (!(total > 0)) continue; // sem peso válido: forma base, como no compute
    const float p[3]{positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
    for (u32 r = 0; r < 3; ++r)
      positions[v * 3 + r] = (m[r] * p[0] + m[3 + r] * p[1] + m[6 + r] * p[2] + m[9 + r]) / total;
  }
  return true;
}

void composeTransform(const float t[3], const float q[4], const float s[3], float out[16]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  const float xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
  out[0] = (1 - 2 * (yy + zz)) * s[0];
  out[1] = 2 * (xy + wz) * s[0];
  out[2] = 2 * (xz - wy) * s[0];
  out[3] = 0;
  out[4] = 2 * (xy - wz) * s[1];
  out[5] = (1 - 2 * (xx + zz)) * s[1];
  out[6] = 2 * (yz + wx) * s[1];
  out[7] = 0;
  out[8] = 2 * (xz + wy) * s[2];
  out[9] = 2 * (yz - wx) * s[2];
  out[10] = (1 - 2 * (xx + yy)) * s[2];
  out[11] = 0;
  out[12] = t[0];
  out[13] = t[1];
  out[14] = t[2];
  out[15] = 1;
}

bool computeSkinPalette(const SkinDefinition &skin, const float drawModel[16],
                        const std::vector<float> &jointWorlds, std::vector<float> &palette) {
  const usize joints = skin.joints.size();
  if (!joints || joints > MaximumSkinJoints || jointWorlds.size() != joints * 16 ||
      skin.inverseBind.size() != joints * 16)
    return false;
  float inverseModel[16];
  if (!invert(drawModel, inverseModel)) return false;
  std::vector<float> result(joints * 16);
  for (usize j = 0; j < joints; ++j) {
    float relative[16];
    multiply(inverseModel, jointWorlds.data() + j * 16, relative);
    multiply(relative, skin.inverseBind.data() + j * 16, result.data() + j * 16);
    for (u32 i = 0; i < 16; ++i)
      if (!std::isfinite(result[j * 16 + i])) return false;
  }
  palette = std::move(result);
  return true;
}

bool skinnedLocalBounds(const SkinDefinition &skin, const std::vector<float> &palette,
                        float center[3], float &radius) {
  const usize joints = skin.joints.size();
  if (palette.size() != joints * 16 || skin.jointSpheres.size() != joints * 4) return false;
  float minimum[3]{0, 0, 0}, maximum[3]{0, 0, 0};
  bool any = false;
  for (usize j = 0; j < joints; ++j) {
    const float *sphere = skin.jointSpheres.data() + j * 4;
    if (sphere[3] < 0) continue;
    const float *m = palette.data() + j * 16;
    float c[3];
    for (u32 axis = 0; axis < 3; ++axis)
      c[axis] = m[axis] * sphere[0] + m[4 + axis] * sphere[1] + m[8 + axis] * sphere[2] + m[12 + axis];
    // Escala máxima da paleta: a esfera continua contendo os vértices mesmo
    // com junta escalada.
    float scale = 0;
    for (u32 column = 0; column < 3; ++column)
      scale = std::max(scale, std::sqrt(m[column * 4] * m[column * 4] + m[column * 4 + 1] * m[column * 4 + 1] +
                                        m[column * 4 + 2] * m[column * 4 + 2]));
    const float r = sphere[3] * scale;
    for (u32 axis = 0; axis < 3; ++axis) {
      if (!any || c[axis] - r < minimum[axis]) minimum[axis] = c[axis] - r;
      if (!any || c[axis] + r > maximum[axis]) maximum[axis] = c[axis] + r;
    }
    any = true;
  }
  if (!any) return false;
  float squared = 0;
  for (u32 axis = 0; axis < 3; ++axis) {
    center[axis] = (minimum[axis] + maximum[axis]) * .5f;
    const float half = (maximum[axis] - minimum[axis]) * .5f;
    squared += half * half;
  }
  radius = std::sqrt(squared);
  return std::isfinite(radius);
}

} // namespace ae::resources

namespace ae::resources {
bool validAnimationCues(std::span<const AnimationCue> cues,float duration) {
  if(cues.size()>MaximumAnimationCues||!std::isfinite(duration)||duration<0)return false;
  std::unordered_set<u64> ids;float previous=-1;u64 previousId=0;
  for(const auto &c:cues) {
    if(!c.id||c.id>static_cast<u64>(std::numeric_limits<i64>::max())||!ids.insert(c.id).second||
       !std::isfinite(c.time)||c.time<0||c.time>duration||static_cast<u32>(c.kind)>1||
       c.name.empty()||c.name.size()>256||c.tag>16777215||!std::isfinite(c.value)||
       (c.time<previous)||(c.time==previous&&c.id<=previousId))return false;
    for(unsigned char ch:c.name)if(ch<32||ch==127)return false;
    previous=c.time;previousId=c.id;
  }
  return true;
}
u64 visitAnimationCues(const AnimationClip &clip,double from,double to,AnimationWrapMode mode,
                      const std::function<void(const AnimationCue &)> &deliver,u32 budget,bool validated) {
  if(!deliver||from==to||!std::isfinite(from)||!std::isfinite(to)||
      !std::isfinite(clip.duration)||clip.duration<=0||
      static_cast<u32>(mode)>3||(!validated&&!validAnimationCues(clip.cues,clip.duration)))return 0;
  const bool increasing=to>from;const double d=clip.duration;
  struct Cursor {const AnimationCue *cue;double time,step;long double remaining;};
  auto compare=[&](const Cursor &a,const Cursor &b){return a.time!=b.time?(increasing?a.time>b.time:a.time<b.time):a.cue->id>b.cue->id;};
  std::priority_queue<Cursor,std::vector<Cursor>,decltype(compare)> heap(compare);
  u64 total=0;const auto maximum=std::numeric_limits<u64>::max();
  const auto series=[&](const AnimationCue &cue,double offset,double period,bool localForward) {
    if(!cue.enabled||cue.kind!=AnimationCueKind::Event||(localForward?!cue.forward:!cue.reverse))return;
    long double count=0;double first=offset;
    if(period>0) {
      const long double origin=(static_cast<long double>(from)-offset)/period;
      const long double destination=(static_cast<long double>(to)-offset)/period;
      if(increasing) {const long double start=std::floor(origin)+1;first=static_cast<double>(offset+start*period);count=std::floor(destination)-start+1;}
      else {const long double start=std::ceil(origin)-1;first=static_cast<double>(offset+start*period);count=start-std::ceil(destination)+1;}
    } else count=increasing?(offset>from&&offset<=to):(offset<from&&offset>=to);
    if(!(count>0))return;
    const u64 occurrences=count>=static_cast<long double>(maximum)?maximum:static_cast<u64>(count);
    total=occurrences>maximum-total?maximum:total+occurrences;
    // Beyond double's temporal precision, account for loss instead of dispatching
    // an occurrence at the excluded origin (or outside the requested interval).
    if(std::isfinite(first)&&(increasing?(first>from&&first<=to):(first<from&&first>=to)))
      heap.push({&cue,first,increasing?period:-period,count});
  };
  for(const auto &cue:clip.cues) {
    if(mode==AnimationWrapMode::Loop)series(cue,cue.time,d,increasing);
    else if(mode==AnimationWrapMode::PingPong) {
      // Direction is the local arriving leg, including each turnaround endpoint.
      series(cue,cue.time,2*d,cue.time==0?false:cue.time==d?true:increasing);
      if(cue.time>0&&cue.time<d)series(cue,2*d-cue.time,2*d,!increasing);
    } else series(cue,cue.time,0,increasing);
  }
  u32 emitted=0;budget=std::min(budget,256u);
  while(!heap.empty()&&emitted<budget) {
    auto cursor=heap.top();heap.pop();deliver(*cursor.cue);++emitted;
    const double next=cursor.time+cursor.step;
    if(cursor.remaining>1&&cursor.step!=0&&next!=cursor.time) {cursor.time=next;--cursor.remaining;heap.push(cursor);}
  }
  return total>emitted?total-emitted:0;
}
}
