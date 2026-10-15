#pragma once
#include "editor/editor_document.h"
#include <string>

namespace ae::editor {
// Identidade dos controles. Os valores fixos são os controles únicos; as faixas
// no fim são para os que existem por entidade ou por eixo, onde o índice entra
// no próprio identificador.
enum class EditorWidget : u32 {
  LibraryOpen=0xCD100000u,LibraryImport,LibraryClose,LibraryPrevious,LibraryNext,LibraryKinds,LibraryPackages,LibrarySearch,LibraryDetailsClose,
  LibraryReimport,LibraryKeepLocal,LibraryReceiveSource,LibraryInstantiate,LibraryControllerAssign,
  LibraryItemBase=0xCD110000u,
  LibraryItemDetailsBase=0xCD140000u,
  // Trilho de tipos da Biblioteca (+ índice do tipo, 0 = todos) e o botão
  // universal de importar da barra superior (abre o menu de contexto).
  LibraryKindBase=0xCD150000u,ImportMenu=0xCD160000u,
  ImportFbxDeformation=0xCD120000u,ImportFbxWeights,ImportFbxSteps,ImportFbxRate,ImportTabRig,
  ImportTakePrevious,ImportTakeNext,ImportTakeRoot,ImportTakeExtract,
  ColliderGeometryOpen=0xDB000200u,ColliderGeometryClose,ColliderGeometryApply,ColliderGeometryVertex,
  ColliderGeometryFace,ColliderGeometryHidden,ColliderGeometryAdditive,ColliderGeometryUndo,ColliderGeometryRedo,
  ColliderGeometryCoordinateX,ColliderGeometryCoordinateY,ColliderGeometryCoordinateZ,
  PhysicsDiagnosticOpen,PhysicsDiagnosticClose,PhysicsDiagnosticRefresh,PhysicsDiagnosticVisual,
  PhysicsDiagnosticCollision,PhysicsDiagnosticSupport,ColliderGeometryAxisX,ColliderGeometryAxisY,ColliderGeometryAxisZ,ColliderAuthoringDetails,
  TabGui = 0xDB000000u,
  GuiCanvasEdit = 0xDB000001u,
  CharacterConversionOpen=0xDB000010u,CharacterConversionApply,CharacterConversionBack,
  SceneTemplatePrevious=0xDB000019u,SceneTemplateNext,
  MotorSetupOpen=0xDB000020u,MotorSetupApply,MotorSetupBack,MotorSetupPreserve,MotorSetupFit,MotorSetupConvex,
  MotorSetupDecompose,MotorBakeStart,MotorBakeCancel,MotorBakeBudgetLow,MotorBakeBudgetMedium,MotorBakeBudgetHigh,
  MotorBakePrevious,MotorBakeNext,MotorBakeSourcesOpen,MotorBakeSourcesDone,MotorBakeSourcesObject,MotorBakeSourcesHierarchy,
  MotorBakeSourcePrevious,MotorBakeSourceNext,MotorBakePartBase=0xDB000080u,MotorBakeSourceBase=0xDB000100u,
  MotorBakeRegenerate=0xDB000300u,MotorBakeConfirmMapping,MotorBakeMappingBase=0xDB000320u,
  MotorBakeSettingsOpen=0xDB000360u,MotorBakeSettingsDone,MotorBakeSettingsPrevious,MotorBakeSettingsNext,
  MotorBakePoseRest,MotorBakePoseAuthored,MotorBakePoseAnimation,MotorBakeSettingsNumberBase=0xDB000370u,
  GuiHierarchyRowBase = 0xDD000000u,
  GuiHierarchyCollapseBase = 0xDE000000u,
  CreationCategoryBase=0x63000000,
  CreationRowBase=0x64000000,
  PrefabOverrideRevertBase=0xD0000000u,
  InspectorInspection=0xD1000000u,InspectorComponents,InspectorOverviewScroll,InspectorOverviewTransform,
  InspectorOverviewObject,InspectorReferenceFlags,InspectorReferenceFlagsClose,InspectorComponentPrevious,InspectorComponentNext,
  InspectorMeshLightmap,
  InspectorOverviewOpenBase=0xD2000000u,
  PrefabReceiveSource=0xD3000000u,
  ComponentSlotEnumBase=0xD4000000u,
  PrefabOverrideApplyBase=0xD5000000u,
  ImportWave=0xD6000000u,
  TweenRestart=0xD7000000u,TweenCancel=0xD7000001u,TweenPause=0xD7000002u,
  PathEditorOpen=0xD8000000u,PathEditorClose,PathPointList,PathPointPrevious,PathPointNext,
  PathPointInsert,PathPointRemove,PathPointMoveUp,PathPointMoveDown,PathPointTangents,
  PathPointPosition,PathPointOrientation,PathPointListPrevious,PathPointListNext,PathFollowRestart,PathFollowStop,
  PathPointSelectBase=0xD9000000u,
  PrefabOverridesRevertAll=0xDA000000u,
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
  CodeGoLine, CodeNewFolder, CodeNewHelper, CodeTemplates, CodeTools, CodeToolsBack, CodeToolsPrevious, CodeToolsNext,
  ColliderFit, LodGroupFit, LodGroupStatus, SkinnedMeshStatus, AnimationStatus, ComponentClipAdd, ComponentPrevious,
  ViewsOpen, ViewsClose, ViewSave, ViewUpdate, ViewRename, ViewDelete, ComponentNext, ScriptFieldsPrevious, ScriptFieldsNext,
  TransformFold, ComponentSearch, ComponentSearchClear,
  MeshGeometryTab, MeshMaterialTab, MeshChoose, MeshPickerClose, MeshClear, MeshSearch, MeshPrevious, MeshNext,
  MeshGenerateCollision, MeshCollisionQualityDown, MeshCollisionQualityUp,
  MeshCollisionErrorDown, MeshCollisionErrorUp, MaterialRestore,
  EnvironmentProfileCreate, EnvironmentProfileUpdate,
  PhysicsMaterialCreate, PhysicsMaterialUpdate,
  ReferenceClose, ReferenceSearch, ReferenceClear, ReferencePrevious, ReferenceNext,
  CodeTemplateClose, CodeConsole,
  ImportMatchInOrder, ImportTreatAsNew, ImportLinkMenu, ImportLinkUnlink, ImportLinkKeep, ImportLinkDelete,
  MaterialSlotPrevious, MaterialSlotNext, MaterialScopeInstance, MaterialScopeShared, MaterialChoose,
  MaterialPickerClose, MaterialClearOverride, MaterialCreateShared, MaterialUseSource,
  // Inspetor: configurações universais do objeto e ações que antes só existiam
  // no menu da hierarquia (padrão Unity/Godot: o objeto selecionado se edita
  // onde estão as propriedades dele).
  ObjectFold, ObjectLayerPrevious, ObjectLayerNext, CreateChildGroup,
  PrefabCreate,
  PrefabUnpack,
  PrefabOverrides,
  PrefabOverridesClose,
  PrefabOverridesRefresh,
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
  TextureTiffExtra=0x09f00100u, TextureTiffPrecision, TextureTiffColor, TextureTiffLosses,
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
  ColliderHandleBase=0x5E000080u,
  CameraPreviewPin=0x5E000030u, CameraPreviewClose, CameraPreviewResolution, CameraPreviewFrequency, CameraPreviewRetry,
  MaterialPreviewShape, MaterialPreviewLeft, MaterialPreviewRight, MaterialPreviewUp, MaterialPreviewDown, MaterialPreviewNear, MaterialPreviewFar,
  PresetOpen=0x5E000040u, PresetClose, PresetSave, PresetApply, PresetAdd, PresetRename, PresetDelete, PresetPrevious, PresetNext,
  // Escolha por campo do preset: marcar tudo, desmarcar tudo e paginar o diff.
  PresetSelectAll, PresetSelectNone, PresetFieldsPrevious, PresetFieldsNext, PresetSaveRecipe,
  PresetLibrary, PresetRecipeDetails, PresetRecipePrevious, PresetRecipeNext,
  PresetChoiceBase=0x5E010000u,
  // + índice da linha do diff: alterna levar ou não aquele campo.
  PresetFieldBase=0x5E020000u,
  PresetInputBase=0x5E040000u,
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
  // Conta-gotas: começar a amostrar e cancelar a amostragem.
  ColorEyedropper=0x5f000350u, ColorPickCancel,
  ColorLibraryBase=0x5f000400u, ColorSwatchBase=0x5f000500u,
  ComponentTripleBase=0x7d000000u,
  ComponentNumberBase=0x78000000u,
  MeshChoiceBase=0x79000000u,
  PropertyTweenPick=0xE0000000u,PropertyTweenChoiceBase=0xE1000000u,PropertyTweenPickerClose=0xE2000000u,
  PhysicsMaterialInspectorClose=0xE3000000u,PhysicsMaterialStepBase=0xE4000000u,
  // Editor de grafo do Animator: + código de animator_widget.
  AnimatorBase=0xE5000000u,
  // Superfície de navegação no Inspector: + código de navigation_widget.
  NavigationBase=0xE6000000u,
  // Hierarquia: caixa de ativo da linha (ITsMagic), + id do objeto.
  HierarchyActiveBase=0xE7000000u,
  // Menu de contexto: + código de context_menu_widget.
  ContextMenuBase=0xE8000000u,
  ScriptAddBase=0x71000000u, ScriptFoldBase=0x72000000u, ScriptMenuBase=0x73000000u,
  ScriptRemoveBase=0x74000000u, ScriptEnabledBase=0x75000000u, ScriptSourceBase=0x76000000u,
  ScriptFieldBase=0x77000000u,
  // Um id por modelo de script; o valor zero é o arquivo vazio.
  CodeTemplateBase=0x7c000000u,
  CodeBody=0x61000000u, CodeTabBase=0x61010000u,
  CodeToolBase=0x61020000u,

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
  // Multisseleção (Unity: Selection.objects): modo "Selecionar vários" na
  // Hierarquia e ações de conjunto.
  HierarchyMultiToggle=0xCD000000u, SelectAll, SelectChildren, SelectInvert, SelectNone,
  // "Definir como o valor de…" (Unity: Set to Value of) — fechar e uma linha por objeto.
  SetValueClose=0xCD000100u, SetValueRowBase=0xCD000200u,
  // Busca global (Unity 6000.0 Search): abrir na barra, campo, chips de
  // provedor, páginas e um resultado por linha.
  GlobalSearchOpen=0xC9000000u, GlobalSearchClose, GlobalSearchField, GlobalSearchPrevious, GlobalSearchNext,
  GlobalSearchProviderBase=0xC9000010u, GlobalSearchResultBase=0xC9000100u,
  // Layouts (Unity 6000.0 Toolbar › Layout): abrir pelo menu Cena, salvar o
  // atual, restaurar o padrão; um por linha (predefinidos e do usuário).
  LayoutsOpen=0xCA000000u, LayoutsClose, LayoutSave, LayoutReset,
  LayoutBuiltinBase=0xCA000100u, LayoutUserBase=0xCA000200u, LayoutDeleteBase=0xCA000300u,
  // Barra de status (Unity 6000.0 Manual/StatusBar): última mensagem do
  // console, contagens, atividade e a janela de trabalhos em segundo plano.
  StatusConsole=0xCB000000u, StatusTasks, StatusTasksClose, StatusTaskCancelBase=0xCB000100u,
  // Inspector de material do projeto (Unity 6000.0 Material Inspector):
  // voltar e localizar os usos na cena.
  MaterialInspectorClose=0xCC000000u, MaterialInspectorUses, MaterialInspectorApply, MaterialInspectorPreview, MaterialInspectorStandalone,
  // Mapa HDRI do projeto em Propriedades: voltar, usos, exposição da prévia,
  // receita (menos/mais por linha), reverter e aplicar com reimportação.
  EnvironmentInspectorClose=0xCC000100u, EnvironmentInspectorUses, EnvironmentExposureDown, EnvironmentExposureUp,
  EnvironmentRecipeRevert, EnvironmentRecipeApply, EnvironmentRecipeDownBase=0xCC000200u, EnvironmentRecipeUpBase=0xCC000210u,
  // Perfil de ambiente do projeto em Propriedades: voltar, usos, abas por grupo
  // do esquema e uma linha por propriedade do perfil.
  ProfileInspectorClose=0xCC000300u, ProfileInspectorUses, ProfileGroupBase=0xCC000400u, ProfileRowBase=0xCC000500u,
  // Abre o recurso do Inspector de recurso numa janela focada.
  AssetInspectorFocus=0xCC000600u,
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
  // Seleção pela vista por objeto (Unity: Scene picking), a mão da linha.
  HierarchyPickBase = 0xCE00'0000u,
  // Vários recursos em Arquivos (Unity: Ctrl/Shift+clique no Project) e o
  // Inspector deles: estreitar por tipo, campos comuns, a lista escolhida.
  TexturePickerScroll = 0xCEFF'0000u,
  FilesMultiToggle = 0xCF00'0000u, FilesSelectNone, MultiAssetApply, MultiAssetRevert, MultiAssetScroll,
  MultiAssetNarrowBase = 0xCF00'0100u, MultiAssetFieldBase = 0xCF00'0200u,
  MultiAssetItemBase = 0xCF00'0300u, MultiAssetRemoveBase = 0xCF00'0400u,
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
  ObjectTagOpen=0x0811'0000u, TagClose, TagNew, TagSearch, TagDelete, TagPrevious, TagNext,
  TagRowBase=0x0812'0000u,
  ObjectGroupsOpen=0x0813'0000u, GroupsClose, GroupNew, GroupsPrevious, GroupsNext,
  GroupEditBase=0x0814'0000u, GroupRemoveBase=0x0815'0000u,
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
  InputTabResponse,InputActionEnabled,
  InputInteractionPress,InputInteractionHold,InputInteractionTap,InputDuration,
  InputGroupTouch,InputGroupKeyboardMouse,InputGroupGamepad,
  InputDetailsToggle,
  InputBindingCapture,InputBindingCaptureNegative,InputBindingCaptureCancel,
  InputActionRowBase=0x0831'0000u,
  InputBindingRowBase=0x0832'0000u,
  PlayTimeScale=0x0000'1000u,
  TimerControlBase=0x0840'0000u, // component index * 4 + start, pause/resume, stop

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
} // namespace detail

namespace animator_widget {
enum : u32 {
  Open=0x0,Close=0x1,Canvas=0x2,AddState=0x3,Connect=0x4,SetDefault=0x5,Delete=0x6,Frame=0x7,
  LayerAdd=0x8,LayerWeight=0x9,LayerMask=0xA,LayerName=0xB,LayerDelete=0xC,ZoomIn=0xD,ZoomOut=0xE,
  Parameters=0xF,Details=0x10,Duplicate=0x11,Undo=0x12,Redo=0x13,DetailsScroll=0x14,ParamsScroll=0x15,
  ParameterSource=0x16,ParameterResponse=0x17,ParameterScale=0x18,MotionSource=0x19,
  ParameterTrigger=0x20,
  Controller=0x21,ControllerCreate=0x22,ControllerChoose=0x23,ControllerDetach=0x24,
  ControllerEdit=0x25,ControllerReload=0x26,ControllerReset=0x27,
  LayerBlend=0x28,LayerReferenceClip=0x29,LayerReferenceTime=0x2A,LayerReferenceReset=0x2B,LayerRuntimeReset=0x2C,
  AddMachine=0x2D,EnterMachine=0x2E,NodeParent=0x2F,Navigate=0x800,
  OverrideClip=0x7100,OverrideReset=0x7500,ControllerChoice=0x7900,
  OverrideOrphanReset=0x9000,
  LayerTab=0x100,ParameterAdd=0x200,ParameterName=0x300,ParameterValue=0x400,ParameterDelete=0x500,ParameterRow=0x600,
  StateName=0x1000,StateKind=0x1001,StateBlendX=0x1004,StateBlendY=0x1005,StateSpeed=0x1006,StateSpeedParameter=0x1007,
  StateLoop=0x1008,MotionAdd=0x1009,EventAdd=0x100A,
  MotionClip=0x1100,MotionThreshold=0x1200,MotionX=0x1300,MotionY=0x1400,MotionRemove=0x1500,
  EventTime=0x1700,EventTag=0x1800,EventRemove=0x1900,
  TransitionExit=0x2000,TransitionExitTime=0x2001,TransitionDuration=0x2002,ConditionAdd=0x2003,
  TransitionEarlier=0x2004,TransitionLater=0x2005,
  TransitionInterruption=0x2006,TransitionOrdered=0x2007,TransitionSelf=0x2008,TransitionOffset=0x2009,TransitionFixed=0x200A,TransitionTarget=0x200B,
  ConditionParameter=0x2100,ConditionMode=0x2200,ConditionThreshold=0x2300,ConditionRemove=0x2400,
  ClipClose=0x3000,ClipChoice=0x3100,MaskClose=0x5000,MaskChoice=0x5100,PickerPrevious=0x6000,PickerNext=0x6001,
};
inline constexpr u32 id(u32 code) noexcept {return widgetId(EditorWidget::AnimatorBase)+code;}
inline constexpr bool owns(u32 widget) noexcept {return (widget&0xff000000u)==widgetId(EditorWidget::AnimatorBase);}
inline constexpr u32 code(u32 widget) noexcept {return widget&0x00ffffffu;}
} // namespace animator_widget
namespace navigation_widget {
enum Code : u32 {Bake=1,Cancel=2,Clear=3,ShowMesh=4};
inline constexpr u32 id(u32 code) noexcept {return widgetId(EditorWidget::NavigationBase)+code;}
inline constexpr bool owns(u32 widget) noexcept {return (widget&0xff000000u)==widgetId(EditorWidget::NavigationBase);}
} // namespace navigation_widget
namespace context_menu_widget {
// Fundo (fecha), voltar, abrir Criar, objeto vazio, a folha com todas as
// receitas e as categorias de criação (+ índice). As ações em si usam os
// widgets de sempre (Duplicar, Renomear, receitas), pelo mesmo caminho.
enum Code : u32 {Close=1,Back=2,Create=3,AllRecipes=4,Category=0x10};
// Níveis: 0 ações do objeto, 1 Criar, 2+n receitas da categoria n. O menu
// Cena (espaços de trabalho) é o mesmo componente num nível próprio.
inline constexpr u32 WorkspaceLevel=0x100;
// Menu ⋮ do estúdio de animação (clipe, camadas, rig, bake, exportar).
inline constexpr u32 StudioLevel=0x200;
// Importar universal: modelo, pasta de pacote, textura, ambiente, áudio.
inline constexpr u32 ImportLevel=0x300;
// Menu da chave (toque longo numa chave da timeline): tempo, valor, tangente,
// copiar, recortar e excluir, pelos widgets do clipe.
inline constexpr u32 KeyLevel=0x400;
inline constexpr u32 id(u32 code) noexcept {return widgetId(EditorWidget::ContextMenuBase)+code;}
inline constexpr bool owns(u32 widget) noexcept {return (widget&0xff000000u)==widgetId(EditorWidget::ContextMenuBase);}
} // namespace context_menu_widget
namespace clip_widget {
enum : u32 {
  Open=0xb000,Close,New,Choose,Play,Loop,PreviousFrame,NextFrame,Time,Duration,
  Curves,Keys,Frame,Expand,Undo,Redo,AddKey,DeleteKey,KeyTime,KeyValue,Tangent,
  Canvas,RowsPrevious,RowsNext,ZoomIn,ZoomOut,Name,PickerPrevious,PickerNext,NewQuaternion,NewEuler,NewProgressive,
  TangentClose,TangentLink,ModeIn,ModeOut,SlopeIn,SlopeOut,WeightIn,WeightOut,WeightedIn,WeightedOut,HandleIn,HandleOut,
  Targets,TargetClose,TargetUp,TargetPrevious,TargetNext,TargetUse,Pose,RemoveTrack,PosePrevious,PoseNext,OwnedCatalog,ImportedCatalog,
    Selection,SelectionAdd,SelectionClear,SelectionOffset,SelectionScale,
    Edits,EditsClose,CopyKeys,CutKeys,PasteReplace,PasteInsertTracks,PasteInsertAll,
    BakeOpen,BakeClose,BakeRate,BakeTolerance,BakeReduction,BakeMode,BakeApply,
    Layers,LayerClose,LayerPrevious,LayerNext,LayerName,LayerAdd,LayerDuplicate,LayerRemove,LayerUp,LayerDown,LayerBlend,LayerWeight,LayerReference,LayerReferenceTime,LayerMute,LayerSolo,LayerCopy,LayerChoose,LayerPickerClose,LayerPickerPrevious,LayerPickerNext,
    BakeTarget,BakeReference,BakeSeedUse,BakeSeedX,BakeSeedY,BakeSeedZ,BakeOrder,
    Cues,CueAddEvent,CueAddMarker,CueName,CueTime,CueTag,CueValue,CueOptions,CueForward,CueReverse,CueEnabled,CueDuplicate,CueDelete,CuePrevious,CueNext,CueOptionsClose,
    SelectMode=0xb080,Row=0xb100,Choice=0xb200,TargetChoice=0xb300,TargetEnter=0xb340,PropertyChoice=0xb380,LayerChoice=0xb3c0,PoseValue=0xb400,
    PoseAutoKey=0xb480,PoseRecord,PoseCancel,PoseIsolate,PoseNumbers,PoseJoints,PoseTranslate,PoseRotate,PoseScale,PoseFocus,Restart,PoseOrbit,PosePan,PoseZoom,PoseViewport,
    Range=0xb4a0,RangeClose,RangeStart,RangeEnd,RangeDestination,RangeInsert,RangeRemove,RangeDuplicate,CueSelectAll,RangeMove,
    RangeTaskEdit,RangeTaskCycle,RangeTaskArrange,RangeCopies,RangeTolerance,RangeCloseLoop,RangeRepeat,
    RangeAlignStart,RangeAlignEnd,RangeAlignCenter,RangeDistribute,
    PoseTools=0xb4c0,PoseToolsClose,PoseToolsRecord,PoseToolsReuse,PosePolicy,PoseArm,PoseArmStart,PoseArmEnd,
    PoseSpace,PoseCopy,PosePaste,PoseResetIdentity,PoseResetReference,PoseUnlock,
    PoseToolsSelection,PoseSelectAdd,PoseSelectClear,PoseSelectAll,PoseSelectParent,PoseSelectChildren,PoseSelectSiblings,
    PoseSetSave,PoseSetPrevious,PoseSetNext,PoseSetLoad,PoseSetDelete,PoseSelectionIsolate,
    CurveView=0xb4e0,CurveViewClose,CurvePin,CurveClearPins,CurveNormalize,CurveIsolate,
    CurveFrameSelection,CurveValueZoomIn,CurveValueZoomOut,CurveValueUp,CurveValueDown,CurveTimeLeft,CurveTimeRight,
    CurveViewSettings,CurveViewReferences,CurveRefPrevious,CurveRefNext,CurveRefChoice=0xb4f4,
    Regions=0xb510,RegionClose,RegionPrevious,RegionNext,RegionName,RegionStart,RegionEnd,RegionAdd,RegionDelete,RegionPreview,RegionList,
    RegionChoice=0xb520,
    Properties=0xb580,PropertyClose,PropertyCatalog,PropertyTracks,PropertyPrevious,PropertyNext,
    PropertyRecord,PropertyRemove,PropertyKeyPrevious,PropertyKeyNext,PropertyKeyDelete,PropertyKeyTime,
    PropertySearch,PropertyRemap,PropertyTime,PropertyCurve,PropertyComponent,PropertyCurveFields,PropertyTangent,PropertyWeighted,
    PropertyValue=0xb5a0,PropertyEntry=0xb5b0,PropertyCurveNumber=0xb5b8,PropertyCurveKey=0xb5c0,
    Drivers=0xb600,DriverCancel,DriverApply,DriverBack,DriverAdd,DriverPrevious,DriverNext,
    DriverInput,DriverOutput,DriverCurve,DriverFields,DriverDelete,DriverClearOutput,
    DriverFirst,DriverSecond,DriverCurvePrevious,DriverCurveNext,DriverCurveAdd,
    DriverCurveDelete,DriverCurveTangent,DriverCurveWeighted,
    DriverConstant=0xb620,DriverCurveValue=0xb630,DriverOperation=0xb640,DriverEntry=0xb650,DriverEnd=0xb660,
    StudioTabKey=0xb668,StudioTabRig,StudioTabPose,StudioTabIk,BoneTreeUp,BoneTreeDown,PoseIk,PoseMirrorRig,PoseIkPoleReset,PoseLibrarySave,PoseGhost,TrackFilter,PoseIkCreate,PoseIkWizard,PosePin,PoseFollowParent,PoseFollow,PoseRootMotion,PoseBake,PoseBakeRoot,PoseExportGlb,TimeRuler,BoneTree,MasterKeys,
    BoneRow=0xb680,PoseLibrary=0xb6e0,
    // Painel do osso: canal (0 posição, 1 rotação, 2 escala) × (X, Y, Z, chave).
    BoneChannel=0xb6f0,StudioMenu=0xb6fc,
    PoseLock=0xb540,
    PoseGizmo=0xb500
};
inline constexpr u32 id(u32 code) {return animator_widget::id(code);}
inline constexpr bool owns(u32 widget) {return animator_widget::owns(widget)&&animator_widget::code(widget)>=Open&&animator_widget::code(widget)<0xb700;}
}
inline constexpr u32 hierarchyRowWidget(EditorEntityId entity) noexcept {
  return widgetId(EditorWidget::HierarchyRowBase) + entity;
}
inline constexpr u32 hierarchyEyeWidget(EditorEntityId entity) noexcept {
  return widgetId(EditorWidget::HierarchyEyeBase) + entity;
}
inline constexpr u32 hierarchyPickWidget(EditorEntityId entity) noexcept {
  return widgetId(EditorWidget::HierarchyPickBase) + entity;
}
inline std::string multiKey(u64 instance, std::string_view property) {
  return std::to_string(instance) + "/" + std::string(property);
}
inline constexpr u32 transformFieldWidget(u32 row, u32 axis) noexcept {
  return widgetId(EditorWidget::TransformFieldBase) + row * 3 + axis;
}
inline constexpr u32 gizmoAxisWidget(u32 axis) noexcept {
  return widgetId(EditorWidget::GizmoAxisBase) + axis;
}

} // namespace ae::editor
