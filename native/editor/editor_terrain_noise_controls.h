#pragma once
#include "editor/editor_widgets.h"

namespace ae::editor {
// Private M05–M08 routing. Existing M05 views stay stable; advanced contexts
// are encoded in views rather than shared editor state/public headers.
inline constexpr u32 terrainNoiseActionBase = 0xEC000C00u;
inline EditorWidget terrainNoiseAction(u32 offset) {
  return static_cast<EditorWidget>(terrainNoiseActionBase + offset);
}
inline bool terrainNoiseView(u32 view) {
  return (view>=4 && view<=17) || (view>=20 && view<=25) || (view>=31 && view<=33) || (view>=39 && view<=41) || view==51 || view==59 || view==62 || view==69;
}
inline bool terrainNoisePicker(u32 view) {
  return (view>=8 && view<=11) || (view>=20 && view<=25) || (view>=39 && view<=41) || view==59 || view==69;
}
inline u32 terrainNoiseSelection(u32 view) {
  if(view==62 || view==69) return 14;
  if(view==51 || view==59) return 13;
  if(view>=39) return view-39+10;
  if(view>=31) return view-31+10;
  if(view>=20) return view-20+4;
  if(view>=12) return view-12+4;
  return view>=8?view-8:view-4;
}
inline u32 terrainNoiseViewFor(u32 selection,bool picker=false) {
  if(selection==14) return picker?69:62;
  if(selection==13) return picker?59:51;
  if(selection>=10) return (picker?39:31)+selection-10;
  if(selection>=4) return (picker?20:12)+selection-4;
  return (picker?8:4)+selection;
}
inline bool terrainNoiseNormal(u32 view) {
  const auto selection=terrainNoiseSelection(view);
  return (selection>=4 && selection<=9) || selection==10;
}
inline bool terrainNoiseLocal(u32 view) {
  const auto selection=terrainNoiseSelection(view);
  return selection==2 || selection==3 || (selection>=7 && selection<=9);
}
template<class Surface>
auto *terrainBoundAntiTile(Surface &surface,u32 layer) {
  for(auto &entry:surface.antiTileArray.entries)
    if(entry.identity==surface.layers[layer].antiTileEntry) return &entry;
  using Entry=decltype(&surface.antiTileArray.entries[0]);
  return Entry(nullptr);
}
template<class Surface>
bool terrainNoiseAvailable(const Surface &surface,u32 layer,u32 view) {
  return layer<surface.layers.size() && terrainNoiseView(view) &&
    (terrainNoiseSelection(view)<10 || terrainNoiseSelection(view)>=13 || terrainBoundAntiTile(surface,layer));
}
template<class Surface>
auto &terrainSelectedNoise(Surface &surface,u32 layer,u32 view) {
  const auto selection=terrainNoiseSelection(view);
  if(selection<2) return selection==0?surface.detailNoise:surface.distanceNoise;
  if(selection<4) return selection==2?surface.layers[layer].detailNoise:surface.layers[layer].distanceNoise;
  if(selection<7) return surface.normalNoise[selection-4];
  if(selection<10) return surface.layers[layer].normalNoise[selection-7];
  if(selection==14) return surface.layers[layer].cluster.selection;
  if(selection==13) return surface.layers[layer].resampleNoise;
  auto &entry=*terrainBoundAntiTile(surface,layer);
  return selection==10?entry.normal:selection==11?entry.detail:entry.distance;
}
template<class Surface>
auto *terrainNoiseNumber(Surface &surface, u32 layer, u32 view, u32 field) {
  if(field==152) return &surface.layers[layer].resampleScale;
  if(field==153) return &surface.layers[layer].resampleStart;
  if(field==154) return &surface.layers[layer].resampleEnd;
  if(field==148) return &surface.layers[layer].globalNormalParticipation[(terrainNoiseSelection(view)-4)%3];
  if(field>=149 && field<=151) return &surface.layers[layer].antiTileParticipation[field-149];
  auto &noise=terrainSelectedNoise(surface,layer,view);
  if(field==128) return &noise.scale;
  if(field==129) return &noise.strength;
  if(field==130) return &noise.fadeStart;
  if(field==131) return &noise.fadeEnd;
  if(field==132) return terrainNoiseSelection(view)%2==0
    ? &surface.layers[layer].globalDetailParticipation : &surface.layers[layer].globalDistanceParticipation;
  if(field==133) return &noise.map.factor;
  if(field==134) return &noise.map.remapMin;
  if(field==135) return &noise.map.remapMax;
  if(field<=139) return &noise.map.colorFactor[field-136];
  if(field<=143) return &noise.map.colorRemapMin[field-140];
  return &noise.map.colorRemapMax[field-144];
}
inline constexpr u32 terrainClusterActionBase=0xEC000D00u;
inline EditorWidget terrainClusterAction(u32 action) {return static_cast<EditorWidget>(terrainClusterActionBase+action);}
inline u32 terrainClusterVariant(u32 view) {
  return view>=100?view-100:view>=90?view-90:view>=80?view-80:view>=70?view-70:0;
}
inline bool terrainClusterVariantView(u32 view) {
  return (view>=70 && view<=72) || (view>=80 && view<=82) || (view>=90 && view<=92) || (view>=100 && view<=102);
}
inline bool terrainClusterField(u32 field) {return (field>=160 && field<=165) || (field>=176 && field<=190);}
template<class Surface>
bool terrainClusterNumberAvailable(const Surface &surface,u32 layer,u32 view,u32 field) {
  if(layer>=surface.layers.size() || !terrainClusterField(field)) return false;
  if(field==164 || field==165) return view==60;
  return terrainClusterVariantView(view) && terrainClusterVariant(view)<surface.layers[layer].cluster.variants.size();
}
template<class Surface>
auto *terrainClusterNumber(Surface &surface,u32 layer,u32 view,u32 field,u32 role) {
  auto &cluster=surface.layers[layer].cluster;
  if(field==164) return &cluster.contrast;
  if(field==165) return &cluster.noiseBoost;
  auto &variant=cluster.variants[terrainClusterVariant(view)];
  if(field==160) return &variant.scale;
  if(field==161) return &variant.offsetX;
  if(field==162) return &variant.offsetY;
  if(field==163) return &variant.rotation;
  auto &map=variant.maps[role<7?role:6];
  if(field==176) return &map.factor;
  if(field==177) return &map.remapMin;
  if(field==178) return &map.remapMax;
  if(field<=182) return &map.colorFactor[field-179];
  if(field<=186) return &map.colorRemapMin[field-183];
  return &map.colorRemapMax[field-187];
}
} // namespace ae::editor
