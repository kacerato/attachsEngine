#pragma once
#include "editor/editor_map_scene.h"
#include "renderer/primitive_geometry.h"
#include "renderer/water_authoring_geometry.h"
#include "runtime/game_world.h"
#include "runtime/scene_physics.h"
#include "runtime/input_actions.h"
#include "runtime/script_bridge.h"
#include "runtime/scene_gui.h"
#include "runtime/scene_animation.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_physics_connections.h"
#include "runtime/component_operations.h"
#include "runtime/scene_event_connections.h"
#include "scene/collider.h"
#include "scene/physics2d_components.h"
#include "scene/timer.h"
#include "runtime/scene_camera_follow.h"
#include "runtime/scene_constraints.h"
#include "runtime/scene_tweens.h"
#include "runtime/scene_tween_sequences.h"
#include "runtime/scene_physics_queries.h"
#include "runtime/scene_virtual_cameras.h"
#include "runtime/scene_paths.h"
#include "runtime/scene_physics2d.h"
#include "runtime/scene_audio.h"
#include "scene/script_behavior.h"

#include <span>

namespace ae::editor {
// Adaptador do editor para o mundo de execução.
//
// **O Play não roda mais sobre uma cópia de `EditorDocument`.** O editor entrega
// a cena autorada ao `runtime::GameWorld`, que passa a ser o dono de tudo que
// executa: identidade com geração, componentes, fila de comandos e relógio. A
// física e os scripts falam com o mundo, não com o documento — é por isso que o
// mesmo par (`ScenePhysics` + `ScriptBridge`) pode ser ligado a um consumidor
// sem editor. A geometria dos recursos continua imutável no `EditorMapScene`, e
// nem pose de renderização nem mutação de script escrevem no documento autoral.
//
// Uma thread só, sem dependência de plataforma.
class EditorPlayScene final {
  // A forma do colisor Malha sai da MESMA geometria que a seleção e o ajuste de
  // colisor usam, no referencial do objeto em que o renderer a desenha.
  class CollisionGeometry final : public runtime::CollisionGeometrySource {
  public:
    explicit CollisionGeometry(const EditorMapScene &resources) : resources_(resources) {}
    bool meshTriangles(u32 mesh,std::vector<float> &out) const override {
      std::span<const EditorPickMesh::Triangle> triangles;float m[16];
      if(!resources_.localGeometry(mesh,triangles,m)) return false;
      out.reserve(out.size()+triangles.size()*9);
      for(const auto &t:triangles) for(u32 v=0;v<3;++v) for(u32 k=0;k<3;++k)
        out.push_back(m[12+k]+m[k]*t[v*3]+m[4+k]*t[v*3+1]+m[8+k]*t[v*3+2]);
      return true;
    }
    bool meshTriangles(const resources::AssetGuid &asset,std::vector<float> &out) const override {
      const auto mesh=resources_.assetSlot(asset);
      return mesh && meshTriangles(mesh,out);
    }
  private:
    const EditorMapScene &resources_;
  };
public:
  ~EditorPlayScene(){stop();}
  // Authoring preflight uses the same geometry adapter and solver as Play,
  // without executing scripts or altering the authored document.
  static bool validatePhysics(const EditorDocument &document,const EditorMapScene &resources,std::string &error) {
    runtime::GameWorld world;runtime::ScenePhysics physics;CollisionGeometry geometry(resources);
    if(!world.load(document)){error="Não foi possível validar a cena candidata";return false;}
    if(!physics.start(world,&geometry)){error=physics.error();return false;}
    error.clear();return true;
  }
  static bool previewPhysics(const EditorDocument &document,const EditorMapScene &resources,
      runtime::GameWorld &world,runtime::ScenePhysics &physics,std::string &error) {
    physics.stop();world.clear();CollisionGeometry geometry(resources);
    if(!world.load(document)){error="Cena autoral inválida";return false;}
    if(!physics.start(world,&geometry)){error=physics.error();return false;}
    error.clear();return true;
  }
  void setScriptRuntime(scene::ScriptRuntimeApi api,const std::string &root) {scripts_.configure(api,root);}
  void configureAudio(runtime::SceneAudio::ClipLoader loader,runtime::SceneAudio::Output output=runtime::SceneAudio::Output::Device){audio_.configure(std::move(loader),output);}
  void setPrefabLoader(runtime::ScriptBridge::PrefabLoader loader) {scripts_.setPrefabLoader(std::move(loader));}
  void setScriptResourceAvailability(runtime::ScriptBridge::ResourceAvailability available) {
    scripts_.setResourceAvailability(std::move(available));
  }
  void configureScriptRendering(renderer::ProjectRenderingSettings authored,
                                renderer::RenderingCapabilities capabilities,
                                renderer::ThermalPressure thermal,
                                const renderer::ResolvedRenderingPolicy &effective,
                                runtime::RuntimeRenderingState::RequestSink sink,
                                const resources::AssetRegistry *assets,
                                const std::vector<resources::EnvironmentProfile> *environmentProfiles) {
    scripts_.configureRendering(std::move(authored),std::move(capabilities),thermal,effective,std::move(sink));
    scripts_.setAssetLibrary(assets,environmentProfiles);
  }
  void setScriptRenderingExecution(renderer::UpscalingFilter executed,renderer::TemporalUpscalerAvailability status) {
    scripts_.setRenderingExecution(executed,status);
  }
  void setScriptTextureStreaming(const renderer::TextureStreamingStats &stats) {scripts_.setTextureStreaming(stats);}
  void setScriptSceneStatistics(const renderer::SceneStatistics &stats) {scripts_.setSceneStatistics(stats);}
  bool completeScriptRenderingRequest(u64 requestId,bool success,
                                      const renderer::ResolvedRenderingPolicy &effective,
                                      bool effectiveAvailable=true) {
    return scripts_.completeRenderingRequest(requestId,success,effective,effectiveAvailable);
  }
  void setScriptLogSink(runtime::ScriptBridge::LogSink sink) {scripts_.setLogSink(std::move(sink));}
  const std::string &scriptDiagnostics() const {return scripts_.diagnostics();}
  const std::string &physicsError() const {return physics2D_.error().empty()?physics_.error():physics2D_.error();}
  const std::string &frameError() const {return frameError_;}
  bool active() const noexcept { return active_; }
  // O grafo EM EXECUÇÃO. Tem o nome antigo porque os consumidores de leitura do
  // editor (câmera de cena, extração de desenho) aceitam qualquer `SceneGraph`.
  const runtime::SceneGraph &document() const noexcept { return world_.graph(); }
  runtime::GameWorld &world() noexcept { return world_; }
  const runtime::GameWorld &world() const noexcept { return world_; }
  runtime::SceneGraph *executionGraph() noexcept { return active_ ? &world_.poseGraph() : nullptr; }
  // O serviço de entrada vive com o mundo: é ele que os scripts consultam, e é
  // por ele que personagem e câmera recebem o que o projeto configurou.
  runtime::InputService &input() noexcept { return input_; }
  const runtime::InputService &input() const noexcept { return input_; }
  void submitInput(const runtime::InputDeviceState &state,double unscaledElapsed=0) { if(active_)sceneGui_.submitInput(world_,input_,state,unscaledElapsed); }
  // Foco: quando a interface consome o toque, o gameplay lê zero e nenhum botão
  // fica preso — a pausa e o cancelamento usam o mesmo caminho.
  void setInputFocus(bool focused) { input_.setGameplayFocus(focused&&!applicationPaused_&&applicationFocused_); }
  static EditorEntityId unresolvedEntity(const runtime::SceneGraph &source) {
    std::vector<EditorEntityId> ids;source.collectSubtree(source.root(),ids);
    for(auto id:ids) if(source.find(id)->components.hasUnresolved()) return id;
    return kInvalidEntity;
  }
  bool start(const runtime::SceneGraph &source, const EditorMapScene &resources) {
    if(active_ || unresolvedEntity(source)!=kInvalidEntity) return false;
    if(!world_.load(source)) return false;
    std::vector<renderer::MapDrawState> validated;
    if(!resources.extract(world_.graph(),validated)) {world_.clear();return false;}
    const CollisionGeometry geometry(resources);
    if(!physics_.start(world_,&geometry)) {world_.clear();return false;}
    if(!physics2D_.start(world_)){physics_.stop();world_.clear();return false;}
    input_.setMap(world_.graph().inputActions());
    input_.reset();
    input_.setGameplayFocus(true);
    // O avaliador existe antes dos scripts: o Start de um comportamento já
    // pode tocar ou misturar clipes.
    animator_.begin(world_.poseGraph(),resources);
    timers_.reset();physicsConnections_.reset();
    cameraFollow_.reset();
    constraints_.reset();
    tweens_.reset();numberTweens_.reset();sequences_.reset();queries_.reset();virtualCameras_.reset();
    paths_.reset();
    events_.reset();
    eventConnections_.reset();
    events_.attach(runtime::ComponentEventQueue::Consumer::Connections,true);
    tweens_.setEvents(&events_);
    sequences_.setEvents(&events_);
    virtualCameras_.setEvents(&events_);
    scripts_.setEvents(&events_);
    debugLines_.reset();
    scripts_.setGameView(&gameView_);
    scripts_.setDebugLines(&debugLines_);
    scripts_.setAnimator(&animator_);
    scripts_.setPhysics2D(&physics2D_);
    scripts_.setPaths(&paths_);
    scripts_.setTimers(&timers_);
    scripts_.setTweens(&tweens_);
    scripts_.setTweenSequences(&sequences_);
    scripts_.setPhysicsQueries(&queries_);
    scripts_.setVirtualCameras(&virtualCameras_);
    scripts_.setNumberTweens(&numberTweens_);
    // Audio outlives scripts; early Awake queries await first reconciliation.
    scripts_.setAudio(&audio_);
    sceneGui_.reconcile(world_);
    scripts_.setGui(&gui_);
    scripts_.setSceneGui(&sceneGui_);
    // Audio outlives scripts; early Awake queries have no diagnostic until reconciliation.
    std::array<runtime::PrimitiveResource,6> primitives{};
    for(u32 i=0;i<resources.assetCount();++i) {
      const auto flags=resources.materialFlagsForAsset(i);
      const auto type=renderer::primitiveFromFlags(flags);
      if(scene::validPrimitive(type) && !(flags&renderer::WaterAuthoringResource))
        primitives[static_cast<u32>(type)]={i+1,resources.assetGuid(i),resources.materialForAsset(i)};
    }
    scripts_.setPrimitiveLibrary(primitives);
    if(!scripts_.start(world_,physics_,input_)) {animator_.reset();physics2D_.stop(&world_);physics_.stop();world_.clear();return false;}
    // Awake/Start executam DEPOIS da montagem inicial e podem desligar formas,
    // alterar corpos ou destruir objetos. Publique isso antes do primeiro Update.
    resources_=&resources;
    if(!drainCommands() || !reconcilePhysics()) {stop();return false;}
    audio_.pause(applicationPaused_||!applicationFocused_||!audioFocused_);
    if(!audio_.start(world_)){stop();return false;}
    active_=true;
    paused_=false;
    return true;
  }
  bool extract(const EditorMapScene &resources, std::vector<renderer::MapDrawState> &draws) const {
    return active_ && resources.extract(world_.graph(),draws);
  }
  void stop() {
    // Ordem definida: scripts primeiro (podem enfileirar comandos no Stop),
    // depois o ponto seguro, depois a física, e só então o mundo — soltar o
    // mundo antes deixaria os dois lados com handles de objetos inexistentes.
    scripts_.stop();
    drainCommands();
    audio_.stop();
    physics2D_.stop(&world_);
    physics_.stop();
    world_.clear();
    input_.cancelBindingCapture();
    input_.reset();
    scripts_.setAnimator(nullptr);
    scripts_.setPaths(nullptr);
    scripts_.setTimers(nullptr);
    scripts_.setTweens(nullptr);
    scripts_.setTweenSequences(nullptr);
    scripts_.setPhysicsQueries(nullptr);
    scripts_.setNumberTweens(nullptr);
    scripts_.setAudio(nullptr);
    scripts_.setGui(nullptr);
    scripts_.setEvents(nullptr);
    scripts_.setGameView(nullptr);
    scripts_.setDebugLines(nullptr);
    debugLines_.reset();
    tweens_.setEvents(nullptr);
    sequences_.setEvents(nullptr);
    virtualCameras_.setEvents(nullptr);
    scripts_.setVirtualCameras(nullptr);
    events_.reset();
    eventConnections_.reset();
    scripts_.setSceneGui(nullptr);
    gui_.load(ui::GuiDocument{});
    sceneGui_.reset();
    paths_.reset();
    animator_.reset();
    timers_.reset();physicsConnections_.reset();
    cameraFollow_.reset();
    constraints_.reset();
    tweens_.reset();numberTweens_.reset();sequences_.reset();queries_.reset();virtualCameras_.reset();
    resources_=nullptr;
    active_=false;
    paused_=false;
  }
  // Edição do Inspector com o Play rodando: aplica a fila estrutural e
  // reconstrói o que as mudanças invalidaram, mesmo com o Play pausado.
  bool commitEdits() {return active_ && drainCommands() && reconcilePhysics() && audio_.advance(world_,0);}
  bool inspectFields(runtime::ObjectId id,std::vector<runtime::ScriptFieldIssue> &issues) {
    return active_ && scripts_.inspectFields(id,issues);
  }
  // Campo ou estado de um comportamento vivo; o grafo do mundo passa a mostrar
  // o valor novo só depois que a instância C# o aceitou.
  bool editBehavior(runtime::ObjectId id,const scene::ScriptBehavior &after,
                    std::span<const scene::ScriptPropertyValue> changed) {
    if(!active_ || !world_.alive(world_.handle(id))) return false;
    if(!scripts_.editBehavior(id,after.instanceId(),after.enabled,changed)) return false;
    auto *components=world_.poseGraph().editComponents(id);
    return components && components->replaceInstance(after.instanceId(),after);
  }
  void pause(bool value) {if(active_){paused_=value;if(value){input_.setGameplayFocus(false);gui_.cancelPointers();sceneGui_.cancelPointers();}updateAudioPause();}}
  void setAudioFocus(bool focused){audioFocused_=focused;updateAudioPause();}
  bool audioWantsFocus()const{return active_&&!paused_&&!applicationPaused_&&applicationFocused_&&audio_.wantsDevice();}
  bool step() {return active_ && paused_ && advanceFrame(1.0/60.0,true);}
  bool setCharacterMove(EditorEntityId id,float right,float forward,float yaw) {
    return active_ && physics_.setCharacterMove(id,right,forward,yaw);
  }
  bool jumpCharacter(EditorEntityId id) {return active_&&!paused_&&physics_.jumpCharacter(id,&world_);}
  bool setDynamicMotorMove(EditorEntityId id,float right,float forward,float yaw) {return active_&&physics_.setDynamicMotorMove(id,right,forward,yaw);}
  bool jumpDynamicMotor(EditorEntityId id) {return active_&&!paused_&&physics_.jumpDynamicMotor(id);}
  bool advance(double elapsed) {
    if(!active_) return false;
    if(paused_) return true;
    return advanceFrame(elapsed);
  }
  // Pausa/foco do aplicativo; vale também com o Play pausado pelo editor.
  bool applicationEvent(scene::ScriptLifecycleEvent event,bool value) {
    if((event==scene::ScriptLifecycleEvent::ApplicationPause&&value)||(event==scene::ScriptLifecycleEvent::ApplicationFocus&&!value)){gui_.cancelPointers();sceneGui_.cancelPointers();}
    if(event==scene::ScriptLifecycleEvent::ApplicationPause)applicationPaused_=value;
    else applicationFocused_=value;
    input_.setGameplayFocus(!applicationPaused_&&applicationFocused_&&!paused_);
    updateAudioPause();
    return !active_ || (scripts_.lifecycle(event,value) && drainCommands());
  }
  const runtime::SceneAnimator &animator() const noexcept {return animator_;}
  runtime::WorldStatus timerCommand(runtime::ObjectId object,u64 instance,u32 operation,float seconds,runtime::SceneTimers::State &state) {
    return timers_.command(world_,{world_.handle(object),instance},operation,seconds,state);
  }
  const runtime::SceneTimers &timers() const noexcept {return timers_;}
  // Mesma porta dos scripts: o método declarado no tipo, com a função efetiva.
  runtime::WorldStatus invokeMethod(runtime::ObjectId object,u64 instance,std::string_view method,
                                    std::span<const scene::ComponentOperationValue> arguments,scene::ComponentOperationValue &result) {
    if(!active_) return runtime::WorldStatus::NotRunning;
    return runtime::invokeComponentMethod(operationServices(),{world_.handle(object),instance},method,arguments,result);
  }
  runtime::ComponentOperationServices operationServices() noexcept {return {&world_,&timers_,&tweens_,&audio_,&paths_,&sequences_,&queries_,&physics_,&virtualCameras_};}
  runtime::ComponentEventQueue &events() noexcept {return events_;}
  const runtime::SceneEventConnections &eventConnections() const noexcept {return eventConnections_;}
  // Quem desenha o quadro publica a vista de jogo antes de avançar o mundo.
  void setGameView(const runtime::GameView &view) noexcept {gameView_=view;}
  const runtime::GameView &gameView() const noexcept {return gameView_;}
  const runtime::DebugLines &debugLines() const noexcept {return debugLines_;}
  void setHaptics(runtime::ScriptBridge::Haptics haptics) {scripts_.setHaptics(std::move(haptics));}
  void setSceneSource(runtime::ScriptBridge::SceneCatalog catalog,runtime::ScriptBridge::SceneLoader loader) {
    scripts_.setSceneSource(std::move(catalog),std::move(loader));
  }
  void setActiveScene(std::string name) {scripts_.setActiveScene(std::move(name));}
  const std::string &activeScene() const noexcept {return scripts_.activeScene();}
  bool takeSceneRequest(runtime::SceneGraph &graph,std::string &name) {return scripts_.takeSceneRequest(graph,name);}
  const runtime::ComponentEventQueue &events() const noexcept {return events_;}
  const runtime::ScenePhysicsConnections &physicsConnections() const noexcept {return physicsConnections_;}
  const runtime::SceneConstraints &constraints() const noexcept {return constraints_;}
  const runtime::SceneTweens &tweens() const noexcept {return tweens_;}
  runtime::SceneTweens &tweens() noexcept {return tweens_;}
  const runtime::SceneTweenSequences &tweenSequences() const noexcept {return sequences_;}
  const runtime::ScenePhysicsQueries &physicsQueries() const noexcept {return queries_;}
  const runtime::SceneVirtualCameras &virtualCameras() const noexcept {return virtualCameras_;}
  const runtime::ScenePaths &paths() const noexcept {return paths_;}
  runtime::ScenePaths &paths() noexcept {return paths_;}
  runtime::SceneAudio &audio() noexcept {return audio_;}
  void configureGui(const ui::GuiDocument &document) { if(!active_) gui_.load(document); }
  ui::GuiRuntime &gui() noexcept { return gui_; }
  runtime::SceneGui &sceneGui() noexcept {return sceneGui_;}
  void configureSceneGui(runtime::SceneGui::Loader loader) {if(!active_)sceneGui_.configure(std::move(loader));}
  const runtime::SceneAudio &audio() const noexcept {return audio_;}
  const runtime::ScenePhysics &physics() const noexcept {return physics_;}
  runtime::ScenePhysics2D &physics2D() noexcept {return physics2D_;}
  u32 pendingCommandCount() const noexcept {return world_.pendingCommandCount();}
private:
  void updateAudioPause(){audio_.pause(paused_||applicationPaused_||!applicationFocused_||!audioFocused_);}
  // Quadro completo, tanto contínuo quanto solicitado com o Play pausado.
  // Update → timers → animação → física (FixedUpdate e contatos) → LateUpdate
  // → constraints em mundo → CameraFollow, que lê a pose final. A física é remontada nos mesmos pontos
  // seguros; o solver conserva sua própria política de acumulação do tempo.
  // Referência de lifecycle: Unity 6.0 (6000.0), Event function execution order:
  // https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html
  bool advanceFrame(double elapsed,bool editorStep=false) {
    stayThisFrame_.clear();
    frameError_.clear();
    const auto stage=[&](bool ok,const char *name){
      if(ok)return true;
      frameError_=std::string("Play interrompido em ")+name;
      if(!scripts_.diagnostics().empty())frameError_+=" · "+scripts_.diagnostics();
      else if(!physicsError().empty())frameError_+=" · "+physicsError();
      return false;
    };
    if(!stage(world_.beginFrame(elapsed,editorStep),"relógio"))return false;
    const double frameElapsed=world_.clock().delta();
    debugLines_.advance(frameElapsed);
    const float scriptElapsed=world_.clock().delta();
    gui_.advance(frameElapsed);
    sceneGui_.advance(world_,frameElapsed);
    return stage(runScripts(scriptElapsed),"Update") && stage(advanceTimers(frameElapsed,std::min(elapsed,.25)),"timers") && stage(animate(scriptElapsed),"animação") &&
           stage(reconcilePhysics(),"reconstrução física") && stage(physics_.advance(frameElapsed,world_,fixedStep,this,triggerEvent,contactEvent,jointBrokenEvent,characterHitEvent),"física / FixedUpdate") && drainCommands() &&
           stage(scripts_.lateUpdate(scriptElapsed),"LateUpdate") && drainCommands() && stage(reconcilePhysics(),"reconstrução final da física") && stage(queries_.advance(world_,physics_),"consultas físicas") &&
           stage(paths_.advance(world_,frameElapsed),"paths") && stage(sequences_.advance(world_,tweens_,frameElapsed,std::min(elapsed,.25)),"sequências de tweens") && stage(tweens_.advance(world_,frameElapsed,std::min(elapsed,.25)),"tweens") && stage(numberTweens_.advance(world_,frameElapsed,std::min(elapsed,.25)),"propriedades animadas") && drainCommands() && stage(constraints_.advance(world_,frameElapsed),"constraints") && stage(cameraFollow_.advance(world_,frameElapsed),"câmera") && stage(advanceVirtualCameras(),"câmeras virtuais") && stage(audio_.advance(world_,std::min(elapsed,.25)),"áudio");
  }
  // Ponto seguro: aplica a fila e avisa a física de quem deixou de existir, para
  // que nenhum corpo do Jolt continue simulando um objeto removido.
  bool drainCommands() {
    // Conexões de evento reagem no ponto seguro, antes de a fila estrutural ser
    // aplicada: uma desativação pedida por elas entra no mesmo flush.
    eventConnections_.process(operationServices(),events_);
    destroyed_.clear();
    world_.flush(&destroyed_);
    for(const auto id:destroyed_) {physics_.releaseObject(id);physics2D_.releaseObject(id,&world_);}
    return true;
  }
  // A órbita lê a mesma ação Olhar do mapa de entrada que Olhar e scripts leem.
  bool advanceVirtualCameras() {
    float look[2]{0,0};input_.axis2(input_.map().lookAction(),look);
    return virtualCameras_.advance(world_,&physics_,look) && drainCommands();
  }
  bool runScripts(float elapsed) {return scripts_.update(elapsed) && drainCommands();}
  // Contrato das propriedades físicas (scene::Invalidate): corpo, forma ou a
  // malha que serve de forma mudaram, então o solver é remontado do grafo.
  bool reconcilePhysics() {
    constexpr u32 physical=scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape|scene::Invalidate::MeshDerived;
    if(!(world_.consumeInvalidation()&physical)) return true;
    if(!resources_) return false;
    const CollisionGeometry geometry(*resources_);
    return physics_.rebuild(world_,&geometry) && physics2D_.rebuild(world_);
  }
  bool advanceTimers(double elapsed,double unscaledElapsed) {
    return timers_.advance(world_,elapsed,unscaledElapsed,[this](runtime::ObjectId object,u64 instance,u32 count) {
      const auto fired=scene::ComponentOperationValue::makeInteger(count);
      events_.emit(world_,object,instance,scene::Timer::descriptor,"elapsed",std::span(&fired,1));
      return scripts_.timer(object,instance,count) && drainCommands();
    });
  }
  // Depois do Update dos scripts e antes da física (runtime/scene_animation.h).
  // Nó com pose publicada pela física não é escrito pela animação.
  bool animate(float elapsed) {
    if(!animator_.active()) return true;
    return animator_.advance(elapsed,[this](runtime::ObjectId id) {
      return world_.authorityOf(world_.handle(id))==runtime::TransformAuthority::Free;
    });
  }
  static bool triggerEvent(void *context,runtime::ObjectId sensor,runtime::ObjectId other,u32 phase) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    self.physicsConnections_.trigger(self.world_,sensor,other,phase);
    self.emitContact(scene::Collider::descriptor,sensor,0,other,phase,true);
    return self.scripts_.trigger(sensor,other,phase) && self.drainCommands();
  }
  static bool contactEvent(void *context,const runtime::ContactEvent &event) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    self.physicsConnections_.contact(self.world_,event.first,event.second,event.phase);
    self.emitContact(scene::Collider::descriptor,event.first,0,event.second,event.phase,false);
    self.emitContact(scene::Collider::descriptor,event.second,0,event.first,event.phase,false);
    return self.scripts_.contact(event) && self.drainCommands();
  }
  static bool characterHitEvent(void *context,const runtime::CharacterHit &hit) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    const scene::ComponentOperationValue values[3]{scene::ComponentOperationValue::makeObject(hit.other),
      scene::ComponentOperationValue::makeVector(hit.point[0],hit.point[1],hit.point[2]),
      scene::ComponentOperationValue::makeVector(hit.normal[0],hit.normal[1],hit.normal[2])};
    self.events_.emit(self.world_,hit.character,hit.instance,scene::Character::descriptor,"collider_hit",values);
    return true;
  }
  static bool jointBrokenEvent(void *context,runtime::ObjectId owner,u64 instance,float force) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    const auto value=scene::ComponentOperationValue::makeNumber(force);
    self.events_.emit(self.world_,owner,instance,scene::Joint::descriptor,"broken",std::span(&value,1));
    return true;
  }
  // Enter/Exit viram eventos de componente; a permanência no sensor também, uma
  // vez por quadro e par; a do contato sólido fica só no callback do script.
  void emitContact(const scene::ComponentType &type,runtime::ObjectId source,u64 instance,runtime::ObjectId other,u32 phase,bool sensor) {
    if(phase==1&&sensor) {
      for(const auto &pair:stayThisFrame_) if(pair.first==source&&pair.second==other) return;
      stayThisFrame_.emplace_back(source,other);
      const auto otherValue=scene::ComponentOperationValue::makeObject(other);
      events_.emit(world_,source,instance,type,"trigger_stay",std::span(&otherValue,1));
      return;
    }
    if(phase!=0 && phase!=2) return;
    const auto otherValue=scene::ComponentOperationValue::makeObject(other);
    const char *id=sensor?(phase==0?"trigger_enter":"trigger_exit"):(phase==0?"collision_enter":"collision_exit");
    events_.emit(world_,source,instance,type,id,std::span(&otherValue,1));
  }
  static bool fixedStep(void *context,float dt) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    self.sceneGui_.driveCharacters(self.world_,self.physics_);
    return self.scripts_.fixedUpdate(dt) && self.drainCommands() && self.reconcilePhysics() && self.physics2D_.advance(dt,self.world_,nullptr,&self,event2D);
  }
  static bool event2D(void *context,const runtime::Physics2DEvent &event){
    auto &self=*static_cast<EditorPlayScene*>(context);
    if(!self.world_.alive(self.world_.handle(event.first)))return true;
    self.physicsConnections_.event2D(self.world_,event.first,event.second,event.phase,event.sensor);
    self.emitContact(scene::Collider2D::descriptor,event.first,event.firstCollider,event.second,event.phase,event.sensor);
    if(!event.sensor)self.emitContact(scene::Collider2D::descriptor,event.second,event.secondCollider,event.first,event.phase,false);
    // A retired visitor can still produce a native exit for a surviving sensor.
    // Managed callbacks retain their existing live-other contract.
    if(!self.world_.alive(self.world_.handle(event.second)))return self.drainCommands();
    if(event.sensor)return self.scripts_.trigger(event.first,event.second,event.phase)&&self.drainCommands();
    runtime::ContactEvent contact{};contact.first=event.first;contact.second=event.second;contact.phase=event.phase;contact.hasNormal=event.hasNormal;contact.normal[0]=event.normal[0];contact.normal[1]=event.normal[1];
    return self.scripts_.contact(contact)&&self.drainCommands();
  }
  runtime::GameWorld world_;
  std::string frameError_;
  runtime::ScenePhysics physics_;
  runtime::ScenePhysics2D physics2D_;
  runtime::SceneAudio audio_;
  ui::GuiRuntime gui_;
  runtime::SceneGui sceneGui_;
  runtime::ScriptBridge scripts_;
  runtime::InputService input_;
  std::vector<runtime::ObjectId> destroyed_;
  runtime::SceneAnimator animator_;
  runtime::SceneTimers timers_;
  runtime::ScenePhysicsConnections physicsConnections_;
  runtime::SceneCameraFollow cameraFollow_;
  runtime::SceneConstraints constraints_;
  runtime::SceneTweens tweens_;
  runtime::SceneTweenSequences sequences_;
  runtime::ScenePhysicsQueries queries_;
  runtime::SceneVirtualCameras virtualCameras_;
  std::vector<std::pair<runtime::ObjectId,runtime::ObjectId>> stayThisFrame_;
  runtime::SceneNumberTweens numberTweens_;
  runtime::ScenePaths paths_;
  runtime::ComponentEventQueue events_;
  runtime::SceneEventConnections eventConnections_;
  runtime::GameView gameView_;
  runtime::DebugLines debugLines_;
  const EditorMapScene *resources_=nullptr;
  bool active_=false;
  bool paused_=false;
  bool applicationPaused_=false,applicationFocused_=true;
  bool audioFocused_=true;
};
}
