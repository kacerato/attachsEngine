#include "harness.h"
#include "runtime/prefab.h"
#include "runtime/game_world.h"
#include "editor/editor_archive.h"
#include "editor/editor_history.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_project_tags.h"
#include "scene/import_link.h"
#include <filesystem>
#include <chrono>
#include "scene/prefab_link.h"
#include "scene/script_behavior.h"
#include "scene/camera_follow.h"

using namespace ae;
using namespace ae::runtime;
using namespace ae::editor;
namespace {
struct Fixture {
  EditorDocument scene;
  ObjectId root=scene.createEntity(scene.root(),ObjectKind::Folder,"Rig");
  ObjectId child=scene.createEntity(root,ObjectKind::Camera,"Câmera");
  resources::AssetGuid guid=resources::assetGuidFromSeed("prefab-test");
  void configure() {
    auto value=*scene.find(root);value.active=false;value.transform.position[0]=3.125f;
    auto *script=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType="Fixture";script->source="Fixture.cs";
    script->properties={{"child","object",std::to_string(child)},
      {"component","component:astra.camera.follow",scene::scriptComponentValue(child,1)},
      {"children","array:object",scene::scriptArrayValue({std::to_string(child)})}};
    scene.applyEntityValues(root,value);
    value=*scene.find(child);
    auto *follow=static_cast<scene::CameraFollow*>(value.components.add(scene::CameraFollow::descriptor));
    follow->target=root;scene.applyEntityValues(child,value);
  }
};
}

AE_TEST(prefab_roundtrip_instantiates_portable_references_and_persistent_ownership) {
  Fixture f;f.configure();Prefab source,loaded;std::string error;
  AE_EXPECT_TRUE(source.capture(f.scene,f.root,f.guid,error),error.c_str());
  const auto bytes=source.write();
  AE_EXPECT_TRUE(!bytes.empty() && loaded.read(bytes,defaultEditorComponentRegistry(),error),error.c_str());
  AE_EXPECT_EQ(loaded.write(),bytes,"canonical roundtrip");
  EditorDocument destination;
  for(int i=0;i<5;++i) destination.createEntity(destination.root(),ObjectKind::Folder,"Unrelated");
  ObjectCloneMap map;const auto root=loaded.instantiate(destination,destination.root(),map,error);
  AE_EXPECT_TRUE(root && map.size()==2,error.c_str());
  const auto &value=*destination.find(root);
  AE_EXPECT_TRUE(!value.active && value.transform.position[0]==3.125f,"authoring preserved");
  const auto *script=scene::scriptBehavior(value.components.at(0));
  AE_EXPECT_EQ(script->properties[0].value,std::to_string(map.at(f.child)),"object reference remapped across graphs");
  AE_EXPECT_EQ(script->properties[1].value,scene::scriptComponentValue(map.at(f.child),1),"component identity retained");
  AE_EXPECT_EQ(script->properties[2].value,scene::scriptArrayValue({std::to_string(map.at(f.child))}),"reference array remapped");
  const auto *follow=static_cast<const scene::CameraFollow*>(destination.find(map.at(f.child))->components.at(0));
  AE_EXPECT_EQ(follow->target,static_cast<u64>(root),"native back reference");
  const auto *link=scene::prefabLink(value.components);
  AE_EXPECT_TRUE(link && link->asset==f.guid && link->sourceObject==f.root && link->instanceRoot==root,"persistent instance identity");
  AE_EXPECT_EQ(link->base,serializePrefabObject(*loaded.graph().find(f.root)),"canonical baseline uses source IDs");
  EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(destination,7),7,reopened),"scene roundtrip with prefab metadata");
  AE_EXPECT_EQ(scene::prefabLink(reopened.find(root)->components)->base,link->base,"baseline survives scene save");
  EditorHistory history;const auto duplicate=history.duplicateEntity(reopened,root);
  AE_EXPECT_TRUE(duplicate && scene::prefabLink(reopened.find(duplicate)->components)->instanceRoot==duplicate,"duplicate has its own owner");
  const auto duplicateChild=reopened.childrenOf(duplicate).front();
  AE_EXPECT_EQ(scene::prefabLink(reopened.find(duplicateChild)->components)->instanceRoot,static_cast<u64>(duplicate),"whole instance remapped");
  AE_EXPECT_TRUE(history.undo(reopened) && !reopened.exists(duplicate) && history.redo(reopened),"duplicate undo redo");
  const auto detached=history.duplicateEntity(reopened,map.at(f.child));
  AE_EXPECT_TRUE(detached && !scene::prefabLink(reopened.find(detached)->components),"copy of child does not claim duplicate source identity");
}

AE_TEST(prefab_rejects_external_references_corrupt_hierarchies_and_partial_replacement) {
  Fixture f;f.configure();Prefab prefab;std::string error;
  AE_EXPECT_TRUE(prefab.capture(f.scene,f.root,f.guid,error),"capture");
  const auto good=prefab.write();
  auto value=*f.scene.find(f.child);
  auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->target=f.scene.root();f.scene.applyEntityValues(f.child,value);
  AE_EXPECT_TRUE(!prefab.capture(f.scene,f.root,f.guid,error) && !error.empty(),"external scene reference is rejected");
  AE_EXPECT_EQ(prefab.write(),good,"capture failure preserves asset");
  for(const auto &bad:std::vector<std::string>{good+"unexpected",good.substr(0,good.size()/2),"ASTRA_PREFAB 99 bad 2 1"}) {
    AE_EXPECT_TRUE(!prefab.read(bad,defaultEditorComponentRegistry(),error) && !error.empty(),"malformed input rejected");
    AE_EXPECT_EQ(prefab.write(),good,"load failure preserves asset");
  }
  auto cycle=good;const auto childPrefix="\n"+std::to_string(f.child)+" "+std::to_string(f.root)+" ";
  const auto pos=cycle.find(childPrefix);AE_EXPECT_TRUE(pos!=std::string::npos,"fixture child located");
  cycle.replace(pos,childPrefix.size(),"\n"+std::to_string(f.child)+" "+std::to_string(f.child)+" ");
  AE_EXPECT_TRUE(!prefab.read(cycle,defaultEditorComponentRegistry(),error),"self-parent rejected");
  EditorDocument target;ObjectCloneMap map{{3,8}};const auto count=target.entityCount();
  AE_EXPECT_TRUE(!prefab.instantiate(target,999,map,error) && map.empty() && target.entityCount()==count,"bad destination cannot leave objects");
}

AE_TEST(prefab_runtime_uses_world_lifecycle_and_rolls_back_failed_activation) {
  Fixture f;f.configure();Prefab prefab;std::string error;
  AE_EXPECT_TRUE(prefab.capture(f.scene,f.root,f.guid,error),"capture");
  SceneGraph empty;GameWorld world;AE_EXPECT_TRUE(world.load(empty),"Play");
  ObjectCloneMap map;WorldStatus status;
  const auto first=world.instantiate(prefab,world.handle(empty.root()),map,status,error);
  AE_EXPECT_TRUE(first.valid() && status==WorldStatus::Ok,error.c_str());
  AE_EXPECT_TRUE(world.finishInstantiation(first,false)==WorldStatus::Ok && !world.alive(first),"failed binding rolls back synchronously");
  AE_EXPECT_EQ(world.graph().entityCount(),1u,"no orphan child");
  const auto second=world.instantiate(prefab,world.handle(empty.root()),map,status,error);
  AE_EXPECT_TRUE(second.valid() && world.finishInstantiation(second,true)==WorldStatus::Ok,"publish entire hierarchy");
  const auto child=world.handle(map.at(f.child));
  AE_EXPECT_TRUE(world.destroyObject(second)==WorldStatus::Ok && !world.alive(child) && world.flush(),"destruction invalidates child handles");
  AE_EXPECT_EQ(world.graph().entityCount(),1u,"world owns lifecycle");
  AE_EXPECT_EQ(f.scene.entityCount(),3u,"source is unchanged");
}

AE_TEST(prefab_editor_publishes_asset_and_registry_then_instantiates_with_undo) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  const auto root=session.history().createEntity(session.document(),session.document().root(),ObjectKind::Folder,"Rig");
  session.history().createEntity(session.document(),root,ObjectKind::Folder,"Child");
  std::string error;const auto asset=session.createPrefab(root,error);
  AE_EXPECT_TRUE(asset.valid(),error.c_str());
  AE_EXPECT_TRUE(fs::exists(path/"Prefabs/Rig.prefab") && fs::exists(path/".astra/assets.astra"),"asset and registry published together");
  AE_EXPECT_TRUE(scene::prefabLink(session.document().find(root)->components),"source becomes instance");
  AE_EXPECT_TRUE(session.history().undo(session.document()) && !scene::prefabLink(session.document().find(root)->components),"undo conversion preserves reusable asset");
  AE_EXPECT_TRUE(fs::exists(path/"Prefabs/Rig.prefab") && session.history().redo(session.document()),"redo restores link");
  const auto count=session.document().entityCount();
  const auto instance=session.instantiatePrefab(asset,session.document().root(),error);
  AE_EXPECT_TRUE(instance && session.document().entityCount()==count+2,error.c_str());
  AE_EXPECT_TRUE(session.history().undo(session.document()) && session.document().entityCount()==count,"one undo removes whole instance");
  AE_EXPECT_TRUE(session.history().redo(session.document()) && session.document().exists(instance),"redo keeps identity");
  const auto child=session.document().childrenOf(instance).front();
  const auto snapshot=serializeEditorDocument(session.document(),7);
  // No source is required to recover effective data from a broken link.
  fs::rename(path/"Prefabs/Rig.prefab",path/"Prefabs/Rig.temporarily-missing");
  AE_EXPECT_TRUE(session.unpackPrefab(child,error),error.c_str());
  AE_EXPECT_TRUE(!scene::prefabLink(session.document().find(instance)->components) &&
    !scene::prefabLink(session.document().find(child)->components),"unpack resolves owner from child");
  AE_EXPECT_TRUE(scene::prefabLink(session.document().find(root)->components),"other instance stays linked");
  AE_EXPECT_EQ(session.document().entityCount(),count+2,"unpack keeps objects and identity");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"unpack undo");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),snapshot,"undo restores exact authored scene");
  AE_EXPECT_TRUE(session.history().redo(session.document()) && !scene::prefabLink(session.document().find(child)->components),"unpack redo");
  fs::rename(path/"Prefabs/Rig.temporarily-missing",path/"Prefabs/Rig.prefab");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(path.string().c_str()),"reopen");
  std::vector<u8> registry;
  AE_EXPECT_TRUE(EditorImportTransaction::read(path/".astra/assets.astra",registry) &&
    reopened.loadAssets(std::string_view(reinterpret_cast<const char*>(registry.data()),registry.size())),"shell reloads registry before scene");
  runtime::Prefab loaded;AE_EXPECT_TRUE(reopened.loadPrefab(asset,loaded,error),error.c_str());
  fs::remove(path/"Prefabs/Rig.prefab");
  const auto before=reopened.document().entityCount();
  AE_EXPECT_TRUE(!reopened.instantiatePrefab(asset,reopened.document().root(),error) && !error.empty(),"missing source diagnosed");
  AE_EXPECT_EQ(reopened.document().entityCount(),before,"no partial instance when source missing");
}

AE_TEST(prefab_import_ownership_is_unique_per_instance_and_tags_protect_saved_assets) {
  Fixture f;f.configure();
  const auto source=resources::assetGuidFromSeed("model-source"),instance=resources::assetGuidFromSeed("model-instance");
  for(const auto id:{f.root,f.child}) {
    auto value=*f.scene.find(id);
    auto *link=static_cast<scene::ImportLink*>(value.components.add(scene::ImportLink::descriptor));
    link->source=source;link->instance=instance;link->root=id==f.root;
    link->node=resources::assetGuidFromSeed(std::to_string(id));value.tag="PrefabOnly";
    AE_EXPECT_TRUE(f.scene.applyEntityValues(id,value),"import ownership");
  }
  Prefab prefab;std::string error;AE_EXPECT_TRUE(prefab.capture(f.scene,f.root,f.guid,error),error.c_str());
  EditorDocument destination;ObjectCloneMap map;
  const auto first=prefab.instantiate(destination,destination.root(),map,error);
  AE_EXPECT_TRUE(first,error.c_str());
  const auto firstOwner=scene::importLink(destination.find(first)->components)->instance;
  AE_EXPECT_EQ(scene::importLink(destination.find(map.at(f.child))->components)->instance,firstOwner,"import descendants belong to this copy");
  const auto second=prefab.instantiate(destination,destination.root(),map,error);
  AE_EXPECT_TRUE(second && scene::importLink(destination.find(second)->components)->instance!=firstOwner,"next copy has separate import ownership");
  AE_EXPECT_TRUE(firstOwner!=instance,"source import ownership is not shared");
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-tags-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Tagged.prefab",prefab.write()),"saved source");
  EditorDocument empty;
  AE_EXPECT_TRUE(!projectTagUnused(path.string(),empty,"PrefabOnly",error) && error.find("prefab")!=std::string::npos,"closed prefab prevents orphan tag");
  AE_EXPECT_TRUE(projectTagUnused(path.string(),empty,"Unused",error),"unrelated tag can be removed");
}
