#pragma once
#include "editor/editor_map_scene.h"
#include "runtime/game_world.h"
#include "runtime/scene_physics.h"
#include "runtime/input_actions.h"
#include "runtime/script_bridge.h"
#include "runtime/scene_animation.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_camera_follow.h"
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
  void setScriptRuntime(scene::ScriptRuntimeApi api,const std::string &root) {scripts_.configure(api,root);}
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
  const std::string &physicsError() const {return physics_.error();}
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
  void submitInput(const runtime::InputDeviceState &state) { if(active_) input_.submit(state); }
  // Foco: quando a interface consome o toque, o gameplay lê zero e nenhum botão
  // fica preso — a pausa e o cancelamento usam o mesmo caminho.
  void setInputFocus(bool focused) { input_.setGameplayFocus(focused); }
  static EditorEntityId unresolvedEntity(const EditorDocument &source) {
    std::vector<EditorEntityId> ids;source.collectSubtree(source.root(),ids);
    for(auto id:ids) if(source.find(id)->components.hasUnresolved()) return id;
    return kInvalidEntity;
  }
  bool start(const EditorDocument &source, const EditorMapScene &resources) {
    if(active_ || unresolvedEntity(source)!=kInvalidEntity) return false;
    if(!world_.load(source)) return false;
    std::vector<renderer::MapDrawState> validated;
    if(!resources.extract(world_.graph(),validated)) {world_.clear();return false;}
    const CollisionGeometry geometry(resources);
    if(!physics_.start(world_,&geometry)) {world_.clear();return false;}
    input_.setMap(world_.graph().inputActions());
    input_.reset();
    input_.setGameplayFocus(true);
    // O avaliador existe antes dos scripts: o Start de um comportamento já
    // pode tocar ou misturar clipes.
    animator_.begin(world_.poseGraph(),resources);
    timers_.reset();
    cameraFollow_.reset();
    scripts_.setAnimator(&animator_);
    if(!scripts_.start(world_,physics_,input_)) {animator_.reset();physics_.stop();world_.clear();return false;}
    // Awake/Start executam DEPOIS da montagem inicial e podem desligar formas,
    // alterar corpos ou destruir objetos. Publique isso antes do primeiro Update.
    resources_=&resources;
    if(!drainCommands() || !reconcilePhysics()) {stop();return false;}
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
    physics_.stop();
    world_.clear();
    input_.reset();
    scripts_.setAnimator(nullptr);
    animator_.reset();
    timers_.reset();
    cameraFollow_.reset();
    resources_=nullptr;
    active_=false;
    paused_=false;
  }
  // Edição do Inspector com o Play rodando: aplica a fila estrutural e
  // reconstrói o que as mudanças invalidaram, mesmo com o Play pausado.
  bool commitEdits() {return active_ && drainCommands() && reconcilePhysics();}
  // Campo ou estado de um comportamento vivo; o grafo do mundo passa a mostrar
  // o valor novo só depois que a instância C# o aceitou.
  bool editBehavior(runtime::ObjectId id,const scene::ScriptBehavior &after,
                    std::span<const scene::ScriptPropertyValue> changed) {
    if(!active_ || !world_.alive(world_.handle(id))) return false;
    if(!scripts_.editBehavior(id,after.instanceId(),after.enabled,changed)) return false;
    auto *components=world_.poseGraph().editComponents(id);
    return components && components->replaceInstance(after.instanceId(),after);
  }
  void pause(bool value) {if(active_) paused_=value;}
  bool step() {if(!active_ || !paused_) return false; world_.advanceClock(1.0/60.0);return runScripts(1.0f/60.0f) && advanceTimers(1.0/60.0) && animate(1.0f/60.0f) && reconcilePhysics() && physics_.advance(1.0/60.0,world_,fixedStep,this,triggerEvent,contactEvent) && drainCommands() && reconcilePhysics() && cameraFollow_.advance(world_,1.0/60.0);}
  bool setCharacterMove(EditorEntityId id,float right,float forward,float yaw) {
    return active_ && physics_.setCharacterMove(id,right,forward,yaw);
  }
  bool jumpCharacter(EditorEntityId id) {return active_&&!paused_&&physics_.jumpCharacter(id);}
  bool advance(double elapsed) {
    if(!active_) return false;
    if(paused_) return true;
    world_.advanceClock(std::min(elapsed,.25));
    // Ordem do quadro: Update → timers → animação → física (FixedUpdate e
    // contatos) → LateUpdate → acompanhamento de câmera, que lê a pose final.
    // A física é remontada fora do passo dela: uma mudança pedida dentro de
    // FixedUpdate ou de um contato espera o próximo ponto seguro de quadro.
    return runScripts(static_cast<float>(std::min(elapsed,.25))) && advanceTimers(std::min(elapsed,.25)) && animate(static_cast<float>(std::min(elapsed,.25))) &&
           reconcilePhysics() && physics_.advance(elapsed,world_,fixedStep,this,triggerEvent,contactEvent) && drainCommands() &&
           scripts_.lateUpdate(static_cast<float>(std::min(elapsed,.25))) && drainCommands() && reconcilePhysics() &&
           cameraFollow_.advance(world_,std::min(elapsed,.25));
  }
  // Pausa/foco do aplicativo; vale também com o Play pausado pelo editor.
  bool applicationEvent(scene::ScriptLifecycleEvent event,bool value) {
    return !active_ || (scripts_.lifecycle(event,value) && drainCommands());
  }
  const runtime::SceneAnimator &animator() const noexcept {return animator_;}
  const runtime::SceneTimers &timers() const noexcept {return timers_;}
  u32 pendingCommandCount() const noexcept {return world_.pendingCommandCount();}
private:
  // Ponto seguro: aplica a fila e avisa a física de quem deixou de existir, para
  // que nenhum corpo do Jolt continue simulando um objeto removido.
  bool drainCommands() {
    destroyed_.clear();
    world_.flush(&destroyed_);
    for(const auto id:destroyed_) physics_.releaseObject(id);
    return true;
  }
  bool runScripts(float elapsed) {return scripts_.update(elapsed) && drainCommands();}
  // Contrato das propriedades físicas (scene::Invalidate): corpo, forma ou a
  // malha que serve de forma mudaram, então o solver é remontado do grafo.
  bool reconcilePhysics() {
    constexpr u32 physical=scene::Invalidate::PhysicsBody|scene::Invalidate::PhysicsShape|scene::Invalidate::MeshDerived;
    if(!(world_.consumeInvalidation()&physical)) return true;
    if(!resources_) return false;
    const CollisionGeometry geometry(*resources_);
    return physics_.rebuild(world_,&geometry);
  }
  bool advanceTimers(double elapsed) {
    return timers_.advance(world_,elapsed,[this](runtime::ObjectId object,u64 instance,u32 count) {
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
    return self.scripts_.trigger(sensor,other,phase) && self.drainCommands();
  }
  static bool contactEvent(void *context,const runtime::ContactEvent &event) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    return self.scripts_.contact(event) && self.drainCommands();
  }
  static bool fixedStep(void *context,float dt) {
    auto &self=*static_cast<EditorPlayScene *>(context);
    return self.scripts_.fixedUpdate(dt) && self.drainCommands();
  }
  runtime::GameWorld world_;
  runtime::ScenePhysics physics_;
  runtime::ScriptBridge scripts_;
  runtime::InputService input_;
  std::vector<runtime::ObjectId> destroyed_;
  runtime::SceneAnimator animator_;
  runtime::SceneTimers timers_;
  runtime::SceneCameraFollow cameraFollow_;
  const EditorMapScene *resources_=nullptr;
  bool active_=false;
  bool paused_=false;
};
}
