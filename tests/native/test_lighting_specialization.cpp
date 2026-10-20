#include "harness.h"
#include "renderer/punctual_lights.h"
#include <limits>
using namespace ae;

AE_TEST(point_lighting_specialization_tracks_the_uploaded_cone_and_shadow_contract) {
  renderer::PunctualLight light;
  light.directionOffset[3] = 1;
  AE_EXPECT_TRUE(renderer::unshadowedPointLighting({&light, 1}), "point without tile is eligible");
  light.colorIntensity[3] = 2;
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting({&light, 1}), "spot retains cone calculation");
  light.colorIntensity[3] = 0;
  light.shadow[0] = 0;
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting({&light, 1}), "allocated tile retains shadow lookup");
  light.shadow[0] = -1;
  light.directionOffset[3] = .5f;
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting({&light, 1}), "non-unit point cone is not specialized");
  light.directionOffset[3] = 1;
  light.shadow[0] = std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting({&light, 1}), "unknown shadow metadata fails closed");
  light.shadow[0] = -1;
  AE_EXPECT_TRUE(renderer::unshadowedPointLighting({&light, 1}), "return to unshadowed point restores eligibility");
  renderer::PunctualLight pair[2]{light, light};
  pair[1].shadow[0] = 6;
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting(pair), "one shadowed light invalidates the whole uploaded list");
  pair[1].shadow[0] = -1;
  AE_EXPECT_TRUE(renderer::unshadowedPointLighting(pair), "removing the last tile restores the whole list");
}

AE_TEST(punctual_light_mask_keeps_only_lights_whose_sphere_reaches_the_draw) {
  renderer::PunctualLight lights[3];
  // Lanterna a 10 m com alcance 4; outra colada ao desenho; terceira longe.
  lights[0].positionRange[0] = 10; lights[0].positionRange[3] = 4;
  lights[1].positionRange[0] = 1;  lights[1].positionRange[3] = 1;
  lights[2].positionRange[0] = 100; lights[2].positionRange[3] = 5;
  const float center[3]{0, 0, 0};
  AE_EXPECT_TRUE(renderer::punctualLightMask(center, 2.0f, lights) == 0b010u, "só a luz que alcança o raio do desenho");
  AE_EXPECT_TRUE(renderer::punctualLightMask(center, 6.0f, lights) == 0b011u, "esfera tangente conta como alcançada");
  AE_EXPECT_TRUE(renderer::punctualLightMask(center, std::numeric_limits<float>::infinity(), lights) == 0b111u,
                 "desenho sem limites conhecidos mantém todas as luzes");
  AE_EXPECT_TRUE(renderer::punctualLightMask(center, 2.0f, {lights, 0}) == 0u, "sem luzes aceitas, máscara vazia");
  // Zero (buildNormalMatrix) é "sem máscara"; a máscara calculada vazia não pode colidir com ele.
  AE_EXPECT_TRUE(renderer::encodePunctualLightMask(0u) == 65536.0f, "máscara vazia é distinta de sem máscara");
  AE_EXPECT_TRUE(renderer::encodePunctualLightMask(0xffffu) == 131071.0f, "dezesseis bits exatos em float");
  AE_EXPECT_TRUE(renderer::encodeDrawLighting(0x3u, false, false) == 65536.0f + 3.0f + 131072.0f + 262144.0f,
                 "flags de sombra e sol acima dos bits de luz, ainda exatas em float");
}

AE_TEST(time_of_day_sun_wins_the_directional_selection_even_at_night) {
  renderer::SceneLight authored{}, timed{};
  authored.modality = timed.modality = renderer::LightModality::Directional;
  authored.objectId = 1;authored.intensity = 5.0f;
  timed.objectId = 2;timed.intensity = 0.0f;timed.moonPhase = 0.3f; // noite: sol apagado
  const renderer::SceneLight lights[]{authored, timed};
  const auto *sun = renderer::selectDirectionalLight(lights);
  AE_EXPECT_TRUE(sun && sun->objectId == 2, "o sol com hora do dia manda, mesmo apagado");
  const renderer::SceneLight plain[]{authored};
  AE_EXPECT_TRUE(renderer::selectDirectionalLight(plain)->objectId == 1, "sem hora do dia, a regra antiga");
}
