#pragma once
// IK em Play (N04): interface leve. A logica vive em scene_ik.cpp/scene_ik_impl.h,
// entao mudar um solver recompila um arquivo, nao o editor inteiro.
#include "runtime/game_world.h"
#include <functional>
#include <memory>
#include <vector>

namespace ae::runtime {
struct SceneIkTypes {
  enum class Issue { MissingTarget, ShortChain, Degenerate, NotConverged, NoGround, InvalidPose };
  struct Diagnostic {ObjectId object;Issue issue;float residual=0;};
  // Raio de apoio: origem, direcao com comprimento; devolve ponto e normal.
  using GroundQuery=std::function<bool(const float origin[3],const float direction[3],ObjectId ignore,float point[3],float normal[3])>;
  static const char *issueText(Issue issue) {
    switch(issue) {
    case Issue::MissingTarget:return "Alvo do IK ausente ou inativo";
    case Issue::ShortChain:return "Cadeia mais curta que o pedido";
    case Issue::Degenerate:return "Osso sem comprimento na cadeia";
    case Issue::NotConverged:return "IK não convergiu na tolerância";
    case Issue::NoGround:return "Sem chão no alcance do raio";
    case Issue::InvalidPose:return "Pose resultante inválida";
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
  bool advance(GameWorld &world,const GroundQuery &ground={});
private:
  std::unique_ptr<SceneIkSolver> solver_;
};
} // namespace ae::runtime
