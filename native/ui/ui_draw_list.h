// Lista de desenho da interface: o que o layout resolveu, na ordem em que a GPU
// vai desenhar.
//
// São três primitivas — retângulo, texto e imagem — e nada mais. O sistema
// visual do ASTRA é feito de retângulos arredondados, rótulos e ícones; um
// traçado vetorial genérico aqui significaria um tesselador, um cache e uma
// classe inteira de bugs de precisão para desenhar coisas que os masters não
// têm. Ícone é imagem porque as folhas de ícone já são bitmaps com contorno
// preto e preenchimento lima: rasterizá-los como caminhos perderia exatamente
// o traço que define a marca.
//
// A lista é reconstruída todo frame. Ela não guarda estado de widget, não sabe
// o que é um botão e não recebe eventos: quem decide a aparência é a camada de
// cima, e é por isso que a mesma lista serve para o editor, para o HUD de
// runtime e para uma captura de tela determinística num teste.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"
#include "ui/ui_text.h"
#include "ui/ui_theme.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::ui {

enum class UiPrimitive : u8 { Rect, Text, Image };

// Identificador de imagem no atlas da interface. Zero é "nenhuma": um comando
// de imagem com id zero é recusado em vez de desenhar o primeiro slot do atlas.
using UiImageId = u32;
inline constexpr UiImageId kUiNoImage = 0;

struct UiDrawCommand final {
  UiPrimitive kind = UiPrimitive::Rect;
  UiRect bounds{};
  // Já é a interseção com a pilha de recorte no momento em que o comando entrou.
  // O consumidor não precisa reconstruir a pilha para desenhar.
  UiRect clip{};
  // Preenchimento do retângulo, cor do texto ou tinta da imagem.
  UiColor color = 0;
  // Fim do degradê horizontal. Igual a `color` quando não há degradê — o
  // consumidor não precisa de um sinalizador separado para descobrir isso.
  UiColor gradientEnd = 0;
  UiColor borderColor = 0;
  float radius = 0.0f;
  float borderWidth = 0.0f;
  UiImageId image = kUiNoImage;
  // Fatia do arena de texto da lista. Só tem sentido em UiPrimitive::Text.
  u32 textOffset = 0;
  u32 textLength = 0;
  UiTypeStyle style{};
  UiAlign horizontalAlign = UiAlign::Start;
  UiAlign verticalAlign = UiAlign::Center;
};

class UiDrawList final {
public:
  static constexpr u32 kMaximumCommands = 8192;
  static constexpr u32 kMaximumClipDepth = 32;

  // Recomeça o frame. O recorte volta a ser `viewport` e todo comando futuro
  // nasce recortado por ele.
  void begin(const UiRect &viewport, const UiFontMetrics &metrics) noexcept;

  // Empilha um recorte, sempre interseção com o atual — um filho nunca pode
  // desenhar fora do pai, e permitir isso transformaria qualquer erro de layout
  // num painel vazando por cima da cena.
  bool pushClip(const UiRect &rect) noexcept;
  void popClip() noexcept;
  const UiRect &currentClip() const noexcept { return clipStack_[clipDepth_]; }

  bool addRect(const UiRect &bounds, UiColor color, float radius = 0.0f) ;
  // Degradê horizontal, da esquerda para a direita. Existe por causa do slider
  // de temperatura de cor, que precisa mostrar a própria escala que edita.
  bool addGradientRect(const UiRect &bounds, UiColor start, UiColor end, float radius = 0.0f);
  // Contorno desenhado PARA DENTRO do retângulo, como no CSS `border`. Traçar
  // centrado faria a borda lima do asset selecionado invadir o vizinho.
  bool addBorder(const UiRect &bounds, UiColor color, float width, float radius = 0.0f);
  bool addText(const UiRect &bounds, std::string_view text, UiColor color,
               const UiTypeStyle &style, UiAlign horizontal = UiAlign::Start,
               UiAlign vertical = UiAlign::Center);
  bool addImage(const UiRect &bounds, UiImageId image, UiColor tint = 0xFFFFFFFF,
                float radius = 0.0f);

  std::span<const UiDrawCommand> commands() const noexcept { return commands_; }
  u32 commandCount() const noexcept { return static_cast<u32>(commands_.size()); }
  // Quantos comandos foram descartados por cair inteiramente fora do recorte.
  // É diagnóstico de layout: um número alto significa painel construído fora da
  // tela, não uma otimização a comemorar.
  u32 culledCommandCount() const noexcept { return culled_; }
  std::string_view textOf(const UiDrawCommand &command) const noexcept;
  const UiFontMetrics &fontMetrics() const noexcept { return *metrics_; }
  // Largura que `addText` vai ocupar. Exposto para o layout medir antes, sem
  // duplicar a regra de tracking em dois lugares.
  float measure(std::string_view text, const UiTypeStyle &style) const noexcept;

private:
  bool push(const UiDrawCommand &command);

  std::vector<UiDrawCommand> commands_;
  std::string textArena_;
  UiRect clipStack_[kMaximumClipDepth]{};
  u32 clipDepth_ = 0;
  u32 culled_ = 0;
  const UiFontMetrics *metrics_ = &fallbackFontMetrics();
};

} // namespace ae::ui
