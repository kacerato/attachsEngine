#pragma once
// Root motion em Play (N05): depois da animacao e antes da fisica, mede o
// quanto o osso raiz andou no espaco do personagem, aplica ao personagem e
// devolve o osso ao lugar. Laco do clipe (o osso volta ao inicio) e detectado
// pelo salto e substituido pelo passo anterior, sem tranco.
#include "runtime/game_world.h"
#include <map>
#include <vector>

namespace ae::runtime {
class ScenePhysics;
class SceneRootMotion final {
public:
  enum class Route : u32 { Transform, Character, Body, Blocked };
  struct Diagnostic {ObjectId object;const char *text;};
  // `realized`/`residual`: o que o personagem andou de fato no passo anterior
  // e a diferenca para o pedido (parede, degrau alto, outro corpo).
  struct Report {ObjectId owner=0;Route route=Route::Transform;float delta[3]{},turn=0,realized[3]{},residual=0;bool loop=false,blocked=false;};
  void reset() {states_.clear();diagnostics_.clear();reports_.clear();}
  // `physics` opcional: sem ela, so a rota de transformacao existe.
  bool advance(GameWorld &world,float dt,ScenePhysics *physics);
  const std::vector<Diagnostic> &diagnostics() const noexcept {return diagnostics_;}
  const std::vector<Report> &reports() const noexcept {return reports_;}
private:
  struct State {float position[3]{},rest[3]{},step[3]{},requested[3]{},origin[3]{};float yaw=0,restYaw=0,turn=0;bool started=false,pending=false;};
  std::map<ObjectId,State> states_;
  std::vector<Diagnostic> diagnostics_;
  std::vector<Report> reports_;
};
} // namespace ae::runtime
