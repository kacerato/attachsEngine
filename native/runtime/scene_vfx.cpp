#include "runtime/scene_vfx.h"

#include "core/rotation_math.h"
#include "runtime/transform_math.h"
#include "scene/audio.h"
#include "scene/particle_system.h"
#include "scene/vfx_behaviors.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ae::runtime {
namespace {
using Quaternion = float[4];

void rotate(const float (&q)[4], const float (&v)[3], float (&out)[3]) {
  const float t[3]{2 * (q[1] * v[2] - q[2] * v[1]), 2 * (q[2] * v[0] - q[0] * v[2]), 2 * (q[0] * v[1] - q[1] * v[0])};
  out[0] = v[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]);
  out[1] = v[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]);
  out[2] = v[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0]);
}
void multiply(const float (&a)[4], const float (&b)[4], float (&out)[4]) {
  const float r[4]{a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                   a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
  std::copy(r, r + 4, out);
}
float axisSign(scene::VfxForwardAxis axis) { return axis == scene::VfxForwardAxis::NegativeZ ? -1.0f : 1.0f; }
// Pose de mundo como posição e quatérnio.
bool worldPose(const GameWorld &world, const ObjectHandle &handle, float (&position)[3], float (&rotation)[4]) {
  Transform t;
  if (world.worldTransform(handle, t) != WorldStatus::Ok) return false;
  std::copy(t.position, t.position + 3, position);
  transformRotationQuaternion(t, rotation);
  return true;
}
// Quatérnio cuja frente (+Z ou −Z local, pelo eixo) aponta para `direction`.
void lookRotation(const float (&direction)[3], scene::VfxForwardAxis axis, float (&q)[4]) {
  const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
  float z[3]{0, 0, 1};
  if (length > 1e-6f) for (u32 k = 0; k < 3; ++k) z[k] = direction[k] / length * axisSign(axis);
  const float reference[3]{std::abs(z[1]) < .99f ? 0.0f : 1.0f, std::abs(z[1]) < .99f ? 1.0f : 0.0f, 0.0f};
  float x[3]{reference[1] * z[2] - reference[2] * z[1], reference[2] * z[0] - reference[0] * z[2], reference[0] * z[1] - reference[1] * z[0]};
  const float xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
  for (auto &c : x) c /= xl > 1e-6f ? xl : 1.0f;
  const float y[3]{z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
  // Matriz de colunas (x, y, z) para quatérnio.
  const float trace = x[0] + y[1] + z[2];
  if (trace > 0) {
    const float s = std::sqrt(trace + 1) * 2;
    q[3] = .25f * s; q[0] = (y[2] - z[1]) / s; q[1] = (z[0] - x[2]) / s; q[2] = (x[1] - y[0]) / s;
  } else if (x[0] > y[1] && x[0] > z[2]) {
    const float s = std::sqrt(1 + x[0] - y[1] - z[2]) * 2;
    q[3] = (y[2] - z[1]) / s; q[0] = .25f * s; q[1] = (y[0] + x[1]) / s; q[2] = (z[0] + x[2]) / s;
  } else if (y[1] > z[2]) {
    const float s = std::sqrt(1 + y[1] - x[0] - z[2]) * 2;
    q[3] = (z[0] - x[2]) / s; q[0] = (y[0] + x[1]) / s; q[1] = .25f * s; q[2] = (z[1] + y[2]) / s;
  } else {
    const float s = std::sqrt(1 + z[2] - x[0] - y[1]) * 2;
    q[3] = (x[1] - y[0]) / s; q[0] = (z[0] + x[2]) / s; q[1] = (z[1] + y[2]) / s; q[2] = .25f * s;
  }
  normalizeRotationQuaternion(q);
}
bool nonzero(const float (&v)[3]) { return v[0] != 0 || v[1] != 0 || v[2] != 0; }
// Consulta da frente: esfera com raio, raio sem; ignora o próprio objeto.
bool cast(const ScenePhysics &physics, ObjectId self, const float (&origin)[3], const float (&translation)[3], float radius,
          QueryHit &hit) {
  if (!validPhysicsQueryRay(origin, translation)) return false;
  QueryFilter filter;
  filter.ignore = self;
  if (radius > 0) {
    QueryShapeDesc shape;
    shape.kind = QueryShapeKind::Sphere;
    shape.radius = radius;
    if (validPhysicsQueryShape(shape)) return physics.shapeCast(shape, origin, translation, filter, hit);
  }
  return physics.rayCast(origin, translation, filter, hit);
}
// Duração das partículas que mandam no efeito: a da raiz, senão a do primeiro
// filho (o mesmo que os scripts de projétil dos pacotes leem).
double effectDuration(const SceneGraph &graph, ObjectId root) {
  const auto durationOf = [&](ObjectId id) -> double {
    const auto *object = graph.find(id);
    const auto *value = object ? object->components.find(scene::ParticleSystem::descriptor) : nullptr;
    return value ? scene::particleSystem(*value).duration : -1.0;
  };
  if (const double own = durationOf(root); own > 0) return own;
  const auto *object = graph.find(root);
  if (object) for (const auto child : graph.childrenOf(root)) if (const double d = durationOf(child); d > 0) return d;
  return 2.0;
}
} // namespace

void SceneVfx::reset() {
  projectiles_.clear();
  sounds_.clear();
  destroying_.clear();
  prefabs_.clear();
  report_ = {};
}

ObjectHandle SceneVfx::spawn(GameWorld &world, resources::AssetGuid asset, const ObjectHandle &parent, const float (&position)[3],
                             const float (&rotation)[4], double lifetime) {
  if (!asset.valid() || !loader_) return {};
  auto cached = prefabs_.find(asset);
  if (cached == prefabs_.end()) {
    Prefab prefab;
    std::string error;
    if (!loader_(asset, prefab, error)) {
      ++report_.refused;
      report_.lastError = "Prefab recusado: " + error;
      return {};
    }
    cached = prefabs_.emplace(asset, std::move(prefab)).first;
  }
  ObjectCloneMap mapping;
  WorldStatus status = WorldStatus::Ok;
  std::string diagnostic;
  const auto created = world.instantiate(cached->second, parent, mapping, status, diagnostic);
  if (!created.valid()) {
    ++report_.refused;
    report_.lastError = diagnostic.empty() ? "Instanciação recusada" : diagnostic;
    return {};
  }
  world.finishInstantiation(created, true);
  Transform pose;
  if (world.worldTransform(created, pose) == WorldStatus::Ok) {
    std::copy(position, position + 3, pose.position);
    float q[4]{rotation[0], rotation[1], rotation[2], rotation[3]};
    if (normalizeRotationQuaternion(q)) rotationEulerXYZ(q, pose.rotationDegrees);
    world.setWorldTransform(created, pose);
  }
  world.destroyAfter(created, lifetime >= 0 ? lifetime : effectDuration(world.graph(), created.id));
  ++report_.spawned;
  return created;
}

bool SceneVfx::advance(GameWorld &world, const ScenePhysics &physics, SceneParticles &particles, SceneAudio &audio, float elapsed) {
  report_.projectiles = report_.lasers = report_.sounds = 0;
  const auto &graph = world.graph();
  std::vector<ObjectId> objects;
  graph.collectSubtree(graph.root(), objects);
  std::set<Key> seenProjectiles, seenSounds;
  const float dt = std::isfinite(elapsed) && elapsed > 0 ? std::min(elapsed, 0.25f) : 0.0f;
  for (const auto id : objects) {
    const auto handle = world.handle(id);
    if (!world.alive(handle) || !graph.activeInHierarchy(id)) continue;
    const auto *object = graph.find(id);
    if (!object) continue;
    // --- Projétil ---------------------------------------------------------
    if (const auto *value = object->components.find(scene::Projectile::descriptor)) {
      const auto projectile = static_cast<const scene::Projectile &>(*value);
      const Key key{id, value->instanceId()};
      seenProjectiles.insert(key);
      auto &state = projectiles_[key];
      float position[3], rotation[4];
      if (projectile.enabled && worldPose(world, handle, position, rotation)) {
        ++report_.projectiles;
        const float local[3]{0, 0, axisSign(projectile.forwardAxis)};
        float forward[3];
        rotate(rotation, local, forward);
        if (!state.started) {
          state.started = true;
          world.destroyAfter(handle, projectile.lifetime);
          if (projectile.muzzle.valid()) spawn(world, projectile.muzzle, world.root(), position, rotation, -1);
        }
        const float travel = projectile.speed * dt;
        const float translation[3]{forward[0] * travel, forward[1] * travel, forward[2] * travel};
        QueryHit hit;
        if (travel > 0 && cast(physics, id, position, translation, projectile.radius, hit)) {
          const float normal[3]{hit.hasNormal ? hit.normal[0] : -forward[0], hit.hasNormal ? hit.normal[1] : -forward[1],
                                hit.hasNormal ? hit.normal[2] : -forward[2]};
          // O ponto de acerto é o da superfície; o centro da esfera fica antes dele.
          const float at[3]{hit.point[0] + normal[0] * projectile.hitOffset, hit.point[1] + normal[1] * projectile.hitOffset,
                            hit.point[2] + normal[2] * projectile.hitOffset};
          float facing[4];
          if (projectile.faceShooter) {
            const float half[4]{0, 1, 0, 0};  // meia-volta em Y
            multiply(rotation, half, facing);
          } else if (nonzero(projectile.rotationOffset)) {
            rotationQuaternionXYZ(projectile.rotationOffset, facing);
          } else {
            lookRotation(normal, projectile.forwardAxis, facing);
          }
          if (projectile.impact.valid()) spawn(world, projectile.impact, world.root(), at, facing, -1);
          // Os filhos marcados ficam na cena (o rastro termina sozinho).
          for (const u64 reference : projectile.detach) {
            const auto child = world.handle(static_cast<ObjectId>(reference));
            if (reference && world.alive(child))
              world.setParent(child, world.root(), std::numeric_limits<u32>::max(), ReparentPosePolicy::KeepWorld);
          }
          world.destroyObject(handle);
          projectiles_.erase(key);
          continue;
        }
        if (travel > 0) {
          Transform pose;
          if (world.worldTransform(handle, pose) == WorldStatus::Ok) {
            for (u32 k = 0; k < 3; ++k) pose.position[k] += translation[k];
            world.setWorldTransform(handle, pose);
          }
        }
      }
    }
    // --- Laser ------------------------------------------------------------
    if (const auto *value = object->components.find(scene::Laser::descriptor)) {
      const auto laser = static_cast<const scene::Laser &>(*value);
      const auto line = world.findComponent(handle, scene::LineRenderer::descriptor.id);
      float position[3], rotation[4];
      if (laser.enabled && line.valid() && worldPose(world, handle, position, rotation)) {
        ++report_.lasers;
        const float local[3]{0, 0, axisSign(laser.forwardAxis)};
        float forward[3];
        rotate(rotation, local, forward);
        const float translation[3]{forward[0] * laser.maxLength, forward[1] * laser.maxLength, forward[2] * laser.maxLength};
        QueryHit hit;
        const bool touched = cast(physics, id, position, translation, 0, hit);
        float end[3];
        for (u32 k = 0; k < 3; ++k) end[k] = touched ? hit.point[k] : position[k] + translation[k];
        // A Linha do objeto vai do laser ao ponto (em mundo).
        world.setProperty(line, "world_space", scene::ComponentPropertyValue{true});
        world.setProperty(line, "point_count", scene::ComponentPropertyValue{2.0f});
        static constexpr const char *ids[6]{"point_0_x", "point_0_y", "point_0_z", "point_1_x", "point_1_y", "point_1_z"};
        for (u32 k = 0; k < 3; ++k) {
          world.setProperty(line, ids[k], scene::ComponentPropertyValue{position[k]});
          world.setProperty(line, ids[3 + k], scene::ComponentPropertyValue{end[k]});
        }
        world.setProperty(line, "tile_length", scene::ComponentPropertyValue{laser.textureTiling > 0 ? 1.0f / laser.textureTiling : 0.0f});
        const auto effect = world.handle(static_cast<ObjectId>(laser.hitEffect));
        if (laser.hitEffect && world.alive(effect)) {
          Transform pose;
          if (world.worldTransform(effect, pose) == WorldStatus::Ok) {
            const float normal[3]{touched && hit.hasNormal ? hit.normal[0] : -forward[0], touched && hit.hasNormal ? hit.normal[1] : -forward[1],
                                  touched && hit.hasNormal ? hit.normal[2] : -forward[2]};
            for (u32 k = 0; k < 3; ++k) pose.position[k] = end[k] + (touched ? normal[k] * laser.hitOffset : 0.0f);
            if (touched) {
              float facing[4];
              if (laser.alignHit) std::copy(rotation, rotation + 4, facing);
              else lookRotation(normal, laser.forwardAxis, facing);
              rotationEulerXYZ(facing, pose.rotationDegrees);
            }
            world.setWorldTransform(effect, pose);
          }
          // As partículas do impacto tocam só enquanto o raio acerta algo.
          std::vector<ObjectId> subtree;
          graph.collectSubtree(effect.id, subtree);
          for (const auto member : subtree) {
            const auto *target = graph.find(member);
            if (!target) continue;
            for (usize c = 0; c < target->components.size(); ++c) {
              const auto *component = target->components.at(c);
              if (&component->type() != &scene::ParticleSystem::descriptor) continue;
              SceneParticles::Status status;
              if (!particles.status(graph, member, component->instanceId(), status)) continue;
              if (touched && !status.emitting) particles.command(graph, member, component->instanceId(), SceneParticles::Command::Play);
              if (!touched && status.emitting) particles.command(graph, member, component->instanceId(), SceneParticles::Command::Stop);
            }
          }
        }
      }
    }
    // --- Girar -------------------------------------------------------------
    if (const auto *value = object->components.find(scene::Rotator::descriptor)) {
      const auto &rotator = static_cast<const scene::Rotator &>(*value);
      Transform local;
      if (rotator.enabled && dt > 0 && world.localTransform(handle, local) == WorldStatus::Ok) {
        for (u32 k = 0; k < 3; ++k) {
          const float turned = std::fmod(local.rotationDegrees[k] + rotator.speed[k] * dt, 360.0f);
          local.rotationDegrees[k] = turned > 180.0f ? turned - 360.0f : turned < -180.0f ? turned + 360.0f : turned;
        }
        world.setLocalTransform(handle, local);
        ++report_.rotators;
      }
    }
    // --- Som repetido -----------------------------------------------------
    if (const auto *value = object->components.find(scene::EffectSound::descriptor)) {
      const auto sound = static_cast<const scene::EffectSound &>(*value);
      const Key key{id, value->instanceId()};
      seenSounds.insert(key);
      const auto source = world.findComponent(handle, scene::AudioSource::descriptor.id);
      if (!sound.enabled || !source.valid()) continue;
      ++report_.sounds;
      auto &state = sounds_[key];
      const auto play = [&] { audio.command(world, source, SceneAudio::Command::Play); };
      if (!state.started) {
        state.started = true;
        state.next = sound.startDelay;
        // Volume sorteado uma vez, na partida, e um toque imediato (como o
        // script do pacote); a repetição começa depois do Atraso.
        if (sound.randomVolume) {
          random_ = random_ * 1664525u + 1013904223u;
          const float t = static_cast<float>(random_ >> 8) * (1.0f / 16777216.0f);
          const float low = std::min(sound.volumeMin, sound.volumeMax), high = std::max(sound.volumeMin, sound.volumeMax);
          world.setProperty(source, "volume", scene::ComponentPropertyValue{low + (high - low) * t});
          play();
        }
      }
      state.time += dt;
      if (sound.repeating && state.time >= state.next) {
        play();
        state.next = state.time + std::max(0.05f, sound.interval);
      }
    }
  }
  for (auto it = projectiles_.begin(); it != projectiles_.end();) it = seenProjectiles.contains(it->first) ? std::next(it) : projectiles_.erase(it);
  for (auto it = sounds_.begin(); it != sounds_.end();) it = seenSounds.contains(it->first) ? std::next(it) : sounds_.erase(it);
  return true;
}

bool SceneVfx::spawnCollisions(GameWorld &world, const SceneParticles &particles) {
  if (particles.collisions().empty()) return true;
  std::map<Key, u32> spawned;
  for (const auto &collision : particles.collisions()) {
    const auto handle = world.handle(collision.object);
    if (!world.alive(handle)) continue;
    const auto *object = world.graph().find(collision.object);
    const auto *value = object ? object->components.find(scene::ParticleCollisionSpawn::descriptor) : nullptr;
    if (!value) continue;
    const auto rule = static_cast<const scene::ParticleCollisionSpawn &>(*value);
    if (!rule.enabled) continue;
    const Key key{collision.object, value->instanceId()};
    auto &count = spawned[key];
    if (count >= static_cast<u32>(rule.maxPerFrame)) continue;
    ++count;
    const float at[3]{collision.position[0] + collision.normal[0] * rule.offset, collision.position[1] + collision.normal[1] * rule.offset,
                      collision.position[2] + collision.normal[2] * rule.offset};
    float facing[4]{0, 0, 0, 1};
    if (rule.faceEmitter) {
      float emitter[3], rotation[4];
      if (worldPose(world, handle, emitter, rotation)) {
        const float toward[3]{emitter[0] - at[0], emitter[1] - at[1], emitter[2] - at[2]};
        lookRotation(toward, rule.forwardAxis, facing);
      }
    } else if (nonzero(rule.rotationOffset) && rule.rotationOffsetOnly) {
      rotationQuaternionXYZ(rule.rotationOffset, facing);
    } else {
      float look[4], offset[4];
      lookRotation(collision.normal, rule.forwardAxis, look);
      rotationQuaternionXYZ(rule.rotationOffset, offset);
      multiply(look, offset, facing);
    }
    const auto parent = rule.worldSpace ? world.root() : handle;
    for (const auto &effect : rule.effects)
      if (effect.valid()) spawn(world, effect, parent, at, facing, rule.destroyDelay);
    if (rule.destroyEmitter && !destroying_[key]) {
      destroying_[key] = true;
      world.destroyAfter(handle, rule.destroyDelay + 0.5);
    }
  }
  return true;
}

} // namespace ae::runtime
