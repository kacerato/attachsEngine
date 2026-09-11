#include "harness.h"
#include "editor/editor_view.h"
#include "renderer/camera_ray.h"

#include <cmath>

using namespace ae;
using namespace ae::editor;
using namespace ae::renderer;

namespace {

PerspectiveFrustum frustum(float yaw, float pitch, float aspect) {
  PerspectiveFrustum f{};
  f.cameraPosition[0] = 3.0f; f.cameraPosition[1] = 9.0f; f.cameraPosition[2] = -4.0f;
  f.yaw = yaw; f.pitch = pitch;
  f.tangentHalfVertical = std::tan(60.0f * 3.14159265358979f / 180.0f * .5f);
  f.tangentHalfHorizontal = f.tangentHalfVertical * aspect;
  f.nearPlane = .05f; f.farPlane = 4000.0f;
  f.valid = true;
  return f;
}

EditorViewport viewport(float yaw = .7f, float pitch = .35f) {
  EditorViewport view{};
  view.rect = {320.0f, 72.0f, 1160.0f, 812.0f};
  view.frustum = frustum(yaw, pitch, view.rect.width / view.rect.height);
  return view;
}

// Maior diferenca entre as duas direcoes depois de normalizadas. Usada no lugar
// de um angulo quando as duas devem coincidir: `acos` perto de 1 e mal
// condicionado e transforma o erro de arredondamento de float em centesimos de
// grau, o que diria que ha divergencia onde nao ha.
float directionDifference(const float a[3], const float b[3]) {
  const float la = std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  const float lb = std::sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
  float worst = 0.0f;
  for (int axis = 0; axis < 3; ++axis)
    worst = std::fmax(worst, std::abs(a[axis] / la - b[axis] / lb));
  return worst;
}

float angleBetween(const float a[3], const float b[3]) {
  const float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
  const float la = std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  const float lb = std::sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
  const float cosine = std::fmin(1.0f, std::fmax(-1.0f, dot / (la * lb)));
  return std::acos(cosine) * 180.0f / 3.14159265358979f;
}

// O triângulo de tela inteira que os passes usam.
constexpr float kFullscreenTriangle[3][2] = {{-1, -1}, {3, -1}, {-1, 3}};

// Pesos baricêntricos de um ponto nesse triângulo.
void weightsFor(float ndcX, float ndcY, float &w0, float &w1, float &w2) {
  w1 = (ndcX + 1.0f) * .25f;
  w2 = (ndcY + 1.0f) * .25f;
  w0 = 1.0f - w1 - w2;
}

} // namespace

AE_TEST(camera_ray_is_affine_in_ndc_so_a_fullscreen_pass_may_interpolate_it) {
  // ESTE é o teste que tranca o defeito. Um passe de tela inteira calcula o
  // raio nos três vértices e deixa a interpolação preencher o resto; isso só é
  // correto porque o raio CRU é afim em NDC. Se alguém voltar a normalizar
  // antes de interpolar, este teste falha.
  const auto view = viewport();
  const HzbScreenTransform identity{};
  for (float ndcY = -1.0f; ndcY <= 1.0f; ndcY += .25f) {
    for (float ndcX = -1.0f; ndcX <= 1.0f; ndcX += .25f) {
      float w0 = 0, w1 = 0, w2 = 0;
      weightsFor(ndcX, ndcY, w0, w1, w2);
      const CameraRay exact = cameraRayFromNdc(view.frustum, identity, ndcX, ndcY);
      const CameraRay interpolated =
          cameraRayIsAffineInNdc(view.frustum, identity, kFullscreenTriangle, w0, w1, w2);
      AE_EXPECT_TRUE(exact.valid && interpolated.valid, "raios válidos");
      AE_EXPECT_TRUE(directionDifference(exact.direction, interpolated.direction) < 1e-5f,
                     "interpolar o raio cru reproduz o raio exato");
    }
  }
}

AE_TEST(normalising_the_ray_before_interpolating_misses_by_degrees) {
  // O controle: a medida do erro que o defeito produzia. Ele está aqui para que
  // a regra acima não pareça zelo — no CENTRO da tela o desvio passa de 15°, e
  // era ele que inclinava a grade contra geometria reta e a fazia escorregar
  // sobre os objetos quando a câmera se movia.
  const auto view = viewport(0.0f, 0.0f);
  const HzbScreenTransform identity{};
  float normalised[3][3]{};
  for (u32 corner = 0; corner < 3; ++corner) {
    const CameraRay ray = cameraRayFromNdc(view.frustum, identity, kFullscreenTriangle[corner][0],
                                           kFullscreenTriangle[corner][1]);
    AE_EXPECT_TRUE(ray.valid, "raio do vértice");
    const float length = std::sqrt(ray.direction[0]*ray.direction[0] +
                                   ray.direction[1]*ray.direction[1] +
                                   ray.direction[2]*ray.direction[2]);
    for (u32 axis = 0; axis < 3; ++axis) normalised[corner][axis] = ray.direction[axis] / length;
  }
  float w0 = 0, w1 = 0, w2 = 0;
  weightsFor(0.0f, 0.0f, w0, w1, w2);
  float blended[3]{};
  for (u32 axis = 0; axis < 3; ++axis)
    blended[axis] = normalised[0][axis]*w0 + normalised[1][axis]*w1 + normalised[2][axis]*w2;

  const CameraRay centre = cameraRayFromNdc(view.frustum, identity, 0.0f, 0.0f);
  AE_EXPECT_TRUE(centre.valid, "raio do centro");
  AE_EXPECT_TRUE(angleBetween(centre.direction, blended) > 15.0f,
                 "normalizar antes de interpolar erra por mais de 15 graus no centro");
}

AE_TEST(a_projected_point_comes_back_through_the_same_pixel) {
  // Ida e volta: projetar um ponto do mundo e disparar o raio pelo pixel
  // resultante tem de devolver um raio que passa por aquele ponto. É o que
  // garante que o toque seleciona o objeto que está sob o dedo.
  const auto view = viewport();
  const float points[4][3] = {{0, 0, 0}, {12, 1.5f, 40}, {-7, 4, 25}, {2.5f, -1, 9}};
  for (const auto &world : points) {
    const auto projected = projectWorldToScreen(view, world);
    if (!projected.valid) continue;
    const EditorRay ray = screenPointToRay(view, projected.screen);
    AE_EXPECT_TRUE(ray.valid, "raio válido");
    const float toPoint[3] = {world[0] - ray.origin[0], world[1] - ray.origin[1],
                              world[2] - ray.origin[2]};
    AE_EXPECT_TRUE(directionDifference(toPoint, ray.direction) < 1e-4f,
                   "o raio do pixel projetado passa pelo ponto");
  }
}

AE_TEST(the_display_pre_rotation_round_trips_in_every_orientation) {
  // Com a tela girada, a pré-rotação entra na projeção e tem de sair no
  // caminho inverso. Sem isso o editor seleciona um objeto e desenha o contorno
  // em outro assim que o aparelho vira.
  const HzbScreenTransform orientations[4] = {
      {1, 0, 0, 1}, {0, -1, 1, 0}, {-1, 0, 0, -1}, {0, 1, -1, 0}};
  auto view = viewport();
  for (const auto &transform : orientations) {
    view.surfaceTransform = transform;
    for (float ndcY = -.75f; ndcY <= .75f; ndcY += .5f) {
      for (float ndcX = -.75f; ndcX <= .75f; ndcX += .5f) {
        const CameraRay ray = cameraRayFromNdc(view.frustum, transform, ndcX, ndcY);
        AE_EXPECT_TRUE(ray.valid, "raio válido sob rotação");
        // Levar o raio de volta para vista e reaplicar a projeção devolve o NDC.
        const auto basis = buildCameraViewBasis(view.frustum.yaw, view.frustum.pitch);
        float viewSpace[3]{};
        cameraWorldToView(basis, ray.direction, viewSpace);
        AE_EXPECT_TRUE(std::abs(viewSpace[2] - 1.0f) < 1e-4f, "profundidade de vista unitária");
        const float planeX = viewSpace[0] / view.frustum.tangentHalfHorizontal;
        const float planeY = -viewSpace[1] / view.frustum.tangentHalfVertical;
        const float backX = transform.xx * planeX + transform.xy * planeY;
        const float backY = transform.yx * planeX + transform.yy * planeY;
        AE_EXPECT_TRUE(std::abs(backX - ndcX) < 1e-4f && std::abs(backY - ndcY) < 1e-4f,
                       "a pré-rotação volta ao mesmo NDC");
      }
    }
  }
}

AE_TEST(the_contract_refuses_a_frustum_it_cannot_invert) {
  // Falhar fechado: um frustum não inicializado ou uma pré-rotação degenerada
  // devolvem um raio inválido, e não uma direção qualquer que selecionaria um
  // objeto ao acaso.
  const HzbScreenTransform identity{};
  PerspectiveFrustum invalid{};
  AE_EXPECT_TRUE(!cameraRayFromNdc(invalid, identity, 0, 0).valid, "frustum inválido");

  auto good = frustum(.2f, .1f, 1.7f);
  AE_EXPECT_TRUE(!cameraRayFromNdc(good, identity, std::nanf(""), 0).valid, "NDC não finito");

  const HzbScreenTransform degenerate{0, 0, 0, 0};
  AE_EXPECT_TRUE(!cameraRayFromNdc(good, degenerate, 0, 0).valid, "pré-rotação singular");

  good.tangentHalfVertical = 0.0f;
  AE_EXPECT_TRUE(!cameraRayFromNdc(good, identity, 0, 0).valid, "campo de visão nulo");
}
