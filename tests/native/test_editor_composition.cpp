#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_history.h"
#include "runtime/scene_physics.h"
#include "editor/editor_physics_body.h"
#include "scene/joint.h"
#include <cmath>

using namespace ae;
using namespace ae::editor;
namespace {
// O Play roda no mundo de execução, não no documento: o teste carrega a cena
// autorada no `GameWorld` e lê as poses publicadas de lá — é exatamente o que
// prova que o documento do editor não é escrito durante a simulação.
struct PlayFixture {
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  bool start(const EditorDocument &doc) {return world.load(doc) && physics.start(world);}
  bool advance(double dt,bool (*before)(void *,float)=nullptr,void *context=nullptr,
               bool (*trigger)(void *,runtime::ObjectId,runtime::ObjectId,u32)=nullptr) {
    return physics.advance(dt,world,before,context,trigger);
  }
  const EditorEntity *find(EditorEntityId id) const {return world.graph().find(id);}
};
EditorEntityId body(EditorDocument &doc,const char *name,float x,scene::BodyMotion motion) {
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);
  auto v=*doc.find(id);v.transform.position[0]=x;
  auto *b=editPhysicsBody(v);b->motion=motion;b->gravityFactor=0;b->linearDamping=0;b->angularDamping=0;
  editCollider(v);return doc.applyEntityValues(id,v)?id:0;
}
}
AE_TEST(composition_archive_duplicate_and_reparent_preserve_instance_ownership) {
  EditorDocument doc;EditorHistory history;
  const auto owner=body(doc,"Owner",0,scene::BodyMotion::Dynamic);
  auto v=*doc.find(owner);auto *extra=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
  AE_EXPECT_TRUE(extra,"second collider");extra->centerX=2;extra->rotationZ=25;const auto instance=extra->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(owner,v),"repeated shape authored");
  const auto child=doc.createEntity(owner,EditorEntityKind::Folder,"Shape child");
  v=*doc.find(child);editCollider(v)->owner=owner;AE_EXPECT_TRUE(doc.applyEntityValues(child,v),"explicit ancestor owner");
  const auto saved=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,0,loaded),"v10 roundtrip");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),saved,"all values and identities retained");
  const auto copy=history.duplicateEntity(loaded,owner);AE_EXPECT_TRUE(copy,"duplicate subtree");
  const auto copiedChild=loaded.childrenOf(copy)[0];
  AE_EXPECT_EQ(colliderComponent(*loaded.find(copiedChild))->owner,copy,"owner remapped within duplicate");
  AE_EXPECT_EQ(colliderComponent(*loaded.find(copy),instance)->rotationZ,25.f,"instance identity and local pose retained");
  AE_EXPECT_TRUE(!history.reparentKeepingWorld(loaded,copiedChild,loaded.root()),"moving shape away from owner rejected atomically");
  AE_EXPECT_EQ(loaded.find(copiedChild)->parent,copy,"parent unchanged on refusal");
  AE_EXPECT_TRUE(history.undo(loaded),"undo whole duplication");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),saved,"original restored");
}
AE_TEST(composition_asymmetric_compound_preserves_origin_and_mass_under_impulse) {
  EditorDocument doc;const auto id=body(doc,"Asymmetric",3,scene::BodyMotion::Dynamic);
  auto v=*doc.find(id);editPhysicsBody(v)->mass=2;editCollider(v)->centerX=2;
  auto *second=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
  second->shape=scene::ColliderShape::Sphere;second->centerX=-1;second->radius=.25f;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,v),"asymmetric compound");
  PlayFixture fx;AE_EXPECT_TRUE(fx.start(doc),fx.physics.error().c_str());
  AE_EXPECT_TRUE(fx.advance(1./60),"stationary step");
  AE_EXPECT_TRUE(std::abs(fx.find(id)->transform.position[0]-3)<.0001f,"COM does not replace authored origin");
  float impulse[3]{4,0,0},velocity[3]{};
  AE_EXPECT_TRUE(fx.physics.applyBodyForce(id,impulse,1),"impulse on compound");
  AE_EXPECT_TRUE(fx.physics.getBodyVelocity(id,velocity),"velocity read");
  AE_EXPECT_TRUE(std::abs(velocity[0]-2)<.001f,"impulse uses total authored mass");
  for(int i=0;i<60;++i) AE_EXPECT_TRUE(fx.advance(1./60),"compound moves");
  AE_EXPECT_TRUE(std::abs(fx.find(id)->transform.position[0]-5)<.01f,"world displacement follows velocity");
  float torque[3]{0,0,1};AE_EXPECT_TRUE(fx.physics.applyBodyForce(id,torque,3),"angular impulse");
  for(int i=0;i<30;++i) AE_EXPECT_TRUE(fx.advance(1./60),"rotation published");
  AE_EXPECT_TRUE(std::abs(fx.find(id)->transform.rotationDegrees[2])>1,"angular command reaches Jolt and document");
}
AE_TEST(composition_child_shapes_build_one_body_and_invalid_shear_is_rejected) {
  EditorDocument doc;const auto owner=body(doc,"Owner",0,scene::BodyMotion::Dynamic);
  auto v=*doc.find(owner);v.components.remove(scene::Collider::descriptor);AE_EXPECT_TRUE(doc.applyEntityValues(owner,v),"owner uses child shape");
  const auto child=doc.createEntity(owner,EditorEntityKind::Folder,"Child");
  v=*doc.find(child);v.transform.position[0]=2;editCollider(v)->owner=owner;editCollider(v)->rotationZ=30;
  AE_EXPECT_TRUE(doc.applyEntityValues(child,v),"rotated child collider");
  PlayFixture fx;AE_EXPECT_TRUE(fx.start(doc),fx.physics.error().c_str());
  AE_EXPECT_EQ(fx.physics.bodyCount(),1u,"one body for hierarchy of shapes");
  AE_EXPECT_TRUE(fx.advance(1./60),"step");fx.physics.stop();
  v=*doc.find(child);v.transform.scale[0]=2;AE_EXPECT_TRUE(doc.applyEntityValues(child,v),"draft shear");
  AE_EXPECT_TRUE(!fx.start(doc),"rotated shape under anisotropic scale rejected");
  AE_EXPECT_TRUE(fx.physics.error().find("shear")!=std::string::npos,"actionable composition error");
  AE_EXPECT_EQ(fx.physics.bodyCount(),0u,"failed composition releases partial world");
}
AE_TEST(composition_force_runs_before_each_substep_and_sensor_callbacks_follow_it) {
  EditorDocument doc;const auto sensor=body(doc,"Sensor",0,scene::BodyMotion::Kinematic);
  const auto target=body(doc,"Target",0,scene::BodyMotion::Dynamic);
  auto v=*doc.find(sensor);editPhysicsBody(v)->sensor=true;editCollider(v)->halfX=3;
  auto *second=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));second->halfX=3;
  AE_EXPECT_TRUE(doc.applyEntityValues(sensor,v),"overlapping sensor parts");
  PlayFixture fx;AE_EXPECT_TRUE(fx.start(doc),fx.physics.error().c_str());
  struct Calls {runtime::ScenePhysics *physics;EditorEntityId sensor,target;u32 steps=0,enter=0,stay=0,exit=0;bool ordered=true;} calls{&fx.physics,sensor,target};
  const auto before=[](void *opaque,float)->bool {auto &c=*static_cast<Calls*>(opaque);++c.steps;float force[3]{6,0,0};return c.physics->applyBodyForce(c.target,force,0);};
  const auto event=[](void *opaque,runtime::ObjectId a,runtime::ObjectId b,u32 phase)->bool {
    auto &c=*static_cast<Calls*>(opaque);c.ordered=c.ordered&&c.steps>0&&a==c.sensor&&b==c.target;
    if(phase==0) ++c.enter;else if(phase==1) ++c.stay;else ++c.exit;return true;
  };
  AE_EXPECT_TRUE(fx.advance(4./60,before,&calls,event),"multiple fixed steps in one frame");
  AE_EXPECT_EQ(calls.steps,4u,"four force callbacks");AE_EXPECT_EQ(calls.enter,1u,"body pair aggregates compound contacts");
  AE_EXPECT_TRUE(calls.stay>=1 && calls.ordered,"sensor delivered after fixed step to owner");
  float velocity[3]{};AE_EXPECT_TRUE(fx.physics.getBodyVelocity(target,velocity),"read velocity");
  AE_EXPECT_TRUE(std::abs(velocity[0]-.4f)<.005f,"force integrated once per substep");
  float pose[7]{20,0,0,0,0,0,1};AE_EXPECT_TRUE(fx.physics.moveKinematic(sensor,pose),"move sensor out");
  for(int i=0;i<6;++i) AE_EXPECT_TRUE(fx.advance(1./60,before,&calls,event),"exit processing");
  AE_EXPECT_EQ(calls.exit,1u,"one exit for body pair");
}
AE_TEST(composition_four_joint_types_and_supported_motors_simulate) {
  for(u32 kind=0;kind<4;++kind) {
    EditorDocument doc;const auto anchor=body(doc,"Anchor",0,scene::BodyMotion::Static);
    const auto moving=body(doc,"Moving",2,scene::BodyMotion::Dynamic);
    auto v=*doc.find(moving);auto *joint=static_cast<scene::Joint*>(v.components.add(scene::Joint::descriptor));
    joint->kind=static_cast<scene::JointKind>(kind);joint->connectedBody=anchor;joint->anchorA[0]=-1;joint->anchorB[0]=1;
    joint->limitMin=kind==3?0:-1;joint->limitMax=kind==1?90:1;
    if(kind==1||kind==2) {joint->motor=1;joint->motorVelocity=kind==1?45:1;}
    AE_EXPECT_TRUE(doc.applyEntityValues(moving,v),"joint authoring");
    PlayFixture fx;AE_EXPECT_TRUE(fx.start(doc),fx.physics.error().c_str());
    AE_EXPECT_EQ(fx.physics.jointCount(),1u,"native constraint created");
    for(int i=0;i<60;++i) AE_EXPECT_TRUE(fx.advance(1./60),"joint solver steps");
    const auto &pose=fx.find(moving)->transform;
    if(kind==1) AE_EXPECT_TRUE(std::abs(pose.rotationDegrees[1])>5,"hinge velocity motor rotates");
    if(kind==2) AE_EXPECT_TRUE(std::abs(pose.position[1])>.2f,"slider velocity motor translates");
    if(kind==0||kind==3) AE_EXPECT_TRUE(std::abs(pose.position[0]-2)<.01f,"stationary constraint retains separation");
  }
}
