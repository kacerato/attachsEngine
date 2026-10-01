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
#include "scene/camera.h"
#include "scene/animation.h"

using namespace ae;
using namespace ae::runtime;
using namespace ae::editor;
AE_TEST(prefab_collection_contract_preserves_identity_and_allocator) {
  scene::Animation first;
  const auto a=first.appendClip(),b=first.appendClip();
  auto second=first;
  AE_EXPECT_TRUE(scene::sameComponentCollectionStructure(first,second),"identical structure");
  second.clips[0].asset=resources::assetGuidFromSeed("different-clip");
  AE_EXPECT_TRUE(scene::sameComponentCollectionStructure(first,second),"resource values are not structure");
  second.moveClip(b,0);
  AE_EXPECT_TRUE(!scene::sameComponentCollectionStructure(first,second),"reordered identical resources retain different element addresses");
  second.moveClip(a,0);
  const auto removed=second.appendClip();second.removeClip(removed);
  AE_EXPECT_TRUE(!scene::sameComponentCollectionStructure(first,second),"deleted element identity frontier is never absorbed by selective Apply");
}
AE_TEST(prefab_animation_collection_reorder_applies_atomically_and_reverts_with_history) {
  namespace fs=std::filesystem;
  const auto directory=fs::temp_directory_path()/("astra-prefab-animation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(directory);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{directory};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(directory.string().c_str()),"project");
  auto &document=session.document();const auto root=document.createEntity(document.root(),ObjectKind::Folder,"Animation");
  auto value=*document.find(root);
  auto *animation=static_cast<scene::Animation*>(value.components.add(scene::Animation::descriptor));
  animation->appendClip();const auto second=animation->appendClip();
  AE_EXPECT_TRUE(document.applyEntityValues(root,value),"author collection");
  std::string error;const auto asset=session.createPrefab(root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  value=*document.find(root);animation=static_cast<scene::Animation*>(value.components.edit(scene::Animation::descriptor));
  animation->moveClip(second,0);animation->speed=2;
  AE_EXPECT_TRUE(document.applyEntityValues(root,value),"reorder local elements");
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  const auto row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto&r){return r.kind==PrefabOverrideKind::Component;});
  AE_EXPECT_TRUE(row!=view.rows.end() && row->applyable && row->applicable,"structural scope explicit, whole Apply and Revert available");
  const auto before=serializeEditorDocument(document,7);
  AE_EXPECT_TRUE(session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error),error.c_str());
  Prefab source;AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());
  const auto *published=static_cast<const scene::Animation*>(source.graph().find(root)->components.find(scene::Animation::descriptor));
  AE_EXPECT_TRUE(published && published->clips.front().id==second && published->speed==2,"whole source includes order and authored values");
  AE_EXPECT_TRUE(session.history().undo(document) && serializeEditorDocument(document,7)==before,"Undo restores source and local override");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  const auto revertedRow=std::find_if(view.rows.begin(),view.rows.end(),[](const auto&r){return r.kind==PrefabOverrideKind::Component;});
  AE_EXPECT_TRUE(revertedRow!=view.rows.end(),"Undo reopens complete address");
  AE_EXPECT_TRUE(session.revertPrefabOverride(view,static_cast<usize>(revertedRow-view.rows.begin()),error),error.c_str());
  const auto *restored=static_cast<const scene::Animation*>(document.find(root)->components.find(scene::Animation::descriptor));
  AE_EXPECT_TRUE(restored->clips[1].id==second && restored->speed==1,"whole Revert restores identity and values");
  AE_EXPECT_TRUE(session.history().undo(document),"undo restored collection");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"undo retains reordered elements and baseline");
}
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
    follow->target=root;value.components.add(scene::Camera::descriptor);scene.applyEntityValues(child,value);
  }
};
}

AE_TEST(groups_roundtrip_history_prefab_and_world_membership_are_one_chain) {
  Fixture f;auto value=*f.scene.find(f.root);
  AE_EXPECT_TRUE(value.groups.add("guards") && value.groups.add("damageable") && value.groups.add("guards"),"membership is idempotent");
  EditorHistory history;AE_EXPECT_TRUE(history.applyValues(f.scene,f.root,value),"history edits memberships");
  EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(f.scene,42),42,reopened),"scene v15 loads groups");
  AE_EXPECT_EQ(reopened.find(f.root)->groups,value.groups,"membership persisted");
  AE_EXPECT_TRUE(history.undo(f.scene) && f.scene.find(f.root)->groups.names().empty(),"Undo removes membership");
  AE_EXPECT_TRUE(history.redo(f.scene),"Redo restores membership");
  Prefab source,loaded;std::string error;
  AE_EXPECT_TRUE(source.capture(f.scene,f.root,f.guid,error) && loaded.read(source.write(),defaultEditorComponentRegistry(),error),"prefab v3 roundtrip");
  ObjectCloneMap mapping;EditorDocument destination;
  const auto clone=loaded.instantiate(destination,destination.root(),mapping,error);
  AE_EXPECT_TRUE(clone && destination.find(clone)->groups==value.groups,"instantiation retains groups");
  GameWorld world;AE_EXPECT_TRUE(world.load(destination),"real runtime loads groups");
  std::array<u64,2> members{};u32 count=0;
  AE_EXPECT_TRUE(world.findGroup("guards",members,count)==WorldStatus::Ok && count==1 && members[0]==clone,"query finds clone");
  const auto handle=world.handle(clone);bool member=false;
  AE_EXPECT_TRUE(world.setGroupMembership(handle,"guards",false)==WorldStatus::Ok &&
    world.isInGroup(handle,"guards",member)==WorldStatus::Ok && !member,"runtime removes membership");
  AE_EXPECT_TRUE(destination.find(clone)->groups.contains("guards"),"runtime preserves authoring document");
  AE_EXPECT_TRUE(world.setGroupMembership(handle,"guards",true)==WorldStatus::Ok && world.setActive(handle,false)==WorldStatus::Ok,"inactive member configured");
  AE_EXPECT_TRUE(world.findGroup("guards",{},count)==WorldStatus::Ok && count==1,"default includes inactive");
  AE_EXPECT_TRUE(world.findGroup("guards",{},count,false)==WorldStatus::Ok && count==0,"active-only excludes inactive branch");
  AE_EXPECT_TRUE(world.destroyObject(handle)==WorldStatus::Ok && world.findGroup("guards",{},count)==WorldStatus::Ok && count==0,"pending destruction excluded");
  AE_EXPECT_TRUE(world.setGroupMembership(handle,"guards",true)!=WorldStatus::Ok,"stale handle rejected");
}

AE_TEST(groups_names_and_legacy_storage_reject_ambiguous_membership) {
  ObjectGroups groups;AE_EXPECT_TRUE(!groups.add("") && !groups.add(" bad") && !groups.add("bad\nname"),"invalid identity rejected");
  AE_EXPECT_TRUE(groups.add("a, b") && groups.add("a") && groups.add("b"),"punctuation has unambiguous storage");
  const auto before=groups;
  std::istringstream duplicate("GROUPS 1 2 \"guards\" \"guards\"");
  AE_EXPECT_TRUE(!groups.read(duplicate) && groups==before,"invalid read preserves membership");
  std::istringstream oversized("GROUPS 1 33");AE_EXPECT_TRUE(!groups.read(oversized),"bounded names");
  Fixture f;auto object=*f.scene.find(f.root);object.groups=groups;
  auto wire=serializePrefabObject(object);const auto marker=wire.rfind(" GROUPS");
  AE_EXPECT_TRUE(marker!=std::string::npos,"groups marker exists");wire.resize(marker);
  SceneObject legacy;std::istringstream old(wire);
  AE_EXPECT_TRUE(deserializePrefabObject(old,defaultEditorComponentRegistry(),legacy) && legacy.groups.names().empty(),"old baseline migrates to empty membership");
  std::istringstream required(wire);AE_EXPECT_TRUE(!deserializePrefabObject(required,defaultEditorComponentRegistry(),legacy,true),"current prefab requires membership section");
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

AE_TEST(prefab_three_way_refresh_preserves_conflicts_references_and_selective_baselines) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-merge-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  Fixture f;f.configure();session.document()=f.scene;std::string error;
  const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  const auto second=session.instantiatePrefab(asset,session.document().root(),error);AE_EXPECT_TRUE(second,error.c_str());
  const auto secondChild=session.document().childrenOf(second).front();
  const auto third=session.instantiatePrefab(asset,session.document().root(),error);AE_EXPECT_TRUE(third,error.c_str());
  const auto thirdChild=session.document().childrenOf(third).front();
  const auto followOf=[&](ObjectId id){return static_cast<const scene::CameraFollow*>(session.document().find(id)->components.find(scene::CameraFollow::descriptor));};
  auto value=*session.document().find(f.child);
  auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->dampingSeconds=.9f;follow->offset[0]=9;
  AE_EXPECT_TRUE(session.document().applyEntityValues(f.child,value),"local conflicting and independent overrides");
  value=*session.document().find(thirdChild);follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->dampingSeconds=.6f;
  AE_EXPECT_TRUE(session.document().applyEntityValues(thirdChild,value),"local value will converge with source");
  value=*session.document().find(second);value.transform.position[0]=25;session.document().applyEntityValues(second,value);
  Prefab authored;AE_EXPECT_TRUE(session.loadPrefab(asset,authored,error),error.c_str());
  auto source=authored.graph();value=*source.find(f.child);
  follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->dampingSeconds=.6f;follow->offset[1]=6;
  source.applyEntityValues(f.child,value);
  value=*source.find(f.root);auto *script=static_cast<scene::ScriptBehavior*>(value.components.edit(scene::ScriptBehavior::descriptor));
  script->setProperty("speed","float","7");source.applyEntityValues(f.root,value);
  AE_EXPECT_TRUE(authored.capture(source,f.root,asset,error),error.c_str());
  const auto raw="\n"+authored.write()+"\n";
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",raw),"external source edit with noncanonical whitespace");
  session.history().clear();
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  AE_EXPECT_EQ(view.sourceHash,Sha256::hex({reinterpret_cast<const u8*>(raw.data()),raw.size()}),"preview guards actual file bytes");
  const auto find=[&](std::string_view field,u32 slot=0){return std::find_if(view.rows.begin(),view.rows.end(),[&](const auto &row){return row.field==field && row.slot==slot;});};
  auto damping=find("damping_seconds"),x=find("offset_x"),y=find("offset_y");
  AE_EXPECT_TRUE(damping!=view.rows.end() && damping->origin==PrefabOverrideOrigin::Conflict,"both local and source changed damping");
  AE_EXPECT_TRUE(x!=view.rows.end() && x->origin==PrefabOverrideOrigin::Local,"independent local offset");
  AE_EXPECT_TRUE(y!=view.rows.end() && y->origin==PrefabOverrideOrigin::Inherited,"external offset is not falsely called local");
  AE_EXPECT_TRUE(find("target")==view.rows.end(),"source references are compared in this instance's namespace");
  const auto before=serializeEditorDocument(session.document(),7);
  AE_EXPECT_TRUE(session.refreshPrefabInstance(view,error),error.c_str());
  AE_EXPECT_TRUE(followOf(f.child)->dampingSeconds==.9f && followOf(f.child)->offset[0]==9 && followOf(f.child)->offset[1]==6,"conflict/local preserved; inherited source received");
  AE_EXPECT_EQ(followOf(f.child)->target,static_cast<u64>(f.root),"native sibling reference retains scene identity");
  const auto *liveScript=scene::scriptBehavior(session.document().find(f.root)->components.find(scene::ScriptBehavior::descriptor));
  AE_EXPECT_TRUE(liveScript->properties.back().id=="speed" && liveScript->properties.back().value=="7","incoming script field is received across the entire instance");
  AE_EXPECT_EQ(followOf(secondChild)->offset[1],2.f,"other instance is not edited by receiving this one");
  AE_EXPECT_EQ(session.history().undoDepth(),1u,"whole instance and baselines form one transaction");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo merge");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"undo exactly restores all old bases and values");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo merge");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(secondChild,view,error) && session.refreshPrefabInstance(view,error),error.c_str());
  AE_EXPECT_TRUE(followOf(secondChild)->dampingSeconds==.6f && followOf(secondChild)->offset[1]==6 && followOf(secondChild)->target==second,"unmodified instance receives source using its own IDs");
  AE_EXPECT_EQ(session.document().find(second)->transform.position[0],25.f,"root placement is preserved");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(thirdChild,view,error),error.c_str());
  AE_EXPECT_TRUE(find("damping_seconds")==view.rows.end(),"a converged value is not an override");
  AE_EXPECT_TRUE(session.refreshPrefabInstance(view,error),error.c_str());
  const auto scenePath=(path/"main.aescene").string();AE_EXPECT_TRUE(session.save(scenePath.c_str(),7),"save scene");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(path.string().c_str()),"reopen project");
  std::vector<u8> registry;
  AE_EXPECT_TRUE(EditorImportTransaction::read(path/".astra/assets.astra",registry) &&
    reopened.loadAssets(std::string_view(reinterpret_cast<const char*>(registry.data()),registry.size())) && reopened.load(scenePath.c_str(),7),"shell reloads registry and persistent scene bases");
  AE_EXPECT_TRUE(reopened.inspectPrefabOverrides(f.child,view,error),error.c_str());
  AE_EXPECT_TRUE(find("damping_seconds")!=view.rows.end() && find("damping_seconds")->origin==PrefabOverrideOrigin::Local && !view.hasSourceChanges,"conflict accepted locally remains a persistent override of the new base");
  AE_EXPECT_TRUE(session.loadPrefab(asset,authored,error),error.c_str());source=authored.graph();value=*source.find(f.child);
  follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->dampingSeconds=.7f;follow->offset[1]=8;source.applyEntityValues(f.child,value);
  AE_EXPECT_TRUE(authored.capture(source,f.root,asset,error) && EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",authored.write()),"source changed a second time");
  const auto unchanged=serializeEditorDocument(session.document(),7);const auto depth=session.history().undoDepth();
  AE_EXPECT_TRUE(!session.refreshPrefabInstance(view,error),"stale preview is refused");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),unchanged,"refusal preserves scene");AE_EXPECT_EQ(session.history().undoDepth(),depth,"refusal preserves history");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  y=find("offset_y");AE_EXPECT_TRUE(y!=view.rows.end() && session.revertPrefabOverride(view,static_cast<usize>(y-view.rows.begin()),error),error.c_str());
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  damping=find("damping_seconds");AE_EXPECT_TRUE(damping!=view.rows.end() && damping->origin==PrefabOverrideOrigin::Conflict,"reverting Y does not rebase a different conflicting field");
  AE_EXPECT_EQ(followOf(f.child)->offset[1],8.f,"selected inherited field received");
}

AE_TEST(prefab_comparison_normalizes_inherited_renderer_caches_without_editing_authoring) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-cache-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  renderer::MapDrawRecord draw{};draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;draw.indexCount=36;
  AE_EXPECT_TRUE(session.importMap({&draw,1},{},false),"real resource library");
  auto &document=session.document();const auto root=document.createEntity(document.root(),ObjectKind::Mesh,"Cache");
  auto value=*document.find(root);auto *render=editMeshRenderer(value);render->mesh=1;render->asset=session.mapScene().assetGuid(0);
  render->material.baseColor[0]=.9f;render->material.enabled=false;document.applyEntityValues(root,value);
  std::string error;const auto asset=session.createPrefab(root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  value=*document.find(root);render=editMeshRenderer(value);render->material.baseColor[0]=.1f;document.applyEntityValues(root,value);
  const auto before=serializeEditorDocument(document,7);const auto revision=document.revision();
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  AE_EXPECT_TRUE(view.rows.empty() && !view.hasSourceChanges,"effective material cache is not a local or incoming authoring change");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"inspection never hydrates the live authoring document");
  AE_EXPECT_EQ(document.revision(),revision,"inspection does not invalidate authoring");
  value=*document.find(root);render=editMeshRenderer(value);render->material.enabled=true;render->material.baseColor[0]=.2f;document.applyEntityValues(root,value);
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error) && !view.rows.empty(),"explicit material overrides remain visible");
}

AE_TEST(prefab_apply_selective_propagates_all_loaded_instances_and_persists_conflicts_with_undo) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-apply-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  Fixture f;f.configure();session.document()=f.scene;std::string error;
  const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  const auto second=session.instantiatePrefab(asset,session.document().root(),error);
  const auto third=session.instantiatePrefab(asset,session.document().root(),error);AE_EXPECT_TRUE(second && third,error.c_str());
  const auto secondChild=session.document().childrenOf(second).front(),thirdChild=session.document().childrenOf(third).front();
  auto value=*session.document().find(f.child);auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->offset[0]=9;follow->dampingSeconds=.9f;session.document().applyEntityValues(f.child,value);
  value=*session.document().find(secondChild);follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->offset[2]=8;session.document().applyEntityValues(secondChild,value);
  value=*session.document().find(thirdChild);follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->offset[0]=7;session.document().applyEntityValues(thirdChild,value);
  value=*session.document().find(second);value.transform.position[0]=25;session.document().applyEntityValues(second,value);
  Prefab source;AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());auto graph=source.graph();value=*graph.find(f.child);
  follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->offset[1]=6;graph.applyEntityValues(f.child,value);
  AE_EXPECT_TRUE(source.capture(graph,f.root,asset,error),error.c_str());const auto beforeFile="\n"+source.write()+"\n";
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",beforeFile),"external source authoring with noncanonical bytes");
  session.history().clear();const auto beforeScene=serializeEditorDocument(session.document(),7),beforeRegistry=session.assets().serialize();
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  const auto row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="offset_x";});
  AE_EXPECT_TRUE(row!=view.rows.end() && row->applyable && row->origin==PrefabOverrideOrigin::Local,"applyable selected local address");
  PrefabApplyReport report;AE_EXPECT_TRUE(session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error,&report),error.c_str());
  AE_EXPECT_EQ(report.instances,3u,"all loaded instances, including original source instance");AE_EXPECT_EQ(report.conflicts,1u,"local sibling conflict retained");
  const auto read=[&](ObjectId id){return static_cast<const scene::CameraFollow*>(session.document().find(id)->components.find(scene::CameraFollow::descriptor));};
  AE_EXPECT_TRUE(read(f.child)->offset[0]==9 && read(f.child)->offset[1]==6 && read(f.child)->dampingSeconds==.9f,"selected instance receives external fields but keeps unselected local value");
  AE_EXPECT_TRUE(read(secondChild)->offset[0]==9 && read(secondChild)->offset[1]==6 && read(secondChild)->offset[2]==8 && read(secondChild)->target==second,"inherited values and stable remapped references, local Z retained");
  AE_EXPECT_TRUE(read(thirdChild)->offset[0]==7 && read(thirdChild)->offset[1]==6 && read(thirdChild)->target==third,"conflict local survives propagation");
  AE_EXPECT_EQ(session.document().find(second)->transform.position[0],25.f,"root placement never published or overwritten");
  AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());const auto *sourceFollow=static_cast<const scene::CameraFollow*>(source.graph().find(f.child)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(sourceFollow->offset[0]==9 && sourceFollow->offset[1]==6 && sourceFollow->dampingSeconds==.2f,"source S retains external Y and does not absorb unselected local damping");
  const auto afterFile=source.write(),afterScene=serializeEditorDocument(session.document(),7),afterRegistry=session.assets().serialize();
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo source and scene as one resource transaction");
  std::vector<u8> bytes;AE_EXPECT_TRUE(EditorImportTransaction::read(path/"Prefabs/Rig.prefab",bytes),"read undo bytes");
  AE_EXPECT_EQ(std::string(bytes.begin(),bytes.end()),beforeFile,"undo restores exact noncanonical original bytes");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),beforeScene,"undo all instances and bases");AE_EXPECT_EQ(session.assets().serialize(),beforeRegistry,"undo registry");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo Apply");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),afterScene,"redo scene exact");AE_EXPECT_EQ(session.assets().serialize(),afterRegistry,"redo registry exact");
  const auto scenePath=(path/"main.aescene").string();AE_EXPECT_TRUE(session.save(scenePath.c_str(),7),"save");
  EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(path.string().c_str()),"reopen project");
  AE_EXPECT_TRUE(EditorImportTransaction::read(path/".astra/assets.astra",bytes) && reopened.loadAssets(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size())) && reopened.load(scenePath.c_str(),7),"reload registry plus scene");
  AE_EXPECT_TRUE(reopened.inspectPrefabOverrides(thirdChild,view,error),error.c_str());
  const auto conflict=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="offset_x";});
  AE_EXPECT_TRUE(conflict!=view.rows.end() && conflict->origin==PrefabOverrideOrigin::Conflict && conflict->applyable,"conflict stays explicit across reopening");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",afterFile+"\n"),"external byte-only edit");
  AE_EXPECT_TRUE(!session.history().undo(session.document()),"undo cannot overwrite changed file bytes");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),afterScene,"failed undo leaves all instances unchanged");
}

AE_TEST(prefab_apply_conflict_requires_choice_and_batch_does_not_publish_other_properties) {
  namespace fs=std::filesystem;const auto path=fs::temp_directory_path()/("astra-prefab-choice-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");Fixture f;f.configure();session.document()=f.scene;std::string error;
  const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  auto value=*session.document().find(f.child);auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
  follow->dampingSeconds=2;follow->offset[0]=4;session.document().applyEntityValues(f.child,value);
  Prefab source;session.loadPrefab(asset,source,error);auto graph=source.graph();value=*graph.find(f.child);
  follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->dampingSeconds=1;graph.applyEntityValues(f.child,value);
  AE_EXPECT_TRUE(source.capture(graph,f.root,asset,error) && EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",source.write()),"incoming conflict");
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  const auto damping=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="damping_seconds";});
  const auto x=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="offset_x";});
  AE_EXPECT_TRUE(damping!=view.rows.end() && x!=view.rows.end() && damping->origin==PrefabOverrideOrigin::Conflict,"conflict classified");
  const auto before=serializeEditorDocument(session.document(),7);const auto hash=session.assets().serialize();
  const usize rows[]{static_cast<usize>(damping-view.rows.begin()),static_cast<usize>(x-view.rows.begin())};
  AE_EXPECT_TRUE(!session.applyPrefabOverrides(view,rows,false,error),"conflict rejected without explicit choice");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"rejected batch no scene changes");AE_EXPECT_EQ(session.assets().serialize(),hash,"rejected batch no registry changes");
  AE_EXPECT_TRUE(session.applyPrefabOverrides(view,rows,true,error),error.c_str());
  AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());const auto *published=static_cast<const scene::CameraFollow*>(source.graph().find(f.child)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(published->dampingSeconds==2 && published->offset[0]==4 && published->offset[1]==2,"exact chosen addresses published");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error) && view.rows.empty(),"applied values converge to source and fresh base");
}

AE_TEST(prefab_apply_refuses_structure_anywhere_and_external_reference_id_collision_without_mutation) {
  namespace fs=std::filesystem;const auto path=fs::temp_directory_path()/("astra-prefab-reject-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");Fixture f;f.configure();session.document()=f.scene;std::string error;
  const auto asset=session.createPrefab(f.root,error);AE_EXPECT_TRUE(asset.valid(),error.c_str());
  const auto second=session.instantiatePrefab(asset,session.document().root(),error);AE_EXPECT_TRUE(second,error.c_str());
  auto value=*session.document().find(f.child);auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->offset[0]=5;session.document().applyEntityValues(f.child,value);
  const auto extra=session.document().createEntity(second,ObjectKind::Folder,"LocalExtra");PrefabOverrideView view;
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());
  auto row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="offset_x";});
  AE_EXPECT_TRUE(row!=view.rows.end() && !row->applyable && !row->applyReason.empty(),"other loaded instance structural gap disables Apply");
  const auto before=serializeEditorDocument(session.document(),7),registry=session.assets().serialize();
  AE_EXPECT_TRUE(!session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error),"entire asset operation refused");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),before,"no partial propagation");AE_EXPECT_EQ(session.assets().serialize(),registry,"registry unchanged");
  session.document().destroyEntity(extra);
  const auto secondChild=session.document().childrenOf(second).front();value=*session.document().find(secondChild);
  follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));follow->target=f.root;session.document().applyEntityValues(secondChild,value);
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(secondChild,view,error),error.c_str());
  row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="target";});AE_EXPECT_TRUE(row!=view.rows.end(),"external authoring reference");
  const auto referencedBefore=serializeEditorDocument(session.document(),7);
  AE_EXPECT_TRUE(!session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error),"external target whose scene ID equals a source ID is refused rather than rebound");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),7),referencedBefore,"external-reference refusal transactional");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(f.child,view,error),error.c_str());row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &v){return v.field=="offset_x";});
  std::vector<u8> file;AE_EXPECT_TRUE(EditorImportTransaction::read(path/"Prefabs/Rig.prefab",file),"read file");
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(path/"Prefabs/Rig.prefab",std::string(file.begin(),file.end())+"\n"),"byte-only source change");
  AE_EXPECT_TRUE(row!=view.rows.end() && !session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error),"stale byte hash refused");
}

#include "scene/path.h"
AE_TEST(prefab_batch_revert_removes_component_and_its_reference_in_one_transaction) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("astra-prefab-batch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(path.string().c_str()),"project");
  Fixture fixture;fixture.configure();session.document()=fixture.scene;std::string error;
  AE_EXPECT_TRUE(session.createPrefab(fixture.root,error).valid(),error.c_str());
  auto &document=session.document();auto value=*document.find(fixture.root);
  const auto *added=value.components.add(scene::CameraFollow::descriptor);const auto addedId=added->instanceId();
  auto *script=static_cast<scene::ScriptBehavior*>(value.components.edit(scene::ScriptBehavior::descriptor));
  script->setProperty("localReference","component:astra.camera.follow",scene::scriptComponentValue(fixture.root,addedId));
  AE_EXPECT_TRUE(document.applyEntityValues(fixture.root,value),"local component and reference");
  PrefabOverrideView view;AE_EXPECT_TRUE(session.inspectPrefabOverrides(fixture.root,view,error),error.c_str());
  std::vector<usize> rows;for(usize i=0;i<view.rows.size();++i) rows.push_back(i);
  const auto before=serializeEditorDocument(document,7);const auto depth=session.history().undoDepth();
  auto invalid=rows;invalid.push_back(view.rows.size());
  AE_EXPECT_TRUE(!session.revertPrefabOverrides(view,invalid,error),"one invalid address rejects entire batch");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"rejected batch preserves scene");
  AE_EXPECT_EQ(session.history().undoDepth(),depth,"rejected batch preserves history");
  std::reverse(rows.begin(),rows.end());rows.push_back(rows.front());
  AE_EXPECT_TRUE(session.revertPrefabOverrides(view,rows,error),error.c_str());
  AE_EXPECT_TRUE(!document.find(fixture.root)->components.findInstance(addedId),"component and reference reverted together");
  AE_EXPECT_EQ(session.history().undoDepth(),depth+1,"one transaction for the batch");
  AE_EXPECT_TRUE(session.history().undo(document),"undo batch");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"undo restores component, reference and baseline");
  AE_EXPECT_TRUE(session.history().redo(document),"redo batch");
  PrefabOverrideView clean;AE_EXPECT_TRUE(session.inspectPrefabOverrides(fixture.root,clean,error)&&clean.rows.empty(),"no overrides remain");
}

AE_TEST(prefab_path_selective_values_and_atomic_structure_preserve_identity_floors){
  namespace fs=std::filesystem;
  const auto directory=fs::temp_directory_path()/("astra-prefab-path-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(directory);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{directory};
  EditorSession session;
  AE_EXPECT_TRUE(session.setProjectDirectory(directory.string().c_str()),"project");
  auto &document=session.document();
  const auto root=document.createEntity(document.root(),ObjectKind::Folder,"Path");
  auto value=*document.find(root);
  auto *path=static_cast<scene::Path*>(value.components.add(scene::Path::descriptor));
  resources::CurvePoint3D point;u64 first=0,second=0;
  path->insertPoint(0,point,first);point.position[0]=10;path->insertPoint(0,point,second);
  document.applyEntityValues(root,value);
  std::string error;const auto asset=session.createPrefab(root,error);
  AE_EXPECT_TRUE(asset.valid(),error.c_str());
  value=*document.find(root);path=static_cast<scene::Path*>(value.components.edit(scene::Path::descriptor));
  point=*path->point(first);point.position[1]=3;path->editPoint(first,point);document.applyEntityValues(root,value);
  PrefabOverrideView view;
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  const auto row=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &r){return r.field=="point_position_y"&&r.slot==0;});
  AE_EXPECT_TRUE(row!=view.rows.end()&&row->applyable,"same point identities permit selective numeric apply");
  AE_EXPECT_TRUE(session.applyPrefabOverride(view,static_cast<usize>(row-view.rows.begin()),false,error),error.c_str());
  Prefab source;AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());
  const auto *sourcePath=static_cast<const scene::Path*>(source.graph().find(root)->components.find(scene::Path::descriptor));
  AE_EXPECT_TRUE(sourcePath->point(first)->position[1]==3&&sourcePath->point(second)->position[1]==0,"only addressed point identity changed");
  AE_EXPECT_TRUE(session.history().undo(document)&&session.history().redo(document),"numeric Apply history remains reversible");
  value=*document.find(root);path=static_cast<scene::Path*>(value.components.edit(scene::Path::descriptor));
  point=*path->point(first);point.position[1]=7;path->editPoint(first,point);document.applyEntityValues(root,value);
  auto changed=source.graph();value=*changed.find(root);path=static_cast<scene::Path*>(value.components.edit(scene::Path::descriptor));
  AE_EXPECT_TRUE(path->movePoint(second,first),"external source reorder preserves point ids");
  changed.applyEntityValues(root,value);
  AE_EXPECT_TRUE(source.capture(changed,root,asset,error),error.c_str());
  const auto bytes=source.write();const auto record=*session.assets().find(asset);
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(directory/record.path,bytes),"external reordered source");
  const auto before=serializeEditorDocument(document,7);const auto depth=session.history().undoDepth();
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  const auto structural=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &r){return r.kind==PrefabOverrideKind::Component;});
  AE_EXPECT_TRUE(structural!=view.rows.end()&&structural->applyable&&structural->applicable&&structural->origin==PrefabOverrideOrigin::Conflict,"collection shown atomic; explicit conflict choice required");
  AE_EXPECT_TRUE(!session.applyPrefabOverride(view,static_cast<usize>(structural-view.rows.begin()),false,error),"conflict cannot silently overwrite reordered source");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"refusal preserves local authoring and prefab baseline");
  AE_EXPECT_EQ(session.history().undoDepth(),depth,"refusal preserves history");
  std::vector<u8> saved;AE_EXPECT_TRUE(EditorImportTransaction::read(directory/record.path,saved),"source still readable");
  AE_EXPECT_EQ(std::string(saved.begin(),saved.end()),bytes,"refusal does not publish source");
  AE_EXPECT_TRUE(session.applyPrefabOverride(view,static_cast<usize>(structural-view.rows.begin()),true,error),error.c_str());
  AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());sourcePath=static_cast<const scene::Path*>(source.graph().find(root)->components.find(scene::Path::descriptor));
  AE_EXPECT_TRUE(sourcePath->point(first)->position[1]==7 && sourcePath->curve.points.front().id==first,"explicit Apply publishes complete local collection and its order");
  AE_EXPECT_TRUE(session.history().undo(document),"whole collection Undo");
  AE_EXPECT_EQ(serializeEditorDocument(document,7),before,"Undo restores exact scene");
  AE_EXPECT_TRUE(EditorImportTransaction::read(directory/record.path,saved) && std::string(saved.begin(),saved.end())==bytes,"Undo restores source bytes");
  AE_EXPECT_TRUE(session.history().redo(document),"whole collection Redo");
  AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());changed=source.graph();value=*changed.find(root);path=static_cast<scene::Path*>(value.components.edit(scene::Path::descriptor));
  ++path->nextPointId;const auto floor=path->nextPointId;changed.applyEntityValues(root,value);
  AE_EXPECT_TRUE(source.capture(changed,root,asset,error)&&EditorImportTransaction::writeText(directory/record.path,source.write()),"source retired another element identity");
  value=*document.find(root);path=static_cast<scene::Path*>(value.components.edit(scene::Path::descriptor));point=*path->point(first);point.position[1]=9;path->editPoint(first,point);document.applyEntityValues(root,value);
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error),error.c_str());
  const auto scalar=std::find_if(view.rows.begin(),view.rows.end(),[](const auto &r){return r.field=="point_position_y"&&r.slot==0;});
  AE_EXPECT_TRUE(scalar!=view.rows.end() && scalar->applyable && scalar->origin==PrefabOverrideOrigin::Local,"retirement bookkeeping does not create a false structural conflict");
  AE_EXPECT_TRUE(session.applyPrefabOverride(view,static_cast<usize>(scalar-view.rows.begin()),false,error),error.c_str());
  AE_EXPECT_TRUE(session.loadPrefab(asset,source,error),error.c_str());sourcePath=static_cast<const scene::Path*>(source.graph().find(root)->components.find(scene::Path::descriptor));
  AE_EXPECT_TRUE(sourcePath->nextPointId>=floor && sourcePath->point(first)->position[1]==9,"scalar Apply cannot rewind source identity floor");
  AE_EXPECT_TRUE(session.inspectPrefabOverrides(root,view,error) && view.rows.empty() && !view.hasSourceChanges,"published collection is clean, including normalized bookkeeping");
}
