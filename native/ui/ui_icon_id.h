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
  EditorEditPoints = 32,  // editor/edit-points
  EditorFocus = 33,  // editor/focus
  EditorFrameObject = 34,  // editor/frame-object
  EditorGizmoAxes = 35,  // editor/gizmo-axes
  EditorMove = 36,  // editor/move
  EditorOrbit = 37,  // editor/orbit
  EditorPan = 38,  // editor/pan
  EditorPivot = 39,  // editor/pivot
  EditorRedo = 40,  // editor/redo
  EditorResize = 41,  // editor/resize
  EditorRotate = 42,  // editor/rotate
  EditorScale = 43,  // editor/scale
  EditorSelect = 44,  // editor/select
  EditorSelectActive = 45,  // editor/select-active
  EditorSelectBox = 46,  // editor/select-box
  EditorSelectionBounds = 47,  // editor/selection-bounds
  EditorSnap = 48,  // editor/snap
  EditorSpaceWorld = 49,  // editor/space-world
  EditorUndo = 50,  // editor/undo
  LightingCloud = 51,  // lighting/cloud
  LightingCloudSun = 52,  // lighting/cloud-sun
  LightingExposure = 53,  // lighting/exposure
  LightingSun = 54,  // lighting/sun
  NatureArchitecture = 55,  // nature/architecture
  NatureLandscape = 56,  // nature/landscape
  NatureLeaf = 57,  // nature/leaf
  NatureMountains = 58,  // nature/mountains
  NatureRock = 59,  // nature/rock
  NatureTree = 60,  // nature/tree
  NatureWater = 61,  // nature/water
  PhysicsCharacter = 62,  // physics/character
  PrimitiveCapsule = 63,  // primitive/capsule
  PrimitiveCone = 64,  // primitive/cone
  PrimitiveCube = 65,  // primitive/cube
  PrimitiveCylinder = 66,  // primitive/cylinder
  PrimitiveSphere = 67,  // primitive/sphere
  RuntimeAudio = 68,  // runtime/audio
  RuntimeCamera = 69,  // runtime/camera
  RuntimeCameraOrbit = 70,  // runtime/camera-orbit
  RuntimePause = 71,  // runtime/pause
  RuntimePlay = 72,  // runtime/play
  RuntimeRestart = 73,  // runtime/restart
  RuntimeStop = 74,  // runtime/stop
  SceneDuplicate = 75,  // scene/duplicate
  SceneFlatten = 76,  // scene/flatten
  SceneIsolateOff = 77,  // scene/isolate-off
  SceneLayerAdd = 78,  // scene/layer-add
  SceneLayers = 79,  // scene/layers
  SceneLock = 80,  // scene/lock
  SceneObject = 81,  // scene/object
  SceneObjectAdd = 82,  // scene/object-add
  SceneObjectPreview = 83,  // scene/object-preview
  SceneObjectRemove = 84,  // scene/object-remove
  ScenePin = 85,  // scene/pin
  SceneUnlock = 86,  // scene/unlock
  SceneVisibility = 87,  // scene/visibility
  SceneVisibilityOff = 88,  // scene/visibility-off
  ScriptingCode = 89,  // scripting/code
  ScriptingNodes = 90,  // scripting/nodes
  UiAdd = 91,  // ui/add
  UiArrowLeft = 92,  // ui/arrow-left
  UiArrowRight = 93,  // ui/arrow-right
  UiCheck = 94,  // ui/check
  UiChevronDown = 95,  // ui/chevron-down
  UiChevronLeft = 96,  // ui/chevron-left
  UiChevronRight = 97,  // ui/chevron-right
  UiChevronUp = 98,  // ui/chevron-up
  UiClose = 99,  // ui/close
  UiHelp = 100,  // ui/help
  UiHome = 101,  // ui/home
  UiInfo = 102,  // ui/info
  UiMenu = 103,  // ui/menu
  UiMoreHorizontal = 104,  // ui/more-horizontal
  UiMoreVertical = 105,  // ui/more-vertical
  UiPanelLeft = 106,  // ui/panel-left
  UiPanelRight = 107,  // ui/panel-right
  UiRemove = 108,  // ui/remove
  UiSettings = 109,  // ui/settings
  UiShare = 110,  // ui/share
  UiSliders = 111,  // ui/sliders
  UiWarning = 112,  // ui/warning
  VfxParticles = 113,  // vfx/particles
  ViewCollapse = 114,  // view/collapse
  ViewCorners = 115,  // view/corners
  ViewExpand = 116,  // view/expand
  ViewGrid = 117,  // view/grid
  BrandWordmark = 118,  // brand/wordmark
  BrandMark = 119,  // brand/mark
};

inline constexpr u32 kUiIconCount = 119;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
