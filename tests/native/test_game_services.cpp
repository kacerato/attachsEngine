#include "harness.h"
#include "editor/editor_document.h"
#include "editor/editor_play_scene.h"
#include "runtime/game_view.h"
#include "scene/script_behavior.h"
#include "scene/script_extensions.h"

#include <cmath>
#include <functional>
#include <string_view>

using namespace ae;
using namespace ae::editor;

// Bloco C: vista de jogo, linhas de depuração, mudanças de hierarquia e
// vibração pelas famílias da ABI42, no Play real (EditorPlayScene ->
// ScriptBridge). O runtime gerenciado é um duplo que só usa a ABI.
namespace {
struct ServicesRuntime {
  static scene::ScriptSceneAccess access;
  static std::function<void()> onStart,onUpdate;
  static int start(const u8 *,int,const u8 *,int,const scene::ScriptSceneAccess *value) {
    access=*value;if(onStart)onStart();return value->available()?0:1;
  }
  static scene::ScriptRuntimeApi api() {
    scene::ScriptRuntimeApi a{};
    a.start=&start;a.update=[](float){if(onUpdate)onUpdate();return 0;};a.fixedUpdate=[](float){return 0;};a.lateUpdate=a.fixedUpdate;a.stop=[]{};
    a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};
    a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};
    return a;
  }
  template<class T> static const T *family(std::string_view name) {
    u32 version=0,size=0;
    const auto *table=access.extension(access.context,reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()),&version,&size);
    return table&&version>=1&&size>=sizeof(T)?static_cast<const T*>(table):nullptr;
  }
};
scene::ScriptSceneAccess ServicesRuntime::access{};
std::function<void()> ServicesRuntime::onStart{},ServicesRuntime::onUpdate{};

EditorEntityId scripted(EditorDocument &doc,const char *name) {
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);auto values=*doc.find(id);
  auto *script=static_cast<scene::ScriptBehavior*>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="project.Probe";script->source="Probe.cs";doc.applyEntityValues(id,values);return id;
}
runtime::GameView viewLookingDownZ(bool orthographic=false) {
  runtime::GameView view;view.valid=true;view.width=800;view.height=400;view.dpi=420;
  renderer::PerspectiveVisibilitySettings settings{};settings.verticalFieldOfViewRadians=1.0471975512f;
  if(orthographic){settings.projection=renderer::CameraProjection::Orthographic;settings.orthographicHalfHeight=5;}
  const float eye[3]{0,1,-10};
  view.frustum=renderer::buildPerspectiveFrustum(eye,0,0,view.width/view.height,settings);
  return view;
}
bool near(float a,float b,float epsilon=1e-3f){return std::abs(a-b)<=epsilon;}
} // namespace

AE_TEST(game_view_screen_ray_and_world_projection_are_inverse_in_both_projections) {
  for(const bool orthographic:{false,true}) {
    const auto view=viewLookingDownZ(orthographic);
    AE_EXPECT_TRUE(view.frustum.valid,"frustum");
    float origin[3],direction[3];
    AE_EXPECT_TRUE(runtime::gameViewScreenRay(view,400,200,origin,direction),"center ray");
    AE_EXPECT_TRUE(near(direction[0],0)&&near(direction[1],0)&&near(direction[2],1),"center looks along +Z");
    const float world[3]{1.5f,2.f,6.f};float screen[3];
    AE_EXPECT_TRUE(runtime::gameViewWorldToScreen(view,world,screen),"projection");
    AE_EXPECT_TRUE(near(screen[2],16)&&screen[0]>400&&screen[1]>200,"right and above: origin bottom-left, z is view depth");
    AE_EXPECT_TRUE(runtime::gameViewScreenRay(view,screen[0],screen[1],origin,direction),"ray back through the pixel");
    // O ponto original está sobre o raio reconstruído.
    float toPoint[3]{world[0]-origin[0],world[1]-origin[1],world[2]-origin[2]};
    const float along=toPoint[0]*direction[0]+toPoint[1]*direction[1]+toPoint[2]*direction[2];
    float miss=0;for(u32 a=0;a<3;++a){const float d=toPoint[a]-direction[a]*along;miss+=d*d;}
    AE_EXPECT_TRUE(std::sqrt(miss)<1e-3f,orthographic?"orthographic roundtrip":"perspective roundtrip");
  }
  runtime::GameView invalid;float origin[3],direction[3];
  AE_EXPECT_TRUE(!runtime::gameViewScreenRay(invalid,1,1,origin,direction),"no view, no ray");
}

AE_TEST(game_services_families_publish_view_debug_lines_and_hierarchy_changes_in_play) {
  EditorDocument doc;const auto id=scripted(doc,"Sonda");
  const auto child=doc.createEntity(id,EditorEntityKind::Folder,"Filho");
  const auto other=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Outro pai");
  bool resolved=false;
  ServicesRuntime::onStart=[&]{
    resolved=ServicesRuntime::family<scene::ScriptViewOperations>(scene::kScriptView)&&
             ServicesRuntime::family<scene::ScriptDebugOperations>(scene::kScriptDebug)&&
             ServicesRuntime::family<scene::ScriptHierarchyOperations>(scene::kScriptHierarchy)&&
             !ServicesRuntime::family<scene::ScriptHapticsOperations>(scene::kScriptHaptics);
  };
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(ServicesRuntime::api(),"/project");
  AE_EXPECT_TRUE(play.start(doc,resources)&&resolved,"view/debug/hierarchy present; haptics absent without a vibrator");
  auto &abi=ServicesRuntime::access;
  const auto *view=ServicesRuntime::family<scene::ScriptViewOperations>(scene::kScriptView);
  scene::ScriptViewState state;
  AE_EXPECT_TRUE(view->state(abi.context,&state)==1&&!(state.flags&scene::kScriptViewValid),"before the first published frame the view is not valid");
  play.setGameView(viewLookingDownZ());
  AE_EXPECT_TRUE(view->state(abi.context,&state)==1&&(state.flags&scene::kScriptViewValid)&&state.width==800&&state.dpi==420,"published view");
  AE_EXPECT_TRUE(!(state.flags&scene::kScriptViewSafeArea)&&state.safeWidth==800&&state.safeHeight==400,"unreported safe area is the whole view and flagged so");
  float origin[3],direction[3];AE_EXPECT_TRUE(view->screenRay(abi.context,400,200,origin,direction)==1&&near(direction[2],1),"ABI ray");
  state.size=0;AE_EXPECT_TRUE(view->state(abi.context,&state)==0,"layout size checked");

  const auto *debug=ServicesRuntime::family<scene::ScriptDebugOperations>(scene::kScriptDebug);
  const float a[3]{0,0,0},b[3]{1,0,0};
  AE_EXPECT_TRUE(debug->drawLine(abi.context,a,b,0xffff0000u,.1f)==1&&debug->drawLine(abi.context,a,b,0xff00ff00u,0)==1,"two lines");
  AE_EXPECT_TRUE(debug->drawLine(abi.context,a,b,0,-1)==0,"negative duration refused");
  AE_EXPECT_EQ(play.debugLines().lines().size(),usize{2},"lines visible this frame");
  AE_EXPECT_TRUE(play.advance(.05),"frame");AE_EXPECT_EQ(play.debugLines().lines().size(),usize{1},"zero-duration line lasted one frame");
  AE_EXPECT_TRUE(play.advance(.1),"frame");AE_EXPECT_TRUE(play.advance(.02),"frame");
  AE_EXPECT_EQ(play.debugLines().lines().size(),usize{0},"timed line expired");

  // Hierarquia: reparent pelo caminho dos scripts, aplicado no ponto seguro.
  const auto *hierarchy=ServicesRuntime::family<scene::ScriptHierarchyOperations>(scene::kScriptHierarchy);
  scene::ScriptHierarchyChange changes[16];
  while(hierarchy->pollChanges(abi.context,changes,16)>0) {}
  AE_EXPECT_TRUE(abi.setParent(abi.context,child,other,0)!=0,"reparent queued");
  AE_EXPECT_TRUE(play.advance(.02),"safe point applies reparent");
  bool parentChanged=false,oldChildren=false,newChildren=false;
  for(int n=hierarchy->pollChanges(abi.context,changes,16),i=0;i<n;++i) {
    parentChanged=parentChanged||(changes[i].object==child&&changes[i].kind==0);
    oldChildren=oldChildren||(changes[i].object==id&&changes[i].kind==1);
    newChildren=newChildren||(changes[i].object==other&&changes[i].kind==1);
  }
  AE_EXPECT_TRUE(parentChanged&&oldChildren&&newChildren,"ParentChanged for the child; ChildrenChanged for both parents");
  AE_EXPECT_EQ(hierarchy->pollChanges(abi.context,changes,16),0,"changes are delivered once");
  play.stop();
  AE_EXPECT_TRUE(view->state(abi.context,&state)==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::NotRunning),"retained table refuses after Stop");
  AE_EXPECT_EQ(play.debugLines().lines().size(),usize{0},"Stop discards debug lines");
  ServicesRuntime::onStart={};
}

AE_TEST(game_services_haptics_family_exists_only_with_a_platform_vibrator) {
  EditorDocument doc;scripted(doc,"Sonda");
  static u32 calls=0,lastMilliseconds=0;calls=0;
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(ServicesRuntime::api(),"/project");
  play.setHaptics([](u32 milliseconds,float){++calls;lastMilliseconds=milliseconds;return true;});
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  auto &abi=ServicesRuntime::access;
  const auto *haptics=ServicesRuntime::family<scene::ScriptHapticsOperations>(scene::kScriptHaptics);
  AE_EXPECT_TRUE(haptics!=nullptr,"vibrator registered: family published");
  AE_EXPECT_TRUE(haptics&&haptics->vibrate(abi.context,40,.5f)==1&&calls==1&&lastMilliseconds==40,"reaches the platform vibrator");
  AE_EXPECT_TRUE(haptics&&haptics->vibrate(abi.context,0,.5f)==0&&haptics->vibrate(abi.context,40,2)==0&&calls==1,"domain checked before the platform");
  play.stop();
}
