#pragma once
#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"
#include "ui/ui_icon_id.h"

namespace ae::editor {
enum class RigToolRoute : u8 {Controls,Mirror,Poses,Retarget,Diagnostics};
// IDs belong to this tool, independently of EditorScreen's large enum. No
// identity of a rig joint or resource is squeezed into a 32-bit widget ID.
enum class RigToolAction : u32 {
  Close=0xCD200000u,Routes,PreviousPage,NextPage,Confirm,CancelPreview,
  RouteControls,RouteMirror,RoutePoses,RouteRetarget,RouteDiagnostics,
  CaptureRig,BindInstance,RevalidateInstance,PreviousControl,NextControl,ControlName,
  ControlChannels,ControlSpace,LimitKind,LimitAxis,LimitMinimum,LimitMaximum,
  LimitCone,ReferenceCapture,CreateControl,RemoveControl,
  MirrorLeft,MirrorRight,MirrorPair,MirrorPlane,MirrorOffset,MirrorChannels,
  MirrorReference,MirrorPreview,MirrorSuggest,
  PoseCapture,PoseSelect,PoseWeight,PoseCompatibility,PosePreview,
  RetargetSource,RetargetTarget,RetargetClip,RetargetProfile,RetargetMapSource,
  RetargetMapTarget,RetargetPosition,RetargetScale,RetargetCorrection,
  RetargetMotionScale,RetargetFacing,RetargetUnmapped,RetargetTime,
  RetargetPlay,RetargetCompare,RetargetPreview,RetargetBake,
  Open,PoseApply,PoseNext
};
struct RigToolField {
  RigToolAction action=RigToolAction::Routes;
  std::string name,value;
  bool enabled=false;
  ui::UiIcon icon=ui::UiIcon::None;
};
// Presentation snapshot. Session derives these fields from the typed resource,
// draft and consumer diagnostics. It resolves actions by enum, not field text.
// This structure owns no resource truth, publisher, scene or runtime instance.
struct EditorRigScreenState {
  RigToolRoute route=RigToolRoute::Controls;
  std::string title,status,confirmLabel;
  std::vector<RigToolField> fields;
  u32 page=0;
  bool routesOpen=false,canConfirm=false,hasPreview=false,error=false;
};
struct EditorRigScreenLayout {ui::UiRect viewport,context;};
EditorRigScreenLayout rigScreenLayout(ui::UiRect);
// Context occupies one side. The real scene/model and rig overlays keep the
// remaining viewport; no replacement mesh, mock image or dummy viewport.
void buildEditorRigScreen(const EditorRigScreenState &,const ui::UiTheme &,
                          ui::UiDrawList &,ui::UiInputRouter &,ui::UiRect context);
} // namespace ae::editor
