#pragma once
#include "scene/physics_field2d.h"
#include "runtime/physics_field_sample.h"
namespace ae::runtime {
struct PhysicsField2DFrame {
 const scene::PhysicsFieldProperties *value=nullptr;int kind=-1;bool active=false;
 float world[16]{},inverse[4]{},right[2]{},center[2]{};
};
inline bool makePhysicsField2DFrame(const SceneGraph &graph,ObjectId id,const scene::ComponentValue &component,PhysicsField2DFrame &out){
 out={};out.kind=scene::physicsField2DKind(component);if(out.kind<0||!component.valid())return false;
 out.value=static_cast<const scene::PhysicsFieldProperties*>(&component);out.active=graph.activeInHierarchy(id)&&out.value->enabled;
 if(!worldMatrix(graph,id,out.world))return false;
 const auto *m=out.world;
 if(std::abs(m[2])>1e-5f||std::abs(m[6])>1e-5f||std::abs(m[8])>1e-5f||std::abs(m[9])>1e-5f)return false;
 const double determinant=double(m[0])*m[5]-double(m[4])*m[1];
 if(!std::isfinite(determinant)||determinant<=1e-10)return false;
 out.inverse[0]=float(m[5]/determinant);out.inverse[1]=float(-m[4]/determinant);out.inverse[2]=float(-m[1]/determinant);out.inverse[3]=float(m[0]/determinant);
 const double length=std::hypot(double(m[0]),double(m[1]));if(length<1e-6)return false;
 for(u32 a=0;a<2;++a){out.right[a]=float(m[a]/length);out.center[a]=m[12+a]+m[a]*out.value->offset[0]+m[4+a]*out.value->offset[1];}
 return true;
}
inline PhysicsFieldSample samplePhysicsField2D(const PhysicsField2DFrame &frame,const float point[2],u32 layer){
 PhysicsFieldSample out;const auto &v=*frame.value;const float x=point[0]-frame.world[12],y=point[1]-frame.world[13];
 const double localX=frame.inverse[0]*x+frame.inverse[1]*y-v.offset[0],localY=frame.inverse[2]*x+frame.inverse[3]*y-v.offset[1];
 const double distance=v.shape==0?std::max(std::abs(localX)/v.halfExtents[0],std::abs(localY)/v.halfExtents[1]):std::hypot(localX,localY)/v.radius;
 if(!std::isfinite(distance)||distance>1)return out;
 out.flags=1;if(!frame.active||(v.affectedLayer&&v.affectedLayer!=layer+1))return out;
 out.flags|=2|(v.wakeBodies?4:0);out.weight=v.falloff==0?1:float(1-distance);
 if(v.falloff==2)out.weight=out.weight*out.weight*(3-2*out.weight);
 if(frame.kind==0||frame.kind==1){
  float *target=frame.kind==0?out.acceleration:out.windVelocity;
  target[0]=frame.right[0]*v.vector[0]-frame.right[1]*v.vector[1];target[1]=frame.right[1]*v.vector[0]+frame.right[0]*v.vector[1];
  if(frame.kind==0){for(u32 a=0;a<2;++a)target[a]*=out.weight;out.overrideWeight=v.replaceWorldGravity?out.weight:0;}
  else out.windDrag=v.coefficient*out.weight;
 }else if(frame.kind==2){out.linearDrag=v.linearDrag*out.weight;out.angularDrag=v.angularDrag*out.weight;}
 else {const double dx=point[0]-frame.center[0],dy=point[1]-frame.center[1],length=std::hypot(dx,dy);
  if(length>1e-6){const double rx=dx/length,ry=dy/length;out.acceleration[0]=float(out.weight*(v.acceleration*rx-v.tangentialAcceleration*ry));out.acceleration[1]=float(out.weight*(v.acceleration*ry+v.tangentialAcceleration*rx));}
 }
 return out;
}
}
