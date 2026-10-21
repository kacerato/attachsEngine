#pragma once
#include "renderer/terrain_surface_binding.h"
#include "renderer/terrain_static_fx_binding.h"
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
inline constexpr u32 FeatureM05=1, FeatureM06=2, FeatureAntiTile=4, FeatureM07=8, FeatureM08=16, FeatureGeo=32, FeatureGlobals=64, FeatureSnow=128, FeatureStaticFx=256;
inline constexpr usize SnowHeaderRows=16,SnowMaps=8,SnowRows=SnowHeaderRows+SnowMaps*TerrainSurfaceMapRows;
inline constexpr usize ResampleHeaderRows=2, ResampleRows=ResampleHeaderRows+NoiseRows;
inline constexpr usize ClusterHeaderRows=2, ClusterRows=ClusterHeaderRows+NoiseRows;
inline constexpr usize VariantRows=1+7*TerrainSurfaceMapRows;
inline constexpr usize ModuleRows=3, ModuleLayerRows=2, GeoRows=4+2*TerrainSurfaceMapRows;
inline constexpr usize GlobalRecordRows=4+5*TerrainSurfaceMapRows, GlobalRecords=4;
inline constexpr usize GeoCurveSamples=256, GeoCurveRows=GeoCurveSamples/4;
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
  const bool geo=binding.surface.geo.mode!=resources::TerrainGeoMode::Off;
  const bool globals=std::any_of(binding.surface.globalMaps.begin(),binding.surface.globalMaps.end(),[](const auto &g){return g.enabled;});
  const bool snow=binding.surface.snow.mode!=resources::TerrainSnowMode::Off;
  if(snow&&(binding.surface.snow.useGlobalLevel||binding.surface.snow.useGlobalHeight)){error="Snow global flags require an authored runtime controller; no controller source is available.";return false;}
  const auto &flow=binding.surface.flow;
  const bool streams=flow.streams,lava=flow.lava,rain=flow.rainDrops,specularFade=flow.specularFade;
  const bool needsSnapshot=flow.wetness||flow.puddles||streams||lava||flow.wetnessMaskSnow;
  const bool fx=needsSnapshot||specularFade;
  if(!diagnoseStaticTerrainFlowPublication(binding,error))return false;
  if(fx&&binding.staticFx.enabled&&(binding.staticFx.textureIndex>=slots.size()||slots[binding.staticFx.textureIndex]==InvalidMapTexture||slots[binding.staticFx.textureIndex]>=16777216u)){
    error="Static FX requires an immutable snapshot with a valid GPU descriptor.";return false;
  }
  const bool modules=geo||globals||snow||fx;
  for(float value:binding.terrainDomain)if(!std::isfinite(value)||std::abs(value)>1e8f){error="Invalid terrain domain.";return false;}
  if(binding.terrainDomain[2]<=0||binding.terrainDomain[3]<=0){error="Invalid terrain domain extent.";return false;}
  const bool advanced=modules || m08 || m07 || m06 || binding.surface.detailNoise.enabled || binding.surface.distanceNoise.enabled ||
    std::any_of(binding.surface.layers.begin(),binding.surface.layers.end(),[](const auto &l){return l.detailNoise.enabled || l.distanceNoise.enabled;});
  const usize baseRows=TerrainSurfaceHeaderRows+binding.surface.layers.size()*TerrainSurfaceLayerRows;
  const usize extensionRow=baseRows+GlobalRows+binding.surface.layers.size()*LayerRows;
  const usize antiDirectoryRow=extensionRow+(m06?binding.surface.layers.size()*LayerExtraRows:0);
  const usize resampleRow=antiDirectoryRow+(antiTile?AntiDirectoryRows+entries.size()*AntiEntryRows:0);
  const usize clusterStart=resampleRow+(m07?binding.surface.layers.size()*ResampleRows:0);
  std::vector<usize> clusterRows(binding.surface.layers.size());usize clusterEnd=clusterStart;
  if(m08)for(usize i=0;i<binding.surface.layers.size();++i){clusterRows[i]=clusterEnd;clusterEnd+=ClusterRows+binding.surface.layers[i].cluster.variants.size()*VariantRows;}
  const usize moduleRoot=clusterEnd,moduleLayers=moduleRoot+ModuleRows+(fx?1:0);
  const usize geoRow=moduleLayers+binding.surface.layers.size()*ModuleLayerRows;
  const usize globalRow=geoRow+(geo?GeoRows:0);
  const usize curveRow=globalRow+(globals?GlobalRecords*GlobalRecordRows:0);
  const bool curve=geo&&binding.surface.geo.curveEnabled;
  const usize snowRow=curveRow+(curve?GeoCurveRows:0),snowLayers=snowRow+SnowRows;
  const usize fxDirectory=snowRow+(snow?SnowRows+binding.surface.layers.size():0);
  const usize fxEntries=3+(streams?1:0)+(lava?1:0)+(rain?1:0)+(specularFade?1:0);
  const usize fxSnapshot=fxDirectory+fxEntries,fxWetness=fxSnapshot+3,fxPuddles=fxWetness+4,fxLayers=fxPuddles+1;
  const usize streamRow=fxLayers+binding.surface.layers.size(),lavaRow=streamRow+(streams?11:0),rainRow=lavaRow+(lava?14:0),specularRow=rainRow+(rain?7:0);
  const usize rows=advanced?(modules?(fx?specularRow+(specularFade?2:0):fxDirectory):clusterEnd):baseRows;
  if(table.size()%4 || table.size()/4+rows>TerrainSurfaceMaximumRows) {
    error="Tabela de materiais do terreno excede o orÃƒÆ’Ã‚Â§amento.";return false;
  }
  std::vector<float> next(rows*4,0.f);
  next[0]=float(binding.surface.layers.size());next[1]=1;next[4]=binding.localMetersPerUv;
  auto writeMap=[&](const resources::TerrainSurfaceMap &map,bool normalRole,float *data,bool clampSampling=false)->bool {
      data[0]=-1;data[1]=float(map.channel);data[2]=map.invert?1.f:0.f;data[3]=map.factor;
      if(map.texture.valid()) {
        const bool normal=normalRole || map.channel==resources::TerrainSurfaceChannel::NormalRGB || map.channel==resources::TerrainSurfaceChannel::NormalRG || map.channel==resources::TerrainSurfaceChannel::NormalGA;
        const u32 packing=normal?(map.channel==resources::TerrainSurfaceChannel::NormalRG?6u:map.channel==resources::TerrainSurfaceChannel::NormalGA?7u:5u):0u;
        const auto policy=map.textureResidency.mode==resources::TerrainTextureResidencyMode::InheritTerrain?binding.surface.textureResidency:map.textureResidency;
        const u32 cap=policy.mode==resources::TerrainTextureResidencyMode::Cap?policy.maxSide:0u;
        const auto entry=std::find_if(binding.textures.begin(),binding.textures.end(),[&](const auto &t){
          return t.identity==map.texture && t.normal==normal && t.linear==(normal || map.linear) && (t.normalChannel==packing || (t.normalChannel==0&&(packing==5||packing==6))) && t.residencyCap==cap && t.clampSampling==clampSampling;
        });
        if(entry==binding.textures.end() || entry->textureIndex>=slots.size() || slots[entry->textureIndex]==InvalidMapTexture) {
          error="Mapa PBR do terreno sem textura ou descritor vÃƒÆ’Ã‚Â¡lido.";return false;
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
    header[5]=float(entries.size());header[6]=float(absoluteBase+rows);header[7]=float(FeatureM05+(m06?FeatureM06:0)+(antiTile?FeatureAntiTile:0)+(m07?FeatureM07:0)+(m08?FeatureM08:0)+(geo?FeatureGeo:0)+(globals?FeatureGlobals:0)+(snow?FeatureSnow:0)+(fx?FeatureStaticFx:0));
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
  if(modules){
    const usize absoluteBase=table.size()/4;next[5]=float(absoluteBase+moduleRoot+1);
    float *root=next.data()+moduleRoot*4;
    root[0]=fx?(rain||specularFade?5:streams||lava?4:3):(snow?2:1);root[1]=geo?float(absoluteBase+geoRow):0;root[2]=globals?float(absoluteBase+globalRow):0;root[3]=float(absoluteBase+moduleLayers);
    root[4]=float(absoluteBase+rows);root[5]=curve?float(absoluteBase+curveRow):0;root[6]=curve?float(GeoCurveSamples):0;
    root[7]=snow?float(absoluteBase+snowRow+1):0;
    std::copy(binding.terrainDomain.begin(),binding.terrainDomain.end(),root+8);
    if(fx){
      root[12]=float(absoluteBase+fxDirectory+1);root[13]=float(fxEntries);root[14]=1;root[15]=float(absoluteBase+rows);
      for(usize n=0;n<3;++n){float *entry=next.data()+(fxDirectory+n)*4;entry[0]=float(n+1);entry[1]=1;entry[2]=float(absoluteBase+(n==0?fxSnapshot:n==1?fxWetness:fxPuddles));entry[3]=float(n==0?3:n==1?4:1);}
      float *snapshot=next.data()+fxSnapshot*4;
      snapshot[0]=binding.staticFx.enabled?float(slots[binding.staticFx.textureIndex]):-1.f;snapshot[1]=binding.staticFx.enabled?float(binding.staticFx.width):0;snapshot[2]=binding.staticFx.enabled?float(binding.staticFx.height):0;snapshot[3]=float((binding.staticFx.enabled?1:0)+(streams?2:0)+(lava?4:0));
      std::copy(binding.terrainDomain.begin(),binding.terrainDomain.end(),snapshot+4);
      snapshot[8]=float(absoluteBase+fxLayers);snapshot[9]=float(binding.surface.layers.size());snapshot[11]=float(absoluteBase+rows);
      float *wet=next.data()+fxWetness*4;
      wet[0]=flow.wetness?1:0;wet[1]=flow.heightWetness?1:0;wet[2]=flow.distanceWetness?1:0;wet[3]=flow.globalPorosity;
      std::copy(flow.wetnessParams.begin(),flow.wetnessParams.end(),wet+4);
      std::copy(flow.heightWetnessParams.begin(),flow.heightWetnessParams.end(),wet+8);
      std::copy(flow.distanceWetnessParams.begin(),flow.distanceWetnessParams.end(),wet+12);
      float *puddle=next.data()+fxPuddles*4;puddle[0]=flow.puddles?1:0;puddle[1]=flow.puddleParams[0];puddle[2]=flow.puddleParams[1];puddle[3]=float((flow.perTexPorosity?1:0)|(flow.wetnessMaskSnow?2:0));
      for(usize n=0;n<binding.surface.layers.size();++n){next[(fxLayers+n)*4]=binding.surface.layers[n].snowPorosity;next[(fxLayers+n)*4+1]=binding.surface.layers[n].flow.foam;}
      const auto writeRawFlowMap=[&](resources::TerrainSurfaceMap map,float *out){map.channel=resources::TerrainSurfaceChannel::RGB;map.linear=true;return writeMap(map,false,out);};
      usize entry=3;
      if(streams){
        float *directory=next.data()+(fxDirectory+entry++)*4;directory[0]=4;directory[1]=1;directory[2]=float(absoluteBase+streamRow);directory[3]=11;
        float *out=next.data()+streamRow*4;out[0]=float(1+(flow.heightDampStreams?2:0)+(flow.perTexFoam?4:0));out[1]=flow.streamMaximum;out[2]=flow.streamBlend;
        std::copy(flow.streamFlowParams.begin(),flow.streamFlowParams.end(),out+4);
        std::copy(flow.streamNormalFoam.begin(),flow.streamNormalFoam.end(),out+8);
        std::copy(flow.streamUvScales.begin(),flow.streamUvScales.end(),out+12);
        std::copy(flow.streamTint.begin(),flow.streamTint.end(),out+16);
        std::copy(flow.streamFades.begin(),flow.streamFades.end(),out+20);
        if(!writeRawFlowMap(flow.streamNormal,out+24))return false;
      }
      if(lava){
        float *directory=next.data()+(fxDirectory+entry++)*4;directory[0]=5;directory[1]=1;directory[2]=float(absoluteBase+lavaRow);directory[3]=14;
        float *out=next.data()+lavaRow*4;out[0]=float(1+(flow.heightDampLava?2:0)+(flow.lavaStochastic?4:0));out[1]=flow.lavaStochasticContrast;out[2]=flow.lavaStochasticSize;out[3]=flow.lavaEmissiveMultiplier;
        std::copy(flow.lavaParams.begin(),flow.lavaParams.end(),out+4);std::copy(flow.lavaParams2.begin(),flow.lavaParams2.end(),out+8);
        std::copy(flow.lavaEdgeColor.begin(),flow.lavaEdgeColor.end(),out+12);std::copy(flow.lavaColorLow.begin(),flow.lavaColorLow.end(),out+16);std::copy(flow.lavaColorHighlight.begin(),flow.lavaColorHighlight.end(),out+20);
        std::copy(flow.lavaUvScale.begin(),flow.lavaUvScale.end(),out+24);std::copy(flow.lavaFades.begin(),flow.lavaFades.end(),out+28);out[32]=flow.lavaDisplacementScale;
        if(!writeRawFlowMap(flow.lavaDiffuse,out+36))return false;
      }
      if(rain){
        float *directory=next.data()+(fxDirectory+entry++)*4;directory[0]=6;directory[1]=1;directory[2]=float(absoluteBase+rainRow);directory[3]=7;
        float *out=next.data()+rainRow*4;out[0]=1;std::copy(flow.rainIntensityScale.begin(),flow.rainIntensityScale.end(),out+4);
        if(!writeRawFlowMap(flow.rainDropTexture,out+8))return false;
      }
      if(specularFade){
        float *directory=next.data()+(fxDirectory+entry++)*4;directory[0]=7;directory[1]=1;directory[2]=float(absoluteBase+specularRow);directory[3]=2;
        float *out=next.data()+specularRow*4;out[0]=1;std::copy(flow.specularFades.begin(),flow.specularFades.end(),out+4);
      }
    }
    for(usize i=0;i<binding.surface.layers.size();++i){const auto &layer=binding.surface.layers[i];float *out=next.data()+(moduleLayers+i*ModuleLayerRows)*4;
      out[0]=layer.geoParticipation;out[1]=layer.geoHeightFilterEnabled?1.f:0.f;out[2]=layer.geoHeightThreshold;out[3]=layer.geoHeightStrength;
      std::copy(layer.globalMapParticipation.begin(),layer.globalMapParticipation.end(),out+4);
    }
    if(geo){const auto &g=binding.surface.geo;float *out=next.data()+geoRow*4;
      out[0]=float(g.mode);out[1]=float(g.blend);out[2]=g.blendStrength;out[3]=g.normalStrength;
      out[4]=g.scaleMeters;out[5]=g.offsetMeters;out[6]=g.rangeEnabled?1.f:0.f;out[7]=g.curveEnabled?1.f:0.f;
      std::copy(g.range.begin(),g.range.end(),out+8);
      out[12]=g.curveScaleMeters;out[13]=g.curveOffsetMeters;out[14]=g.curveRotationDegrees;out[15]=g.heightContrast;
      if(!writeMap(g.albedo,false,out+16)||!writeMap(g.normal,true,out+36))return false;
    }
    if(globals)for(usize role=0;role<GlobalRecords;++role){const auto &g=binding.surface.globalMaps[role];float *out=next.data()+(globalRow+role*GlobalRecordRows)*4;
      out[0]=g.enabled?1.f:0.f;out[1]=float(role);out[2]=g.intensity;out[3]=float(g.normalBlend);
      out[4]=g.fadeStart;out[5]=g.fadeEnd;out[6]=g.nearStrength;out[7]=g.farStrength;
      out[8]=g.scaleX;out[9]=g.scaleY;out[10]=g.offsetX;out[11]=g.offsetY;out[12]=float(g.blend);
      if(!writeMap(g.map,role==1,out+16)||!writeMap(g.smoothness,false,out+36)||!writeMap(g.ao,false,out+56)||!writeMap(g.metallic,false,out+76)||!writeMap(g.mask,false,out+96))return false;
    }
    if(curve)for(usize i=0;i<GeoCurveSamples;++i){resources::AnimationCurveSample sample;
      if((!binding.surface.geo.curve.keys.empty()&&!resources::sampleValidatedAnimationCurve(binding.surface.geo.curve,double(i)/double(GeoCurveSamples-1),sample))||!std::isfinite(sample.value)||std::abs(sample.value)>1e8){error="Geo curve bake exceeds finite GPU value range.";return false;}
      next[curveRow*4+i]=float(sample.value);
    }
    if(snow){const auto &s=binding.surface.snow;float *out=next.data()+snowRow*4;
      u32 flags=(s.normalNoiseEnabled?1:0)|(s.distanceResampleEnabled?2:0)|(s.perLayerEnabled?4:0)|(s.maskEnabled?8:0)|(s.worldSpaceUV?16:0)|(s.stochasticEnabled?32:0)|(s.triplanarEnabled?64:0)|(s.useGlobalLevel?128:0)|(s.useGlobalHeight?256:0)|(s.sssEnabled?512:0)|(s.rimEnabled?1024:0)|(s.sparkleEnabled?2048:0);
      out[0]=1;out[1]=float(s.mode);out[2]=float(flags);out[3]=float(absoluteBase+rows);
      const auto vector=[&](usize row,const auto &v){std::copy(v.begin(),v.end(),out+row*4);};
      vector(1,s.upVector);vector(2,s.tint);out[12]=s.amount;out[13]=s.stochasticScale;out[14]=s.stochasticContrast;out[15]=s.rimPower;
      vector(4,s.uvScales);vector(5,s.heightAngleRange);vector(6,s.parameters);vector(7,s.normalNoiseParams);vector(8,s.distanceResample);
      vector(9,s.sssTint);vector(10,s.rimColor);vector(11,s.sparkleTint);
      out[48]=s.sparkleEmission;out[49]=s.sparkleStrength;out[50]=s.sparkleSize;out[51]=s.sparkleDensity;
      out[52]=s.sparkleNoiseDensity;out[53]=s.sparkleNoiseAmplitude;out[54]=s.sparkleViewDependency;out[55]=float(absoluteBase+snowLayers);
      const resources::TerrainSurfaceMap *maps[]={&s.diffuse,&s.height,&s.normal,&s.smoothness,&s.ao,&s.normalNoiseMap,&s.mask,&s.sparkleNoiseMap};
      for(usize i=0;i<SnowMaps;++i)if(!writeMap(*maps[i],i==2||i==5,out+(SnowHeaderRows+i*TerrainSurfaceMapRows)*4,i==6))return false;
      for(usize i=0;i<binding.surface.layers.size();++i){next[(snowLayers+i)*4]=binding.surface.layers[i].snowParticipation;next[(snowLayers+i)*4+1]=binding.surface.layers[i].snowPorosity;}
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
    if(rows!=TerrainSurfaceHeaderRows+usize(part[0])*TerrainSurfaceLayerRows||part[5]!=0)return fail();
    error.clear();return true;
  }
  const usize advanced=usize(part[2])-1;
  const usize count=usize(part[0]);
  if(advanced!=TerrainSurfaceHeaderRows+count*TerrainSurfaceLayerRows || advanced+AdvancedHeaderRows>rows)return fail();
  float *header=part.data()+advanced*4;
  if(!integer(header[7])||header[7]<1||header[7]>FeatureM05+FeatureM06+FeatureAntiTile+FeatureM07+FeatureM08+FeatureGeo+FeatureGlobals+FeatureSnow+FeatureStaticFx)return fail();
  const u32 features=u32(header[7]);const bool m06=(features&FeatureM06)!=0,antiTile=(features&FeatureAntiTile)!=0,m07=(features&FeatureM07)!=0,m08=(features&FeatureM08)!=0,geo=(features&FeatureGeo)!=0,globals=(features&FeatureGlobals)!=0;
  const bool snow=(features&FeatureSnow)!=0,fx=(features&FeatureStaticFx)!=0,modules=geo||globals||snow||fx;
  if(!(features&FeatureM05)||(antiTile&&!m06))return fail();
  const usize extension=advanced+GlobalRows+count*LayerRows;
  const usize antiDirectory=extension+(m06?count*LayerExtraRows:0);
  if(!integer(header[5])||header[5]>32)return fail();
  const usize entries=usize(header[5]);
  const usize resample=antiDirectory+(antiTile?AntiDirectoryRows+entries*AntiEntryRows:0);
  const usize clusterStart=resample+(m07?count*ResampleRows:0);
  if(header[0]!=TerrainSurfaceAdvancedVersion || header[1]!=part[0] || header[2]!=float(advanced+AdvancedHeaderRows) ||
     header[3]!=float(advanced+GlobalRows) || header[4]!=(antiTile?float(antiDirectory):0.f) ||
     (!antiTile&&entries!=0) || header[6]!=float(rows) || ((m08||modules)?rows<clusterStart:rows!=clusterStart))return fail();
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
  if(modules){
    const usize moduleRoot=clusterCursor,moduleLayers=moduleRoot+ModuleRows+(fx?1:0),geoRow=moduleLayers+count*ModuleLayerRows,globalRow=geoRow+(geo?GeoRows:0),curveRow=globalRow+(globals?GlobalRecords*GlobalRecordRows:0);
    if(moduleRoot+ModuleRows+(fx?1:0)>rows||part[5]!=float(moduleRoot+1)||curveRow>rows)return fail();
    const float *root=part.data()+moduleRoot*4;
    const bool curve=root[6]!=0;
    const usize snowRow=curveRow+(curve?GeoCurveRows:0),snowLayers=snowRow+SnowRows;
    usize entries=3;bool streams=false,lava=false,rain=false,specularFade=false;
    if(fx){
      if(!integer(root[13])||root[13]<3||root[13]>7)return fail();entries=usize(root[13]);
      const usize directory=snowRow+(snow?SnowRows+count:0);
      if(directory+entries>rows)return fail();
      float previous=3;for(usize n=3;n<entries;++n){const float type=part[(directory+n)*4];if(type<=previous)return fail();previous=type;if(type==4)streams=true;else if(type==5)lava=true;else if(type==6)rain=true;else if(type==7)specularFade=true;else return fail();}
      if(entries!=3+(streams?1:0)+(lava?1:0)+(rain?1:0)+(specularFade?1:0))return fail();
    }
    if(root[0]!=(fx?(rain||specularFade?5.f:streams||lava?4.f:3.f):(snow?2.f:1.f))||root[1]!=(geo?float(geoRow):0.f)||root[2]!=(globals?float(globalRow):0.f)||root[3]!=float(moduleLayers)||root[4]!=float(rows)||
      root[5]!=(curve?float(curveRow):0.f)||root[6]!=(curve?float(GeoCurveSamples):0.f)||root[7]!=(snow?float(snowRow+1):0.f)||rows!=snowRow+(snow?SnowRows+count:0)+(fx?entries+8+count+(streams?11:0)+(lava?14:0)+(rain?7:0)+(specularFade?2:0):0)||(!geo&&curve))return fail();
    if(snow){const float *s=part.data()+snowRow*4;if(s[0]!=1||!integer(s[1])||s[1]<1||s[1]>2||!integer(s[2])||s[2]>4095||s[3]!=float(rows)||s[55]!=float(snowLayers))return fail();
      pointers.push_back(moduleRoot*4+7);pointers.push_back(snowRow*4+3);pointers.push_back(snowRow*4+55);}
    if(fx){
      const usize directory=snowRow+(snow?SnowRows+count:0),snapshot=directory+entries,wet=snapshot+3,puddle=wet+4,controls=puddle+1,streamRow=controls+count,lavaRow=streamRow+(streams?11:0),rainRow=lavaRow+(lava?14:0),specularRow=rainRow+(rain?7:0);
      if(root[12]!=float(directory+1)||root[13]!=float(entries)||root[14]!=1||root[15]!=float(rows))return fail();
      pointers.push_back(moduleRoot*4+12);pointers.push_back(moduleRoot*4+15);
      for(usize n=0;n<3;++n){const float *entry=part.data()+(directory+n)*4;
        if(entry[0]!=float(n+1)||entry[1]!=1||entry[2]!=float(n==0?snapshot:n==1?wet:puddle)||entry[3]!=float(n==0?3:n==1?4:1))return fail();
        pointers.push_back((directory+n)*4+2);
      }
      const float *record=part.data()+snapshot*4;
      if(!integer(record[3])||record[3]>7||(u32(record[3])&6)!=u32((streams?2:0)+(lava?4:0))||record[8]!=float(controls)||record[9]!=float(count)||record[10]!=0||record[11]!=float(rows))return fail();
      const bool snapshotProvided=(u32(record[3])&1)!=0;
      if(snapshotProvided){if(!integer(record[0])||record[0]>=16777216||!integer(record[1])||record[1]<1||record[1]>2048||!integer(record[2])||record[2]<1||record[2]>2048)return fail();}
      else if(record[0]!=-1||record[1]!=0||record[2]!=0||streams||lava)return fail();
      for(usize n=0;n<4;++n)if(record[4+n]!=root[8+n])return fail();
      for(usize n=0;n<16;++n)if(!std::isfinite(part[wet*4+n]))return fail();
      const float *w=part.data()+wet*4,*p=part.data()+puddle*4;
      for(usize n=0;n<3;++n)if(w[n]!=0&&w[n]!=1)return fail();
      if(w[3]<0||w[3]>1||(w[0]==1&&(w[4]<0||w[5]<w[4]||w[5]>1||(w[2]==1&&(w[14]<=0||w[13]<0||w[13]>1||w[15]<0||w[15]>1))))||!integer(p[0])||p[0]>1||!std::isfinite(p[1])||(p[0]==1&&p[1]<0)||!std::isfinite(p[2])||(p[0]==1&&(p[2]<0||p[2]>1))||!integer(p[3])||p[3]>3)return fail();
      if(!snapshotProvided&&(w[0]!=0||p[0]!=0||(u32(p[3])&2)!=0))return fail();
      if(rain&&p[0]!=1)return fail();
      for(usize n=0;n<count;++n){const float *layer=part.data()+(controls+n)*4;if(!std::isfinite(layer[0])||layer[0]<0||layer[0]>1||!std::isfinite(layer[1])||layer[1]<0||layer[1]>2||layer[2]!=0||layer[3]!=0)return fail();}
      usize entry=3;
      const auto typedRecord=[&](usize type,usize start,usize length,usize mapOffset){
        const float *d=part.data()+(directory+entry)*4;
        if(d[0]!=float(type)||d[1]!=1||d[2]!=float(start)||d[3]!=float(length))return false;
        for(usize n=0;n<length*4;++n)if(!std::isfinite(part[start*4+n]))return false;
        const float *r=part.data()+start*4;
        if(!integer(r[0])||r[0]<1||r[0]>7||(u32(r[0])&1)==0||!integer(r[mapOffset*4])||r[mapOffset*4]>=16777216||r[mapOffset*4+1]!=float(resources::TerrainSurfaceChannel::RGB))return false;
        if(type==4){if(r[1]<0||r[1]>1||r[2]<0||r[2]>65536||r[3]!=0)return false;}
        else if(type==5){if(r[1]<0||r[1]>64||r[2]<.0001f||r[2]>65536||r[3]<0||r[3]>65536)return false;}
        else if(type==6){if(r[0]!=1||r[1]!=0||r[2]!=0||r[3]!=0||r[4]<0||r[4]>1)return false;}
        pointers.push_back((directory+entry)*4+2);++entry;return true;
      };
      if(streams&&!typedRecord(4,streamRow,11,6))return fail();
      if(lava&&!typedRecord(5,lavaRow,14,9))return fail();
      if(rain&&!typedRecord(6,rainRow,7,2))return fail();
      if(specularFade){
        const float *d=part.data()+(directory+entry)*4,*r=part.data()+specularRow*4;
        if(d[0]!=7||d[1]!=1||d[2]!=float(specularRow)||d[3]!=2||r[0]!=1||r[1]!=0||r[2]!=0||r[3]!=0)return fail();
        for(usize n=4;n<8;++n)if(!std::isfinite(r[n]))return fail();if(r[5]<r[4])return fail();
        pointers.push_back((directory+entry)*4+2);++entry;
      }
      pointers.push_back(snapshot*4+8);pointers.push_back(snapshot*4+11);
    }
    if(geo&&((part[geoRow*4+7]>.5)!=curve))return fail();
    for(usize i=0;i<4;++i)if(!std::isfinite(root[8+i])||std::abs(root[8+i])>1e8f)return fail();
    if(root[10]<=0||root[11]<=0)return fail();
    pointers.push_back(5);pointers.push_back(moduleRoot*4+3);pointers.push_back(moduleRoot*4+4);
    if(geo)pointers.push_back(moduleRoot*4+1);if(globals)pointers.push_back(moduleRoot*4+2);if(curve)pointers.push_back(moduleRoot*4+5);
  }else if(clusterCursor!=rows||part[5]!=0)return fail();
  // No mutation until every version, extent and pointer has been checked.
  for(usize pointer:pointers)part[pointer]+=float(destinationRow);
  error.clear();return true;
}

}
