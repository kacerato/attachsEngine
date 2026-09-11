#pragma once
#include "editor/editor_screen.h"
#include "ui/ui_icon_id.h"
#include <array>
#include "ui/ui_text.h"
#include <string>

namespace ae::editor {
inline std::string editorSearchKey(std::string_view text) {
  std::string key;
  for(usize cursor=0;cursor<text.size();) {
    auto c=ui::uiUppercase(ui::nextUiCodepoint(text,cursor));
    if(c>=0xc0&&c<=0xc5)c='A';else if(c==0xc7)c='C';
    else if(c>=0xc8&&c<=0xcb)c='E';else if(c>=0xcc&&c<=0xcf)c='I';
    else if(c>=0xd2&&c<=0xd6)c='O';else if(c>=0xd9&&c<=0xdc)c='U';
    if(c<128)key.push_back(static_cast<char>(c));
  }
  return key;
}
// Catálogo de ferramentas de autoria, separado do registro de componentes do
// runtime. Cada entrada aponta para uma operação existente e transacional.
struct EditorCreationEntry {
  EditorWidget action;
  unsigned category;
  const char *name;
  const char *description;
  ui::UiIcon icon;
};
inline constexpr const char *creationCategories[]{"Básicos","Geometria","Água","Física"};
inline constexpr std::array<EditorCreationEntry,9> editorCreationCatalog{{
  {EditorWidget::CreateGroup,0,"Objeto vazio","Organiza filhos e transforma o conjunto.",ui::UiIcon::EditorAuthorObject},
  {EditorWidget::CreateCamera,0,"Câmera","Captura a vista atual para executar a cena.",ui::UiIcon::EditorAuthorCamera},
  {EditorWidget::CreateCube,1,"Cubo","Malha com transformação e material editáveis.",ui::UiIcon::EditorAuthorObject},
  {EditorWidget::CreateGround,1,"Chão","Superfície geométrica para compor o cenário.",ui::UiIcon::EditorAuthorGrid},
  {EditorWidget::CreateFiniteWater,2,"Superfície de água","Volume finito com profundidade e corrente.",ui::UiIcon::WaterAuthorSurface},
  {EditorWidget::CreateOceanWater,2,"Oceano","Superfície extensa com ondas espectrais.",ui::UiIcon::WaterAuthorSurface},
  {EditorWidget::CreateRiverWater,2,"Rio por pontos","Traçado com largura, profundidade e fluxo.",ui::UiIcon::WaterAuthorRoute},
  {EditorWidget::CreateBuoyantBox,3,"Caixa flutuante","Corpo rígido com massa e arrasto na água.",ui::UiIcon::WaterAuthorPhysics},
  {EditorWidget::ImportModel,1,"Importar modelo","Abre um .glb do aparelho e traz suas malhas.",ui::UiIcon::EditorAuthorZoom}
}};
inline bool creationAvailable(const EditorScreenState &state,u32 index) {
  return index<editorCreationCatalog.size() && (state.creationAvailable & (1u<<index));
}
// Temporary capability adapter for the existing imported water library.
inline bool waterCreationAvailable(const EditorScreenState &state) {
  return creationAvailable(state,4) || creationAvailable(state,5) || creationAvailable(state,6);
}
}
