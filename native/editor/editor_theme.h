#pragma once
#include "ui/ui_theme.h"

namespace ae::editor {
// Tema do editor separado da entrada Astra e das interfaces de jogo.
inline const ui::UiTheme &editorTheme() noexcept {
  static const ui::UiTheme theme=[] {
    ui::UiTheme value=ui::defaultTheme();
    value.color.accent=0xFF8DE54C;
    value.color.accentWash=0x178DE54C;
    value.color.accentInk=0xFF15210F;
    return value;
  }();
  return theme;
}
// Approved code workspace palette. The scene keeps its own established theme.
inline const ui::UiTheme &editorCodeTheme() noexcept {
  static const ui::UiTheme theme=[] {
    auto value=editorTheme();
    value.color.canvas=0xFF0C1217;value.color.surface=0xFF0F161C;
    value.color.silhouette=0xFF121C23;value.color.raised=0xFF1B2730;
    value.color.line=0xFF344651;value.color.lineSoft=0xFF23313A;
    value.color.text=0xFFF3F2EB;value.color.textDim=0xFFC3CBCF;
    value.color.textMuted=0xFF95A5AF;value.color.accent=0xFFB9F227;
    // Explicit surface color: a restrained selection, independent of linear
    // framebuffer blending (a translucent lime looked bright olive on sRGB).
    value.color.accentWash=0xFF202F23;value.color.accentInk=0xFF152008;
    value.color.danger=0xFFFF6464;value.color.warning=0xFFFFCB38;
    value.radius.control=6;value.radius.card=10;
    value.type.caption.medium=false;
    return value;
  }();
  return theme;
}
} // namespace ae::editor
