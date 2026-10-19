#include "renderer/procedural_sky.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
constexpr float Pi = 3.14159265358979f;
constexpr float OuterRadius = 1.025f, InnerRadius = 1.0f, CameraHeight = 0.0001f;
constexpr float Scale = 1.0f / (OuterRadius - InnerRadius);
constexpr float ScaleDepth = 0.25f, ScaleOverScaleDepth = Scale / ScaleDepth;
constexpr float Mie = 0.0010f, SunBrightness = 20.0f;
constexpr int Samples = 2;

// Aproximação de O'Neil da profundidade óptica fora de escala.
float scaleFunction(float cosine) {
  const float x = 1.0f - cosine;
  return ScaleDepth * std::exp(-0.00287f + x * (0.459f + x * (3.83f + x * (-6.80f + x * 5.25f))));
}
float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
} // namespace

void evaluateProceduralSky(const ProceduralSkyParameters &p, const float toSun[3], const float view[3],
                           float out[3]) noexcept {
  // Comprimentos de onda dirigidos pelo tom: 0,5 dá o espectro padrão; quanto
  // mais azul o tom, menor o comprimento azul e mais espalhamento azul.
  float invWavelength[3];
  const float defaultWavelength[3]{0.65f, 0.57f, 0.475f};
  for (u32 c = 0; c < 3; ++c) {
    const float tint = std::clamp(p.skyTint[c], 0.0f, 1.0f);
    const float wavelength = defaultWavelength[c] + 0.15f * ((1.0f - tint) * 2.0f - 1.0f);
    invWavelength[c] = 1.0f / std::pow(wavelength, 4.0f);
  }
  const float rayleigh = 0.0025f * std::pow(std::clamp(p.atmosphereThickness, 0.0f, 5.0f), 2.5f);
  const float krESun = rayleigh * SunBrightness, kmESun = Mie * SunBrightness;
  const float kr4Pi = rayleigh * 4.0f * Pi, km4Pi = Mie * 4.0f * Pi;
  const float camera[3]{0.0f, InnerRadius + CameraHeight, 0.0f};
  float front[3]{0, 0, 0};
  float cIn[3], cOut[3];
  if (view[1] >= 0.0f) {
    // Céu: do olho até a borda externa da atmosfera.
    const float far = std::sqrt(OuterRadius * OuterRadius + InnerRadius * InnerRadius * view[1] * view[1] -
                                InnerRadius * InnerRadius) - InnerRadius * view[1];
    const float height = InnerRadius + CameraHeight;
    const float depth = std::exp(ScaleOverScaleDepth * (-CameraHeight));
    const float startAngle = dot3(view, camera) / height;
    const float startOffset = depth * scaleFunction(startAngle);
    const float sampleLength = far / Samples, scaledLength = sampleLength * Scale;
    float point[3]{camera[0] + view[0] * sampleLength * 0.5f, camera[1] + view[1] * sampleLength * 0.5f,
                   camera[2] + view[2] * sampleLength * 0.5f};
    for (int i = 0; i < Samples; ++i) {
      const float h = std::sqrt(dot3(point, point));
      const float d = std::exp(ScaleOverScaleDepth * (InnerRadius - h));
      const float lightAngle = dot3(toSun, point) / h, cameraAngle = dot3(view, point) / h;
      const float scatter = startOffset + d * (scaleFunction(lightAngle) - scaleFunction(cameraAngle));
      for (u32 c = 0; c < 3; ++c) {
        const float attenuate = std::exp(-std::clamp(scatter, 0.0f, 50.0f) * (invWavelength[c] * kr4Pi + km4Pi));
        front[c] += attenuate * d * scaledLength;
      }
      for (u32 a = 0; a < 3; ++a) point[a] += view[a] * sampleLength;
    }
    for (u32 c = 0; c < 3; ++c) { cIn[c] = front[c] * invWavelength[c] * krESun; cOut[c] = front[c] * kmESun; }
    const float cosine = dot3(toSun, view);
    const float rayleighPhase = 0.75f * (1.0f + cosine * cosine);
    for (u32 c = 0; c < 3; ++c) out[c] = p.exposure * cIn[c] * rayleighPhase;
  } else {
    // Chão: do olho até a superfície do planeta, como a Unity faz abaixo do horizonte.
    const float far = -CameraHeight / std::min(-0.001f, view[1]);
    const float depth = std::exp(-CameraHeight / ScaleDepth);
    // O ponto do chão fica a ~CameraHeight do olho: a posição é praticamente o
    // "para cima" unitário, então os ângulos são as componentes verticais.
    const float cameraAngle = -view[1];
    const float lightAngle = toSun[1];
    const float cameraScale = scaleFunction(cameraAngle), lightScale = scaleFunction(lightAngle);
    const float cameraOffset = depth * cameraScale, temp = lightScale + cameraScale;
    const float sampleLength = far / Samples, scaledLength = sampleLength * Scale;
    float point[3]{camera[0] + view[0] * sampleLength * 0.5f, camera[1] + view[1] * sampleLength * 0.5f,
                   camera[2] + view[2] * sampleLength * 0.5f};
    float attenuate[3]{};
    for (int i = 0; i < Samples; ++i) {
      const float h = std::sqrt(dot3(point, point));
      const float d = std::exp(ScaleOverScaleDepth * (InnerRadius - h));
      const float scatter = d * temp - cameraOffset;
      for (u32 c = 0; c < 3; ++c) {
        attenuate[c] = std::exp(-std::clamp(scatter, 0.0f, 50.0f) * (invWavelength[c] * kr4Pi + km4Pi));
        front[c] += attenuate[c] * d * scaledLength;
      }
      for (u32 a = 0; a < 3; ++a) point[a] += view[a] * sampleLength;
    }
    for (u32 c = 0; c < 3; ++c) {
      cIn[c] = front[c] * (invWavelength[c] * krESun + kmESun);
      cOut[c] = std::clamp(attenuate[c], 0.0f, 1.0f);
      out[c] = p.exposure * (cIn[c] + p.ground[c] * p.ground[c] * cOut[c]);
    }
  }
  for (u32 c = 0; c < 3; ++c) out[c] = std::isfinite(out[c]) ? std::max(out[c], 0.0f) : 0.0f;
}

void proceduralSkyAmbient(const ProceduralSkyParameters &p, const float toSun[3], float sky[3], float equator[3],
                          float ground[3]) noexcept {
  const float zenith[3]{0.0f, 1.0f, 0.0f};
  evaluateProceduralSky(p, toSun, zenith, sky);
  for (u32 c = 0; c < 3; ++c) equator[c] = 0.0f;
  for (int k = 0; k < 4; ++k) {
    const float angle = Pi * 0.5f * static_cast<float>(k);
    const float direction[3]{std::cos(angle) * 0.985f, 0.17f, std::sin(angle) * 0.985f};
    float sample[3];
    evaluateProceduralSky(p, toSun, direction, sample);
    for (u32 c = 0; c < 3; ++c) equator[c] += sample[c] * 0.25f;
  }
  const float down[3]{0.0f, -1.0f, 0.0f};
  evaluateProceduralSky(p, toSun, down, ground);
}

} // namespace ae::renderer
