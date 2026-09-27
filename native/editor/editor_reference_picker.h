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
// Campo de script: tipo declarado "object" aceita qualquer objeto;
// "component:<id>" só objetos que tenham esse componente (o seletor e o
// arraste usam a mesma validação de `referenceAccepts`).
inline scene::ComponentObjectReference editorScriptReference(std::string_view declaredType) {
  const auto component=scene::scriptComponentTypeId(declaredType);
  if(component.empty()) return editorAnyObjectReference;
  return {"component","Componente",component,scene::ObjectReferenceScope::Any,"Nenhum"};
}
// Unity: arrastar um GameObject para um campo de componente atribui o
// primeiro componente daquele tipo no objeto. Zero quando não há nenhum.
inline u64 editorFirstComponentInstance(const runtime::SceneGraph &graph,u64 object,std::string_view type) {
  const auto *entity=object<=std::numeric_limits<EditorEntityId>::max()?graph.find(static_cast<EditorEntityId>(object)):nullptr;
  if(!entity) return 0;
  for(usize i=0;i<entity->components.size();++i)
    if(entity->components.at(i)->type().id==type) return entity->components.at(i)->instanceId();
  return 0;
}
// Valor que o campo guarda para `target`: o id do objeto, ou objeto:instância
// do primeiro componente do tipo. Vazio quando o objeto não serve.
inline std::string editorScriptReferenceValue(const runtime::SceneGraph &graph,std::string_view declaredType,u64 target) {
  const auto component=scene::scriptComponentTypeId(declaredType);
  if(component.empty()) return std::to_string(target);
  if(!target) return scene::scriptComponentValue(0,0);
  const u64 instance=editorFirstComponentInstance(graph,target,component);
  return instance?scene::scriptComponentValue(target,instance):std::string();
}
inline std::vector<EditorEntityId> editorReferenceChoices(const runtime::SceneGraph &document,EditorEntityId source,
    const scene::ComponentObjectReference &property,std::string_view search) {
  std::vector<EditorEntityId> ids,result;document.collectSubtree(document.root(),ids);const auto query=editorSearchKey(search);
  for(auto id:ids) if(editorReferenceAccepts(document,source,property,id,true)) {
    const auto *entity=document.find(id);
    if(query.empty()||editorSearchKey(std::string(entity->name)+" "+std::to_string(id)).find(query)!=std::string::npos) result.push_back(id);
  }
  return result;
}
}
