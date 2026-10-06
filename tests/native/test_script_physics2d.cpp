#include "harness.h"
#include "editor/editor_play_scene.h"
#include "scene/physics2d_components.h"
#include "scene/script_behavior.h"
#include <cmath>
using namespace ae;using namespace ae::editor;
namespace {scene::ScriptSceneAccess physicsAccess;
scene::ScriptRuntimeApi physicsApi(){scene::ScriptRuntimeApi a;
 a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess*s){physicsAccess=*s;return s->available()?0:1;};
 a.update=[](float){return 0;};a.fixedUpdate=a.update;a.lateUpdate=a.update;a.stop=[]{};a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;}
}
AE_TEST(physics2d_script_abi_routes_real_solver_queries_and_rejects_stale_handles){
 EditorDocument doc;auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Body2D");auto o=*doc.find(id);auto*b=static_cast<scene::Body2D*>(o.components.add(scene::Body2D::descriptor));b->linearDamping=0;b->angularDamping=0;o.components.add(scene::Collider2D::descriptor);auto*s=static_cast<scene::ScriptBehavior*>(o.components.add(scene::ScriptBehavior::descriptor));s->scriptType="test.Probe";s->source="Probe.cs";AE_EXPECT_TRUE(doc.applyEntityValues(id,o),"real typed authoring");
 EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(physicsApi(),"/test");AE_EXPECT_TRUE(play.start(doc,resources),"Play installs ABI");AE_EXPECT_TRUE(physicsAccess.version==scene::ScriptSceneAccess{}.version&&physicsAccess.size==sizeof(physicsAccess),"exact v25 ABI");const auto handle=play.world().handle(id);float input[3]{2,3,45},output[3]{};
 AE_EXPECT_TRUE(physicsAccess.body2DCommand(physicsAccess.context,id,handle.world,handle.generation,1,input,nullptr)==1,"set solver velocity");AE_EXPECT_TRUE(physicsAccess.body2DCommand(physicsAccess.context,id,handle.world,handle.generation,0,nullptr,output)==1&&std::abs(output[0]-2)<.001f&&std::abs(output[2]-45)<.01f,"read actual linear and angular velocity");
 input[0]=1;input[1]=0;input[2]=0;AE_EXPECT_TRUE(physicsAccess.body2DCommand(physicsAccess.context,id,handle.world,handle.generation,3,input,nullptr)==1,"real impulse");physicsAccess.body2DCommand(physicsAccess.context,id,handle.world,handle.generation,0,nullptr,output);AE_EXPECT_TRUE(output[0]>2,"impulse changed body");
 AE_EXPECT_TRUE(physicsAccess.body2DCommand(physicsAccess.context,id,handle.world+1,handle.generation,1,input,nullptr)==0,"foreign world refused");AE_EXPECT_TRUE(physicsAccess.body2DCommand(physicsAccess.context,id,handle.world,handle.generation+1,1,input,nullptr)==0,"stale generation refused");
 scene::ScriptQueryFilter filter;scene::ScriptQueryHit hit;const float from[2]{-2,0},along[2]{4,0},center[2]{};
 AE_EXPECT_TRUE(physicsAccess.query2D(physicsAccess.context,handle.world,0,from,along,0,&filter,&hit,1)==1&&hit.object==id&&hit.colliderObject==id&&hit.collider!=0&&hit.point[2]==0&&hit.normal[2]==0&&(hit.flags&1),"ray carries XY collider and normal");
 AE_EXPECT_TRUE(physicsAccess.query2D(physicsAccess.context,handle.world,1,center,nullptr,1,&filter,nullptr,0)==1,"overlap count without buffer");AE_EXPECT_TRUE(physicsAccess.query2D(physicsAccess.context,handle.world,1,center,nullptr,1,&filter,&hit,1)==1&&!(hit.flags&1),"overlap omits invented normal");
 AE_EXPECT_TRUE(physicsAccess.query2D(physicsAccess.context,handle.world+1,0,from,along,0,&filter,&hit,1)==-1,"queries reject other worlds");play.stop();
}
