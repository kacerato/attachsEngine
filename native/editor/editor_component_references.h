#pragma once
#include "editor/editor_document.h"
namespace ae::editor {
// Drafts may retain a null reference; execution requires a compatible live target.
inline bool editorReferenceAccepts(const EditorDocument &document,EditorEntityId source,
    const scene::ComponentObjectReference &property,u64 target,bool requireTarget=false) {
  if(!target && !requireTarget) return true;
  if(!target && property.scope==scene::ObjectReferenceScope::SelfOrAncestor) target=source;
  if(!target || target>std::numeric_limits<EditorEntityId>::max()) return false;
  const auto id=static_cast<EditorEntityId>(target);const auto *entity=document.find(id);
  if(!entity || (!property.requiredType.empty()&&!entity->components.find(property.requiredType))) return false;
  if(property.scope==scene::ObjectReferenceScope::Other && id==source) return false;
  if(property.scope==scene::ObjectReferenceScope::SelfOrAncestor && id!=source&&!document.isDescendantOf(source,id)) return false;
  if(property.scope==scene::ObjectReferenceScope::SelfOrAncestor && !property.requiredType.empty())
    for(auto p=source;p!=id;p=document.find(p)->parent)
      if(document.find(p)->components.find(property.requiredType)) return false;
  return true;
}
inline bool editorReferencesAccept(const EditorDocument &document,EditorEntityId source,const scene::ComponentValue &value) {
  for(const auto &property:value.type().references)
    if(!editorReferenceAccepts(document,source,property,property.read(value))) return false;
  return true;
}
}
