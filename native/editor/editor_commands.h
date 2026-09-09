#pragma once
#include "editor/editor_document.h"
#include <string>

namespace ae::editor {
// Fronteira de ferramentas: valores copiados, sem ponteiros para a cena.
// Chamadas são processadas na thread da sessão, nunca diretamente da UI Android.
struct EditorSceneVersion { u64 epoch=0, revision=0; };
enum class EditorAction { Select, Rename, Transform, NumericProperty, Duplicate, Remove,
                          Reparent, Undo, Redo, FrameSelection };
enum class EditorActionStatus { Applied, StaleScene, Busy, InvalidTarget, InvalidValue };
struct EditorActionRequest {
  EditorSceneVersion version;
  EditorAction action=EditorAction::Select;
  EditorEntityId entity=0, parent=0;
  std::string name;
  EditorTransform transform;
  u32 property=0;
  float number=0;
};
struct EditorActionResult {
  EditorActionStatus status=EditorActionStatus::InvalidValue;
  EditorSceneVersion version;
  EditorEntityId entity=0;
};
}
