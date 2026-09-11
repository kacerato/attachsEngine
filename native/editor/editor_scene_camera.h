#pragma once
#include "editor/editor_map_scene.h"
#include <cmath>

namespace ae::editor {
struct SceneCameraPose {
  EditorEntityId entity=0;
  float position[3]{};
  float yaw=0,pitch=0;
  float verticalFov=60,nearPlane=.1f,farPlane=2000,priority=0;
};
// Projeção de câmera da cena: +Z para frente, hierarquia completa e nenhum
// controlador implícito. Maior prioridade vence; menor ID desempata.
inline SceneCameraPose resolveSceneCamera(const runtime::SceneGraph &document) {
  SceneCameraPose result;
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *entity=document.find(id);
    const auto *camera=cameraComponent(*entity);
    if(!camera || !camera->enabled || !camera->valid() ||
       (result.entity && (camera->priority<result.priority || (camera->priority==result.priority && id>=result.entity)))) continue;
    bool active=true;
    for(auto parent=entity;parent;parent=document.find(parent->parent)) if(!parent->active) {active=false;break;}
    float world[16];if(!active || !editorWorldMatrix(document,id,world)) continue;
    const float length=std::sqrt(world[8]*world[8]+world[9]*world[9]+world[10]*world[10]);
    if(length<1e-6f) continue;
    result.entity=id;std::copy(world+12,world+15,result.position);
    result.verticalFov=camera->verticalFov;result.nearPlane=camera->nearPlane;
    result.farPlane=camera->farPlane;result.priority=camera->priority;
    result.yaw=std::atan2(world[8],world[10]);
    result.pitch=std::asin(std::clamp(-world[9]/length,-1.0f,1.0f));
  }
  return result;
}
}
