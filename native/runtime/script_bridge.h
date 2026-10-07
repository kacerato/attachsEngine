// A ponte entre o mundo de execução e o runtime C#.
//
// Era `editor/editor_script_bridge.h` e falava o formato do documento do editor.
// Agora fala `GameWorld`: é aqui que a ABI (scene/script_runtime.h) ganha
// corpo, e é o único lugar do nativo que sabe como um `Behavior` endereça
// objetos, componentes e propriedades.
//
// A plataforma injeta o serviço gerenciado; este arquivo não conhece CLR.
#pragma once
#include "ui/gui_document.h"
#include "runtime/scene_gui.h"
#include "runtime/script_inspection.h"
#include "runtime/game_world.h"
#include "runtime/prefab.h"
#include "runtime/input_actions.h"
#include "runtime/scene_physics.h"
#include "runtime/scene_physics2d.h"
#include "runtime/scene_animation.h"
#include "runtime/scene_paths.h"
#include "runtime/scene_audio.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_tweens.h"
#include "runtime/scene_number_tweens.h"
#include "runtime/runtime_rendering_state.h"
#include "scene/script_runtime.h"
#include "scene/script_extensions.h"
#include "runtime/component_operations.h"
#include "runtime/game_view.h"
#include "resources/asset_registry.h"
#include "resources/environment_profile.h"

#include <deque>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ae::runtime {

class ScriptBridge final {
public:
  using LogSink = std::function<void(u64, std::string_view)>;
  // O callback pode publicar sob demanda. O candidato já contém o novo GUID;
  // ele também pode reconciliar campos transitórios, como o índice de malha.
  using ResourceAvailability = ComponentResourceResolver;
  using PrefabLoader = std::function<bool(resources::AssetGuid,Prefab &,std::string &)>;
  void setPrefabLoader(PrefabLoader loader) {if(!running_) prefabLoader_=std::move(loader);}
  void setLogSink(LogSink sink) { logSink_ = std::move(sink); }
  void setPrimitiveLibrary(const std::array<PrimitiveResource,6> &library) {if(!running_) primitives_=library;}
  ~ScriptBridge() { stop(); }
  void configure(scene::ScriptRuntimeApi api, std::string root) {
    if (!running_) { api_ = api; root_ = std::move(root); }
  }
  void configureRendering(renderer::ProjectRenderingSettings authored,
                          renderer::RenderingCapabilities capabilities,
                          renderer::ThermalPressure thermal,
                          const renderer::ResolvedRenderingPolicy &effective,
                          RuntimeRenderingState::RequestSink sink) {
    if (running_) {
      rendering_.refresh(world_?world_->worldId():0,std::move(capabilities),thermal,effective);
      return;
    }
    rendering_.configure(std::move(authored), std::move(capabilities), thermal, std::move(sink));
    initialEffective_ = effective;
    renderingConfigured_ = true;
  }
  void setAssetLibrary(const resources::AssetRegistry *assets,
                       const std::vector<resources::EnvironmentProfile> *environmentProfiles) {
    if (!running_) { assets_ = assets; environmentProfiles_ = environmentProfiles; }
  }
  void setResourceAvailability(ResourceAvailability available) {
    if(!running_) resourceAvailable_=std::move(available);
  }
  // O que o renderer executou no último quadro (Graphics.State.ExecutedUpscaler).
  void setRenderingExecution(renderer::UpscalingFilter executed, renderer::TemporalUpscalerAvailability status) {
    rendering_.setExecution(executed, status);
  }
  // S2: Graphics.State.TextureStreaming.
  void setTextureStreaming(const renderer::TextureStreamingStats &stats) { rendering_.setTextureStreaming(stats); }
  // S5: Graphics.State.Frame.
  void setSceneStatistics(const renderer::SceneStatistics &stats) { rendering_.setSceneStatistics(stats); }
  bool completeRenderingRequest(u64 requestId, bool success,
                                const renderer::ResolvedRenderingPolicy &effective,
                                bool effectiveAvailable=true) {
    return rendering_.complete(requestId, success, effective, effectiveAvailable);
  }
  bool start(GameWorld &world, ScenePhysics &physics, InputService &input);
  // Avaliador de animação do Play; nulo recusa os comandos de animação com
  // NotRunning em vez de fingir que tocaram.
  void setAnimator(SceneAnimator *animator) noexcept { animator_ = animator; }
  void setPhysics2D(ScenePhysics2D *physics) noexcept { physics2D_ = physics; }
  void setAudio(SceneAudio *audio) noexcept { audio_ = audio; }
  void setGui(ui::GuiRuntime *gui) noexcept { gui_ = gui; }
  void setSceneGui(SceneGui *gui) noexcept {sceneGui_=gui;}
  void setNumberTweens(SceneNumberTweens *numberTweens) noexcept {numberTweens_=numberTweens;}
  void setTweens(SceneTweens *tweens) noexcept {tweens_=tweens;}
  void setTweenSequences(SceneTweenSequences *sequences) noexcept {sequences_=sequences;}
  void setPhysicsQueries(ScenePhysicsQueries *queries) noexcept {queries_=queries;}
  void setVirtualCameras(SceneVirtualCameras *cameras) noexcept {cameras_=cameras;}
  void setTimers(SceneTimers *timers) noexcept { timers_=timers; }
  void setPaths(ScenePaths *paths) noexcept { paths_ = paths; }
  // Fila de eventos de componente do Play; os scripts a leem pela família
  // `astra.component.operations` da ABI.
  // Vista de jogo do quadro corrente e linhas de depuração (famílias
  // astra.view e astra.debug). Nulos recusam as operações com NotRunning.
  void setGameView(const GameView *view) noexcept { gameView_=view; }
  void setDebugLines(DebugLines *lines) noexcept { debugLines_=lines; }
  // Vibrador da plataforma. Sem ele, a família astra.haptics não existe.
  using Haptics = std::function<bool(u32 milliseconds,float amplitude)>;
  void setHaptics(Haptics haptics) { if(!running_) haptics_=std::move(haptics); }
  // Cenas do projeto: quem conhece o sistema de arquivos entrega o catálogo e
  // o carregador. O carregador devolve o grafo pronto para o mundo (tags e
  // recursos reconciliados) e o nome canônico da cena.
  using SceneCatalog = std::function<std::vector<std::string>()>;
  using SceneLoader = std::function<bool(std::string_view request,SceneGraph &out,std::string &name,std::string &error)>;
  void setSceneSource(SceneCatalog catalog,SceneLoader loader) {if(!running_){sceneCatalog_=std::move(catalog);sceneLoader_=std::move(loader);}}
  void setActiveScene(std::string name) {activeScene_=std::move(name);}
  const std::string &activeScene() const noexcept {return activeScene_;}
  // Troca de cena pedida por script: entregue uma vez a quem conduz o Play.
  bool takeSceneRequest(SceneGraph &graph,std::string &name) {
    if(!sceneRequest_.pending) return false;
    graph=std::move(sceneRequest_.graph);name=std::move(sceneRequest_.name);sceneRequest_={};return true;
  }
  void setEvents(ComponentEventQueue *events) noexcept {
    if(events_ && events_!=events) events_->attach(ComponentEventQueue::Consumer::Scripts,false);
    events_=events;
    if(events_ && running_) events_->attach(ComponentEventQueue::Consumer::Scripts,true);
  }
  bool update(float elapsed);
  bool lateUpdate(float elapsed);
  bool lifecycle(scene::ScriptLifecycleEvent event, bool value);
  bool fixedUpdate(float elapsed);
  bool trigger(ObjectId sensor, ObjectId other, u32 phase);
  // Contato sólido: o mesmo evento chega aos DOIS objetos do par, cada um
  // recebendo o outro. A normal acompanha só Enter/Stay.
  bool contact(const ContactEvent &event);
  bool timer(ObjectId object,u64 instance,u32 count);
  // Inspector em Play (Unity: mudar um campo do script com o jogo rodando): o
  // estado e os campos em `changed` chegam à instância viva. Falso, com o motivo
  // em `diagnostics()`, quando o runtime não tem a função ou recusou o valor.
  bool editBehavior(ObjectId object,u64 instance,bool enabled,std::span<const scene::ScriptPropertyValue> changed);
  bool inspectFields(ObjectId object,std::vector<ScriptFieldIssue> &issues);
  void stop();
  const std::string &diagnostics() const { return diagnostics_; }
  static bool hasScripts(const SceneGraph &graph);
  // Descrição JSON dos comportamentos anexados, na ordem de pré-ordem da cena.
  static std::string attachments(const SceneGraph &graph,ObjectId root=0);

private:
  void collectDiagnostics();
  void installAccess();
  void installExtensions();
  ComponentOperationServices operationServices() const noexcept {return {world_,timers_,tweens_,audio_,paths_,sequences_,queries_,physics_,cameras_};}
  QueryFilter queryFilter(const scene::ScriptQueryFilter &filter) const;
  static QueryShapeDesc queryShape(const scene::ScriptShapeQuery &shape);
  static void copyHits(const std::vector<QueryHit> &hits, u32 total, scene::ScriptQueryHit *out, int capacity);
  scene::ScriptRuntimeApi api_{};
  scene::ScriptSceneAccess access_{};
  scene::ScriptComponentOperations componentOperations_{};
  scene::ScriptViewOperations viewOperations_{};
  scene::ScriptDebugOperations debugOperations_{};
  scene::ScriptHierarchyOperations hierarchyOperations_{};
  scene::ScriptHapticsOperations hapticsOperations_{};
  scene::ScriptMotorControlOperations motorControlOperations_{};
  scene::ScriptMotorMotionOperations motorMotionOperations_{};
  ComponentEventQueue *events_=nullptr;
  const GameView *gameView_=nullptr;
  DebugLines *debugLines_=nullptr;
  Haptics haptics_;
  std::deque<GameWorld::HierarchyChange> pendingHierarchy_;
  scene::ScriptSceneOperations sceneOperations_{};
  SceneCatalog sceneCatalog_;
  SceneLoader sceneLoader_;
  std::string activeScene_;
  struct SceneRequest {SceneGraph graph;std::string name;bool pending=false;} sceneRequest_;
  std::string root_, diagnostics_;
  LogSink logSink_;
  GameWorld *world_ = nullptr;
  ScenePhysics *physics_ = nullptr;
  ScenePhysics2D *physics2D_ = nullptr;
  ScenePaths *paths_ = nullptr;
  SceneAudio *audio_ = nullptr;
  ui::GuiRuntime *gui_ = nullptr;
  SceneGui *sceneGui_=nullptr;
  bool guiRequestActive_=false;
  SceneTimers *timers_=nullptr;
  SceneTweens *tweens_=nullptr;
  SceneTweenSequences *sequences_=nullptr;
  ScenePhysicsQueries *queries_=nullptr;
  SceneVirtualCameras *cameras_=nullptr;
  SceneNumberTweens *numberTweens_=nullptr;
  InputService *input_ = nullptr;
  SceneAnimator *animator_ = nullptr;
  const resources::AssetRegistry *assets_ = nullptr;
  const std::vector<resources::EnvironmentProfile> *environmentProfiles_ = nullptr;
  ResourceAvailability resourceAvailable_;
  PrefabLoader prefabLoader_;
  std::array<PrimitiveResource,6> primitives_{};
  RuntimeRenderingState rendering_{};
  renderer::ResolvedRenderingPolicy initialEffective_{};
  bool renderingConfigured_ = false;
  WorldStatus lastStatus_ = WorldStatus::Ok;
  bool running_ = false;
};

} // namespace ae::runtime
