#pragma once
#include "resources/terrain_surface_asset.h"
#include <algorithm>
namespace ae::editor {
inline bool terrainFlowRain(u32 view){return view==350 || view==362 || view==382;}
inline bool terrainFlowMapView(u32 view){return view==360 || view==361 || view==362;}
inline bool terrainFlowPicker(u32 view){return view==380 || view==381 || view==382;}
inline bool terrainFlowLava(u32 view){return (view>=340 && view<=345) || view==361 || view==381;}
inline bool terrainFlowModuleView(u32 view){return (view>=330 && view<=334) || (view>=340 && view<=345) || view==350 || view==351 || terrainFlowMapView(view) || terrainFlowPicker(view);}
inline bool terrainFlowField(u32 field){return (field>=368 && field<=385) || (field>=390 && field<=432);}
template<class Surface>auto &terrainFlowMap(Surface &surface,u32 view){return terrainFlowRain(view)?surface.flow.rainDropTexture:terrainFlowLava(view)?surface.flow.lavaDiffuse:surface.flow.streamNormal;}
template<class State>bool terrainFlowNumberAvailable(const State &s,u32 field){
  const auto view=s.terrainMaterialView;
  if(view==350)return field==429 || field==430;
  if(view==351)return field==431 || field==432;
  if(view==330)return field==368 || field==369 || (field>=378 && field<=380);
  if(view==331)return field>=370 && field<=373;
  if(view==332)return field==374 || field==375 || (field==385 && s.terrainDraft.surface.flow.perTexFoam && s.terrainBrush.layer<s.terrainDraft.surface.layers.size());
  if(view==333)return field>=381 && field<=384;
  if(view==334)return field==376 || field==377;
  if(view==340)return field==390 || field==391 || field==398;
  if(view==341)return field==392 || field==393;
  if(view==342)return field>=394 && field<=397;
  if(view==343)return field>=403 && field<=411;
  if(view==344)return field>=412 && field<=415;
  if(view==345)return field==399 || field==400 || ((field==401 || field==402) && s.terrainDraft.surface.flow.lavaStochastic);
  if(terrainFlowMapView(view)){
    if(field==416)return true;
    if(field>=417 && field<=428)return terrainFlowLava(view) || terrainFlowRain(view) || (field-417)%4!=3;
  }
  return false;
}
template<class Surface>auto terrainFlowNumber(Surface &surface,u32 layer,u32 view,u32 field)->decltype(&surface.flow.streamMaximum){
  auto &flow=surface.flow;
  if(field==429 || field==430)return &flow.rainIntensityScale[field-429];
  if(field==431 || field==432)return &flow.specularFades[field-431];
  if(field==368)return &flow.streamMaximum;if(field==369)return &flow.streamBlend;
  if(field>=370 && field<=373)return &flow.streamFlowParams[field-370];
  if(field==374 || field==375)return &flow.streamNormalFoam[field-374];
  if(field==376 || field==377)return &flow.streamUvScales[field-376];
  if(field>=378 && field<=380)return &flow.streamTint[field-378];
  if(field>=381 && field<=384)return &flow.streamFades[field-381];
  if(field==385 && layer<surface.layers.size())return &surface.layers[layer].flow.foam;
  if(field>=390 && field<=393)return &flow.lavaParams[field-390];
  if(field>=394 && field<=397)return &flow.lavaParams2[field-394];
  if(field==398)return &flow.lavaEmissiveMultiplier;
  if(field==399 || field==400)return &flow.lavaUvScale[field-399];
  if(field==401)return &flow.lavaStochasticContrast;if(field==402)return &flow.lavaStochasticSize;
  if(field>=403 && field<=405)return &flow.lavaEdgeColor[field-403];
  if(field>=406 && field<=408)return &flow.lavaColorLow[field-406];
  if(field>=409 && field<=411)return &flow.lavaColorHighlight[field-409];
  if(field>=412 && field<=415)return &flow.lavaFades[field-412];
  auto &map=terrainFlowMap(surface,view);if(field==416)return &map.factor;
  if(field>=417 && field<=420)return &map.colorFactor[field-417];
  if(field>=421 && field<=424)return &map.colorRemapMin[field-421];
  if(field>=425 && field<=428)return &map.colorRemapMax[field-425];
  return nullptr;
}
// Exact source-SHA catalog IDs from flow-assets-integration; engine GUID is
// always read from the matched entry, never derived from a list position/name.
template<class Catalog>auto terrainFlowPreset(const Catalog &catalog,bool lava,u32 alternative)->decltype(&catalog.textures.front()){
  constexpr const char *ids[]{"36c7dafbeb8063adabd0e6dbe33050473ed26f2d717659fe2e17d569c9024f25",
    "1870d43c24fb49bb7550c22f496a23ed81ed60b63c4c0936a7d8d69dc486289c",
    "12768441ddd7ce7e1fc71abb0a1c00246f8945a514f6563a1fd16b095ca503b7"};
  const auto id=ids[lava?1+std::min(alternative,1u):0];
  for(const auto &texture:catalog.textures)if(texture.id==id)return &texture;
  return nullptr;
}

template<class Catalog>auto terrainRainPreset(const Catalog &catalog)->decltype(&catalog.textures.front()){
  for(const auto &texture:catalog.textures)if(texture.id=="6bfc4b0da37610c32e499bd387ccffd2724890a6db15cdabcf8344b5963ba17d")return &texture;
  return nullptr;
}

}
