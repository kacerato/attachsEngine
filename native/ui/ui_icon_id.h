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
  ComponentCameraFollow = 29,  // component/camera-follow
  ComponentCharacter = 30,  // component/character
  ComponentCollider = 31,  // component/collider
  ComponentJoint = 32,  // component/joint
  ComponentLodGroup = 33,  // component/lod-group
  ComponentLook = 34,  // component/look
  ComponentPhysics = 35,  // component/physics
  ComponentSkinnedMesh = 36,  // component/skinned-mesh
  ComponentTimer = 37,  // component/timer
  DebugBug = 38,  // debug/bug
  DebugStats = 39,  // debug/stats
  DebugTools = 40,  // debug/tools
  EditorAlign = 41,  // editor/align
  EditorAuthorAdd = 42,  // editor/author-add
  EditorAuthorCamera = 43,  // editor/author-camera
  EditorAuthorChevron = 44,  // editor/author-chevron
  EditorAuthorEye = 45,  // editor/author-eye
  EditorAuthorEyeOff = 46,  // editor/author-eye-off
  EditorAuthorFolder = 47,  // editor/author-folder
  EditorAuthorFrame = 48,  // editor/author-frame
  EditorAuthorGrid = 49,  // editor/author-grid
  EditorAuthorMore = 50,  // editor/author-more
  EditorAuthorMove = 51,  // editor/author-move
  EditorAuthorObject = 52,  // editor/author-object
  EditorAuthorOrbit = 53,  // editor/author-orbit
  EditorAuthorPan = 54,  // editor/author-pan
  EditorAuthorPlay = 55,  // editor/author-play
  EditorAuthorRedo = 56,  // editor/author-redo
  EditorAuthorRotate = 57,  // editor/author-rotate
  EditorAuthorScale = 58,  // editor/author-scale
  EditorAuthorSelect = 59,  // editor/author-select
  EditorAuthorSettings = 60,  // editor/author-settings
  EditorAuthorStop = 61,  // editor/author-stop
  EditorAuthorSun = 62,  // editor/author-sun
  EditorAuthorUndo = 63,  // editor/author-undo
  EditorAuthorZoom = 64,  // editor/author-zoom
  EditorEditPoints = 65,  // editor/edit-points
  EditorFocus = 66,  // editor/focus
  EditorFrameObject = 67,  // editor/frame-object
  EditorGizmoAxes = 68,  // editor/gizmo-axes
  EditorMove = 69,  // editor/move
  EditorOrbit = 70,  // editor/orbit
  EditorPan = 71,  // editor/pan
  EditorPivot = 72,  // editor/pivot
  EditorRedo = 73,  // editor/redo
  EditorResize = 74,  // editor/resize
  EditorRotate = 75,  // editor/rotate
  EditorScale = 76,  // editor/scale
  EditorSelect = 77,  // editor/select
  EditorSelectActive = 78,  // editor/select-active
  EditorSelectBox = 79,  // editor/select-box
  EditorSelectionBounds = 80,  // editor/selection-bounds
  EditorSnap = 81,  // editor/snap
  EditorSpaceWorld = 82,  // editor/space-world
  EditorUndo = 83,  // editor/undo
  IdeAdd = 84,  // ide/add
  IdeCode = 85,  // ide/code
  IdeCompile = 86,  // ide/compile
  IdeConsole = 87,  // ide/console
  IdeCopy = 88,  // ide/copy
  IdeError = 89,  // ide/error
  IdeFiles = 90,  // ide/files
  IdePlay = 91,  // ide/play
  IdeSave = 92,  // ide/save
  IdeSearch = 93,  // ide/search
  IdeUndo = 94,  // ide/undo
  IdeWarning = 95,  // ide/warning
  InputAction = 96,  // input/action
  InputBinding = 97,  // input/binding
  LightDirectional = 98,  // light/directional
  LightPoint = 99,  // light/point
  LightSpot = 100,  // light/spot
  LightingCloud = 101,  // lighting/cloud
  LightingCloudSun = 102,  // lighting/cloud-sun
  LightingExposure = 103,  // lighting/exposure
  LightingFog = 104,  // lighting/fog
  LightingPostProcessing = 105,  // lighting/post-processing
  LightingSceneEffects = 106,  // lighting/scene-effects
  LightingSceneLighting = 107,  // lighting/scene-lighting
  LightingSky = 108,  // lighting/sky
  LightingSun = 109,  // lighting/sun
  NatureArchitecture = 110,  // nature/architecture
  NatureLandscape = 111,  // nature/landscape
  NatureLeaf = 112,  // nature/leaf
  NatureMountains = 113,  // nature/mountains
  NatureRock = 114,  // nature/rock
  NatureTree = 115,  // nature/tree
  NatureWater = 116,  // nature/water
  PhysicsCharacter = 117,  // physics/character
  PhysicsDynamicBox = 118,  // physics/dynamic-box
  PhysicsDynamicCapsule = 119,  // physics/dynamic-capsule
  PhysicsDynamicSphere = 120,  // physics/dynamic-sphere
  PhysicsKinematicBox = 121,  // physics/kinematic-box
  PhysicsKinematicSphere = 122,  // physics/kinematic-sphere
  PhysicsSensorBox = 123,  // physics/sensor-box
  PhysicsSensorCapsule = 124,  // physics/sensor-capsule
  PhysicsSensorSphere = 125,  // physics/sensor-sphere
  PhysicsStaticBox = 126,  // physics/static-box
  PhysicsStaticCapsule = 127,  // physics/static-capsule
  PhysicsStaticSphere = 128,  // physics/static-sphere
  PrimitiveCapsule = 129,  // primitive/capsule
  PrimitiveCone = 130,  // primitive/cone
  PrimitiveCube = 131,  // primitive/cube
  PrimitiveCylinder = 132,  // primitive/cylinder
  PrimitiveSphere = 133,  // primitive/sphere
  RuntimeAudio = 134,  // runtime/audio
  RuntimeCamera = 135,  // runtime/camera
  RuntimeCameraOrbit = 136,  // runtime/camera-orbit
  RuntimePause = 137,  // runtime/pause
  RuntimePlay = 138,  // runtime/play
  RuntimeRestart = 139,  // runtime/restart
  RuntimeStop = 140,  // runtime/stop
  SceneDuplicate = 141,  // scene/duplicate
  SceneFlatten = 142,  // scene/flatten
  SceneIsolateOff = 143,  // scene/isolate-off
  SceneLayerAdd = 144,  // scene/layer-add
  SceneLayers = 145,  // scene/layers
  SceneLock = 146,  // scene/lock
  SceneObject = 147,  // scene/object
  SceneObjectAdd = 148,  // scene/object-add
  SceneObjectPreview = 149,  // scene/object-preview
  SceneObjectRemove = 150,  // scene/object-remove
  ScenePin = 151,  // scene/pin
  SceneUnlock = 152,  // scene/unlock
  SceneVisibility = 153,  // scene/visibility
  SceneVisibilityOff = 154,  // scene/visibility-off
  ScriptingCode = 155,  // scripting/code
  ScriptingNodes = 156,  // scripting/nodes
  UiAdd = 157,  // ui/add
  UiArrowLeft = 158,  // ui/arrow-left
  UiArrowRight = 159,  // ui/arrow-right
  UiCheck = 160,  // ui/check
  UiChevronDown = 161,  // ui/chevron-down
  UiChevronLeft = 162,  // ui/chevron-left
  UiChevronRight = 163,  // ui/chevron-right
  UiChevronUp = 164,  // ui/chevron-up
  UiClose = 165,  // ui/close
  UiDiagnostics = 166,  // ui/diagnostics
  UiHelp = 167,  // ui/help
  UiHome = 168,  // ui/home
  UiInfo = 169,  // ui/info
  UiMenu = 170,  // ui/menu
  UiMoreHorizontal = 171,  // ui/more-horizontal
  UiMoreVertical = 172,  // ui/more-vertical
  UiPanelLeft = 173,  // ui/panel-left
  UiPanelRight = 174,  // ui/panel-right
  UiRemove = 175,  // ui/remove
  UiSettings = 176,  // ui/settings
  UiShare = 177,  // ui/share
  UiSliders = 178,  // ui/sliders
  UiWarning = 179,  // ui/warning
  VfxParticles = 180,  // vfx/particles
  ViewCollapse = 181,  // view/collapse
  ViewCorners = 182,  // view/corners
  ViewExpand = 183,  // view/expand
  ViewGrid = 184,  // view/grid
  WaterAuthorFlow = 185,  // water/author-flow
  WaterAuthorLayers = 186,  // water/author-layers
  WaterAuthorPhysics = 187,  // water/author-physics
  WaterAuthorPoints = 188,  // water/author-points
  WaterAuthorRoute = 189,  // water/author-route
  WaterAuthorSurface = 190,  // water/author-surface
  BrandWordmark = 191,  // brand/wordmark
  BrandMark = 192,  // brand/mark
};

inline constexpr u32 kUiIconCount = 192;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
