// A ponte entre o mundo de execução e o runtime C#.
//
// Era `editor/editor_script_bridge.h` e falava o formato do documento do editor.
// Agora fala `GameWorld`: é aqui que a ABI v3 (scene/script_runtime.h) ganha
// corpo, e é o único lugar do nativo que sabe como um `Behavior` endereça
// objetos, componentes e propriedades.
//
// A plataforma injeta o serviço gerenciado; este arquivo não conhece CLR.
#pragma once
#include "runtime/game_world.h"
#include "runtime/input_actions.h"
#include "runtime/scene_physics.h"
#include "scene/script_runtime.h"

#include <functional>
#include <string>
#include <vector>

namespace ae::runtime {

class ScriptBridge final {
public:
  using LogSink = std::function<void(u64, std::string_view)>;
  void setLogSink(LogSink sink) { logSink_ = std::move(sink); }
  ~ScriptBridge() { stop(); }
  void configure(scene::ScriptRuntimeApi api, std::string root) {
    if (!running_) { api_ = api; root_ = std::move(root); }
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
  WorldStatus lastStatus_ = WorldStatus::Ok;
  bool running_ = false;
};

} // namespace ae::runtime
