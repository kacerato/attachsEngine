#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_component_visuals.h"
#include "runtime/scene_constraints.h"
#include "scene/component_reflection.h"
#include <sstream>
#include <filesystem>
#include "renderer/primitive_geometry.h"
#include "runtime/primitive_object.h"
#include "editor/editor_import_transaction.h"
using namespace ae;
namespace {
scene::ScriptSceneAccess bulkAccess;
scene::ScriptRuntimeApi bulkApi(){scene::ScriptRuntimeApi a;
 a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *s){bulkAccess=*s;return s->available()?0:1;};
 a.update=[](float){return 0;};a.fixedUpdate=a.update;a.lateUpdate=a.update;a.stop=[]{};a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;
}
template<class T> u64 spring(runtime::SceneGraph &g,runtime::ObjectId id,runtime::ObjectId source){auto v=*g.find(id);auto *c=static_cast<T*>(v.components.add(T::descriptor));c->target=source;const auto instance=c->instanceId();g.applyEntityValues(id,v);return instance;}
bool close(float a,float b,float epsilon=.002f){return std::abs(a-b)<epsilon;}
runtime::ObjectId body(editor::EditorDocument &d,const char *name,scene::BodyMotion motion,bool script=false){auto id=d.createEntity(d.root(),runtime::ObjectKind::Folder,name);auto v=*d.find(id);auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=motion;b->gravityFactor=0;b->linearDamping=b->angularDamping=0;v.components.add(scene::Collider::descriptor);if(script){auto *s=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));s->scriptType="Bulk50";s->source="Bulk50.cs";}d.applyEntityValues(id,v);return id;}
}

AE_TEST(bulk50_authoring_creation_history_archive_clone_and_consumers){
 AE_EXPECT_TRUE(scene::auditComponentContracts().empty(),"schema properties declare consumer and supported capability");
 editor::EditorSession session;auto &d=session.document();auto source=d.createEntity(d.root(),runtime::ObjectKind::Folder,"Fonte");runtime::Transform pose;pose.position[0]=10;pose.rotationDegrees[1]=90;pose.scale[0]=3;d.setTransform(source,pose);
 const char *recipes[]{"spring.position","spring.rotation","spring.scale"};std::array<runtime::ObjectId,3> followers{};
 for(u32 k=0;k<3;++k){session.setSelection(source);u32 recipe=0;AE_EXPECT_TRUE(editor::findCreationRecipe(recipes[k],&recipe),"real recipe");followers[k]=session.createRecipe(recipe,d.root());AE_EXPECT_TRUE(followers[k],"source-bound creation");}
 auto value=*d.find(followers[0]);auto *p=static_cast<scene::SpringPositionConstraint*>(value.components.edit(scene::SpringPositionConstraint::descriptor));p->frequency=2;p->dampingRatio=.7f;p->weight=.5f;p->maxSpeed=2;p->offset[0]=2;AE_EXPECT_TRUE(d.applyEntityValues(followers[0],value),"all response properties applied");
 const auto data=editor::serializeEditorDocument(d,123);editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(data,123,reopened),"three types persist");
 auto *saved=static_cast<const scene::SpringPositionConstraint*>(reopened.find(followers[0])->components.find(scene::SpringPositionConstraint::descriptor));AE_EXPECT_TRUE(saved&&saved->target==source&&close(saved->frequency,2)&&close(saved->maxSpeed,2)&&close(saved->offset[0],2)&&close(saved->dampingRatio,.7f),"actual payload values");
 AE_EXPECT_TRUE(session.history().undo(d)&&!d.exists(followers[2]),"undo recipe");AE_EXPECT_TRUE(session.history().redo(d)&&d.exists(followers[2]),"redo recipe");
 runtime::GameWorld w;AE_EXPECT_TRUE(w.load(reopened),"runtime from reopened scene");runtime::SceneConstraints eval;
 for(u32 n=0;n<120;++n)AE_EXPECT_TRUE(eval.advance(w,1.0/60),"advance actual consumer");
 runtime::Transform out;w.worldTransform(w.handle(followers[0]),out);AE_EXPECT_TRUE(out.position[0]>3.5f&&out.position[0]<4.01f,"speed cap is vector displacement per second");
 w.worldTransform(w.handle(followers[1]),out);AE_EXPECT_TRUE(close(out.rotationDegrees[1],90),"rotation consumed");w.worldTransform(w.handle(followers[2]),out);AE_EXPECT_TRUE(close(out.scale[0],3),"scale consumed");
 const auto visuals=editor::collectComponentVisuals(reopened,followers[0],1);AE_EXPECT_TRUE(!visuals.empty()&&visuals.front().segments.size()>=4,"native icon and source gizmo");
 AE_EXPECT_TRUE(reopened.find(followers[0])->transform.position[0]!=4,"runtime never writes authoring");
 runtime::SceneGraph clone;auto group=clone.createEntity(clone.root(),runtime::ObjectKind::Folder,"Group");auto a=clone.createEntity(group,runtime::ObjectKind::Folder,"Source"),b=clone.createEntity(group,runtime::ObjectKind::Folder,"Follower");spring<scene::SpringPositionConstraint>(clone,b,a);runtime::ObjectCloneMap map;clone.cloneSubtree(group,clone.root(),map);const auto *cloned=static_cast<const scene::SpringPositionConstraint*>(clone.find(map[b])->components.find(scene::SpringPositionConstraint::descriptor));AE_EXPECT_EQ(cloned->target,map[a],"clone remaps reflected reference");
}

AE_TEST(bulk50_spring_timestep_pause_reset_cycles_and_authority){
 runtime::SceneGraph g;auto target=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Target"),follower=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Follower");runtime::Transform goal;goal.position[0]=10;goal.rotationDegrees[1]=-179;g.setTransform(target,goal);runtime::Transform initial;initial.rotationDegrees[1]=179;g.setTransform(follower,initial);spring<scene::SpringPositionConstraint>(g,follower,target);spring<scene::SpringRotationConstraint>(g,follower,target);
 for(float damping:{0.f,.5f,1.f,3.f}){
  auto v=*g.find(follower);auto *p=static_cast<scene::SpringPositionConstraint*>(v.components.edit(scene::SpringPositionConstraint::descriptor));p->dampingRatio=damping;g.applyEntityValues(follower,v);
  runtime::GameWorld a,b;AE_EXPECT_TRUE(a.load(g)&&b.load(g),"load equal worlds");runtime::SceneConstraints ea,eb;for(int n=0;n<60;++n)ea.advance(a,1.0/60);for(int n=0;n<30;++n)eb.advance(b,1.0/30);
  runtime::Transform pa,pb;a.worldTransform(a.handle(follower),pa);b.worldTransform(b.handle(follower),pb);AE_EXPECT_TRUE(close(pa.position[0],pb.position[0],.005f),"analytical response is timestep invariant");float qa[4],qg[4];runtime::transformRotationQuaternion(pa,qa);runtime::transformRotationQuaternion(goal,qg);float dot=0;for(u32 k=0;k<4;++k)dot+=qa[k]*qg[k];AE_EXPECT_TRUE(std::abs(dot)>.99999f,"short angular path across wrap");
  const auto prior=pa;goal.position[0]=40;a.setWorldTransform(a.handle(target),goal);ea.advance(a,0);a.worldTransform(a.handle(follower),pa);AE_EXPECT_TRUE(close(pa.position[0],prior.position[0]),"paused clock holds spring output");
  a.setAuthority(follower,runtime::TransformAuthority::PhysicsBody);ea.advance(a,.1);a.worldTransform(a.handle(follower),pa);AE_EXPECT_TRUE(close(pa.position[0],prior.position[0])&&!ea.diagnostics().empty(),"physics authority preserved and diagnosed");
 }
 runtime::GameWorld w;AE_EXPECT_TRUE(w.load(g),"load lifecycle");runtime::SceneConstraints eval;eval.advance(w,.1);
 // External edit becomes new initial state, not the previous cached velocity.
 initial.position[0]=30;w.setWorldTransform(w.handle(follower),initial);eval.advance(w,0);runtime::Transform out;w.worldTransform(w.handle(follower),out);AE_EXPECT_TRUE(close(out.position[0],30),"external pose resets state");
 AE_EXPECT_TRUE(w.setProperty(w.findComponent(w.handle(follower),"astra.spring.position"),"enabled",false)==runtime::WorldStatus::Ok,"disable via real contract");eval.advance(w,.1);w.setProperty(w.findComponent(w.handle(follower),"astra.spring.position"),"enabled",true);eval.advance(w,0);w.worldTransform(w.handle(follower),out);AE_EXPECT_TRUE(close(out.position[0],30),"disable/re-enable discards old spring velocity");
 spring<scene::SpringPositionConstraint>(g,target,follower);runtime::GameWorld cycle;AE_EXPECT_TRUE(cycle.load(g),"cycle remains diagnosable draft");eval.reset();eval.advance(cycle,.1);AE_EXPECT_TRUE(!eval.diagnostics().empty()&&eval.diagnostics().front().issue==runtime::SceneConstraints::Issue::Cycle,"shared dependency graph detects cycle");
}

AE_TEST(bulk50_body_solver_properties_and_real_script_bridge_commands){
 editor::EditorDocument d;auto id=body(d,"Dynamic",scene::BodyMotion::Dynamic,true),staticId=body(d,"Static",scene::BodyMotion::Static);runtime::Transform staticPose;staticPose.position[0]=100;d.setTransform(staticId,staticPose);
 auto v=*d.find(id);auto *b=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));const auto instance=b->instanceId();b->freezePosition[2]=true;b->freezeRotation[0]=true;b->maxLinearVelocity=3;b->maxAngularVelocity=4;b->continuousCollision=true;b->solverVelocitySteps=12;AE_EXPECT_TRUE(d.applyEntityValues(id,v),"persisted simulation config");
 const auto payload=editor::serializeEditorDocument(d,55);editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(payload,55,reopened),"body v4 roundtrip");b=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));b->freezePosition[0]=b->freezePosition[1]=true;b->freezeRotation[1]=b->freezeRotation[2]=true;AE_EXPECT_TRUE(!b->valid(),"all locks explicitly rejected before Jolt");
 editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(bulkApi(),"/test");AE_EXPECT_TRUE(play.start(reopened,resources),"real solver and ScriptBridge");AE_EXPECT_TRUE(bulkAccess.version==scene::ScriptSceneAccess{}.version&&bulkAccess.available(),"complete versioned ABI");
 const auto handle=play.world().handle(id);float zero[3]{},linear[3]{1,0,0},angular[3]{0,2,0};scene::ScriptBodyState state;
 auto command=[&](u32 op,const float *a,const float *p){return bulkAccess.bodyCommand(bulkAccess.context,id,handle.world,handle.generation,instance,op,a,p,&state);};
 AE_EXPECT_TRUE(command(6,linear,angular),"atomic velocity pair");AE_EXPECT_TRUE(close(state.linear[0],1)&&close(state.angular[1],2),"actual solver velocities");float point[3]{1,0,0};AE_EXPECT_TRUE(command(9,point,zero)&&close(state.linear[2],-2),"point velocity includes angular cross lever arm");
 AE_EXPECT_TRUE(command(3,zero,zero)&&state.flags&8,"sleep solver body");AE_EXPECT_TRUE(command(2,zero,zero)&&state.flags&1,"wake solver body");
 AE_EXPECT_TRUE(command(5,linear,point),"off-center impulse");AE_EXPECT_TRUE(command(4,linear,point),"force at point");float delta[3]{.2f,0,0};AE_EXPECT_TRUE(command(7,delta,zero)&&command(8,delta,zero)&&command(1,angular,zero),"linear/angular change and setter");
 float fast[3]{100,0,100};AE_EXPECT_TRUE(command(6,fast,fast),"velocity limits applied atomically");float length=std::sqrt(state.linear[0]*state.linear[0]+state.linear[1]*state.linear[1]+state.linear[2]*state.linear[2]);AE_EXPECT_TRUE(length<=3.001f,"linear speed cap");AE_EXPECT_TRUE(play.advance(.1),"real fixed steps");AE_EXPECT_TRUE(command(0,zero,zero)&&close(state.linear[2],0)&&close(state.angular[0],0),"solver enforces authored world locks");
 // Rebuild retains the same component identity and reflects current velocities.
 AE_EXPECT_TRUE(play.world().setProperty(play.world().findComponent(handle,"astra.physics.body"),"max_angular_velocity",2.f)==runtime::WorldStatus::Ok,"live authoring edit");AE_EXPECT_TRUE(play.advance(.01)&&command(0,zero,zero),"safe-point rebuild retains instance access");
 state.reserved=1;AE_EXPECT_TRUE(!command(0,zero,zero),"invalid layout reservation rejected");state={};
 AE_EXPECT_TRUE(!bulkAccess.bodyCommand(bulkAccess.context,id,handle.world+1,handle.generation,instance,0,zero,zero,&state),"foreign world rejected");AE_EXPECT_TRUE(!bulkAccess.bodyCommand(bulkAccess.context,id,handle.world,handle.generation+1,instance,0,zero,zero,&state),"stale generation rejected");AE_EXPECT_TRUE(!bulkAccess.bodyCommand(bulkAccess.context,id,handle.world,handle.generation,instance+9999,0,zero,zero,&state),"component identity checked");
 auto stale=bulkAccess;play.stop();AE_EXPECT_TRUE(!stale.bodyCommand(stale.context,id,handle.world,handle.generation,instance,0,zero,zero,&state)&&stale.lastStatus(stale.context)==u32(runtime::WorldStatus::NotRunning),"retained callback after Stop refuses");
 // Old v3 body data loads with defaults, not field-order reinterpretation.
 std::stringstream old("2 1 0.5 0 0 0 0 0 0 0 0.05 0.05 1 0 1");scene::PhysicsBody migrated;AE_EXPECT_TRUE(migrated.read(old,3)&&migrated.valid()&&close(migrated.maxLinearVelocity,500),"v3 compatibility");
}

AE_TEST(bulk50_ccd_stops_fast_body_at_thin_wall){
 for(bool ccd:{false,true}){editor::EditorDocument d;auto projectile=body(d,"Projectile",scene::BodyMotion::Dynamic),wall=body(d,"Thin wall",scene::BodyMotion::Static);
  runtime::Transform pose;pose.position[0]=2;d.setTransform(wall,pose);auto v=*d.find(wall);auto *c=static_cast<scene::Collider*>(v.components.edit(scene::Collider::descriptor));c->halfX=.02f;c->halfY=c->halfZ=5;d.applyEntityValues(wall,v);
  v=*d.find(projectile);auto *b=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));b->continuousCollision=ccd;b->velocityX=300;b->allowSleep=false;c=static_cast<scene::Collider*>(v.components.edit(scene::Collider::descriptor));c->halfX=c->halfY=c->halfZ=.1f;d.applyEntityValues(projectile,v);
  runtime::GameWorld w;runtime::ScenePhysics physics;AE_EXPECT_TRUE(w.load(d)&&physics.start(w),"authoring into Jolt");AE_EXPECT_TRUE(physics.advance(1.0/60,w),"one fast step");runtime::Transform out;w.worldTransform(w.handle(projectile),out);if(ccd)AE_EXPECT_TRUE(out.position[0]<2,"CCD prevents tunneling");else AE_EXPECT_TRUE(out.position[0]>3,"same fast scene tunnels in discrete mode");
 }
}

// Export through real authoring/primitive/resource paths; refuse existing data.
int writeBulk50Project(const char *directory){
 namespace fs=std::filesystem;
 if(!directory||!*directory)return 2;
 std::error_code ec;auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
 if(ec||fs::is_symlink(fs::symlink_status(root,ec)))return 2;
 ec.clear();
 if(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))return 2;
 for(auto folder:{"Scripts","scenes"}){fs::create_directories(root/folder,ec);if(ec)return 2;}
 std::vector<u8> bytes;if(!editor::EditorImportTransaction::read(fs::path(AETHER_REPOSITORY_ROOT)/"tests/fixtures/physics/Bulk50Probe.cs",bytes)||!editor::EditorImportTransaction::write(root/"Scripts/Bulk50Probe.cs",bytes))return 2;
 editor::EditorSession session;if(!session.setProjectDirectory(root.generic_string().c_str()))return 2;
 std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
 if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)||!session.importMap(draws,materials,false,vertices,indices,0))return 2;
 auto &d=session.document();
 auto mesh=[&](const char *name,scene::PrimitiveType type,float x,float z,bool physics){auto id=d.createEntity(d.root(),runtime::ObjectKind::Folder,name);auto v=*d.find(id);const auto slot=static_cast<u32>(type);
  if(!runtime::configurePrimitive(v,type,{slot+1,session.mapScene().assetGuid(slot),session.mapScene().materialForAsset(slot)}))return runtime::ObjectId{0};
  if(!physics){v.components.remove(scene::Collider::descriptor);v.components.remove(scene::PhysicsBody::descriptor);}v.transform.position[0]=x;v.transform.position[1]=1;v.transform.position[2]=z;
  if(!d.applyEntityValues(id,v))return runtime::ObjectId{0};
  return id;};
 const auto source=mesh("Fonte",scene::PrimitiveType::Capsule,0,0,false),position=mesh("Mola de posição",scene::PrimitiveType::Sphere,-4,0,false),rotation=mesh("Mola de rotação",scene::PrimitiveType::Cube,0,2,false),scale=mesh("Mola de escala",scene::PrimitiveType::Cylinder,0,-2,false),physical=mesh("Corpo ABI32",scene::PrimitiveType::Sphere,4,0,true);
 if(!source||!position||!rotation||!scale||!physical)return 2;
 spring<scene::SpringPositionConstraint>(d,position,source);spring<scene::SpringRotationConstraint>(d,rotation,source);spring<scene::SpringScaleConstraint>(d,scale,source);
 auto v=*d.find(position);auto *p=static_cast<scene::SpringPositionConstraint*>(v.components.edit(scene::SpringPositionConstraint::descriptor));p->offset[0]=2;p->dampingRatio=.7f;p->maxSpeed=8;d.applyEntityValues(position,v);
 v=*d.find(physical);auto *b=static_cast<scene::PhysicsBody*>(v.components.edit(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;b->gravityFactor=0;b->continuousCollision=true;b->maxLinearVelocity=3;b->maxAngularVelocity=4;b->freezePosition[2]=true;b->freezeRotation[0]=true;b->solverVelocitySteps=12;d.applyEntityValues(physical,v);
 v=*d.find(source);auto *probe=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));probe->scriptType="acceptance.bulk50";probe->source="Scripts/Bulk50Probe.cs";d.applyEntityValues(source,v);
 auto camera=d.createEntity(d.root(),runtime::ObjectKind::Camera,"Camera");v=*d.find(camera);v.components.add(scene::Camera::descriptor);v.transform.position[1]=4;v.transform.position[2]=-10;v.transform.rotationDegrees[0]=15;d.applyEntityValues(camera,v);
 auto light=d.createEntity(d.root(),runtime::ObjectKind::Folder,"Sol");v=*d.find(light);v.components.add(scene::Light::descriptor);v.transform.rotationDegrees[0]=-45;d.applyEntityValues(light,v);
 const auto data=editor::serializeEditorDocument(d,0);editor::EditorDocument reopened;if(!editor::deserializeEditorDocument(data,0,reopened)||!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",data))return 2;
 const std::string descriptor="{\"format\":\"ASTRA-PROJECT-1\",\"resourceSource\":\"independent\",\"project\":{\"name\":\"Bulk50-20261001\",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\"mainScene\":\"scenes/editor.aescene\",\"editorScene\":\"scenes/editor.aescene\"}";
 if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor))return 2;
 std::printf("Bulk50 editable project: %s\n",root.generic_string().c_str());return 0;
}
