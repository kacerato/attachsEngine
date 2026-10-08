#pragma once
// Typed relative pose math, shared by runtime and future authoring/preview.
// Local rotation delta = inverse(reference) * sample; apply on the right.
#include "resources/skeletal_animation.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace ae::resources {
inline bool normalizePoseQuaternion(std::span<float> q) {
  if(q.size()!=4) return false;
  float n=0;for(float v:q) {if(!std::isfinite(v)) return false;n+=v*v;}
  if(!std::isfinite(n)||n<1e-20f) return false;
  n=1/std::sqrt(n);for(float &v:q) v*=n;return true;
}
inline void multiplyPoseQuaternion(const float *a,const float *b,float *q) {
  const float r[]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
    a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
    a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
  std::copy(r,r+4,q);
}
inline void canonicalPoseQuaternion(std::span<float> q) {
  float sign=q[3];
  if(sign==0) for(u32 i=0;i<3;++i) if(q[i]!=0) {sign=q[i];break;}
  if(sign<0) for(float &v:q) v=-v;
}
inline bool relativeAnimationPose(AnimationPath path,std::span<const float> value,
                                  std::span<const float> reference,std::vector<float> &delta) {
  if(value.empty()||value.size()!=reference.size()) return false;
  delta.resize(value.size());
  for(usize i=0;i<value.size();++i) if(!std::isfinite(value[i])||!std::isfinite(reference[i])) return false;
  if(path==AnimationPath::Rotation) {
    if(value.size()!=4) return false;
    std::vector<float> q(value.begin(),value.end()),r(reference.begin(),reference.end());
    if(!normalizePoseQuaternion(q)||!normalizePoseQuaternion(r)) return false;
    r[0]=-r[0];r[1]=-r[1];r[2]=-r[2];multiplyPoseQuaternion(r.data(),q.data(),delta.data());
    if(!normalizePoseQuaternion(delta)) return false;
    canonicalPoseQuaternion(delta);
  } else for(usize i=0;i<value.size();++i) {
    if(path==AnimationPath::Scale) {
      if(std::abs(reference[i])<1e-8f) return false;
      delta[i]=value[i]/reference[i];
    } else delta[i]=value[i]-reference[i];
    if(!std::isfinite(delta[i])) return false;
  }
  return true;
}
inline bool applyRelativeAnimationPose(AnimationPath path,std::span<float> result,
                                       std::span<const float> delta,float weight) {
  if(result.size()!=delta.size()||!std::isfinite(weight)||weight<0||weight>1) return false;
  if(path==AnimationPath::Rotation) {
    if(result.size()!=4) return false;
    std::vector<float> q(delta.begin(),delta.end());if(!normalizePoseQuaternion(q)) return false;
    canonicalPoseQuaternion(q);
    const float theta=std::acos(std::clamp(q[3],-1.f,1.f)),s=std::sin(theta);
    const float k=s>1e-6f?std::sin(theta*weight)/s:weight;
    for(u32 i=0;i<3;++i) q[i]*=k;
    q[3]=std::cos(theta*weight);
    std::vector<float> candidate(result.begin(),result.end());
    if(!normalizePoseQuaternion(candidate)) return false;
    multiplyPoseQuaternion(candidate.data(),q.data(),candidate.data());
    if(!normalizePoseQuaternion(candidate)) return false;
    std::copy(candidate.begin(),candidate.end(),result.begin());return true;
  }
  std::vector<float> candidate(result.begin(),result.end());
  for(usize i=0;i<result.size();++i) {
    if(path==AnimationPath::Scale) candidate[i]*=1+(delta[i]-1)*weight;
    else candidate[i]+=delta[i]*weight;
    if(!std::isfinite(candidate[i])) return false;
  }
  std::copy(candidate.begin(),candidate.end(),result.begin());
  return true;
}
} // namespace ae::resources
