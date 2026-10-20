#pragma once
#include "editor/editor_terrain_noise_controls.h"
#include <algorithm>
#include <array>
#include "resources/animation_curve_domain.h"

namespace ae::editor {
inline constexpr u32 terrainModuleActionBase=0xEC000E00u;
inline EditorWidget terrainModuleAction(u32 action) {return static_cast<EditorWidget>(terrainModuleActionBase+action);}
inline bool terrainModuleView(u32 view) {
  return view==120 || view==121 || view==123 || view==124 || view==130 || view==131 || view==132 || view==140 || view==141 || view==150 || view==151 ||
    view==159 || (view>=160 && view<=163) || (view>=170 && view<=173) || (view>=180 && view<=183);
}
inline bool terrainModuleMapView(u32 view) {return view==140 || view==141 || (view>=170 && view<=173);}
inline bool terrainModulePicker(u32 view) {return view==150 || view==151 || (view>=180 && view<=183);}
inline u32 terrainModuleGlobal(u32 view) {return view>=180?view-180:view>=170?view-170:view-160;}
inline bool terrainModuleNormal(u32 view) {return view==141 || view==151 || view==161 || view==171 || view==181;}
template<class Curve>
std::array<float,2> terrainGeoCurveDomain(const Curve &curve) {
  if(curve.keys.empty()) return {0,1};
  return {std::min(0.f,curve.keys.front().time),std::max(1.f,curve.keys.back().time)};
}
inline bool terrainModuleField(u32 field) {return (field>=191 && field<=236) || field==255;}
template<class Surface>
auto &terrainModuleMap(Surface &surface,u32 view,u32 role) {
  if(view==140 || view==150) return surface.geo.albedo;
  if(view==141 || view==151) return surface.geo.normal;
  auto &global=surface.globalMaps[terrainModuleGlobal(view)];
  if(terrainModuleGlobal(view)!=2) return global.map;
  return role%4==0?global.smoothness:role%4==1?global.ao:role%4==2?global.metallic:global.mask;
}
template<class State>
bool terrainModuleNumberAvailable(const State &s,u32 field) {
  const auto view=s.terrainMaterialView;
  if(!terrainModuleView(view) || s.terrainBrush.layer>=s.terrainDraft.surface.layers.size()) return false;
  if(field==255) return (view==130 || view==131) && !s.terrainDraft.surface.geo.curve.keys.empty();
  if(field>=199 && field<=201) return view==132;
  if(field>=206 && field<=211) {
    const auto &curve=s.terrainDraft.surface.geo.curve;
    if(curve.keys.empty()) return false;
    const auto selected=std::min<usize>(s.terrainProfilePoint,curve.keys.size()-1);
    const auto &key=curve.keys[selected];
    if(field==206) return view==130;
    if(field==207) return view==130;
    if(view!=131) return false;
    if(field==208) return key.incoming==resources::AnimationTangentMode::Free;
    if(field==209) return key.outgoing==resources::AnimationTangentMode::Free;
    return field==210?key.weightedIn:key.weightedOut;
  }
  if(field>=195 && field<=198) return view==124 && s.terrainDraft.surface.geo.rangeEnabled;
  if(field>=203 && field<=205) return view==123 && (field==203 || s.terrainDraft.surface.layers[s.terrainBrush.layer].geoHeightFilterEnabled);
  if(field>=212 && field<=221) return view>=160 && view<=163;
  if(field>=222 && field<=236) {
    if(!terrainModuleMapView(view)) return false;
    if(view==172) return field<=224;
    if(field==223 || field==224) return false;
    const bool normal=terrainModuleNormal(view);
    const bool alpha=field==228 || field==232 || field==236;
    if((normal || view==173) && alpha) return false;
    if(normal && terrainModuleMap(s.terrainDraft.surface,view,s.terrainMapRole).channel==resources::TerrainSurfaceChannel::NormalRG &&
      (field==227 || field==231 || field==235)) return false;
    return true;
  }
  return field==191 || field==192 || field==193 || field==194 || field==202 ? view==120:false;
}
template<class Surface>
auto *terrainModuleNumber(Surface &surface,u32 layer,u32 view,u32 role,u32 keyIndex,u32 field) {
  auto &geo=surface.geo;
  if(field==191) return &geo.blendStrength;
  if(field==192) return &geo.normalStrength;
  if(field==193) return &geo.scaleMeters;
  if(field==194) return &geo.offsetMeters;
  if(field>=195 && field<=198) return &geo.range[field-195];
  if(field==199) return &geo.curveScaleMeters;
  if(field==200) return &geo.curveOffsetMeters;
  if(field==201) return &geo.curveRotationDegrees;
  if(field==202) return &geo.heightContrast;
  if(field==203) return &surface.layers[layer].geoParticipation;
  if(field==204) return &surface.layers[layer].geoHeightThreshold;
  if(field==205) return &surface.layers[layer].geoHeightStrength;
  if(field>=206 && field<=211) {
    if(geo.curve.keys.empty()) return decltype(&geo.scaleMeters)(nullptr);
    auto &key=geo.curve.keys[std::min<usize>(keyIndex,geo.curve.keys.size()-1)];
    if(field==206) return &key.time;
    if(field==207) return &key.value;
    if(field==208) return &key.inSlope;
    if(field==209) return &key.outSlope;
    if(field==210) return &key.inWeight;
    return &key.outWeight;
  }
  if(field>=212 && field<=221) {
    auto &global=surface.globalMaps[terrainModuleGlobal(view)];
    if(field==212) return &global.intensity;
    if(field==213) return &global.fadeStart;
    if(field==214) return &global.fadeEnd;
    if(field==215) return &global.nearStrength;
    if(field==216) return &global.farStrength;
    if(field==217) return &global.scaleX;
    if(field==218) return &global.scaleY;
    if(field==219) return &global.offsetX;
    if(field==220) return &global.offsetY;
    return &surface.layers[layer].globalMapParticipation[terrainModuleGlobal(view)];
  }
  auto &map=terrainModuleMap(surface,view,role);
  if(field==222) return &map.factor;
  if(field==223) return &map.remapMin;
  if(field==224) return &map.remapMax;
  if(field<=228) return &map.colorFactor[field-225];
  if(field<=232) return &map.colorRemapMin[field-229];
  return &map.colorRemapMax[field-233];
}
} // namespace ae::editor
