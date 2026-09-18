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
#include "editor/editor_filesystem.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_console.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_history.h"
#include "editor/editor_view.h"
#include "scene/component_preset.h"
#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"
#include "ui/ui_theme.h"
#include <string>

namespace ae::editor {
class EditorMapScene;

// Uma linha do diff de um preset, já com a escolha do autor.
//
// O endereço vem de `scene::FieldAddress` e aponta para a identidade
// persistente da propriedade — nunca para o índice da linha na tela. Uma lista
// reordenada, paginada ou filtrada continua aplicando o mesmo campo.
struct EditorPresetField {
  scene::FieldAddress address;
  std::string label,current,candidate;
  bool differs=false,applicable=false,selected=false;
};

// Identidade dos controles. Os valores fixos são os controles únicos; as faixas
// no fim são para os que existem por entidade ou por eixo, onde o índice entra
// no próprio identificador.
enum class EditorWidget : u32 {
  CreationCategoryBase=0x63000000,
  CreationRowBase=0x64000000,
  None = 0,
  FilesUp=0x65000000,FilesRefresh,FilesPrevious,FilesNext,FilesSplitter,FilesCollapse,FilesRename,FilesDelete,
  ConsoleInfo,ConsoleWarning,ConsoleError,ConsoleClear,ConsoleCollapse,
  ConsoleProblems,ConsoleLogs,ConsoleSource,ConsoleSearch,ConsoleFollow,ConsoleExpand,
  ConsoleOpenSource,ConsoleCopy,ConsoleExport,ConsoleDetailClose,ConsoleDetailNext,ConsoleDetailPrevious,
  ConsoleResize,
  FileRowBase=0x66000000,
  ConsoleRowBase=0x51000000,
  Undo=1,
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
  HierarchySearch,
  HierarchyExpandAll,
  HierarchyCollapseAll,
  HierarchyClearSearch,
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
  NavigationOrbit,
  NavigationPan,
  NavigationZoom,
  FrameSelection,
  FrameAll,
  DeleteSelection,
  DuplicateSelection,
  CreateGroup,
  AssetsPrevious,
  AssetsNext,
  SaveDocument,
  PropertyPrevious,
  PropertyNext,
  ReparentSelection,
  MoveToRoot,
  MoveEarlier,
  MoveLater,
  RenameSelection,
  NameApply,
  NameCancel,
  NameBackspace,
  NameClear,
  NameShift,
  NumericApply,
  NumericCancel,
  NumericBackspace,
  NumericClear,
  CreateFiniteWater,
  CreateOceanWater,
  CreateRiverWater,
  CreateBuoyantBox,
  ImportModel,
  ImportAccept,
  ImportIntoScene,
  ImportPreviousPage,
  ImportNextPage,
  ImportCancel,
  CodeRecover,
  CodeDiscardRecovery,
  AssetInstantiate,
  AssetReimport,
  CreateMenuClose,
  CreationSearch,
  CreationClearSearch,
  CreationPrevious,
  CreationNext,
  CreateCamera,
  CreateGround,
  CreateCube,
  WorkspaceMenuClose,
  WaterTabSurface,
  WaterTabRoute,
  WaterTabPhysics,
  WaterTabEffects,
  RoutePointAdd,
  RoutePointRemove,
  RoutePointPrevious,
  RoutePointNext,
  ToggleWaterPhysics,
  ToggleRigidBody,
  CompactViewport,
  CompactFiles,
  CompactPanelMenu,
  ToggleSceneBody,
  ToggleDynamicBody,
  PausePlay,
  StepPlay,
  ToggleCharacter,
  JumpCharacter,
  PlaySecondaryAction,
  ToggleCameraLook,
  AddComponentMenu,
  CodeOpen, CodeScene, CodeNew, CodeEdit, CodeSave, CodeUndo, CodeRedo, CodeSearch, CodeClose, CodeApply,
  CodeMenu, CodeSaveAll, CodeFiles, CodeTabsPrevious, CodeTabsNext, CodeFindPrevious, CodeFindNext,
  CodeGoLine, CodeNewFolder, CodeNewHelper, CodeTemplates,
  ColliderFit, ComponentPrevious, ComponentNext, ScriptFieldsPrevious, ScriptFieldsNext,
  TransformFold, ComponentSearch, ComponentSearchClear, ComponentCategory,
  MeshGeometryTab, MeshMaterialTab, MeshChoose, MeshPickerClose, MeshClear, MeshSearch, MeshPrevious, MeshNext, MaterialRestore,
  ReferenceClose, ReferenceSearch, ReferenceClear, ReferencePrevious, ReferenceNext,
  CodeTemplateClose, CodeConsole,
  ImportMatchInOrder, ImportTreatAsNew, ImportLinkMenu, ImportLinkUnlink, ImportLinkKeep, ImportLinkDelete,
  MaterialSlotPrevious, MaterialSlotNext, MaterialScopeInstance, MaterialScopeShared, MaterialChoose,
  MaterialPickerClose, MaterialClearOverride, MaterialCreateShared, MaterialUseSource,
  // Inspetor: configurações universais do objeto e ações que antes só existiam
  // no menu da hierarquia (padrão Unity/Godot: o objeto selecionado se edita
  // onde estão as propriedades dele).
  ObjectFold, ObjectLayerPrevious, ObjectLayerNext, CreateChildGroup,
  TransformMenu, TransformCopy, TransformPaste, TransformReset,
  TransformResetPosition, TransformResetRotation, TransformResetScale,
  // R3: importação no painel de Propriedades (abas, perfil e ações do perfil).
  ImportTabSummary, ImportTabStructure, ImportTabTextures, ImportTabProfile,
  ImportScaleDown, ImportScaleUp,
  ImportTextureDimension256, ImportTextureDimension512, ImportTextureDimension1024, ImportTextureDimension2048,
  ImportApplyProfile, ImportSaveDefaultProfile,
  // G2: geometria derivada no perfil — normais, ponderação e tangentes.
  ImportNormalsCycle, ImportNormalWeightingCycle, ImportTangentsCycle, ImportCamerasToggle,
  // R4: textura por binding de material e extração das imagens de um GLB.
  TexturePickerClose, TextureUseInherited, TextureUseNone, AssetExtractTextures,
  MaterialAlphaCycle, MaterialCutoffDown, MaterialCutoffUp, MaterialSidesCycle,
  MaterialOcclusionSourceCycle, MaterialOcclusionTexture, MaterialOcclusionStrengthDown, MaterialOcclusionStrengthUp,
  MaterialChannelRoughness, MaterialChannelMetallic, MaterialChannelOcclusion,
  MaterialNormalFlipCycle, MaterialAlphaSourceCycle, MaterialIsolateCycle,
  TextureViewerClose, TextureViewerChannel, TextureViewerMipDown, TextureViewerMipUp,
  TextureViewerZoom, TextureViewerBackground,
  TextureProfileInterpretation, TextureProfileDimension, TextureProfileMipmaps, TextureProfileEdges, TextureProfileAnisotropy,
  TextureManagerClose, TextureSearch, TextureManagerPrevious, TextureManagerNext,
  TextureSamplingUv, TextureSamplingWrap, TextureSamplingFilter, TextureUvReset,
  // + máscara de ImportOverride.
  ImportLinkRevertBase=0x52000000u,
  // + índice do material do projeto / do campo numérico do material.
  MaterialChoiceBase=0x53000000u, MaterialNumberBase=0x54000000u,
  // + binding de textura (0..3) / + índice da textura do projeto.
  MaterialTextureBase=0x55000000u, TextureChoiceBase=0x56000000u,
  // + índice da textura do projeto: abre o visualizador.
  TextureViewBase=0x57000000u,
  // + campo*2 (+1 para somar): deslocamento U/V, escala U/V, rotação da UV do binding.
  TextureUvStepBase=0x58000000u,
  // R4: filtro do gerenciador, textura da grade e usuário da textura em Propriedades.
  TextureFilterBase=0x59000000u, TextureManagerRowBase=0x5A000000u, TextureUserBase=0x5B000000u,
  ComponentGroupBase=0x5C000000u, ComponentVisualBase=0x5D000000u,
  ComponentVisualsToggle=0x5E000000u, CameraView, CameraViewClose, CameraAlignView, CameraPilot,
  ComponentReferenceBase=0x7a000000u, ReferenceChoiceBase=0x7b000000u,
  CameraLens=0x5E000010u, CameraNear, CameraFar,
  CameraHandleBase=0x5E000020u,
  CameraPreviewPin=0x5E000030u, CameraPreviewClose, CameraPreviewResolution, CameraPreviewFrequency, CameraPreviewRetry,
  PresetOpen=0x5E000040u, PresetClose, PresetSave, PresetApply, PresetAdd, PresetRename, PresetDelete, PresetPrevious, PresetNext,
  // Escolha por campo do preset: marcar tudo, desmarcar tudo e paginar o diff.
  PresetSelectAll, PresetSelectNone, PresetFieldsPrevious, PresetFieldsNext, PresetSaveRecipe,
  PresetChoiceBase=0x5E010000u,
  // + índice da linha do diff: alterna levar ou não aquele campo.
  PresetFieldBase=0x5E020000u,
  // + linha da aba Estrutura da importação: inclui ou exclui aquele nó.
  ImportNodeToggleBase=0x5E030000u,
  ImpactOpenBase=0x60000000u, ImpactRowBase=0x63000000u, ImpactClose=0x64000000u, ImpactPrevious, ImpactNext, ImpactRepair, ImpactRepairApply, ImpactRepairShared, ImpactRepairScope,
  ComponentColorBase=0x7e000000u,
  ColorHueBase=0x5f000000u, ColorSvBase=0x5f000100u, ColorApply=0x5f000200u, ColorCancel,
  ComponentTripleBase=0x7d000000u,
  ComponentNumberBase=0x78000000u,
  MeshChoiceBase=0x79000000u,
  ScriptAddBase=0x71000000u, ScriptFoldBase=0x72000000u, ScriptMenuBase=0x73000000u,
  ScriptRemoveBase=0x74000000u, ScriptEnabledBase=0x75000000u, ScriptSourceBase=0x76000000u,
  ScriptFieldBase=0x77000000u,
  // Um id por modelo de script; o valor zero é o arquivo vazio.
  CodeTemplateBase=0x7c000000u,
  CodeBody=0x61000000u, CodeTabBase=0x61010000u,

  ComponentAddBase=0x67000000u,
  ComponentFoldBase=0x68000000u,
  ComponentRemoveBase=0x69000000u,
  ComponentBooleanBase=0x6a000000u,
  ComponentCopyBase=0x6b000000u,
  ComponentPasteBase=0x6c000000u,
  ComponentResetBase=0x6d000000u,
  ComponentMenuBase=0x6e000000u,
  ComponentEnumBase=0x6f000000u,
  RoutePointBase=0x62000000u,
  NumericKeyBase = 0x5000'0000u,


  // Faixas. O identificador de uma linha da hierarquia é a base mais o id da
  // entidade, o que dispensa uma tabela de tradução por frame.
  NameKeyBase = 0x7000'0000u,
  HierarchyCollapseBase = 0x8000'0000u,
  AssetRowBase = 0x6000'0000u,
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
enum class EditorWorkspace : u8 { Scene, Assets, Lighting, Play, Settings, Code };
enum class EditorNavigationMode : u8 { Orbit, Pan, Zoom };
enum class EditorInspectorTab : u8 { Transform, Material, Properties };

struct EditorScreenState final {
  // Superfície inteira em pixels lógicos (dp), incluindo o que fica sob o
  // recorte da câmera. As áreas seguras entram por `safeArea`.
  ui::UiRect surface{};
  ui::UiInsets safeArea{};
  const EditorDocument *document = nullptr;
  const EditorFileSystem *files=nullptr;
  const EditorCodeWorkspace *code=nullptr;
  const EditorMapScene *resources=nullptr;
  const resources::AssetRegistry *assetRegistry=nullptr;
  bool codeCompilerAvailable=false,codeBuildBusy=false;
  bool editingCode=false,creatingScript=false,searchingCode=false;
  u64 codeSearchRequest=0;
  // O console: compilador, scripts e editor no mesmo lugar.
  const EditorConsole *console = nullptr;
  // Quantas linhas o usuário subiu a partir do fim. Zero é o fim, que é onde um
  // console deve estar: a linha que acabou de aparecer é a que interessa.
  u32 consoleScroll = 0;
  bool consoleCollapsed = true;
  bool consoleProblems=false,consoleExpanded=false,consoleFollow=true,searchingConsole=false;
  float consoleFraction=.58f;
  u64 consoleSelected=0,consoleAnchor=0;
  u32 consoleDetailPage=0;
  float consoleDragRemainder=0;
  int consoleOrigin=-1;
  std::string consoleQuery;
  // O menu da barra do IDE. Uma lista que abre num ícone é o que tira da barra
  // tudo o que não é frequente, sem escondê-lo atrás de um gesto.
  bool codeMenu = false;
  bool codeFiles = false,platformCodeView=false,codeComposing=false;
  bool goingToLine=false,creatingCodeFolder=false;
  u32 codeFirstTab=0;
  // O arquivo escolhido no painel. As ações de recurso agem sobre ele, e é o
  // painel que decide qual é — não a seleção da cena, que é outra coisa.
  std::string selectedFile;
  bool renamingResource=false;
  // Apagar pede confirmação em DOIS toques, e não num diálogo. Guarda o caminho
  // que o primeiro toque recusou; o segundo, no mesmo caminho, confirma.
  std::string pendingResourceDelete;
  // Escolha do modelo antes de pedir o nome da classe. `~0u` é o arquivo
  // vazio, que continua sendo o primeiro item da lista.
  bool choosingTemplate=false;
  u32 scriptTemplate=~0u;
  std::string codeQuery;
  u32 fileScroll=0;
  float fileScrollOffset=0;
  float filePanelRatio=.46f;
  bool filesCollapsed=false;
  EditorEntityId selection = kInvalidEntity;
  // Widget sob o dedo agora, para o realce de pressionado.
  u32 pressedWidget = 0;
  EditorGizmoMode tool = EditorGizmoMode::Translate;
  EditorWorkspace workspace = EditorWorkspace::Scene;
  bool playHasScripts=false;
  bool playFirstPerson=false,playHasCharacter=false;
  std::string playSecondaryActionLabel,playHudMessage;
  bool playPaused=false;
  bool playStepRequested=false;
  EditorInspectorTab tab = EditorInspectorTab::Transform;
  // Folding is editor-only, keyed by object and stable component instance identity.
  EditorEntityId componentSelection=0;
  std::string expandedComponent;
  u64 expandedNative=0,nativeMenu=0;
  u64 referenceInstance=0;
  std::string referenceProperty,referenceQuery;
  bool referenceScript=false,editingReferenceSearch=false;
  u32 referencePage=0;
  bool addingComponent=false;
  bool editingComponentSearch=false,editingMeshSearch=false,meshPicker=false;
  std::string componentQuery,meshQuery;
  u32 componentCategory=0,meshPage=0,meshTab=0;
  u32 componentPage=0,scriptPropertyPage=0;
  u64 expandedScript=0,scriptMenu=0,editingScriptInstance=0;
  EditorEntityId editingScriptEntity=0;
  std::string editingScriptProperty,editingScriptType;
  std::shared_ptr<const EditorComponentValue> componentClipboard;
  bool presetPanel=false,presetNaming=false,presetRenaming=false,presetDeleteConfirm=false;
  // Salvar o OBJETO como receita é um terceiro destino do mesmo campo de nome;
  // sem distinguir, renomear e salvar disputariam o mesmo estado.
  bool presetRecipeNaming=false;
  // O preset em foco traz vários componentes; o painel troca o rótulo da ação.
  bool presetSelectedIsRecipe=false;
  EditorEntityId presetEntity=0;
  u64 presetInstance=0,presetSelected=0,presetEpoch=0,presetRevision=0;
  u32 presetPage=0;
  std::vector<std::pair<u64,std::string>> presetChoices;
  std::vector<std::string> presetPreview;
  // O diff campo a campo do preset selecionado. `selected` é a escolha do autor
  // sobre LEVAR aquele campo; começa marcado no que difere e é aplicável, que é
  // o comportamento antigo de "aplicar tudo" expresso pelo mesmo caminho.
  std::vector<EditorPresetField> presetFields;
  u32 presetFieldPage=0;
  std::string presetName;
  // Largura dos painéis em dp. Zero pede o padrão proporcional; qualquer outro
  // valor é o que o usuário arrastou e é preservado entre frames.
  float hierarchyWidth = 0.0f;
  float inspectorWidth = 0.0f;
  enum class CompactPanel { Viewport, Hierarchy, Inspector, Files };
  CompactPanel compactPanel = CompactPanel::Viewport;
  bool compactPanelMenu=false;
  bool hierarchyVisible = true;
  bool inspectorVisible = true;
  // Quantas linhas a hierarquia já rolou, em linhas inteiras.
  u32 hierarchyScroll = 0;
  float hierarchyScrollRemainder = 0.0f;
  // Câmera do viewport. Sem ela a cena continua aparecendo, mas sem grade nem
  // gizmo — que é exatamente a diferença entre um preview e um editor.
  const EditorViewport *view = nullptr;
  bool showGrid = true;
  bool showComponentVisuals=true;
  EditorEntityId cameraViewEntity=0;
  EditorEntityId cameraPreviewEntity=0;
  bool cameraPreviewReady=false,cameraPreviewFailed=false;
  u32 cameraPreviewWidth=640,cameraPreviewHeight=360;
  float cameraPreviewFrequency=15;
  bool cameraPiloting=false;
  std::string componentGroup;
  EditorNavigationMode navigation = EditorNavigationMode::Orbit;
  EditorGizmoHandle activeGizmoAxis = EditorGizmoHandle::None;
  const char *projectName = "Untitled";
  u64 impactInstance=0;
  EditorEntityId impactEntity=0;
  u32 impactPage=0;
  resources::AssetGuid impactAsset{};
  std::vector<std::pair<resources::AssetGuid,u32>> impactTrail;
  bool impactRepair=false;
  bool impactRepairScene=false;
  resources::AssetGuid impactReplacement{};
  u64 impactRepairRevision=0;
  u64 impactRepairEpoch=0;
  resources::AssetGuid impactRepairMaterial{};
  u32 impactRepairMaterialRevision=0;
  u32 colorField=0;
  EditorEntityId colorEntity=0;
  u64 colorInstance=0,colorRevision=0;
  std::string colorProperty;
  float colorHue=0,colorSaturation=0,colorValue=1;
  u32 numericField = 0;
  u64 numericInstance=0;
  std::string numericProperty;
  EditorEntityId numericEntity = kInvalidEntity;
  char numericText[48]{};
  bool numericReplace = false;
  bool numericError = false;
  bool platformTextInput = false;
  // Texto VIVO vindo do IME do sistema, e o cursor em bytes dentro dele.
  //
  // O teclado é do Android; o CAMPO é do editor. A ponte não desenha nada: ela
  // carrega o IME, entrega o texto a cada tecla e diz quanto da tela o teclado
  // ocupa. Antes disso a edição inteira acontecia num `AlertDialog` que cobria
  // a tela, e o usuário não via o que estava editando enquanto editava.
  std::string platformDraft;
  u32 platformCaret = 0;
  // Fração da altura da superfície ocupada pelo teclado do sistema, 0..1. Vem
  // como fração e não como pixels porque o Android mede em pixels físicos e
  // esta superfície é lógica; converter no meio do caminho seria mais uma
  // unidade para errar.
  float platformImeFraction = 0.0f;
  bool entityMenu = false;
  bool creationMenu=false;
  bool workspaceMenu=false;
  unsigned creationCategory=0,creationSelection=0,creationPage=0;
  u32 creationAvailable=3; // Basic object and camera; resource tools opt in on import.
  // O editor não conhece Android: ele levanta o pedido e o shell abre o seletor.
  bool modelImportRequested=false;
  bool importPanel=false,importReady=false,importAccept=false,importCancel=false,importIntoScene=false,importError=false;
  std::string importSummary,importPath;
  u32 importPage=0;
  bool codeRecoveryPending=false;
  // Última mensagem da importação, para a barra de status.
  std::string importStatus;
  // M08.2: ambiguidades da reimportação e a escolha EXPLÍCITA do usuário
  // (0 nenhuma, 1 associar pela ordem, 2 tratar como novos).
  u32 importAmbiguities=0;
  u32 importAmbiguityChoice=0;
  // R3: o importador vive em Propriedades, com dados estruturados por aba em vez
  // de um texto paginado numa janela que bloqueava o editor inteiro.
  enum class ImportTab : u8 { Summary, Structure, Textures, Profile };
  ImportTab importTab=ImportTab::Summary;
  // Uma linha da aba Estrutura. `node` é a identidade que o nó terá no mapa —
  // a MESMA que a publicação vai gravar —, e é por ela que a exclusão é pedida.
  // `excluded` já vem propagado: filho de nó excluído também não vem.
  struct ImportNodeRow { std::string name; u32 depth=0, draws=0; resources::AssetGuid node{}; bool excluded=false; };
  struct ImportTextureRow { u32 width=0,height=0,levels=0,uses=0; bool srgb=true,astc=false; u64 bytes=0; };
  std::vector<ImportNodeRow> importNodes;
  std::vector<ImportTextureRow> importTextures;
  // Tamanho do modelo preparado, em unidades da cena, já com a escala preparada.
  float importExtent[3]{};
  bool importHasExtent=false;
  // Perfil em edição (rascunho) e o perfil com que a prévia atual foi preparada.
  // Publicar exige os dois iguais: a prévia tem de ser a do perfil mostrado.
  float importScale=1,importPreparedScale=1;
  u32 importTextureDimension=2048,importPreparedTextureDimension=2048;
  // Geometria derivada do perfil (G2). O campo `Prepared` é o que a prévia na
  // tela já reflete; divergir dele é o que habilita "Preparar com este perfil".
  u8 importNormals=0,importPreparedNormals=0;
  u8 importNormalWeighting=0,importPreparedNormalWeighting=0;
  u8 importTangents=0,importPreparedTangents=0;
  bool importCameras=false,importPreparedCameras=false;
  // Nós que o autor tirou da importação. Não pede nova preparação: a saída do
  // importador é a mesma; muda o que a reconciliação instancia.
  std::vector<resources::AssetGuid> importExcludedNodes;
  // O que a publicação faz com a cena aberta. Separado do resumo porque muda a
  // cada nó marcado ou desmarcado, sem reler o arquivo.
  std::string importImpact;
  bool importReprepare=false;
  // Vínculo do objeto selecionado com a fonte, preparado pela sessão.
  struct ImportLinkView { bool linked=false,orphan=false,root=false; std::string source,node; u32 overrides=0; };
  ImportLinkView importLink;
  bool importLinkMenu=false;
  // Material por slot (Entrega 2), preparado pela sessão para o objeto
  // selecionado. `values` está no ALCANCE escolhido: a instância mostra o que
  // é desenhado neste objeto; o compartilhado mostra o recurso do projeto.
  struct MaterialSlotView {
    u32 slots=0;
    std::string name;
    bool shared=false,missing=false,overridden=false;
    float values[11]{};
    // R4: textura efetiva de cada binding no alcance em edição e de onde vem.
    std::string textureNames[4],textureOrigins[4];
    // R4: modo de alfa, corte e faces efetivos no alcance em edição.
    std::string alphaLabel,alphaOrigin,cutoffLabel,sidesLabel,sidesOrigin;
    float alphaCutoff=.5f;
    bool cutoffEditable=false;
    // R4: oclusão, canais, normal, origem do alfa e isolamento no alcance em edição.
    std::string occlusionLabel,occlusionOrigin,occlusionTextureLabel,occlusionStrengthLabel;
    std::string channelLabels[3];
    std::string normalFlipLabel,normalFlipOrigin,alphaSourceLabel,alphaSourceOrigin,isolateLabel;
  };
  MaterialSlotView materialSlotView;
  std::vector<std::string> projectMaterials;
  u32 materialSlot=0,materialPage=0;
  // R4: dado do material isolado na prévia (MaterialIsolate*); transitório.
  u8 materialIsolate=0;
  // R4: textura escolhida em Arquivos mostrada em Propriedades e gerenciador de texturas.
  bool textureInspector=false,textureManager=false,searchingTextures=false;
  u8 textureFilter=0;
  u32 textureManagerPage=0;
  std::string textureQuery,textureFolder;
  std::vector<u32> textureManagerRows;
  std::vector<std::string> textureUserLabels;
  std::vector<EditorEntityId> textureUserEntities;
  bool materialShared=false,materialPicker=false;
  // R4: seletor de textura de um binding e as texturas do projeto para ele.
  bool texturePicker=false;
  // R4: o renderer do aparelho descarta faces por material (culling dinâmico).
  bool materialCulling=true;
  u32 textureBinding=0;
  std::vector<std::string> projectTextureNames,projectTextureDetails;
  // R4: miniatura de cada textura do projeto no atlas de prévia (vazia enquanto não gerada).
  std::vector<ui::UiRect> projectTextureThumbs;
  // R4: amostragem do binding aberto no seletor, no alcance em edição.
  std::string textureUvLabel,textureWrapLabel,textureFilterLabel;
  bool textureSamplerEditable=false;
  // Deslocamento U/V, escala U/V e rotação efetivos da UV do binding.
  std::string textureUvLabels[5];
  // R4: visualizador de textura em Propriedades.
  bool textureViewer=false;
  u32 textureViewerIndex=0,textureViewerLevel=0,textureViewerLevels=0;
  u8 textureViewerChannel=0,textureViewerZoom=0,textureViewerBackground=0;
  ui::UiRect textureViewerImage{};
  std::string textureViewerTitle,textureViewerLevelLabel,textureViewerChannelLabel,textureViewerInfo;
  std::string textureViewerZoomLabel,textureViewerBackgroundLabel;
  // R4: perfil da textura (interpretação, tamanho, mips, bordas, anisotropia) e residência.
  std::string textureProfileLabels[5],textureResidencyLabel;
  // Menu de ações do objeto (⋮ do cabeçalho do inspetor) e do card Transformação.
  bool inspectorMenu=false,transformMenu=false;
  // Transformação copiada, para colar em outro objeto.
  bool hasTransformClipboard=false;
  EditorTransform transformClipboard{};
  bool editingCreationSearch=false;
  char creationSearch[kEditorNameCapacity]{};
  bool editingHierarchySearch=false;
  char hierarchySearch[kEditorNameCapacity]{};
  u32 waterTab=0,routePoint=0;
  std::vector<EditorEntityId> collapsedEntities;
  EditorEntityId renameEntity = kInvalidEntity;
  char renameText[kEditorNameCapacity]{};
  bool renameUppercase = false;
  u32 propertyPage = 0;
  EditorEntityId reparentEntity = kInvalidEntity;
  u32 assetCount = 0;
  u32 assetScroll = 0;
  bool draggingAsset = false;
  EditorEntityId draggingEntity = kInvalidEntity;
  ui::UiPoint assetDragPosition{};
  bool saveRequested = false;
  std::string status;
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
  ui::UiRect filesPanel{};
  ui::UiRect inspectorPanel{};
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
