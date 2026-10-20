#pragma once
#include "resources/terrain_asset.h"
#include "resources/mesh_lod_build.h"

namespace ae::resources {
// Read the authored policy on every import, including a cold scene reopen.
// Older recipes keep the caller's generic import settings. Version 6 carries
// an explicit terrain override, including disabled generation (one level).
inline bool applyTerrainLodRecipe(std::string_view text, MeshLodSettings &settings, std::string &error) {
  if(text.empty())return true;
  TerrainRecipe recipe;
  if(!deserializeTerrainRecipe(text,recipe,error))return false;
  // The recipe reader already rejects unknown versions. Every accepted version
  // from six onward carries this policy, including later authoring extensions.
  const auto version=text.substr(0,text.find(' '));
  if(version=="AETERRAIN1"||version=="AETERRAIN2"||version=="AETERRAIN3"||
     version=="AETERRAIN4"||version=="AETERRAIN5")return true;
  settings.generate=recipe.lodLevels>1;
  settings.maximumLevels=recipe.lodLevels;
  settings.maximumRelativeError=recipe.lodRelativeError;
  settings.preserveSurfaceAttributes=true;
  return true;
}
} // namespace ae::resources
