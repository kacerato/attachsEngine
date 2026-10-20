#include "harness.h"
#include "resources/terrain_surface_asset.h"
#include "resources/animation_curve_domain.h"
#include "resources/terrain_surface_texture_refs.h"
#include "renderer/terrain_surface_table.h"
#include <cmath>
#include <limits>
#include <fstream>
#include <iterator>
using namespace ae::resources;
namespace {
TerrainSurfaceAsset moduleSurface(){
  TerrainSurfaceAsset a;TerrainSurfaceLayer layer;layer.identity={41,1};a.layers.push_back(layer);
  a.geo.mode=TerrainGeoMode::AlbedoNormal;a.geo.albedo.texture={41,101};a.geo.normal.texture={41,102};
  a.geo.rangeEnabled=true;a.geo.range={-20,-10,25,40};a.geo.curveEnabled=true;
  a.geo.curve.keys.clear();
  for(ae::u32 i=0;i<40;++i){AnimationCurveKey key;key.id=i+1;key.time=float(i)/39;key.value=key.time*key.time;
    key.incoming=key.outgoing=AnimationTangentMode::Free;key.inSlope=key.outSlope=2*key.time;
    key.weightedIn=key.weightedOut=true;key.inWeight=.21f;key.outWeight=.43f;a.geo.curve.keys.push_back(key);}
  a.layers[0].geoHeightFilterEnabled=true;a.layers[0].geoHeightThreshold=-.4f;a.layers[0].geoHeightStrength=.7f;
  a.layers[0].globalMapParticipation={.5f,2,1.5f,2};
  for(auto &g:a.globalMaps)g.enabled=true;
  a.globalMaps[0].map.texture={41,101};a.globalMaps[1].map.texture={41,102};a.globalMaps[3].map.texture={41,104};
  auto &saom=a.globalMaps[2];saom.smoothness.texture=saom.ao.texture=saom.metallic.texture=saom.mask.texture={41,103};
  return a;
}
ae::renderer::TerrainSurfaceBinding moduleBinding(const TerrainSurfaceAsset &a){
  ae::renderer::TerrainSurfaceBinding binding;binding.surface=a;binding.terrainDomain={-120,-80,240,160};
  for(const auto &ref:terrainSurfaceTextureRefs(a))binding.textures.push_back({ref.identity,ref.linear,ref.normal,ae::u32(binding.textures.size())});
  return binding;
}
}
AE_TEST(terrain_modules_v2_weighted_curve_and_full_layer_roundtrip){
  auto a=moduleSurface();std::string text,error;
  AE_EXPECT_TRUE(serializeTerrainSurface(a,text,error),error.c_str());
  AE_EXPECT_TRUE(text.starts_with("AETERRAINSURFACE2 "),"new writer declares version two");
  TerrainSurfaceAsset b;AE_EXPECT_TRUE(deserializeTerrainSurface(text,b,error),error.c_str());
  AE_EXPECT_EQ(b.geo.curve.keys.size(),size_t(40),"curve authoring exceeds legacy vector limit without truncation");
  AE_EXPECT_EQ(b.geo.curve.keys[31].outWeight,a.geo.curve.keys[31].outWeight,"weighted handles survive persistence");
  AE_EXPECT_EQ(b.geo.curve.keys[31].outgoing,AnimationTangentMode::Free,"tangent mode retained");
  AE_EXPECT_EQ(b.layers[0].globalMapParticipation,a.layers[0].globalMapParticipation,"participation above one retained");
  AE_EXPECT_EQ(b.globalMaps[2].mask.channel,TerrainSurfaceChannel::A,"SAOM mask is independent alpha channel");
  std::string again;AE_EXPECT_TRUE(serializeTerrainSurface(b,again,error),error.c_str());AE_EXPECT_EQ(again,text,"canonical v2 roundtrip");
  auto before=again;text.resize(text.size()-8);AE_EXPECT_TRUE(!deserializeTerrainSurface(text,b,error),"truncated extension rejects atomically");
  AE_EXPECT_TRUE(serializeTerrainSurface(b,again,error),error.c_str());AE_EXPECT_EQ(again,before,"failed load retains full applied surface");
}
AE_TEST(terrain_modules_refs_lut_domain_and_atomic_relocation){
  using namespace ae::renderer;auto a=moduleSurface();auto binding=moduleBinding(a);std::vector<ae::u32> slots(binding.textures.size());
  for(ae::u32 i=0;i<slots.size();++i)slots[i]=i+7;
  std::vector<float> canonical;ae::u32 offset=0;std::string error;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  const ae::u32 root=ae::u32(canonical[5])-1;
  AE_EXPECT_EQ(canonical[root*4+8],-120.f,"explicit domain origin transported");AE_EXPECT_EQ(canonical[root*4+10],240.f,"nondefault domain extent transported");
  const ae::u32 lut=ae::u32(canonical[root*4+5]);
  AE_EXPECT_EQ(canonical[root*4+6],256.f,"bounded deterministic bake consumes the full authoring curve");
  AE_EXPECT_EQ(canonical[lut*4],0.f,"curve first endpoint");AE_EXPECT_EQ(canonical[lut*4+255],1.f,"curve last endpoint");
  AnimationCurveSample expected;AE_EXPECT_TRUE(sampleAnimationCurve(a.geo.curve,128.0/255,expected),"shared evaluator valid");
  AE_EXPECT_TRUE(std::abs(canonical[lut*4+128]-float(expected.value))<1e-6f,"table LUT uses weighted shared evaluator");
  auto relocated=canonical;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,13,error),error.c_str());
  std::vector<float> direct(52,0);AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,direct,offset,error),error.c_str());
  AE_EXPECT_EQ(relocated,std::vector<float>(direct.begin()+52,direct.end()),"all module and LUT pointers relocate as direct append");
  auto bad=canonical;bad[root*4]=99;auto saved=bad;AE_EXPECT_TRUE(!relocateTerrainSurfaceTable(bad,13,error),"unknown module directory version rejected");AE_EXPECT_EQ(bad,saved,"relocation failure has no partial writes");
  auto missing=binding;missing.textures.pop_back();auto savedTable=canonical;
  AE_EXPECT_TRUE(!appendTerrainSurfaceTable(missing,slots,canonical,offset,error),"missing global descriptor rejects entire publication");AE_EXPECT_EQ(canonical,savedTable,"failed publication retains previous table");
  std::vector<TerrainSurfaceDiagnostic> diagnostics;
  AE_EXPECT_TRUE(!resolveTerrainSurface(a,[](AssetGuid guid){return guid.low!=103;},diagnostics,error),"all packed references participate in project validation");
  AE_EXPECT_EQ(diagnostics.size(),size_t(4),"four SAOM channel references remain explicit despite GPU sample reuse");
}
AE_TEST(terrain_modules_strict_ranges_and_curve_identity){
  auto a=moduleSurface();std::string error;AE_EXPECT_TRUE(validTerrainSurface(a,error),error.c_str());
  a.layers[0].globalMapParticipation[1]=2.01f;AE_EXPECT_TRUE(!validTerrainSurface(a,error),"source normal participation max two");
  a=moduleSurface();a.geo.range[2]=-30;AE_EXPECT_TRUE(!validTerrainSurface(a,error),"unordered four-height range rejected");
  a=moduleSurface();a.geo.curve.keys[2].id=a.geo.curve.keys[1].id;AE_EXPECT_TRUE(!validTerrainSurface(a,error),"curve IDs cannot silently alias");
  a=moduleSurface();a.geo.curve.keys.back().time=a.geo.curve.keys[38].time;AE_EXPECT_TRUE(!validTerrainSurface(a,error),"duplicate Geo key times reject atomically");
  a=moduleSurface();a.geo.blend=TerrainColorBlend::Overlay;AE_EXPECT_TRUE(!validTerrainSurface(a,error),"Geo exposes only its two real source modes");
}
AE_TEST(terrain_modules_and_legacy_features_share_one_relocatable_table){
  using namespace ae::renderer;
  auto surface=moduleSurface();auto &layer=surface.layers[0];
  surface.detailNoise.enabled=true;surface.detailNoise.map=surface.geo.albedo;
  surface.normalNoise[2].enabled=true;surface.normalNoise[2].map=surface.geo.normal;
  TerrainAntiTileEntry entry;entry.identity={41,900};entry.normal=surface.normalNoise[2];
  surface.antiTileArray.enabled=true;surface.antiTileArray.entries={entry};layer.antiTileEntry=entry.identity;
  layer.resampleAlbedo=layer.resampleNormal=layer.resampleHeightBlend=true;layer.resampleScale=2;
  layer.cluster.enabled=layer.cluster.stochastic=true;
  for(ae::u32 i=0;i<2;++i) {
    TerrainSurfaceVariant variant;variant.identity={41,1000+i};
    variant.maps[0]=i?surface.globalMaps[3].map:surface.geo.albedo;
    variant.maps[1]=surface.geo.normal;variant.scale=float(i+1);
    layer.cluster.variants.push_back(variant);
  }
  layer.cluster.selection.enabled=true;layer.cluster.selection.map=surface.geo.albedo;
  const auto binding=moduleBinding(surface);std::vector<ae::u32> slots(binding.textures.size());
  for(ae::u32 i=0;i<slots.size();++i)slots[i]=i;
  std::vector<float> canonical;ae::u32 offset=0;std::string error;
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,canonical,offset,error),error.c_str());
  const auto advanced=ae::usize(canonical[2])-1;
  AE_EXPECT_EQ(canonical[(advanced+1)*4+3],127.f,"M05 through M10 share one feature directory");
  auto relocated=canonical;
  AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,31,error),error.c_str());
  std::vector<float> appended(31*4,0);
  AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,appended,offset,error),error.c_str());
  AE_EXPECT_EQ(relocated,std::vector<float>(appended.begin()+31*4,appended.end()),
               "cluster, resampling, anti-tile, Geo, globals and LUT relocate together exactly");
}
AE_TEST(terrain_modules_signed_and_zero_uv_scales_roundtrip){
  auto a=moduleSurface();a.globalMaps[0].scaleX=-2;a.globalMaps[0].scaleY=0;a.globalMaps[1].scaleX=a.globalMaps[1].scaleY=0;
  std::string text,error;AE_EXPECT_TRUE(validTerrainSurface(a,error),error.c_str());
  AE_EXPECT_TRUE(serializeTerrainSurface(a,text,error),error.c_str());TerrainSurfaceAsset b;AE_EXPECT_TRUE(deserializeTerrainSurface(text,b,error),error.c_str());
  AE_EXPECT_EQ(b.globalMaps[0].scaleX,-2.f,"global mirroring remains signed in persistence");AE_EXPECT_EQ(b.globalMaps[0].scaleY,0.f,"zero UV scale remains an authored constant coordinate");
  AE_EXPECT_EQ(b.globalMaps[1].scaleX,0.f,"degenerate normal UV is retained for runtime frame fallback");
}
AE_TEST(terrain_modules_historical_v1_fixture_migrates_without_loss){
  // Produced by the actual 2750745c serializer, not by stripping the new v2
  // writer. Fixture generator provenance lives in fixtures/README-terrain-v1.md.
  std::ifstream fixture("tests/native/fixtures/terrain-surface-v1.txt",std::ios::binary);
  std::string text{std::istreambuf_iterator<char>(fixture),std::istreambuf_iterator<char>()},error;
  AE_EXPECT_TRUE(text.starts_with("AETERRAINSURFACE1 "),"actual historic v1 fixture is present");
  TerrainSurfaceAsset a;AE_EXPECT_TRUE(deserializeTerrainSurface(text,a,error),error.c_str());
  AE_EXPECT_EQ(a.geo.mode,TerrainGeoMode::Off,"v1 initializes Geo disabled");AE_EXPECT_TRUE(!a.geo.curveEnabled,"v1 has neutral curve consumer");
  for(auto &g:a.globalMaps)AE_EXPECT_TRUE(!g.enabled,"all new global roles initialize disabled");
  AE_EXPECT_EQ(a.layers.size(),size_t(1),"historic layer count preserved");AE_EXPECT_EQ(a.layers[0].identity,(AssetGuid{7,11}),"historic stable identity preserved");
  AE_EXPECT_EQ(a.layers[0].scaleX,3.f,"old tile X metres retained");AE_EXPECT_EQ(a.layers[0].scaleY,5.f,"old tile Y metres retained");
  AE_EXPECT_EQ(a.layers[0].offsetX,-2.f,"old offset retained");AE_EXPECT_EQ(a.layers[0].rotation,17.f,"old rotation retained");
  AE_EXPECT_EQ(a.layers[0].detailNoise.scale,2.f,"old M05 noise parameters retained");AE_EXPECT_EQ(a.layers[0].globalDetailParticipation,.3f,"old participation retained");
  AE_EXPECT_EQ(a.layers[0].globalMapParticipation,(std::array<float,4>{1,1,1,1}),"new layer influences initialize neutrally");
  std::string v2,again;AE_EXPECT_TRUE(serializeTerrainSurface(a,v2,error),error.c_str());AE_EXPECT_TRUE(v2.starts_with("AETERRAINSURFACE2 "),"migration writes the new declared version");
  TerrainSurfaceAsset b;AE_EXPECT_TRUE(deserializeTerrainSurface(v2,b,error),error.c_str());AE_EXPECT_TRUE(serializeTerrainSurface(b,again,error),error.c_str());
  AE_EXPECT_EQ(v2,again,"v1 to v2 cold roundtrip is canonical without dropped old fields");
}

namespace {
AnimationCurve signedWeightedGeoCurve(){
  AnimationCurve curve;AnimationCurveKey a,b;a.id=1;b.id=2;a.time=-2;b.time=3;a.value=-1;b.value=1;
  a.incoming=a.outgoing=b.incoming=b.outgoing=AnimationTangentMode::Free;a.broken=b.broken=true;
  a.inSlope=a.outSlope=2;b.inSlope=b.outSlope=.5f;a.weightedOut=b.weightedIn=true;a.outWeight=.2f;b.inWeight=.4f;
  curve.keys={a,b};return curve;
}
std::string signedCurveSnapshot(const AnimationCurve &curve){auto a=moduleSurface();a.geo.curve=curve;std::string text,error;serializeTerrainSurface(a,text,error);return text;}
}
AE_TEST(terrain_geo_signed_domain_preserves_general_policy_and_weighted_curve){
  const auto signedDomain=AnimationCurveDomain::Signed;auto curve=signedWeightedGeoCurve();
  AE_EXPECT_TRUE(validAnimationCurveDomain(curve,signedDomain),"signed Geo keys retain original domain");
  AE_EXPECT_TRUE(!validAnimationCurve(curve),"default animation validation still rejects negative time");
  AE_EXPECT_TRUE(!validAnimationCurveDomain(curve,AnimationCurveDomain::NonNegative),"explicit legacy domain matches default");
  // Known Bezier parameter u=.6 gives X=.664 and Y=.44 from these weighted
  // control points. This checks weighted time, not a second evaluator oracle.
  AnimationCurveSample sample;AE_EXPECT_TRUE(sampleValidatedAnimationCurve(curve,.664,sample),"signed curve can be sampled inside the visible LUT interval");
  AE_EXPECT_TRUE(std::abs(sample.value-.44)<1e-6,"outside weighted handles influence the visible curve without clamping key times");
  auto original=signedCurveSnapshot(curve);ae::u64 frontier=3,result=91;
  AE_EXPECT_TRUE(!splitAnimationCurve(curve,-.25f,frontier,result),"default split keeps its nonnegative policy");
  AE_EXPECT_EQ(signedCurveSnapshot(curve),original,"legacy split rejection retains every weighted key");AE_EXPECT_EQ(frontier,ae::u64(3),"failed legacy split retains frontier");AE_EXPECT_EQ(result,ae::u64(91),"failed legacy split retains output id");
  std::array<double,21> before{};for(size_t i=0;i<before.size();++i){AE_EXPECT_TRUE(sampleValidatedAnimationCurve(curve,-2+5*double(i)/20,sample),"before-split samples valid");before[i]=sample.value;}
  AE_EXPECT_TRUE(splitAnimationCurveDomain(curve,-.25f,frontier,result,signedDomain),"signed De Casteljau split accepts negative interior time");
  AE_EXPECT_EQ(result,ae::u64(3),"split allocates stable key identity");AE_EXPECT_EQ(frontier,ae::u64(4),"split advances frontier once");
  for(size_t i=0;i<before.size();++i){AE_EXPECT_TRUE(sampleValidatedAnimationCurve(curve,-2+5*double(i)/20,sample),"after-split samples valid");AE_EXPECT_TRUE(std::abs(sample.value-before[i])<2e-6,"negative weighted split preserves both complete segments");}
  auto saved=signedCurveSnapshot(curve);result=99;AE_EXPECT_TRUE(!splitAnimationCurveDomain(curve,std::numeric_limits<float>::quiet_NaN(),frontier,result,signedDomain),"nonfinite split rejected");
  AE_EXPECT_EQ(signedCurveSnapshot(curve),saved,"failed signed split is atomic");AE_EXPECT_EQ(frontier,ae::u64(4),"failed signed split retains frontier");AE_EXPECT_EQ(result,ae::u64(99),"failed signed split retains output");
  AE_EXPECT_TRUE(splitAnimationCurveDomain(curve,-.25f,frontier,result,signedDomain),"existing-time split retains identity");AE_EXPECT_EQ(result,ae::u64(3),"existing key resolved by time");AE_EXPECT_EQ(frontier,ae::u64(4),"existing-time split allocates nothing");
}
AE_TEST(terrain_geo_signed_editing_ids_steps_and_empty_policy){
  const auto domain=AnimationCurveDomain::Signed;AnimationCurve curve;AnimationCurveKey key;key.time=-3;key.value=.25f;ae::u64 frontier=1,result=0;
  AE_EXPECT_TRUE(!validAnimationCurveDomain(curve,domain),"generic domain validator intentionally excludes empty curves");
  AE_EXPECT_TRUE(putAnimationCurveKeyDomain(curve,key,frontier,result,domain),"signed authoring inserts the first key");AE_EXPECT_EQ(result,ae::u64(1),"first identity allocated");
  key.value=.5f;AE_EXPECT_TRUE(putAnimationCurveKeyDomain(curve,key,frontier,result,domain),"same-time insertion updates existing identity");AE_EXPECT_EQ(frontier,ae::u64(2),"same-time insertion does not consume IDs");
  auto before=signedCurveSnapshot(curve);key.id=99;AE_EXPECT_TRUE(!putAnimationCurveKeyDomain(curve,key,frontier,result,domain),"unknown explicit identity rejected");AE_EXPECT_EQ(signedCurveSnapshot(curve),before,"unknown identity is atomic");
  key.id=0;key.time=2;key.value=1;AE_EXPECT_TRUE(putAnimationCurveKeyDomain(curve,key,frontier,result,domain),"keys beyond one remain authored");
  curve.keys[0].incoming=curve.keys[0].outgoing=curve.keys[1].incoming=curve.keys[1].outgoing=AnimationTangentMode::Constant;
  AE_EXPECT_TRUE(splitAnimationCurveDomain(curve,-1,frontier,result,domain),"negative step segment can be split");AnimationCurveSample sample;
  AE_EXPECT_TRUE(sampleValidatedAnimationCurve(curve,-.5,sample)&&sample.value==.5,"signed constant split preserves hold value");
  const ae::u64 missing=999;before=signedCurveSnapshot(curve);AE_EXPECT_TRUE(!eraseAnimationCurveKeysDomain(curve,std::span<const ae::u64>(&missing,1),domain),"missing deletion rejected");AE_EXPECT_EQ(signedCurveSnapshot(curve),before,"failed deletion retains curve");
  std::vector<ae::u64> ids;for(const auto &k:curve.keys)ids.push_back(k.id);AE_EXPECT_TRUE(eraseAnimationCurveKeysDomain(curve,ids,domain),"Geo can erase the last remaining key");AE_EXPECT_TRUE(curve.keys.empty(),"deletion explicitly produces empty authoring");
  auto geo=moduleSurface();geo.geo.curve=curve;std::string text,error;AE_EXPECT_TRUE(serializeTerrainSurface(geo,text,error),"Geo alone defines empty curve as constant zero");
  auto duplicate=signedWeightedGeoCurve();duplicate.keys[1].id=duplicate.keys[0].id;AE_EXPECT_TRUE(!validAnimationCurveDomain(duplicate,domain),"signed boundary rejects duplicate stable IDs");
  key={};key.time=-1;frontier=std::numeric_limits<ae::u64>::max();result=17;AE_EXPECT_TRUE(!putAnimationCurveKeyDomain(curve,key,frontier,result,domain),"exhausted ID frontier rejected");AE_EXPECT_TRUE(curve.keys.empty()&&result==17,"frontier failure retains empty state and result");
}
AE_TEST(terrain_geo_arbitrary_domain_empty_single_roundtrip_and_lut){
  using namespace ae::renderer;std::array<AnimationCurve,4> curves;curves[2]=signedWeightedGeoCurve();
  AnimationCurveKey key;key.id=77;key.time=-4;key.value=.25f;curves[1].keys={key};key.time=2;key.value=-.125f;curves[3].keys={key};
  for(const auto &curve:curves){auto surface=moduleSurface();surface.geo.curve=curve;std::string text,error;
    AE_EXPECT_TRUE(serializeTerrainSurface(surface,text,error),error.c_str());TerrainSurfaceAsset restored;AE_EXPECT_TRUE(deserializeTerrainSurface(text,restored,error),error.c_str());
    AE_EXPECT_EQ(restored.geo.curve.keys.size(),curve.keys.size(),"all original keys persist without endpoint injection or trimming");
    if(!curve.keys.empty())AE_EXPECT_EQ(restored.geo.curve.keys.front().time,curve.keys.front().time,"signed/outside key time survives cold load");
    auto binding=moduleBinding(restored);std::vector<ae::u32> slots(binding.textures.size());for(ae::u32 i=0;i<slots.size();++i)slots[i]=i;
    std::vector<float> table;ae::u32 offset=0;AE_EXPECT_TRUE(appendTerrainSurfaceTable(binding,slots,table,offset,error),error.c_str());
    const ae::u32 root=ae::u32(table[5])-1,lut=ae::u32(table[root*4+5]);
    for(ae::u32 sampleIndex:{0u,64u,128u,255u}){AnimationCurveSample expected;if(!curve.keys.empty())AE_EXPECT_TRUE(sampleValidatedAnimationCurve(curve,double(sampleIndex)/255,expected),"signed/constant evaluator valid");AE_EXPECT_TRUE(std::abs(table[lut*4+sampleIndex]-float(expected.value))<1e-6,"inline LUT uses all original keys; empty is explicitly zero");}
    auto relocated=table;AE_EXPECT_TRUE(relocateTerrainSurfaceTable(relocated,9,error),error.c_str());AE_EXPECT_EQ(relocated[5],table[5]+9,"domain changes do not disturb module relocation ABI");
  }
}
