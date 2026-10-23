#include "renderer/time_of_day.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
constexpr float Pi = 3.14159265358979f;
constexpr float Radians = Pi / 180.0f;

float smoothstep(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
float mix(float a, float b, float t) { return a + (b - a) * t; }

// Aproximação de Tanner Helland do corpo negro, normalizada pelo branco de 6500 K.
void kelvinToLinear(float kelvin, float out[3]) {
  const float t = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
  float r = t <= 66.0f ? 255.0f : 329.698727446f * std::pow(t - 60.0f, -0.1332047592f);
  float g = t <= 66.0f ? 99.4708025861f * std::log(t) - 161.1195681661f
                       : 288.1221695283f * std::pow(t - 60.0f, -0.0755148492f);
  float b = t >= 66.0f ? 255.0f : (t <= 19.0f ? 0.0f : 138.5177312231f * std::log(t - 10.0f) - 305.0447927307f);
  const float srgb[3]{std::clamp(r, 0.0f, 255.0f) / 255.0f, std::clamp(g, 0.0f, 255.0f) / 255.0f,
                      std::clamp(b, 0.0f, 255.0f) / 255.0f};
  for (u32 i = 0; i < 3; ++i) {
    const float c = srgb[i];
    out[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
  }
  const float peak = std::max({out[0], out[1], out[2], 1e-4f});
  for (u32 i = 0; i < 3; ++i) out[i] /= peak;
}
} // namespace

TimeOfDayState evaluateTimeOfDay(const TimeOfDayInput &input) noexcept {
  TimeOfDayState state{};
  const float hour = std::isfinite(input.hour) ? input.hour : 12.0f;
  const float day = std::isfinite(input.dayOfYear) ? input.dayOfYear : 172.0f;
  const float latitude = std::clamp(std::isfinite(input.latitude) ? input.latitude : 0.0f, -90.0f, 90.0f) * Radians;
  // Cooper: declinação = 23,44° · sen(360/365 · (284 + n)).
  const float declination = 23.44f * Radians * std::sin(2.0f * Pi * (284.0f + day) / 365.0f);
  const float hourAngle = (hour - 12.0f) * 15.0f * Radians;
  const float sinElevation = std::sin(latitude) * std::sin(declination) +
                             std::cos(latitude) * std::cos(declination) * std::cos(hourAngle);
  const float elevation = std::asin(std::clamp(sinElevation, -1.0f, 1.0f));
  // Azimute a partir do norte, crescendo para leste.
  const float cosAzimuth = (std::sin(declination) - std::sin(elevation) * std::sin(latitude)) /
                           std::max(std::cos(elevation) * std::cos(latitude), 1e-5f);
  float azimuth = std::acos(std::clamp(cosAzimuth, -1.0f, 1.0f));
  if (hourAngle > 0.0f) azimuth = 2.0f * Pi - azimuth; // tarde: oeste
  azimuth += (std::isfinite(input.northDegrees) ? input.northDegrees : 0.0f) * Radians;
  const float horizontal = std::cos(elevation);
  state.toSun[0] = horizontal * std::sin(azimuth);   // leste = +X
  state.toSun[1] = std::sin(elevation);
  state.toSun[2] = -horizontal * std::cos(azimuth);  // norte = -Z
  state.elevation = state.toSun[1];

  // Temperatura: ~2000 K no horizonte, ~5800 K com o sol a 40° ou mais.
  const float high = smoothstep(-0.02f, 0.65f, state.elevation);
  kelvinToLinear(mix(2000.0f, 5800.0f, high), state.sunColor);
  // Intensidade some suave entre +3° e -4° (o disco cruza o horizonte, depois o
  // crepúsculo civil); de dia sobe com a massa de ar menor.
  state.sunIntensity = smoothstep(-0.07f, 0.05f, state.elevation) * mix(0.35f, 1.0f, high);

  const float lunation = 29.530589f;
  // Lua nova de referência em 6 jan (dia 6); fase em fração do ciclo.
  float phase = std::fmod((day - 6.0f) / lunation, 1.0f);
  if (phase < 0.0f) phase += 1.0f;
  state.moonPhase = phase;
  return state;
}

MoonState evaluateMoon(const TimeOfDayState &sun) noexcept {
  MoonState moon{};
  const float *s = sun.toSun;
  // eixo = normalize(cross(toSun, up)); cross(a, (0,1,0)) = (-a.z, 0, a.x).
  float axis[3]{-s[2], 0.0f, s[0]};
  float length = std::sqrt(axis[0] * axis[0] + axis[2] * axis[2]);
  if (length < 1e-3f) { axis[0] = 1.0f; axis[2] = 0.0f; length = 1.0f; }
  axis[0] /= length; axis[2] /= length;
  const float angle = 2.0f * Pi * sun.moonPhase;
  const float c = std::cos(angle), sn = std::sin(angle);
  const float turned[3]{axis[1] * s[2] - axis[2] * s[1], axis[2] * s[0] - axis[0] * s[2],
                        axis[0] * s[1] - axis[1] * s[0]};
  for (u32 i = 0; i < 3; ++i) moon.toMoon[i] = s[i] * c + turned[i] * sn;
  const float norm = std::sqrt(moon.toMoon[0] * moon.toMoon[0] + moon.toMoon[1] * moon.toMoon[1] +
                               moon.toMoon[2] * moon.toMoon[2]);
  if (norm > 1e-6f) for (float &v : moon.toMoon) v /= norm;
  moon.elevation = moon.toMoon[1];
  moon.illuminated = 0.5f * (1.0f - c);
  const float above = smoothstep(-0.04f, 0.06f, moon.elevation);
  const float sunDown = 1.0f - smoothstep(-0.10f, 0.02f, sun.elevation);
  moon.intensity = above * sunDown * moon.illuminated * moon.illuminated * moon.illuminated;
  return moon;
}

void celestialSphereRows(const TimeOfDayInput &input, float rows[3][3]) noexcept {
  const float hour = std::isfinite(input.hour) ? input.hour : 12.0f;
  const float day = std::isfinite(input.dayOfYear) ? input.dayOfYear : 172.0f;
  const float latitude = std::clamp(std::isfinite(input.latitude) ? input.latitude : 0.0f, -90.0f, 90.0f) * Radians;
  const float north = (std::isfinite(input.northDegrees) ? input.northDegrees : 0.0f) * Radians;
  // Mesmas convenções de evaluateTimeOfDay: azimute a partir do norte (-Z),
  // crescendo para o leste (+X), girado por northDegrees.
  const float n[3]{std::sin(north), 0.0f, -std::cos(north)};
  const float east[3]{std::cos(north), 0.0f, std::sin(north)};
  const float up[3]{0.0f, 1.0f, 0.0f};
  float pole[3], equator[3], west[3];
  for (u32 i = 0; i < 3; ++i) {
    pole[i] = n[i] * std::cos(latitude) + up[i] * std::sin(latitude);
    equator[i] = up[i] * std::cos(latitude) - n[i] * std::sin(latitude);
    west[i] = -east[i];
  }
  // Ângulo sideral local: ângulo horário do sol mais a ascensão reta dele
  // (zero no equinócio de março, dia 80).
  const float sidereal = (hour - 12.0f) * 15.0f * Radians + 2.0f * Pi * (day - 80.0f) / 365.2422f;
  const float c = std::cos(sidereal), s = std::sin(sidereal);
  for (u32 i = 0; i < 3; ++i) {
    rows[0][i] = c * equator[i] + s * west[i];
    rows[1][i] = s * equator[i] - c * west[i];
    rows[2][i] = pole[i];
  }
}

float advanceTimeOfDayHour(float authoredHour, float dayLengthMinutes, double elapsedSeconds) noexcept {
  double hour = std::isfinite(authoredHour) ? authoredHour : 12.0;
  if (dayLengthMinutes > 0.0f && std::isfinite(dayLengthMinutes) && std::isfinite(elapsedSeconds))
    hour += elapsedSeconds * 24.0 / (static_cast<double>(dayLengthMinutes) * 60.0);
  hour = std::fmod(hour, 24.0);
  if (hour < 0.0) hour += 24.0;
  return static_cast<float>(hour);
}


} // namespace ae::renderer
