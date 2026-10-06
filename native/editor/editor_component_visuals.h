#pragma once
#include "editor/editor_component_catalog.h"
#include "editor/editor_collider_geometry.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_scene_camera.h"
#include "scene/constant_force.h"
#include "runtime/physics_field_sample.h"
#include "runtime/physics_requirements.h"
#include "scene/transform_constraints.h"
#include "scene/spring_constraint.h"
#include "scene/transform_tween.h"
#include "scene/event_connection.h"
#include "scene/physics2d_components.h"
#include "scene/audio.h"
#include "scene/path.h"
#include "scene/path_follow.h"
#include "resources/curve3d_transform.h"
#include <bit>

namespace ae::editor {
// Derived editor data. These segments/icons never enter meshes, collision,
// shadows, serialization or exports. Providers consume the actual component.
struct ComponentVisualSegment {float a[3]{},b[3]{};u32 emphasis=0;};
struct ComponentVisual {
  EditorEntityId entity=0;
  u64 instance=0;
  ui::UiIcon icon=ui::UiIcon::EditorAuthorObject;
  bool enabled=true,marker=true;
  u32 markerIndex=0;
  float origin[3]{};
  std::vector<ComponentVisualSegment> segments;
};
inline constexpr usize MaximumVisibleColliderSegments=9600;
using ComponentVisualBuilder=void (*)(const scene::ComponentValue &,const EditorEntity &,const EditorMapScene *,
                                      const float *,float,bool,ComponentVisual &);
struct ComponentVisualProvider {const scene::ComponentType *type;ui::UiIcon icon;bool marker;ComponentVisualBuilder build;};
namespace visual_detail {
inline void segment(ComponentVisual &out,const float *world,const float *a,const float *b) {
  ComponentVisualSegment line;
  for(u32 k=0;k<3;++k) {
    line.a[k]=world[k]*a[0]+world[4+k]*a[1]+world[8+k]*a[2]+world[12+k];
    line.b[k]=world[k]*b[0]+world[4+k]*b[1]+world[8+k]*b[2]+world[12+k];
  }
  out.segments.push_back(line);
}
inline void ring(ComponentVisual &out,const float *world,float radius,float depth,u32 normal) {
  for(u32 i=0;i<48;++i) {
    float a[3]{},b[3]{};a[normal]=b[normal]=depth;
    const u32 u=(normal+1)%3,v=(normal+2)%3;
    a[u]=radius*std::cos(i*6.28318530718f/48);a[v]=radius*std::sin(i*6.28318530718f/48);
    b[u]=radius*std::cos((i+1)*6.28318530718f/48);b[v]=radius*std::sin((i+1)*6.28318530718f/48);
    segment(out,world,a,b);
  }
}
// Camera and light ranges are world distances. Remove object scale/shear from
// their optical frame while retaining orientation; colliders keep full scale.
inline bool opticalFrame(const float *world,float *pose) {
  return editorOpticalFrame(world,pose);
}
inline void characterGround(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,float,bool detail,ComponentVisual &out) {
  const auto&c=static_cast<const scene::Character&>(value);if(c.inheritPlatformHorizontal)out.icon=ui::UiIcon::PhysicsCharacterPlatformCarry;
  if(!detail||!c.valid())return;
  // Runtime capsules are upright world-Y and require unit scale. Object rotation
  // does not rotate gravity or the Jolt CharacterVirtual capsule.
  float pose[16]{};pose[0]=pose[5]=pose[10]=pose[15]=1;std::copy(world+12,world+15,pose+12);
  const float bottom=c.radius,top=c.radius+2*c.halfHeight;
  ring(out,pose,c.radius,bottom,1);ring(out,pose,c.radius,top,1);
  for(u32 axis:{0u,2u}) {
    for(u32 n=0;n<24;++n)for(bool upper:{false,true}) {
      const float a=n*3.14159265359f/24,b=(n+1)*3.14159265359f/24;
      float p[3]{},q[3]{};p[axis]=c.radius*std::cos(a);q[axis]=c.radius*std::cos(b);
      p[1]=(upper?top:bottom)+(upper?1:-1)*c.radius*std::sin(a);q[1]=(upper?top:bottom)+(upper?1:-1)*c.radius*std::sin(b);segment(out,pose,p,q);
    }
    for(float side:{-1.f,1.f}){float p[3]{},q[3]{};p[axis]=q[axis]=side*c.radius;p[1]=bottom;q[1]=top;segment(out,pose,p,q);}
  }
  // Two distinct measurements from the foot: climb above, floor search below.
  const float x=c.radius+.2f;
  for(float distance:{c.stepHeight,-c.floorSnapLength})if(distance!=0){const float a[3]{x,0,0},b[3]{x,distance,0};segment(out,pose,a,b);out.segments.back().emphasis=1;const float tip[3]{x+.1f,distance,0};segment(out,pose,b,tip);out.segments.back().emphasis=1;}
}
inline void camera(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,
                   float aspect,bool detail,ComponentVisual &out) {
  const auto &c=static_cast<const scene::Camera &>(value);out.enabled=c.enabled;
  if(!detail || !c.valid()) return;
  float pose[16];if(!opticalFrame(world,pose)) return;
  const bool orthographic=c.projection==scene::CameraProjection::Orthographic;
  const float tanV=orthographic?c.orthographicHalfHeight:std::tan(c.verticalFov*0.00872664626f),tanH=tanV*aspect;
  float corners[8][3]{};
  for(u32 p=0;p<2;++p) for(u32 i=0;i<4;++i) {
    const float z=p?c.farPlane:c.nearPlane;
    const float scale=orthographic?1.f:z;
    corners[p*4+i][0]=(i==0||i==3?-1.f:1.f)*scale*tanH;
    corners[p*4+i][1]=(i<2?-1.f:1.f)*scale*tanV;corners[p*4+i][2]=z;
  }
  for(u32 i=0;i<4;++i) {
    segment(out,pose,corners[i],corners[(i+1)%4]);
    segment(out,pose,corners[4+i],corners[4+(i+1)%4]);
    segment(out,pose,corners[i],corners[4+i]);
  }
  // A short oriented camera body remains readable even with a distant far plane.
  const float a[3]{-.2f,.18f,0},b[3]{.2f,.18f,0},tip[3]{0,.38f,0};
  segment(out,pose,a,b);segment(out,pose,b,tip);segment(out,pose,tip,a);
}
inline void light(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,
                  float,bool detail,ComponentVisual &out) {
  const auto &c=static_cast<const scene::Light &>(value);out.enabled=c.enabled;
  if(!detail || !c.valid()) return;
  float pose[16];if(!opticalFrame(world,pose)) return;
  if(c.kind==scene::LightKind::Point) {for(u32 axis=0;axis<3;++axis) ring(out,pose,c.range,0,axis);}
  else if(c.kind==scene::LightKind::Spot) {
    const float origin[3]{};
    for(float angle:{c.innerAngle,c.outerAngle}) {
      const float radius=c.range*std::sin(angle*0.01745329252f),depth=c.range*std::cos(angle*0.01745329252f);
      ring(out,pose,radius,depth,2);
      for(u32 i=0;i<4;++i) {float p[3]{radius*std::cos(i*1.57079632679f),radius*std::sin(i*1.57079632679f),depth};segment(out,pose,origin,p);}
    }
  } else {
    ring(out,pose,.4f,0,2);
    for(u32 i=0;i<4;++i) {
      float a[3]{.4f*std::cos(i*1.57079632679f),.4f*std::sin(i*1.57079632679f),0},b[3]{a[0],a[1],1.2f};
      segment(out,pose,a,b);
    }
  }
}
inline void multiply(const float *a,const float *b,float *out) {
  float value[16]{};
  for(u32 col=0;col<4;++col) for(u32 row=0;row<4;++row)
    for(u32 k=0;k<4;++k) value[col*4+row]+=a[k*4+row]*b[col*4+k];
  std::copy(value,value+16,out);
}
inline std::vector<u32> meshColliderSlots(const scene::Collider &c,const EditorEntity &entity,
                                          const EditorMapScene &resources) {
  std::vector<u32> slots;
  if(c.collisionMesh.valid()) {
    if(const auto slot=resources.assetSlot(c.collisionMesh)) slots.push_back(slot);
  } else if(const auto *render=meshRenderer(entity)) {
    for(u32 index=0;index<render->slotCount();++index) {
      const auto slot=render->slotMesh(index);
      if(slot && std::find(slots.begin(),slots.end(),slot)==slots.end()) slots.push_back(slot);
    }
  }
  return slots;
}
inline void meshCollider(const scene::Collider &c,const EditorEntity &entity,const EditorMapScene &resources,
                          const float *pose,ComponentVisual &out) {
  constexpr usize MaximumMeshSegments=2400;
  const auto slots=meshColliderSlots(c,entity,resources);
  if(c.convex) {
    EditorMapScene::CollisionHullPreview preview;
    if(resources.collisionHullPreview(slots,c.hullTolerance,preview)) {
      const auto remaining=MaximumMeshSegments/3;
      const usize stride=std::max<usize>(1,(preview.triangles.size()+remaining-1)/remaining);
      for(usize triangle=0;triangle<preview.triangles.size() && out.segments.size()+3<=MaximumMeshSegments;triangle+=stride) {
        const auto &points=preview.triangles[triangle];
        for(u32 edge=0;edge<3;++edge)
          segment(out,pose,points.data()+edge*3,points.data()+((edge+1)%3)*3);
      }
    }
    return;
  }
  for(const auto slot:slots) {
    std::span<const EditorPickMesh::Triangle> triangles;float relative[16],meshPose[16];
    if(!resources.localGeometry(slot,triangles,relative)) continue;
    multiply(pose,relative,meshPose);
    const auto remaining=(MaximumMeshSegments-out.segments.size())/3;
    if(!remaining) break;
    const usize stride=std::max<usize>(1,(triangles.size()+remaining-1)/remaining);
    for(usize triangle=0;triangle<triangles.size() && out.segments.size()+3<=MaximumMeshSegments;triangle+=stride) {
      const auto &points=triangles[triangle];
      for(u32 edge=0;edge<3;++edge) {
        const float *a=points.data()+edge*3,*b=points.data()+((edge+1)%3)*3;
        segment(out,meshPose,a,b);
      }
    }
  }
}
inline void collider(const scene::ComponentValue &value,const EditorEntity &entity,const EditorMapScene *resources,
                     const float *world,float,bool detail,ComponentVisual &out) {
  const auto &c=static_cast<const scene::Collider &>(value);out.enabled=c.enabled;if(!detail||!c.valid()) return;
  if(c.shape==scene::ColliderShape::Mesh) {
    // Legacy mesh pose remains opt-out; authoring can opt in to independent
    // center/rotation without altering the visual resource.
    float pose[16];std::copy(world,world+16,pose);
    if(c.meshLocalPose){EditorTransform t;t.position[0]=c.centerX;t.position[1]=c.centerY;t.position[2]=c.centerZ;
      t.rotationDegrees[0]=c.rotationX;t.rotationDegrees[1]=c.rotationY;t.rotationDegrees[2]=c.rotationZ;
      float local[16];editorTransformMatrix(t,local);multiply(world,local,pose);}
    if(resources) meshCollider(c,entity,*resources,pose,out);
    return;
  }
  EditorTransform t;t.position[0]=c.centerX;t.position[1]=c.centerY;t.position[2]=c.centerZ;
  t.rotationDegrees[0]=c.rotationX;t.rotationDegrees[1]=c.rotationY;t.rotationDegrees[2]=c.rotationZ;
  float local[16],pose[16];editorTransformMatrix(t,local);multiply(world,local,pose);
  editorColliderSegments(c,[&](const auto &a,const auto &b) {const float p[3]{a[0],a[1],a[2]},q[3]{b[0],b[1],b[2]};segment(out,pose,p,q);});
}
inline void forceArrow(ComponentVisual &out,const float *origin,float x,float y,float z) {
  const double length=std::sqrt(double(x)*x+double(y)*y+double(z)*z);
  if(length<1e-6) return;
  // Direction preview, not a scaled Newton metre: a large force must not make
  // the editor zoom out a kilometre. The inspector retains the exact units.
  const float size=std::clamp(static_cast<float>(std::log1p(length)*.35),.3f,2.f);
  const float direction[]{x/static_cast<float>(length),y/static_cast<float>(length),z/static_cast<float>(length)};
  ComponentVisualSegment line;
  for(u32 k=0;k<3;++k) {line.a[k]=origin[k];line.b[k]=origin[k]+direction[k]*size;}
  out.segments.push_back(line);
  float side[3]{-direction[2],0,direction[0]};
  float norm=std::sqrt(side[0]*side[0]+side[2]*side[2]);
  if(norm<.001f) {side[0]=1;side[1]=side[2]=0;norm=1;}
  for(float sign:{-1.f,1.f}) {
    ComponentVisualSegment head;
    for(u32 k=0;k<3;++k) {head.a[k]=line.b[k];head.b[k]=line.b[k]-direction[k]*size*.2f+side[k]/norm*size*.12f*sign;}
    out.segments.push_back(head);
  }
}
inline void constantForce(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,
                          float,bool detail,ComponentVisual &out) {
  const auto &f=static_cast<const scene::ConstantForce&>(value);out.enabled=f.enabled;
  if(!detail || !f.valid()) return;
  float pose[16];if(!opticalFrame(world,pose)) return;
  const float x=f.forceX+pose[0]*f.relativeForceX+pose[4]*f.relativeForceY+pose[8]*f.relativeForceZ;
  const float y=f.forceY+pose[1]*f.relativeForceX+pose[5]*f.relativeForceY+pose[9]*f.relativeForceZ;
  const float z=f.forceZ+pose[2]*f.relativeForceX+pose[6]*f.relativeForceY+pose[10]*f.relativeForceZ;
  forceArrow(out,world+12,x,y,z);
  // Torque is drawn from a separate raised origin with a small ring, so its
  // axis is distinguishable from linear propulsion, including torque-only.
  const float tx=f.torqueX+pose[0]*f.relativeTorqueX+pose[4]*f.relativeTorqueY+pose[8]*f.relativeTorqueZ;
  const float ty=f.torqueY+pose[1]*f.relativeTorqueX+pose[5]*f.relativeTorqueY+pose[9]*f.relativeTorqueZ;
  const float tz=f.torqueZ+pose[2]*f.relativeTorqueX+pose[6]*f.relativeTorqueY+pose[10]*f.relativeTorqueZ;
  if(tx!=0 || ty!=0 || tz!=0) {
    float origin[3]{world[12],world[13]+.2f,world[14]};
    forceArrow(out,origin,tx,ty,tz);
    float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;
    std::copy(origin,origin+3,identity+12);ring(out,identity,.15f,0,1);
  }
}
inline void constraint(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,
                       float,bool detail,ComponentVisual &out) {
  for(const auto &p:value.type().booleans) if(p.id=="enabled") out.enabled=p.read(value);
  if(!detail) return;
  float pose[16];if(!opticalFrame(world,pose)) return;
  const float origin[3]{};
  for(u32 axis=0;axis<3;++axis) {
    float tip[3]{};tip[axis]=.35f;segment(out,pose,origin,tip);
  }
}
template<class Connection> inline void physicsConnection(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *,
    float,bool,ComponentVisual &out) {
  const auto &connection=static_cast<const Connection&>(value);out.enabled=connection.enabled&&connection.action!=0;
}
inline void environment(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,
                        float,bool detail,ComponentVisual &out) {
  const auto &environment=static_cast<const scene::Environment&>(value);
  out.enabled=environment.values.active;
  if(!detail||!environment.valid()||environment.shape==renderer::EnvironmentVolumeShape::Global) return;
  if(environment.shape==renderer::EnvironmentVolumeShape::Sphere) {
    for(u32 axis=0;axis<3;++axis) ring(out,world,environment.sphereRadius,0,axis);
    return;
  }
  float corners[8][3];
  for(u32 i=0;i<8;++i) for(u32 axis=0;axis<3;++axis)
    corners[i][axis]=((i>>axis)&1?1.f:-1.f)*environment.boxSize[axis]*.5f;
  constexpr u32 edges[12][2]{{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
  for(const auto &edge:edges) segment(out,world,corners[edge[0]],corners[edge[1]]);
}
inline void collider2D(const scene::ComponentValue&value,const EditorEntity&,const EditorMapScene*,const float*world,float,bool detail,ComponentVisual&out){
 if(!detail||!value.valid()){return;}const auto&c=static_cast<const scene::Collider2D&>(value);float pose[16];std::copy(world,world+16,pose);for(u32 a=0;a<3;++a)pose[12+a]+=world[a]*c.offsetX+world[4+a]*c.offsetY;
 if(c.shape==scene::Collider2DShape::Box){float p[4][3]{{-c.halfX,-c.halfY,0},{c.halfX,-c.halfY,0},{c.halfX,c.halfY,0},{-c.halfX,c.halfY,0}};for(u32 a=0;a<4;++a)segment(out,pose,p[a],p[(a+1)%4]);}
 else if(c.shape==scene::Collider2DShape::Circle)ring(out,pose,c.radius,0,2);
 else {for(u32 half=0;half<2;++half)for(u32 a=0;a<24;++a){const float angle=(half?3.14159265f:0)+a*3.14159265f/24,next=angle+3.14159265f/24,center=half?-c.capsuleHalfLength:c.capsuleHalfLength;float p[3]{c.radius*std::cos(angle),center+c.radius*std::sin(angle),0},q[3]{c.radius*std::cos(next),center+c.radius*std::sin(next),0};segment(out,pose,p,q);}for(float sign:{-1.f,1.f}){float p[3]{sign*c.radius,-c.capsuleHalfLength,0},q[3]{sign*c.radius,c.capsuleHalfLength,0};segment(out,pose,p,q);}}
}
inline void force2D(const scene::ComponentValue&value,const EditorEntity&,const EditorMapScene*,const float*world,float,bool detail,ComponentVisual&out){const auto&c=static_cast<const scene::ConstantForce2D&>(value);out.enabled=c.enabled;if(!detail||!c.valid())return;float pose[16];if(!opticalFrame(world,pose))return;forceArrow(out,world+12,c.forceX+pose[0]*c.relativeForceX+pose[4]*c.relativeForceY,c.forceY+pose[1]*c.relativeForceX+pose[5]*c.relativeForceY,0);if(c.torque!=0)ring(out,pose,.2f,0,2);}
inline void audioSource(const scene::ComponentValue&value,const EditorEntity&,const EditorMapScene*,const float*world,float,bool detail,ComponentVisual&out){const auto&c=static_cast<const scene::AudioSource&>(value);out.enabled=c.enabled&&!c.mute;if(!detail||!c.valid()||c.dimension!=scene::AudioDimension::Spatial)return;float pose[16];if(!opticalFrame(world,pose))return;for(u32 a=0;a<3;++a){ring(out,pose,c.minDistance,0,a);ring(out,pose,c.maxDistance,0,a);}const float origin[3]{};for(float angle:{c.coneInner,c.coneOuter}){if(angle>=360)continue;const float half=angle*.00872664626f,r=c.maxDistance*std::sin(half),depth=c.maxDistance*std::cos(half);ring(out,pose,r,depth,2);for(u32 a=0;a<4;++a){float tip[3]{r*std::cos(a*1.57079633f),r*std::sin(a*1.57079633f),depth};segment(out,pose,origin,tip);}}}
inline void listener(const scene::ComponentValue&value,const EditorEntity&e,const EditorMapScene*r,const float*world,float aspect,bool detail,ComponentVisual&out){constraint(value,e,r,world,aspect,detail,out);if(detail){float pose[16];if(opticalFrame(world,pose)){const float origin[3]{},forward[3]{0,0,.8f};segment(out,pose,origin,forward);}}}
}
inline void pathVisual(const scene::ComponentValue&value,const EditorEntity&,const EditorMapScene*,const float*world,float,bool detail,ComponentVisual&out) {
  if(!detail)return;
  const auto &path=static_cast<const scene::Path&>(value);
  // One selected curve cache; bounded ownership, invalidated by authoring data.
  static resources::BakedCurve3D baked;static u64 last=0;
  u64 key=1469598103934665603ull;auto hash=[&](u64 n){key=(key^n)*1099511628211ull;};
  hash(value.instanceId());hash(path.curve.closed);hash(path.curve.points.size());
  for(u32 n=0;n<16;++n)hash(std::bit_cast<u32>(world[n]));
  for(float n:path.curve.up)hash(std::bit_cast<u32>(n));
  for(const auto&p:path.curve.points){hash(p.id);hash(std::bit_cast<u32>(p.rollDegrees));for(const auto*v:{&p.position,&p.in,&p.out})for(float n:*v)hash(std::bit_cast<u32>(n));}
  if(key!=last){std::array<float,16>matrix;std::copy_n(world,16,matrix.begin());if(!baked.bake(resources::transformedCurve3D(path.curve,matrix),.01))return;last=key;}
  float identity[16]{};for(u32 n=0;n<4;++n)identity[n*5]=1;
  const auto &samples=baked.samples();const usize steps=std::min<usize>(512,samples.size()>0?samples.size()-1:0);
  for(usize n=0;n<steps;++n){const usize a=n*(samples.size()-1)/steps,b=(n+1)*(samples.size()-1)/steps;visual_detail::segment(out,identity,samples[a].position.data(),samples[b].position.data());}
  for(const auto&p:path.curve.points)for(u32 a=0;a<3;++a){auto lo=p.position,hi=p.position;lo[a]-=.08f;hi[a]+=.08f;visual_detail::segment(out,world,lo.data(),hi.data());}
  // Bounded orientation markers from the same baked frame/roll as the consumer.
  if(baked.length()>1e-12)for(u32 n=0;n<=12;++n){resources::BakedCurve3D::Frame frame;if(!baked.sampleFrame(baked.length()*n/12.,frame))continue;auto tip=frame.position,wing=frame.position;for(u32 a=0;a<3;++a){tip[a]+=.28f*frame.up[a];wing[a]=tip[a]-.07f*frame.up[a]+.05f*frame.right[a];}visual_detail::segment(out,identity,frame.position.data(),tip.data());visual_detail::segment(out,identity,wing.data(),tip.data());for(u32 a=0;a<3;++a)wing[a]=tip[a]-.07f*frame.up[a]-.05f*frame.right[a];visual_detail::segment(out,identity,wing.data(),tip.data());}
}
inline void physicsFieldVisual(const scene::ComponentValue &value,const EditorEntity &entity,const EditorMapScene *,const float *world,float,bool detail,ComponentVisual &out){
  const auto &field=static_cast<const scene::PhysicsFieldProperties&>(value);out.enabled=field.enabled;
  float frame[16];std::copy(world,world+16,frame);for(u32 a=0;a<3;++a){for(u32 c=0;c<3;++c)frame[12+a]+=world[c*4+a]*field.offset[c];out.origin[a]=frame[12+a];}
  if(!detail)return;
  scene::Collider shape;shape.shape=field.shape==0?scene::ColliderShape::Box:scene::ColliderShape::Sphere;shape.halfX=field.halfExtents[0];shape.halfY=field.halfExtents[1];shape.halfZ=field.halfExtents[2];shape.radius=field.radius;
  editorColliderSegments(shape,[&](const auto &a,const auto &b){visual_detail::segment(out,frame,a.data(),b.data());});
  const int kind=scene::physicsFieldKind(value);
  if(kind==0||kind==1){
    // Same normalized optical frame as runtime: volume scaling must not amplify the effect vector.
    float pose[16];if(!visual_detail::opticalFrame(frame,pose))return;
    const float length=std::sqrt(field.vector[0]*field.vector[0]+field.vector[1]*field.vector[1]+field.vector[2]*field.vector[2]);
    if(length>1e-6f){float origin[3]{},tip[3];for(u32 a=0;a<3;++a)tip[a]=field.vector[a]/length*1.5f;visual_detail::segment(out,pose,origin,tip);out.segments.back().emphasis=1;visual_detail::ring(out,pose,.12f,0,1);}
  }else if(kind==3){
    for(u32 axis=0;axis<3;++axis)for(float sign:{-1.f,1.f}){float a[3]{},b[3]{};a[axis]=sign*(field.acceleration<0?1.2f:.2f);b[axis]=sign*(field.acceleration<0?.2f:1.2f);visual_detail::segment(out,frame,a,b);out.segments.back().emphasis=1;}
  }
  (void)entity;
}
inline void physicsField2DVisual(const scene::ComponentValue &value,const EditorEntity &,const EditorMapScene *,const float *world,float,bool detail,ComponentVisual &out){
 const auto &field=static_cast<const scene::PhysicsFieldProperties&>(value);out.enabled=field.enabled;
 float frame[16];std::copy_n(world,16,frame);for(u32 a=0;a<3;++a){frame[12+a]+=world[a]*field.offset[0]+world[4+a]*field.offset[1];out.origin[a]=frame[12+a];}
 if(!detail)return;
 if(field.shape==1)visual_detail::ring(out,frame,field.radius,0,2);
 else {const float points[4][3]{{-field.halfExtents[0],-field.halfExtents[1],0},{field.halfExtents[0],-field.halfExtents[1],0},{field.halfExtents[0],field.halfExtents[1],0},{-field.halfExtents[0],field.halfExtents[1],0}};for(u32 n=0;n<4;++n)visual_detail::segment(out,frame,points[n],points[(n+1)%4]);}
 const int kind=scene::physicsField2DKind(value);
 if(kind==0||kind==1){const float length=std::hypot(field.vector[0],field.vector[1]);if(length>1e-6f){float pose[16]{};const float norm=std::hypot(frame[0],frame[1]);if(norm<1e-6f)return;pose[0]=frame[0]/norm;pose[1]=frame[1]/norm;pose[4]=-pose[1];pose[5]=pose[0];pose[10]=pose[15]=1;std::copy_n(frame+12,3,pose+12);const float origin[3]{},tip[3]{field.vector[0]/length*1.5f,field.vector[1]/length*1.5f,0};visual_detail::segment(out,pose,origin,tip);out.segments.back().emphasis=1;}}
 else if(kind==3){for(u32 axis=0;axis<2;++axis)for(float sign:{-1.f,1.f}){float a[3]{},b[3]{};a[axis]=sign*(field.acceleration<0?1.2f:.2f);b[axis]=sign*(field.acceleration<0?.2f:1.2f);visual_detail::segment(out,frame,a,b);out.segments.back().emphasis=1;}}
}
inline const std::array<ComponentVisualProvider,34> componentVisualProviders{{
  {&scene::GravityField2D::descriptor,ui::UiIcon::PhysicsFieldGravity2d,true,physicsField2DVisual},
  {&scene::WindField2D::descriptor,ui::UiIcon::PhysicsFieldWind2d,true,physicsField2DVisual},
  {&scene::DragField2D::descriptor,ui::UiIcon::PhysicsFieldDrag2d,true,physicsField2DVisual},
  {&scene::RadialField2D::descriptor,ui::UiIcon::PhysicsFieldRadial2d,true,physicsField2DVisual},
  {&scene::GravityField::descriptor,ui::UiIcon::PhysicsFieldGravity,true,physicsFieldVisual},
  {&scene::WindField::descriptor,ui::UiIcon::PhysicsFieldWind,true,physicsFieldVisual},
  {&scene::DragField::descriptor,ui::UiIcon::PhysicsFieldDrag,true,physicsFieldVisual},
  {&scene::RadialField::descriptor,ui::UiIcon::PhysicsFieldRadial,true,physicsFieldVisual},
  {&scene::Character::descriptor,ui::UiIcon::PhysicsCharacterGround,true,visual_detail::characterGround},
  {&scene::PhysicsEventConnection3D::descriptor,ui::UiIcon::EventPhysicsConnection,true,visual_detail::physicsConnection<scene::PhysicsEventConnection3D>},
  {&scene::PhysicsEventConnection2D::descriptor,ui::UiIcon::EventPhysicsConnection2d,true,visual_detail::physicsConnection<scene::PhysicsEventConnection2D>},
  {&scene::EventConnection::descriptor,ui::UiIcon::ComponentEventConnection,true,visual_detail::physicsConnection<scene::EventConnection>},
  {&scene::Path::descriptor,ui::UiIcon::PathCurve,true,pathVisual},
  {&scene::PathFollow::descriptor,ui::UiIcon::ComponentPathFollow,true,visual_detail::constraint},
  {&scene::ParentConstraint::descriptor,ui::UiIcon::ComponentParentConstraint,true,visual_detail::constraint},
  {&scene::LookAtConstraint::descriptor,ui::UiIcon::ComponentLookAtConstraint,true,visual_detail::constraint},
  {&scene::TransformTween::descriptor,ui::UiIcon::ComponentTweenTransform,true,visual_detail::constraint},
  {&scene::Collider2D::descriptor,ui::UiIcon::PhysicsCollider2d,false,visual_detail::collider2D},
  {&scene::Joint2D::descriptor,ui::UiIcon::PhysicsJoint2d,true,visual_detail::constraint},
  {&scene::ConstantForce2D::descriptor,ui::UiIcon::PhysicsConstantForce2d,true,visual_detail::force2D},
  {&scene::AudioSource::descriptor,ui::UiIcon::AudioSource,true,visual_detail::audioSource},
  {&scene::AudioListener::descriptor,ui::UiIcon::AudioListener,true,visual_detail::listener},
  {&scene::SpringPositionConstraint::descriptor,ui::UiIcon::ComponentSpringPosition,true,visual_detail::constraint},
  {&scene::SpringRotationConstraint::descriptor,ui::UiIcon::ComponentSpringRotation,true,visual_detail::constraint},
  {&scene::SpringScaleConstraint::descriptor,ui::UiIcon::ComponentSpringScale,true,visual_detail::constraint},
  {&scene::PositionConstraint::descriptor,ui::UiIcon::ComponentPositionConstraint,true,visual_detail::constraint},
  {&scene::RotationConstraint::descriptor,ui::UiIcon::ComponentRotationConstraint,true,visual_detail::constraint},
  {&scene::ScaleConstraint::descriptor,ui::UiIcon::ComponentScaleConstraint,true,visual_detail::constraint},
  {&scene::AimConstraint::descriptor,ui::UiIcon::ComponentAimConstraint,true,visual_detail::constraint},
  {&scene::ConstantForce::descriptor,ui::UiIcon::ComponentConstantForce,true,visual_detail::constantForce},
  {&scene::Camera::descriptor,ui::UiIcon::EditorAuthorCamera,true,visual_detail::camera},
  {&scene::Light::descriptor,ui::UiIcon::EditorAuthorSun,true,visual_detail::light},
  {&scene::Collider::descriptor,ui::UiIcon::ComponentCollider,false,visual_detail::collider},
  {&scene::Environment::descriptor,ui::UiIcon::EditorAuthorObject,false,visual_detail::environment}
}};
// Match mesh viewport visibility/picking. Hidden entities have no visual or hit;
// pick-off entities can remain drawn but never gain a marker selection bypass.
inline bool componentVisualVisible(const runtime::SceneGraph &document,EditorEntityId id,
    u32 hiddenLayers,std::span<const EditorEntityId> hidden){
  const auto *entity=document.find(id);
  if(!entity)return false;
  if((entity->layer<32&&(hiddenLayers&(1u<<entity->layer)))||
     std::find(hidden.begin(),hidden.end(),id)!=hidden.end())return false;
  for(auto *parent=entity;parent;parent=document.find(parent->parent))if(!parent->visible)return false;
  return true;
}
inline bool componentVisualSelectable(const runtime::SceneGraph &document,EditorEntityId id,
    u32 hiddenLayers,u32 unpickableLayers,std::span<const EditorEntityId> hidden,
    std::span<const EditorEntityId> pickOff){
  if(!componentVisualVisible(document,id,hiddenLayers,hidden))return false;
  const auto *entity=document.find(id);
  if((entity->layer<32&&(unpickableLayers&(1u<<entity->layer)))||
     std::find(pickOff.begin(),pickOff.end(),id)!=pickOff.end())return false;
  for(auto *parent=entity;parent;parent=document.find(parent->parent))if(!parent->active)return false;
  return true;
}
// Nearest authored Collider surface within the active object's component
// collection. A disabled part is still editable. Resource absence is a miss,
// never a renderer-bounds fallback; depth compares world-ray parameters.
// A component UID is local to its object. Keep the physical body and the
// authoring object separate; never infer ownership from hierarchy alone.
inline bool colliderBelongsToBody(const runtime::SceneGraph &document,EditorEntityId id,
    const scene::Collider &collider,EditorEntityId body) {
  const auto *object=document.find(body);
  const auto *value=object?object->components.find(scene::PhysicsBody::descriptor):nullptr;
  if(!value || &value->type()!=&scene::PhysicsBody::descriptor)return false;
  return (collider.owner?collider.owner:id)==body &&
      runtime::referenceAccepts(document,id,scene::colliderReferences[0],collider.owner,true);
}
inline EditorEntityId colliderInspectionBody(const runtime::SceneGraph &document,
    EditorEntityId selected,u64 instance) {
  const auto *object=document.find(selected);
  const auto *value=object?object->components.findInstance(instance):nullptr;
  if(!value)return 0;
  if(&value->type()==&scene::PhysicsBody::descriptor)return selected;
  if(&value->type()!=&scene::Collider::descriptor)return 0;
  const auto &c=static_cast<const scene::Collider&>(*value);
  if(c.owner>std::numeric_limits<EditorEntityId>::max())return 0;
  const auto body=c.owner?static_cast<EditorEntityId>(c.owner):selected;
  return colliderBelongsToBody(document,selected,c,body)?body:0;
}
// The authoring object's visual mesh is not an obstacle to editing its own
// shape. Other objects occlude only through real triangles, never bounds.
inline bool colliderPointOccluded(std::span<const EditorPickCandidate> occluders,
    const EditorViewport &view,ui::UiPoint point,EditorEntityId authored,float depth) {
  if(occluders.empty())return false;
  const auto hit=pickNearest(occluders,screenPointToRay(view,point),authored,true);
  return hit.hit && hit.distance<depth-std::max(.0001f,std::abs(depth)*.00001f);
}
// Recover the world depth at the closest *projected* point on an outline.
// Screen interpolation is not world interpolation under perspective. Using
// the ray through that pixel also handles clipped endpoints and orthographic views.
inline bool colliderContourWorldDepth(const EditorViewport &view,const ComponentVisualSegment &line,
    ui::UiPoint screen,float &depth) {
  const auto ray=screenPointToRay(view,screen);if(!ray.valid)return false;
  double vv=0,rv=0,ra=0,va=0;
  for(u32 i=0;i<3;++i){const double v=double(line.b[i])-line.a[i],a=double(line.a[i])-ray.origin[i];
    vv+=v*v;rv+=ray.direction[i]*v;ra+=ray.direction[i]*a;va+=v*a;}
  const double denominator=vv-rv*rv;
  double distance;
  if(denominator>vv*1e-10)
    distance=ra+rv*std::clamp((rv*ra-va)/denominator,0.,1.);
  else distance=std::max(double(ray.minimumDistance),std::min(ra,ra+rv));
  const double epsilon=std::max(.0001,double(ray.maximumDistance)*1e-6);
  if(!std::isfinite(distance)||distance<ray.minimumDistance-epsilon||distance>ray.maximumDistance+epsilon)return false;
  depth=static_cast<float>(std::clamp(distance,double(ray.minimumDistance),double(ray.maximumDistance)));return true;
}
inline u64 pickColliderSurface(const runtime::SceneGraph &document,EditorEntityId id,
    const EditorViewport &view,const EditorMapScene *resources,ui::UiPoint point,
    EditorEntityId body=0,float *hitDistance=nullptr,std::span<const EditorPickCandidate> occluders={}) {
  if(!view.rect.contains(point))return 0;
  const auto ray=screenPointToRay(view,point);if(!ray.valid)return 0;
  const auto *entity=document.find(id);float world[16];
  if(!entity || !editorWorldMatrix(document,id,world))return 0;
  float nearest=ray.maximumDistance;u64 instance=0;
  for(usize i=0;i<entity->components.size();++i) {
    const auto *value=entity->components.at(i);
    if(&value->type()!=&scene::Collider::descriptor)continue;
    const auto &c=static_cast<const scene::Collider&>(*value);if(!c.valid())continue;
    if(body && !colliderBelongsToBody(document,id,c,body))continue;
    float pose[16];std::copy(world,world+16,pose);
    if(c.shape!=scene::ColliderShape::Mesh || c.meshLocalPose) {
      EditorTransform t;t.position[0]=c.centerX;t.position[1]=c.centerY;t.position[2]=c.centerZ;
      t.rotationDegrees[0]=c.rotationX;t.rotationDegrees[1]=c.rotationY;t.rotationDegrees[2]=c.rotationZ;
      float local[16];editorTransformMatrix(t,local);runtime::multiplyMatrix(world,local,pose);
    }
    float depth;bool hit=false;
    if(c.shape==scene::ColliderShape::Mesh) {
      if(resources)hit=resources->intersectColliderMesh(visual_detail::meshColliderSlots(c,*entity,*resources),c.convex,c.hullTolerance,ray,pose,depth);
    } else hit=intersectColliderPrimitive(c,ray,pose,depth);
    if(hit && !colliderPointOccluded(occluders,view,point,id,depth) &&
       (depth<nearest || (depth==nearest && (!instance || value->instanceId()<instance)))) {
      nearest=depth;instance=value->instanceId();
    }
  }
  if(instance && hitDistance)*hitDistance=nearest;
  return instance;
}
// Select the actual drawn collider contour, not the renderer or a bounds proxy.
// Lines remain useful x-ray diagnostics. Their selection still respects visible
// scene geometry at the nearest point within the touch tolerance.
// A tap is cold work: only this object's colliders build their cached previews.
inline u64 pickColliderContour(const runtime::SceneGraph &document,EditorEntityId id,
    const EditorViewport &view,const EditorMapScene *resources,ui::UiPoint point,
    float tolerance=8.f,EditorEntityId body=0,float *hitDistance=nullptr,
    std::span<const EditorPickCandidate> occluders={}) {
  if(!view.rect.contains(point)||!std::isfinite(point.x)||!std::isfinite(point.y)||
     !std::isfinite(tolerance)||tolerance<=0)return 0;
  const auto *entity=document.find(id);float world[16];
  if(!entity||!editorWorldMatrix(document,id,world))return 0;
  double nearest=double(tolerance)*tolerance;u64 instance=0;
  for(usize i=0;i<entity->components.size();++i) {
    const auto *value=entity->components.at(i);
    if(&value->type()!=&scene::Collider::descriptor)continue;
    if(body && !colliderBelongsToBody(document,id,static_cast<const scene::Collider&>(*value),body))continue;
    ComponentVisual visual;visual_detail::collider(*value,*entity,resources,world,1,true,visual);
    for(const auto &line:visual.segments) {
      ui::UiPoint a,b;if(!projectSegmentToScreen(view,line.a,line.b,a,b))continue;
      const double x=double(b.x)-a.x,y=double(b.y)-a.y,length=x*x+y*y;
      const double t=length>0?std::clamp(((double(point.x)-a.x)*x+(double(point.y)-a.y)*y)/length,0.,1.):0.;
      const double dx=double(point.x)-a.x-t*x,dy=double(point.y)-a.y-t*y,distance=dx*dx+dy*dy;
      // Persistent identity resolves coincident contours independently of order.
      if(distance<nearest||(distance==nearest&&(!instance||value->instanceId()<instance))) {
        const ui::UiPoint closest{static_cast<float>(a.x+t*x),static_cast<float>(a.y+t*y)};
        float depth;
        if(!colliderContourWorldDepth(view,line,closest,depth) ||
           colliderPointOccluded(occluders,view,closest,id,depth))continue;
        nearest=distance;instance=value->instanceId();
      }
    }
  }
  if(instance && hitDistance)*hitDistance=static_cast<float>(nearest);
  return instance;
}
struct ColliderVisualHit {EditorEntityId entity=0;u64 instance=0;bool surface=false;};
inline ColliderVisualHit pickInspectedCollider(const runtime::SceneGraph &document,
    EditorEntityId selected,u64 inspected,const EditorViewport &view,const EditorMapScene *resources,
    ui::UiPoint point,u32 hiddenLayers=0,u32 unpickableLayers=0,
    std::span<const EditorEntityId> hidden={},std::span<const EditorEntityId> pickOff={},
    std::span<const EditorPickCandidate> occluders={}) {
  const auto body=colliderInspectionBody(document,selected,inspected);
  std::vector<EditorEntityId> ids;
  if(body)document.collectSubtree(body,ids);else ids.push_back(selected);
  // All surfaces take priority over x-ray contours, including across objects.
  for(bool surface:{true,false}) {
    float nearest=std::numeric_limits<float>::max();ColliderVisualHit result;
    for(const auto id:ids) {
      if(!componentVisualSelectable(document,id,hiddenLayers,unpickableLayers,hidden,pickOff))continue;
      float distance=0;
      const auto instance=surface?pickColliderSurface(document,id,view,resources,point,body,&distance,occluders):
          pickColliderContour(document,id,view,resources,point,8.f,body,&distance,occluders);
      if(instance && (distance<nearest || (distance==nearest &&
          (!result.entity || id<result.entity || (id==result.entity && instance<result.instance))))) {
        nearest=distance;result={id,instance,surface};
      }
    }
    if(result.entity)return result;
  }
  return {};
}
inline std::vector<ComponentVisual> collectComponentVisuals(const runtime::SceneGraph &document,
    EditorEntityId selected,float aspect,const EditorMapScene *resources=nullptr,u64 selectedPathInstance=0,u64 selectedPathPoint=0,EditorEntityId selectedPathEntity=0,u64 editedConnectionInstance=0) {
  std::vector<ComponentVisual> result;std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  // A finite diagnostic drawing budget never truncates the physics/picking
  // geometry. Give the explicitly inspected object first use of the budget.
  if(const auto found=std::find(ids.begin(),ids.end(),selected);found!=ids.end())std::rotate(ids.begin(),found,found+1);
  usize colliderSegmentsUsed=0;
  EditorEntityId referencedPath=0;
  const auto inspectedBody=colliderInspectionBody(document,selected,editedConnectionInstance);
  if(const auto*object=document.find(selected))for(usize n=0;n<object->components.size();++n)if(object->components.at(n)->type().id==scene::PathFollow::descriptor.id)
    referencedPath=static_cast<EditorEntityId>(static_cast<const scene::PathFollow&>(*object->components.at(n)).target);
  for(auto id:ids) {
    const auto *entity=document.find(id);if(!entity||id==document.root()) continue;
    bool visible=true;
    for(auto p=entity;p;p=document.find(p->parent)) if(!p->visible) {visible=false;break;}
    if(!visible) continue;
    float world[16];if(!editorWorldMatrix(document,id,world)) continue;
    u32 markerIndex=0;
    for(usize i=0;i<entity->components.size();++i) {
      const auto *value=entity->components.at(i);
      const bool linkedCollider=inspectedBody && &value->type()==&scene::Collider::descriptor &&
          colliderBelongsToBody(document,id,static_cast<const scene::Collider&>(*value),inspectedBody);
      if(inspectedBody && &value->type()==&scene::Collider::descriptor && !linkedCollider)continue;
      for(const auto &provider:componentVisualProviders) if(&value->type()==provider.type && (provider.marker||id==selected||linkedCollider)) {
        ComponentVisual v;v.entity=id;v.instance=value->instanceId();v.icon=provider.icon;v.marker=provider.marker;
        if(v.marker) v.markerIndex=markerIndex++;
        const bool collider=&value->type()==&scene::Collider::descriptor;
        const bool detail=(id==selected||linkedCollider||id==referencedPath||id==selectedPathEntity)&&(!collider||colliderSegmentsUsed<MaximumVisibleColliderSegments);
        std::copy(world+12,world+15,v.origin);provider.build(*value,*entity,resources,world,aspect,detail,v);
        if(collider){if(v.segments.size()>MaximumVisibleColliderSegments-colliderSegmentsUsed)v.segments.resize(MaximumVisibleColliderSegments-colliderSegmentsUsed);colliderSegmentsUsed+=v.segments.size();}
        if(id==selected&&value->type().id==scene::Collider::descriptor.id&&value->instanceId()==editedConnectionInstance)
          for(auto &line:v.segments)line.emphasis=1;
        if(id==selected&&value->type().id==scene::PathFollow::descriptor.id&&referencedPath){float source[16];if(editorWorldMatrix(document,referencedPath,source)){ComponentVisualSegment link;std::copy(world+12,world+15,link.a);std::copy(source+12,source+15,link.b);link.emphasis=1;v.segments.push_back(link);}}
        if(id==(selectedPathEntity?selectedPathEntity:selected)&&value->type().id==scene::Path::descriptor.id&&value->instanceId()==selectedPathInstance) {
          const auto *point=static_cast<const scene::Path&>(*value).point(selectedPathPoint);
          if(point){
            for(const auto *offset:{&point->in,&point->out}){auto tip=point->position;for(u32 a=0;a<3;++a)tip[a]+=(*offset)[a];visual_detail::segment(v,world,point->position.data(),tip.data());v.segments.back().emphasis=1;for(u32 a=0;a<3;++a){auto lo=tip,hi=tip;lo[a]-=.1f;hi[a]+=.1f;visual_detail::segment(v,world,lo.data(),hi.data());v.segments.back().emphasis=1;}}
            for(u32 a=0;a<3;++a){auto lo=point->position,hi=point->position;lo[a]-=.16f;hi[a]+=.16f;visual_detail::segment(v,world,lo.data(),hi.data());v.segments.back().emphasis=2;}
          }
        }
        if(id==selected && (value->type().id.starts_with("astra.constraint.")||value->type().id.starts_with("astra.spring."))) {
          for(const auto &reference:value->type().references) if(reference.id=="target") {
            float source[16];const auto target=reference.read(*value);
            if(target && target<=std::numeric_limits<u32>::max() && editorWorldMatrix(document,static_cast<EditorEntityId>(target),source)) {
              ComponentVisualSegment link;
              std::copy(world+12,world+15,link.a);std::copy(source+12,source+15,link.b);v.segments.push_back(link);
            }
          }
        }
        if(id==selected&&(value->type().id==scene::PhysicsEventConnection3D::descriptor.id||value->type().id==scene::PhysicsEventConnection2D::descriptor.id)) {
          const bool is2D=value->type().id==scene::PhysicsEventConnection2D::descriptor.id;
          const auto action=is2D?static_cast<const scene::PhysicsEventConnection2D&>(*value).action:static_cast<const scene::PhysicsEventConnection3D&>(*value).action;
          const auto receiver=is2D?static_cast<const scene::PhysicsEventConnection2D&>(*value).receiver:static_cast<const scene::PhysicsEventConnection3D&>(*value).receiver;
          const bool enabled=is2D?static_cast<const scene::PhysicsEventConnection2D&>(*value).enabled:static_cast<const scene::PhysicsEventConnection3D&>(*value).enabled;
          v.enabled=enabled&&action!=0;
          // Replace the generic axes with a contextual receiver link only while this instance is edited.
          v.segments.clear();
          if(action&&value->instanceId()==editedConnectionInstance&&receiver) {
            float target[16];
            if(editorWorldMatrix(document,static_cast<EditorEntityId>(receiver),target)) {
              ComponentVisualSegment link;std::copy(world+12,world+15,link.a);std::copy(target+12,target+15,link.b);
              link.emphasis=1;v.segments.push_back(link);visual_detail::ring(v,target,.15f,0,1);
            }
          }
        }
        if(id==selected&&&value->type()==&scene::EventConnection::descriptor) {
          // Mesma linha contextual das conexões físicas: só da instância em edição
          // e só quando o receptor é outro objeto (vazio significa o próprio emissor).
          const auto &connection=scene::eventConnection(*value);
          v.segments.clear();
          if(connection.action&&connection.receiver&&value->instanceId()==editedConnectionInstance) {
            float target[16];
            if(editorWorldMatrix(document,static_cast<EditorEntityId>(connection.receiver),target)) {
              ComponentVisualSegment link;std::copy(world+12,world+15,link.a);std::copy(target+12,target+15,link.b);
              link.emphasis=1;v.segments.push_back(link);visual_detail::ring(v,target,.15f,0,1);
            }
          }
        }
        if(id==selected&&value->type().id==scene::Joint2D::descriptor.id){const auto&joint=static_cast<const scene::Joint2D&>(*value);float identity[16]{};for(u32 a=0;a<4;++a)identity[a*5]=1;float a[3]{joint.anchorAX,joint.anchorAY,0},b[3]{joint.anchorBX,joint.anchorBY,0};ComponentVisual temporary;visual_detail::segment(temporary,world,a,a);float target[16];bool valid=joint.worldAnchor;if(joint.worldAnchor){std::copy(identity,identity+16,target);target[14]=world[14];}else if(joint.target<=std::numeric_limits<u32>::max())valid=editorWorldMatrix(document,static_cast<EditorEntityId>(joint.target),target);if(valid){ComponentVisual other;visual_detail::segment(other,target,b,b);ComponentVisualSegment link=temporary.segments.front();std::copy(other.segments.front().a,other.segments.front().a+3,link.b);v.segments.push_back(link);for(const auto*p:{link.a,link.b}){float frame[16];std::copy(identity,identity+16,frame);std::copy(p,p+3,frame+12);visual_detail::ring(v,frame,.12f,0,2);}}}
        if(id==selected&&value->type().id==scene::TransformTween::descriptor.id){const auto&c=static_cast<const scene::TransformTween&>(*value);auto destination=entity->transform;for(u32 a=0;a<3;++a){if(c.position)destination.position[a]=c.destination[a]+(c.relative?entity->transform.position[a]:0);if(c.rotation)destination.rotationDegrees[a]=c.destination[3+a]+(c.relative?entity->transform.rotationDegrees[a]:0);if(c.scale)destination.scale[a]=c.relative?entity->transform.scale[a]*c.destination[6+a]:c.destination[6+a];}float parent[16],local[16],goal[16];if(runtime::parentWorldMatrix(document,id,parent)){editorTransformMatrix(destination,local);visual_detail::multiply(parent,local,goal);ComponentVisualSegment link;std::copy(world+12,world+15,link.a);std::copy(goal+12,goal+15,link.b);v.segments.push_back(link);const float origin[3]{};for(u32 a=0;a<3;++a){float tip[3]{};tip[a]=.5f;visual_detail::segment(v,goal,origin,tip);}}}
        if(id==selected&&value->type().id==scene::TransformTween::descriptor.id) {
          const auto &tween=*static_cast<const scene::TransformTween*>(value);
          if(tween.finishedAction)v.icon=ui::UiIcon::EventTweenCompletion;
          if(tween.finishedAction&&tween.finishedTarget&&value->instanceId()==editedConnectionInstance) {
            float receiver[16];if(editorWorldMatrix(document,static_cast<EditorEntityId>(tween.finishedTarget),receiver)) {
              ComponentVisualSegment link;std::copy(world+12,world+15,link.a);std::copy(receiver+12,receiver+15,link.b);link.emphasis=1;v.segments.push_back(link);visual_detail::ring(v,receiver,.15f,0,1);
            }
          }
        }
        result.push_back(std::move(v));break;
      }
    }
  }
  return result;
}
}
