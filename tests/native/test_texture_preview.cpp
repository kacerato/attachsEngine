// R4 — atlas de prévia de texturas: composição na CPU, sem Vulkan.
#include "harness.h"
#include "editor/editor_texture_preview.h"

#include <array>
#include <cmath>
#include <span>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
std::vector<u8> solid(u32 width, u32 height, std::array<u8, 4> rgba) {
  std::vector<u8> pixels(static_cast<usize>(width) * height * 4);
  for (usize i = 0; i < pixels.size(); i += 4) std::copy(rgba.begin(), rgba.end(), pixels.begin() + static_cast<std::ptrdiff_t>(i));
  return pixels;
}
std::array<u8, 4> texel(const TexturePreviewAtlas &atlas, float x, float y) {
  const usize at = (static_cast<usize>(y) * TexturePreviewAtlasSize + static_cast<usize>(x)) * 4;
  const auto &p = atlas.pixels();
  return {p[at], p[at + 1], p[at + 2], p[at + 3]};
}
} // namespace

AE_TEST(r4_texture_preview_atlas_fits_thumbnails_and_isolates_channels) {
  TexturePreviewAtlas atlas;
  AE_EXPECT_EQ(atlas.pixels().size(), static_cast<usize>(TexturePreviewAtlasSize) * TexturePreviewAtlasSize * 4, "atlas RGBA8 1024x1024");
  AE_EXPECT_TRUE(!atlas.dirty(), "limpo ao nascer");

  // 200x100 com alfa pela metade: a miniatura cabe na célula e mantém 2:1.
  const auto wide = solid(200, 100, {200, 64, 32, 128});
  const auto cell = TexturePreviewAtlas::thumbnailCell(11);
  const auto rect = atlas.writeThumbnail(11, wide, 200, 100);
  AE_EXPECT_TRUE(atlas.dirty(), "escrever marca o atlas para envio");
  AE_EXPECT_TRUE(rect.x >= cell.x && rect.right() <= cell.right() && rect.y >= cell.y && rect.bottom() <= cell.bottom(),
                 "miniatura dentro da própria célula");
  AE_EXPECT_TRUE(std::fabs(rect.width / rect.height - 2.0f) < 0.05f, "proporção preservada");
  const auto centre = texel(atlas, rect.x + rect.width / 2, rect.y + rect.height / 2);
  AE_EXPECT_EQ(centre[3], u8{255}, "prévia RGBA é opaca (composta sobre xadrez)");
  AE_EXPECT_TRUE(centre[0] > centre[1] && centre[1] >= 64, "cor da textura misturada ao xadrez pelo alfa");
  const auto outside = texel(atlas, cell.x, cell.y);
  AE_EXPECT_EQ(outside[3], u8{0}, "sobra da célula fica transparente");

  // Visualizador: canal verde isolado sai em cinza opaco.
  atlas.markClean();
  const auto view = atlas.writeViewer(wide, 200, 100, TexturePreviewChannel::Green);
  const auto region = TexturePreviewAtlas::viewerRegion();
  AE_EXPECT_TRUE(atlas.dirty() && !view.isEmpty() && view.y >= region.y && view.right() <= region.right(), "na região do visualizador");
  const auto green = texel(atlas, view.x + view.width / 2, view.y + view.height / 2);
  AE_EXPECT_TRUE(green[0] == 64 && green[1] == 64 && green[2] == 64 && green[3] == 255, "canal verde em cinza");
  const auto alpha = atlas.writeViewer(wide, 200, 100, TexturePreviewChannel::Alpha);
  const auto a = texel(atlas, alpha.x + alpha.width / 2, alpha.y + alpha.height / 2);
  AE_EXPECT_EQ(a[0], u8{128}, "canal alfa em cinza");

  // Imagem menor que a área é ampliada, e o fundo antigo não sobra.
  const auto tiny = solid(4, 4, {10, 20, 30, 255});
  const auto grown = atlas.writeViewer(tiny, 4, 4, TexturePreviewChannel::Rgba);
  AE_EXPECT_TRUE(grown.width == region.width && grown.height == region.height, "4x4 ocupa a região inteira");
  AE_EXPECT_EQ(texel(atlas, grown.x + 1, grown.y + 1)[0], u8{10}, "cor ampliada sem mistura");

  // Zoom 2× numa imagem com metade esquerda preta e direita branca: o centro
  // recortado ainda mostra as duas metades, cada uma com metade da região.
  std::vector<u8> halves(8 * 8 * 4, 255);
  for (u32 y = 0; y < 8; ++y)
    for (u32 x = 0; x < 4; ++x) std::fill_n(halves.begin() + static_cast<std::ptrdiff_t>((y * 8 + x) * 4), 3, u8{0});
  const auto zoomed = atlas.writeViewer(halves, 8, 8, TexturePreviewChannel::Rgba, 1);
  AE_EXPECT_TRUE(zoomed.width == region.width, "recorte central ocupa a região");
  AE_EXPECT_EQ(texel(atlas, zoomed.x + zoomed.width * 0.25f, zoomed.y + 10)[0], u8{0}, "metade preta à esquerda");
  AE_EXPECT_EQ(texel(atlas, zoomed.x + zoomed.width * 0.75f, zoomed.y + 10)[0], u8{255}, "metade branca à direita");
  // Fundo preto sob alfa zero.
  const auto clear = solid(4, 4, {255, 255, 255, 0});
  const auto black = atlas.writeViewer(clear, 4, 4, TexturePreviewChannel::Rgba, 0, TexturePreviewBackground::Black);
  AE_EXPECT_EQ(texel(atlas, black.x + 3, black.y + 3)[0], u8{0}, "fundo preto onde o alfa é zero");

  AE_EXPECT_TRUE(TexturePreviewAtlas::thumbnailCell(TextureThumbnailCapacity).isEmpty(), "sem célula fora da capacidade");
  AE_EXPECT_TRUE(atlas.writeThumbnail(0, std::span<const u8>{}, 0, 0).isEmpty(), "imagem vazia não ocupa lugar");
}
