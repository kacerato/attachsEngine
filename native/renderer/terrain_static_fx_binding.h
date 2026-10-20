#pragma once
#include "renderer/terrain_surface_binding.h"
#include <cmath>
namespace ae::renderer {
// Deep pixel/sampler/hash validation belongs to terrain_static_fx_import.
inline bool validTerrainStaticFxBinding(const TerrainStaticFxBinding &fx,
    const std::array<float,4> &domain,std::string &error) {
  const auto fail=[&](const char *message){error=message;return false;};
  if(!fx.enabled) {
    if(fx.textureIndex!=InvalidMapTexture)return fail("Disabled static FX snapshot has a texture slot.");
    error.clear();return true;
  }
  if(fx.textureIndex==InvalidMapTexture||!fx.identity.valid()||!fx.revision||
      !fx.width||!fx.height||fx.width>2048||fx.height>2048||!fx.linear||!fx.clampSampling||fx.domain!=domain)
    return fail("Invalid static FX snapshot metadata.");
  if(fx.rgbaSha256.size()!=64)return fail("Invalid static FX pixel hash.");
  for(char c:fx.rgbaSha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return fail("Invalid static FX pixel hash.");
  for(float value:fx.domain)if(!std::isfinite(value)||std::abs(value)>1e8f)return fail("Invalid static FX domain.");
  if(fx.domain[2]<=0||fx.domain[3]<=0)return fail("Invalid static FX extent.");
  error.clear();return true;
}
// Shared gate for importer, cache, publisher and table. Presence is established
// by metadata here; the import/cache caller additionally verifies actual pixels.
inline bool diagnoseStaticTerrainFlowPublication(const TerrainSurfaceBinding &binding,std::string &error) {
  if(!validTerrainStaticFxBinding(binding.staticFx,binding.terrainDomain,error))return false;
  const auto &flow=binding.surface.flow;
  if((flow.wetness||flow.puddles||flow.wetnessMaskSnow)&&!binding.staticFx.enabled){error="Static Flow requires an immutable RecipeFX snapshot.";return false;}
  resources::TerrainFlowConsumerCapabilities caps;
  caps.wetness=caps.puddles=caps.heightWetness=caps.distanceWetness=caps.snowClearing=true;
  caps.staticControlProvided=binding.staticFx.enabled;
  std::vector<resources::TerrainSurfaceDiagnostic> diagnostics;
  return resources::diagnoseTerrainFlowPublication(binding.surface,[&](resources::AssetGuid identity){
    for(const auto &texture:binding.textures)if(texture.identity==identity&&texture.textureIndex!=InvalidMapTexture)return true;
    return false;
  },caps,diagnostics,error);
}

}
