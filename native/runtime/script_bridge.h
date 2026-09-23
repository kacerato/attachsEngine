// A ponte entre o mundo de execução e o runtime C#.
//
// Era `editor/editor_script_bridge.h` e falava o formato do documento do editor.
// Agora fala `GameWorld`: é aqui que a ABI (scene/script_runtime.h) ganha
// corpo, e é o único lugar do nativo que sabe como um `Behavior` endereça
// objetos, componentes e propriedades.
//
// A plataforma injeta o serviço gerenciado; este arquivo não conhece CLR.
#pragma once
#include "runtime/game_world.h"
#include "runtime/input_actions.h"
#include "runtime/scene_physics.h"
#include "runtime/runtime_rendering_state.h"
#include "scene/script_runtime.h"
#include "resources/asset_registry.h"
#include "resources/environment_profile.h"

#include <functional>
#include <string>
#include <vector>

namespace ae::runtime {

class ScriptBridge final {
public:
  using LogSink = std::function<void(u64, std::string_view)>;
  // O callback pode publicar sob demanda. O candidato já contém o novo GUID;
  // ele também pode reconciliar campos transitórios, como o índice de malha.
  using ResourceAvailability = ComponentResourceResolver;
  void setLogSink(LogSink sink) { logSink_ = std::move(sink); }
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
  bool completeRenderingRequest(u64 requestId, bool success,
                                const renderer::ResolvedRenderingPolicy &effective,
                                bool effectiveAvailable=true) {
    return rendering_.complete(requestId, success, effective, effectiveAvailable);
  }
  bool start(GameWorld &world, ScenePhysics &physics, InputService &input);
  bool update(float elapsed);
  bool fixedUpdate(float elapsed);
  bool trigger(ObjectId sensor, ObjectId other, u32 phase);
  // Contato sólido: o mesmo evento chega aos DOIS objetos do par, cada um
  // recebendo o outro. A normal acompanha só Enter/Stay.
  bool contact(const ContactEvent &event);
  void stop();
  const std::string &diagnostics() const { return diagnostics_; }
  static bool hasScripts(const SceneGraph &graph);
  // Descrição JSON dos comportamentos anexados, na ordem de pré-ordem da cena.
  static std::string attachments(const SceneGraph &graph);

private:
  void collectDiagnostics();
  void installAccess();
  QueryFilter queryFilter(const scene::ScriptQueryFilter &filter) const;
  static QueryShapeDesc queryShape(const scene::ScriptShapeQuery &shape);
  static void copyHits(const std::vector<QueryHit> &hits, u32 total, scene::ScriptQueryHit *out, int capacity);
  scene::ScriptRuntimeApi api_{};
  scene::ScriptSceneAccess access_{};
  std::string root_, diagnostics_;
  LogSink logSink_;
  GameWorld *world_ = nullptr;
  ScenePhysics *physics_ = nullptr;
  InputService *input_ = nullptr;
  const resources::AssetRegistry *assets_ = nullptr;
  const std::vector<resources::EnvironmentProfile> *environmentProfiles_ = nullptr;
  ResourceAvailability resourceAvailable_;
  RuntimeRenderingState rendering_{};
  renderer::ResolvedRenderingPolicy initialEffective_{};
  bool renderingConfigured_ = false;
  WorldStatus lastStatus_ = WorldStatus::Ok;
  bool running_ = false;
};

} // namespace ae::runtime
