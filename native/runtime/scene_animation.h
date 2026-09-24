#pragma once
// Reprodução dos componentes Animation no mundo de execução (G6-B).
//
// Ordem no quadro, comparada à Unity (execution-order): scripts `Update` →
// ANIMAÇÃO → física → extração. Na Unity o Animation legado avalia depois do
// Update e antes do LateUpdate; aqui ainda não existe LateUpdate, então a pose
// animada é a que a física e o desenho recebem no mesmo quadro.
//
// Autoridade de pose: um nó cuja pose é publicada pela física (corpo dinâmico
// ou personagem) não é escrito pela animação — a física é dona dele. Os demais
// recebem translação, rotação e escala dos canais do clipe; componentes que o
// clipe não anima ficam como estão (script pode movê-los).
//
// Sem dependência de editor: quem conhece as fontes importadas entrega os
// clipes por `AnimationLibrary`.
#include "resources/asset_registry.h"
#include "resources/skeletal_animation.h"
#include "runtime/scene_graph.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ae::runtime {

// Clipes de uma fonte importada e a identidade (mapa de nós) de cada nó que os
// canais endereçam: `nodes[channel.node]`.
struct SourceAnimations {
  std::vector<resources::AnimationClip> clips;
  std::vector<resources::AssetGuid> nodes;
};

class AnimationLibrary {
public:
  virtual ~AnimationLibrary() = default;
  virtual const SourceAnimations *animations(const resources::AssetGuid &source) const = 0;
};

// Objetos-alvo de um dono de Animation: por índice de nó da fonte, o objeto da
// MESMA instância importada no dono ou abaixo dele (zero quando não existe).
// Falso quando o dono não veio de uma fonte importada.
bool resolveAnimationTargets(const SceneGraph &graph, ObjectId owner, const SourceAnimations &source,
                             std::vector<ObjectId> &targets);

// Escreve a pose do clipe no tempo local `time` (já envolvido pelo modo).
// `writable` recusa nós cuja pose pertence a outro sistema. Devolve quantos
// objetos mudaram de pose.
u32 applyAnimationPose(SceneGraph &graph, const resources::AnimationClip &clip, float time,
                       const std::vector<ObjectId> &targets, const std::function<bool(ObjectId)> &writable);

class SceneAnimator {
public:
  // Esquece o que já começou: o próximo Play volta a tocar do zero.
  void reset();
  // Avança todos os componentes Animation ativos do grafo por `delta`
  // segundos. Play Automatically liga `playing` na primeira passagem de cada
  // componente; depois disso, quem decide é o script.
  bool advance(SceneGraph &graph, const AnimationLibrary &library, float delta,
               const std::function<bool(ObjectId)> &writable);
  // Diagnóstico do último avanço: clipes tocando e nós escritos.
  u32 playingCount() const noexcept { return playing_; }
  u32 posedObjects() const noexcept { return posed_; }

private:
  std::unordered_set<u64> started_;
  std::unordered_map<ObjectId, std::vector<ObjectId>> targets_;
  u32 playing_ = 0, posed_ = 0;
};

} // namespace ae::runtime
