#include "editor/editor_gizmo.h"

#include <cmath>
#include <limits>

namespace ae::editor {
namespace {

using ui::UiPoint;

u32 axisIndexOf(EditorGizmoHandle handle) noexcept {
  switch (handle) {
    case EditorGizmoHandle::AxisX: return 0;
    case EditorGizmoHandle::AxisY: return 1;
    case EditorGizmoHandle::AxisZ: return 2;
    default: return 3;
  }
}

float distanceToSegment(UiPoint point, UiPoint start, UiPoint end) noexcept {
  const float dx = end.x - start.x;
  const float dy = end.y - start.y;
  const float lengthSquared = dx * dx + dy * dy;
  if (lengthSquared <= 0.0f) {
    const float px = point.x - start.x;
    const float py = point.y - start.y;
    return std::sqrt(px * px + py * py);
  }
  float t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
  // Preso ao segmento: prolongar a reta faria a seta ser pegável do outro lado
  // do objeto, onde nada está desenhado.
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  const float closestX = start.x + dx * t;
  const float closestY = start.y + dy * t;
  const float px = point.x - closestX;
  const float py = point.y - closestY;
  return std::sqrt(px * px + py * py);
}

float snapValue(float value, float step) noexcept {
  if (!(step > 0.0f) || !std::isfinite(step) || !std::isfinite(value)) return value;
  return std::round(value / step) * step;
}

} // namespace

bool isGizmoSettingsValid(const EditorGizmoSettings &settings) noexcept {
  if (!std::isfinite(settings.screenLengthPixels) || settings.screenLengthPixels <= 0.0f)
    return false;
  if (!std::isfinite(settings.pickThresholdPixels) || settings.pickThresholdPixels <= 0.0f)
    return false;
  if (!std::isfinite(settings.minimumAxisPixels) || settings.minimumAxisPixels < 0.0f) return false;
  if (!std::isfinite(settings.snapStep) || settings.snapStep < 0.0f) return false;
  return true;
}

EditorGizmoFrame buildGizmoFrame(const EditorViewport &viewport, const float origin[3],
                                 const EditorGizmoSettings &settings) noexcept {
  EditorGizmoFrame frame{};
  if (!isGizmoSettingsValid(settings) || origin == nullptr) return frame;
  const EditorProjectedPoint projected = projectWorldToScreen(viewport, origin);
  if (!projected.valid) return frame;

  // Pixels por unidade de mundo na profundidade do objeto. A conta usa a altura
  // porque é a dimensão a que o campo de visão vertical se refere; usar a
  // largura mudaria o tamanho do gizmo ao girar a tela.
  const float pixelsPerWorldUnit =
      viewport.rect.height / (2.0f * viewport.frustum.tangentHalfVertical * projected.viewDepth);
  if (!std::isfinite(pixelsPerWorldUnit) || pixelsPerWorldUnit <= 0.0f) return frame;

  frame.axisWorldLength = settings.screenLengthPixels / pixelsPerWorldUnit;
  if (!std::isfinite(frame.axisWorldLength) || frame.axisWorldLength <= 0.0f) return frame;

  for (u32 axis = 0; axis < 3; ++axis) frame.origin[axis] = origin[axis];
  frame.originScreen = projected.screen;

  for (u32 axis = 0; axis < 3; ++axis) {
    float tip[3] = {origin[0], origin[1], origin[2]};
    tip[axis] += frame.axisWorldLength;
    const EditorProjectedPoint end = projectWorldToScreen(viewport, tip);
    if (!end.valid) {
      // A ponta caiu atrás do plano próximo: o eixo atravessa a câmera e não
      // tem uma projeção utilizável. Ele fica inativo em vez de virar um
      // segmento espelhado.
      frame.axisEndScreen[axis] = projected.screen;
      frame.axisUsable[axis] = false;
      continue;
    }
    frame.axisEndScreen[axis] = end.screen;
    const float dx = end.screen.x - projected.screen.x;
    const float dy = end.screen.y - projected.screen.y;
    frame.axisUsable[axis] = std::sqrt(dx * dx + dy * dy) >= settings.minimumAxisPixels;
  }
  frame.valid = true;
  return frame;
}

float distanceToGizmoAxis(const EditorGizmoFrame &frame, EditorGizmoHandle handle,
                          ui::UiPoint screen) noexcept {
  const u32 axis = axisIndexOf(handle);
  if (!frame.valid || axis > 2) return std::numeric_limits<float>::infinity();
  if (!frame.axisUsable[axis]) return std::numeric_limits<float>::infinity();
  return distanceToSegment(screen, frame.originScreen, frame.axisEndScreen[axis]);
}

EditorGizmoHandle pickGizmoHandle(const EditorGizmoFrame &frame, ui::UiPoint screen,
                                  const EditorGizmoSettings &settings) noexcept {
  if (!frame.valid || !isGizmoSettingsValid(settings)) return EditorGizmoHandle::None;
  if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) return EditorGizmoHandle::None;

  const EditorGizmoHandle handles[3] = {EditorGizmoHandle::AxisX, EditorGizmoHandle::AxisY,
                                        EditorGizmoHandle::AxisZ};
  EditorGizmoHandle best = EditorGizmoHandle::None;
  float bestDistance = settings.pickThresholdPixels;
  for (u32 axis = 0; axis < 3; ++axis) {
    const float distance = distanceToGizmoAxis(frame, handles[axis], screen);
    // Estritamente menor: com dois eixos empatados sob o dedo, vence X sobre Y
    // sobre Z. É arbitrário, mas é estável — e um empate resolvido por ordem de
    // laço mudaria conforme a câmera girasse.
    if (!(distance < bestDistance)) continue;
    bestDistance = distance;
    best = handles[axis];
  }
  return best;
}

bool beginGizmoDrag(const EditorGizmoFrame &frame, EditorGizmoHandle handle,
                    const EditorTransform &initial, const EditorGizmoSettings &settings,
                    EditorGizmoDrag &outDrag) noexcept {
  outDrag = EditorGizmoDrag{};
  if (!frame.valid || handle == EditorGizmoHandle::None) return false;
  if (!isGizmoSettingsValid(settings) || !isTransformValid(initial)) return false;
  const u32 axis = axisIndexOf(handle);
  const bool plane = handle >= EditorGizmoHandle::PlaneYZ && handle <= EditorGizmoHandle::PlaneXY;
  if (!plane && (axis > 2 || !frame.axisUsable[axis])) return false;
  outDrag.frame = frame;
  outDrag.handle = handle;
  outDrag.initial = initial;
  outDrag.settings = settings;
  outDrag.active = true;
  return true;
}

bool resolveGizmoTranslation(const EditorGizmoDrag &drag, ui::UiPoint totalScreenDelta,
                             EditorTransform &outTransform) noexcept {
  if (!drag.active || !drag.frame.valid) return false;
  if (!std::isfinite(totalScreenDelta.x) || !std::isfinite(totalScreenDelta.y)) return false;
  const u32 axis = axisIndexOf(drag.handle);
  if (axis > 2) return false;

  const float axisX = drag.frame.axisEndScreen[axis].x - drag.frame.originScreen.x;
  const float axisY = drag.frame.axisEndScreen[axis].y - drag.frame.originScreen.y;
  const float axisLengthSquared = axisX * axisX + axisY * axisY;
  if (!(axisLengthSquared > 0.0f)) return false;

  // Projeção do arraste sobre o eixo na tela. O resultado é adimensional: a
  // fração do comprimento do eixo que o dedo percorreu, e é essa fração que
  // vira metros ao multiplicar pelo comprimento de mundo congelado no toque.
  const float travelled =
      (totalScreenDelta.x * axisX + totalScreenDelta.y * axisY) / axisLengthSquared;
  const float worldDelta = travelled * drag.frame.axisWorldLength;
  if (!std::isfinite(worldDelta)) return false;

  outTransform = drag.initial;
  const float moved = drag.initial.position[axis] + worldDelta;
  // O encaixe é aplicado à posição FINAL, não ao deslocamento: encaixar o
  // deslocamento manteria o desalinhamento original do objeto para sempre, que
  // é o oposto do que um passo de grade serve para fazer.
  outTransform.position[axis] = snapValue(moved, drag.settings.snapStep);
  return isTransformValid(outTransform);
}

bool resolveGizmoTransform(const EditorGizmoDrag &drag, EditorGizmoMode mode,
                           ui::UiPoint delta, EditorTransform &out) noexcept {
  if (mode == EditorGizmoMode::Translate) return resolveGizmoTranslation(drag, delta, out);
  if (!drag.active || !drag.frame.valid || !std::isfinite(delta.x) || !std::isfinite(delta.y)) return false;
  const u32 axis = axisIndexOf(drag.handle);
  if (axis > 2 || mode == EditorGizmoMode::Select) return false;
  const float x = drag.frame.axisEndScreen[axis].x - drag.frame.originScreen.x;
  const float y = drag.frame.axisEndScreen[axis].y - drag.frame.originScreen.y;
  const float length = std::sqrt(x*x + y*y);
  if (length <= 0.0f) return false;
  const float pixels = (delta.x*x + delta.y*y) / length;
  out = drag.initial;
  if (mode == EditorGizmoMode::Rotate) out.rotationDegrees[axis] += pixels;
  else out.scale[axis] = std::clamp(out.scale[axis] * std::exp(pixels / 100.0f), 0.001f, 10000.0f);
  return isTransformValid(out);
}

void gizmoRingPoint(const float origin[3], u32 axis, float radius, float angle, float out[3]) noexcept {
  for(u32 i=0;i<3;++i) out[i]=origin[i];
  if(axis>2) return;
  out[(axis+1)%3]+=radius*std::cos(angle);
  out[(axis+2)%3]+=radius*std::sin(angle);
}
bool gizmoPlanePoint(const EditorViewport &view,const float origin[3],u32 axis,
                     ui::UiPoint point,float out[3]) noexcept {
  if(axis>2 || !origin || !out) return false;
  const auto ray=screenPointToRay(view,point);
  if(!ray.valid || std::abs(ray.direction[axis])<.0001f) return false;
  const float t=(origin[axis]-ray.origin[axis])/ray.direction[axis];
  if(!std::isfinite(t) || t<=0) return false;
  float hit[3];
  for(u32 i=0;i<3;++i) {
    hit[i]=ray.origin[i]+t*ray.direction[i];
    if(!std::isfinite(hit[i])) return false;
  }
  for(u32 i=0;i<3;++i) out[i]=hit[i];
  return true;
}
bool gizmoRingAngle(const EditorViewport &view,const float origin[3],u32 axis,
                    ui::UiPoint point,float &angle) noexcept {
  if(axis>2) return false;
  const auto ray=screenPointToRay(view,point);
  if(!ray.valid || std::abs(ray.direction[axis])<.0001f) return false;
  const float t=(origin[axis]-ray.origin[axis])/ray.direction[axis];
  if(!std::isfinite(t) || t<=0) return false;
  const u32 u=(axis+1)%3,v=(axis+2)%3;
  const float x=ray.origin[u]+t*ray.direction[u]-origin[u];
  const float y=ray.origin[v]+t*ray.direction[v]-origin[v];
  if(!std::isfinite(x) || !std::isfinite(y) || x*x+y*y<1e-12f) return false;
  angle=std::atan2(y,x);return true;
}

} // namespace ae::editor
