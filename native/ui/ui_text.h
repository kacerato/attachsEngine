// Métricas de texto da interface.
//
// A UI do editor é desenhada pela engine, então medir texto é responsabilidade
// dela: sem isso, um rótulo não sabe sua largura e nenhuma caixa `Hug` fecha.
// As métricas vêm do mesmo atlas SDF que o desenho usa — se as duas fontes
// divergissem, o layout reservaria um espaço e o glifo apareceria em outro.
//
// **Escopo declarado, não escondido.** A tabela cobre ASCII imprimível. Isso
// atende os rótulos do editor, que são identificadores e termos técnicos, e NÃO
// atende nome de projeto, nome de objeto ou qualquer texto que o usuário digite.
// Um codepoint fora da tabela recebe a largura de fallback: o layout continua
// coerente e o glifo aparece como o que o atlas tiver. Acentuação, CJK e
// bidirecional exigem atlas por faixa e shaping de verdade, e nenhum dos dois
// entra por acidente numa tabela de 96 posições.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "ui/ui_theme.h"

#include <string_view>

namespace ae::ui {

// Decodificação compartilhada por medição, corte e desenho. O cursor sempre
// avança; sequências inválidas produzem substituição sem ler além do texto.
inline u32 nextUiCodepoint(std::string_view text, usize &cursor) noexcept {
  const auto first=static_cast<unsigned char>(text[cursor++]);
  if(first<0x80) return first;
  const unsigned count=first>=0xc2&&first<=0xdf?1:first>=0xe0&&first<=0xef?2:first>=0xf0&&first<=0xf4?3:0;
  if(!count || text.size()-cursor<count) return 0xfffd;
  u32 value=first & (0x7f>>count);
  for(unsigned i=0;i<count;++i) {
    const auto byte=static_cast<unsigned char>(text[cursor+i]);
    if((byte&0xc0)!=0x80) return 0xfffd;
    value=(value<<6)|(byte&0x3f);
  }
  if(value<(count==1?0x80u:count==2?0x800u:0x10000u) || value>0x10ffff || (value>=0xd800&&value<=0xdfff)) return 0xfffd;
  cursor+=count;return value;
}
inline u32 uiUppercase(u32 value) noexcept {
  return (value>='a'&&value<='z') || (value>=0xe0&&value<=0xfe&&value!=0xf7) ? value-32:value;
}

inline constexpr u32 kUiFirstGlyph = 32;  // espaço
inline constexpr u32 kUiLastGlyph = 255;  // til
inline constexpr u32 kUiGlyphCount = kUiLastGlyph - kUiFirstGlyph + 1;

struct UiFontMetrics final {
  // Avanços em fração do corpo (em), não em pixels: a mesma tabela serve para
  // o rótulo de 11 px e o título de 38 px.
  float advance[kUiGlyphCount]{};
  float fallbackAdvance = 0.5f;
  float ascent = 0.75f;
  float descent = 0.25f;
  // Altura da caixa alta, em em. Os masters do ASTRA foram medidos pela altura
  // do "P", não pelo corpo da fonte — alinhar por ela é o que faz um rótulo
  // cair no mesmo lugar do mockup.
  float capHeight = 0.72f;

  bool isReady() const noexcept { return advance[0] > 0.0f || fallbackAdvance > 0.0f; }
  float advanceOf(u32 codepoint) const noexcept {
    if (codepoint < kUiFirstGlyph || codepoint > kUiLastGlyph) return fallbackAdvance;
    return advance[codepoint - kUiFirstGlyph];
  }
};

// Métricas de reserva: monoespaçadas em meio em. Não são a fonte do produto —
// existem para que layout e testes tenham um comportamento definido antes de o
// atlas ser carregado, em vez de medir tudo como zero e empilhar os painéis.
const UiFontMetrics &fallbackFontMetrics() noexcept;

// Largura em pixels de uma linha. `tracking` é somado depois de cada glifo,
// inclusive o último — é assim que o desenho acumula, e descontar aqui faria a
// caixa medida ficar mais estreita que o traço.
float measureTextWidth(std::string_view text, const UiFontMetrics &metrics,
                       const UiTypeStyle &style) noexcept;

// Altura de uma linha, do topo ao fundo da caixa de linha.
float lineHeight(const UiFontMetrics &metrics, const UiTypeStyle &style) noexcept;

// Onde fica a linha de base para que a caixa alta comece em `capTop`. Mesma
// conversão que `Design.baselineForCapTop` faz no shell Java, pela mesma razão.
float baselineForCapTop(const UiFontMetrics &metrics, const UiTypeStyle &style,
                        float capTop) noexcept;

// Maior prefixo de `text` que cabe em `maximumWidth`, em bytes. Devolve o
// tamanho inteiro quando tudo cabe. Não insere reticências: quem trunca decide
// se o corte ganha um "…" e precisa medir esse glifo também.
usize truncateToWidth(std::string_view text, const UiFontMetrics &metrics,
                      const UiTypeStyle &style, float maximumWidth) noexcept;

} // namespace ae::ui
