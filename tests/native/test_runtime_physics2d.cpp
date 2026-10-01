#include "harness.h"
#include "runtime/scene_physics2d.h"
#include "scene/physics2d_components.h"
#include <sstream>
#include <algorithm>
using namespace ae;using namespace ae::runtime;
namespace {
ObjectId body2d(SceneGraph &graph,const char *name,float x,float y,bool dynamic=true,bool sensor=false){const auto id=graph.createEntity(graph.root(),ObjectKind::Folder,name);auto o=*graph.find(id);o.transform.position[0]=x;o.transform.position[1]=y;auto *shape=static_cast<scene::Collider2D*>(o.components.add(scene::Collider2D::descriptor));shape->sensor=sensor;if(dynamic){auto *body=static_cast<scene::Body2D*>(o.components.add(scene::Body2D::descriptor));body->linearDamping=0;body->angularDamping=0;}graph.applyEntityValues(id,o);return id;}
bool events2d(void *context,const Physics2DEvent &e){static_cast<std::vector<Physics2DEvent>*>(context)->push_back(e);return true;}
}
AE_TEST(physics2d_real_fall_contacts_queries_and_depth_preservation){
 SceneGraph graph;auto floor=body2d(graph,"Floor",0,-.5f,false);auto box=body2d(graph,"Box",0,3);auto o=*graph.find(floor);auto *c=o.components.find(scene::Collider2D::descriptor);auto copy=c->clone();static_cast<scene::Collider2D&>(*copy).halfX=10;o.components.replaceInstance(c->instanceId(),*copy);graph.applyEntityValues(floor,o);o=*graph.find(box);o.transform.position[2]=17;graph.applyEntityValues(box,o);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"2D scene load");ScenePhysics2D physics;AE_EXPECT_TRUE(physics.start(world),"real Box2D world");std::vector<Physics2DEvent> events;
 for(u32 i=0;i<120;++i){AE_EXPECT_TRUE(physics.advance(1.0/60.0,world,nullptr,&events,events2d),"fixed step publishes scene");}
 Transform pose;world.worldTransform(world.handle(box),pose);AE_EXPECT_TRUE(pose.position[1]>.45f&&pose.position[1]<.6f&&pose.position[2]==17,"rests on floor and keeps Z");
 AE_EXPECT_TRUE(std::any_of(events.begin(),events.end(),[](const auto &e){return !e.sensor&&e.phase==0;}),"real contact enter");AE_EXPECT_TRUE(std::any_of(events.begin(),events.end(),[](const auto &e){return !e.sensor&&e.phase==1;}),"real contact stay");
 const float origin[2]{0,5},translation[2]{0,-10};Physics2DHit hit;AE_EXPECT_TRUE(physics.rayCast(origin,translation,{},hit)&&hit.object==box&&hit.colliderInstance!=0,"ray returns owning object and collider");
 const float center[2]{0,.5f};AE_EXPECT_TRUE(physics.overlapCircle(center,.7f,{},nullptr,0)>=1,"query count available with zero buffer");physics.stop(&world);AE_EXPECT_TRUE(world.authorityOf(world.handle(box))==TransformAuthority::Free,"stop clears 2D pose authority");
}
AE_TEST(physics2d_mass_impulse_live_rebuild_capsule_and_serialization){
 SceneGraph graph;auto id=body2d(graph,"Capsule",0,0);auto o=*graph.find(id);auto *b=o.components.find(scene::Body2D::descriptor);auto copy=b->clone();static_cast<scene::Body2D&>(*copy).mass=2;o.components.replaceInstance(b->instanceId(),*copy);auto *c=o.components.find(scene::Collider2D::descriptor);copy=c->clone();static_cast<scene::Collider2D&>(*copy).shape=scene::Collider2DShape::Capsule;o.components.replaceInstance(c->instanceId(),*copy);graph.applyEntityValues(id,o);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"capsule data");ScenePhysics2D physics;physics.setGravity(0,0);AE_EXPECT_TRUE(physics.start(world),"capsule backend");const float impulse[2]{4,0};AE_EXPECT_TRUE(physics.addImpulse(id,impulse),"impulse applied");float velocity[2],angular;physics.velocity(id,velocity,angular);AE_EXPECT_TRUE(std::abs(velocity[0]-2)<.01f,"real authored mass determines impulse response");
 o=*world.graph().find(id);b=o.components.find(scene::Body2D::descriptor);copy=b->clone();static_cast<scene::Body2D&>(*copy).mass=4;o.components.replaceInstance(b->instanceId(),*copy);world.poseGraph().applyEntityValues(id,o);AE_EXPECT_TRUE(physics.rebuild(world),"live body mass rebuild");physics.velocity(id,velocity,angular);AE_EXPECT_TRUE(std::abs(velocity[0]-2)<.01f,"unrelated rebuild preserves runtime velocity");physics.addImpulse(id,impulse);physics.velocity(id,velocity,angular);AE_EXPECT_TRUE(std::abs(velocity[0]-3)<.01f,"new mass takes effect");
 std::stringstream payload;static_cast<const scene::Body2D*>(world.graph().find(id)->components.find(scene::Body2D::descriptor))->write(payload);scene::Body2D restored;AE_EXPECT_TRUE(restored.read(payload,1)&&restored.mass==4,"typed data persists");physics.releaseObject(id,&world);AE_EXPECT_TRUE(!physics.addImpulse(id,impulse)&&physics.bodyCount()==0,"removed body cannot receive impulse");
}
AE_TEST(physics2d_sensor_events_layer_masks_and_kinematic_target){
 SceneGraph graph;auto sensor=body2d(graph,"Sensor",0,0,false,true);auto mover=body2d(graph,"Mover",-3,0);auto o=*graph.find(mover);auto *c=o.components.find(scene::Collider2D::descriptor);auto copy=c->clone();static_cast<scene::Collider2D&>(*copy).shape=scene::Collider2DShape::Circle;o.components.replaceInstance(c->instanceId(),*copy);graph.applyEntityValues(mover,o);
 GameWorld world;AE_EXPECT_TRUE(world.load(graph),"sensor scene");ScenePhysics2D physics;physics.setGravity(0,0);AE_EXPECT_TRUE(physics.start(world),"sensor shape");const float velocity[2]{3,0};physics.setVelocity(mover,velocity,0);std::vector<Physics2DEvent> events;for(u32 i=0;i<140;++i)physics.advance(1.0/60.0,world,nullptr,&events,events2d);
 AE_EXPECT_TRUE(std::any_of(events.begin(),events.end(),[sensor](const auto &e){return e.sensor&&e.first==sensor&&e.phase==0;}),"sensor enter");AE_EXPECT_TRUE(std::any_of(events.begin(),events.end(),[](const auto &e){return e.sensor&&e.phase==2;}),"sensor exit without collision response");const float center[2]{0,0};Physics2DFilter filter;filter.includeSensors=true;filter.layerMask=0;AE_EXPECT_TRUE(physics.overlapCircle(center,1,filter,nullptr,0)==0,"query layer mask");
 physics.stop(&world);o=*world.graph().find(mover);auto *b=o.components.find(scene::Body2D::descriptor);copy=b->clone();static_cast<scene::Body2D&>(*copy).motion=scene::Body2DMotion::Kinematic;o.components.replaceInstance(b->instanceId(),*copy);world.poseGraph().applyEntityValues(mover,o);AE_EXPECT_TRUE(physics.start(world),"kinematic mode");Transform before;world.worldTransform(world.handle(mover),before);const float target[2]{before.position[0]+1,2};AE_EXPECT_TRUE(physics.moveKinematic(mover,target,45)&&physics.advance(1.0/60.0,world),"kinematic target moves by solver");Transform after;world.worldTransform(world.handle(mover),after);AE_EXPECT_TRUE(std::abs(after.position[0]-target[0])<.01f&&std::abs(after.position[1]-target[1])<.01f,"kinematic target published");
}
AE_TEST(physics2d_rejects_tilt_and_nonuniform_round_geometry){
 SceneGraph graph;auto id=body2d(graph,"Invalid",0,0);auto o=*graph.find(id);o.transform.rotationDegrees[0]=30;graph.applyEntityValues(id,o);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"authored tilted draft");ScenePhysics2D physics;AE_EXPECT_TRUE(!physics.start(world)&&!physics.error().empty(),"tilt explicit refusal");world.clear();o=*graph.find(id);o.transform.rotationDegrees[0]=0;o.transform.scale[0]=2;auto *c=o.components.find(scene::Collider2D::descriptor);auto copy=c->clone();static_cast<scene::Collider2D&>(*copy).shape=scene::Collider2DShape::Circle;o.components.replaceInstance(c->instanceId(),*copy);graph.applyEntityValues(id,o);AE_EXPECT_TRUE(world.load(graph)&&!physics.start(world),"ellipse unsupported rather than wrong circle");
}
AE_TEST(physics2d_joint_modes_real_constraints_motor_limits_and_remap){
 for(u32 mode=0;mode<4;++mode){SceneGraph graph;auto owner=body2d(graph,"Joint owner",mode==3?2.f:0.f,0);auto object=*graph.find(owner);auto *joint=static_cast<scene::Joint2D*>(object.components.add(scene::Joint2D::descriptor));joint->kind=static_cast<scene::Joint2DKind>(mode);joint->worldAnchor=true;joint->length=2;
  if(mode==1){joint->motorEnabled=true;joint->limitEnabled=true;joint->lowerLimit=-45;joint->upperLimit=45;}
  if(mode==2){joint->motorEnabled=true;joint->limitEnabled=true;joint->lowerLimit=-1;joint->upperLimit=1;}
  graph.applyEntityValues(owner,object);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"joint authoring typed data");ScenePhysics2D physics;physics.setGravity(0,0);AE_EXPECT_TRUE(physics.start(world)&&physics.jointCount()==1,"native joint exists");if(mode==0||mode==3){const float impulse[2]{mode==0?4.f:0.f,mode==3?2.f:0.f};physics.addImpulse(owner,impulse);}
  for(u32 frame=0;frame<120;++frame){AE_EXPECT_TRUE(physics.advance(1.0/60.0,world),"joint solver step");}
  Transform pose;world.worldTransform(world.handle(owner),pose);
  if(mode==0)AE_EXPECT_TRUE(std::abs(pose.position[0])<.04f,"weld prevents free translation");
  if(mode==1)AE_EXPECT_TRUE(std::abs(pose.rotationDegrees[2])>30&&std::abs(pose.rotationDegrees[2])<50,"revolute motor reaches actual angular limit");
  if(mode==2)AE_EXPECT_TRUE(std::abs(pose.position[0])>.8f&&std::abs(pose.position[0])<1.05f&&std::abs(pose.position[1])<.02f,"prismatic motor follows bounded axis");
  if(mode==3){AE_EXPECT_TRUE(std::abs(std::hypot(pose.position[0],pose.position[1])-2)<.05f,"distance constraint keeps true anchor separation");}
  physics.releaseObject(owner,&world);AE_EXPECT_TRUE(physics.jointCount()==0,"body lifecycle releases attached joint");
 }
 SceneGraph graph;auto owner=body2d(graph,"Owner",0,0);auto target=body2d(graph,"Target",2,0);auto object=*graph.find(owner);auto *joint=static_cast<scene::Joint2D*>(object.components.add(scene::Joint2D::descriptor));joint->target=target;joint->kind=scene::Joint2DKind::Distance;joint->length=2;graph.applyEntityValues(owner,object);ObjectCloneMap mapping{{owner,42},{target,43}};auto clone=*graph.find(owner);AE_EXPECT_TRUE(remapObjectReferences(clone,mapping),"joint reference remap path");const auto *remapped=static_cast<const scene::Joint2D*>(clone.components.find(scene::Joint2D::descriptor));AE_EXPECT_TRUE(remapped->target==43,"target follows cloned identity");std::stringstream payload;remapped->write(payload);scene::Joint2D roundtrip;AE_EXPECT_TRUE(roundtrip.read(payload,1)&&roundtrip.target==43&&roundtrip.length==2,"joint persistent frames and identity");
}
AE_TEST(physics2d_constant_force_live_local_orientation_and_zero_sleep){
 SceneGraph graph;const auto owner=body2d(graph,"Thruster",0,0);auto object=*graph.find(owner);object.transform.rotationDegrees[2]=90;object.components.add(scene::ConstantForce2D::descriptor);graph.applyEntityValues(owner,object);GameWorld world;AE_EXPECT_TRUE(world.load(graph),"force data");ScenePhysics2D physics;physics.setGravity(0,0);AE_EXPECT_TRUE(physics.start(world),"dynamic force consumer");for(u32 i=0;i<180;++i)physics.advance(1.0/60.0,world);AE_EXPECT_TRUE(physics.sleeping(owner),"enabled zero force does not prevent sleep");
 object=*world.graph().find(owner);const auto *old=object.components.find(scene::ConstantForce2D::descriptor);auto copy=old->clone();auto &changed=static_cast<scene::ConstantForce2D&>(*copy);changed.relativeForceX=2;object.components.replaceInstance(old->instanceId(),changed);world.poseGraph().applyEntityValues(owner,object);for(u32 i=0;i<60;++i)physics.advance(1.0/60.0,world);float velocity[2],angular;physics.velocity(owner,velocity,angular);AE_EXPECT_TRUE(std::abs(velocity[0])<.02f&&std::abs(velocity[1]-2)<.05f,"local X is world Y at 90 degrees with F over mass");AE_EXPECT_TRUE(!physics.sleeping(owner),"nonzero force wakes body");
 object=*world.graph().find(owner);old=object.components.find(scene::ConstantForce2D::descriptor);copy=old->clone();static_cast<scene::ConstantForce2D&>(*copy).enabled=false;object.components.replaceInstance(old->instanceId(),*copy);world.poseGraph().applyEntityValues(owner,object);physics.advance(1.0/60.0,world);float after[2];physics.velocity(owner,after,angular);AE_EXPECT_TRUE(std::abs(after[1]-velocity[1])<.001f,"live disable stops acceleration");
}

#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_play_scene.h"
AE_TEST(physics2d_editor_recipe_history_archive_play_edit_destroy_stop){
 using namespace ae::editor;
 EditorSession session;auto &document=session.document();u32 recipe=0;
 AE_EXPECT_TRUE(findCreationRecipe("physics2d.hinge",&recipe),"real hinge recipe");
 const auto owner=session.createRecipe(recipe,document.root());AE_EXPECT_TRUE(owner!=0,"recipe creates complete body shape and joint");
 AE_EXPECT_TRUE(session.history().undo(document)&&!document.exists(owner),"undo entire physics recipe");
 AE_EXPECT_TRUE(session.history().redo(document)&&document.exists(owner),"redo entire physics recipe");
 const auto saved=serializeEditorDocument(document,51);EditorDocument reopened;
 AE_EXPECT_TRUE(deserializeEditorDocument(saved,51,reopened),"archive restores typed 2D components");
 EditorMapScene resources;EditorPlayScene play;
 AE_EXPECT_TRUE(play.start(reopened,resources)&&play.physics2D().bodyCount()==1&&play.physics2D().jointCount()==1,"EditorPlay creates real backend");
 const auto joint=play.world().findComponent(play.world().handle(owner),"astra.physics2d.joint");
 AE_EXPECT_TRUE(play.world().setProperty(joint,"motor_enabled",true)==WorldStatus::Ok,"runtime reflected motor edit");
 AE_EXPECT_TRUE(play.world().setProperty(joint,"motor_angular_speed_degrees",90.f)==WorldStatus::Ok&&play.commitEdits(),"safe point rebuild consumes edit");
 const float originalAngle=reopened.find(owner)->transform.rotationDegrees[2];
 for(u32 frame=0;frame<30;++frame){AE_EXPECT_TRUE(play.advance(1.0/60.0),"integrated fixed steps");}
 Transform pose;play.world().worldTransform(play.world().handle(owner),pose);
 AE_EXPECT_TRUE(std::abs(pose.rotationDegrees[2]-originalAngle)>20,"edited motor changes simulated orientation");
 AE_EXPECT_EQ(serializeEditorDocument(reopened,51),saved,"Play edits preserve authoring archive");
 AE_EXPECT_TRUE(play.world().destroyObject(play.world().handle(owner))==WorldStatus::Ok&&play.commitEdits(),"queued destroy reaches safe point");
 AE_EXPECT_TRUE(play.physics2D().jointCount()==0&&play.physics2D().bodyCount()==0,"destroy releases body and attached joint");
 play.stop();AE_EXPECT_TRUE(!play.active()&&!play.world().running(),"Stop releases integrated world");
 AE_EXPECT_TRUE(play.start(reopened,resources)&&play.physics2D().jointCount()==1,"restart reconstructs archived authoring");play.stop();
}

AE_TEST(physics2d_connections_reject_sensor_mismatch_and_preserve_archive_and_remap) {
 using namespace ae::editor;EditorSession session;u32 index=0;findCreationRecipe("physics2d.connected_sensor",&index);
 const auto emitter=session.createRecipe(index,session.document().root());const auto receiver=session.document().createEntity(emitter,ObjectKind::Folder,"Receptor filho");
 auto value=*session.document().find(emitter);auto *connection=static_cast<scene::PhysicsEventConnection2D*>(value.components.edit(scene::PhysicsEventConnection2D::descriptor));connection->receiver=receiver;connection->event=3;
 session.document().applyEntityValues(emitter,value);EditorMapScene resources;EditorPlayScene play;
 AE_EXPECT_TRUE(!play.start(session.document(),resources)&&!play.physicsError().empty(),"solid event on sensor is refused");
 value=*session.document().find(emitter);static_cast<scene::PhysicsEventConnection2D*>(value.components.edit(scene::PhysicsEventConnection2D::descriptor))->event=0;session.document().applyEntityValues(emitter,value);
 EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(session.document(),0),0,reopened),"connection persists");
 ObjectCloneMap mapping;const auto cloned=reopened.cloneSubtree(emitter,reopened.root(),mapping);const auto *copy=static_cast<const scene::PhysicsEventConnection2D*>(reopened.find(cloned)->components.find(scene::PhysicsEventConnection2D::descriptor));
 AE_EXPECT_TRUE(copy&&copy->receiver!=receiver&&reopened.find(static_cast<ObjectId>(copy->receiver))->parent==cloned,"receiver follows cloned subtree");
 AE_EXPECT_TRUE(play.start(reopened,resources),"matching authored sensors use real solver");
 const auto handle=play.world().findComponent(play.world().handle(emitter),scene::PhysicsEventConnection2D::descriptor.id);
 AE_EXPECT_TRUE(play.world().setProperty(handle,"event",u32{3})==WorldStatus::Ok&&!play.commitEdits(),"runtime incompatible event edit is diagnosed at the safe point");play.stop();
}
