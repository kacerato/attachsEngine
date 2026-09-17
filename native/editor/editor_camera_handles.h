#pragma once
#include "editor/editor_scene_camera.h"
#include "editor/editor_view.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
// Geometry shared by drawing and interaction. Optical distances ignore scale.
struct EditorCameraHandle {
  float point[3]{}, axis[3]{};
  float depth=1;
};
inline bool cameraHandleGeometry(const EditorDocument &document,EditorEntityId id,
                                 u32 kind,EditorCameraHandle &out) {
  const auto *entity=document.find(id);const auto *camera=entity?cameraComponent(*entity):nullptr;
  float world[16],pose[16];
  if(!camera || !camera->valid() || kind>2 || !editorWorldMatrix(document,id,world) ||
     !editorOpticalFrame(world,pose)) return false;
  out.depth=kind==1?camera->nearPlane:kind==2?camera->farPlane:
    std::clamp(5.f,camera->nearPlane,camera->farPlane);
  const float height=camera->projection==scene::CameraProjection::Orthographic?
    camera->orthographicHalfHeight:out.depth*std::tan(camera->verticalFov*0.00872664626f);
  for(u32 i=0;i<3;++i) {
    out.point[i]=pose[12+i]+pose[8+i]*out.depth+(kind==0?pose[4+i]*height:0);
    out.axis[i]=pose[(kind==0?4:8)+i];
  }
  return true;
}
inline bool cameraHandleRayParameter(const EditorViewport &view,const EditorCameraHandle &handle,
                                     ui::UiPoint point,float &parameter) {
  const auto ray=screenPointToRay(view,point);if(!ray.valid) return false;
  float directionDot=0,axisOffset=0,rayOffset=0;
  for(u32 i=0;i<3;++i) {
    const float offset=ray.origin[i]-handle.point[i];
    directionDot+=handle.axis[i]*ray.direction[i];
    axisOffset+=handle.axis[i]*offset;rayOffset+=ray.direction[i]*offset;
  }
  const float denominator=1-directionDot*directionDot;
  if(denominator<.0025f) return false; // Almost axial view: do not amplify touch noise.
  parameter=(axisOffset-directionDot*rayOffset)/denominator;
  return std::isfinite(parameter);
}
inline bool applyCameraHandleDelta(scene::Camera &camera,u32 kind,float depth,float delta) {
  if(!std::isfinite(delta)||kind>2) return false;
  if(kind==0) {
    if(camera.projection==scene::CameraProjection::Orthographic)
      camera.orthographicHalfHeight=std::clamp(camera.orthographicHalfHeight+delta,.001f,100000.f);
    else {
      const float height=depth*std::tan(camera.verticalFov*.00872664626f)+delta;
      camera.verticalFov=std::clamp(std::atan2(std::max(0.f,height),depth)*114.591559f,1.f,170.f);
    }
  } else if(kind==1) camera.nearPlane=std::clamp(camera.nearPlane+delta,.001f,std::min(10000.f,camera.farPlane-.001f));
  else camera.farPlane=std::clamp(camera.farPlane+delta,std::max(.01f,camera.nearPlane+.001f),1000000.f);
  return camera.valid();
}
}
