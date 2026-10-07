#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_archive.h"
#include "editor/editor_history.h"
#include "editor/editor_session.h"
#include "runtime/prefab.h"
#include "scene/dynamic_body_motor.h"
#include "scene/script_behavior.h"
#include "scene/script_extensions.h"
#include "runtime/scene_camera_follow.h"
#include <sstream>
#include <limits>
using namespace ae;
using namespace ae::runtime;
AE_TEST(u07_autostart_waits_for_first_script_publication) {
  editor::EditorSession session;auto &doc=session.document();
  const auto id=doc.createEntity(doc.root(),ObjectKind::Folder,"Script owner");
  auto entity=*doc.find(id);auto *behavior=static_cast<scene::ScriptBehavior*>(entity.components.add(scene::ScriptBehavior::descriptor));
  behavior->scriptType="project.Pending";doc.applyEntityValues(id,entity);
  AE_EXPECT_TRUE(!session.startPlay()&&!session.isPlaying()&&!session.playRequested(),"empty catalog must not start scripted scene before automatic compilation");
  entity=*doc.find(id);entity.components.remove(scene::ScriptBehavior::descriptor);doc.applyEntityValues(id,entity);
  AE_EXPECT_TRUE(session.startPlay(),"script-free scenes do not depend on a compiler");
}
namespace {
struct Rig {
  editor::EditorDocument doc;ObjectId dynamic=0,character=0;
  Rig(){auto floor=doc.createEntity(doc.root(),ObjectKind::Folder,"Floor");auto e=*doc.find(floor);e.transform.position[1]=-.5f;e.components.add(scene::PhysicsBody::descriptor);auto *shape=static_cast<scene::Collider*>(e.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=100;doc.applyEntityValues(floor,e);
    dynamic=doc.createEntity(doc.root(),ObjectKind::Folder,"Dynamic");e=*doc.find(dynamic);e.transform.position[1]=.6f;auto *b=static_cast<scene::PhysicsBody*>(e.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;for(auto &axis:b->freezeRotation)axis=true;e.components.add(scene::Collider::descriptor);e.components.add(scene::DynamicBodyMotor::descriptor);doc.applyEntityValues(dynamic,e);
    character=doc.createEntity(doc.root(),ObjectKind::Folder,"Character");e=*doc.find(character);e.transform.position[0]=5;e.components.add(scene::Character::descriptor);doc.applyEntityValues(character,e);
  }
};
ComponentHandle motor(GameWorld &world,ObjectId id){const auto *e=world.find(world.handle(id));const auto *c=e->components.find(scene::DynamicBodyMotor::descriptor);if(!c)c=e->components.find(scene::Character::descriptor);return {world.handle(id),c->instanceId()};}
std::string componentBytes(const scene::Components &components){std::ostringstream out;for(usize n=0;n<components.size();++n){const auto *c=components.at(n);if(c->type().id=="astra.prefab.link")continue;out<<c->type().id<<':'<<c->type().version<<':';c->write(out);out<<'\n';}return out.str();}
ui::GuiDocument controls(){ui::GuiDocument d;std::string error;for(auto k:{ui::GuiKind::Joystick,ui::GuiKind::ActionButton}){auto id=d.create(k);auto n=*d.find(id);n.offsets={float(id-1)*200,0,float(id)*200,200};n.control.deadzone=0;n.control.inputRadius=80;d.update(n,error);}return d;}
scene::ScriptSceneAccess access;
std::function<void()> scriptTick;
scene::ScriptRuntimeApi api(){scene::ScriptRuntimeApi a;a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *s){access=*s;return 0;};a.update=[](float){if(scriptTick)scriptTick();return 0;};a.fixedUpdate=[](float){return 0;};a.lateUpdate=a.fixedUpdate;a.stop=[]{scriptTick={};};a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;}
}
AE_TEST(u07_five_sources_real_actuators_policy_live_edit_and_momentum) {
  Rig rig;GameWorld world;ScenePhysics physics;AE_EXPECT_TRUE(world.load(rig.doc)&&physics.start(world),"real Jolt actuators");for(int n=0;n<50;++n)AE_EXPECT_TRUE(physics.advance(1./60,world),"settle");
  for(auto id:{rig.dynamic,rig.character}) {
    const auto h=motor(world,id);MotorControlSnapshot state;
    for(u32 source=1;source<=5;++source)AE_EXPECT_TRUE(physics.submitMotorControl(id,MotorControlSource(source),source==4?-.8f:.8f,0,0),"five origin commands");
    AE_EXPECT_TRUE(physics.advance(1./60,world)&&physics.motorControlState(id,state)&&state.source==MotorControlSource::Script&&state.candidates==62,"default Script priority wins every candidate");
    AE_EXPECT_TRUE(world.setProperty(h,"control_priority_ai",50.f)==WorldStatus::Ok,"live priority");
    AE_EXPECT_TRUE(physics.advance(1./60,world)&&physics.motorControlState(id,state)&&state.source==MotorControlSource::Ai&&state.right>.7f,"AI priority consumed by actuator");
    AE_EXPECT_TRUE(world.setProperty(h,"control_source",u32(MotorControlSource::Ui))==WorldStatus::Ok&&physics.releaseMotorControl(id,MotorControlSource::Ui),"exclusive policy");
    AE_EXPECT_TRUE(physics.advance(1./60,world)&&physics.motorControlState(id,state)&&state.source==MotorControlSource::Ui&&state.right==0,"exclusive idle blocks remaining sources");
    AE_EXPECT_TRUE(world.setProperty(h,"control_source",6u)==WorldStatus::Rejected,"invalid policy atomic");
    AE_EXPECT_TRUE(world.setProperty(h,"control_source",0u)==WorldStatus::Ok,"return to automatic");
    for(u32 source=1;source<=5;++source)physics.releaseMotorControl(id,MotorControlSource(source));
    const float before=world.graph().find(id)->transform.position[0];physics.submitMotorControl(id,MotorControlSource::Keyboard,1,0,0);
    for(int n=0;n<30;++n)AE_EXPECT_TRUE(physics.advance(1./60,world),"actual movement");
    AE_EXPECT_TRUE(world.graph().find(id)->transform.position[0]>before+.3f,"selected intent moves solver pose");
    if(id==rig.dynamic){float v[3],after[3];physics.getBodyVelocity(id,v);world.consumeInvalidation();AE_EXPECT_TRUE(world.setProperty(h,"control_priority_ui",33.f)==WorldStatus::Ok&&!(world.pendingInvalidation()&u32(scene::Invalidate::PhysicsBody))&&physics.getBodyVelocity(id,after)&&std::abs(v[0]-after[0])<.0001f,"policy edit preserves body momentum and physics generation");}
  }
}
AE_TEST(u07_frame_lifetime_jump_loss_focus_and_atomic_invalid_input) {
  MotorControlState control;scene::MotorControlPolicy policy;control.submit(MotorControlSource::Ui,1,0,0,true);control.submit(MotorControlSource::Script,0,0,0);
  AE_EXPECT_TRUE(control.resolve(policy,true).source==MotorControlSource::Script,"zero Script claims stop");control.cancel(MotorControlSource::Script);
  AE_EXPECT_TRUE(!control.resolve(policy,true).jump,"losing UI pulse never delayed");control.submit(MotorControlSource::Ai,.4f,0,0,true);control.beginScriptFrame();
  auto sample=control.resolve(policy,true);AE_EXPECT_TRUE(sample.source==MotorControlSource::Ui&&!sample.jump,"higher priority UI rejects AI pulse across subframe");control.cancel(MotorControlSource::Ui);
  control.beginScriptFrame();AE_EXPECT_TRUE(control.resolve(policy,true).source==MotorControlSource::None,"expired AI cannot retain ownership");
  control.submit(MotorControlSource::Script,.5f,0,0,true);AE_EXPECT_TRUE(!control.submit(MotorControlSource::Script,std::numeric_limits<float>::quiet_NaN(),0,0),"bad input refused");
  sample=control.resolve(policy,true);AE_EXPECT_TRUE(sample.right==.5f&&sample.jump,"failed submission leaves valid command intact");
  control.submit(MotorControlSource::Ui,1,0,0,true);control.resolve(policy,false);AE_EXPECT_TRUE(control.resolve(policy,true).source==MotorControlSource::None,"focus cancels axes and pulses");
  policy.scriptPriority=policy.aiPriority=policy.gamepadPriority=policy.keyboardPriority=policy.uiPriority=10;
  for(u32 source=1;source<=5;++source)control.submit(MotorControlSource(source),.5f,0,0);
  AE_EXPECT_TRUE(control.resolve(policy,true).source==MotorControlSource::Ai,"documented stable tie");
}
AE_TEST(u07_archive_legacy_versions_prefab_history_and_visibility) {
  for(u32 v=4;v<=7;++v)for(bool extended:{false,true}) {
    std::string payload="2 17 .5 0 1 2 3 0 0 0 .05 .05 1 500 47";
    if(extended)payload+=" 8";
    payload+=" 0 1 0 0 0 1 1 1 0";
    if(v>=5)payload+=" 2 3 -";
    if(v>=6)payload+=" 4";
    if(v>=7)payload+=" 0 1";
    scene::PhysicsBody old;std::istringstream input(payload);
    AE_EXPECT_TRUE(old.read(input,v)&&old.valid()&&old.mass==17&&old.velocityZ==3&&old.freezeRotation[2]&&old.solverVelocitySteps==(extended?8:0),"both historical body layouts preserve numeric and flag alignment");
    if(v>=6)AE_EXPECT_TRUE(old.surface==4,"legacy surface preserved");
    if(v>=7)AE_EXPECT_TRUE(!old.monitoring&&old.monitorable,"legacy monitoring preserved");
    std::ostringstream archive;archive<<"1 2 1 \"astra.physics.body\" "<<v<<' '<<std::quoted(payload);
    scene::Components components;std::istringstream wrapped(archive.str());
    const scene::ComponentType *registry[]{&scene::PhysicsBody::descriptor};
    AE_EXPECT_TRUE(components.read(wrapped,registry,scene::UnknownComponentPolicy::Reject,true)&&components.find(scene::PhysicsBody::descriptor)->instanceId()==1,"registered migration also accepts every legacy layout and preserves identity");
  }
  Rig rig;editor::EditorHistory history;
  for(auto id:{rig.dynamic,rig.character}){auto e=*rig.doc.find(id);const auto type=id==rig.dynamic?scene::DynamicBodyMotor::descriptor.id:scene::Character::descriptor.id;
    AE_EXPECT_TRUE(scene::setComponentProperty(e.components,type,"control_priority_gamepad",71.f)==scene::ComponentPropertyStatus::Applied&&scene::setComponentProperty(e.components,type,"control_source",3u)==scene::ComponentPropertyStatus::Applied&&history.applyValues(rig.doc,id,e),"authoring properties through history");
    AE_EXPECT_TRUE(history.undo(rig.doc)&&history.redo(rig.doc),"reversible full policy");
    Prefab prefab;std::string error;AE_EXPECT_TRUE(prefab.capture(rig.doc,id,resources::assetGuidFromSeed("u07-prefab"),error),error.c_str());SceneGraph copy;ObjectCloneMap mapping;auto fresh=prefab.instantiate(copy,copy.root(),mapping,error);AE_EXPECT_TRUE(fresh&&componentBytes(copy.find(fresh)->components)==componentBytes(rig.doc.find(id)->components),"prefab retains policy and identities");
  }
  editor::EditorDocument restored;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(rig.doc,0),0,restored),"native archive");
  for(auto id:{rig.dynamic,rig.character})AE_EXPECT_EQ(componentBytes(restored.find(id)->components),componentBytes(rig.doc.find(id)->components),"full policy round-trip");
  for(u32 v=1;v<=4;++v){scene::Character c;std::string bytes=".3 .6 1.5 4 50";if(v>=2)bytes+=" 5";if(v>=3)bytes+=" .4 .5 9.81";if(v==4)bytes+=" 1";std::istringstream input(bytes);AE_EXPECT_TRUE(c.read(input,v)&&c.control.source==MotorControlSource::None&&c.control.scriptPriority==20,"legacy Character migration keeps default arbitration");}
  for(u32 v=1;v<=2;++v){scene::DynamicBodyMotor c;std::istringstream input(v==1?"1 1 6 24 36 .25 6 1 .15 .3 50":"1 1 1 6 24 36 .25 6 1 .15 .3 50");AE_EXPECT_TRUE(c.read(input,v)&&c.automaticSupport==(v==2)&&c.control.uiPriority==10,"legacy dynamic motor preserves support semantics");}
  scene::DynamicBodyMotor c;const auto &p=scene::dynamicMotorNumbers.back();AE_EXPECT_TRUE(p.presentation.isVisible(c),"automatic priorities discoverable");c.control.source=MotorControlSource::Ui;AE_EXPECT_TRUE(!p.presentation.isVisible(c),"exclusive mode hides irrelevant priorities");
}
AE_TEST(u07_input_maps_source_isolation_disconnect_context_and_focus) {
  Rig rig;GameWorld world;AE_EXPECT_TRUE(world.load(rig.doc),"world");SceneGui gui;InputService input;auto authored=input.map();auto action=*authored.find("Mover");action.bindings.push_back({InputSource::Key,32,29,0,1,false});action.bindings.push_back({InputSource::GamepadAxis,0,0,0,1,false});AE_EXPECT_TRUE(authored.replace("Mover",action),"actual authored keyboard and gamepad bindings");input.setMap(authored);InputDeviceState raw;raw.keys={32};raw.gamepadAxes[0]=-.9f;raw.moveX=.4f;
  gui.submitInput(world,input,raw,1./60);float ui[2],keys[2],pad[2];gui.sourceInput(MotorControlSource::Ui)->axis2("Mover",ui);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Mover",keys);gui.sourceInput(MotorControlSource::Gamepad)->axis2("Mover",pad);
  AE_EXPECT_TRUE(ui[0]>0&&keys[0]>0&&pad[0]<0,"separate policies evaluate actual input map");
  raw.canceledDeviceGroups=InputGamepad;gui.submitInput(world,input,raw,1./60);gui.sourceInput(MotorControlSource::Gamepad)->axis2("Mover",pad);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Mover",keys);AE_EXPECT_TRUE(pad[0]==0&&keys[0]>0,"disconnect cancels only gamepad");
  auto map=input.map();AE_EXPECT_TRUE(map.rename("Mover","Marchar"),"authored movement role renamed");input.setMap(map);gui.submitInput(world,input,raw,1./60);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Marchar",keys);AE_EXPECT_TRUE(keys[0]>0,"role and remap preserved per origin");
  InputBinding rebound{InputSource::Key,45,29,0,1,false};AE_EXPECT_TRUE(input.overrideBinding("Marchar",2,rebound),"actual rebind on keyboard binding");raw.keys={45};gui.submitInput(world,input,raw,1./60);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Marchar",keys);AE_EXPECT_TRUE(keys[0]>0,"rebind reaches origin channel");
  auto contextual=input.map();auto configured=*contextual.find("Marchar");configured.context="Gameplay";AE_EXPECT_TRUE(contextual.replace("Marchar",configured),"authored context");input.setMap(contextual);input.setContextEnabled("Gameplay",false);gui.submitInput(world,input,raw,1./60);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Marchar",keys);AE_EXPECT_TRUE(keys[0]==0,"disabled action context propagated");input.setContextEnabled("Gameplay",true);
  input.setGameplayFocus(false);gui.submitInput(world,input,raw,1./60);gui.sourceInput(MotorControlSource::Keyboard)->axis2("Marchar",keys);AE_EXPECT_TRUE(keys[0]==0,"focus policy propagated");gui.reset();
}
AE_TEST(u07_root_canvas_cancellation_receiver_swap_and_camera_follow_binding) {
  Rig rig;auto &g=rig.doc;auto hud=g.createEntity(g.root(),ObjectKind::Folder,"Root HUD");auto e=*g.find(hud);auto *canvasSettings=static_cast<scene::UiCanvas*>(e.components.add(scene::UiCanvas::descriptor));canvasSettings->document=resources::assetGuidFromSeed("root-controls");g.applyEntityValues(hud,e);
  auto camera=g.createEntity(g.root(),ObjectKind::Camera,"Follow camera");e=*g.find(camera);e.components.add(scene::Camera::descriptor);auto *follow=static_cast<scene::CameraFollow*>(e.components.add(scene::CameraFollow::descriptor));follow->target=rig.dynamic;g.applyEntityValues(camera,e);
  AE_EXPECT_EQ(editor::EditorPlayScene::defaultMotorReceiver(g,camera,hud),rig.dynamic,"camera target routes hardware independently of editor selection");
  auto map=g.inputActions();auto moveAction=*map.find("Mover");moveAction.sensitivity=4;moveAction.bindings.push_back({InputSource::Key,32,29,0,1,false});AE_EXPECT_TRUE(map.replace("Mover",moveAction)&&g.setInputActions(map),"authored amplified input action");
  editor::EditorMapScene resources;editor::EditorPlayScene play;play.configureSceneGui([](resources::AssetGuid,ui::GuiDocument &out,std::string&){out=controls();return true;});AE_EXPECT_TRUE(play.start(g,resources),"root canvas in actual Play");for(int n=0;n<40;++n)AE_EXPECT_TRUE(play.advance(1./60),"settle");
  auto &gui=play.sceneGui();auto *canvas=gui.find(play.world(),gui.instanceFor(play.world(),hud));canvas->runtime.layout({0,0,400,200});canvas->runtime.pointer({1,ui::UiPointerPhase::Down,{100,100}});canvas->runtime.pointer({1,ui::UiPointerPhase::Move,{180,100}});play.submitInput({},1./60);AE_EXPECT_TRUE(play.routeDefaultMotorControl(rig.dynamic,0)&&play.advance(1./60),"global UI to default recipient");
  MotorControlSnapshot state;AE_EXPECT_TRUE(play.physics().motorControlState(rig.dynamic,state)&&state.right>.9f,"root contribution measured");
  AE_EXPECT_TRUE(play.routeDefaultMotorControl(rig.character,0)&&play.advance(1./60)&&!canvas->runtime.captures(1),"target swap requires fresh UI gesture");AE_EXPECT_TRUE(play.physics().motorControlState(rig.dynamic,state)&&state.right==0&&play.physics().motorControlState(rig.character,state)&&state.right==0,"no transfer or stale old recipient");
  canvas->runtime.pointer({2,ui::UiPointerPhase::Down,{300,100}});canvas->runtime.pointer({2,ui::UiPointerPhase::Up,{300,100}});play.submitInput({},0);
  AE_EXPECT_TRUE(play.world().setProperty(canvas->owner,"enabled",false)==WorldStatus::Ok&&play.advance(1./60)&&play.physics().motorControlState(rig.character,state)&&!state.jump,"disabled root cancels pending pulse without another sample");
  InputDeviceState hardware;hardware.keys={32};play.submitInput(hardware,1./60);AE_EXPECT_TRUE(play.routeDefaultMotorControl(rig.character,0)&&play.advance(1./60)&&play.physics().motorControlState(rig.character,state)&&state.source==MotorControlSource::Keyboard&&state.right==1,"amplified authored actions saturate motor input without aborting Play");play.stop();
  GameWorld world;ScenePhysics physics;AE_EXPECT_TRUE(world.load(rig.doc)&&physics.start(world),"standalone focus lifecycle");physics.setControlFocus(false);AE_EXPECT_TRUE(physics.submitMotorControl(rig.dynamic,MotorControlSource::Ai,1,0,0,true),"suspended valid commands consumed without aborting producer");physics.setControlFocus(true);AE_EXPECT_TRUE(physics.advance(1./60,world)&&physics.motorControlState(rig.dynamic,state)&&state.source==MotorControlSource::None&&!state.jump,"suspended commands never resume later");
}
AE_TEST(u07_canvas_retarget_delete_and_pause_no_retained_intention) {
  Rig rig;auto &g=rig.doc;auto hud=g.createEntity(g.root(),ObjectKind::Folder,"HUD");auto e=*g.find(hud);auto *c=static_cast<scene::UiCanvas*>(e.components.add(scene::UiCanvas::descriptor));c->document=resources::assetGuidFromSeed("u07-controls");c->inputReceiver=rig.dynamic;g.applyEntityValues(hud,e);
  editor::EditorMapScene resources;editor::EditorPlayScene play;play.configureSceneGui([](resources::AssetGuid,ui::GuiDocument &out,std::string&){out=controls();return true;});AE_EXPECT_TRUE(play.start(g,resources),"real Play");for(int i=0;i<40;++i)AE_EXPECT_TRUE(play.advance(1./60),"settle");auto &host=play.sceneGui();auto *canvas=host.find(play.world(),host.instanceFor(play.world(),hud));canvas->runtime.layout({0,0,400,200});canvas->runtime.pointer({1,ui::UiPointerPhase::Down,{100,100}});canvas->runtime.pointer({1,ui::UiPointerPhase::Move,{180,100}});
  play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"scoped input");MotorControlSnapshot state;AE_EXPECT_TRUE(play.physics().motorControlState(rig.dynamic,state)&&state.source==MotorControlSource::Ui&&state.right>.9f,"UI owns actor");
  const ComponentHandle canvasHandle{play.world().handle(hud),canvas->owner.instance};AE_EXPECT_TRUE(play.world().setProperty(canvasHandle,"input_receiver",scene::ObjectReference{rig.character})==WorldStatus::Ok&&play.advance(1./60),"retarget before next raw sample");
  AE_EXPECT_TRUE(play.physics().motorControlState(rig.dynamic,state)&&state.right==0,"old recipient released without another device sample");
  play.setCharacterMove(rig.character,1,0,0);play.pause(true);AE_EXPECT_TRUE(play.physics().motorControlState(rig.character,state)&&!state.focused&&state.source==MotorControlSource::None,"pause cancels every origin");play.pause(false);play.setInputFocus(true);
  AE_EXPECT_TRUE(play.world().removeComponent(canvasHandle)==WorldStatus::Ok&&play.advance(1./60),"canvas lifecycle safe removal");
  AE_EXPECT_TRUE(play.physics().motorControlState(rig.character,state)&&state.right==0&&!state.jump,"no delayed AI movement or jump after resume");play.stop();
}
AE_TEST(u07_sdk_extension_identity_invalid_input_and_actual_state) {
  Rig rig;auto e=*rig.doc.find(rig.dynamic);auto *script=static_cast<scene::ScriptBehavior*>(e.components.add(scene::ScriptBehavior::descriptor));script->scriptType="u07.Probe";script->source="Probe.cs";rig.doc.applyEntityValues(rig.dynamic,e);
  editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(api(),"/test");AE_EXPECT_TRUE(play.start(rig.doc,resources),"install optional real SDK family");u32 version=0,size=0;const auto name=scene::kScriptMotorControl;
  const auto *family=static_cast<const scene::ScriptMotorControlOperations*>(access.extension(access.context,reinterpret_cast<const u8*>(name.data()),int(name.size()),&version,&size));AE_EXPECT_TRUE(family&&version==1&&size==sizeof(*family),"versioned ABI without frozen core change");
  auto h=motor(play.world(),rig.dynamic);float input[3]{.7f,0,0};scene::ScriptMotorControlState state;
  AE_EXPECT_TRUE(family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,0,5,input,0,&state)==1,"AI to actual bound motor");
  scriptTick=[&]{family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,0,5,input,0,&state);};
  AE_EXPECT_TRUE(play.advance(1./60)&&family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,2,0,nullptr,0,&state)==1&&state.source==5&&(state.flags&2)&&state.right>.6f,"read measured solver selection");scriptTick={};
  input[0]=2;AE_EXPECT_TRUE(!family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,0,4,input,0,&state),"invalid range rejected");
  AE_EXPECT_TRUE(!family->command(access.context,h.object.id,h.object.world+1,h.object.generation,h.instance,2,0,nullptr,0,&state)&&!family->command(access.context,h.object.id,h.object.world,h.object.generation+1,h.instance,2,0,nullptr,0,&state)&&!family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance+900,2,0,nullptr,0,&state),"foreign/stale component handles refused");
  AE_EXPECT_TRUE(family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,1,5,nullptr,0,&state)==1&&play.advance(1./60),"release ownership");ScenePhysics::Diagnostic diagnostic;AE_EXPECT_TRUE(play.physics().diagnostic(play.world(),rig.dynamic,.5f,diagnostic)&&diagnostic.hasControl&&diagnostic.control.source==MotorControlSource::None,"debug observes same source as SDK");
  AE_EXPECT_TRUE(play.world().setActive(h.object,false)==WorldStatus::Ok&&!family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,0,5,input,0,&state)&&play.advance(1./60),"inactive hierarchy cannot consume intentions");
  AE_EXPECT_TRUE(play.world().setActive(h.object,true)==WorldStatus::Ok&&play.advance(1./60)&&play.world().removeComponent(h)==WorldStatus::Ok&&play.advance(1./60),"remove motor at safe point");WorldStatus status;const auto replacement=play.world().addComponent(h.object,scene::DynamicBodyMotor::descriptor.id,status);
  AE_EXPECT_TRUE(status==WorldStatus::Ok&&replacement.instance!=h.instance&&play.advance(1./60)&&!family->command(access.context,h.object.id,h.object.world,h.object.generation,h.instance,2,0,nullptr,0,&state),"replacement motor never accepts retired instance");
  AE_EXPECT_TRUE(family->command(access.context,h.object.id,h.object.world,h.object.generation,replacement.instance,2,0,nullptr,0,&state)&&state.source==0,"fresh component has no inherited intention");play.stop();
}

AE_TEST(u07_third_person_orbit_relative_motion_and_solver_facing) {
  Rig rig;auto e=*rig.doc.find(rig.dynamic);
  static_cast<scene::PhysicsBody*>(e.components.edit(scene::PhysicsBody::descriptor))->freezeRotation[1]=false;
  rig.doc.applyEntityValues(rig.dynamic,e);
  auto camera=rig.doc.createEntity(rig.doc.root(),ObjectKind::Camera,"Orbit");e=*rig.doc.find(camera);
  e.components.add(scene::Camera::descriptor);e.components.add(scene::CameraLook::descriptor);
  auto *follow=static_cast<scene::CameraFollow*>(e.components.add(scene::CameraFollow::descriptor));
  follow->target=rig.dynamic;follow->orbit=true;follow->pivotHeight=1.1f;follow->offset[1]=0;follow->offset[2]=-5;follow->dampingSeconds=0;
  e.transform.rotationDegrees[1]=90;rig.doc.applyEntityValues(camera,e);
  editor::EditorDocument reopened;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(rig.doc,0),0,reopened),"orbit native archive round trip");
  const auto *saved=static_cast<const scene::CameraFollow*>(reopened.find(camera)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_TRUE(saved->orbit&&std::abs(saved->pivotHeight-1.1f)<.0001f,"orbit authoring survives reopen");
  scene::CameraFollow old;std::istringstream legacy("2 1 0 2 -5 .2");AE_EXPECT_TRUE(old.read(legacy,1)&&!old.orbit&&old.pivotHeight==0,"v1 follows world offset unchanged");
  GameWorld world;ScenePhysics physics;SceneCameraFollow orbit;
  AE_EXPECT_TRUE(world.load(reopened)&&physics.start(world)&&orbit.advance(world,1./60),"actual camera and Jolt");
  Transform actor{},pose{};world.worldTransform(world.handle(rig.dynamic),actor);world.worldTransform(world.handle(camera),pose);
  AE_EXPECT_TRUE(std::abs(pose.position[0]-actor.position[0]+5)<.001f&&std::abs(pose.position[2]-actor.position[2])<.001f&&std::abs(pose.position[1]-actor.position[1]-1.1f)<.001f,"yaw 90 orbits around elevated pivot");
  pose.rotationDegrees[0]=30;world.setWorldTransform(world.handle(camera),pose);orbit.advance(world,1./60);world.worldTransform(world.handle(camera),pose);
  AE_EXPECT_TRUE(std::abs(pose.position[0]-actor.position[0]+4.330127f)<.001f&&std::abs(pose.position[1]-actor.position[1]-3.6f)<.001f,"pitch changes orbit height and keeps look axis toward pivot");
  auto local=world.find(world.handle(camera))->transform;local.rotationDegrees[0]=15;local.rotationDegrees[1]=0;local.rotationDegrees[2]=0;
  AE_EXPECT_TRUE(world.setLocalTransform(world.handle(camera),local)==WorldStatus::Ok,"reset camera look angles");
  for(int n=0;n<120;++n) {
    AE_EXPECT_TRUE(world.applyCameraLook(world.handle(camera),.01f,0)==WorldStatus::Ok&&orbit.advance(world,1./60),"full 360 degree orbit crosses Euler poles");
    const auto &actual=world.find(world.handle(camera))->transform;
    AE_EXPECT_TRUE(std::abs(actual.rotationDegrees[0]-15)<.001f&&std::abs(actual.rotationDegrees[2])<.001f,"position follow never changes pitch/roll through yaw 90/180/270");
    world.worldTransform(world.handle(camera),pose);
    const float dx=pose.position[0]-actor.position[0],dy=pose.position[1]-actor.position[1]-1.1f,dz=pose.position[2]-actor.position[2];
    AE_EXPECT_TRUE(std::abs(std::sqrt(dx*dx+dy*dy+dz*dz)-5)<.001f,"orbit radius stays constant across every angle");
  }
  physics.submitMotorControl(rig.dynamic,MotorControlSource::Ui,0,1,1.57079632679f);
  for(int n=0;n<60;++n)AE_EXPECT_TRUE(physics.advance(1./60,world),"camera relative movement");
  world.worldTransform(world.handle(rig.dynamic),actor);
  AE_EXPECT_TRUE(actor.position[0]>1&&std::abs(actor.position[2])<.05f,"forward follows camera yaw, not world straight line");
  const auto body=world.findComponent(world.handle(rig.dynamic),"astra.physics.body");AetherBodyStateV1 state;
  float before[3],after[3];physics.getBodyVelocity(rig.dynamic,before);
  AE_EXPECT_TRUE(physics.bodyCommand(world,world.handle(rig.dynamic),body.instance,1,{0,2,0},{},state)==WorldStatus::Ok&&physics.getBodyVelocity(rig.dynamic,after)&&std::abs(before[0]-after[0])<.0001f,"solver yaw command preserves linear motion");
  for(int n=0;n<20;++n){AE_EXPECT_TRUE(physics.bodyCommand(world,world.handle(rig.dynamic),body.instance,1,{0,2,0},{},state)==WorldStatus::Ok,"facing intent renewed each FixedUpdate against contact friction");AE_EXPECT_TRUE(physics.advance(1./60,world),"solver actually turns body");}
  world.worldTransform(world.handle(rig.dynamic),actor);
  std::fprintf(stderr,"U07_FACING rotation=%f/%f/%f angular=%f/%f/%f\n",actor.rotationDegrees[0],actor.rotationDegrees[1],actor.rotationDegrees[2],state.angular.x,state.angular.y,state.angular.z);
  AE_EXPECT_TRUE(std::abs(actor.rotationDegrees[1])>15&&std::abs(actor.rotationDegrees[0])<.01f&&std::abs(actor.rotationDegrees[2])<.01f,"upright collision and real rig rotate around Y");
}
AE_TEST(u07_measured_motion_air_control_and_sdk_lifecycle) {
  Rig rig;auto e=*rig.doc.find(rig.dynamic);
  static_cast<scene::DynamicBodyMotor*>(e.components.edit(scene::DynamicBodyMotor::descriptor))->airControl=.85f;
  auto *script=static_cast<scene::ScriptBehavior*>(e.components.add(scene::ScriptBehavior::descriptor));script->scriptType="u07.Probe";script->source="Probe.cs";rig.doc.applyEntityValues(rig.dynamic,e);
  editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(api(),"/test");
  AE_EXPECT_TRUE(play.start(rig.doc,resources),"real motion extension");u32 version=0,size=0;const auto name=scene::kScriptMotorMotion;
  const auto *family=static_cast<const scene::ScriptMotorMotionOperations*>(access.extension(access.context,reinterpret_cast<const u8*>(name.data()),int(name.size()),&version,&size));
  AE_EXPECT_TRUE(family&&version==1&&size==sizeof(*family),"typed motion optional family");
  scriptTick={};for(int n=0;n<60;++n)AE_EXPECT_TRUE(play.advance(1./60),"settle actual solver");
  const auto h=motor(play.world(),rig.dynamic);scene::ScriptMotorMotionState motion;
  AE_EXPECT_TRUE(family->state(access.context,h.object.id,h.object.world,h.object.generation,h.instance,&motion)&&motion.flags==3&&motion.support&&motion.normal[1]>.9f,"measured Jolt ground contact and support object");
  auto &solver=const_cast<ScenePhysics&>(play.physics());
  for(int n=0;n<12;++n){solver.submitMotorControl(rig.dynamic,MotorControlSource::Ui,1,0,0);AE_EXPECT_TRUE(play.advance(1./60),"accelerate");}
  solver.submitMotorControl(rig.dynamic,MotorControlSource::Ui,1,0,0,true);AE_EXPECT_TRUE(play.advance(1./60),"jump");
  AE_EXPECT_TRUE(family->state(access.context,h.object.id,h.object.world,h.object.generation,h.instance,&motion)&&(motion.flags&1)&&!(motion.flags&2)&&motion.velocity[1]>1,"airborne state never reports walking support");
  for(int n=0;n<15;++n){solver.submitMotorControl(rig.dynamic,MotorControlSource::Ui,-1,0,0);AE_EXPECT_TRUE(play.advance(1./60),"reverse in air");}
  AE_EXPECT_TRUE(family->state(access.context,h.object.id,h.object.world,h.object.generation,h.instance,&motion)&&!(motion.flags&2)&&motion.velocity[0]<-.2f,"effective air steering reverses horizontal velocity before landing");
  AE_EXPECT_TRUE(!family->state(access.context,h.object.id,h.object.world,h.object.generation+1,h.instance,&motion),"stale object identity rejected");
  AE_EXPECT_TRUE(play.world().removeComponent(h)==WorldStatus::Ok&&play.advance(1./60)&&!family->state(access.context,h.object.id,h.object.world,h.object.generation,h.instance,&motion),"retired motor cannot report stale motion");play.stop();
}
AE_TEST(u07_virtual_orbit_floor_clearance_canvas_look_and_hardware_target) {
  Rig rig;auto &g=rig.doc;
  const auto pivot=g.createEntity(rig.dynamic,ObjectKind::Folder,"Pivot");auto e=*g.find(pivot);e.transform.position[1]=1.1f;g.applyEntityValues(pivot,e);
  const auto camera=g.createEntity(g.root(),ObjectKind::Camera,"Camera");e=*g.find(camera);e.components.add(scene::Camera::descriptor);auto *brain=static_cast<scene::CameraBrain*>(e.components.add(scene::CameraBrain::descriptor));brain->defaultBlend=scene::CameraBlendStyle::Cut;g.applyEntityValues(camera,e);
  const auto vcam=g.createEntity(g.root(),ObjectKind::Folder,"Orbit");e=*g.find(vcam);auto *v=static_cast<scene::VirtualCamera*>(e.components.add(scene::VirtualCamera::descriptor));v->trackingTarget=v->lookAtTarget=pivot;v->position=scene::VirtualCameraPosition::Orbit;v->rotation=scene::VirtualCameraRotation::LookAt;v->avoidObstacles=true;v->cameraRadius=.25f;v->minimumDistance=.1f;v->positionDamping=0;v->orbitPitch=-65;g.applyEntityValues(vcam,e);
  const auto hud=g.createEntity(g.root(),ObjectKind::Folder,"Canvas");e=*g.find(hud);auto *c=static_cast<scene::UiCanvas*>(e.components.add(scene::UiCanvas::descriptor));c->document=resources::assetGuidFromSeed("u07-camera");c->inputReceiver=rig.dynamic;c->inputCamera=camera;c->movementSpace=2;g.applyEntityValues(hud,e);
  editor::EditorMapScene resources;editor::EditorPlayScene play;
  play.configureSceneGui([](resources::AssetGuid,ui::GuiDocument &out,std::string &error){out={};auto id=out.create(ui::GuiKind::LookArea);auto n=*out.find(id);n.anchorMax={1,1};n.offsets={0,0,0,0};n.control.action="Olhar";return out.update(n,error);});
  AE_EXPECT_TRUE(play.start(g,resources)&&play.advance(1./60),"virtual-camera lifecycle");
  AE_EXPECT_EQ(play.motorReceiver(camera,hud),rig.dynamic,"live camera target resolves motor ancestor independent of editor selection");
  auto &gui=play.sceneGui();auto *canvas=gui.find(play.world(),gui.instanceFor(play.world(),hud));canvas->runtime.layout({0,0,400,200});
  canvas->runtime.pointer({1,ui::UiPointerPhase::Down,{200,100}});canvas->runtime.pointer({1,ui::UiPointerPhase::Move,{280,100}});play.submitInput({},1./60);
  float scoped[2]{};AE_EXPECT_TRUE(gui.cameraLookInput(camera,scoped)&&std::abs(scoped[0])>.01f,"scoped look delivered to CameraBrain");
  AE_EXPECT_TRUE(play.advance(1./60),"brain consumes look without CameraLook");
  const auto h=play.world().findComponent(play.world().handle(vcam),scene::VirtualCamera::descriptor.id);
  // Full orbit with the desired camera below the floor: sphere sweep must
  // contract to a safe pose immediately at every angle.
  for(int n=0;n<72;++n) {
    AE_EXPECT_TRUE(play.world().setProperty(h,"orbit_yaw",float(n*5-180))==WorldStatus::Ok&&play.advance(1./60),"orbit around floor");
    Transform pose;AE_EXPECT_TRUE(play.world().worldTransform(play.world().handle(camera),pose)==WorldStatus::Ok&&pose.position[1]>=.24f,"camera sphere does not pass through the physical floor");
  }
  play.stop();
}
AE_TEST(u07_imported_near_pole_bone_matrix_preserves_trs_and_rejects_shear) {
  const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  const float bone[16]{2.80141808e-6f,6.20484279e-5f,.999999821f,0,-.999999821f,3.27825501e-6f,2.71201111e-6f,0,-3.18884827e-6f,-.999999821f,6.21080326e-5f,0,-.00390888238f,-.109281547f,-.00220557395f,1};
  Transform t;AE_EXPECT_TRUE(localTransformForWorld(bone,identity,t),"valid Godot bone near -90 degree Euler pole");
  float rebuilt[16];transformMatrix(t,rebuilt);for(u32 i=0;i<16;++i)AE_EXPECT_TRUE(std::abs(rebuilt[i]-bone[i])<.0001f,"original matrix reconstruction remains strict");
  float shear[16];std::copy(identity,identity+16,shear);shear[4]=.2f;AE_EXPECT_TRUE(!localTransformForWorld(shear,identity,t),"no acceptance of unsupported shear");
}
