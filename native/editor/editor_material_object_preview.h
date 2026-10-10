#pragma once
#include "editor/editor_camera.h"
#include "editor/editor_map_scene.h"
#include <memory>
#include <limits>

namespace ae::editor {
struct MaterialObjectPreviewView {
  renderer::PerspectiveFrustum frustum;
  std::shared_ptr<const std::vector<u64>> objects;
  bool localOverrides=false;
  std::string diagnostic;
};

// Uses published authored geometry and effective materials. No stand-in mesh,
// hidden entity, source rewrite or mutation of the editor's main camera.
inline bool buildMaterialObjectPreview(const EditorDocument &document,const EditorMapScene &library,
    EditorEntityId root,resources::AssetGuid material,u32 width,u32 height,MaterialObjectPreviewView &out) {
  out={};
  if(!document.find(root)||!material.valid()||!library.sharedMaterial(material)||!width||!height) {
    out.diagnostic="Objeto ou material da prévia indisponível.";return false;
  }
  std::vector<EditorEntityId> subtree;document.collectSubtree(root,subtree);
  bool usesMaterial=false;
  for(const auto id:subtree) if(const auto *entity=document.find(id)) if(const auto *render=meshRenderer(*entity))
    for(u32 slot=0;slot<render->slotCount();++slot) if(render->slotMaterialAsset(slot)==material) {
      usesMaterial=true;
      out.localOverrides|=render->slotMaterial(slot).enabled || render->slotSurface(slot).overrides() ||
        render->slotChannels(slot).overrides() || render->slotOcclusionTexture(slot).valid();
      for(u32 binding=0;binding<scene::MaterialTextureCount;++binding)
        out.localOverrides|=render->slotTextures(slot)[binding].valid() || render->slotSampling(slot)[binding].overrides();
    }
  if(!usesMaterial) {out.diagnostic="O objeto selecionado e seus descendentes não usam este material.";return false;}
  std::sort(subtree.begin(),subtree.end());
  std::vector<EditorMapUpdate> draws;
  if(!library.extract(document,draws)) {out.diagnostic="Geometria autoral não pôde ser extraída para a prévia.";return false;}
  float lower[3]{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
  float upper[3]{-lower[0],-lower[1],-lower[2]};
  auto objects=std::make_shared<std::vector<u64>>();
  for(const auto &draw:draws) {
    if(!draw.visible||!std::binary_search(subtree.begin(),subtree.end(),draw.objectId)) continue;
    const auto &bounds=draw.pose.draw;
    if(!std::isfinite(bounds.boundsRadius)||bounds.boundsRadius<=0) continue;
    for(u32 axis=0;axis<3;++axis) {
      if(!std::isfinite(bounds.boundsCenter[axis])) {out.diagnostic="Limites inválidos na prévia.";return false;}
      lower[axis]=std::min(lower[axis],bounds.boundsCenter[axis]-bounds.boundsRadius);
      upper[axis]=std::max(upper[axis],bounds.boundsCenter[axis]+bounds.boundsRadius);
    }
    objects->push_back(draw.objectId);
  }
  if(objects->empty()) {out.diagnostic="O objeto não possui malha visível publicada; importe ou escolha uma malha real.";return false;}
  std::sort(objects->begin(),objects->end());objects->erase(std::unique(objects->begin(),objects->end()),objects->end());
  EditorCamera camera;float radius2=0;
  for(u32 axis=0;axis<3;++axis) {
    camera.target[axis]=(lower[axis]+upper[axis])*.5f;
    const float extent=(upper[axis]-lower[axis])*.5f;radius2+=extent*extent;
  }
  const float radius=std::sqrt(radius2);
  if(!std::isfinite(radius)||radius<=0) {out.diagnostic="Escala da malha inválida para a prévia.";return false;}
  renderer::PerspectiveVisibilitySettings settings;settings.verticalFieldOfViewRadians=.78539816f;
  const float aspect=static_cast<float>(width)/height;
  const float limitingHalfFov=std::atan(std::tan(settings.verticalFieldOfViewRadians*.5f)*std::min(aspect,1.0f));
  camera.distance=radius/std::sin(limitingHalfFov)*1.1f;camera.yaw=.6f;camera.pitch=.25f;
  settings.nearPlane=std::max(.0001f,camera.distance-radius*1.2f);
  settings.farPlane=std::max(settings.nearPlane+.01f,camera.distance+radius*2.0f);
  float position[3];editorCameraPosition(camera,position);
  out.frustum=renderer::buildPerspectiveFrustum(position,camera.yaw,camera.pitch,aspect,settings);
  out.objects=std::move(objects);
  if(!out.frustum.valid) {out.diagnostic="Enquadramento da prévia inválido.";return false;}
  if(out.localOverrides) out.diagnostic="Inclui substituições locais dos slots.";
  return true;
}
} // namespace ae::editor
