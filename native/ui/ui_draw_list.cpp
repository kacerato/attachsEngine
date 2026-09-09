#include "ui/ui_draw_list.h"

#include <cmath>

namespace ae::ui {

void UiDrawList::begin(const UiRect &viewport, const UiFontMetrics &metrics) noexcept {
  commands_.clear();
  textArena_.clear();
  clipDepth_ = 0;
  culled_ = 0;
  clipStack_[0] = isFinite(viewport) ? viewport : UiRect{};
  metrics_ = metrics.isReady() ? &metrics : &fallbackFontMetrics();
}

bool UiDrawList::pushClip(const UiRect &rect) noexcept {
  if (clipDepth_ + 1 >= kMaximumClipDepth) return false;
  if (!isFinite(rect)) return false;
  clipStack_[clipDepth_ + 1] = intersect(clipStack_[clipDepth_], rect);
  ++clipDepth_;
  return true;
}

void UiDrawList::popClip() noexcept {
  if (clipDepth_ > 0) --clipDepth_;
}

bool UiDrawList::push(const UiDrawCommand &command) {
  if (commands_.size() >= kMaximumCommands) return false;
  if (!isFinite(command.bounds)) return false;
  // Um comando totalmente fora do recorte não vira geometria. Ele é contado
  // porque é sintoma: uma lista longa recortada inteira é layout errado, e o
  // número precisa aparecer em algum lugar para que isso seja notado.
  if (command.clip.isEmpty() || intersect(command.bounds, command.clip).isEmpty()) {
    ++culled_;
    return true;
  }
  commands_.push_back(command);
  return true;
}

bool UiDrawList::addRect(const UiRect &bounds, UiColor color, float radius) {
  if (!std::isfinite(radius) || radius < 0.0f) return false;
  UiDrawCommand command{};
  command.kind = UiPrimitive::Rect;
  command.bounds = bounds;
  command.clip = currentClip();
  command.color = color;
  command.gradientEnd = color;
  command.radius = radius;
  return push(command);
}

bool UiDrawList::addGradientRect(const UiRect &bounds, UiColor start, UiColor end, float radius) {
  if (!std::isfinite(radius) || radius < 0.0f) return false;
  UiDrawCommand command{};
  command.kind = UiPrimitive::Rect;
  command.bounds = bounds;
  command.clip = currentClip();
  command.color = start;
  command.gradientEnd = end;
  command.radius = radius;
  return push(command);
}

bool UiDrawList::addBorder(const UiRect &bounds, UiColor color, float width, float radius) {
  if (!std::isfinite(width) || width <= 0.0f) return false;
  if (!std::isfinite(radius) || radius < 0.0f) return false;
  UiDrawCommand command{};
  command.kind = UiPrimitive::Rect;
  command.bounds = bounds;
  command.clip = currentClip();
  // Preenchimento transparente: a borda é um comando próprio e não presume que
  // alguém já pintou o fundo. Um card com fundo e contorno emite dois comandos.
  command.color = 0;
  command.gradientEnd = 0;
  command.borderColor = color;
  command.borderWidth = width;
  command.radius = radius;
  return push(command);
}

bool UiDrawList::addText(const UiRect &bounds, std::string_view text, UiColor color,
                         const UiTypeStyle &style, UiAlign horizontal, UiAlign vertical) {
  if (text.empty()) return true;
  if (!std::isfinite(style.size) || style.size <= 0.0f) return false;
  if (textArena_.size() + text.size() > 0xffffffffull) return false;
  UiDrawCommand command{};
  command.kind = UiPrimitive::Text;
  command.bounds = bounds;
  command.clip = currentClip();
  command.color = color;
  command.gradientEnd = color;
  command.style = style;
  command.horizontalAlign = horizontal;
  command.verticalAlign = vertical;
  command.textOffset = static_cast<u32>(textArena_.size());
  command.textLength = static_cast<u32>(text.size());
  // O arena cresce antes do recorte ser avaliado: um texto descartado deixa
  // bytes órfãos até o próximo `begin`. É desperdício de memória temporária, e
  // não um defeito — desfazer a escrita exigiria um comando parcialmente
  // construído, que é pior de ler do que alguns bytes por frame.
  textArena_.append(text);
  return push(command);
}

bool UiDrawList::addImage(const UiRect &bounds, UiImageId image, UiColor tint, float radius) {
  if (image == kUiNoImage) return false;
  if (!std::isfinite(radius) || radius < 0.0f) return false;
  UiDrawCommand command{};
  command.kind = UiPrimitive::Image;
  command.bounds = bounds;
  command.clip = currentClip();
  command.color = tint;
  command.gradientEnd = tint;
  command.image = image;
  command.radius = radius;
  return push(command);
}

bool UiDrawList::addLine(UiPoint from, UiPoint to, UiColor color, float width) {
  if (!std::isfinite(width) || width <= 0.0f) return false;
  if (!std::isfinite(from.x) || !std::isfinite(from.y)) return false;
  if (!std::isfinite(to.x) || !std::isfinite(to.y)) return false;
  // Clip before GPU interpolation: near-plane projections may be millions of
  // pixels away. Huge quads lose precision even when fragment clipping is exact.
  const UiRect clip=currentClip();
  const double dx=double(to.x)-from.x,dy=double(to.y)-from.y;
  double enter=0,leave=1;
  auto edge=[&](double p,double q) {
    if(p==0) return q>=0;
    const double t=q/p;
    if(p<0) enter=std::max(enter,t);else leave=std::min(leave,t);
    return enter<=leave;
  };
  const double margin=width*.5+1;
  if(!edge(-dx,double(from.x)-clip.x+margin) || !edge(dx,double(clip.x)+clip.width+margin-from.x) ||
     !edge(-dy,double(from.y)-clip.y+margin) || !edge(dy,double(clip.y)+clip.height+margin-from.y)) return false;
  const UiPoint origin=from;
  from={float(origin.x+enter*dx),float(origin.y+enter*dy)};
  to={float(origin.x+leave*dx),float(origin.y+leave*dy)};
  UiDrawCommand command{};
  command.kind = UiPrimitive::Line;
  const float half = width * 0.5f + 1.0f;  // um pixel a mais para a suavização
  const float left = std::min(from.x, to.x) - half;
  const float top = std::min(from.y, to.y) - half;
  command.bounds = {left, top, std::max(from.x, to.x) + half - left,
                    std::max(from.y, to.y) + half - top};
  // As pontas ocupam o campo do atlas porque uma linha não amostra textura. É
  // um campo reaproveitado, não um significado novo: quem lê sabe pelo `kind`.
  command.atlas = {from.x, from.y, to.x, to.y};
  command.clip = currentClip();
  command.color = color;
  command.gradientEnd = color;
  command.borderWidth = width;
  return push(command);
}

std::string_view UiDrawList::textOf(const UiDrawCommand &command) const noexcept {
  if (command.kind != UiPrimitive::Text) return {};
  if (static_cast<usize>(command.textOffset) + command.textLength > textArena_.size()) return {};
  return std::string_view(textArena_).substr(command.textOffset, command.textLength);
}

float UiDrawList::measure(std::string_view text, const UiTypeStyle &style) const noexcept {
  return measureTextWidth(text, *metrics_, style);
}

} // namespace ae::ui
