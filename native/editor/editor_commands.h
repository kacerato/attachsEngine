#pragma once
#include "editor/editor_document.h"
#include "scene/component_properties.h"
#include <string>

namespace ae::editor {
// Fronteira de ferramentas: valores copiados, sem ponteiros para a cena.
// Chamadas são processadas na thread da sessão, nunca diretamente da UI Android.
struct EditorSceneVersion { u64 epoch=0, revision=0; };
enum class EditorAction { Select, Rename, Transform, NumericProperty, Duplicate, Remove,
                          Reparent, Undo, Redo, FrameSelection, ComponentProperty, ComponentResource,
                          AddComponent, AddScript, RemoveComponent, ScriptProperty, ScriptEnabled,
                          FitCollider, GenerateCollisionMesh, AssignMesh, RestoreMaterial,
                          // Valor por endereço (`componentSlot`): peso de blend shape, parâmetro por slot.
                          ComponentSlotProperty };
enum class EditorActionStatus { Applied, StaleScene, Busy, InvalidTarget, InvalidValue };
struct EditorActionRequest {
  EditorSceneVersion version;
  EditorAction action=EditorAction::Select;
  EditorEntityId entity=0, parent=0;
  std::string name;
  EditorTransform transform;
  u32 property=0;
  float number=0;
  std::string componentType,componentProperty;
  scene::ComponentPropertyValue componentValue=0.0f;
  u64 componentInstance=0;
  resources::AssetGuid componentResource{};
  u32 componentResourceSlot=0,componentSlot=0;
  std::string scriptType,scriptPropertyType,scriptPropertyValue;
  bool enabled=true;
};
struct EditorActionResult {
  EditorActionStatus status=EditorActionStatus::InvalidValue;
  EditorSceneVersion version;
  EditorEntityId entity=0;
};
}
