#pragma once
#include "resources/terrain_surface_asset.h"
namespace ae::resources {
struct TerrainSurfaceTextureRef {AssetGuid identity;bool linear=true,normal=false,normalRG=false;};
inline std::vector<TerrainSurfaceTextureRef> terrainSurfaceTextureRefs(const TerrainSurfaceAsset &surface) {
  std::vector<TerrainSurfaceTextureRef> refs;
  const auto add=[&](const TerrainSurfaceMap &map,bool normal=false) {
    if(!map.texture.valid())return;
    normal=normal || map.channel==TerrainSurfaceChannel::NormalRGB || map.channel==TerrainSurfaceChannel::NormalRG;
    const bool linear=normal || map.linear;
    const bool rg=normal && map.channel==TerrainSurfaceChannel::NormalRG;
    for(const auto &ref:refs)if(ref.identity==map.texture && ref.linear==linear && ref.normal==normal && ref.normalRG==rg)return;
    refs.push_back({map.texture,linear,normal,rg});
  };
  const auto noise=[&](const TerrainSurfaceNoise &n,bool normal=false){add(n.map,normal);};
  noise(surface.detailNoise);noise(surface.distanceNoise);
  add(surface.geo.albedo);add(surface.geo.normal,true);
  for(u32 role=0;role<surface.globalMaps.size();++role){
    const auto &global=surface.globalMaps[role];add(global.map,role==1);
    add(global.smoothness);add(global.ao);add(global.metallic);add(global.mask);
  }
  for(const auto &n:surface.normalNoise)noise(n,true);
  for(const auto &entry:surface.antiTileArray.entries){noise(entry.normal,true);noise(entry.detail);noise(entry.distance);}
  for(const auto &layer:surface.layers) {
    if(layer.thumbnail.valid()){TerrainSurfaceMap map;map.texture=layer.thumbnail;map.linear=false;add(map);}
    for(u32 i=0;i<7;++i)add(layer.maps[i],i==1);
    noise(layer.detailNoise);noise(layer.distanceNoise);noise(layer.resampleNoise);noise(layer.cluster.selection);
    for(const auto &n:layer.normalNoise)noise(n,true);
    for(const auto &variant:layer.cluster.variants)for(u32 i=0;i<7;++i)add(variant.maps[i],i==1);
  }
  return refs;
}
inline std::string terrainSurfaceHex(std::string_view text) {
  constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(text.size()*2);
  for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;
}
inline bool terrainSurfaceUnhex(std::string_view token,std::string &out) {
  if(token.size()>2*1024*1024 || token.size()%2)return false;
  const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  std::string text;for(usize i=0;i<token.size();i+=2){const int a=digit(token[i]),b=digit(token[i+1]);if(a<0||b<0)return false;text+=char(a*16+b);}
  out=std::move(text);return true;
}
}
