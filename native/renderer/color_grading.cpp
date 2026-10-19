#include "renderer/color_grading.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::renderer {
namespace {

constexpr float kLuma[3]{0.2126f, 0.7152f, 0.0722f};

float luma(const float c[3]) { return c[0] * kLuma[0] + c[1] * kLuma[1] + c[2] * kLuma[2]; }

float smoothstep(float edge0, float edge1, float x) {
  if (edge1 <= edge0) return x < edge0 ? 0.0f : 1.0f;
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// URP ColorUtils: CAT02 em torno do ponto branco D65; a temperatura desloca o
// x do iluminante padrão e o matiz desloca o y.
void lmsWhite(float temperature, float tint, float out[3]) {
  const float t1 = temperature / 65.0f, t2 = tint / 65.0f;
  const float x = 0.31271f - t1 * (t1 < 0.0f ? 0.1f : 0.05f);
  const float standardY = 2.87f * x - 3.0f * x * x - 0.27509507f;
  const float y = standardY + t2 * 0.05f;
  const float X = x / y, Y = 1.0f, Z = (1.0f - x - y) / y;
  out[0] = 0.7328f * X + 0.4296f * Y - 0.1624f * Z;
  out[1] = -0.7036f * X + 1.6975f * Y + 0.0061f * Z;
  out[2] = 0.0030f * X + 0.0136f * Y + 0.9834f * Z;
}

constexpr float kLinearToLms[9]{3.90405e-1f, 5.49941e-1f, 8.92632e-3f,
                                7.08416e-2f, 9.63172e-1f, 1.35775e-3f,
                                2.31082e-2f, 1.28021e-1f, 9.36245e-1f};
constexpr float kLmsToLinear[9]{2.85847e+0f, -1.62879e+0f, -2.48910e-2f,
                                -2.10182e-1f, 1.15820e+0f, 3.24281e-4f,
                                -4.18120e-2f, -1.18169e-1f, 1.06867e+0f};

void multiply(const float m[9], const float in[3], float out[3]) {
  for (u32 row = 0; row < 3; ++row)
    out[row] = m[row * 3] * in[0] + m[row * 3 + 1] * in[1] + m[row * 3 + 2] * in[2];
}

bool white(const float color[3]) { return color[0] == 1.0f && color[1] == 1.0f && color[2] == 1.0f; }

// Parâmetros derivados uma vez por montagem, não por texel.
struct Prepared final {
  bool whiteBalance = false, toneRanges = false, liftGammaGain = false;
  float balance[3]{1, 1, 1};
  float shadows[3]{1, 1, 1}, midtones[3]{1, 1, 1}, highlights[3]{1, 1, 1};
  float shadowsStart = 0, shadowsEnd = 0, highlightsStart = 0, highlightsEnd = 0;
  float lift[3]{}, inverseGamma[3]{1, 1, 1}, gain[3]{1, 1, 1};
  float vibrance = 0, saturation = 1, contrast = 1;
};

Prepared prepare(const ColorGradingLook &look) {
  const auto &g = look.grading;
  Prepared p;
  p.whiteBalance = g.temperature != 0.0f || g.tint != 0.0f;
  if (p.whiteBalance) {
    // Normalizado pelo próprio branco neutro: temperatura e matiz zero dão
    // exatamente 1, sem o resíduo das constantes de D65.
    float neutral[3], target[3];
    lmsWhite(0.0f, 0.0f, neutral);
    lmsWhite(g.temperature, g.tint, target);
    for (u32 c = 0; c < 3; ++c) p.balance[c] = neutral[c] / target[c];
  }
  p.toneRanges = g.shadows != 0.0f || g.midtones != 0.0f || g.highlights != 0.0f ||
                 !white(g.shadowsColor) || !white(g.midtonesColor) || !white(g.highlightsColor);
  if (p.toneRanges) {
    // Cor do trackball vezes (1 + deslocamento): -1 apaga a faixa, +1 dobra.
    for (u32 c = 0; c < 3; ++c) {
      p.shadows[c] = g.shadowsColor[c] * (1.0f + g.shadows);
      p.midtones[c] = g.midtonesColor[c] * (1.0f + g.midtones);
      p.highlights[c] = g.highlightsColor[c] * (1.0f + g.highlights);
    }
    p.shadowsStart = g.shadowsStart;p.shadowsEnd = g.shadowsEnd;
    p.highlightsStart = g.highlightsStart;p.highlightsEnd = g.highlightsEnd;
  }
  p.liftGammaGain = g.lift != 0.0f || g.gamma != 0.0f || g.gain != 0.0f ||
                    !white(g.liftColor) || !white(g.gammaColor) || !white(g.gainColor);
  if (p.liftGammaGain) {
    // URP PrepareLiftGammaGain: o matiz do trackball sai como desvio em torno da
    // própria luminância; o deslocamento move a faixa inteira. Lift × 0,2,
    // gama e ganho × 0,8, como lá, para o curso do controle não estourar.
    const float liftLuma = luma(g.liftColor), gammaLuma = luma(g.gammaColor), gainLuma = luma(g.gainColor);
    for (u32 c = 0; c < 3; ++c) {
      p.lift[c] = 0.2f * (g.liftColor[c] - liftLuma + g.lift);
      p.gain[c] = 0.8f * (g.gainColor[c] - gainLuma + g.gain) + 1.0f;
      p.inverseGamma[c] = 1.0f / std::max(0.8f * (g.gammaColor[c] - gammaLuma + g.gamma) + 1.0f, 1.0e-3f);
    }
  }
  p.vibrance = g.vibrance;
  p.saturation = look.saturation;
  p.contrast = look.contrast;
  return p;
}

void grade(const Prepared &p, const float in[3], float out[3]) {
  float c[3]{std::max(in[0], 0.0f), std::max(in[1], 0.0f), std::max(in[2], 0.0f)};
  if (p.whiteBalance) {
    float lms[3];
    multiply(kLinearToLms, c, lms);
    for (u32 i = 0; i < 3; ++i) lms[i] *= p.balance[i];
    multiply(kLmsToLinear, lms, c);
    for (float &value : c) value = std::max(value, 0.0f);
  }
  if (p.toneRanges) {
    const float y = luma(c);
    const float shadowWeight = 1.0f - smoothstep(p.shadowsStart, p.shadowsEnd, y);
    const float highlightWeight = smoothstep(p.highlightsStart, p.highlightsEnd, y);
    const float midWeight = std::max(0.0f, 1.0f - shadowWeight - highlightWeight);
    for (u32 i = 0; i < 3; ++i)
      c[i] *= p.shadows[i] * shadowWeight + p.midtones[i] * midWeight + p.highlights[i] * highlightWeight;
  }
  if (p.liftGammaGain) {
    for (u32 i = 0; i < 3; ++i) {
      const float value = std::max(c[i] * p.gain[i] + p.lift[i], 0.0f);
      c[i] = std::pow(value, p.inverseGamma[i]);
    }
  }
  if (p.vibrance != 0.0f) {
    const float high = std::max({c[0], c[1], c[2]}), low = std::min({c[0], c[1], c[2]});
    const float chroma = high > 1.0e-5f ? (high - low) / high : 0.0f;
    const float scale = std::max(0.0f, 1.0f + p.vibrance * (1.0f - chroma));
    const float gray = luma(c);
    for (float &value : c) value = std::max(gray + (value - gray) * scale, 0.0f);
  }
  // Mesma ordem e mesma fórmula do caminho sem LUT em post_process_common.glsl.
  const float gray = luma(c);
  for (u32 i = 0; i < 3; ++i) {
    const float saturated = gray + (c[i] - gray) * p.saturation;
    out[i] = (saturated - 0.5f) * p.contrast + 0.5f;
  }
}

template <class Store>
bool build(const ColorGradingLook &look, Store &&store) {
  const Prepared prepared = prepare(look);
  constexpr float last = static_cast<float>(ColorGradingLutSize - 1);
  usize index = 0;
  for (u32 b = 0; b < ColorGradingLutSize; ++b)
    for (u32 g = 0; g < ColorGradingLutSize; ++g)
      for (u32 r = 0; r < ColorGradingLutSize; ++r, ++index) {
        // Coordenada = sqrt(cor): a célula i representa (i / 31)².
        const float coordinate[3]{r / last, g / last, b / last};
        const float in[3]{coordinate[0] * coordinate[0], coordinate[1] * coordinate[1],
                          coordinate[2] * coordinate[2]};
        float out[3];
        grade(prepared, in, out);
        for (float value : out) if (!std::isfinite(value)) return false;
        store(index, out);
      }
  return true;
}

} // namespace

ColorGradingLook colorGradingLook(const SceneEnvironment &environment) noexcept {
  return {environment.grading, environment.contrast, environment.saturation};
}

void gradeDisplayColor(const ColorGradingLook &look, const float in[3], float out[3]) noexcept {
  grade(prepare(look), in, out);
}

bool buildColorGradingLut(const ColorGradingLook &look, std::span<float> rgba) noexcept {
  if (rgba.size() != static_cast<usize>(ColorGradingLutTexels) * 4u) return false;
  return build(look, [&](usize index, const float out[3]) {
    float *texel = rgba.data() + index * 4u;
    texel[0] = out[0];texel[1] = out[1];texel[2] = out[2];texel[3] = 1.0f;
  });
}

bool buildColorGradingLutHalf(const ColorGradingLook &look, std::span<u16> rgba) noexcept {
  if (rgba.size() != static_cast<usize>(ColorGradingLutTexels) * 4u) return false;
  const u16 one = colorGradingFloatToHalf(1.0f);
  return build(look, [&](usize index, const float out[3]) {
    u16 *texel = rgba.data() + index * 4u;
    for (u32 c = 0; c < 3; ++c) texel[c] = colorGradingFloatToHalf(out[c]);
    texel[3] = one;
  });
}

void sampleColorGradingLut(std::span<const float> rgba, const float in[3], float out[3]) noexcept {
  out[0] = out[1] = out[2] = 0.0f;
  if (rgba.size() != static_cast<usize>(ColorGradingLutTexels) * 4u) return;
  constexpr u32 n = ColorGradingLutSize;
  // Shader: coord = sqrt(clamp(c)) * (n-1)/n + 0.5/n, filtro linear. Em
  // unidades de texel isso é sqrt(c) * (n-1), entre centros de célula.
  float position[3];u32 base[3];float fraction[3];
  for (u32 axis = 0; axis < 3; ++axis) {
    position[axis] = std::sqrt(std::clamp(in[axis], 0.0f, 1.0f)) * static_cast<float>(n - 1);
    base[axis] = std::min(static_cast<u32>(position[axis]), n - 2);
    fraction[axis] = position[axis] - static_cast<float>(base[axis]);
  }
  for (u32 corner = 0; corner < 8; ++corner) {
    float weight = 1.0f;u32 cell[3];
    for (u32 axis = 0; axis < 3; ++axis) {
      const bool upper = (corner >> axis) & 1u;
      cell[axis] = base[axis] + (upper ? 1u : 0u);
      weight *= upper ? fraction[axis] : 1.0f - fraction[axis];
    }
    const float *texel = rgba.data() + (static_cast<usize>(cell[2]) * n * n + cell[1] * n + cell[0]) * 4u;
    for (u32 c = 0; c < 3; ++c) out[c] += texel[c] * weight;
  }
}

u16 colorGradingFloatToHalf(float value) noexcept {
  u32 bits;std::memcpy(&bits, &value, sizeof bits);
  const u32 sign = (bits >> 16) & 0x8000u;
  const i32 exponent = static_cast<i32>((bits >> 23) & 0xffu) - 127 + 15;
  u32 mantissa = bits & 0x7fffffu;
  if (((bits >> 23) & 0xffu) == 0xffu) return static_cast<u16>(sign | 0x7c00u | (mantissa ? 0x200u : 0u));
  if (exponent >= 31) return static_cast<u16>(sign | 0x7c00u);
  if (exponent <= 0) {
    if (exponent < -10) return static_cast<u16>(sign);
    mantissa |= 0x800000u;
    const u32 shift = static_cast<u32>(14 - exponent);
    u32 half = mantissa >> shift;
    const u32 remainder = mantissa & ((1u << shift) - 1u), halfway = 1u << (shift - 1u);
    if (remainder > halfway || (remainder == halfway && (half & 1u))) ++half;
    return static_cast<u16>(sign | half);
  }
  u32 half = (static_cast<u32>(exponent) << 10) | (mantissa >> 13);
  const u32 remainder = mantissa & 0x1fffu;
  if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u))) ++half; // pode subir o expoente: correto
  return static_cast<u16>(sign | half);
}

float colorGradingHalfToFloat(u16 value) noexcept {
  const u32 sign = (value & 0x8000u) << 16, exponent = (value >> 10) & 0x1fu, mantissa = value & 0x3ffu;
  u32 bits;
  if (exponent == 0) {
    if (!mantissa) bits = sign;
    else {
      // Subnormal: normaliza.
      u32 m = mantissa;i32 e = -1;
      do { ++e; m <<= 1; } while (!(m & 0x400u));
      bits = sign | (static_cast<u32>(127 - 15 - e) << 23) | ((m & 0x3ffu) << 13);
    }
  } else if (exponent == 31) bits = sign | 0x7f800000u | (mantissa << 13);
  else bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  float result;std::memcpy(&result, &bits, sizeof result);
  return result;
}

} // namespace ae::renderer
