#pragma once
#include "resources/asset_registry.h"
#include "scene/material_parameters.h"
#include <cmath>

namespace ae::scene {
// Per-instance indirect diffuse irradiance, in linear RGB (not albedo or emission).
// Unity 6000.0 Renderer.lightmapScaleOffset; Godot 4.5 LightmapGI UV2 contract.
// UV1 must be authored without overlapping charts. This is not a bake service.
struct LightmapBinding {
  resources::AssetGuid texture{};
  bool enabled=false;
  float scale[2]{1,1};
  float offset[2]{};
  float intensity=1;
  bool valid() const {
    if(!std::isfinite(intensity) || intensity<0 || intensity>10000) return false;
    for(unsigned i=0;i<2;++i)
      if(!std::isfinite(scale[i]) || !std::isfinite(offset[i]) || scale[i]<=0 || scale[i]>1 ||
         offset[i]<0 || offset[i]>1 || scale[i]+offset[i]>1.00001f) return false;
    return true;
  }
  friend bool operator==(const LightmapBinding &a,const LightmapBinding &b) {
    return a.texture==b.texture && a.enabled==b.enabled && a.intensity==b.intensity &&
      a.scale[0]==b.scale[0] && a.scale[1]==b.scale[1] && a.offset[0]==b.offset[0] && a.offset[1]==b.offset[1];
  }
};
inline MaterialSampling lightmapSampling() {
  MaterialSampling value;
  value.uvSet=MaterialUv1;value.wrap=MaterialWrapClamp;value.filter=MaterialFilterLinear;
  return value;
}
}
