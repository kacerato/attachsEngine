#pragma once

#include "core/base.h"
#include <algorithm>
#include <cmath>

namespace ae::scene {

enum class LightKind : u32 { Directional = 0, Point = 1, Spot = 2 };

// O valor autoral continua explícito. Cenas v1 usam Engine para preservar a
// aparência; cenas novas podem usar as unidades físicas sem reinterpretar o
// número quando a modalidade muda.
enum class LightUnit : u32 {
  Engine = 0,
  LuxCandela = 1, // direcional: lux; pontual/spot: candela
  LuxLumen = 2,   // direcional: lux; pontual/spot: lúmen
};

inline constexpr float kMaximumLuminousEfficacy = 683.0f;

inline float lightIntensityForShader(LightKind kind, LightUnit unit, float value,
                                     float innerAngleDegrees, float outerAngleDegrees) noexcept {
  if (!std::isfinite(value) || value < 0.0f) return 0.0f;
  if (unit == LightUnit::Engine) return value;

  float luminousIntensity = value;
  if (unit == LightUnit::LuxLumen && kind != LightKind::Directional) {
    constexpr float pi = 3.14159265358979323846f;
    if (kind == LightKind::Point) {
      luminousIntensity = value / (4.0f * pi);
    } else {
      const float radians = pi / 180.0f;
      const float innerCos = std::cos(std::clamp(innerAngleDegrees, 0.0f, 89.0f) * radians);
      const float outerCos = std::cos(std::clamp(outerAngleDegrees, 0.0f, 89.0f) * radians);
      // O shader vale 1 dentro do cone interno e interpola linearmente em
      // cos(theta) até o cone externo. Esta é a integral dessa mesma janela,
      // não a aproximação de um cone duro diferente do que será desenhado.
      const float solidAngle = 2.0f * pi * (1.0f - 0.5f * (innerCos + outerCos));
      luminousIntensity = value / std::max(solidAngle, 1.0e-4f);
    }
  }
  // A iluminação trabalha em RGB linear. A eficácia máxima de 683 lm/W fixa
  // uma ponte única para lux, candela e lúmen, deixando a exposição da cena
  // controlar a apresentação em vez de escalas arbitrárias por modalidade.
  return luminousIntensity / kMaximumLuminousEfficacy;
}

namespace detail {
inline void blackBodyLinearRgb(float kelvin, float out[3]) noexcept {
  const double temperature = std::clamp(static_cast<double>(kelvin), 1667.0, 25000.0);
  const double t2 = temperature * temperature;
  const double t3 = t2 * temperature;
  const double x = temperature <= 4000.0
      ? -0.2661239e9 / t3 - 0.2343580e6 / t2 + 0.8776956e3 / temperature + 0.179910
      : -3.0258469e9 / t3 + 2.1070379e6 / t2 + 0.2226347e3 / temperature + 0.240390;
  double y = 0.0;
  if (temperature <= 2222.0)
    y = -1.1063814 * x * x * x - 1.34811020 * x * x + 2.18555832 * x - 0.20219683;
  else if (temperature <= 4000.0)
    y = -0.9549476 * x * x * x - 1.37418593 * x * x + 2.09137015 * x - 0.16748867;
  else
    y = 3.0817580 * x * x * x - 5.87338670 * x * x + 3.75112997 * x - 0.37001483;
  const double X = x / y;
  const double Z = (1.0 - x - y) / y;
  out[0] = static_cast<float>(std::max(0.0,  3.2404542 * X - 1.5371385 - 0.4985314 * Z));
  out[1] = static_cast<float>(std::max(0.0, -0.9692660 * X + 1.8760108 + 0.0415560 * Z));
  out[2] = static_cast<float>(std::max(0.0,  0.0556434 * X - 0.2040259 + 1.0572252 * Z));
}
} // namespace detail

inline void lightTemperatureColor(float kelvin, float out[3]) noexcept {
  float value[3]{}, white[3]{};
  detail::blackBodyLinearRgb(kelvin, value);
  detail::blackBodyLinearRgb(6500.0f, white);
  float maximum = 0.0f;
  for (u32 channel = 0; channel < 3; ++channel) {
    out[channel] = white[channel] > 1.0e-6f ? value[channel] / white[channel] : 0.0f;
    maximum = std::max(maximum, out[channel]);
  }
  if (maximum > 1.0f)
    for (u32 channel = 0; channel < 3; ++channel) out[channel] /= maximum;
}

} // namespace ae::scene
