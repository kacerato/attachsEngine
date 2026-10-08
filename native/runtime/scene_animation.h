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
#include <memory>
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
  struct PoseChannel {
    ObjectId target=kInvalidObject;resources::AnimationPath path{};
    std::vector<float> value;float coverage=0;bool additive=false;
  };
  // Values are captured before upper-layer attenuation. Rotation sums retain
  // their magnitude: normalizing here would change a partially weighted pose.
  struct LayerPose {std::vector<PoseChannel> channels;};
  std::shared_ptr<const LayerPose> layerPose(ObjectId owner,u64 instance,u32 layer) const;
  // Amostra pronta vinda de outro avaliador (Animator): o clipe no tempo local
  // dado, com peso e camada; `mask` limita os alvos à subárvore daquele objeto.
  struct ExternalSample {
    ObjectId owner = kInvalidObject;
    resources::AssetGuid clip;
    float time = 0, weight = 0;
    u32 layer = 0;
    ObjectId mask = kInvalidObject;
    bool additive = false;
    resources::AssetGuid referenceClip{};
    float referenceTime = 0;
    ObjectId controllerOwner=kInvalidObject;u64 instance=0;
    float layerWeight=1;
    std::shared_ptr<const LayerPose> frozen{};
  };
  // Valem para o próximo advance e são consumidas por ele.
  void setExternalSamples(std::vector<ExternalSample> samples) { external_ = std::move(samples); }
  const AnimationLibrary *library() const noexcept { return library_; }
  std::string_view compositionDiagnostic() const noexcept {return compositionDiagnostic_;}
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
  struct CapturedLayer {ObjectId owner;u64 instance;u32 layer;std::shared_ptr<LayerPose> pose;};
  std::vector<CapturedLayer> layerPoses_;
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
  std::vector<ExternalSample> external_;
  std::string compositionDiagnostic_;
  // Alvos resolvidos por (dono, clipe) das amostras externas.
  struct ExternalTargets { ObjectId owner; resources::AssetGuid clip; std::vector<ObjectId> targets; };
  std::vector<ExternalTargets> externalTargets_;
  // Retired channels of an active graph return to their baseline. Inactive
  // graphs retain their last pose, matching the legacy enable/disable contract.
  struct ExternalBinding {ObjectId owner;u32 layer;ObjectId target;resources::AnimationPath path;usize width;};
  std::vector<ExternalBinding> externalBindings_;
  u32 playing_ = 0, posed_ = 0;
};

} // namespace ae::runtime
