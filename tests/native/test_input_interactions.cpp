#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_archive.h"
#include "scene/script_behavior.h"
#include "platform/android/android_game_input.h"
#include <limits>
#include <sstream>
using namespace ae;
using namespace ae::runtime;
namespace {
InputActionMap timedMap() {
  InputActionMap map;
  for(auto name:{"Hold","Tap"}){
    InputAction action;action.id=name;action.context="game";action.duration=.5f;
    action.interaction=std::string_view(name)=="Hold"?InputInteraction::Hold:InputInteraction::Tap;
    action.bindings={{InputSource::TouchButton,0},{InputSource::Key,62},{InputSource::GamepadButton,96}};
    map.add(action);
  }
  return map;
}
void legacyMap(std::ostream&out,const InputActionMap&map){
 out<<' '<<map.actions().size()<<' '<<std::quoted(map.moveAction())<<' '<<std::quoted(map.lookAction())<<' '<<std::quoted(map.jumpAction());
 for(const auto&a:map.actions()){
  out<<' '<<std::quoted(a.id)<<' '<<static_cast<u32>(a.kind)<<' '<<a.deadzone<<' '<<a.sensitivity<<' '<<std::quoted(a.context)<<' '<<a.bindings.size();
  for(const auto&b:a.bindings)out<<' '<<static_cast<u32>(b.source)<<' '<<b.code<<' '<<b.negativeCode<<' '<<b.axis<<' '<<b.scale<<' '<<b.invert;
 }
}
scene::ScriptSceneAccess access;
scene::ScriptRuntimeApi api(){scene::ScriptRuntimeApi a;
 a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess*s){access=*s;return s->available()?0:1;};
 a.update=[](float){return 0;};a.fixedUpdate=a.lateUpdate=a.update;a.stop=[]{};a.copyDiagnostics=[](u8*,int){return 0;};
 a.trigger=[](u64,u64,u32){return 0;};a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;
}
}
AE_TEST(input_interactions_clock_boundaries_cancel_and_device_filters){
 InputService input;input.setMap(timedMap());InputDeviceState state;state.touchButtons=1;
 input.submit(state,.7);AE_EXPECT_TRUE(!input.pressed("Hold")&&input.phase("Hold")==InputPhase::Started,"initial sample cannot precharge");
 input.submit(state,.25);AE_EXPECT_TRUE(input.progress("Hold")==.5f&&!input.justPressed("Hold"),"half duration remains pending");
 input.submit(state,.25);AE_EXPECT_TRUE(input.justPressed("Hold")&&input.pressed("Hold")&&input.phase("Hold")==InputPhase::Performed,"exact threshold performs");
 input.submit(state,.1);AE_EXPECT_TRUE(!input.justPressed("Hold")&&input.phase("Tap")==InputPhase::Canceled,"held action does not repeat and tap expires");
 state.touchButtons=0;input.submit(state,.01);AE_EXPECT_TRUE(input.justReleased("Hold")&&!input.justPressed("Tap"),"late release cannot perform tap");
 state.keys={62};input.submit(state,0);input.submit(state,.125);state.keys.clear();input.submit(state,.125);
 AE_EXPECT_TRUE(input.justPressed("Tap")&&input.axis("Tap")==1&&!input.pressed("Hold"),"quick key release performs one tap and cancels hold");
 input.submit(state,0);AE_EXPECT_TRUE(!input.justPressed("Tap")&&input.justReleased("Tap"),"tap pulse ends next sample");
 state.keys={62};input.submit(state,0);input.setGameplayFocus(false);
 state.keys.clear();input.submit(state,.1);AE_EXPECT_TRUE(!input.justPressed("Tap"),"focus cancellation is not a release tap");
 input.setGameplayFocus(true);state.touchButtons=1;input.submit(state,0);input.setContextEnabled("game",false);
 input.setContextEnabled("game",true);state.touchButtons=0;input.submit(state,.1);AE_EXPECT_TRUE(!input.justPressed("Tap"),"context toggles abort even without intervening sample");
 state.gamepadButtons={96};input.submit(state,0);state.gamepadButtons.clear();state.canceledDeviceGroups=InputGamepad;input.submit(state,.1);
 AE_EXPECT_TRUE(!input.justPressed("Tap")&&input.phase("Tap")==InputPhase::Canceled,"disconnect aborts pending tap");
 state.canceledDeviceGroups=0;AE_EXPECT_TRUE(input.setDeviceGroups(InputKeyboardMouse),"typed device filter");state.touchButtons=1;input.submit(state,1);
 AE_EXPECT_TRUE(!input.pressed("Saltar")&&!input.pressed("Hold"),"touch filtered before aggregation");
 state.keys={62};input.submit(state,0);input.submit(state,.5);AE_EXPECT_TRUE(input.justPressed("Hold"),"keyboard still consumed");
 AE_EXPECT_TRUE(!input.setDeviceGroups(8)&&input.deviceGroups()==InputKeyboardMouse,"invalid mask leaves policy untouched");
 input.setActionEnabled("Hold",false);AE_EXPECT_TRUE(!input.pressed("Hold")&&input.phase("Hold")==InputPhase::Disabled,"disable immediate and real");
 input.restoreActionEnabled("Hold");input.submit(state,0);input.submit(state,std::numeric_limits<double>::quiet_NaN());
 AE_EXPECT_TRUE(!input.pressed("Hold")&&input.progress("Hold")==0,"invalid clock resets pending state");
 input.setMap(timedMap());ae::platform::android::AndroidGameInputState producer;
 producer.key(3,true,96,true);input.submit(producer.snapshot(),0);producer.finishFrame();input.submit(producer.snapshot(),.25);
 producer.key(4,true,96,true);input.submit(producer.snapshot(),.25);
 AE_EXPECT_TRUE(!input.pressed("Hold")&&input.progress("Hold")==0,"new device owner cannot inherit prior charge");
 producer.finishFrame();input.submit(producer.snapshot(),0);input.submit(producer.snapshot(),.5);
 AE_EXPECT_TRUE(input.justPressed("Hold"),"new owner charges independently after cancellation sample");
 input.setMap(timedMap());producer.clear();producer.finishFrame();producer.key(10,false,62,true);
 producer.mouse(20,0,0,100,100,1);input.submit(producer.snapshot(),0);producer.finishFrame();input.submit(producer.snapshot(),.25);
 producer.disconnect(20);input.submit(producer.snapshot(),.25);
 AE_EXPECT_TRUE(input.justPressed("Hold"),"mouse loss cannot cancel a keyboard binding in the same filter group");
 producer.finishFrame();producer.disconnect(10);input.submit(producer.snapshot(),.1);
 AE_EXPECT_TRUE(!input.pressed("Hold")&&!input.justPressed("Tap"),"keyboard ownership loss cancels its own pending interaction");
}
AE_TEST(input_interactions_archive_legacy_and_profile_authoring_guards){
 auto map=timedMap();auto action=*map.find("Hold");action.deviceGroups=InputTouch|InputGamepad;action.enabled=false;action.duration=std::nextafter(.5f,1.f);
 AE_EXPECT_TRUE(map.replace(action.id,action),"author complete settings");editor::EditorDocument doc;doc.setInputActions(map);
 const auto archive=editor::serializeEditorDocument(doc,0);editor::EditorDocument reopened;
 AE_EXPECT_TRUE(archive.starts_with("AETHER_EDITOR 17 ")&&editor::deserializeEditorDocument(archive,0,reopened)&&reopened.inputActions()==map,"scene17 exact float and settings persistence");
 auto defaults=InputActionMap{};std::ostringstream old;legacyMap(old,defaults);InputActionMap restored;
 std::istringstream reader(old.str());AE_EXPECT_TRUE(restored.read(reader)&&restored==defaults,"legacy maps inherit Press enabled all devices");
 editor::EditorDocument legacyDocument;auto legacyArchive=editor::serializeEditorDocument(legacyDocument,0);
 const auto section=legacyArchive.find("INPUT"),next=legacyArchive.find('\n',section);
 legacyArchive.replace(section,next-section,"INPUT"+old.str());legacyArchive.replace(0,17,"AETHER_EDITOR 16 ");
 editor::EditorDocument migrated;
 AE_EXPECT_TRUE(editor::deserializeEditorDocument(legacyArchive,0,migrated)&&migrated.inputActions()==defaults,"complete scene16 still opens with inherited defaults");
 auto disguised=archive;disguised.replace(0,17,"AETHER_EDITOR 16 ");
 AE_EXPECT_TRUE(!editor::deserializeEditorDocument(disguised,0,migrated)&&migrated.inputActions()==defaults,"old scene cannot disguise new input wire payload");
 auto previous=restored;std::istringstream bad("ASTRA_ACTION_MAP 3 0");AE_EXPECT_TRUE(!restored.read(bad)&&restored==previous,"unsupported map atomically refused");
 InputService service;service.setMap(defaults);std::ostringstream profile;profile<<"ASTRA_INPUT_PROFILE 1 ";legacyMap(profile,defaults);legacyMap(profile,defaults);
 AE_EXPECT_TRUE(service.importProfile(profile.str()),"old player profile accepted for unchanged authoring");
 service.setMap(map);const auto saved=service.exportProfile();service.setActionEnabled("Hold",true);service.setDeviceGroups(InputGamepad);
 AE_EXPECT_TRUE(service.importProfile(saved)&&service.actionEnabled("Hold")&&service.deviceGroups()==InputGamepad,"rebind profile does not overwrite independent runtime policy");
 action.duration=2;auto changed=map;changed.replace(action.id,action);service.setMap(changed);
 AE_EXPECT_TRUE(!service.importProfile(saved)&&service.map()==changed,"profile authored interaction mismatch refused");
 action.kind=ActionKind::Axis1D;AE_EXPECT_TRUE(!action.valid(),"timed interaction forbidden on axes");
}
AE_TEST(input_interactions_live_play_abi_state_enabled_and_group_ownership){
 editor::EditorDocument doc;doc.setInputActions(timedMap());const auto id=doc.createEntity(doc.root(),ObjectKind::Folder,"Probe");
 auto entity=*doc.find(id);auto*s=static_cast<scene::ScriptBehavior*>(entity.components.add(scene::ScriptBehavior::descriptor));s->scriptType="test.Input";s->source="Input.cs";doc.applyEntityValues(id,entity);
 editor::EditorMapScene resources;editor::EditorPlayScene play;play.setScriptRuntime(api(),"/test");
 AE_EXPECT_TRUE(play.start(doc,resources)&&access.version==36&&access.available(),"real world publishes ABI36");
 auto incomplete=access;incomplete.inputActionCommand=nullptr;AE_EXPECT_TRUE(!incomplete.available(),"new callback is mandatory");
 auto command=[&](const char*name,u32 op,scene::ScriptInputActionState&state){return access.inputActionCommand(access.context,reinterpret_cast<const u8*>(name),static_cast<int>(std::char_traits<char>::length(name)),op,&state);};
 InputDeviceState input;input.keys={62};play.submitInput(input,0);play.submitInput(input,.25);scene::ScriptInputActionState state;
 AE_EXPECT_TRUE(command("Hold",0,state)==1&&state.phase==1&&state.progress==.5f&&state.elapsed==.25f,"query reads actual pending consumer");
 play.world().setTimeScale(0);play.submitInput(input,.25);AE_EXPECT_TRUE(command("Hold",0,state)==1&&state.phase==2,"hold time independent of simulation scale");
 state.flags=0;AE_EXPECT_TRUE(command("Hold",1,state)==1&&state.flags==2&&state.phase==4&&!play.input().pressed("Hold"),"runtime override leaves authored enabled bit");
 AE_EXPECT_TRUE(command("Hold",2,state)==1&&(state.flags&1),"restore authored setting");
 state.deviceGroups=InputTouch;AE_EXPECT_TRUE(command("",4,state)==1&&state.deviceGroups==InputTouch,"actual global group policy");
 play.submitInput(input,1);AE_EXPECT_TRUE(!play.input().pressed("Hold"),"filtered keyboard cannot charge hold");
 state.deviceGroups=8;AE_EXPECT_TRUE(command("",4,state)==0&&play.input().deviceGroups()==InputTouch,"invalid policy refused");
 state.size=0;AE_EXPECT_TRUE(command("Hold",0,state)==0,"foreign payload size refused");
 AE_EXPECT_TRUE(doc.inputActions().find("Hold")->enabled&&doc.inputActions().find("Hold")->deviceGroups==InputAllDevices,"runtime policy never changes authoring");
 play.stop();state={};AE_EXPECT_TRUE(command("Hold",0,state)==0,"stopped world refuses stale command");
}
