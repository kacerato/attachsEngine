#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "runtime/transform_math.h"
#include "runtime/physics_field2d_sample.h"
using namespace ae;
namespace {
bool near(float a,float b){return std::abs(a-b)<.001f;}
bool same(const float *a,const float *b){for(u32 i=0;i<16;++i)if(!near(a[i],b[i]))return false;return true;}
scene::ScriptSceneAccess access;
scene::ScriptRuntimeApi api(){scene::ScriptRuntimeApi a;a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *s){access=*s;return s->available()?0:1;};a.update=[](float){return 0;};a.fixedUpdate=a.lateUpdate=a.update;a.stop=[]{};a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;}
}
AE_TEST(family_base_object_clone_groups_references_archive_and_lifecycle){
 editor::EditorDocument g;auto source=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Composition");auto target=g.createEntity(source,runtime::ObjectKind::Folder,"Target");auto camera=g.createEntity(source,runtime::ObjectKind::Camera,"Camera");auto v=*g.find(camera);v.components.add(scene::Camera::descriptor);auto *follow=static_cast<scene::CameraFollow*>(v.components.add(scene::CameraFollow::descriptor));follow->target=target;v.groups.add("actors");v.layer=7;g.applyEntityValues(camera,v);
 runtime::GameWorld w;AE_EXPECT_TRUE(w.load(g),"real world");runtime::WorldStatus status;runtime::ObjectCloneMap mapping;auto clone=w.instantiate(w.handle(source),w.handle(g.root()),mapping,status);
 AE_EXPECT_TRUE(status==runtime::WorldStatus::Ok&&w.finishInstantiation(clone,true)==runtime::WorldStatus::Ok,"real publication");
 const auto *copied=w.graph().find(mapping.at(camera));const auto *ref=static_cast<const scene::CameraFollow*>(copied->components.find(scene::CameraFollow::descriptor));
 AE_EXPECT_TRUE(ref->target==mapping.at(target)&&copied->layer==7&&copied->groups.contains("actors"),"identity remapping and metadata");
 AE_EXPECT_TRUE(w.setActive(clone,false)==runtime::WorldStatus::Ok&&!w.activeInHierarchy(w.handle(copied->id))&&w.activeSelf(w.handle(copied->id)),"active self vs hierarchy");
 runtime::ObjectCloneMap archiveMapping;AE_EXPECT_TRUE(g.cloneSubtree(source,g.root(),archiveMapping),"authoring clone uses same remapping implementation");
 editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,123),123,reopened),"archive authored clone");
 AE_EXPECT_TRUE(reopened.find(archiveMapping.at(camera))->groups.contains("actors")&&reopened.find(archiveMapping.at(camera))->layer==7,"metadata persists");
 auto old=w.handle(mapping.at(target));AE_EXPECT_TRUE(w.destroyObject(clone)==runtime::WorldStatus::Ok&&w.validate(old)==runtime::WorldStatus::StaleHandle,"descendants stale immediately");AE_EXPECT_TRUE(w.flush()==1&&!w.graph().exists(clone.id),"storage removed at safe point");
}
AE_TEST(family_base_reparent_revalidates_physics_ownership_at_safe_point){
 for(auto policy:{runtime::ReparentPosePolicy::KeepLocal,runtime::ReparentPosePolicy::KeepWorld})for(auto authority:{runtime::TransformAuthority::PhysicsBody,runtime::TransformAuthority::PhysicsBody2D,runtime::TransformAuthority::Character}){
  editor::EditorDocument g;auto a=g.createEntity(g.root(),runtime::ObjectKind::Folder,"A"),b=g.createEntity(g.root(),runtime::ObjectKind::Folder,"B"),child=g.createEntity(a,runtime::ObjectKind::Folder,"Child");runtime::GameWorld w;AE_EXPECT_TRUE(w.load(g),"world");u64 ticket=0;
  AE_EXPECT_TRUE(w.setParent(w.handle(a),w.handle(b),0,policy,&ticket)==runtime::WorldStatus::Ok,"queue before ownership changes");w.setAuthority(child,authority);
  AE_EXPECT_TRUE(w.flush()==0&&w.graph().find(a)->parent==g.root(),"ownership acquired before safe point forbids reparent");runtime::WorldOperationState state;runtime::WorldStatus result;
  AE_EXPECT_TRUE(w.operationResult(w.worldId(),ticket,state,result)==runtime::WorldStatus::Ok&&state==runtime::WorldOperationState::Failed&&result==runtime::WorldStatus::TransformOwnedByPhysics,"tracked failure, no partial mutation");
 }
}
AE_TEST(family_base_editor_reparent_exact_world_undo_and_shear_rejection){
 editor::EditorSession session;auto &g=session.document();auto a=g.createEntity(g.root(),runtime::ObjectKind::Folder,"A"),b=g.createEntity(g.root(),runtime::ObjectKind::Folder,"B"),child=g.createEntity(a,runtime::ObjectKind::Folder,"Child");runtime::Transform t;t.position[0]=10;t.rotationDegrees[1]=30;t.scale[0]=t.scale[1]=t.scale[2]=2;g.setTransform(a,t);t.position[0]=-3;t.rotationDegrees[1]=-20;g.setTransform(b,t);t={};t.position[0]=2;t.rotationDegrees[2]=35;g.setTransform(child,t);float before[16],after[16];runtime::worldMatrix(g,child,before);
 editor::EditorActionRequest request;request.version=session.sceneVersion();request.action=editor::EditorAction::Reparent;request.entity=child;request.parent=b;
 AE_EXPECT_TRUE(session.dispatch(request).status==editor::EditorActionStatus::Applied,"actual editor action");runtime::worldMatrix(g,child,after);AE_EXPECT_TRUE(same(before,after),"world matrix preserved");
 AE_EXPECT_TRUE(session.history().undo(g)&&g.find(child)->parent==a,"undo parent");runtime::worldMatrix(g,child,after);AE_EXPECT_TRUE(same(before,after),"undo world");AE_EXPECT_TRUE(session.history().redo(g),"redo");
 t=g.find(b)->transform;t.scale[0]=1;t.scale[1]=3;g.setTransform(b,t);runtime::worldMatrix(g,child,before);const auto depth=session.history().undoDepth();
 AE_EXPECT_TRUE(!session.history().reparentKeepingWorld(g,child,g.root())&&session.history().undoDepth()==depth&&g.find(child)->parent==b,"unrepresentable shear fails atomically");runtime::worldMatrix(g,child,after);AE_EXPECT_TRUE(same(before,after),"no lossy decomposition");
}
AE_TEST(family_base_actual_layer_callback_changes_box2d_membership_and_invalidates){
 editor::EditorDocument g;auto area=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Wind"),body=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Body");auto v=*g.find(area);auto *field=v.components.add(scene::WindField2D::descriptor);const auto instance=field->instanceId();auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));script->scriptType="BaseProbe";script->source="BaseProbe.cs";g.applyEntityValues(area,v);v=*g.find(body);v.components.add(scene::Body2D::descriptor);v.components.add(scene::Collider2D::descriptor);g.applyEntityValues(body,v);
 editor::EditorMapScene map;editor::EditorPlayScene play;play.setScriptRuntime(api(),"/test");AE_EXPECT_TRUE(play.start(g,map),"real bridge and Box2D");auto owner=play.world().handle(area),actor=play.world().handle(body);
 AE_EXPECT_TRUE(access.version==scene::ScriptSceneAccess{}.version&&access.objectLayer(access.context,body,actor.world,actor.generation,-1)==0,"ABI35 reads real layer");
 AE_EXPECT_TRUE(access.objectLayer(access.context,body,actor.world,actor.generation,7)==7&&play.world().graph().find(body)->layer==7,"SDK transport changes model");play.world().setProperty({owner,instance},"affected_layer",u32{8});float point[3]{};scene::ScriptFieldState sample;
 AE_EXPECT_TRUE(access.fieldQuery(access.context,area,owner.world,owner.generation,instance,2,point,0,&sample)&&sample.affectedBodies==1,"new layer reaches actual Box2D field filter");
 AE_EXPECT_TRUE(access.objectLayer(access.context,body,actor.world,actor.generation,2)==2&&access.fieldQuery(access.context,area,owner.world,owner.generation,instance,2,point,0,&sample)&&sample.affectedBodies==0,"changing layer changes actual membership");
 AE_EXPECT_TRUE(access.objectLayer(access.context,body,actor.world,actor.generation,32)==-1&&play.world().graph().find(body)->layer==2,"invalid layer unchanged");
 AE_EXPECT_TRUE(access.objectLayer(access.context,body,actor.world+1,actor.generation,-1)==-1&&access.objectLayer(access.context,body,actor.world,actor.generation+1,-1)==-1,"world and generation checked");const auto old=access;play.stop();AE_EXPECT_TRUE(old.objectLayer(old.context,body,actor.world,actor.generation,-1)==-1,"stopped callback fails");
}
