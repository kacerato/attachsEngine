#include "ui/ui_text.h"

#include <cmath>

namespace ae::ui {
namespace {

// Maiúscula ASCII. A UI usa `uppercase` só em rótulos técnicos, que são ASCII
// por construção; um byte fora dessa faixa passa intacto em vez de virar lixo.
u32 toUpperAscii(u32 codepoint) noexcept {
  return uiUppercase(codepoint);
}

} // namespace

const UiFontMetrics &fallbackFontMetrics() noexcept {
  static const UiFontMetrics metrics = [] {
    UiFontMetrics result{};
    for (u32 index = 0; index < kUiGlyphCount; ++index) result.advance[index] = 0.5f;
    return result;
  }();
  return metrics;
}

float measureTextWidth(std::string_view text, const UiFontMetrics &metrics,
                       const UiTypeStyle &style) noexcept {
  if (!std::isfinite(style.size) || style.size <= 0.0f) return 0.0f;
  float width = 0.0f;
  usize count=0;
  for (usize cursor=0;cursor<text.size();++count) {
    const u32 codepoint = nextUiCodepoint(text,cursor);
    u32 substitutes[3];
    if (const u32 n = codepoint > kUiLastGlyph ? uiLatinFallback(codepoint, substitutes) : 0u) {
      for (u32 i = 0; i < n; ++i) width += metrics.advanceOf(substitutes[i]);
      count += n - 1;
      continue;
    }
    width += metrics.advanceOf(style.uppercase ? toUpperAscii(codepoint) : codepoint);
  }
  const float trackingPixels = style.tracking * style.size * static_cast<float>(count);
  return width * style.size + trackingPixels;
}

float lineHeight(const UiFontMetrics &metrics, const UiTypeStyle &style) noexcept {
  if (!std::isfinite(style.size) || style.size <= 0.0f) return 0.0f;
  // O fator do estilo multiplica a altura da fonte, não o corpo nominal: duas
  // fontes de corpo 15 com ascendentes diferentes não devem produzir a mesma
  // caixa de linha.
  const float fontHeight = (metrics.ascent + metrics.descent) * style.size;
  return fontHeight * (style.lineHeight > 0.0f ? style.lineHeight : 1.0f);
}

float baselineForCapTop(const UiFontMetrics &metrics, const UiTypeStyle &style,
                        float capTop) noexcept {
  if (!std::isfinite(style.size) || style.size <= 0.0f || !std::isfinite(capTop)) return capTop;
  return capTop + metrics.capHeight * style.size;
}

usize truncateToWidth(std::string_view text, const UiFontMetrics &metrics,
                      const UiTypeStyle &style, float maximumWidth) noexcept {
  if (!std::isfinite(maximumWidth) || maximumWidth <= 0.0f) return 0;
  if (!std::isfinite(style.size) || style.size <= 0.0f) return 0;
  const float tracking = style.tracking * style.size;
  float width = 0.0f;
  for (usize cursor=0;cursor<text.size();) {
    const usize index=cursor;
    const u32 codepoint=nextUiCodepoint(text,cursor);
    u32 substitutes[3];
    const u32 n = codepoint > kUiLastGlyph ? uiLatinFallback(codepoint, substitutes) : 0u;
    float advance = 0;
    if (n) for (u32 i = 0; i < n; ++i) advance += metrics.advanceOf(substitutes[i]) * style.size + tracking;
    else advance = metrics.advanceOf(style.uppercase ? toUpperAscii(codepoint) : codepoint) * style.size + tracking;
    if (width + advance > maximumWidth) return index;
    width += advance;
  }
  return text.size();
}

} // namespace ae::ui
