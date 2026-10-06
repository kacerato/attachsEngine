#include "harness.h"
#include "editor/editor_play_scene.h"
#include "scene/script_behavior.h"
#include "ui/gui_workbench.h"
#include "ui/ui_instance_builder.h"
#include "imgui.h"
#include <sstream>
#include <limits>
#include <chrono>
#include <cstdio>
using namespace ae;
using namespace ae::ui;
namespace {
std::string encoded(const GuiDocument &d) { std::ostringstream s;d.write(s);return s.str(); }
scene::ScriptSceneAccess access;
scene::ScriptRuntimeApi scriptApi() {
  scene::ScriptRuntimeApi a;
  a.start=[](const u8*,int,const u8*,int,const scene::ScriptSceneAccess *s){access=*s;return s->available()?0:1;};
  a.update=[](float){return 0;};a.fixedUpdate=a.update;a.lateUpdate=a.update;a.stop=[]{};
  a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};
  a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};return a;
}
}
AE_TEST(gui_document_roundtrip_history_and_strict_transactional_load) {
  GuiDocument d;GuiHistory h;std::string error;
  h.begin(d);const auto parent=d.create(GuiKind::Panel),child=d.create(GuiKind::Button,parent);h.commit(d);
  auto node=*d.find(child);node.name="start";node.text="Iniciar \"jogo\"\nAgora";
  AE_EXPECT_TRUE(d.update(node,error),"editable node");
  const auto original=encoded(d);GuiDocument loaded;std::istringstream good(original);
  AE_EXPECT_TRUE(loaded.read(good,error)&&encoded(loaded)==original,"all authoring data roundtrip");
  std::istringstream corrupt("AEUI 1 1 2\n1 1 9 invalid");
  AE_EXPECT_TRUE(!loaded.read(corrupt,error)&&encoded(loaded)==original,"bad file leaves previous document intact");
  node.anchorMax.x=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!d.update(node,error)&&encoded(d)==original,"invalid property edit rolls back");
  h.begin(d);const auto copy=d.duplicate(parent);h.commit(d);
  AE_EXPECT_TRUE(copy!=0&&d.nodes().size()==4&&d.validate(error),"full subtree copied with new IDs and names");
  AE_EXPECT_TRUE(h.undo(d)&&d.nodes().size()==2&&h.redo(d)&&d.nodes().size()==4,"subtree undo/redo");
  h.begin(d);d.remove(parent);h.commit(d);
  AE_EXPECT_TRUE(d.nodes().size()==2&&!d.find(child),"remove owns descendants");
  AE_EXPECT_TRUE(h.undo(d)&&d.nodes().size()==4,"removal reversible");
}
AE_TEST(gui_runtime_anchors_clipping_capture_and_authoring_isolation) {
  GuiDocument authored;std::string error;
  const auto parent=authored.create(GuiKind::Panel),toggle=authored.create(GuiKind::Toggle,parent);
  auto panel=*authored.find(parent);panel.anchorMax={1,1};panel.offsets={10,10,-10,-10};
  AE_EXPECT_TRUE(authored.update(panel,error),"stretch layout");
  GuiRuntime r;r.load(authored);r.layout({0,0,400,300});
  AE_EXPECT_TRUE(r.placement(parent)->bounds.width==380,"responsive anchors");
  const auto p=r.placement(toggle)->bounds;const UiPoint point{p.x+10,p.y+10};
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Down,point})&&r.pointer({7,UiPointerPhase::Up,point}),"real toggle input");
  GuiEvent e;AE_EXPECT_TRUE(r.poll(e)&&e.node==toggle&&e.value==1&&authored.find(toggle)->value==0,"runtime event and isolated authoring");
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Down,point}),"capture");
  AE_EXPECT_TRUE(r.setEnabled(parent,false)&&!r.captures(7)&&!r.pointer({8,UiPointerPhase::Down,point}),"parent disabled cancels descendants immediately");
  r.setEnabled(parent,true);r.setVisible(parent,false);
  AE_EXPECT_EQ(r.hit(point,true),0u,"hidden parent owns visibility");
  r.setVisible(parent,true);r.layout({0,0,800,600});
  AE_EXPECT_TRUE(r.placement(parent)->bounds.width==780,"resolution change recomputes");
  AE_EXPECT_TRUE(!r.setValue(parent,1),"only value controls accept value commands");
  // A transparent clickable control must stay transparent while held.
  GuiDocument transparent;const auto button=transparent.create(GuiKind::Button);
  auto clear=*transparent.find(button);clear.background=0x00123456;
  AE_EXPECT_TRUE(transparent.update(clear,error),"transparent authoring");
  std::ostringstream saved;transparent.write(saved);GuiDocument restored;std::istringstream input(saved.str());
  AE_EXPECT_TRUE(restored.read(input,error)&&restored.find(button)->background==clear.background,"alpha persists");
  r.load(restored);r.layout({0,0,400,300});const UiPoint click{40,40};
  AE_EXPECT_TRUE(r.pointer({17,UiPointerPhase::Down,click}),"transparent control captures");
  UiDrawList drawing;drawing.begin({0,0,400,300},fallbackFontMetrics());r.draw(drawing);
  for(const auto &command:drawing.commands())AE_EXPECT_TRUE(command.kind!=UiPrimitive::Rect,"press does not restore background");
  AE_EXPECT_TRUE(r.pointer({17,UiPointerPhase::Up,click})&&r.poll(e)&&e.node==button,"transparent control still emits click");
}
AE_TEST(gui_imgui_real_mesh_pointer_and_ime_input) {
  ImmediateGui im;UiDrawList list;UiFont font;UiIconAtlas icons;std::vector<UiInstance> instances;
  bool clicked=false;char text[64]="original";
  const auto frame=[&] {
    list.begin({0,0,500,300},fallbackFontMetrics());im.begin(500,300,1.f/60);
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({500,300});
    ImGui::Begin("test",nullptr,ImGuiWindowFlags_NoDecoration);
    clicked=ImGui::Button("Criar",{150,40})||clicked;
    ImGui::InputText("Texto",text,sizeof(text),ImGuiInputTextFlags_CallbackAlways,ImmediateGui::inputCallback,&im);
    ImGui::End();im.end(list);
  };
  frame();frame();
  im.pointer({1,UiPointerPhase::Down,{30,30}});frame();
  im.pointer({1,UiPointerPhase::Up,{30,30}});frame();frame();
  AE_EXPECT_TRUE(clicked,"official ImGui widget owns click");
  im.pointer({2,UiPointerPhase::Down,{40,75}});frame();
  im.pointer({2,UiPointerPhase::Up,{40,75}});frame();frame();
  const auto id=im.inputId();
  AE_EXPECT_TRUE(id && im.replaceInput(id,"Editado pelo IME"),"platform input tied to focused field ID");
  frame();AE_EXPECT_TRUE(std::string(text)=="Editado pelo IME","IME edits actual ImGui text buffer");
  const auto built=buildUiInstances(list,font,icons,16384,instances);
  AE_EXPECT_TRUE(built.emitted>100 && !built.dropped && im.rejectedCommands()==0,"actual indexed font and geometry reach engine instances");
  AE_EXPECT_TRUE(im.atlas().size()==static_cast<usize>(im.atlasWidth())*im.atlasHeight()*4,"official font atlas uploaded shape");
  AE_EXPECT_TRUE(instances.front().params[2]==static_cast<float>(UiInstanceKind::Triangle),"mesh never replaced with fake native widgets");
}
AE_TEST(gui_workbench_canvas_drag_undo_cancel_and_preview) {
  GuiWorkbench w;UiDrawList list;
  const auto id=w.document().create(GuiKind::Toggle);const auto before=encoded(w.document());
  const auto frame=[&]{list.begin({0,0,1280,720},fallbackFontMetrics());w.draw({0,60,1280,660},{0,0,1280,720},list);};
  frame();frame();w.preview().load(w.document());w.preview().layout(w.canvas());
  const auto bounds=w.preview().placement(id)->bounds;UiPoint p{bounds.x+20,bounds.y+20};
  AE_EXPECT_TRUE(w.pointer({1,UiPointerPhase::Down,p}),"canvas owns pointer");
  w.pointer({1,UiPointerPhase::Move,{p.x+50,p.y+20}});w.pointer({1,UiPointerPhase::Up,{p.x+50,p.y+20}});
  AE_EXPECT_TRUE(w.document().find(id)->offsets.x==74,"drag edits authoring layout");
  AE_EXPECT_TRUE(w.history().undo(w.document())&&encoded(w.document())==before,"one drag is one history entry");
  frame();w.pointer({2,UiPointerPhase::Down,p});w.pointer({2,UiPointerPhase::Move,{p.x+10,p.y}});w.cancelPointers();
  AE_EXPECT_EQ(encoded(w.document()),before,"cancel rolls back active drag");
  w.setPreview(true);frame();
  const auto previewBounds=w.preview().placement(id)->bounds;p={previewBounds.x+20,previewBounds.y+20};
  w.pointer({3,UiPointerPhase::Down,p});w.pointer({3,UiPointerPhase::Up,p});
  AE_EXPECT_TRUE(w.preview().document().find(id)->value==1&&encoded(w.document())==before,"preview consumes real control without changing asset");
}
AE_TEST(gui_imgui_platform_numeric_input_uses_builtin_filter_and_buffer) {
  ImmediateGui im;UiDrawList list;float value=.5f;
  const auto frame=[&] {
    list.begin({0,0,500,300},fallbackFontMetrics());im.begin(500,300,1.f/60);
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({500,300});
    ImGui::Begin("numeric",nullptr,ImGuiWindowFlags_NoDecoration);
    ImGui::InputFloat("Valor",&value);ImGui::End();im.end(list);
  };
  frame();frame();im.pointer({1,UiPointerPhase::Down,{50,25}});frame();
  im.pointer({1,UiPointerPhase::Up,{50,25}});frame();frame();
  AE_EXPECT_TRUE(im.inputId() && im.replaceInput(im.inputId(),"0.75"),"native numeric field accepts platform draft without a custom callback");
  frame();frame();AE_EXPECT_TRUE(value==.75f,"numeric platform input reaches the typed value");
  im.finishInput(true);frame();frame();AE_EXPECT_TRUE(value==.75f && !im.inputId(),"numeric keyboard completion preserves the value and releases focus");
}
AE_TEST(gui_script_bridge_play_commands_events_and_lifetime) {
  editor::EditorDocument scene;editor::EditorMapScene map;editor::EditorPlayScene play;
  const auto object=scene.createEntity(scene.root(),runtime::ObjectKind::Folder,"Script");auto entity=*scene.find(object);
  auto *script=static_cast<scene::ScriptBehavior *>(entity.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="GuiProbe";script->source="GuiProbe.cs";scene.applyEntityValues(object,entity);
  GuiDocument authored;const auto button=authored.create(GuiKind::Button);std::string error;
  auto n=*authored.find(button);n.name="start";authored.update(n,error);
  play.configureGui(authored);play.setScriptRuntime(scriptApi(),"/test");
  AE_EXPECT_TRUE(play.start(scene,map)&&access.version==42&&access.available(),"real Play publishes complete ABI42");
  scene::ScriptGuiState state;
  const auto call=[&](u32 world,u32 node,u32 operation,std::string_view text={},float value=0) {
    return access.guiCommand(access.context,world,node,operation,reinterpret_cast<const u8*>(text.data()),static_cast<int>(text.size()),value,&state);
  };
  AE_EXPECT_EQ(call(0,0,0,"start"),1,"Find authored UI");const auto world=state.world;
  AE_EXPECT_TRUE(state.node==button&&call(world,button,2,"Executar")==1&&authored.find(button)->text=="Button","native API text changes Play copy");
  scene::ScriptGuiProperties properties;
  AE_EXPECT_EQ(access.guiProperties(access.context,world,button,0,&properties),1,"read actual runtime properties");
  properties.anchors[2]=.5f;properties.background=0xFF123456;properties.fontSize=22;
  AE_EXPECT_EQ(access.guiProperties(access.context,world,button,1,&properties),1,"write layout and style through ABI");
  play.gui().layout({0,0,640,480});
  AE_EXPECT_TRUE(play.gui().placement(button)->bounds.width==520 && play.gui().document().find(button)->background==0xFF123456,"ABI properties reach runtime layout and draw data");
  properties.fontSize=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(access.guiProperties(access.context,world,button,1,&properties)==0 && play.gui().document().find(button)->fontSize==22,"invalid ABI mutation preserves previous runtime state");
  u8 text[64]{};
  AE_EXPECT_TRUE(access.guiText(access.context,world,button,0,nullptr,0)==8 && access.guiText(access.context,world,button,0,text,sizeof(text))==8 && std::string(reinterpret_cast<char *>(text),8)=="Executar","native UTF8 size/read protocol");
  AE_EXPECT_TRUE(play.gui().pointer({1,UiPointerPhase::Down,{40,40}})&&play.gui().pointer({1,UiPointerPhase::Up,{40,40}}),"runtime receives actual pointer");
  AE_EXPECT_TRUE(call(world,0,6)==1&&state.event==1&&state.node==button,"script observes UI click");
  AE_EXPECT_TRUE(call(world,0,6)==0&&access.lastStatus(access.context)==u32(runtime::WorldStatus::Ok),"empty queue is not failure");
  state.kind=u32(GuiKind::Slider);
  AE_EXPECT_EQ(call(world,0,7,"volume"),1,"runtime API creates actual node");const auto slider=state.node;
  AE_EXPECT_TRUE(call(world,slider,3,{},.75f)==1&&play.gui().document().find(slider)->value==.75f,"value consumer state");
  AE_EXPECT_EQ(call(world,slider,8),1,"remove by stable ID");
  AE_EXPECT_TRUE(call(world,slider,1)==0&&access.lastStatus(access.context)==u32(runtime::WorldStatus::UnknownElement),"removed handle rejected");
  AE_EXPECT_TRUE(call(world+1,button,1)==0&&access.lastStatus(access.context)==u32(runtime::WorldStatus::ForeignWorld),"foreign world rejected");
  state.kind=u32(GuiKind::VBox);AE_EXPECT_EQ(call(world,0,7,"column"),1,"container created through actual ABI");const auto column=state.node;
  scene::ScriptGuiSizing sizing;AE_EXPECT_EQ(access.guiSizing(access.context,world,column,0,&sizing),1,"read typed container sizing");
  sizing.padding[0]=30;sizing.spacing[1]=15;AE_EXPECT_EQ(access.guiSizing(access.context,world,column,1,&sizing),1,"write runtime composition");
  AE_EXPECT_TRUE(play.gui().document().find(column)->sizing.padding.left==30 && play.gui().document().find(column)->sizing.spacing.y==15,"sizing reaches the real document");
  sizing.alignment=256;AE_EXPECT_EQ(access.guiSizing(access.context,world,column,1,&sizing),0,"invalid wide enum cannot truncate into valid state");
  state.kind=u32(GuiKind::Image);AE_EXPECT_EQ(call(world,column,7,"picture"),1,"image created as real child");const auto picture=state.node;
  AE_EXPECT_EQ(call(world,picture,10,"UI/banner.png"),1,"image resource changed by API");
  AE_EXPECT_TRUE(access.guiText(access.context,world,picture,2,nullptr,0)==13 && call(world,picture,10,"../bad.png")==0,"image UTF8 read and path validation");
  scene::ScriptGuiBehavior behavior;
  AE_EXPECT_EQ(access.guiBehavior(access.context,world,picture,0,&behavior),1,"read real Image composition");
  behavior.clickable=1;behavior.action=4;behavior.enabled=1;behavior.to[0]=40;behavior.duration=.2f;
  AE_EXPECT_EQ(access.guiBehavior(access.context,world,picture,1,&behavior),1,"write interaction and animation through native ABI");
  AE_EXPECT_TRUE(call(world,picture,13)==1,"API starts the authored Image animation");
  play.gui().advance(.2);play.gui().layout({0,0,640,480});
  AE_EXPECT_TRUE(play.gui().document().find(picture)->motion.to.x==40 && call(world,picture,14)==1,"animation uses actual authored pose and stop command");
  behavior.easing=256;AE_EXPECT_EQ(access.guiBehavior(access.context,world,picture,1,&behavior),0,"wide enum rejected without truncating");
  scene::ScriptGuiAction action{1,2,picture,0};
  AE_EXPECT_EQ(access.guiAction(access.context,world,button,2,0,&action),1,"append actual ordered action via ABI41");
  AE_EXPECT_EQ(access.guiAction(access.context,world,button,0,0,nullptr),1,"runtime owns persisted listener list");
  action.event=256;AE_EXPECT_EQ(access.guiAction(access.context,world,button,3,0,&action),-1,"reject wide event without mutation");
  AE_EXPECT_EQ(access.guiAction(access.context,world,button,1,0,&action),1,"read original action after failed update");
  AE_EXPECT_EQ(action.event,1u,"failed update preserves event");
  scene::ScriptGuiTransitions transitions;transitions.enabled=1;transitions.duration=0;transitions.poses[10]=.5f;
  AE_EXPECT_EQ(access.guiTransitions(access.context,world,picture,1,&transitions),1,"write typed state composition through real ABI");
  play.gui().setEnabled(picture,false);play.gui().layout({0,0,640,480});
  AE_EXPECT_TRUE(play.gui().placement(picture)->scale==.5f && play.gui().placement(picture)->opacity==.45f,"ABI state properties affect runtime layout");
  transitions.easing=256;AE_EXPECT_EQ(access.guiTransitions(access.context,world,picture,1,&transitions),0,"invalid transition preserves valid state");
  scene::ScriptGuiCanvas canvas;AE_EXPECT_EQ(access.guiCanvas(access.context,world,0,&canvas),1,"read authoring canvas copy");
  canvas.mode=1;canvas.position[2]=5;canvas.rotation[1]=20;AE_EXPECT_EQ(access.guiCanvas(access.context,world,1,&canvas),1,"write actual world plane via ABI");
  AE_EXPECT_TRUE(play.gui().document().canvas().mode==GuiCanvasMode::World && authored.canvas().mode==GuiCanvasMode::Screen,"world configuration preserves authoring isolation");
  play.stop();AE_EXPECT_TRUE(call(world,button,1)==0&&access.lastStatus(access.context)==u32(runtime::WorldStatus::NotRunning),"Stop invalidates handles");
}
AE_TEST(gui_image_actions_persistence_duplication_and_lifecycle) {
  GuiDocument d;std::string error;const auto root=d.create(GuiKind::Panel),image=d.create(GuiKind::Image,root),target=d.create(GuiKind::Text,root);
  auto n=*d.find(image);n.interaction={true,GuiClickAction::ToggleVisible,target,0};d.update(n,error);
  const auto saved=encoded(d);GuiDocument restored;std::istringstream stream(saved);
  AE_EXPECT_TRUE(restored.read(stream,error)&&encoded(restored)==saved,"interaction and motion roundtrip v3");
  const auto clone=restored.duplicate(root);const auto *copy=restored.find(clone+1);
  AE_EXPECT_TRUE(copy && copy->interaction.target==clone+2,"duplicate remaps action targets inside subtree");
  GuiRuntime r;r.load(d);r.layout({0,0,640,480});const auto bounds=r.placement(image)->bounds;const UiPoint point{bounds.x+5,bounds.y+5};
  AE_EXPECT_TRUE(r.hit(point,true)==image && r.pointer({1,UiPointerPhase::Down,point}) && r.pointer({1,UiPointerPhase::Up,point}),"Image retains identity and accepts click");
  GuiEvent event;AE_EXPECT_TRUE(r.poll(event)&&event.kind==GuiEventKind::Click&&event.node==image&&!r.document().find(target)->visible&&d.find(target)->visible,"action executes and event reaches script independently of authoring");
  r.pointer({1,UiPointerPhase::Down,point});r.setEnabled(root,false);
  AE_EXPECT_TRUE(!r.captures(1),"ancestor disable cancels image capture");r.setEnabled(root,true);
  r.document().remove(target);r.pointer({1,UiPointerPhase::Down,point});r.pointer({1,UiPointerPhase::Up,point});
  AE_EXPECT_TRUE(!r.diagnostic().empty(),"deleted target produces explicit diagnostic");
  r.document().remove(root);AE_EXPECT_TRUE(!r.poll(event),"removed event source cannot escape as a stale handle");
  // Legacy images remain decorative; load defaults do not invent click behavior.
  GuiDocument legacy;legacy.create(GuiKind::Image);std::istringstream lines(encoded(legacy));std::string header,canvas,line;
  std::getline(lines,header);std::getline(lines,canvas);std::getline(lines,line);for(int i=0;i<38;++i)line.erase(line.find_last_of(' '));
  std::istringstream v2("AEUI 2 1 2\n"+canvas+"\n"+line+"\n");
  AE_EXPECT_TRUE(restored.read(v2,error)&&!restored.nodes()[0].interaction.clickable&&!restored.nodes()[0].motion.enabled,"v2 migration preserves existing meaning");
}
AE_TEST(gui_motion_composes_layout_children_hit_opacity_and_clock) {
  GuiDocument d;std::string error;const auto root=d.create(GuiKind::Panel),image=d.create(GuiKind::Image,root);
  auto parent=*d.find(root);parent.offsets={100,100,200,200};parent.motion.enabled=parent.motion.autoPlay=true;
  parent.motion.easing=GuiEasing::Linear;parent.motion.duration=1;parent.motion.to={100,0,2,.4f};d.update(parent,error);
  auto child=*d.find(image);child.offsets={10,10,30,30};child.interaction.clickable=true;d.update(child,error);
  const auto saved=encoded(d);GuiRuntime r;r.load(d);r.layout({0,0,640,480});r.advance(.5);
  const auto *p=r.placement(image);
  AE_EXPECT_TRUE(p && p->bounds.x==140 && p->bounds.y==90 && p->bounds.width==30 && std::abs(p->opacity-.7f)<.001f,"children inherit animated affine pose and opacity");
  AE_EXPECT_TRUE(r.hit({145,95},true)==image && r.hit({115,115},true)==0,"hit area follows the rendered pose");
  GuiRuntime split;split.load(d);split.layout({0,0,640,480});for(int i=0;i<4;++i)split.advance(.125);
  AE_EXPECT_EQ(split.placement(image)->bounds.x,p->bounds.x,"same elapsed time is independent of frame count");
  GuiRuntime author;author.load(d,false);author.layout({0,0,640,480});author.advance(.5);
  AE_EXPECT_TRUE(author.placement(image)->bounds.x==110 && encoded(d)==saved,"authoring remains unchanged and unanimated");
  AE_EXPECT_TRUE(r.stopAnimation(root) && r.placement(image)->bounds.x==110,"stop restores base layout recursively");
  parent.motion.delay=1;parent.motion.loop=parent.motion.pingPong=true;d.update(parent,error);r.load(d);r.layout({0,0,640,480});r.advance(2.5);
  AE_EXPECT_EQ(r.placement(image)->bounds.x,140.f,"large step honors initial delay and return leg");
  parent.motion.loop=parent.motion.pingPong=false;parent.motion.delay=0;parent.motion.to={0,0,1,0};d.update(parent,error);r.load(d);r.layout({0,0,640,480});r.advance(1);
  AE_EXPECT_TRUE(r.placement(image)->opacity==0 && r.hit({115,115},true)==0,"fully faded hierarchy no longer intercepts input");
  d.remove(root);r.document()=d;r.advance(.1);AE_EXPECT_TRUE(!r.playAnimation(root)&&r.placements().empty(),"removal releases animation ownership");
}
AE_TEST(gui_ordered_actions_roundtrip_history_and_error_continuation) {
  GuiDocument d;std::string error;const auto root=d.create(GuiKind::Panel),source=d.create(GuiKind::Image,root),target=d.create(GuiKind::Progress,root);
  auto n=*d.find(source);n.interaction.clickable=true;
  n.actions={{GuiEventKind::Click,GuiClickAction::ToggleVisible,target,0},{GuiEventKind::Click,GuiClickAction::SetValue,target,.8f},{GuiEventKind::Click,GuiClickAction::PlayAnimation,target,0}};d.update(n,error);
  auto t=*d.find(target);t.visible=false;t.motion.enabled=true;t.motion.to={50,0,1,.5f};d.update(t,error);
  GuiHistory history;history.begin(d);n.transitions.enabled=true;d.update(n,error);history.commit(d);
  const auto saved=encoded(d);GuiDocument restored;std::istringstream stream(saved);AE_EXPECT_TRUE(restored.read(stream,error)&&encoded(restored)==saved,"AEUI4 stores ordered actions and state styles");
  AE_EXPECT_TRUE(history.undo(d)&&!d.find(source)->transitions.enabled&&history.redo(d)&&encoded(d)==saved,"state authoring participates in undo/redo");
  const auto clone=restored.duplicate(root);AE_EXPECT_TRUE(restored.find(clone+1)->actions[2].target==clone+2,"all subtree listener references remap on duplication");
  GuiRuntime r;r.load(d);r.layout({0,0,640,480});const auto b=r.placement(source)->bounds;const UiPoint point{b.x+5,b.y+5};
  r.pointer({1,UiPointerPhase::Down,point});r.pointer({1,UiPointerPhase::Up,point});r.advance(.3);
  AE_EXPECT_TRUE(r.document().find(target)->visible&&r.document().find(target)->value==.8f&&r.placement(target)->opacity==.5f,"one Image click shows target, assigns value, then animates it");
  n=*r.document().find(source);n.actions[0].target=999;r.document().update(n,error);r.setValue(target,0);
  r.pointer({1,UiPointerPhase::Down,point});r.pointer({1,UiPointerPhase::Up,point});
  AE_EXPECT_TRUE(r.document().find(target)->value==.8f&&!r.diagnostic().empty(),"broken listener reports error while following actions still execute");
  n.actions.assign(17,{});AE_EXPECT_TRUE(!r.document().update(n,error),"bounded listener count rejects oversized mutations transactionally");
}
AE_TEST(gui_visual_states_compose_animation_and_interrupt_without_snapping) {
  GuiDocument d;std::string error;const auto id=d.create(GuiKind::Image);auto n=*d.find(id);n.offsets={100,100,200,200};n.interaction.clickable=true;
  n.transitions.enabled=true;n.transitions.duration=1;n.transitions.easing=GuiEasing::Linear;n.transitions.pressed={{0,0,.5f,.6f},0xFF00FFFF};
  n.motion.enabled=n.motion.autoPlay=true;n.motion.duration=1;n.motion.easing=GuiEasing::Linear;n.motion.to={20,0,2,1};d.update(n,error);
  GuiRuntime r;r.load(d);r.layout({0,0,640,480});const auto revision=r.document().revision();
  r.pointer({1,UiPointerPhase::Down,{105,105}});r.advance(.5);
  AE_EXPECT_TRUE(std::abs(r.placement(id)->scale-1.125f)<.001f&&std::abs(r.placement(id)->opacity-.8f)<.001f,"pressed pose composes with clocked animation");
  const auto current=r.placement(id)->opacity;r.pointer({1,UiPointerPhase::Cancel,{105,105}});
  AE_EXPECT_TRUE(std::abs(r.placement(id)->opacity-current)<.001f,"release starts from current blended pose");r.advance(.5);
  AE_EXPECT_TRUE(std::abs(r.placement(id)->opacity-.9f)<.001f&&r.document().revision()==revision,"return transition never writes authored data");
  r.setEnabled(id,false);r.advance(1);AE_EXPECT_TRUE(!r.placement(id)->enabled&&std::abs(r.placement(id)->opacity-.45f)<.001f&&r.hit({150,150},true)==0,"disabled visuals and input agree");
  r.setEnabled(id,true);r.advance(1);r.stopAnimation(id);
  r.pointer({2,UiPointerPhase::Down,{101,101}});r.advance(1);r.pointer({2,UiPointerPhase::Up,{101,101}});GuiEvent e;
  AE_EXPECT_TRUE(r.poll(e)&&e.kind==GuiEventKind::Click,"shrinking pressed visuals retain stable capture geometry until release");
  UiDrawList draw;r.draw(draw);AE_EXPECT_TRUE(n.background==0,"visual states do not invent an opaque Image background");
  n=*r.document().find(id);n.transitions.pressed.pose.opacity=0;n.transitions.duration=0;r.document().update(n,error);r.layout({0,0,640,480});
  r.pointer({3,UiPointerPhase::Down,{150,150}});AE_EXPECT_TRUE(r.captures(3)&&r.placement(id)->opacity==0,"a transparent pressed state preserves the captured gesture");
  r.pointer({3,UiPointerPhase::Up,{150,150}});AE_EXPECT_TRUE(r.poll(e)&&r.placement(id)->opacity==1,"release restores normal state and delivers the valid click");
}
AE_TEST(gui_value_actions_bound_recursive_dispatch_and_preserve_sources) {
  GuiDocument d;std::string error;const auto id=d.create(GuiKind::Toggle);auto n=*d.find(id);
  n.actions={{GuiEventKind::ValueChanged,GuiClickAction::SetValue,0,0},{GuiEventKind::ValueChanged,GuiClickAction::SetValue,0,1}};d.update(n,error);
  GuiRuntime r;r.load(d);r.layout({0,0,640,480});r.pointer({1,UiPointerPhase::Down,{40,40}});r.pointer({1,UiPointerPhase::Up,{40,40}});
  AE_EXPECT_TRUE(r.diagnostic().find("limit")!=std::string::npos,"cyclic listener graph is bounded and diagnosed without recursion");
  GuiEvent e;u32 count=0;while(r.poll(e))++count;AE_EXPECT_TRUE(count<=256,"script event queue remains bounded");
  r.document().remove(id);r.advance(.1);AE_EXPECT_TRUE(r.placements().empty(),"removed source releases runtime state");
}
AE_TEST(gui_motion_full_document_keeps_authoring_and_measures_update_cost) {
  GuiDocument d;std::string error;
  for(u32 i=0;i<GuiDocument::kMaximumNodes;++i) {
    const auto id=d.create(GuiKind::Panel);auto n=*d.find(id);
    n.offsets={float(i%16)*32,float(i/16)*12,float(i%16)*32+24,float(i/16)*12+10};
    n.motion.enabled=n.motion.autoPlay=n.motion.loop=true;n.motion.duration=.7f;n.motion.to={20,0,1.1f,.5f};
    AE_EXPECT_TRUE(d.update(n,error),"authored animation in maximum-size document");
  }
  GuiRuntime r;r.load(d);r.layout({0,0,1200,900});const auto revision=r.document().revision();
  for(int frame=0;frame<8;++frame)r.advance(1.0/60);
  const auto start=std::chrono::steady_clock::now();
  for(int frame=0;frame<120;++frame)r.advance(1.0/60);
  const auto microseconds=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/120;
  std::printf("PROFILE UI motion: 1024 nodes/animations, 120 warm frames, host CPU update %.3f us/frame (no GPU draw)\n",microseconds);
  AE_EXPECT_TRUE(r.document().revision()==revision && r.placements().size()==1024 && r.diagnostic().empty(),"animation does not mutate authored data or overflow at the documented node limit");
  for(const auto &p:r.placements())AE_EXPECT_TRUE(isFinite(p.bounds)&&p.opacity>=.5f&&p.opacity<=1,"every animated node has usable geometry and opacity");
}
