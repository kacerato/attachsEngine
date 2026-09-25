// GERADO por tools/pack-icon-atlas.py — não edite à mão.
//
// O índice de cada ícone é a posição dele no atlas binário `astra-ui-icons.aeui`.
// Os dois são produzidos na mesma execução: mudar a ordem aqui sem regerar o
// binário faria a interface desenhar o ícone errado, em silêncio.
//
// `kUiNoIcon` é zero e não indexa nada — é o valor de "sem ícone", pela mesma
// razão que `kUiNoImage` existe em ui_draw_list.h.
#pragma once

#include "core/base.h"

namespace ae::ui {

enum class UiIcon : u32 {
  None = 0,
  AssetsAnimation = 1,  // assets/animation
  AssetsBookmark = 2,  // assets/bookmark
  AssetsChecker = 3,  // assets/checker
  AssetsCloudDownload = 4,  // assets/cloud-download
  AssetsCloudUpload = 5,  // assets/cloud-upload
  AssetsCopy = 6,  // assets/copy
  AssetsDocs = 7,  // assets/docs
  AssetsDownload = 8,  // assets/download
  AssetsExport = 9,  // assets/export
  AssetsFile = 10,  // assets/file
  AssetsFileMesh = 11,  // assets/file-mesh
  AssetsFilter = 12,  // assets/filter
  AssetsFolder = 13,  // assets/folder
  AssetsFolderOpen = 14,  // assets/folder-open
  AssetsGrid = 15,  // assets/grid
  AssetsImport = 16,  // assets/import
  AssetsLibrary = 17,  // assets/library
  AssetsList = 18,  // assets/list
  AssetsMaterial = 19,  // assets/material
  AssetsMaterialChecker = 20,  // assets/material-checker
  AssetsPackage = 21,  // assets/package
  AssetsSave = 22,  // assets/save
  AssetsSearch = 23,  // assets/search
  AssetsStaticMesh = 24,  // assets/static-mesh
  AssetsTexture = 25,  // assets/texture
  AssetsUpload = 26,  // assets/upload
  AssetsVideo = 27,  // assets/video
  ComponentAdd = 28,  // component/add
  ComponentCharacter = 29,  // component/character
  ComponentCollider = 30,  // component/collider
  ComponentJoint = 31,  // component/joint
  ComponentLook = 32,  // component/look
  ComponentPhysics = 33,  // component/physics
  DebugBug = 34,  // debug/bug
  DebugStats = 35,  // debug/stats
  DebugTools = 36,  // debug/tools
  EditorAlign = 37,  // editor/align
  EditorAuthorAdd = 38,  // editor/author-add
  EditorAuthorCamera = 39,  // editor/author-camera
  EditorAuthorChevron = 40,  // editor/author-chevron
  EditorAuthorEye = 41,  // editor/author-eye
  EditorAuthorEyeOff = 42,  // editor/author-eye-off
  EditorAuthorFolder = 43,  // editor/author-folder
  EditorAuthorFrame = 44,  // editor/author-frame
  EditorAuthorGrid = 45,  // editor/author-grid
  EditorAuthorMore = 46,  // editor/author-more
  EditorAuthorMove = 47,  // editor/author-move
  EditorAuthorObject = 48,  // editor/author-object
  EditorAuthorOrbit = 49,  // editor/author-orbit
  EditorAuthorPan = 50,  // editor/author-pan
  EditorAuthorPlay = 51,  // editor/author-play
  EditorAuthorRedo = 52,  // editor/author-redo
  EditorAuthorRotate = 53,  // editor/author-rotate
  EditorAuthorScale = 54,  // editor/author-scale
  EditorAuthorSelect = 55,  // editor/author-select
  EditorAuthorSettings = 56,  // editor/author-settings
  EditorAuthorStop = 57,  // editor/author-stop
  EditorAuthorSun = 58,  // editor/author-sun
  EditorAuthorUndo = 59,  // editor/author-undo
  EditorAuthorZoom = 60,  // editor/author-zoom
  EditorEditPoints = 61,  // editor/edit-points
  EditorFocus = 62,  // editor/focus
  EditorFrameObject = 63,  // editor/frame-object
  EditorGizmoAxes = 64,  // editor/gizmo-axes
  EditorMove = 65,  // editor/move
  EditorOrbit = 66,  // editor/orbit
  EditorPan = 67,  // editor/pan
  EditorPivot = 68,  // editor/pivot
  EditorRedo = 69,  // editor/redo
  EditorResize = 70,  // editor/resize
  EditorRotate = 71,  // editor/rotate
  EditorScale = 72,  // editor/scale
  EditorSelect = 73,  // editor/select
  EditorSelectActive = 74,  // editor/select-active
  EditorSelectBox = 75,  // editor/select-box
  EditorSelectionBounds = 76,  // editor/selection-bounds
  EditorSnap = 77,  // editor/snap
  EditorSpaceWorld = 78,  // editor/space-world
  EditorUndo = 79,  // editor/undo
  IdeAdd = 80,  // ide/add
  IdeCode = 81,  // ide/code
  IdeCompile = 82,  // ide/compile
  IdeConsole = 83,  // ide/console
  IdeCopy = 84,  // ide/copy
  IdeError = 85,  // ide/error
  IdeFiles = 86,  // ide/files
  IdePlay = 87,  // ide/play
  IdeSave = 88,  // ide/save
  IdeSearch = 89,  // ide/search
  IdeUndo = 90,  // ide/undo
  IdeWarning = 91,  // ide/warning
  LightingCloud = 92,  // lighting/cloud
  LightingCloudSun = 93,  // lighting/cloud-sun
  LightingExposure = 94,  // lighting/exposure
  LightingFog = 95,  // lighting/fog
  LightingPostProcessing = 96,  // lighting/post-processing
  LightingSceneEffects = 97,  // lighting/scene-effects
  LightingSceneLighting = 98,  // lighting/scene-lighting
  LightingSky = 99,  // lighting/sky
  LightingSun = 100,  // lighting/sun
  NatureArchitecture = 101,  // nature/architecture
  NatureLandscape = 102,  // nature/landscape
  NatureLeaf = 103,  // nature/leaf
  NatureMountains = 104,  // nature/mountains
  NatureRock = 105,  // nature/rock
  NatureTree = 106,  // nature/tree
  NatureWater = 107,  // nature/water
  PhysicsCharacter = 108,  // physics/character
  PrimitiveCapsule = 109,  // primitive/capsule
  PrimitiveCone = 110,  // primitive/cone
  PrimitiveCube = 111,  // primitive/cube
  PrimitiveCylinder = 112,  // primitive/cylinder
  PrimitiveSphere = 113,  // primitive/sphere
  RuntimeAudio = 114,  // runtime/audio
  RuntimeCamera = 115,  // runtime/camera
  RuntimeCameraOrbit = 116,  // runtime/camera-orbit
  RuntimePause = 117,  // runtime/pause
  RuntimePlay = 118,  // runtime/play
  RuntimeRestart = 119,  // runtime/restart
  RuntimeStop = 120,  // runtime/stop
  SceneDuplicate = 121,  // scene/duplicate
  SceneFlatten = 122,  // scene/flatten
  SceneIsolateOff = 123,  // scene/isolate-off
  SceneLayerAdd = 124,  // scene/layer-add
  SceneLayers = 125,  // scene/layers
  SceneLock = 126,  // scene/lock
  SceneObject = 127,  // scene/object
  SceneObjectAdd = 128,  // scene/object-add
  SceneObjectPreview = 129,  // scene/object-preview
  SceneObjectRemove = 130,  // scene/object-remove
  ScenePin = 131,  // scene/pin
  SceneUnlock = 132,  // scene/unlock
  SceneVisibility = 133,  // scene/visibility
  SceneVisibilityOff = 134,  // scene/visibility-off
  ScriptingCode = 135,  // scripting/code
  ScriptingNodes = 136,  // scripting/nodes
  UiAdd = 137,  // ui/add
  UiArrowLeft = 138,  // ui/arrow-left
  UiArrowRight = 139,  // ui/arrow-right
  UiCheck = 140,  // ui/check
  UiChevronDown = 141,  // ui/chevron-down
  UiChevronLeft = 142,  // ui/chevron-left
  UiChevronRight = 143,  // ui/chevron-right
  UiChevronUp = 144,  // ui/chevron-up
  UiClose = 145,  // ui/close
  UiDiagnostics = 146,  // ui/diagnostics
  UiHelp = 147,  // ui/help
  UiHome = 148,  // ui/home
  UiInfo = 149,  // ui/info
  UiMenu = 150,  // ui/menu
  UiMoreHorizontal = 151,  // ui/more-horizontal
  UiMoreVertical = 152,  // ui/more-vertical
  UiPanelLeft = 153,  // ui/panel-left
  UiPanelRight = 154,  // ui/panel-right
  UiRemove = 155,  // ui/remove
  UiSettings = 156,  // ui/settings
  UiShare = 157,  // ui/share
  UiSliders = 158,  // ui/sliders
  UiWarning = 159,  // ui/warning
  VfxParticles = 160,  // vfx/particles
  ViewCollapse = 161,  // view/collapse
  ViewCorners = 162,  // view/corners
  ViewExpand = 163,  // view/expand
  ViewGrid = 164,  // view/grid
  WaterAuthorFlow = 165,  // water/author-flow
  WaterAuthorLayers = 166,  // water/author-layers
  WaterAuthorPhysics = 167,  // water/author-physics
  WaterAuthorPoints = 168,  // water/author-points
  WaterAuthorRoute = 169,  // water/author-route
  WaterAuthorSurface = 170,  // water/author-surface
  BrandWordmark = 171,  // brand/wordmark
  BrandMark = 172,  // brand/mark
};

inline constexpr u32 kUiIconCount = 172;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
