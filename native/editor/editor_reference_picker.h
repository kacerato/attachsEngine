#pragma once
#include "editor/editor_component_references.h"
#include "editor/editor_creation_catalog.h"
namespace ae::editor {
inline const scene::ComponentObjectReference *editorReferenceProperty(const EditorEntity &entity,u64 instance,std::string_view id) {
  const auto *component=entity.components.findInstance(instance);if(!component) return nullptr;
  for(const auto &p:component->type().references) if(p.id==id) return &p;
  return nullptr;
}
inline const scene::ComponentObjectReference editorAnyObjectReference{"object","Objeto","",scene::ObjectReferenceScope::Any,"Nenhum"};
inline std::vector<EditorEntityId> editorReferenceChoices(const EditorDocument &document,EditorEntityId source,
    const scene::ComponentObjectReference &property,std::string_view search) {
  std::vector<EditorEntityId> ids,result;document.collectSubtree(document.root(),ids);const auto query=editorSearchKey(search);
  for(auto id:ids) if(editorReferenceAccepts(document,source,property,id,true)) {
    const auto *entity=document.find(id);
    if(query.empty()||editorSearchKey(std::string(entity->name)+" "+std::to_string(id)).find(query)!=std::string::npos) result.push_back(id);
  }
  return result;
}
}
