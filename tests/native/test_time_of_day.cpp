#include "harness.h"

#include "renderer/time_of_day.h"
#include "scene/component_schema.h"
#include "scene/time_of_day.h"
#include "renderer/procedural_sky.h"
#include "editor/editor_document.h"
#include "runtime/scene_time_of_day.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include <array>

#include <cmath>
#include <locale>
#include <sstream>

using namespace ae;

namespace {
renderer::TimeOfDayState at(float hour, float latitude = 0.0f, float day = 80.0f) {
  renderer::TimeOfDayInput input;
  input.hour = hour;input.latitude = latitude;input.dayOfYear = day;
  return renderer::evaluateTimeOfDay(input);
}
}

AE_TEST(time_of_day_places_the_sun_by_hour_latitude_and_date) {
  // Equinócio (~dia 80) no equador: meio-dia no zênite, meia-noite no nadir.
  const auto noon = at(12.0f);
  AE_EXPECT_TRUE(noon.elevation > 0.99f, "meio-dia no equador, equinócio: zênite");
  AE_EXPECT_TRUE(at(0.0f).elevation < -0.99f, "meia-noite: sol embaixo");
  const auto morning = at(9.0f), afternoon = at(15.0f);
  AE_EXPECT_TRUE(morning.toSun[0] > 0.5f && afternoon.toSun[0] < -0.5f, "manhã a leste (+X), tarde a oeste");
  AE_EXPECT_TRUE(std::abs(morning.elevation - afternoon.elevation) < 1e-3f, "simétrico em torno do meio-dia");
  // São Paulo (-23,5°) no solstício de junho: sol ao norte (-Z) e baixo.
  const auto winter = at(12.0f, -23.5f, 172.0f);
  AE_EXPECT_TRUE(winter.toSun[2] < 0.0f && winter.elevation < 0.8f, "inverno austral: sol ao norte e mais baixo");
  const float length = std::sqrt(winter.toSun[0]*winter.toSun[0]+winter.toSun[1]*winter.toSun[1]+winter.toSun[2]*winter.toSun[2]);
  AE_EXPECT_TRUE(std::abs(length - 1.0f) < 1e-4f, "direção unitária");
}

AE_TEST(time_of_day_warms_the_sun_near_the_horizon_and_puts_it_out_at_night) {
  const auto noon = at(12.0f), sunset = at(17.85f), night = at(22.0f);
  AE_EXPECT_TRUE(noon.sunIntensity > 0.95f, "meio-dia com intensidade cheia");
  AE_EXPECT_TRUE(night.sunIntensity == 0.0f, "à noite o sol apaga");
  AE_EXPECT_TRUE(sunset.sunIntensity > 0.0f && sunset.sunIntensity < noon.sunIntensity, "pôr do sol mais fraco");
  AE_EXPECT_TRUE(sunset.sunColor[2] / sunset.sunColor[0] < noon.sunColor[2] / noon.sunColor[0] * 0.7f,
                 "perto do horizonte o sol fica mais quente (menos azul)");
  AE_EXPECT_TRUE(at(12.0f, 0.0f, 6.0f).moonPhase < 0.01f, "dia 6: lua nova de referência");
  AE_EXPECT_TRUE(std::abs(at(12.0f, 0.0f, 6.0f + 14.765f).moonPhase - 0.5f) < 0.01f, "meio ciclo: lua cheia");
}

AE_TEST(time_of_day_advances_with_play_time_and_wraps) {
  AE_EXPECT_TRUE(renderer::advanceTimeOfDayHour(10.0f, 0.0f, 999.0) == 10.0f, "duração zero congela");
  AE_EXPECT_TRUE(std::abs(renderer::advanceTimeOfDayHour(10.0f, 24.0f, 60.0) - 11.0f) < 1e-4f,
                 "dia de 24 min: 1 min real = 1 h");
  AE_EXPECT_TRUE(std::abs(renderer::advanceTimeOfDayHour(23.0f, 24.0f, 120.0) - 1.0f) < 1e-4f, "passa da meia-noite");
}

AE_TEST(procedural_sky_follows_the_sun_like_unity_skybox_procedural) {
  renderer::ProceduralSkyParameters sky;
  const float up[3]{0, 1, 0};
  auto at = [&](float elevation, const float view[3]) {
    const float toSun[3]{std::sqrt(1 - elevation * elevation), elevation, 0};
    float out[3];renderer::evaluateProceduralSky(sky, toSun, view, out);return std::array<float, 3>{out[0], out[1], out[2]};
  };
  const auto noon = at(1.0f, up);
  AE_EXPECT_TRUE(noon[2] > noon[1] && noon[1] > noon[0], "meio-dia: zênite azul");
  const auto night = at(-0.3f, up);
  AE_EXPECT_TRUE(night[0] + night[1] + night[2] < 0.01f, "sol bem abaixo do horizonte: céu escuro");
  const auto dusk = at(0.02f, up);
  AE_EXPECT_TRUE(dusk[2] < noon[2] * 0.6f, "sol no horizonte: zênite escurece");
  sky.skyTint[0] = sky.skyTint[1] = 0.2f;sky.skyTint[2] = 0.9f;
  const auto tinted = at(1.0f, up);
  AE_EXPECT_TRUE(tinted[2] / tinted[0] > noon[2] / noon[0], "tom azul deixa o céu mais azul");
}

AE_TEST(time_of_day_rotates_the_light_transform_like_a_unity_cycle_script) {
  editor::EditorDocument doc;
  const auto id = doc.createEntity(doc.root(), editor::EditorEntityKind::Folder, "Sol");
  auto values = *doc.find(id);
  auto *light = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
  auto *time = static_cast<scene::TimeOfDay *>(values.components.add(scene::TimeOfDay::descriptor));
  AE_EXPECT_TRUE(light && time, "luz e hora do dia");
  light->kind = scene::LightKind::Directional;
  time->hour = 9.0f;time->latitude = 0.0f;time->dayOfYear = 80.0f;
  values.transform.position[1] = 12.0f;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "aplicado");
  u32 rotated = runtime::applyTimeOfDay(doc, 0.0, [&](runtime::ObjectId object, const runtime::Transform &t) {
    return doc.setTransform(object, t);});
  AE_EXPECT_EQ(rotated, 1u, "o sol foi girado");
  float world[16];AE_EXPECT_TRUE(runtime::worldMatrix(doc, id, world), "matriz de mundo");
  renderer::TimeOfDayInput input;input.hour = 9.0f;input.latitude = 0.0f;input.dayOfYear = 80.0f;
  const auto sun = renderer::evaluateTimeOfDay(input);
  // +Z do objeto (coluna 2) é a emissão: aponta para longe do sol.
  float alignment = 0;for (int a = 0; a < 3; ++a) alignment += world[8 + a] * -sun.toSun[a];
  AE_EXPECT_TRUE(alignment > 0.999f, "a frente da luz aponta para longe do sol");
  AE_EXPECT_TRUE(doc.find(id)->transform.position[1] == 12.0f, "posição autoral intacta");
  AE_EXPECT_EQ(runtime::applyTimeOfDay(doc, 0.0, [&](runtime::ObjectId object, const runtime::Transform &t) {
    return doc.setTransform(object, t);}), 0u, "já na hora: nada a girar");
  // No Play, a hora anda com o relógio da cena: 6 min reais num dia de 24 min = 6 h.
  time->dayLengthMinutes = 24.0f;AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "duração aplicada");
  runtime::applyTimeOfDay(doc, 360.0, [&](runtime::ObjectId object, const runtime::Transform &t) {return doc.setTransform(object, t);});
  AE_EXPECT_TRUE(runtime::worldMatrix(doc, id, world), "matriz depois de 6 h");
  AE_EXPECT_TRUE(world[8] > 0.5f && world[9] < 0.0f, "às 15 h o sol está a oeste: a luz aponta para leste e para baixo");
}

AE_TEST(time_of_day_component_round_trips_and_requires_a_light) {
  scene::TimeOfDay authored;
  authored.hour = 18.5f;authored.dayLengthMinutes = 12.0f;authored.latitude = -23.5f;
  std::ostringstream out;out.imbue(std::locale::classic());authored.write(out);
  scene::TimeOfDay reread;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in, scene::TimeOfDay::descriptor.version), "relido");
  AE_EXPECT_TRUE(reread.hour == 18.5f && reread.dayLengthMinutes == 12.0f && reread.latitude == -23.5f, "campos atravessam");
  scene::TimeOfDay broken;broken.hour = 30.0f;
  AE_EXPECT_TRUE(!broken.valid(), "hora fora de 0..24 recusada");
  const auto *schema = scene::findComponentSchema(scene::TimeOfDay::descriptor);
  AE_EXPECT_TRUE(schema && !schema->requirements.empty(), "exige Luz no objeto");
}
