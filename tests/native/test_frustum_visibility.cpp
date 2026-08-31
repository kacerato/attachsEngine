#include "harness.h"
#include "renderer/frustum_visibility.h"

#include <cmath>
#include <limits>

using namespace ae::renderer;

AE_TEST(Frustum_visibility_accepts_inside_and_rejects_all_six_outside_planes) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.boundsScale = 1.0f;
  settings.boundsMargin = 0.0f;
  settings.nearPlane = 1.0f;
  settings.farPlane = 100.0f;
  const PerspectiveFrustum frustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, settings);
  const float inside[3]{0.0f, 0.0f, 10.0f};
  const float behind[3]{0.0f, 0.0f, -2.0f};
  const float beyondFar[3]{0.0f, 0.0f, 102.0f};
  const float outsideLeft[3]{-8.0f, 0.0f, 10.0f};
  const float outsideRight[3]{8.0f, 0.0f, 10.0f};
  const float outsideTop[3]{0.0f, 8.0f, 10.0f};
  const float outsideBottom[3]{0.0f, -8.0f, 10.0f};
  AE_EXPECT_TRUE(isSphereVisible(frustum, inside, 0.1f), "centro do frustum");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, behind, 0.1f), "atrás do near plane");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, beyondFar, 0.1f), "além do far plane");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, outsideLeft, 0.1f), "fora à esquerda");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, outsideRight, 0.1f), "fora à direita");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, outsideTop, 0.1f), "fora acima");
  AE_EXPECT_TRUE(!isSphereVisible(frustum, outsideBottom, 0.1f), "fora abaixo");
}

AE_TEST(Frustum_visibility_matches_camera_yaw_pitch_and_keeps_intersections) {
  const float camera[3]{3.0f, 2.0f, -4.0f};
  PerspectiveVisibilitySettings settings{};
  settings.boundsScale = 1.0f;
  settings.boundsMargin = 0.0f;
  const float halfPi = 1.57079632679f;
  const PerspectiveFrustum yawed = buildPerspectiveFrustum(camera, halfPi, 0.0f, 2.0f, settings);
  const float forwardAfterYaw[3]{13.0f, 2.0f, -4.0f};
  const float oldForward[3]{3.0f, 2.0f, 6.0f};
  AE_EXPECT_TRUE(isSphereVisible(yawed, forwardAfterYaw, 0.1f), "yaw deve girar o forward para +X");
  AE_EXPECT_TRUE(!isSphereVisible(yawed, oldForward, 0.1f), "forward antigo sai do frustum");

  const PerspectiveFrustum pitched = buildPerspectiveFrustum(camera, 0.0f, 0.5f, 2.0f, settings);
  const float pitchedForward[3]{3.0f, -2.794255f, 4.775826f};
  AE_EXPECT_TRUE(isSphereVisible(pitched, pitchedForward, 0.2f), "pitch deve seguir a matriz do shader");

  const float touchingRight[3]{6.773502f, 2.0f, 6.0f};
  const PerspectiveFrustum straight = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, settings);
  AE_EXPECT_TRUE(isSphereVisible(straight, touchingRight, 0.01f), "esfera tocando plano não pode sumir");
}

AE_TEST(Frustum_visibility_fails_open_for_invalid_data_and_expands_bounds) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.boundsScale = 1.0f;
  settings.boundsMargin = 1.0f;
  const PerspectiveFrustum frustum = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, settings);
  const float justOutside[3]{6.7f, 0.0f, 10.0f};
  AE_EXPECT_TRUE(isSphereVisible(frustum, justOutside, 0.0f), "margem conservadora mantém borda visível");

  const float invalidCenter[3]{std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f};
  AE_EXPECT_TRUE(isSphereVisible(frustum, invalidCenter, 1.0f), "bounds inválidos falham abertos");
  PerspectiveVisibilitySettings disabled{};
  disabled.enabled = false;
  const PerspectiveFrustum invalid = buildPerspectiveFrustum(camera, 0.0f, 0.0f, 1.0f, disabled);
  const float anywhere[3]{10000.0f, 10000.0f, -10000.0f};
  AE_EXPECT_TRUE(isSphereVisible(invalid, anywhere, 0.0f), "política desativada não remove conteúdo");
}
