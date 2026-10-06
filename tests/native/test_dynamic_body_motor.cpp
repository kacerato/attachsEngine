#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_archive.h"
#include "renderer/primitive_geometry.h"
#include "scene/dynamic_body_motor.h"
#include <sstream>
#include <cmath>
using namespace ae;
namespace {
bool motorLibrary(editor::EditorSession &s){std::vector<u8> v;std::vector<u32> i;std::vector<renderer::MapDrawRecord>d;std::vector<renderer::MapMaterialRecord>m;return renderer::appendPrimitiveLibrary(renderer::MapVertexStride,v,i,d,m)&&s.importMap(d,m,false,v,i,0);}
u32 motorRecipe(){for(u32 i=0;i<editor::editorCreationCatalog.size();++i)if(editor::editorCreationCatalog[i].id=="physics.motor_cylinder")return i;return ~0u;}
runtime::ObjectId motorFloor(runtime::SceneGraph &g,bool moving=false){auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Floor");auto e=*g.find(id);e.transform.position[1]=-.5f;auto *b=static_cast<scene::PhysicsBody*>(e.components.add(scene::PhysicsBody::descriptor));if(moving){b->motion=scene::BodyMotion::Kinematic;b->velocityX=1;}auto *c=static_cast<scene::Collider*>(e.components.add(scene::Collider::descriptor));c->halfX=c->halfZ=30;c->halfY=.5f;g.applyEntityValues(id,e);return id;}
ui::GuiDocument motorControls(){ui::GuiDocument d;std::string error;for(auto k:{ui::GuiKind::Joystick,ui::GuiKind::ActionButton}){auto id=d.create(k);auto n=*d.find(id);n.offsets={float(id-1)*200,0,float(id)*200,200};n.control.deadzone=0;n.control.inputRadius=80;d.update(n,error);}return d;}
}
AE_TEST(dynamic_motor_recipe_real_forces_history_persistence_and_impulse) {
  editor::EditorSession s;AE_EXPECT_TRUE(motorLibrary(s),"real primitive resources");const auto recipe=motorRecipe();AE_EXPECT_TRUE(recipe!=~0u&&editor::creationAvailable(s.screen(),recipe),"functional recipe available");
  const auto depth=s.history().undoDepth();const auto actor=s.createRecipe(recipe,s.document().root());AE_EXPECT_TRUE(actor,"composed root created");auto &g=s.document();const auto visual=g.childrenOf(actor)[0];
  AE_EXPECT_TRUE(g.find(actor)->components.find(scene::DynamicBodyMotor::descriptor)&&runtime::physicsBody(*g.find(actor))->motion==scene::BodyMotion::Dynamic&&!runtime::characterComponent(*g.find(actor))&&!runtime::physicsBody(*g.find(visual)),"one rigid body authority and separate visual");
  AE_EXPECT_TRUE(s.history().undoDepth()==depth+1&&s.history().undo(g)&&!g.find(actor)&&!g.find(visual)&&s.history().redo(g)&&g.find(visual),"single reversible creation preserves IDs");motorFloor(g);
  editor::EditorDocument restored;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,restored),"native round-trip including motor settings");runtime::GameWorld world;runtime::ScenePhysics physics;AE_EXPECT_TRUE(world.load(restored)&&physics.start(world),"actual Jolt body and motor");
  for(int i=0;i<45;++i){AE_EXPECT_TRUE(physics.advance(1./60,world),"settle");}
  runtime::ScenePhysics::DynamicMotorState state;
  AE_EXPECT_TRUE(physics.dynamicMotorState(actor,state)&&state.grounded&&world.authorityOf(world.handle(actor))==runtime::TransformAuthority::PhysicsBody,"support measured by solver queries; Body owns pose");
  AE_EXPECT_TRUE(physics.setDynamicMotorMove(actor,1,0,0),"move command");for(int i=0;i<60;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"force integrated");float pose[16];runtime::worldMatrix(world.poseGraph(),actor,pose);
  AE_EXPECT_TRUE(pose[12]>4&&pose[13]>.9f,"acceleration moves physical body across floor");float child[16];runtime::worldMatrix(world.poseGraph(),visual,child);AE_EXPECT_TRUE(std::abs(pose[12]-child[12])<.001f,"visual samples same physics pose");
  physics.setDynamicMotorMove(actor,0,0,0);for(int i=0;i<30;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"braking");float velocity[3];physics.getBodyVelocity(actor,velocity);AE_EXPECT_TRUE(std::abs(velocity[0])<.1f,"force braking stops idle motor");
  const float impulse[3]{490,0,0};AE_EXPECT_TRUE(physics.applyBodyForce(actor,impulse,1)&&physics.advance(1./60,world)&&physics.getBodyVelocity(actor,velocity)&&velocity[0]>5,"external impulse is not erased by target velocity assignment");
  const auto before=world.graph().find(actor)->transform.position[0];AE_EXPECT_TRUE(physics.rebuild(world,nullptr)&&physics.advance(1./60,world)&&world.graph().find(actor)->transform.position[0]>before,"rebuild preserves body momentum");
  const auto motor=world.findComponent(world.handle(actor),scene::DynamicBodyMotor::descriptor.id);AetherBodyStateV1 snapshot;
  AE_EXPECT_TRUE(physics.bodyCommand(world,motor.object,motor.instance,103,{},{},snapshot)==runtime::WorldStatus::Ok&&snapshot.linear.x>0,"instance-bound API reads actual body snapshot");
  AE_EXPECT_TRUE(physics.bodyCommand(world,motor.object,motor.instance+999,100,{1,0,0},{},snapshot)==runtime::WorldStatus::ComponentMissing&&physics.bodyCommand(world,motor.object,motor.instance,100,{2,0,0},{},snapshot)==runtime::WorldStatus::InvalidArgument,"stale instance and invalid move rejected");
  AE_EXPECT_TRUE(physics.bodyCommand(world,motor.object,motor.instance,100,{1,0,0},{},snapshot)==runtime::WorldStatus::Ok&&physics.advance(1./60,world)&&physics.dynamicMotorState(actor,state)&&state.move[0]==1,"script command uses same force motor");
  physics.beginScriptInputFrame();AE_EXPECT_TRUE(physics.advance(1./60,world)&&physics.dynamicMotorState(actor,state)&&state.move[0]==0,"script move expires next frame; no stale vector");
}
AE_TEST(dynamic_motor_scoped_ui_jump_and_cancel_and_typed_removal) {
  editor::EditorSession s;AE_EXPECT_TRUE(motorLibrary(s),"library");auto actor=s.createRecipe(motorRecipe(),s.document().root());auto &g=s.document();motorFloor(g);const auto asset=resources::assetGuidFromSeed("dynamic-controls");auto hud=g.createEntity(g.root(),runtime::ObjectKind::Folder,"HUD");auto e=*g.find(hud);auto *c=static_cast<scene::UiCanvas*>(e.components.add(scene::UiCanvas::descriptor));c->document=asset;c->inputReceiver=actor;g.applyEntityValues(hud,e);
  const auto *motor=g.find(actor)->components.find(scene::DynamicBodyMotor::descriptor);
  AE_EXPECT_TRUE(runtime::referenceAccepts(g,hud,scene::uiCanvasReferences[0],actor)&&runtime::componentRemovalReferenceUse(g,actor,motor->instanceId()).object==hud,"shared picker predicate protects referenced motor removal");
  editor::EditorPlayScene play;play.configureSceneGui([&](resources::AssetGuid,ui::GuiDocument &out,std::string&){out=motorControls();return true;});AE_EXPECT_TRUE(play.start(g,s.mapScene()),"actual GUI and physics integration");for(int i=0;i<45;++i)AE_EXPECT_TRUE(play.advance(1./60),"settle");auto &host=play.sceneGui();const auto lease=host.instanceFor(play.world(),hud);auto *instance=host.find(play.world(),lease);AE_EXPECT_TRUE(instance,"scoped canvas");
  const float eye[3]{0,3,-7};auto view=renderer::buildPerspectiveFrustum(eye,0,0,2);host.prepare(play.world(),view,{0,0,400,200},{0,0,400,200});
  instance->runtime.pointer({1,ui::UiPointerPhase::Down,{100,100}});instance->runtime.pointer({1,ui::UiPointerPhase::Move,{180,100}});
  for(int i=0;i<12;++i){play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"UI move");}
  instance->runtime.pointer({2,ui::UiPointerPhase::Down,{300,100}});instance->runtime.pointer({2,ui::UiPointerPhase::Up,{300,100}});play.submitInput({},1./120);AE_EXPECT_TRUE(play.advance(1./120),"jump pulse before fixed tick");play.submitInput({},1./120);AE_EXPECT_TRUE(play.advance(1./120),"queued jump consumed at fixed tick");
  float velocity[3];AE_EXPECT_TRUE(play.physics().getBodyVelocity(actor,velocity)&&velocity[1]>5&&velocity[0]>0,"move and jump on independent pointers");
  // Another pulse while ascending cannot grant an air jump.
  instance->runtime.pointer({2,ui::UiPointerPhase::Down,{300,100}});instance->runtime.pointer({2,ui::UiPointerPhase::Up,{300,100}});play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"air pulse consumed");float next[3];play.physics().getBodyVelocity(actor,next);AE_EXPECT_TRUE(next[1]<velocity[1],"ground probe does not grant repeated ascending jump");
  play.setInputFocus(false);play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60)&&!instance->runtime.captures(1),"focus loss cancels source");runtime::ScenePhysics::DynamicMotorState state;AE_EXPECT_TRUE(play.physics().dynamicMotorState(actor,state)&&state.move[0]==0&&state.move[1]==0,"motor does not retain a stuck UI vector");
}
AE_TEST(dynamic_motor_platform_support_layers_and_invalid_authority) {
  editor::EditorSession s;AE_EXPECT_TRUE(motorLibrary(s),"library");auto actor=s.createRecipe(motorRecipe(),s.document().root());auto &g=s.document();const auto floor=motorFloor(g,true);
  runtime::GameWorld world;runtime::ScenePhysics physics;AE_EXPECT_TRUE(world.load(g)&&physics.start(world),"moving platform");for(int i=0;i<90;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"platform carry");runtime::ScenePhysics::DynamicMotorState state;float pose[16];runtime::worldMatrix(world.poseGraph(),actor,pose);
  AE_EXPECT_TRUE(physics.dynamicMotorState(actor,state)&&state.grounded&&state.support==floor&&state.supportVelocity[0]>.9f&&pose[12]>.8f,"motor inherits velocity at supporting body point");physics.stop();
  auto bad=g;auto object=*bad.find(actor);static_cast<scene::PhysicsBody*>(object.components.edit(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;bad.applyEntityValues(actor,object);AE_EXPECT_TRUE(world.load(bad)&&!physics.start(world)&&!physics.error().empty(),"invalid static motor fails explicitly");
  const auto plan=scene::planComponentAddition(g.find(actor)->components,scene::Character::descriptor.id);AE_EXPECT_TRUE(!plan.ready,"cannot add a second Character authority");
  object=*g.find(actor);object.layer=1;g.applyEntityValues(actor,object);auto layers=g.layers();layers.setInteraction(0,1,false);g.setLayers(layers);AE_EXPECT_TRUE(world.load(g)&&physics.start(world),"excluded support layer");for(int i=0;i<10;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"no support through collision-disabled layer");AE_EXPECT_TRUE(physics.dynamicMotorState(actor,state)&&!state.grounded,"ground queries respect collision layer matrix");
}
AE_TEST(dynamic_motor_existing_mesh_authoring_fit_convex_undo_and_migration) {
  editor::EditorSession s;AE_EXPECT_TRUE(motorLibrary(s),"real geometry");auto &g=s.document();
  const float at[]{0,4,0};const auto actor=s.instantiateAsset(0,g.root(),at);AE_EXPECT_TRUE(actor,"existing cube mesh");
  auto e=*g.find(actor);while(e.components.remove(scene::Collider::descriptor)){}while(e.components.remove(scene::PhysicsBody::descriptor)){}
  e.transform.scale[0]=2.7f;e.transform.scale[1]=.4f;e.transform.scale[2]=1.1f;e.transform.rotationDegrees[1]=35;
  e.transform.rotationDegrees[0]=20;e.transform.rotationDegrees[2]=12;
  auto *mesh=runtime::editMeshRenderer(e);mesh->material.baseColor[0]=.23f;
  scene::MeshSubmesh secondary;secondary.mesh=4;secondary.asset=s.mapScene().assetGuid(3);mesh->submeshes.push_back(secondary);
  const auto meshId=mesh->instanceId();std::ostringstream original;mesh->write(original);g.applyEntityValues(actor,e);
  g.createEntity(actor,runtime::ObjectKind::Folder,"Attachment");const auto oldDepth=s.history().undoDepth();std::string error;
  AE_EXPECT_TRUE(!s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::Preserve,error)&&!error.empty()&&!runtime::physicsBody(*g.find(actor)),"no hidden fallback or mutation when collision absent");
  AE_EXPECT_TRUE(s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::FitPrimitive,error),error.c_str());
  std::ostringstream after;runtime::meshRenderer(*g.find(actor))->write(after);
  AE_EXPECT_TRUE(after.str()==original.str()&&runtime::meshRenderer(*g.find(actor))->instanceId()==meshId&&g.childrenOf(actor).size()==1,"materials, all visual slots, identity and attachment preserved");
  AE_EXPECT_TRUE(runtime::colliderComponent(*g.find(actor))->shape==scene::ColliderShape::Box,"nonuniform object gets supported fitted primitive");
  AE_EXPECT_TRUE(s.history().undoDepth()==oldDepth+1&&s.history().undo(g)&&!runtime::physicsBody(*g.find(actor))&&s.history().redo(g),"one reversible configuration on original object");
  AE_EXPECT_TRUE(s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::ConvexMesh,error),error.c_str());
  motorFloor(g);editor::EditorDocument loaded;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,loaded),"v2 motor native archive");
  editor::EditorPlayScene play;AE_EXPECT_TRUE(play.start(loaded,s.mapScene()),"real multislot convex cooking and world-scaled solver shape");
  for(int i=0;i<100;++i)AE_EXPECT_TRUE(play.advance(1./60),"settle actual shape");
  runtime::ScenePhysics::DynamicMotorState state;AE_EXPECT_TRUE(play.physics().dynamicMotorState(actor,state)&&state.grounded,"automatic support has no cylinder height dependency");
  AE_EXPECT_TRUE(play.jumpDynamicMotor(actor)&&play.advance(1./60),"jump original mesh");float velocity[3];AE_EXPECT_TRUE(play.physics().getBodyVelocity(actor,velocity)&&velocity[1]>5,"force jump on scaled convex body");
  scene::DynamicBodyMotor legacy;std::istringstream old("1 1 6 24 36 .25 6 2 .15 .4 50");
  AE_EXPECT_TRUE(legacy.read(old,1)&&!legacy.automaticSupport&&legacy.probeHeight==2,"v1 retains manually authored support instead of reinterpreting scene");
  // An unresolved SECOND slot must abort fitting, not silently fit slot zero.
  e=*g.find(actor);runtime::editMeshRenderer(e)->submeshes[0].asset=resources::assetGuidFromSeed("missing-second-mesh");g.applyEntityValues(actor,e);
  const auto depth=s.history().undoDepth();
  AE_EXPECT_TRUE(!s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::FitPrimitive,error)&&s.history().undoDepth()==depth,"missing geometry fails before touching history");
}
AE_TEST(dynamic_motor_compound_child_shapes_offset_support_and_empty_space) {
  editor::EditorSession s;auto &g=s.document();auto actor=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Compound actor");auto e=*g.find(actor);e.transform.position[1]=1.2f;
  auto *body=static_cast<scene::PhysicsBody*>(e.components.add(scene::PhysicsBody::descriptor));body->gravityFactor=0;body->mass=17;for(auto &axis:body->freezeRotation)axis=true;g.applyEntityValues(actor,e);
  for(int sign:{-1,1}) {
    auto child=g.createEntity(actor,runtime::ObjectKind::Folder,"Foot");auto part=*g.find(child);part.transform.position[0]=sign*1.5f;part.transform.position[2]=sign*1.5f;part.transform.position[1]=-.8f;
    auto *c=static_cast<scene::Collider*>(part.components.add(scene::Collider::descriptor));c->owner=actor;c->halfX=.5f;c->halfY=.4f;c->halfZ=.5f;g.applyEntityValues(child,part);
  }
  auto floor=motorFloor(g);e=*g.find(floor);static_cast<scene::Collider*>(e.components.edit(scene::Collider::descriptor))->halfX=.25f;g.applyEntityValues(floor,e);
  std::string error;AE_EXPECT_TRUE(s.configureDynamicMotor(actor,editor::EditorSession::MotorCollisionPolicy::Preserve,error),error.c_str());
  AE_EXPECT_TRUE(!runtime::colliderComponent(*g.find(actor))&&runtime::physicsBody(*g.find(actor))->mass==17&&runtime::referenceAccepts(g,floor,scene::uiCanvasReferences[0],actor),"compound-only root and typed input receiver; body parameters preserved");
  runtime::GameWorld world;runtime::ScenePhysics physics;AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.advance(1./60,world),"actual compound collider ownership");
  runtime::ScenePhysics::DynamicMotorState state;AE_EXPECT_TRUE(physics.dynamicMotorState(actor,state)&&!state.grounded,"floor beneath empty area of bounds is not physical support");physics.stop();
  e=*g.find(floor);static_cast<scene::Collider*>(e.components.edit(scene::Collider::descriptor))->halfX=30;g.applyEntityValues(floor,e);
  AE_EXPECT_TRUE(world.load(g)&&physics.start(world)&&physics.advance(1./60,world)&&physics.dynamicMotorState(actor,state)&&state.grounded,"offset child colliders grant real support despite body origin elsewhere");
  AE_EXPECT_TRUE(physics.setDynamicMotorMove(actor,1,0,0),"same motor on compound");for(int i=0;i<45;++i)AE_EXPECT_TRUE(physics.advance(1./60,world),"compound movement");
  AE_EXPECT_TRUE(world.graph().find(actor)->transform.position[0]>2,"root moves with compound collision");
  auto invalid=g;e=*invalid.find(g.childrenOf(actor)[0]);static_cast<scene::Collider*>(e.components.edit(scene::Collider::descriptor))->enabled=false;invalid.applyEntityValues(e.id,e);
  e=*invalid.find(g.childrenOf(actor)[1]);static_cast<scene::Collider*>(e.components.edit(scene::Collider::descriptor))->enabled=false;invalid.applyEntityValues(e.id,e);
  AE_EXPECT_TRUE(world.load(invalid)&&!physics.start(world)&&!physics.error().empty(),"all collision parts disabled cannot silently run enabled motor");
}
