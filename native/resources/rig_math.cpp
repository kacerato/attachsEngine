#include "resources/rig_asset.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {
bool unit(std::span<const float> values) {
  double length=0;for(float v:values){if(!std::isfinite(v))return false;length+=double(v)*v;}
  return std::abs(length-1)<1e-4;
}
}
bool validRigTransform(const RigTransform &t) {
  for(float p:t.position)if(!std::isfinite(p)||std::abs(p)>1e7f)return false;
  if(!unit(t.rotation))return false;
  for(float s:t.scale)if(!std::isfinite(s)||s<1e-5f||s>1e5f||std::abs(s-t.scale[0])>1e-5f*t.scale[0])return false;
  return true;
}
bool validRigLimit(const RigAngularLimit &limit) {
  return static_cast<u8>(limit.kind)<=2&&unit(limit.axis)&&std::isfinite(limit.minimum)&&std::isfinite(limit.maximum)&&
      limit.minimum>=-180&&limit.maximum<=180&&limit.minimum<=limit.maximum&&std::isfinite(limit.cone)&&limit.cone>=0&&limit.cone<=180;
}
std::array<float,4> multiplyRigRotation(const std::array<float,4> &a,const std::array<float,4> &b) {
  std::array<float,4> q{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
    a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
    a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
  normalizeRotationQuaternion(q.data());return q;
}
std::array<float,4> inverseRigRotation(const std::array<float,4> &q) {return {-q[0],-q[1],-q[2],q[3]};}
std::array<float,3> rotateRigVector(const std::array<float,4> &q,const std::array<float,3> &v) {
  const std::array<float,3> t{2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
  return {v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
}
RigTransform composeRigTransform(const RigTransform &p,const RigTransform &l) {
  RigTransform out;out.rotation=multiplyRigRotation(p.rotation,l.rotation);
  std::array<float,3> position=l.position;for(u32 c=0;c<3;++c)position[c]*=p.scale[c];
  position=rotateRigVector(p.rotation,position);
  for(u32 c=0;c<3;++c){out.position[c]=p.position[c]+position[c];out.scale[c]=p.scale[c]*l.scale[c];}
  return out;
}
RigTransform relativeRigTransform(const RigTransform &w,const RigTransform &p) {
  RigTransform out;out.rotation=multiplyRigRotation(inverseRigRotation(p.rotation),w.rotation);
  std::array<float,3> offset;for(u32 c=0;c<3;++c)offset[c]=w.position[c]-p.position[c];
  offset=rotateRigVector(inverseRigRotation(p.rotation),offset);
  for(u32 c=0;c<3;++c){out.position[c]=offset[c]/p.scale[c];out.scale[c]=w.scale[c]/p.scale[c];}
  return out;
}
bool constrainRigRotation(const RigAngularLimit &limit,const std::array<float,4> &reference,std::array<float,4> &rotation) {
  if(!validRigLimit(limit)||!unit(reference)||!unit(rotation))return false;
  if(limit.kind==RigLimitKind::Free)return true;
  auto delta=multiplyRigRotation(inverseRigRotation(reference),rotation);
  if(delta[3]<0)for(float &v:delta)v=-v;
  const float projection=delta[0]*limit.axis[0]+delta[1]*limit.axis[1]+delta[2]*limit.axis[2];
  std::array<float,4> twist{projection*limit.axis[0],projection*limit.axis[1],projection*limit.axis[2],delta[3]};
  if(!normalizeRotationQuaternion(twist.data()))twist={0,0,0,1};
  auto swing=multiplyRigRotation(delta,inverseRigRotation(twist));
  constexpr float degrees=57.29577951308232f,radians=.017453292519943295f;
  const float signedTwist=2*std::atan2(twist[0]*limit.axis[0]+twist[1]*limit.axis[1]+twist[2]*limit.axis[2],twist[3])*degrees;
  const float angle=std::clamp(signedTwist,limit.minimum,limit.maximum)*radians*.5f;
  twist={limit.axis[0]*std::sin(angle),limit.axis[1]*std::sin(angle),limit.axis[2]*std::sin(angle),std::cos(angle)};
  if(limit.kind==RigLimitKind::Hinge)swing={0,0,0,1};
  else {
    const float swingAngle=2*std::acos(std::clamp(std::abs(swing[3]),0.f,1.f))*degrees;
    if(swingAngle>limit.cone&&swingAngle>1e-6f) {
      const std::array<float,4> identity{0,0,0,1};
      if(!interpolateRotationArc(identity.data(),swing.data(),limit.cone/swingAngle,swing.data()))return false;
    }
  }
  rotation=multiplyRigRotation(reference,multiplyRigRotation(swing,twist));return true;
}
} // namespace ae::resources
