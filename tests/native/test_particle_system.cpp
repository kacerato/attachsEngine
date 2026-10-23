// G5 P1: componente ParticleSystem, simulação determinística, emissão e
// rajadas, formas, curvas/gradientes e teto por nível.
#include "harness.h"

#include "renderer/particles/particle_simulation.h"
#include "renderer/particles/particle_textures.h"
#include "runtime/scene_particles.h"
#include "scene/component_reflection.h"
#include "scene/component_schema.h"
#include "scene/particle_system.h"

#include <cmath>
#include <cstring>
#include <locale>
#include <sstream>

using namespace ae;
using renderer::particles::ParticleEmitter;
using renderer::particles::ParticleInstance;

namespace {
constexpr float kIdentity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
const float kCamera[3]{0, 0, -10};

std::string serialize(const scene::ParticleSystem &system) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  system.write(out);
  return out.str();
}
bool roundTrip(const scene::ParticleSystem &source, scene::ParticleSystem &out) {
  std::istringstream in(serialize(source));
  in.imbue(std::locale::classic());
  return out.read(in, scene::ParticleSystem::descriptor.version);
}
std::vector<ParticleInstance> run(const scene::ParticleSystemSettings &settings, u32 seed, u32 frames, float dt) {
  ParticleEmitter emitter;
  emitter.configure(settings, seed);
  emitter.play();
  for (u32 i = 0; i < frames; ++i) emitter.simulate(dt, kIdentity);
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  return out;
}
} // namespace

AE_TEST(particle_system_registered_with_effects_family_and_reflection) {
  const auto *schema = scene::findComponentSchema("astra.effects.particle_system");
  AE_EXPECT_TRUE(schema != nullptr, "ParticleSystem registrado no schema");
  AE_EXPECT_TRUE(schema->family == scene::ComponentFamily::Effects, "família Efeitos");
  AE_EXPECT_TRUE(schema->apiName == "ParticleSystem", "fachada C# gerada");
  AE_EXPECT_TRUE(scene::componentSchemaIdsUnique(), "ids únicos");
  AE_EXPECT_TRUE(core::engineCapabilityAuthorable("render.particles.simulation"), "simulação implementada");
  AE_EXPECT_TRUE(core::engineCapabilityAuthorable("render.particles.soft"), "suave autorável (limitada pelo aparelho)");
  AE_EXPECT_TRUE(!core::engineCapabilityAuthorable("render.particles.trails"), "rastro ainda planejado");
  // Toda propriedade tem consumidor declarado e capacidade autorável.
  for (const auto &p : scene::ParticleSystem::descriptor.numbers)
    AE_EXPECT_TRUE(p.presentation.hasConsumer() && !p.presentation.consumer.empty() && !p.id.empty(), "número com consumidor");
  for (const auto &p : scene::ParticleSystem::descriptor.enums)
    AE_EXPECT_TRUE(p.presentation.hasConsumer() && !p.presentation.consumer.empty(), "enum com consumidor");
  // A mesma auditoria do aether_tests (component_contracts), restrita a este tipo.
  for (const auto &issue : scene::auditComponentContracts())
    if (issue.typeId == "astra.effects.particle_system") {
      std::fprintf(stderr, "  contrato: %.*s — %s\n", static_cast<int>(issue.propertyId.size()), issue.propertyId.data(), issue.message.c_str());
      AE_EXPECT_TRUE(false, "contrato de propriedade incompleto");
    }
  scene::ParticleSystem defaults;
  AE_EXPECT_TRUE(defaults.valid(), "padrões válidos");
  AE_EXPECT_TRUE(scene::findComponentMethod(scene::ParticleSystem::descriptor, "emit") != nullptr, "método Emit");
  AE_EXPECT_TRUE(scene::findComponentEvent(scene::ParticleSystem::descriptor, "stopped") != nullptr, "evento Parou");
}

AE_TEST(particle_system_archive_round_trip_preserves_curves_gradients_bursts) {
  scene::ParticleSystem source;
  source.duration = 2.5f;
  source.looping = false;
  source.startLifetime.mode = scene::ParticleCurveMode::TwoConstants;
  source.startLifetime.constantMin = 0.4f;
  source.startLifetime.constantMax = 1.2f;
  source.sizeOverLifetimeEnabled = true;
  source.sizeOverLifetime.curveMax = scene::particleLinearCurve(1, 0.1f);
  source.startColor.mode = scene::ParticleGradientMode::TwoColors;
  source.startColor.colorMin[0] = 4.0f;  // HDR
  source.colorOverLifetimeEnabled = true;
  source.colorOverLifetime.gradientMax.colors = {{0, {1, .8f, .3f}}, {.5f, {1, .3f, .05f}}, {1, {.2f, .2f, .2f}}};
  source.shapeType = scene::ParticleShapeType::Box;
  source.shapeBoxEmitFrom = scene::ParticleBoxEmitFrom::Edge;
  source.materialBlend = scene::ParticleBlend::Additive;
  source.materialNoise = scene::ParticleNoise::Cells;
  source.materialSoftDistance = 0.75f;
  source.burstCount = 2;
  source.bursts[0] = {0, 20, 40, 3, 0.2f, 0.5f};
  source.bursts[1] = {1.5f, 5, 5, 0, 0.5f, 1};
  AE_EXPECT_TRUE(source.valid(), "fonte válida");
  scene::ParticleSystem copy;
  AE_EXPECT_TRUE(roundTrip(source, copy), "lê o que escreveu");
  AE_EXPECT_EQ(serialize(copy), serialize(source), "texto idêntico após reabrir");
  AE_EXPECT_TRUE(copy.bursts == source.bursts, "rajadas preservadas");
  AE_EXPECT_EQ(copy.startColor.colorMin[0], 4.0f, "cor HDR preservada");
  AE_EXPECT_EQ(copy.colorOverLifetime.gradientMax.colors.size(), usize(3), "gradiente preservado");

  // Pelo arquivo de componentes inteiro (o caminho da cena), com identidade.
  scene::Components components;
  auto *added = components.add(scene::ParticleSystem::descriptor);
  AE_EXPECT_TRUE(added != nullptr, "componente adicionado");
  static_cast<scene::ParticleSystemSettings &>(scene::particleSystem(*added)) = source;
  std::ostringstream archive;
  archive.imbue(std::locale::classic());
  AE_EXPECT_TRUE(components.write(archive, true), "arquivo escrito");
  std::array<const scene::ComponentType *, 1> registry{&scene::ParticleSystem::descriptor};
  scene::Components reopened;
  std::istringstream in(archive.str());
  in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reopened.read(in, registry, scene::UnknownComponentPolicy::Reject, true), "arquivo relido");
  const auto *value = reopened.find(scene::ParticleSystem::descriptor);
  AE_EXPECT_TRUE(value && value->instanceId() == added->instanceId(), "identidade preservada");
  AE_EXPECT_EQ(serialize(scene::particleSystem(*value)), serialize(source), "valores preservados");
  AE_EXPECT_TRUE(reopened.at(0)->clone()->valid(), "clone válido");
}

AE_TEST(particle_system_archive_rejects_unknown_ids_versions_and_ranges) {
  scene::ParticleSystem source, target;
  auto text = serialize(source);
  {
    std::istringstream in(text);
    AE_EXPECT_TRUE(!target.read(in, 4), "versão futura recusada");
  }
  {
    auto bad = text;
    bad.replace(bad.find("duration"), 8, "duracaoX");
    std::istringstream in(bad);
    AE_EXPECT_TRUE(!target.read(in, 1), "id desconhecido recusado");
  }
  {
    auto bad = text;
    const auto at = bad.find("max_particles ") + 14;
    bad.replace(at, bad.find(' ', at) - at, "0");
    std::istringstream in(bad);
    AE_EXPECT_TRUE(!target.read(in, 1), "máximo zero fora do domínio");
  }
  {
    // Formato mínimo de uma versão antiga hipotética: só alguns ids; o resto
    // fica no padrão autoral (migração por ausência).
    std::istringstream in("1 duration 3 0 0 0");
    AE_EXPECT_TRUE(target.read(in, 1), "subconjunto aceito");
    AE_EXPECT_EQ(target.duration, 3.0f, "valor lido");
    AE_EXPECT_EQ(target.maxParticles, 1000.0f, "resto no padrão");
  }
  scene::ParticleSystem tooMany;
  tooMany.burstCount = 5;
  AE_EXPECT_TRUE(!tooMany.valid(), "no máximo 4 rajadas");
  tooMany.burstCount = 1;
  tooMany.bursts[0].countMin = 50;  // acima do máximo: aceito, sorteado entre os dois
  AE_EXPECT_TRUE(tooMany.valid(), "edição campo a campo nunca é recusada");
}

AE_TEST(particle_simulation_is_deterministic_by_seed) {
  scene::ParticleSystem settings;
  settings.startSpeed.mode = scene::ParticleCurveMode::TwoConstants;
  settings.startSpeed.constantMin = 1;
  settings.startSize.mode = scene::ParticleCurveMode::TwoConstants;
  settings.startSize.constantMin = 0.2f;
  settings.gravityModifier = scene::particleCurveDefault(0.5f);
  settings.emissionRate = scene::particleCurveDefault(200);
  const auto a = run(settings, 1234, 90, 1.0f / 60.0f);
  const auto b = run(settings, 1234, 90, 1.0f / 60.0f);
  const auto c = run(settings, 4321, 90, 1.0f / 60.0f);
  AE_EXPECT_TRUE(!a.empty(), "emitiu");
  AE_EXPECT_EQ(a.size(), b.size(), "mesma contagem");
  AE_EXPECT_TRUE(std::memcmp(a.data(), b.data(), a.size() * sizeof(ParticleInstance)) == 0, "mesma semente, bytes iguais");
  AE_EXPECT_TRUE(c.size() != a.size() || std::memcmp(a.data(), c.data(), a.size() * sizeof(ParticleInstance)) != 0,
                 "semente diferente, partículas diferentes");
  // A semente automática é estável por objeto e difere entre objetos.
  AE_EXPECT_EQ(runtime::particleSeed(settings, 7, 1), runtime::particleSeed(settings, 7, 1), "semente estável");
  AE_EXPECT_TRUE(runtime::particleSeed(settings, 7, 1) != runtime::particleSeed(settings, 8, 1), "objetos diferentes");
  settings.autoRandomSeed = false;
  settings.randomSeed = 99;
  AE_EXPECT_EQ(runtime::particleSeed(settings, 7, 1), 99u, "semente manual");
}

AE_TEST(particle_emission_rate_bursts_delay_and_loop) {
  scene::ParticleSystem settings;
  settings.duration = 1;
  settings.looping = false;
  settings.startLifetime = scene::particleCurveDefault(10);
  settings.emissionRate = scene::particleCurveDefault(100);
  // Taxa: 100/s por meio segundo ≈ 50 (o acumulador não perde frações).
  ParticleEmitter emitter;
  emitter.configure(settings, 1);
  emitter.play();
  for (u32 i = 0; i < 30; ++i) emitter.simulate(1.0f / 60.0f, kIdentity);
  AE_EXPECT_TRUE(emitter.alive() >= 49 && emitter.alive() <= 50, "taxa por tempo");
  for (u32 i = 0; i < 60; ++i) emitter.simulate(1.0f / 60.0f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 100u, "um ciclo de 1 s a 100/s");
  AE_EXPECT_TRUE(!emitter.emitting(), "sem repetir, para de emitir");

  // Rajadas: 3 ciclos de 10 a cada 0,25 s, mais uma de 7 em 0,9 s.
  settings.emissionRate = scene::particleCurveDefault(0);
  settings.burstCount = 2;
  settings.bursts[0] = {0, 10, 10, 3, 0.25f, 1};
  settings.bursts[1] = {0.9f, 7, 7, 1, 0.01f, 1};
  emitter = {};
  emitter.configure(settings, 1);
  emitter.play();
  emitter.simulate(0.01f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 10u, "rajada no tempo zero");
  for (u32 i = 0; i < 50; ++i) emitter.simulate(0.01f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 30u, "três ciclos até 0,5 s");
  for (u32 i = 0; i < 50; ++i) emitter.simulate(0.01f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 37u, "segunda rajada");

  // Probabilidade zero nunca emite; atraso inicial adia a primeira rajada.
  settings.burstCount = 1;
  settings.bursts[0] = {0, 10, 10, 1, 0.01f, 0};
  emitter = {};
  emitter.configure(settings, 1);
  emitter.play();
  emitter.simulate(0.5f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 0u, "probabilidade zero");
  settings.bursts[0] = {0, 10, 10, 1, 0.01f, 1};
  settings.startDelay = scene::particleCurveDefault(0.5f);
  emitter = {};
  emitter.configure(settings, 1);
  emitter.play();
  emitter.simulate(0.2f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 0u, "ainda no atraso");
  emitter.simulate(0.2f, kIdentity);
  emitter.simulate(0.2f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 10u, "rajada depois do atraso");

  // Repetir: a rajada do tempo zero volta a cada ciclo.
  settings.startDelay = scene::particleCurveDefault(0);
  settings.looping = true;
  emitter = {};
  emitter.configure(settings, 1);
  emitter.play();
  for (u32 i = 0; i < 250; ++i) emitter.simulate(0.01f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 30u, "uma rajada por ciclo em 2,5 ciclos");
}

AE_TEST(particle_lifecycle_play_stop_pause_clear_emit_and_finish) {
  scene::ParticleSystem settings;
  settings.duration = 0.5f;
  settings.looping = false;
  settings.startLifetime = scene::particleCurveDefault(0.3f);
  settings.emissionRate = scene::particleCurveDefault(100);
  ParticleEmitter emitter;
  emitter.configure(settings, 3);
  AE_EXPECT_TRUE(emitter.state() == ParticleEmitter::State::Stopped, "começa parado");
  emitter.emit(5, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 5u, "Emit funciona parado");
  emitter.clear();
  AE_EXPECT_EQ(emitter.alive(), 0u, "Clear");
  emitter.play();
  emitter.simulate(0.1f, kIdentity);
  const u32 before = emitter.alive();
  emitter.pause();
  emitter.simulate(0.1f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), before, "Pause congela");
  emitter.play();
  emitter.stop(false);
  AE_EXPECT_TRUE(!emitter.emitting() && emitter.alive() == before, "Stop deixa as vivas");
  for (u32 i = 0; i < 40; ++i) emitter.simulate(0.02f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 0u, "vivas terminam a vida");
  AE_EXPECT_TRUE(emitter.takeFinished(), "terminou");
  AE_EXPECT_TRUE(!emitter.takeFinished(), "uma vez só");
  emitter.play();
  emitter.simulate(0.1f, kIdentity);
  emitter.stop(true);
  AE_EXPECT_EQ(emitter.alive(), 0u, "Stop com limpar");
  // Teto: o nível do aparelho corta sem mudar o autoral.
  settings.maxParticles = 500;
  settings.looping = true;
  settings.emissionRate = scene::particleCurveDefault(100000);
  settings.startLifetime = scene::particleCurveDefault(5);
  emitter = {};
  emitter.configure(settings, 3);
  emitter.setCapacityLimit(120);
  emitter.play();
  emitter.simulate(0.1f, kIdentity);
  AE_EXPECT_EQ(emitter.alive(), 120u, "teto do nível");
  AE_EXPECT_EQ(emitter.settings().maxParticles, 500.0f, "autoral intacto");
}

AE_TEST(particle_shapes_stay_inside_their_volumes) {
  scene::ParticleSystem settings;
  settings.startSpeed = scene::particleCurveDefault(0);
  settings.startLifetime = scene::particleCurveDefault(100);
  settings.emissionRate = scene::particleCurveDefault(0);
  settings.maxParticles = 2000;
  const auto sample = [&](scene::ParticleShapeType type, auto check) {
    settings.shapeType = type;
    ParticleEmitter emitter;
    emitter.configure(settings, 77);
    emitter.emit(1000, kIdentity);
    std::vector<ParticleInstance> out;
    emitter.build(kIdentity, kCamera, out);
    bool ok = out.size() == 1000;
    for (const auto &p : out) ok = ok && check(p.position[0], p.position[1], p.position[2]);
    return ok;
  };
  settings.shapeRadius = 2;
  settings.shapeRadiusThickness = 1;
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Sphere, [](float x, float y, float z) { return std::sqrt(x * x + y * y + z * z) <= 2.0001f; }), "esfera");
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Hemisphere, [](float x, float y, float z) { return z >= 0 && std::sqrt(x * x + y * y + z * z) <= 2.0001f; }), "hemisfério");
  settings.shapeRadiusThickness = 0;
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Sphere, [](float x, float y, float z) { return std::abs(std::sqrt(x * x + y * y + z * z) - 2) < 1e-3f; }), "casca da esfera");
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Circle, [](float x, float y, float z) { return z == 0 && std::abs(std::hypot(x, y) - 2) < 1e-3f; }), "borda do círculo");
  settings.shapeRadiusThickness = 1;
  settings.shapeArc = 90;
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Cone, [](float x, float y, float z) { return z == 0 && x >= -1e-4f && y >= -1e-4f && std::hypot(x, y) <= 2.0001f; }), "base do cone com arco de 90°");
  settings.shapeArc = 360;
  settings.shapeBox[0] = 2; settings.shapeBox[1] = 4; settings.shapeBox[2] = 6;
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Box, [](float x, float y, float z) { return std::abs(x) <= 1 && std::abs(y) <= 2 && std::abs(z) <= 3; }), "caixa");
  settings.shapeBoxEmitFrom = scene::ParticleBoxEmitFrom::Shell;
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Box, [](float x, float y, float z) {
    return std::abs(std::abs(x) - 1) < 1e-4f || std::abs(std::abs(y) - 2) < 1e-4f || std::abs(std::abs(z) - 3) < 1e-4f; }), "casca da caixa");
  AE_EXPECT_TRUE(sample(scene::ParticleShapeType::Edge, [](float x, float y, float z) { return std::abs(x) <= 2 && y == 0 && z == 0; }), "aresta");
  // Direções: cone de 0° e raio 0 sai em +Z; aresta sai em +Y.
  settings.shapeType = scene::ParticleShapeType::Cone;
  settings.shapeAngle = 0;
  settings.shapeRadius = 0;
  settings.startSpeed = scene::particleCurveDefault(3);
  ParticleEmitter emitter;
  emitter.configure(settings, 1);
  emitter.emit(10, kIdentity);
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  AE_EXPECT_TRUE(std::abs(out[0].velocity[2] - 3) < 1e-4f, "cone sai em +Z");
  // Espaço mundo: um objeto deslocado nasce no lugar dele.
  settings.simulationSpace = scene::ParticleSimulationSpace::World;
  float moved[16];
  std::copy(kIdentity, kIdentity + 16, moved);
  moved[12] = 5;
  emitter = {};
  emitter.configure(settings, 1);
  emitter.emit(1, moved);
  out.clear();
  emitter.build(kIdentity, kCamera, out);
  AE_EXPECT_TRUE(std::abs(out[0].position[0] - 5) < 1e-4f, "espaço mundo usa a matriz do objeto");
}

AE_TEST(particle_curves_gradients_over_lifetime_and_sorting) {
  scene::ParticleSystem settings;
  settings.startSpeed = scene::particleCurveDefault(0);
  settings.startLifetime = scene::particleCurveDefault(1);
  settings.startSize = scene::particleCurveDefault(2);
  settings.emissionRate = scene::particleCurveDefault(0);
  settings.sizeOverLifetimeEnabled = true;
  settings.sizeOverLifetime.curveMax = scene::particleLinearCurve(1, 0);  // encolhe até zero
  settings.colorOverLifetimeEnabled = true;  // padrão: alfa 1 → 0
  settings.rotationOverLifetimeEnabled = true;
  settings.rotationOverLifetime = scene::particleCurveDefault(90);
  ParticleEmitter emitter;
  emitter.configure(settings, 5);
  emitter.play();
  emitter.emit(1, kIdentity);
  for (u32 i = 0; i < 50; ++i) emitter.simulate(0.01f, kIdentity);  // meia vida
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  AE_EXPECT_EQ(out.size(), usize(1), "uma partícula");
  AE_EXPECT_TRUE(std::abs(out[0].size - 1.0f) < 0.05f, "tamanho na metade da vida");
  AE_EXPECT_TRUE(std::abs(renderer::particles::halfToFloat(out[0].color[3]) - 0.5f) < 0.03f, "alfa na metade da vida");
  AE_EXPECT_TRUE(std::abs(out[0].rotation - 0.5f * 3.14159265f / 2) < 0.02f, "45° girados em meio segundo a 90°/s");
  // TwoConstants: sorteio dentro dos limites.
  scene::ParticleMinMaxCurve between = scene::particleCurveDefault(5);
  between.mode = scene::ParticleCurveMode::TwoConstants;
  between.constantMin = 1;
  renderer::particles::BakedMinMaxCurve baked;
  baked.bake(between);
  AE_EXPECT_EQ(baked.evaluate(0.3f, 0), 1.0f, "mínimo");
  AE_EXPECT_EQ(baked.evaluate(0.3f, 1), 5.0f, "máximo");
  // Ordenação por distância: a mais distante da câmera primeiro.
  settings.sortMode = scene::ParticleSortMode::ByDistance;
  settings.shapeType = scene::ParticleShapeType::Box;
  settings.shapeBox[2] = 10;
  settings.sizeOverLifetimeEnabled = false;
  emitter = {};
  emitter.configure(settings, 9);
  emitter.emit(50, kIdentity);
  out.clear();
  emitter.build(kIdentity, kCamera, out);
  bool sorted = true;
  for (usize i = 1; i < out.size(); ++i) {
    const auto d = [](const ParticleInstance &p) {
      return (p.position[0] - kCamera[0]) * (p.position[0] - kCamera[0]) + (p.position[1] - kCamera[1]) * (p.position[1] - kCamera[1]) +
             (p.position[2] - kCamera[2]) * (p.position[2] - kCamera[2]);
    };
    sorted = sorted && d(out[i - 1]) >= d(out[i]);
  }
  AE_EXPECT_TRUE(sorted, "de trás para a frente");
  AE_EXPECT_EQ(renderer::particles::halfToFloat(renderer::particles::floatToHalf(3.5f)), 3.5f, "meia precisão exata");
}

AE_TEST(particle_scene_world_play_on_awake_budget_and_build_order) {
  runtime::SceneGraph graph;
  const auto near = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Perto");
  const auto far = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Longe");
  runtime::Transform t;
  t.position[2] = 20;
  AE_EXPECT_TRUE(graph.setTransform(far, t), "posição");
  for (const auto id : {near, far}) {
    auto *components = graph.editComponents(id);
    auto *value = components ? components->add(scene::ParticleSystem::descriptor) : nullptr;
    AE_EXPECT_TRUE(value != nullptr, "componente na cena");
    auto &system = scene::particleSystem(*value);
    system.maxParticles = 3000;
    system.emissionRate = scene::particleCurveDefault(100000);
  }
  runtime::SceneParticles particles;
  particles.setBudget(renderer::particles::particleTierBudget(renderer::particles::ParticleTier::C));
  particles.advance(graph, 0.1f);
  AE_EXPECT_EQ(particles.emitterCount(), 2u, "dois emissores");
  AE_EXPECT_TRUE(particles.aliveTotal() <= 1000 && particles.aliveTotal() >= 998, "teto do nível C repartido");
  {
    // Um sistema parado (taxa zero, máximo alto) não toma a fatia de quem emite.
    runtime::SceneGraph rainy;
    const auto rain = rainy.createEntity(rainy.root(), runtime::ObjectKind::Folder, "Chuva");
    const auto snow = rainy.createEntity(rainy.root(), runtime::ObjectKind::Folder, "Neve");
    auto &r = scene::particleSystem(*rainy.editComponents(rain)->add(scene::ParticleSystem::descriptor));
    r.maxParticles = 8000;r.emissionRate = scene::particleCurveDefault(3000);r.startLifetime = scene::particleCurveDefault(1.6f);
    auto &n = scene::particleSystem(*rainy.editComponents(snow)->add(scene::ParticleSystem::descriptor));
    n.maxParticles = 12000;n.emissionRate = scene::particleCurveDefault(0);
    runtime::SceneParticles weather;
    weather.setBudget(8000);
    weather.advance(rainy, 0.0f);
    AE_EXPECT_EQ(weather.capacityOf(rain, rainy.find(rain)->components.at(0)->instanceId()), 8000u, "a chuva cabe inteira");
  }
  renderer::particles::ParticleFrame frame;
  const float camera[3]{0, 0, -5};
  particles.build(graph, camera, frame);
  AE_EXPECT_EQ(frame.batches.size(), usize(2), "um lote por sistema");
  AE_EXPECT_TRUE(frame.batches[0].sortDistance > frame.batches[1].sortDistance, "sistema distante primeiro");
  AE_EXPECT_EQ(frame.alive, particles.aliveTotal(), "contagem do quadro");
  particles.setBudget(renderer::particles::particleTierBudget(renderer::particles::ParticleTier::B));
  // Desativar apaga; a remoção do componente remove o emissor.
  AE_EXPECT_TRUE(graph.setActive(far, false), "desativar");
  particles.advance(graph, 0.01f);
  runtime::SceneParticles::Status status;
  AE_EXPECT_TRUE(particles.status(graph, far, graph.find(far)->components.at(0)->instanceId(), status) && status.alive == 0, "inativo sem partículas");
  auto *components = graph.editComponents(near);
  AE_EXPECT_TRUE(components->remove(scene::ParticleSystem::descriptor), "remover");
  graph.setName(near, "Perto2");  // nova revisão
  particles.advance(graph, 0.01f);
  AE_EXPECT_EQ(particles.emitterCount(), 1u, "emissor removido");
  // Comando por id persistente.
  const auto instance = graph.find(far)->components.at(0)->instanceId();
  AE_EXPECT_TRUE(particles.command(graph, far, instance, runtime::SceneParticles::Command::Emit, 4), "Emit");
  AE_EXPECT_TRUE(!particles.command(graph, near, 1, runtime::SceneParticles::Command::Play), "componente ausente recusado");
}

AE_TEST(particle_textures_are_deterministic_tileable_and_shaped) {
  const auto a = renderer::particles::generateParticleTextures(64);
  const auto b = renderer::particles::generateParticleTextures(64);
  AE_EXPECT_TRUE(a.texels == b.texels, "bytes determinísticos");
  AE_EXPECT_EQ(a.layers, renderer::particles::kParticleTextureLayers, "14 camadas");
  AE_EXPECT_EQ(a.size, 256u, "atlas 4x4");
  AE_EXPECT_EQ(a.levels, renderer::particles::kParticleAtlasLevels, "mips limitados pela borda");
  AE_EXPECT_TRUE(a.texel(0, .5f, .5f)[3] > 240 && a.texel(0, 0, 0)[3] == 0, "ponto suave: miolo cheio, canto vazio");
  AE_EXPECT_TRUE(a.texel(2, .5f, .8f)[3] > a.texel(2, .5f, .1f)[3], "chama mais densa embaixo");
  // Ruído encaixa nas bordas: a primeira e a última coluna úteis são vizinhas.
  const u32 clouds = renderer::particles::particleNoiseLayer(scene::ParticleNoise::Clouds);
  int seam = 0, interior = 0;
  for (u32 y = 0; y < 56; ++y) {
    const float v = (static_cast<float>(y) + .5f) / 56;
    seam += std::abs(int(a.texel(clouds, 0.f, v)[3]) - int(a.texel(clouds, .999f, v)[3]));
    interior += std::abs(int(a.texel(clouds, .5f, v)[3]) - int(a.texel(clouds, .5f + 1.f / 56, v)[3]));
  }
  AE_EXPECT_TRUE(seam <= interior * 3 + 64, "ruído sem costura");
  AE_EXPECT_EQ(renderer::particles::particleNoiseLayer(scene::ParticleNoise::None), ~0u, "sem ruído, sem camada");
  AE_EXPECT_EQ(renderer::particles::particleMaskLayer(scene::ParticleMask::Ring), 13u, "última camada");
}

// --- P2a: Velocity, Limit Velocity, Force over Lifetime e Noise -----------
namespace {
// Uma partícula parada na origem, vida longa, para medir só o módulo.
scene::ParticleSystem still() {
  scene::ParticleSystem s;
  s.shapeEnabled = false;
  s.emissionEnabled = false;
  s.startSpeed = scene::particleCurveDefault(0);
  s.startLifetime = scene::particleCurveDefault(100);
  s.gravityModifier = scene::particleCurveDefault(0);
  s.simulationSpace = scene::ParticleSimulationSpace::World;
  return s;
}
std::vector<ParticleInstance> one(const scene::ParticleSystemSettings &settings, u32 frames, float dt,
                                  const float (&world)[16] = kIdentity) {
  ParticleEmitter emitter;
  emitter.configure(settings, 3);
  emitter.play();
  emitter.emit(1, world);
  for (u32 i = 0; i < frames; ++i) emitter.simulate(dt, world);
  std::vector<ParticleInstance> out;
  emitter.build(world, kCamera, out);
  return out;
}
} // namespace

AE_TEST(particle_p2a_archive_v1_opens_with_modules_off_and_v2_round_trips) {
  std::istringstream old("1 duration 2 0 0 0");
  scene::ParticleSystem fromV1;
  AE_EXPECT_TRUE(fromV1.read(old, 1), "arquivo v1 aceito");
  AE_EXPECT_TRUE(!fromV1.velocityOverLifetimeEnabled && !fromV1.forceOverLifetimeEnabled && !fromV1.limitVelocityEnabled &&
                 !fromV1.noiseEnabled, "módulos novos desligados");
  scene::ParticleSystem source;
  source.forceOverLifetimeEnabled = true;source.forceX = scene::particleCurveDefault(3);
  source.forceSpace = scene::ParticleSimulationSpace::Local;
  source.noiseEnabled = true;source.noiseFrequency = 1.5f;source.noiseOctaves = 3;
  source.limitVelocityEnabled = true;source.limitDrag = 0.4f;
  source.velocityOverLifetimeEnabled = true;source.velocityY.mode = scene::ParticleCurveMode::Curve;
  source.velocityY.curveMax = scene::particleLinearCurve(0, 2);source.velocityY.multiplier = 4;
  scene::ParticleSystem back;
  AE_EXPECT_TRUE(roundTrip(source, back), "ida e volta v2");
  AE_EXPECT_TRUE(back.forceOverLifetimeEnabled && back.forceX.constantMax == 3 &&
                 back.forceSpace == scene::ParticleSimulationSpace::Local && back.noiseOctaves == 3 &&
                 back.limitDrag == 0.4f && back.velocityY.mode == scene::ParticleCurveMode::Curve &&
                 back.velocityY.multiplier == 4 && back.velocityY.curveMax.keys.size() == 2, "módulos preservados");
  scene::ParticleSystem fraction = source;fraction.noiseOctaves = 2.5f;
  AE_EXPECT_TRUE(!fraction.valid(), "oitavas inteiras");
}

AE_TEST(particle_p2a_force_accelerates_and_velocity_adds_without_accumulating) {
  auto force = still();
  force.forceOverLifetimeEnabled = true;force.forceX = scene::particleCurveDefault(2);
  const auto pushed = one(force, 60, 1.0f / 60);
  // x = a t² / 2 = 1 m depois de 1 s (integração explícita: um pouco acima).
  AE_EXPECT_TRUE(pushed.size() == 1 && pushed[0].position[0] > 0.95f && pushed[0].position[0] < 1.1f, "força acelera");
  AE_EXPECT_TRUE(std::abs(pushed[0].velocity[0] - 2.0f) < 0.05f, "velocidade acumulada pela força");
  auto velocity = still();
  velocity.velocityOverLifetimeEnabled = true;velocity.velocityX = scene::particleCurveDefault(2);
  velocity.velocitySpace = scene::ParticleSimulationSpace::World;
  const auto moved = one(velocity, 60, 1.0f / 60);
  AE_EXPECT_TRUE(std::abs(moved[0].position[0] - 2.0f) < 0.05f, "velocidade no tempo: 2 m em 1 s, sem acelerar");
  AE_EXPECT_TRUE(std::abs(moved[0].velocity[0] - 2.0f) < 1e-3f, "o traço esticado vê a velocidade animada");
  velocity.speedModifier = scene::particleCurveDefault(0.5f);
  AE_EXPECT_TRUE(std::abs(one(velocity, 60, 1.0f / 60)[0].position[0] - 1.0f) < 0.05f, "modificador de velocidade escala");
  // Força de mundo num emissor Local girado 90° em Y continua apontando +X do mundo.
  const float turned[16]{0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
  auto local = force;local.simulationSpace = scene::ParticleSimulationSpace::Local;
  local.forceSpace = scene::ParticleSimulationSpace::World;
  const auto world = one(local, 60, 1.0f / 60, turned);
  AE_EXPECT_TRUE(world[0].position[0] > 0.95f && std::abs(world[0].position[2]) < 1e-3f, "vento de mundo não gira com o objeto");
}

AE_TEST(particle_p2a_limit_velocity_clamps_and_drag_slows) {
  auto fast = still();
  fast.startSpeed = scene::particleCurveDefault(10);
  fast.shapeEnabled = true;fast.shapeType = scene::ParticleShapeType::Box;fast.shapeBox[0] = fast.shapeBox[1] = fast.shapeBox[2] = 0;
  fast.limitVelocityEnabled = true;fast.limitSpeed = scene::particleCurveDefault(3);fast.limitDampen = 1;
  const auto clamped = one(fast, 10, 1.0f / 60);
  const float speed = std::sqrt(clamped[0].velocity[0] * clamped[0].velocity[0] + clamped[0].velocity[1] * clamped[0].velocity[1] +
                                clamped[0].velocity[2] * clamped[0].velocity[2]);
  AE_EXPECT_TRUE(std::abs(speed - 3.0f) < 1e-3f, "Dampen 1 corta no limite");
  fast.limitDampen = 0.2f;
  const auto soft = one(fast, 1, 1.0f / 30);
  const float softSpeed = std::abs(soft[0].velocity[2]);
  AE_EXPECT_TRUE(std::abs(softSpeed - (3.0f + 7.0f * 0.8f)) < 0.05f, "Dampen 0,2 tira 20% do excesso por quadro de 30 Hz");
  fast.limitSpeed = scene::particleCurveDefault(1000);fast.limitDrag = 1;
  const auto dragged = one(fast, 30, 1.0f / 60);
  AE_EXPECT_TRUE(std::abs(dragged[0].velocity[2]) < 10.0f * 0.65f && std::abs(dragged[0].velocity[2]) > 10.0f * 0.55f,
                 "arrasto de 1/s: cerca de 60% depois de meio segundo");
}

AE_TEST(particle_p2a_noise_is_deterministic_and_turbulent) {
  auto noisy = still();
  noisy.noiseEnabled = true;noisy.noiseStrength = scene::particleCurveDefault(2);noisy.noiseFrequency = 0.8f;
  noisy.noiseOctaves = 2;noisy.noiseScrollSpeed = 0.5f;
  const auto a = one(noisy, 90, 1.0f / 60), b = one(noisy, 90, 1.0f / 60);
  AE_EXPECT_TRUE(std::memcmp(a.data(), b.data(), sizeof(ParticleInstance)) == 0, "mesma semente, mesma trajetória");
  const float moved = std::sqrt(a[0].position[0] * a[0].position[0] + a[0].position[1] * a[0].position[1] +
                                a[0].position[2] * a[0].position[2]);
  AE_EXPECT_TRUE(moved > 0.05f && moved < 5.5f, "o ruído desloca sem passar da força (2 m/s por eixo, 1,5 s)");
  noisy.noiseEnabled = false;
  const auto calm = one(noisy, 90, 1.0f / 60);
  AE_EXPECT_TRUE(calm[0].position[0] == 0 && calm[0].position[1] == 0 && calm[0].position[2] == 0, "desligado não mexe");
}

// --- P2b: Colisão com planos e Sub-emissores -------------------------------
AE_TEST(particle_p2b_collision_plane_bounces_dampens_and_kills) {
  auto falling = still();
  falling.gravityModifier = scene::particleCurveDefault(1);
  falling.startSize = scene::particleCurveDefault(0);
  falling.collisionEnabled = true;falling.collisionBounce = 0.5f;falling.collisionDampen = 0;
  const float above[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 2, 0, 1};  // nasce 2 m acima do plano y=0
  ParticleEmitter emitter;
  emitter.configure(falling, 5);
  emitter.play();
  emitter.emit(1, above);
  const renderer::particles::ParticleCollisionPlane ground{};
  emitter.setCollisionPlanes(&ground, 1);
  float lowest = 10, upward = 0;
  for (u32 i = 0; i < 120; ++i) {
    emitter.simulate(1.0f / 60, above);
    std::vector<ParticleInstance> out;
    emitter.build(above, kCamera, out);
    lowest = std::min(lowest, out[0].position[1]);
    upward = std::max(upward, out[0].velocity[1]);
  }
  AE_EXPECT_TRUE(lowest >= -1e-4f, "não atravessa o chão");
  // Queda de 2 m: ~6,26 m/s; quique 0,5 devolve ~3 m/s para cima.
  AE_EXPECT_TRUE(upward > 2.5f && upward < 3.5f, "quique devolve metade da velocidade");
  falling.collisionLifetimeLoss = 1;
  falling.subEmittersEnabled = true;falling.subEmitterDeath = 9;falling.subEmitterCollision = 9;
  emitter.configure(falling, 5);
  emitter.clear();
  emitter.emit(1, above);
  u32 collisions = 0, deaths = 0;
  for (u32 i = 0; i < 120; ++i) {
    emitter.clearEvents();
    emitter.simulate(1.0f / 60, above);
    collisions += static_cast<u32>(emitter.collisions().size());
    deaths += static_cast<u32>(emitter.deaths().size());
  }
  AE_EXPECT_TRUE(emitter.alive() == 0 && collisions == 1 && deaths == 1, "perda de vida 1: morre no primeiro toque, com eventos");
}

AE_TEST(particle_p2b_sub_emitters_spawn_where_particles_collide_and_die) {
  runtime::SceneGraph graph;
  const auto parent = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Gotas");
  const auto splash = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Respingo");
  const auto floor = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Chão");
  {
    auto values = *graph.find(splash);
    auto *p = static_cast<scene::ParticleSystem *>(values.components.add(scene::ParticleSystem::descriptor));
    p->emissionEnabled = false;p->shapeEnabled = false;p->startLifetime = scene::particleCurveDefault(5);
    p->startSpeed = scene::particleCurveDefault(0);p->gravityModifier = scene::particleCurveDefault(0);
    p->simulationSpace = scene::ParticleSimulationSpace::World;
    graph.applyEntityValues(splash, values);
  }
  {
    auto values = *graph.find(parent);
    values.transform.position[1] = 1;values.transform.position[0] = 4;
    auto *p = static_cast<scene::ParticleSystem *>(values.components.add(scene::ParticleSystem::descriptor));
    *static_cast<scene::ParticleSystemSettings *>(p) = still();
    p->emissionEnabled = true;p->emissionRate = scene::particleCurveDefault(0);
    p->burstCount = 1;p->bursts[0].countMin = p->bursts[0].countMax = 3;
    p->gravityModifier = scene::particleCurveDefault(1);p->startSize = scene::particleCurveDefault(0);
    p->collisionEnabled = true;p->collisionPlane0 = floor;p->collisionLifetimeLoss = 1;
    p->subEmittersEnabled = true;p->subEmitterCollision = splash;p->subEmitterCollisionCount = 2;
    p->subEmitterDeath = splash;p->subEmitterDeathCount = 1;
    graph.applyEntityValues(parent, values);
  }
  runtime::SceneParticles particles;
  for (u32 i = 0; i < 60; ++i) particles.advance(graph, 1.0f / 60);
  const auto instance = graph.find(splash)->components.find(scene::ParticleSystem::descriptor)->instanceId();
  runtime::SceneParticles::Status status;
  AE_EXPECT_TRUE(particles.status(graph, splash, instance, status) && status.alive == 3 * (2 + 1), "2 no toque + 1 na morte, por gota");
  renderer::particles::ParticleFrame frame;
  particles.build(graph, kCamera, frame);
  bool onFloor = true;
  for (const auto &p : frame.instances) if (std::abs(p.position[0] - 4) > 1e-4f || std::abs(p.position[1]) > 1e-3f) onFloor = false;
  AE_EXPECT_TRUE(frame.alive == status.alive && onFloor, "respingos no ponto do toque, no chão");
  // Plano desativado: a gota passa direto e não há respingo.
  graph.setActive(floor, false);
  runtime::SceneParticles fresh;
  for (u32 i = 0; i < 60; ++i) fresh.advance(graph, 1.0f / 60);
  AE_EXPECT_TRUE(fresh.status(graph, splash, instance, status) && status.alive == 0, "sem plano a gota não toca nem morre: nenhum respingo");
}

// --- P2c: Herdar velocidade, Taxa por distância e "pela velocidade" --------
AE_TEST(particle_p2c_inherit_velocity_and_rate_over_distance_follow_the_emitter) {
  auto moving = still();
  moving.emissionEnabled = true;moving.emissionRate = scene::particleCurveDefault(0);
  moving.emissionRateOverDistance = 10;
  moving.inheritVelocityEnabled = true;moving.inheritVelocityMultiplier = 0.5f;
  moving.maxParticles = 1000;
  ParticleEmitter emitter;
  emitter.configure(moving, 1);
  emitter.play();
  float world[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  // 4 m/s em +X por 1 s: 4 m andados, 40 partículas, cada uma com +2 m/s.
  for (u32 i = 0; i < 60; ++i) {
    world[12] = 4.0f * static_cast<float>(i) / 60.0f;
    emitter.simulate(1.0f / 60, world);
  }
  AE_EXPECT_TRUE(emitter.alive() >= 38 && emitter.alive() <= 40, "10 por metro em 3,93 m");
  std::vector<ParticleInstance> out;
  emitter.build(world, kCamera, out);
  bool inherited = true;
  for (const auto &p : out) inherited = inherited && std::abs(p.velocity[0] - 2.0f) < 1e-3f;
  AE_EXPECT_TRUE(inherited, "metade da velocidade do emissor");
  ParticleEmitter parked;
  parked.configure(moving, 1);
  parked.play();
  for (u32 i = 0; i < 60; ++i) parked.simulate(1.0f / 60, kIdentity);
  AE_EXPECT_TRUE(parked.alive() == 0, "parado não emite por distância");
}

AE_TEST(particle_p2c_size_and_color_by_speed_read_the_speed_range) {
  auto fast = still();
  fast.shapeEnabled = true;fast.shapeType = scene::ParticleShapeType::Box;fast.shapeBox[0] = fast.shapeBox[1] = fast.shapeBox[2] = 0;
  fast.startSpeed = scene::particleCurveDefault(5);
  fast.startSize = scene::particleCurveDefault(2);
  fast.sizeBySpeedEnabled = true;fast.sizeBySpeedRange[0] = 0;fast.sizeBySpeedRange[1] = 10;  // curva linear 0→1
  fast.colorBySpeedEnabled = true;fast.colorBySpeedRange[0] = 0;fast.colorBySpeedRange[1] = 5;  // gradiente de alfa 1→0
  const auto p = one(fast, 1, 1.0f / 60);
  AE_EXPECT_TRUE(std::abs(p[0].size - 1.0f) < 0.02f, "5 m/s na faixa 0–10: metade do tamanho");
  AE_EXPECT_TRUE(renderer::particles::halfToFloat(p[0].color[3]) < 0.02f, "no topo da faixa de cor: alfa do fim do gradiente");
  fast.rotationBySpeedEnabled = true;fast.rotationBySpeed = scene::particleCurveDefault(90);
  fast.rotationBySpeedRange[0] = 0;fast.rotationBySpeedRange[1] = 5;
  const auto spun = one(fast, 60, 1.0f / 60);
  AE_EXPECT_TRUE(std::abs(spun[0].rotation - 3.14159265f / 2) < 0.02f, "90°/s constante por 1 s");
}

AE_TEST(particle_p2d_donut_and_rectangle_shapes_stay_in_their_volumes) {
  scene::ParticleSystem donut;
  donut.shapeType = scene::ParticleShapeType::Donut;donut.shapeRadius = 2;donut.shapeDonutRadius = 0.25f;
  donut.startSpeed = scene::particleCurveDefault(0);donut.startLifetime = scene::particleCurveDefault(10);
  donut.gravityModifier = scene::particleCurveDefault(0);donut.emissionRate = scene::particleCurveDefault(600);
  bool inside = true;
  for (const auto &p : run(donut, 9, 30, 1.0f / 30)) {
    const float ring = std::sqrt(p.position[0] * p.position[0] + p.position[1] * p.position[1]);
    const float tube = std::sqrt((ring - 2) * (ring - 2) + p.position[2] * p.position[2]);
    inside = inside && tube <= 0.25f + 1e-4f;
  }
  AE_EXPECT_TRUE(inside, "rosca: todo ponto dentro do tubo em volta do anel");
  scene::ParticleSystem rect = donut;
  rect.shapeType = scene::ParticleShapeType::Rectangle;rect.shapeBox[0] = 4;rect.shapeBox[1] = 1;
  rect.startSpeed = scene::particleCurveDefault(1);
  bool flat = true;
  for (const auto &p : run(rect, 9, 2, 1.0f / 30)) {
    flat = flat && std::abs(p.position[0]) <= 2 + 1e-4f && std::abs(p.position[1]) <= .5f + 1e-4f && p.velocity[2] > 0.99f;
  }
  AE_EXPECT_TRUE(flat, "retângulo 4×1 no plano XY emitindo para +Z");
}

// PK-10a: Texture Sheet Animation (modo Grid da Unity) e Material de projeto.
AE_TEST(particle_pk10a_texture_sheet_walks_the_grid_over_lifetime) {
  AE_EXPECT_EQ(sizeof(ParticleInstance), usize(48), "instância com o quadro da folha");
  scene::ParticleSystemSettings sheet;
  sheet.startLifetime = scene::particleCurveDefault(1);
  sheet.startSpeed = scene::particleCurveDefault(0);
  sheet.emissionRate = scene::particleCurveDefault(0);
  sheet.textureSheetEnabled = true;
  sheet.sheetTiles[0] = 4;sheet.sheetTiles[1] = 2;
  const auto frameAt = [&](const scene::ParticleSystemSettings &settings, float age) {
    ParticleEmitter emitter;
    emitter.configure(settings, 3);
    emitter.play();
    emitter.emit(1, kIdentity);
    // Passos de 0,1 s: o passo da simulação é limitado a 0,25 s.
    for (float left = age; left > 1e-6f; left -= 0.1f) emitter.simulate(std::min(left, 0.1f), kIdentity);
    std::vector<ParticleInstance> out;
    emitter.build(kIdentity, kCamera, out);
    return out.empty() ? -1.0f : out[0].frame;
  };
  AE_EXPECT_EQ(frameAt(sheet, 0), 0.0f, "nasce no primeiro quadro");
  AE_EXPECT_EQ(frameAt(sheet, 0.26f), 2.0f, "um quarto da vida: quadro 2 de 8");
  AE_EXPECT_EQ(frameAt(sheet, 0.9f), 7.0f, "fim da vida: último quadro");
  auto twice = sheet;twice.sheetCycles = 2;
  AE_EXPECT_EQ(frameAt(twice, 0.55f), 0.0f, "dois ciclos: recomeça na metade");
  auto start = sheet;start.sheetStartFrame = scene::particleCurveDefault(3);
  AE_EXPECT_EQ(frameAt(start, 0.9f), 2.0f, "quadro inicial soma e dá a volta (7 + 3 = 10 → 2)");
  auto row = sheet;row.sheetAnimation = scene::ParticleSheetAnimation::SingleRow;row.sheetRow = 1;
  AE_EXPECT_EQ(frameAt(row, 0), 4.0f, "uma linha: começa na linha escolhida");
  AE_EXPECT_EQ(frameAt(row, 0.9f), 7.0f, "e anda só pelas colunas dela");
  auto random = row;random.sheetRowMode = scene::ParticleSheetRowMode::Random;
  bool inside = true;
  for (u32 seed = 0; seed < 16; ++seed) {
    ParticleEmitter emitter;
    emitter.configure(random, seed);
    emitter.play();
    emitter.emit(8, kIdentity);
    std::vector<ParticleInstance> out;
    emitter.build(kIdentity, kCamera, out);
    for (const auto &p : out) inside = inside && (p.frame == 0 || p.frame == 4);
  }
  AE_EXPECT_TRUE(inside, "linha sorteada é o início de uma das linhas");
  auto off = sheet;off.textureSheetEnabled = false;
  AE_EXPECT_EQ(frameAt(off, 0.9f), 0.0f, "módulo desligado: imagem inteira");
  ParticleEmitter emitter;
  emitter.configure(sheet, 1);
  const auto material = emitter.material();
  AE_EXPECT_TRUE(material.sheetTiles[0] == 4 && material.sheetTiles[1] == 2 && material.projectTexture == ~0u,
                 "a grade vai ao lote; sem Material de projeto, textura própria");
}

AE_TEST(particle_pk10a_archive_v3_keeps_material_and_sheet_and_reads_v2) {
  scene::ParticleSystem source;
  source.materialAsset = resources::assetGuidFromSeed("material:fogo");
  source.textureSheetEnabled = true;source.sheetTiles[0] = 8;source.sheetTiles[1] = 4;
  source.sheetAnimation = scene::ParticleSheetAnimation::SingleRow;source.sheetRowMode = scene::ParticleSheetRowMode::Random;
  source.sheetCycles = 3;source.sheetStartFrame.mode = scene::ParticleCurveMode::TwoConstants;
  source.sheetStartFrame.constantMin = 1;source.sheetStartFrame.constantMax = 5;
  source.sheetFrameOverTime.curveMax = scene::particleLinearCurve(0, 0.5f);
  scene::ParticleSystem back;
  AE_EXPECT_TRUE(roundTrip(source, back), "ida e volta v3");
  AE_EXPECT_TRUE(back.materialAsset == source.materialAsset && back.textureSheetEnabled && back.sheetTiles[0] == 8 &&
                 back.sheetTiles[1] == 4 && back.sheetAnimation == scene::ParticleSheetAnimation::SingleRow &&
                 back.sheetRowMode == scene::ParticleSheetRowMode::Random && back.sheetCycles == 3 &&
                 back.sheetStartFrame.constantMin == 1 && back.sheetFrameOverTime.curveMax.keys.size() == 2,
                 "material e folha preservados");
  AE_EXPECT_EQ(scene::ParticleSystem::descriptor.version, 3u, "versão 3");
  AE_EXPECT_EQ(scene::ParticleSystem::descriptor.resourceBindings.size(), usize(1), "binding do Material");
  scene::ParticleSystem none;
  AE_EXPECT_TRUE(roundTrip(scene::ParticleSystem{}, none) && !none.materialAsset.valid(), "sem material grava '-'");
  // Um arquivo v2 não tem o token do material no fim.
  std::istringstream v2("1 duration 2 0 0 0");
  scene::ParticleSystem old;
  AE_EXPECT_TRUE(old.read(v2, 2) && !old.materialAsset.valid() && !old.textureSheetEnabled, "v2 abre com folha desligada");
  std::istringstream v3missing("1 duration 2 0 0 0");
  AE_EXPECT_TRUE(!old.read(v3missing, 3), "v3 sem o material é recusado");
  scene::ParticleSystem fraction = source;fraction.sheetTiles[0] = 2.5f;
  AE_EXPECT_TRUE(!fraction.valid(), "grade inteira");
  fraction = source;fraction.sheetStartFrame.mode = scene::ParticleCurveMode::Curve;
  AE_EXPECT_TRUE(!fraction.valid(), "quadro inicial só constante ou sorteio");
}

AE_TEST(particle_pk10a_project_material_reaches_the_batch) {
  runtime::SceneGraph graph;
  const auto fire = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Fogo");
  auto &system = scene::particleSystem(*graph.editComponents(fire)->add(scene::ParticleSystem::descriptor));
  system.emissionRate = scene::particleCurveDefault(50);
  system.materialTint[0] = 0.8f;
  const auto guid = resources::assetGuidFromSeed("material:fogo");
  system.materialAsset = guid;
  runtime::SceneParticles particles;
  particles.advance(graph, 0.2f);
  renderer::particles::ParticleFrame frame;
  particles.build(graph, kCamera, frame);
  AE_EXPECT_TRUE(!frame.batches.empty() && frame.batches[0].material.projectTexture == ~0u, "sem resolvedor, textura própria");
  u32 asked = 0;
  const runtime::ParticleMaterialResolver resolver = [&](const resources::AssetGuid &id, runtime::ParticleMaterialLook &look) {
    ++asked;
    if (id != guid) return false;
    look.texture = 7;look.color[0] = 0.5f;look.additive = true;
    return true;
  };
  particles.build(graph, kCamera, frame, &resolver);
  AE_EXPECT_TRUE(asked == 1 && frame.batches.size() == 1, "resolvido uma vez por lote");
  const auto &m = frame.batches[0].material;
  AE_EXPECT_TRUE(m.projectTexture == 7 && std::abs(m.tint[0] - 0.4f) < 1e-6f &&
                 m.blend == static_cast<u32>(scene::ParticleBlend::Additive), "imagem, cor base e mistura do Material");
  const runtime::ParticleMaterialResolver refuse = [](const resources::AssetGuid &, runtime::ParticleMaterialLook &) { return false; };
  particles.build(graph, kCamera, frame, &refuse);
  AE_EXPECT_TRUE(frame.batches[0].material.projectTexture == ~0u && frame.batches[0].material.blend == 0,
                 "Material sem shader Partícula: material embutido");
}
