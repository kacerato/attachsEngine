#pragma once
#include "editor/editor_document.h"
#include "runtime/scene_components.h"
namespace ae::editor {
// Definidos em runtime/scene_components.h: a mesma regra vale para o documento
// autoral e para o mundo de execução, que compartilham o tipo de grafo.
inline bool editorReferenceAccepts(const runtime::SceneGraph &graph,EditorEntityId source,
    const scene::ComponentObjectReference &property,u64 target,bool requireTarget=false) {
  return runtime::referenceAccepts(graph,source,property,target,requireTarget);
}
inline bool editorReferencesAccept(const runtime::SceneGraph &graph,EditorEntityId source,const scene::ComponentValue &value) {
  return runtime::referencesAccept(graph,source,value);
}
}
