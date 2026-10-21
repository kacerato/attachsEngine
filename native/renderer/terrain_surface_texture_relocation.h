#pragma once
#include "renderer/terrain_surface_binding.h"
#include <limits>
namespace ae::renderer {
// Source-library texture indices become complete-package indices exactly once.
// This is unrelated to GPU table-row relocation and never offsets descriptors.
inline bool offsetTerrainSurfaceSourceTextureIndices(TerrainSurfaceBinding &binding,
 u32 offset,usize sourceCount,std::string &error){
 const auto fits=[&](u32 index){return index!=InvalidMapTexture&&usize(index)<sourceCount&&u64(index)+offset<InvalidMapTexture;};
 for(const auto &texture:binding.textures)if(!fits(texture.textureIndex)){error="Invalid terrain source texture index or offset overflow.";return false;}
 if(binding.staticFx.enabled&&!fits(binding.staticFx.textureIndex)){error="Invalid StaticFX source texture index or offset overflow.";return false;}
 for(auto &texture:binding.textures)texture.textureIndex+=offset;
 if(binding.staticFx.enabled)binding.staticFx.textureIndex+=offset;
 error.clear();return true;
}
}
