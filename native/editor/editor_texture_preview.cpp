#include "editor/editor_texture_preview.h"

#include <algorithm>
#include <cmath>

namespace ae::editor {
const char *texturePreviewChannelName(TexturePreviewChannel channel) noexcept {
  switch (channel) {
  case TexturePreviewChannel::Rgba: return "RGBA";
  case TexturePreviewChannel::Red: return "Vermelho";
  case TexturePreviewChannel::Green: return "Verde";
  case TexturePreviewChannel::Blue: return "Azul";
  case TexturePreviewChannel::Alpha: return "Alfa";
  }
  return "RGBA";
}

const char *texturePreviewBackgroundName(TexturePreviewBackground background) noexcept {
  switch (background) {
  case TexturePreviewBackground::Checker: return "Xadrez";
  case TexturePreviewBackground::Black: return "Preto";
  case TexturePreviewBackground::White: return "Branco";
  }
  return "Xadrez";
}

TexturePreviewAtlas::TexturePreviewAtlas()
    : pixels_(static_cast<usize>(TexturePreviewAtlasSize) * TexturePreviewAtlasSize * 4, 0) {}

ui::UiRect TexturePreviewAtlas::thumbnailCell(u32 index) noexcept {
  if (index >= TextureThumbnailCapacity) return {};
  const u32 column = index % TextureThumbnailColumns, row = index / TextureThumbnailColumns;
  // Dois texels de margem: a filtragem linear da interface não puxa a vizinha.
  return {static_cast<float>(column * TextureThumbnailSize + 2), static_cast<float>(row * TextureThumbnailSize + 2),
          static_cast<float>(TextureThumbnailSize - 4), static_cast<float>(TextureThumbnailSize - 4)};
}

ui::UiRect TexturePreviewAtlas::writeThumbnail(u32 index, std::span<const u8> rgba, u32 width, u32 height) {
  const auto cell = thumbnailCell(index);
  if (cell.isEmpty()) return {};
  return writeFitted(cell, rgba, width, height, TexturePreviewChannel::Rgba, 0, TexturePreviewBackground::Checker);
}

ui::UiRect TexturePreviewAtlas::writeViewer(std::span<const u8> rgba, u32 width, u32 height, TexturePreviewChannel channel,
                                            u8 zoomStep, TexturePreviewBackground background) {
  return writeFitted(viewerRegion(), rgba, width, height, channel, zoomStep, background);
}

ui::UiRect TexturePreviewAtlas::writeFitted(ui::UiRect area, std::span<const u8> rgba, u32 width, u32 height,
                                            TexturePreviewChannel channel, u8 zoomStep, TexturePreviewBackground background) {
  const u32 areaX = static_cast<u32>(area.x), areaY = static_cast<u32>(area.y);
  const u32 areaW = static_cast<u32>(area.width), areaH = static_cast<u32>(area.height);
  // A área inteira volta a transparente antes: uma imagem mais estreita não pode
  // deixar sobra da anterior.
  for (u32 y = 0; y < areaH; ++y)
    std::fill_n(pixels_.begin() + static_cast<std::ptrdiff_t>((static_cast<usize>(areaY + y) * TexturePreviewAtlasSize + areaX) * 4),
                static_cast<usize>(areaW) * 4, u8{0});
  dirty_ = true;
  if (width == 0 || height == 0 || rgba.size() < static_cast<usize>(width) * height * 4) return {};
  // Recorte central do zoom, em texels da imagem.
  const u32 zoom = u32{1} << std::min<u8>(zoomStep, TexturePreviewZoomSteps - 1);
  const u32 cropW = std::max<u32>(1, width / zoom), cropH = std::max<u32>(1, height / zoom);
  const u32 cropX = (width - cropW) / 2, cropY = (height - cropH) / 2;
  const double scale = std::min(static_cast<double>(areaW) / cropW, static_cast<double>(areaH) / cropH);
  const u32 outW = std::clamp<u32>(static_cast<u32>(std::floor(cropW * scale)), 1, areaW);
  const u32 outH = std::clamp<u32>(static_cast<u32>(std::floor(cropH * scale)), 1, areaH);
  const u32 originX = areaX + (areaW - outW) / 2, originY = areaY + (areaH - outH) / 2;
  for (u32 y = 0; y < outH; ++y) {
    const u32 sy0 = cropY + static_cast<u32>(static_cast<u64>(y) * cropH / outH);
    const u32 sy1 = std::max(sy0 + 1, cropY + static_cast<u32>(static_cast<u64>(y + 1) * cropH / outH));
    for (u32 x = 0; x < outW; ++x) {
      const u32 sx0 = cropX + static_cast<u32>(static_cast<u64>(x) * cropW / outW);
      const u32 sx1 = std::max(sx0 + 1, cropX + static_cast<u32>(static_cast<u64>(x + 1) * cropW / outW));
      u64 sum[4]{};
      u64 samples = 0;
      for (u32 sy = sy0; sy < sy1 && sy < height; ++sy)
        for (u32 sx = sx0; sx < sx1 && sx < width; ++sx) {
          const usize source = (static_cast<usize>(sy) * width + sx) * 4;
          for (u32 c = 0; c < 4; ++c) sum[c] += rgba[source + c];
          ++samples;
        }
      u8 texel[4]{};
      for (u32 c = 0; c < 4; ++c) texel[c] = static_cast<u8>(sum[c] / std::max<u64>(samples, 1));
      u8 out[4]{0, 0, 0, 255};
      if (channel == TexturePreviewChannel::Rgba) {
        // Xadrez de 8 texels (cinza claro e escuro), preto ou branco, onde o alfa deixa ver.
        const u32 checker = background == TexturePreviewBackground::Black   ? 0u
                            : background == TexturePreviewBackground::White ? 255u
                            : ((x / 8) + (y / 8)) % 2                       ? 150u
                                                                            : 200u;
        const u32 alpha = texel[3];
        for (u32 c = 0; c < 3; ++c) out[c] = static_cast<u8>((texel[c] * alpha + checker * (255 - alpha)) / 255);
      } else {
        const u8 value = texel[static_cast<u32>(channel) - 1];
        out[0] = out[1] = out[2] = value;
      }
      const usize target = (static_cast<usize>(originY + y) * TexturePreviewAtlasSize + originX + x) * 4;
      std::copy(out, out + 4, pixels_.begin() + static_cast<std::ptrdiff_t>(target));
    }
  }
  return {static_cast<float>(originX), static_cast<float>(originY), static_cast<float>(outW), static_cast<float>(outH)};
}
} // namespace ae::editor
