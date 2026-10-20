#pragma once
#include "renderer/terrain_surface_binding.h"
#include <algorithm>
#include <cmath>
#include <span>

namespace ae::renderer {
// Internal advanced-v1 ABI; matching constants live in terrain_pbr.glsl.
namespace terrain_surface_abi {
inline constexpr usize NoiseRows=7, NoiseCount=5;
inline constexpr usize AdvancedHeaderRows=2, LayerDirectoryRows=2, NoiseHeaderRows=2;
inline constexpr usize GlobalRows=AdvancedHeaderRows+NoiseCount*NoiseRows;
inline constexpr usize LayerRows=LayerDirectoryRows+NoiseCount*NoiseRows;
inline constexpr usize LayerExtraRows=3, AntiEntryRows=3*NoiseRows, AntiDirectoryRows=1;
inline constexpr u32 FeatureM05=1, FeatureM06=2, FeatureAntiTile=4, FeatureM07=8, FeatureM08=16;
inline constexpr usize ResampleHeaderRows=2, ResampleRows=ResampleHeaderRows+NoiseRows;
inline constexpr usize ClusterHeaderRows=2, ClusterRows=ClusterHeaderRows+NoiseRows;
inline constexpr usize VariantRows=1+7*TerrainSurfaceMapRows;
}

// Atomic append to the std430 table. A missing descriptor is a publication
// error, never a white texture silently substituted for authored PBR data.
inline bool appendTerrainSurfaceTable(const TerrainSurfaceBinding &binding,
    std::span<const u32> slots,std::vector<float> &table,u32 &offsetPlusOne,std::string &error) {
  if(binding.surface.layers.empty()){offsetPlusOne=0;error.clear();return true;}
  using namespace terrain_surface_abi;
  if(!resources::validTerrainSurface(binding.surface,error))return false;
  if(!std::isfinite(binding.localMetersPerUv)||binding.localMetersPerUv<=0){
    error="Escala local de UV do terreno deve ser finita e positiva.";return false;
  }
  // Advanced v1: two header rows, five global seven-row noises, then
  // two directory rows and five seven-row local noises per authored layer.
  // M06 keeps those strides. Layer directory.z/.w point to anti binding /
  // global normal participation in three appended rows per layer. Optional
  // GUID-sorted anti directory follows, with three seven-row noises per entry.
  // Header1 features: 1=M05, 2=M06 extension, 4=anti-tile, 8=M07, 16=M08.
  // Layer participation row.z points to its appended two-property-row plus
  // seven-row resample-noise record; row.w points to the M08 cluster record.
  // Cluster records: enabled/stochastic/count/quality; contrast/noiseBoost/
  // variantsAbsolute/endExclusive; seven-row selection noise; 36 rows/variant.
  const bool normalNoise=std::any_of(binding.surface.normalNoise.begin(),binding.surface.normalNoise.end(),[](const auto &n){return n.enabled;}) ||
    std::any_of(binding.surface.layers.begin(),binding.surface.layers.end(),[](const auto &l){return std::any_of(l.normalNoise.begin(),l.normalNoise.end(),[](const auto &n){return n.enabled;});});
  const bool antiTile=binding.surface.antiTileArray.enabled;
  const bool m06=normalNoise||antiTile;
  std::vector<const resources::TerrainAntiTileEntry*> entries;
  if(antiTile){for(const auto &entry:binding.surface.antiTileArray.entries)entries.push_back(&entry);
    std::sort(entries.begin(),entries.end(),[](const auto *a,const auto *b){return a->identity<b->identity;});}
  const bool m07=std::any_of(binding.surface.layers.begin(),binding.surface.layers.end(),[](const auto &l){return l.resampleAlbedo||l.resampleNormal||l.resampleHeightBlend;});
  const bool m08=std::any_of(binding.surface.layers.begin(),binding.surface.layers.end(),[](const auto &l){return l.cluster.enabled||l.cluster.stochastic;});
  const bool advanced=m08 || m07 || m06 || binding.surface.detailNoise.enabled || binding.surface.distanceNoise.enabled ||
    std::any_of(binding.surface.layers.begin(),binding.surface.layers.end(),[](const auto &l){return l.detailNoise.enabled || l.distanceNoise.enabled;});
  const usize baseRows=TerrainSurfaceHeaderRows+binding.surface.layers.size()*TerrainSurfaceLayerRows;
  const usize extensionRow=baseRows+GlobalRows+binding.surface.layers.size()*LayerRows;
  const usize antiDirectoryRow=extensionRow+(m06?binding.surface.layers.size()*LayerExtraRows:0);
  const usize resampleRow=antiDirectoryRow+(antiTile?AntiDirectoryRows+entries.size()*AntiEntryRows:0);
  const usize clusterStart=resampleRow+(m07?binding.surface.layers.size()*ResampleRows:0);
  std::vector<usize> clusterRows(binding.surface.layers.size());usize clusterEnd=clusterStart;
  if(m08)for(usize i=0;i<binding.surface.layers.size();++i){clusterRows[i]=clusterEnd;clusterEnd+=ClusterRows+binding.surface.layers[i].cluster.variants.size()*VariantRows;}
  const usize rows=advanced?clusterEnd:baseRows;
  if(table.size()%4 || table.size()/4+rows>TerrainSurfaceMaximumRows) {
    error="Tabela de materiais do terreno excede o orçamento.";return false;
  }
  std::vector<float> next(rows*4,0.f);
  next[0]=float(binding.surface.layers.size());next[1]=1;next[4]=binding.localMetersPerUv;
  auto writeMap=[&](const resources::TerrainSurfaceMap &map,bool normalRole,float *data)->bool {
      data[0]=-1;data[1]=float(map.channel);data[2]=map.invert?1.f:0.f;data[3]=map.factor;
      if(map.texture.valid()) {
        const bool normal=normalRole || map.channel==resources::TerrainSurfaceChannel::NormalRGB || map.channel==resources::TerrainSurfaceChannel::NormalRG;
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
    return true;
  };
  for(usize i=0;i<binding.surface.layers.size();++i) {
    const auto &layer=binding.surface.layers[i];float *out=next.data()+(TerrainSurfaceHeaderRows+i*TerrainSurfaceLayerRows)*4;
    out[0]=layer.active?1.f:0.f;out[1]=float(layer.projection);out[2]=layer.axisContrast;out[3]=layer.faceNormals?1.f:0.f;
    out[4]=layer.scaleX;out[5]=layer.scaleY;out[6]=layer.offsetX;out[7]=layer.offsetY;
    out[8]=layer.rotation;out[9]=layer.saturation;out[10]=layer.heightBlend?1.f:0.f;out[11]=layer.heightContrast;
    out[12]=layer.heightWidth;out[13]=layer.heightInfluence;out[14]=float(layer.heightQuality);
    std::copy(layer.tint.begin(),layer.tint.end(),out+16);
    for(usize role=0;role<7;++role) {
      const auto &map=layer.maps[role];float *data=out+(5+role*TerrainSurfaceMapRows)*4;
      if(!writeMap(map,role==1,data))return false;
    }
  }
  if(advanced) {
    const usize absoluteBase=table.size()/4;
    next[2]=float(absoluteBase+baseRows+1);
    float *header=next.data()+baseRows*4;
    header[0]=1;header[1]=float(binding.surface.layers.size());
    header[2]=float(absoluteBase+baseRows+AdvancedHeaderRows);header[3]=float(absoluteBase+baseRows+GlobalRows);
    header[4]=antiTile?float(absoluteBase+antiDirectoryRow):0;
    header[5]=float(entries.size());header[6]=float(absoluteBase+rows);header[7]=float(FeatureM05+(m06?FeatureM06:0)+(antiTile?FeatureAntiTile:0)+(m07?FeatureM07:0)+(m08?FeatureM08:0));
    auto writeNoise=[&](const resources::TerrainSurfaceNoise &noise,usize row,bool normalRole=false)->bool {
      float *out=next.data()+row*4;
      out[0]=noise.enabled?1.f:0.f;out[1]=float(noise.projection);out[2]=noise.scale;out[3]=noise.strength;
      out[4]=noise.fadeStart;out[5]=noise.fadeEnd;
      return writeMap(noise.map,normalRole,out+NoiseHeaderRows*4);
    };
    auto writeNoises=[&](const auto &source,usize row)->bool {
      if(!writeNoise(source.detailNoise,row)||!writeNoise(source.distanceNoise,row+NoiseRows))return false;
      if(m06)for(usize j=0;j<3;++j)if(!writeNoise(source.normalNoise[j],row+2*NoiseRows+j*NoiseRows,true))return false;
      return true;
    };
    if(!writeNoises(binding.surface,baseRows+AdvancedHeaderRows))return false;
    for(usize i=0;i<binding.surface.layers.size();++i){
      usize directory=baseRows+GlobalRows+i*LayerRows;
      float *out=next.data()+directory*4;
      out[0]=float(absoluteBase+directory+LayerDirectoryRows);out[1]=float(absoluteBase+directory+1);
      out[4]=binding.surface.layers[i].globalDetailParticipation;
      out[5]=binding.surface.layers[i].globalDistanceParticipation;
      if(m07)out[6]=float(absoluteBase+resampleRow+i*ResampleRows);
      if(m08)out[7]=float(absoluteBase+clusterRows[i]);
      const auto &layer=binding.surface.layers[i];
      if(m06){
        const usize extra=extensionRow+i*LayerExtraRows;
        out[2]=float(absoluteBase+extra+1);out[3]=float(absoluteBase+extra);
        float *extension=next.data()+extra*4;
        std::copy(layer.globalNormalParticipation.begin(),layer.globalNormalParticipation.end(),extension);
        if(antiTile&&layer.antiTileEntry.valid()){
          const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto *entry){return entry->identity==layer.antiTileEntry;});
          if(found==entries.end()){error="Unresolved anti-tile identity.";return false;}
          extension[4]=float(absoluteBase+antiDirectoryRow+AntiDirectoryRows+usize(found-entries.begin())*AntiEntryRows);extension[5]=1;
        }
        std::copy(layer.antiTileParticipation.begin(),layer.antiTileParticipation.end(),extension+8);
      }
      if(!writeNoises(layer,directory+LayerDirectoryRows))return false;
    }
    if(antiTile){
      float *directory=next.data()+antiDirectoryRow*4;
      directory[0]=1;directory[1]=1;directory[2]=float(absoluteBase+antiDirectoryRow+AntiDirectoryRows);directory[3]=float(absoluteBase+resampleRow);
      for(usize i=0;i<entries.size();++i){
        const usize row=antiDirectoryRow+AntiDirectoryRows+i*AntiEntryRows;
        if(!writeNoise(entries[i]->normal,row,true)||!writeNoise(entries[i]->detail,row+NoiseRows)||!writeNoise(entries[i]->distance,row+2*NoiseRows))return false;
      }
    }
    if(m07)for(usize i=0;i<binding.surface.layers.size();++i){
      const auto &layer=binding.surface.layers[i];const usize row=resampleRow+i*ResampleRows;
      float *out=next.data()+row*4;
      out[0]=layer.resampleAlbedo?1.f:0.f;out[1]=layer.resampleNormal?1.f:0.f;
      out[2]=layer.resampleHeightBlend?1.f:0.f;out[3]=layer.resampleScale;
      out[4]=layer.resampleStart;out[5]=layer.resampleEnd;out[6]=float(layer.resampleBlend);
      if(!writeNoise(layer.resampleNoise,row+ResampleHeaderRows))return false;
    }
    if(m08)for(usize i=0;i<binding.surface.layers.size();++i){
      const auto &cluster=binding.surface.layers[i].cluster;const usize row=clusterRows[i];
      float *out=next.data()+row*4;
      out[0]=cluster.enabled?1.f:0.f;out[1]=cluster.stochastic?1.f:0.f;
      out[2]=float(cluster.variants.size());out[3]=float(cluster.quality);
      out[4]=cluster.contrast;out[5]=cluster.noiseBoost;out[6]=float(absoluteBase+row+ClusterRows);
      out[7]=float(absoluteBase+row+ClusterRows+cluster.variants.size()*VariantRows);
      if(!writeNoise(cluster.selection,row+ClusterHeaderRows))return false;
      for(usize j=0;j<cluster.variants.size();++j){
        const auto &variant=cluster.variants[j];float *data=next.data()+(row+ClusterRows+j*VariantRows)*4;
        data[0]=variant.scale;data[1]=variant.offsetX;data[2]=variant.offsetY;data[3]=variant.rotation;
        for(usize role=0;role<variant.maps.size();++role)if(!writeMap(variant.maps[role],role==1,data+(1+role*TerrainSurfaceMapRows)*4))return false;
      }
    }
  }
  offsetPlusOne=u32(table.size()/4)+1;
  table.insert(table.end(),next.begin(),next.end());error.clear();return true;
}
// Standalone parts are canonical at row zero for deduplication. Relocate only
// the copy being inserted, after finding/storing that canonical key.
inline bool relocateTerrainSurfaceTable(std::vector<float> &part,u32 destinationRow,std::string &error) {
  using namespace terrain_surface_abi;
  auto fail=[&](){error="Diretorio avancado de terreno invalido.";return false;};
  if(part.size()%4 || part.size()<TerrainSurfaceHeaderRows*4)return fail();
  const usize rows=part.size()/4;
  if(destinationRow>TerrainSurfaceMaximumRows || rows>TerrainSurfaceMaximumRows-destinationRow)return fail();
  auto integer=[](float x){return std::isfinite(x)&&x>=0&&std::floor(x)==x;};
  if(!integer(part[0])||part[0]>32||!integer(part[2])||part[2]>float(rows))return fail();
  if(part[1]!=1 || !std::isfinite(part[4]) || part[4]<=0 || rows<TerrainSurfaceHeaderRows+usize(part[0])*TerrainSurfaceLayerRows)return fail();
  if(part[2]==0){
    if(rows!=TerrainSurfaceHeaderRows+usize(part[0])*TerrainSurfaceLayerRows)return fail();
    error.clear();return true;
  }
  const usize advanced=usize(part[2])-1;
  const usize count=usize(part[0]);
  if(advanced!=TerrainSurfaceHeaderRows+count*TerrainSurfaceLayerRows || advanced+AdvancedHeaderRows>rows)return fail();
  float *header=part.data()+advanced*4;
  if(!integer(header[7])||header[7]<1||header[7]>FeatureM05+FeatureM06+FeatureAntiTile+FeatureM07+FeatureM08)return fail();
  const u32 features=u32(header[7]);const bool m06=(features&FeatureM06)!=0,antiTile=(features&FeatureAntiTile)!=0,m07=(features&FeatureM07)!=0,m08=(features&FeatureM08)!=0;
  if(!(features&FeatureM05)||(antiTile&&!m06))return fail();
  const usize extension=advanced+GlobalRows+count*LayerRows;
  const usize antiDirectory=extension+(m06?count*LayerExtraRows:0);
  if(!integer(header[5])||header[5]>32)return fail();
  const usize entries=usize(header[5]);
  const usize resample=antiDirectory+(antiTile?AntiDirectoryRows+entries*AntiEntryRows:0);
  const usize clusterStart=resample+(m07?count*ResampleRows:0);
  if(header[0]!=TerrainSurfaceAdvancedVersion || header[1]!=part[0] || header[2]!=float(advanced+AdvancedHeaderRows) ||
     header[3]!=float(advanced+GlobalRows) || header[4]!=(antiTile?float(antiDirectory):0.f) ||
     (!antiTile&&entries!=0) || header[6]!=float(rows) || (m08?rows<clusterStart:rows!=clusterStart))return fail();
  std::vector<usize> pointers{2,advanced*4+2,advanced*4+3,advanced*4+6};
  if(antiTile){
    if(entries==0)return fail();
    const float *directory=part.data()+antiDirectory*4;
    if(directory[0]!=1||directory[1]!=1||directory[2]!=float(antiDirectory+AntiDirectoryRows)||directory[3]!=float(resample))return fail();
    pointers.push_back(advanced*4+4);pointers.push_back(antiDirectory*4+2);pointers.push_back(antiDirectory*4+3);
  }
  usize clusterCursor=clusterStart;
  for(usize i=0;i<count;++i){
    const usize directory=advanced+GlobalRows+i*LayerRows;const float *entry=part.data()+directory*4;
    if(entry[0]!=float(directory+LayerDirectoryRows)||entry[1]!=float(directory+1))return fail();
    pointers.push_back(directory*4);pointers.push_back(directory*4+1);
    if(m07){
      if(entry[6]!=float(resample+i*ResampleRows))return fail();
      pointers.push_back(directory*4+6);
    }else if(entry[6]!=0)return fail();
    if(m08){
      if(entry[7]!=float(clusterCursor)||clusterCursor+ClusterRows>rows)return fail();
      const float *cluster=part.data()+clusterCursor*4;
      if((cluster[0]!=0&&cluster[0]!=1)||(cluster[1]!=0&&cluster[1]!=1)||!integer(cluster[2])||cluster[2]>3||
        (cluster[0]==1&&cluster[2]<2)||!integer(cluster[3])||cluster[3]>2)return fail();
      const usize end=clusterCursor+ClusterRows+usize(cluster[2])*VariantRows;
      if(end>rows||cluster[6]!=float(clusterCursor+ClusterRows)||cluster[7]!=float(end))return fail();
      pointers.push_back(directory*4+7);pointers.push_back(clusterCursor*4+6);pointers.push_back(clusterCursor*4+7);
      clusterCursor=end;
    }else if(entry[7]!=0)return fail();
    if(m06){
      const usize extra=extension+i*LayerExtraRows;
      if(entry[2]!=float(extra+1)||entry[3]!=float(extra))return fail();
      pointers.push_back(directory*4+2);pointers.push_back(directory*4+3);
      const float *binding=part.data()+(extra+1)*4;
      if(binding[1]!=0&&binding[1]!=1)return fail();
      if(binding[1]==1){
        if(!antiTile||!integer(binding[0])||binding[0]<float(antiDirectory+AntiDirectoryRows)||binding[0]>=float(resample)||
          (usize(binding[0])-antiDirectory-AntiDirectoryRows)%AntiEntryRows!=0)return fail();
        pointers.push_back((extra+1)*4);
      }else if(binding[0]!=0)return fail();
    }else if(entry[2]!=0||entry[3]!=0)return fail();
  }
  if(clusterCursor!=rows)return fail();
  // No mutation until every version, extent and pointer has been checked.
  for(usize pointer:pointers)part[pointer]+=float(destinationRow);
  error.clear();return true;
}

}
