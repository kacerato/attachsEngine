#include "ui/ui_font.h"

#include <cmath>
#include <cstring>

namespace ae::ui {
namespace {

// 'AEUF' em little-endian. Mesma disciplina de renderer/texture_payload.h: a
// leitura é byte a byte, e nunca um despejo de struct do compilador.
constexpr u32 kFontMagic = 0x46554541;
constexpr u32 kFontVersion = 1;
constexpr u32 kHeaderBytes = 36;
constexpr u32 kGlyphBytes = 28;      // 5 floats + 4 u16
constexpr u32 kWeightHeaderBytes = 12;  // ascent, descent, capHeight
// Tetos de sanidade. Um arquivo corrompido tem de ser recusado aqui, não virar
// uma alocação de gigabytes ou um índice fora dos limites três funções adiante.
constexpr u32 kMaximumAtlasSide = 8192;
constexpr u32 kMaximumGlyphs = 512;

u32 readU32(std::span<const u8> bytes, usize offset) noexcept {
  return static_cast<u32>(bytes[offset]) | static_cast<u32>(bytes[offset + 1]) << 8 |
         static_cast<u32>(bytes[offset + 2]) << 16 | static_cast<u32>(bytes[offset + 3]) << 24;
}

u16 readU16(std::span<const u8> bytes, usize offset) noexcept {
  return static_cast<u16>(static_cast<u16>(bytes[offset]) |
                          static_cast<u16>(bytes[offset + 1]) << 8);
}

float readF32(std::span<const u8> bytes, usize offset) noexcept {
  const u32 raw = readU32(bytes, offset);
  float value = 0.0f;
  // memcpy e não reinterpret_cast: é a única forma definida de reinterpretar os
  // bits, e o compilador a resolve para zero instruções.
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

bool isFiniteGlyph(const UiGlyph &glyph) noexcept {
  return std::isfinite(glyph.advance) && std::isfinite(glyph.bearingX) &&
         std::isfinite(glyph.bearingY) && std::isfinite(glyph.sizeX) &&
         std::isfinite(glyph.sizeY) && glyph.advance >= 0.0f && glyph.sizeX >= 0.0f &&
         glyph.sizeY >= 0.0f;
}

u32 toUpperAscii(u32 codepoint) noexcept {
  return uiUppercase(codepoint);
}

} // namespace

void UiFont::unload() noexcept {
  ready_ = false;
  atlasWidth_ = 0;
  atlasHeight_ = 0;
  emPixels_ = 0;
  spreadPixels_ = 0.0f;
  firstGlyph_ = 0;
  glyphCount_ = 0;
  for (Weight &weight : weights_) weight = Weight{};
  glyphs_.clear();
  atlasPixels_ = {};
}

bool UiFont::load(std::span<const u8> bytes) {
  unload();
  if (bytes.size() < kHeaderBytes) return false;
  if (readU32(bytes, 0) != kFontMagic || readU32(bytes, 4) != kFontVersion) return false;

  const u32 width = readU32(bytes, 8);
  const u32 height = readU32(bytes, 12);
  const u32 emPixels = readU32(bytes, 16);
  const float spread = readF32(bytes, 20);
  const u32 firstGlyph = readU32(bytes, 24);
  const u32 glyphCount = readU32(bytes, 28);
  const u32 weightCount = readU32(bytes, 32);

  if (width == 0 || height == 0 || width > kMaximumAtlasSide || height > kMaximumAtlasSide)
    return false;
  if (emPixels == 0 || !std::isfinite(spread) || spread <= 0.0f) return false;
  if (glyphCount == 0 || glyphCount > kMaximumGlyphs) return false;
  // O binário pode trazer mais pesos do que este runtime conhece, e isso é uma
  // versão futura, não um arquivo quebrado. Menos pesos, sim, é quebrado.
  if (weightCount < kUiFontWeightCount) return false;
  if (firstGlyph > kUiLastGlyph) return false;

  const u64 tableBytes = static_cast<u64>(weightCount) *
                         (kWeightHeaderBytes + static_cast<u64>(glyphCount) * kGlyphBytes);
  const u64 pixelBytes = static_cast<u64>(width) * height;
  if (bytes.size() != kHeaderBytes + tableBytes + pixelBytes) return false;

  glyphs_.resize(static_cast<usize>(kUiFontWeightCount) * glyphCount);
  usize cursor = kHeaderBytes;
  for (u32 weightIndex = 0; weightIndex < weightCount; ++weightIndex) {
    const float ascent = readF32(bytes, cursor);
    const float descent = readF32(bytes, cursor + 4);
    const float capHeight = readF32(bytes, cursor + 8);
    cursor += kWeightHeaderBytes;

    const bool wanted = weightIndex < kUiFontWeightCount;
    if (wanted) {
      if (!std::isfinite(ascent) || !std::isfinite(descent) || !std::isfinite(capHeight) ||
          ascent <= 0.0f || descent < 0.0f || capHeight <= 0.0f) {
        unload();
        return false;
      }
      weights_[weightIndex] = {ascent, descent, capHeight, weightIndex * glyphCount};
    }

    for (u32 index = 0; index < glyphCount; ++index) {
      if (!wanted) {
        cursor += kGlyphBytes;
        continue;
      }
      UiGlyph glyph{};
      glyph.advance = readF32(bytes, cursor);
      glyph.bearingX = readF32(bytes, cursor + 4);
      glyph.bearingY = readF32(bytes, cursor + 8);
      glyph.sizeX = readF32(bytes, cursor + 12);
      glyph.sizeY = readF32(bytes, cursor + 16);
      glyph.atlasX = readU16(bytes, cursor + 20);
      glyph.atlasY = readU16(bytes, cursor + 22);
      glyph.atlasWidth = readU16(bytes, cursor + 24);
      glyph.atlasHeight = readU16(bytes, cursor + 26);
      cursor += kGlyphBytes;
      // Um retângulo que sai do atlas amostraria o glifo do vizinho, o que é
      // pior do que não desenhar nada: dá para não notar por muito tempo.
      const bool insideAtlas = static_cast<u32>(glyph.atlasX) + glyph.atlasWidth <= width &&
                               static_cast<u32>(glyph.atlasY) + glyph.atlasHeight <= height;
      if (!isFiniteGlyph(glyph) || !insideAtlas) {
        unload();
        return false;
      }
      glyphs_[weights_[weightIndex].firstGlyphIndex + index] = glyph;
    }
  }

  atlasWidth_ = width;
  atlasHeight_ = height;
  emPixels_ = emPixels;
  spreadPixels_ = spread;
  firstGlyph_ = firstGlyph;
  glyphCount_ = glyphCount;
  atlasPixels_ = bytes.subspan(static_cast<usize>(kHeaderBytes + tableBytes),
                               static_cast<usize>(pixelBytes));
  ready_ = true;
  return true;
}

const UiGlyph *UiFont::glyph(UiFontWeight weight, u32 codepoint) const noexcept {
  if (!ready_) return nullptr;
  const u32 weightIndex = static_cast<u32>(weight);
  if (weightIndex >= kUiFontWeightCount) return nullptr;
  if (codepoint < firstGlyph_ || codepoint >= firstGlyph_ + glyphCount_) return nullptr;
  return &glyphs_[weights_[weightIndex].firstGlyphIndex + (codepoint - firstGlyph_)];
}

UiFontMetrics UiFont::metrics(UiFontWeight weight) const noexcept {
  UiFontMetrics result{};
  if (!ready_) return fallbackFontMetrics();
  const u32 weightIndex = static_cast<u32>(weight);
  if (weightIndex >= kUiFontWeightCount) return fallbackFontMetrics();
  const Weight &data = weights_[weightIndex];
  result.ascent = data.ascent;
  result.descent = data.descent;
  result.capHeight = data.capHeight;
  // O avanço de reserva é o do espaço: um codepoint fora da tabela ocupa uma
  // lacuna do tamanho de um espaço, que é o erro menos surpreendente possível.
  const UiGlyph *space = glyph(weight, ' ');
  result.fallbackAdvance = space != nullptr ? space->advance : 0.5f;
  for (u32 index = 0; index < kUiGlyphCount; ++index) {
    const UiGlyph *entry = glyph(weight, kUiFirstGlyph + index);
    result.advance[index] = entry != nullptr ? entry->advance : result.fallbackAdvance;
  }
  return result;
}

float UiFont::layoutLine(std::string_view text, UiFontWeight weight, const UiTypeStyle &style,
                         UiPoint penBaseline, std::vector<UiPositionedGlyph> &out) const {
  if (!ready_) return 0.0f;
  if (!std::isfinite(style.size) || style.size <= 0.0f) return 0.0f;
  if (!std::isfinite(penBaseline.x) || !std::isfinite(penBaseline.y)) return 0.0f;

  const float tracking = style.tracking * style.size;
  float pen = penBaseline.x;
  u32 pending[3];
  u32 pendingCount = 0, pendingAt = 0;
  for (usize cursor=0;cursor<text.size() || pendingAt<pendingCount;) {
    u32 codepoint;
    if (pendingAt < pendingCount) codepoint = pending[pendingAt++];
    else {
      codepoint = nextUiCodepoint(text,cursor);
      if (style.uppercase) codepoint = toUpperAscii(codepoint);
      // Pontuação fora do atlas: desenha o equivalente (ui_text.h), igual à medida.
      if (codepoint > kUiLastGlyph && (pendingCount = uiLatinFallback(codepoint, pending)) != 0) {
        codepoint = pending[0];
        pendingAt = 1;
      } else pendingCount = pendingAt = 0;
    }
    if (pendingAt >= pendingCount) pendingCount = pendingAt = 0;
    const UiGlyph *entry = glyph(weight, codepoint);
    if (entry == nullptr) {
      // Fora da tabela: avança como um espaço e não desenha. É o mesmo que
      // measureTextWidth faz, então a caixa medida continua batendo com o traço.
      const UiGlyph *space = glyph(weight, ' ');
      pen += (space != nullptr ? space->advance : 0.5f) * style.size + tracking;
      continue;
    }
    if (entry->hasImage()) {
      UiPositionedGlyph positioned{};
      positioned.bounds = {pen + entry->bearingX * style.size,
                           penBaseline.y + entry->bearingY * style.size,
                           entry->sizeX * style.size, entry->sizeY * style.size};
      positioned.atlas = {static_cast<float>(entry->atlasX), static_cast<float>(entry->atlasY),
                          static_cast<float>(entry->atlasWidth),
                          static_cast<float>(entry->atlasHeight)};
      out.push_back(positioned);
    }
    pen += entry->advance * style.size + tracking;
  }
  return pen - penBaseline.x;
}

UiFontWeight weightForStyle(const UiTypeStyle &style) noexcept {
  if (!style.medium) return UiFontWeight::Regular;
  return style.uppercase ? UiFontWeight::SemiBold : UiFontWeight::Medium;
}

} // namespace ae::ui
