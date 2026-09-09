#pragma once
#include "editor/editor_document.h"
#include <string>
namespace ae::editor {
std::string serializeEditorDocument(const EditorDocument &document, u64 packageFingerprint);
// Transactional: invalid, truncated or foreign-package input leaves document intact.
bool deserializeEditorDocument(std::string_view text, u64 packageFingerprint, EditorDocument &document);
bool saveEditorDocument(const char *path, const EditorDocument &document, u64 packageFingerprint);
bool loadEditorDocument(const char *path, u64 packageFingerprint, EditorDocument &document);
}
