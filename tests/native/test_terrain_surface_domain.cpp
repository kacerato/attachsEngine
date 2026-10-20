#include "harness.h"
#include "resources/terrain_asset.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/terrain_textures.generated.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using namespace ae;
AE_TEST(terrain_global_domain_survives_glb_cache_independently_of_texture_tiling) {
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=8;
  recipe.size=120;recipe.height=0;recipe.textureScale=3;
  resources::TerrainSurfaceLayer layer;layer.identity=resources::assetGuidFromSeed("terrain-domain");
  layer.name="Domain";recipe.surface.layers={layer};
  std::string error;resources::TerrainBuild build;
  AE_EXPECT_TRUE(resources::updateTerrain(recipe,nullptr,build,error),error.c_str());
  std::vector<u8> bytes;
  const std::array<std::vector<u8>,4> legacy{{
    {std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},
    {std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},
    {std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},
    {std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
  AE_EXPECT_TRUE(resources::writeTerrainGlb(build,legacy, {},bytes,error),error.c_str());
  resources::GltfImport imported;
  AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},imported),imported.diagnostic.c_str());
  AE_EXPECT_TRUE(!imported.terrainSurfaces.empty(),"real GLB consumer has a surface");
  const std::array<float,4> expected{-60,-60,120,120};
  AE_EXPECT_TRUE(imported.terrainSurfaces[0].terrainDomain==expected,"full domain derives size, not textureScale");
  AE_EXPECT_EQ(imported.terrainSurfaces[0].localMetersPerUv,3.f,"UV reconstruction remains independent");
  auto key=resources::importCacheKey("terrain-domain",{});std::vector<u8> cache;
  AE_EXPECT_TRUE(resources::writeImportCache(imported,key,cache),"domain cache writes");
  resources::GltfImport cold;
  AE_EXPECT_TRUE(resources::readImportCache(cache,key,cold),"cold cache reads");
  AE_EXPECT_TRUE(cold.terrainSurfaces[0].terrainDomain==expected,"domain survives cold cache");
  // Two exact float origins that collide under six-decimal std::to_string
  // must not reuse each other's binding when importing one real GLB.
  u32 jsonLength=0;std::memcpy(&jsonLength,bytes.data()+12,4);
  std::string json(reinterpret_cast<const char*>(bytes.data()+20),jsonLength);
  const auto array=json.find("\"materials\":[");
  AE_EXPECT_TRUE(array!=std::string::npos,"generated GLB materials");
  const auto begin=json.find('{',array);usize end=begin;unsigned depth=0;bool quoted=false,escaped=false;
  for(;end<json.size();++end) {
    const char ch=json[end];
    if(quoted) {if(escaped)escaped=false;else if(ch=='\\')escaped=true;else if(ch=='\"')quoted=false;continue;}
    if(ch=='\"')quoted=true;
    else if(ch=='{')++depth;
    else if(ch=='}' && --depth==0){++end;break;}
  }
  AE_EXPECT_TRUE(end>begin && end<json.size(),"complete material object");
  std::string first=json.substr(begin,end-begin),second=first;
  const auto domainPosition=first.find("\"terrainDomain\":[-60");
  AE_EXPECT_TRUE(domainPosition!=std::string::npos,"material has explicit domain");
  const auto originPosition=domainPosition+std::string("\"terrainDomain\":[").size();
  first.replace(originPosition,3,"0.0000001");second.replace(originPosition,3,"0.0000002");
  json.replace(begin,end-begin,first+","+second);while(json.size()%4)json+=' ';
  std::vector<u8> doubled(bytes.begin(),bytes.begin()+20);
  doubled.insert(doubled.end(),json.begin(),json.end());
  doubled.insert(doubled.end(),bytes.begin()+20+jsonLength,bytes.end());
  const u32 newTotal=u32(doubled.size()),newJsonLength=u32(json.size());
  std::memcpy(doubled.data()+8,&newTotal,4);std::memcpy(doubled.data()+12,&newJsonLength,4);
  resources::GltfImport two;
  AE_EXPECT_TRUE(resources::importGlb(doubled,{},{},two),two.diagnostic.c_str());
  AE_EXPECT_TRUE(two.terrainSurfaces.size()>=2,"two surface material bindings imported");
  AE_EXPECT_EQ(two.terrainSurfaces[0].terrainDomain[0],.0000001f,"first exact origin retained");
  AE_EXPECT_EQ(two.terrainSurfaces[1].terrainDomain[0],.0000002f,"second tiny origin has its own canonical binding");
  auto invalid=imported;invalid.terrainSurfaces[0].terrainDomain[2]=0;
  auto protectedBytes=cache;
  AE_EXPECT_TRUE(!resources::writeImportCache(invalid,key,protectedBytes),"zero extent cannot publish");
  AE_EXPECT_TRUE(protectedBytes.empty(),"invalid derived cache is refused without partial bytes");
  invalid=imported;invalid.terrainSurfaces[0].terrainDomain[0]=std::numeric_limits<float>::infinity();
  AE_EXPECT_TRUE(!resources::writeImportCache(invalid,key,protectedBytes),"nonfinite origin cannot publish");
  // Mutate only same-length JSON numbers: GLB chunk offsets and binary payload
  // stay valid, so refusal exercises domain metadata rather than GLB framing.
  const std::string metadata="\"terrainDomain\":[-60,-60,120,120]";
  auto position=std::search(bytes.begin(),bytes.end(),metadata.begin(),metadata.end());
  AE_EXPECT_TRUE(position!=bytes.end(),"explicit domain metadata exists");
  const auto offset=usize(position-bytes.begin());
  auto bad=bytes;bad[offset+metadata.find("120")]='-';
  resources::GltfImport rejected;
  AE_EXPECT_TRUE(!resources::importGlb(bad,{},{},rejected),"negative domain extent rejected by real import");
  AE_EXPECT_TRUE(rejected.diagnostic.find("extent")!=std::string::npos,"specific domain diagnostic");
}

// Import-codec coverage belongs to the full terrain/import targets, not the
// minimal catalog target.
#include "resources/terrain_surface_catalog.h"
#include "resources/terrain_surface_import.h"
#include <fstream>
#include <iterator>
AE_TEST(terrain_original_geo_pair_imports_color_and_normal_role_views) {
  using namespace resources;
  std::ifstream catalogFile("assets/terrain/library/catalog.json",std::ios::binary);
  std::string catalogJson{std::istreambuf_iterator<char>(catalogFile),std::istreambuf_iterator<char>()};
  TerrainSurfaceCatalog catalog;std::string error;
  AE_EXPECT_TRUE(loadTerrainSurfaceCatalog(catalogJson,catalog,error),error.c_str());
  const char *guids[]{"863f19b183b2443859f959a96f7dce32","192aaea3237044f608d663c9884e0c70"};
  std::vector<TerrainSurfaceImage> images;
  TerrainSurfaceAsset surface;TerrainSurfaceLayer layer;
  layer.identity=assetGuidFromSeed("geo-original-pair-test");surface.layers={layer};
  surface.geo.mode=TerrainGeoMode::AlbedoNormal;
  for(size_t i=0;i<2;++i) {
    const TerrainCatalogTexture *found=nullptr;
    for(const auto &texture:catalog.textures)if(texture.identity.text()==guids[i])found=&texture;
    AE_EXPECT_TRUE(found,"supplied Geo GUID exists in real catalog");
    std::ifstream file("assets/terrain/library/"+found->runtimePath,std::ios::binary);
    std::vector<u8> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    AE_EXPECT_TRUE(!bytes.empty(),"original PNG readable");
    images.emplace_back(found->identity,std::move(bytes));
    (i==0?surface.geo.albedo:surface.geo.normal).texture=found->identity;
  }
  std::vector<renderer::SharedAuthoringTexture> textures;
  renderer::TerrainSurfaceBinding binding;
  AE_EXPECT_TRUE(importTerrainSurface(surface,images,1,1ull<<20,textures,binding,error),error.c_str());
  AE_EXPECT_EQ(binding.textures.size(),size_t(2),"only explicit Geo pair imported");
  for(const auto &ref:binding.textures) {
    const bool normal=ref.identity==surface.geo.normal.texture;
    AE_EXPECT_EQ(ref.normal,normal,"normal filtering follows Geo role");
    AE_EXPECT_EQ(ref.linear,normal,"Geo color sRGB and normal linear");
    AE_EXPECT_TRUE(ref.textureIndex<textures.size(),"valid imported texture index");
    AE_EXPECT_EQ(textures[ref.textureIndex]->srgb,!normal,"actual decoded role view");
  }
}

AE_TEST(terrain_surface_v3_glb_requires_explicit_domain) {
  resources::TerrainRecipe recipe;
  recipe.cells=8;recipe.controlCells=8;recipe.size=120;
  recipe.height=0;recipe.textureScale=3;
  resources::TerrainSurfaceLayer layer;
  layer.identity=resources::assetGuidFromSeed("v3-missing-domain-regression");
  layer.name="V3 domain";recipe.surface.layers={layer};
  std::string serialized,error;
  AE_EXPECT_TRUE(resources::serializeTerrainSurface(recipe.surface,serialized,error),error.c_str());
  AE_EXPECT_TRUE(serialized.starts_with("AETERRAINSURFACE3 "),"regression exercises V3, not only V2");
  resources::TerrainBuild build;
  AE_EXPECT_TRUE(resources::updateTerrain(recipe,nullptr,build,error),error.c_str());
  const std::array<std::vector<u8>,4> legacy{{
    {std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},
    {std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},
    {std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},
    {std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(build,legacy,{},bytes,error),error.c_str());
  resources::GltfImport baseline;
  AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},baseline),baseline.diagnostic.c_str());
  AE_EXPECT_TRUE(!baseline.terrainSurfaces.empty(),"baseline has an actual surface consumer");
  const std::array<float,4> expected{-60,-60,120,120};
  AE_EXPECT_TRUE(baseline.terrainSurfaces[0].terrainDomain==expected,"baseline retains nondefault domain");
  // Rename only the metadata key with equal-length valid JSON. No GLB framing,
  // offsets, binary payload, schema or image references are changed.
  const std::string key="\"terrainDomain\"",unknown="\"missingDomain\"";
  AE_EXPECT_EQ(key.size(),unknown.size(),"same-length JSON mutation");
  const auto position=std::search(bytes.begin(),bytes.end(),key.begin(),key.end());
  AE_EXPECT_TRUE(position!=bytes.end(),"explicit domain key exists");
  std::copy(unknown.begin(),unknown.end(),position);
  resources::GltfImport rejected;
  AE_EXPECT_TRUE(!resources::importGlb(bytes,{},{},rejected),"V3 cannot silently fall back to default domain");
  AE_EXPECT_TRUE(rejected.diagnostic.find("requires its full domain")!=std::string::npos,"failure is the missing-domain contract, not malformed GLB");
}
