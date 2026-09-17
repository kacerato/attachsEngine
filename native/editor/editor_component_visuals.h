#pragma once
#include "editor/editor_component_catalog.h"
#include "editor/editor_collider_geometry.h"
#include "editor/editor_scene_camera.h"

namespace ae::editor {
// Derived editor data. These segments/icons never enter meshes, collision,
// shadows, serialization or exports. Providers consume the actual component.
struct ComponentVisualSegment {float a[3]{},b[3]{};};
struct ComponentVisual {
  EditorEntityId entity=0;
  u64 instance=0;
  ui::UiIcon icon=ui::UiIcon::EditorAuthorObject;
  bool enabled=true,marker=true;
  u32 markerIndex=0;
  float origin[3]{};
  std::vector<ComponentVisualSegment> segments;
};
using ComponentVisualBuilder=void (*)(const scene::ComponentValue &,const float *,float,bool,ComponentVisual &);
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
inline void camera(const scene::ComponentValue &value,const float *world,float aspect,bool detail,ComponentVisual &out) {
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
inline void light(const scene::ComponentValue &value,const float *world,float,bool detail,ComponentVisual &out) {
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
inline void collider(const scene::ComponentValue &value,const float *world,float,bool detail,ComponentVisual &out) {
  const auto &c=static_cast<const scene::Collider &>(value);out.enabled=c.enabled;if(!detail||!c.valid()) return;
  EditorTransform t;t.position[0]=c.centerX;t.position[1]=c.centerY;t.position[2]=c.centerZ;
  t.rotationDegrees[0]=c.rotationX;t.rotationDegrees[1]=c.rotationY;t.rotationDegrees[2]=c.rotationZ;
  float local[16],pose[16]{};editorTransformMatrix(t,local);
  for(u32 col=0;col<4;++col) for(u32 row=0;row<4;++row)
    for(u32 k=0;k<4;++k) pose[col*4+row]+=world[k*4+row]*local[col*4+k];
  editorColliderSegments(c,[&](const auto &a,const auto &b) {const float p[3]{a[0],a[1],a[2]},q[3]{b[0],b[1],b[2]};segment(out,pose,p,q);});
}
}
inline const std::array<ComponentVisualProvider,3> componentVisualProviders{{
  {&scene::Camera::descriptor,ui::UiIcon::EditorAuthorCamera,true,visual_detail::camera},
  {&scene::Light::descriptor,ui::UiIcon::EditorAuthorSun,true,visual_detail::light},
  {&scene::Collider::descriptor,ui::UiIcon::ComponentCollider,false,visual_detail::collider}
}};
inline std::vector<ComponentVisual> collectComponentVisuals(const EditorDocument &document,
    EditorEntityId selected,float aspect) {
  std::vector<ComponentVisual> result;std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *entity=document.find(id);if(!entity||id==document.root()) continue;
    bool visible=true;
    for(auto p=entity;p;p=document.find(p->parent)) if(!p->visible) {visible=false;break;}
    if(!visible) continue;
    float world[16];if(!editorWorldMatrix(document,id,world)) continue;
    u32 markerIndex=0;
    for(usize i=0;i<entity->components.size();++i) {
      const auto *value=entity->components.at(i);
      for(const auto &provider:componentVisualProviders) if(&value->type()==provider.type && (provider.marker||id==selected)) {
        ComponentVisual v;v.entity=id;v.instance=value->instanceId();v.icon=provider.icon;v.marker=provider.marker;
        if(v.marker) v.markerIndex=markerIndex++;
        std::copy(world+12,world+15,v.origin);provider.build(*value,world,aspect,id==selected,v);
        result.push_back(std::move(v));break;
      }
    }
  }
  return result;
}
}
