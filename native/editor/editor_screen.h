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

#include "renderer/rendering_policy.h"
#include "renderer/texture_streaming.h"
#include "renderer/scene_statistics.h"
#include "resources/environment_map_asset.h"
#include "resources/texture_profile.h"
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
#include "ui/ui_icon_id.h"
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
  TabProject,

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
  SceneLightingToggle,
  SceneEffectsToggle,
  SceneEffectsMenu,
  SceneSkyToggle,
  SceneFogToggle,
  ScenePostToggle,

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
  // S0: pasta de modelo (glTF com .bin e texturas ao lado), copiada para o projeto.
  ImportFolder,
  ImportEnvironment,
  ImportTexture,
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
  CreationAtRoot,
  CreationAsChild,
  CreationSearch,
  CreationClearSearch,
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
  PlayInspect,
  NumericExpressionToggle,
  ToggleCharacter,
  JumpCharacter,
  PlaySecondaryAction,
  ToggleCameraLook,
  AddComponentMenu,
  ComponentPreviewBack,
  ComponentPreviewConfirm,
  CodeOpen, CodeScene, CodeNew, CodeEdit, CodeSave, CodeUndo, CodeRedo, CodeSearch, CodeClose, CodeApply,
  CodeMenu, CodeSaveAll, CodeFiles, CodeTabsPrevious, CodeTabsNext, CodeFindPrevious, CodeFindNext,
  CodeGoLine, CodeNewFolder, CodeNewHelper, CodeTemplates,
  ColliderFit, LodGroupFit, LodGroupStatus, SkinnedMeshStatus, AnimationStatus, ComponentClipAdd, ComponentPrevious,
  ViewsOpen, ViewsClose, ViewSave, ViewUpdate, ViewRename, ViewDelete, ComponentNext, ScriptFieldsPrevious, ScriptFieldsNext,
  TransformFold, ComponentSearch, ComponentSearchClear,
  MeshGeometryTab, MeshMaterialTab, MeshChoose, MeshPickerClose, MeshClear, MeshSearch, MeshPrevious, MeshNext,
  MeshGenerateCollision, MeshCollisionQualityDown, MeshCollisionQualityUp,
  MeshCollisionErrorDown, MeshCollisionErrorUp, MaterialRestore,
  EnvironmentProfileCreate, EnvironmentProfileUpdate,
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
  CreateSceneTemplate, SceneTemplateClose,
  QualityOpen, QualityClose, QualityLevel, QualityScaleDown, QualityScaleUp, QualityDynamic,
  QualityAntiAliasing, QualitySharpenDown, QualitySharpenUp, QualityRate, QualityApply,
  QualityTabGeneral, QualityTabShadows, QualityTabLighting, QualityTabPerformance,
  QualityPagePrevious, QualityPageNext,
  QualityUpscaling, QualityTextures,
  QualityShadows, QualityShadowCascades, QualityShadowResolution,
  QualityShadowDistanceDown, QualityShadowDistanceUp,
  QualityShadowBiasDown, QualityShadowBiasUp, QualityShadowSlopeDown, QualityShadowSlopeUp,
  QualityShadowNormalDown, QualityShadowNormalUp, QualityShadowCache,
  QualityAmbient, QualityEnvironmentBrdf, QualityPost, QualityBloomThresholdDown,
  QualityBloomThresholdUp, QualityBloomIntensityDown, QualityBloomIntensityUp,
  QualityTemporalWeightDown, QualityTemporalWeightUp, QualityVignette,
  QualityTemporalDebug,
  QualityTemporalDebugQuick,
  QualityDynamicMinimumDown, QualityDynamicMinimumUp, QualityLodSelection,
  QualityLodErrorDown, QualityLodErrorUp, QualityLodHysteresisDown,
  QualityLodHysteresisUp, QualityMaterialVariants,
  QualityTemporalMode, QualityTemporalQuality, QualityTemporalModeQuick,
  // S2: aba Texturas do painel Qualidade (Mipmap Streaming da Unity).
  QualityTabTextures, QualityTextureStreaming, QualityStreamingBudgetDown, QualityStreamingBudgetUp,
  QualityStreamingReductionDown, QualityStreamingReductionUp, QualityStreamingUploadDown, QualityStreamingUploadUp,
  QualityStreamingDebugView,
  // S5: estatísticas do quadro no viewport (Statistics do Game View).
  QualitySceneStatistics,
  // S4: Explorador de luzes (Light Explorer da Unity). Oito linhas por página,
  // cada uma com o nome (seleciona) e o interruptor Acesa/Apagada.
  LightExplorerOpen, LightExplorerClose, LightExplorerFilter, LightExplorerPrevious, LightExplorerNext,
  LightExplorerIntensityDown, LightExplorerIntensityUp, LightExplorerApply,
  LightExplorerRow0, LightExplorerRowLast = LightExplorerRow0 + 7,
  LightExplorerToggle0, LightExplorerToggleLast = LightExplorerToggle0 + 7,
  // Bloco F: gerenciador alterna entre texturas do projeto e das fontes; a
  // textura de uma fonte leva à própria fonte em Arquivos (perfil, Reimportar).
  TextureManagerShowProject, TextureManagerShowSources, TextureSourceShowOrigin, TextureSourceShowAll,
  ImportTabSummary, ImportTabStructure, ImportTabMeshes, ImportTabTextures, ImportTabProfile,
  EnvironmentPanoramaDown, EnvironmentPanoramaUp,
  EnvironmentSpecularDown, EnvironmentSpecularUp,
  EnvironmentBrdfDown, EnvironmentBrdfUp,
  EnvironmentSpecularSamplesDown, EnvironmentSpecularSamplesUp,
  EnvironmentBrdfSamplesDown, EnvironmentBrdfSamplesUp,
  ImportMeshClose,
  ImportScaleDown, ImportScaleUp,
  ImportTextureDimension256, ImportTextureDimension512, ImportTextureDimension1024, ImportTextureDimension2048,
  ImportApplyProfile, ImportSaveDefaultProfile,
  // G2: geometria derivada no perfil — normais, ponderação e tangentes.
  ImportNormalsCycle, ImportNormalWeightingCycle, ImportTangentsCycle, ImportCamerasToggle, ImportLightsToggle,
  ImportTextureCompressionCycle,
  // S2: Stream Mipmap Levels e Priority das texturas da fonte.
  ImportTextureStreamingToggle, ImportTextureStreamingPriorityCycle,
  // S3: Generate LODs, Maximum Levels e Optimize Mesh > Polygon Order.
  ImportGenerateLodsToggle, ImportLodLevelsCycle, ImportOptimizeOrderToggle,
  ImportSmoothingDown, ImportSmoothingUp,
  // R4: textura por binding de material e extração das imagens de um GLB.
  TexturePickerClose, TextureUseInherited, TextureUseNone, AssetExtractTextures,
  MaterialAlphaCycle, MaterialCutoffDown, MaterialCutoffUp, MaterialSidesCycle,
  MaterialOcclusionSourceCycle, MaterialOcclusionTexture, MaterialOcclusionStrengthDown, MaterialOcclusionStrengthUp,
  MaterialChannelRoughness, MaterialChannelMetallic, MaterialChannelOcclusion,
  MaterialNormalFlipCycle, MaterialAlphaSourceCycle, MaterialIsolateCycle,
  TextureViewerClose, TextureViewerChannel, TextureViewerMipDown, TextureViewerMipUp,
  TextureViewerZoom, TextureViewerBackground,
  TextureProfileInterpretation, TextureProfileDimension, TextureProfileMipmaps, TextureProfileEdges, TextureProfileAnisotropy,
  TextureProfileNormalGreen, TextureProfileCoverage, TextureProfileCoverageCutoff,
  // S2: Stream Mipmap Levels e Priority, na mesma sequência (índice + página).
  TextureProfileStreaming, TextureProfileStreamingPriority, TextureProfileApply, TextureProfileRevert,
  TextureProfilePrevious, TextureProfileNext,
  TextureManagerClose, TextureSearch, TextureManagerPrevious, TextureManagerNext,
  TextureSamplingUv, TextureSamplingWrap, TextureSamplingFilter, TextureUvReset,
  DiagnosticDockToggle=0x09000000u, DiagnosticOlder, DiagnosticNewer,
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
  // Inspector de textura em cartões: sub-ids em `detail::TextureInspector*`.
  TextureInspectorBase=0xB4000000u,
  ComponentGroupBase=0x5C000000u, ComponentVisualBase=0x5D000000u,
  ComponentVisualsToggle=0x5E000000u, CameraView, CameraViewClose, CameraAlignView, CameraPilot,
  ComponentReferenceBase=0x7a000000u, ReferenceChoiceBase=0x7b000000u,
  // + componente + binding<<8 + slot<<16. Abre o seletor do tipo do binding.
  ComponentResourceBase=0x7f000000u,
  CameraLens=0x5E000010u, CameraNear, CameraFar,
  CameraHandleBase=0x5E000020u,
  ComponentHandleBase=0x5E000060u,
  CameraPreviewPin=0x5E000030u, CameraPreviewClose, CameraPreviewResolution, CameraPreviewFrequency, CameraPreviewRetry,
  PresetOpen=0x5E000040u, PresetClose, PresetSave, PresetApply, PresetAdd, PresetRename, PresetDelete, PresetPrevious, PresetNext,
  // Escolha por campo do preset: marcar tudo, desmarcar tudo e paginar o diff.
  PresetSelectAll, PresetSelectNone, PresetFieldsPrevious, PresetFieldsNext, PresetSaveRecipe,
  PresetChoiceBase=0x5E010000u,
  // + índice da linha do diff: alterna levar ou não aquele campo.
  PresetFieldBase=0x5E020000u,
  // + linha da aba Estrutura da importação: inclui ou exclui aquele nó.
  ImportNodeToggleBase=0x5E030000u,
  // Faixas próprias: o painel de impacto já dividiu números com
  // AssetRowBase (0x60) e com CreationCategoryBase/CreationRowBase (0x63/0x64),
  // e engolia o toque na categoria do menu Adicionar objeto.
  // + índice da vista salva: aplica aquele enquadramento.
  SceneViewRowBase=0x93000000u,
  // + índice da malha na aba Malhas: abre o cartão de detalhe daquela malha.
  ImportMeshRowBase=0x94000000u,
  // + índice do modelo de cena: monta aquele cenário.
  SceneTemplateRowBase=0x95000000u,
  // + componente + propriedade<<8 + endereço<<16: valor por endereço (ComponentSlotNumber).
  ComponentSlotNumberBase=0x96000000u,
  ComponentClipMoveUpBase=0x97000000u,
  ComponentClipMoveDownBase=0x98000000u,
  ComponentClipRemoveBase=0x99000000u,
  ImpactOpenBase=0x90000000u, ImpactRowBase=0x91000000u, ImpactClose=0x92000000u, ImpactPrevious, ImpactNext, ImpactRepair, ImpactRepairApply, ImpactRepairShared, ImpactRepairScope, ImpactRemoveConfirm,
  ComponentColorBase=0x7e000000u,
  ColorHueBase=0x5f000000u, ColorSvBase=0x5f000100u, ColorApply=0x5f000200u, ColorCancel,
  // Janela de cor (Unity Manual/InspectorColorPicker): quadrado SV e matiz
  // contínuos, abas de modo, barras de canal, hexadecimal, cor original e
  // bibliotecas de amostras.
  ColorSquare=0x5f000300u, ColorHueStrip, ColorHex, ColorOriginal, ColorSwatchAdd, ColorLibraryToggle, ColorLibraryNew,
  ColorModeBase=0x5f000310u, ColorSliderBase=0x5f000320u, ColorSwatchActionBase=0x5f000340u,
  ColorLibraryBase=0x5f000400u, ColorSwatchBase=0x5f000500u,
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
  // + 0 = todas as famílias, + 1 + família = aquela família (scene::ComponentFamily).
  ComponentFamilyBase=0xB5000000u,
  // + posição do componente no objeto. Menu comum a nativos e comportamentos
  // (Unity 6000.0, Manual/UsingComponents: Move Up/Down, Paste Component As
  // New, ícone de ajuda) e interruptor de ativo no cabeçalho.
  ComponentMoveUpBase=0xB6000000u,
  ComponentMoveDownBase=0xB7000000u,
  ComponentPasteNewBase=0xB8000000u,
  ComponentHelpBase=0xB9000000u,
  ComponentEnableBase=0xBA000000u,
  // + índice da opção na lista aberta pelo campo de enumeração.
  ComponentEnumOptionBase=0xBB000000u,
  ComponentEnumPickerClose=0xBC000000u,
  // Lista em campo de script (Unity Manual/InspectorArray): + índice do
  // componente; elemento e alça levam também o elemento << 8.
  ScriptArraySizeBase=0xBD000000u,
  ScriptArrayElementBase=0xBE000000u,
  ScriptArrayHandleBase=0xBF000000u,
  ScriptArrayAddBase=0xC0000000u,
  ScriptArrayRemoveBase=0xC1000000u,
  // Editor de gradiente (Unity Gradient Editor): faixas de paradas, painel da
  // parada escolhida, modos e presets.
  GradientBase=0xC2000000u, GradientAlphaLane, GradientColorLane, GradientApply, GradientCancel, GradientLocation,
  GradientAlphaValue, GradientColorSwatch, GradientDeleteStop, GradientPresetAdd, GradientLibraryToggle, GradientLibraryNew,
  GradientModeBase=0xC2000010u, GradientPresetActionBase=0xC2000030u, GradientAlphaStopBase=0xC2000100u,
  GradientColorStopBase=0xC2000200u, GradientLibraryBase=0xC2000400u, GradientPresetBase=0xC2000500u,
  // Editor de curvas (Unity Curve Editor): gráfico, enquadrar/zoom, chave
  // escolhida (tempo, valor, tangentes), repetição e presets.
  CurveBase=0xC3000000u, CurveGraph, CurveApply, CurveCancel, CurveFrame, CurveZoomIn, CurveZoomOut, CurveKeyTime,
  CurveKeyValue, CurveKeyDelete, CurvePresetAdd, CurveLibraryToggle, CurveLibraryNew, CurveLibraryFactory,
  CurvePreWrapBase=0xC3000010u, CurvePostWrapBase=0xC3000014u, CurveTangentBase=0xC3000020u,
  CurveLeftModeBase=0xC3000028u, CurveRightModeBase=0xC300002Cu, CurvePresetActionBase=0xC3000030u,
  CurveLibraryBase=0xC3000400u, CurvePresetBase=0xC3000500u,
  // Barra dividida do LOD Group (Unity Manual/InspectorBarSliders): a barra,
  // divisores entre níveis, segmentos e o menu do segmento.
  LodBar=0xC4000000u, LodBarInsert, LodBarDelete, LodBarDividerBase=0xC4000010u, LodBarSegmentBase=0xC4000020u,
  // Seletor de objeto avançado: alternar Clássico/Avançado, visões, filtro de
  // tipo, escolher o destacado e os resultados.
  ReferenceModeToggle=0xC5000000u, ReferenceTypeFilter, ReferenceAssign, ReferenceViewBase=0xC5000010u,
  ReferenceResultBase=0xC5010000u,
  // Gestão do Inspector (Unity Manual/InspectorOptions e InspectorFocused):
  // cadeado, modo Debug e Ping.
  InspectorLock=0xC6000000u, InspectorDebugToggle, InspectorPing,
  // Inspectors focados (Unity: Properties): abrir, abas, fechar, ⋮, minimizar.
  InspectorOpenFocused, FocusedMenu, FocusedPing, FocusedCloseAll, FocusedCollapse, FocusedChip, HierarchyProperties,
  FocusedTabBase=0xC6000100u, FocusedCloseBase=0xC6000200u, ComponentPropertiesBase=0xC6010000u,
  // Histórico de Desfazer (Unity 6000.0 Manual/UndoWindow): fechar, ordem,
  // páginas e uma linha por ponto do histórico (0 = cena como abriu).
  UndoHistoryClose=0xC7000000u, UndoHistoryOrder, UndoHistoryPrevious, UndoHistoryNext,
  UndoHistoryRowBase=0xC7000100u,
  // Busca global (Unity 6000.0 Search): abrir na barra, campo, chips de
  // provedor, páginas e um resultado por linha.
  GlobalSearchOpen=0xC9000000u, GlobalSearchClose, GlobalSearchField, GlobalSearchPrevious, GlobalSearchNext,
  GlobalSearchProviderBase=0xC9000010u, GlobalSearchResultBase=0xC9000100u,
  // Camadas na vista da cena (Unity 6000.0 View Options › Layers, Scene
  // visibility e Scene picking): abrir, Tudo/Nada, páginas, olho e seleção.
  SceneLayersOpen=0xC8000000u, SceneLayersClose, SceneLayersShowAll, SceneLayersHideAll, SceneLayersPickAll,
  SceneLayersPrevious, SceneLayersNext, SceneLayerVisibleBase=0xC8000100u, SceneLayerPickBase=0xC8000200u,
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
  ComponentPreviewComposition=0x9a00'0000u,
  ComponentPreviewValues,
  ComponentPreviewPrevious,
  ComponentPreviewNext,
  ComponentPropertySearch=0x9b00'0000u,
  ComponentPropertySearchClear,
  ComponentFieldResetBase=0xa000'0000u,
  // + índice da receita em editorCreationCatalog: cria aquela composição.
  // Faixa fixa para que acrescentar receitas não renumere controles existentes.
  CreationRecipeBase=0x0800'0000u,
  // + EditorProjectSection: seção da workspace Projeto.
  ProjectSectionBase=0x0810'0000u,
  PhysicsLayerPrevious=0x0820'0000u,
  PhysicsLayerNext,
  PhysicsLayerAdd,
  PhysicsLayerRename,
  PhysicsMatrixPrevious,
  PhysicsMatrixNext,
  PhysicsInteractionBase=0x0821'0000u,
  InputActionPrevious=0x0830'0000u,InputActionNext,InputActionAdd,InputActionRename,InputActionRemove,
  InputTabAction,InputTabBinding,InputActionKind,InputActionContext,
  InputRoleMove,InputRoleLook,InputRoleJump,InputDeadzone,InputSensitivity,
  InputBindingPrevious,InputBindingNext,InputBindingAdd,InputBindingRemove,
  InputBindingSource,InputBindingAxis,InputBindingInvert,InputBindingCode,
  InputBindingNegativeCode,InputBindingScale,
  InputActionPagePrevious,InputActionPageNext,
  InputBindingPagePrevious,InputBindingPageNext,
  InputDetailsToggle,
  InputActionRowBase=0x0831'0000u,
  InputBindingRowBase=0x0832'0000u,
};

inline constexpr u32 widgetId(EditorWidget widget) noexcept { return static_cast<u32>(widget); }
// Cada faixa-base (id + índice) reserva um intervalo inteiro de ids. Duas
// faixas que se cruzam fazem um painel receber o toque do outro sem erro
// nenhum — foi assim que o impacto engoliu o menu Adicionar objeto. Por isso a
// lista é conferida na compilação. Faixa nova entra aqui, com o seu tamanho.
namespace detail {
struct WidgetRange {EditorWidget base;u32 span;};
inline constexpr u32 kWideRange=0x1000'0000u,kRange=0x0100'0000u;
// Bloco F: linhas de textura das fontes na mesma faixa do gerenciador.
inline constexpr u32 TextureManagerSourceRowOffset=0x0080'0000u;
// Inspector de textura em cartões, somados a TextureInspectorBase: cartão
// recolhível (+seção), copiar origem (+linha), pasta da trilha (+nível) e ações.
inline constexpr u32 TextureInspectorSection=0x00,TextureInspectorCopy=0x10,TextureInspectorCrumb=0x20,
                     TextureInspectorScroll=0x40,TextureInspectorExpand=0x41,TextureInspectorExpandClose=0x42,
                     TextureInspectorLocate=0x43,TextureInspectorReimport=0x44,TextureInspectorGpuInfo=0x45,
                     TextureInspectorUsersMore=0x46;
enum TextureInspectorCard : u32 {TextureCardPreview,TextureCardProperties,TextureCardImport,TextureCardOrigin,TextureCardUsers};
inline constexpr WidgetRange widgetRanges[]{
  {EditorWidget::HierarchyRowBase,kWideRange},{EditorWidget::HierarchyEyeBase,kWideRange},
  {EditorWidget::TransformFieldBase,kWideRange},{EditorWidget::GizmoAxisBase,kWideRange},
  {EditorWidget::NumericKeyBase,kRange},{EditorWidget::ConsoleRowBase,kRange},{EditorWidget::ImportLinkRevertBase,kRange},
  {EditorWidget::MaterialChoiceBase,kRange},{EditorWidget::MaterialNumberBase,kRange},{EditorWidget::MaterialTextureBase,kRange},
  {EditorWidget::TextureChoiceBase,kRange},{EditorWidget::TextureViewBase,kRange},{EditorWidget::TextureUvStepBase,kRange},
  {EditorWidget::TextureFilterBase,kRange},{EditorWidget::TextureManagerRowBase,kRange},{EditorWidget::TextureUserBase,kRange},
  {EditorWidget::TextureInspectorBase,kRange},
  {EditorWidget::ComponentGroupBase,kRange},{EditorWidget::ComponentVisualBase,kRange},{EditorWidget::ComponentVisualsToggle,kRange},
  {EditorWidget::ColorHueBase,kRange},{EditorWidget::AssetRowBase,kRange},{EditorWidget::CodeBody,kRange},
  {EditorWidget::RoutePointBase,kRange},{EditorWidget::CreationCategoryBase,kRange},{EditorWidget::CreationRowBase,kRange},
  {EditorWidget::FilesUp,kRange},{EditorWidget::FileRowBase,kRange},{EditorWidget::ComponentAddBase,kRange},
  {EditorWidget::ComponentFoldBase,kRange},{EditorWidget::ComponentRemoveBase,kRange},{EditorWidget::ComponentBooleanBase,kRange},
  {EditorWidget::ComponentCopyBase,kRange},{EditorWidget::ComponentPasteBase,kRange},{EditorWidget::ComponentResetBase,kRange},
  {EditorWidget::ComponentMenuBase,kRange},{EditorWidget::ComponentEnumBase,kRange},{EditorWidget::NameKeyBase,kRange},
  {EditorWidget::ScriptAddBase,kRange},{EditorWidget::ScriptFoldBase,kRange},{EditorWidget::ScriptMenuBase,kRange},
  {EditorWidget::ScriptRemoveBase,kRange},{EditorWidget::ScriptEnabledBase,kRange},{EditorWidget::ScriptSourceBase,kRange},
  {EditorWidget::ScriptFieldBase,kRange},{EditorWidget::ComponentNumberBase,kRange},{EditorWidget::MeshChoiceBase,kRange},
  {EditorWidget::ComponentReferenceBase,kRange},{EditorWidget::ReferenceChoiceBase,kRange},{EditorWidget::CodeTemplateBase,kRange},
  {EditorWidget::ComponentTripleBase,kRange},{EditorWidget::ComponentColorBase,kRange},{EditorWidget::ComponentResourceBase,kRange},
  {EditorWidget::HierarchyCollapseBase,kWideRange},{EditorWidget::ComponentPreviewComposition,kRange},
  {EditorWidget::ComponentPropertySearch,kRange},
  {EditorWidget::ComponentFieldResetBase,kWideRange},
  {EditorWidget::ImpactOpenBase,kRange},{EditorWidget::ImpactRowBase,kRange},{EditorWidget::ImpactClose,kRange},
  {EditorWidget::SceneViewRowBase,kRange},{EditorWidget::ImportMeshRowBase,kRange},
  {EditorWidget::SceneTemplateRowBase,kRange},{EditorWidget::ComponentSlotNumberBase,kRange},
  {EditorWidget::ComponentClipMoveUpBase,kRange},{EditorWidget::ComponentClipMoveDownBase,kRange},
  {EditorWidget::ComponentClipRemoveBase,kRange},
  {EditorWidget::InputActionRowBase,64},{EditorWidget::InputBindingRowBase,16},
  {EditorWidget::CreationRecipeBase,0x0010'0000u},{EditorWidget::ComponentFamilyBase,kRange},
  {EditorWidget::ComponentMoveUpBase,kRange},{EditorWidget::ComponentMoveDownBase,kRange},
  {EditorWidget::ComponentPasteNewBase,kRange},{EditorWidget::ComponentHelpBase,kRange},
  {EditorWidget::ComponentEnableBase,kRange},{EditorWidget::ComponentEnumOptionBase,kRange},
  {EditorWidget::ComponentEnumPickerClose,kRange},{EditorWidget::ScriptArraySizeBase,kRange},
  {EditorWidget::ScriptArrayElementBase,kRange},{EditorWidget::ScriptArrayHandleBase,kRange},
  {EditorWidget::ScriptArrayAddBase,kRange},{EditorWidget::ScriptArrayRemoveBase,kRange},{EditorWidget::GradientBase,kRange},
  {EditorWidget::CurveBase,kRange},{EditorWidget::LodBar,kRange},{EditorWidget::ReferenceModeToggle,kRange},
  {EditorWidget::InspectorLock,kRange},{EditorWidget::UndoHistoryClose,kRange},{EditorWidget::SceneLayersOpen,kRange},{EditorWidget::GlobalSearchOpen,kRange}};
inline constexpr bool widgetRangesDisjoint() {
  for(const auto &a:widgetRanges) for(const auto &b:widgetRanges) {
    if(&a==&b) continue;
    const u64 a0=widgetId(a.base),b0=widgetId(b.base);
    if(a0<b0+b.span && b0<a0+a.span) return false;
  }
  return true;
}
}
static_assert(detail::widgetRangesDisjoint(),"duas faixas de widget se cruzam");
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
// Seis contextos fixos. Configuração de projeto (camadas, entrada, água) é UMA
// workspace com seções; um tipo de componente ou uma lista de ajustes nunca
// vira workspace própria — ver docs/planos/EXPANSAO-OBJETOS-COMPONENTES-API.
enum class EditorWorkspace : u8 { Scene, Assets, Lighting, Play, Project, Code };
enum class EditorProjectSection : u8 { Layers, Input, Water };
enum class EditorNavigationMode : u8 { Orbit, Pan, Zoom };
enum class EditorInspectorTab : u8 { Transform, Material, Properties };

enum class EditorSearchProvider : u8 { All = 0, Scene, Project, Create };

struct EditorSearchResult {
  EditorSearchProvider provider = EditorSearchProvider::Scene;
  // Objeto: id da entidade; receita: índice no catálogo; arquivo: índice no
  // índice de arquivos passado à busca.
  u64 key = 0;
  std::string title, detail;
  ui::UiIcon icon = ui::UiIcon::None;
  u8 rank = 0;  // 0 começa com a palavra, 1 contém numa fronteira, 2 contém
};

struct EditorSearchCounts { u32 scene = 0, project = 0, create = 0; };

struct EditorScreenState final {
  // Superfície inteira em pixels lógicos (dp), incluindo o que fica sob o
  // recorte da câmera. As áreas seguras entram por `safeArea`.
  ui::UiRect surface{};
  ui::UiInsets safeArea{};
  // O que a Hierarquia e o Inspector mostram: o documento autoral, ou o grafo
  // do mundo de execução quando o Play é inspecionado. Os dois são
  // runtime::SceneGraph; a escrita autoral continua indo ao EditorDocument.
  const runtime::SceneGraph *document = nullptr;
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
  EditorProjectSection projectSection=EditorProjectSection::Layers;
  u32 physicsLayer=0,physicsMatrixPage=0;
  bool editingPhysicsLayerName=false;
  u32 inputActionIndex=0,inputBindingIndex=0,inputTab=0,inputEditField=0;
  u32 inputActionPage=0,inputBindingPage=0;
  bool inputDetails=false;
  bool editingInputActionName=false,editingInputContext=false;
  bool playHasScripts=false;
  bool playFirstPerson=false,playHasCharacter=false;
  std::string playSecondaryActionLabel,playHudMessage;
  bool playPaused=false;
  bool playStepRequested=false;
  // Hierarquia e Inspector abertos com o Play rodando (Unity: o Inspector
  // continua editável em Play e tudo volta ao sair). Mostram o mundo de
  // execução; a escrita vai ao mundo, nunca ao documento autoral.
  bool playInspect=false;
  // Resultado da última edição feita em Play, na faixa do Inspector. Recusada
  // pelo mundo, a faixa passa à cor de aviso com o motivo.
  std::string playEditNote;
  bool playEditRefused=false;
  // Arraste do cabeçalho para reordenar (Unity: arrastar o componente no
  // Inspector). Índice+1 do cartão levantado e do cabeçalho sob o dedo.
  u32 componentReorder=0,componentReorderTarget=0;
  ui::UiPoint componentReorderPoint{};
  EditorInspectorTab tab = EditorInspectorTab::Transform;
  // Diagnósticos da sessão são ferramentas editoriais, fora da cena e do Undo.
  bool diagnosticDockOpen=false;
  // Folding is editor-only, keyed by object and stable component instance identity.
  EditorEntityId componentSelection=0;
  std::string expandedComponent;
  u64 expandedNative=0,nativeMenu=0;
  u64 referenceInstance=0;
  std::string referenceProperty,referenceQuery;
  bool referenceScript=false,editingReferenceSearch=false;
  // Seletor avançado (preferência do projeto), visão 0 lista/1 grade/2 tabela,
  // filtro de tipo ligado e o resultado destacado (painel de inspeção).
  bool pickerAdvanced=false,referenceTypeFilter=true;
  // Inspector travado num objeto (Unity: cadeado); 0 segue a seleção. O alvo
  // de quem abriu o modal aberto (seletor, lista de enumeração, Add) — o
  // Inspector travado, um focado ou a seleção.
  EditorEntityId inspectorLocked=0,inspectorTarget=0;
  // Modo Debug do Inspector (Unity ⋮ > Debug): valores crus, só leitura.
  bool inspectorDebug=false;
  // Inspectors focados (Unity 6000.0 Manual/InspectorFocused): cada um presa
  // a um objeto, ou a um componente dele (instância), sem seguir a seleção.
  struct FocusedInspector { EditorEntityId entity=0; u64 component=0; };
  std::vector<FocusedInspector> focusedInspectors;
  u32 focusedActive=0;       // aba visível (índice+1)
  bool focusedCollapsed=false,focusedMenu=false;
  // Histórico de Desfazer: aberto pelo toque longo em Desfazer/Refazer. As
  // entradas vêm do EditorHistory (mais antiga primeiro); `undoApplied` é
  // quantas estão aplicadas, isto é, o ponto atual.
  bool undoHistory=false,undoNewestFirst=true;
  u32 undoHistoryPage=0,undoApplied=0;
  std::vector<std::string> undoEntries;
  // Camadas escondidas e não selecionáveis na vista da cena: estado do editor
  // (preferência do projeto), nunca da cena — o jogo e o Play não mudam.
  u32 hiddenLayers=0,unpickableLayers=0,sceneLayersPage=0;
  // Busca global: consulta, provedor escolhido (chip), página e resultados que
  // a sessão recalcula enquanto a janela está aberta.
  bool globalSearch=false,editingGlobalSearch=false;
  std::string globalQuery;
  EditorSearchProvider globalProvider=EditorSearchProvider::All;
  u32 globalPage=0;
  std::vector<EditorSearchResult> globalResults;
  EditorSearchCounts globalCounts;
  bool sceneLayersPanel=false;
  // Ping: objeto destacado na Hierarquia até `pingUntil` (relógio `uiTime`).
  EditorEntityId pingEntity=0;
  double pingUntil=0,uiTime=0;
  u8 pickerView=0;
  u64 referenceHighlight=0;
  // Tipo declarado do campo de script aberto no seletor: "object" ou
  // "component:<id>" (referência a componente).
  std::string referenceScriptType;
  u32 referencePage=0;
  // Destino do seletor de recurso. Instância zero mantém o seletor de malha
  // visual existente; valor não zero endereça um binding refletido.
  u64 resourceInstance=0;
  std::string resourceProperty;
  u32 resourceSlot=0;
  // Receita em edição no seletor da malha física. Passos finitos deixam o
  // controle utilizável por toque e mantêm a mesma validação da persistência.
  u8 collisionTrianglePercent=25;
  float collisionMaximumError=.02f;
  bool addingComponent=false;
  u32 componentPreview=0; // índice do catálogo + 1; zero mantém a lista
  bool componentPreviewValues=false;
  u32 componentPreviewPage=0;
  std::string scriptPreviewType;
  u64 scriptPreviewGeneration=0;
  bool editingComponentSearch=false,editingMeshSearch=false,meshPicker=false;
  std::string componentQuery,meshQuery;
  bool editingPropertySearch=false;
  std::string propertyQuery;
  u32 componentCategory=0,meshPage=0,meshTab=0;
  // Catálogo do Add: linha do topo da lista rolável e os tipos adicionados por
  // último nesta sessão (ids de schema), mais recente primeiro.
  u32 addScroll=0,addRailScroll=0;
  float addScrollRemainder=0;
  std::vector<std::string> recentComponents;
  u32 componentPage=0,scriptPropertyPage=0;
  u64 expandedScript=0,scriptMenu=0,editingScriptInstance=0;
  // Lista de script aberta (id do campo) e o elemento escolhido (índice+1):
  // "−" remove o escolhido, ou o último quando nenhum está.
  std::string expandedScriptArray;
  u32 scriptArraySelected=0;
  // Texto em edição: elemento (índice+1) da lista aberta, ou o Tamanho.
  u32 editingScriptElement=0;
  bool editingScriptArraySize=false;
  // Seletor de referência aberto para um elemento (índice+1).
  u32 referenceScriptElement=0;
  // Arraste pela alça para reordenar elementos (índice+1 do levantado e do alvo).
  u32 scriptArrayDrag=0,scriptArrayDragTarget=0;
  ui::UiPoint scriptArrayDragPoint{};
  EditorEntityId editingScriptEntity=0;
  std::string editingScriptProperty,editingScriptType;
  std::shared_ptr<const EditorComponentValue> componentClipboard;
  // Link externo pedido pela interface (ajuda do componente); o shell do
  // Android abre no navegador e limpa. Só https.
  std::string externalLink;
  // Lista de opções aberta por um campo de enumeração: o id do campo
  // (ComponentEnumBase + componente + campo<<8), ou zero fechada.
  u32 enumPicker=0;
  bool presetPanel=false,presetNaming=false,presetRenaming=false,presetDeleteConfirm=false;
  // Vistas salvas da cena: painel aberto, linha escolhida e o fluxo de nome
  // (salvar uma nova ou renomear a escolhida).
  bool viewsPanel=false,viewNaming=false,viewRenaming=false;
  // Escolha do modelo de cena a montar.
  bool templatePanel=false;
  // Painel Qualidade: o rascunho que o autor edita, se difere do aplicado, e o
  // que o renderer está fazendo de fato agora (resolução real e custo de GPU).
  bool qualityPanel=false,qualityDirty=false;
  u32 qualityTab=0;
  u32 qualityPage=0;
  renderer::ProjectRenderingSettings qualityDraft{};
  std::string qualityStats,qualityDetected;
  u32 qualityTemporalDebug=0;
  bool qualityTemporalAvailable=false;
  bool qualityMotionAvailable=false;
  // Diagnósticos que dependem do histórico do TAA nativo (histórico usado e
  // rejeição). Com Arm ASR/FSR 2 o histórico é da biblioteca e não é exibido.
  bool qualityHistoryDiagnostics=false;
  // Ampliação temporal: o que o aparelho suporta, o que está salvo no projeto
  // e o que o renderer realmente executou no último quadro.
  renderer::TemporalUpscalerAvailability qualityArmAsr=renderer::TemporalUpscalerAvailability::NotProbed;
  renderer::TemporalUpscalerAvailability qualityFsr2=renderer::TemporalUpscalerAvailability::NotProbed;
  renderer::ProjectRenderingSettings qualityApplied{};
  renderer::UpscalingFilter qualityExecutedUpscaler=renderer::UpscalingFilter::Bilinear;
  renderer::TemporalUpscalerAvailability qualityExecutedStatus=renderer::TemporalUpscalerAvailability::Available;
  bool qualityTemporalAaExecuted=false;
  // S2: o que o streaming de mipmaps fez no último quadro do aparelho.
  renderer::TextureStreamingStats qualityTextureStreaming{};
  // Vista de depuração do streaming na Scene View; ferramenta, fora do Undo.
  bool qualityTextureStreamingDebug=false;
  // S4: Explorador de luzes — linhas da página atual, contagens da cena e a
  // intensidade que "Aplicar" escreve nas luzes do filtro (unidade de cada luz).
  struct LightExplorerRow {
    EditorEntityId entity=kInvalidEntity;
    u64 instance=0;
    std::string name,detail;
    bool enabled=true,dark=false;
  };
  bool lightExplorer=false,lightExplorerDarkOnly=false;
  u32 lightExplorerPage=0,lightExplorerTotal=0,lightExplorerDark=0,lightExplorerFiltered=0;
  float lightExplorerIntensity=1000;
  std::vector<LightExplorerRow> lightExplorerRows;
  // Bloco F: texturas das fontes importadas (as imagens de um modelo) no mesmo
  // gerenciador e Inspector das texturas do projeto. A linha i da lista usa a
  // célula i % 50 do atlas de miniaturas; o Inspector troca o perfil (que é da
  // fonte) pela origem e pela fonte dona.
  bool textureManagerSources=false,textureViewerSource=false;
  u32 textureViewerSourceIndex=0; // posição na lista de texturas das fontes
  std::vector<std::string> sourceTextureNames;
  std::vector<ui::UiRect> sourceTextureThumbs;
  // Inspector de textura em cartões (projeto e fontes). A sessão preenche os
  // dados; a tela decide só a forma (grade, chips, trilha).
  struct TextureField {std::string label,value;};
  struct TextureOrigin {std::string label,value,copy;};
  std::vector<TextureField> textureProperties;  // além de Canal e Mip, que são controles
  std::vector<TextureOrigin> textureOrigin;
  std::vector<std::string> textureBreadcrumb;   // pastas do projeto até o arquivo
  std::string textureDimensions,textureGpuNote,textureMipValue;
  u32 textureCardsClosed=0;                     // bit por TextureInspectorCard recolhido
  float textureInspectorScroll=0;
  bool textureViewerExpanded=false,textureUsersExpanded=false;
  // S5: overlay de estatísticas no viewport e o que o renderer relatou.
  bool sceneStatisticsVisible=false;
  renderer::SceneStatistics sceneStatistics{};
  u32 viewSelected=0;
  std::string viewName;
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
  // Opções da câmera editorial, equivalentes à barra de visualização da Scene
  // View. São estado do editor: não alteram o Ambiente salvo nem entram no
  // histórico da cena.
  bool sceneLighting=true,sceneEffects=true,sceneSky=true,sceneFog=true,scenePost=true;
  bool sceneEffectsMenu=false;
  EditorEntityId cameraViewEntity=0;
  EditorEntityId cameraPreviewEntity=0;
  bool cameraPreviewReady=false,cameraPreviewFailed=false;
  std::string cameraPreviewDiagnostic;
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
  bool impactRemoval=false;
  u64 impactRemovalEpoch=0,impactRemovalRevision=0;
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
  // Alfa e intensidade HDR (stops: cor = base × 2^intensidade) quando o campo
  // os tem; a cor com que a janela abriu, em RGB linear, para voltar a ela.
  float colorAlpha=1,colorIntensity=0;
  float colorOriginal[4]{1,1,1,1};
  bool colorHasAlpha=false,colorHdr=false;
  // 0 = RGB 0–255, 1 = RGB 0–1, 2 = HSV.
  u8 colorMode=0;
  // Alvo: 0 = triple de componente, 1 = campo de script, 2 = elemento de lista.
  u8 colorTarget=0;
  std::string colorScriptType;
  u32 colorScriptElement=0;
  // Menus da faixa de amostras: amostra (índice+1) e lista de bibliotecas.
  u32 colorSwatchMenu=0;
  bool colorLibraryMenu=false;
  // Texto em edição na janela: 1 = hexadecimal, 2 = nome da amostra, 3 = nova biblioteca.
  u8 colorText=0;
  const class EditorValueLibraries *colorLibraries=nullptr;
  // Alvo 3 da janela de cor: a parada de cor escolhida no editor de gradiente.
  // Editor de gradiente aberto (chave do campo), rascunho no formato do campo,
  // parada escolhida (índice+1, de alfa ou de cor) e parada sendo arrastada
  // para fora (vai ser apagada ao soltar).
  u32 gradientField=0;
  std::string gradientDraft,gradientType;
  EditorEntityId gradientEntity=0;
  u64 gradientInstance=0;
  std::string gradientProperty;
  u32 gradientElement=0,gradientSelected=0;
  bool gradientSelectedAlpha=false,gradientRemoving=false;
  u32 gradientPresetMenu=0;
  bool gradientLibraryMenu=false;
  u8 gradientText=0;
  const class EditorValueLibraries *gradientLibraries=nullptr;
  // Editor de curvas aberto (chave do campo), rascunho no formato do campo,
  // chave escolhida (índice+1), faixa visível {t0,v0,t1,v1} e menus de preset.
  u32 curveField=0;
  std::string curveDraft,curveType;
  EditorEntityId curveEntity=0;
  u64 curveInstance=0;
  std::string curveProperty;
  u32 curveElement=0,curveSelected=0;
  float curveView[4]{0,0,1,1};
  u32 curvePresetMenu=0;
  bool curveLibraryMenu=false;
  u8 curveText=0;
  const class EditorValueLibraries *curveLibraries=nullptr;
  // Barra do LOD Group: nível escolhido (índice+1; levelCount+1 = Culled),
  // menu do segmento aberto e a altura da vista atual em % (-1 sem leitura).
  u32 lodSelected=0,lodMenu=0;
  float lodViewPercent=-1;
  u32 numericField = 0;
  u64 numericInstance=0;
  std::string numericProperty;
  EditorEntityId numericEntity = kInvalidEntity;
  char numericText[48]{};
  // Valor do campo quando ele abriu: base de `+=` e da prévia do resultado.
  double numericCurrent=0;
  // Teclado de texto no lugar do numérico do sistema, para digitar expressões
  // (operadores, parênteses, pi, L e R não existem no teclado numérico).
  bool numericExpression=false;
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
  bool creationAsChild=false;
  bool workspaceMenu=false;
  unsigned creationCategory=0,creationSelection=0,creationScroll=0;
  std::vector<u8> creationAvailable{1,1}; // Basic object and camera until resource availability is resolved.
  // O editor não conhece Android: ele levanta o pedido e o shell abre o seletor.
  bool modelImportRequested=false,environmentImportRequested=false,textureImportRequested=false,folderImportRequested=false;
  bool importPanel=false,importReady=false,importAccept=false,importCancel=false,importIntoScene=false,importError=false;
  bool importEnvironment=false,importTexture=false;
  resources::EnvironmentMapImportSettings environmentImportSettings{};
  bool environmentImportReprepare=false;
  resources::TextureProfile textureImportSettings{},textureImportPreparedSettings{};
  bool textureImportReprepare=false;
  u32 textureImportSourceWidth=0,textureImportSourceHeight=0,textureImportDroppedMips=0;
  bool textureImportSourceHasAlpha=false;
  ui::UiRect textureImportImage{};
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
  enum class ImportTab : u8 { Summary, Structure, Meshes, Textures, Profile };
  ImportTab importTab=ImportTab::Summary;
  // Uma linha da aba Estrutura. `node` é a identidade que o nó terá no mapa —
  // a MESMA que a publicação vai gravar —, e é por ela que a exclusão é pedida.
  // `excluded` já vem propagado: filho de nó excluído também não vem.
  struct ImportNodeRow { std::string name; u32 depth=0, draws=0; resources::AssetGuid node{}; bool excluded=false; };
  // `format` é o de `renderer::AuthoringTexture` (RGBA8 ou ASTC NxN) que vai para a GPU.
  struct ImportTextureRow { u32 width=0,height=0,levels=0,uses=0,format=0; bool srgb=true; u64 bytes=0; };
  // G6-A: uma linha por malha da fonte, com os fatos que o Mesh asset Inspector
  // da Unity mostra (contagem e canais) e a medida que ela não traz — densidade
  // de texel. `level` é 0 sem nada a apontar, 1 atenção, 2 erro.
  struct ImportMeshRow {
    std::string name,material,channels,counts,size,density,stretch;
    std::vector<std::string> issues;
    u8 level=0;
  };
  std::vector<ImportNodeRow> importNodes;
  std::vector<ImportTextureRow> importTextures;
  std::vector<ImportMeshRow> importMeshes;
  // Leitura do arquivo inteiro (hierarquia, escala e densidade) e a malha
  // aberta no cartão de detalhe.
  std::string importMeshSummary;
  u32 importMeshSelected=0;
  bool importMeshDetail=false;
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
  u32 importSmoothingAngle=60,importPreparedSmoothingAngle=60;
  u8 importTangents=0,importPreparedTangents=0;
  bool importCameras=false,importPreparedCameras=false;
  bool importLights=false,importPreparedLights=false;
  // S1: compressão das texturas no aparelho (0 RGBA8, 4/6/8 ASTC NxN) e se o
  // aparelho amostra ASTC — sem isso a escolha fica salva e o painel diz que não vale aqui.
  u8 importTextureCompression=0,importPreparedTextureCompression=0;
  // S3: LOD na própria malha e ordem de índices; pedem nova preparação.
  bool importGenerateLods=false,importPreparedGenerateLods=false;
  bool importOptimizeOrder=false,importPreparedOptimizeOrder=false;
  u8 importLodLevels=4,importPreparedLodLevels=4;
  // S2: streaming das texturas desta fonte; não pede nova preparação.
  bool importTextureStreaming=true;
  i32 importTextureStreamingPriority=0;
  bool importAstcSupported=false;
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
  // Três páginas de quatro: as duas últimas da terceira são leitura do
  // streaming (mip carregado e o que a tela pede), sem toque.
  std::string textureProfileLabels[12],textureResidencyLabel;
  resources::TextureProfile textureProfileDraft{},textureProfileSaved{};
  bool textureProfileDirty=false;
  u32 textureProfilePage=0;
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
  // Nível que a câmera da vista escolhe no LOD Group selecionado, com a altura
  // relativa — a leitura da barra de LOD do Inspector da Unity. Vazio sem grupo.
  std::string lodStatus;
  // G6-B: skin e juntas resolvidos da Malha com esqueleto, e o clipe da
  // Animação selecionada (nome, duração e, no Play, o tempo corrente).
  std::string skinStatus, animationStatus;
  // Nomes dos blend shapes da malha selecionada, na ordem dos endereços de peso.
  std::vector<std::string> blendShapeNames;
  bool canUndo = false;
  bool canRedo = false;
};

struct EditorScreenLayout final {
  // Áreas contínuas da janela de cor; o toque vira valor a partir delas.
  ui::UiRect colorSquare{},colorHue{};
  ui::UiRect colorSliders[5]{};
  ui::UiRect gradientBar{},gradientLocation{},gradientAlpha{};
  ui::UiRect curveGraph{};
  ui::UiRect lodBar{};
  // Janela dos Inspectors focados (vazia quando nenhum está aberto).
  ui::UiRect focusedWindow{};
  // A área da cena, JÁ descontados os painéis. É este retângulo que
  // `EditorViewport::rect` recebe, e é por isso que a projeção do gizmo
  // continua certa quando o usuário arrasta um divisor.
  ui::UiRect viewport{};
  ui::UiRect topBar{};
  ui::UiRect hierarchyPanel{};
  ui::UiRect filesPanel{};
  ui::UiRect inspectorPanel{};
  ui::UiRect diagnosticDock{};
  // Bloco F: linhas da lista de texturas das fontes desenhadas neste quadro;
  // só elas geram miniatura.
  std::vector<u32> visibleSourceTextureRows;
  // Inspector de textura: altura rolável e quanto dela cabe na janela.
  float textureInspectorContent=0,textureInspectorWindow=0;
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
