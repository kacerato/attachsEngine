#pragma once
#include "editor/editor_map_scene.h"
#include "renderer/camera_ray.h"
#include "renderer/frustum_visibility.h"
#include "renderer/render_view.h"
#include "ui/ui_geometry.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::editor {
struct SceneCameraPose {
  EditorEntityId entity=0;
  float position[3]{};
  float yaw=0,pitch=0,roll=0;
  float verticalFov=60,nearPlane=.1f,farPlane=2000,priority=0;
  scene::CameraProjection projection=scene::CameraProjection::Perspective;
  float orthographicHalfHeight=5;
  u32 environmentMask=~0u;
  // PK-7: o restante do Inspector da Câmera, copiado para quem desenha.
  scene::CameraBackground background=scene::CameraBackground::Skybox;
  float backgroundColor[3]{.0307f,.0742f,.1912f};
  u32 cullingMask=~0u;
  // Viewport Rect da Unity: x, y a partir do canto INFERIOR esquerdo, largura e
  // altura, tudo em fração da vista de jogo.
  float viewport[4]{0,0,1,1};
  bool postProcessing=true,renderShadows=true,hdr=true;
  scene::CameraAntialiasing antialiasing=scene::CameraAntialiasing::Project;
  bool physicalCamera=false;
  float focalLength=20.7846097f,sensorWidth=36,sensorHeight=24;
  scene::CameraGateFit gateFit=scene::CameraGateFit::Horizontal;
  // Retângulo recortado à vista; falso quando nada dele fica na tela.
  bool clippedViewport(float out[4]) const {
    const float x0=std::clamp(viewport[0],0.f,1.f),y0=std::clamp(viewport[1],0.f,1.f);
    const float x1=std::clamp(viewport[0]+viewport[2],0.f,1.f),y1=std::clamp(viewport[1]+viewport[3],0.f,1.f);
    out[0]=x0;out[1]=y0;out[2]=x1-x0;out[3]=y1-y0;
    return out[2]>1e-4f && out[3]>1e-4f;
  }
  bool coversView() const {
    float r[4];return clippedViewport(r) && r[0]<=0 && r[1]<=0 && r[2]>=1 && r[3]>=1;
  }
  // O retângulo da câmera dentro de `area` (coordenadas de interface, y para
  // baixo): a Unity mede y de baixo para cima.
  ui::UiRect screenRect(const ui::UiRect &area) const {
    float r[4];if(!clippedViewport(r)) return {};
    return {area.x+r[0]*area.width,area.y+(1-r[1]-r[3])*area.height,r[2]*area.width,r[3]*area.height};
  }
  scene::CameraLens lens(float screenAspect) const {
    return scene::cameraLens(physicalCamera,verticalFov,focalLength,sensorWidth,sensorHeight,gateFit,screenAspect);
  }
  // Projeção para uma vista de `width`×`height`: a lente efetiva (câmera física
  // inclusive) e a proporção que a projeção deve usar.
  float buildProjection(renderer::PerspectiveVisibilitySettings &settings,float width,float height) const {
    const auto effective=lens(height>0?width/height:1.f);
    settings.verticalFieldOfViewRadians=effective.verticalFovRadians;
    settings.nearPlane=nearPlane;settings.farPlane=farPlane;settings.roll=roll;
    settings.projection=projection==scene::CameraProjection::Orthographic?
      renderer::CameraProjection::Orthographic:renderer::CameraProjection::Perspective;
    settings.orthographicHalfHeight=orthographicHalfHeight;
    return effective.aspect;
  }
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
// Objetos com componente de câmera, em pré-ordem, enquanto a estrutura do grafo
// não muda (cache por thread em editor_session_render.cpp). O Play resolvia a
// câmera várias vezes por quadro percorrendo o grafo inteiro (perfil da Vila, G1).
const std::vector<EditorEntityId> &sceneCameraEntities(const runtime::SceneGraph &document);

inline SceneCameraPose resolveSceneCamera(const runtime::SceneGraph &document,EditorEntityId requested=0,bool preview=false) {
  SceneCameraPose result;
  // A escolha (maior prioridade; no empate, menor id) não depende da ordem.
  // Camera.main da Unity: com uma câmera marcada "MainCamera", ela é a câmera
  // do jogo (entrada, raios de tela, olhar) mesmo sob outra de prioridade maior
  // na pilha da tela; sem a marca, vale a maior prioridade.
  bool resultMain=false;
  for(auto id:sceneCameraEntities(document)) {
    const auto *entity=document.find(id);
    if(!entity || (requested && id!=requested)) continue;
    const auto *camera=cameraComponent(*entity);
    const bool main=!requested && entity->tag=="MainCamera";
    if(!camera || (!preview && !camera->enabled) || !camera->valid() ||
       (result.entity && (resultMain>main || (resultMain==main &&
        (camera->priority<result.priority || (camera->priority==result.priority && id>=result.entity)))))) continue;
    bool active=true;
    for(auto parent=entity;parent;parent=document.find(parent->parent)) if(!parent->active) {active=false;break;}
    float world[16],pose[16];if((!preview&&!active) || !editorWorldMatrix(document,id,world)||!editorOpticalFrame(world,pose)) continue;
    const float length=std::sqrt(world[8]*world[8]+world[9]*world[9]+world[10]*world[10]);
    if(length<1e-6f) continue;
    result.entity=id;resultMain=main;std::copy(world+12,world+15,result.position);
    result.verticalFov=camera->verticalFov;result.nearPlane=camera->nearPlane;
    result.farPlane=camera->farPlane;result.priority=camera->priority;
    result.projection=camera->projection;result.orthographicHalfHeight=camera->orthographicHalfHeight;
    result.environmentMask=camera->environmentMask;
    result.background=camera->background;std::copy(camera->backgroundColor,camera->backgroundColor+3,result.backgroundColor);
    result.cullingMask=camera->cullingMask;
    result.viewport[0]=camera->viewportX;result.viewport[1]=camera->viewportY;
    result.viewport[2]=camera->viewportWidth;result.viewport[3]=camera->viewportHeight;
    result.postProcessing=camera->postProcessing;result.renderShadows=camera->renderShadows;result.hdr=camera->hdr;
    result.antialiasing=camera->antialiasing;result.physicalCamera=camera->physicalCamera;
    result.focalLength=camera->focalLength;result.sensorWidth=camera->sensorWidth;result.sensorHeight=camera->sensorHeight;
    result.gateFit=camera->gateFit;
    result.yaw=std::atan2(world[8],world[10]);
    result.pitch=std::asin(std::clamp(-world[9]/length,-1.0f,1.0f));
    const auto base=renderer::buildCameraViewBasis(result.yaw,result.pitch);
    float cosine=0,sine=0;for(u32 k=0;k<3;++k) {cosine+=pose[k]*base.row0[k];sine+=pose[k]*base.row1[k];}
    result.roll=std::atan2(sine,cosine);
  }
  return result;
}
// Copia para uma vista fora da tela o que a câmera decide sobre a imagem.
inline void applySceneCameraOutput(const SceneCameraPose &pose,renderer::RenderViewSnapshot &out,float projectionAspect) {
  out.cullingMask=pose.cullingMask;out.background=static_cast<u32>(pose.background);
  std::copy(pose.backgroundColor,pose.backgroundColor+3,out.backgroundColor);
  out.postProcessing=pose.postProcessing;out.hdr=pose.hdr;out.antialiasing=static_cast<u32>(pose.antialiasing);
  out.projectionAspect=out.width&&out.height&&std::abs(projectionAspect-static_cast<float>(out.width)/out.height)>1e-4f?projectionAspect:0.f;
}
// Câmeras da tela na ordem em que desenham (Unity: Depth/Priority crescente;
// a de cima cobre a de baixo no próprio retângulo). Uma câmera que cobre a vista
// inteira e limpa o fundo esconde todas as anteriores, então elas nem entram:
// é o mesmo quadro que a Unity mostraria, sem desenhar o que ninguém vê. No
// empate de prioridade desenha por último a de menor id, a mesma que
// `resolveSceneCamera` elege; com uma câmera só, o resultado é idêntico.
inline std::vector<SceneCameraPose> resolveSceneCameraStack(const runtime::SceneGraph &document) {
  std::vector<SceneCameraPose> cameras;
  for(auto id:sceneCameraEntities(document)) {
    auto pose=resolveSceneCamera(document,id,false);
    if(pose.entity) cameras.push_back(pose);
  }
  std::sort(cameras.begin(),cameras.end(),[](const SceneCameraPose &a,const SceneCameraPose &b){
    return a.priority!=b.priority?a.priority<b.priority:a.entity>b.entity;
  });
  std::erase_if(cameras,[](const SceneCameraPose &pose){float r[4];return !pose.clippedViewport(r);});
  usize first=0;
  for(usize i=cameras.size();i-->0;)
    if(cameras[i].coversView() && cameras[i].background!=scene::CameraBackground::Uninitialized) {first=i;break;}
  cameras.erase(cameras.begin(),cameras.begin()+static_cast<std::ptrdiff_t>(first));
  return cameras;
}
}
