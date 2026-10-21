#include "harness.h"
#include "editor/editor_session.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_textures.generated.h"
#include "resources/gltf_import.h"
#include "renderer/terrain_surface_texture_relocation.h"
#include "core/sha256.h"
#include "convex_bake_fixture.h"
#include <set>
using namespace ae;
namespace {
renderer::TerrainSurfaceBinding sourceBinding(){renderer::TerrainSurfaceBinding b;b.textures.push_back({{1,1},true,false,0});b.textures.push_back({{1,2},true,false,1});b.staticFx.enabled=true;b.staticFx.textureIndex=2;return b;}
std::vector<u8> actualFxGlb(u8 red,std::string &error){
 resources::TerrainRecipe r;r.cells=8;r.controlCells=8;r.height=0;resources::TerrainSurfaceLayer layer;layer.identity={711,1};layer.name="Actual source layer";r.surface.layers={layer};r.surface.flow.wetness=true;
 r.fx.enabled=true;r.fx.width=r.fx.height=5;r.fx.seedPixels.resize(100);for(usize n=0;n<100;n+=4){r.fx.seedPixels[n]=red;r.fx.seedPixels[n+1]=102;r.fx.seedPixels[n+2]=153;r.fx.seedPixels[n+3]=204;}
 resources::TerrainBuild build;std::vector<u8> bytes;
 const std::array<std::vector<u8>,4> legacy{{{std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},{std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},{std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},{std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
 if(!resources::updateTerrain(r,nullptr,build,error)||!resources::writeTerrainGlb(build,legacy,{},bytes,error))return {};return bytes;
}
}
AE_TEST(terrain_static_fx_flat_dedup_mapping_is_atomic_and_not_a_descriptor_offset){
 auto b=sourceBinding();std::string e;const std::array<u32,4> remap{2,0,3,1};
 AE_EXPECT_TRUE(renderer::remapTerrainSurfaceFlatTextureIndices(b,remap,e),e.c_str());AE_EXPECT_EQ(b.textures[0].textureIndex,2u,"PBR flat0->dedup2");AE_EXPECT_EQ(b.textures[1].textureIndex,0u,"PBR flat1->dedup0");AE_EXPECT_EQ(b.staticFx.textureIndex,3u,"StaticFX flat2->dedup3 exactly once");
 AE_EXPECT_TRUE(renderer::offsetTerrainSurfaceSourceTextureIndices(b,7,4,e),e.c_str());AE_EXPECT_EQ(b.staticFx.textureIndex,10u,"Android package prefix is applied to dedup index3, independently of table rows or descriptors");
}
AE_TEST(terrain_static_fx_flat_dedup_rejects_invalid_reference_without_partial_mutation){
 auto b=sourceBinding();std::string e;const std::array<u32,3> missing{2,1,renderer::InvalidMapTexture};
 AE_EXPECT_TRUE(!renderer::remapTerrainSurfaceFlatTextureIndices(b,missing,e),"invalid StaticFX dedup destination refused");AE_EXPECT_EQ(b.textures[0].textureIndex,0u,"earlier PBR index not mutated before validating FX");AE_EXPECT_EQ(b.staticFx.textureIndex,2u,"FX original source index retained");
 b.staticFx.enabled=false;b.staticFx.textureIndex=renderer::InvalidMapTexture;const std::array<u32,2> valid{1,0};AE_EXPECT_TRUE(renderer::remapTerrainSurfaceFlatTextureIndices(b,valid,e),e.c_str());AE_EXPECT_EQ(b.staticFx.textureIndex,renderer::InvalidMapTexture,"disabled placeholder stays absent");
}
AE_TEST(terrain_static_fx_two_actual_glb_sources_prefix_then_nonidentity_dedup_and_rollback){
 const auto project=std::filesystem::temp_directory_path()/("astra-fx-remap-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(project);
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{project};
 std::string e;const auto first=actualFxGlb(51,e),second=actualFxGlb(85,e),invalidBytes=actualFxGlb(119,e);AE_EXPECT_TRUE(!first.empty()&&!second.empty()&&!invalidBytes.empty(),e.c_str());
 resources::GltfImport source1,source2,bad;AE_EXPECT_TRUE(resources::importGlb(first,{},{},source1),source1.diagnostic.c_str());AE_EXPECT_TRUE(resources::importGlb(second,{},{},source2),source2.diagnostic.c_str());AE_EXPECT_TRUE(resources::importGlb(invalidBytes,{},{},bad),bad.diagnostic.c_str());
 const auto &local2=source2.terrainSurfaces.front().staticFx;const auto secondHash=local2.rgbaSha256;const auto expectedFlat2=u32(source1.textures.size())+local2.textureIndex;
 editor::EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(project.generic_string().c_str()),"real project");test::ConvexCpuLibrary library;AE_EXPECT_TRUE(library.connect(session),"actual CPU geometry publication consumer");
 std::set<std::string> observedHashes;bool sawNonidentityDedup=false;u32 accepted=0;
 session.setGeometryPublisher([&](auto vertices,auto indices,auto draws,auto materials,std::span<const renderer::SharedAuthoringTexture> textures,auto &published){
  for(const auto &surface:session.terrainSurfacePublication())if(surface.staticFx.enabled){const auto &fx=surface.staticFx;
   if(fx.textureIndex>=textures.size()||!textures[fx.textureIndex]||Sha256::hex(textures[fx.textureIndex]->mipChain)!=fx.rgbaSha256)return false;
   observedHashes.insert(fx.rgbaSha256);
   if(fx.rgbaSha256==secondHash)sawNonidentityDedup=fx.textureIndex!=local2.textureIndex&&fx.textureIndex!=expectedFlat2&&textures.size()<source1.textures.size()+source2.textures.size();
  }
  if(!library.publish(vertices,indices,draws,materials,published))return false;++accepted;return true;
 });
 editor::EditorSession::ModelImportReport report;
 AE_EXPECT_TRUE(session.importModel(first,"first.glb",{},report),report.diagnostic.c_str());
 AE_EXPECT_TRUE(session.importModel(second,"second.glb",{},report),"real second source must publish its own StaticFX bytes, not first-source local slot");
 AE_EXPECT_TRUE(sawNonidentityDedup&&observedHashes.size()==2&&accepted>=2,"actual shared original textures force a nonidentity flat->dedup mapping; both distinct mask hashes reach real publisher");
 const auto previousDraws=library.draws.size();const auto previousAccepted=accepted;
 for(auto &surface:bad.terrainSurfaces)if(surface.staticFx.enabled)surface.staticFx.textureIndex=renderer::InvalidMapTexture;
 AE_EXPECT_TRUE(!session.commitModelImport(invalidBytes,bad,"bad.glb","",report),"invalid third source refuses before any partial package can reach renderer");
 AE_EXPECT_EQ(library.draws.size(),previousDraws,"last good geometry package retained after invalid source");AE_EXPECT_TRUE(accepted<=previousAccepted+1,"only optional explicit old-package rollback may republish; invalid package never accepted");
}

#include "renderer/terrain_used_layers.h"
AE_TEST(terrain_used_channels_keep_sparse_control_pixels_and_closed_side_colors) {
  std::vector<u8> vertices(renderer::MapVertexStride*3);vertices[44]=255;
  vertices[renderer::MapVertexStride+46]=1; // closed side only, blue
  std::vector<u32> indices{0,1,2};renderer::MapDrawRecord draw{};draw.indexCount=3;
  auto map=std::make_shared<renderer::AuthoringTexture>();map->srgb=false;map->width=2;map->height=2;map->levels=1;
  map->mipChain.resize(16);map->mipChain[15]=1; // even 1/255 alpha at the last pixel is retained
  std::vector<renderer::SharedAuthoringTexture> textures{map};
  renderer::TerrainControlBinding control;control.textureIndex=0;
  const std::span<const renderer::MapDrawRecord> draws(&draw,1);
  const std::span<const renderer::TerrainControlBinding> controls(&control,1);
  AE_EXPECT_EQ(renderer::terrainUsedLayerChannels(0,vertices,indices,draws,controls,textures),13u,"R/B/A all affect shading");
  map->mipChain[1]=1;
  AE_EXPECT_EQ(renderer::terrainUsedLayerChannels(0,vertices,indices,draws,controls,textures),15u,"painting green restores its layer");
  map->format=renderer::AuthoringTextureAstc4x4;
  AE_EXPECT_EQ(renderer::terrainUsedLayerChannels(0,vertices,indices,draws,controls,textures),15u,"unknown compressed contents cannot be pruned");
  map->format=renderer::AuthoringTextureRgba8;indices[2]=999;
  AE_EXPECT_EQ(renderer::terrainUsedLayerChannels(0,vertices,indices,draws,controls,textures),15u,"invalid geometry disables the optimization");
}
