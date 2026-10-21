#include "harness.h"

#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "renderer/cloud_layer.h"
#include "renderer/scene_environment.h"
#include "resources/environment_profile.h"
#include "runtime/scene_environment.h"
#include "scene/environment.h"

#include <cmath>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;

namespace {
const std::vector<u8> &cloudMap() {
  static const std::vector<u8> map = [] {
    std::vector<u8> out;
    renderer::buildCloudMapTexture(renderer::CloudMapSize, 1u, out);
    return out;
  }();
  return map;
}
// Fração média do céu coberta, amostrando o mapa como o shader (sem mip).
float meanAlpha(const renderer::CloudLayer &clouds) {
  float sum = 0.0f;
  constexpr int steps = 48;
  for (int y = 0; y < steps; ++y) for (int x = 0; x < steps; ++x) {
    const float u = (x + 0.5f) / steps, v = (y + 0.5f) / steps;
    float base[3], detail[3];
    renderer::sampleCloudMap(cloudMap(), renderer::CloudMapSize, u, v, base);
    renderer::sampleCloudMap(cloudMap(), renderer::CloudMapSize, u * 3.0f + 0.37f, v * 3.0f + 0.61f, detail);
    sum += renderer::cloudLayerAlpha(clouds, base[0], base[1], detail[2]);
  }
  return sum / (steps * steps);
}
std::vector<std::string> tokens(const std::string &text) {
  std::istringstream in(text);
  return {std::istream_iterator<std::string>(in), std::istream_iterator<std::string>()};
}
std::string join(const std::vector<std::string> &parts) {
  std::string out;
  for (const auto &part : parts) { if (!out.empty()) out += ' '; out += part; }
  return out;
}
} // namespace

AE_TEST(cloud_map_is_tileable_with_a_full_mip_chain) {
  const auto &map = cloudMap();
  usize expected = 0;
  for (u32 side = renderer::CloudMapSize; side >= 1; side /= 2) expected += static_cast<usize>(side) * side * 4;
  AE_EXPECT_EQ(map.size(), expected, "256² até 1², RGBA8");
  std::vector<u8> again;
  AE_EXPECT_TRUE(renderer::buildCloudMapTexture(renderer::CloudMapSize, 1u, again) && again == map, "determinístico");
  // Borda esquerda e direita (e topo e base) continuam o mesmo ruído: a diferença
  // entre colunas vizinhas através da costura é do tamanho da diferença interna.
  const u32 side = renderer::CloudMapSize;
  double seam = 0.0, inner = 0.0;
  for (u32 y = 0; y < side; ++y) {
    const auto at = [&](u32 x) { return static_cast<double>(map[(static_cast<usize>(y) * side + x) * 4]); };
    seam += std::abs(at(side - 1) - at(0));
    inner += std::abs(at(side / 2) - at(side / 2 + 1));
  }
  AE_EXPECT_TRUE(seam < inner * 2.0 + side, "sem costura na repetição");
  AE_EXPECT_TRUE(!renderer::buildCloudMapTexture(100, 1u, again), "lado não potência de dois recusado");
}

AE_TEST(cloud_coverage_goes_from_clear_sky_to_overcast_like_the_weather_profiles) {
  renderer::CloudLayer clouds;
  clouds.enabled = true;clouds.cirrus = 0.0f;
  clouds.coverage = 0.0f;
  AE_EXPECT_TRUE(meanAlpha(clouds) < 0.01f, "cobertura 0: céu limpo");
  clouds.coverage = 1.0f;
  AE_EXPECT_TRUE(meanAlpha(clouds) > 0.97f, "cobertura 1: encoberto");
  float previous = -1.0f;
  for (float coverage = 0.1f; coverage <= 0.91f; coverage += 0.2f) {
    clouds.coverage = coverage;
    const float alpha = meanAlpha(clouds);
    AE_EXPECT_TRUE(alpha > previous, "mais cobertura, mais céu coberto");
    previous = alpha;
  }
  clouds.coverage = 0.5f;
  const float half = meanAlpha(clouds);
  AE_EXPECT_TRUE(half > 0.3f && half < 0.7f, "metade da cobertura cobre perto da metade do céu");
  AE_EXPECT_TRUE(std::abs(renderer::cloudSkyOcclusion(clouds) - half) < 0.2f, "estimativa do ambiente acompanha o mapa");
  clouds.opacity = 0.5f;
  AE_EXPECT_TRUE(std::abs(meanAlpha(clouds) - half * 0.5f) < 1e-3f, "Opacity escala a camada");
  clouds.enabled = false;
  AE_EXPECT_EQ(meanAlpha(clouds), 0.0f, "Cloud Type None: nada");
  AE_EXPECT_EQ(renderer::cloudSkyOcclusion(clouds), 0.0f, "nenhum efeito no ambiente");
  clouds.enabled = true;clouds.opacity = 1.0f;clouds.coverage = 0.0f;clouds.cirrus = 1.0f;
  const float veil = meanAlpha(clouds);
  AE_EXPECT_TRUE(veil > 0.02f && veil < 0.5f, "cirros: véu parcial sem cúmulos");
}

AE_TEST(cloud_rows_carry_wind_exposure_and_shadow_to_the_gpu) {
  renderer::CloudLayer clouds;
  auto rows = renderer::packCloudLayer(clouds, true, 10.0);
  AE_EXPECT_EQ(rows.parameters[0], 0.0f, "desligado: opacidade zero, o shader pula tudo");
  clouds.enabled = true;clouds.windSpeedKmh = 36.0f;clouds.windOrientationDegrees = 0.0f;
  clouds.sizeMeters = 1000.0f;clouds.exposureEv = 1.0f;clouds.shadowMultiplier = 0.6f;
  rows = renderer::packCloudLayer(clouds, false, 10.0);
  AE_EXPECT_EQ(rows.parameters[0], 0.0f, "ambiente inativo: sem nuvens");
  rows = renderer::packCloudLayer(clouds, true, 10.0);
  AE_EXPECT_TRUE(std::abs(rows.shape[2] - 100.0f) < 1e-3f && std::abs(rows.shape[3]) < 1e-3f, "36 km/h a 0°: 100 m para +X em 10 s");
  rows = renderer::packCloudLayer(clouds, true, 3600.0 * 5.0 + 10.0);
  AE_EXPECT_TRUE(rows.shape[2] >= 0.0f && rows.shape[2] < 1000.0f, "deslocamento reduzido ao tamanho do mapa");
  AE_EXPECT_TRUE(std::abs(rows.shape[2] - 100.0f) < 0.05f, "e continua o mesmo ponto do mapa repetido");
  clouds.windOrientationDegrees = 90.0f;
  rows = renderer::packCloudLayer(clouds, true, 10.0);
  AE_EXPECT_TRUE(std::abs(rows.shape[3] - 100.0f) < 1e-2f, "90°: sopra para +Z");
  AE_EXPECT_TRUE(std::abs(rows.tint[0] - 2.0f) < 1e-5f, "+1 EV dobra o brilho");
  AE_EXPECT_TRUE(std::abs(rows.shape[0] - 0.001f) < 1e-7f && rows.shape[1] == clouds.altitudeMeters, "escala e altitude");
  AE_EXPECT_TRUE(std::abs(rows.shadow[0] - 0.6f) < 1e-6f, "Shadow Multiplier");
  clouds.castShadows = false;
  AE_EXPECT_EQ(renderer::packCloudLayer(clouds, true, 0.0).shadow[0], 0.0f, "Cast Shadows desligado");
}

AE_TEST(cloud_layer_round_trips_in_environment_v16_and_old_scenes_have_no_clouds) {
  scene::Environment authored;
  auto &clouds = authored.values.clouds;
  clouds.enabled = true;clouds.castShadows = false;clouds.coverage = 0.8f;clouds.cirrus = 0.1f;
  clouds.altitudeMeters = 3500.0f;clouds.sizeMeters = 9000.0f;clouds.tint[2] = 0.75f;
  clouds.exposureEv = -0.5f;clouds.windOrientationDegrees = 135.0f;clouds.windSpeedKmh = 60.0f;
  clouds.density = 0.9f;clouds.ambientDimmer = 0.4f;clouds.shadowMultiplier = 0.3f;
  authored.overrideClouds = false;
  AE_EXPECT_TRUE(authored.valid(), "nuvens autorais válidas");
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::Environment reread;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_EQ(scene::Environment::descriptor.version, 16u, "Ambiente v16");
  AE_EXPECT_TRUE(reread.read(in, 16), "relido");
  AE_EXPECT_TRUE(reread.values.clouds == clouds && !reread.overrideClouds, "todos os campos atravessam");

  // Arquivo v15: o mesmo texto sem os 3 interruptores das nuvens (depois da
  // exposição automática) e sem os 14 números do fim.
  auto parts = tokens(out.str());
  AE_EXPECT_TRUE(parts.size() > 40, "texto completo");
  std::vector<std::string> legacy(parts.begin(), parts.begin() + 20);
  legacy.insert(legacy.end(), parts.begin() + 23, parts.end() - 14);
  scene::Environment old;std::istringstream oldIn(join(legacy));oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn, 15), "cena v15 abre");
  AE_EXPECT_TRUE(!old.values.clouds.enabled && old.values.clouds == renderer::CloudLayer{} && old.overrideClouds,
                 "cena antiga: sem nuvens, o mesmo céu de antes");
  AE_EXPECT_TRUE(old.values.exposureEv == authored.values.exposureEv, "o resto do arquivo antigo continua no lugar");

  const auto &type = scene::Environment::descriptor;
  bool coverage = false, toggle = false, tint = false;
  for (const auto &number : type.numbers)
    if (number.id == "cloud_coverage") coverage = number.presentation.group == std::string_view("Nuvens");
  for (const auto &boolean : type.booleans) if (boolean.id == std::string_view("clouds")) toggle = true;
  for (const auto &triple : scene::environmentTriples) if (triple.id == std::string_view("cloud_tint")) tint = true;
  AE_EXPECT_TRUE(coverage && toggle && tint, "Inspector: seção Nuvens com interruptor, cobertura e cor");
}

AE_TEST(cloud_layer_round_trips_in_environment_profile_v11) {
  resources::EnvironmentProfile profile;
  profile.guid = resources::assetGuidFromSeed("perfil-nublado");
  profile.name = "Nublado";
  profile.values.clouds.enabled = true;profile.values.clouds.coverage = 0.95f;profile.values.clouds.windSpeedKmh = 80.0f;
  const auto text = profile.serialize();
  resources::EnvironmentProfile reread;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(text, reread), "perfil v11 relido");
  AE_EXPECT_TRUE(reread.values.clouds == profile.values.clouds, "nuvens no perfil compartilhado");
  // v10: troca a versão e corta as 16 palavras das nuvens.
  auto parts = tokens(text);
  parts[1] = "10";
  parts.resize(parts.size() - 16);
  resources::EnvironmentProfile old;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(join(parts), old), "perfil v10 abre");
  AE_EXPECT_TRUE(old.values.clouds == renderer::CloudLayer{}, "perfil antigo sem nuvens");
}

AE_TEST(cloud_volume_overrides_only_the_clouds_like_a_unity_cloud_layer_override) {
  renderer::SceneEnvironmentVolume base;
  base.environment.active = true;base.environment.sky = renderer::SkyModel::PhysicalAtmosphere;
  base.environment.fog = true;base.environment.exposureEv = 1.5f;base.stableId = 1;
  renderer::SceneEnvironmentVolume cloudy;
  cloudy.environment.active = true;cloudy.environment.priority = 1.0f;cloudy.stableId = 2;
  cloudy.environment.clouds.enabled = true;cloudy.environment.clouds.coverage = 0.9f;
  cloudy.overrides = renderer::EnvironmentOverrideClouds;
  const float camera[3]{0, 0, 0};
  const renderer::SceneEnvironmentVolume both[]{base, cloudy};
  auto look = renderer::resolveSceneEnvironment(both, camera);
  AE_EXPECT_TRUE(look.clouds.enabled && look.clouds.coverage == 0.9f, "nuvens do volume");
  AE_EXPECT_TRUE(look.sky == renderer::SkyModel::PhysicalAtmosphere && look.fog && look.exposureEv == 1.5f,
                 "céu, neblina e pós continuam os do Ambiente");
  // Peso 0,5: o céu abre aos poucos pela opacidade, sem trocar a forma.
  renderer::SceneEnvironmentVolume halfVolume = cloudy;halfVolume.weight = 0.5f;
  const renderer::SceneEnvironmentVolume halfPair[]{base, halfVolume};
  look = renderer::resolveSceneEnvironment(halfPair, camera);
  AE_EXPECT_TRUE(std::abs(look.clouds.opacity - 0.5f) < 1e-5f && look.clouds.coverage == 0.9f, "meia influência, meia opacidade");
  // Sem o override, o volume não participa das nuvens.
  renderer::SceneEnvironmentVolume skyOnly = cloudy;skyOnly.overrides = renderer::EnvironmentOverrideSky;
  const renderer::SceneEnvironmentVolume skyPair[]{base, skyOnly};
  AE_EXPECT_TRUE(!renderer::resolveSceneEnvironment(skyPair, camera).clouds.enabled, "override Cloud Layer desmarcado");
  // Um volume local sem nuvens por cima limpa o céu ao entrar nele.
  renderer::SceneEnvironmentVolume clear;
  clear.environment.active = true;clear.environment.priority = 2.0f;clear.stableId = 3;
  clear.overrides = renderer::EnvironmentOverrideClouds;
  const renderer::SceneEnvironmentVolume three[]{base, cloudy, clear};
  look = renderer::resolveSceneEnvironment(three, camera);
  AE_EXPECT_TRUE(!look.clouds.enabled && look.clouds.opacity == 0.0f, "volume limpo vence");
}

AE_TEST(cloud_volume_recipe_creates_a_global_cloud_layer_above_the_scene_environment) {
  editor::EditorSession session;
  u32 recipe = ~0u;
  for (u32 i = 0; i < editor::editorCreationCatalog.size(); ++i)
    if (editor::editorCreationCatalog[i].id == "light.clouds") recipe = i;
  AE_EXPECT_TRUE(recipe != ~0u, "Criar › Luzes › Volume de nuvens");
  auto &graph = session.document();
  const auto ambient = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Ambiente");
  auto values = *graph.find(ambient);
  auto *environment = static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
  environment->values.sky = renderer::SkyModel::PhysicalAtmosphere;
  AE_EXPECT_TRUE(graph.applyEntityValues(ambient, values), "Ambiente da cena");
  const auto created = session.createRecipe(recipe, graph.root());
  AE_EXPECT_TRUE(created != 0, "volume criado");
  const auto *component = static_cast<const scene::Environment *>(
      graph.find(created)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(component && component->values.clouds.enabled && component->overrideClouds && !component->overrideSky &&
                 !component->overrideFog && !component->overridePost && component->values.priority == 1.0f,
                 "só o override das nuvens, acima do Ambiente");
  renderer::SceneEnvironment look;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(graph, look), "cena resolvida");
  AE_EXPECT_TRUE(look.clouds.enabled && look.sky == renderer::SkyModel::PhysicalAtmosphere,
                 "céu físico da cena com as nuvens do volume");
}
