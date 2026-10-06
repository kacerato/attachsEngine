#include "harness.h"
#include "runtime/scene_gui.h"
#include "editor/editor_archive.h"
#include "editor/editor_session.h"
#include "runtime/primitive_object.h"
#include "renderer/primitive_geometry.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_theme.h"
#include "scene/character.h"
#include "scene/camera_look.h"
#include <cmath>
#include <chrono>
#include <sstream>
#include <fstream>
using namespace ae;using namespace ae::ui;
namespace {
GuiDocument inputControls() {
  GuiDocument d;std::string error;
  for(auto kind:{GuiKind::Joystick,GuiKind::ActionButton,GuiKind::LookArea}) {
    auto id=d.create(kind);auto n=*d.find(id);n.offsets={float(id-1)*200,0,float(id)*200,200};n.control.deadzone=0;n.control.inputRadius=80;d.update(n,error);
  }
  return d;
}
}
AE_TEST(gui_authored_controls_roundtrip_simultaneous_sources_and_cancel) {
  auto d=inputControls();std::ostringstream out;d.write(out);GuiDocument saved;std::string error;std::istringstream in(out.str());
  AE_EXPECT_TRUE(saved.read(in,error)&&saved.find(1)->control.action=="Mover"&&saved.find(3)->kind==GuiKind::LookArea,"AEUI5 typed controls persist");
  GuiRuntime r;r.load(saved);r.layout({0,0,600,200});
  AE_EXPECT_TRUE(r.pointer({1,UiPointerPhase::Down,{100,100}})&&r.pointer({2,UiPointerPhase::Down,{300,100}})&&r.pointer({3,UiPointerPhase::Down,{500,100}}),"three pointers captured independently");
  r.pointer({1,UiPointerPhase::Move,{180,100}});r.pointer({3,UiPointerPhase::Move,{520,80}});
  AE_EXPECT_TRUE(r.controlState(1)->value.x>.99f&&r.controlState(2)->down&&r.controlState(3)->value.x>.09f,"movement, held button and relative look coexist");
  r.pointer({2,UiPointerPhase::Up,{300,100}});AE_EXPECT_TRUE(r.takeControlPresses(2)==1&&r.takeControlPresses(2)==0,"quick press buffered once despite release before sample");
  r.pointer({1,UiPointerPhase::Cancel,{180,100}});AE_EXPECT_TRUE(r.controlState(1)->value.x==0&&r.captures(3),"cancel one source leaves look captured");
  r.setEnabled(3,false);AE_EXPECT_TRUE(!r.captures(3)&&r.controlState(3)->value.x==0,"disabling clears relative delta and capture");
  auto bad=*saved.find(1);bad.control.deadzone=1;AE_EXPECT_TRUE(!saved.update(bad,error),"invalid curve rejected transactionally");
}
AE_TEST(gui_scoped_actions_drive_real_character_and_buffer_jump) {
  editor::EditorDocument g;editor::EditorMapScene resources;auto floor=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Floor");auto object=*g.find(floor);object.transform.position[1]=-.5f;
  object.components.add(scene::PhysicsBody::descriptor);auto *shape=static_cast<scene::Collider*>(object.components.add(scene::Collider::descriptor));shape->halfX=20;shape->halfY=.5f;shape->halfZ=20;g.applyEntityValues(floor,object);
  auto actor=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Character");object=*g.find(actor);auto *character=static_cast<scene::Character*>(object.components.add(scene::Character::descriptor));character->speed=2;g.applyEntityValues(actor,object);
  const auto asset=resources::assetGuidFromSeed("input-controls");auto owner=g.createEntity(g.root(),runtime::ObjectKind::Folder,"HUD");object=*g.find(owner);auto *c=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));c->document=asset;c->inputReceiver=actor;g.applyEntityValues(owner,object);
  const auto authored=editor::serializeEditorDocument(g,0);editor::EditorDocument restored;AE_EXPECT_TRUE(editor::deserializeEditorDocument(authored,0,restored),"Canvas receiver reference survives scene persistence");
  editor::EditorPlayScene play;play.configureSceneGui([&](resources::AssetGuid,GuiDocument &out,std::string &){out=inputControls();return true;});AE_EXPECT_TRUE(play.start(restored,resources),"real Jolt motor and scoped UI host");
  for(int i=0;i<30;++i)AE_EXPECT_TRUE(play.advance(1./60),"settle on ground");
  auto &host=play.sceneGui();auto lease=host.instanceFor(play.world(),owner);auto *instance=host.find(play.world(),lease);AE_EXPECT_TRUE(instance,"live source instance");
  const float eye[3]{0,3,-7};auto view=renderer::buildPerspectiveFrustum(eye,0,0,3);host.prepare(play.world(),view,{0,0,600,200},{0,0,600,200});
  instance->runtime.pointer({1,UiPointerPhase::Down,{100,100}});instance->runtime.pointer({1,UiPointerPhase::Move,{180,100}});
  for(int i=0;i<60;++i){play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"UI action drives fixed physics");}
  AE_EXPECT_TRUE(play.world().graph().find(actor)->transform.position[0]>1.8f,"character moves at authored speed");
  instance->runtime.pointer({1,UiPointerPhase::Up,{180,100}});instance->runtime.pointer({2,UiPointerPhase::Down,{300,100}});instance->runtime.pointer({2,UiPointerPhase::Up,{300,100}});play.submitInput({},1./240);
  AE_EXPECT_TRUE(play.advance(1./240),"sample can precede a fixed tick");play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"buffered jump consumed by motor");
  float apex=0;for(int i=0;i<35;++i){play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"jump trajectory");apex=std::max(apex,play.world().graph().find(actor)->transform.position[1]);}
  AE_EXPECT_TRUE(apex>.5f,"short tap produces one real grounded jump");
  for(int i=0;i<90;++i){play.submitInput({},1./60);AE_EXPECT_TRUE(play.advance(1./60),"land again");}
  instance->runtime.pointer({2,UiPointerPhase::Down,{300,100}});instance->runtime.pointer({2,UiPointerPhase::Up,{300,100}});play.submitInput({},0);
  instance->runtime.setEnabled(2,false);AE_EXPECT_TRUE(play.advance(1./60)&&play.world().graph().find(actor)->transform.position[1]<.1f,"disabled source cancels a queued jump before fixed physics");
  instance->runtime.pointer({1,UiPointerPhase::Down,{100,100}});instance->runtime.pointer({1,UiPointerPhase::Move,{180,100}});play.submitInput({},0);
  play.setInputFocus(false);play.submitInput({},0);play.setInputFocus(true);play.submitInput({},0);
  AE_EXPECT_TRUE(host.inputFor(actor)->axis(host.inputFor(actor)->map().moveAction())==0&&!instance->runtime.captures(1),"focus loss requires a fresh gesture after resume");
  AE_EXPECT_EQ(editor::serializeEditorDocument(restored,0),authored,"runtime input never mutates authoring");play.stop();
}
AE_TEST(gui_virtual_action_sources_aggregate_without_cross_cancellation) {
  runtime::InputService input;runtime::InputDeviceState state;
  state.virtualActions={{1,1,"Mover",runtime::ActionKind::Axis2D,.8f,0,0},{2,1,"Mover",runtime::ActionKind::Axis2D,0,.6f,0},
    {1,2,"Saltar",runtime::ActionKind::Button,1,0,1},{2,2,"Saltar",runtime::ActionKind::Button,1,0,1}};
  input.submit(state,1./60);float v[2];input.axis2("Mover",v);AE_EXPECT_TRUE(v[0]>.7f&&v[1]==0&&input.pressed("Saltar"),"greatest vector magnitude and button OR");
  state.virtualActions.erase(state.virtualActions.begin());state.virtualActions.erase(state.virtualActions.begin()+1);state.virtualActions.back().pressCount=0;input.submit(state,1./60);input.axis2("Mover",v);
  AE_EXPECT_TRUE(v[0]==0&&v[1]>.5f&&input.pressed("Saltar")&&!input.justPressed("Saltar"),"removing one Canvas source leaves the other held contribution");
  input.setDeviceGroups(runtime::InputKeyboardMouse);input.submit(state,1./60);AE_EXPECT_TRUE(input.axis("Mover")==0&&!input.pressed("Saltar"),"authored sources honor the touch device filter");
}
AE_TEST(gui_scoped_look_uses_assigned_camera_and_receiver_isolation) {
  runtime::SceneGraph g;const auto asset=resources::assetGuidFromSeed("look-control");
  auto actor=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Actor");auto object=*g.find(actor);object.components.add(scene::Character::descriptor);g.applyEntityValues(actor,object);
  auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Assigned camera");object=*g.find(camera);object.components.add(scene::Camera::descriptor);object.components.add(scene::CameraLook::descriptor);g.applyEntityValues(camera,object);
  auto other=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Other camera");object=*g.find(other);object.components.add(scene::Camera::descriptor);object.components.add(scene::CameraLook::descriptor);g.applyEntityValues(other,object);
  auto hud=g.createEntity(g.root(),runtime::ObjectKind::Folder,"HUD");object=*g.find(hud);auto *c=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));c->document=asset;c->inputReceiver=actor;c->inputCamera=camera;c->movementSpace=2;g.applyEntityValues(hud,object);
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(g),"live world with typed receiver/camera references");runtime::SceneGui gui;gui.configure([&](resources::AssetGuid,GuiDocument &out,std::string &){out=inputControls();return true;});
  const float eye[3]{0,0,-7};auto view=renderer::buildPerspectiveFrustum(eye,0,0,3);gui.prepare(world,view,{0,0,600,200},{0,0,600,200});
  AE_EXPECT_TRUE(gui.pointer(world,{3,UiPointerPhase::Down,{500,100}})&&gui.pointer(world,{3,UiPointerPhase::Move,{540,100}}),"scene host routes authored look area");
  runtime::InputService global;gui.submitInput(world,global,{},1./60);
  AE_EXPECT_TRUE(std::abs(world.graph().find(camera)->transform.rotationDegrees[1])>1&&world.graph().find(other)->transform.rotationDegrees[1]==0&&global.axis("Olhar")==0,"only assigned camera rotates; scoped action does not leak to global player");
  const auto yaw=world.graph().find(camera)->transform.rotationDegrees[1];gui.submitInput(world,global,{},1./60);AE_EXPECT_EQ(world.graph().find(camera)->transform.rotationDegrees[1],yaw,"relative look delta consumed once");
  const auto lease=gui.instanceFor(world,hud);gui.cancelPointers();
  AE_EXPECT_TRUE(!gui.find(world,lease)->runtime.captures(3)&&gui.pointer(world,{3,UiPointerPhase::Up,{540,100}})&&!gui.captures(3),"cancellation clears control state and consumes the retired gesture release");
}
AE_TEST(gui_editor_projection_shared_source_and_stable_identity) {
  runtime::SceneGraph graph;resources::AssetRegistry assets;
  const auto guid=resources::assetGuidFromSeed("editor-ui-tree");
  resources::AssetRecord record;record.guid=guid;record.type=resources::AssetType::UiDocument;record.path="UI/tree.aeui";
  AE_EXPECT_TRUE(assets.add(record),"real registered UI source");
  GuiDocument source;const auto box=source.create(GuiKind::HBox),image=source.create(GuiKind::Image,box);
  const auto label=source.create(GuiKind::Text);std::string error;
  const auto a=graph.createEntity(graph.root(),runtime::ObjectKind::Folder,"A");auto object=*graph.find(a);
  auto *value=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));value->document=guid;
  const auto instance=value->instanceId();graph.applyEntityValues(a,object);
  runtime::ObjectCloneMap remap;const auto b=graph.cloneSubtree(a,graph.root(),remap);AE_EXPECT_TRUE(b,"real duplicate Canvas owner");
  const auto before=graph.revision();editor::EditorGuiTree tree;int reads=0;
  auto load=[&](resources::AssetGuid asset,GuiDocument &out,std::string &){++reads;out=source;return asset==guid;};
  tree.rebuild(graph,assets,load,"",{});
  AE_EXPECT_TRUE(reads==1&&tree.rows().size()==8&&graph.revision()==before,"two projections, one immutable load, no fake scene nodes");
  u32 token=0;for(const auto &row:tree.rows())if(row.target.owner==a&&row.target.node==image) {
    token=row.token;AE_EXPECT_TRUE(row.parent==box&&row.depth==2,"actual UI parent and depth");
  }
  auto node=*source.find(image);node.name="clickable-image";source.update(node,error);source.reorder(label,-1);
  tree.rebuild(graph,assets,load,record.path,source);
  AE_EXPECT_TRUE(reads==1&&tree.find(token)&&tree.find(token)->name==node.name,"rename/reorder do not retarget a captured widget");
  u32 matches=0;for(const auto &row:tree.rows())if(row.name==node.name)++matches;
  AE_EXPECT_TRUE(matches==2,"both instances reflect the same active editable source");
  graph.destroyEntity(a);tree.rebuild(graph,assets,load,record.path,source);
  AE_EXPECT_TRUE(!tree.find(token)&&tree.rows().size()==4,"removal retires only the removed owner projection");
  AE_EXPECT_TRUE(tree.rows()[0].target.component==instance,"duplicated local component identity stays scoped by owner");
}
namespace {
runtime::ObjectId canvas(runtime::SceneGraph &g,resources::AssetGuid asset,std::string_view name,float x=0) {
  auto id=g.createEntity(g.root(),runtime::ObjectKind::Folder,name);auto v=*g.find(id);
  auto *c=static_cast<scene::UiCanvas*>(v.components.add(scene::UiCanvas::descriptor));
  c->document=asset;c->mode=1;c->resolution[0]=400;c->resolution[1]=200;c->unitsPerPixel=.005f;
  v.transform.position[0]=x;v.transform.position[2]=5;g.applyEntityValues(id,v);return id;
}
GuiDocument clickable() {GuiDocument doc;std::string error;auto id=doc.create(GuiKind::Image);auto n=*doc.find(id);n.interaction.clickable=true;n.offsets={40,40,160,100};doc.update(n,error);return doc;}
}
AE_TEST(gui_multi_pointer_device_identity_and_individual_cancellation) {
  GuiDocument d;std::string error;
  for(u32 i=0;i<3;++i){auto id=d.create(GuiKind::Toggle);auto n=*d.find(id);n.interaction.clickable=true;n.offsets={20+float(i)*130,20,120+float(i)*130,80};d.update(n,error);}
  GuiRuntime r;r.load(d);r.layout({0,0,500,300});
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Down,{30,30}})&&r.pointer({8,UiPointerPhase::Down,{160,30}})&&r.pointer({7,UiPointerPhase::Down,{290,30},0,UiPointerDevice::Mouse}),"three independent controls and mouse/touch with the same ID");
  AE_EXPECT_EQ(r.captureCount(),3u,"all gestures coexist");
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Down,{160,30}}),"repeated down is consumed without stealing");
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Up,{30,30}})&&r.captureCount()==2,"release cancels only its own device and pointer");
  AE_EXPECT_TRUE(r.document().find(1)->value==1&&r.document().find(2)->value==0&&r.document().find(3)->value==0,"unreleased controls stay unchanged");
  r.setEnabled(2,false);
  AE_EXPECT_TRUE(!r.captures(8)&&r.captures(7,UiPointerDevice::Mouse),"disabled node does not cancel another control");
  AE_EXPECT_TRUE(r.pointer({7,UiPointerPhase::Up,{290,30},0,UiPointerDevice::Mouse})&&r.document().find(3)->value==1,"remaining mouse capture releases normally");
  GuiEvent event;u32 clicks=0;while(r.poll(event))if(event.kind==GuiEventKind::Click)++clicks;
  AE_EXPECT_EQ(clicks,2u,"no duplicate click from a repeated down or a canceled capture");
}
AE_TEST(gui_editor_contextual_commands_global_undo_and_reopen) {
  const auto root=std::filesystem::absolute(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build"/("gui-context-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
  std::error_code ec;std::filesystem::create_directories(root,ec);editor::EditorSession session;
  AE_EXPECT_TRUE(!ec&&session.setProjectDirectory(root.generic_string().c_str()),"real authoring storage");
  auto read=[](const std::filesystem::path &path){std::ifstream file(path,std::ios::binary);return std::vector<u8>(std::istreambuf_iterator<char>(file),{});};
  UiFont font;UiIconAtlas icons;const auto visuals=std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui";
  AE_EXPECT_TRUE(font.load(read(visuals/"astra-ui-font.aeuf"))&&icons.load(read(visuals/"astra-ui-icons.aeui")),"real editor font and atlas");
  session.initialize(&font,&icons);session.setSurface({0,0,1920,1080},{});session.gui().document()=clickable();
  AE_EXPECT_TRUE(session.gui().save(),"registered shared source");resources::AssetRegistry assets;
  resources::AssetRegistry::deserialize(session.serializeAssets(),assets);const auto guid=assets.findByPath("UI/main.aeui")->guid;
  const auto owner=canvas(session.document(),guid,"UI Owner");const auto other=canvas(session.document(),guid,"Other");
  const auto component=session.document().find(owner)->components.find(scene::UiCanvas::descriptor.id)->instanceId();
  const editor::EditorGuiTarget target{owner,component,guid,1};session.refreshGuiTree();
  session.update();session.update();
  UiDrawList screen;UiInputRouter router;screen.begin(session.screen().surface,fallbackFontMetrics());
  const auto layout=editor::buildEditorScreen(session.screen(),editor::editorTheme(),screen,router);
  u32 token=0;for(const auto &row:session.screen().guiRows)if(row.target==target)token=row.token;
  UiPoint point{};bool found=false;
  for(float y=layout.hierarchyPanel.y;y<layout.hierarchyPanel.bottom();y+=2) {
    const UiPoint sample{layout.hierarchyPanel.x+layout.hierarchyPanel.width*.6f,y};
    if(router.hitTest(sample).widgetId==editor::widgetId(editor::EditorWidget::GuiHierarchyRowBase)+token){point=sample;found=true;break;}
  }
  AE_EXPECT_TRUE(found&&token,"UI element has a real hierarchy hit region");
  session.handlePointer({19,UiPointerPhase::Down,point,1});session.handlePointer({19,UiPointerPhase::Up,point,1.1});session.update();
  AE_EXPECT_TRUE(session.screen().guiSelection==target&&session.screen().workspace==editor::EditorWorkspace::Scene&&session.screen().guiInspector,"actual hierarchy click selects contextual Inspector without leaving scene");
  AE_EXPECT_TRUE(session.gui().command(ui::GuiWorkbench::NodeCommand::Duplicate),"actual contextual duplicate command");
  const auto duplicate=session.gui().selected();session.refreshGuiTree();u32 copies=0;
  for(const auto &row:session.screen().guiRows)if(row.target.node==duplicate)++copies;
  AE_EXPECT_TRUE(copies==2&&session.history().canUndo(),"shared authoring projection and global resource history");
  auto moved=*session.document().find(owner);moved.transform.position[0]=3;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),owner,moved),"scene operation after UI edit");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.document().find(owner)->transform.position[0]==0&&session.gui().document().find(duplicate),"first undo restores scene only");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!session.gui().document().find(duplicate),"second undo restores authored UI");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.gui().document().find(duplicate),"redo restores the same UI identity");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"branch from UI undo");
  const auto fresh=session.gui().create(GuiKind::Text);AE_EXPECT_TRUE(fresh>duplicate,"new undo branch cannot recycle a stale node identity");
  AE_EXPECT_TRUE(session.gui().command(ui::GuiWorkbench::NodeCommand::Up)&&session.gui().document().nodes().front().id==fresh,"contextual reorder changes actual document order");
  AE_EXPECT_TRUE(session.gui().command(ui::GuiWorkbench::NodeCommand::Down)&&session.gui().document().nodes().back().id==fresh,"contextual down restores sibling order");
  AE_EXPECT_TRUE(session.gui().command(ui::GuiWorkbench::NodeCommand::Remove)&&!session.gui().document().find(fresh),"contextual removal changes the runtime source");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.gui().document().find(fresh),"global undo restores removed UI identity");
  auto node=*session.gui().document().find(fresh);node.name="Author text";node.text="Visible authored text";std::string error;
  session.gui().history().begin(session.gui().document());session.gui().document().update(node,error);session.gui().history().commit(session.gui().document());
  AE_EXPECT_TRUE(session.gui().save(),"source transaction persists contextual edit");
  const auto archive=editor::serializeEditorDocument(session.document(),0);editor::EditorSession reopened;
  AE_EXPECT_TRUE(reopened.setProjectDirectory(root.generic_string().c_str())&&reopened.loadAssets(session.serializeAssets())&&editor::deserializeEditorDocument(archive,0,reopened.document()),"scene and UI reopen together");
  reopened.initialize(nullptr,nullptr);reopened.refreshGuiTree();u32 persisted=0;
  for(const auto &row:reopened.screen().guiRows)if(row.name==node.name)++persisted;
  AE_EXPECT_TRUE(persisted==2&&reopened.gui().document().find(fresh)->text==node.text,"both Canvas trees reopen the authored source");
  AE_EXPECT_TRUE(!reopened.selectGuiElement({other,component,guid,99999}),"missing node is not rebound to another control");
  runtime::GameWorld world;runtime::SceneGui execution;world.load(session.document());execution.configure([&](resources::AssetGuid id,GuiDocument &doc,std::string &e){return session.loadGuiDocument(id,doc,e);});
  AE_EXPECT_TRUE(execution.reconcile(world)&&execution.instances().size()==2,"contextual authoring feeds real runtime instances");
  for(const auto &i:execution.instances())AE_EXPECT_TRUE(i->runtime.document().find(fresh)->text==node.text,"runtime consumes contextual values");
}
AE_TEST(gui_world_host_affine_inverse_reflection_and_singular_rejection) {
  GuiCanvas c;c.mode=GuiCanvasMode::World;c.resolution={400,200};c.unitsPerPixel=.004f;c.rotation[1]=15;
  const float eye[3]{};auto camera=renderer::buildPerspectiveFrustum(eye,0,0,1.5f);
  float host[16]{1.4f,.1f,0,0,.45f,.7f,.08f,0,0,0,1,0,.2f,.1f,5,1};GuiWorldFrame f;
  for(int reflection=0;reflection<2;++reflection) {
    AE_EXPECT_TRUE(f.configure(c,camera,{20,40,600,400},{0,0,640,480},host),"nonuniform scaled/sheared host remains representable");
    for(UiPoint p: {UiPoint{50,50},UiPoint{350,150}}) {UiPoint screen,back;float depth,distance;
      AE_EXPECT_TRUE(f.project(p,screen,depth)&&f.map(screen,back,distance)&&std::abs(p.x-back.x)<.003f&&std::abs(p.y-back.y)<.003f,"render and ray input use the same complete affine matrix");}
    host[0]=-host[0];host[1]=-host[1];
  }
  host[4]=host[0];host[5]=host[1];host[6]=0;
  c.rotation[1]=0;
  AE_EXPECT_TRUE(!f.configure(c,camera,{0,0,600,400},{0,0,600,400},host)&&!f.valid(),"singular plane does not publish NaN geometry");
}
AE_TEST(gui_scene_instances_without_editor_isolation_moving_pose_and_retirement) {
  const auto asset=resources::assetGuidFromSeed("ui-scene-test");runtime::SceneGraph graph;
  const auto a=canvas(graph,asset,"A",-.9f),b=canvas(graph,asset,"B",.9f);const auto original=clickable();
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(graph),"ordinary runtime scene owns canvas components");
  runtime::SceneGui gui;u32 reads=0;gui.configure([&](resources::AssetGuid id,GuiDocument &out,std::string &){++reads;out=original;return id==asset;});
  const float eye[3]{};const auto camera=renderer::buildPerspectiveFrustum(eye,0,0,1.5f);
  gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});const auto ia=gui.instanceFor(world,a),ib=gui.instanceFor(world,b);
  AE_EXPECT_TRUE(ia&&ib&&ia!=ib&&reads==1,"shared immutable asset produces separate runtime leases");
  AE_EXPECT_TRUE(gui.find(world,ia)->runtime.setText(1,"A changed")&&gui.find(world,ib)->runtime.document().find(1)->text==original.find(1)->text,"instance mutation does not edit another copy or the asset");
  auto outside=world.graph().find(a)->transform;outside.position[1]=5;world.poseGraph().setTransform(a,outside);gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});
  UiPoint clipped;float clippedDepth;
  AE_EXPECT_TRUE(gui.find(world,ia)->frame.project({60,60},clipped,clippedDepth)&&!(UiRect{0,30,600,400}).contains(clipped)&&!gui.pointer(world,{19,UiPointerPhase::Down,clipped}),"world pixels clipped outside the viewport cannot capture input");
  outside.position[1]=0;world.poseGraph().setTransform(a,outside);gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});
  UiPoint pa,pb;float depth;
  AE_EXPECT_TRUE(gui.find(world,ia)->frame.project({60,60},pa,depth)&&gui.find(world,ib)->frame.project({60,60},pb,depth),"both hosts render in the real camera");
  AE_EXPECT_TRUE(gui.pointer(world,{11,UiPointerPhase::Down,pa})&&gui.pointer(world,{12,UiPointerPhase::Down,pb}),"two world panels capture different fingers");
  auto pose=world.graph().find(a)->transform;pose.position[0]-=.3f;world.poseGraph().setTransform(a,pose);gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});
  gui.find(world,ia)->frame.project({60,60},pa,depth);
  AE_EXPECT_TRUE(gui.pointer(world,{11,UiPointerPhase::Up,pa})&&gui.captures(12),"host motion changes the mapping but not target ownership");
  GuiEvent e;AE_EXPECT_TRUE(gui.find(world,ia)->runtime.poll(e)&&e.kind==GuiEventKind::Click,"release over the moved host clicks its Image");
  world.destroyObject(world.handle(b));gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});
  AE_EXPECT_TRUE(!gui.find(world,ib)&&gui.pointer(world,{12,UiPointerPhase::Move,pb})&&gui.pointer(world,{12,UiPointerPhase::Up,pb})&&!gui.captures(12),"retired owner consumes the rest of its gesture without leaking to gameplay");
  gui.invalidateResources();gui.prepare(world,camera,{0,30,600,400},{0,0,600,450});
  AE_EXPECT_TRUE(!gui.find(world,ia)&&gui.instanceFor(world,a)!=ia,"asset reload cannot reinterpret an old lease");
}
AE_TEST(gui_scene_component_duplicate_archive_and_asset_registry_roundtrip) {
  scene::UiCanvas screen,worldCanvas;worldCanvas.mode=1;
  AE_EXPECT_TRUE(!scene::uiCanvasNumbers[0].presentation.isVisible(screen)&&scene::uiCanvasNumbers[0].presentation.isVisible(worldCanvas)&&!scene::uiCanvasBooleans[1].presentation.isVisible(screen),"world-only presentation properties are not offered as ineffective screen controls");
  const auto asset=resources::assetGuidFromSeed("ui-archive-test");editor::EditorDocument graph;
  const auto first=canvas(graph,asset,"host");runtime::ObjectCloneMap mapping;const auto clone=graph.cloneSubtree(first,graph.root(),mapping);
  AE_EXPECT_TRUE(clone&&clone!=first,"host duplication uses the existing scene graph");
  const auto bytes=editor::serializeEditorDocument(graph,0);editor::EditorDocument reopened;
  AE_EXPECT_TRUE(editor::deserializeEditorDocument(bytes,0,reopened),"scene serializer persists the new component");
  auto *a=static_cast<const scene::UiCanvas*>(reopened.find(first)->components.find(scene::UiCanvas::descriptor.id));
  auto *b=static_cast<const scene::UiCanvas*>(reopened.find(clone)->components.find(scene::UiCanvas::descriptor.id));
  AE_EXPECT_TRUE(a&&b&&a->document==asset&&b->document==asset&&a->instanceId()==b->instanceId(),"component identity is owner-local and document GUID persists");
  resources::AssetRegistry registry;resources::AssetRecord record;record.guid=asset;record.type=resources::AssetType::UiDocument;record.path="UI/panel.aeui";
  AE_EXPECT_TRUE(registry.add(record),"UI document is an actual registered resource kind");
  resources::AssetRegistry loaded;
  AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(registry.serialize(),loaded)&&loaded.find(asset)->type==resources::AssetType::UiDocument,"resource GUID/type survives registry serialization");
  runtime::GameWorld world;runtime::SceneGui gui;AE_EXPECT_TRUE(world.load(reopened),"reopened scene executes");
  gui.configure([&](resources::AssetGuid,GuiDocument &out,std::string &){out=clickable();return true;});gui.reconcile(world);
  AE_EXPECT_TRUE(gui.instanceFor(world,first)!=gui.instanceFor(world,clone)&&gui.instances().size()==2,"clone and original reopen as separate leases");
}
AE_TEST(gui_scene_document_publication_keeps_guid_and_rejects_external_edits) {
  const auto root=std::filesystem::absolute(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"build"/("gui-storage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
  std::error_code ec;std::filesystem::create_directories(root,ec);
  AE_EXPECT_TRUE(!ec,"isolated persistent fixture directory");
  editor::EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(root.generic_string().c_str()),"actual editor storage configured");
  session.gui().document()=clickable();
  AE_EXPECT_TRUE(session.gui().save(),"initial document and registry publish together");
  resources::AssetRegistry first;AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(session.serializeAssets(),first),"saved registry parses");
  const auto *record=first.findByPath("UI/main.aeui");AE_EXPECT_TRUE(record&&record->type==resources::AssetType::UiDocument,"save registers the document for scene binding");const auto guid=record->guid;
  std::string error;auto n=*session.gui().document().find(1);n.text="second save";session.gui().document().update(n,error);
  AE_EXPECT_TRUE(session.gui().save(),"existing document transaction uses the previous source hash");
  editor::EditorSession reopened;AE_EXPECT_TRUE(reopened.setProjectDirectory(root.generic_string().c_str())&&reopened.loadAssets(session.serializeAssets()),"project and asset registry reopen");
  resources::AssetRegistry second;resources::AssetRegistry::deserialize(reopened.serializeAssets(),second);
  AE_EXPECT_TRUE(second.findByPath("UI/main.aeui")->guid==guid&&reopened.gui().document().find(1)->text=="second save","stable GUID and edited content survive reopening");
  auto other=clickable();std::ostringstream bytes;other.write(bytes);
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(root/"UI/other.aeui",bytes.str()),"second authored document fixture");
  n=*reopened.gui().document().find(1);n.text="unsaved";reopened.gui().document().update(n,error);
  AE_EXPECT_TRUE(!reopened.gui().openResource("UI/other.aeui")&&reopened.gui().document().find(1)->text=="unsaved","switching canvas cannot discard an unsaved document");
  AE_EXPECT_TRUE(editor::EditorImportTransaction::writeText(root/"UI/main.aeui",bytes.str()),"simulate external source change");
  AE_EXPECT_TRUE(!reopened.gui().save(),"external modification rejected without overwriting the source or registry");
  std::vector<u8> retained;editor::EditorImportTransaction::read(root/"UI/main.aeui",retained);
  AE_EXPECT_EQ(std::string(retained.begin(),retained.end()),bytes.str(),"failed save preserves the external edit");
}
AE_TEST(gui_scene_canvas_follows_actual_dynamic_body_without_pose_authority) {
  editor::EditorSession session;std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)&&session.importMap(draws,materials,false,vertices,indices,0),"real cylinder geometry and resource library");
  auto &g=session.document();auto id=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"Cylinder");auto object=*g.find(id);
  AE_EXPECT_TRUE(runtime::configurePrimitive(object,scene::PrimitiveType::Cylinder,{4,session.mapScene().assetGuid(3),session.mapScene().materialForAsset(3)}),"real body and cylinder shape");
  auto *body=static_cast<scene::PhysicsBody*>(object.components.edit(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->gravityFactor=0;body->velocityX=.5f;body->angularY=12;g.applyEntityValues(id,object);
  const auto asset=resources::assetGuidFromSeed("physical-canvas");auto child=g.createEntity(id,runtime::ObjectKind::Folder,"UI");object=*g.find(child);auto *c=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));c->document=asset;c->mode=1;g.applyEntityValues(child,object);
  editor::EditorPlayScene play;play.configureSceneGui([&](resources::AssetGuid,GuiDocument &out,std::string &){out=clickable();return true;});
  AE_EXPECT_TRUE(play.start(g,session.mapScene()),"physics, scene and canvas start together");
  for(int step=0;step<600;++step){const bool ok=play.advance(1.0/60);if(!ok)std::fprintf(stderr,"step %d: %s\n",step,play.frameError().c_str());AE_EXPECT_TRUE(ok,"rotating solver body and complete Play frame advance");}
  const auto &world=play.world();
  AE_EXPECT_TRUE(world.authorityOf(world.handle(id))==runtime::TransformAuthority::PhysicsBody&&world.authorityOf(world.handle(child))==runtime::TransformAuthority::Free,"only the body owns the physics transform");
  const float eye[3]{0,0,-7};auto camera=renderer::buildPerspectiveFrustum(eye,0,0,1.5f);play.sceneGui().prepare(play.world(),camera,{0,0,600,400},{0,0,600,400});
  const auto lease=play.sceneGui().instanceFor(play.world(),child);auto *instance=play.sceneGui().find(play.world(),lease);
  AE_EXPECT_TRUE(instance&&instance->prepared&&instance->hostMatrix[12]>.1f&&std::abs(instance->hostMatrix[12]-world.graph().find(id)->transform.position[0])<.0001f,"UI samples the final solver pose used by the mesh");play.stop();
}
