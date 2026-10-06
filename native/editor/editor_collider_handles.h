#pragma once
#include "editor/editor_camera_handles.h"
#include "editor/editor_gizmo.h"
#include "scene/collider.h"
#include "scene/component_properties.h"
#include "renderer/normal_matrix.h"

namespace ae::editor {
// Shape dimensions and local pose have distinct ownership from object TRS.
// One frozen frame is shared by drawing, hit testing and the complete gesture.
enum class ColliderHandleKind : u32 {
  SizePositiveX, SizeNegativeX, SizePositiveY, SizeNegativeY, SizePositiveZ, SizeNegativeZ,
  CenterX, CenterY, CenterZ, RotationX, RotationY, RotationZ, Count
};
struct ColliderHandle : EditorCameraHandle {
  ColliderHandleKind kind{};
  u64 instance=0;
  std::string_view property;
  float unitsPerProperty=1;
  float origin[3]{}, pose[16]{}, inverseTranspose[12]{}, radius=1;
  bool rotation=false;
};
inline const scene::Collider *inspectedCollider(const runtime::SceneGraph &document,
    EditorEntityId object,u64 instance) {
  const auto *e=document.find(object);const auto *c=e?e->components.findInstance(instance):nullptr;
  return c && &c->type()==&scene::Collider::descriptor?static_cast<const scene::Collider*>(c):nullptr;
}
inline bool colliderHandleMatchesTool(ColliderHandleKind kind,EditorGizmoMode tool) {
  const u32 index=static_cast<u32>(kind);
  return tool==EditorGizmoMode::Translate?index>=6&&index<9:
      tool==EditorGizmoMode::Rotate?index>=9&&index<12:tool==EditorGizmoMode::Scale&&index<6;
}
inline void colliderRingPoint(const ColliderHandle &handle,float angle,float out[3]) {
  const u32 axis=static_cast<u32>(handle.kind)-9,u=(axis+1)%3,v=(axis+2)%3;
  for(u32 i=0;i<3;++i)out[i]=handle.pose[12+i]+handle.radius*(
      handle.pose[u*4+i]*std::cos(angle)+handle.pose[v*4+i]*std::sin(angle));
}
inline bool colliderRingAngle(const EditorViewport &view,const ColliderHandle &handle,
    ui::UiPoint pixel,float &angle) {
  const auto ray=screenPointToRay(view,pixel);if(!ray.valid||!handle.rotation)return false;
  float origin[3]{},direction[3]{};
  for(u32 row=0;row<3;++row)for(u32 column=0;column<3;++column) {
    const float inverse=handle.inverseTranspose[row*4+column];
    origin[row]+=inverse*(ray.origin[column]-handle.pose[12+column]);
    direction[row]+=inverse*ray.direction[column];
  }
  const u32 axis=static_cast<u32>(handle.kind)-9,u=(axis+1)%3,v=(axis+2)%3;
  float normalLength=0;
  for(u32 i=0;i<3;++i)normalLength+=handle.inverseTranspose[axis*4+i]*handle.inverseTranspose[axis*4+i];
  if(std::abs(direction[axis])<.05f*std::sqrt(normalLength))return false;
  const float t=-origin[axis]/direction[axis];
  if(!std::isfinite(t)||t<ray.minimumDistance||t>ray.maximumDistance)return false;
  const float x=origin[u]+direction[u]*t,y=origin[v]+direction[v]*t;
  if(!std::isfinite(x)||!std::isfinite(y)||x*x+y*y<1e-10f)return false;
  angle=std::atan2(y,x);return true;
}
inline bool colliderHandleGeometry(const runtime::SceneGraph &document,EditorEntityId object,u64 instance,
    ColliderHandleKind kind,const EditorViewport &view,ColliderHandle &out) {
  const auto *c=inspectedCollider(document,object,instance);
  const u32 index=static_cast<u32>(kind);float world[16],normal[12];
  if(!c||!c->valid()||index>=12||!editorWorldMatrix(document,object,world)||
     !renderer::buildNormalMatrix(world,normal))return false;
  out={};out.kind=kind;out.instance=instance;
  const bool dimensions=index<6,rotation=index>=9;
  if(dimensions && c->shape==scene::ColliderShape::Mesh)return false;
  if(!dimensions && !scene::colliderHasLocalPose(*c))return false;
  EditorTransform local;
  local.position[0]=c->centerX;local.position[1]=c->centerY;local.position[2]=c->centerZ;
  for(u32 row=0;row<3;++row)out.origin[row]=world[12+row]+world[row]*c->centerX+
      world[4+row]*c->centerY+world[8+row]*c->centerZ;
  const auto projected=projectWorldToScreen(view,out.origin);if(!projected.valid)return false;
  const float worldLength=72.f*2.f*renderer::projectionHalfHeight(view.frustum)*
      renderer::projectionDivisor(view.frustum,projected.viewDepth)/view.rect.height;
  if(!std::isfinite(worldLength)||worldLength<=0)return false;
  const u32 axis=dimensions?index/2:rotation?index-9:index-6;
  const float sign=dimensions&&index%2?-1.f:1.f;
  float extent=0;
  if(dimensions) {
    local.rotationDegrees[0]=c->rotationX;local.rotationDegrees[1]=c->rotationY;local.rotationDegrees[2]=c->rotationZ;
    if(c->shape==scene::ColliderShape::Box) {
      const float extents[]{c->halfX,c->halfY,c->halfZ};extent=extents[axis];
      constexpr std::string_view properties[]{"half_x","half_y","half_z"};out.property=properties[axis];
    } else if(c->shape!=scene::ColliderShape::Sphere&&axis==1) {
      extent=c->halfHeight+(c->shape==scene::ColliderShape::Capsule?c->radius:0);out.property="half_height";
    } else {extent=c->radius;out.property="radius";}
  } else if(rotation) {
    // Euler is Rz*Ry*Rx. Each ring is in the frame preceding that factor;
    // using the full rotated frame would change a different rotation channel.
    if(axis==0)local.rotationDegrees[1]=c->rotationY;
    if(axis<2)local.rotationDegrees[2]=c->rotationZ;
    constexpr std::string_view properties[]{"rotation_x","rotation_y","rotation_z"};out.property=properties[axis];
  } else {
    constexpr std::string_view properties[]{"center_x","center_y","center_z"};out.property=properties[axis];
  }
  float localMatrix[16];editorTransformMatrix(local,localMatrix);runtime::multiplyMatrix(world,localMatrix,out.pose);
  const float length=std::sqrt(out.pose[axis*4]*out.pose[axis*4]+out.pose[axis*4+1]*out.pose[axis*4+1]+out.pose[axis*4+2]*out.pose[axis*4+2]);
  if(!std::isfinite(length)||length<1e-6f)return false;
  for(u32 i=0;i<3;++i)out.axis[i]=sign*out.pose[axis*4+i]/length;
  if(rotation) {
    if(!renderer::buildNormalMatrix(out.pose,out.inverseTranspose))return false;
    float scale=0;for(u32 column=0;column<3;++column){float squared=0;for(u32 i=0;i<3;++i)squared+=out.pose[column*4+i]*out.pose[column*4+i];scale=std::max(scale,std::sqrt(squared));}
    out.radius=worldLength/scale;out.rotation=true;colliderRingPoint(out,0,out.point);
  } else {
    if(!dimensions)extent=worldLength/length;
    for(u32 i=0;i<3;++i)out.point[i]=out.origin[i]+sign*out.pose[axis*4+i]*extent;
    out.unitsPerProperty=length;
  }
  return true;
}
inline bool applyColliderHandleDelta(EditorEntity &entity,const ColliderHandle &handle,float delta) {
  if(!std::isfinite(delta)||handle.unitsPerProperty<=0)return false;
  const auto *c=entity.components.findInstance(handle.instance);
  if(!c||&c->type()!=&scene::Collider::descriptor)return false;
  for(const auto &number:c->type().numbers)if(number.id==handle.property) {
    if(!number.presentation.isEditable(*c))return false;
    const float value=number.read(*c)+(handle.rotation?delta*57.295779513f:delta/handle.unitsPerProperty);
    if(!std::isfinite(value))return false;
    return scene::setComponentProperty(entity.components,c->type().id,handle.property,
        std::clamp(value,number.minimum,number.maximum),handle.instance)==scene::ComponentPropertyStatus::Applied;
  }
  return false;
}
} // namespace ae::editor
