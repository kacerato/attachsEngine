#pragma once
#include "editor/editor_document.h"
#include "runtime/transform_math.h"
#include "renderer/map_draw_update.h"
#include "editor/editor_view.h"

namespace ae::editor {
// Immutable package geometry plus authored transforms. No Vulkan or Android.
// assetId identifies a package draw, not a process pointer. The package fingerprint
// must accompany saved documents so references cannot silently target another map.
using EditorMapUpdate = renderer::MapDrawState;
class EditorMapScene {
public:
  u32 assetCount() const { return static_cast<u32>(source_.size()); }
  const renderer::MapDrawRecord *asset(u32 index) const { return index<source_.size()?&source_[index]:nullptr; }
  // Loading a resource library need not instantiate its contents in the scene.
  bool import(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
              std::span<const renderer::MapMaterialRecord> materials = {}, bool instantiate = true, std::span<const u8> vertices = {}, std::span<const u32> indices = {});
  bool extract(const runtime::SceneGraph &document, std::vector<EditorMapUpdate> &out) const;
  bool bounds(const runtime::SceneGraph &document, EditorEntityId entity, float center[3], float &radius) const;
  bool localGeometry(u32 assetId,std::span<const EditorPickMesh::Triangle> &triangles,float relative[16]) const;
  bool pickGeometry(const runtime::SceneGraph &document, EditorEntityId id, EditorPickCandidate &out) const;
  void hydrateMaterials(EditorDocument &document) const;
  renderer::MaterialOverride materialForAsset(u32 index) const;
  u32 materialFlagsForAsset(u32 index) const {
    return index<source_.size() && source_[index].materialIndex<materials_.size()
        ? materials_[source_[index].materialIndex].flags : 0;
  }
private:
  std::vector<std::shared_ptr<const EditorPickMesh>> pickMeshes_;
  std::vector<renderer::MapMaterialRecord> materials_;
  std::vector<renderer::MapDrawRecord> source_;
};
// Column-major local/world matrices with Rz * Ry * Rx Euler convention.
// A convenção mora em runtime/transform_math.h: o editor apenas a reexporta com
// os nomes que seus arquivos já usam, para que gizmo, física e scripts não
// possam divergir por terem cópias da mesma decomposição.
inline void editorTransformMatrix(const EditorTransform &transform, float out[16]) {
  runtime::transformMatrix(transform, out);
}
inline bool editorWorldMatrix(const runtime::SceneGraph &graph, EditorEntityId entity, float out[16]) {
  return runtime::worldMatrix(graph, entity, out);
}
inline bool editorLocalTransformForWorld(const float world[16], const float parent[16], EditorTransform &out) {
  return runtime::localTransformForWorld(world, parent, out);
}
} // namespace ae::editor
