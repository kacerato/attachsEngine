#include "harness.h"

// V-2: a noite como nos ciclos de dia da Unity/HDRP -- Lua como direcional que
// troca a sombra com o sol, céu noturno que quase não ilumina, estrelas presas
// à esfera celeste.

#include "editor/editor_creation_catalog.h"
#include "editor/editor_session.h"
#include "renderer/night_sky.h"
#include "renderer/punctual_lights.h"
#include "renderer/time_of_day.h"
#include "runtime/scene_lights.h"
#include "runtime/scene_time_of_day.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include "scene/light_defaults.h"
#include "scene/time_of_day.h"

#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace ae;

namespace {
renderer::TimeOfDayInput input(float hour, float day, float latitude = -15.0f) {
  renderer::TimeOfDayInput value;
  value.hour = hour;value.dayOfYear = day;value.latitude = latitude;
  return value;
}
constexpr float kFullMoonDay = 6.0f + 14.765f;
}

AE_TEST(moon_rises_opposite_the_sun_and_fades_with_phase_and_daylight) {
  const auto midnight = renderer::evaluateTimeOfDay(input(0.0f, kFullMoonDay));
  const auto moon = renderer::evaluateMoon(input(0.0f, kFullMoonDay), midnight);
  AE_EXPECT_TRUE(moon.illuminated > 0.99f, "lua cheia acesa por inteiro");
  float dot = 0;for (u32 i = 0; i < 3; ++i) dot += moon.toMoon[i] * midnight.toSun[i];
  AE_EXPECT_TRUE(dot < -0.99f, "lua cheia oposta ao sol");
  AE_EXPECT_TRUE(moon.elevation > 0.5f && moon.intensity > 0.9f, "meia-noite de lua cheia: alta e com brilho cheio");
  const auto noon = renderer::evaluateMoon(input(12.0f, kFullMoonDay), renderer::evaluateTimeOfDay(input(12.0f, kFullMoonDay)));
  AE_EXPECT_TRUE(noon.intensity == 0.0f, "de dia a Lua apaga (abaixo do horizonte e sol alto)");
  const auto newMoon = renderer::evaluateMoon(input(0.0f, 6.0f), renderer::evaluateTimeOfDay(input(0.0f, 6.0f)));
  AE_EXPECT_TRUE(newMoon.intensity < 1e-4f, "lua nova não ilumina");
  // Quarto crescente: metade acesa, ~1/8 do brilho da cheia (curva de fase).
  const auto quarterSun = renderer::evaluateTimeOfDay(input(20.0f, 6.0f + 7.38f));
  const auto quarter = renderer::evaluateMoon(input(20.0f, 6.0f + 7.38f), quarterSun);
  AE_EXPECT_TRUE(std::abs(quarter.illuminated - 0.5f) < 0.02f, "quarto: metade acesa");
  AE_EXPECT_TRUE(quarter.intensity <= 0.126f, "quarto: no máximo ~1/8 da lua cheia");
}

// A lua segue a própria órbita: a cheia nasce perto do pôr do sol e fica a
// noite inteira no céu (antes andava no plano vertical do sol, nascia às 20 h e
// sumia 2-3 h depois); um dia depois ela nasce ~50 min mais tarde.
AE_TEST(full_moon_stays_up_from_dusk_to_dawn_and_rises_later_each_day) {
  const auto moonAt = [](float hour, float day) {
    return renderer::evaluateMoon(input(hour, day), renderer::evaluateTimeOfDay(input(hour, day)));
  };
  for (float hour : {19.0f, 21.0f, 23.0f, 0.5f, 2.0f, 4.0f, 5.0f}) {
    const float day = hour < 12.0f ? kFullMoonDay + 1.0f : kFullMoonDay;
    AE_EXPECT_TRUE(moonAt(hour, day).elevation > 0.0f, ("lua cheia acima do horizonte às " + std::to_string(hour) + " h").c_str());
  }
  AE_EXPECT_TRUE(moonAt(12.0f, kFullMoonDay).elevation < -0.5f, "ao meio-dia a lua cheia está embaixo");
  // Hora em que a lua cruza o horizonte subindo, de 0,1 em 0,1 h.
  const auto rise = [&](float day) {
    float previous = moonAt(12.0f, day).elevation;
    for (float hour = 12.1f; hour < 36.0f; hour += 0.1f) {
      const float h = std::fmod(hour, 24.0f), d = day + (hour >= 24.0f ? 1.0f : 0.0f);
      const float e = moonAt(h, d).elevation;
      if (previous <= 0.0f && e > 0.0f) return hour;
      previous = e;
    }
    return -1.0f;
  };
  const float today = rise(kFullMoonDay), tomorrow = rise(kFullMoonDay + 1.0f);
  AE_EXPECT_TRUE(today > 16.5f && today < 19.5f, "a lua cheia nasce perto do pôr do sol");
  AE_EXPECT_TRUE(tomorrow - today > 0.5f && tomorrow - today < 1.3f, "no dia seguinte ela nasce ~50 min depois");
}

AE_TEST(celestial_sphere_holds_the_sun_still_while_the_sky_turns) {
  // O sol de um mesmo dia é um ponto fixo da esfera celeste: as linhas da hora
  // levam a direção dele sempre ao mesmo lugar, enquanto no mundo ele cruza o céu.
  float reference[3]{};
  for (float hour = 6.0f; hour <= 18.0f; hour += 1.5f) {
    const auto time = input(hour, 172.0f, -23.5f);
    const auto sun = renderer::evaluateTimeOfDay(time);
    float rows[3][3];renderer::celestialSphereRows(time, rows);
    float celestial[3];
    for (u32 r = 0; r < 3; ++r) celestial[r] = rows[r][0] * sun.toSun[0] + rows[r][1] * sun.toSun[1] + rows[r][2] * sun.toSun[2];
    if (hour == 6.0f) std::copy_n(celestial, 3, reference);
    float error = 0;for (u32 r = 0; r < 3; ++r) error += std::abs(celestial[r] - reference[r]);
    AE_EXPECT_TRUE(error < 0.02f, "o sol fica parado na esfera das estrelas");
    for (u32 r = 0; r < 3; ++r) {
      const float length = rows[r][0] * rows[r][0] + rows[r][1] * rows[r][1] + rows[r][2] * rows[r][2];
      AE_EXPECT_TRUE(std::abs(length - 1.0f) < 1e-4f, "linhas de uma rotação");
    }
  }
  // O polo celeste sul fica a 23,5° acima do horizonte sul em São Paulo.
  float rows[3][3];renderer::celestialSphereRows(input(22.0f, 172.0f, -23.5f), rows);
  AE_EXPECT_TRUE(std::abs(rows[2][1] - std::sin(-23.5f * 3.14159265f / 180.0f)) < 1e-4f, "polo na altura da latitude");
  AE_EXPECT_TRUE(rows[2][2] < 0.0f, "polo norte para o norte (-Z), abaixo do horizonte no sul");
}

AE_TEST(time_of_day_v2_keeps_the_moon_reference_and_reads_v1_without_it) {
  scene::TimeOfDay authored;authored.hour = 22.0f;authored.moonLight = 4242;
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::TimeOfDay reread;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in, 2) && reread.moonLight == 4242 && reread.hour == 22.0f, "v2 relê a Lua");
  std::istringstream legacy("1 22 0 -15 172 0");legacy.imbue(std::locale::classic());
  scene::TimeOfDay old;old.moonLight = 7;
  AE_EXPECT_TRUE(old.read(legacy, 1) && old.moonLight == 0 && old.hour == 22.0f, "v1 sem Lua");
  AE_EXPECT_EQ(scene::TimeOfDay::descriptor.references.size(), usize{1}, "Hora do dia › Lua no Inspector");
}

AE_TEST(moon_light_turns_with_the_sun_and_takes_the_shadow_at_night) {
  editor::EditorDocument doc;
  const auto sunId = doc.createEntity(doc.root(), editor::EditorEntityKind::Folder, "Sol");
  const auto moonId = doc.createEntity(doc.root(), editor::EditorEntityKind::Folder, "Lua");
  auto moonValues = *doc.find(moonId);
  auto *moonLight = static_cast<scene::Light *>(moonValues.components.add(scene::Light::descriptor));
  moonLight->kind = scene::LightKind::Directional;moonLight->unit = scene::LightUnit::LuxLumen;moonLight->intensity = 0.5f;
  AE_EXPECT_TRUE(doc.applyEntityValues(moonId, moonValues), "Lua");
  auto sunValues = *doc.find(sunId);
  auto *sunLight = static_cast<scene::Light *>(sunValues.components.add(scene::Light::descriptor));
  auto *time = static_cast<scene::TimeOfDay *>(sunValues.components.add(scene::TimeOfDay::descriptor));
  sunLight->kind = scene::LightKind::Directional;
  time->hour = 0.0f;time->dayOfYear = kFullMoonDay;time->latitude = -15.0f;time->moonLight = moonId;
  AE_EXPECT_TRUE(doc.applyEntityValues(sunId, sunValues), "Sol com Hora do dia e Lua");
  const auto write = [&](runtime::ObjectId object, const runtime::Transform &t) {return doc.setTransform(object, t);};
  AE_EXPECT_EQ(runtime::applyTimeOfDay(doc, 0.0, write), 2u, "sol e lua girados");
  float world[16];AE_EXPECT_TRUE(runtime::worldMatrix(doc, moonId, world), "matriz da Lua");
  const auto moon = renderer::evaluateMoon(input(0.0f, kFullMoonDay), renderer::evaluateTimeOfDay(input(0.0f, kFullMoonDay)));
  float alignment = 0;for (u32 a = 0; a < 3; ++a) alignment += world[8 + a] * -moon.toMoon[a];
  AE_EXPECT_TRUE(alignment > 0.999f, "a Lua emite para longe da lua");

  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights, 0.0), "luzes coletadas");
  const auto *main = renderer::selectDirectionalLight(lights);
  AE_EXPECT_TRUE(main && main->objectId == moonId && main->celestialBody == renderer::CelestialLightBody::Moon,
                 "à meia-noite a Lua é a luz principal (cascatas)");
  const float expected = scene::lightIntensityForShader(scene::LightKind::Directional, scene::LightUnit::LuxLumen, 0.5f, 0, 0) *
                         moon.intensity;
  AE_EXPECT_TRUE(std::abs(main->intensity - expected) < 1e-7f, "0,5 lux vezes a fase e a altura");
  AE_EXPECT_TRUE(main->celestialToSun[1] < -0.5f, "o céu recebe o sol verdadeiro, abaixo do horizonte");

  time->hour = 12.0f;AE_EXPECT_TRUE(doc.applyEntityValues(sunId, sunValues), "meio-dia");
  runtime::applyTimeOfDay(doc, 0.0, write);
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights, 0.0), "luzes do meio-dia");
  main = renderer::selectDirectionalLight(lights);
  AE_EXPECT_TRUE(main && main->objectId == sunId, "de dia o sol volta a ser a luz principal");
}

AE_TEST(night_sky_lights_with_moonlight_not_with_the_day_ambient) {
  const float ambient[3]{0.30f, 0.36f, 0.45f}, ground[3]{0.10f, 0.10f, 0.09f};
  const float moon[3]{7.3e-4f, 7.6e-4f, 8.4e-4f};
  const auto day = renderer::evaluateNightSkyLighting(ambient, ground, 1.0f, moon, renderer::nightBlend(0.8f));
  AE_EXPECT_TRUE(day.sky[0] == ambient[0] && day.ground[2] == ground[2] && day.specularScale == 1.0f, "de dia nada muda");
  const float night = renderer::nightBlend(-0.4f);
  AE_EXPECT_TRUE(night == 1.0f && renderer::nightBlend(0.05f) == 0.0f, "noite depois do crepúsculo náutico");
  const auto moonlit = renderer::evaluateNightSkyLighting(ambient, ground, 0.015f, moon, night);
  const float moonSky = renderer::nightLuminance(moon) * renderer::NightMoonSkyFraction / 3.14159265f;
  AE_EXPECT_TRUE(renderer::nightLuminance(moonlit.sky) < moonSky * 1.2f + renderer::nightLuminance(ambient) * 0.04f,
                 "à noite o céu ilumina com uma fração do luar");
  AE_EXPECT_TRUE(moonlit.sky[2] > moonlit.sky[0] * 1.8f, "luz da noite fria (azulada)");
  AE_EXPECT_TRUE(moonlit.specularScale < 0.015f * 0.1f, "reflexo do céu do dia quase apagado");
  const float dark[3]{};
  const auto moonless = renderer::evaluateNightSkyLighting(ambient, ground, 0.015f, dark, night);
  AE_EXPECT_TRUE(renderer::nightLuminance(moonless.sky) > 0.0f &&
                 renderer::nightLuminance(moonless.sky) < renderer::nightLuminance(ambient) * 0.05f,
                 "lua nova: só o piso das estrelas");
}

AE_TEST(day_night_recipe_creates_the_moon_linked_to_the_sun) {
  editor::EditorSession session;
  u32 recipe = ~0u;
  AE_EXPECT_TRUE(editor::findCreationRecipe("light.day_night", &recipe), "Criar › Luzes › Sol com dia e noite");
  auto &graph = session.document();
  const auto depth = session.history().undoDepth();
  const auto sun = session.createRecipe(recipe, graph.root());
  AE_EXPECT_TRUE(sun != 0, "sol criado");
  const auto *time = static_cast<const scene::TimeOfDay *>(graph.find(sun)->components.find(scene::TimeOfDay::descriptor));
  AE_EXPECT_TRUE(time && time->moonLight != 0, "Hora do dia já aponta para a Lua");
  const auto *moonObject = graph.find(time->moonLight);
  const auto *moon = moonObject ? static_cast<const scene::Light *>(moonObject->components.find(scene::Light::descriptor)) : nullptr;
  AE_EXPECT_TRUE(moon && moon->kind == scene::LightKind::Directional && moon->unit == scene::LightUnit::LuxLumen &&
                 moon->intensity == scene::kMoonLightDefault.intensity && moon->useColorTemperature &&
                 moon->colorTemperature == scene::kMoonColorTemperature,
                 "Lua de 0,5 lux, fria, como no tutorial HDRP Night");
  AE_EXPECT_TRUE(std::string_view(moonObject->name) == "Lua" && moonObject->parent == graph.root(), "irmã do sol na hierarquia");
  AE_EXPECT_EQ(session.history().undoDepth(), depth + 1, "um só passo de desfazer");
}
