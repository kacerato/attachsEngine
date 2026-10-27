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

// Ramo "fora da câmera" do passe principal (mesma regra do shader): o centro
// do pixel conta como dentro da vista com um pixel de saída de folga. A borda
// vem da superfície girada e cai em fração de pixel; sem a folga, a linha logo
// abaixo da barra do editor saía com a cor crua da cena (sem exposição).
inline bool postPixelInsideViewport(float uv, float viewportStart, float viewportExtent, float outputPixel) {
  return uv >= viewportStart - outputPixel && uv <= viewportStart + viewportExtent + outputPixel;
}

} // namespace ae::renderer
