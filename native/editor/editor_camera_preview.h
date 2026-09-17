#pragma once
#include "editor/editor_commands.h"
#include "editor/editor_scene_camera.h"
#include "renderer/render_view.h"

namespace ae::editor {
// One pinned preview per session. Submission is a lease: only its matching
// completion may publish an image. No image is claimed before backend success.
class EditorCameraPreview {
public:
  bool pin(const EditorDocument &document,EditorSceneVersion version,EditorEntityId camera) {
    if(!camera || resolveSceneCamera(document,camera,true).entity!=camera) return false;
    close();entity_=camera;epoch_=version.epoch;return true;
  }
  void close() {entity_=0;pending_=false;published_=false;failed_=false;lastAttempt_=-1;}
  void invalidateTarget() {pending_=false;published_=false;failed_=false;lastAttempt_=-1;}
  bool configure(u32 width,u32 height,const renderer::PreviewViewBudget &budget) {
    u32 w,h;if(!renderer::previewViewExtent(width,height,budget,w,h)) return false;
    if(w!=width_||h!=height_||budget.updatesPerSecond!=budget_.updatesPerSecond) invalidateTarget();
    width_=w;height_=h;budget_=budget;return true;
  }
  bool request(const EditorDocument &document,EditorSceneVersion version,double now,bool visible,
               bool animated,renderer::RenderViewSnapshot &out) {
    out={};
    if(!entity_) return false;
    if(version.epoch!=epoch_) {close();return false;}
    const auto pose=resolveSceneCamera(document,entity_,true);
    if(!pose.entity) {close();return false;}
    if(!visible) {invalidateTarget();return false;}
    if(failed_||!std::isfinite(now)||now<0) return false;
    if(lastAttempt_>now) invalidateTarget(); // Monotonic clock reset after host recovery.
    if(pending_) {
      // A lost completion must not leave the panel preparing forever.
      if(now-lastAttempt_>5.0) {pending_=false;published_=false;failed_=true;}
      return false;
    }
    if(lastAttempt_>=0 && now-lastAttempt_<1.0/budget_.updatesPerSecond) return false;
    if(published_ && revision_==version.revision && !animated) return false;
    renderer::PerspectiveVisibilitySettings settings;
    settings.verticalFieldOfViewRadians=pose.verticalFov*.01745329252f;
    settings.nearPlane=pose.nearPlane;settings.farPlane=pose.farPlane;settings.roll=pose.roll;
    settings.projection=pose.projection==scene::CameraProjection::Orthographic?
      renderer::CameraProjection::Orthographic:renderer::CameraProjection::Perspective;
    settings.orthographicHalfHeight=pose.orthographicHalfHeight;
    out.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,
      static_cast<float>(width_)/height_,settings);
    if(!out.frustum.valid) {out={};return false;}
    out.width=width_;out.height=height_;out.sceneEpoch=version.epoch;out.sceneRevision=version.revision;
    out.cameraEntity=entity_;out.requestId=++serial_;lease_=out;pending_=true;lastAttempt_=now;
    return true;
  }
  bool complete(const renderer::RenderViewSnapshot &request,EditorSceneVersion current,bool success) {
    if(!pending_||request.requestId!=lease_.requestId) return false;
    pending_=false;
    if(current.epoch!=lease_.sceneEpoch||current.revision!=lease_.sceneRevision) return false;
    if(!success) {failed_=true;published_=false;return false;}
    published_=true;revision_=lease_.sceneRevision;return true;
  }
  bool hasCurrentImage(EditorSceneVersion version) const {
    return published_&&version.epoch==epoch_&&version.revision==revision_;
  }
  bool failed() const {return failed_;}
  u32 width() const {return width_;}
  u32 height() const {return height_;}
  float frequency() const {return budget_.updatesPerSecond;}
  EditorEntityId camera() const {return entity_;}
private:
  EditorEntityId entity_=0;
  u64 epoch_=0,revision_=0,serial_=0;
  u32 width_=640,height_=360;
  double lastAttempt_=-1;
  bool pending_=false,published_=false,failed_=false;
  renderer::PreviewViewBudget budget_{};
  renderer::RenderViewSnapshot lease_{};
};
}
