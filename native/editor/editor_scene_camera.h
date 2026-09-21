#pragma once
#include "editor/editor_map_scene.h"
#include "renderer/camera_ray.h"
#include <cmath>

namespace ae::editor {
struct SceneCameraPose {
  EditorEntityId entity=0;
  float position[3]{};
  float yaw=0,pitch=0,roll=0;
  float verticalFov=60,nearPlane=.1f,farPlane=2000,priority=0;
  scene::CameraProjection projection=scene::CameraProjection::Perspective;
  float orthographicHalfHeight=5;
  u32 environmentMask=~0u;
};
// Orthonormal optical frame: hierarchy determines orientation; object scale
// does not change lens/range. Degenerate transforms have no valid camera pose.
inline bool editorOpticalFrame(const float *world,float *pose) {
  std::copy(world,world+16,pose);
  const auto normalize=[](float *v) {
    const float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(!std::isfinite(n)||n<1e-6f) return false;
    for(u32 i=0;i<3;++i) v[i]/=n;
    return true;
  };
  float forward[3]{world[8],world[9],world[10]};if(!normalize(forward)) return false;
  float right[3]{world[5]*forward[2]-world[6]*forward[1],world[6]*forward[0]-world[4]*forward[2],world[4]*forward[1]-world[5]*forward[0]};
  if(!normalize(right)) return false;
  float up[3]{forward[1]*right[2]-forward[2]*right[1],forward[2]*right[0]-forward[0]*right[2],forward[0]*right[1]-forward[1]*right[0]};
  for(u32 i=0;i<3;++i) {pose[i]=right[i];pose[4+i]=up[i];pose[8+i]=forward[i];}
  return true;
}
// Projeção de câmera da cena: +Z para frente, hierarquia completa e nenhum
// controlador implícito. Maior prioridade vence; menor ID desempata.
inline SceneCameraPose resolveSceneCamera(const runtime::SceneGraph &document,EditorEntityId requested=0,bool preview=false) {
  SceneCameraPose result;
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  for(auto id:ids) {
    const auto *entity=document.find(id);
    if(!entity || (requested && id!=requested)) continue;
    const auto *camera=cameraComponent(*entity);
    if(!camera || (!preview && !camera->enabled) || !camera->valid() ||
       (result.entity && (camera->priority<result.priority || (camera->priority==result.priority && id>=result.entity)))) continue;
    bool active=true;
    for(auto parent=entity;parent;parent=document.find(parent->parent)) if(!parent->active) {active=false;break;}
    float world[16],pose[16];if((!preview&&!active) || !editorWorldMatrix(document,id,world)||!editorOpticalFrame(world,pose)) continue;
    const float length=std::sqrt(world[8]*world[8]+world[9]*world[9]+world[10]*world[10]);
    if(length<1e-6f) continue;
    result.entity=id;std::copy(world+12,world+15,result.position);
    result.verticalFov=camera->verticalFov;result.nearPlane=camera->nearPlane;
    result.farPlane=camera->farPlane;result.priority=camera->priority;
    result.projection=camera->projection;result.orthographicHalfHeight=camera->orthographicHalfHeight;
    result.environmentMask=camera->environmentMask;
    result.yaw=std::atan2(world[8],world[10]);
    result.pitch=std::asin(std::clamp(-world[9]/length,-1.0f,1.0f));
    const auto base=renderer::buildCameraViewBasis(result.yaw,result.pitch);
    float cosine=0,sine=0;for(u32 k=0;k<3;++k) {cosine+=pose[k]*base.row0[k];sine+=pose[k]*base.row1[k];}
    result.roll=std::atan2(sine,cosine);
  }
  return result;
}
}
