#include "renderer/view_rendering_settings.h"
#include <string_view>
namespace ae::renderer {
const char *editorViewportQualityName(EditorViewportQuality quality) {
  switch(quality) {case EditorViewportQuality::Low:return "low";
    case EditorViewportQuality::High:return "high";default:return "medium";}
}
EditorViewportQuality parseEditorViewportQuality(const char *name) {
  const std::string_view value=name?name:"";
  return value=="low"?EditorViewportQuality::Low:value=="high"?EditorViewportQuality::High:EditorViewportQuality::Medium;
}
ProjectRenderingSettings settingsForRenderView(RenderViewContext context,
    const ProjectRenderingSettings &game,const EditorViewportSettings &viewport) {
  if(context!=RenderViewContext::Authoring) {auto result=game;result.maximumRenderHz=50;return result;}
  ProjectRenderingSettings result;
  result.preset=viewport.quality==EditorViewportQuality::Low?QualityPreset::C:
      viewport.quality==EditorViewportQuality::High?QualityPreset::A:QualityPreset::B;
  result.resolutionScale=viewport.quality==EditorViewportQuality::Low?.5f:
      viewport.quality==EditorViewportQuality::High?1.f:.75f;
  result.maximumRenderHz=30;
  result.dynamicResolution=FeatureOverride::Disabled;
  result.antiAliasing=viewport.quality==EditorViewportQuality::Low?AntiAliasingMode::Off:AntiAliasingMode::Fxaa;
  result.upscalingFilter=UpscalingFilter::Bilinear;
  // No temporal history in authoring; never alter source assets or residency mip.
  result.textures=TextureQuality::Full;
  result.textureStreaming=FeatureOverride::Disabled;
  result.post=PostQuality::Tonemap;
  return result;
}
} // namespace ae::renderer
