#pragma once
#include "core/base.h"
#include "ui/ui_geometry.h"
#include <span>
#include <vector>

namespace ae::editor {
// R4: atlas dinâmico da prévia de texturas, composto na CPU e enviado ao
// renderer da interface quando muda.
//
// Layout fixo de 1024×1024 texels:
//  - miniaturas: grade 10×5 de células de 96 px no topo (índice = ordem das
//    texturas do projeto);
//  - visualizador: região de 512×512 embaixo, reescrita a cada troca de nível
//    de mip ou de canal.
// Cada imagem entra reduzida por média de caixa (ampliada por vizinho quando é
// menor que a área), com a proporção preservada. Em RGBA ela é composta sobre
// um xadrez para o alfa ficar legível; um canal isolado sai em cinza opaco.
inline constexpr u32 TexturePreviewAtlasSize = 1024;
inline constexpr u32 TextureThumbnailSize = 96;
inline constexpr u32 TextureThumbnailColumns = 10, TextureThumbnailRows = 5;
inline constexpr u32 TextureThumbnailCapacity = TextureThumbnailColumns * TextureThumbnailRows;
inline constexpr u32 TextureViewerSize = 512;

enum class TexturePreviewChannel : u8 { Rgba, Red, Green, Blue, Alpha };
inline constexpr u8 TexturePreviewChannelCount = 5;
const char *texturePreviewChannelName(TexturePreviewChannel channel) noexcept;
// Fundo sob o alfa no modo RGBA.
enum class TexturePreviewBackground : u8 { Checker, Black, White };
inline constexpr u8 TexturePreviewBackgroundCount = 3;
const char *texturePreviewBackgroundName(TexturePreviewBackground background) noexcept;
// Zoom do visualizador como potência de dois (0 = 1×, 3 = 8×), sempre no centro.
inline constexpr u8 TexturePreviewZoomSteps = 4;

class TexturePreviewAtlas final {
public:
  TexturePreviewAtlas();
  const std::vector<u8> &pixels() const noexcept { return pixels_; }
  bool dirty() const noexcept { return dirty_; }
  void markDirty() noexcept { dirty_ = true; }
  void markClean() noexcept { dirty_ = false; }
  // Célula da miniatura `index`, em texels; vazia fora da capacidade.
  static ui::UiRect thumbnailCell(u32 index) noexcept;
  // Região livre ao lado do visualizador (canto inferior direito): a prévia de
  // um recurso aberto numa janela focada, sem disputar com o Inspector principal.
  static ui::UiRect secondaryViewerRegion() noexcept {
    return {static_cast<float>(TexturePreviewAtlasSize - TextureViewerSize), static_cast<float>(TexturePreviewAtlasSize - TextureViewerSize),
            static_cast<float>(TextureViewerSize), static_cast<float>(TextureViewerSize)};
  }
  static ui::UiRect viewerRegion() noexcept {
    return {0.0f, static_cast<float>(TexturePreviewAtlasSize - TextureViewerSize), static_cast<float>(TextureViewerSize),
            static_cast<float>(TextureViewerSize)};
  }
  // Devolvem o retângulo ocupado pela imagem (texels); vazio quando a imagem é inválida.
  ui::UiRect writeThumbnail(u32 index, std::span<const u8> rgba, u32 width, u32 height);
  // `zoomStep` recorta o centro da imagem em 1/2^zoomStep de cada lado.
  ui::UiRect writeViewer(std::span<const u8> rgba, u32 width, u32 height, TexturePreviewChannel channel, u8 zoomStep = 0,
                         TexturePreviewBackground background = TexturePreviewBackground::Checker);
  ui::UiRect writeSecondaryViewer(std::span<const u8> rgba, u32 width, u32 height) {
    return writeFitted(secondaryViewerRegion(), rgba, width, height, TexturePreviewChannel::Rgba, 0, TexturePreviewBackground::Black);
  }

private:
  ui::UiRect writeFitted(ui::UiRect area, std::span<const u8> rgba, u32 width, u32 height, TexturePreviewChannel channel,
                         u8 zoomStep, TexturePreviewBackground background);
  std::vector<u8> pixels_;
  bool dirty_ = false;
};
} // namespace ae::editor
