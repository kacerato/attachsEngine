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
  LightingCloud = 80,  // lighting/cloud
  LightingCloudSun = 81,  // lighting/cloud-sun
  LightingExposure = 82,  // lighting/exposure
  LightingSun = 83,  // lighting/sun
  NatureArchitecture = 84,  // nature/architecture
  NatureLandscape = 85,  // nature/landscape
  NatureLeaf = 86,  // nature/leaf
  NatureMountains = 87,  // nature/mountains
  NatureRock = 88,  // nature/rock
  NatureTree = 89,  // nature/tree
  NatureWater = 90,  // nature/water
  PhysicsCharacter = 91,  // physics/character
  PrimitiveCapsule = 92,  // primitive/capsule
  PrimitiveCone = 93,  // primitive/cone
  PrimitiveCube = 94,  // primitive/cube
  PrimitiveCylinder = 95,  // primitive/cylinder
  PrimitiveSphere = 96,  // primitive/sphere
  RuntimeAudio = 97,  // runtime/audio
  RuntimeCamera = 98,  // runtime/camera
  RuntimeCameraOrbit = 99,  // runtime/camera-orbit
  RuntimePause = 100,  // runtime/pause
  RuntimePlay = 101,  // runtime/play
  RuntimeRestart = 102,  // runtime/restart
  RuntimeStop = 103,  // runtime/stop
  SceneDuplicate = 104,  // scene/duplicate
  SceneFlatten = 105,  // scene/flatten
  SceneIsolateOff = 106,  // scene/isolate-off
  SceneLayerAdd = 107,  // scene/layer-add
  SceneLayers = 108,  // scene/layers
  SceneLock = 109,  // scene/lock
  SceneObject = 110,  // scene/object
  SceneObjectAdd = 111,  // scene/object-add
  SceneObjectPreview = 112,  // scene/object-preview
  SceneObjectRemove = 113,  // scene/object-remove
  ScenePin = 114,  // scene/pin
  SceneUnlock = 115,  // scene/unlock
  SceneVisibility = 116,  // scene/visibility
  SceneVisibilityOff = 117,  // scene/visibility-off
  ScriptingCode = 118,  // scripting/code
  ScriptingNodes = 119,  // scripting/nodes
  UiAdd = 120,  // ui/add
  UiArrowLeft = 121,  // ui/arrow-left
  UiArrowRight = 122,  // ui/arrow-right
  UiCheck = 123,  // ui/check
  UiChevronDown = 124,  // ui/chevron-down
  UiChevronLeft = 125,  // ui/chevron-left
  UiChevronRight = 126,  // ui/chevron-right
  UiChevronUp = 127,  // ui/chevron-up
  UiClose = 128,  // ui/close
  UiHelp = 129,  // ui/help
  UiHome = 130,  // ui/home
  UiInfo = 131,  // ui/info
  UiMenu = 132,  // ui/menu
  UiMoreHorizontal = 133,  // ui/more-horizontal
  UiMoreVertical = 134,  // ui/more-vertical
  UiPanelLeft = 135,  // ui/panel-left
  UiPanelRight = 136,  // ui/panel-right
  UiRemove = 137,  // ui/remove
  UiSettings = 138,  // ui/settings
  UiShare = 139,  // ui/share
  UiSliders = 140,  // ui/sliders
  UiWarning = 141,  // ui/warning
  VfxParticles = 142,  // vfx/particles
  ViewCollapse = 143,  // view/collapse
  ViewCorners = 144,  // view/corners
  ViewExpand = 145,  // view/expand
  ViewGrid = 146,  // view/grid
  WaterAuthorFlow = 147,  // water/author-flow
  WaterAuthorLayers = 148,  // water/author-layers
  WaterAuthorPhysics = 149,  // water/author-physics
  WaterAuthorPoints = 150,  // water/author-points
  WaterAuthorRoute = 151,  // water/author-route
  WaterAuthorSurface = 152,  // water/author-surface
  BrandWordmark = 153,  // brand/wordmark
  BrandMark = 154,  // brand/mark
};

inline constexpr u32 kUiIconCount = 154;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
