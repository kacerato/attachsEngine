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
  InputAction = 92,  // input/action
  InputBinding = 93,  // input/binding
  LightingCloud = 94,  // lighting/cloud
  LightingCloudSun = 95,  // lighting/cloud-sun
  LightingExposure = 96,  // lighting/exposure
  LightingFog = 97,  // lighting/fog
  LightingPostProcessing = 98,  // lighting/post-processing
  LightingSceneEffects = 99,  // lighting/scene-effects
  LightingSceneLighting = 100,  // lighting/scene-lighting
  LightingSky = 101,  // lighting/sky
  LightingSun = 102,  // lighting/sun
  NatureArchitecture = 103,  // nature/architecture
  NatureLandscape = 104,  // nature/landscape
  NatureLeaf = 105,  // nature/leaf
  NatureMountains = 106,  // nature/mountains
  NatureRock = 107,  // nature/rock
  NatureTree = 108,  // nature/tree
  NatureWater = 109,  // nature/water
  PhysicsCharacter = 110,  // physics/character
  PrimitiveCapsule = 111,  // primitive/capsule
  PrimitiveCone = 112,  // primitive/cone
  PrimitiveCube = 113,  // primitive/cube
  PrimitiveCylinder = 114,  // primitive/cylinder
  PrimitiveSphere = 115,  // primitive/sphere
  RuntimeAudio = 116,  // runtime/audio
  RuntimeCamera = 117,  // runtime/camera
  RuntimeCameraOrbit = 118,  // runtime/camera-orbit
  RuntimePause = 119,  // runtime/pause
  RuntimePlay = 120,  // runtime/play
  RuntimeRestart = 121,  // runtime/restart
  RuntimeStop = 122,  // runtime/stop
  SceneDuplicate = 123,  // scene/duplicate
  SceneFlatten = 124,  // scene/flatten
  SceneIsolateOff = 125,  // scene/isolate-off
  SceneLayerAdd = 126,  // scene/layer-add
  SceneLayers = 127,  // scene/layers
  SceneLock = 128,  // scene/lock
  SceneObject = 129,  // scene/object
  SceneObjectAdd = 130,  // scene/object-add
  SceneObjectPreview = 131,  // scene/object-preview
  SceneObjectRemove = 132,  // scene/object-remove
  ScenePin = 133,  // scene/pin
  SceneUnlock = 134,  // scene/unlock
  SceneVisibility = 135,  // scene/visibility
  SceneVisibilityOff = 136,  // scene/visibility-off
  ScriptingCode = 137,  // scripting/code
  ScriptingNodes = 138,  // scripting/nodes
  UiAdd = 139,  // ui/add
  UiArrowLeft = 140,  // ui/arrow-left
  UiArrowRight = 141,  // ui/arrow-right
  UiCheck = 142,  // ui/check
  UiChevronDown = 143,  // ui/chevron-down
  UiChevronLeft = 144,  // ui/chevron-left
  UiChevronRight = 145,  // ui/chevron-right
  UiChevronUp = 146,  // ui/chevron-up
  UiClose = 147,  // ui/close
  UiDiagnostics = 148,  // ui/diagnostics
  UiHelp = 149,  // ui/help
  UiHome = 150,  // ui/home
  UiInfo = 151,  // ui/info
  UiMenu = 152,  // ui/menu
  UiMoreHorizontal = 153,  // ui/more-horizontal
  UiMoreVertical = 154,  // ui/more-vertical
  UiPanelLeft = 155,  // ui/panel-left
  UiPanelRight = 156,  // ui/panel-right
  UiRemove = 157,  // ui/remove
  UiSettings = 158,  // ui/settings
  UiShare = 159,  // ui/share
  UiSliders = 160,  // ui/sliders
  UiWarning = 161,  // ui/warning
  VfxParticles = 162,  // vfx/particles
  ViewCollapse = 163,  // view/collapse
  ViewCorners = 164,  // view/corners
  ViewExpand = 165,  // view/expand
  ViewGrid = 166,  // view/grid
  WaterAuthorFlow = 167,  // water/author-flow
  WaterAuthorLayers = 168,  // water/author-layers
  WaterAuthorPhysics = 169,  // water/author-physics
  WaterAuthorPoints = 170,  // water/author-points
  WaterAuthorRoute = 171,  // water/author-route
  WaterAuthorSurface = 172,  // water/author-surface
  BrandWordmark = 173,  // brand/wordmark
  BrandMark = 174,  // brand/mark
};

inline constexpr u32 kUiIconCount = 174;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
