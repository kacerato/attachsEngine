#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_archive.h"
#include "editor/editor_physics_body.h"
#include "scene/script_behavior.h"
#include <limits>
using namespace ae;
namespace {
scene::ScriptSceneAccess queryAccess;
scene::ScriptRuntimeApi queryApi(){
 scene::ScriptRuntimeApi a;
 a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess*s){queryAccess=*s;return s->available()?0:1;};
 a.update=[](float){return 0;};a.fixedUpdate=a.lateUpdate=a.update;a.stop=[]{};
 a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;
}
runtime::ObjectId box(editor::EditorDocument&doc,const char*name,float x){
 const auto id=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,name);auto value=*doc.find(id);
 value.transform.position[0]=x;editor::editPhysicsBody(value)->motion=scene::BodyMotion::Static;
 auto*c=editor::editCollider(value);c->halfX=c->halfY=c->halfZ=1;
 doc.applyEntityValues(id,value);return id;
}
}
AE_TEST(query_contract_initial_overlap_range_and_real_component_identity){
 editor::EditorDocument doc;const auto inside=box(doc,"Inside",0),far=box(doc,"Far",5);
 const auto archive=editor::serializeEditorDocument(doc,0);editor::EditorDocument reopened;
 AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened),"authoring archive is actual input to physics");
 runtime::GameWorld world;runtime::ScenePhysics physics;
 AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world),physics.error().c_str());
 const float origin[3]{0,0,0},direction[3]{10,0,0};runtime::QueryHit hit;
 AE_EXPECT_TRUE(physics.rayCast(origin,direction,{},hit)&&hit.object==inside&&hit.fraction==0&&hit.distance==0,"ray begins inside convex solid at fraction zero");
 AE_EXPECT_TRUE(hit.colliderInstance!=0&&!hit.hasNormal,"inside hit has identity but no invented surface normal");
 runtime::QueryShapeDesc shape;shape.radius=.25f;
 AE_EXPECT_TRUE(physics.shapeCast(shape,origin,direction,{},hit)&&hit.object==inside&&hit.fraction==0,"shape cast reports initial penetration even when moving outward");
 runtime::QueryHit overlaps[1]{};
 AE_EXPECT_EQ(physics.overlap(shape,origin,{},overlaps,1),1u,"stationary overlap is explicit");
 AE_EXPECT_TRUE(overlaps[0].object==inside&&overlaps[0].colliderInstance!=0&&!overlaps[0].hasNormal,"overlap maps live object and collider without fabricated normal");
 runtime::QueryFilter filter;filter.ignore=inside;const float shortRay[3]{3,0,0};
 AE_EXPECT_TRUE(!physics.rayCast(origin,shortRay,filter,hit),"maximum distance is actual translation length");
 AE_EXPECT_TRUE(physics.rayCast(origin,direction,filter,hit)&&hit.object==far&&std::abs(hit.distance-4)<.01f,"longer range reaches second body");
 auto malformed=shape;malformed.rotation[3]=0;
 AE_EXPECT_TRUE(!physics.shapeCast(malformed,origin,direction,filter,hit),"zero quaternion rejected before Jolt");
 malformed=shape;malformed.radius=std::numeric_limits<float>::quiet_NaN();
 AE_EXPECT_EQ(physics.overlap(malformed,origin,filter,overlaps,1),0u,"nonfinite dimensions rejected before shape creation");
 physics.stop();AE_EXPECT_EQ(physics.overlap(shape,origin,{},overlaps,1),0u,"stopped physics cannot leak old hit");
}
AE_TEST(query_contract_script_bridge_buffers_errors_and_stopped_world){
 editor::EditorDocument doc;const auto near=box(doc,"Near",3);box(doc,"Far",7);
 auto value=*doc.find(near);auto*s=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
 s->scriptType="test.Query";s->source="Query.cs";doc.applyEntityValues(near,value);
 editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(queryApi(),"/test");
 AE_EXPECT_TRUE(play.start(doc,resources),"real Play installs query callbacks");
 const float from[3]{0,0,0},along[3]{10,0,0};scene::ScriptQueryFilter filter;scene::ScriptQueryHit hit;
 auto&a=queryAccess;
 AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,&hit,1),2,"small buffer reports real total");
 AE_EXPECT_TRUE(hit.object==near&&hit.collider!=0,"nearest hit has authored identities");
 AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,nullptr,0),2,"count-only query permitted");
 hit.object=999;AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,nullptr,1),-1,"null output with nonzero capacity refused");
 filter.flags=8;AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,&hit,1),-1,"unknown filter bits are errors");
 AE_EXPECT_EQ(hit.object,999ull,"rejected query never publishes partial output");filter={};
 const float bad[3]{std::numeric_limits<float>::infinity(),0,0};
 AE_EXPECT_EQ(a.rayCast(a.context,from,bad,&filter,&hit,1),-1,"invalid translation differs from no-hit");
 scene::ScriptShapeQuery shape;shape.radius=-1;
 AE_EXPECT_EQ(a.shapeCast(a.context,&shape,from,along,&filter,&hit),-1,"negative shape refused without backend assertion");
 shape={};shape.rotation[3]=0;
 AE_EXPECT_EQ(a.overlap(a.context,&shape,from,&filter,&hit,1),-1,"zero quaternion rejected in packet");
 play.stop();filter={};AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,&hit,1),-1,"stopped callback refuses world before dereference");
}
