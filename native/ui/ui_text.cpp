#include "ui/ui_text.h"

#include <cmath>

namespace ae::ui {
namespace {

// Maiúscula ASCII. A UI usa `uppercase` só em rótulos técnicos, que são ASCII
// por construção; um byte fora dessa faixa passa intacto em vez de virar lixo.
u32 toUpperAscii(u32 codepoint) noexcept {
  return (codepoint >= 'a' && codepoint <= 'z') ? codepoint - 32 : codepoint;
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
  for (const char byte : text) {
    const u32 codepoint = static_cast<u32>(static_cast<unsigned char>(byte));
    width += metrics.advanceOf(style.uppercase ? toUpperAscii(codepoint) : codepoint);
  }
  const float trackingPixels = style.tracking * style.size * static_cast<float>(text.size());
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
  for (usize index = 0; index < text.size(); ++index) {
    const u32 codepoint = static_cast<u32>(static_cast<unsigned char>(text[index]));
    const float advance =
        metrics.advanceOf(style.uppercase ? toUpperAscii(codepoint) : codepoint) * style.size +
        tracking;
    if (width + advance > maximumWidth) return index;
    width += advance;
  }
  return text.size();
}

} // namespace ae::ui
