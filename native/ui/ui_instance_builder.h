// Converte a lista de desenho da interface nas instâncias que a GPU consome.
//
// **Uma instância é um quad, e o frame inteiro é UM draw call.** Não há troca de
// pipeline, de scissor nem de textura entre um painel e o rótulo dentro dele: o
// recorte viaja em cada instância e é resolvido no fragmento, e as duas texturas
// (campo de distância da fonte e atlas de ícones) ficam as duas ligadas o tempo
// todo. A alternativa — um `vkCmdSetScissor` por painel — custaria uma quebra de
// lote a cada recorte, que numa hierarquia com dezenas de linhas é a diferença
// entre uma submissão e cem.
//
// Esta camada é onde o texto deixa de ser texto: cada glifo vira o seu próprio
// quad, com o retângulo do atlas já resolvido. O backend não conhece fonte,
// nem alinhamento, nem maiúsculas — ele conhece quads.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "ui/ui_draw_list.h"
#include "ui/ui_font.h"
#include "ui/ui_geometry.h"
#include "ui/ui_icon_atlas.h"

#include <span>
#include <vector>

namespace ae::ui {

// Espelho de `UiInstance` em astra_ui.vert, campo a campo. 80 bytes, alinhado a
// 16 como std430 exige; um `static_assert` no .cpp tranca o tamanho.
enum class UiInstanceKind : u32 {
  // Retângulo arredondado, com preenchimento, degradê horizontal e/ou contorno.
  Rect = 0,
  // Glifo amostrado do campo de distância. A forma vem da textura, não da caixa.
  Glyph = 1,
  // Ícone amostrado do atlas RGBA, multiplicado pela cor (branco = intacto).
  Icon = 2,
};

struct alignas(16) UiInstance final {
  // x, y, largura, altura em pixels lógicos.
  float bounds[4]{};
  // Recorte já intersectado. O fragmento descarta fora dele.
  float clip[4]{};
  // Retângulo no atlas, em texels. Zerado para Rect.
  float atlas[4]{};
  // raio, espessura do contorno, tipo (UiInstanceKind), maciez da borda do campo.
  float params[4]{};
  // preenchimento, fim do degradê, contorno, reservado. 0xAARRGGBB.
  u32 colors[4]{};
};

struct UiInstanceBuildResult final {
  u32 emitted = 0;
  // Instâncias que não couberam na capacidade. Diferente de zero significa que
  // parte do frame não foi desenhada — é falha de orçamento, não de conteúdo, e
  // por isso é contada em vez de silenciada.
  u32 dropped = 0;
  // Comandos de texto cujo peso não existia na fonte carregada.
  u32 missingGlyphRuns = 0;
};

// Anexa as instâncias de `list` em `out`. `font` e `icons` podem estar
// descarregados: nesse caso os comandos que dependem deles são pulados e
// contados, e os retângulos continuam sendo desenhados — uma interface sem
// rótulo ainda mostra a forma dos painéis, o que é muito melhor do que uma tela
// preta enquanto o atlas não chega.
UiInstanceBuildResult buildUiInstances(const UiDrawList &list, const UiFont &font,
                                       const UiIconAtlas &icons, u32 maximumInstances,
                                       std::vector<UiInstance> &out);

// Origem da linha de base para um comando de texto dentro da caixa dele. Exposta
// porque o teste precisa checar o alinhamento sem reimplementar a regra.
UiPoint resolveTextOrigin(const UiDrawCommand &command, const UiFont &font,
                          UiFontWeight weight, float lineWidth) noexcept;

} // namespace ae::ui
