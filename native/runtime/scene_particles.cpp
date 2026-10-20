#include "runtime/scene_particles.h"

#include "core/job_system.h"
#include "runtime/transform_math.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <locale>
#include <sstream>
#include <thread>

namespace ae::runtime {
namespace {
// Um conjunto de workers para todas as partículas do processo (prévia e Play),
// criado no primeiro uso. Poucos workers: a thread principal também trabalha e
// o resto da engine (física, áudio) tem os seus.
JobSystem &particleJobs() {
  static JobSystem jobs(std::clamp(std::thread::hardware_concurrency() / 3u, 1u, 3u));
  return jobs;
}
// Abaixo disto, despachar jobs custa mais do que simular na thread principal.
constexpr u32 kParallelThreshold = 1500;

std::string fingerprintOf(const scene::ParticleSystem &system) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  system.write(out);
  return out.str();
}
} // namespace

u32 particleSeed(const scene::ParticleSystemSettings &settings, ObjectId object, u64 instance) {
  if (!settings.autoRandomSeed) return static_cast<u32>(settings.randomSeed);
  u64 h = (static_cast<u64>(object) << 32) ^ instance ^ 0x9e3779b97f4a7c15ULL;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
  return static_cast<u32>(h);
}

void SceneParticles::reset() {
  entries_.clear();
  knownRevision_ = ~u64{0};
  simulationMs_ = 0;
}

u32 SceneParticles::aliveTotal() const noexcept {
  u32 total = 0;
  for (const auto &[key, entry] : entries_) total += entry.emitter.alive();
  return total;
}

void SceneParticles::reconcile(const SceneGraph &graph) {
  if (knownRevision_ == graph.revision()) return;
  knownRevision_ = graph.revision();
  for (auto &[key, entry] : entries_) entry.seen = false;
  std::vector<ObjectId> objects;
  graph.collectSubtree(graph.root(), objects);
  for (const auto id : objects) {
    const auto *object = graph.find(id);
    if (!object) continue;
    for (usize i = 0; i < object->components.size(); ++i) {
      const auto *value = object->components.at(i);
      if (&value->type() != &scene::ParticleSystem::descriptor) continue;
      const auto &system = scene::particleSystem(*value);
      const Key key{id, value->instanceId()};
      auto [it, created] = entries_.try_emplace(key);
      auto &entry = it->second;
      entry.seen = true;
      auto fingerprint = fingerprintOf(system);
      if (created || fingerprint != entry.fingerprint) {
        entry.emitter.configure(system, particleSeed(system, id, value->instanceId()));
        entry.fingerprint = std::move(fingerprint);
      }
      const bool active = graph.activeInHierarchy(id);
      // Unity: desativar o objeto apaga as partículas; reativar com "Tocar ao
      // iniciar" recomeça. O primeiro encontro conta como ativação.
      if (active && (created || !entry.active) && system.playOnAwake) entry.emitter.restart();
      if (!active && entry.active) entry.emitter.stop(true);
      entry.active = active;
    }
  }
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (!it->second.seen) it = entries_.erase(it);
    else ++it;
  }
  distributeBudget();
}

void SceneParticles::distributeBudget() {
  u64 requested = 0;
  for (const auto &[key, entry] : entries_)
    if (entry.active) requested += static_cast<u64>(entry.emitter.settings().maxParticles);
  for (auto &[key, entry] : entries_) {
    const auto authored = static_cast<u64>(entry.emitter.settings().maxParticles);
    u32 limit = static_cast<u32>(authored);
    if (requested > budget_) limit = static_cast<u32>(std::max<u64>(1, authored * budget_ / requested));
    entry.emitter.setCapacityLimit(limit);
  }
}

void SceneParticles::advance(const SceneGraph &graph, float deltaSeconds, std::vector<Finished> *finished) {
  const auto start = std::chrono::steady_clock::now();
  reconcile(graph);
  std::vector<Entry *> work;
  work.reserve(entries_.size());
  u32 alive = 0;
  for (auto &[key, entry] : entries_) {
    if (!entry.active) continue;
    if (!worldMatrix(graph, key.object, entry.world)) continue;
    work.push_back(&entry);
    alive += entry.emitter.alive();
  }
  if (deltaSeconds > 0) {
    const auto step = [deltaSeconds](Entry *entry) { entry->emitter.simulate(deltaSeconds, entry->world); };
    if (alive >= kParallelThreshold && work.size() > 1) {
      // Um emissor por job; o maior começa primeiro. O último fica para esta thread.
      std::sort(work.begin(), work.end(), [](const Entry *a, const Entry *b) { return a->emitter.alive() > b->emitter.alive(); });
      auto &jobs = particleJobs();
      for (usize i = 1; i < work.size(); ++i) jobs.submit([entry = work[i], step] { step(entry); });
      step(work[0]);
      jobs.waitIdle();
    } else {
      for (auto *entry : work) step(entry);
    }
    for (auto &[key, entry] : entries_) {
      if (!entry.active) continue;
      if (entry.emitter.takeFinished()) {
        if (finished) finished->push_back({key.object, key.instance, entry.emitter.settings().stopAction});
        entry.idle = 0;
      }
      if (previewLoop_ && entry.emitter.state() == renderer::particles::ParticleEmitter::State::Stopped &&
          entry.emitter.settings().playOnAwake) {
        entry.idle += deltaSeconds;
        if (entry.idle >= 1.0f) { entry.idle = 0; entry.emitter.restart(); }
      }
    }
  }
  simulationMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void SceneParticles::build(const SceneGraph &graph, const float (&camera)[3], renderer::particles::ParticleFrame &out) const {
  (void)graph;
  const auto start = std::chrono::steady_clock::now();
  out.clear();
  out.budget = budget_;
  out.simulationMs = simulationMs_;
  for (const auto &[key, entry] : entries_) {
    if (!entry.active || !entry.emitter.alive()) continue;
    renderer::particles::ParticleBatch batch;
    batch.firstInstance = static_cast<u32>(out.instances.size());
    entry.emitter.build(entry.world, camera, out.instances);
    batch.instanceCount = static_cast<u32>(out.instances.size()) - batch.firstInstance;
    if (!batch.instanceCount) continue;
    batch.material = entry.emitter.material();
    const float d[3]{entry.world[12] - camera[0], entry.world[13] - camera[1], entry.world[14] - camera[2]};
    batch.sortDistance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + entry.emitter.sortingFudge();
    out.batches.push_back(batch);
    out.alive += batch.instanceCount;
    ++out.systems;
  }
  std::stable_sort(out.batches.begin(), out.batches.end(),
                   [](const auto &a, const auto &b) { return a.sortDistance > b.sortDistance; });
  out.buildMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool SceneParticles::command(const SceneGraph &graph, ObjectId object, u64 instance, Command command, u32 count) {
  reconcile(graph);
  const auto it = entries_.find({object, instance});
  if (it == entries_.end()) return false;
  auto &entry = it->second;
  auto &emitter = entry.emitter;
  switch (command) {
  case Command::Play: emitter.play(); break;
  case Command::Stop: emitter.stop(false); break;
  case Command::StopAndClear: emitter.stop(true); break;
  case Command::Pause: emitter.pause(); break;
  case Command::Clear: emitter.clear(); break;
  case Command::Restart: emitter.restart(); break;
  case Command::Emit:
    if (!worldMatrix(graph, object, entry.world)) return false;
    emitter.emit(count, entry.world);
    break;
  }
  entry.idle = 0;
  return true;
}

bool SceneParticles::status(const SceneGraph &graph, ObjectId object, u64 instance, Status &out) {
  reconcile(graph);
  const auto it = entries_.find({object, instance});
  if (it == entries_.end()) return false;
  const auto &emitter = it->second.emitter;
  out.playing = emitter.state() != renderer::particles::ParticleEmitter::State::Stopped;
  out.paused = emitter.state() == renderer::particles::ParticleEmitter::State::Paused;
  out.emitting = emitter.emitting();
  out.alive = emitter.alive();
  out.time = emitter.time();
  return true;
}

bool SceneParticles::peek(ObjectId object, u64 instance, Status &out) const {
  const auto it = entries_.find({object, instance});
  if (it == entries_.end()) return false;
  const auto &emitter = it->second.emitter;
  out.playing = emitter.state() != renderer::particles::ParticleEmitter::State::Stopped;
  out.paused = emitter.state() == renderer::particles::ParticleEmitter::State::Paused;
  out.emitting = emitter.emitting();
  out.alive = emitter.alive();
  out.time = emitter.time();
  return true;
}

u32 SceneParticles::capacityOf(ObjectId object, u64 instance) const {
  const auto it = entries_.find({object, instance});
  return it == entries_.end() ? 0 : it->second.emitter.capacity();
}

void SceneParticles::restartAll() {
  for (auto &[key, entry] : entries_)
    if (entry.active) entry.emitter.restart();
}

} // namespace ae::runtime
