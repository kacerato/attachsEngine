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
  EditorEyedropper = 66,  // editor/eyedropper
  EditorFocus = 67,  // editor/focus
  EditorFrameObject = 68,  // editor/frame-object
  EditorGizmoAxes = 69,  // editor/gizmo-axes
  EditorMove = 70,  // editor/move
  EditorOrbit = 71,  // editor/orbit
  EditorPan = 72,  // editor/pan
  EditorPivot = 73,  // editor/pivot
  EditorRedo = 74,  // editor/redo
  EditorResize = 75,  // editor/resize
  EditorRotate = 76,  // editor/rotate
  EditorScale = 77,  // editor/scale
  EditorSelect = 78,  // editor/select
  EditorSelectActive = 79,  // editor/select-active
  EditorSelectBox = 80,  // editor/select-box
  EditorSelectionBounds = 81,  // editor/selection-bounds
  EditorSnap = 82,  // editor/snap
  EditorSpaceWorld = 83,  // editor/space-world
  EditorUndo = 84,  // editor/undo
  IdeAdd = 85,  // ide/add
  IdeCode = 86,  // ide/code
  IdeCompile = 87,  // ide/compile
  IdeConsole = 88,  // ide/console
  IdeCopy = 89,  // ide/copy
  IdeError = 90,  // ide/error
  IdeFiles = 91,  // ide/files
  IdePlay = 92,  // ide/play
  IdeSave = 93,  // ide/save
  IdeSearch = 94,  // ide/search
  IdeUndo = 95,  // ide/undo
  IdeWarning = 96,  // ide/warning
  InputAction = 97,  // input/action
  InputBinding = 98,  // input/binding
  LightDirectional = 99,  // light/directional
  LightPoint = 100,  // light/point
  LightSpot = 101,  // light/spot
  LightingCloud = 102,  // lighting/cloud
  LightingCloudSun = 103,  // lighting/cloud-sun
  LightingExposure = 104,  // lighting/exposure
  LightingFog = 105,  // lighting/fog
  LightingPostProcessing = 106,  // lighting/post-processing
  LightingSceneEffects = 107,  // lighting/scene-effects
  LightingSceneLighting = 108,  // lighting/scene-lighting
  LightingSky = 109,  // lighting/sky
  LightingSun = 110,  // lighting/sun
  NatureArchitecture = 111,  // nature/architecture
  NatureLandscape = 112,  // nature/landscape
  NatureLeaf = 113,  // nature/leaf
  NatureMountains = 114,  // nature/mountains
  NatureRock = 115,  // nature/rock
  NatureTree = 116,  // nature/tree
  NatureWater = 117,  // nature/water
  PhysicsCharacter = 118,  // physics/character
  PhysicsDynamicBox = 119,  // physics/dynamic-box
  PhysicsDynamicCapsule = 120,  // physics/dynamic-capsule
  PhysicsDynamicSphere = 121,  // physics/dynamic-sphere
  PhysicsKinematicBox = 122,  // physics/kinematic-box
  PhysicsKinematicSphere = 123,  // physics/kinematic-sphere
  PhysicsSensorBox = 124,  // physics/sensor-box
  PhysicsSensorCapsule = 125,  // physics/sensor-capsule
  PhysicsSensorSphere = 126,  // physics/sensor-sphere
  PhysicsStaticBox = 127,  // physics/static-box
  PhysicsStaticCapsule = 128,  // physics/static-capsule
  PhysicsStaticSphere = 129,  // physics/static-sphere
  PrimitiveCapsule = 130,  // primitive/capsule
  PrimitiveCone = 131,  // primitive/cone
  PrimitiveCube = 132,  // primitive/cube
  PrimitiveCylinder = 133,  // primitive/cylinder
  PrimitiveSphere = 134,  // primitive/sphere
  RuntimeAudio = 135,  // runtime/audio
  RuntimeCamera = 136,  // runtime/camera
  RuntimeCameraOrbit = 137,  // runtime/camera-orbit
  RuntimePause = 138,  // runtime/pause
  RuntimePlay = 139,  // runtime/play
  RuntimeRestart = 140,  // runtime/restart
  RuntimeStop = 141,  // runtime/stop
  SceneDuplicate = 142,  // scene/duplicate
  SceneFlatten = 143,  // scene/flatten
  SceneIsolateOff = 144,  // scene/isolate-off
  SceneLayerAdd = 145,  // scene/layer-add
  SceneLayers = 146,  // scene/layers
  SceneLock = 147,  // scene/lock
  SceneObject = 148,  // scene/object
  SceneObjectAdd = 149,  // scene/object-add
  SceneObjectPreview = 150,  // scene/object-preview
  SceneObjectRemove = 151,  // scene/object-remove
  ScenePin = 152,  // scene/pin
  SceneUnlock = 153,  // scene/unlock
  SceneVisibility = 154,  // scene/visibility
  SceneVisibilityOff = 155,  // scene/visibility-off
  ScriptingCode = 156,  // scripting/code
  ScriptingNodes = 157,  // scripting/nodes
  UiAdd = 158,  // ui/add
  UiArrowLeft = 159,  // ui/arrow-left
  UiArrowRight = 160,  // ui/arrow-right
  UiCheck = 161,  // ui/check
  UiChevronDown = 162,  // ui/chevron-down
  UiChevronLeft = 163,  // ui/chevron-left
  UiChevronRight = 164,  // ui/chevron-right
  UiChevronUp = 165,  // ui/chevron-up
  UiClose = 166,  // ui/close
  UiDiagnostics = 167,  // ui/diagnostics
  UiHelp = 168,  // ui/help
  UiHome = 169,  // ui/home
  UiInfo = 170,  // ui/info
  UiMenu = 171,  // ui/menu
  UiMoreHorizontal = 172,  // ui/more-horizontal
  UiMoreVertical = 173,  // ui/more-vertical
  UiPanelLeft = 174,  // ui/panel-left
  UiPanelRight = 175,  // ui/panel-right
  UiRemove = 176,  // ui/remove
  UiSettings = 177,  // ui/settings
  UiShare = 178,  // ui/share
  UiSliders = 179,  // ui/sliders
  UiWarning = 180,  // ui/warning
  VfxParticles = 181,  // vfx/particles
  ViewCollapse = 182,  // view/collapse
  ViewCorners = 183,  // view/corners
  ViewExpand = 184,  // view/expand
  ViewGrid = 185,  // view/grid
  WaterAuthorFlow = 186,  // water/author-flow
  WaterAuthorLayers = 187,  // water/author-layers
  WaterAuthorPhysics = 188,  // water/author-physics
  WaterAuthorPoints = 189,  // water/author-points
  WaterAuthorRoute = 190,  // water/author-route
  WaterAuthorSurface = 191,  // water/author-surface
  BrandWordmark = 192,  // brand/wordmark
  BrandMark = 193,  // brand/mark
};

inline constexpr u32 kUiIconCount = 193;

// Nome de catálogo do ícone, para log e diagnóstico. Nunca para busca: procurar
// um ícone por string em tempo de execução desfaria a garantia do enum.
const char *uiIconName(UiIcon icon) noexcept;

} // namespace ae::ui
