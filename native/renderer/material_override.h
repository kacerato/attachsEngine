#pragma once
#include "renderer/map_package.h"
namespace ae::renderer {
// Scalar instance overrides preserve the shared textures and pipeline class.
struct MaterialOverride {
  bool enabled = false;
  float baseColor[3]{1,1,1};
  float roughness = 0.5f;
  float metallic = 0;
  float normalScale = 1;
  float specular = 1;
  float emission[3]{};
  float emissionStrength = 1;
};
inline MaterialOverride materialOverrideFrom(const MapMaterialRecord &source) {
  MaterialOverride result;
  for(u32 i=0;i<3;++i) {result.baseColor[i]=source.baseColorFactor[i];result.emission[i]=source.emissiveFactorAndStrength[i];}
  result.roughness=source.roughness;result.metallic=source.metallic;
  result.normalScale=source.normalScale;result.specular=source.specular;
  result.emissionStrength=source.emissiveFactorAndStrength[3];return result;
}
inline MapMaterialRecord applyMaterialOverride(const MapMaterialRecord &source,const MaterialOverride &value) {
  auto result=source;if(!value.enabled) return result;
  for(u32 i=0;i<3;++i) {result.baseColorFactor[i]=value.baseColor[i];result.emissiveFactorAndStrength[i]=value.emission[i];}
  result.roughness=value.roughness;result.metallic=value.metallic;
  result.normalScale=value.normalScale;result.specular=value.specular;
  result.emissiveFactorAndStrength[3]=value.emissionStrength;return result;
}
}
