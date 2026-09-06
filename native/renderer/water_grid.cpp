#include "renderer/water_grid.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {

bool finite(float value) noexcept { return std::isfinite(value); }

// Posição bruta do eixo, sem checagem de contrato: a metade central cresce
// linearmente e a externa recebe uma terceira potência, o que dá continuidade
// de curvatura na emenda em |coordinate| = 0,5 sem duplicar vértice.
float axisPosition(const WaterGridSettings &settings, u32 index) noexcept {
  const float coordinate = 2.0f * static_cast<float>(index) /
                           static_cast<float>(settings.segments) - 1.0f;
  const float magnitude = std::fabs(coordinate);
  const float outer = std::max(0.0f, (magnitude - 0.5f) * 2.0f);
  const float offset = settings.nearExtent * magnitude +
                       (settings.farExtent - settings.nearExtent) * outer * outer * outer;
  return std::copysign(offset, coordinate);
}

} // namespace

bool validateWaterGrid(const WaterGridSettings &settings) noexcept {
  if (settings.segments < MinimumWaterGridSegments ||
      settings.segments > MaximumWaterGridSegments || settings.segments % 4u != 0u) {
    return false;
  }
  if (!finite(settings.nearExtent) || !finite(settings.farExtent)) return false;
  if (settings.nearExtent <= 0.0f) return false;
  return settings.farExtent >= settings.nearExtent;
}

float waterGridAxisPosition(const WaterGridSettings &settings, u32 index) noexcept {
  if (!validateWaterGrid(settings) || index > settings.segments) return 0.0f;
  return axisPosition(settings, index);
}

float waterGridAxisSpacing(const WaterGridSettings &settings, u32 index) noexcept {
  if (!validateWaterGrid(settings) || index > settings.segments) return 0.0f;
  const float here = axisPosition(settings, index);
  const float before = axisPosition(settings, index == 0u ? 0u : index - 1u);
  const float after = axisPosition(settings, std::min(settings.segments, index + 1u));
  return std::max(here - before, after - here);
}

const char *waterMeshQualityName(WaterMeshQuality quality) noexcept {
  switch (quality) {
    case WaterMeshQuality::Inherit: return "inherit";
    case WaterMeshQuality::Ultra: return "ultra";
    case WaterMeshQuality::High: return "high";
    case WaterMeshQuality::Medium: return "medium";
    case WaterMeshQuality::Low: return "low";
    case WaterMeshQuality::VeryLow: return "verylow";
    case WaterMeshQuality::Count: break;
  }
  return "medium";
}

WaterGridSettings selectWaterGrid(WaterMeshQuality quality, float windSpeed,
                                  float farExtent) noexcept {
  // Topo de cada perfil. Ultra reproduz a malha assada hoje, para que trocar o
  // cozimento pelo runtime não mude o que já foi medido.
  static constexpr u32 kSegmentsByQuality[static_cast<usize>(WaterMeshQuality::Count)] = {
      128u,  // Inherit: cai no ponto de Medium, ver o cabeçalho
      256u, 192u, 128u, 96u, 64u,
  };
  // Piso: abaixo disto a onda longa vira polígono visível mesmo em mar parado,
  // e a economia deixa de ser invisível.
  static constexpr u32 kFloorByQuality[static_cast<usize>(WaterMeshQuality::Count)] = {
      64u,   // Inherit: idem
      128u, 96u, 64u, 48u, 32u,
  };

  const usize slot = static_cast<usize>(sanitizeWaterMeshQuality(static_cast<u32>(quality)));
  const u32 ceiling = kSegmentsByQuality[slot];
  const u32 floor = kFloorByQuality[slot];

  // Vento inválido é tratado como mar formado, não como mar parado: errar para
  // o lado da malha densa custa quadro, errar para o outro custa a onda.
  const float wind = (!finite(windSpeed) || windSpeed < 0.0f)
                         ? MaximumWaterGridWindSpeed
                         : std::min(windSpeed, MaximumWaterGridWindSpeed);
  const float sea = wind / MaximumWaterGridWindSpeed;
  // Raiz quadrada, não linear: a altura significativa cresce com o quadrado do
  // vento, então a densidade precisa subir cedo. Uma rampa linear deixaria o
  // mar de brisa poligonal justo na faixa em que a onda já é visível.
  const float density = std::sqrt(sea);

  const float span = static_cast<float>(ceiling - floor);
  const u32 raw = floor + static_cast<u32>(std::lround(span * density));
  // O contrato pede múltiplo de 4; arredondar para cima nunca desce do piso.
  const u32 segments = std::clamp(((raw + 3u) / 4u) * 4u, floor, ceiling);

  WaterGridSettings settings{};
  settings.segments = segments;
  settings.farExtent = (finite(farExtent) && farExtent > 0.0f) ? farExtent : 8000.0f;
  // A proporção entre a metade densa e o alcance é o que mantém constante o
  // tamanho angular do triângulo perto da câmera. Ela vem da malha assada
  // (384 de 8000) e não é um número escolhido aqui.
  settings.nearExtent = std::max(1.0f, settings.farExtent * (384.0f / 8000.0f));
  return settings;
}

bool buildWaterGrid(const WaterGridSettings &settings,
                    WaterGridVertex *vertices, usize vertexCapacity,
                    u32 *indices, usize indexCapacity) noexcept {
  if (!validateWaterGrid(settings) || vertices == nullptr || indices == nullptr) return false;
  if (vertexCapacity < waterGridVertexCount(settings)) return false;
  if (indexCapacity < waterGridIndexCount(settings)) return false;

  const u32 stride = settings.segments + 1u;
  const float span = settings.farExtent * 2.0f;

  // O eixo é o mesmo nas duas direções, então calculá-lo uma vez por índice e
  // reusar evita repetir a cúbica 66 mil vezes. Cabe na pilha: 513 floats no
  // maior caso permitido.
  float axis[MaximumWaterGridSegments + 1u];
  float spacing[MaximumWaterGridSegments + 1u];
  for (u32 index = 0; index < stride; ++index) {
    axis[index] = axisPosition(settings, index);
  }
  for (u32 index = 0; index < stride; ++index) {
    const float before = axis[index == 0u ? 0u : index - 1u];
    const float after = axis[index + 1u >= stride ? settings.segments : index + 1u];
    spacing[index] = std::max(axis[index] - before, after - axis[index]);
  }

  for (u32 row = 0; row < stride; ++row) {
    for (u32 column = 0; column < stride; ++column) {
      WaterGridVertex &vertex = vertices[row * stride + column];
      vertex.position[0] = axis[column];
      vertex.position[1] = 0.0f;
      vertex.position[2] = axis[row];
      vertex.uv[0] = axis[column] / span + 0.5f;
      vertex.uv[1] = axis[row] / span + 0.5f;
      // O maior dos dois eixos: um vértice na borda de uma linha densa e de uma
      // coluna esparsa é limitado pela esparsa, não pela média das duas.
      vertex.bandLimit = std::max(spacing[row], spacing[column]);
    }
  }

  usize cursor = 0;
  for (u32 row = 0; row < settings.segments; ++row) {
    for (u32 column = 0; column < settings.segments; ++column) {
      const u32 a = row * stride + column;
      const u32 b = a + 1u, c = a + stride, d = c + 1u;
      // Ordem a,c,b / b,c,d: com x e z crescendo, o produto vetorial aponta
      // para +Y. Uma malha de água virada para baixo desaparece no backface
      // culling e o sintoma é "a água sumiu", não "a normal inverteu".
      indices[cursor++] = a; indices[cursor++] = c; indices[cursor++] = b;
      indices[cursor++] = b; indices[cursor++] = c; indices[cursor++] = d;
    }
  }
  return true;
}

} // namespace ae::renderer
