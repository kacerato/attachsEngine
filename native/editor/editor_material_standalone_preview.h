#pragma once
#include "editor/editor_camera.h"
#include "editor/editor_map_scene.h"
#include "renderer/material_preview_geometry.h"

namespace ae::editor {
struct MaterialStandalonePreview {
  std::shared_ptr<const renderer::MaterialPreviewGeometry> geometry;
  renderer::MaterialOverride material{};
  renderer::PerspectiveFrustum frustum{};
  resources::AssetGuid guid{};
  std::string diagnostic;
};

// Resolves a shared material through the same library as scene slots, with no
// local overrides and no hidden scene entity. GPU publication remains the
// renderer's responsibility; a resolved CPU packet does not claim an image.
inline bool buildMaterialStandalonePreview(const EditorMapScene &library,resources::AssetGuid guid,
    std::shared_ptr<const renderer::MaterialPreviewGeometry> geometry,const EditorCamera &orbit,
    u32 width,u32 height,MaterialStandalonePreview &out) {
  out={};
  if(!guid.valid()||!library.sharedMaterial(guid)||!geometry||!width||!height||!isEditorCameraValid(orbit)) {
    out.diagnostic="Material, geometria ou câmera da prévia indisponível.";return false;
  }
  scene::MeshRenderer slot;slot.materialAsset=guid;
  out.material=library.slotMaterial(slot,0);
  if(!out.material.enabled) {out.diagnostic="Material compartilhado sem parâmetros efetivos.";return false;}
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
    const auto identity=library.slotTexture(slot,0,binding);
    if(identity.valid()&&identity!=scene::MaterialTextureNone&&out.material.textures[binding]==scene::MaterialTextureKeep) {
      out.diagnostic="Textura do material ainda não foi publicada: "+identity.text();return false;
    }
  }
  const auto occlusion=library.slotOcclusionTexture(slot,0);
  if(occlusion.valid()&&occlusion!=scene::MaterialTextureNone&&out.material.occlusionTexture==scene::MaterialTextureKeep) {
    out.diagnostic="Textura de oclusão ainda não foi publicada: "+occlusion.text();return false;
  }
  renderer::PerspectiveVisibilitySettings settings;settings.verticalFieldOfViewRadians=.78539816f;
  settings.nearPlane=std::max(.0001f,orbit.distance-geometry->draw.boundsRadius*1.2f);
  settings.farPlane=std::max(settings.nearPlane+.01f,orbit.distance+geometry->draw.boundsRadius*2.f);
  float position[3];editorCameraPosition(orbit,position);
  out.frustum=renderer::buildPerspectiveFrustum(position,orbit.yaw,orbit.pitch,static_cast<float>(width)/height,settings);
  if(!out.frustum.valid) {out.diagnostic="Projeção da prévia inválida.";return false;}
  out.geometry=std::move(geometry);out.guid=guid;return true;
}
} // namespace ae::editor
