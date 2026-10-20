#include "harness.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_textures.generated.h"
#include "core/sha256.h"
#include <cmath>
#include <fstream>
#include <iterator>
using namespace ae::resources;
namespace {
TerrainRecipe recipe(){TerrainRecipe r;r.cells=8;r.controlCells=8;r.height=0;r.fx.width=r.fx.height=5;return r;}
TerrainFxStroke stroke(){TerrainFxStroke s;s.identity=1;s.radiusMeters=1;s.hardness=1;s.flow=1;s.sampleSeconds=1;return s;}
std::string meshHash(const TerrainMesh &mesh){std::string bytes;for(const auto &part:mesh.parts)bytes.append(reinterpret_cast<const char*>(part.vertices.data()),part.vertices.size()*sizeof(TerrainVertex));return ae::Sha256::hex(std::span<const ae::u8>(reinterpret_cast<const ae::u8*>(bytes.data()),bytes.size()));}
}
AE_TEST(terrain_fx_recipe9_real_v8_fixture_loads_neutral_fx_without_losing_authoring){
  std::ifstream file("tests/native/fixtures/terrain-recipe-v8.txt",std::ios::binary);std::string text{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()},error;
  AE_EXPECT_TRUE(text.starts_with("AETERRAIN8 "),"actual frozen V8 writer fixture, never a relabelled V9");TerrainRecipe r;
  AE_EXPECT_TRUE(deserializeTerrainRecipe(text,r,error),error.c_str());AE_EXPECT_TRUE(!r.fx.enabled&&r.fx.seedPixels.empty()&&r.fx.strokes.empty(),"historical recipe acquires no FX effect or painting");
  AE_EXPECT_EQ(r.size,120.f,"historical size survives neutral FX domain default");AE_EXPECT_EQ(r.controlCells,128u,"PBR control resolution remains independent");AE_EXPECT_EQ(r.strokes[0].gesture,9u,"legacy sculpt history survives");AE_EXPECT_EQ(r.paths[0].name,std::string("Historic path"),"legacy path remains");
  AE_EXPECT_EQ(r.surface.layers[0].snowPorosity,.2f,"Snow canonical porosity retained");AE_EXPECT_EQ(r.surface.snow.amount,.7f,"M11 Snow amount retained");
  auto v9=serializeTerrainRecipe(r);AE_EXPECT_TRUE(v9.starts_with("AETERRAIN9 "),"writer declares current version");TerrainRecipe cold;
  AE_EXPECT_TRUE(deserializeTerrainRecipe(v9,cold,error),error.c_str());AE_EXPECT_EQ(serializeTerrainRecipe(cold),v9,"V8 toV9 cold authoring is canonical");
}
AE_TEST(terrain_fx_only_update_reuses_geometry_collision_and_pbr_exactly){
  auto r=recipe();TerrainBuild base,next;std::string error;
  AE_EXPECT_TRUE(updateTerrain(r,nullptr,base,error),error.c_str());const auto hash=meshHash(base.mesh),key=terrainRecipeNonFxKey(r);
  r.fx.enabled=true;r.fx.strokes={stroke()};r.fx.strokes[0].channel=TerrainFxChannel::Puddle;
  AE_EXPECT_EQ(terrainRecipeNonFxKey(r),key,"FX never contaminates geometry/collision/PBR compatibility key");
  AE_EXPECT_TRUE(updateTerrain(r,&base,next,error),error.c_str());AE_EXPECT_EQ(meshHash(next.mesh),hash,"all vertex positions/normals/UV/PBR weight bytes reused");
  AE_EXPECT_TRUE(next.field.heights==base.field.heights&&next.field.density==base.field.density&&next.field.holes==base.field.holes&&next.field.controlWeights==base.field.controlWeights,"collision and normalized PBR authoring data unchanged");
  AE_EXPECT_TRUE(next.dirty.empty()&&next.mesh.rebuiltChunks==0&&next.mesh.reusedChunks==next.mesh.chunks,"FX-only bypasses field generation and meshing");
  AE_EXPECT_EQ(next.fxField.values[12][1],1.f,"only the independent Puddle channel is painted");AE_EXPECT_TRUE(!next.fxDirty.empty(),"FX upload has its own dirty region");
  TerrainFxLimits limits;limits.maximumWorkingBytes=499;
  AE_EXPECT_TRUE(!updateTerrainWithFxFrame(r,&base,next,error,{}, {},limits),"captured FX working budget reaches update producer before field allocation");
  AE_EXPECT_EQ(next.fxField.values[12][1],1.f,"budget failure preserves published FX field");
  limits.maximumWorkingBytes=256ull*1048576;
  AE_EXPECT_TRUE(updateTerrainWithFxFrame(r,&base,next,error,{}, {},limits),"explicit larger UI budget is accepted by same producer API");
  auto before=next.fxField.values;AE_EXPECT_TRUE(!updateTerrain(r,&next,base,error,[](float){return false;}),"stale/cancelled FX-only update rejected before publication");AE_EXPECT_TRUE(next.fxField.values==before,"previous publication remains immutable");
}
AE_TEST(terrain_fx_recipe9_persists_full_1024_and_2048_bitmaps_without_surface_hex_limit){
  for(ae::u32 dimension:{1024u,2048u}){
    auto r=recipe();r.fx.enabled=true;r.fx.width=r.fx.height=dimension;r.fx.sourceImage={7,dimension};r.fx.seedPixels.resize(ae::usize(dimension)*dimension*4);
    for(ae::usize i=0;i<r.fx.seedPixels.size();++i)r.fx.seedPixels[i]=ae::u8((i*37+11)&255);
    const auto text=serializeTerrainRecipe(r);AE_EXPECT_TRUE(!text.empty()&&text.size()<TerrainRecipeMaximumBytes,"legal bitmap has explicit recipe quota and no truncation");
    if(dimension==2048)AE_EXPECT_TRUE(text.size()>32ull*1048576,"2048² linear bitmap persists beyond old8MiB and surface2MiB limits");
    TerrainRecipe cold;std::string error;AE_EXPECT_TRUE(deserializeTerrainRecipe(text,cold,error),error.c_str());
    AE_EXPECT_TRUE(cold.fx.seedPixels==r.fx.seedPixels&&cold.fx.width==dimension&&cold.fx.height==dimension,"every source texel and independent resolution survives cold load");
  }
}
AE_TEST(terrain_fx_recipe9_length_quota_truncation_and_multidigit_versions_are_safe){
  auto r=recipe();r.fx.enabled=true;r.fx.strokes={stroke()};const auto text=serializeTerrainRecipe(r);std::string fx,error;
  AE_EXPECT_TRUE(serializeTerrainFxAsset(r.fx,fx,error),error.c_str());const auto marker=" 1 "+std::to_string(fx.size())+" "+fx;
  const auto at=text.rfind(marker);AE_EXPECT_TRUE(at!=std::string::npos,"FX payload uses version+length, not a surface hex helper");
  std::vector<std::string> invalid{text.substr(0,text.size()-1),text+" extra",text};invalid[2].replace(at,marker.size()," 1 18446744073709551615 x");
  for(const auto version:{"AETERRAIN10","AETERRAIN09","AETERRAIN9x","AETERRAIN42949672960"}){auto bad=text;bad.replace(0,10,version);invalid.push_back(std::move(bad));}
  TerrainRecipe output=r;output.seed=999;
  for(const auto &bad:invalid){AE_EXPECT_TRUE(!deserializeTerrainRecipe(bad,output,error),"truncated/extra/oversize-length and unknown multi-digit versions rejected");AE_EXPECT_EQ(output.seed,999u,"reader is transactional before any allocation/publication");}
  auto incompatible=r;incompatible.size=120;AE_EXPECT_TRUE(!validTerrainRecipe(incompatible,error),"size change requires explicit FX-domain rebase");
}
AE_TEST(terrain_fx_world_slope_producer_obeys_inverse_transpose_and_is_atomic){
  TerrainField field;field.width=field.depth=3;field.height=1;field.step=1;field.heights={-1,0,1,-1,0,1,-1,0,1};
  TerrainFxAsset fx;fx.width=fx.height=1;TerrainFxSlopeFrame frame;std::vector<float> slope;std::string error;
  AE_EXPECT_TRUE(makeTerrainFxSlopeField(field,fx,frame,slope,error),error.c_str());AE_EXPECT_TRUE(std::abs(slope[0]-(1-1/std::sqrt(2.f)))<1e-6,"world identity produces inverse normalized slope, not degrees");
  frame.normalToWorld={.5,0,0,0,1,0,0,0,1};AE_EXPECT_TRUE(makeTerrainFxSlopeField(field,fx,frame,slope,error),error.c_str());AE_EXPECT_TRUE(std::abs(slope[0]-(1-1/std::sqrt(1.25f)))<1e-6,"nonuniform inverse-transpose changes actual world normal");
  frame.normalToWorld={0,1,0,-1,0,0,0,0,1};AE_EXPECT_TRUE(makeTerrainFxSlopeField(field,fx,frame,slope,error),error.c_str());AE_EXPECT_EQ(slope[0],1.f,"rotation against worldup reaches inverse-slope maximum");
  auto before=slope;frame.normalToWorld={};AE_EXPECT_TRUE(!makeTerrainFxSlopeField(field,fx,frame,slope,error),"singular world transform is diagnosed");AE_EXPECT_TRUE(slope==before,"failed slope job cannot replace captured producer");
}
AE_TEST(terrain_fx_recipe9_existing_glb_path_escapes_and_restores_quotes_and_controls){
  auto r=recipe();r.fx.enabled=true;r.fx.strokes={stroke()};r.fx.sourcePropertiesJson="{\"quoted\":\"line\\path\"}\nraw\tcontrols";
  TerrainBuild build;std::string error;AE_EXPECT_TRUE(updateTerrain(r,nullptr,build,error),error.c_str());
  const std::array<std::vector<ae::u8>,4> textures{{{std::begin(terrainTexture0),std::end(terrainTexture0)},{std::begin(terrainTexture1),std::end(terrainTexture1)},{std::begin(terrainTexture2),std::end(terrainTexture2)},{std::begin(terrainTexture3),std::end(terrainTexture3)}}};
  std::vector<ae::u8> glb;AE_EXPECT_TRUE(writeTerrainGlb(build,textures,glb,error),error.c_str());TerrainRecipe cold;
  AE_EXPECT_TRUE(terrainRecipeFromGlb(glb,cold,error),error.c_str());AE_EXPECT_EQ(cold.fx.sourcePropertiesJson,r.fx.sourcePropertiesJson,"GLB string JSON escapes do not terminate or alter inline FX recipe");AE_EXPECT_EQ(serializeTerrainRecipe(cold),serializeTerrainRecipe(r),"existing GLB recipe path retains sole authored FX owner");
}
AE_TEST(terrain_fx_recipe9_full_2048_bitmap_survives_glb_recipe_without_second_authority){
  auto r=recipe();r.fx.enabled=true;r.fx.width=r.fx.height=2048;r.fx.sourceImage={9,2048};r.fx.seedPixels.assign(2048ull*2048*4,37);
  for(ae::usize i=0;i<r.fx.seedPixels.size();++i)r.fx.seedPixels[i]=ae::u8((i*37+11)&255); // worst-case raw fallback
  r.fx.seedPixels.front()=0;r.fx.seedPixels.back()=255;
  auto geometry=recipe();TerrainMesh mesh;std::string error;AE_EXPECT_TRUE(generateTerrain(geometry,mesh,error),error.c_str());
  const std::array<std::vector<ae::u8>,4> textures{{{std::begin(terrainTexture0),std::end(terrainTexture0)},{std::begin(terrainTexture1),std::end(terrainTexture1)},{std::begin(terrainTexture2),std::end(terrainTexture2)},{std::begin(terrainTexture3),std::end(terrainTexture3)}}};
  std::vector<ae::u8> glb;AE_EXPECT_TRUE(writeTerrainGlb(r,mesh,textures,glb,error),error.c_str());AE_EXPECT_TRUE(glb.size()>32ull*1048576,"GLB contains every authored FX texel, not a silent reduced mask");
  TerrainRecipe cold;AE_EXPECT_TRUE(terrainRecipeFromGlb(glb,cold,error),error.c_str());AE_EXPECT_TRUE(cold.fx.seedPixels==r.fx.seedPixels&&cold.fx.width==2048&&cold.fx.height==2048,"large legal FX recipe reloads exact source through existing GLB path");
}
