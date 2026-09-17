#include "editor/editor_view.h"

#include <cmath>

namespace ae::editor {
namespace {

using ui::UiPoint;

// A base e a projecao NAO sao redefinidas aqui. Elas vivem em
// renderer/camera_ray.h, que e o contrato unico que o vertice da cena, o ceu, o
// passe da grade e a oclusao espelham. Um editor com a sua propria copia
// selecionaria um objeto e desenharia o contorno em outro assim que uma das
// copias divergisse -- foi o que aconteceu com a grade.
using ViewBasis = renderer::CameraViewBasis;

ViewBasis buildViewBasis(float yaw, float pitch,float roll=0) noexcept {
  return renderer::buildCameraViewBasis(yaw, pitch,roll);
}

void worldToView(const ViewBasis &basis, const float delta[3], float outView[3]) noexcept {
  renderer::cameraWorldToView(basis, delta, outView);
}

bool isFiniteTriple(const float values[3]) noexcept {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

} // namespace

bool isViewportValid(const EditorViewport &viewport) noexcept {
  if (!viewport.frustum.valid) return false;
  if (!ui::isFinite(viewport.rect) || viewport.rect.isEmpty()) return false;
  if(renderer::isOrthographic(viewport.frustum) &&
     (!std::isfinite(viewport.frustum.orthographicHalfHeight)||viewport.frustum.orthographicHalfHeight<=0 ||
      !std::isfinite(viewport.frustum.orthographicHalfWidth)||viewport.frustum.orthographicHalfWidth<=0)) return false;
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
  const ViewBasis basis = buildViewBasis(viewport.frustum.yaw, viewport.frustum.pitch,viewport.frustum.roll);
  float view[3]{};
  worldToView(basis, delta, view);
  result.viewDepth = view[2];
  // Atrás do plano próximo a divisão perspectiva espelha o ponto para o outro
  // lado da tela. Um gizmo desenhado ali apareceria invertido e responderia ao
  // contrário do arraste; dizer "não dá" é a única resposta honesta.
  if (!std::isfinite(view[2]) || view[2] < viewport.frustum.nearPlane) return result;
  if(renderer::isOrthographic(viewport.frustum) && view[2]>viewport.frustum.farPlane) return result;

  // Mesma cadeia do vertex shader: Y do espaço de vista aponta para cima e o
  // clip do Vulkan aponta para baixo, então o sinal troca aqui e em nenhum
  // outro lugar.
  const float divisor=renderer::projectionDivisor(viewport.frustum,view[2]);
  const float planeX = view[0] / (divisor * renderer::projectionHalfWidth(viewport.frustum));
  const float planeY = -view[1] / (divisor * renderer::projectionHalfHeight(viewport.frustum));
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

  // O raio vem do contrato comum, cru. Normalizar aqui e correto porque este
  // caminho calcula UM ponto; o que nao pode e normalizar antes de interpolar.
  const renderer::CameraRay raw =
      renderer::cameraRayFromNdc(viewport.frustum, viewport.surfaceTransform, ndcX, ndcY);
  if (!raw.valid) return ray;
  const float length = std::sqrt(raw.direction[0] * raw.direction[0] +
                                 raw.direction[1] * raw.direction[1] +
                                 raw.direction[2] * raw.direction[2]);
  if (!std::isfinite(length) || length <= 0.0f) return ray;
  ray.minimumDistance=viewport.frustum.nearPlane*length;
  ray.maximumDistance=viewport.frustum.farPlane*length;
  if(!std::isfinite(ray.maximumDistance)||ray.minimumDistance<0 ||
     ray.maximumDistance<=ray.minimumDistance) return ray;

  for (u32 axis = 0; axis < 3; ++axis) {
    ray.origin[axis] = viewport.frustum.cameraPosition[axis]+raw.originOffset[axis];
    ray.direction[axis] = raw.direction[axis] / length;
  }
  ray.valid = true;
  return ray;
}

EditorPickResult pickNearest(std::span<const EditorPickCandidate> candidates,
                             const EditorRay &ray) noexcept {
  EditorPickResult result{};
  if (!ray.valid || !std::isfinite(ray.minimumDistance) || !std::isfinite(ray.maximumDistance) ||
      ray.minimumDistance<0 || ray.maximumDistance<ray.minimumDistance) return result;
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
    if (farHit < ray.minimumDistance || nearHit > ray.maximumDistance) continue;
    // Bounds-only fallback: an enclosing sphere starts at the first allowed
    // distance. Meshes below still require an actual visible triangle hit.
    float distance = std::max(nearHit,ray.minimumDistance);
    if(candidate.mesh) {
      // Start the triangle query at the near plane. Filtering the first hit
      // afterwards would lose a second, visible surface of the same mesh.
      float clippedOrigin[3];
      for(u32 k=0;k<3;++k) clippedOrigin[k]=ray.origin[k]+ray.direction[k]*ray.minimumDistance;
      if(!candidate.mesh->intersect(clippedOrigin,ray.direction,candidate.model,distance)) continue;
      distance+=ray.minimumDistance;
    }
    if(!std::isfinite(distance)||distance>ray.maximumDistance) continue;
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

  const ViewBasis basis=buildViewBasis(viewport.frustum.yaw,viewport.frustum.pitch,viewport.frustum.roll);
  double clip[2][3]{};
  for(unsigned endpoint=0;endpoint<2;++endpoint) {
    const float *world=endpoint?to:from;double view[3]{};
    const float *rows[]{basis.row0,basis.row1,basis.row2};
    for(unsigned r=0;r<3;++r) for(unsigned k=0;k<3;++k)
      view[r]+=rows[r][k]*(static_cast<double>(world[k])-viewport.frustum.cameraPosition[k]);
    const double x=view[0]/renderer::projectionHalfWidth(viewport.frustum);
    const double y=-view[1]/renderer::projectionHalfHeight(viewport.frustum);
    clip[endpoint][0]=viewport.surfaceTransform.xx*x+viewport.surfaceTransform.xy*y;
    clip[endpoint][1]=viewport.surfaceTransform.yx*x+viewport.surfaceTransform.yy*y;
    clip[endpoint][2]=view[2];
  }
  // Recorte homogêneo ANTES da divisão perspectiva. Não gera coordenadas
  // gigantes nem reconstrói um ponto no plano próximo em precisão simples.
  double first=0,last=1;
  const auto plane=[&](double a,double b) {
    if(a<0 && b<0) return false;
    if(a<0) first=std::max(first,a/(a-b));
    else if(b<0) last=std::min(last,a/(a-b));
    return first<=last;
  };
  if(!plane(clip[0][2]-viewport.frustum.nearPlane,clip[1][2]-viewport.frustum.nearPlane)) return false;
  const bool orthographic=renderer::isOrthographic(viewport.frustum);
  if(orthographic && !plane(viewport.frustum.farPlane-clip[0][2],viewport.frustum.farPlane-clip[1][2])) return false;
  for(unsigned axis=0;axis<2;++axis) for(int sign:{-1,1})
    if(!plane((orthographic?1:clip[0][2])+sign*clip[0][axis],(orthographic?1:clip[1][2])+sign*clip[1][axis])) return false;
  const auto project=[&](double t) {
    const double depth=orthographic?1:clip[0][2]+t*(clip[1][2]-clip[0][2]);
    const double x=(clip[0][0]+t*(clip[1][0]-clip[0][0]))/depth;
    const double y=(clip[0][1]+t*(clip[1][1]-clip[0][1]))/depth;
    return UiPoint{static_cast<float>(viewport.rect.x+(x*.5+.5)*viewport.rect.width),
                   static_cast<float>(viewport.rect.y+(y*.5+.5)*viewport.rect.height)};
  };
  outFrom=project(first);outTo=project(last);
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
