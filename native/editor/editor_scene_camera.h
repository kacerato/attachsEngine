#pragma once
#include "editor/editor_map_scene.h"
#include <cmath>

namespace ae::editor {
struct SceneCameraPose {
  EditorEntityId entity=0;
  float position[3]{};
  float yaw=0,pitch=0;
};
// Projeção de câmera da cena: +Z para frente, hierarquia completa e nenhum
// controlador implícito. Em múltiplas câmeras, o menor ID ativo é determinístico.
inline SceneCameraPose resolveSceneCamera(const EditorDocument &document) {
  SceneCameraPose result;
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *entity=document.find(id);
    if(entity->kind!=EditorEntityKind::Camera || (result.entity && id>=result.entity)) continue;
    bool active=true;
    for(auto parent=entity;parent;parent=document.find(parent->parent)) if(!parent->active) {active=false;break;}
    float world[16];if(!active || !editorWorldMatrix(document,id,world)) continue;
    const float length=std::sqrt(world[8]*world[8]+world[9]*world[9]+world[10]*world[10]);
    if(length<1e-6f) continue;
    result.entity=id;std::copy(world+12,world+15,result.position);
    result.yaw=std::atan2(world[8],world[10]);
    result.pitch=std::asin(std::clamp(-world[9]/length,-1.0f,1.0f));
  }
  return result;
}
}
