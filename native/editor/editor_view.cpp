#include "editor/editor_view.h"

#include <cmath>

namespace ae::editor {
namespace {

using ui::UiPoint;

struct ViewBasis final {
  // Linhas da matriz mundo→vista. Ela é ortonormal, então a inversa é a
  // transposta — é o que `viewDirectionToWorld` usa, em vez de uma inversão
  // genérica que introduziria erro e um caso de determinante nulo.
  float row0[3]{};
  float row1[3]{};
  float row2[3]{};
};

ViewBasis buildViewBasis(float yaw, float pitch) noexcept {
  const float cosineYaw = std::cos(yaw);
  const float sineYaw = std::sin(yaw);
  const float cosinePitch = std::cos(pitch);
  const float sinePitch = std::sin(pitch);
  ViewBasis basis{};
  basis.row0[0] = cosineYaw;
  basis.row0[1] = 0.0f;
  basis.row0[2] = -sineYaw;
  basis.row1[0] = sinePitch * sineYaw;
  basis.row1[1] = cosinePitch;
  basis.row1[2] = sinePitch * cosineYaw;
  basis.row2[0] = cosinePitch * sineYaw;
  basis.row2[1] = -sinePitch;
  basis.row2[2] = cosinePitch * cosineYaw;
  return basis;
}

void worldToView(const ViewBasis &basis, const float delta[3], float outView[3]) noexcept {
  outView[0] = basis.row0[0] * delta[0] + basis.row0[1] * delta[1] + basis.row0[2] * delta[2];
  outView[1] = basis.row1[0] * delta[0] + basis.row1[1] * delta[1] + basis.row1[2] * delta[2];
  outView[2] = basis.row2[0] * delta[0] + basis.row2[1] * delta[1] + basis.row2[2] * delta[2];
}

void viewToWorld(const ViewBasis &basis, const float view[3], float outWorld[3]) noexcept {
  outWorld[0] = basis.row0[0] * view[0] + basis.row1[0] * view[1] + basis.row2[0] * view[2];
  outWorld[1] = basis.row0[1] * view[0] + basis.row1[1] * view[1] + basis.row2[1] * view[2];
  outWorld[2] = basis.row0[2] * view[0] + basis.row1[2] * view[1] + basis.row2[2] * view[2];
}

bool isFiniteTriple(const float values[3]) noexcept {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

} // namespace

bool isViewportValid(const EditorViewport &viewport) noexcept {
  if (!viewport.frustum.valid) return false;
  if (!ui::isFinite(viewport.rect) || viewport.rect.isEmpty()) return false;
  if (!std::isfinite(viewport.frustum.tangentHalfHorizontal) ||
      viewport.frustum.tangentHalfHorizontal <= 0.0f)
    return false;
  if (!std::isfinite(viewport.frustum.tangentHalfVertical) ||
      viewport.frustum.tangentHalfVertical <= 0.0f)
    return false;
  const float transform[] = {viewport.surfaceTransform.xx, viewport.surfaceTransform.xy,
                             viewport.surfaceTransform.yx, viewport.surfaceTransform.yy};
  for (const float value : transform)
    if (!std::isfinite(value)) return false;
  // Determinante nulo colapsaria a tela numa linha e tornaria o raio inverso
  // impossível. Nenhuma pré-rotação real produz isso; entrada corrompida sim.
  const float determinant = viewport.surfaceTransform.xx * viewport.surfaceTransform.yy -
                            viewport.surfaceTransform.xy * viewport.surfaceTransform.yx;
  return std::fabs(determinant) > 1e-6f;
}

EditorProjectedPoint projectWorldToScreen(const EditorViewport &viewport,
                                          const float world[3]) noexcept {
  EditorProjectedPoint result{};
  if (!isViewportValid(viewport) || world == nullptr || !isFiniteTriple(world)) return result;

  const float delta[3] = {world[0] - viewport.frustum.cameraPosition[0],
                          world[1] - viewport.frustum.cameraPosition[1],
                          world[2] - viewport.frustum.cameraPosition[2]};
  const ViewBasis basis = buildViewBasis(viewport.frustum.yaw, viewport.frustum.pitch);
  float view[3]{};
  worldToView(basis, delta, view);
  result.viewDepth = view[2];
  // Atrás do plano próximo a divisão perspectiva espelha o ponto para o outro
  // lado da tela. Um gizmo desenhado ali apareceria invertido e responderia ao
  // contrário do arraste; dizer "não dá" é a única resposta honesta.
  if (!std::isfinite(view[2]) || view[2] < viewport.frustum.nearPlane) return result;

  // Mesma cadeia do vertex shader: Y do espaço de vista aponta para cima e o
  // clip do Vulkan aponta para baixo, então o sinal troca aqui e em nenhum
  // outro lugar.
  const float planeX = view[0] / (view[2] * viewport.frustum.tangentHalfHorizontal);
  const float planeY = -view[1] / (view[2] * viewport.frustum.tangentHalfVertical);
  const float ndcX = viewport.surfaceTransform.xx * planeX + viewport.surfaceTransform.xy * planeY;
  const float ndcY = viewport.surfaceTransform.yx * planeX + viewport.surfaceTransform.yy * planeY;
  if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return result;

  result.screen.x = viewport.rect.x + (ndcX * 0.5f + 0.5f) * viewport.rect.width;
  result.screen.y = viewport.rect.y + (ndcY * 0.5f + 0.5f) * viewport.rect.height;
  result.valid = true;
  return result;
}

EditorRay screenPointToRay(const EditorViewport &viewport, ui::UiPoint screen) noexcept {
  EditorRay ray{};
  if (!isViewportValid(viewport)) return ray;
  if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) return ray;

  const float ndcX = (screen.x - viewport.rect.x) / viewport.rect.width * 2.0f - 1.0f;
  const float ndcY = (screen.y - viewport.rect.y) / viewport.rect.height * 2.0f - 1.0f;

  const float determinant = viewport.surfaceTransform.xx * viewport.surfaceTransform.yy -
                            viewport.surfaceTransform.xy * viewport.surfaceTransform.yx;
  const float inverse = 1.0f / determinant;
  const float planeX = (viewport.surfaceTransform.yy * ndcX - viewport.surfaceTransform.xy * ndcY) *
                       inverse;
  const float planeY = (-viewport.surfaceTransform.yx * ndcX + viewport.surfaceTransform.xx * ndcY) *
                       inverse;

  // Profundidade de vista 1: qualquer valor positivo dá a mesma direção depois
  // de normalizar, e 1 evita multiplicar por um plano distante grande.
  const float view[3] = {planeX * viewport.frustum.tangentHalfHorizontal,
                         -planeY * viewport.frustum.tangentHalfVertical, 1.0f};
  const ViewBasis basis = buildViewBasis(viewport.frustum.yaw, viewport.frustum.pitch);
  float world[3]{};
  viewToWorld(basis, view, world);
  const float length = std::sqrt(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]);
  if (!std::isfinite(length) || length <= 0.0f) return ray;

  for (u32 axis = 0; axis < 3; ++axis) {
    ray.origin[axis] = viewport.frustum.cameraPosition[axis];
    ray.direction[axis] = world[axis] / length;
  }
  ray.valid = true;
  return ray;
}

EditorPickResult pickNearest(std::span<const EditorPickCandidate> candidates,
                             const EditorRay &ray) noexcept {
  EditorPickResult result{};
  if (!ray.valid) return result;
  for (const EditorPickCandidate &candidate : candidates) {
    if (!candidate.selectable || candidate.id == 0) continue;
    if (!std::isfinite(candidate.radius) || candidate.radius <= 0.0f) continue;
    if (!isFiniteTriple(candidate.center)) continue;

    const float toCentre[3] = {candidate.center[0] - ray.origin[0],
                               candidate.center[1] - ray.origin[1],
                               candidate.center[2] - ray.origin[2]};
    const float projection = toCentre[0] * ray.direction[0] + toCentre[1] * ray.direction[1] +
                             toCentre[2] * ray.direction[2];
    const float centreDistanceSquared =
        toCentre[0] * toCentre[0] + toCentre[1] * toCentre[1] + toCentre[2] * toCentre[2];
    const float perpendicularSquared = centreDistanceSquared - projection * projection;
    const float radiusSquared = candidate.radius * candidate.radius;
    if (perpendicularSquared > radiusSquared) continue;

    const float halfChord = std::sqrt(radiusSquared - perpendicularSquared);
    const float nearHit = projection - halfChord;
    const float farHit = projection + halfChord;
    if (farHit < 0.0f) continue;  // inteiramente atrás da câmera
    // Câmera dentro da esfera conta como acerto na distância zero: o usuário
    // está dentro do objeto e tocar a tela deve selecioná-lo.
    const float distance = nearHit >= 0.0f ? nearHit : 0.0f;
    // Estritamente menor: empate fica com quem foi registrado antes, o que
    // torna a seleção a mesma entre frames com a mesma lista.
    if (result.hit && distance >= result.distance) continue;
    result.hit = true;
    result.id = candidate.id;
    result.distance = distance;
  }
  return result;
}

bool projectSegmentToScreen(const EditorViewport &viewport, const float from[3],
                            const float to[3], ui::UiPoint &outFrom,
                            ui::UiPoint &outTo) noexcept {
  if (!isViewportValid(viewport) || from == nullptr || to == nullptr) return false;
  if (!isFiniteTriple(from) || !isFiniteTriple(to)) return false;

  const ViewBasis basis = buildViewBasis(viewport.frustum.yaw, viewport.frustum.pitch);
  const auto depthOf = [&](const float world[3]) {
    const float delta[3] = {world[0] - viewport.frustum.cameraPosition[0],
                            world[1] - viewport.frustum.cameraPosition[1],
                            world[2] - viewport.frustum.cameraPosition[2]};
    float view[3]{};
    worldToView(basis, delta, view);
    return view[2];
  };

  // Uma folga sobre o plano próximo: exatamente NO plano a divisão ainda é
  // instável em precisão simples, e o ponto interpolado sairia tremendo.
  const float nearPlane = viewport.frustum.nearPlane * 1.01f;
  float start[3] = {from[0], from[1], from[2]};
  float end[3] = {to[0], to[1], to[2]};
  const float startDepth = depthOf(start);
  const float endDepth = depthOf(end);
  if (startDepth < nearPlane && endDepth < nearPlane) return false;

  if (startDepth < nearPlane || endDepth < nearPlane) {
    const float span = endDepth - startDepth;
    if (std::fabs(span) < 1e-6f) return false;
    const float t = (nearPlane - startDepth) / span;
    float *moved = startDepth < nearPlane ? start : end;
    for (u32 axis = 0; axis < 3; ++axis)
      moved[axis] = from[axis] + (to[axis] - from[axis]) * t;
  }

  const EditorProjectedPoint projectedFrom = projectWorldToScreen(viewport, start);
  const EditorProjectedPoint projectedTo = projectWorldToScreen(viewport, end);
  if (!projectedFrom.valid || !projectedTo.valid) return false;
  outFrom = projectedFrom.screen;
  outTo = projectedTo.screen;
  return true;
}

float distanceToCamera(const EditorViewport &viewport, const float world[3]) noexcept {
  if (world == nullptr || !isFiniteTriple(world)) return 0.0f;
  const float delta[3] = {world[0] - viewport.frustum.cameraPosition[0],
                          world[1] - viewport.frustum.cameraPosition[1],
                          world[2] - viewport.frustum.cameraPosition[2]};
  return std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
}

} // namespace ae::editor
