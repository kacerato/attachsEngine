// Independent authoring policy. Game settings remain the export authority.
// Reference: Unity 6000.0 Manual/ViewModes.html and Manual/GameView.html.
#pragma once
#include "renderer/rendering_policy.h"
namespace ae::renderer {
enum class RenderViewContext : u32 { Authoring, PlayPreview, Player };
enum class EditorViewportQuality : u32 { Low, Medium, High };
struct EditorViewportSettings final {
  EditorViewportQuality quality=EditorViewportQuality::Medium;
  bool animatedMaterials=false;
};
const char *editorViewportQualityName(EditorViewportQuality quality);
EditorViewportQuality parseEditorViewportQuality(const char *name);
ProjectRenderingSettings settingsForRenderView(RenderViewContext context,
    const ProjectRenderingSettings &game,const EditorViewportSettings &viewport);
} // namespace ae::renderer
