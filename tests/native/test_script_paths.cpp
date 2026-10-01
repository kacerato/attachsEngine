#include "harness.h"
#include "editor/editor_play_scene.h"
#include "scene/path.h"
#include "scene/path_follow.h"
#include "scene/script_behavior.h"
#include <limits>

using namespace ae;
namespace {
scene::ScriptSceneAccess pathAccess;
scene::ScriptRuntimeApi pathApi() {
  scene::ScriptRuntimeApi api;
  api.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *access){pathAccess=*access;return access->available()?0:1;};
  api.update=[](float){return 0;};api.fixedUpdate=api.update;api.lateUpdate=api.update;
  api.stop=[]{};api.copyDiagnostics=[](u8*,int){return 0;};api.trigger=[](u64,u64,u32){return 0;};
  api.contact=[](u64,u64,u32,const float*){return 0;};api.timer=[](u64,u64,u32){return 0;};api.lifecycle=[](u32,u32){return 0;};
  return api;
}
}
AE_TEST(groups_script_abi_queries_mutates_and_bounds_live_world_membership) {
  editor::EditorDocument doc;const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Guard");
  auto value=*doc.find(id);value.groups.add("guards");
  auto *script=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="test.Groups";script->source="Groups.cs";doc.applyEntityValues(id,value);
  editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(pathApi(),"/test");
  AE_EXPECT_TRUE(play.start(doc,resources) && pathAccess.version==36 && pathAccess.available(),"complete groups ABI");
  const auto *name=reinterpret_cast<const u8*>("guards");auto &a=pathAccess;
  AE_EXPECT_EQ(a.groupMembership(a.context,id,name,6,-1),1,"query authored membership");
  AE_EXPECT_EQ(a.groupAt(a.context,id,std::numeric_limits<u32>::max(),nullptr,0),1,"enumeration count");
  std::array<u8,64> text{};AE_EXPECT_EQ(a.groupAt(a.context,id,0,text.data(),text.size()),6,"enumeration name");
  AE_EXPECT_EQ(std::string(reinterpret_cast<const char*>(text.data()),6),std::string("guards"),"actual identity");
  std::array<u64,1> ids{};AE_EXPECT_EQ(a.findGroup(a.context,name,6,ids.data(),1,1),1,"group query count");
  AE_EXPECT_EQ(ids[0],static_cast<u64>(id),"world-scoped query result");
  AE_EXPECT_EQ(a.groupMembership(a.context,id,name,6,0),1,"remove through ABI");
  AE_EXPECT_EQ(a.findGroup(a.context,name,6,nullptr,0,1),0,"removed member disappears");
  AE_EXPECT_TRUE(doc.find(id)->groups.contains("guards"),"authoring unaffected");
  AE_EXPECT_EQ(a.groupMembership(a.context,id,name,6,2),-1,"invalid operation rejected");
  AE_EXPECT_EQ(a.findGroup(a.context,name,6,nullptr,1,1),-1,"null buffer capacity rejected");
  AE_EXPECT_EQ(a.groupMembership(a.context,id,name,6,1),1,"re-add through ABI");
  play.world().destroyObject(play.world().handle(id));
  AE_EXPECT_EQ(a.groupMembership(a.context,id,name,6,-1),-1,"pending destruction cannot be queried");
}

AE_TEST(path_script_abi_points_identity_sampling_runtime_control_and_stale_handles) {
  editor::EditorDocument doc;
  const auto pathId=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Curve");
  const auto followerId=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"Follower");
  auto authored=*doc.find(pathId);
  const auto pathInstance=authored.components.add(scene::Path::descriptor)->instanceId();
  auto *script=static_cast<scene::ScriptBehavior *>(authored.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="test.CurveProbe";script->source="CurveProbe.cs";
  AE_EXPECT_TRUE(doc.applyEntityValues(pathId,authored),"author Path and script");
  authored=*doc.find(followerId);
  auto *follow=static_cast<scene::PathFollow *>(authored.components.add(scene::PathFollow::descriptor));
  follow->target=pathId;follow->speed=2;const auto followInstance=follow->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(followerId,authored),"author follower");
  editor::EditorMapScene resources;editor::EditorPlayScene play;
  play.setScriptRuntime(pathApi(),"/test");
  AE_EXPECT_TRUE(play.start(doc,resources),"real Play bridge installs complete ABI");
  AE_EXPECT_TRUE(pathAccess.version==36&&pathAccess.size==sizeof(pathAccess)&&pathAccess.available(),"exact complete layout");
  auto incomplete=pathAccess;incomplete.pathRuntimeCommand=nullptr;AE_EXPECT_TRUE(!incomplete.available(),"new callback required");
  const auto handle=play.world().handle(pathId),follower=play.world().handle(followerId);
  float input[9]{},output[9]{};u64 a=0,b=0,identity=0;
  auto points=[&](u32 op,u64 element,u32 index,const float *in,float *out,u64 *id){return pathAccess.pathPointCommand(pathAccess.context,pathId,handle.world,handle.generation,pathInstance,op,element,index,in,out,id);};
  AE_EXPECT_EQ(points(3,0,0,input,nullptr,&a),1,"insert through live world");
  input[2]=10;AE_EXPECT_EQ(points(3,0,1,input,nullptr,&b),1,"insert second point");
  AE_EXPECT_TRUE(a&&b&&a!=b,"identities returned, never indices");
  AE_EXPECT_EQ(points(6,a,1,nullptr,nullptr,nullptr),1,"move point to final index");
  AE_EXPECT_EQ(points(2,a,0,nullptr,output,&identity),1,"read by stable identity after move");
  AE_EXPECT_TRUE(identity==a&&output[2]==0,"identity keeps original point value");
  AE_EXPECT_EQ(points(6,a,0,nullptr,nullptr,nullptr),1,"restore ordering");
  input[0]=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_EQ(points(4,b,0,input,nullptr,nullptr),-1,"nonfinite edit refused atomically");
  AE_EXPECT_EQ(points(2,b,0,nullptr,output,&identity),1,"original point still exists");
  AE_EXPECT_EQ(output[2],10.f,"invalid update preserves value");
  double length=0;
  AE_EXPECT_EQ(pathAccess.pathRuntimeCommand(pathAccess.context,pathId,handle.world,handle.generation,pathInstance,0,3,0,output,&length),1,"sample real world curve");
  AE_EXPECT_TRUE(std::abs(output[2]-3)<.001&&std::abs(length-10)<.001,"world length and sample");
  float rolled[10]{},full[10]{};rolled[2]=10;rolled[9]=90;
  AE_EXPECT_EQ(points(10,b,0,rolled,nullptr,nullptr),1,"atomic geometry and roll through ABI35");
  input[0]=0;AE_EXPECT_EQ(points(4,b,0,input,nullptr,nullptr),1,"legacy geometry edit preserves authored roll");
  AE_EXPECT_EQ(points(8,b,0,nullptr,full,&identity),1,"full point by identity");AE_EXPECT_TRUE(identity==b&&full[9]==90,"roll survives legacy geometry command");
  rolled[9]=std::numeric_limits<float>::infinity();AE_EXPECT_EQ(points(10,b,0,rolled,nullptr,nullptr),-1,"invalid roll refuses entire write");
  AE_EXPECT_EQ(pathAccess.pathRuntimeCommand(pathAccess.context,pathId,handle.world,handle.generation,pathInstance,5,3,0,full,&length),1,"sample frame from real consumer");
  AE_EXPECT_TRUE(std::abs(full[9]-27)<.001&&full[6]<-.4&&full[7]>.8,"distance-interpolated roll rotates actual world up");
  auto runtimeCommand=[&](u32 op,double *scalar){return pathAccess.pathRuntimeCommand(pathAccess.context,followerId,follower.world,follower.generation,followInstance,op,0,0,nullptr,scalar);};
  AE_EXPECT_EQ(runtimeCommand(1,nullptr),1,"restart without prior evaluation");
  AE_EXPECT_TRUE(play.advance(.25),"Play advances path consumer");
  runtime::Transform pose;AE_EXPECT_TRUE(play.world().worldTransform(follower,pose)==runtime::WorldStatus::Ok&&std::abs(pose.position[2]-.5)<.001,"follower visibly moved by actual runtime");
  double progress=0;AE_EXPECT_EQ(runtimeCommand(3,&progress),1,"runtime distance available");
  AE_EXPECT_TRUE(std::abs(progress-.5)<.001,"distance reflects movement");
  AE_EXPECT_EQ(runtimeCommand(2,nullptr),1,"stop follower");AE_EXPECT_TRUE(play.advance(.25),"advance stopped world");
  AE_EXPECT_EQ(runtimeCommand(3,&progress),1,"stopped state readable");AE_EXPECT_TRUE(std::abs(progress-.5)<.001,"stop preserves progress");
  AE_EXPECT_EQ(pathAccess.pathPointCommand(pathAccess.context,pathId,handle.world+1,handle.generation,pathInstance,0,0,0,nullptr,nullptr,nullptr),-1,"foreign world refused");
  AE_EXPECT_TRUE(play.world().destroyObject(handle)==runtime::WorldStatus::Ok&&play.commitEdits(),"destroy path and drain");
  AE_EXPECT_EQ(points(0,0,0,nullptr,nullptr,nullptr),-1,"stale object generation refused");
  play.stop();AE_EXPECT_TRUE(doc.find(pathId)->components.find(scene::Path::descriptor)&&static_cast<const scene::Path *>(doc.find(pathId)->components.find(scene::Path::descriptor))->curve.points.empty(),"Play changes never alter authoring");
}
