// Lista de desenho da interface: o que o layout resolveu, na ordem em que a GPU
// vai desenhar.
//
// São quatro primitivas — retângulo, texto, imagem e linha — e nada mais. O sistema
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

enum class UiPrimitive : u8 { Rect, Text, Image, Line, Triangle };

struct UiMeshVertex final { UiPoint position{}, uv{}; UiColor color = 0xFFFFFFFF; };

// Identificador de imagem no atlas da interface. Zero é "nenhuma": um comando
// de imagem com id zero é recusado em vez de desenhar o primeiro slot do atlas.
using UiImageId = u32;
inline constexpr UiImageId kUiNoImage = 0;
// R4: imagem do atlas DINÂMICO de prévia (miniaturas e visualizador de textura).
// O retângulo em texels viaja no próprio comando (`atlas`): o conteúdo desse
// atlas é composto pela sessão e não tem tabela fixa como o atlas de ícones.
inline constexpr UiImageId kUiPreviewImage = 0x40000000u;
inline constexpr UiImageId kUiCameraPreviewImage = 0x40000001u;
inline constexpr UiImageId kUiGuiImage = 0x40000002u;

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
  // Só usado por UiPrimitive::Line, onde carrega as duas pontas do segmento
  // (x0, y0, x1, y1). É um campo reaproveitado e não um significado novo: quem
  // lê descobre pelo `kind`, que é a mesma disciplina do resto da struct.
  UiRect atlas{};
  // Fatia do arena de texto da lista. Só tem sentido em UiPrimitive::Text.
  u32 textOffset = 0;
  u32 textLength = 0;
  UiTypeStyle style{};
  UiAlign horizontalAlign = UiAlign::Start;
  UiAlign verticalAlign = UiAlign::Center;
  UiMeshVertex vertices[3]{};
  float projection[12]{}; // local pixel X/Y/constant -> homogeneous clip XYZW
  u32 worldFlags=0; // 1 world plane, 2 scene depth occlusion
  UiRect worldClip{};
};

class UiDrawList final {
public:
  static constexpr u32 kMaximumCommands = 32768;
  static constexpr u32 kMaximumClipDepth = 32;

  // Recomeça o frame. O recorte volta a ser `viewport` e todo comando futuro
  // nasce recortado por ele.
  void begin(const UiRect &viewport, const UiFontMetrics &metrics) noexcept;
  // UV de um texel branco do atlas imediato: triângulos sólidos (addSolidTriangle)
  // amostram este ponto e ficam com a cor pura dos vértices.
  void setSolidUv(UiPoint uv) noexcept { solidUv_ = uv; }
  bool addSolidTriangle(UiPoint a, UiPoint b, UiPoint c, UiColor color) {
    return addTriangle({a, solidUv_, color}, {b, solidUv_, color}, {c, solidUv_, color});
  }

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
  // R4: recorte `texels` do atlas de prévia desenhado em `bounds`.
  bool addPreviewImage(const UiRect &bounds, const UiRect &texels, UiColor tint = 0xFFFFFFFF,
                       float radius = 0.0f);
  bool addGuiImage(const UiRect &bounds,const UiRect &texels,UiColor tint,float radius);
  void projectRange(usize first,const float projection[12],bool occlusion,const UiRect &viewport);
  // Segmento com extremidades arredondadas. `bounds` do comando guarda a caixa
  // envolvente, que é o que o recorte precisa; as pontas viajam em `atlas`.
  bool addLine(UiPoint from, UiPoint to, UiColor color, float width);
  // Normalized UVs in the dedicated immediate-mode font atlas. Actual mesh,
  // including per-vertex colors; not a widget-name translation.
  bool addTriangle(const UiMeshVertex &a, const UiMeshVertex &b, const UiMeshVertex &c);
  bool append(const UiDrawList &other);

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
  UiPoint solidUv_{};
  UiRect clipStack_[kMaximumClipDepth]{};
  u32 clipDepth_ = 0;
  u32 culled_ = 0;
  const UiFontMetrics *metrics_ = &fallbackFontMetrics();
};

} // namespace ae::ui
