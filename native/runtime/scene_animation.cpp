#include "runtime/scene_animation.h"

#include "runtime/transform_math.h"
#include "scene/animation.h"
#include "scene/import_link.h"

#include <cmath>

namespace ae::runtime {

bool resolveAnimationTargets(const SceneGraph &graph, ObjectId owner, const SourceAnimations &source,
                             std::vector<ObjectId> &targets) {
  targets.assign(source.nodes.size(), kInvalidObject);
  const auto *object = graph.find(owner);
  const auto *link = object ? scene::importLink(object->components) : nullptr;
  if (!link || !link->source.valid() || !link->instance.valid()) return false;
  std::unordered_map<resources::AssetGuid, u32, resources::AssetGuidHash> index;
  for (u32 i = 0; i < source.nodes.size(); ++i) index.emplace(source.nodes[i], i);
  std::vector<ObjectId> subtree;
  graph.collectSubtree(owner, subtree);
  for (const auto id : subtree) {
    const auto *candidate = graph.find(id);
    const auto *other = candidate ? scene::importLink(candidate->components) : nullptr;
    // A parte artificial de uma primitiva (representação legada) não é nó.
    if (!other || other->instance != link->instance || other->source != link->source || !other->node.valid() ||
        other->primitive >= 0)
      continue;
    const auto found = index.find(other->node);
    if (found != index.end() && targets[found->second] == kInvalidObject) targets[found->second] = id;
  }
  return true;
}

u32 applyAnimationPose(SceneGraph &graph, const resources::AnimationClip &clip, float time,
                       const std::vector<ObjectId> &targets, const std::function<bool(ObjectId)> &writable) {
  struct Pose {
    ObjectId id = kInvalidObject;
    float t[3], q[4], s[3];
    bool changed = false;
  };
  std::vector<Pose> poses;
  const auto poseOf = [&](ObjectId id) -> Pose * {
    for (auto &pose : poses) if (pose.id == id) return &pose;
    const auto *object = graph.find(id);
    if (!object) return nullptr;
    Pose pose;
    pose.id = id;
    std::copy(object->transform.position, object->transform.position + 3, pose.t);
    std::copy(object->transform.scale, object->transform.scale + 3, pose.s);
    transformRotationQuaternion(object->transform, pose.q);
    poses.push_back(pose);
    return &poses.back();
  };
  for (const auto &channel : clip.channels) {
    if (channel.node >= targets.size() || targets[channel.node] == kInvalidObject) continue;
    const ObjectId id = targets[channel.node];
    if (writable && !writable(id)) continue;
    float value[4];
    if (!resources::sampleAnimationChannel(channel, time, value)) continue;
    auto *pose = poseOf(id);
    if (!pose) continue;
    switch (channel.path) {
    case resources::AnimationPath::Translation: std::copy(value, value + 3, pose->t); break;
    case resources::AnimationPath::Rotation: std::copy(value, value + 4, pose->q); break;
    case resources::AnimationPath::Scale: std::copy(value, value + 3, pose->s); break;
    }
    pose->changed = true;
  }
  u32 written = 0;
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (const auto &pose : poses) {
    if (!pose.changed) continue;
    float local[16];
    resources::composeTransform(pose.t, pose.q, pose.s, local);
    Transform transform;
    // Escala negativa ou nula no clipe não cabe em TRS de Euler: o nó mantém a
    // pose anterior em vez de receber uma decomposição inventada.
    if (!localTransformForWorld(local, identity, transform)) continue;
    const auto *object = graph.find(pose.id);
    if (!object) continue;
    const auto &current = object->transform;
    if (std::equal(current.position, current.position + 3, transform.position) &&
        std::equal(current.rotationDegrees, current.rotationDegrees + 3, transform.rotationDegrees) &&
        std::equal(current.scale, current.scale + 3, transform.scale))
      continue;
    if (graph.setTransform(pose.id, transform)) ++written;
  }
  return written;
}

void SceneAnimator::reset() {
  started_.clear();
  targets_.clear();
  playing_ = posed_ = 0;
}

bool SceneAnimator::advance(SceneGraph &graph, const AnimationLibrary &library, float delta,
                            const std::function<bool(ObjectId)> &writable) {
  playing_ = posed_ = 0;
  if (!std::isfinite(delta) || delta < 0) return false;
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (const auto id : ids) {
    const auto *object = graph.find(id);
    if (!object || !graph.activeInHierarchy(id)) continue;
    const auto *component = static_cast<const scene::Animation *>(object->components.find(scene::Animation::descriptor));
    const auto *link = scene::importLink(object->components);
    if (!component || !component->valid()) continue;
    auto state = *component;
    if (started_.insert(component->instanceId()).second && state.playAutomatically) state.playing = true;
    const auto *source = link ? library.animations(link->source) : nullptr;
    const resources::AnimationClip *clip =
        source && state.clipIndex() < source->clips.size() ? &source->clips[state.clipIndex()] : nullptr;
    bool finished = false;
    float local = 0;
    if (state.playing && clip) {
      state.time += delta * state.speed;
      if (!std::isfinite(state.time) || std::fabs(state.time) > 1000000) state.time = 0;
      local = resources::wrapAnimationTime(state.time, clip->duration, state.wrapMode, finished);
      // Tocando de trás para frente, Once termina ao passar do início.
      if (state.wrapMode == resources::AnimationWrapMode::Once && state.time < 0) {
        finished = true;
        local = 0;
      }
      ++playing_;
    }
    if (state.playing && clip) {
      auto &targets = targets_[id];
      bool stale = targets.size() != source->nodes.size();
      for (const auto target : targets) stale = stale || (target != kInvalidObject && !graph.exists(target));
      if (stale) resolveAnimationTargets(graph, id, *source, targets);
      posed_ += applyAnimationPose(graph, *clip, local, targets, writable);
    }
    // WrapMode.Once da Unity: no fim o clipe para e o tempo volta ao início;
    // a última pose amostrada fica.
    if (finished) {
      state.playing = false;
      state.time = 0;
    }
    if (state.playing != component->playing || state.time != component->time) {
      auto *components = graph.editComponents(id);
      auto *target = components ? components->findInstance(component->instanceId()) : nullptr;
      if (!target || &target->type() != &scene::Animation::descriptor) continue;
      auto &value = static_cast<scene::Animation &>(*components->editInstance(component->instanceId()));
      value.playing = state.playing;
      value.time = state.time;
    }
  }
  return true;
}

} // namespace ae::runtime
