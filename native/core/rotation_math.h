#pragma once
#include "core/base.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae {
// Scene convention: Euler degrees, Rz * Ry * Rx, quaternion (x,y,z,w).
// Shared by authored Euler curves and the runtime Transform, never a UI-only
// approximation of the rotation used by rendering and physics.
inline void rotationQuaternionXYZ(const float degrees[3],float out[4]) {
  constexpr float half=0.00872664626f;
  const float x=degrees[0]*half,y=degrees[1]*half,z=degrees[2]*half;
  const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
  out[0]=sx*cy*cz-cx*sy*sz;out[1]=cx*sy*cz+sx*cy*sz;
  out[2]=cx*cy*sz-sx*sy*cz;out[3]=cx*cy*cz+sx*sy*sz;
}
inline bool normalizeRotationQuaternion(float q[4]) {
  double norm=0;for(u32 c=0;c<4;++c)norm+=double(q[c])*q[c];
  if(norm<1e-20||!std::isfinite(norm))return false;
  const double inverse=1/std::sqrt(norm);for(u32 c=0;c<4;++c)q[c]=static_cast<float>(q[c]*inverse);
  return true;
}
// Canonical Rz * Ry * Rx branch, used only when initializing a new Euler pose.
// Existing Euler curves/turns must never be replaced by this canonical branch.
inline bool rotationEulerXYZ(const float quaternion[4],float degrees[3]) {
  float q[4];std::copy_n(quaternion,4,q);if(!normalizeRotationQuaternion(q))return false;
  const double x=q[0],y=q[1],z=q[2],w=q[3],pitch=std::clamp(2*(w*y-z*x),-1.0,1.0);
  constexpr double toDegrees=57.29577951308232;
  const double cosine=std::hypot(1-2*(y*y+z*z),2*(x*y+w*z));
  degrees[1]=static_cast<float>(std::atan2(pitch,cosine)*toDegrees);
  if(cosine<1e-6) {degrees[0]=static_cast<float>(std::atan2(2*(w*x-y*z),1-2*(x*x+z*z))*toDegrees);degrees[2]=0;}
  else {degrees[0]=static_cast<float>(std::atan2(2*(w*x+y*z),1-2*(x*x+y*y))*toDegrees);degrees[2]=static_cast<float>(std::atan2(2*(w*z+x*y),1-2*(y*y+z*z))*toDegrees);}
  return true;
}
// Lift an orientation to the XYZ Euler branch nearest an explicit reference.
// At gimbal lock X/Z share one degree of freedom: choose the closest pair,
// rather than arbitrarily zeroing Z and introducing a 180-degree jump.
inline double rotationDistanceDegrees(const float a[4],const float b[4]);
inline bool rotationEulerXYZNear(const float quaternion[4],const float reference[3],float degrees[3]) {
  float q[4];std::copy_n(quaternion,4,q);if(!normalizeRotationQuaternion(q))return false;
  for(u32 c=0;c<3;++c)if(!std::isfinite(reference[c])||std::abs(reference[c])>1e7f)return false;
  float canonical[3];if(!rotationEulerXYZ(q,canonical))return false;
  const double cosine=std::hypot(1-2*(double(q[1])*q[1]+double(q[2])*q[2]),2*(double(q[0])*q[1]+double(q[3])*q[2]));
  const auto near=[](double v,double r){return v+360*std::round((r-v)/360);};
  if(cosine<1e-6) {
    const double sign=canonical[1]>=0?1:-1;
    const double difference=near(canonical[0],reference[0]-sign*reference[2]);
    const double adjustment=(difference-(reference[0]-sign*reference[2]))*.5;
    degrees[0]=static_cast<float>(reference[0]+adjustment);
    degrees[1]=static_cast<float>(near(canonical[1],reference[1]));
    degrees[2]=static_cast<float>(reference[2]-sign*adjustment);
  } else {
    double best=std::numeric_limits<double>::infinity();
    for(u32 branch=0;branch<2;++branch) {
      double candidate[3]{canonical[0]+180*branch,branch?180-canonical[1]:canonical[1],canonical[2]+180*branch},cost=0;
      for(u32 c=0;c<3;++c) {candidate[c]=near(candidate[c],reference[c]);cost+=(candidate[c]-reference[c])*(candidate[c]-reference[c]);}
      if(cost<best) {best=cost;for(u32 c=0;c<3;++c)degrees[c]=static_cast<float>(candidate[c]);}
    }
  }
  float reconstructed[4];rotationQuaternionXYZ(degrees,reconstructed);
  return rotationDistanceDegrees(q,reconstructed)<.001;
}
// Double precision normalization avoids fictitious travel between identical
// float poses, and atan2 retains precision for very small rotations.
inline bool rotationArc(const float a[4],const float b[4],double &angle,double start[4],double direction[4]) {
  double na=0,nb=0;for(u32 c=0;c<4;++c) {na+=double(a[c])*a[c];nb+=double(b[c])*b[c];}
  if(!std::isfinite(na)||!std::isfinite(nb)||na<1e-20||nb<1e-20)return false;
  na=std::sqrt(na);nb=std::sqrt(nb);double target[4],dot=0;
  for(u32 c=0;c<4;++c) {start[c]=a[c]/na;target[c]=b[c]/nb;dot+=start[c]*target[c];}
  if(dot<0) {dot=-dot;for(auto &v:target)v=-v;}
  bool identical=true;for(u32 c=0;c<4;++c)identical&=start[c]==target[c];
  if(identical) {angle=0;for(u32 c=0;c<4;++c)direction[c]=0;return true;}
  dot=std::clamp(dot,0.0,1.0);double length=0;
  for(u32 c=0;c<4;++c) {direction[c]=target[c]-dot*start[c];length+=direction[c]*direction[c];}
  length=std::sqrt(length);angle=length==0?0:std::atan2(length,dot);
  if(angle)for(u32 c=0;c<4;++c)direction[c]/=length;
  return true;
}
inline double rotationDistanceDegrees(const float a[4],const float b[4]) {
  double angle,start[4],direction[4];
  return rotationArc(a,b,angle,start,direction)?angle*114.59155902616465:std::numeric_limits<double>::quiet_NaN();
}
inline bool interpolateRotationArc(const float a[4],const float b[4],double progress,float out[4]) {
  if(!std::isfinite(progress))return false;
  double angle,start[4],direction[4];if(!rotationArc(a,b,angle,start,direction))return false;
  // Unlike the common near-parallel nlerp shortcut, this retains constant
  // angular speed and meaningful overshoot even on very small rotations.
  const double phase=angle*progress;if(!std::isfinite(phase))return false;
  for(u32 c=0;c<4;++c)out[c]=static_cast<float>(std::cos(phase)*start[c]+(angle?std::sin(phase)*direction[c]:0));
  return normalizeRotationQuaternion(out);
}
} // namespace ae
