#pragma once
#include "renderer/terrain_surface_binding.h"
#include <algorithm>
#include <cmath>
#include <span>

namespace ae::renderer {
// Atomic append to the std430 table. A missing descriptor is a publication
// error, never a white texture silently substituted for authored PBR data.
inline bool appendTerrainSurfaceTable(const TerrainSurfaceBinding &binding,
    std::span<const u32> slots,std::vector<float> &table,u32 &offsetPlusOne,std::string &error) {
  if(binding.surface.layers.empty()){offsetPlusOne=0;error.clear();return true;}
  if(!resources::validTerrainSurface(binding.surface,error) ||
     !std::isfinite(binding.localMetersPerUv) || binding.localMetersPerUv<=0) return false;
  const usize rows=TerrainSurfaceHeaderRows+binding.surface.layers.size()*TerrainSurfaceLayerRows;
  if(table.size()%4 || table.size()/4+rows>TerrainSurfaceMaximumRows) {
    error="Tabela de materiais do terreno excede o orçamento.";return false;
  }
  std::vector<float> next(rows*4,0.f);
  next[0]=float(binding.surface.layers.size());next[1]=1;next[4]=binding.localMetersPerUv;
  for(usize i=0;i<binding.surface.layers.size();++i) {
    const auto &layer=binding.surface.layers[i];float *out=next.data()+(TerrainSurfaceHeaderRows+i*TerrainSurfaceLayerRows)*4;
    out[0]=layer.active?1.f:0.f;out[1]=float(layer.projection);out[2]=layer.axisContrast;out[3]=layer.faceNormals?1.f:0.f;
    out[4]=layer.scaleX;out[5]=layer.scaleY;out[6]=layer.offsetX;out[7]=layer.offsetY;
    out[8]=layer.rotation;out[9]=layer.saturation;out[10]=layer.heightBlend?1.f:0.f;out[11]=layer.heightContrast;
    out[12]=layer.heightWidth;out[13]=layer.heightInfluence;out[14]=float(layer.heightQuality);
    std::copy(layer.tint.begin(),layer.tint.end(),out+16);
    for(usize role=0;role<7;++role) {
      const auto &map=layer.maps[role];float *data=out+(5+role*TerrainSurfaceMapRows)*4;
      data[0]=-1;data[1]=float(map.channel);data[2]=map.invert?1.f:0.f;data[3]=map.factor;
      if(map.texture.valid()) {
        const bool normal=role==1 || map.channel==resources::TerrainSurfaceChannel::NormalRGB || map.channel==resources::TerrainSurfaceChannel::NormalRG;
        const auto entry=std::find_if(binding.textures.begin(),binding.textures.end(),[&](const auto &t){
          return t.identity==map.texture && t.normal==normal && t.linear==(normal || map.linear);
        });
        if(entry==binding.textures.end() || entry->textureIndex>=slots.size() || slots[entry->textureIndex]==InvalidMapTexture) {
          error="Mapa PBR do terreno sem textura ou descritor válido.";return false;
        }
        if(slots[entry->textureIndex]>=16777216u){error="Descritor PBR fora do intervalo exato da GPU.";return false;}
        data[0]=float(slots[entry->textureIndex]);
      }
      data[4]=map.remapMin;data[5]=map.remapMax;data[6]=map.linear?1.f:0.f;
      std::copy(map.colorFactor.begin(),map.colorFactor.end(),data+8);
      std::copy(map.colorRemapMin.begin(),map.colorRemapMin.end(),data+12);
      std::copy(map.colorRemapMax.begin(),map.colorRemapMax.end(),data+16);
    }
  }
  offsetPlusOne=u32(table.size()/4)+1;
  table.insert(table.end(),next.begin(),next.end());error.clear();return true;
}
}
