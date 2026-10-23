#include "harness.h"

#include "renderer/scene_environment.h"
#include "resources/environment_profile.h"
#include "scene/environment.h"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;

namespace {
// Linha v15 a partir da atual: sem os 4 campos das nuvens depois da exposição
// automática (palavras 20..23: 3 interruptores da v16 e o tipo da v18) e sem
// os 18 números delas no fim (14 da v16, espessura da v17, 3 da v18).
inline std::string withoutClouds(const std::string &text) {
  std::istringstream in(text);std::vector<std::string> words;std::string word;
  while (in >> word) words.push_back(word);
  std::string out;
  for (usize i = 0; i + 18 < words.size(); ++i) {
    if (i >= 20 && i <= 23) continue;
    if (!out.empty()) out += ' ';
    out += words[i];
  }
  return out;
}
std::string writeEnvironment(const scene::Environment &environment) {
  std::ostringstream out;out.imbue(std::locale::classic());
  out << std::setprecision(9);
  environment.write(out);
  return out.str();
}
// Remove os `count` últimos números de uma linha serializada.
std::string dropTail(std::string text, u32 count) {
  auto cut = text.size();
  for (u32 removed = 0; removed < count; ++removed) cut = text.rfind(' ', cut - 1);
  return text.substr(0, cut);
}
}

AE_TEST(sky_celestial_defaults_are_valid_and_out_of_range_values_are_rejected) {
  renderer::SceneEnvironment values{};
  AE_EXPECT_TRUE(values.valid(), "padrões válidos");
  AE_EXPECT_TRUE(values.sunGlare > 0.0f && values.moonIntensity > 0.0f && values.starIntensity > 0.0f,
                 "auréola, lua e estrelas ligadas por padrão");
  AE_EXPECT_TRUE(values.moonPhase == 0.5f, "lua cheia por padrão, oposta ao sol");
  auto broken = values;broken.moonPhase = 1.5f;
  AE_EXPECT_TRUE(!broken.valid(), "fase fora de 0..1 recusada");
  broken = values;broken.moonSizeDegrees = 0.0f;
  AE_EXPECT_TRUE(!broken.valid(), "lua sem tamanho recusada");
  broken = values;broken.starIntensity = std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!broken.valid(), "NaN recusado");
}

AE_TEST(sky_celestial_round_trips_and_v13_scenes_receive_the_new_defaults) {
  scene::Environment authored;
  authored.values.sunGlare = 1.5f;authored.values.moonPhase = 0.25f;
  authored.values.moonSizeDegrees = 2.0f;authored.values.moonIntensity = 0.5f;authored.values.starIntensity = 3.0f;
  const auto text = writeEnvironment(authored);
  scene::Environment reread;
  std::istringstream in(text);in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in, scene::Environment::descriptor.version), "v14 relido");
  AE_EXPECT_TRUE(reread.values.sunGlare == 1.5f && reread.values.moonPhase == 0.25f &&
                 reread.values.moonSizeDegrees == 2.0f && reread.values.moonIntensity == 0.5f &&
                 reread.values.starIntensity == 3.0f, "corpos celestes atravessam a cena");
  scene::Environment old;
  std::istringstream oldIn(dropTail(withoutClouds(text), 5));oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn, 13), "v13 lido");
  const renderer::SceneEnvironment defaults{};
  AE_EXPECT_TRUE(old.values.sunGlare == defaults.sunGlare && old.values.moonIntensity == defaults.moonIntensity &&
                 old.values.starIntensity == defaults.starIntensity, "cena v13 ganha sol com auréola, lua e estrelas");
  usize celestial = 0;
  for (const auto &number : scene::Environment::descriptor.numbers)
    celestial += number.id == "sun_glare" || number.id.starts_with("moon_") || number.id == "star_intensity";
  AE_EXPECT_EQ(celestial, usize{5}, "5 propriedades no Inspector e na API");
}

AE_TEST(sky_celestial_profile_v10_round_trips_and_v9_reads_with_defaults) {
  resources::EnvironmentProfile profile;
  profile.guid = resources::assetGuidFromSeed("test-sky-celestial");profile.revision = 1;profile.name = "Noite";
  profile.values.moonPhase = 0.1f;profile.values.starIntensity = 2.0f;
  resources::EnvironmentProfile parsed;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(), parsed), "v10 relido");
  AE_EXPECT_TRUE(parsed.values.moonPhase == 0.1f && parsed.values.starIntensity == 2.0f, "perfil guarda lua e estrelas");
  auto text = profile.serialize();
  text.replace(text.find(" 13 "), 4, " 9 ");
  // v9: sem os 21 das nuvens (v11..v13) e os 5 dos corpos celestes (v10).
  text = dropTail(text.substr(0, text.size() - 1), 26) + "\n";
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(text, parsed), "v9 lido");
  AE_EXPECT_TRUE(parsed.values.moonPhase == renderer::SceneEnvironment{}.moonPhase, "v9 recebe os padrões");
}

AE_TEST(sky_celestial_volumes_blend_with_the_sky_group_only) {
  renderer::SceneEnvironmentVolume base{}, night{};
  base.environment.active = night.environment.active = true;
  night.environment.moonPhase = 0.0f;night.environment.starIntensity = 4.0f;
  night.environment.priority = 1.0f;base.stableId = 1;night.stableId = 2;night.weight = 0.5f;
  const float camera[3]{};
  const renderer::SceneEnvironmentVolume volumes[]{base, night};
  const auto mixed = renderer::resolveSceneEnvironment(volumes, camera);
  AE_EXPECT_TRUE(mixed.starIntensity > 1.0f && mixed.starIntensity < 4.0f, "meio peso mistura as estrelas");
  night.overrides = renderer::EnvironmentOverridePost;
  const renderer::SceneEnvironmentVolume postOnly[]{base, night};
  AE_EXPECT_TRUE(renderer::resolveSceneEnvironment(postOnly, camera).starIntensity == 1.0f, "corpos celestes são do grupo Céu");
}
