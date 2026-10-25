#pragma once

#include "core/base.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace ae::renderer {

// PK-5: Bloom › Lens Dirt da URP. A textura escolhida pelo autor (qualquer
// tamanho) é reamostrada na CPU para um quadrado fixo de 512² RGBA8 sRGB, a
// imagem que o pós lê; a proporção original fica para o recorte "cover" que
// a URP aplica (a sujeira cobre a tela sem esticar).
inline constexpr u32 LensDirtSize = 512;

struct LensDirtTexture final {
  float aspect = 1.0f;     // largura/altura da textura original
  std::vector<u8> rgba;    // LensDirtSize² texels RGBA8, linha 0 no topo
  bool valid() const noexcept {
    return aspect > 0.0f && rgba.size() == static_cast<usize>(LensDirtSize) * LensDirtSize * 4u;
  }
};
using SharedLensDirt = std::shared_ptr<const LensDirtTexture>;

// Reamostragem por área (média dos texels que caem em cada célula), suficiente
// para uma textura que só aparece borrada pelo bloom.
inline bool resampleLensDirt(const u8 *rgba, u32 width, u32 height, LensDirtTexture &out) {
  if (!rgba || !width || !height) return false;
  out.aspect = static_cast<float>(width) / static_cast<float>(height);
  out.rgba.assign(static_cast<usize>(LensDirtSize) * LensDirtSize * 4u, 0);
  for (u32 y = 0; y < LensDirtSize; ++y) {
    const u32 y0 = static_cast<u32>(static_cast<u64>(y) * height / LensDirtSize);
    const u32 y1 = std::max(y0 + 1, static_cast<u32>(static_cast<u64>(y + 1) * height / LensDirtSize));
    for (u32 x = 0; x < LensDirtSize; ++x) {
      const u32 x0 = static_cast<u32>(static_cast<u64>(x) * width / LensDirtSize);
      const u32 x1 = std::max(x0 + 1, static_cast<u32>(static_cast<u64>(x + 1) * width / LensDirtSize));
      u64 sum[4]{};u64 count = 0;
      for (u32 sy = y0; sy < y1 && sy < height; ++sy)
        for (u32 sx = x0; sx < x1 && sx < width; ++sx, ++count) {
          const u8 *texel = rgba + (static_cast<usize>(sy) * width + sx) * 4u;
          for (u32 c = 0; c < 4; ++c) sum[c] += texel[c];
        }
      u8 *target = out.rgba.data() + (static_cast<usize>(y) * LensDirtSize + x) * 4u;
      for (u32 c = 0; c < 4; ++c) target[c] = static_cast<u8>(count ? sum[c] / count : 0);
    }
  }
  return true;
}

} // namespace ae::renderer
