#include "runtime/scene_animation.h"

#include "runtime/transform_math.h"
#include "scene/animation.h"
#include "scene/import_link.h"
#include "scene/skinned_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>

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
} // namespace

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
  // 2) Sem vínculo: nome único na subárvore (retarget por nome).
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
  graph_ = nullptr;
  library_ = nullptr;
  players_.clear();
  rest_.clear();
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
  target->enabled = value.enabled;
  target->time = value.time;
  target->speed = value.speed;
  target->weight = target->targetWeight = value.weight;
  target->fadeRate = 0;
  target->stopAtZero = false;
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

bool SceneAnimator::advance(float delta, const std::function<bool(ObjectId)> &writable) {
  playing_ = posed_ = 0;
  if (!graph_ || !library_ || !std::isfinite(delta) || delta < 0) return graph_ == nullptr;
  auto &graph = *graph_;

  // Componentes vivos; um jogador cujo componente sumiu sai junto.
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::vector<std::pair<ObjectId, u64>> live;
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) continue;
    for (usize i = 0; i < object->components.size(); ++i) {
      const auto *value = object->components.at(i);
      if (&value->type() == &scene::Animation::descriptor) live.emplace_back(id, value->instanceId());
    }
  }
  std::erase_if(players_, [&](const Player &p) {
    return std::find(live.begin(), live.end(), std::make_pair(p.owner, p.instance)) == live.end();
  });

  // Uma entrada por propriedade animada neste quadro.
  struct Contribution {
    u32 layer;
    float weight;
    std::vector<float> value;
  };
  struct Property {
    ObjectId object;
    AnimationPath path;
    std::vector<Contribution> list;
  };
  std::vector<Property> properties;
  std::unordered_map<u64, usize> propertyIndex;
  const auto contribute = [&](ObjectId object, AnimationPath path, u32 layer, float weight, std::vector<float> value) {
    const u64 key = (u64(object) << 2) | static_cast<u64>(path);
    auto found = propertyIndex.find(key);
    if (found == propertyIndex.end()) {
      found = propertyIndex.emplace(key, properties.size()).first;
      properties.push_back({object, path, {}});
    }
    properties[found->second].list.push_back({layer, weight, std::move(value)});
  };

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
      state.time += delta * state.speed;
      if (!std::isfinite(state.time) || std::fabs(state.time) > 1e6f) state.time = 0;
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
      for (const auto &channel : view.clip->channels) {
        if (channel.node >= targets.size() || targets[channel.node] == kInvalidObject) continue;
        const ObjectId target = targets[channel.node];
        if (channel.path != AnimationPath::Weights && writable && !writable(target)) continue;
        std::vector<float> value(channel.components());
        if (!resources::sampleAnimationChannel(channel, local, value)) continue;
        contribute(target, channel.path, state.layer, state.weight, std::move(value));
      }
    }
  }

  // Repouso: a pose (e os pesos) do nó quando a animação o tocou primeiro.
  const auto restOf = [&](ObjectId object) -> Rest & {
    auto &rest = rest_[object];
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

  struct Pose {
    float t[3], q[4], s[3];
    bool changed = false;
  };
  std::unordered_map<ObjectId, Pose> poses;
  for (auto &property : properties) {
    const auto object = property.object;
    auto &rest = restOf(object);
    std::vector<float> restValue;
    float restT[3], restQ[4], restS[3];
    decompose(rest.transform, restT, restQ, restS);
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
    std::vector<float> result(width, 0.0f);
    const bool rotation = property.path == AnimationPath::Rotation;
    const float *reference = nullptr;
    const auto accumulate = [&](const std::vector<float> &value, float weight) {
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
    std::vector<u32> layers;
    for (const auto &c : property.list) if (std::find(layers.begin(), layers.end(), c.layer) == layers.end()) layers.push_back(c.layer);
    std::sort(layers.begin(), layers.end(), std::greater<>());
    float remaining = 1;
    for (const auto layer : layers) {
      float sum = 0;
      for (const auto &c : property.list) if (c.layer == layer) sum += c.weight;
      if (sum <= 0) continue;
      const float take = std::min(sum, 1.0f) * remaining;
      for (const auto &c : property.list) if (c.layer == layer) accumulate(c.value, c.weight * take / sum);
      remaining -= take;
      if (remaining <= 1e-6f) break;
    }
    if (remaining > 1e-6f) accumulate(restValue, remaining);
    if (rotation) {
      const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] + result[2] * result[2] + result[3] * result[3]);
      if (!(length > 1e-12f)) continue;
      for (auto &v : result) v /= length;
    }
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
    auto found = poses.find(object);
    if (found == poses.end()) {
      Pose pose;
      decompose(graph.find(object)->transform, pose.t, pose.q, pose.s);
      found = poses.emplace(object, pose).first;
    }
    auto &pose = found->second;
    if (property.path == AnimationPath::Translation) std::copy(result.begin(), result.end(), pose.t);
    else if (property.path == AnimationPath::Rotation) std::copy(result.begin(), result.end(), pose.q);
    else std::copy(result.begin(), result.end(), pose.s);
    pose.changed = true;
  }
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (const auto &[object, pose] : poses) {
    if (!pose.changed) continue;
    float local[16];
    resources::composeTransform(pose.t, pose.q, pose.s, local);
    Transform transform;
    // Escala negativa ou nula no clipe não cabe em TRS de Euler: o nó mantém a
    // pose anterior em vez de receber uma decomposição inventada.
    if (!localTransformForWorld(local, identity, transform)) continue;
    const auto &current = graph.find(object)->transform;
    if (std::equal(current.position, current.position + 3, transform.position) &&
        std::equal(current.rotationDegrees, current.rotationDegrees + 3, transform.rotationDegrees) &&
        std::equal(current.scale, current.scale + 3, transform.scale))
      continue;
    if (graph.setTransform(object, transform)) ++posed_;
  }
  return true;
}

} // namespace ae::runtime
