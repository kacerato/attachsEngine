#pragma once
#include "editor/editor_document.h"
#include "runtime/scene_components.h"
#include "scene/camera_look.h"
namespace ae::editor {
using EditorCameraLook=scene::CameraLook;
using scene::cameraLookNumbers;
using runtime::cameraLook;
using runtime::editCameraLook;
// Input is normalized by the viewport, independent of DPI and frame duration.
// Local axes are intentional: a parent rig supplies its own world orientation.
inline bool applyCameraLook(runtime::SceneGraph &document,EditorEntityId id,float x,float y) {
  const auto *e=document.find(id);if(!e||!cameraComponent(*e)||!std::isfinite(x)||!std::isfinite(y)) return false;
  const auto *settings=cameraLook(*e);if(!settings||!settings->valid()) return false;
  if(x==0&&y==0) return true;
  auto transform=e->transform;
  transform.rotationDegrees[1]=std::remainder(transform.rotationDegrees[1]+x*settings->yawSensitivity,360.0f);
  transform.rotationDegrees[0]=std::clamp(transform.rotationDegrees[0]+y*settings->pitchSensitivity,-settings->pitchLimit,settings->pitchLimit);
  return document.setTransform(id,transform);
}
}
