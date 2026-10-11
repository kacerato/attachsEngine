#pragma once
#include "editor/editor_view.h"
#include "runtime/rig_instance.h"
#include "ui/ui_draw_list.h"

namespace ae::editor {

struct EditorRigHandle {
  u64 control=0,joint=0;
  runtime::ObjectId object=0;
  ui::UiPoint screen;
  float depth=0;
};

// Buffers belong to the editor workspace and retain capacity across frames.
// Scene matrices are accumulated parent-before-child once, not by traversing
// the complete ancestor path separately for every control.
struct EditorRigOverlay {
  std::vector<std::array<float,16>> world;
  std::vector<u8> live;
  std::vector<EditorRigHandle> handles;
  ui::UiRect viewport;
  void clear(){world.clear();live.clear();handles.clear();viewport={};}
};

// Draws only the bound, live hierarchy. It does not create geometry, scene
// objects, skins or controls. Invalid/missing joints are omitted independently.
bool buildEditorRigOverlay(const resources::RigAsset &,const resources::CompiledRig &,
    const runtime::RigBindings &,const runtime::SceneGraph &,const EditorViewport &,
    u64 selectedControl,const ui::UiTheme &,ui::UiDrawList &,EditorRigOverlay &,
    const resources::RigTransform *rootPlacement=nullptr);

// Viewport fallback picking avoids consuming thousands of UiInputRouter
// regions. The Session handles a rig control before scene selection/orbit.
// The drawn mark is compact; its touch square is 48 logical pixels by default.
u64 pickEditorRigControl(const EditorRigOverlay &,ui::UiPoint,float touchSide=48);

} // namespace ae::editor
