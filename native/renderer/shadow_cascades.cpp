#include "renderer/shadow_cascades.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {

struct Vec3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;
};

Vec3 load(const float v[3]) { return {v[0], v[1], v[2]}; }
void store(const Vec3 &v, float out[3]) { out[0] = v.x; out[1] = v.y; out[2] = v.z; }

Vec3 add(const Vec3 &a, const Vec3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(const Vec3 &a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3 &a, const Vec3 &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(const Vec3 &a) { return std::sqrt(dot(a, a)); }

bool finite(const Vec3 &a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

bool normalize(Vec3 &a) {
  const float len = length(a);
  if (!std::isfinite(len) || len <= 1e-6f) return false;
  a = scale(a, 1.0f / len);
  return true;
}

} // namespace

u32 computeCascadeSplits(float nearPlane, float shadowDistance, u32 count, float lambda,
                         float *outSplits) {
  if (outSplits == nullptr || count == 0 || count > MaximumShadowCascades) return 0;
  if (!std::isfinite(nearPlane) || !std::isfinite(shadowDistance)) return 0;
  if (nearPlane <= 0.0f || shadowDistance <= nearPlane) return 0;
  const float blend = std::clamp(std::isfinite(lambda) ? lambda : DefaultCascadeSplitLambda,
                                 0.0f, 1.0f);
  const float ratio = shadowDistance / nearPlane;
  for (u32 index = 0; index < count; ++index) {
    const float fraction = static_cast<float>(index + 1) / static_cast<float>(count);
    // Uniforme distribui distância igualmente; logarítmica distribui resolução
    // angular igualmente. A mistura é o compromisso padrão de CSM.
    const float uniform = nearPlane + (shadowDistance - nearPlane) * fraction;
    const float logarithmic = nearPlane * std::pow(ratio, fraction);
    outSplits[index] = blend * logarithmic + (1.0f - blend) * uniform;
  }
  // A última fatia termina exatamente na distância pedida: acumular erro de
  // arredondamento aqui deixaria uma faixa sem sombra no limite do alcance.
  outSplits[count - 1] = shadowDistance;
  return count;
}

u32 computeShadowCascades(const ShadowCascadeInput &input, u32 count, float lambda,
                          ShadowCascade *outCascades) {
  if (outCascades == nullptr || count == 0 || count > MaximumShadowCascades) return 0;
  if (input.cascadeResolution == 0) return 0;

  Vec3 forward = load(input.cameraForward);
  Vec3 up = load(input.cameraUp);
  Vec3 lightDirection = load(input.lightDirection);
  const Vec3 eye = load(input.cameraPosition);
  if (!finite(eye) || !normalize(forward) || !normalize(lightDirection)) return 0;
  if (!std::isfinite(input.verticalFovRadians) || input.verticalFovRadians <= 0.0f ||
      input.verticalFovRadians >= 3.14159f) {
    return 0;
  }
  if (!std::isfinite(input.aspectRatio) || input.aspectRatio <= 0.0f) return 0;

  // Ortonormaliza o "up" da câmera contra a direção de visão. Um up degenerado
  // (paralelo ao forward) invalida a base e a cascata inteira.
  up = sub(up, scale(forward, dot(forward, up)));
  if (!normalize(up)) return 0;
  const Vec3 right = cross(forward, up);

  float splits[MaximumShadowCascades]{};
  if (computeCascadeSplits(input.nearPlane, input.shadowDistance, count, lambda, splits) == 0) {
    return 0;
  }

  const float tanHalfVertical = std::tan(input.verticalFovRadians * 0.5f);
  const float tanHalfHorizontal = tanHalfVertical * input.aspectRatio;

  // Base da luz. Escolhe um "up" auxiliar que não seja paralelo à direção da luz,
  // senão o produto vetorial colapsa quando o sol está a pino.
  Vec3 lightUp{0.0f, 1.0f, 0.0f};
  if (std::fabs(dot(lightDirection, lightUp)) > 0.99f) lightUp = {0.0f, 0.0f, 1.0f};
  Vec3 lightRight = cross(lightUp, lightDirection);
  if (!normalize(lightRight)) return 0;
  lightUp = cross(lightDirection, lightRight);
  if (!normalize(lightUp)) return 0;

  float sliceNear = input.nearPlane;
  const float blendRatio = std::isfinite(input.cascadeBlendRatio)
                               ? std::clamp(input.cascadeBlendRatio, 0.0f, 0.5f) : 0.0f;
  for (u32 index = 0; index < count; ++index) {
    const float sliceFar = splits[index];
    ShadowCascade &cascade = outCascades[index];
    cascade = {};
    cascade.nearDistance = sliceNear;
    cascade.farDistance = sliceFar;
    // Shading blends BEFORE the nominal split. Fit both receivers, otherwise
    // the next lookup can read outside its projection while its weight is nonzero.
    const float previousStart = index > 1 ? splits[index - 2] : 0.0f;
    const float receiverNear = index > 0
        ? std::max(input.nearPlane, sliceNear - (sliceNear - previousStart) * blendRatio)
        : sliceNear;

    // Esfera que circunscreve a fatia do frustum. Usar a esfera, e não a caixa
    // alinhada à luz, é o que mantém o volume invariante à rotação da câmera: o
    // jogador olhar em volta não pode mudar a resolução efetiva da sombra.
    const Vec3 nearCenter = add(eye, scale(forward, receiverNear));
    const Vec3 farCenter = add(eye, scale(forward, sliceFar));
    const Vec3 farCorner = add(add(farCenter, scale(right, sliceFar * tanHalfHorizontal)),
                               scale(up, sliceFar * tanHalfVertical));
    const Vec3 nearCorner = add(add(nearCenter, scale(right, receiverNear * tanHalfHorizontal)),
                                scale(up, receiverNear * tanHalfVertical));
    // O centro da esfera fica no eixo de visão; o raio cobre os dois cantos.
    const Vec3 axisMidpoint = add(eye, scale(forward, (receiverNear + sliceFar) * 0.5f));
    const float exactRadius = std::max(length(sub(farCorner, axisMidpoint)),
                                       length(sub(nearCorner, axisMidpoint)));
    const float guardBand = std::clamp(input.receiverGuardBandRatio, 1.0f, 1.25f);
    const float radius = exactRadius * guardBand;
    if (!std::isfinite(radius) || radius <= 0.0f) return 0;

    Vec3 center = axisMidpoint;
    const float diameter = radius * 2.0f;
    const float texelWorldSize = diameter / static_cast<float>(input.cascadeResolution);

    if (texelWorldSize <= 0.0f || !std::isfinite(texelWorldSize)) return 0;

    // Ancoragem a texel: projeta o centro na base da luz, arredonda cada eixo para
    // um múltiplo inteiro do texel e volta para o mundo. Sem isto a borda da sombra
    // ferve a cada movimento sub-texel da câmera.
    const float centerRight = dot(center, lightRight);
    const float centerUp = dot(center, lightUp);
    const float centerDepth = dot(center, lightDirection);
    const float snappedRight = std::floor(centerRight / texelWorldSize) * texelWorldSize;
    const float snappedUp = std::floor(centerUp / texelWorldSize) * texelWorldSize;
    center = add(add(scale(lightRight, snappedRight), scale(lightUp, snappedUp)),
                 scale(lightDirection, centerDepth));

    store(center, cascade.centerWorld);
    cascade.radiusWorld = radius;
    cascade.worldUnitsPerTexel = texelWorldSize;

    // Olho da luz recuado o suficiente para que casters fora do frustum ainda
    // entrem no volume. `casterExtrusion` é política, não constante mágica.
    const float extrusion = std::max(input.casterExtrusion, radius);
    store(lightDirection, cascade.lightDirection);
    cascade.casterExtrusionWorld = extrusion;
    const Vec3 lightEye = sub(center, scale(lightDirection, extrusion));
    const float depthRange = extrusion + radius;

    // View da luz: linhas são a base ortonormal, translação é -base·olho.
    const float viewRight[3] = {lightRight.x, lightRight.y, lightRight.z};
    const float viewUp[3] = {lightUp.x, lightUp.y, lightUp.z};
    const float viewForward[3] = {lightDirection.x, lightDirection.y, lightDirection.z};
    const float translationX = -dot(lightRight, lightEye);
    const float translationY = -dot(lightUp, lightEye);
    const float translationZ = -dot(lightDirection, lightEye);

    // Ortográfica com profundidade em [0,1] (convenção Vulkan), sem inversão de Y
    // aqui — a pré-rotação da surface é aplicada no passe de shading, não no mapa.
    const float inverseRadius = 1.0f / radius;
    const float inverseDepth = 1.0f / std::max(depthRange, 1e-4f);

    // Column-major: coluna c ocupa viewProjection[c*4 .. c*4+3].
    float *m = cascade.viewProjection;
    m[0] = viewRight[0] * inverseRadius;
    m[1] = viewUp[0] * inverseRadius;
    m[2] = viewForward[0] * inverseDepth;
    m[3] = 0.0f;
    m[4] = viewRight[1] * inverseRadius;
    m[5] = viewUp[1] * inverseRadius;
    m[6] = viewForward[1] * inverseDepth;
    m[7] = 0.0f;
    m[8] = viewRight[2] * inverseRadius;
    m[9] = viewUp[2] * inverseRadius;
    m[10] = viewForward[2] * inverseDepth;
    m[11] = 0.0f;
    m[12] = translationX * inverseRadius;
    m[13] = translationY * inverseRadius;
    m[14] = translationZ * inverseDepth;
    m[15] = 1.0f;

    sliceNear = sliceFar;
  }
  return count;
}

bool isShadowCasterVisible(const ShadowCascade &cascade, const float center[3], float radius) {
  if (center == nullptr || !std::isfinite(radius) || radius < 0.0f ||
      !std::isfinite(cascade.radiusWorld) || cascade.radiusWorld <= 0.0f ||
      !std::isfinite(cascade.casterExtrusionWorld) || cascade.casterExtrusionWorld <= 0.0f) {
    return true;
  }
  const Vec3 casterCenter = load(center);
  const Vec3 cascadeCenter = load(cascade.centerWorld);
  Vec3 lightDirection = load(cascade.lightDirection);
  if (!finite(casterCenter) || !finite(cascadeCenter) || !normalize(lightDirection)) return true;

  const Vec3 delta = sub(casterCenter, cascadeCenter);
  const float axialDistance = dot(delta, lightDirection);
  // A esfera inteira está antes do olho da luz ou depois da fatia receptora.
  if (axialDistance + radius < -cascade.casterExtrusionWorld ||
      axialDistance - radius > cascade.radiusWorld) {
    return false;
  }

  // Raios da luz são paralelos: só casters cuja projeção lateral cruza a esfera
  // receptora podem lançar sombra nela. O círculo é mais conservador para esse
  // receptor que testar apenas o frustum da câmera.
  const float deltaSquared = dot(delta, delta);
  const float perpendicularSquared = std::max(0.0f, deltaSquared - axialDistance * axialDistance);
  const float lateralLimit = cascade.radiusWorld + radius;
  return perpendicularSquared <= lateralLimit * lateralLimit;
}

bool canReuseStaticShadowCascade(const ShadowCascade &cached, const ShadowCascade &desired,
                                 float guardBandRatio) {
  if (!std::isfinite(guardBandRatio) || guardBandRatio < 1.0f ||
      !std::isfinite(cached.radiusWorld) || !std::isfinite(desired.radiusWorld) ||
      cached.radiusWorld <= 0.0f || desired.radiusWorld <= 0.0f) return false;
  Vec3 cachedLight = load(cached.lightDirection);
  Vec3 desiredLight = load(desired.lightDirection);
  if (!normalize(cachedLight) || !normalize(desiredLight) || dot(cachedLight, desiredLight) < 0.99999f)
    return false;
  if (std::fabs(cached.nearDistance - desired.nearDistance) > 1e-4f ||
      std::fabs(cached.farDistance - desired.farDistance) > 1e-3f) return false;

  const Vec3 delta = sub(load(desired.centerWorld), load(cached.centerWorld));
  if (!finite(delta)) return false;
  const float desiredExactRadius = desired.radiusWorld / guardBandRatio;
  const float axial = dot(delta, cachedLight);
  const float perpendicularSquared = std::max(0.0f, dot(delta, delta) - axial * axial);
  const float lateralAllowance = cached.radiusWorld - desiredExactRadius;
  if (lateralAllowance < 0.0f || perpendicularSquared > lateralAllowance * lateralAllowance)
    return false;
  return axial - desiredExactRadius >= -cached.casterExtrusionWorld &&
         axial + desiredExactRadius <= cached.radiusWorld;
}

} // namespace ae::renderer
