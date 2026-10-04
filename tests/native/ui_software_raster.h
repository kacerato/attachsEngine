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
                        const ui::UiIconAtlas &icons, UiSoftwareTarget &target,
                        std::span<const u8> immediateAtlas={},u32 immediateWidth=0,u32 immediateHeight=0,
                        std::span<const u8> guiAtlas={},u32 guiSize=0) {
  for (const ui::UiInstance &instance : instances) {
    const u32 kind = static_cast<u32>(instance.params[2] + 0.5f);
    const bool triangle=kind==static_cast<u32>(ui::UiInstanceKind::Triangle);
    const float minX=triangle?std::min({instance.bounds[0],instance.bounds[2],instance.atlas[0]}):instance.bounds[0];
    const float minY=triangle?std::min({instance.bounds[1],instance.bounds[3],instance.atlas[1]}):instance.bounds[1];
    const float maxX=triangle?std::max({instance.bounds[0],instance.bounds[2],instance.atlas[0]}):instance.bounds[0]+instance.bounds[2];
    const float maxY=triangle?std::max({instance.bounds[1],instance.bounds[3],instance.atlas[1]}):instance.bounds[1]+instance.bounds[3];
    const i32 left=static_cast<i32>(std::floor(minX)),top=static_cast<i32>(std::floor(minY));
    const i32 right=static_cast<i32>(std::ceil(maxX)),bottom=static_cast<i32>(std::ceil(maxY));

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
        if (triangle) {
          if(immediateAtlas.empty() || !immediateWidth || !immediateHeight) continue;
          const float ax=instance.bounds[0],ay=instance.bounds[1],bx=instance.bounds[2],by=instance.bounds[3];
          const float cx=instance.atlas[0],cy=instance.atlas[1];
          const float determinant=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax);
          if(std::abs(determinant)<0.00001f) continue;
          const float v=((px-ax)*(cy-ay)-(py-ay)*(cx-ax))/determinant;
          const float w=((bx-ax)*(py-ay)-(by-ay)*(px-ax))/determinant,u=1-v-w;
          if(u<0 || v<0 || w<0) continue;
          const auto ownsEdge=[&](float x0,float y0,float x1,float y1) {
            if(determinant<0) {std::swap(x0,x1);std::swap(y0,y1);}
            return y1<y0 || (y1==y0 && x1>x0);
          };
          if((std::abs(u)<1e-7f && !ownsEdge(bx,by,cx,cy)) ||
             (std::abs(v)<1e-7f && !ownsEdge(cx,cy,ax,ay)) ||
             (std::abs(w)<1e-7f && !ownsEdge(ax,ay,bx,by))) continue;
          float bcolor[4],ccolor[4],texel[4];
          detail::unpackColor(instance.colors[1],bcolor);detail::unpackColor(instance.colors[2],ccolor);
          detail::sampleRgba8(immediateAtlas,immediateWidth,immediateHeight,
            instance.atlas[2]*u+instance.params[0]*v+instance.extra[0]*w,
            instance.atlas[3]*u+instance.params[1]*v+instance.extra[1]*w,texel);
          for(u32 k=0;k<4;++k) colour[k]=(fill[k]*u+bcolor[k]*v+ccolor[k]*w)*texel[k];
        } else if (kind == static_cast<u32>(ui::UiInstanceKind::Line)) {
          // Mesma distância ponto-segmento do fragmento. O quad orientado do
          // vértice não é reproduzido aqui: percorrer a caixa envolvente dá o
          // mesmo resultado, porque o que decide o pixel é a distância.
          const float ax = instance.atlas[0];
          const float ay = instance.atlas[1];
          const float bx = instance.atlas[2] - ax;
          const float by = instance.atlas[3] - ay;
          const float lengthSquared = std::max(bx * bx + by * by, 0.0001f);
          const float t = std::clamp(((px - ax) * bx + (py - ay) * by) / lengthSquared, 0.0f, 1.0f);
          const float dx = px - (ax + bx * t);
          const float dy = py - (ay + by * t);
          const float distance = std::sqrt(dx * dx + dy * dy) - instance.params[1] * 0.5f;
          colour[3] = fill[3] * detail::coverageFromDistance(distance);
        } else if (kind == static_cast<u32>(ui::UiInstanceKind::Glyph)) {
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
        } else if(kind==static_cast<u32>(ui::UiInstanceKind::GuiImage)) {
          if(guiAtlas.empty() || !guiSize)continue;
          float texel[4];const float u=(instance.atlas[0]+(px-instance.bounds[0])/instance.bounds[2]*instance.atlas[2])/guiSize,v=(instance.atlas[1]+(py-instance.bounds[1])/instance.bounds[3]*instance.atlas[3])/guiSize;
          detail::sampleRgba8(guiAtlas,guiSize,guiSize,u,v,texel);for(u32 c=0;c<4;++c)colour[c]=texel[c]*fill[c];
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
