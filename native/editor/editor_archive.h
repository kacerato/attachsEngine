#pragma once
#include "editor/editor_document.h"
#include <string>
namespace ae::editor {
using EditorComponentRegistry=std::span<const EditorComponentType *const>;
EditorComponentRegistry defaultEditorComponentRegistry();
std::string serializeEditorDocument(const EditorDocument &document, u64 packageFingerprint);
// Transactional: invalid, truncated or foreign-package input leaves document intact.
bool deserializeEditorDocument(std::string_view text, u64 packageFingerprint, EditorDocument &document, EditorComponentRegistry registry=defaultEditorComponentRegistry());
bool saveEditorDocument(const char *path, const EditorDocument &document, u64 packageFingerprint, EditorComponentRegistry registry=defaultEditorComponentRegistry());
bool loadEditorDocument(const char *path, u64 packageFingerprint, EditorDocument &document, EditorComponentRegistry registry=defaultEditorComponentRegistry());
}
