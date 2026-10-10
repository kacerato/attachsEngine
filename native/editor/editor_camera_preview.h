#pragma once
#include "editor/editor_commands.h"
#include "editor/editor_scene_camera.h"
#include "editor/editor_material_object_preview.h"
#include "editor/editor_material_standalone_preview.h"
#include "renderer/render_view.h"
#include <string>
#include <string_view>

namespace ae::editor {
// One pinned preview per session. Submission is a lease: only its matching
// completion may publish an image. No image is claimed before backend success.
class EditorCameraPreview {
public:
  bool pin(const EditorDocument &document,EditorSceneVersion version,EditorEntityId camera) {
    if(!camera || resolveSceneCamera(document,camera,true).entity!=camera) return false;
    close();entity_=camera;epoch_=version.epoch;return true;
  }
  bool pinMaterial(const EditorDocument &document,EditorSceneVersion version,const EditorMapScene &library,
                   EditorEntityId root,resources::AssetGuid material) {
    MaterialObjectPreviewView view;
    if(!buildMaterialObjectPreview(document,library,root,material,width_,height_,view)) {
      close();diagnostic_=view.diagnostic;return false;
    }
    close();entity_=root;material_=material;epoch_=version.epoch;diagnostic_=view.diagnostic;return true;
  }
  bool pinStandalone(EditorSceneVersion version,const EditorMapScene &library,resources::AssetGuid material) {
    close();geometry_=renderer::makeMaterialPreviewGeometry(renderer::MaterialPreviewShape::Sphere);
    orbit_={};orbit_.distance=2.5f;
    MaterialStandalonePreview view;
    if(!buildMaterialStandalonePreview(library,material,geometry_,orbit_,width_,height_,view)) {
      close();diagnostic_=view.diagnostic;return false;
    }
    material_=material;epoch_=version.epoch;return true;
  }
  bool changeShape() {
    if(!geometry_) return false;
    const auto next=static_cast<renderer::MaterialPreviewShape>((static_cast<u32>(geometry_->shape)+1)%3);
    auto geometry=renderer::makeMaterialPreviewGeometry(next);if(!geometry) return false;
    geometry_=std::move(geometry);invalidateTarget();return true;
  }
  bool orbit(float yaw,float pitch,float zoom=1.f) {
    if(!geometry_) return false;
    EditorCameraLimits limits;limits.minimumDistance=1.05f;limits.maximumDistance=12;
    orbitEditorCamera(orbit_,{yaw/limits.orbitRadiansPerPixel,pitch/limits.orbitRadiansPerPixel},limits);
    zoomEditorCamera(orbit_,zoom,limits);invalidateTarget();return true;
  }
  void close() {entity_=0;material_={};geometry_.reset();pending_=false;published_=false;failed_=false;lastAttempt_=-1;diagnostic_.clear();lease_={};}
  void invalidateTarget() {pending_=false;published_=false;failed_=false;lastAttempt_=-1;diagnostic_.clear();}
  bool configure(u32 width,u32 height,const renderer::PreviewViewBudget &budget) {
    u32 w,h;if(!renderer::previewViewExtent(width,height,budget,w,h)) return false;
    if(w!=width_||h!=height_||budget.updatesPerSecond!=budget_.updatesPerSecond) invalidateTarget();
    width_=w;height_=h;budget_=budget;return true;
  }
  bool request(const EditorDocument &document,EditorSceneVersion version,double now,bool visible,
               bool animated,renderer::RenderViewSnapshot &out,const EditorMapScene *library=nullptr,u32 resourceRevision=0) {
    out={};
    if(!active()) return false;
    if(version.epoch!=epoch_) {close();return false;}
    if(!geometry_&&!document.find(entity_)) {close();return false;}
    const auto pose=material_.valid()?SceneCameraPose{}:resolveSceneCamera(document,entity_,true);
    if(!material_.valid()&&!pose.entity) {close();return false;}
    if(!visible) {invalidateTarget();return false;}
    if(failed_||!std::isfinite(now)||now<0) return false;
    if(lastAttempt_>now) invalidateTarget(); // Monotonic clock reset after host recovery.
    if(pending_) {
      // A lost completion must not leave the panel preparing forever.
      if(now-lastAttempt_>5.0) {pending_=false;published_=false;failed_=true;}
      return false;
    }
    if(lastAttempt_>=0 && now-lastAttempt_<1.0/budget_.updatesPerSecond) return false;
    if(published_ && revision_==version.revision && resourceRevision_==resourceRevision && !animated) return false;
    renderer::PerspectiveVisibilitySettings settings;
    settings.verticalFieldOfViewRadians=pose.verticalFov*.01745329252f;
    settings.nearPlane=pose.nearPlane;settings.farPlane=pose.farPlane;settings.roll=pose.roll;
    settings.projection=pose.projection==scene::CameraProjection::Orthographic?
      renderer::CameraProjection::Orthographic:renderer::CameraProjection::Perspective;
    settings.orthographicHalfHeight=pose.orthographicHalfHeight;
    if(geometry_) {
      MaterialStandalonePreview view;
      if(!library||!buildMaterialStandalonePreview(*library,material_,geometry_,orbit_,width_,height_,view)) {
        failed_=true;published_=false;diagnostic_=library?view.diagnostic:"Biblioteca da prévia indisponível.";return false;
      }
      out.frustum=view.frustum;out.materialGeometry=geometry_;out.previewMaterial=view.material;
    } else if(material_.valid()) {
      MaterialObjectPreviewView view;
      if(!library||!buildMaterialObjectPreview(document,*library,entity_,material_,width_,height_,view)) {
        failed_=true;published_=false;diagnostic_=library?view.diagnostic:"Biblioteca da prévia indisponível.";return false;
      }
      out.frustum=view.frustum;out.isolatedObjects=std::move(view.objects);diagnostic_=view.diagnostic;
    } else out.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,
      static_cast<float>(width_)/height_,settings);
    if(!out.frustum.valid) {out={};return false;}
    out.width=width_;out.height=height_;out.sceneEpoch=version.epoch;out.sceneRevision=version.revision;
    out.cameraEntity=entity_;out.environmentLayerMask=material_.valid()?~0u:pose.environmentMask;
    out.resourceRevision=resourceRevision;
    out.requestId=++serial_;lease_=out;pending_=true;lastAttempt_=now;
    return true;
  }
  bool complete(const renderer::RenderViewSnapshot &request,EditorSceneVersion current,bool success,
                std::string_view diagnostic={},u32 resourceRevision=0) {
    if(!pending_||request.requestId!=lease_.requestId) return false;
    pending_=false;
    if(current.epoch!=lease_.sceneEpoch||current.revision!=lease_.sceneRevision||resourceRevision!=lease_.resourceRevision) return false;
    if(!success) {failed_=true;published_=false;diagnostic_=diagnostic;return false;}
    if(!material_.valid()) diagnostic_.clear();
    published_=true;revision_=lease_.sceneRevision;resourceRevision_=resourceRevision;return true;
  }
  bool hasCurrentImage(EditorSceneVersion version,u32 resourceRevision=0) const {
    return published_&&version.epoch==epoch_&&version.revision==revision_&&resourceRevision_==resourceRevision;
  }
  bool failed() const {return failed_;}
  const std::string &diagnostic() const {return diagnostic_;}
  u32 width() const {return width_;}
  u32 height() const {return height_;}
  float frequency() const {return budget_.updatesPerSecond;}
  EditorEntityId camera() const {return entity_;}
  bool active() const {return entity_||geometry_;}
  bool standalone() const {return static_cast<bool>(geometry_);}
  renderer::MaterialPreviewShape shape() const {return geometry_?geometry_->shape:renderer::MaterialPreviewShape::Sphere;}
  resources::AssetGuid material() const {return material_;}
private:
  EditorEntityId entity_=0;
  resources::AssetGuid material_{};
  std::shared_ptr<const renderer::MaterialPreviewGeometry> geometry_;
  EditorCamera orbit_{};
  u32 resourceRevision_=0;
  u64 epoch_=0,revision_=0,serial_=0;
  u32 width_=640,height_=360;
  double lastAttempt_=-1;
  bool pending_=false,published_=false,failed_=false;
  renderer::PreviewViewBudget budget_{};
  renderer::RenderViewSnapshot lease_{};
  std::string diagnostic_;
};
}
