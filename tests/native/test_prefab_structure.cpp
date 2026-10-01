#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_transaction.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_paths.h"
#include "scene/timer.h"
#include "scene/path.h"
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/script_behavior.h"
#include "scene/prefab_link.h"
#include <chrono>
#include <filesystem>
using namespace ae;using namespace ae::editor;using namespace ae::runtime;
namespace {
struct StructureFixture {
  std::filesystem::path path=std::filesystem::temp_directory_path()/("astra-prefab-structure-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  EditorSession session;ObjectId root=0,child=0,second=0;u64 timer=0,pathId=0;resources::AssetGuid asset;std::string error;
  bool initialize() {
    std::filesystem::create_directories(path);if(!session.setProjectDirectory(path.string().c_str()))return false;
    auto &d=session.document();root=d.createEntity(d.root(),ObjectKind::Folder,"Structure");child=d.createEntity(root,ObjectKind::Folder,"Receiver");
    auto v=*d.find(root);timer=v.components.add(scene::Timer::descriptor)->instanceId();pathId=v.components.add(scene::Path::descriptor)->instanceId();
    if(!d.applyEntityValues(root,v))return false;
    asset=session.createPrefab(root,error);if(!asset.valid())return false;
    second=session.instantiatePrefab(asset,d.root(),error);session.history().clear();return second!=0;
  }
  ~StructureFixture(){std::error_code ec;std::filesystem::remove_all(path,ec);}
  std::string file()const{std::vector<u8>b;EditorImportTransaction::read(path/"Prefabs/Structure.prefab",b);return {b.begin(),b.end()};}
  usize row(PrefabOverrideView &v,PrefabOverrideKind kind,u64 id=0){for(usize i=0;i<v.rows.size();++i)if(v.rows[i].kind==kind && (!id||v.rows[i].component==id))return i;return v.rows.size();}
};
}
AE_TEST(prefab_collection_whole_apply_preserves_sibling_conflict_ids_archive_and_runtime) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();
  const auto third=f.session.instantiatePrefab(f.asset,d.root(),f.error);AE_EXPECT_TRUE(third,f.error.c_str());
  u64 first=0,second=0;
  auto v=*d.find(f.root);auto *path=static_cast<scene::Path*>(v.components.editInstance(f.pathId));resources::CurvePoint3D p;
  AE_EXPECT_TRUE(path->insertPoint(0,p,first),"first authored point");p.position[0]=7;AE_EXPECT_TRUE(path->insertPoint(0,p,second),"second authored point");d.applyEntityValues(f.root,v);
  v=*d.find(f.second);path=static_cast<scene::Path*>(v.components.editInstance(f.pathId));p.position[0]=9;u64 local=0;
  AE_EXPECT_TRUE(path->insertPoint(0,p,local),"independent sibling branch");d.applyEntityValues(f.second,v);
  const auto before=serializeEditorDocument(d,7),beforeFile=f.file();PrefabOverrideView view;
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());PrefabApplyReport report;
  const auto row=f.row(view,PrefabOverrideKind::Component,f.pathId);
  AE_EXPECT_TRUE(row<view.rows.size() && view.rows[row].applyable,"complete collection can be applied");
  AE_EXPECT_TRUE(f.session.applyPrefabOverride(view,row,false,f.error,&report),f.error.c_str());
  const auto read=[&](ObjectId id){return static_cast<const scene::Path*>(d.find(id)->components.findInstance(f.pathId));};
  AE_EXPECT_TRUE(read(third)->curve.points.size()==2 && read(third)->curve.points[1].id==second,"inherited sibling receives complete stable identities");
  AE_EXPECT_TRUE(read(f.second)->curve.points.size()==1 && read(f.second)->curve.points.front().position[0]==9 && read(f.second)->nextPointId>=read(f.root)->nextPointId,"local collection retained, retirement floor advanced");
  AE_EXPECT_TRUE(report.conflicts==1 && f.session.inspectPrefabOverrides(f.second,view,f.error),"conflict remains inspectable");
  auto conflict=f.row(view,PrefabOverrideKind::Component,f.pathId);
  AE_EXPECT_TRUE(conflict<view.rows.size() && view.rows[conflict].origin==PrefabOverrideOrigin::Conflict,"whole collection conflict");
  const auto after=serializeEditorDocument(d,7);AE_EXPECT_TRUE(f.session.history().undo(d) && serializeEditorDocument(d,7)==before && f.file()==beforeFile,"atomic source and scene Undo");
  AE_EXPECT_TRUE(f.session.history().redo(d) && serializeEditorDocument(d,7)==after,"Redo exact");
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(after,7,reopened),"archive reopened");d=std::move(reopened);
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.second,view,f.error),f.error.c_str());
  conflict=f.row(view,PrefabOverrideKind::Component,f.pathId);
  AE_EXPECT_TRUE(conflict<view.rows.size() && view.rows[conflict].origin==PrefabOverrideOrigin::Conflict,"conflict survives archive");
  GameWorld world;AE_EXPECT_TRUE(world.load(d),"real runtime graph");ScenePaths paths;std::array<float,3> position{},tangent{};
  AE_EXPECT_TRUE(paths.sample(world,third,3,position,tangent) && std::abs(position[0]-3)<.05f,"runtime path cache consumes propagated collection");
  AE_EXPECT_TRUE(f.session.revertPrefabOverride(view,f.row(view,PrefabOverrideKind::Component,f.pathId),f.error),f.error.c_str());
  AE_EXPECT_TRUE(read(f.second)->curve.points.size()==2,"explicit Receive replaces conflicted collection");
}
AE_TEST(prefab_structure_add_order_roundtrip_undo_and_real_timer_consumer) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();auto v=*d.find(f.root);
  auto *timer=static_cast<scene::Timer*>(v.components.add(scene::Timer::descriptor));const auto added=timer->instanceId();
  timer->intervalSeconds=.1f;timer->repeat=false;timer->elapsedAction=2;timer->elapsedTarget=f.child;
  v.components.moveInstance(f.pathId,0);AE_EXPECT_TRUE(d.applyEntityValues(f.root,v),"author addition and order");
  const auto before=serializeEditorDocument(d,7),beforeFile=f.file();PrefabOverrideView view;
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  const usize chosen[]{f.row(view,PrefabOverrideKind::AddedComponent,added),f.row(view,PrefabOverrideKind::ComponentOrder)};
  AE_EXPECT_TRUE(chosen[0]<view.rows.size() && chosen[1]<view.rows.size(),"both structural addresses exposed");
  PrefabApplyReport report;AE_EXPECT_TRUE(f.session.applyPrefabOverrides(view,chosen,false,f.error,&report),f.error.c_str());
  AE_EXPECT_EQ(report.instances,2u,"both loaded instances publish atomically");
  const auto secondChild=d.childrenOf(f.second).front();const auto *published=static_cast<const scene::Timer*>(d.find(f.second)->components.findInstance(added));
  AE_EXPECT_TRUE(published && published->elapsedTarget==secondChild && published->intervalSeconds==.1f,"new component reference remapped");
  AE_EXPECT_EQ(d.find(f.second)->components.at(0)->instanceId(),f.pathId,"shared component order propagated");
  const auto after=serializeEditorDocument(d,7);AE_EXPECT_TRUE(f.session.history().undo(d),"undo resource transaction");
  AE_EXPECT_EQ(serializeEditorDocument(d,7),before,"undo scene exact");AE_EXPECT_EQ(f.file(),beforeFile,"undo source exact");
  AE_EXPECT_TRUE(f.session.history().redo(d),"redo");AE_EXPECT_EQ(serializeEditorDocument(d,7),after,"redo exact");
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(after,7,reopened),"scene archive reopened");
  GameWorld world;AE_EXPECT_TRUE(world.load(reopened),"actual runtime graph");SceneTimers timers;unsigned fired=0;
  AE_EXPECT_TRUE(timers.advance(world,.11,[&](ObjectId,u64,u32){++fired;return true;}),"real timer consumer");
  AE_EXPECT_TRUE(fired==2 && !world.activeSelf(world.handle(f.child)) && !world.activeSelf(world.handle(secondChild)),"propagated authored actions execute independently");
}
AE_TEST(prefab_structure_remove_preserves_modified_sibling_as_persistent_conflict) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();auto v=*d.find(f.root);
  v.components.removeInstance(f.timer);AE_EXPECT_TRUE(d.applyEntityValues(f.root,v),"remove local timer");
  v=*d.find(f.second);static_cast<scene::Timer*>(v.components.editInstance(f.timer))->intervalSeconds=2;d.applyEntityValues(f.second,v);
  PrefabOverrideView view;AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  AE_EXPECT_TRUE(f.session.applyPrefabOverride(view,f.row(view,PrefabOverrideKind::RemovedComponent,f.timer),false,f.error),f.error.c_str());
  AE_EXPECT_TRUE(d.find(f.second)->components.findInstance(f.timer) && !d.find(f.root)->components.findInstance(f.timer),"local modified sibling retained");
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.second,view,f.error),f.error.c_str());auto row=f.row(view,PrefabOverrideKind::AddedComponent,f.timer);
  AE_EXPECT_TRUE(row<view.rows.size() && view.rows[row].origin==PrefabOverrideOrigin::Conflict,"membership conflict remains explicit");
  const auto saved=serializeEditorDocument(d,7);EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(saved,7,reopened),"reopen");d=std::move(reopened);
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.second,view,f.error),f.error.c_str());row=f.row(view,PrefabOverrideKind::AddedComponent,f.timer);
  AE_EXPECT_TRUE(row<view.rows.size() && view.rows[row].origin==PrefabOverrideOrigin::Conflict,"conflict persists in saved base");
  AE_EXPECT_TRUE(f.session.revertPrefabOverride(view,row,f.error),f.error.c_str());
  AE_EXPECT_TRUE(!d.find(f.second)->components.findInstance(f.timer),"receive source removal explicitly");
}
AE_TEST(prefab_structure_concurrent_additions_require_choice_without_overwriting_local_data) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();u64 added=0;
  for(const auto id:{f.root,f.second}){auto v=*d.find(id);auto *t=static_cast<scene::Timer*>(v.components.add(scene::Timer::descriptor));added=t->instanceId();t->intervalSeconds=id==f.root?3:4;d.applyEntityValues(id,v);}
  PrefabOverrideView view;AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  AE_EXPECT_TRUE(f.session.applyPrefabOverride(view,f.row(view,PrefabOverrideKind::AddedComponent,added),false,f.error),f.error.c_str());
  AE_EXPECT_EQ(static_cast<const scene::Timer*>(d.find(f.second)->components.findInstance(added))->intervalSeconds,4.f,"concurrent addition retained");
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.second,view,f.error),f.error.c_str());const auto row=f.row(view,PrefabOverrideKind::Component,added);
  AE_EXPECT_TRUE(row<view.rows.size() && view.rows[row].origin==PrefabOverrideOrigin::Conflict,"birth conflict uses whole component scope");
  const auto scene=serializeEditorDocument(d,7),file=f.file();AE_EXPECT_TRUE(!f.session.applyPrefabOverride(view,row,false,f.error),"explicit conflict choice required");
  AE_EXPECT_EQ(serializeEditorDocument(d,7),scene,"rejected scene untouched");AE_EXPECT_EQ(f.file(),file,"rejected file untouched");
  AE_EXPECT_TRUE(f.session.revertPrefabOverride(view,row,f.error),f.error.c_str());
  AE_EXPECT_EQ(static_cast<const scene::Timer*>(d.find(f.second)->components.findInstance(added))->intervalSeconds,3.f,"receive complete source addition");
}
AE_TEST(prefab_structure_dependency_and_external_reference_failures_are_atomic) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();auto v=*d.find(f.root);
  auto *follow=v.components.add(scene::CameraFollow::descriptor);const auto followId=follow->instanceId();d.applyEntityValues(f.root,v);
  PrefabOverrideView view;AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  const auto blocked=f.row(view,PrefabOverrideKind::AddedComponent,followId);
  AE_EXPECT_TRUE(blocked<view.rows.size() && !view.rows[blocked].applyable && !view.rows[blocked].applyReason.empty(),"inspector states real dependency preflight");
  const auto file=f.file(),scene=serializeEditorDocument(d,7);AE_EXPECT_TRUE(!f.session.applyPrefabOverride(view,blocked,false,f.error),"missing real camera dependency refused");
  AE_EXPECT_EQ(f.file(),file,"source preserved");AE_EXPECT_EQ(serializeEditorDocument(d,7),scene,"scene preserved");
  v=*d.find(f.root);const auto cameraId=v.components.add(scene::Camera::descriptor)->instanceId();d.applyEntityValues(f.root,v);
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  const usize dependencies[]{f.row(view,PrefabOverrideKind::AddedComponent,followId),f.row(view,PrefabOverrideKind::AddedComponent,cameraId)};
  AE_EXPECT_TRUE(f.session.applyPrefabOverrides(view,dependencies,false,f.error),f.error.c_str());
  AE_EXPECT_TRUE(d.find(f.second)->components.find(scene::Camera::descriptor) && d.find(f.second)->components.find(scene::CameraFollow::descriptor),"complete real dependency chain propagated");
  const auto publishedFile=f.file();
  const auto external=d.createEntity(d.root(),ObjectKind::Folder,"External owner");v=*d.find(external);auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="Fixture";script->source="Fixture.cs";
  script->setProperty("newCamera","component:astra.camera",scene::scriptComponentValue(f.second,cameraId));
  script->setProperty("target","component:astra.time.timer",scene::scriptComponentValue(f.second,f.timer));d.applyEntityValues(external,v);
  const auto beforeUndo=serializeEditorDocument(d,7);
  AE_EXPECT_TRUE(!f.session.history().undo(d),"resource undo cannot invalidate a new external reference");
  AE_EXPECT_EQ(serializeEditorDocument(d,7),beforeUndo,"rejected undo atomic");AE_EXPECT_EQ(f.file(),publishedFile,"undo source preserved");
  v=*d.find(external);script=static_cast<scene::ScriptBehavior*>(v.components.edit(scene::ScriptBehavior::descriptor));script->setProperty("newCamera","component:astra.camera","0:0");d.applyEntityValues(external,v);
  v=*d.find(f.root);v.components.removeInstance(followId);v.components.removeInstance(f.timer);d.applyEntityValues(f.root,v);
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());const auto before=serializeEditorDocument(d,7);
  AE_EXPECT_TRUE(!f.session.applyPrefabOverride(view,f.row(view,PrefabOverrideKind::RemovedComponent,f.timer),false,f.error),"removal cannot dangle external typed reference");
  AE_EXPECT_EQ(f.file(),publishedFile,"no publication");AE_EXPECT_EQ(serializeEditorDocument(d,7),before,"no partial propagation");
}
AE_TEST(prefab_structure_external_source_addition_relocates_metadata_without_losing_ownership) {
  StructureFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();Prefab source;
  AE_EXPECT_TRUE(f.session.loadPrefab(f.asset,source,f.error),f.error.c_str());auto graph=source.graph();auto v=*graph.find(f.root);
  const auto added=v.components.add(scene::Timer::descriptor)->instanceId();
  AE_EXPECT_TRUE(d.find(f.root)->components.findInstance(added)->type().id=="astra.prefab.link","exercise actual source/metadata identity collision");
  static_cast<scene::Timer*>(v.components.editInstance(added))->intervalSeconds=3;
  graph.applyEntityValues(f.root,v);AE_EXPECT_TRUE(source.capture(graph,f.root,f.asset,f.error),f.error.c_str());
  AE_EXPECT_TRUE(EditorImportTransaction::writeText(f.path/"Prefabs/Structure.prefab",source.write()),"external source edit");
  PrefabOverrideView view;AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error),f.error.c_str());
  AE_EXPECT_TRUE(f.session.refreshPrefabInstance(view,f.error),f.error.c_str());
  AE_EXPECT_TRUE(d.find(f.root)->components.find(scene::PrefabLink::descriptor) && d.find(f.root)->components.findInstance(added)->type().id==scene::Timer::descriptor.id,"source addition and ownership both survive");
  AE_EXPECT_TRUE(f.session.inspectPrefabOverrides(f.root,view,f.error) && view.rows.empty(),"fresh coherent base");
  AE_EXPECT_TRUE(f.session.history().undo(d),"metadata relocation undo");
  AE_EXPECT_TRUE(d.find(f.root)->components.findInstance(added)->type().id=="astra.prefab.link","old ownership ID restored");
}
