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
// Seletor avançado (Unity 6000.0 Manual/search-advanced-object-picker): a
// consulta aceita "t:<tipo>" (tem um componente cujo nome ou id contém o
// texto) e palavras soltas (o nome contém). Com o filtro de tipo ligado, só
// aparecem objetos que têm o tipo exigido pelo campo; desligado, todos. Os
// que o campo recusa (escopo, falta do tipo) vêm marcados, não escondidos.
struct EditorReferenceResult {
  EditorEntityId id=0;
  bool compatible=false;
};
inline bool editorEntityHasComponent(const EditorEntity &entity,std::string_view key) {
  for(usize i=0;i<entity.components.size();++i) {
    const auto &type=entity.components.at(i)->type();
    const auto *schema=scene::findComponentSchema(type.id);
    if(editorSearchKey(type.id).find(key)!=std::string::npos) return true;
    if(schema && editorSearchKey(schema->name).find(key)!=std::string::npos) return true;
    if(const auto *script=scene::scriptBehavior(entity.components.at(i)))
      if(editorSearchKey(script->scriptType).find(key)!=std::string::npos) return true;
  }
  return false;
}
inline std::vector<EditorReferenceResult> editorAdvancedReferenceResults(const runtime::SceneGraph &document,EditorEntityId source,
    const scene::ComponentObjectReference &property,std::string_view query,bool typeFilter) {
  std::vector<std::string> types,words;
  std::string token;
  const auto flush=[&] {
    if(token.empty()) return;
    if(token.size()>2 && (token[0]=='t'||token[0]=='T') && token[1]==':') types.push_back(editorSearchKey(token.substr(2)));
    else words.push_back(editorSearchKey(token));
    token.clear();
  };
  for(const char c:query) {if(c==' ') flush();else token+=c;}
  flush();
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  std::vector<EditorReferenceResult> results;
  for(const auto id:ids) {
    if(id==document.root()) continue;
    const auto *entity=document.find(id);
    if(!entity) continue;
    if(typeFilter && !property.requiredType.empty() && !entity->components.find(property.requiredType)) continue;
    bool match=true;
    for(const auto &type:types) match=match && editorEntityHasComponent(*entity,type);
    const auto name=editorSearchKey(std::string(entity->name)+" "+std::to_string(id));
    for(const auto &word:words) match=match && name.find(word)!=std::string::npos;
    if(!match) continue;
    results.push_back({id,editorReferenceAccepts(document,source,property,id,true)});
  }
  return results;
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
inline std::vector<EditorEntityId> editorRecipeReferenceChoices(const runtime::SceneGraph &document,
    std::span<const EditorEntityId> compatible,std::string_view search) {
  std::vector<EditorEntityId> result;const auto query=editorSearchKey(search);
  for(const auto id:compatible)if(const auto *entity=document.find(id);entity &&
      (query.empty() || editorSearchKey(std::string(entity->name)+" "+std::to_string(id)).find(query)!=std::string::npos))result.push_back(id);
  return result;
}
inline std::vector<EditorReferenceResult> editorRecipeReferenceResults(const runtime::SceneGraph &document,
    EditorEntityId source,std::span<const EditorEntityId> compatible,std::string_view query,bool filter) {
  auto results=editorAdvancedReferenceResults(document,source,editorAnyObjectReference,query,false);
  for(auto &r:results)r.compatible=std::find(compatible.begin(),compatible.end(),r.id)!=compatible.end();
  if(filter)std::erase_if(results,[](const auto &r){return !r.compatible;});
  return results;
}
}
