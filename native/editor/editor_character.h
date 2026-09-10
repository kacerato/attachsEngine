#pragma once
#include "editor/editor_document.h"
#include "scene/character.h"
namespace ae::editor {
using EditorCharacter=scene::Character;
using scene::characterNumbers;
inline const EditorCharacter *characterComponent(const EditorEntity &e) {
  return static_cast<const EditorCharacter*>(e.components.find(EditorCharacter::descriptor));
}
inline EditorCharacter *editCharacter(EditorEntity &e) {
  return static_cast<EditorCharacter*>(e.components.edit(EditorCharacter::descriptor));
}
}
