#include "runtime/animation_clip_events.h"
#include "runtime/scene_animation.h"
#include "resources/animation_composition.h"
#include "resources/animation_binding_path.h"

#include "runtime/transform_math.h"
#include "scene/animation.h"
#include "scene/import_link.h"
#include "scene/skinned_mesh.h"

#include "core/job_system.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iterator>
#include <span>
#include <thread>
#include <unordered_set>

namespace ae::runtime {
namespace {
using resources::AnimationPath;
using resources::AnimationWrapMode;

constexpr float PercentPerUnit = 100.0f; // pesos do glTF (0..1) → escala da Unity (0..100)

const scene::Animation *animationComponent(const SceneGraph &graph, ObjectId owner, u64 instance) {
  const auto *object = graph.find(owner);
  const auto *value = object ? object->components.findInstance(instance) : nullptr;
  return value && &value->type() == &scene::Animation::descriptor ? static_cast<const scene::Animation *>(value) : nullptr;
}

// Quaternion (x,y,z,w) do transform e de volta, pela convenção da cena.
void decompose(const Transform &transform, float t[3], float q[4], float s[3]) {
  std::copy(transform.position, transform.position + 3, t);
  std::copy(transform.scale, transform.scale + 3, s);
  transformRotationQuaternion(transform, q);
}

// Vetor com os primeiros N elementos em linha. Translação, rotação e escala
// cabem sem tocar o heap; pesos de blend shape longos usam o vetor de reserva.
// A mistura de animação criava milhares destes por quadro com std::vector.
template <class T, usize N>
class SmallVector {
public:
  SmallVector() = default;
  explicit SmallVector(usize count, const T &value = T{}) { resize(count, value); }
  SmallVector(std::span<const T> values) { assign(values.begin(), values.end()); }
  SmallVector &operator=(std::span<const T> values) { assign(values.begin(), values.end()); return *this; }
  usize size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T *data() { return heap_.empty() ? inline_.data() : heap_.data(); }
  const T *data() const { return heap_.empty() ? inline_.data() : heap_.data(); }
  T *begin() { return data(); }
  T *end() { return data() + size_; }
  const T *begin() const { return data(); }
  const T *end() const { return data() + size_; }
  T &operator[](usize index) { return data()[index]; }
  const T &operator[](usize index) const { return data()[index]; }
  T &front() { return data()[0]; }
  const T &front() const { return data()[0]; }
  void clear() { size_ = 0; heap_.clear(); }
  void resize(usize count, const T &value = T{}) {
    if (count > N || !heap_.empty()) {
      if (heap_.empty()) heap_.assign(inline_.begin(), inline_.begin() + size_);
      heap_.resize(count, value);
    } else {
      for (usize i = size_; i < count; ++i) inline_[i] = value;
    }
    size_ = count;
  }
  template <class It> void assign(It first, It last) {
    const usize count = static_cast<usize>(std::distance(first, last));
    heap_.clear();
    if (count > N) {
      heap_.assign(first, last);
    } else {
      std::copy(first, last, inline_.begin());
    }
    size_ = count;
  }
  void push_back(const T &value) {
    if (size_ < N && heap_.empty()) inline_[size_] = value;
    else {
      if (heap_.empty()) heap_.assign(inline_.begin(), inline_.begin() + size_);
      heap_.push_back(value);
    }
    ++size_;
  }

private:
  std::array<T, N> inline_{};
  std::vector<T> heap_;
  usize size_ = 0;
};
using AnimationValue = SmallVector<float, 4>;

// Índice u64 -> u32 por endereçamento aberto. clear() mantém a tabela; só o
// crescimento aloca, então depois do primeiro quadro não há nós de hash.
class FlatIndex {
public:
  static constexpr u32 Missing = 0xffffffffu;
  void clear(usize expected) {
    usize capacity = 64;
    while (capacity < expected * 2) capacity *= 2;
    if (keys_.size() < capacity) { keys_.assign(capacity, 0); values_.assign(capacity, 0); }
    else std::fill(keys_.begin(), keys_.end(), 0);
    count_ = 0;
  }
  u32 find(u64 key) const {
    const usize mask = keys_.size() - 1;
    for (usize slot = hash(key) & mask;; slot = (slot + 1) & mask) {
      if (keys_[slot] == 0) return Missing;
      if (keys_[slot] == key + 1) return values_[slot];
    }
  }
  // Devolve o valor existente ou insere `value`; `inserted` diz qual.
  u32 insert(u64 key, u32 value, bool &inserted) {
    if ((count_ + 1) * 2 > keys_.size()) grow();
    const usize mask = keys_.size() - 1;
    for (usize slot = hash(key) & mask;; slot = (slot + 1) & mask) {
      if (keys_[slot] == key + 1) { inserted = false; return values_[slot]; }
      if (keys_[slot] == 0) {
        keys_[slot] = key + 1; values_[slot] = value; ++count_; inserted = true; return value;
      }
    }
  }

private:
  static usize hash(u64 key) {
    key ^= key >> 33; key *= 0xff51afd7ed558ccdull; key ^= key >> 33;
    return static_cast<usize>(key);
  }
  void grow() {
    std::vector<u64> keys(std::max<usize>(64, keys_.size() * 2), 0);
    std::vector<u32> values(keys.size(), 0);
    const usize mask = keys.size() - 1;
    for (usize i = 0; i < keys_.size(); ++i) if (keys_[i]) {
      usize slot = hash(keys_[i] - 1) & mask;
      while (keys[slot]) slot = (slot + 1) & mask;
      keys[slot] = keys_[i]; values[slot] = values_[i];
    }
    keys_.swap(keys); values_.swap(values);
  }
  std::vector<u64> keys_;
  std::vector<u32> values_;
  usize count_ = 0;
};

struct AnimatorContribution {
  u32 layer = 0;
  float weight = 0;
  AnimationValue value;
  bool additive = false;
  AnimationValue reference;
  bool relative = false;
  ObjectId sourceOwner = kInvalidObject; u64 sourceInstance = 0; float coverage = 0;
};
struct AnimatorProperty {
  ObjectId object = kInvalidObject;
  AnimationPath path{};
  std::vector<AnimatorContribution> list;
};
struct AnimatorPose {
  float t[3], q[4], s[3];
  bool changed = false;
  // Bits 1 T, 2 R, 4 S escritos pela mistura. Só o que falta vem da
  // transformação atual: um osso com T, R e S no clipe não decompõe nada.
  u8 written = 0;
};
} // namespace

namespace {
// Pool da composição de animação. Próprio, e não o da física: waitIdle espera
// a fila inteira do pool. Até 4 workers além da thread do jogo.
// Desligado por padrão: no POCO (24/10) as fases paralelas eram só ~6% da
// thread do jogo; a thread caiu 0,47 ms e o processo subiu 1,3 ms. Volta a
// ligar quando a amostragem por personagem também for paralela.
std::atomic<bool> parallelAnimationEnabled{false};
JobSystem &animationJobs() {
  static JobSystem jobs(std::clamp(std::thread::hardware_concurrency(), 2u, 5u) - 1u);
  return jobs;
}
// Divide [0, count) em blocos; a thread chamadora faz o primeiro. Abaixo de
// dois blocos de `minimumPerChunk` roda inteiro aqui, sem custo de fila.
template <typename Fn> void parallelAnimationRange(usize count, usize minimumPerChunk, const Fn &fn) {
  if (!parallelAnimationEnabled.load(std::memory_order_relaxed) || count < minimumPerChunk * 2) {fn(0, count);return;}
  auto &jobs = animationJobs();
  const usize chunks = std::min<usize>(jobs.workerCount() + 1u, count / minimumPerChunk);
  const usize step = (count + chunks - 1) / chunks;
  for (usize chunk = 1; chunk < chunks; ++chunk) {
    const usize first = chunk * step, last = std::min(count, first + step);
    if (first < last) jobs.submit([&fn, first, last] { fn(first, last); });
  }
  fn(0, std::min(count, step));
  jobs.waitIdle();
}
} // namespace

namespace {
// Resultado da composição de uma propriedade (bits).
constexpr u8 ComposedOutcome = 1, ReferenceInvalidOutcome = 2, AdditiveInvalidOutcome = 4;
struct PoseWrite {Transform transform{};u8 state = 0;};

// Mistura pura de [first, last): camadas de cima para baixo, aditivas sobre o
// resultado e o repouso no que sobra. Só lê as propriedades e o repouso; cada
// propriedade escreve no próprio buffer, então roda em qualquer thread.
template <typename RestT>
void composeProperties(const AnimatorProperty *properties, RestT *const *rests, AnimationValue *restValues,
                       AnimationValue *results, u8 *outcomes, usize first, usize last) {
  for (usize index = first; index < last; ++index) {
    const auto &property = properties[index];
    const RestT &rest = *rests[index];
    AnimationValue &restValue = restValues[index];
    restValue.clear();
    float restT[3], restQ[4], restS[3];
    if (rest.decomposed) {
      std::copy(rest.t, rest.t + 3, restT); std::copy(rest.q, rest.q + 4, restQ); std::copy(rest.s, rest.s + 3, restS);
    } else {
      decompose(rest.transform, restT, restQ, restS);
    }
    switch (property.path) {
    case AnimationPath::Translation: restValue.assign(restT, restT + 3); break;
    case AnimationPath::Rotation: restValue.assign(restQ, restQ + 4); break;
    case AnimationPath::Scale: restValue.assign(restS, restS + 3); break;
    case AnimationPath::Weights:
      restValue.resize(property.list.front().value.size(), 0.0f);
      for (usize i = 0; i < restValue.size() && i < rest.weights.size(); ++i) restValue[i] = rest.weights[i] / PercentPerUnit;
      break;
    }
    const usize width = restValue.size();
    AnimationValue &result = results[index];
    result.clear();
    result.resize(width, 0.0f);
    u8 &outcome = outcomes[index];
    outcome = 0;
    const bool rotation = property.path == AnimationPath::Rotation;
    const float *reference = nullptr;
    const auto accumulate = [&](std::span<const float> value, float weight) {
      if (value.size() != width || weight <= 0) return;
      float sign = 1;
      if (rotation) {
        if (!reference) reference = value.data();
        float dot = 0;
        for (u32 i = 0; i < 4; ++i) dot += reference[i] * value[i];
        sign = dot < 0 ? -1.0f : 1.0f;
      }
      for (usize i = 0; i < width; ++i) result[i] += weight * sign * value[i];
    };
    SmallVector<u32, 8> layers;
    for (const auto &c : property.list) if (std::find(layers.begin(), layers.end(), c.layer) == layers.end()) layers.push_back(c.layer);
    std::sort(layers.begin(), layers.end(), std::greater<>());
    float remaining = 1;
    struct AdditiveLayer {AnimationValue delta;float weight;};
    std::vector<AdditiveLayer> additions;
    for (const auto layer : layers) {
      float additiveSum=0;AnimationValue delta(width,0.f);
      for(const auto &c:property.list) if(c.layer==layer&&c.additive) {
        if(c.weight<=0) continue;
        std::vector<float> relative;
        if(c.relative) relative.assign(c.value.begin(),c.value.end());
        else if(!resources::relativeAnimationPose(property.path,c.value,c.reference.empty()?restValue:c.reference,relative)) {
          outcome|=ReferenceInvalidOutcome;continue;
        }
        if(rotation&&additiveSum>0) {float dot=0;for(usize i=0;i<width;++i) dot+=delta[i]*relative[i];if(dot<0) for(float &v:relative) v=-v;}
        for(usize i=0;i<width;++i) delta[i]+=relative[i]*c.weight;
        additiveSum+=c.weight;
      }
      if(additiveSum>0&&remaining>1e-6f) {
        for(float &v:delta) v/=additiveSum;
        additions.push_back({std::move(delta),std::min(additiveSum,1.f)*remaining});
      }
      float sum = 0;
      for (const auto &c : property.list) if (c.layer == layer&&!c.additive) sum += c.weight;
      if (sum <= 0) continue;
      const float take = std::min(sum, 1.0f) * remaining;
      for (const auto &c : property.list) if (c.layer == layer&&!c.additive) accumulate(c.value, c.weight * take / sum);
      remaining -= take;
      if (remaining <= 1e-6f) break;
    }
    if (remaining > 1e-6f) accumulate(restValue, remaining);
    if (rotation) {
      const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] + result[2] * result[2] + result[3] * result[3]);
      if (!(length > 1e-12f)) continue;
      for (auto &v : result) v /= length;
    }
    for(auto addition=additions.rbegin();addition!=additions.rend();++addition)
      if(!resources::applyRelativeAnimationPose(property.path,result,addition->delta,addition->weight))
        outcome|=AdditiveInvalidOutcome;
    outcome|=ComposedOutcome;
  }
}

// TRS → Transform de Euler de [first, last), só lendo o grafo. state 1 escreve,
// 2 é pose não representável, 0 não muda nada.
void convertPoses(const SceneGraph &graph, std::pair<ObjectId, AnimatorPose> *poses, PoseWrite *writes, usize first, usize last) {
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (usize index = first; index < last; ++index) {
    auto &[object, pose] = poses[index];
    auto &out = writes[index];
    out.state = 0;
    if (!pose.changed) continue;
    const auto &current = graph.find(object)->transform;
    if (pose.written != 7) {
      float t[3], q[4], s[3];
      decompose(current, t, q, s);
      if (!(pose.written & 1)) std::copy(t, t + 3, pose.t);
      if (!(pose.written & 2)) std::copy(q, q + 4, pose.q);
      if (!(pose.written & 4)) std::copy(s, s + 3, pose.s);
    }
    float local[16];
    resources::composeTransform(pose.t, pose.q, pose.s, local);
    // Escala negativa ou nula no clipe não cabe em TRS de Euler: o nó mantém a
    // pose anterior em vez de receber uma decomposição inventada.
    if (!localTransformForWorld(local, identity, out.transform)) {out.state = 2;continue;}
    if (std::equal(current.position, current.position + 3, out.transform.position) &&
        std::equal(current.rotationDegrees, current.rotationDegrees + 3, out.transform.rotationDegrees) &&
        std::equal(current.scale, current.scale + 3, out.transform.scale))
      continue;
    out.state = 1;
  }
}
} // namespace

void SceneAnimator::setParallelEvaluation(bool enabled) noexcept {
  parallelAnimationEnabled.store(enabled, std::memory_order_relaxed);
}

struct SceneAnimator::AdvanceScratch {
  std::vector<ObjectId> ids;
  // Componentes Animation vivos, válidos enquanto (identidade, estrutura) do grafo
  // não mudam: quadros que só mexem em poses não percorrem o grafo de novo.
  std::vector<std::pair<ObjectId, u64>> live;
  u64 liveIdentity = 0, liveStructure = 0;
  bool liveValid = false;
  // Propriedades do quadro: os primeiros `propertyCount` valem; os demais
  // guardam a capacidade das listas para o próximo quadro.
  std::vector<AnimatorProperty> properties;
  usize propertyCount = 0;
  FlatIndex propertyIndex;
  // Pares (dono<<32|camada, alvo<<2|caminho) já vistos neste quadro.
  FlatIndex bindingSeen;
  std::vector<std::pair<ObjectId, AnimatorPose>> poses;
  FlatIndex poseIndex;
  std::vector<ExternalBinding> nextBindings;
  // Repouso de cada propriedade (CPU, 23/10/2026: o lookup em rest_ por osso e
  // quadro era 2% da thread do jogo na Vila). Nós de unordered_map não mudam de
  // endereço; o ponteiro vale até rest_ ser limpo (geração) ou o animador mudar.
  std::vector<std::pair<ObjectId, Rest *>> restCache;
  // Buffers das fases da composição; mantêm a capacidade entre quadros.
  std::vector<Rest *> restRefs;
  std::vector<AnimationValue> restValues, results;
  std::vector<u8> outcomes;
  std::vector<PoseWrite> transforms;
  // Caminho por personagem: cada Animator com as suas propriedades, índices,
  // bindings, repouso, poses e escritas, avaliado inteiro num job.
  struct CharacterWork {
    ObjectId owner = kInvalidObject;
    usize firstSample = 0, lastSample = 0, channels = 0;
    std::vector<AnimatorProperty> properties;
    usize count = 0;
    FlatIndex index, bindingSeen, poseIndex;
    std::vector<ExternalBinding> bindings;
    std::vector<std::pair<ObjectId, Rest *>> restCache;
    std::vector<Rest *> rests;
    std::vector<AnimationValue> restValues, results;
    std::vector<u8> outcomes;
    std::vector<std::pair<ObjectId, AnimatorPose>> poses;
    std::vector<PoseWrite> writes;
    std::string diagnostic;
    u32 playing = 0;
  };
  struct PreparedSample {bool valid = false;AnimationClipView view;usize targets = 0;};
  std::vector<CharacterWork> characters;
  std::vector<PreparedSample> prepared;
  FlatIndex targetCharacter;
  // Alvo → intervalo do Animator no quadro (taxa por tamanho na tela) e
  // osso → interpolação ativa.
  FlatIndex strideTargets, interpolationIndex;
  const void *restOwner = nullptr;
  u64 restGeneration = 0;
};

void resolveAnimationTargets(const SceneGraph &graph, ObjectId owner, const SourceAnimations &source,
                             std::vector<ObjectId> &targets) {
  targets.assign(source.nodes.size(), kInvalidObject);
  std::unordered_map<resources::AssetGuid, u32, resources::AssetGuidHash> index;
  for (u32 i = 0; i < source.nodes.size(); ++i) index.emplace(source.nodes[i], i);
  std::vector<ObjectId> subtree;
  graph.collectSubtree(owner, subtree);
  // 1) Identidade do nó importado da MESMA fonte, o mais perto do dono.
  for (const auto id : subtree) {
    const auto *candidate = graph.find(id);
    const auto *link = candidate ? scene::importLink(candidate->components) : nullptr;
    // A parte artificial de uma primitiva (representação legada) não é nó.
    if (!link || link->source != source.source || !link->node.valid() || link->primitive >= 0) continue;
    const auto found = index.find(link->node);
    if (found != index.end() && targets[found->second] == kInvalidObject) targets[found->second] = id;
  }
  // 2) Explicit authored paths, resolved one parent at a time. Duplicate
  // siblings are ambiguous; duplicate short names in different branches are
  // valid. This preserves bindings across save/load and prefab instantiation.
  if(!source.nodePaths.empty()) {
    for(u32 i=0;i<targets.size()&&i<source.nodePaths.size();++i) {
      if(targets[i]!=kInvalidObject)continue;
      const std::string_view path=source.nodePaths[i];
      if(path.size()>1024||path.find('\\')!=path.npos||(!path.empty()&&(path.front()=='/'||path.back()=='/')))continue;
      ObjectId current=owner;
      for(usize start=0;current&&start<path.size();) {
        const auto end=path.find('/',start);const auto encoded=path.substr(start,end==path.npos?path.size()-start:end-start);
        std::string name;if(!resources::animationBindingName(encoded,name)) {current=kInvalidObject;break;}
        ObjectId match=kInvalidObject;bool ambiguous=false;
        for(const auto id:graph.childrenOf(current))if(const auto *object=graph.find(id);object&&name==object->name) {
          if(match) {ambiguous=true;break;}match=id;
        }
        current=ambiguous?kInvalidObject:match;
        if(end==path.npos)break;
        start=end+1;
      }
      if(graph.exists(current))targets[i]=current;
    }
    return;
  }
  // 3) Sem vínculo: nome único na subárvore (retarget por nome).
  std::unordered_map<std::string_view, i64> byName;
  for (const auto id : subtree) {
    const auto *candidate = graph.find(id);
    if (!candidate) continue;
    auto [it, inserted] = byName.emplace(std::string_view(candidate->name), static_cast<i64>(id));
    if (!inserted) it->second = -1; // repetido: ambíguo, não adivinha
  }
  for (u32 i = 0; i < targets.size() && i < source.nodeNames.size(); ++i) {
    if (targets[i] != kInvalidObject) continue;
    const auto found = byName.find(source.nodeNames[i]);
    if (found != byName.end() && found->second > 0) targets[i] = static_cast<ObjectId>(found->second);
  }
}

void SceneAnimator::begin(SceneGraph &graph, const AnimationLibrary &library) {
  reset();
  graph_ = &graph;
  library_ = &library;
}

void SceneAnimator::reset() {
  eventWorld_=nullptr;events_=nullptr;suppressedCues_=0;
  layerPoses_.clear();
  propertyMixer_.reset();
  driverGui_={};driverParameter_={};propertyPublication_={};
  graph_ = nullptr;
  library_ = nullptr;
  players_.clear();
  rest_.clear();
  ++restGeneration_;
  interpolations_.clear();
  external_.clear();
  compositionDiagnostic_.clear();
  externalTargets_.clear();
  externalBindings_.clear();
  playing_ = posed_ = 0;
}

float SceneAnimator::clipLength(const resources::AssetGuid &clip) const {
  AnimationClipView view;
  return library_ && library_->findClip(clip, view) ? view.clip->duration : 0.0f;
}

const SceneAnimator::Player *SceneAnimator::findPlayer(ObjectId owner, u64 instance) const {
  for (const auto &entry : players_) if (entry.owner == owner && entry.instance == instance) return &entry;
  return nullptr;
}

void SceneAnimator::syncStates(Player &player) {
  const auto *component = graph_ ? animationComponent(*graph_, player.owner, player.instance) : nullptr;
  if (!component) return;
  // Uma troca/remocao da lista em Play nao pode deixar o clipe antigo tocando.
  // Estados e alvos usam o mesmo indice; removemos o par antes de acrescentar
  // qualquer estado novo do componente autoral efetivo.
  for(usize i=0;i<player.states.size();) {
    const auto &id=player.states[i].clip;
    if(id==component->clip || component->contains(id)) {++i;continue;}
    player.states.erase(player.states.begin()+static_cast<std::ptrdiff_t>(i));
    player.targets.erase(player.targets.begin()+static_cast<std::ptrdiff_t>(i));
  }
  const auto ensure = [&](const resources::AssetGuid &clip) {
    if (!clip.valid()) return;
    for (const auto &state : player.states) if (state.clip == clip) return;
    State state;
    state.clip = clip;
    state.speed = component->speed;
    state.wrapMode = component->wrapMode;
    player.states.push_back(state);
    player.targets.emplace_back();
  };
  ensure(component->clip);
  for (const auto &entry : component->clips) ensure(entry.asset);
}

SceneAnimator::Player *SceneAnimator::player(ObjectId owner, u64 instance, AnimationCommandStatus &status) {
  status = AnimationCommandStatus::UnknownComponent;
  if (!graph_ || !animationComponent(*graph_, owner, instance)) return nullptr;
  status = AnimationCommandStatus::Ok;
  for (auto &entry : players_)
    if (entry.owner == owner && entry.instance == instance) {
      syncStates(entry);
      return &entry;
    }
  Player created;
  created.owner = owner;
  created.instance = instance;
  players_.push_back(std::move(created));
  auto &fresh = players_.back();
  syncStates(fresh);
  // Tocar ao iniciar acontece quando o reprodutor nasce — antes de qualquer
  // comando do Start de um script, como o Play Automatically da Unity, que
  // corre na ativação do componente.
  fresh.started = true;
  const auto *component = animationComponent(*graph_, owner, instance);
  if (component->playAutomatically && component->clip.valid())
    for (auto &state : fresh.states)
      if (state.clip == component->clip) {
        state.enabled = true;
        state.weight = state.targetWeight = 1;
      }
  return &fresh;
}

SceneAnimator::State *SceneAnimator::stateOf(Player &player, const resources::AssetGuid &clip,
                                             AnimationCommandStatus &status) {
  const auto *component = animationComponent(*graph_, player.owner, player.instance);
  const auto id = clip.valid() ? clip : component ? component->clip : resources::AssetGuid{};
  status = AnimationCommandStatus::ClipNotInComponent;
  if (!id.valid()) return nullptr;
  for (auto &state : player.states)
    if (state.clip == id) {
      AnimationClipView view;
      status = library_ && library_->findClip(id, view) ? AnimationCommandStatus::Ok : AnimationCommandStatus::UnknownClip;
      return status == AnimationCommandStatus::Ok ? &state : nullptr;
    }
  return nullptr;
}

AnimationCommandStatus SceneAnimator::play(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                                           AnimationPlayMode mode) {
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  auto *target = stateOf(*p, clip, status);
  if (!target) return status;
  for (auto &state : p->states) {
    if (&state == target || (mode == AnimationPlayMode::StopSameLayer && state.layer != target->layer)) continue;
    state.enabled = false;
    state.weight = state.targetWeight = state.fadeRate = 0;
    state.time = 0;
  }
  // Play num clipe que já toca não rebobina, como na Unity.
  target->enabled = true;
  target->weight = target->targetWeight = 1;
  target->fadeRate = 0;
  target->stopAtZero = false;
  return AnimationCommandStatus::Ok;
}

AnimationCommandStatus SceneAnimator::crossFade(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                                                float seconds, AnimationPlayMode mode) {
  if (!std::isfinite(seconds) || seconds < 0) return AnimationCommandStatus::InvalidArgument;
  if (seconds == 0) return play(owner, instance, clip, mode);
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  auto *target = stateOf(*p, clip, status);
  if (!target) return status;
  if (!target->enabled) {
    target->enabled = true;
    target->weight = 0;
    target->time = 0;
  }
  target->targetWeight = 1;
  target->fadeRate = (1 - target->weight) / seconds;
  target->stopAtZero = false;
  for (auto &state : p->states) {
    if (&state == target || !state.enabled ||
        (mode == AnimationPlayMode::StopSameLayer && state.layer != target->layer)) continue;
    state.targetWeight = 0;
    state.fadeRate = state.weight / seconds;
    state.stopAtZero = true;
  }
  return AnimationCommandStatus::Ok;
}

AnimationCommandStatus SceneAnimator::blend(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                                            float targetWeight, float seconds) {
  if (!std::isfinite(targetWeight) || targetWeight < 0 || targetWeight > 1 || !std::isfinite(seconds) || seconds < 0)
    return AnimationCommandStatus::InvalidArgument;
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  auto *target = stateOf(*p, clip, status);
  if (!target) return status;
  if (!target->enabled) {
    target->enabled = true;
    target->weight = 0;
  }
  target->targetWeight = targetWeight;
  target->stopAtZero = false;
  if (seconds == 0) {
    target->weight = targetWeight;
    target->fadeRate = 0;
  } else {
    target->fadeRate = std::fabs(targetWeight - target->weight) / seconds;
  }
  return AnimationCommandStatus::Ok;
}

AnimationCommandStatus SceneAnimator::stop(ObjectId owner, u64 instance, const resources::AssetGuid &clip) {
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  for (auto &state : p->states) {
    if (clip.valid() && state.clip != clip) continue;
    // Stop da Unity também rebobina; a pose atual fica.
    state.enabled = false;
    state.weight = state.targetWeight = state.fadeRate = 0;
    state.time = 0;
    state.stopAtZero = false;
    if (clip.valid()) return AnimationCommandStatus::Ok;
  }
  return clip.valid() ? AnimationCommandStatus::ClipNotInComponent : AnimationCommandStatus::Ok;
}

AnimationCommandStatus SceneAnimator::rewind(ObjectId owner, u64 instance, const resources::AssetGuid &clip) {
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  bool found = false;
  for (auto &state : p->states)
    if (!clip.valid() || state.clip == clip) {
      state.time = 0;
      found = true;
    }
  return found || !clip.valid() ? AnimationCommandStatus::Ok : AnimationCommandStatus::ClipNotInComponent;
}

bool SceneAnimator::isPlaying(ObjectId owner, u64 instance, const resources::AssetGuid &clip) const {
  const auto *p = findPlayer(owner, instance);
  if (!p) return false;
  for (const auto &state : p->states)
    if ((!clip.valid() || state.clip == clip) && state.enabled) return true;
  return false;
}

AnimationCommandStatus SceneAnimator::state(ObjectId owner, u64 instance, const resources::AssetGuid &clip,
                                            AnimationStateView &out) const {
  if (!graph_ || !animationComponent(*graph_, owner, instance)) return AnimationCommandStatus::UnknownComponent;
  const auto *component = animationComponent(*graph_, owner, instance);
  const auto id = clip.valid() ? clip : component->clip;
  out = {};
  out.clip = id;
  out.speed = component->speed;
  out.wrapMode = component->wrapMode;
  if (!component->contains(id) && component->clip != id) return AnimationCommandStatus::ClipNotInComponent;
  out.length = clipLength(id);
  if (const auto *p = findPlayer(owner, instance))
    for (const auto &state : p->states)
      if (state.clip == id) {
        out.enabled = state.enabled;
        out.time = state.time;
        out.speed = state.speed;
        out.weight = state.weight;
        out.layer = state.layer;
        out.wrapMode = state.wrapMode;
      }
  return AnimationCommandStatus::Ok;
}

AnimationCommandStatus SceneAnimator::setState(ObjectId owner, u64 instance, const AnimationStateView &value) {
  if (!std::isfinite(value.time) || std::fabs(value.time) > 1e6f || !std::isfinite(value.speed) ||
      std::fabs(value.speed) > 10 || !std::isfinite(value.weight) || value.weight < 0 || value.weight > 1 ||
      static_cast<u32>(value.wrapMode) > 3 || value.layer > 64)
    return AnimationCommandStatus::InvalidArgument;
  AnimationCommandStatus status;
  auto *p = player(owner, instance, status);
  if (!p) return status;
  auto *target = stateOf(*p, value.clip, status);
  if (!target) return status;
  const bool replacesBlend=value.enabled!=target->enabled||value.weight!=target->weight||value.layer!=target->layer;
  target->enabled = value.enabled;
  target->time = value.time;
  target->speed = value.speed;
  target->weight = value.weight;
  // AnimationState setters transmit a complete snapshot. Changing cadence,
  // phase or wrap mode must not silently cancel an ongoing CrossFade.
  if(replacesBlend) {
    target->targetWeight=value.weight;target->fadeRate=0;target->stopAtZero=false;
  }
  target->layer = value.layer;
  target->wrapMode = value.wrapMode;
  return AnimationCommandStatus::Ok;
}

u32 SceneAnimator::clipCount(ObjectId owner, u64 instance) const {
  const auto *component = graph_ ? animationComponent(*graph_, owner, instance) : nullptr;
  return component ? static_cast<u32>(component->clips.size()) : 0u;
}

bool SceneAnimator::clipAt(ObjectId owner, u64 instance, u32 index, resources::AssetGuid &clip, std::string &name) const {
  const auto *component = graph_ ? animationComponent(*graph_, owner, instance) : nullptr;
  if (!component || index >= component->clips.size()) return false;
  clip = component->clips[index].asset;
  AnimationClipView view;
  name = library_ && clip.valid() && library_->findClip(clip, view) ? view.name : std::string();
  return true;
}

std::shared_ptr<const SceneAnimator::LayerPose> SceneAnimator::layerPose(ObjectId owner,u64 instance,u32 layer) const {
  for(const auto &entry:layerPoses_) if(entry.owner==owner&&entry.instance==instance&&entry.layer==layer) return entry.pose;
  return {};
}
bool sampleChannel(const SourceAnimations &source,const resources::AnimationChannel &channel,float time,std::span<float> out) {
  return source.channelsValidated?resources::sampleValidatedAnimationChannel(channel,time,out):resources::sampleAnimationChannel(channel,time,out);
}

bool SceneAnimator::advance(float delta, const std::function<bool(ObjectId)> &writable) {
  u32 cueBudget=256;suppressedCues_=0;
  playing_ = posed_ = 0;
  compositionDiagnostic_.clear();
  if (!graph_ || !library_ || !std::isfinite(delta) || delta < 0) return graph_ == nullptr;
  auto &graph = *graph_;
  layerPoses_.clear();
  if (!scratch_) scratch_ = std::make_shared<AdvanceScratch>();
  auto &scratch = *scratch_;
  ++frame_;
  // Escrita de pose. Intervalo 1 escreve direto; maior guarda a pose mostrada e
  // a avaliada e anda 1/intervalo por quadro até a próxima avaliação.
  scratch.interpolationIndex.clear(interpolations_.size());
  for (usize index = 0; index < interpolations_.size(); ++index) {
    bool inserted = false;
    scratch.interpolationIndex.insert(interpolations_[index].object, static_cast<u32>(index), inserted);
  }
  const auto blendWrite = [&](RateInterpolation &rate) {
    const float alpha = static_cast<float>(rate.step) / static_cast<float>(rate.steps);
    float t[3], q[4], s[3], dot = 0;
    for (u32 i = 0; i < 4; ++i) dot += rate.from[3 + i] * rate.to[3 + i];
    const float sign = dot < 0 ? -1.0f : 1.0f;
    for (u32 i = 0; i < 3; ++i) {
      t[i] = rate.from[i] + (rate.to[i] - rate.from[i]) * alpha;
      s[i] = rate.from[7 + i] + (rate.to[7 + i] - rate.from[7 + i]) * alpha;
    }
    float length = 0;
    for (u32 i = 0; i < 4; ++i) {q[i] = rate.from[3 + i] * (1 - alpha) + sign * rate.to[3 + i] * alpha;length += q[i] * q[i];}
    length = std::sqrt(length);
    if (!(length > 1e-12f)) return;
    for (auto &v : q) v /= length;
    float local[16];
    resources::composeTransform(t, q, s, local);
    const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    Transform transform;
    if (localTransformForWorld(local, identity, transform) && graph.setTransform(rate.object, transform)) ++posed_;
  };
  const auto writePose = [&](ObjectId object, const Transform &transform, u8 stride) {
    const u32 existing = scratch.interpolationIndex.find(object);
    if (stride <= 1) {
      if (existing != FlatIndex::Missing) interpolations_[existing].steps = 0;  // aposentada no fim do quadro
      if (graph.setTransform(object, transform)) ++posed_;
      return;
    }
    RateInterpolation rate;
    rate.object = object;rate.steps = stride;rate.step = 1;rate.frame = frame_;
    decompose(graph.find(object)->transform, rate.from, rate.from + 3, rate.from + 7);
    decompose(transform, rate.to, rate.to + 3, rate.to + 7);
    if (existing != FlatIndex::Missing) interpolations_[existing] = rate;
    else {
      bool inserted = false;
      scratch.interpolationIndex.insert(object, static_cast<u32>(interpolations_.size()), inserted);
      interpolations_.push_back(rate);
    }
    blendWrite(existing != FlatIndex::Missing ? interpolations_[existing] : interpolations_.back());
  };

  // Componentes vivos; um jogador cujo componente sumiu sai junto.
  auto &live = scratch.live;
  if (!scratch.liveValid || scratch.liveIdentity != graph.identity() || scratch.liveStructure != graph.structureRevision()) {
    scratch.liveValid = true;
    scratch.liveIdentity = graph.identity();
    scratch.liveStructure = graph.structureRevision();
    auto &ids = scratch.ids;
    graph.collectSubtree(graph.root(), ids);
    live.clear();
    for (const auto id : ids) {
      const auto *object = graph.find(id);
      if (!object) continue;
      for (usize i = 0; i < object->components.size(); ++i) {
        const auto *value = object->components.at(i);
        if (&value->type() == &scene::Animation::descriptor) live.emplace_back(id, value->instanceId());
      }
    }
  }
  std::erase_if(players_, [&](const Player &p) {
    return std::find(live.begin(), live.end(), std::make_pair(p.owner, p.instance)) == live.end();
  });

  // Uma entrada por propriedade animada neste quadro.
  auto &properties = scratch.properties;
  scratch.propertyCount = 0;
  scratch.propertyIndex.clear(std::max<usize>((external_.size() + live.size()) * 64, properties.size()));
  std::vector<AnimationPropertyContribution> typedProperties;bool typedValid=true;
  std::vector<AnimationDriverRequest> driverRequests;
  const ExternalSample *contributionSource=nullptr;
  const auto contributeTyped=[&](const resources::AnimationClip &clip,std::span<const ObjectId> targets,float time,float weight,u32 layer,bool additive,ObjectId mask) {
    if(clip.drivers&&weight>0) {
      AnimationDriverRequest request;request.binding=clip.drivers;request.targets.assign(targets.begin(),targets.end());request.time=time;request.weight=weight;request.layer=layer;request.mask=mask;
      if(contributionSource){request.sourceOwner=contributionSource->controllerOwner;request.sourceInstance=contributionSource->instance;request.coverage=contributionSource->layerWeight>0?weight/contributionSource->layerWeight:0;}
      for(const auto &target:clip.drivers->targets) {
        if(target.node>=targets.size()||!graph.exists(targets[target.node])) {compositionDiagnostic_="Binding de saída de driver ausente";typedValid=false;return;}
        AnimationPropertyContribution baseline;baseline.object=targets[target.node];baseline.address=target.address;baseline.weight=0;baseline.coverage=0;
        typedProperties.push_back(std::move(baseline));
      }
      driverRequests.push_back(std::move(request));
    }
    for(const auto &channel:clip.properties) {
      if(channel.node>=targets.size()||!graph.exists(targets[channel.node])) {compositionDiagnostic_="Binding de propriedade ausente: "+channel.address.property;typedValid=false;return;}
      const auto target=targets[channel.node];if(mask&&target!=mask&&!graph.isDescendantOf(target,mask))continue;
      AnimationPropertyContribution contribution;contribution.object=target;contribution.address=channel.address;
      contribution.weight=weight;contribution.layer=layer;contribution.additive=additive;
      if(!channel.data.sample(time,contribution.value)) {compositionDiagnostic_="Propriedade não produz valor válido: "+channel.address.property;typedValid=false;return;}
      if(contributionSource) {
        contribution.sourceOwner=contributionSource->controllerOwner;contribution.sourceInstance=contributionSource->instance;
        contribution.coverage=contributionSource->layerWeight>0?weight/contributionSource->layerWeight:0;
        if(additive&&contributionSource->referenceClip.valid()) {
          AnimationClipView reference;
          if(!library_->findClip(contributionSource->referenceClip,reference)||!reference.clip||!reference.source) {
            compositionDiagnostic_="Referência aditiva tipada ausente";typedValid=false;return;
          }
          std::vector<ObjectId> referenceTargets;resolveAnimationTargets(graph,contributionSource->owner,*reference.source,referenceTargets);
          const auto found=std::find_if(reference.clip->properties.begin(),reference.clip->properties.end(),[&](const auto &candidate) {
            return candidate.node<referenceTargets.size()&&referenceTargets[candidate.node]==target&&candidate.address==channel.address;
          });
          resources::AnimationPropertyValue value;
          if(found==reference.clip->properties.end()||!found->data.sample(std::clamp(contributionSource->referenceTime,0.f,reference.clip->duration),value)) {
            compositionDiagnostic_="Propriedade ausente na referência aditiva: "+channel.address.property;typedValid=false;return;
          }
          contribution.reference=std::move(value);
        }
      }
      typedProperties.push_back(std::move(contribution));
    }
  };
  const auto contribute = [&](ObjectId object, AnimationPath path, u32 layer, float weight, std::span<const float> value,
                              bool additive=false,std::span<const float> reference={},bool relative=false) {
    const u64 key = (u64(object) << 2) | static_cast<u64>(path);
    bool inserted = false;
    const u32 index = scratch.propertyIndex.insert(key, static_cast<u32>(scratch.propertyCount), inserted);
    if (inserted) {
      if (scratch.propertyCount < properties.size()) {
        auto &reused = properties[scratch.propertyCount];
        reused.object = object; reused.path = path; reused.list.clear();
      } else {
        properties.push_back({object, path, {}});
      }
      ++scratch.propertyCount;
    }
    const auto *source=contributionSource;
    auto &entry = properties[index].list.emplace_back();
    entry.layer = layer;
    entry.weight = source?weight*source->layerWeight:weight;
    entry.value = value;
    entry.additive = additive;
    entry.reference = reference;
    entry.relative = relative;
    entry.sourceOwner = source?source->controllerOwner:kInvalidObject;
    entry.sourceInstance = source?source->instance:0;
    entry.coverage = source?weight:0;
  };
  const auto hasProperty = [&](u64 key) { return scratch.propertyIndex.find(key) != FlatIndex::Missing; };

  for (const auto &[owner, instance] : live) {
    if (!graph.activeInHierarchy(owner)) continue;
    const auto *component = animationComponent(graph, owner, instance);
    if (!component || !component->enabled) continue;
    AnimationCommandStatus status;
    auto *p = player(owner, instance, status);
    if (!p) continue;
    for (usize s = 0; s < p->states.size(); ++s) {
      auto &state = p->states[s];
      if (!state.enabled) continue;
      AnimationClipView view;
      if (!library_->findClip(state.clip, view)) continue;
      // Fade no peso, depois o tempo.
      if (state.fadeRate > 0) {
        const float step = state.fadeRate * delta;
        if (std::fabs(state.targetWeight - state.weight) <= step) {
          state.weight = state.targetWeight;
          state.fadeRate = 0;
        } else {
          state.weight += state.targetWeight > state.weight ? step : -step;
        }
        if (state.weight <= 0 && state.stopAtZero) {
          state.enabled = false;
          state.time = 0;
          state.stopAtZero = false;
          continue;
        }
      }
      const float previousTime=state.time;
      state.time += delta * state.speed;
      const bool validClock=std::isfinite(state.time)&&std::fabs(state.time)<=1e6f;
      if(!validClock)state.time=0;
      if(eventWorld_&&state.weight>0&&validClock) {
        const auto lost=emitAnimationClipCues(*eventWorld_,events_,owner,p->instance,scene::Animation::descriptor,*view.clip,previousTime,state.time,state.wrapMode,cueBudget,view.source&&view.source->channelsValidated);
        suppressedCues_=lost>std::numeric_limits<u64>::max()-suppressedCues_?std::numeric_limits<u64>::max():suppressedCues_+lost;emitAnimationCueLoss(*eventWorld_,events_,owner,p->instance,scene::Animation::descriptor,lost);
      }
      bool finished = false;
      float local = resources::wrapAnimationTime(state.time, view.clip->duration, state.wrapMode, finished);
      if (state.wrapMode == AnimationWrapMode::Once && state.time < 0) {
        finished = true;
        local = 0;
      }
      ++playing_;
      // WrapMode.Once da Unity: no fim o clipe para e rebobina; a última pose
      // amostrada fica (este quadro ainda contribui com ela).
      if (finished) {
        state.enabled = false;
        state.time = 0;
      }
      if (state.weight <= 0) continue;
      auto &targets = p->targets[s];
      bool stale = targets.size() != view.source->nodes.size();
      for (const auto target : targets) stale = stale || (target != kInvalidObject && !graph.exists(target));
      if (stale) resolveAnimationTargets(graph, owner, *view.source, targets);
      contributeTyped(*view.clip,targets,local,state.weight,state.layer,false,kInvalidObject);
      for (const auto &channel : view.clip->channels) {
        if (channel.node >= targets.size() || targets[channel.node] == kInvalidObject) continue;
        const ObjectId target = targets[channel.node];
        if (channel.path != AnimationPath::Weights && writable && !writable(target)) continue;
        AnimationValue value(channel.components());
        if(!sampleChannel(*view.source,channel,local,value)) {
          compositionDiagnostic_="Canal não produz valor válido: "+state.clip.text()+" · nó "+std::to_string(channel.node);continue;
        }
        contribute(target, channel.path, state.layer, state.weight, value);
      }
    }
  }

  // Active layer markers also reach this path when a state has no clip.
  std::unordered_set<u64> activeLayers;
  for(const auto &sample:external_) if(graph.exists(sample.owner)) {
    activeLayers.insert((u64(sample.owner)<<32)|sample.layer);
  }
  auto &nextBindings = scratch.nextBindings;
  nextBindings.clear();
  scratch.bindingSeen.clear(externalBindings_.size());
  // Primeira vez que (dono, camada) liga (alvo, caminho) neste quadro.
  const auto firstBinding = [&](ObjectId owner, u32 layer, ObjectId target, AnimationPath path) {
    const u64 key = (((u64(owner) << 32) | layer) * 0x9e3779b97f4a7c15ull) ^ ((u64(target) << 2) | static_cast<u64>(path));
    bool inserted = false;
    scratch.bindingSeen.insert(key, 0, inserted);
    if (!inserted) {
      // Colisão do hash combinado: confirma pelos dados antes de recusar.
      return std::none_of(nextBindings.begin(), nextBindings.end(), [&](const ExternalBinding &b) {
        return b.owner == owner && b.layer == layer && b.target == target && b.path == path;
      });
    }
    return true;
  };
  for(const auto &binding:externalBindings_) {
    if(!graph.exists(binding.owner)||!graph.exists(binding.target)) continue;
    if(!activeLayers.contains((u64(binding.owner)<<32)|binding.layer)) {
      nextBindings.push_back(binding);continue;
    }
  }
  // Pose de cada camada antes das camadas de cima e do repouso, para a
  // interrupção de transição (só existe enquanto uma transição roda).
  const auto captureLayers = [&](const AnimatorProperty &property, const AnimationValue &restValue) {
    const auto object = property.object;
    const usize width = restValue.size();
    SmallVector<std::pair<const LayerPose *, bool>, 4> emitted;
    for(const auto &seed:property.list) if(seed.sourceOwner!=kInvalidObject&&seed.coverage>0) {
      auto found=std::find_if(layerPoses_.begin(),layerPoses_.end(),[&](const CapturedLayer &e){return e.owner==seed.sourceOwner&&e.instance==seed.sourceInstance&&e.layer==seed.layer;});
      if(found==layerPoses_.end()) {layerPoses_.push_back({seed.sourceOwner,seed.sourceInstance,seed.layer,std::make_shared<LayerPose>()});found=std::prev(layerPoses_.end());}
      auto &channels=found->pose->channels;
      const LayerPose *capturedPose=found->pose.get();
      if(std::any_of(emitted.begin(),emitted.end(),[&](const auto &e){return e.first==capturedPose&&e.second==seed.additive;})) continue;
      std::vector<float> mixed(width,0);AnimationValue first;float sum=0;
      for(const auto &c:property.list) if(c.sourceOwner==seed.sourceOwner&&c.sourceInstance==seed.sourceInstance&&c.layer==seed.layer&&c.additive==seed.additive&&c.coverage>0) {
        AnimationValue v=c.value;
        if(v.size()!=width) continue;
        if(c.additive&&!c.relative) {
          std::vector<float> relative;
          if(!resources::relativeAnimationPose(property.path,v,c.reference.empty()?restValue:c.reference,relative)) continue;
          v.assign(relative.begin(),relative.end());
        }
        if(property.path==AnimationPath::Rotation) {
          if(first.empty()) first=v;
          float dot=0;for(usize k=0;k<width;++k) dot+=first[k]*v[k];if(dot<0) for(float &f:v) f=-f;
        }
        for(usize k=0;k<width;++k) mixed[k]+=v[k]*c.coverage;
        sum+=c.coverage;
      }
      if(sum>0) {for(float &f:mixed) f/=sum;channels.push_back({object,property.path,std::move(mixed),sum,seed.additive});emitted.push_back({capturedPose,seed.additive});}
    }
  };
  // --- Personagens inteiros em paralelo -----------------------------------
  // Como os Animation Jobs da Unity: cada job avalia Animators do começo ao fim
  // (amostragem, contribuições, mistura e pose) nos buffers do personagem; o
  // mundo só é escrito numa passada curta em série. Vale quando o quadro só
  // tem amostras de Animator sem propriedades tipadas, drivers ou pose
  // congelada, cada Animator é um bloco contíguo e os alvos não se cruzam;
  // fora disso o laço serial abaixo dá o mesmo resultado.
  const auto characterPath = [&]() -> bool {
    if (!parallelAnimationEnabled.load(std::memory_order_relaxed) || scratch.propertyCount != 0 ||
        !typedProperties.empty() || !driverRequests.empty() || external_.size() < 2) return false;
    auto &characters = scratch.characters;
    auto &prepared = scratch.prepared;
    prepared.assign(external_.size(), {});
    usize characterCount = 0, channels = 0;
    for (usize index = 0; index < external_.size(); ++index) {
      const auto &sample = external_[index];
      auto &ready = prepared[index];
      if (!(sample.weight < 0 || !std::isfinite(sample.weight) || !graph.exists(sample.owner))) {
        if (sample.frozen) return false;
        if (library_->findClip(sample.clip, ready.view)) {
          if (!ready.view.clip->properties.empty() || ready.view.clip->drivers) return false;
          auto found = std::find_if(externalTargets_.begin(), externalTargets_.end(), [&](const ExternalTargets &e) {
            return e.owner == sample.owner && e.clip == sample.clip;
          });
          if (found == externalTargets_.end()) found = externalTargets_.insert(externalTargets_.end(), {sample.owner, sample.clip, {}});
          auto &targets = found->targets;
          bool stale = targets.size() != ready.view.source->nodes.size();
          for (const auto target : targets) stale = stale || (target != kInvalidObject && !graph.exists(target));
          if (stale) resolveAnimationTargets(graph, sample.owner, *ready.view.source, targets);
          ready.targets = static_cast<usize>(found - externalTargets_.begin());
          ready.valid = true;
          channels += ready.view.clip->channels.size();
        }
      }
      if (characterCount == 0 || characters[characterCount - 1].owner != sample.owner) {
        for (usize other = 0; other < characterCount; ++other) if (characters[other].owner == sample.owner) return false;
        if (characters.size() <= characterCount) characters.emplace_back();
        auto &work = characters[characterCount++];
        work.owner = sample.owner;work.firstSample = index;work.channels = 0;
      }
      characters[characterCount - 1].lastSample = index + 1;
      if (ready.valid) characters[characterCount - 1].channels += ready.view.clip->channels.size();
    }
    if (characterCount < 2 || channels < 256) return false;
    // Alvos de um personagem não podem receber contribuição de outro.
    auto &targetCharacter = scratch.targetCharacter;
    targetCharacter.clear(channels);
    for (usize c = 0; c < characterCount; ++c)
      for (usize index = characters[c].firstSample; index < characters[c].lastSample; ++index) {
        if (!prepared[index].valid) continue;
        for (const auto target : externalTargets_[prepared[index].targets].targets) {
          if (target == kInvalidObject) continue;
          bool inserted = false;
          if (targetCharacter.insert(target, static_cast<u32>(c), inserted) != c) return false;
        }
      }

    // P1 (paralelo): amostragem e contribuições de cada personagem.
    const auto sampleCharacter = [&](AdvanceScratch::CharacterWork &work) {
      work.count = 0;work.playing = 0;work.diagnostic.clear();work.bindings.clear();
      work.index.clear(std::max<usize>(work.channels, work.properties.size()));
      work.bindingSeen.clear(work.channels);
      const auto contributeLocal = [&](const ExternalSample *source, ObjectId object, AnimationPath path, u32 layer, float weight,
                                       std::span<const float> value, bool additive = false, std::span<const float> reference = {},
                                       bool relative = false) {
        bool inserted = false;
        const u32 slot = work.index.insert((u64(object) << 2) | static_cast<u64>(path), static_cast<u32>(work.count), inserted);
        if (inserted) {
          if (work.count < work.properties.size()) {
            auto &reused = work.properties[work.count];
            reused.object = object; reused.path = path; reused.list.clear();
          } else {
            work.properties.push_back({object, path, {}});
          }
          ++work.count;
        }
        auto &entry = work.properties[slot].list.emplace_back();
        entry.layer = layer;
        entry.weight = source ? weight * source->layerWeight : weight;
        entry.value = value;
        entry.additive = additive;
        entry.reference = reference;
        entry.relative = relative;
        entry.sourceOwner = source ? source->controllerOwner : kInvalidObject;
        entry.sourceInstance = source ? source->instance : 0;
        entry.coverage = source ? weight : 0;
      };
      for (usize index = work.firstSample; index < work.lastSample; ++index) {
        const auto &ready = prepared[index];
        if (!ready.valid) continue;
        const auto &sample = external_[index];
        const auto &view = ready.view;
        std::unordered_map<u64,std::vector<float>> references;
        if(sample.additive&&sample.referenceClip.valid()) {
          AnimationClipView ref;
          if(!library_->findClip(sample.referenceClip,ref)||!ref.clip||!ref.source) {
            work.diagnostic="Pose de referência ausente: "+sample.referenceClip.text();
          } else {
            std::vector<ObjectId> refTargets;resolveAnimationTargets(graph,sample.owner,*ref.source,refTargets);
            for(const auto &channel:ref.clip->channels) {
              if(channel.node>=refTargets.size()||refTargets[channel.node]==kInvalidObject) continue;
              AnimationValue value(channel.components());
              if(sampleChannel(*ref.source,channel,std::clamp(sample.referenceTime,0.f,ref.clip->duration),value))
                references.emplace((u64(refTargets[channel.node])<<2)|static_cast<u64>(channel.path),std::vector<float>(value.begin(),value.end()));
              else work.diagnostic="Canal de referência não produz valor válido: "+sample.referenceClip.text()+" · nó "+std::to_string(channel.node);
            }
          }
        }
        const auto &targets = externalTargets_[ready.targets].targets;
        const auto masked = [&](ObjectId object) {
          if (sample.mask == kInvalidObject) return true;
          for (ObjectId o = object; o != kInvalidObject; ) {
            if (o == sample.mask) return true;
            const auto *e = graph.find(o);
            if (!e || o == graph.root()) break;
            o = e->parent;
          }
          return false;
        };
        ++work.playing;
        for (const auto &channel : view.clip->channels) {
          if (channel.node >= targets.size() || targets[channel.node] == kInvalidObject) continue;
          const ObjectId target = targets[channel.node];
          if (channel.path != AnimationPath::Weights && writable && !writable(target)) continue;
          AnimationValue value(channel.components());
          if(!sampleChannel(*view.source,channel,sample.time,value)) {
            work.diagnostic="Canal não produz valor válido: "+sample.clip.text()+" · nó "+std::to_string(channel.node);continue;
          }
          {
            const u64 key = (((u64(sample.owner) << 32) | sample.layer) * 0x9e3779b97f4a7c15ull) ^ ((u64(target) << 2) | static_cast<u64>(channel.path));
            bool inserted = false;
            work.bindingSeen.insert(key, 0, inserted);
            if (inserted || std::none_of(work.bindings.begin(), work.bindings.end(), [&](const ExternalBinding &b) {
                  return b.owner == sample.owner && b.layer == sample.layer && b.target == target && b.path == channel.path;
                }))
              work.bindings.push_back({sample.owner,sample.layer,target,channel.path,value.size()});
          }
          if(!masked(target)) {
            if(rest_.contains(target)) contributeLocal(&sample,target,channel.path,sample.layer,0,value);
            continue;
          }
          std::span<const float> ref;
          if(sample.additive&&sample.referenceClip.valid()) {
            const auto foundRef=references.find((u64(target)<<2)|static_cast<u64>(channel.path));
            if(foundRef==references.end()||foundRef->second.size()!=value.size()) {
              if(work.diagnostic.empty()) work.diagnostic="Referência sem canal compatível: objeto "+std::to_string(target);
              contributeLocal(&sample,target,channel.path,sample.layer,0,value);continue;
            }
            ref=foundRef->second;
          }
          contributeLocal(&sample,target,channel.path,sample.layer,sample.weight,value,sample.additive,ref);
        }
      }
      // Canais aposentados de uma camada ativa voltam ao repouso (peso zero).
      for(const auto &binding:externalBindings_) {
        if(binding.owner!=work.owner) continue;
        if(!graph.exists(binding.target)||!activeLayers.contains((u64(binding.owner)<<32)|binding.layer)) continue;
        if(work.index.find((u64(binding.target)<<2)|static_cast<u64>(binding.path))!=FlatIndex::Missing) continue;
        if(binding.path==AnimationPath::Weights||!writable||writable(binding.target))
          contributeLocal(nullptr,binding.target,binding.path,binding.layer,0,AnimationValue(binding.width,0.f));
      }
    };
    const auto forCharacters = [&](const auto &fn) {
      parallelAnimationRange(characterCount, 1, [&](usize first, usize last) {
        for (usize c = first; c < last; ++c) fn(characters[c]);
      });
    };
    forCharacters(sampleCharacter);

    // S1 (série): repouso; rest_ cresce aqui.
    if (scratch.restOwner != &rest_ || scratch.restGeneration != restGeneration_) {
      scratch.restCache.clear();
      for (auto &work : characters) work.restCache.clear();
      scratch.restOwner = &rest_;
      scratch.restGeneration = restGeneration_;
    }
    for (usize c = 0; c < characterCount; ++c) {
      auto &work = characters[c];
      for (const auto &binding : work.bindings) nextBindings.push_back(binding);
      playing_ += work.playing;
      if (!work.diagnostic.empty()) compositionDiagnostic_ = work.diagnostic;
      if (work.restCache.size() < work.count) work.restCache.resize(work.count, {kInvalidObject, nullptr});
      work.rests.resize(work.count);
      for (usize index = 0; index < work.count; ++index) {
        const auto object = work.properties[index].object;
        auto &cached = work.restCache[index];
        if (cached.first != object || !cached.second) cached = {object, &rest_[object]};
        auto &rest = *cached.second;
        if (!rest.hasTransform || !rest.hasWeights) {
          const auto *entity = graph.find(object);
          if (!rest.hasTransform && entity) {rest.transform = entity->transform;rest.hasTransform = true;}
          if (!rest.hasWeights && entity)
            if (const auto *mesh = static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor))) {
              rest.weights = mesh->blendShapeWeights;rest.hasWeights = true;
            }
        }
        if (!rest.decomposed && rest.hasTransform) {decompose(rest.transform, rest.t, rest.q, rest.s);rest.decomposed = true;}
        work.rests[index] = &rest;
      }
    }

    // P2 (paralelo): mistura, poses do personagem e TRS → Euler.
    forCharacters([&](AdvanceScratch::CharacterWork &work) {
      work.restValues.resize(work.count);work.results.resize(work.count);work.outcomes.resize(work.count);
      composeProperties(work.properties.data(), work.rests.data(), work.restValues.data(), work.results.data(),
                        work.outcomes.data(), 0, work.count);
      work.poses.clear();
      work.poseIndex.clear(work.count);
      for (usize index = 0; index < work.count; ++index) {
        const auto &property = work.properties[index];
        if (!(work.outcomes[index] & ComposedOutcome) || property.path == AnimationPath::Weights) continue;
        const auto &result = work.results[index];
        bool insertedPose = false;
        const u32 slot = work.poseIndex.insert(property.object, static_cast<u32>(work.poses.size()), insertedPose);
        if (insertedPose) work.poses.emplace_back(property.object, AnimatorPose{});
        auto &pose = work.poses[slot].second;
        if (property.path == AnimationPath::Translation) {std::copy(result.begin(), result.end(), pose.t);pose.written |= 1;}
        else if (property.path == AnimationPath::Rotation) {std::copy(result.begin(), result.end(), pose.q);pose.written |= 2;}
        else {std::copy(result.begin(), result.end(), pose.s);pose.written |= 4;}
        pose.changed = true;
      }
      work.writes.resize(work.poses.size());
      convertPoses(graph, work.poses.data(), work.writes.data(), 0, work.poses.size());
    });

    // S2 (série): captura de camada, diagnósticos, blend shapes e o grafo.
    for (usize c = 0; c < characterCount; ++c) {
      auto &work = characters[c];
      for (usize index = 0; index < work.count; ++index) {
        const auto &property = work.properties[index];
        const auto object = property.object;
        captureLayers(property, work.restValues[index]);
        const u8 outcome = work.outcomes[index];
        if (outcome & ReferenceInvalidOutcome)
          compositionDiagnostic_="Referência inválida (escala zero ou quaternion): objeto "+std::to_string(object);
        if (!(outcome & ComposedOutcome)) continue;
        if (outcome & AdditiveInvalidOutcome)
          compositionDiagnostic_="Composição aditiva inválida: objeto "+std::to_string(object);
        if (property.path != AnimationPath::Weights) continue;
        const auto &result = work.results[index];
        const usize width = work.restValues[index].size();
        auto *components = graph.editComponents(object);
        auto *mesh = components ? static_cast<scene::SkinnedMesh *>(components->edit(scene::SkinnedMesh::descriptor)) : nullptr;
        if (!mesh) continue;
        if (mesh->blendShapeWeights.size() < width) mesh->blendShapeWeights.resize(width, 0.0f);
        bool changed = false;
        for (usize i = 0; i < width; ++i) {
          const float percent = std::clamp(result[i] * PercentPerUnit, -scene::SkinnedMesh::MaximumBlendShapeWeight,
                                           scene::SkinnedMesh::MaximumBlendShapeWeight);
          changed = changed || mesh->blendShapeWeights[i] != percent;
          mesh->blendShapeWeights[i] = percent;
        }
        if (changed) ++posed_;
      }
      const u8 stride = external_[work.firstSample].updateStride;
      for (usize index = 0; index < work.poses.size(); ++index) {
        const auto object = work.poses[index].first;
        const auto &out = work.writes[index];
        if (out.state == 2) compositionDiagnostic_="Pose TRS não representável: objeto "+std::to_string(object);
        else if (out.state == 1 || (stride > 1 && out.state == 0 && work.poses[index].second.changed))
          writePose(object, out.state == 1 ? out.transform : graph.find(object)->transform, stride);
      }
    }
    return true;
  };
  if (!characterPath()) {
  // Amostras do Animator: mesmo caminho de mistura, com máscara por subárvore.
  for (const auto &sample : external_) {
    if (sample.weight < 0 || !std::isfinite(sample.weight) || !graph.exists(sample.owner)) continue;
    contributionSource=&sample;
    if(sample.frozen) {
      for(const auto &property:sample.frozen->properties) {
        if(!graph.exists(property.object)||(sample.mask&&property.object!=sample.mask&&!graph.isDescendantOf(property.object,sample.mask)))continue;
        auto contribution=property;contribution.weight=sample.weight*property.coverage;contribution.layer=sample.layer;
        contribution.sourceOwner=sample.controllerOwner;contribution.sourceInstance=sample.instance;
        typedProperties.push_back(std::move(contribution));
      }
      for(const auto &channel:sample.frozen->channels) {
        if(!graph.exists(channel.target)||(channel.path!=AnimationPath::Weights&&writable&&!writable(channel.target))) continue;
        bool allowed=sample.mask==kInvalidObject;
        for(ObjectId id=channel.target;!allowed&&id!=kInvalidObject;) {
          if(id==sample.mask) {allowed=true;break;}
          const auto *entity=graph.find(id);if(!entity||id==graph.root()) break;id=entity->parent;
        }
        if(!allowed) continue;
        if(firstBinding(sample.owner,sample.layer,channel.target,channel.path))
          nextBindings.push_back({sample.owner,sample.layer,channel.target,channel.path,channel.value.size()});
        contribute(channel.target,channel.path,sample.layer,sample.weight*channel.coverage,channel.value,channel.additive,{},channel.additive);
      }
      continue;
    }
    AnimationClipView view;
    if (!library_->findClip(sample.clip, view)) continue;
    std::unordered_map<u64,std::vector<float>> references;
    if(sample.additive&&sample.referenceClip.valid()) {
      AnimationClipView ref;
      if(!library_->findClip(sample.referenceClip,ref)||!ref.clip||!ref.source) {
        compositionDiagnostic_="Pose de referência ausente: "+sample.referenceClip.text();
      } else {
      std::vector<ObjectId> refTargets;resolveAnimationTargets(graph,sample.owner,*ref.source,refTargets);
      for(const auto &channel:ref.clip->channels) {
        if(channel.node>=refTargets.size()||refTargets[channel.node]==kInvalidObject) continue;
        AnimationValue value(channel.components());
        if(sampleChannel(*ref.source,channel,std::clamp(sample.referenceTime,0.f,ref.clip->duration),value))
          references.emplace((u64(refTargets[channel.node])<<2)|static_cast<u64>(channel.path),std::vector<float>(value.begin(),value.end()));
        else compositionDiagnostic_="Canal de referência não produz valor válido: "+sample.referenceClip.text()+" · nó "+std::to_string(channel.node);
      }
      }
    }
    auto found = std::find_if(externalTargets_.begin(), externalTargets_.end(), [&](const ExternalTargets &e) {
      return e.owner == sample.owner && e.clip == sample.clip;
    });
    if (found == externalTargets_.end()) found = externalTargets_.insert(externalTargets_.end(), {sample.owner, sample.clip, {}});
    auto &targets = found->targets;
    bool stale = targets.size() != view.source->nodes.size();
    for (const auto target : targets) stale = stale || (target != kInvalidObject && !graph.exists(target));
    if (stale) resolveAnimationTargets(graph, sample.owner, *view.source, targets);
    contributeTyped(*view.clip,targets,sample.time,sample.weight*sample.layerWeight,sample.layer,sample.additive,sample.mask);
    const auto masked = [&](ObjectId object) {
      if (sample.mask == kInvalidObject) return true;
      for (ObjectId o = object; o != kInvalidObject; ) {
        if (o == sample.mask) return true;
        const auto *e = graph.find(o);
        if (!e || o == graph.root()) break;
        o = e->parent;
      }
      return false;
    };
    ++playing_;
    for (const auto &channel : view.clip->channels) {
      if (channel.node >= targets.size() || targets[channel.node] == kInvalidObject) continue;
      const ObjectId target = targets[channel.node];
      if (channel.path != AnimationPath::Weights && writable && !writable(target)) continue;
      AnimationValue value(channel.components());
      if(!sampleChannel(*view.source,channel,sample.time,value)) {
        compositionDiagnostic_="Canal não produz valor válido: "+sample.clip.text()+" · nó "+std::to_string(channel.node);continue;
      }
      if(firstBinding(sample.owner,sample.layer,target,channel.path))
        nextBindings.push_back({sample.owner,sample.layer,target,channel.path,value.size()});
      if(!masked(target)) {
        if(rest_.contains(target)) contribute(target,channel.path,sample.layer,0,value);
        continue;
      }
      std::span<const float> ref;
      if(sample.additive&&sample.referenceClip.valid()) {
        const auto foundRef=references.find((u64(target)<<2)|static_cast<u64>(channel.path));
        if(foundRef==references.end()||foundRef->second.size()!=value.size()) {
          if(compositionDiagnostic_.empty()) compositionDiagnostic_="Referência sem canal compatível: objeto "+std::to_string(target);
          contribute(target,channel.path,sample.layer,0,value);continue;
        }
        ref=foundRef->second;
      }
      contribute(target,channel.path,sample.layer,sample.weight,value,sample.additive,ref);
    }
  }
  contributionSource=nullptr;
  for(const auto &binding:externalBindings_) {
    if(!graph.exists(binding.target)||!activeLayers.contains((u64(binding.owner)<<32)|binding.layer)) continue;
    if(hasProperty((u64(binding.target)<<2)|static_cast<u64>(binding.path))) continue;
    if(binding.path==AnimationPath::Weights||!writable||writable(binding.target))
      contribute(binding.target,binding.path,binding.layer,0,AnimationValue(binding.width,0.f));
  }
  }
  bool anyStride = false;
  for (const auto &sample : external_) anyStride = anyStride || sample.updateStride > 1;
  if (anyStride) {
    scratch.strideTargets.clear(256);
    for (const auto &sample : external_) {
      if (sample.updateStride <= 1) continue;
      const auto found = std::find_if(externalTargets_.begin(), externalTargets_.end(), [&](const ExternalTargets &e) {
        return e.owner == sample.owner && e.clip == sample.clip;
      });
      if (found == externalTargets_.end()) continue;
      for (const auto target : found->targets) if (target != kInvalidObject) {
        bool inserted = false;
        scratch.strideTargets.insert(target, sample.updateStride, inserted);
      }
    }
  }
  external_.clear();
  // Troca de buffers: os dois vetores mantêm a capacidade entre quadros.
  externalBindings_.swap(nextBindings);
  if(!typedValid)return false;
  std::vector<AnimationPropertyContribution> driven;
  // O misturador limpa seu erro; diagnosticos TRS ja reportados neste quadro
  // (referencia aditiva parcial, canal invalido) nao podem ser apagados.
  std::string typedError;
  if(!propertyMixer_.apply(graph,typedProperties,typedError,[&](auto &writes,const auto &baselines,std::string &error) {
    return evaluateAnimationDriverRequests(graph,driverRequests,driverGui_,driverParameter_,writes,baselines,driven,error);
  })) {compositionDiagnostic_=typedError;return false;}
  for(const auto &driver:driven)std::erase_if(typedProperties,[&](const auto &property) {
    return property.sourceOwner==driver.sourceOwner&&property.sourceInstance==driver.sourceInstance&&property.layer==driver.layer&&
      property.object==driver.object&&property.address==driver.address;
  });
  typedProperties.insert(typedProperties.end(),driven.begin(),driven.end());
  if(propertyPublication_&&!typedProperties.empty())propertyPublication_(typedProperties);
  for(const auto &property:typedProperties)if(property.sourceOwner!=kInvalidObject&&property.coverage>0) {
    auto found=std::find_if(layerPoses_.begin(),layerPoses_.end(),[&](const auto &entry){return entry.owner==property.sourceOwner&&entry.instance==property.sourceInstance&&entry.layer==property.layer;});
    if(found==layerPoses_.end()){layerPoses_.push_back({property.sourceOwner,property.sourceInstance,property.layer,std::make_shared<LayerPose>()});found=std::prev(layerPoses_.end());}
    found->pose->properties.push_back(property);
  }

  // Repouso: a pose (e os pesos) do nó quando a animação o tocou primeiro.
  if (scratch.restOwner != &rest_ || scratch.restGeneration != restGeneration_) {
    scratch.restCache.clear();
    scratch.restOwner = &rest_;
    scratch.restGeneration = restGeneration_;
  }
  if (scratch.restCache.size() < scratch.propertyCount)
    scratch.restCache.resize(scratch.propertyCount, {kInvalidObject, nullptr});
  const auto restOf = [&](ObjectId object, usize slot) -> Rest & {
    auto &cached = scratch.restCache[slot];
    if (cached.first != object || !cached.second) cached = {object, &rest_[object]};
    auto &rest = *cached.second;
    if (rest.hasTransform && rest.hasWeights) return rest;
    const auto *entity = graph.find(object);
    if (!rest.hasTransform && entity) {
      rest.transform = entity->transform;
      rest.hasTransform = true;
    }
    if (!rest.hasWeights && entity)
      if (const auto *mesh = static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor))) {
        rest.weights = mesh->blendShapeWeights;
        rest.hasWeights = true;
      }
    return rest;
  };

  // Composição por propriedade (CPU, 24/10/2026: 13% da thread do jogo na Vila
  // com 11 personagens). Três fases, como os Animation Jobs da Unity avaliam
  // Animators em paralelo e só escrevem as transformações no fim:
  //  1. série: repouso de cada propriedade (rest_ muda de tamanho aqui);
  //  2. paralelo: a mistura pura de camadas, aditivas e normalização, cada
  //     propriedade no seu próprio buffer;
  //  3. série: captura de camada para interrupção, pesos de blend shape, poses
  //     e diagnósticos, na mesma ordem do laço único que existia antes.
  const usize propertyCount = scratch.propertyCount;
  auto &restRefs = scratch.restRefs;
  restRefs.resize(propertyCount);
  for (usize index = 0; index < propertyCount; ++index) {
    auto &rest = restOf(properties[index].object, index);
    if (!rest.decomposed && rest.hasTransform) {
      decompose(rest.transform, rest.t, rest.q, rest.s);
      rest.decomposed = true;
    }
    restRefs[index] = &rest;
  }
  auto &restValues = scratch.restValues;
  auto &results = scratch.results;
  auto &outcomes = scratch.outcomes;
  restValues.resize(propertyCount);
  results.resize(propertyCount);
  outcomes.resize(propertyCount);
  const auto composeRange = [&](usize first, usize last) {
    composeProperties(properties.data(), restRefs.data(), restValues.data(), results.data(), outcomes.data(), first, last);
  };
  parallelAnimationRange(propertyCount, 48, composeRange);

  auto &poses = scratch.poses;
  poses.clear();
  scratch.poseIndex.clear(propertyCount);
  for (usize propertyIndexValue = 0; propertyIndexValue < propertyCount; ++propertyIndexValue) {
    auto &property = properties[propertyIndexValue];
    const auto object = property.object;
    const AnimationValue &restValue = restValues[propertyIndexValue];
    const usize width = restValue.size();
    // Cache this layer's own composition, including sparse-channel coverage,
    // before higher layers or the rest pose affect it. Frozen deltas stay deltas.
    // (objeto, caminho) é único por propriedade: basta lembrar o que esta
    // propriedade já emitiu, em vez de varrer todos os canais da camada.
    captureLayers(property, restValue);
    const u8 outcome = outcomes[propertyIndexValue];
    if (outcome & ReferenceInvalidOutcome)
      compositionDiagnostic_="Referência inválida (escala zero ou quaternion): objeto "+std::to_string(object);
    if (!(outcome & ComposedOutcome)) continue;
    if (outcome & AdditiveInvalidOutcome)
      compositionDiagnostic_="Composição aditiva inválida: objeto "+std::to_string(object);
    const AnimationValue &result = results[propertyIndexValue];
    if (property.path == AnimationPath::Weights) {
      auto *components = graph.editComponents(object);
      auto *mesh = components ? static_cast<scene::SkinnedMesh *>(components->edit(scene::SkinnedMesh::descriptor)) : nullptr;
      if (!mesh) continue;
      if (mesh->blendShapeWeights.size() < width) mesh->blendShapeWeights.resize(width, 0.0f);
      bool changed = false;
      for (usize i = 0; i < width; ++i) {
        const float percent = std::clamp(result[i] * PercentPerUnit, -scene::SkinnedMesh::MaximumBlendShapeWeight,
                                         scene::SkinnedMesh::MaximumBlendShapeWeight);
        changed = changed || mesh->blendShapeWeights[i] != percent;
        mesh->blendShapeWeights[i] = percent;
      }
      if (changed) ++posed_;
      continue;
    }
    bool insertedPose = false;
    const u32 poseSlot = scratch.poseIndex.insert(object, static_cast<u32>(poses.size()), insertedPose);
    if (insertedPose) poses.emplace_back(object, AnimatorPose{});
    auto &pose = poses[poseSlot].second;
    if (property.path == AnimationPath::Translation) {std::copy(result.begin(), result.end(), pose.t);pose.written |= 1;}
    else if (property.path == AnimationPath::Rotation) {std::copy(result.begin(), result.end(), pose.q);pose.written |= 2;}
    else {std::copy(result.begin(), result.end(), pose.s);pose.written |= 4;}
    pose.changed = true;
  }
  // TRS → Transform de Euler por osso em paralelo (só leitura do grafo); a
  // escrita no grafo fica em série, na ordem das poses.
  auto &transforms = scratch.transforms;
  transforms.resize(poses.size());
  const auto convertRange = [&](usize first, usize last) {
    convertPoses(graph, poses.data(), transforms.data(), first, last);
  };
  parallelAnimationRange(poses.size(), 24, convertRange);
  for (usize index = 0; index < poses.size(); ++index) {
    const auto object = poses[index].first;
    const auto &out = transforms[index];
    const u32 found = anyStride ? scratch.strideTargets.find(object) : FlatIndex::Missing;
    const u8 stride = found == FlatIndex::Missing ? 1 : static_cast<u8>(found);
    if (out.state == 2) compositionDiagnostic_="Pose TRS não representável: objeto "+std::to_string(object);
    else if (out.state == 1 || (stride > 1 && out.state == 0 && poses[index].second.changed))
      writePose(object, out.state == 1 ? out.transform : graph.find(object)->transform, stride);
  }
  // Quadros sem avaliação: cada osso anda mais um passo até a pose avaliada.
  for (auto &rate : interpolations_)
    if (rate.steps > 1 && rate.frame != frame_ && rate.step < rate.steps) {++rate.step;rate.frame = frame_;blendWrite(rate);}
  std::erase_if(interpolations_, [](const RateInterpolation &rate) {return rate.steps <= 1 || rate.step >= rate.steps;});
  return true;
}

} // namespace ae::runtime
