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
struct Compound {runtime::ObjectId body,near,far;};
Compound compound(editor::EditorDocument &doc) {
 const auto body=doc.createEntity(doc.root(),runtime::ObjectKind::Folder,"SharedBody");
 auto value=*doc.find(body);auto*b=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Static;doc.applyEntityValues(body,value);
 auto child=[&](const char*name,float x){
  const auto id=doc.createEntity(body,runtime::ObjectKind::Folder,name);auto v=*doc.find(id);v.transform.position[0]=x;
  auto*c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->owner=body;c->halfX=c->halfY=c->halfZ=.5f;doc.applyEntityValues(id,v);return id;
 };
 return {body,child("Near",3),child("Far",7)};
}
}
AE_TEST(query_contract_compound_shape_identity_archive_and_rebuild){
 editor::EditorDocument doc;const auto ids=compound(doc);const auto archive=editor::serializeEditorDocument(doc,0);
 editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened),"real persisted compound is query input");
 runtime::GameWorld world;runtime::ScenePhysics physics;AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world),physics.error().c_str());
 const float from[]{0,0,0},along[]{10,0,0},between[]{5,0,0};runtime::QueryHit hit,all[2];
 AE_EXPECT_TRUE(physics.rayCast(from,along,{},hit)&&hit.object==ids.body&&hit.colliderObject==ids.near&&hit.colliderInstance==1,"closest ray distinguishes Body from authored Collider");
 AE_EXPECT_EQ(physics.rayCastAll(from,along,{},all,2),2u,"all ray reports both subshapes of the same Body");
 AE_EXPECT_TRUE(all[0].object==ids.body&&all[1].object==ids.body&&all[0].colliderObject==ids.near&&all[1].colliderObject==ids.far&&all[0].colliderInstance==1&&all[1].colliderInstance==1,"repeated local IDs are unambiguous");
 runtime::QueryShapeDesc sphere;sphere.radius=.25f;
 AE_EXPECT_TRUE(physics.shapeCast(sphere,between,along,{},hit)&&hit.colliderObject==ids.far&&hit.object==ids.body,"shape cast resolves the distant child rather than first Collider on Body");
 sphere.rotation[3]=2;AE_EXPECT_TRUE(physics.shapeCast(sphere,between,along,{},hit)&&hit.colliderObject==ids.far,"finite nonzero rotations are normalized before Jolt");
 runtime::QueryShapeDesc volume;volume.kind=runtime::QueryShapeKind::Box;volume.halfExtent[0]=3;volume.halfExtent[1]=volume.halfExtent[2]=1;
 AE_EXPECT_EQ(physics.overlap(volume,between,{},all,2),2u,"overlap reports each real part");
 AE_EXPECT_TRUE(all[0].colliderObject!=all[1].colliderObject&&all[0].colliderInstance==1&&all[1].colliderInstance==1,"overlap preserves the authoring pair");
 runtime::QueryFilter ignore;ignore.ignore=ids.body;AE_EXPECT_EQ(physics.rayCastAll(from,along,ignore,all,2),0u,"Ignore retains whole Body semantics");
 auto value=*reopened.find(ids.near);static_cast<scene::Collider*>(value.components.editInstance(1))->enabled=false;reopened.applyEntityValues(ids.near,value);
 AE_EXPECT_TRUE(world.load(reopened)&&physics.rebuild(world,nullptr),physics.error().c_str());
 AE_EXPECT_TRUE(physics.rayCast(from,along,{},hit)&&hit.colliderObject==ids.far&&hit.colliderInstance==1,"rebuild remaps compacted part zero to the remaining child");
 AE_EXPECT_TRUE(editor::deserializeEditorDocument(archive,0,reopened)&&world.load(reopened)&&physics.rebuild(world,nullptr),physics.error().c_str());
 AE_EXPECT_TRUE(physics.rayCast(from,along,{},hit)&&hit.colliderObject==ids.near,"restoration rebuilds original authored identity");
 physics.stop();AE_EXPECT_TRUE(!physics.rayCast(from,along,{},hit),"stopped world cannot query retired bindings");
}
AE_TEST(query_contract_compound_script_packet_and_abi_negotiation){
 editor::EditorDocument doc;const auto ids=compound(doc);auto value=*doc.find(ids.body);
 auto*s=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));s->scriptType="test.Query";s->source="Query.cs";doc.applyEntityValues(ids.body,value);
 editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(queryApi(),"/test");
 AE_EXPECT_TRUE(play.start(doc,resources),"real Play connects compound queries to script ABI");auto&a=queryAccess;
 AE_EXPECT_TRUE(a.version==scene::ScriptSceneAccess{}.version&&a.available(),"core ABI negotiates the 64-byte query stride");auto old=a;old.version=43;AE_EXPECT_TRUE(!old.available(),"older packet contract is rejected");
 const float from[]{0,0,0},along[]{10,0,0},between[]{5,0,0};scene::ScriptQueryFilter filter;
 struct Guarded {scene::ScriptQueryHit hits[2];u64 sentinel=0x123456789abcdef0ull;} output;
 AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,output.hits,1),2,"truncated script result reports total");
 AE_EXPECT_TRUE(output.hits[0].object==ids.body&&output.hits[0].colliderObject==ids.near&&output.hits[0].collider==1&&output.hits[1].object==0,"truncated packet has child identity without touching next slot");
 AE_EXPECT_EQ(a.rayCast(a.context,from,along,&filter,output.hits,2),2,"two packets use agreed stride");
 AE_EXPECT_TRUE(output.hits[1].object==ids.body&&output.hits[1].colliderObject==ids.far&&output.hits[1].collider==1&&output.sentinel==0x123456789abcdef0ull,"second packet and guard survive ABI copy");
 scene::ScriptShapeQuery shape;shape.radius=.25f;AE_EXPECT_EQ(a.shapeCast(a.context,&shape,between,along,&filter,output.hits),1,"script shape cast executes in Jolt");
 AE_EXPECT_TRUE(output.hits[0].colliderObject==ids.far,"script sweep retains exact child");
 shape.kind=0;shape.halfExtent[0]=3;shape.halfExtent[1]=shape.halfExtent[2]=1;
 AE_EXPECT_EQ(a.overlap(a.context,&shape,between,&filter,output.hits,2),2,"script overlap consumes real compound");
 AE_EXPECT_TRUE(output.hits[0].colliderObject!=output.hits[1].colliderObject&&output.sentinel==0x123456789abcdef0ull,"overlap copies both local identities safely");
 play.stop();
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
