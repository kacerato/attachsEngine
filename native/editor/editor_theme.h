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
} // namespace ae::editor
