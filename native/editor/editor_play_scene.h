#pragma once
#include "editor/editor_map_scene.h"
#include "runtime/game_world.h"
#include "runtime/scene_physics.h"
#include "runtime/script_bridge.h"

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
public:
  ~EditorPlayScene(){stop();}
  void setScriptRuntime(scene::ScriptRuntimeApi api,const std::string &root) {scripts_.configure(api,root);}
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
    if(!physics_.start(world_)) {world_.clear();return false;}
    if(!scripts_.start(world_,physics_)) {physics_.stop();world_.clear();return false;}
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
    active_=false;
    paused_=false;
  }
  void pause(bool value) {if(active_) paused_=value;}
  bool step() {return active_ && paused_ && runScripts(1.0f/60.0f) && physics_.advance(1.0/60.0,world_,fixedStep,this,triggerEvent,contactEvent) && drainCommands();}
  bool setCharacterMove(EditorEntityId id,float right,float forward,float yaw) {
    return active_ && physics_.setCharacterMove(id,right,forward,yaw);
  }
  bool jumpCharacter(EditorEntityId id) {return active_&&!paused_&&physics_.jumpCharacter(id);}
  bool advance(double elapsed) {
    if(!active_) return false;
    if(paused_) return true;
    world_.advanceClock(std::min(elapsed,.25));
    return runScripts(static_cast<float>(std::min(elapsed,.25))) &&
           physics_.advance(elapsed,world_,fixedStep,this,triggerEvent,contactEvent) && drainCommands();
  }
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
  std::vector<runtime::ObjectId> destroyed_;
  bool active_=false;
  bool paused_=false;
};
}
