// A tela do editor: o que os mockups mostram, montado a partir do documento.
//
// **Os painéis flutuam sobre o viewport, não o encolhem.** É a decisão de layout
// mais visível dos masters: a cena aparece atrás e ao redor da hierarquia e do
// Inspector, com cantos arredondados por cima dela. Um editor de desktop divide
// a janela em regiões porque tem janela de sobra; num celular em paisagem, dar
// 800 px de largura a dois painéis deixaria o viewport com uma fresta. Por isso
// o viewport é a tela inteira e os painéis são cartões ancorados nas bordas.
//
// A consequência que importa: o toque num painel NÃO pode cair na cena por trás.
// Cada painel registra um bloqueio no roteador (ver ui_input.h) antes de
// registrar os próprios controles.
//
// Esta camada não guarda estado. Ela recebe um retrato do editor e produz a
// lista de desenho mais as regiões de toque do frame; quem responde ao toque é
// quem chamou, com o documento e o histórico na mão.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_document.h"
#include "editor/editor_gizmo.h"
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

  DockAdd,
  DockAssets,
  DockLighting,
  DockPlay,
  DockSettings,

  HierarchyAdd,
  HierarchyMenu,
  InspectorMenu,
  InspectorActive,
  TabTransform,
  TabMaterial,
  TabProperties,

  ToggleVisible,
  ToggleCastShadow,
  ToggleReceiveShadow,
  ToggleStatic,
  LayerDropdown,

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

enum class EditorDock : u8 { Add, Assets, Lighting, Play, Settings };
enum class EditorInspectorTab : u8 { Transform, Material, Properties };

struct EditorScreenState final {
  // Superfície inteira em pixels lógicos, incluindo o que fica sob o recorte da
  // câmera. As áreas seguras entram por `safeArea`, não encolhendo isto.
  ui::UiRect surface{};
  ui::UiInsets safeArea{};
  const EditorDocument *document = nullptr;
  EditorEntityId selection = kInvalidEntity;
  // Entidade sob o dedo agora, para o realce de pressionado.
  u32 pressedWidget = 0;
  EditorGizmoMode tool = EditorGizmoMode::Translate;
  EditorDock dock = EditorDock::Add;
  EditorInspectorTab tab = EditorInspectorTab::Transform;
  bool hierarchyVisible = true;
  bool inspectorVisible = true;
  const char *projectName = "Untitled";
  const char *projectSubtitle = "Scene";
  bool canUndo = false;
  bool canRedo = false;
};

struct EditorScreenLayout final {
  // Onde a cena 3D é útil: a superfície inteira menos nada, porque os painéis
  // flutuam. É este retângulo que `EditorViewport::rect` recebe, e é por isso
  // que a projeção do gizmo continua certa com os painéis abertos.
  ui::UiRect viewport{};
  ui::UiRect topBar{};
  ui::UiRect dock{};
  ui::UiRect hierarchyPanel{};
  ui::UiRect inspectorPanel{};
  // Linhas visíveis da hierarquia, na ordem em que foram desenhadas. Quem trata
  // o toque usa isto para saber o que foi tocado sem repetir a travessia.
  u32 hierarchyRowCount = 0;
};

// Monta o frame. `router` recebe os bloqueios e as regiões; `list` recebe o
// desenho. As duas coisas acontecem na mesma passagem de propósito: uma região
// registrada num lugar e desenhada em outro é a origem clássica do botão que
// responde onde não está.
EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const ui::UiTheme &theme,
                                     ui::UiDrawList &list, ui::UiInputRouter &router);

} // namespace ae::editor
