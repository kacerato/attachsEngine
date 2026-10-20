#pragma once
#include "core/base.h"
#include <algorithm>
namespace ae::resources {
enum class TerrainTextureResidencyMode : u32 { InheritTerrain, Original, Cap };
struct TerrainTextureResidency {
  TerrainTextureResidencyMode mode=TerrainTextureResidencyMode::InheritTerrain;
  u32 maxSide=0;
};
inline bool validTerrainTextureResidency(TerrainTextureResidency choice,bool terrainLevel=false){
  if(u32(choice.mode)>u32(TerrainTextureResidencyMode::Cap)||(terrainLevel&&choice.mode==TerrainTextureResidencyMode::InheritTerrain))return false;
  if(choice.mode!=TerrainTextureResidencyMode::Cap)return choice.maxSide==0;
  return choice.maxSide==4096||choice.maxSide==2048||choice.maxSide==1024||choice.maxSide==512||choice.maxSide==256;
}
// Pure authored bound; sampling quality and the global dynamic mip policy are
// deliberately independent. Output remains unchanged on a rejected chain.
inline bool terrainTextureFirstAllowedMip(u32 width,u32 height,u32 levels,
    TerrainTextureResidency terrain,TerrainTextureResidency map,u32 &first){
  if(!width||!height||!levels||!validTerrainTextureResidency(terrain,true)||!validTerrainTextureResidency(map))return false;
  u32 expected=1;for(u32 w=width,h=height;w>1||h>1;){w=std::max(1u,w/2);h=std::max(1u,h/2);++expected;}
  if(levels!=expected)return false;
  auto resolved=map.mode==TerrainTextureResidencyMode::InheritTerrain?terrain:map;
  u32 chosen=0;if(resolved.mode==TerrainTextureResidencyMode::Cap){
    while(std::max(width,height)>resolved.maxSide){width=std::max(1u,width/2);height=std::max(1u,height/2);++chosen;}
  }
  first=chosen;return true;
}
} // namespace ae::resources
