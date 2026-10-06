#include "renderer/gpu_draw_culling.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::renderer {
namespace {

// Os dois lados do contrato de layout. O kernel lê um std430 de 32 bytes por
// registro e um bloco de push constants de 128; se um destes mudar sem o
// shader, o build para aqui em vez de produzir corte silenciosamente errado.
static_assert(sizeof(GpuCullDrawRecord) == 32);
static_assert(offsetof(GpuCullDrawRecord, boundsRadius) == 12);
static_assert(offsetof(GpuCullDrawRecord, stateIndex) == 16);
static_assert(offsetof(GpuCullDrawRecord, flags) == 20);
static_assert(sizeof(GpuCullParameters) == 128);
static_assert(offsetof(GpuCullParameters, orthographicHalfWidth) == 112);
static_assert(offsetof(GpuCullParameters, cosineYaw) == 16);
static_assert(offsetof(GpuCullParameters, tangentHalfHorizontal) == 32);
static_assert(offsetof(GpuCullParameters, boundsMargin) == 48);
static_assert(offsetof(GpuCullParameters, surfaceTransform) == 64);
static_assert(offsetof(GpuCullParameters, drawCount) == 80);
static_assert(offsetof(GpuCullParameters, hzbLevelCount) == 96);

bool finite(float value) { return std::isfinite(value); }

// Passo de redução do nível: exatamente a divisão-teto que createHzbResources
// planeja e hzb_reduce_compute.comp executa.
u32 halveCeiling(u32 extent) { return (extent + 1) / 2; }

} // namespace

bool buildGpuCullHzbView(const HzbPyramid &pyramid, GpuCullHzbView &out) {
  out = {};
  if (!pyramid.valid || pyramid.mips.empty() || pyramid.texels.empty()) return false;
  u32 width = pyramid.mips[0].width;
  u32 height = pyramid.mips[0].height;
  usize offset = 0;
  for (const HzbMipLevel &mip : pyramid.mips) {
    if (mip.width != width || mip.height != height || mip.offset != offset) return false;
    const usize texels = static_cast<usize>(width) * height;
    if (texels == 0 || offset > pyramid.texels.size() ||
        texels > pyramid.texels.size() - offset) return false;
    offset += texels;
    width = halveCeiling(width);
    height = halveCeiling(height);
  }
  out.texels = pyramid.texels.data();
  out.texelCount = pyramid.texels.size();
  out.baseWidth = pyramid.mips[0].width;
  out.baseHeight = pyramid.mips[0].height;
  out.levelCount = static_cast<u32>(pyramid.mips.size());
  return true;
}

GpuCullMotionGuard buildGpuCullMotionGuard(const PerspectiveFrustum &frustum,
                                           const float pyramidCameraPosition[3],
                                           float pyramidYaw, float pyramidPitch) {
  GpuCullMotionGuard guard{};
  if (!frustum.valid || pyramidCameraPosition == nullptr || !finite(pyramidYaw) ||
      !finite(pyramidPitch)) return guard;
  float translationSquared = 0.0f;
  for (u32 axis = 0; axis < 3; ++axis) {
    if (!finite(pyramidCameraPosition[axis])) return guard;
    const float delta = frustum.cameraPosition[axis] - pyramidCameraPosition[axis];
    translationSquared += delta * delta;
  }
  const float translation = std::sqrt(translationSquared);
  if (!finite(translation)) return guard;

  // Diferença angular no menor arco: um wrap de yaw não pode virar uma guarda
  // gigante que desliga o estágio por um frame inteiro.
  auto shortestAngle = [](float a, float b) {
    constexpr float twoPi = 6.283185307179586f;
    float delta = std::fmod(a - b, twoPi);
    if (delta > twoPi * 0.5f) delta -= twoPi;
    if (delta < -twoPi * 0.5f) delta += twoPi;
    return std::abs(delta);
  };
  const float yawDelta = shortestAngle(frustum.yaw, pyramidYaw);
  const float pitchDelta = shortestAngle(frustum.pitch, pyramidPitch);
  if (isOrthographic(frustum)) {
    // A previous-frame depth image cannot certify newly exposed surfaces.
    // Resume only at exactly the recorded pose; reset hysteresis while moving.
    // Lens/viewport changes invalidate the pyramid in the renderer separately.
    guard.valid = translationSquared == 0.0f && yawDelta == 0.0f && pitchDelta == 0.0f;
    return guard;
  }
  const float smallestTangent = std::min(frustum.tangentHalfHorizontal, frustum.tangentHalfVertical);
  if (!(smallestTangent > 0.0f) || !finite(smallestTangent)) return guard;

  // Rotação: um ângulo A desloca a projeção em tan(A)/tanHalf em NDC, e o
  // espaço normalizado [0,1] tem metade da escala do NDC [-1,1]. tan é
  // superestimado por linearização apenas para ângulos grandes, onde a guarda
  // já desliga o corte de qualquer forma; usar tan direto mantém a monotonia.
  const float angularDelta = yawDelta + pitchDelta;
  const float clampedAngular = std::min(angularDelta, 1.5f); // tan explode perto de pi/2.
  guard.screenDilation = 0.5f * std::tan(clampedAngular) / smallestTangent;
  if (angularDelta > clampedAngular) guard.screenDilation = 1.0f; // Dilata a tela inteira.
  guard.screenDilation = std::min(guard.screenDilation, 1.0f);

  // Aproximação: o pior caso é a câmera ter andado o deslocamento inteiro na
  // direção do candidato.
  guard.viewDepthGuard = translation;
  // Translação lateral: deslocamento em tela de translation/(z*tanHalf) em NDC,
  // metade disso em espaço normalizado. O kernel divide por z do candidato.
  guard.translationDilationScale = 0.5f * translation / smallestTangent;
  guard.valid = true;
  return guard;
}

bool buildGpuCullParameters(const PerspectiveFrustum &frustum,
                            const HzbScreenTransform &screenTransform,
                            const GpuCullHzbView &hzb, const GpuCullMotionGuard &guard,
                            float normalizedDepthBias, u32 hysteresisFrames, u32 drawCount,
                            bool pyramidUsable, GpuCullParameters &out) {
  out = {};
  if (!frustum.valid || drawCount == 0) return false;
  if (isOrthographic(frustum) && (!finite(frustum.orthographicHalfWidth) ||
      !finite(frustum.orthographicHalfHeight) || frustum.orthographicHalfWidth<=0 ||
      frustum.orthographicHalfHeight<=0)) return false;
  if (!finite(frustum.roll) || std::abs(frustum.roll) > 1e-6f) return false;
  if (!finite(normalizedDepthBias) || normalizedDepthBias < 0.0f) return false;
  const float transformValues[] = {screenTransform.xx, screenTransform.xy,
                                   screenTransform.yx, screenTransform.yy};
  for (float value : transformValues) if (!finite(value)) return false;
  if (!finite(frustum.tangentHalfHorizontal) || !finite(frustum.tangentHalfVertical) ||
      frustum.tangentHalfHorizontal <= 0.0f || frustum.tangentHalfVertical <= 0.0f) return false;
  if (!finite(frustum.nearPlane) || !finite(frustum.farPlane) ||
      frustum.nearPlane <= 0.0f || frustum.farPlane <= frustum.nearPlane) return false;
  if (!finite(frustum.boundsScale) || !finite(frustum.boundsMargin) ||
      frustum.boundsScale < 0.0f || frustum.boundsMargin < 0.0f) return false;

  const bool usable = pyramidUsable && hzb.baseWidth > 0 && hzb.baseHeight > 0 &&
                      hzb.levelCount > 0;
  // Uma guarda inválida não é motivo para cortar sem guarda: sem saber quanto a
  // pose mudou, a pirâmide não é utilizável neste frame.
  const bool guarded = guard.valid && finite(guard.screenDilation) &&
                       finite(guard.viewDepthGuard) && finite(guard.translationDilationScale) &&
                       guard.screenDilation >= 0.0f && guard.viewDepthGuard >= 0.0f &&
                       guard.translationDilationScale >= 0.0f;

  out.cameraPosition[0] = frustum.cameraPosition[0];
  out.cameraPosition[1] = frustum.cameraPosition[1];
  out.cameraPosition[2] = frustum.cameraPosition[2];
  out.boundsScale = frustum.boundsScale;
  out.cosineYaw = std::cos(frustum.yaw);
  out.sineYaw = std::sin(frustum.yaw);
  out.cosinePitch = std::cos(frustum.pitch);
  out.sinePitch = std::sin(frustum.pitch);
  out.tangentHalfHorizontal = frustum.tangentHalfHorizontal;
  out.tangentHalfVertical = frustum.tangentHalfVertical;
  out.nearPlane = frustum.nearPlane;
  out.farPlane = frustum.farPlane;
  out.boundsMargin = frustum.boundsMargin;
  out.normalizedDepthBias = normalizedDepthBias;
  out.screenDilation = guarded ? guard.screenDilation : 0.0f;
  out.viewDepthGuard = guarded ? guard.viewDepthGuard : 0.0f;
  out.translationDilationScale = guarded ? guard.translationDilationScale : 0.0f;
  out.surfaceTransform[0] = screenTransform.xx;
  out.surfaceTransform[1] = screenTransform.xy;
  out.surfaceTransform[2] = screenTransform.yx;
  out.surfaceTransform[3] = screenTransform.yy;
  out.drawCount = drawCount;
  out.hysteresisFrames = hysteresisFrames;
  out.hzbBaseWidth = hzb.baseWidth;
  out.hzbBaseHeight = hzb.baseHeight;
  out.hzbLevelCount = hzb.levelCount;
  // Mesmo orçamento de varredura da referência de CPU: qualquer nível é
  // correto, este número só limita quantos texels uma invocação compara.
  out.maximumScannedTexels = 64;
  out.flags = (usable && guarded) ? GpuCullPyramidUsable : 0u;
  if (isOrthographic(frustum)) {
    out.flags |= GpuCullOrthographic;
    out.orthographicHalfWidth = frustum.orthographicHalfWidth;
    out.orthographicHalfHeight = frustum.orthographicHalfHeight;
  }
  return true;
}

GpuCullOutcome cullDrawRecordReference(const GpuCullParameters &parameters,
                                       const GpuCullDrawRecord &record,
                                       const GpuCullHzbView &hzb, u32 &streak) {
  GpuCullOutcome outcome{};
  const bool present = (record.flags & GpuCullRecordPresent) != 0;
  if (!present) {
    outcome.present = false;
    return outcome; // Slot de sobra: nenhuma escrita, nem no estado.
  }
  outcome.present = true;
  const bool testable = (record.flags & GpuCullRecordTestable) != 0;
  const bool usable = (parameters.flags & GpuCullPyramidUsable) != 0 && hzb.texels != nullptr &&
                      hzb.levelCount > 0 && hzb.baseWidth > 0 && hzb.baseHeight > 0;
  if (!testable || !usable) {
    streak = 0;
    return outcome; // visible = true, tested = false.
  }
  outcome.tested = true;

  // Todo caminho de "não pôde concluir oclusão" é o mesmo caminho de "não
  // ocluído": é assim que a CPU já se comporta (isOccludedByHzb devolve false
  // para retângulo inválido e o chamador alimenta a histerese com esse false),
  // e é o que mantém o revive contabilizado igual nos dois lados.
  auto notOccluded = [&]() -> GpuCullOutcome & {
    outcome.revived = streak > 0;
    streak = 0;
    outcome.visible = true;
    return outcome;
  };

  for (u32 axis = 0; axis < 3; ++axis)
    if (!finite(record.boundsCenter[axis])) return notOccluded();
  if (!finite(record.boundsRadius) || record.boundsRadius < 0.0f) return notOccluded();

  // --- Projeção: contraparte exata de projectBoundsToHzbScreenRect ---------
  const float deltaX = record.boundsCenter[0] - parameters.cameraPosition[0];
  const float deltaY = record.boundsCenter[1] - parameters.cameraPosition[1];
  const float deltaZ = record.boundsCenter[2] - parameters.cameraPosition[2];
  const float yawX = parameters.cosineYaw * deltaX - parameters.sineYaw * deltaZ;
  const float yawZ = parameters.sineYaw * deltaX + parameters.cosineYaw * deltaZ;
  const float viewX = yawX;
  const float viewY = parameters.cosinePitch * deltaY + parameters.sinePitch * yawZ;
  const float viewZ = -parameters.sinePitch * deltaY + parameters.cosinePitch * yawZ;

  const float expandedRadius = record.boundsRadius * parameters.boundsScale + parameters.boundsMargin;
  // A guarda de aproximação entra aqui, antes de qualquer projeção: puxar a
  // esfera para perto aumenta o retângulo e diminui a profundidade testada com
  // uma única subtração, que é exatamente o efeito de a câmera ter andado na
  // direção dela desde que a pirâmide foi construída.
  const float nearDepth = viewZ - expandedRadius - parameters.viewDepthGuard;
  // Fail-open idêntico ao da CPU: uma esfera que cruza ou fica atrás do plano
  // próximo não pode ser limitada com segurança em tela e nunca oclui.
  if (!finite(nearDepth) || nearDepth < parameters.nearPlane) return notOccluded();

  const bool orthographic = (parameters.flags & GpuCullOrthographic) != 0;
  const float invHorizontal = 1.0f / (orthographic ? parameters.orthographicHalfWidth : nearDepth * parameters.tangentHalfHorizontal);
  const float invVertical = 1.0f / (orthographic ? parameters.orthographicHalfHeight : nearDepth * parameters.tangentHalfVertical);
  float ndcX[2] = {(viewX - expandedRadius) * invHorizontal,
                         (viewX + expandedRadius) * invHorizontal};
  float ndcY[2] = {(-viewY - expandedRadius) * invVertical,
                         (-viewY + expandedRadius) * invVertical};
  const float farDepth=viewZ+expandedRadius+parameters.viewDepthGuard;
  const float farHorizontal=1.f/(orthographic?parameters.orthographicHalfWidth:farDepth*parameters.tangentHalfHorizontal);
  const float farVertical=1.f/(orthographic?parameters.orthographicHalfHeight:farDepth*parameters.tangentHalfVertical);
  ndcX[0]=std::min(ndcX[0],(viewX-expandedRadius)*farHorizontal);
  ndcX[1]=std::max(ndcX[1],(viewX+expandedRadius)*farHorizontal);
  ndcY[0]=std::min(ndcY[0],(-viewY-expandedRadius)*farVertical);
  ndcY[1]=std::max(ndcY[1],(-viewY+expandedRadius)*farVertical);
  for (u32 i = 0; i < 2; ++i)
    if (!finite(ndcX[i]) || !finite(ndcY[i])) return notOccluded();

  float surfaceMinX = std::numeric_limits<float>::max();
  float surfaceMinY = surfaceMinX;
  float surfaceMaxX = -surfaceMinX;
  float surfaceMaxY = -surfaceMinX;
  for (u32 xi = 0; xi < 2; ++xi) {
    for (u32 yi = 0; yi < 2; ++yi) {
      const float transformedX = parameters.surfaceTransform[0] * ndcX[xi] +
                                 parameters.surfaceTransform[1] * ndcY[yi];
      const float transformedY = parameters.surfaceTransform[2] * ndcX[xi] +
                                 parameters.surfaceTransform[3] * ndcY[yi];
      surfaceMinX = std::min(surfaceMinX, transformedX);
      surfaceMaxX = std::max(surfaceMaxX, transformedX);
      surfaceMinY = std::min(surfaceMinY, transformedY);
      surfaceMaxY = std::max(surfaceMaxY, transformedY);
    }
  }
  surfaceMinX = std::clamp(surfaceMinX, -1.0f, 1.0f);
  surfaceMaxX = std::clamp(surfaceMaxX, -1.0f, 1.0f);
  surfaceMinY = std::clamp(surfaceMinY, -1.0f, 1.0f);
  surfaceMaxY = std::clamp(surfaceMaxY, -1.0f, 1.0f);
  if (surfaceMinX >= surfaceMaxX || surfaceMinY >= surfaceMaxY) return notOccluded();

  // Dilatação lateral resolvida por candidato: o deslocamento em tela de uma
  // translação perpendicular é inversamente proporcional à profundidade.
  const float dilation = parameters.screenDilation +
                         parameters.translationDilationScale / (orthographic ? 1.0f : nearDepth);
  float minU = std::clamp(surfaceMinX * 0.5f + 0.5f - dilation, 0.0f, 1.0f);
  float maxU = std::clamp(surfaceMaxX * 0.5f + 0.5f + dilation, 0.0f, 1.0f);
  float minV = std::clamp(surfaceMinY * 0.5f + 0.5f - dilation, 0.0f, 1.0f);
  float maxV = std::clamp(surfaceMaxY * 0.5f + 0.5f + dilation, 0.0f, 1.0f);

  const float normalizedDepth = orthographic ?
      (nearDepth - parameters.nearPlane) / (parameters.farPlane - parameters.nearPlane) :
      (parameters.farPlane * nearDepth - parameters.nearPlane * parameters.farPlane) /
      ((parameters.farPlane - parameters.nearPlane) * nearDepth);
  if (!finite(normalizedDepth)) return notOccluded();
  const float testedDepth = std::clamp(normalizedDepth, 0.0f, 1.0f);

  // --- Seleção de nível e varredura: contraparte exata de isOccludedByHzb ---
  u32 width = hzb.baseWidth;
  u32 height = hzb.baseHeight;
  usize offset = 0;
  u32 chosenWidth = width;
  usize chosenOffset = 0;
  u32 minX = 0, maxX = 0, minY = 0, maxY = 0;
  for (u32 level = 0; level < hzb.levelCount; ++level) {
    const u32 levelMinX = std::min(static_cast<u32>(minU * static_cast<float>(width)), width - 1);
    const u32 levelMaxX = std::min(
        static_cast<u32>(std::max(std::ceil(maxU * static_cast<float>(width)) - 1.0f, 0.0f)),
        width - 1);
    const u32 levelMinY = std::min(static_cast<u32>(minV * static_cast<float>(height)), height - 1);
    const u32 levelMaxY = std::min(
        static_cast<u32>(std::max(std::ceil(maxV * static_cast<float>(height)) - 1.0f, 0.0f)),
        height - 1);
    const u32 texelCount = (levelMaxX - levelMinX + 1) * (levelMaxY - levelMinY + 1);
    minX = levelMinX; maxX = levelMaxX; minY = levelMinY; maxY = levelMaxY;
    chosenWidth = width;
    chosenOffset = offset;
    if (texelCount <= parameters.maximumScannedTexels) break;
    offset += static_cast<usize>(width) * height;
    width = halveCeiling(width);
    height = halveCeiling(height);
  }

  const u32 scanned = (maxX - minX + 1) * (maxY - minY + 1);
  if (scanned > GpuCullMaximumScanTexels) return notOccluded();

  float farthestNearestDepth = -std::numeric_limits<float>::max();
  for (u32 y = minY; y <= maxY; ++y) {
    for (u32 x = minX; x <= maxX; ++x) {
      const usize index = chosenOffset + static_cast<usize>(y) * chosenWidth + x;
      if (index >= hzb.texelCount) return notOccluded();
      farthestNearestDepth = std::max(farthestNearestDepth, hzb.texels[index]);
    }
  }

  const bool occluded = testedDepth > farthestNearestDepth + parameters.normalizedDepthBias;
  outcome.occludedThisFrame = occluded;

  // --- Histerese: contraparte exata de updateHzbHysteresis -----------------
  if (!occluded) return notOccluded();
  if (streak != std::numeric_limits<u32>::max()) ++streak;
  outcome.visible = streak < parameters.hysteresisFrames;
  return outcome;
}

} // namespace ae::renderer
