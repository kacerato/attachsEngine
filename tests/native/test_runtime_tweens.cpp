#include "harness.h"
#include "runtime/scene_tweens.h"
#include <sstream>
using namespace ae;using namespace ae::runtime;
AE_TEST(tween_delay_relative_pingpong_serialization_cancel_and_authority) {
 SceneGraph g;auto id=g.createEntity(g.root(),ObjectKind::Folder,"Tween");auto value=*g.find(id);value.transform.position[0]=2;auto*c=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));c->duration=.5f;c->delay=.25f;c->relative=true;c->pingpong=true;c->destination[0]=4;const auto instance=c->instanceId();
 std::stringstream payload;c->write(payload);scene::TransformTween copy;AE_EXPECT_TRUE(copy.read(payload,2)&&copy.pingpong&&copy.relative,"versioned tween authoring");g.applyEntityValues(id,value);
 GameWorld w;AE_EXPECT_TRUE(w.load(g),"real world");SceneTweens runtime;runtime.advance(w,.125);Transform out;w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(out.position[0]==2&&runtime.state(id,instance)->status==SceneTweens::Status::Delayed,"delay holds start");
 runtime.advance(w,.125);runtime.advance(w,.25);w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(std::abs(out.position[0]-4)<.001f,"relative midpoint");runtime.advance(w,.25);w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(std::abs(out.position[0]-6)<.001f,"forward endpoint");runtime.advance(w,.25);runtime.advance(w,.25);w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(std::abs(out.position[0]-2)<.001f&&runtime.state(id,instance)->status==SceneTweens::Status::Completed,"finite pingpong returns to rest");
 AE_EXPECT_TRUE(runtime.restart(w,id,instance),"explicit restart");runtime.advance(w,0);AE_EXPECT_TRUE(runtime.cancel(w,id,instance),"cancel");runtime.advance(w,.25);AE_EXPECT_TRUE(runtime.state(id,instance)->status==SceneTweens::Status::Cancelled,"cancel remains cancelled");
 runtime.restart(w,id,instance);w.setAuthority(id,TransformAuthority::PhysicsBody);runtime.advance(w,.25);AE_EXPECT_TRUE(runtime.state(id,instance)->status==SceneTweens::Status::Authority,"physics retains transform authority");
 w.setAuthority(id,TransformAuthority::Free);runtime.restart(w,id,instance);runtime.advance(w,.25);runtime.advance(w,.25);w.localTransform(w.handle(id),out);const float beforeRetarget=out.position[0];
 const float destination[3]{8,0,0};AE_EXPECT_TRUE(w.setTriple({w.handle(id),instance},"position_destination",destination)==WorldStatus::Ok,"live authoring target");runtime.advance(w,0);w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(std::abs(out.position[0]-beforeRetarget)<.001f,"retarget keeps current pose");runtime.advance(w,.25);w.localTransform(w.handle(id),out);AE_EXPECT_TRUE(std::abs(out.position[0]-(beforeRetarget+4))<.001f,"relative retarget consumes new destination");
 runtime.cancel(w,id,instance);const float ignored[3]{1,0,0};w.setTriple({w.handle(id),instance},"position_destination",ignored);runtime.advance(w,.25);AE_EXPECT_TRUE(runtime.state(id,instance)->status==SceneTweens::Status::Cancelled,"authoring edit does not resurrect cancellation");
}
AE_TEST(tween_cancel_validates_world_component_removal_and_session_lifetime) {
 SceneGraph graph;const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,"Tween A");const auto other=graph.createEntity(graph.root(),ObjectKind::Folder,"Tween B");
 auto value=*graph.find(id);auto*a=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));a->autoplay=false;const auto first=a->instanceId();
 graph.applyEntityValues(id,value);value=*graph.find(other);auto*b=static_cast<scene::TransformTween*>(value.components.add(scene::TransformTween::descriptor));b->autoplay=false;const auto second=b->instanceId();graph.applyEntityValues(other,value);
 GameWorld world,foreign;AE_EXPECT_TRUE(world.load(graph)&&foreign.load(graph),"real independent worlds");SceneTweens tweens;AE_EXPECT_TRUE(tweens.advance(world,0),"initialize per-instance state");
 AE_EXPECT_TRUE(tweens.restart(world,other,second)&&tweens.cancel(world,other,second),"second object accepts commands");
 AE_EXPECT_TRUE(tweens.state(id,first)->status==SceneTweens::Status::Idle&&tweens.state(other,second)->status==SceneTweens::Status::Cancelled,"first object stays untouched");
 AE_EXPECT_TRUE(!tweens.cancel(foreign,other,second),"matching ids in another world cannot mutate cached state");
 AE_EXPECT_TRUE(world.removeComponent({world.handle(other),second})==WorldStatus::Ok,"remove live tween");world.flush();
 AE_EXPECT_TRUE(!tweens.cancel(world,other,second)&&!tweens.restart(world,other,second),"removed component rejects retained commands");
 tweens.advance(world,0);AE_EXPECT_TRUE(!tweens.state(other,second),"cache releases removed identity");
 world.clear();AE_EXPECT_TRUE(!tweens.cancel(world,id,first)&&!tweens.restart(world,id,first),"closed session rejects commands");
}
