#pragma once
#include "core/base.h"
#include "ui/ui_geometry.h"
#include <vector>

namespace ae::editor {
struct EditorScreenLayout final {
  float clipBoneRows=0; // linhas visiveis da arvore de ossos do estudio
  unsigned clipBoneCount=0; // ossos na arvore: limite da rolagem pelo dedo
  // Janela rolável da Biblioteca (lista ou diagnóstico) e a altura do conteúdo.
  ui::UiRect libraryScrollWindow{};float libraryScrollContent=0;
  // Painel de detalhe da Biblioteca (diagnóstico em chips), rolável à parte.
  ui::UiRect libraryDetailWindow{};float libraryDetailContent=0;
  // Áreas contínuas da janela de cor; o toque vira valor a partir delas.
  ui::UiRect colorSquare{},colorHue{};
  ui::UiRect colorSliders[5]{};
  ui::UiRect gradientBar{},gradientLocation{},gradientAlpha{};
  ui::UiRect curveGraph{};
  ui::UiRect animatorCanvas{};
  ui::UiRect clipCanvas{},clipRows{};
  u32 clipVisibleRows=0,clipPickerRows=0,clipTargetRows=0;
  ui::UiRect animatorDetailsWindow{},animatorParamsWindow{};
  float animatorDetailsExtent=0,animatorParamsExtent=0;
  ui::UiRect lodBar{};
  // Janela dos Inspectors focados (vazia quando nenhum está aberto).
  ui::UiRect focusedWindow{};
  // A área da cena, JÁ descontados os painéis. É este retângulo que
  // `EditorViewport::rect` recebe, e é por isso que a projeção do gizmo
  // continua certa quando o usuário arrasta um divisor.
  ui::UiRect viewport{};
  ui::UiRect topBar{};
  ui::UiRect statusBar{};
  ui::UiRect hierarchyPanel{};
  ui::UiRect filesPanel{};
  ui::UiRect inspectorPanel{};
  ui::UiRect diagnosticDock{};
  // Bloco F: linhas da lista de texturas das fontes desenhadas neste quadro;
  // só elas geram miniatura.
  std::vector<u32> visibleSourceTextureRows;
  // Inspector de textura: altura rolável e quanto dela cabe na janela.
  float textureInspectorContent=0,textureInspectorWindow=0;
  float multiAssetContent=0,multiAssetWindow=0;
  ui::UiRect componentOverviewWindow{};
  float componentOverviewContent=0;
  // Coluna rolável do Inspector (principal e focado): a janela visível e a
  // altura de tudo que foi empilhado nela.
  ui::UiRect inspectorScrollWindow{},focusedScrollWindow{};
  // Lista do menu de contexto: janela visível e altura total, para rolar.
  ui::UiRect contextMenuWindow{};
  float contextMenuContent=0;
  float inspectorScrollContent=0,focusedScrollContent=0;
  float texturePickerContent[2]{},texturePickerWindow[2]{};
  // O corpo do editor de código e quantas linhas dele cabem. O toque vira
  // posição de cursor a partir deste retângulo, e a rolagem acompanha o cursor
  // a partir desta contagem.
  ui::UiRect codeMenu{};
  ui::UiRect consolePanel{};
  u32 consoleVisibleRows = 0;
  u32 consoleRowCount = 0;
  ui::UiRect codeBody{};
  ui::UiRect codeAccessory{};
  ui::UiRect codeTabs{};
  u32 codeVisibleLines = 0;
  float codeLineHeight = 24.0f;
  u32 hierarchyRowCount = 0;
  u32 componentPage=0;
  // Linhas do catálogo do Add e quantas couberam; a sessão limita a rolagem.
  u32 addRowCount=0,addVisibleRows=0,addRailCount=0,addRailVisible=0;
  u32 creationRowCount=0,creationVisibleRows=0;
  // Quantas linhas caberiam. Menor que o total significa que há rolagem.
  u32 hierarchyVisibleRows = 0;
};

} // namespace ae::editor
