// A fonte da interface do lado da engine: métricas, glifos e o posicionamento
// de uma linha de texto.
//
// O arquivo `.aeuf` é produzido por `tools/bake-font-atlas.py` e traz o campo de
// distância junto com a tabela de glifos. O formato está descrito em
// `tools/ui_asset_format.py`, e os dois lados precisam concordar campo a campo —
// por isso a decodificação aqui é explícita em little-endian, nunca um `memcpy`
// para dentro de uma struct. O alinhamento e a ordem de bytes do host que assou
// o atlas não são os do aparelho que o lê.
//
// **As métricas são relativas à linha de base.** É o que permite alinhar um
// rótulo de 11 px e um valor de 14 px na mesma linha do Inspector; alinhar pelo
// topo da caixa deixaria os dois flutuando um em relação ao outro conforme o
// corpo mudasse.
//
// Sem Vulkan, sem Android, sem I/O: recebe bytes já lidos e testável no host.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"
#include "ui/ui_text.h"
#include "ui/ui_theme.h"

#include <span>
#include <string_view>
#include <vector>

namespace ae::ui {

// A ordem é a do binário. Trocá-la sem reassar o atlas desenharia o peso errado.
enum class UiFontWeight : u32 { Regular = 0, Medium = 1, SemiBold = 2 };
inline constexpr u32 kUiFontWeightCount = 3;

struct UiGlyph final {
  // Unidades de em: multiplicadas pelo corpo, viram pixels.
  float advance = 0.0f;
  float bearingX = 0.0f;
  // Negativo acima da linha de base, que é onde quase todo glifo começa.
  float bearingY = 0.0f;
  float sizeX = 0.0f;
  float sizeY = 0.0f;
  // Texels no atlas. Tudo zero significa glifo sem desenho — o espaço, que tem
  // avanço e nenhuma tinta.
  u16 atlasX = 0;
  u16 atlasY = 0;
  u16 atlasWidth = 0;
  u16 atlasHeight = 0;

  bool hasImage() const noexcept { return atlasWidth > 0 && atlasHeight > 0; }
};

// Um glifo já resolvido em coordenadas de tela e de atlas, pronto para virar um
// quad. É o que a lista de desenho entrega ao backend.
struct UiPositionedGlyph final {
  UiRect bounds{};
  UiRect atlas{};
};

class UiFont final {
public:
  // `bytes` continua pertencendo ao chamador e precisa sobreviver ao uso de
  // `atlasPixels()`. A tabela de glifos é copiada (poucos KiB); os pixels não
  // são (meio MiB), porque eles existem para ser enviados à GPU uma vez e
  // duplicá-los seria meio MiB parado pelo resto da execução.
  bool load(std::span<const u8> bytes);
  void unload() noexcept;
  bool isReady() const noexcept { return ready_; }

  u32 atlasWidth() const noexcept { return atlasWidth_; }
  u32 atlasHeight() const noexcept { return atlasHeight_; }
  // Canal único, uma linha após a outra, sem preenchimento entre linhas.
  std::span<const u8> atlasPixels() const noexcept { return atlasPixels_; }
  // Alcance do campo em texels do atlas. O fragment shader precisa dele para
  // converter a derivada de tela em largura de transição.
  float spreadPixels() const noexcept { return spreadPixels_; }
  u32 emPixels() const noexcept { return emPixels_; }

  const UiGlyph *glyph(UiFontWeight weight, u32 codepoint) const noexcept;
  // Tabela de avanços do peso, no formato que ui_text.h consome. Existe para que
  // a medição do layout e a do desenho sejam literalmente a mesma tabela.
  UiFontMetrics metrics(UiFontWeight weight) const noexcept;

  // Posiciona uma linha com a caneta na linha de base em `penBaseline` e anexa
  // os glifos desenháveis em `out`. Devolve o avanço total, que é a largura da
  // linha — inclusive quando nenhum glifo tem desenho.
  float layoutLine(std::string_view text, UiFontWeight weight, const UiTypeStyle &style,
                   UiPoint penBaseline, std::vector<UiPositionedGlyph> &out) const;

private:
  bool ready_ = false;
  u32 atlasWidth_ = 0;
  u32 atlasHeight_ = 0;
  u32 emPixels_ = 0;
  float spreadPixels_ = 0.0f;
  u32 firstGlyph_ = 0;
  u32 glyphCount_ = 0;
  struct Weight final {
    float ascent = 0.0f;
    float descent = 0.0f;
    float capHeight = 0.0f;
    u32 firstGlyphIndex = 0;
  };
  Weight weights_[kUiFontWeightCount]{};
  std::vector<UiGlyph> glyphs_;
  std::span<const u8> atlasPixels_;
};

// Peso do estilo. `medium` do token vira SemiBold quando o texto é um rótulo em
// caixa alta, porque é assim que os masters foram desenhados: os rótulos do
// ASTRA são pequenos e espaçados, e em Medium eles somem contra o painel.
UiFontWeight weightForStyle(const UiTypeStyle &style) noexcept;

} // namespace ae::ui
