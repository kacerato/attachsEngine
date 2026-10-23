#include "renderer/environment_ambient.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
constexpr float Pi = 3.14159265358979f;
// Convolução lambertiana por banda (Ramamoorthi e Hanrahan 2001): A0 = pi,
// A1 = 2pi/3, A2 = pi/4. Radiância projetada vezes A_l é irradiância.
constexpr float BandScale[DiffuseIrradianceShCoefficientCount]{
    Pi, 2.0f * Pi / 3.0f, 2.0f * Pi / 3.0f, 2.0f * Pi / 3.0f,
    Pi / 4.0f, Pi / 4.0f, Pi / 4.0f, Pi / 4.0f, Pi / 4.0f};
constexpr u32 ProbeSamples = 256;

// Direção de mundo -> espaço da sonda: o mesmo giro do shader.
void toSource(const float world[3], float rotation, float out[3]) {
  const float c = std::cos(rotation), s = std::sin(rotation);
  out[0] = c * world[0] - s * world[2];
  out[1] = world[1];
  out[2] = s * world[0] + c * world[2];
}

template <class Radiance>
void project(Radiance radiance, float rotation, DiffuseIrradianceSh9 &out) {
  out = {};
  // Espiral de Fibonacci: pontos quase uniformes na esfera, mesmo peso.
  const float golden = Pi * (3.0f - std::sqrt(5.0f));
  const float weight = 4.0f * Pi / static_cast<float>(ProbeSamples);
  for (u32 i = 0; i < ProbeSamples; ++i) {
    const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(ProbeSamples);
    const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
    const float angle = golden * static_cast<float>(i);
    const float world[3]{std::cos(angle) * radius, y, std::sin(angle) * radius};
    float value[3];
    radiance(world, value);
    float source[3];
    toSource(world, rotation, source);
    const auto basis = diffuseIrradianceShBasis(source);
    for (u32 k = 0; k < DiffuseIrradianceShCoefficientCount; ++k)
      for (u32 c = 0; c < 3; ++c) out[k][c] += value[c] * basis[k] * weight;
  }
  for (u32 k = 0; k < DiffuseIrradianceShCoefficientCount; ++k)
    for (u32 c = 0; c < 3; ++c) out[k][c] *= BandScale[k];
}
} // namespace

void ambientProbeFromProceduralSky(const ProceduralSkyParameters &sky, const float toSun[3],
                                   float rotation, DiffuseIrradianceSh9 &out) noexcept {
  project([&](const float direction[3], float value[3]) { evaluateProceduralSky(sky, toSun, direction, value); },
          rotation, out);
}

void ambientProbeFromGradient(const float skyColor[3], const float equator[3], const float ground[3],
                              DiffuseIrradianceSh9 &out) noexcept {
  // O Gradient da Unity: céu em cima, equador no horizonte, chão embaixo,
  // interpolados pela altura da direção. Sem giro: o gradiente é vertical.
  project([&](const float direction[3], float value[3]) {
    const float y = direction[1];
    for (u32 c = 0; c < 3; ++c)
      value[c] = y >= 0.0f ? equator[c] + (skyColor[c] - equator[c]) * y
                           : equator[c] + (ground[c] - equator[c]) * -y;
  }, 0.0f, out);
}

void ambientProbeFromColor(const float color[3], DiffuseIrradianceSh9 &out) noexcept {
  // Radiância constante L: irradiância pi*L em toda normal, só a banda 0.
  out = {};
  const float y0 = 0.2820947918f;
  for (u32 c = 0; c < 3; ++c) out[0][c] = Pi * color[c] / y0;
}

const DiffuseIrradianceSh9 &ProceduralAmbientProbeCache::resolve(const ProceduralSkyParameters &sky,
                                                                 const float toSun[3], float rotation) noexcept {
  const float key[13]{sky.skyTint[0], sky.skyTint[1], sky.skyTint[2], sky.ground[0], sky.ground[1], sky.ground[2],
                      sky.atmosphereThickness, sky.exposure, toSun[0], toSun[1], toSun[2], rotation, 1.0f};
  bool same = valid_;
  for (u32 i = 0; same && i < 13; ++i) same = key[i] == key_[i];
  if (!same) {
    ambientProbeFromProceduralSky(sky, toSun, rotation, probe_);
    for (u32 i = 0; i < 13; ++i) key_[i] = key[i];
    valid_ = true;
  }
  return probe_;
}

} // namespace ae::renderer
