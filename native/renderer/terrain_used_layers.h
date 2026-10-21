// Conservative publication-time proof: skip only channels that cannot contribute.
#pragma once
#include "renderer/terrain_control.h"
#include "renderer/authoring_texture.h"
namespace ae::renderer {
inline u32 terrainUsedLayerChannels(u32 material,std::span<const u8> vertices,
    std::span<const u32> indices,std::span<const MapDrawRecord> draws,
    std::span<const TerrainControlBinding> controls,std::span<const SharedAuthoringTexture> textures) {
  constexpr u32 all=15;
  if(vertices.size()%MapVertexStride) return all;
  u32 used=0;
  if(material<controls.size() && controls[material].textureIndex!=InvalidMapTexture) {
    const auto index=controls[material].textureIndex;
    if(index>=textures.size() || !textures[index]) return all;
    const auto &map=*textures[index];
    if(map.format!=AuthoringTextureRgba8 || map.srgb || map.firstLevel || map.levels!=1 ||
       !map.width || !map.height || u64(map.width)*map.height*4!=map.mipChain.size()) return all;
    for(usize n=0;n<map.mipChain.size();++n) if(map.mipChain[n]) used|=1u<<(n%4);
  }
  bool found=false;
  for(const auto &draw:draws) if(draw.materialIndex==material) {
    found=true;
    if(u64(draw.firstIndex)+draw.indexCount>indices.size()) return all;
    for(u32 n=0;n<draw.indexCount;++n) {
      const u64 vertex=u64(indices[draw.firstIndex+n])+draw.vertexOffset;
      if(vertex>=vertices.size()/MapVertexStride) return all;
      // Packed COLOR_0 also covers closed sides/base and voxel fallback.
      for(u32 channel=0;channel<4;++channel) if(vertices[vertex*MapVertexStride+44+channel]) used|=1u<<channel;
    }
  }
  return found?used:all;
}
}
