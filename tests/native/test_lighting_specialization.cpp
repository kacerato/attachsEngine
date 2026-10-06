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
