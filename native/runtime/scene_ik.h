#pragma once
// IK em Play (N04): interface leve. A logica vive em scene_ik.cpp/scene_ik_impl.h,
// entao mudar um solver recompila um arquivo, nao o editor inteiro.
#include "runtime/game_world.h"
#include <functional>
#include <memory>
#include <vector>

namespace ae::runtime {
struct SceneIkTypes {
  enum class Issue { MissingTarget, ShortChain, Degenerate, NotConverged, NoGround, InvalidPose, NotInteractable };
  struct Diagnostic {ObjectId object;Issue issue;float residual=0;};
  // Acontecimento do pacote C, entregue a fila de eventos de componente pelo Play.
  struct Event {ObjectId system=0;const char *id="";ObjectId effector=0,object=0;};
  // Raio de apoio: origem, direcao com comprimento; devolve ponto e normal.
  using GroundQuery=std::function<bool(const float origin[3],const float direction[3],ObjectId ignore,float point[3],float normal[3])>;
  // Move um corpo fisico cinematico para a pose (posicao + quaternion xyzw).
  using BodyMover=std::function<bool(ObjectId body,const float pose[7])>;
  static const char *issueText(Issue issue) {
    switch(issue) {
    case Issue::MissingTarget:return "Alvo do IK ausente ou inativo";
    case Issue::ShortChain:return "Cadeia mais curta que o pedido";
    case Issue::Degenerate:return "Osso sem comprimento na cadeia";
    case Issue::NotConverged:return "IK não convergiu na tolerância";
    case Issue::NoGround:return "Sem chão no alcance do raio";
    case Issue::InvalidPose:return "Pose resultante inválida";
    case Issue::NotInteractable:return "Objeto sem Objeto interagível ou mão sem cadeia";
    }
    return "IK";
  }
};
class SceneIkSolver;
class SceneIk final : public SceneIkTypes {
public:
  SceneIk();
  ~SceneIk();
  SceneIk(const SceneIk &)=delete;
  SceneIk &operator=(const SceneIk &)=delete;
  const std::vector<Diagnostic> &diagnostics() const;
  void reset();
  // Pose de repouso (autoria) para a Referencia "Pose de repouso" do plano
  // rotativo; chamada logo apos carregar o mundo, antes da animacao.
  void captureRest(const SceneGraph &graph);
  bool advance(GameWorld &world,const GroundQuery &ground={});
  // Pacote C: golpe, recuo, interacao e colisao. Recusam objeto sem o
  // componente ativo; o efeito aparece no proximo advance.
  WorldStatus hit(GameWorld &world,ObjectId object,const float direction[3],float force);
  WorldStatus hitFront(GameWorld &world,ObjectId object,float force);
  WorldStatus fire(GameWorld &world,ObjectId object,float magnitude);
  WorldStatus interactionStart(GameWorld &world,ObjectId system,ObjectId effector,ObjectId object,bool &started);
  WorldStatus interactionStop(GameWorld &world,ObjectId system,ObjectId effector);
  void collision(GameWorld &world,ObjectId object,const float normal[3]);
  std::vector<Event> takeEvents();
  // Sem mover, objetos com corpo fisico nao podem ser carregados (diagnostico).
  void setBodyMover(BodyMover mover);
private:
  std::unique_ptr<SceneIkSolver> solver_;
};
} // namespace ae::runtime
