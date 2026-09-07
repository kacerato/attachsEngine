#include "editor/editor_camera.h"

#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {

bool isFiniteTriple(const float values[3]) noexcept {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

// Direção para onde a câmera olha, na MESMA convenção do resto da engine: o
// espaço de vista tem X à direita, Y para cima e Z para frente, e é a terceira
// linha da matriz mundo→vista transposta que dá o "para frente" em mundo.
void forwardOf(const EditorCamera &camera, float outForward[3]) noexcept {
  const float cosinePitch = std::cos(camera.pitch);
  outForward[0] = cosinePitch * std::sin(camera.yaw);
  outForward[1] = -std::sin(camera.pitch);
  outForward[2] = cosinePitch * std::cos(camera.yaw);
}

void rightOf(const EditorCamera &camera, float outRight[3]) noexcept {
  outRight[0] = std::cos(camera.yaw);
  outRight[1] = 0.0f;
  outRight[2] = -std::sin(camera.yaw);
}

void upOf(const EditorCamera &camera, float outUp[3]) noexcept {
  const float sinePitch = std::sin(camera.pitch);
  outUp[0] = sinePitch * std::sin(camera.yaw);
  outUp[1] = std::cos(camera.pitch);
  outUp[2] = sinePitch * std::cos(camera.yaw);
}

} // namespace

bool isEditorCameraValid(const EditorCamera &camera) noexcept {
  return isFiniteTriple(camera.target) && std::isfinite(camera.distance) &&
         camera.distance > 0.0f && std::isfinite(camera.yaw) && std::isfinite(camera.pitch);
}

void editorCameraPosition(const EditorCamera &camera, float outPosition[3]) noexcept {
  if (outPosition == nullptr) return;
  if (!isEditorCameraValid(camera)) {
    outPosition[0] = outPosition[1] = outPosition[2] = 0.0f;
    return;
  }
  float forward[3]{};
  forwardOf(camera, forward);
  for (u32 axis = 0; axis < 3; ++axis)
    outPosition[axis] = camera.target[axis] - forward[axis] * camera.distance;
}

EditorViewport buildEditorViewport(const EditorCamera &camera, const ui::UiRect &rect,
                                   const renderer::HzbScreenTransform &surfaceTransform,
                                   const renderer::PerspectiveVisibilitySettings &settings) {
  EditorViewport viewport{};
  if (!isEditorCameraValid(camera) || rect.isEmpty()) return viewport;
  float position[3]{};
  editorCameraPosition(camera, position);
  viewport.frustum = renderer::buildPerspectiveFrustum(position, camera.yaw, camera.pitch,
                                                       rect.width / rect.height, settings);
  viewport.surfaceTransform = surfaceTransform;
  viewport.rect = rect;
  return viewport;
}

void orbitEditorCamera(EditorCamera &camera, ui::UiPoint delta,
                       const EditorCameraLimits &limits) noexcept {
  if (!isEditorCameraValid(camera)) return;
  if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) return;
  camera.yaw -= delta.x * limits.orbitRadiansPerPixel;
  camera.pitch += delta.y * limits.orbitRadiansPerPixel;
  // Preso longe dos polos. Exatamente neles a órbita perde a referência de
  // "para cima" e a câmera passa a girar em torno do próprio eixo.
  camera.pitch = std::clamp(camera.pitch, -limits.maximumPitch, limits.maximumPitch);
  // O yaw dá voltas, e mantê-lo em [-pi, pi] evita que horas de uso acumulem
  // um número grande o bastante para o seno perder precisão.
  constexpr float kTwoPi = 6.28318530718f;
  while (camera.yaw > 3.14159265f) camera.yaw -= kTwoPi;
  while (camera.yaw < -3.14159265f) camera.yaw += kTwoPi;
}

void panEditorCamera(EditorCamera &camera, ui::UiPoint delta, const ui::UiRect &rect,
                     const renderer::PerspectiveVisibilitySettings &settings,
                     const EditorCameraLimits &limits) noexcept {
  (void)limits;
  if (!isEditorCameraValid(camera) || rect.isEmpty()) return;
  if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) return;
  // Quantas unidades de mundo cabem num pixel na profundidade do alvo. Sem
  // isso, deslocar de longe atravessaria o mapa e de perto não sairia do lugar.
  const float tangentHalfVertical = std::tan(settings.verticalFieldOfViewRadians * 0.5f);
  const float worldPerPixel = 2.0f * tangentHalfVertical * camera.distance / rect.height;
  if (!std::isfinite(worldPerPixel)) return;

  float right[3]{};
  float up[3]{};
  rightOf(camera, right);
  upOf(camera, up);
  for (u32 axis = 0; axis < 3; ++axis) {
    // O mundo acompanha o dedo: arrastar para a direita traz o conteúdo para a
    // direita, o que significa mover o alvo para a esquerda.
    camera.target[axis] -= right[axis] * delta.x * worldPerPixel;
    camera.target[axis] += up[axis] * delta.y * worldPerPixel;
  }
}

void zoomEditorCamera(EditorCamera &camera, float factor,
                      const EditorCameraLimits &limits) noexcept {
  if (!isEditorCameraValid(camera)) return;
  if (!std::isfinite(factor) || factor <= 0.0f) return;
  camera.distance =
      std::clamp(camera.distance * factor, limits.minimumDistance, limits.maximumDistance);
}

void frameEditorCamera(EditorCamera &camera, const float center[3], float radius,
                       const renderer::PerspectiveVisibilitySettings &settings,
                       const EditorCameraLimits &limits) noexcept {
  if (center == nullptr || !isFiniteTriple(center)) return;
  if (!std::isfinite(radius) || radius <= 0.0f) return;
  for (u32 axis = 0; axis < 3; ++axis) camera.target[axis] = center[axis];
  // Distância em que a esfera cabe na altura da tela, com uma folga para o
  // objeto não encostar na borda — enquadrar exatamente parece erro de corte.
  const float tangentHalfVertical = std::tan(settings.verticalFieldOfViewRadians * 0.5f);
  const float fitted = radius / std::max(tangentHalfVertical, 0.001f) * 1.4f;
  camera.distance = std::clamp(fitted, limits.minimumDistance, limits.maximumDistance);
}

} // namespace ae::editor
