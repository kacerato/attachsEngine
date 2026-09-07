#include "editor/editor_view.h"
#include "harness.h"

#include <cmath>
#include <vector>

using namespace ae;
using namespace ae::editor;
using namespace ae::renderer;

namespace {

// Câmera na origem olhando para +Z, que é a direção "para frente" do espaço de
// vista desta engine (ver a nota de convenção em editor_view.h).
EditorViewport viewportLookingForward(float width = 1600.0f, float height = 900.0f) {
  EditorViewport viewport{};
  const float position[3] = {0.0f, 0.0f, 0.0f};
  PerspectiveVisibilitySettings settings{};
  settings.nearPlane = 0.1f;
  settings.farPlane = 1000.0f;
  viewport.frustum = buildPerspectiveFrustum(position, 0.0f, 0.0f, width / height, settings);
  viewport.rect = {0.0f, 0.0f, width, height};
  return viewport;
}

bool nearlyEqual(float first, float second, float tolerance = 0.05f) {
  return std::fabs(first - second) <= tolerance;
}

} // namespace

AE_TEST(view_projects_the_point_straight_ahead_to_the_centre) {
  const EditorViewport viewport = viewportLookingForward();
  const float ahead[3] = {0.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, ahead);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 800.0f), "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.y, 450.0f), "");
  AE_EXPECT_TRUE(nearlyEqual(projected.viewDepth, 10.0f), "profundidade em unidades de mundo");
}

AE_TEST(view_up_in_the_world_is_up_on_the_screen) {
  // O eixo Y do mundo aponta para cima e o Y da tela cresce para baixo. Errar
  // este sinal faz a seta verde do gizmo apontar para o chao.
  const EditorViewport viewport = viewportLookingForward();
  const float above[3] = {0.0f, 2.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, above);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(projected.screen.y < 450.0f, "acima no mundo e acima na tela");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 800.0f), "e nao desloca lateralmente");
}

AE_TEST(view_right_in_the_world_is_right_on_the_screen) {
  const EditorViewport viewport = viewportLookingForward();
  const float right[3] = {2.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, right);
  AE_EXPECT_TRUE(projected.valid && projected.screen.x > 800.0f, "");
}

AE_TEST(view_refuses_to_project_what_is_behind_the_near_plane) {
  // Um gizmo desenhado atras da camera apareceria espelhado e responderia ao
  // contrario do arraste.
  const EditorViewport viewport = viewportLookingForward();
  const float behind[3] = {0.0f, 0.0f, -5.0f};
  AE_EXPECT_TRUE(!projectWorldToScreen(viewport, behind).valid, "");
  const float atCamera[3] = {0.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!projectWorldToScreen(viewport, atCamera).valid, "");
}

AE_TEST(view_ray_and_projection_are_inverses) {
  // A propriedade que sustenta a selecao: o raio que sai do pixel de um objeto
  // tem de acertar aquele objeto.
  const EditorViewport viewport = viewportLookingForward();
  const float points[][3] = {{0, 0, 10}, {3, -2, 25}, {-7, 4, 12}, {1.5f, 0.5f, 40}};
  for (const auto &point : points) {
    const EditorProjectedPoint projected = projectWorldToScreen(viewport, point);
    AE_EXPECT_TRUE(projected.valid, "");
    const EditorRay ray = screenPointToRay(viewport, projected.screen);
    AE_EXPECT_TRUE(ray.valid, "");
    const float distance = distanceToCamera(viewport, point);
    for (u32 axis = 0; axis < 3; ++axis) {
      const float reconstructed = ray.origin[axis] + ray.direction[axis] * distance;
      AE_EXPECT_TRUE(nearlyEqual(reconstructed, point[axis], 0.01f),
                     "o raio do pixel reconstroi o ponto original");
    }
  }
}

AE_TEST(view_ray_from_the_centre_points_forward) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  AE_EXPECT_TRUE(ray.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[0], 0.0f, 0.001f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[1], 0.0f, 0.001f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 1.0f, 0.001f), "");
}

AE_TEST(view_rotating_the_camera_rotates_the_ray) {
  // Com yaw de 90 graus a camera olha para +X: o raio central tem de segui-la.
  EditorViewport viewport = viewportLookingForward();
  const float position[3] = {0.0f, 0.0f, 0.0f};
  viewport.frustum = buildPerspectiveFrustum(position, 1.57079633f, 0.0f, 16.0f / 9.0f, {});
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  AE_EXPECT_TRUE(ray.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[0], 1.0f, 0.01f), "");
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 0.0f, 0.01f), "");
}

AE_TEST(view_uses_the_viewport_rectangle_and_not_the_whole_screen) {
  // No editor a vista 3D fica entre a barra superior e a dock. Projetar contra
  // a tela inteira deslocaria toda selecao pela altura da barra.
  EditorViewport viewport = viewportLookingForward();
  viewport.rect = {320.0f, 56.0f, 880.0f, 772.0f};
  const float ahead[3] = {0.0f, 0.0f, 10.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, ahead);
  AE_EXPECT_TRUE(projected.valid, "");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.x, 760.0f), "centro do retangulo da vista");
  AE_EXPECT_TRUE(nearlyEqual(projected.screen.y, 442.0f), "");

  const EditorRay ray = screenPointToRay(viewport, projected.screen);
  AE_EXPECT_TRUE(nearlyEqual(ray.direction[2], 1.0f, 0.01f), "e o raio inverso concorda");
}

AE_TEST(view_survives_a_ninety_degree_surface_pre_rotation) {
  // A pre-rotacao do display entra aqui do mesmo jeito que entra na oclusao;
  // ida e volta tem de continuar exata com a tela girada.
  EditorViewport viewport = viewportLookingForward();
  viewport.surfaceTransform = {0.0f, -1.0f, 1.0f, 0.0f};
  const float point[3] = {2.0f, 1.0f, 15.0f};
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, point);
  AE_EXPECT_TRUE(projected.valid, "");
  const EditorRay ray = screenPointToRay(viewport, projected.screen);
  AE_EXPECT_TRUE(ray.valid, "");
  const float distance = distanceToCamera(viewport, point);
  for (u32 axis = 0; axis < 3; ++axis)
    AE_EXPECT_TRUE(
        nearlyEqual(ray.origin[axis] + ray.direction[axis] * distance, point[axis], 0.01f), "");
}

AE_TEST(view_invalid_viewports_are_refused_instead_of_producing_pixels) {
  EditorViewport empty{};
  AE_EXPECT_TRUE(!isViewportValid(empty), "frustum invalido");
  const float point[3] = {0, 0, 10};
  AE_EXPECT_TRUE(!projectWorldToScreen(empty, point).valid, "");
  AE_EXPECT_TRUE(!screenPointToRay(empty, {0, 0}).valid, "");

  EditorViewport degenerate = viewportLookingForward();
  degenerate.rect = {0, 0, 0, 900};
  AE_EXPECT_TRUE(!isViewportValid(degenerate), "vista de largura zero");

  EditorViewport collapsed = viewportLookingForward();
  collapsed.surfaceTransform = {0.0f, 0.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!isViewportValid(collapsed),
                 "determinante nulo colapsaria a tela numa linha");
}

AE_TEST(picking_selects_the_nearest_candidate_along_the_ray) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 50.0f}, 2.0f, true});
  candidates.push_back({2, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({3, {0.0f, 0.0f, 30.0f}, 2.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_TRUE(result.hit, "");
  AE_EXPECT_EQ(result.id, 2u, "o mais proximo vence, nao o primeiro da lista");
  AE_EXPECT_TRUE(nearlyEqual(result.distance, 8.0f, 0.01f), "distancia ate a superficie");
}

AE_TEST(picking_misses_what_the_ray_does_not_cross) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {100.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_ignores_what_is_behind_the_camera) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, -20.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_respects_the_selectable_flag) {
  // O olho e o cadeado da hierarquia precisam significar algo no viewport.
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 2.0f, false});
  candidates.push_back({2, {0.0f, 0.0f, 30.0f}, 2.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_EQ(result.id, 2u, "o objeto travado e atravessado, nao selecionado");
}

AE_TEST(picking_ties_go_to_the_first_registered_candidate) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({7, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({8, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_EQ(pickNearest(candidates, ray).id, 7u,
               "empate resolvido por ordem torna a selecao a mesma entre frames");
}

AE_TEST(picking_with_the_camera_inside_the_sphere_hits_at_zero) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 0.0f}, 5.0f, true});
  const EditorPickResult result = pickNearest(candidates, ray);
  AE_EXPECT_TRUE(result.hit && result.distance == 0.0f,
                 "estar dentro do objeto e um acerto, nao um erro");
}

AE_TEST(picking_ignores_malformed_candidates) {
  const EditorViewport viewport = viewportLookingForward();
  const EditorRay ray = screenPointToRay(viewport, {800.0f, 450.0f});
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({0, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 0.0f, true});
  candidates.push_back({2, {std::nanf(""), 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, ray).hit, "");
}

AE_TEST(picking_with_an_invalid_ray_never_hits) {
  std::vector<EditorPickCandidate> candidates;
  candidates.push_back({1, {0.0f, 0.0f, 10.0f}, 2.0f, true});
  AE_EXPECT_TRUE(!pickNearest(candidates, EditorRay{}).hit, "");
}
