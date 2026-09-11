#include "editor/editor_document.h"
#include "editor/editor_properties.h"

namespace ae::editor {

bool EditorDocument::acceptObject(const runtime::SceneObject &object) const {
  return runtime::SceneGraph::acceptObject(object) && validEditorAppearance(object);
}

} // namespace ae::editor
