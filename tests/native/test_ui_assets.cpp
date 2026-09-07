#include "harness.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::ui;

namespace {

// ---------------------------------------------------------------------------
// Construtores de binário sintético.
//
// Eles existem para exercitar o decodificador em casos que o arquivo real nunca
// vai ter — cabeçalho truncado, retângulo saindo do atlas, contagem de pesos
// menor que a esperada. O arquivo real prova que a ferramenta e o leitor
// concordam; estes provam que o leitor recusa o que não deveria aceitar.
// ---------------------------------------------------------------------------

void pushU32(std::vector<u8> &bytes, u32 value) {
  bytes.push_back(static_cast<u8>(value & 0xffu));
  bytes.push_back(static_cast<u8>((value >> 8) & 0xffu));
  bytes.push_back(static_cast<u8>((value >> 16) & 0xffu));
  bytes.push_back(static_cast<u8>((value >> 24) & 0xffu));
}

void pushU16(std::vector<u8> &bytes, u16 value) {
  bytes.push_back(static_cast<u8>(value & 0xffu));
  bytes.push_back(static_cast<u8>((value >> 8) & 0xffu));
}

void pushF32(std::vector<u8> &bytes, float value) {
  u32 raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  pushU32(bytes, raw);
}

struct SyntheticGlyph final {
  float advance = 0.5f;
  float bearingX = 0.02f;
  float bearingY = -0.7f;
  float sizeX = 0.5f;
  float sizeY = 0.7f;
  u16 atlasX = 0;
  u16 atlasY = 0;
  u16 atlasWidth = 8;
  u16 atlasHeight = 8;
};

// Fonte mínima com dois glifos ('A' e 'B') e três pesos.
std::vector<u8> buildFont(u32 atlasWidth = 16, u32 atlasHeight = 16, u32 weightCount = 3,
                          u32 firstGlyph = 'A', u32 glyphCount = 2,
                          SyntheticGlyph overrideFirst = {}) {
  std::vector<u8> bytes;
  pushU32(bytes, 0x46554541);
  pushU32(bytes, 1);
  pushU32(bytes, atlasWidth);
  pushU32(bytes, atlasHeight);
  pushU32(bytes, 40);
  pushF32(bytes, 5.0f);
  pushU32(bytes, firstGlyph);
  pushU32(bytes, glyphCount);
  pushU32(bytes, weightCount);
  for (u32 weight = 0; weight < weightCount; ++weight) {
    pushF32(bytes, 0.97f);
    pushF32(bytes, 0.24f);
    pushF32(bytes, 0.73f);
    for (u32 index = 0; index < glyphCount; ++index) {
      SyntheticGlyph glyph = index == 0 ? overrideFirst : SyntheticGlyph{};
      // Um avanço por peso, para provar que a tabela certa é lida.
      glyph.advance += static_cast<float>(weight) * 0.1f;
      pushF32(bytes, glyph.advance);
      pushF32(bytes, glyph.bearingX);
      pushF32(bytes, glyph.bearingY);
      pushF32(bytes, glyph.sizeX);
      pushF32(bytes, glyph.sizeY);
      pushU16(bytes, glyph.atlasX);
      pushU16(bytes, glyph.atlasY);
      pushU16(bytes, glyph.atlasWidth);
      pushU16(bytes, glyph.atlasHeight);
    }
  }
  bytes.resize(bytes.size() + static_cast<usize>(atlasWidth) * atlasHeight, 0x40);
  return bytes;
}

std::vector<u8> buildIcons(u32 count, u32 atlasWidth = 256, u32 atlasHeight = 128,
                           u16 firstWidth = 96) {
  std::vector<u8> bytes;
  pushU32(bytes, 0x49554541);
  pushU32(bytes, 1);
  pushU32(bytes, atlasWidth);
  pushU32(bytes, atlasHeight);
  pushU32(bytes, count);
  for (u32 index = 0; index < count; ++index) {
    pushU16(bytes, static_cast<u16>((index % 2) * 96));
    pushU16(bytes, 0);
    pushU16(bytes, index == 0 ? firstWidth : 96);
    pushU16(bytes, 96);
  }
  bytes.resize(bytes.size() + static_cast<usize>(atlasWidth) * atlasHeight * 4, 0x20);
  return bytes;
}

bool readRepositoryFile(const char *relative, std::vector<u8> &out) {
  const std::string path = std::string(AETHER_REPOSITORY_ROOT) + "/" + relative;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  if (size <= 0) {
    std::fclose(file);
    return false;
  }
  out.resize(static_cast<usize>(size));
  const usize read = std::fread(out.data(), 1, out.size(), file);
  std::fclose(file);
  return read == out.size();
}

} // namespace

AE_TEST(font_decodes_a_well_formed_binary) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  AE_EXPECT_TRUE(font.load(bytes), "");
  AE_EXPECT_EQ(font.atlasWidth(), 16u, "");
  AE_EXPECT_EQ(font.atlasHeight(), 16u, "");
  AE_EXPECT_EQ(font.emPixels(), 40u, "");
  AE_EXPECT_TRUE(font.spreadPixels() == 5.0f, "");
  AE_EXPECT_EQ(font.atlasPixels().size(), usize{256}, "os pixels sao o resto do arquivo");
}

AE_TEST(font_reads_the_table_of_the_requested_weight) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  AE_EXPECT_TRUE(font.load(bytes), "");
  const UiGlyph *regular = font.glyph(UiFontWeight::Regular, 'A');
  const UiGlyph *semibold = font.glyph(UiFontWeight::SemiBold, 'A');
  AE_EXPECT_TRUE(regular != nullptr && semibold != nullptr, "");
  AE_EXPECT_TRUE(std::fabs(regular->advance - 0.5f) < 1e-6f, "");
  AE_EXPECT_TRUE(std::fabs(semibold->advance - 0.7f) < 1e-6f,
                 "cada peso tem sua propria tabela, e nao um deslocamento errado dela");
}

AE_TEST(font_returns_nothing_outside_the_glyph_range) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  font.load(bytes);
  AE_EXPECT_TRUE(font.glyph(UiFontWeight::Regular, 'Z') == nullptr, "");
  AE_EXPECT_TRUE(font.glyph(UiFontWeight::Regular, ' ') == nullptr, "");
  AE_EXPECT_TRUE(font.glyph(static_cast<UiFontWeight>(9), 'A') == nullptr, "");
}

AE_TEST(font_rejects_a_wrong_magic_version_or_truncation) {
  std::vector<u8> bytes = buildFont();
  UiFont font;

  std::vector<u8> wrongMagic = bytes;
  wrongMagic[0] = 0;
  AE_EXPECT_TRUE(!font.load(wrongMagic), "");

  std::vector<u8> wrongVersion = bytes;
  wrongVersion[4] = 99;
  AE_EXPECT_TRUE(!font.load(wrongVersion), "");

  std::vector<u8> truncated = bytes;
  truncated.resize(truncated.size() - 1);
  AE_EXPECT_TRUE(!font.load(truncated),
                 "o tamanho e conferido contra a tabela: um byte a menos e recusa");

  std::vector<u8> longer = bytes;
  longer.push_back(0);
  AE_EXPECT_TRUE(!font.load(longer), "e um byte a mais tambem");
  AE_EXPECT_TRUE(!font.isReady(), "recusa deixa a fonte descarregada, nao meio carregada");
}

AE_TEST(font_rejects_a_glyph_that_leaves_the_atlas) {
  // Amostrar fora do retangulo pega o glifo do vizinho, o que e pior do que nao
  // desenhar: da para nao notar por muito tempo.
  SyntheticGlyph escaping{};
  escaping.atlasX = 12;
  escaping.atlasWidth = 8;  // 12 + 8 > 16
  const std::vector<u8> bytes = buildFont(16, 16, 3, 'A', 2, escaping);
  UiFont font;
  AE_EXPECT_TRUE(!font.load(bytes), "");
}

AE_TEST(font_rejects_fewer_weights_than_the_runtime_uses) {
  const std::vector<u8> bytes = buildFont(16, 16, 2);
  UiFont font;
  AE_EXPECT_TRUE(!font.load(bytes), "faltando um peso, algum estilo desenharia o errado");
}

AE_TEST(font_metrics_match_the_table_used_for_drawing) {
  // O layout mede com UiFontMetrics e o desenho posiciona com UiGlyph. Se as
  // duas fontes divergissem, a caixa reservaria um espaco e o traco sairia em
  // outro -- e nada na tela diria qual dos dois esta certo.
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  font.load(bytes);
  const UiFontMetrics metrics = font.metrics(UiFontWeight::Medium);
  AE_EXPECT_TRUE(std::fabs(metrics.advanceOf('A') - 0.6f) < 1e-6f, "");
  AE_EXPECT_TRUE(std::fabs(metrics.capHeight - 0.73f) < 1e-6f, "");

  UiTypeStyle style{};
  style.size = 20.0f;
  std::vector<UiPositionedGlyph> glyphs;
  const float laidOut = font.layoutLine("AB", UiFontWeight::Medium, style, {0.0f, 0.0f}, glyphs);
  const float measured = measureTextWidth("AB", metrics, style);
  AE_EXPECT_TRUE(std::fabs(laidOut - measured) < 1e-3f,
                 "medir e posicionar tem de dar a mesma largura");
}

AE_TEST(font_layout_places_glyphs_above_the_baseline) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  font.load(bytes);
  UiTypeStyle style{};
  style.size = 100.0f;
  std::vector<UiPositionedGlyph> glyphs;
  font.layoutLine("A", UiFontWeight::Regular, style, {50.0f, 200.0f}, glyphs);
  AE_EXPECT_EQ(glyphs.size(), usize{1}, "");
  AE_EXPECT_TRUE(std::fabs(glyphs[0].bounds.x - 52.0f) < 0.01f, "bearing horizontal aplicado");
  AE_EXPECT_TRUE(std::fabs(glyphs[0].bounds.y - 130.0f) < 0.01f,
                 "bearing vertical negativo sobe a partir da linha de base");
  AE_EXPECT_TRUE(std::fabs(glyphs[0].bounds.height - 70.0f) < 0.01f, "");
}

AE_TEST(font_layout_advances_unknown_codepoints_without_drawing_them) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  font.load(bytes);
  UiTypeStyle style{};
  style.size = 10.0f;
  std::vector<UiPositionedGlyph> glyphs;
  const float width = font.layoutLine("AZB", UiFontWeight::Regular, style, {0.0f, 0.0f}, glyphs);
  AE_EXPECT_EQ(glyphs.size(), usize{2}, "o codepoint desconhecido nao vira quad");
  AE_EXPECT_TRUE(width > 10.0f, "mas ocupa espaco, senao a caixa medida encolheria");
}

AE_TEST(font_layout_applies_tracking_and_uppercase) {
  const std::vector<u8> bytes = buildFont();
  UiFont font;
  font.load(bytes);
  UiTypeStyle plain{};
  plain.size = 10.0f;
  UiTypeStyle tracked = plain;
  tracked.tracking = 0.3f;
  std::vector<UiPositionedGlyph> glyphs;
  const float narrow = font.layoutLine("AB", UiFontWeight::Regular, plain, {0, 0}, glyphs);
  glyphs.clear();
  const float wide = font.layoutLine("AB", UiFontWeight::Regular, tracked, {0, 0}, glyphs);
  AE_EXPECT_TRUE(wide > narrow, "");

  UiTypeStyle upper = plain;
  upper.uppercase = true;
  glyphs.clear();
  font.layoutLine("ab", UiFontWeight::Regular, upper, {0, 0}, glyphs);
  AE_EXPECT_EQ(glyphs.size(), usize{2},
               "minusculas viram maiusculas e encontram os glifos da tabela");
}

AE_TEST(font_weight_for_style_follows_the_masters) {
  UiTypeStyle body{};
  AE_EXPECT_TRUE(weightForStyle(body) == UiFontWeight::Regular, "");
  UiTypeStyle name{};
  name.medium = true;
  AE_EXPECT_TRUE(weightForStyle(name) == UiFontWeight::Medium, "");
  UiTypeStyle label{};
  label.medium = true;
  label.uppercase = true;
  AE_EXPECT_TRUE(weightForStyle(label) == UiFontWeight::SemiBold,
                 "rotulo pequeno em caixa alta some em Medium contra o painel");
}

AE_TEST(icons_decode_and_index_by_enum) {
  const std::vector<u8> bytes = buildIcons(kUiIconCount);
  UiIconAtlas atlas;
  AE_EXPECT_TRUE(atlas.load(bytes), "");
  AE_EXPECT_EQ(atlas.width(), 256u, "");
  const UiRect first = atlas.rectOf(static_cast<UiIcon>(1));
  AE_EXPECT_TRUE(first.width == 96.0f && first.height == 96.0f, "");
}

AE_TEST(icons_none_and_out_of_range_return_an_empty_rect) {
  const std::vector<u8> bytes = buildIcons(kUiIconCount);
  UiIconAtlas atlas;
  atlas.load(bytes);
  AE_EXPECT_TRUE(atlas.rectOf(UiIcon::None).isEmpty(),
                 "desenhar o slot zero por engano e o erro que o enum impede");
  AE_EXPECT_TRUE(atlas.rectOf(static_cast<UiIcon>(kUiIconCount + 1)).isEmpty(), "");
}

AE_TEST(icons_reject_a_binary_from_another_generation) {
  // O binario e o enum saem da mesma execucao da ferramenta. Uma contagem
  // diferente desenharia todos os icones deslocados por um -- e quase todo
  // icone continuaria sendo *um* icone, o que faz o defeito passar batido.
  UiIconAtlas atlas;
  AE_EXPECT_TRUE(!atlas.load(buildIcons(kUiIconCount - 1)), "");
  AE_EXPECT_TRUE(!atlas.load(buildIcons(kUiIconCount + 1)), "");
}

AE_TEST(icons_reject_a_rect_that_leaves_the_atlas) {
  UiIconAtlas atlas;
  AE_EXPECT_TRUE(!atlas.load(buildIcons(kUiIconCount, 256, 128, 300)), "");
  AE_EXPECT_TRUE(!atlas.isReady(), "");
}

AE_TEST(icons_reject_a_truncated_payload) {
  std::vector<u8> bytes = buildIcons(kUiIconCount);
  bytes.resize(bytes.size() - 4);
  UiIconAtlas atlas;
  AE_EXPECT_TRUE(!atlas.load(bytes), "");
}

// ---------------------------------------------------------------------------
// O par que prova que a ferramenta e o leitor concordam. Um teste sintético não
// pega uma divergência de formato — ele testa o leitor contra a ideia que o
// leitor tem do formato. Só abrir o arquivo que a ferramenta escreveu pega.
// ---------------------------------------------------------------------------

AE_TEST(shipped_font_asset_loads_and_covers_printable_ascii) {
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(readRepositoryFile("assets/astra-visual/ui/astra-ui-font.aeuf", bytes),
                 "o asset assado precisa estar no repositorio");
  UiFont font;
  AE_EXPECT_TRUE(font.load(bytes), "o binario da ferramenta tem de passar no leitor");
  AE_EXPECT_EQ(font.atlasPixels().size(),
               static_cast<usize>(font.atlasWidth()) * font.atlasHeight(), "");

  for (u32 code = kUiFirstGlyph; code <= kUiLastGlyph; ++code) {
    AE_EXPECT_TRUE(font.glyph(UiFontWeight::Regular, code) != nullptr,
                   "todo ASCII imprimivel tem entrada");
  }
  const UiGlyph *space = font.glyph(UiFontWeight::Regular, ' ');
  AE_EXPECT_TRUE(space != nullptr && !space->hasImage() && space->advance > 0.0f,
                 "o espaco avanca e nao desenha");
  const UiGlyph *capital = font.glyph(UiFontWeight::Regular, 'H');
  AE_EXPECT_TRUE(capital != nullptr && capital->hasImage(), "");
}

namespace {

// Área de tinta do glifo no atlas: texels dentro do traço, ou seja, com o campo
// acima da borda. É a medida direta de "quão gordo é este peso".
u32 inkTexels(const UiFont &font, UiFontWeight weight, u32 codepoint) {
  const UiGlyph *glyph = font.glyph(weight, codepoint);
  if (glyph == nullptr || !glyph->hasImage()) return 0;
  const std::span<const u8> pixels = font.atlasPixels();
  u32 count = 0;
  for (u32 row = 0; row < glyph->atlasHeight; ++row) {
    const usize offset = static_cast<usize>(glyph->atlasY + row) * font.atlasWidth() +
                         glyph->atlasX;
    for (u32 column = 0; column < glyph->atlasWidth; ++column)
      if (pixels[offset + column] > 128) ++count;
  }
  return count;
}

float wordAdvance(const UiFont &font, UiFontWeight weight, std::string_view word) {
  float total = 0.0f;
  for (const char byte : word) {
    const UiGlyph *glyph = font.glyph(weight, static_cast<u32>(static_cast<unsigned char>(byte)));
    if (glyph != nullptr) total += glyph->advance;
  }
  return total;
}

} // namespace

AE_TEST(shipped_font_weights_are_three_different_weights) {
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(readRepositoryFile("assets/astra-visual/ui/astra-ui-font.aeuf", bytes), "");
  UiFont font;
  AE_EXPECT_TRUE(font.load(bytes), "");

  // O avanco NAO serve como prova sozinho: o Inter mantem varios glifos
  // metricamente identicos entre pesos -- o "H" tem exatamente o mesmo avanco e
  // a mesma caixa nos tres. O que muda e a espessura do traco. Medir a tinta e
  // o unico jeito de provar que os tres pesos foram mesmo assados de eixos
  // diferentes, em vez de tres copias do mesmo.
  const u32 regularInk = inkTexels(font, UiFontWeight::Regular, 'H');
  const u32 mediumInk = inkTexels(font, UiFontWeight::Medium, 'H');
  const u32 semiboldInk = inkTexels(font, UiFontWeight::SemiBold, 'H');
  AE_EXPECT_TRUE(regularInk > 0, "");
  AE_EXPECT_TRUE(mediumInk > regularInk, "Medium tem mais tinta que Regular");
  AE_EXPECT_TRUE(semiboldInk > mediumInk, "SemiBold tem mais tinta que Medium");

  // E numa palavra real o avanco tambem cresce, que e o que faz o Inspector
  // reservar mais largura para um rotulo em SemiBold.
  const float regularWidth = wordAdvance(font, UiFontWeight::Regular, "Position");
  const float semiboldWidth = wordAdvance(font, UiFontWeight::SemiBold, "Position");
  AE_EXPECT_TRUE(semiboldWidth > regularWidth, "");
}

AE_TEST(shipped_font_cap_height_matches_the_measured_glyph) {
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(readRepositoryFile("assets/astra-visual/ui/astra-ui-font.aeuf", bytes), "");
  UiFont font;
  font.load(bytes);
  const UiFontMetrics metrics = font.metrics(UiFontWeight::Regular);
  const UiGlyph *capital = font.glyph(UiFontWeight::Regular, 'H');
  AE_EXPECT_TRUE(capital != nullptr, "");
  // O topo do "H" fica uma altura de caixa alta acima da linha de base -- MAIS a
  // margem do campo de distancia, que faz parte do ladrilho e nao do desenho.
  // Ignorar essa margem alinharia todo rotulo cinco pixels acima do mockup.
  const float margin = font.spreadPixels() / static_cast<float>(font.emPixels());
  AE_EXPECT_TRUE(std::fabs(-capital->bearingY - margin - metrics.capHeight) < 0.02f,
                 "a caixa alta medida bate com o desenho, descontada a margem do campo");
}

AE_TEST(shipped_icon_asset_loads_with_the_generated_enum) {
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(readRepositoryFile("assets/astra-visual/ui/astra-ui-icons.aeui", bytes),
                 "o atlas empacotado precisa estar no repositorio");
  UiIconAtlas atlas;
  AE_EXPECT_TRUE(atlas.load(bytes),
                 "contagem diferente aqui significa enum e binario de geracoes diferentes");
  AE_EXPECT_EQ(atlas.pixels().size(),
               static_cast<usize>(atlas.width()) * atlas.height() * 4, "");
  for (u32 index = 1; index <= kUiIconCount; ++index) {
    const UiRect rect = atlas.rectOf(static_cast<UiIcon>(index));
    AE_EXPECT_TRUE(!rect.isEmpty(), "todo icone do enum tem retangulo");
    AE_EXPECT_TRUE(rect.right() <= static_cast<float>(atlas.width()) &&
                       rect.bottom() <= static_cast<float>(atlas.height()),
                   "e cabe dentro do atlas");
  }
}

AE_TEST(shipped_icon_names_line_up_with_the_enum) {
  AE_EXPECT_TRUE(std::string(uiIconName(UiIcon::None)) == "none", "");
  AE_EXPECT_TRUE(std::string(uiIconName(UiIcon::EditorMove)) == "editor/move",
                 "o nome de catalogo acompanha o indice gerado");
  AE_EXPECT_TRUE(std::string(uiIconName(UiIcon::RuntimePlay)) == "runtime/play", "");
  AE_EXPECT_TRUE(std::string(uiIconName(static_cast<UiIcon>(kUiIconCount + 5))) == "unknown", "");
}
