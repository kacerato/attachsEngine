#pragma once
// Reprodução dos componentes Animation no mundo de execução (G6-B).
//
// Modelo do Animation legado da Unity: cada componente tem um ESTADO por clipe
// (tempo, velocidade, peso, camada, repetição, ligado) e comandos Play,
// CrossFade, Blend, Stop e Rewind. O estado vive aqui, não no componente — é
// execução, nunca vai para o arquivo, e o Play sempre parte do zero.
//
// Mistura, por propriedade animada (translação, rotação, escala de um nó ou os
// pesos dos blend shapes de uma malha):
//   1. Camadas da maior para a menor. Numa camada, os pesos dos estados ligados
//      que animam a propriedade somam S; acima de 1 eles são normalizados.
//   2. A camada recebe min(S,1) do peso que sobrou das camadas acima.
//   3. O que sobra no fim vai para a pose de repouso: a pose que o nó tinha
//      quando a animação o tocou pela primeira vez neste Play.
// Rotação mistura quaternions no mesmo hemisfério e renormaliza.
//
// Ordem no quadro, comparada à Unity: scripts `Update` → ANIMAÇÃO → física →
// extração. Nó cuja pose é publicada pela física não é escrito aqui.
//
// Sem dependência de editor: quem conhece as fontes importadas entrega os
// clipes por `AnimationLibrary`.
#include "resources/asset_registry.h"
#include "resources/skeletal_animation.h"
#include "runtime/scene_graph.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae::runtime {

// Clipes de uma fonte importada, com a identidade (e o nome, para o retarget
// por nome) de cada nó que os canais endereçam: `nodes[channel.node]`.
struct SourceAnimations {
  resources::AssetGuid source;
  std::vector<resources::AnimationClip> clips;
  std::vector<resources::AssetGuid> clipIds;
  std::vector<resources::AssetGuid> nodes;
  std::vector<std::string> nodeNames;
};

struct AnimationClipView {
  const resources::AnimationClip *clip = nullptr;
  const SourceAnimations *source = nullptr;
  std::string name;
};

class AnimationLibrary {
public:
  virtual ~AnimationLibrary() = default;
  virtual bool findClip(const resources::AssetGuid &clip, AnimationClipView &out) const = 0;
};

// Objetos-alvo de um clipe a partir de um dono: por nó da fonte, o objeto no
// dono ou abaixo dele com o mesmo nó importado; sem vínculo, o objeto de mesmo
// nome, se for único na subárvore. Zero onde não há alvo.
void resolveAnimationTargets(const SceneGraph &graph, ObjectId owner, const SourceAnimations &source,
                             std::vector<ObjectId> &targets);

enum class AnimationPlayMode : u32 { StopSameLayer = 0, StopAll = 1 };

enum class AnimationCommandStatus : u32 {
  Ok = 0, UnknownComponent = 1, UnknownClip = 2, ClipNotInComponent = 3, InvalidArgument = 4
};

// Estado de um clipe num componente, como a Unity expõe em AnimationState.
struct AnimationStateView {
  resources::AssetGuid clip;
  bool enabled = false;
  float time = 0, speed = 1, weight = 0, length = 0;
  u32 layer = 0;
  resources::AnimationWrapMode wrapMode = resources::AnimationWrapMode::Loop;
};

class SceneAnimator {
public:
  // Liga ao grafo do Play e à biblioteca. `reset` esquece tudo: o próximo Play
  // parte do zero.
  void begin(SceneGraph &graph, const AnimationLibrary &library);
  void reset();
  bool active() const noexcept { return graph_ != nullptr; }

  // Avança fades e tempos, mistura e escreve poses e pesos. `writable` recusa
  // nós cuja pose pertence a outro sistema.
  bool advance(float delta, const std::function<bool(ObjectId)> &writable);

  // --- comandos (scripts, testes) -----------------------------------------
  // `clip` inválido = clipe padrão do componente. Clipe fora da lista do
  // componente é recusado, como na Unity.
  AnimationCommandStatus play(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                              AnimationPlayMode mode = AnimationPlayMode::StopSameLayer);
  AnimationCommandStatus crossFade(ObjectId owner, u64 instance, const resources::AssetGuid &clip, float seconds,
                                   AnimationPlayMode mode = AnimationPlayMode::StopSameLayer);
  AnimationCommandStatus blend(ObjectId owner, u64 instance, const resources::AssetGuid &clip, float targetWeight,
                               float seconds);
  // `clip` inválido = todos os clipes do componente.
  AnimationCommandStatus stop(ObjectId owner, u64 instance, const resources::AssetGuid &clip);
  AnimationCommandStatus rewind(ObjectId owner, u64 instance, const resources::AssetGuid &clip);
  bool isPlaying(ObjectId owner, u64 instance, const resources::AssetGuid &clip) const;
  AnimationCommandStatus state(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                               AnimationStateView &out) const;
  // Escreve tempo, velocidade, peso, camada, repetição e ligado do estado.
  AnimationCommandStatus setState(ObjectId owner, u64 instance, const AnimationStateView &value);
  // Clipes do componente, na ordem da lista, com o nome de exibição.
  u32 clipCount(ObjectId owner, u64 instance) const;
  bool clipAt(ObjectId owner, u64 instance, u32 index, resources::AssetGuid &clip, std::string &name) const;

  // Diagnóstico do último avanço: estados tocando e objetos escritos.
  u32 playingCount() const noexcept { return playing_; }
  u32 posedObjects() const noexcept { return posed_; }

private:
  struct State {
    resources::AssetGuid clip;
    bool enabled = false;
    float time = 0, speed = 1, weight = 0, targetWeight = 0, fadeRate = 0;
    bool stopAtZero = false;
    u32 layer = 0;
    resources::AnimationWrapMode wrapMode = resources::AnimationWrapMode::Loop;
  };
  struct Player {
    ObjectId owner = kInvalidObject;
    u64 instance = 0;
    bool started = false;
    std::vector<State> states;
    // Alvos por clipe (mesma ordem de `states`), resolvidos na primeira vez.
    std::vector<std::vector<ObjectId>> targets;
  };
  struct Rest {
    Transform transform{};
    std::vector<float> weights;
    bool hasTransform = false, hasWeights = false;
  };
  Player *player(ObjectId owner, u64 instance, AnimationCommandStatus &status);
  const Player *findPlayer(ObjectId owner, u64 instance) const;
  State *stateOf(Player &player, const resources::AssetGuid &clip, AnimationCommandStatus &status);
  void syncStates(Player &player);
  float clipLength(const resources::AssetGuid &clip) const;

  SceneGraph *graph_ = nullptr;
  const AnimationLibrary *library_ = nullptr;
  std::vector<Player> players_;
  std::unordered_map<ObjectId, Rest> rest_;
  u32 playing_ = 0, posed_ = 0;
};

} // namespace ae::runtime
