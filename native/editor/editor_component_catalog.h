#pragma once
#include "editor/editor_properties.h"
#include "scene/component_schema.h"
#include "ui/ui_icon_id.h"

#include <string_view>
#include <vector>

namespace ae::editor {
// O catálogo anexável do inspetor é uma VISTA do registro de schemas.
//
// Nome, descrição, família, ícone, termos de busca, exigências e
// incompatibilidades vêm de `scene::componentSchemas`, que também alimenta o
// mundo de execução, o arquivo e, pelo mundo, a API em C#. Antes existia aqui
// uma segunda lista, com ícone e grupo de propriedades por tipo, e cada tipo
// novo precisava ser lembrado nas duas — a mesma divergência que o schema
// comum veio eliminar. O que a UI acrescenta é só a resolução do nome do ícone
// para o índice do atlas, feita uma vez.
struct EditorComponentEntry {
  const scene::ComponentSchema *schema;
  const EditorComponentType *type;
  const char *name;
  const char *description;
  scene::ComponentFamily family;
  ui::UiIcon icon;
  // Termos de descoberta de outras engines; não alteram o TypeId nem prometem
  // equivalência de API. A composição continua vindo do schema Astra.
  std::string_view searchTerms;
  // Motivo pelo qual o tipo não pode ser anexado a esta entidade, ou nullptr.
  const char *unavailable(const EditorEntity &entity) const {
    return scene::componentAdditionBlockedReason(*schema, entity.components);
  }
};

// Índice do atlas para o nome do catálogo de ícones, ou None quando o nome não
// existe. Chamado só ao montar o catálogo; um teste exige que todo schema
// resolva, então um nome errado falha no build de testes e não no aparelho.
inline ui::UiIcon editorIconByName(std::string_view name) {
  if(name.empty()) return ui::UiIcon::None;
  for(u32 index=1;index<=ui::kUiIconCount;++index)
    if(name==ui::uiIconName(static_cast<ui::UiIcon>(index))) return static_cast<ui::UiIcon>(index);
  return ui::UiIcon::None;
}

namespace detail {
inline std::vector<EditorComponentEntry> buildEditorComponentCatalog() {
  std::vector<EditorComponentEntry> entries;
  entries.reserve(scene::componentSchemas.size());
  for(const auto &schema:scene::componentSchemas) {
    if(!schema.listedInAdd) continue;
    entries.push_back({&schema,schema.type,schema.name,schema.description,schema.family,
                       editorIconByName(schema.icon),schema.searchTerms});
  }
  return entries;
}
} // namespace detail

// Os tipos com consumidor implementado, na ordem das famílias. Comportamento
// C# é anexado pela área de código, com o script escolhido, e por isso não
// aparece aqui (continua registrado e persistido pelo schema).
inline const std::vector<EditorComponentEntry> editorComponentCatalog=detail::buildEditorComponentCatalog();

inline const EditorComponentEntry *findEditorComponent(std::string_view id) {
  for(const auto &entry:editorComponentCatalog) if(entry.type && entry.type->id==id) return &entry;
  return nullptr;
}
inline u32 editorComponentIndex(std::string_view id) {
  for(u32 i=0;i<editorComponentCatalog.size();++i) if(editorComponentCatalog[i].type->id==id) return i;
  return static_cast<u32>(editorComponentCatalog.size());
}
} // namespace ae::editor
