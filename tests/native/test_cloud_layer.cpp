#include "harness.h"

#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "renderer/cloud_layer.h"
#include "renderer/cloud_noise.h"
#include "rhi/memory_allocator.h"
#include "renderer/scene_environment.h"
#include "resources/environment_profile.h"
#include "runtime/scene_environment.h"
#include "scene/environment.h"
#include "environment_legacy_lines.h"

#include <cmath>
#include <filesystem>
#include <fstream>
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
  AE_EXPECT_EQ(scene::Environment::descriptor.version, 20u, "Ambiente v20");
  AE_EXPECT_TRUE(reread.read(in, 20), "relido");
  AE_EXPECT_TRUE(reread.values.clouds == clouds && !reread.overrideClouds, "todos os campos atravessam");

  // Arquivo v15: o mesmo texto sem os 3 interruptores das nuvens (depois da
  // exposição automática) e sem os 14 números do fim.
  AE_EXPECT_TRUE(tokens(out.str()).size() > 40, "texto completo");
  scene::Environment v16;std::istringstream v16In(environment_legacy::componentAsV16(out.str()));
  v16In.imbue(std::locale::classic());
  AE_EXPECT_TRUE(v16.read(v16In, 16) && v16.values.clouds == clouds && !v16.overrideClouds,
                 "cena v16 (nuvens antes da Exposure) abre com as nuvens");
  scene::Environment old;std::istringstream oldIn(environment_legacy::componentAsV15(out.str()));oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn, 15), "cena v15 abre");
  AE_EXPECT_TRUE(!old.values.clouds.enabled && old.values.clouds == renderer::CloudLayer{} && old.overrideClouds,
                 "cena antiga: sem nuvens, o mesmo céu de antes");
  AE_EXPECT_TRUE(old.values.fogDensity == authored.values.fogDensity && old.values.contrast == authored.values.contrast,
                 "o resto do arquivo antigo continua no lugar");

  const auto &type = scene::Environment::descriptor;
  bool coverage = false, toggle = false, tint = false;
  for (const auto &number : type.numbers)
    if (number.id == "cloud_coverage") coverage = number.presentation.group == std::string_view("Nuvens");
  for (const auto &boolean : type.booleans) if (boolean.id == std::string_view("clouds")) toggle = true;
  for (const auto &triple : scene::environmentTriples) if (triple.id == std::string_view("cloud_tint")) tint = true;
  AE_EXPECT_TRUE(coverage && toggle && tint, "Inspector: seção Nuvens com interruptor, cobertura e cor");
}

AE_TEST(cloud_layer_round_trips_in_environment_profile_v12) {
  resources::EnvironmentProfile profile;
  profile.guid = resources::assetGuidFromSeed("perfil-nublado");
  profile.name = "Nublado";
  profile.values.clouds.enabled = true;profile.values.clouds.coverage = 0.95f;profile.values.clouds.windSpeedKmh = 80.0f;
  const auto text = profile.serialize();
  resources::EnvironmentProfile reread;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(text, reread), "perfil v12 relido");
  AE_EXPECT_TRUE(reread.values.clouds == profile.values.clouds, "nuvens no perfil compartilhado");
  resources::EnvironmentProfile v11;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(environment_legacy::profileAsV11(text), v11) &&
                 v11.values.clouds == profile.values.clouds, "perfil v11 (nuvens antes da Exposure) abre");
  // v10: sem as 16 palavras das nuvens.
  resources::EnvironmentProfile old;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(environment_legacy::profileAsV10(text), old), "perfil v10 abre");
  AE_EXPECT_TRUE(old.values.clouds == renderer::CloudLayer{}, "perfil antigo sem nuvens");
}

AE_TEST(cloud_volume_overrides_only_the_clouds_like_a_unity_cloud_layer_override) {
  renderer::SceneEnvironmentVolume base;
  base.environment.active = true;base.environment.sky = renderer::SkyModel::PhysicalAtmosphere;
  base.environment.fog = true;base.environment.exposureCompensation = 1.5f;base.stableId = 1;
  renderer::SceneEnvironmentVolume cloudy;
  cloudy.environment.active = true;cloudy.environment.priority = 1.0f;cloudy.stableId = 2;
  cloudy.environment.clouds.enabled = true;cloudy.environment.clouds.coverage = 0.9f;
  cloudy.overrides = renderer::volumeOverridesFromLegacyGroups(renderer::EnvironmentOverrideClouds);
  const float camera[3]{0, 0, 0};
  const renderer::SceneEnvironmentVolume both[]{base, cloudy};
  auto look = renderer::resolveSceneEnvironment(both, camera);
  AE_EXPECT_TRUE(look.clouds.enabled && look.clouds.coverage == 0.9f, "nuvens do volume");
  AE_EXPECT_TRUE(look.sky == renderer::SkyModel::PhysicalAtmosphere && look.fog && look.exposureCompensation == 1.5f,
                 "céu, neblina e pós continuam os do Ambiente");
  // Peso 0,5: o céu abre aos poucos pela opacidade, sem trocar a forma.
  renderer::SceneEnvironmentVolume halfVolume = cloudy;halfVolume.weight = 0.5f;
  const renderer::SceneEnvironmentVolume halfPair[]{base, halfVolume};
  look = renderer::resolveSceneEnvironment(halfPair, camera);
  AE_EXPECT_TRUE(std::abs(look.clouds.opacity - 0.5f) < 1e-5f && look.clouds.coverage == 0.9f, "meia influência, meia opacidade");
  // Sem o override, o volume não participa das nuvens.
  renderer::SceneEnvironmentVolume skyOnly = cloudy;skyOnly.overrides = renderer::volumeOverridesFromLegacyGroups(renderer::EnvironmentOverrideSky);
  const renderer::SceneEnvironmentVolume skyPair[]{base, skyOnly};
  AE_EXPECT_TRUE(!renderer::resolveSceneEnvironment(skyPair, camera).clouds.enabled, "override Cloud Layer desmarcado");
  // Um volume local sem nuvens por cima limpa o céu ao entrar nele.
  renderer::SceneEnvironmentVolume clear;
  clear.environment.active = true;clear.environment.priority = 2.0f;clear.stableId = 3;
  clear.overrides = renderer::volumeOverridesFromLegacyGroups(renderer::EnvironmentOverrideClouds);
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
                 !component->overrideFog && !component->overridePost && !component->overrideExposure && !component->overrideBloom &&
                 component->values.priority == 1.0f,
                 "só o override das nuvens, acima do Ambiente");
  renderer::SceneEnvironment look;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(graph, look), "cena resolvida");
  AE_EXPECT_TRUE(look.clouds.enabled && look.sky == renderer::SkyModel::PhysicalAtmosphere,
                 "céu físico da cena com as nuvens do volume");
}

AE_TEST(cloud_layer_follows_the_planet_curvature_and_v19_scenes_get_volume) {
  // Casca esférica: zênite na altitude, horizonte a ~sqrt(2Rh), não no infinito.
  AE_EXPECT_TRUE(std::abs(renderer::cloudShellDistance(2000.0f, 1.0f) - 2000.0f) < 0.5f, "zênite: a própria altitude");
  const float horizon = renderer::cloudShellDistance(2000.0f, 0.0f);
  const float expected = std::sqrt(2.0f * renderer::CloudPlanetRadiusMeters * 2000.0f);
  AE_EXPECT_TRUE(std::abs(horizon - expected) / expected < 0.01f, "horizonte: ~160 km a 2 km de altura");
  const float plane30 = 2000.0f / std::sin(30.0f * 0.0174533f);
  AE_EXPECT_TRUE(std::abs(renderer::cloudShellDistance(2000.0f, 0.5f) - plane30) / plane30 < 0.01f,
                 "30° acima do horizonte a curvatura quase não pesa");
  float previous = 0.0f;
  for (float sine = 1.0f; sine >= 0.0f; sine -= 0.05f) {
    const float distance = renderer::cloudShellDistance(2000.0f, sine);
    AE_EXPECT_TRUE(distance > previous, "mais rasante, mais longe");
    previous = distance;
  }
  // Base e topo: a camada tem espessura, e a espessura vai para a GPU.
  renderer::CloudLayer clouds;clouds.enabled = true;clouds.thicknessMeters = 1200.0f;
  AE_EXPECT_EQ(renderer::packCloudLayer(clouds, true, 0.0).shadow[1], 1200.0f, "espessura no bloco");
  clouds.thicknessMeters = 6000.0f;
  AE_EXPECT_TRUE(!clouds.valid(), "espessura acima de 5 km recusada");
  // Cena v19 (sem a espessura nem as Volumetric Clouds): ganha a espessura padrão.
  scene::Environment authored;authored.values.clouds.enabled = true;authored.values.clouds.thicknessMeters = 300.0f;
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::Environment old;std::istringstream in(environment_legacy::componentAsV19(out.str()));in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(in, 19), "cena v19 abre");
  AE_EXPECT_TRUE(old.values.clouds.enabled && old.values.clouds.thicknessMeters == renderer::CloudLayer{}.thicknessMeters,
                 "v19 mantém as nuvens e recebe a espessura padrão");
  resources::EnvironmentProfile profile;profile.guid = resources::assetGuidFromSeed("perfil-espesso");profile.name = "Espesso";
  profile.values.clouds = authored.values.clouds;profile.values.clouds.type = 1;
  resources::EnvironmentProfile reread, v13;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(), reread) &&
                 reread.values.clouds == profile.values.clouds, "perfil v15 guarda espessura e tipo");
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(environment_legacy::profileAsV14(profile.serialize()), v13) &&
                 v13.values.clouds.type == 0 && v13.values.clouds.thicknessMeters == renderer::CloudLayer{}.thicknessMeters,
                 "perfil v14 abre como camada 2D com a espessura padrão");
}

AE_TEST(volumetric_clouds_noise_tiles_and_v19_scenes_keep_the_2d_layer) {
  // Ruído 3D de Schneider: repetível nos três eixos e com contraste útil.
  std::vector<u8> shape, erosion;
  AE_EXPECT_TRUE(renderer::buildCloudShapeNoise(renderer::CloudShapeNoiseSize, 7u, shape), "forma 64³");
  AE_EXPECT_TRUE(renderer::buildCloudErosionNoise(renderer::CloudErosionNoiseSize, 13u, erosion), "erosão 32³");
  const u32 n = renderer::CloudShapeNoiseSize;
  AE_EXPECT_EQ(shape.size(), static_cast<usize>(n) * n * n * 4, "RGBA8");
  u32 low = 255, high = 0;
  for (usize i = 0; i < shape.size(); i += 4) { low = std::min<u32>(low, shape[i]); high = std::max<u32>(high, shape[i]); }
  AE_EXPECT_TRUE(high - low > 150, "Perlin-Worley com contraste");
  AE_EXPECT_TRUE(std::abs(renderer::tileableWorley(0.001f, 0.3f, 0.6f, 4, 1u) -
                          renderer::tileableWorley(1.001f, 0.3f, 0.6f, 4, 1u)) < 1e-4f, "Worley repete em X");
  AE_EXPECT_TRUE(std::abs(renderer::tileableWorley(0.2f, 0.3f, 0.0005f, 8, 1u) -
                          renderer::tileableWorley(0.2f, 0.3f, 1.0005f, 8, 1u)) < 1e-4f, "Worley repete em Z");
  // Mips dos ruídos (sem eles o ruído longe vira moiré): cadeia até 1³, média preservada.
  std::vector<u8> chain;
  AE_EXPECT_TRUE(renderer::buildCloudNoiseMips(shape, n, chain), "mips da forma");
  usize expected = 0;
  for (u32 side = n; side >= 1; side /= 2) expected += static_cast<usize>(side) * side * side * 4;
  AE_EXPECT_EQ(chain.size(), expected, "níveis 64³..1³");
  AE_EXPECT_EQ(renderer::cloudNoiseMipCount(n), 7u, "7 níveis");
  double mean = 0;
  for (usize i = 0; i < shape.size(); i += 4) mean += shape[i];
  mean /= static_cast<double>(shape.size() / 4);
  AE_EXPECT_TRUE(std::abs(static_cast<double>(chain[chain.size() - 4]) - mean) < 3.0, "1³ é a média do volume");
  rhi::ImageDesc volume{};
  volume.width = volume.height = volume.depth = n;volume.mipLevels = renderer::cloudNoiseMipCount(n);
  volume.format = VK_FORMAT_R8G8B8A8_UNORM;volume.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  volume.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  AE_EXPECT_TRUE(rhi::isImageDescValid(volume), "imagem 3D amostrável com mips");
  volume.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  AE_EXPECT_TRUE(!rhi::isImageDescValid(volume), "3D continua sem anexo");
  // Tipo volumétrico vai para a GPU; cena v19 abre como camada 2D.
  renderer::CloudLayer clouds;clouds.enabled = true;clouds.type = 1;clouds.steps = 32.0f;clouds.shapeFactor = 0.7f;
  const auto rows = renderer::packCloudLayer(clouds, true, 0.0);
  AE_EXPECT_TRUE(rows.shadow[2] == 1.0f && rows.shadow[3] == 32.0f && rows.volume[0] == 0.7f, "tipo, passos e forma no bloco");
  // O ambiente escurece pela fração de céu realmente coberta pelos cúmulos.
  clouds.coverage = 0.5f;
  AE_EXPECT_TRUE(renderer::cloudSkyOcclusion(clouds) < 0.3f, "0,5 volumétrico cobre pouco céu");
  clouds.coverage = 0.95f;
  AE_EXPECT_TRUE(renderer::cloudSkyOcclusion(clouds) > 0.95f, "0,95 volumétrico fecha o céu");
  clouds.type = 2;AE_EXPECT_TRUE(!clouds.valid(), "tipo desconhecido recusado");
  scene::Environment authored;authored.values.clouds.enabled = true;authored.values.clouds.type = 1;
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::Environment reread;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in, 20) && reread.values.clouds.type == 1, "v20 guarda o tipo volumétrico");
  scene::Environment old;std::istringstream oldIn(environment_legacy::componentAsV19(out.str()));oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn, 19) && old.values.clouds.type == 0 && old.values.clouds.enabled, "v19 continua camada 2D");
  bool typeEnum = false;
  for (const auto &item : scene::environmentEnums) if (item.id == std::string_view("cloud_type")) typeEnum = true;
  AE_EXPECT_TRUE(typeEnum, "Inspector: Tipo Camada 2D / Volumétricas");
}

// Cached Shadows (G1b) e nuvens dividem o bloco de ambiente. O G1b gravava o
// flag do atlas dinâmico em cloudShadow.y, que é a espessura da camada: a cada
// quadro com sombra dinâmica a camada caía de 1200 m para max(1, 200) m e as
// nuvens viravam recortes chapados, sem volume nem autossombra. O flag mora em
// shadowDynamicParameters.y; a espessura empacotada é a que chega ao shader.
AE_TEST(cached_shadow_flag_keeps_the_cloud_thickness) {
  renderer::CloudLayer clouds;
  clouds.enabled = true;
  clouds.type = 1;
  clouds.thicknessMeters = 1200.0f;
  const auto rows = renderer::packCloudLayer(clouds, true, 10.0);
  AE_EXPECT_EQ(rows.shadow[1], 1200.0f, "a espessura vai em cloudShadow.y");
#ifdef AETHER_REPOSITORY_ROOT
  const auto text = [](const char *relative) {
    std::ifstream in(std::filesystem::path(AETHER_REPOSITORY_ROOT) / relative, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
  };
  const auto renderer = text("native/platform/android/instanced_renderer.cpp");
  const auto lighting = text("native/rhi/shaders/environment_lighting.glsl");
  AE_EXPECT_TRUE(!renderer.empty() && !lighting.empty(), "fontes do renderer e do shader");
  AE_EXPECT_TRUE(renderer.find("cloudShadow[1] =") == std::string::npos &&
                 renderer.find("cloudShadow[1]=") == std::string::npos,
                 "o renderer não grava nada por cima da espessura");
  AE_EXPECT_TRUE(renderer.find("frame->shadowDynamicParameters[1] =") != std::string::npos,
                 "o flag do atlas dinâmico vai em shadowDynamicParameters.y");
  AE_EXPECT_TRUE(lighting.find("cloudShadow.y>0.5") == std::string::npos &&
                 lighting.find("shadowDynamicParameters.y>0.5") != std::string::npos,
                 "a sombra lê o flag no lugar dele");
#endif
}
