#pragma once

#include "core/base.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {

// Leitura dos alvos em 1/2 do pós (bloom, DoF, AO) com resolução dinâmica.
// Espelho exato de halfActiveExtent/clampHalfUv em rhi/shaders/
// post_process_common.glsl: o alvo é alocado na escala do recurso, mas só o
// canto ceil(interna/2) é escrito no quadro. A leitura fica meio texel DO
// ALVO para dentro desse canto; meio texel da cena (o limite antigo) é um
// quarto de texel do alvo e deixava o bilinear puxar o texel não escrito.
inline u32 halfActiveExtent(u32 internalExtent) {
  return std::max(1u, (internalExtent + 1u) / 2u);
}

// uv (0..1 da alocação do alvo em 1/2) presa ao retângulo ativo.
inline float clampHalfUv(float uv, u32 internalExtent, u32 halfAllocation) {
  const float size = static_cast<float>(std::max(1u, halfAllocation));
  const float active = static_cast<float>(std::min(halfActiveExtent(internalExtent), halfAllocation));
  return std::clamp(uv, 0.5f / size, (active - 0.5f) / size);
}

// Limites da leitura da cena (clampSourceUv no shader), em texels: centro do
// primeiro e do último texel que a vista cobre POR INTEIRO. A borda vem da
// superfície girada e cai em fração; o texel cortado por ela pode ter só o
// fundo da câmera (a geometria e a limpeza arredondam diferente no aparelho).
inline float firstDrawnTexelCenter(float viewportStartTexels) { return std::ceil(viewportStartTexels) + 0.5f; }
inline float lastDrawnTexelCenter(float viewportEndTexels) { return std::floor(viewportEndTexels) - 0.5f; }

// Vista da cena em pixels inteiros de uma grade (swapchain ou alvo da cena):
// borda arredondada ao pixel mais próximo dos dois lados, pelo menos 1 pixel.
// Geometria, fundo e pós usam os mesmos inteiros.
struct PixelSpan final {
  u32 first = 0;
  u32 count = 1;
};
inline PixelSpan snapViewportToPixels(float start, float extent, u32 pixels) {
  const float size = static_cast<float>(std::max(1u, pixels));
  const float s = std::isfinite(start) ? start : 0.0f, e = std::isfinite(extent) ? extent : 1.0f;
  const u32 first = static_cast<u32>(std::clamp(std::round(s * size), 0.0f, size - 1.0f));
  const u32 last = static_cast<u32>(std::clamp(std::round((s + e) * size), static_cast<float>(first + 1), size));
  return {first, last - first};
}

} // namespace ae::renderer
