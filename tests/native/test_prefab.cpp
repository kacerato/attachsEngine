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
  AE_EXPECT_EQ(session.document().find(instance)->components.size(),usize{1},"resource validation does not add a renderer to an empty object");
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

AE_TEST(prefab_identity_frontier_survives_deleted_objects_and_legacy_migration) {
  SceneGraph graph;const auto root=graph.createEntity(graph.root(),ObjectKind::Folder,"Root");
  const auto removed=graph.createEntity(root,ObjectKind::Folder,"Removed");
  AE_EXPECT_TRUE(graph.destroyEntity(removed),"remove highest identity");
  Prefab prefab,loaded;std::string error;const auto guid=resources::assetGuidFromSeed("frontier");
  AE_EXPECT_TRUE(prefab.capture(graph,root,guid,error) && loaded.read(prefab.write(),defaultEditorComponentRegistry(),error),error.c_str());
  auto editing=loaded.graph();
  AE_EXPECT_TRUE(editing.createEntity(root,ObjectKind::Folder,"New")>removed,"deleted source identity is never reused after reload");
  const auto bytes=loaded.write();const auto newline=bytes.find('\n');
  const auto legacy="ASTRA_PREFAB 1 "+guid.text()+" "+std::to_string(root)+" 1"+bytes.substr(newline);
  AE_EXPECT_TRUE(loaded.read(legacy,defaultEditorComponentRegistry(),error),"v1 remains readable");
  const auto good=loaded.write();
  for(const auto next:{root,SceneGraph::kMaximumObjects+2}) {
    const auto bad="ASTRA_PREFAB 2 "+guid.text()+" "+std::to_string(root)+" 1 "+std::to_string(next)+bytes.substr(newline);
    AE_EXPECT_TRUE(!loaded.read(bad,defaultEditorComponentRegistry(),error),"invalid frontier refused");
    AE_EXPECT_EQ(loaded.write(),good,"failure is transactional");
  }
  AE_EXPECT_TRUE(editing.reserveObjectIdsUntil(SceneGraph::kMaximumObjects+1),"exhaust frontier without huge allocation");
  const auto revision=editing.revision();
  AE_EXPECT_TRUE(!editing.createEntity(root,ObjectKind::Folder,"Overflow") && editing.revision()==revision,"exhaustion never creates an unserializable object");
}

AE_TEST(prefab_selective_revert_preserves_other_overrides_references_history_and_persistence) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-revert-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  Fixture f;f.configure();session.document()=f.scene;
  std::string error;const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  const auto instance=session.instantiatePrefab(asset,session.document().root(),error);
  AE_EXPECT_TRUE(instance,error.c_str());
  const auto child=session.document().childrenOf(instance).front();
  auto value=*session.document().find(child);
  auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->dampingSeconds=2;follow->offset[0]=9;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),child,value),"local fields");
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(child,view,error),error.c_str());
  const auto row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="damping_seconds";});
  AE_EXPECT_TRUE(row!=view.rows.end() && view.rows.size()==2,"native reference is normalized, two actual overrides");
  const auto before=serializeEditorDocument(session.document(),7);
  AE_EXPECT_TRUE(session.revertPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),error),error.c_str());
  const auto *result=static_cast<const scene::CameraFollow*>(session.document().find(child)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(result->dampingSeconds==.2f && result->offset[0]==9 && result->target==instance,"selective revert preserves other property and remapped reference");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo revert");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"exact undo");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo revert");
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(session.document(),7),7,reopened),"save reopen");
  result=static_cast<const scene::CameraFollow*>(reopened.find(child)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(result->dampingSeconds==.2f && result->offset[0]==9,"selective result persists");
  value=*session.document().find(instance);value.transform.position[0]=25;
  auto *script=static_cast<scene::ScriptBehavior*>(value.components.edit(scene::ScriptBehavior::descriptor));
  script->setProperty("child","object","0");script->setProperty("newLocal","int32","17");
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),instance,value),"script overrides and placement");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(instance,view,error) && view.rows.size()==2,"root placement is not a source override");
  const auto scriptRow=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="child";});
  AE_EXPECT_TRUE(scriptRow!=view.rows.end() && session.revertPrefabOverride(view,static_cast<usize>(scriptRow-view.rows.begin()),error),error.c_str());
  const auto *live=scene::scriptBehavior(session.document().find(instance)->components.find(scene::ScriptBehavior::descriptor));
  AE_EXPECT_TRUE(live->properties[0].value==std::to_string(child) && live->properties.back().value=="17","script object refers to this instance; other field stays");
  AE_EXPECT_EQ(session.document().find(instance)->transform.position[0],25.f,"root placement preserved");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(instance,view,error) && view.rows.size()==1,"one field remains");
  AE_EXPECT_TRUE(session.revertPrefabOverride(view,0,error),error.c_str());
  live=scene::scriptBehavior(session.document().find(instance)->components.find(scene::ScriptBehavior::descriptor));
  AE_EXPECT_EQ(live->properties.size(),usize{3},"local-only script field returns to constructor default by removing authored entry");
}

AE_TEST(prefab_revert_restores_component_identity_and_refuses_stale_or_dangling_edits) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-guards-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  Fixture f;f.configure();session.document()=f.scene;std::string error;
  const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  auto value=*session.document().find(f.child);value.components.removeInstance(1);
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),f.child,value),"removed source component");
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error) && view.rows.size()==1,error.c_str());
  AE_EXPECT_TRUE(view.rows[0].kind==PrefabOverrideKind::RemovedComponent && session.revertPrefabOverride(view,0,error),error.c_str());
  AE_EXPECT_TRUE(session.document().find(f.child)->components.findInstance(1),"original component identity restored for existing script references");
  value=*session.document().find(f.root);value.visible=false;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),f.root,value),"local visibility");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.root,view,error) && view.rows.size()==1,error.c_str());
  value.active=true;AE_EXPECT_TRUE(session.history().applyValues(session.document(),f.root,value),"edit after preview");
  const auto before=serializeEditorDocument(session.document(),7);const auto depth=session.history().undoDepth();
  AE_EXPECT_TRUE(!session.revertPrefabOverride(view,0,error),"stale scene refused");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"stale preview has no mutation");
  AE_EXPECT_EQ(session.history().undoDepth(),depth,"no history pollution");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.root,view,error),error.c_str());
  Prefab source;AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());
  auto changed=source.graph();value=*changed.find(f.root);runtime::assignObjectName(value,"Changed source");changed.applyEntityValues(f.root,value);
  AE_EXPECT_TRUE(source.capture(changed,f.root,asset,error),error.c_str());
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",source.write()),"external source edit");
  AE_EXPECT_TRUE(!session.revertPrefabOverride(view,0,error) && error.find("fonte mudou")!=std::string::npos,"stale source refused");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"external edit cannot silently change preview");
  value=*session.document().find(f.root);
  auto *added=value.components.add(scene::CameraFollow::descriptor);AE_EXPECT_TRUE(added,"local component");
  const auto addedId=added->instanceId();
  auto *script=static_cast<scene::ScriptBehavior*>(value.components.edit(scene::ScriptBehavior::descriptor));
  script->setProperty("localReference","component:astra.camera.follow",scene::scriptComponentValue(f.root,addedId));
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),f.root,value),"script refers to added component");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.root,view,error),error.c_str());
  auto removal=std::find_if(view.rows.begin(),view.rows.end(),[&](const auto &r){return r.kind==PrefabOverrideKind::AddedComponent && r.component==addedId;});
  AE_EXPECT_TRUE(removal!=view.rows.end() && !session.revertPrefabOverride(view,static_cast<usize>(removal-view.rows.begin()),error),"removal cannot leave a dangling component reference");
  value=*session.document().find(f.root);script=static_cast<scene::ScriptBehavior*>(value.components.edit(scene::ScriptBehavior::descriptor));
  script->setProperty("localReference","component:astra.camera.follow","0:0");
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),f.root,value),"clear reference");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.root,view,error),error.c_str());
  removal=std::find_if(view.rows.begin(),view.rows.end(),[&](const auto &r){return r.kind==PrefabOverrideKind::AddedComponent && r.component==addedId;});
  AE_EXPECT_TRUE(removal!=view.rows.end() && session.revertPrefabOverride(view,static_cast<usize>(removal-view.rows.begin()),error),error.c_str());
  AE_EXPECT_TRUE(!session.document().find(f.root)->components.findInstance(addedId),"unreferenced local addition removed");
}
