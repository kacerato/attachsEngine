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
  LightingSun = 95,  // lighting/sun
  NatureArchitecture = 96,  // nature/architecture
  NatureLandscape = 97,  // nature/landscape
  NatureLeaf = 98,  // nature/leaf
  NatureMountains = 99,  // nature/mountains
  NatureRock = 100,  // nature/rock
  NatureTree = 101,  // nature/tree
  NatureWater = 102,  // nature/water
  PhysicsCharacter = 103,  // physics/character
  PrimitiveCapsule = 104,  // primitive/capsule
  PrimitiveCone = 105,  // primitive/cone
  PrimitiveCube = 106,  // primitive/cube
  PrimitiveCylinder = 107,  // primitive/cylinder
  PrimitiveSphere = 108,  // primitive/sphere
  RuntimeAudio = 109,  // runtime/audio
  RuntimeCamera = 110,  // runtime/camera
  RuntimeCameraOrbit = 111,  // runtime/camera-orbit
  RuntimePause = 112,  // runtime/pause
  RuntimePlay = 113,  // runtime/play
  RuntimeRestart = 114,  // runtime/restart
  RuntimeStop = 115,  // runtime/stop
  SceneDuplicate = 116,  // scene/duplicate
  SceneFlatten = 117,  // scene/flatten
  SceneIsolateOff = 118,  // scene/isolate-off
  SceneLayerAdd = 119,  // scene/layer-add
  SceneLayers = 120,  // scene/layers
  SceneLock = 121,  // scene/lock
  SceneObject = 122,  // scene/object
  SceneObjectAdd = 123,  // scene/object-add
  SceneObjectPreview = 124,  // scene/object-preview
  SceneObjectRemove = 125,  // scene/object-remove
  ScenePin = 126,  // scene/pin
  SceneUnlock = 127,  // scene/unlock
  SceneVisibility = 128,  // scene/visibility
  SceneVisibilityOff = 129,  // scene/visibility-off
  ScriptingCode = 130,  // scripting/code
  ScriptingNodes = 131,  // scripting/nodes
  UiAdd = 132,  // ui/add
  UiArrowLeft = 133,  // ui/arrow-left
  UiArrowRight = 134,  // ui/arrow-right
  UiCheck = 135,  // ui/check
  UiChevronDown = 136,  // ui/chevron-down
  UiChevronLeft = 137,  // ui/chevron-left
  UiChevronRight = 138,  // ui/chevron-right
  UiChevronUp = 139,  // ui/chevron-up
  UiClose = 140,  // ui/close
  UiHelp = 141,  // ui/help
  UiHome = 142,  // ui/home
  UiInfo = 143,  // ui/info
  UiMenu = 144,  // ui/menu
  UiMoreHorizontal = 145,  // ui/more-horizontal
  UiMoreVertical = 146,  // ui/more-vertical
  UiPanelLeft = 147,  // ui/panel-left
  UiPanelRight = 148,  // ui/panel-right
  UiRemove = 149,  // ui/remove
  UiSettings = 150,  // ui/settings
  UiShare = 151,  // ui/share
  UiSliders = 152,  // ui/sliders
  UiWarning = 153,  // ui/warning
  VfxParticles = 154,  // vfx/particles
  ViewCollapse = 155,  // view/collapse
  ViewCorners = 156,  // view/corners
  ViewExpand = 157,  // view/expand
  ViewGrid = 158,  // view/grid
  WaterAuthorFlow = 159,  // water/author-flow
  WaterAuthorLayers = 160,  // water/author-layers
  WaterAuthorPhysics = 161,  // water/author-physics
  WaterAuthorPoints = 162,  // water/author-points
  WaterAuthorRoute = 163,  // water/author-route
  WaterAuthorSurface = 164,  // water/author-surface
  BrandWordmark = 165,  // brand/wordmark
  BrandMark = 166,  // brand/mark
};

inline constexpr u32 kUiIconCount = 166;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
