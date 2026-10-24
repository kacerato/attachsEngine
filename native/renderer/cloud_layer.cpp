#include "renderer/cloud_layer.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
u32 hash(u32 x, u32 y, u32 seed) noexcept {
  u32 h = x * 0x8da6b343u ^ y * 0xd8163841u ^ seed * 0xcb1ab31fu;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}
float fade(float t) noexcept { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
// Ruído de gradiente (Perlin) periódico em [0,1)²; resultado ~[-0.7,0.7].
float periodicNoise(float u, float v, int periodX, int periodY, u32 seed) noexcept {
  const float x = u * static_cast<float>(periodX), y = v * static_cast<float>(periodY);
  const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
  float corners[4];
  for (int corner = 0; corner < 4; ++corner) {
    const int dx = corner & 1, dy = corner >> 1;
    // Período por eixo: as faixas de cirro usam períodos diferentes em X e Y.
    const u32 wrappedX = static_cast<u32>((((x0 + dx) % periodX) + periodX) % periodX);
    const u32 wrappedY = static_cast<u32>((((y0 + dy) % periodY) + periodY) % periodY);
    const float angle = static_cast<float>(hash(wrappedX, wrappedY, seed) >> 8) * (6.28318530718f / 16777216.0f);
    corners[corner] = std::cos(angle) * (fx - static_cast<float>(dx)) + std::sin(angle) * (fy - static_cast<float>(dy));
  }
  const float sx = fade(fx), sy = fade(fy);
  const float bottom = corners[0] + (corners[1] - corners[0]) * sx;
  const float top = corners[2] + (corners[3] - corners[2]) * sx;
  return bottom + (top - bottom) * sy;
}
float fbm(float u, float v, int periodX, int periodY, int octaves, float gain, u32 seed) noexcept {
  float sum = 0.0f, amplitude = 0.5f, norm = 0.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += amplitude * periodicNoise(u, v, periodX << octave, periodY << octave, seed + static_cast<u32>(octave) * 101u);
    norm += amplitude; amplitude *= gain;
  }
  return sum / norm;
}
float saturate(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
float smoothstep(float edge0, float edge1, float x) noexcept {
  const float t = saturate((x - edge0) / (edge1 - edge0));
  return t * t * (3.0f - 2.0f * t);
}
u8 unorm(float value) noexcept { return static_cast<u8>(std::lround(saturate(value) * 255.0f)); }
} // namespace

bool buildCloudMapTexture(u32 size, u32 seed, std::vector<u8> &output) {
  if (size < 16 || size > 1024 || (size & (size - 1))) return false;
  std::vector<u8> result(static_cast<usize>(size) * size * 4);
  const float inverse = 1.0f / static_cast<float>(size);
  for (u32 y = 0; y < size; ++y) for (u32 x = 0; x < size; ++x) {
    const float u = (static_cast<float>(x) + 0.5f) * inverse, v = (static_cast<float>(y) + 0.5f) * inverse;
    // Cúmulos: base suave de 4 células por repetição; o contraste leva o ruído
    // (~±0,5) para 0..1 com média perto de 0,5, o que torna a cobertura linear.
    const float cumulus = 0.5f + 1.35f * fbm(u, v, 4, 4, 5, 0.45f, seed);
    // Cirros: esticados em X (períodos 2 × 12) e só a crista do ruído.
    const float cirrus = 0.5f + 1.6f * fbm(u, v, 2, 12, 4, 0.55f, seed + 7919u);
    const float detail = 0.5f + 1.1f * fbm(u, v, 16, 16, 3, 0.5f, seed + 104729u);
    const usize index = (static_cast<usize>(y) * size + x) * 4;
    result[index + 0] = unorm(cumulus);
    result[index + 1] = unorm(cirrus);
    result[index + 2] = unorm(detail);
    result[index + 3] = 255;
  }
  usize previous = 0;
  for (u32 width = size; width > 1; width /= 2) {
    const u32 next = width / 2;
    const usize start = result.size();
    result.resize(start + static_cast<usize>(next) * next * 4);
    for (u32 y = 0; y < next; ++y) for (u32 x = 0; x < next; ++x) for (u32 channel = 0; channel < 4; ++channel) {
      u32 sum = 0;
      for (u32 dy = 0; dy < 2; ++dy) for (u32 dx = 0; dx < 2; ++dx)
        sum += result[previous + ((y * 2 + dy) * width + x * 2 + dx) * 4 + channel];
      result[start + (y * next + x) * 4 + channel] = static_cast<u8>((sum + 2) / 4);
    }
    previous = start;
  }
  output = std::move(result);
  return true;
}

void sampleCloudMap(const std::vector<u8> &map, u32 size, float u, float v, float out[3]) noexcept {
  const float x = (u - std::floor(u)) * static_cast<float>(size) - 0.5f;
  const float y = (v - std::floor(v)) * static_cast<float>(size) - 0.5f;
  const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
  const auto texel = [&](int tx, int ty, u32 channel) {
    const u32 wx = static_cast<u32>(((tx % static_cast<int>(size)) + static_cast<int>(size)) % static_cast<int>(size));
    const u32 wy = static_cast<u32>(((ty % static_cast<int>(size)) + static_cast<int>(size)) % static_cast<int>(size));
    return static_cast<float>(map[(static_cast<usize>(wy) * size + wx) * 4 + channel]) / 255.0f;
  };
  for (u32 channel = 0; channel < 3; ++channel) {
    const float bottom = texel(x0, y0, channel) + (texel(x0 + 1, y0, channel) - texel(x0, y0, channel)) * fx;
    const float top = texel(x0, y0 + 1, channel) + (texel(x0 + 1, y0 + 1, channel) - texel(x0, y0 + 1, channel)) * fx;
    out[channel] = bottom + (top - bottom) * fy;
  }
}

// Mesma regra de rhi/shaders/cloud_layer.glsl (cloudLayerAlpha).
float cloudLayerAlpha(const CloudLayer &clouds, float cumulus, float cirrus, float detail) noexcept {
  if (!clouds.enabled || clouds.opacity <= 0.0f) return 0.0f;
  // A cobertura move o limiar: 0 deixa só o topo do ruído (céu limpo), 1 passa
  // abaixo do vale (encoberto). O detalhe corrói as bordas, não o miolo.
  const float shape = cumulus - (detail - 0.5f) * 0.12f;
  const float threshold = 0.9f - 1.2f * clouds.coverage;
  const float cumulusAlpha = smoothstep(threshold, threshold + 0.3f, shape) * smoothstep(0.0f, 0.06f, clouds.coverage);
  const float cirrusAlpha = smoothstep(0.52f, 0.9f, cirrus) * clouds.cirrus * 0.6f;
  return saturate(cumulusAlpha + cirrusAlpha * (1.0f - cumulusAlpha)) * clouds.opacity;
}

CloudLayerRows packCloudLayer(const CloudLayer &clouds, bool environmentActive, double timeSeconds) noexcept {
  CloudLayerRows rows{};
  if (!environmentActive || !clouds.enabled || !clouds.valid() || clouds.opacity <= 0.0f) return rows;
  rows.parameters[0] = clouds.opacity;
  rows.parameters[1] = clouds.coverage;
  rows.parameters[2] = clouds.cirrus;
  rows.parameters[3] = clouds.density;
  rows.shape[0] = 1.0f / clouds.sizeMeters;
  rows.shape[1] = clouds.altitudeMeters;
  const double radians = static_cast<double>(clouds.windOrientationDegrees) * 0.017453292519943295;
  const double speed = static_cast<double>(clouds.windSpeedKmh) / 3.6;
  const double size = static_cast<double>(clouds.sizeMeters);
  const double time = std::isfinite(timeSeconds) ? timeSeconds : 0.0;
  const double offset[2]{std::cos(radians) * speed * time, std::sin(radians) * speed * time};
  for (u32 axis = 0; axis < 2; ++axis) {
    double wrapped = std::fmod(offset[axis], size);
    if (wrapped < 0.0) wrapped += size;
    rows.shape[2 + axis] = static_cast<float>(wrapped);
  }
  const float exposure = std::exp2(clouds.exposureEv);
  for (u32 channel = 0; channel < 3; ++channel) rows.tint[channel] = clouds.tint[channel] * exposure;
  rows.tint[3] = clouds.ambientDimmer;
  rows.shadow[0] = clouds.castShadows ? clouds.shadowMultiplier : 0.0f;
  rows.shadow[1] = clouds.thicknessMeters;
  rows.shadow[2] = clouds.type == 1 ? 1.0f : 0.0f;
  rows.shadow[3] = clouds.steps;
  rows.volume[0] = clouds.shapeFactor;
  rows.volume[1] = clouds.erosion;
  return rows;
}

float cloudShellDistance(float heightMeters, float viewSine) noexcept {
  // Em quilômetros, como o shader: |R²y²| ~ 4e7 cabe no float com folga.
  const double r = CloudPlanetRadiusMeters * 0.001, h = std::max(heightMeters, 0.0f) * 0.001;
  const double y = std::clamp(static_cast<double>(viewSine), -1.0, 1.0);
  const double c = 2.0 * r * h + h * h;
  const double root = std::sqrt(r * r * y * y + c);
  // t = -R y + sqrt(...) reescrito como c / (R y + sqrt(...)) para y >= 0.
  const double t = y >= 0.0 ? c / (r * y + root) : root - r * y;
  return static_cast<float>(t * 1000.0);
}

float cloudSkyOcclusion(const CloudLayer &clouds) noexcept {
  if (!clouds.enabled || clouds.opacity <= 0.0f) return 0.0f;
  // Volumetric Clouds: o ruído 3D recorta a mancha, e a fração de céu coberta
  // cresce bem mais devagar que a cobertura (0,5 cobre ~25%; 0,68 ~70%; 0,9
  // fecha o céu), medida no emulador do cloud_volume.frag.
  if (clouds.type == 1) {
    const float t = saturate((clouds.coverage - 0.35f) / 0.45f);
    return t * t * (3.0f - 2.0f * t) * clouds.opacity;
  }
  // Área média coberta pela regra de cobertura acima, aproximada pela própria
  // cobertura (o mapa tem média 0,5 e o limiar é linear nela), mais o véu.
  const float cumulus = saturate(clouds.coverage * 1.05f - 0.02f);
  const float cirrus = clouds.cirrus * 0.25f;
  return saturate(cumulus + cirrus * (1.0f - cumulus)) * clouds.opacity;
}

} // namespace ae::renderer
