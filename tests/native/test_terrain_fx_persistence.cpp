#include "harness.h"
#include "editor/editor_terrain_recovery.h"
#include "editor/editor_archive.h"
#include "scene/import_link.h"
#include <chrono>
#include <fstream>
using namespace ae;
namespace {
struct Project {
  std::filesystem::path root=std::filesystem::temp_directory_path()/("astra-fx-persistence-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Project(){std::filesystem::create_directories(root);}
  ~Project(){std::error_code ec;std::filesystem::remove_all(root,ec);}
  std::string name()const{const auto text=root.u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};}
};
resources::TerrainRecipe fxRecipe(u32 size){resources::TerrainRecipe r;r.cells=8;r.height=0;r.fx.enabled=true;r.fx.width=r.fx.height=size;r.fx.sourceImage={91,size};r.fx.seedPixels.resize(usize(size)*size*4);
  for(usize i=0;i<r.fx.seedPixels.size();++i)r.fx.seedPixels[i]=u8((i*37+11)&255);return r;}
std::string hash(std::string_view text){return Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(text.data()),text.size()));}
std::string readText(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
usize files(const std::filesystem::path &path){usize count=0;for(const auto &entry:std::filesystem::directory_iterator(path))if(entry.is_regular_file())++count;return count;}
}
AE_TEST(terrain_fx_2048_noise_cold_project_scene_and_recovery_keep_single_authored_payload){
  Project project;auto recipe=fxRecipe(2048);std::string error;const auto revision=hash("lost source GLB revision2048");
  AE_EXPECT_TRUE(editor::writeTerrainRecovery(project.name(),revision,recipe,{},error),error.c_str());
  resources::AssetRegistry registry;resources::AssetRecord record;record.guid=resources::assetGuidFromSeed("terrain-fx-scene-source");record.path="Terrenos/source.glb";record.contentHash=revision;
  AE_EXPECT_TRUE(registry.add(record),"project registry stores source identity/hash, not a second bitmap");
  const auto registryPath=project.root/".astra"/"assets.registry";
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(registryPath,registry.serialize(),&error),error.c_str());
  editor::EditorDocument document;const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Terrain FX");auto value=*document.find(id);
  auto *link=static_cast<scene::ImportLink*>(value.components.add(scene::ImportLink::descriptor));AE_EXPECT_TRUE(link,"typed source link available");
  link->source=record.guid;link->instance={11,12};link->root=true;link->unlinked=true;link->baseName="Terrain FX";
  AE_EXPECT_TRUE(document.applyEntityValues(id,value),"scene source link applied");
  const auto scenePath=project.root/"main.aescene";AE_EXPECT_TRUE(editor::saveEditorDocument(scenePath.string().c_str(),document,44),"atomic scene save contains only references");
  AE_EXPECT_TRUE(std::filesystem::file_size(registryPath)<4096&&std::filesystem::file_size(scenePath)<8192,"legal2048 mask does not inflate scene/registry with duplicate bitmap");
  resources::AssetRegistry coldRegistry;AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(readText(registryPath),coldRegistry),"cold asset registry load");
  editor::EditorDocument coldScene;AE_EXPECT_TRUE(editor::loadEditorDocument(scenePath.string().c_str(),44,coldScene),"cold scene load");
  const auto *coldLink=scene::importLink(coldScene.find(id)->components);AE_EXPECT_TRUE(coldLink&&coldLink->source==record.guid,"scene source GUID survives");const auto *coldRecord=coldRegistry.find(coldLink->source);AE_EXPECT_TRUE(coldRecord,"source resolves through project registry");
  resources::TerrainRecipe cold;bool recovered=false;
  AE_EXPECT_TRUE(editor::readTerrainAuthoring(project.name(),coldRecord->path,coldRecord->contentHash,cold,error,recovered),error.c_str());
  AE_EXPECT_TRUE(recovered&&cold.fx.width==2048&&cold.fx.height==2048&&cold.fx.seedPixels==recipe.fx.seedPixels,"lost GLB fallback restores all worst-case raw pixels with no downres");
  const auto broken=project.root/"broken.aescene";AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(broken,"AETHER_EDITOR",&error),error.c_str());
  AE_EXPECT_TRUE(!editor::loadEditorDocument(broken.string().c_str(),44,coldScene),"truncated scene cannot publish partial object graph");AE_EXPECT_TRUE(scene::importLink(coldScene.find(id)->components)->source==record.guid,"scene source remains intact after failed load");
}
AE_TEST(terrain_fx_history_delta_checkpoint_gc_and_recover_preserve_exact_bytes){
  Project project;resources::TerrainHistoryStore store(project.root/"objects");auto recipe=fxRecipe(256);const auto a=resources::serializeTerrainRecipe(recipe);
  recipe.fx.seedPixels[111]^=1;const auto b=resources::serializeTerrainRecipe(recipe);std::string first,second,same,orphan,loaded;
  AE_EXPECT_TRUE(store.put(a,{},first)&&store.put(a,first,same),"put identical authoring revision");AE_EXPECT_EQ(same,first,"unchanged authoring never creates a duplicate bitmap or delta");AE_EXPECT_EQ(files(project.root/"objects"),usize(1),"single physical checkpoint for identical revisions");
  AE_EXPECT_TRUE(store.put(b,first,second),"one pixel edit stores exact legacy-compatible delta");AE_EXPECT_TRUE(std::filesystem::file_size(project.root/"objects"/(second+".aethist"))<1024,"small pixel mutation does not duplicate unchanged raw bitmap");
  AE_EXPECT_TRUE(store.read(second,loaded)&&loaded==b,"cold delta restores exact Recipe9 raw fallback bytes");AE_EXPECT_TRUE(store.put("unreferenced",{},orphan),"create GC candidate");
  usize removed=0;AE_EXPECT_TRUE(store.collect({second},{first},removed)&&removed==1,"scene/undo roots retain chain; only unreferenced object collected");AE_EXPECT_TRUE(store.read(first,loaded)&&loaded==a,"protected undo root remains readable");
  const auto owned=project.root/"objects"/(orphan+".aethist.aether-tmp"),unrelated=project.root/"objects"/"user.tmp";
  {std::ofstream out(owned);out<<"interrupted";}{std::ofstream out(unrelated);out<<"user";}
  AE_EXPECT_TRUE(store.recover(removed)&&removed==1&&std::filesystem::exists(unrelated),"recovery removes only owned interrupted objects");
  {std::ofstream out(project.root/"objects"/(first+".aethist"),std::ios::trunc);out<<"ATH1F";}
  loaded="protected";AE_EXPECT_TRUE(!store.read(second,loaded)&&loaded=="protected","truncated ancestor never yields partial recipe");
  AE_EXPECT_TRUE(!store.collect({second},{},removed)&&std::filesystem::exists(project.root/"objects"/(second+".aethist")),"GC validates all roots before any sweep deletion");
}
AE_TEST(terrain_fx_recovery_preflight_errors_do_not_create_or_modify_project_files){
  Project project;auto bad=fxRecipe(5);bad.fx.width=2049;std::string error;const auto revision=hash("invalid revision");
  AE_EXPECT_TRUE(!editor::writeTerrainRecovery(project.name(),revision,bad,{},error)&&!error.empty(),"bad dimensions report precise model failure");AE_EXPECT_TRUE(!std::filesystem::exists(project.root/".astra"),"invalid recipe rejected before store directories or temp recovery");
  auto good=fxRecipe(5);const auto oldHash=hash("old valid revision"),newHash=hash("new valid revision");AE_EXPECT_TRUE(editor::writeTerrainRecovery(project.name(),oldHash,good,{},error),error.c_str());
  std::filesystem::path objects,reference;AE_EXPECT_TRUE(editor::terrainRecoveryPaths(project.name(),oldHash,objects,reference),"owned paths");const auto oldId=readText(reference),oldBytes=readText(objects/(oldId+".aethist"));
  const auto owned=objects/(std::string(64,'a')+".aethist.aether-tmp");{std::ofstream out(owned);out<<"pending";}
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(reference,"not-a-hash",&error),error.c_str());
  AE_EXPECT_TRUE(!editor::writeTerrainRecovery(project.name(),newHash,good,oldHash,error)&&!error.empty(),"broken lineage diagnosed before any new publication");
  AE_EXPECT_TRUE(std::filesystem::exists(owned)&&readText(objects/(oldId+".aethist"))==oldBytes,"failed lineage preflight does not recover temps or overwrite immutable checkpoints");
  std::filesystem::path ignored,newReference;AE_EXPECT_TRUE(editor::terrainRecoveryPaths(project.name(),newHash,ignored,newReference),"new owned path");AE_EXPECT_TRUE(!std::filesystem::exists(newReference),"failed request never publishes revision reference");
}
AE_TEST(terrain_fx_history_size_boundaries_are_explicit_and_transactional){
  Project project;resources::TerrainHistoryStore::Limits defaults;
  AE_EXPECT_EQ(defaults.maxBytes,usize(resources::TerrainRecipeMaximumBytes),"history quota covers the complete legal Recipe9 worst case, not only compressible maps");
  resources::TerrainHistoryStore store(project.root/"boundary",{4096,2});std::string exact(4096,'x'),id,out="protected";
  AE_EXPECT_TRUE(store.put(exact,{},id)&&store.read(id,out)&&out==exact,"exact configured byte boundary accepts complete checkpoint");
  const auto count=files(project.root/"boundary");out="protected";
  AE_EXPECT_TRUE(!store.put(std::string(4097,'x'),{},out)&&out=="protected","one byte above quota rejected before immutable file write");AE_EXPECT_EQ(files(project.root/"boundary"),count,"overquota request creates no object/temp file");
  resources::TerrainHistoryStore smaller(project.root/"boundary",{4095,2});AE_EXPECT_TRUE(!smaller.read(id,out)&&out=="protected","reader rejects reconstruction above its explicit decoded output quota");
  resources::TerrainHistoryStore invalid(project.root/"invalid-limit",{0,2});AE_EXPECT_TRUE(!std::filesystem::exists(project.root/"invalid-limit"),"invalid configured quota cannot create a store directory");
}
AE_TEST(terrain_fx_project_source_read_accepts_legal_glb_above_old64mib_and_rejects_oversize){
  Project project;auto recipe=fxRecipe(5);resources::TerrainMesh mesh;std::string error;std::vector<u8> original;
  AE_EXPECT_TRUE(resources::generateTerrain(recipe,mesh,error)&&resources::writeTerrainGlb(recipe,mesh,{},original,error),error.c_str());
  const auto word=[](const std::vector<u8> &v,usize i){return u32(v[i])|(u32(v[i+1])<<8)|(u32(v[i+2])<<16)|(u32(v[i+3])<<24);};
  const u32 oldJson=word(original,12),oldBinary=word(original,20+oldJson);std::string json(reinterpret_cast<const char*>(original.data()+20),oldJson);
  const std::string marker="\"buffers\":[{\"byteLength\":";auto at=json.find(marker);AE_EXPECT_TRUE(at!=std::string::npos,"real generated GLB buffer length");at+=marker.size();auto end=at;while(end<json.size()&&json[end]>='0'&&json[end]<='9')++end;
  const u32 binary=65u*1048576;json.replace(at,end-at,std::to_string(binary));while(json.size()%4)json+=' ';
  std::vector<u8> large;const auto append=[&](u32 value){for(u32 shift=0;shift<32;shift+=8)large.push_back(u8(value>>shift));};
  const u32 total=u32(28+json.size()+binary);append(0x46546c67);append(2);append(total);append(u32(json.size()));append(0x4e4f534a);large.insert(large.end(),json.begin(),json.end());append(binary);append(0x004e4942);
  large.insert(large.end(),original.begin()+28+oldJson,original.begin()+28+oldJson+oldBinary);large.resize(total,0);
  const auto revision=Sha256::hex(large);const auto path=project.root/"large.glb";AE_EXPECT_TRUE(editor::EditorImportTransaction::write(path,large,&error),error.c_str());
  resources::TerrainRecipe cold;bool recovered=true;AE_EXPECT_TRUE(editor::readTerrainAuthoring(project.name(),"large.glb",revision,cold,error,recovered),error.c_str());AE_EXPECT_TRUE(!recovered&&cold.fx.seedPixels==recipe.fx.seedPixels,"legal source above64 MiB reads without dropping data or requiring a recovery duplicate");
  const auto tooLarge=project.root/"too-large.glb";{std::ofstream file(tooLarge,std::ios::binary);file.seekp(std::streamoff(resources::TerrainSourceMaximumBytes));file.put('x');}
  cold.seed=999;AE_EXPECT_TRUE(!editor::readTerrainAuthoring(project.name(),"too-large.glb",hash("no recovery"),cold,error,recovered),"oversize source without recovery is rejected before its payload allocation");AE_EXPECT_TRUE(error.find("budget")!=std::string::npos&&cold.seed==999,"explicit quota diagnostic and no partial output");
}
