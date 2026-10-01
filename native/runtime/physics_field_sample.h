#pragma once
#include "scene/physics_field.h"
#include "runtime/transform_math.h"
#include "renderer/normal_matrix.h"

namespace ae::runtime {
// Frozen ABI33 payload. Flags: geometric containment, effective enabled, wake.
struct PhysicsFieldSample {
  u32 size=64,flags=0;float weight=0,acceleration[3]{},windVelocity[3]{};
  float windDrag=0,linearDrag=0,angularDrag=0,overrideWeight=0;
  u32 affectedBodies=0;float affectedMass=0;u32 reserved=0;
};
static_assert(sizeof(PhysicsFieldSample)==64);
struct PhysicsFieldFrame {
  const scene::PhysicsFieldProperties *value=nullptr;
  ObjectId object=0;int kind=-1;bool active=false;
  float world[16]{},inverse[9]{},basis[9]{},center[3]{};
};
inline bool makePhysicsFieldFrame(const SceneGraph &graph,ObjectId id,const scene::ComponentValue &component,PhysicsFieldFrame &out){
  out={};out.kind=scene::physicsFieldKind(component);
  if(out.kind<0||!component.valid())return false;
  out.value=static_cast<const scene::PhysicsFieldProperties*>(&component);out.object=id;
  out.active=graph.activeInHierarchy(id)&&out.value->enabled;
  if(!worldMatrix(graph,id,out.world))return false;
  float normal[12];if(!renderer::buildNormalMatrix(out.world,normal))return false;
  for(u32 r=0;r<3;++r)for(u32 c=0;c<3;++c)out.inverse[r*3+c]=normal[r*4+c];
  auto normalize=[](float *v){double n=0;for(u32 a=0;a<3;++a)n+=double(v[a])*v[a];if(n<1e-12||!std::isfinite(n))return false;n=std::sqrt(n);for(u32 a=0;a<3;++a)v[a]=float(v[a]/n);return true;};
  float forward[3]{out.world[8],out.world[9],out.world[10]};if(!normalize(forward))return false;
  float right[3]{out.world[5]*forward[2]-out.world[6]*forward[1],out.world[6]*forward[0]-out.world[4]*forward[2],out.world[4]*forward[1]-out.world[5]*forward[0]};if(!normalize(right))return false;
  float up[3]{forward[1]*right[2]-forward[2]*right[1],forward[2]*right[0]-forward[0]*right[2],forward[0]*right[1]-forward[1]*right[0]};
  for(u32 a=0;a<3;++a){out.basis[a]=right[a];out.basis[3+a]=up[a];out.basis[6+a]=forward[a];out.center[a]=out.world[12+a];for(u32 c=0;c<3;++c)out.center[a]+=out.world[c*4+a]*out.value->offset[c];}
  return true;
}
inline PhysicsFieldSample samplePhysicsField(const PhysicsFieldFrame &frame,const float point[3],u32 layer){
  PhysicsFieldSample out;const auto &v=*frame.value;float p[3]{};
  for(u32 r=0;r<3;++r){for(u32 c=0;c<3;++c)p[r]+=frame.inverse[r*3+c]*(point[c]-frame.world[12+c]);p[r]-=v.offset[r];}
  double distance=0;
  if(v.shape==0){for(u32 a=0;a<3;++a)distance=std::max(distance,double(std::abs(p[a])/v.halfExtents[a]));}
  else {for(float n:p)distance+=double(n)*n;distance=std::sqrt(distance)/v.radius;}
  if(distance>1||!std::isfinite(distance))return out;
  out.flags=1;
  if(!frame.active||(v.affectedLayer&&v.affectedLayer!=layer+1))return out;
  out.flags|=2|(v.wakeBodies?4:0);
  out.weight=v.falloff==0?1:float(1-distance);
  if(v.falloff==2)out.weight=out.weight*out.weight*(3-2*out.weight);
  auto direction=[&](const float *local,float *world){for(u32 a=0;a<3;++a)for(u32 c=0;c<3;++c)world[a]+=frame.basis[c*3+a]*local[c];};
  if(frame.kind==0){direction(v.vector,out.acceleration);for(float &n:out.acceleration)n*=out.weight;out.overrideWeight=v.replaceWorldGravity?out.weight:0;}
  else if(frame.kind==1){direction(v.vector,out.windVelocity);out.windDrag=v.coefficient*out.weight;}
  else if(frame.kind==2){out.linearDrag=v.linearDrag*out.weight;out.angularDrag=v.angularDrag*out.weight;}
  else {
    double n=0;float radial[3];for(u32 a=0;a<3;++a){radial[a]=point[a]-frame.center[a];n+=double(radial[a])*radial[a];}
    if(n>1e-12){n=std::sqrt(n);for(float &a:radial)a=float(a/n);
      float tangent[3]{frame.basis[4]*radial[2]-frame.basis[5]*radial[1],frame.basis[5]*radial[0]-frame.basis[3]*radial[2],frame.basis[3]*radial[1]-frame.basis[4]*radial[0]};
      const float length=std::sqrt(tangent[0]*tangent[0]+tangent[1]*tangent[1]+tangent[2]*tangent[2]);
      for(u32 a=0;a<3;++a)out.acceleration[a]=out.weight*(v.acceleration*radial[a]+(length>1e-6f?v.tangentialAcceleration*tangent[a]/length:0));
    }
  }
  return out;
}
}
