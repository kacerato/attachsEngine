// A tela do editor: o que os mockups mostram, montado a partir do documento.
//
// **A tela é uma divisão, não uma sobreposição: painel | viewport | painel.**
// A primeira versão tinha os painéis flutuando sobre a cena, como nos masters.
// Ficou bonito e errado: com o viewport ocupando a tela inteira, metade do que
// a câmera enquadra vive permanentemente atrás de um painel, e não há como
// enquadrar um objeto sem fechar a interface. Um editor precisa que a área
// visível seja a área utilizável. Os painéis agora tomam largura de verdade, e
// a largura deles é ajustável pelo usuário — que é a única resposta honesta
// para "quanto espaço a hierarquia merece", porque depende da cena.
//
// **Os modos vivem em abas no topo, não numa doca embaixo.** Numa tela em
// paisagem a borda inferior é a mais cara: é onde o polegar cobre o conteúdo e
// onde a barra de gestos do sistema disputa o toque. As abas no topo também
// deixam claro que elas trocam o CONTEXTO do viewport, e não executam uma ação.
//
// Esta camada não guarda estado próprio. `EditorScreenState` é do chamador, que
// o passa para desenhar e o entrega de volta para `applyEditorPointer` mutar.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_document.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_history.h"
#include "editor/editor_view.h"
#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"
#include "ui/ui_theme.h"

namespace ae::editor {

// Identidade dos controles. Os valores fixos são os controles únicos; as faixas
// no fim são para os que existem por entidade ou por eixo, onde o índice entra
// no próprio identificador.
enum class EditorWidget : u32 {
  None = 0,
  Undo,
  Redo,
  OpenProject,
  ProjectMenu,
  PlayFromTopBar,
  SceneChip,

  TabScene,
  TabAssets,
  TabLighting,
  TabPlay,
  TabSettings,

  HierarchyAdd,
  HierarchyMenu,
  HierarchyToggle,
  InspectorMenu,
  InspectorActive,
  InspectorToggle,
  InspectorTabTransform,
  InspectorTabMaterial,
  InspectorTabProperties,

  ToggleVisible,
  ToggleCastShadow,
  ToggleReceiveShadow,
  ToggleStatic,

  SplitterLeft,
  SplitterRight,

  ViewModeImage,
  ViewModeSolid,
  Fullscreen,

  ToolSelect,
  ToolMove,
  ToolRotate,
  ToolScale,

  // Faixas. O identificador de uma linha da hierarquia é a base mais o id da
  // entidade, o que dispensa uma tabela de tradução por frame.
  HierarchyRowBase = 0x1000'0000u,
  HierarchyEyeBase = 0x2000'0000u,
  TransformFieldBase = 0x3000'0000u,  // + linha * 3 + eixo
  GizmoAxisBase = 0x4000'0000u,       // + eixo
};

inline constexpr u32 widgetId(EditorWidget widget) noexcept { return static_cast<u32>(widget); }
inline constexpr u32 hierarchyRowWidget(EditorEntityId entity) noexcept {
  return widgetId(EditorWidget::HierarchyRowBase) + entity;
}
inline constexpr u32 hierarchyEyeWidget(EditorEntityId entity) noexcept {
  return widgetId(EditorWidget::HierarchyEyeBase) + entity;
}
inline constexpr u32 transformFieldWidget(u32 row, u32 axis) noexcept {
  return widgetId(EditorWidget::TransformFieldBase) + row * 3 + axis;
}
inline constexpr u32 gizmoAxisWidget(u32 axis) noexcept {
  return widgetId(EditorWidget::GizmoAxisBase) + axis;
}

// O que o viewport está mostrando. É a aba do topo, e trocá-la troca o contexto
// inteiro — não é um botão que dispara algo.
enum class EditorWorkspace : u8 { Scene, Assets, Lighting, Play, Settings };
enum class EditorInspectorTab : u8 { Transform, Material, Properties };

struct EditorScreenState final {
  // Superfície inteira em pixels lógicos (dp), incluindo o que fica sob o
  // recorte da câmera. As áreas seguras entram por `safeArea`.
  ui::UiRect surface{};
  ui::UiInsets safeArea{};
  const EditorDocument *document = nullptr;
  EditorEntityId selection = kInvalidEntity;
  // Widget sob o dedo agora, para o realce de pressionado.
  u32 pressedWidget = 0;
  EditorGizmoMode tool = EditorGizmoMode::Translate;
  EditorWorkspace workspace = EditorWorkspace::Scene;
  EditorInspectorTab tab = EditorInspectorTab::Transform;
  // Largura dos painéis em dp. Zero pede o padrão proporcional; qualquer outro
  // valor é o que o usuário arrastou e é preservado entre frames.
  float hierarchyWidth = 0.0f;
  float inspectorWidth = 0.0f;
  bool hierarchyVisible = true;
  bool inspectorVisible = true;
  // Quantas linhas a hierarquia já rolou, em linhas inteiras.
  u32 hierarchyScroll = 0;
  // Câmera do viewport. Sem ela a cena continua aparecendo, mas sem grade nem
  // gizmo — que é exatamente a diferença entre um preview e um editor.
  const EditorViewport *view = nullptr;
  bool showGrid = true;
  EditorGizmoHandle activeGizmoAxis = EditorGizmoHandle::None;
  const char *projectName = "Untitled";
  bool canUndo = false;
  bool canRedo = false;
};

struct EditorScreenLayout final {
  // A área da cena, JÁ descontados os painéis. É este retângulo que
  // `EditorViewport::rect` recebe, e é por isso que a projeção do gizmo
  // continua certa quando o usuário arrasta um divisor.
  ui::UiRect viewport{};
  ui::UiRect topBar{};
  ui::UiRect hierarchyPanel{};
  ui::UiRect inspectorPanel{};
  u32 hierarchyRowCount = 0;
  // Quantas linhas caberiam. Menor que o total significa que há rolagem.
  u32 hierarchyVisibleRows = 0;
};

// Monta o frame. `router` recebe os bloqueios e as regiões; `list` recebe o
// desenho. As duas coisas acontecem na mesma passagem de propósito: uma região
// registrada num lugar e desenhada em outro é a origem clássica do botão que
// responde onde não está.
EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const ui::UiTheme &theme,
                                     ui::UiDrawList &list, ui::UiInputRouter &router);

// O que um ponteiro fez com a tela. Devolvido para que o chamador saiba se
// precisa reagir fora da interface — mover a câmera, por exemplo.
struct EditorPointerOutcome final {
  bool consumed = false;
  // O toque pertence à cena: orbitar, selecionar ou arrastar o gizmo.
  bool viewport = false;
  bool documentChanged = false;
  bool requestPlay = false;
};

// Aplica um evento de ponteiro já roteado. Muta `state` (seleção, aba, largura
// dos painéis) e o documento através do histórico — nunca direto, para que tudo
// que o dedo faz seja desfazível.
EditorPointerOutcome applyEditorPointer(EditorScreenState &state,
                                        const EditorScreenLayout &layout,
                                        const ui::UiPointerRouting &routing,
                                        EditorDocument &document, EditorHistory &history);

} // namespace ae::editor
