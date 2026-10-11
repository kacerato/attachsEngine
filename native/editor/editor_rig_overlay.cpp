#include "editor/editor_rig_overlay.h"
#include "runtime/transform_math.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::editor {
namespace {
using Vec=std::array<float,3>;
Vec cross(const Vec &a,const Vec &b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
bool normalize(Vec &v) {
  const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if(!std::isfinite(length)||length<1e-8f)return false;
  for(auto &value:v)value/=length;
  return true;
}
void limitOverlay(const resources::RigControl &control,const resources::CompiledRigJoint &joint,
    const float basis[16],const float center[3],const EditorViewport &view,const ui::UiTheme &theme,ui::UiDrawList &list) {
  if(control.limit.kind==resources::RigLimitKind::Free)return;
  const auto projected=projectWorldToScreen(view,center);if(!projected.valid)return;
  const float radius=44.f*2.f*renderer::projectionHalfHeight(view.frustum)*
      renderer::projectionDivisor(view.frustum,projected.viewDepth)/view.rect.height;
  if(!std::isfinite(radius)||radius<=0)return;
  const auto axis=control.limit.axis;
  Vec seed=std::abs(axis[0])<.8f?Vec{1,0,0}:Vec{0,1,0};
  Vec u=cross(axis,seed);if(!normalize(u))return;
  Vec v=cross(axis,u);if(!normalize(v))return;
  const auto point=[&](const Vec &direction) {
    // The limit's axis is expressed about the authored reference rotation.
    // Parent basis contains the evaluated parent pose, not its bind pose.
    const auto rotated=resources::rotateRigVector(joint.reference.rotation,direction);
    Vec world;
    for(u32 c=0;c<3;++c)world[c]=basis[c]*rotated[0]+basis[4+c]*rotated[1]+basis[8+c]*rotated[2];
    if(!normalize(world))return Vec{center[0],center[1],center[2]};
    for(u32 c=0;c<3;++c)world[c]=center[c]+world[c]*radius;
    return world;
  };
  const auto line=[&](const Vec &a,const Vec &b,float width=1.5f) {
    ui::UiPoint p,q;if(projectSegmentToScreen(view,a.data(),b.data(),p,q))list.addLine(p,q,theme.color.warning,width);
  };
  const Vec origin{center[0],center[1],center[2]};
  constexpr float radians=.017453292519943295f;
  Vec previous{},first{},last{};
  for(u32 step=0;step<=48;++step) {
    const float angle=(control.limit.minimum+(control.limit.maximum-control.limit.minimum)*float(step)/48.f)*radians;
    Vec direction;for(u32 c=0;c<3;++c)direction[c]=u[c]*std::cos(angle)+v[c]*std::sin(angle);
    const auto at=point(direction);if(step)line(previous,at);else first=at;
    previous=last=at;
  }
  line(origin,first);line(origin,last);line(origin,point(axis),2);
  if(control.limit.kind==resources::RigLimitKind::ConeTwist) {
    const float cone=control.limit.cone*radians;
    for(u32 step=0;step<=48;++step) {
      const float angle=float(step)*360.f/48.f*radians;
      Vec direction;
      for(u32 c=0;c<3;++c)direction[c]=axis[c]*std::cos(cone)+(u[c]*std::cos(angle)+v[c]*std::sin(angle))*std::sin(cone);
      const auto at=point(direction);if(step)line(previous,at);
      if(step%12==0)line(origin,at,1);
      previous=at;
    }
  }
}
} // namespace
bool buildEditorRigOverlay(const resources::RigAsset &asset,const resources::CompiledRig &rig,
    const runtime::RigBindings &bindings,const runtime::SceneGraph &scene,const EditorViewport &view,
    u64 selected,const ui::UiTheme &theme,ui::UiDrawList &list,EditorRigOverlay &out,const resources::RigTransform *placement) {
  out.handles.clear();out.viewport={};
  if(!isViewportValid(view)||asset.guid!=rig.guid||asset.revision!=rig.revision||
      bindings.rig!=rig.guid||bindings.revision!=rig.revision||bindings.objects.size()!=rig.joints.size()||
      rig.joints.empty()||rig.joints.size()>resources::RigAsset::MaximumJoints)return false;
  out.world.resize(rig.joints.size());out.live.assign(rig.joints.size(),0);
  out.handles.reserve(asset.controls.size());out.viewport=view.rect;
  for(usize i=0;i<rig.joints.size();++i) {
    const auto *object=scene.find(bindings.objects[i]);if(!object)continue;
    const auto parent=rig.joints[i].parent;
    if(parent<0) {
      if(object->id!=bindings.root||!runtime::worldMatrix(scene,object->id,out.world[i].data()))continue;
    } else {
      if(static_cast<usize>(parent)>=i||!out.live[parent]||object->parent!=bindings.objects[parent])continue;
      float local[16];runtime::transformMatrix(object->transform,local);
      runtime::multiplyMatrix(out.world[parent].data(),local,out.world[i].data());
      if(std::any_of(out.world[i].begin(),out.world[i].end(),[](float value){return !std::isfinite(value);}))continue;
    }
    out.live[i]=1;
  }
  if(!list.pushClip(view.rect))return false;
  for(usize i=0;i<rig.joints.size();++i) {
    const auto parent=rig.joints[i].parent;if(!out.live[i]||parent<0||!out.live[parent])continue;
    ui::UiPoint a,b;
    if(projectSegmentToScreen(view,&out.world[parent][12],&out.world[i][12],a,b))
      list.addLine(a,b,theme.color.textMuted,1.5f);
  }
  for(const auto &control:asset.controls) {
    const auto at=rig.index.find(control.joint);if(at==rig.index.end()||!out.live[at->second])continue;
    const auto point=projectWorldToScreen(view,&out.world[at->second][12]);
    if(!point.valid||!view.rect.contains(point.screen))continue;
    out.handles.push_back({control.id,control.joint,bindings.objects[at->second],point.screen,point.viewDepth});
    const bool chosen=control.id==selected;
    const auto color=chosen?theme.color.accent:control.limit.kind==resources::RigLimitKind::Free?theme.color.text:theme.color.warning;
    const float half=chosen?7.f:4.f;
    const ui::UiRect mark{point.screen.x-half,point.screen.y-half,half*2,half*2};
    list.addRect(mark,theme.color.canvas,chosen?2.f:1.f);list.addBorder(mark,color,chosen?2.f:1.5f,chosen?2.f:1.f);
    if(chosen) {
      const auto &joint=rig.joints[at->second];float basis[16];bool showLimit=false;
      if(joint.parent>=0) {std::copy_n(out.world[joint.parent].data(),16,basis);showLimit=true;}
      else if(placement&&resources::validRigTransform(*placement)) {
        runtime::Transform placementTransform;
        if(runtime::rigTransformToScene(*placement,placementTransform,placementTransform)) {
          float parent[16],offset[16];runtime::transformMatrix(placementTransform,offset);
          if(runtime::parentWorldMatrix(scene,bindings.root,parent)) {runtime::multiplyMatrix(parent,offset,basis);showLimit=true;}
        }
      }
      if(showLimit)limitOverlay(control,joint,basis,&out.world[at->second][12],view,theme,list);
      const float width=std::min(180.f,std::max(0.f,view.rect.right()-point.screen.x-14));
      if(width>=48)list.addText({point.screen.x+14,point.screen.y-12,width,24},control.name,theme.color.text,theme.type.body);
    }
  }
  list.popClip();return true;
}

u64 pickEditorRigControl(const EditorRigOverlay &overlay,ui::UiPoint point,float touchSide) {
  if(!overlay.viewport.contains(point)||!std::isfinite(touchSide)||touchSide<48)return 0;
  const float half=touchSide*.5f;
  float best=std::numeric_limits<float>::max(),depth=std::numeric_limits<float>::max();u64 chosen=0;
  for(const auto &handle:overlay.handles) {
    const float x=point.x-handle.screen.x,y=point.y-handle.screen.y;
    if(std::abs(x)>half||std::abs(y)>half)continue;
    const float distance=x*x+y*y;
    if(distance<best-.01f||(std::abs(distance-best)<=.01f&&(handle.depth<depth-.001f||
        (std::abs(handle.depth-depth)<=.001f&&(!chosen||handle.control<chosen))))) {
      best=distance;depth=handle.depth;chosen=handle.control;
    }
  }
  return chosen;
}
} // namespace ae::editor
