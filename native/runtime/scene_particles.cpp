#include "runtime/scene_particles.h"

#include "core/job_system.h"
#include "runtime/scene_lights.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include "scene/vfx_behaviors.h"

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

std::shared_ptr<const renderer::particles::ParticleMeshSet> builtinParticleMeshes() {
  static const auto set = [] {
    auto value = std::make_shared<renderer::particles::ParticleMeshSet>();
    value->revision = 1;
    renderer::particles::appendBuiltinParticleMeshes(*value);
    return std::shared_ptr<const renderer::particles::ParticleMeshSet>(std::move(value));
  }();
  return set;
}

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
      // G6a: Instanciar ao colidir no mesmo objeto quer os toques deste sistema.
      const auto *listener = object->components.find(scene::ParticleCollisionSpawn::descriptor);
      entry.listens = listener && static_cast<const scene::ParticleCollisionSpawn *>(listener)->enabled;
      entry.emitter.setCollisionEventsWanted(entry.listens);
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

// Quantas partículas o sistema pode ter vivas ao mesmo tempo pelo que emite
// (taxa máxima × vida máxima + rajadas), limitado ao Máximo autoral. Repartir o
// teto pelo Máximo deixava um sistema parado (a Neve com taxa zero) com a maior
// fatia, e o que emite (a Chuva) batia no teto: as gotas novas só nasciam quando
// as velhas morriam, todas juntas, e a chuva vinha em ondas. Um piso pequeno
// guarda lugar para Emit e sub-emissores.
static u64 particleDemand(const scene::ParticleSystemSettings &s) {
  const auto peak = [](const scene::ParticleMinMaxCurve &curve) {
    float top = std::max(curve.constantMin, curve.constantMax);
    if (curve.usesCurves()) {
      float keys = 0;
      for (const auto *c : {&curve.curveMin, &curve.curveMax})
        for (const auto &key : c->keys) keys = std::max(keys, key.value);
      top = keys * curve.multiplier;
    }
    return std::max(0.0f, top);
  };
  const auto authored = static_cast<u64>(std::max(1.0f, s.maxParticles));
  double alive = s.emissionEnabled ? static_cast<double>(peak(s.emissionRate)) * peak(s.startLifetime) : 0.0;
  for (usize b = 0; b < s.activeBursts(); ++b) alive += std::max(s.bursts[b].countMin, s.bursts[b].countMax);
  const u64 floor = std::min<u64>(authored, 256);
  return std::clamp<u64>(static_cast<u64>(std::ceil(alive)), floor, authored);
}

void SceneParticles::distributeBudget() {
  u64 requested = 0;
  for (const auto &[key, entry] : entries_)
    if (entry.active) requested += particleDemand(entry.emitter.settings());
  for (auto &[key, entry] : entries_) {
    const auto authored = static_cast<u64>(entry.emitter.settings().maxParticles);
    u32 limit = static_cast<u32>(authored);
    if (requested > budget_)
      limit = static_cast<u32>(std::max<u64>(1, particleDemand(entry.emitter.settings()) * budget_ / requested));
    entry.emitter.setCapacityLimit(limit);
  }
}

void SceneParticles::preparePlanes(const SceneGraph &graph, Entry &entry) const {
  const auto &settings = entry.emitter.settings();
  renderer::particles::ParticleCollisionPlane planes[2];
  u32 count = 0;
  if (settings.collisionEnabled)
    for (const u64 reference : {settings.collisionPlane0, settings.collisionPlane1}) {
      const auto id = static_cast<ObjectId>(reference);
      float world[16];
      if (!reference || !graph.exists(id) || !graph.activeInHierarchy(id) || !worldMatrix(graph, id, world)) continue;
      // +Y do objeto é a normal; a escala não muda o plano.
      const float length = std::sqrt(world[4] * world[4] + world[5] * world[5] + world[6] * world[6]);
      if (!(length > 1e-6f)) continue;
      auto &plane = planes[count++];
      for (u32 k = 0; k < 3; ++k) { plane.point[k] = world[12 + k]; plane.normal[k] = world[4 + k] / length; }
    }
  entry.emitter.setCollisionPlanes(planes, count);
}

// Modo de escala (Unity Scaling Mode): Hierarquia lê a escala de mundo (o
// objeto e os pais), Local só a do objeto, Forma não escala a partícula.
float SceneParticles::sizeScale(const SceneGraph &graph, ObjectId object, const Entry &entry) const {
  const auto mode = entry.emitter.settings().scalingMode;
  if (mode == scene::ParticleScalingMode::Shape) return 1.0f;
  if (mode == scene::ParticleScalingMode::Local) {
    const auto *value = graph.find(object);
    if (!value) return 1.0f;
    const auto &s = value->transform.scale;
    return (std::abs(s[0]) + std::abs(s[1]) + std::abs(s[2])) / 3.0f;
  }
  const float *m = entry.world;
  float sum = 0;
  for (u32 c = 0; c < 3; ++c) sum += std::sqrt(m[c * 4] * m[c * 4] + m[c * 4 + 1] * m[c * 4 + 1] + m[c * 4 + 2] * m[c * 4 + 2]);
  return sum / 3.0f;
}

SceneParticles::Entry *SceneParticles::firstEntryOf(ObjectId object) {
  const auto it = entries_.lower_bound({object, 0});
  return it != entries_.end() && it->first.object == object ? &it->second : nullptr;
}

void SceneParticles::routeSubEmitters(const SceneGraph &graph) {
  for (auto &[key, entry] : entries_) {
    if (!entry.active) continue;
    const auto &settings = entry.emitter.settings();
    if (!settings.subEmittersEnabled) continue;
    const struct { u64 target; float count; const std::vector<renderer::particles::ParticleEvent> &events; } routes[]{
        {settings.subEmitterBirth, settings.subEmitterBirthCount, entry.emitter.births()},
        {settings.subEmitterCollision, settings.subEmitterCollisionCount, entry.emitter.collisions()},
        {settings.subEmitterDeath, settings.subEmitterDeathCount, entry.emitter.deaths()}};
    // G6a: extras emitem as rajadas do próprio sub-sistema (como a Unity), ao
    // menos uma partícula por acontecimento.
    const auto burstsOf = [&](u64 target) -> float {
      const auto *other = firstEntryOf(static_cast<ObjectId>(target));
      if (!other) return 1;
      float total = 0;
      const auto &s = other->emitter.settings();
      for (usize b = 0; b < s.activeBursts(); ++b) total += std::max(s.bursts[b].countMin, s.bursts[b].countMax);
      return std::max(1.0f, std::floor(total));
    };
    std::vector<std::pair<u64, std::pair<float, const std::vector<renderer::particles::ParticleEvent> *>>> extras;
    for (usize k = 0; k < scene::ParticleSystemSettings::kExtraSubEmitters; ++k) {
      if (!settings.subEmitterExtra[k]) continue;
      const u32 type = settings.subEmitterExtraType[k];
      const auto *events = type == 0 ? &entry.emitter.births() : type == 1 ? &entry.emitter.collisions() : &entry.emitter.deaths();
      extras.push_back({settings.subEmitterExtra[k], {burstsOf(settings.subEmitterExtra[k]), events}});
    }
    for (const auto &extra : extras) {
      const auto child = static_cast<ObjectId>(extra.first);
      const auto &events = *extra.second.second;
      if (events.empty() || child == key.object) continue;
      auto *target = firstEntryOf(child);
      float world[16];
      if (!target || !target->active || !worldMatrix(graph, child, world)) continue;
      for (const auto &event : events) {
        for (u32 k = 0; k < 3; ++k) world[12 + k] = event.position[k];
        target->emitter.emit(static_cast<u32>(extra.second.first), world);
      }
      target->idle = 0;
    }
    for (const auto &route : routes) {
      const auto child = static_cast<ObjectId>(route.target);
      // Um sistema não é sub-emissor de si mesmo: seria uma reação em cadeia.
      if (!route.target || route.events.empty() || route.count < 1 || child == key.object) continue;
      auto *target = firstEntryOf(child);
      float world[16];
      if (!target || !target->active || !worldMatrix(graph, child, world)) continue;
      // A forma do sub-sistema, com a orientação dele, no ponto do evento.
      for (const auto &event : route.events) {
        for (u32 k = 0; k < 3; ++k) world[12 + k] = event.position[k];
        target->emitter.emit(static_cast<u32>(route.count), world);
      }
      target->idle = 0;
    }
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
    entry.emitter.clearEvents();
    preparePlanes(graph, entry);
    entry.emitter.setWorldCollider(worldRaycast_, worldContext_, worldQueries_);
    entry.emitter.setSizeScale(sizeScale(graph, key.object, entry));
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
    routeSubEmitters(graph);
    collisions_.clear();
    for (auto &[key, entry] : entries_) {
      if (!entry.active || !entry.listens) continue;
      for (const auto &event : entry.emitter.collisions()) {
        Collision collision;
        collision.object = key.object;
        collision.instance = key.instance;
        std::copy(event.position, event.position + 3, collision.position);
        std::copy(event.velocity, event.velocity + 3, collision.velocity);
        std::copy(event.normal, event.normal + 3, collision.normal);
        collisions_.push_back(collision);
      }
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

// Módulo Luzes (Unity Lights): a luz modelo é lida do grafo a cada quadro (o
// autor pode animá-la); cada partícula entra por um sorteio fixo da semente
// dela contra a Proporção, na ordem do lote, até o Máximo de luzes.
void SceneParticles::appendLights(const SceneGraph &graph, const Entry &entry, const renderer::particles::ParticleBatch &batch,
                                  renderer::particles::ParticleFrame &out) const {
  const auto &s = entry.emitter.settings();
  if (!s.lightsEnabled || !s.lightTemplate || s.lightMaxLights < 1 || s.lightRatio <= 0) return;
  const auto *object = graph.find(static_cast<ObjectId>(s.lightTemplate));
  const auto *light = object ? static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor)) : nullptr;
  if (!light || !light->enabled || !light->valid() || light->kind == scene::LightKind::Directional) return;
  renderer::SceneLight base;
  sceneLightValues(*light, base);
  const u32 limit = static_cast<u32>(s.lightMaxLights);
  u32 lit = 0;
  for (u32 i = 0; i < batch.instanceCount && lit < limit; ++i) {
    const auto &p = out.instances[batch.firstInstance + i];
    if (renderer::particles::particleRandom(p.random, kLightChannel) >= s.lightRatio) continue;
    renderer::particles::ParticleLight l;
    std::copy(p.position, p.position + 3, l.position);
    const float alpha = renderer::particles::halfToFloat(p.color[3]);
    for (u32 k = 0; k < 3; ++k)
      l.color[k] = base.color[k] * (s.lightUseParticleColor ? renderer::particles::halfToFloat(p.color[k]) : 1.0f);
    l.intensity = base.intensity * s.lightIntensityMultiplier * (s.lightAlphaAffectsIntensity ? alpha : 1.0f);
    l.range = base.range * s.lightRangeMultiplier * (s.lightSizeAffectsRange ? p.size : 1.0f);
    l.modality = static_cast<u32>(base.modality);
    l.photometric = base.photometric;
    l.cullingMask = base.filter.cullingMask;
    l.renderingLayers = base.filter.renderingLayers;
    if (l.intensity <= 0 || l.range <= 0) continue;
    out.lights.push_back(l);
    ++lit;
  }
}

void appendParticleLights(const renderer::particles::ParticleFrame &frame, std::vector<renderer::SceneLight> &out) {
  for (const auto &light : frame.lights) {
    renderer::SceneLight entry{};
    entry.modality = static_cast<renderer::LightModality>(light.modality);
    std::copy(light.position, light.position + 3, entry.position);
    std::copy(light.color, light.color + 3, entry.color);
    entry.intensity = light.intensity;
    entry.range = light.range;
    entry.photometric = light.photometric;
    entry.filter.cullingMask = light.cullingMask;
    entry.filter.renderingLayers = light.renderingLayers;
    out.push_back(entry);
  }
}

void SceneParticles::build(const SceneGraph &graph, const float (&camera)[3], renderer::particles::ParticleFrame &out,
                           const ParticleMaterialResolver *materials, const ParticleMeshes *meshes) const {
  const auto start = std::chrono::steady_clock::now();
  out.clear();
  out.budget = budget_;
  out.simulationMs = simulationMs_;
  out.meshes = meshes && meshes->set ? meshes->set : builtinParticleMeshes();
  for (const auto &[key, entry] : entries_) {
    if (!entry.active || !entry.emitter.alive()) continue;
    renderer::particles::ParticleBatch batch;
    batch.firstInstance = static_cast<u32>(out.instances.size());
    entry.emitter.build(entry.world, camera, out.instances);
    batch.instanceCount = static_cast<u32>(out.instances.size()) - batch.firstInstance;
    if (!batch.instanceCount) continue;
    batch.material = entry.emitter.material();
    // G6a: a malha do lote. A do projeto, quando resolvida, vale mais que a
    // embutida; Malha do projeto que não resolve não desenha (o renderer pula o
    // lote) em vez de trocar a forma por outra.
    if (entry.emitter.settings().renderMode == scene::ParticleRenderMode::Mesh) {
      const auto &settings = entry.emitter.settings();
      if (settings.meshAsset.valid()) {
        u32 range = ~0u;
        if (meshes && meshes->resolve && meshes->resolve(settings.meshAsset, range)) batch.mesh = range;
      } else {
        batch.mesh = static_cast<u32>(settings.meshShape);
      }
    }
    // Material de projeto: a imagem dele e a cor base, como o Material do
    // Particle System Renderer da Unity. A mistura segue o Surface do material.
    if (const auto &asset = entry.emitter.settings().materialAsset; asset.valid() && materials && *materials) {
      ParticleMaterialLook look;
      if ((*materials)(asset, look)) {
        batch.material.projectTexture = look.texture;
        for (u32 k = 0; k < 3; ++k) batch.material.tint[k] *= look.color[k];
        // Distorção é do componente: o Surface do material não a desfaz.
        if (batch.material.blend != static_cast<u32>(scene::ParticleBlend::Distortion))
          batch.material.blend = static_cast<u32>(look.additive ? scene::ParticleBlend::Additive : scene::ParticleBlend::AlphaBlend);
      }
    }
    const float d[3]{entry.world[12] - camera[0], entry.world[13] - camera[1], entry.world[14] - camera[2]};
    batch.sortDistance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + entry.emitter.sortingFudge();
    appendLights(graph, entry, batch, out);
    out.batches.push_back(batch);
    out.alive += batch.instanceCount;
    // Trilhas: um lote a mais do mesmo sistema, logo atrás das partículas.
    renderer::particles::ParticleBatch trails;
    trails.firstInstance = static_cast<u32>(out.instances.size());
    entry.emitter.buildTrails(entry.world, out.instances);
    trails.instanceCount = static_cast<u32>(out.instances.size()) - trails.firstInstance;
    if (trails.instanceCount) {
      trails.material = entry.emitter.trailMaterial();
      trails.material.projectTexture = batch.material.projectTexture;
      std::copy(batch.material.tint, batch.material.tint + 3, trails.material.tint);
      trails.material.blend = batch.material.blend;
      trails.sortDistance = batch.sortDistance + 1e-3f;
      out.batches.push_back(trails);
    }
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
