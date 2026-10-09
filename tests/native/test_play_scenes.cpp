#include "script_publication_fixture.h"
#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_session.h"
#include "scene/script_behavior.h"
#include "scene/script_extensions.h"
#include "scene/timer.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <string_view>

using namespace ae;
using namespace ae::editor;

// Bloco C2: cenas do projeto em Play. Aditiva pelo GameWorld e pela família
// astra.scenes; única pela sessão real, trocando o mundo no fim do quadro.
namespace {
struct SceneRuntime {
  static scene::ScriptSceneAccess access;
  static std::function<void()> onUpdate;
  static scene::ScriptRuntimeApi api() {
    scene::ScriptRuntimeApi a{};
    a.start=[](const u8 *,int,const u8 *,int,const scene::ScriptSceneAccess *s){access=*s;return s->available()?0:1;};
    a.update=[](float){if(onUpdate)onUpdate();return 0;};a.fixedUpdate=[](float){return 0;};a.lateUpdate=a.fixedUpdate;a.stop=[]{};
    a.copyDiagnostics=[](u8*,int){return 0;};a.trigger=[](u64,u64,u32){return 0;};
    a.contact=[](u64,u64,u32,const float*){return 0;};a.timer=[](u64,u64,u32){return 0;};a.lifecycle=[](u32,u32){return 0;};
    return a;
  }
  static const scene::ScriptSceneOperations *scenes() {
    const std::string_view name=scene::kScriptScenes;u32 version=0,size=0;
    return static_cast<const scene::ScriptSceneOperations*>(access.extension(access.context,reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()),&version,&size));
  }
  static int request(std::string_view name) {
    return scenes()->requestSingle(access.context,reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()));
  }
};
scene::ScriptSceneAccess SceneRuntime::access{};
std::function<void()> SceneRuntime::onUpdate{};

void script(EditorDocument &doc,EditorEntityId id) {
  auto values=*doc.find(id);
  auto *behavior=static_cast<scene::ScriptBehavior*>(values.components.add(scene::ScriptBehavior::descriptor));
  behavior->scriptType="project.Probe";behavior->source="Probe.cs";doc.applyEntityValues(id,values);
}
// Cena com dois objetos de topo, o segundo apontando para o primeiro.
EditorDocument levelScene(const char *first,const char *second) {
  EditorDocument doc;const auto a=doc.createEntity(doc.root(),EditorEntityKind::Folder,first);
  const auto b=doc.createEntity(doc.root(),EditorEntityKind::Folder,second);auto values=*doc.find(b);
  auto *timer=static_cast<scene::Timer*>(values.components.add(scene::Timer::descriptor));timer->elapsedAction=1;timer->elapsedTarget=a;
  doc.applyEntityValues(b,values);script(doc,a);return doc;
}
std::string sceneName(const scene::ScriptSceneOperations *ops) {
  char buffer[128]{};const int length=ops->active(SceneRuntime::access.context,reinterpret_cast<u8*>(buffer),sizeof(buffer));
  return length>0?std::string(buffer,static_cast<usize>(length)):std::string();
}
} // namespace

AE_TEST(play_scenes_additive_instantiation_remaps_cross_references_under_a_container) {
  const auto source=levelScene("Porta","Relógio");
  EditorDocument current;const auto existing=current.createEntity(current.root(),EditorEntityKind::Folder,"Mundo");
  runtime::GameWorld world;AE_EXPECT_TRUE(world.load(current),"world");
  runtime::ObjectCloneMap mapping;runtime::WorldStatus status;
  const auto container=world.instantiateScene(source,world.handle(world.graph().root()),"Nivel2",mapping,status);
  AE_EXPECT_TRUE(status==runtime::WorldStatus::Ok&&container.valid(),"scene instantiated");
  const auto *root=world.graph().find(container.id);
  AE_EXPECT_TRUE(root&&std::string_view(root->name)=="Nivel2"&&world.graph().childrenOf(container.id).size()==2,"container named after the scene holds both top-level objects");
  const auto children=world.graph().childrenOf(container.id);
  const auto *clock=world.graph().find(children[1]);
  const auto *timer=clock?static_cast<const scene::Timer*>(clock->components.find(scene::Timer::descriptor)):nullptr;
  AE_EXPECT_TRUE(timer&&timer->elapsedTarget==children[0]&&children[0]!=1u,"reference between scene objects remapped to the copy");
  AE_EXPECT_TRUE(world.graph().exists(existing),"current world kept");
  runtime::ObjectCloneMap again;
  AE_EXPECT_TRUE(!world.instantiateScene(source,world.handle(world.graph().root()),"",again,status).valid()&&status==runtime::WorldStatus::InvalidArgument,"empty name refused");
}

AE_TEST(play_scenes_abi_lists_loads_additively_and_refuses_unknown_or_concurrent_requests) {
  EditorDocument doc;const auto probe=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Sonda");script(doc,probe);
  const auto level=levelScene("Porta","Relógio");
  EditorMapScene resources;EditorPlayScene play;play.setScriptRuntime(SceneRuntime::api(),"/project");
  play.setSceneSource([]{return std::vector<std::string>{"Fases/Nivel2","Menu"};},
    [&](std::string_view request,runtime::SceneGraph &out,std::string &name,std::string &error) {
      if(request!="Nivel2"&&request!="Fases/Nivel2") {error="ausente";return false;}
      out=level;name="Nivel2";return true;
    });
  play.setActiveScene("Inicio");
  AE_EXPECT_TRUE(play.start(doc,resources),"Play");
  const auto *ops=SceneRuntime::scenes();auto &abi=SceneRuntime::access;
  AE_EXPECT_TRUE(ops&&ops->count(abi.context)==2&&sceneName(ops)=="Inicio","catalog and active scene");
  const std::string_view name="Nivel2";
  const auto root=ops->loadAdditive(abi.context,play.world().graph().root(),reinterpret_cast<const u8*>(name.data()),static_cast<int>(name.size()));
  AE_EXPECT_TRUE(root!=0&&abi.finishInstantiation(abi.context,root,1)==1,"additive scene published like a prefab");
  AE_EXPECT_TRUE(play.world().graph().childrenOf(static_cast<runtime::ObjectId>(root)).size()==2,"scene objects in the live world");
  AE_EXPECT_TRUE(SceneRuntime::request("Inexistente")==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::UnknownResource),"unknown scene refused in the same call");
  AE_EXPECT_TRUE(SceneRuntime::request("Nivel2")==1,"single load accepted");
  AE_EXPECT_TRUE(SceneRuntime::request("Nivel2")==0&&abi.lastStatus(abi.context)==static_cast<u32>(runtime::WorldStatus::LimitReached),"second request in the same frame refused");
  runtime::SceneGraph next;std::string nextName;
  AE_EXPECT_TRUE(play.takeSceneRequest(next,nextName)&&nextName=="Nivel2"&&next.entityCount()==level.entityCount(),"request handed over once");
  AE_EXPECT_TRUE(!play.takeSceneRequest(next,nextName),"no duplicate hand-over");
  play.stop();
}

AE_TEST(play_scenes_session_resolves_project_scenes_and_swaps_the_world_at_frame_end) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("astra-play-scenes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{root};
  fs::create_directories(root/"Fases");fs::create_directories(root/"Extra");
  EditorSession session;AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"project");
  const auto level=levelScene("Porta","Relógio");
  AE_EXPECT_TRUE(saveEditorDocument((root/"Fases"/"Nivel2.aescene").string().c_str(),level,0),"level saved");
  AE_EXPECT_TRUE(saveEditorDocument((root/"Fases"/"Menu.aescene").string().c_str(),level,0)&&
                 saveEditorDocument((root/"Extra"/"Menu.aescene").string().c_str(),level,0),"two scenes with the same file name");
  const auto scenes=session.projectScenes();
  AE_EXPECT_TRUE(scenes.size()==3&&scenes[0]=="Extra/Menu"&&scenes[2]=="Fases/Nivel2","catalog lists relative paths in stable order");
  runtime::SceneGraph graph;std::string name,error;
  AE_EXPECT_TRUE(session.loadPlayScene("Nivel2",graph,name,error)&&name=="Nivel2"&&graph.entityCount()==level.entityCount(),"unique file name resolves");
  AE_EXPECT_TRUE(!session.loadPlayScene("Menu",graph,name,error)&&error.find("ambíguo")!=std::string::npos,"ambiguous file name refused");
  AE_EXPECT_TRUE(session.loadPlayScene("Extra/Menu",graph,name,error),"path disambiguates");
  AE_EXPECT_TRUE(!session.loadPlayScene("Nada",graph,name,error),"missing scene refused");

  auto &doc=session.document();const auto probe=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Início");script(doc,probe);
  const auto authored=serializeEditorDocument(doc,0);
  static bool requested;requested=false;
  SceneRuntime::onUpdate=[]{if(!requested) requested=SceneRuntime::request("Nivel2")==1;};
  session.setScriptRuntime(SceneRuntime::api());
  AE_EXPECT_TRUE(publishCompiledScript(session,"project.Probe","Probe.cs"),"scripts publicados pelo hospedeiro");
  AE_EXPECT_TRUE(session.startPlay(),"Play");session.update();
  std::vector<renderer::MapDrawState> draws;session.advanceClock(1);
  AE_EXPECT_TRUE(session.extractPlayMap(draws),"first frame requests the swap");
  session.advanceClock(1.05);AE_EXPECT_TRUE(session.extractPlayMap(draws),"next frame runs the new world");
  const auto &world=session.playScene().world().graph();
  bool hasDoor=false,hasProbe=false;std::vector<runtime::ObjectId> ids;world.collectSubtree(world.root(),ids);
  for(const auto id:ids){const auto *o=world.find(id);hasDoor=hasDoor||std::string_view(o->name)=="Porta";hasProbe=hasProbe||std::string_view(o->name)=="Início";}
  AE_EXPECT_TRUE(requested&&hasDoor&&!hasProbe,"single load replaced the whole world");
  AE_EXPECT_TRUE(SceneRuntime::scenes()&&sceneName(SceneRuntime::scenes())=="Nivel2","new session reports the new active scene");
  AE_EXPECT_EQ(serializeEditorDocument(session.document(),0),authored,"authoring document untouched by Play scene loads");
  SceneRuntime::onUpdate={};
}
