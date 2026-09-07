// Rasterizador em software da interface — espelho de astra_ui.frag.
//
// Ele existe por uma razão específica: a interface do editor é desenhada pela
// GPU, e sem aparelho não há como ver se um painel ficou onde deveria. Um teste
// que compara números prova que a caixa mede 320 px; ele não prova que a tela
// parece a tela. Este rasterizador fecha essa distância antes de existir
// qualquer pipeline Vulkan para culpar.
//
// **Ele é um espelho, não uma segunda implementação.** Cada função aqui tem a
// contraparte no shader e as duas precisam concordar; divergir é defeito, não
// aproximação de CPU. É a mesma disciplina de `cullDrawRecordReference` para
// `draw_cull.comp`.
//
// Vive em tests/ e não em native/: nada do produto o linka, e ele não deve
// virar um caminho de desenho alternativo por conveniência.
#pragma once

#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include "ui/ui_instance_builder.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace ae::test {

struct UiSoftwareTarget final {
  u32 width = 0;
  u32 height = 0;
  // RGBA linear em [0,1], uma linha após a outra.
  std::vector<float> pixels;

  void resize(u32 newWidth, u32 newHeight, float red, float green, float blue) {
    width = newWidth;
    height = newHeight;
    pixels.assign(static_cast<usize>(width) * height * 4, 0.0f);
    for (usize index = 0; index < pixels.size(); index += 4) {
      pixels[index] = red;
      pixels[index + 1] = green;
      pixels[index + 2] = blue;
      pixels[index + 3] = 1.0f;
    }
  }

  // Cor de um pixel, para uma asserção pontual num teste.
  void sample(u32 x, u32 y, float outRgba[4]) const {
    const usize index = (static_cast<usize>(y) * width + x) * 4;
    for (u32 channel = 0; channel < 4; ++channel)
      outRgba[channel] = index + channel < pixels.size() ? pixels[index + channel] : 0.0f;
  }
};

namespace detail {

inline void unpackColor(u32 packed, float out[4]) {
  out[0] = static_cast<float>((packed >> 16) & 0xffu) / 255.0f;
  out[1] = static_cast<float>((packed >> 8) & 0xffu) / 255.0f;
  out[2] = static_cast<float>(packed & 0xffu) / 255.0f;
  out[3] = static_cast<float>((packed >> 24) & 0xffu) / 255.0f;
}

// Espelho de roundedBoxDistance.
inline float roundedBoxDistance(float px, float py, const float bounds[4], float radius) {
  const float halfWidth = bounds[2] * 0.5f;
  const float halfHeight = bounds[3] * 0.5f;
  const float clamped = std::min(radius, std::min(halfWidth, halfHeight));
  const float cx = std::fabs(px - (bounds[0] + halfWidth)) - halfWidth + clamped;
  const float cy = std::fabs(py - (bounds[1] + halfHeight)) - halfHeight + clamped;
  const float outsideX = std::max(cx, 0.0f);
  const float outsideY = std::max(cy, 0.0f);
  return std::sqrt(outsideX * outsideX + outsideY * outsideY) +
         std::min(std::max(cx, cy), 0.0f) - clamped;
}

inline float coverageFromDistance(float distance) {
  return std::clamp(0.5f - distance, 0.0f, 1.0f);
}

// Amostragem bilinear, como um sampler VK_FILTER_LINEAR com borda presa.
inline float sampleR8(std::span<const u8> pixels, u32 width, u32 height, float u, float v) {
  const float x = std::clamp(u * static_cast<float>(width) - 0.5f, 0.0f,
                             static_cast<float>(width) - 1.0f);
  const float y = std::clamp(v * static_cast<float>(height) - 0.5f, 0.0f,
                             static_cast<float>(height) - 1.0f);
  const u32 x0 = static_cast<u32>(x);
  const u32 y0 = static_cast<u32>(y);
  const u32 x1 = std::min(x0 + 1, width - 1);
  const u32 y1 = std::min(y0 + 1, height - 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const auto texel = [&](u32 tx, u32 ty) {
    return static_cast<float>(pixels[static_cast<usize>(ty) * width + tx]) / 255.0f;
  };
  const float top = texel(x0, y0) * (1.0f - fx) + texel(x1, y0) * fx;
  const float bottom = texel(x0, y1) * (1.0f - fx) + texel(x1, y1) * fx;
  return top * (1.0f - fy) + bottom * fy;
}

inline void sampleRgba8(std::span<const u8> pixels, u32 width, u32 height, float u, float v,
                        float out[4]) {
  const float x = std::clamp(u * static_cast<float>(width) - 0.5f, 0.0f,
                             static_cast<float>(width) - 1.0f);
  const float y = std::clamp(v * static_cast<float>(height) - 0.5f, 0.0f,
                             static_cast<float>(height) - 1.0f);
  const u32 x0 = static_cast<u32>(x);
  const u32 y0 = static_cast<u32>(y);
  const u32 x1 = std::min(x0 + 1, width - 1);
  const u32 y1 = std::min(y0 + 1, height - 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  for (u32 channel = 0; channel < 4; ++channel) {
    const auto texel = [&](u32 tx, u32 ty) {
      return static_cast<float>(pixels[(static_cast<usize>(ty) * width + tx) * 4 + channel]) /
             255.0f;
    };
    const float top = texel(x0, y0) * (1.0f - fx) + texel(x1, y0) * fx;
    const float bottom = texel(x0, y1) * (1.0f - fx) + texel(x1, y1) * fx;
    out[channel] = top * (1.0f - fy) + bottom * fy;
  }
}

} // namespace detail

// Desenha as instâncias na ordem em que vieram, com mistura por alfa direto —
// os mesmos fatores que o pipeline usa.
inline void rasterizeUi(std::span<const ui::UiInstance> instances, const ui::UiFont &font,
                        const ui::UiIconAtlas &icons, UiSoftwareTarget &target) {
  for (const ui::UiInstance &instance : instances) {
    const i32 left = static_cast<i32>(std::floor(instance.bounds[0]));
    const i32 top = static_cast<i32>(std::floor(instance.bounds[1]));
    const i32 right = static_cast<i32>(std::ceil(instance.bounds[0] + instance.bounds[2]));
    const i32 bottom = static_cast<i32>(std::ceil(instance.bounds[1] + instance.bounds[3]));
    const u32 kind = static_cast<u32>(instance.params[2] + 0.5f);

    float fill[4];
    detail::unpackColor(instance.colors[0], fill);

    for (i32 y = std::max(0, top); y < std::min(static_cast<i32>(target.height), bottom); ++y) {
      for (i32 x = std::max(0, left); x < std::min(static_cast<i32>(target.width), right); ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        if (px < instance.clip[0] || py < instance.clip[1] ||
            px >= instance.clip[0] + instance.clip[2] ||
            py >= instance.clip[1] + instance.clip[3])
          continue;

        float colour[4] = {fill[0], fill[1], fill[2], fill[3]};
        if (kind == static_cast<u32>(ui::UiInstanceKind::Glyph)) {
          if (!font.isReady()) continue;
          const float u = (instance.atlas[0] +
                           (px - instance.bounds[0]) / instance.bounds[2] * instance.atlas[2]) /
                          static_cast<float>(font.atlasWidth());
          const float v = (instance.atlas[1] +
                           (py - instance.bounds[1]) / instance.bounds[3] * instance.atlas[3]) /
                          static_cast<float>(font.atlasHeight());
          const float field =
              detail::sampleR8(font.atlasPixels(), font.atlasWidth(), font.atlasHeight(), u, v);
          const float texelsPerPixel = instance.atlas[3] / std::max(instance.bounds[3], 0.001f);
          const float softness =
              std::max(texelsPerPixel / (2.0f * std::max(instance.params[3], 0.001f)), 0.0015f);
          colour[3] = fill[3] * std::clamp((field - 0.5f) / softness + 0.5f, 0.0f, 1.0f);
        } else if (kind == static_cast<u32>(ui::UiInstanceKind::Icon)) {
          if (!icons.isReady()) continue;
          const float u = (instance.atlas[0] +
                           (px - instance.bounds[0]) / instance.bounds[2] * instance.atlas[2]) /
                          static_cast<float>(icons.width());
          const float v = (instance.atlas[1] +
                           (py - instance.bounds[1]) / instance.bounds[3] * instance.atlas[3]) /
                          static_cast<float>(icons.height());
          float texel[4];
          detail::sampleRgba8(icons.pixels(), icons.width(), icons.height(), u, v, texel);
          for (u32 channel = 0; channel < 4; ++channel) colour[channel] = texel[channel] * fill[channel];
          if (instance.params[0] > 0.0f)
            colour[3] *= detail::coverageFromDistance(
                detail::roundedBoxDistance(px, py, instance.bounds, instance.params[0]));
        } else {
          const float distance =
              detail::roundedBoxDistance(px, py, instance.bounds, instance.params[0]);
          const float outer = detail::coverageFromDistance(distance);
          const float gradient =
              std::clamp((px - instance.bounds[0]) / std::max(instance.bounds[2], 0.001f), 0.0f,
                         1.0f);
          float endColour[4];
          detail::unpackColor(instance.colors[1], endColour);
          for (u32 channel = 0; channel < 4; ++channel)
            colour[channel] = fill[channel] * (1.0f - gradient) + endColour[channel] * gradient;

          const float border = instance.params[1];
          if (border > 0.0f) {
            const float inner[4] = {instance.bounds[0] + border, instance.bounds[1] + border,
                                    instance.bounds[2] - border * 2.0f,
                                    instance.bounds[3] - border * 2.0f};
            const float innerCoverage = detail::coverageFromDistance(detail::roundedBoxDistance(
                px, py, inner, std::max(instance.params[0] - border, 0.0f)));
            float borderColour[4];
            detail::unpackColor(instance.colors[2], borderColour);
            for (u32 channel = 0; channel < 3; ++channel)
              colour[channel] = borderColour[channel] * (1.0f - innerCoverage) +
                                colour[channel] * innerCoverage;
            colour[3] = (borderColour[3] * (outer - innerCoverage)) * (1.0f - innerCoverage) +
                        colour[3] * innerCoverage;
          }
          colour[3] *= outer;
        }

        const usize index = (static_cast<usize>(y) * target.width + static_cast<usize>(x)) * 4;
        const float alpha = std::clamp(colour[3], 0.0f, 1.0f);
        for (u32 channel = 0; channel < 3; ++channel)
          target.pixels[index + channel] =
              target.pixels[index + channel] * (1.0f - alpha) + colour[channel] * alpha;
        target.pixels[index + 3] = target.pixels[index + 3] * (1.0f - alpha) + alpha;
      }
    }
  }
}

} // namespace ae::test
