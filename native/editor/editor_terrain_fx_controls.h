#pragma once
#include "editor/editor_widgets.h"
#include "ui/ui_theme.h"
#include <memory>
#include "resources/terrain_fx_asset.h"
#include "editor/editor_terrain_streams_lava_controls.h"
namespace ae::editor {
struct TerrainFxEditorBitmap;
struct TerrainFxUiState {
  // Session preference, not terrain authoring: passed to every core allocation.
  u32 budgetMiB=96,resolution=256,mode=0,page=0;
  bool erase=false,cursorVisible=false,recipePending=true;
  u64 previewFullEncodes=0,previewSnapshotCopies=0,previewJobsQueued=0;
  float cursorU=0,cursorV=0;
  resources::TerrainFxStroke brush;
  std::shared_ptr<const TerrainFxEditorBitmap> bitmap;
  mutable ui::UiRect canvas;
};
constexpr u32 terrainFxActionBase=0xEC002000;
inline EditorWidget terrainFxAction(u32 action){return EditorWidget(terrainFxActionBase+action);}
inline bool terrainStaticFlowView(u32 view){return (view>=310 && view<=313) || view==320;}
inline bool terrainFxView(u32 view){return (view>=300 && view<=303) || view==306 || view==307 || terrainStaticFlowView(view) || terrainFlowModuleView(view);}
inline bool terrainStaticFlowField(u32 field){return field>=336 && field<=349;}
template<class State>bool terrainStaticFlowNumberAvailable(const State &s,u32 field){
  const auto view=s.terrainMaterialView;
  if(view==310)return field==336 || field==337;
  if(view==311)return field>=338 && field<=341;
  if(view==312)return field>=342 && field<=345;
  if(view==320)return field==346 || field==347;
  if(view==313)return field==348?!s.terrainDraft.surface.flow.perTexPorosity:
    field==349 && s.terrainDraft.surface.flow.perTexPorosity && s.terrainBrush.layer<s.terrainDraft.surface.layers.size();
  return false;
}
template<class Surface>auto terrainStaticFlowNumber(Surface &surface,u32 layer,u32 field)->decltype(&surface.flow.globalPorosity){
  auto &flow=surface.flow;
  if(field==336 || field==337)return &flow.wetnessParams[field-336];
  if(field>=338 && field<=341)return &flow.heightWetnessParams[field-338];
  if(field==342)return &flow.distanceWetnessParams[0];
  if(field==344)return &flow.distanceWetnessParams[1];
  if(field==345)return &flow.distanceWetnessParams[3];
  if(field==346 || field==347)return &flow.puddleParams[field-346];
  if(field==348)return &flow.globalPorosity;
  if(field==349 && layer<surface.layers.size())return &surface.layers[layer].snowPorosity;
  return nullptr;
}
inline bool terrainFxField(u32 field){return field>=320 && field<=329;}
template<class State>bool terrainFxNumberAvailable(const State &s,u32 field){
  if(s.terrainMaterialView==300)return (field==320 && !s.terrainFx.erase) || field==321;
  if(s.terrainMaterialView==301)return (field>=322 && field<=325) || field==328 || field==329;
  if(s.terrainMaterialView==302)return field==326 || field==327;
  return false;
}
inline resources::TerrainFxLimits terrainFxLimits(const TerrainFxUiState &state){
  resources::TerrainFxLimits limits;limits.maximumWorkingBytes=u64(state.budgetMiB)<<20;return limits;
}
inline double *terrainFxNumber(TerrainFxUiState &state,u32 field){
  switch(field){case 320:return &state.brush.target;case 321:return &state.brush.radiusMeters;
  case 322:return &state.brush.flow;case 323:return &state.brush.falloff;
  case 324:return &state.brush.hardness;case 325:return &state.brush.opacity;
  case 326:return &state.brush.slopeMin;case 327:return &state.brush.slopeMax;
  case 328:return &state.brush.aspect;case 329:return &state.brush.rotationDegrees;default:return nullptr;}
}
}
