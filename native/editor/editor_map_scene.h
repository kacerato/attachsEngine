#pragma once
#include "editor/editor_document.h"
#include "runtime/transform_math.h"
#include "renderer/map_draw_update.h"
#include "editor/editor_view.h"
#include "resources/asset_registry.h"

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
              std::span<const renderer::MapMaterialRecord> materials = {}, bool instantiate = true, std::span<const u8> vertices = {}, std::span<const u32> indices = {},
              u64 packageFingerprint = 0);
  // A identidade estável do desenho `index` (0-based) do pacote carregado.
  //
  // Ela é DERIVADA, não sorteada: a mesma impressão digital de pacote e o mesmo
  // índice produzem o mesmo GUID em qualquer máquina e em qualquer dia. É o que
  // permite uma cena salva antes do registro existir ganhar identidade sem
  // precisar de um arquivo de tradução guardado em lugar nenhum.
  resources::AssetGuid assetGuid(u32 index) const {
    return index < assets_.size() ? assets_[index] : resources::AssetGuid{};
  }
  // O slot (1-based, como `MeshRenderer::mesh`) de uma identidade, ou zero
  // quando o recurso não está neste pacote — uma referência ausente, que quem
  // chama precisa explicar em vez de desenhar outra malha no lugar.
  u32 assetSlot(const resources::AssetGuid &guid) const;
  // Casa slot e identidade depois de carregar uma cena. Cena com identidade
  // manda: o slot é recalculado. Cena antiga, sem identidade, ganha a derivada
  // do slot que ela já trazia.
  void reconcileAssets(EditorDocument &document) const;
  // Adota um pacote novo SEM tocar no documento. É o que a importação usa: a
  // biblioteca do processo cresce, os objetos que já existem continuam onde
  // estão, e a reconciliação reata slot e identidade.
  //
  // `identities` é opcional e vale por desenho: uma entrada válida é usada como
  // está — é o caso da geometria importada, cuja identidade vem do arquivo de
  // origem e da chave estável, não do índice — e uma entrada vazia recebe a
  // identidade derivada da impressão digital do pacote.
  // `pivots` traz três floats por desenho: a origem em torno da qual o objeto
  // gira e pela qual o gizmo o pega. Vazio mantém a convenção do pacote — pivô
  // no centro dos limites —, que é o que faz um cubo primitivo girar em torno
  // de si. Geometria importada usa a origem do NÓ, porque é ela que segura a
  // dobradiça de uma porta; o centro visual não é o pivô do arquivo.
  bool adoptPackage(EditorDocument &document, std::span<const renderer::MapDrawRecord> draws,
                    std::span<const renderer::MapMaterialRecord> materials,
                    std::span<const u8> vertices, std::span<const u32> indices,
                    std::span<const resources::AssetGuid> identities, u64 packageFingerprint,
                    std::span<const float> pivots = {});
  // O pivô do desenho `index` (0-based), em espaço do mesh.
  void pivotOf(u32 index, float out[3]) const;
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
  std::vector<resources::AssetGuid> assets_;
  // Três floats por desenho; vazio significa "pivô no centro dos limites".
  std::vector<float> pivots_;
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
