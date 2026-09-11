// O documento de cena do editor: o que a hierarquia mostra, o que o gizmo move
// e o que o Inspector edita.
//
// **Toda mutação passa por aqui, e o documento nunca é editado direto pela UI.**
// É o que torna undo/redo possível sem que cada painel se lembre de implementar
// o seu (ver editor_history.h). Um campo que a UI pudesse escrever sem passar
// por um comando seria um campo que o Ctrl+Z não desfaz — e o usuário não tem
// como saber quais são.
//
// Snapshots have value semantics, including optional owned data. Copying an
// entity never aliases mutable extension data with the document or history.
// An empty component collection has no payload allocation; reads do not allocate.
//
// **O armazenamento é `runtime::SceneGraph`** (ver runtime/scene_graph.h): o
// mesmo tipo de objeto que o mundo de execução possui. O documento acrescenta
// apenas as invariantes AUTORAIS — os limites das propriedades numéricas do
// Inspector — sobre um grafo que o runtime usa sem saber que existe um editor.
// A extração para lotes de renderização e para a ECS em C# é uma projeção dele,
// e mora em outro lugar.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once
#include "editor/editor_components.h"
#include "runtime/scene_components.h"

#include "core/base.h"
#include "scene/mesh_renderer.h"
#include "scene/camera.h"

#include <span>
#include <string_view>
#include <vector>

namespace ae::editor {

// Zero nunca é uma entidade válida: é o pai da raiz e o valor de "nada
// selecionado". Um id é estável pela vida do documento e nunca é reciclado,
// então um comando antigo no histórico nunca acerta um objeto diferente.
using EditorEntityId = runtime::ObjectId;
inline constexpr EditorEntityId kInvalidEntity = runtime::kInvalidObject;

using EditorEntityKind = runtime::ObjectKind;

inline constexpr u32 kEditorNameCapacity = runtime::kNameCapacity;

// Euler em graus, na ordem que o Inspector mostra (X, Y, Z).
using EditorTransform = runtime::Transform;
using EditorEntity = runtime::SceneObject;

using runtime::isTransformValid;

// Copia com truncamento e terminador garantido. Exposta porque o histórico e os
// testes precisam montar entidades sem passar pelo documento.
inline void assignEntityName(EditorEntity &entity, std::string_view name) noexcept {
  runtime::assignObjectName(entity, name);
}

// Acessores tipados: definidos em runtime/scene_components.h e reexportados
// aqui com os nomes que os arquivos do editor já usam.
using runtime::meshRenderer;
using runtime::editMeshRenderer;
using runtime::meshAsset;
using runtime::meshMaterial;
using runtime::cameraComponent;
using runtime::editCamera;

class EditorDocument final : public runtime::SceneGraph {
public:
  static constexpr u32 kMaximumEntities = runtime::SceneGraph::kMaximumObjects;

protected:
  // Acrescenta os limites autorais do Inspector à validação geral do grafo.
  // O runtime não os aplica: um valor recusado aqui nunca chega ao arquivo,
  // e um valor já salvo continua carregando pela mesma checagem.
  bool acceptObject(const runtime::SceneObject &object) const override;
};

} // namespace ae::editor
