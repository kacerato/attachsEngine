#pragma once
#include "renderer/terrain_surface_binding.h"
#include <limits>
#include <span>
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
// Flat source-library indices become deduplicated publication indices. These
// are still NOT GPU descriptors or renderer package indices. Validate all
// references before changing any PBR or StaticFX index.
inline bool remapTerrainSurfaceFlatTextureIndices(TerrainSurfaceBinding &binding,std::span<const u32> remap,std::string &error){
 const auto fits=[&](u32 index){return index!=InvalidMapTexture&&usize(index)<remap.size()&&remap[index]!=InvalidMapTexture;};
 for(const auto &texture:binding.textures)if(!fits(texture.textureIndex)){error="Invalid flat PBR source index or dedup destination.";return false;}
 if(binding.staticFx.enabled&&!fits(binding.staticFx.textureIndex)){error="Invalid flat StaticFX source index or dedup destination.";return false;}
 for(auto &texture:binding.textures)texture.textureIndex=remap[texture.textureIndex];
 if(binding.staticFx.enabled)binding.staticFx.textureIndex=remap[binding.staticFx.textureIndex];
 error.clear();return true;
}

}
