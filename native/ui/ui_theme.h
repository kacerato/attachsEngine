// Tokens do sistema visual ASTRA, do lado nativo.
//
// Os valores vêm de `assets/astra-visual/tokens/astra.tokens.json`, que por sua
// vez foi amostrado dos masters — não são estimativas de olho. Esta é a segunda
// cópia deles (a primeira é `Design.java`, do shell de projetos) e ela existe
// porque a UI do editor é desenhada pela engine, não pelo Canvas do Android.
// Duas cópias do mesmo número é uma dívida real: quando o shell migrar para
// esta camada, `Design.java` sai e este arquivo fica como fonte única.
//
// Sem Vulkan, sem Android, sem I/O.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"

namespace ae::ui {

// 0xAARRGGBB. A ordem casa com a do Android e com o literal hexadecimal que
// aparece nos tokens, então um valor pode ser conferido de olho contra o JSON.
using UiColor = u32;

inline constexpr UiColor uiColor(u32 argb) noexcept { return argb; }

// Multiplica apenas o alfa, preservando a cor. Usado por estados desabilitados
// e por camadas de sobreposição; nunca para clarear ou escurecer, o que exigiria
// um token próprio em vez de um fator aplicado no lugar de uso.
inline UiColor withAlpha(UiColor color, float alpha) noexcept {
  const float clamped = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
  const u32 base = static_cast<u32>((color >> 24) & 0xffu);
  const u32 scaled = static_cast<u32>(static_cast<float>(base) * clamped + 0.5f);
  return (color & 0x00ffffffu) | (scaled << 24);
}

struct UiPalette final {
  UiColor voidBlack = 0xFF000000;
  UiColor canvas = 0xFF050505;
  UiColor silhouette = 0xFF101010;
  UiColor surface = 0xFF0F0F0F;
  UiColor raised = 0xFF141414;
  UiColor line = 0xFF1E1E1E;
  UiColor lineSoft = 0xFF131313;
  UiColor track = 0xFF3D3D3D;
  UiColor text = 0xFFFFFFFF;
  UiColor textDim = 0xFF9A9A9A;
  UiColor textMuted = 0xFF6E6E6E;
  UiColor textFaint = 0xFF4A4A4A;
  UiColor accent = 0xFFCAFB04;
  UiColor accentInk = 0xFF050505;
  // rgba(202,251,4,0.09) do JSON, pré-multiplicado no canal alfa do formato.
  UiColor accentWash = 0x17CAFB04;
  // Os três eixos do gizmo. Vermelho/verde/azul não é escolha estética: é a
  // convenção que o usuário já traz de qualquer outra ferramenta 3D, e trocá-la
  // por cores da marca custaria mais do que a coerência ganharia.
  UiColor axisX = 0xFFE5484D;
  UiColor axisY = 0xFF8CE04B;
  UiColor axisZ = 0xFF3E7BFA;
};

struct UiRadii final {
  float card = 16.0f;
  float thumb = 8.0f;
  float control = 10.0f;
  float mark = 24.0f;
  float bar = 5.0f;
};

// Escala de espaçamento. Passos de 4 dp: é o que mantém painel, controle e
// rótulo alinhados sem que cada tela invente seu próprio número.
struct UiSpacing final {
  float hairline = 1.0f;
  float tiny = 4.0f;
  float small = 8.0f;
  float medium = 12.0f;
  float large = 16.0f;
  float huge = 24.0f;
  float section = 32.0f;
};

// Corpo em pixels lógicos e entrelinha como fator do corpo. `tracking` é em
// fração do corpo, como no JSON, e não em pixels absolutos: mudar o corpo de um
// rótulo não pode mudar a proporção do espaçamento entre letras.
struct UiTypeStyle final {
  float size = 15.0f;
  float tracking = 0.0f;
  float lineHeight = 1.25f;
  bool uppercase = false;
  bool medium = false;
};

struct UiTypography final {
  UiTypeStyle label{11.0f, 0.22f, 1.2f, true, true};
  UiTypeStyle labelWide{11.0f, 0.34f, 1.2f, true, true};
  UiTypeStyle body{15.0f, 0.0f, 1.3f, false, false};
  UiTypeStyle cardName{17.0f, 0.0f, 1.25f, false, true};
  UiTypeStyle title{38.0f, 0.0f, 1.1f, false, true};
  // Campo numérico do Inspector. Tabular porque as três colunas X/Y/Z têm de
  // ficar alinhadas mesmo quando os valores mudam de largura durante um arraste.
  UiTypeStyle numeric{14.0f, 0.0f, 1.2f, false, false};
};

// Métricas de toque. Separadas da tipografia porque respondem a uma regra
// diferente: a acessibilidade define o mínimo, não a estética da tela.
struct UiTouchMetrics final {
  // 48 dp é o mínimo do Material e o que 3.1.5 do plano exige.
  float minimumTarget = 48.0f;
  // Deslocamento a partir do qual um toque deixa de ser toque e vira arraste.
  // Em pixels lógicos: mais alto que o padrão de 8 dp porque o dedo sobre um
  // gizmo pequeno treme mais do que sobre uma lista.
  float dragSlop = 10.0f;
  // Acima disto, dois toques deixam de ser interpretados como duplo.
  float doubleTapSlop = 24.0f;
};

struct UiTheme final {
  UiPalette color{};
  UiRadii radius{};
  UiSpacing spacing{};
  UiTypography type{};
  UiTouchMetrics touch{};
  // Painéis e barras do editor ficam dentro desta margem. As áreas seguras do
  // aparelho (notch, barra de gestos) entram por cima, em tempo de execução.
  UiInsets safeArea{};
};

inline const UiTheme &defaultTheme() noexcept {
  static const UiTheme theme{};
  return theme;
}

} // namespace ae::ui
