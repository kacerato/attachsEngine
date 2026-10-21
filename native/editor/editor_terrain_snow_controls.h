#pragma once
#include "editor/editor_screen.h"
#include "editor/editor_terrain_modules_controls.h"
#include <cmath>
namespace ae::editor {
inline constexpr u32 terrainSnowActionBase=0xEC001000u,terrainSnowNumberBase=0xEC001100u,terrainSnowSliderBase=0xEC001200u;
inline EditorWidget terrainSnowAction(u32 action){return EditorWidget(terrainSnowActionBase+action);}
inline bool terrainSnowView(u32 view){return (view>=200 && view<=227) || view==245 || view==246;}
inline bool terrainSnowMapView(u32 view){return view>=210 && view<=217;}
inline bool terrainSnowPicker(u32 view){return view>=220 && view<=227;}
inline bool terrainSnowField(u32 field){return (field>=256 && field<=300) || (field>=304 && field<=318);}
inline u32 terrainSnowMapRole(u32 view){return view>=220?view-220:view-210;}
template<class Snow> auto &terrainSnowMap(Snow &snow,u32 role){
  switch(role){case 0:return snow.diffuse;case 1:return snow.height;case 2:return snow.normal;case 3:return snow.smoothness;
    case 4:return snow.ao;case 5:return snow.normalNoiseMap;case 6:return snow.mask;default:return snow.sparkleNoiseMap;}
}
inline const char *terrainSnowMapName(u32 role){const char *names[]{"Cor","Altura","Normal","Suavidade","Oclusão","Ruído normal","Máscara","Ruído de brilho"};return names[std::min(role,7u)];}
template<class Surface> auto *terrainSnowNumber(Surface &surface,u32 layer,u32 view,u32 field){
  auto &snow=surface.snow;using Pointer=decltype(&snow.amount);
  if(field==256)return &snow.amount;
  if(field>=257 && field<=259)return &snow.tint[field-257];
  if(field>=260 && field<=262)return &snow.upVector[field-260];
  if(field>=263 && field<=264)return &snow.uvScales[field-263];
  if(field>=265 && field<=268)return &snow.heightAngleRange[field-265];
  if(field>=269 && field<=272)return &snow.parameters[field-269];
  if(field>=273 && field<=274)return &snow.normalNoiseParams[field-273];
  if(field>=275 && field<=278)return &snow.distanceResample[field-275];
  if(field==279)return &snow.stochasticScale;if(field==280)return &snow.stochasticContrast;
  if(field==281 && layer<surface.layers.size())return &surface.layers[layer].snowParticipation;
  if(field==282 && layer<surface.layers.size())return &surface.layers[layer].snowPorosity;
  if(field>=283 && field<=286)return &snow.sssTint[field-283];
  if(field>=287 && field<=289)return &snow.rimColor[field-287];if(field==290)return &snow.rimPower;
  if(field>=291 && field<=293)return &snow.sparkleTint[field-291];
  switch(field){case 294:return &snow.sparkleEmission;case 295:return &snow.sparkleStrength;case 296:return &snow.sparkleSize;
    case 297:return &snow.sparkleDensity;case 298:return &snow.sparkleNoiseDensity;case 299:return &snow.sparkleNoiseAmplitude;case 300:return &snow.sparkleViewDependency;}
  if(field>=304 && field<=318 && terrainSnowMapView(view)){
    auto &map=terrainSnowMap(snow,terrainSnowMapRole(view));
    if(field==304)return &map.factor;if(field==305)return &map.remapMin;if(field==306)return &map.remapMax;
    if(field<=310)return &map.colorFactor[field-307];if(field<=314)return &map.colorRemapMin[field-311];return &map.colorRemapMax[field-315];
  }
  return Pointer(nullptr);
}
template<class State> bool terrainSnowNumberAvailable(const State &s,u32 field){
  if(s.terrainTab!=2 || !terrainSnowView(s.terrainMaterialView) || !terrainSnowField(field))return false;
  const auto view=s.terrainMaterialView;
  if(view==200)return field==256 || (field>=257 && field<=259);
  if(view==205)return (field>=263 && field<=264) || field==279 || field==280;
  if(view==206)return field>=260 && field<=268 && field!=263 && field!=264;
  if(view==202)return field>=269 && field<=272;
  if(view==207)return field==273 || field==274;
  if(view==208)return field>=275 && field<=278;
  if(view==204)return s.terrainBrush.layer<s.terrainDraft.surface.layers.size() &&
    (field==282 || (field==281 && s.terrainDraft.surface.snow.mode==resources::TerrainSnowMode::Rich && s.terrainDraft.surface.snow.perLayerEnabled));
  if(view==245)return field>=283 && field<=286;
  if(view==246)return field>=287 && field<=290;
  if(view==209)return field>=291 && field<=300;
  if(!terrainSnowMapView(view) || field<304)return false;
  const auto role=terrainSnowMapRole(view);const auto &map=terrainSnowMap(s.terrainDraft.surface.snow,role);
  const bool scalar=role==1 || role==3 || role==4,normal=role==2 || role==5;
  if(field==304)return true;
  if(field==305 || field==306)return scalar;
  if(scalar)return false;
  const auto channel=(field-307)%4;
  if(channel==3)return false;
  if(role==6 && channel>=2)return false;
  if(normal && channel==2 && map.channel!=resources::TerrainSurfaceChannel::NormalRGB)return false;
  return true;
}
template<class State> bool terrainSnowFlatDraft(const State &s){const auto &r=s.terrainDraft;return !s.terrainTarget && r.mode==resources::TerrainMode::Heightfield && r.height==0 && !r.island && r.strokes.empty() && r.paths.empty();}
template<class State> bool terrainSnowMeasuredPreview(const State &s){
  if(!s.terrainPreviewBuild || s.terrainPreviewBuild->mesh.parts.empty())return false;
  auto before=s.terrainPreviewBuild->recipe,now=s.terrainDraft;
  before.surface=now.surface;before.textureScale=now.textureScale;before.macroVariation=now.macroVariation;before.blendedSurface=now.blendedSurface;
  return resources::serializeTerrainRecipe(before)==resources::serializeTerrainRecipe(now);
}
template<class Surface> auto *terrainCurrentAuthoredMap(Surface &surface,u32 layer,u32 view,u32 role){
  using Pointer=decltype(&surface.snow.diffuse);
  if(terrainFlowMapView(view))return &terrainFlowMap(surface,view);
  if(terrainSnowMapView(view))return &terrainSnowMap(surface.snow,terrainSnowMapRole(view));
  if(layer>=surface.layers.size())return Pointer(nullptr);
  if(view==3)return &surface.layers[layer].maps[std::min(role,6u)];
  if(terrainModuleMapView(view))return &terrainModuleMap(surface,view,role);
  if(view>=100 && view<=102){auto &variants=surface.layers[layer].cluster.variants;const auto index=terrainClusterVariant(view);if(index<variants.size())return &variants[index].maps[std::min(role,6u)];}
  if(terrainNoiseAvailable(surface,layer,view) && !terrainNoisePicker(view))return &terrainSelectedNoise(surface,layer,view).map;
  return Pointer(nullptr);
}
template<class State> auto *terrainCurrentAuthoredMap(State &s){return terrainCurrentAuthoredMap(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView,s.terrainMapRole);}
inline std::string terrainResidencyName(resources::TerrainTextureResidency policy){
  using Mode=resources::TerrainTextureResidencyMode;
  return policy.mode==Mode::InheritTerrain?"Herdar terreno":policy.mode==Mode::Original?"Original":std::to_string(policy.maxSide)+" px";
}
inline resources::TerrainTextureResidency terrainResidencyNextCap(resources::TerrainTextureResidency current){
  constexpr u32 caps[]{4096,2048,1024,512,256};u32 next=0;
  if(current.mode==resources::TerrainTextureResidencyMode::Cap)for(u32 i=0;i<5;++i)if(caps[i]==current.maxSide)next=(i+1)%5;
  return {resources::TerrainTextureResidencyMode::Cap,caps[next]};
}
}
