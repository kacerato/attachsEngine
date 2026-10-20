#include "harness.h"

#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "renderer/scene_environment.h"
#include "runtime/scene_environment.h"
#include "runtime/scene_particles.h"
#include "runtime/scene_weather.h"
#include "runtime/transform_math.h"
#include "scene/component_schema.h"
#include "scene/environment.h"
#include "scene/particle_system.h"
#include "scene/weather.h"

#include <cmath>
#include <locale>
#include <sstream>
#include <string>

using namespace ae;

namespace {
struct WeatherScene {
  runtime::SceneGraph graph;
  runtime::ObjectId weather = 0, rain = 0, snow = 0;
};
// Ambiente (volume) + Clima, com Chuva e Neve como filhos: o que a receita monta.
WeatherScene makeWeatherScene(scene::WeatherKind kind, float intensity) {
  WeatherScene s;
  auto &g = s.graph;
  s.weather = g.createEntity(g.root(), runtime::ObjectKind::Folder, "Clima");
  s.rain = g.createEntity(s.weather, runtime::ObjectKind::Folder, "Chuva");
  s.snow = g.createEntity(s.weather, runtime::ObjectKind::Folder, "Neve");
  for (auto id : {s.rain, s.snow}) {
    auto values = *g.find(id);
    auto *p = static_cast<scene::ParticleSystem *>(values.components.add(scene::ParticleSystem::descriptor));
    p->simulationSpace = scene::ParticleSimulationSpace::World;
    p->shapeType = scene::ParticleShapeType::Box;
    p->shapeRotation[0] = 90;
    p->startSpeed = scene::particleCurveDefault(10);
    g.applyEntityValues(id, values);
  }
  auto values = *g.find(s.weather);
  auto *environment = static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
  environment->overrideSky = environment->overridePost = environment->overrideIndirect = false;
  environment->overrideFog = true;
  environment->values.priority = 2;
  auto *weather = static_cast<scene::Weather *>(values.components.add(scene::Weather::descriptor));
  weather->kind = kind;weather->intensity = intensity;
  weather->rain = s.rain;weather->snow = s.snow;
  g.applyEntityValues(s.weather, values);
  return s;
}
const scene::Environment &environmentOf(const runtime::SceneGraph &g, runtime::ObjectId id) {
  return *static_cast<const scene::Environment *>(g.find(id)->components.find(scene::Environment::descriptor));
}
const scene::ParticleSystem &particlesOf(const runtime::SceneGraph &g, runtime::ObjectId id) {
  return scene::particleSystem(*g.find(id)->components.find(scene::ParticleSystem::descriptor));
}
bool writeTransform(runtime::SceneGraph &g, runtime::ObjectId id, const runtime::Transform &t) { return g.setTransform(id, t); }
} // namespace

AE_TEST(weather_component_is_registered_round_trips_and_refuses_unknown_data) {
  const auto *schema = scene::findComponentSchema("astra.environment.weather");
  AE_EXPECT_TRUE(schema && schema->family == scene::ComponentFamily::Rendering, "Clima na família Renderização");
  AE_EXPECT_TRUE(core::engineCapabilityAuthorable("render.environment.weather"), "capacidade com consumidor");
  scene::Weather weather;
  weather.kind = scene::WeatherKind::Snow;weather.intensity = 0.4f;weather.rain = 7;weather.snow = 9;
  weather.followCamera = false;weather.lightningInterval = 20.0f;
  std::ostringstream out;out.imbue(std::locale::classic());weather.write(out);
  scene::Weather back;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(back.read(in, 1), "ida e volta v1");
  AE_EXPECT_TRUE(back.kind == scene::WeatherKind::Snow && back.intensity == 0.4f && back.rain == 7 && back.snow == 9 &&
                 !back.followCamera && back.lightningInterval == 20.0f, "mesmos valores");
  std::istringstream future(out.str());
  AE_EXPECT_TRUE(!scene::Weather{}.read(future, 2), "versão futura recusada");
  std::istringstream badKind("9 0 0 1 1 1 0.5 8 1800 700 10 0.02 9 3");
  AE_EXPECT_TRUE(!scene::Weather{}.read(badKind, 1), "tipo desconhecido recusado");
}

AE_TEST(weather_targets_order_the_sky_from_clear_to_storm) {
  using K = scene::WeatherKind;
  const auto clear = runtime::weatherTarget(K::Clear, 1), cloudy = runtime::weatherTarget(K::Cloudy, 1),
             rain = runtime::weatherTarget(K::Rain, 1), storm = runtime::weatherTarget(K::Storm, 1),
             snow = runtime::weatherTarget(K::Snow, 1);
  AE_EXPECT_TRUE(clear.coverage < cloudy.coverage && cloudy.coverage < rain.coverage && rain.coverage <= storm.coverage,
                 "cobertura cresce de limpo a tempestade");
  AE_EXPECT_TRUE(storm.tint < rain.tint && rain.tint < cloudy.tint, "tempestade mais escura");
  AE_EXPECT_TRUE(rain.rain == 1 && storm.rain == 1 && snow.rain == 0 && clear.rain == 0, "só chuva e tempestade chovem");
  AE_EXPECT_TRUE(snow.snow == 1 && rain.snow == 0, "só a neve neva");
  AE_EXPECT_TRUE(storm.storm == 1 && rain.storm == 0, "relâmpago só na tempestade");
  AE_EXPECT_TRUE(runtime::weatherTarget(K::Rain, 0.25f).rain == 0.25f, "intensidade dosa a chuva");
}

AE_TEST(weather_writes_clouds_fog_emission_and_follows_the_camera_in_edit) {
  auto s = makeWeatherScene(scene::WeatherKind::Rain, 0.5f);
  runtime::SceneWeather weather;
  const float camera[3]{3, 2, -4};
  const auto report = weather.advance(s.graph, 0, 0, false, camera,
      [&](runtime::ObjectId id, const runtime::Transform &t) { return writeTransform(s.graph, id, t); });
  AE_EXPECT_TRUE(report.weathers == 1 && report.writes > 0, "o clima escreveu");
  const auto look = runtime::weatherTarget(scene::WeatherKind::Rain, 0.5f);
  const auto &env = environmentOf(s.graph, s.weather);
  AE_EXPECT_TRUE(env.values.clouds.enabled && env.overrideClouds && env.values.clouds.coverage == look.coverage,
                 "nuvens do Ambiente com a cobertura da chuva");
  AE_EXPECT_TRUE(std::abs(env.values.fogDensity - 0.02f * look.fog) < 1e-6f && env.values.fog, "neblina dosada");
  AE_EXPECT_TRUE(particlesOf(s.graph, s.rain).emissionRate.constantMax == 900.0f, "1800/s × 0,5");
  AE_EXPECT_TRUE(particlesOf(s.graph, s.snow).emissionRate.constantMax == 0.0f, "neve parada na chuva");
  float world[16];
  AE_EXPECT_TRUE(runtime::worldMatrix(s.graph, s.rain, world) && std::abs(world[12] - 3) < 1e-4f &&
                 std::abs(world[13] - 12) < 1e-4f && std::abs(world[14] + 4) < 1e-4f, "chuva 10 m acima da câmera");
  const auto revision = s.graph.revision();
  const auto again = weather.advance(s.graph, 0, 0, false, camera,
      [&](runtime::ObjectId id, const runtime::Transform &t) { return writeTransform(s.graph, id, t); });
  AE_EXPECT_TRUE(again.writes == 0 && s.graph.revision() == revision, "clima parado não mexe no grafo");
  // A cena resolvida (o que o renderer recebe) mostra as nuvens do clima.
  renderer::SceneEnvironment resolved;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(s.graph, resolved) && resolved.clouds.coverage == look.coverage,
                 "o renderer recebe a cobertura do clima");
}

AE_TEST(weather_transition_in_play_moves_from_rain_to_snow_over_time) {
  auto s = makeWeatherScene(scene::WeatherKind::Rain, 1.0f);
  runtime::SceneWeather weather;
  const auto write = [&](runtime::ObjectId id, const runtime::Transform &t) { return writeTransform(s.graph, id, t); };
  weather.advance(s.graph, 0, 0.016f, true, nullptr, write);
  AE_EXPECT_TRUE(particlesOf(s.graph, s.rain).emissionRate.constantMax == 1800.0f, "começa no clima autoral");
  auto values = *s.graph.find(s.weather);
  static_cast<scene::Weather *>(values.components.edit(scene::Weather::descriptor))->kind = scene::WeatherKind::Snow;
  s.graph.applyEntityValues(s.weather, values);
  double t = 0;
  for (int i = 0; i < 60; ++i) weather.advance(s.graph, t += 1.0 / 60, 1.0f / 60, true, nullptr, write);
  const float rainMid = particlesOf(s.graph, s.rain).emissionRate.constantMax;
  const float snowMid = particlesOf(s.graph, s.snow).emissionRate.constantMax;
  AE_EXPECT_TRUE(rainMid < 1800.0f && rainMid > 600.0f && snowMid > 50.0f && snowMid < 700.0f,
                 "1 s de 8: chuva diminuindo, neve começando");
  for (int i = 0; i < 60 * 30; ++i) weather.advance(s.graph, t += 1.0 / 60, 1.0f / 60, true, nullptr, write);
  AE_EXPECT_TRUE(particlesOf(s.graph, s.rain).emissionRate.constantMax == 0.0f, "a chuva para de verdade");
  AE_EXPECT_TRUE(std::abs(particlesOf(s.graph, s.snow).emissionRate.constantMax - 700.0f) < 1.0f, "neve cheia");
}

AE_TEST(weather_storm_flashes_lightning_only_in_play_and_deterministically) {
  float peak = 0;
  for (double t = 0; t < 60; t += 0.02) peak = std::max(peak, runtime::lightningFlash(42, t, 9));
  AE_EXPECT_TRUE(peak > 0.8f, "há raios em um minuto");
  AE_EXPECT_TRUE(runtime::lightningFlash(42, 31.3, 9) == runtime::lightningFlash(42, 31.3, 9), "determinístico");
  auto s = makeWeatherScene(scene::WeatherKind::Storm, 1.0f);
  runtime::SceneWeather weather;
  const auto write = [&](runtime::ObjectId id, const runtime::Transform &t) { return writeTransform(s.graph, id, t); };
  float editFlash = 0, playFlash = 0, exposure = 0;
  for (double t = 0; t < 60; t += 0.02) editFlash = std::max(editFlash, weather.advance(s.graph, t, 0.02f, false, nullptr, write).flash);
  for (double t = 0; t < 60; t += 0.02) {
    playFlash = std::max(playFlash, weather.advance(s.graph, t, 0.02f, true, nullptr, write).flash);
    exposure = std::max(exposure, environmentOf(s.graph, s.weather).values.clouds.exposureEv);
  }
  AE_EXPECT_TRUE(editFlash == 0 && playFlash > 0.5f, "relâmpago só em Play");
  AE_EXPECT_TRUE(exposure > 1.5f, "o clarão acende as nuvens");
}

AE_TEST(weather_rain_particles_fall_below_the_emitter) {
  auto s = makeWeatherScene(scene::WeatherKind::Rain, 1.0f);
  runtime::SceneWeather weather;
  const float camera[3]{0, 0, 0};
  weather.advance(s.graph, 0, 0, false, camera,
      [&](runtime::ObjectId id, const runtime::Transform &t) { return writeTransform(s.graph, id, t); });
  runtime::SceneParticles particles;
  for (int i = 0; i < 30; ++i) particles.advance(s.graph, 1.0f / 30);
  renderer::particles::ParticleFrame frame;
  const float eye[3]{0, 1.7f, 0};
  particles.build(s.graph, eye, frame);
  AE_EXPECT_TRUE(frame.alive > 100, "chove");
  runtime::SceneParticles::Status snow;
  const auto snowInstance = s.graph.find(s.snow)->components.find(scene::ParticleSystem::descriptor)->instanceId();
  AE_EXPECT_TRUE(particles.status(s.graph, s.snow, snowInstance, snow) && snow.alive == 0, "neve sem emissão");
}

AE_TEST(weather_recipe_creates_climate_with_linked_rain_and_snow) {
  editor::EditorSession session;
  u32 recipe = ~0u;
  AE_EXPECT_TRUE(editor::findCreationRecipe("light.weather", &recipe), "Criar › Luzes › Clima");
  auto &graph = session.document();
  const auto created = session.createRecipe(recipe, graph.root());
  AE_EXPECT_TRUE(created != 0, "clima criado");
  const auto *weather = static_cast<const scene::Weather *>(graph.find(created)->components.find(scene::Weather::descriptor));
  AE_EXPECT_TRUE(weather && weather->kind == scene::WeatherKind::Rain && weather->rain && weather->snow, "chuva e neve ligadas");
  AE_EXPECT_TRUE(graph.find(static_cast<runtime::ObjectId>(weather->rain))->parent == created &&
                 std::string_view(graph.find(static_cast<runtime::ObjectId>(weather->rain))->name) == "Chuva" &&
                 std::string_view(graph.find(static_cast<runtime::ObjectId>(weather->snow))->name) == "Neve", "filhos com nome");
  const auto &rain = particlesOf(graph, static_cast<runtime::ObjectId>(weather->rain));
  AE_EXPECT_TRUE(rain.simulationSpace == scene::ParticleSimulationSpace::World &&
                 rain.renderMode == scene::ParticleRenderMode::StretchedBillboard && rain.prewarm, "gota esticada, Mundo, prewarm");
  const auto &env = environmentOf(graph, created);
  AE_EXPECT_TRUE(env.overrideClouds && env.overrideFog && !env.overrideSky && !env.overridePost && env.values.priority == 2,
                 "volume só de nuvens e neblina");
  session.update();
  AE_EXPECT_TRUE(environmentOf(graph, created).values.clouds.coverage ==
                 runtime::weatherTarget(scene::WeatherKind::Rain, 0.7f).coverage, "a edição já mostra o céu de chuva");
  AE_EXPECT_TRUE(session.history().undo(graph) && !graph.exists(created), "um desfazer remove clima e filhos");
}
