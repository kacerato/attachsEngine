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
  DebugBug = 28,  // debug/bug
  DebugStats = 29,  // debug/stats
  DebugTools = 30,  // debug/tools
  EditorAlign = 31,  // editor/align
  EditorAuthorAdd = 32,  // editor/author-add
  EditorAuthorCamera = 33,  // editor/author-camera
  EditorAuthorChevron = 34,  // editor/author-chevron
  EditorAuthorEye = 35,  // editor/author-eye
  EditorAuthorEyeOff = 36,  // editor/author-eye-off
  EditorAuthorFolder = 37,  // editor/author-folder
  EditorAuthorFrame = 38,  // editor/author-frame
  EditorAuthorGrid = 39,  // editor/author-grid
  EditorAuthorMore = 40,  // editor/author-more
  EditorAuthorMove = 41,  // editor/author-move
  EditorAuthorObject = 42,  // editor/author-object
  EditorAuthorOrbit = 43,  // editor/author-orbit
  EditorAuthorPan = 44,  // editor/author-pan
  EditorAuthorPlay = 45,  // editor/author-play
  EditorAuthorRedo = 46,  // editor/author-redo
  EditorAuthorRotate = 47,  // editor/author-rotate
  EditorAuthorScale = 48,  // editor/author-scale
  EditorAuthorSelect = 49,  // editor/author-select
  EditorAuthorSettings = 50,  // editor/author-settings
  EditorAuthorStop = 51,  // editor/author-stop
  EditorAuthorSun = 52,  // editor/author-sun
  EditorAuthorUndo = 53,  // editor/author-undo
  EditorAuthorZoom = 54,  // editor/author-zoom
  EditorEditPoints = 55,  // editor/edit-points
  EditorFocus = 56,  // editor/focus
  EditorFrameObject = 57,  // editor/frame-object
  EditorGizmoAxes = 58,  // editor/gizmo-axes
  EditorMove = 59,  // editor/move
  EditorOrbit = 60,  // editor/orbit
  EditorPan = 61,  // editor/pan
  EditorPivot = 62,  // editor/pivot
  EditorRedo = 63,  // editor/redo
  EditorResize = 64,  // editor/resize
  EditorRotate = 65,  // editor/rotate
  EditorScale = 66,  // editor/scale
  EditorSelect = 67,  // editor/select
  EditorSelectActive = 68,  // editor/select-active
  EditorSelectBox = 69,  // editor/select-box
  EditorSelectionBounds = 70,  // editor/selection-bounds
  EditorSnap = 71,  // editor/snap
  EditorSpaceWorld = 72,  // editor/space-world
  EditorUndo = 73,  // editor/undo
  LightingCloud = 74,  // lighting/cloud
  LightingCloudSun = 75,  // lighting/cloud-sun
  LightingExposure = 76,  // lighting/exposure
  LightingSun = 77,  // lighting/sun
  NatureArchitecture = 78,  // nature/architecture
  NatureLandscape = 79,  // nature/landscape
  NatureLeaf = 80,  // nature/leaf
  NatureMountains = 81,  // nature/mountains
  NatureRock = 82,  // nature/rock
  NatureTree = 83,  // nature/tree
  NatureWater = 84,  // nature/water
  PhysicsCharacter = 85,  // physics/character
  PrimitiveCapsule = 86,  // primitive/capsule
  PrimitiveCone = 87,  // primitive/cone
  PrimitiveCube = 88,  // primitive/cube
  PrimitiveCylinder = 89,  // primitive/cylinder
  PrimitiveSphere = 90,  // primitive/sphere
  RuntimeAudio = 91,  // runtime/audio
  RuntimeCamera = 92,  // runtime/camera
  RuntimeCameraOrbit = 93,  // runtime/camera-orbit
  RuntimePause = 94,  // runtime/pause
  RuntimePlay = 95,  // runtime/play
  RuntimeRestart = 96,  // runtime/restart
  RuntimeStop = 97,  // runtime/stop
  SceneDuplicate = 98,  // scene/duplicate
  SceneFlatten = 99,  // scene/flatten
  SceneIsolateOff = 100,  // scene/isolate-off
  SceneLayerAdd = 101,  // scene/layer-add
  SceneLayers = 102,  // scene/layers
  SceneLock = 103,  // scene/lock
  SceneObject = 104,  // scene/object
  SceneObjectAdd = 105,  // scene/object-add
  SceneObjectPreview = 106,  // scene/object-preview
  SceneObjectRemove = 107,  // scene/object-remove
  ScenePin = 108,  // scene/pin
  SceneUnlock = 109,  // scene/unlock
  SceneVisibility = 110,  // scene/visibility
  SceneVisibilityOff = 111,  // scene/visibility-off
  ScriptingCode = 112,  // scripting/code
  ScriptingNodes = 113,  // scripting/nodes
  UiAdd = 114,  // ui/add
  UiArrowLeft = 115,  // ui/arrow-left
  UiArrowRight = 116,  // ui/arrow-right
  UiCheck = 117,  // ui/check
  UiChevronDown = 118,  // ui/chevron-down
  UiChevronLeft = 119,  // ui/chevron-left
  UiChevronRight = 120,  // ui/chevron-right
  UiChevronUp = 121,  // ui/chevron-up
  UiClose = 122,  // ui/close
  UiHelp = 123,  // ui/help
  UiHome = 124,  // ui/home
  UiInfo = 125,  // ui/info
  UiMenu = 126,  // ui/menu
  UiMoreHorizontal = 127,  // ui/more-horizontal
  UiMoreVertical = 128,  // ui/more-vertical
  UiPanelLeft = 129,  // ui/panel-left
  UiPanelRight = 130,  // ui/panel-right
  UiRemove = 131,  // ui/remove
  UiSettings = 132,  // ui/settings
  UiShare = 133,  // ui/share
  UiSliders = 134,  // ui/sliders
  UiWarning = 135,  // ui/warning
  VfxParticles = 136,  // vfx/particles
  ViewCollapse = 137,  // view/collapse
  ViewCorners = 138,  // view/corners
  ViewExpand = 139,  // view/expand
  ViewGrid = 140,  // view/grid
  WaterAuthorFlow = 141,  // water/author-flow
  WaterAuthorLayers = 142,  // water/author-layers
  WaterAuthorPhysics = 143,  // water/author-physics
  WaterAuthorPoints = 144,  // water/author-points
  WaterAuthorRoute = 145,  // water/author-route
  WaterAuthorSurface = 146,  // water/author-surface
  BrandWordmark = 147,  // brand/wordmark
  BrandMark = 148,  // brand/mark
};

inline constexpr u32 kUiIconCount = 148;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
