#include "ui/ui_instance_builder.h"

#include <cmath>

namespace ae::ui {
namespace {

// astra_ui.vert lê o buffer como um array std430 destas. Um campo a mais aqui e
// o shader passa a ler a instância seguinte, sem nenhum sintoma imediato.
static_assert(sizeof(UiInstance) == 80, "astra_ui.vert le instancias de 80 bytes.");
static_assert(alignof(UiInstance) == 16, "std430 alinha a instancia em 16 bytes.");

void writeRect(float (&destination)[4], const UiRect &rect) noexcept {
  destination[0] = rect.x;
  destination[1] = rect.y;
  destination[2] = rect.width;
  destination[3] = rect.height;
}

} // namespace

UiPoint resolveTextOrigin(const UiDrawCommand &command, const UiFont &font, UiFontWeight weight,
                          float lineWidth) noexcept {
  const UiFontMetrics metrics = font.metrics(weight);
  const UiTypeStyle &style = command.style;

  float x = command.bounds.x;
  const float slack = command.bounds.width - lineWidth;
  if (command.horizontalAlign == UiAlign::Center) x += slack * 0.5f;
  else if (command.horizontalAlign == UiAlign::End) x += slack;

  const float ascent = metrics.ascent * style.size;
  const float descent = metrics.descent * style.size;
  float y = command.bounds.y + ascent;
  if (command.verticalAlign == UiAlign::Center) {
    // Centrado pela ALTURA DA CAIXA ALTA, não pela caixa cheia da fonte.
    //
    // A caixa cheia inclui o descendente, que quase nenhum rótulo da interface
    // usa — "Position", "Transform", "MESH" não têm um único descendente. Centrar
    // por ela empurra o texto para cima e deixa toda linha do Inspector parecendo
    // desalinhada com o ícone ao lado. Centrando pela caixa alta, o miolo óptico
    // do texto coincide com o miolo da linha, que é o que o olho compara.
    //
    // Dois corpos diferentes na mesma linha não podem ter a mesma linha de base
    // se cada um for centrado — o maior desce mais, geometricamente. O que eles
    // passam a compartilhar é o centro da caixa alta, e é isso que faz um rótulo
    // de 11 px e um valor de 14 px parecerem na mesma linha.
    y = command.bounds.y + command.bounds.height * 0.5f + metrics.capHeight * style.size * 0.5f;
  } else if (command.verticalAlign == UiAlign::End) {
    y = command.bounds.bottom() - descent;
  }
  return {x, y};
}

UiInstanceBuildResult buildUiInstances(const UiDrawList &list, const UiFont &font,
                                       const UiIconAtlas &icons, u32 maximumInstances,
                                       std::vector<UiInstance> &out) {
  UiInstanceBuildResult result{};
  std::vector<UiPositionedGlyph> glyphs;

  const auto push = [&](const UiInstance &instance) {
    if (out.size() >= maximumInstances) {
      ++result.dropped;
      return;
    }
    out.push_back(instance);
    ++result.emitted;
  };

  for (const UiDrawCommand &command : list.commands()) {
    UiInstance instance{};
    writeRect(instance.clip, command.clip);
    instance.params[0] = command.radius;
    instance.params[1] = command.borderWidth;
    instance.colors[0] = command.color;
    instance.colors[1] = command.gradientEnd;
    instance.colors[2] = command.borderColor;

    switch (command.kind) {
      case UiPrimitive::Rect: {
        writeRect(instance.bounds, command.bounds);
        instance.params[2] = static_cast<float>(UiInstanceKind::Rect);
        push(instance);
        break;
      }
      case UiPrimitive::Image: {
        const UiRect atlasRect = icons.rectOf(static_cast<UiIcon>(command.image));
        if (atlasRect.isEmpty()) break;  // atlas ausente: a forma do painel sobrevive
        writeRect(instance.bounds, command.bounds);
        writeRect(instance.atlas, atlasRect);
        instance.params[2] = static_cast<float>(UiInstanceKind::Icon);
        push(instance);
        break;
      }
      case UiPrimitive::Line: {
        writeRect(instance.bounds, command.bounds);
        writeRect(instance.atlas, command.atlas);
        instance.params[1] = command.borderWidth;
        instance.params[2] = static_cast<float>(UiInstanceKind::Line);
        push(instance);
        break;
      }
      case UiPrimitive::Text: {
        if (!font.isReady()) {
          ++result.missingGlyphRuns;
          break;
        }
        const std::string_view text = list.textOf(command);
        if (text.empty()) break;
        const UiFontWeight weight = weightForStyle(command.style);
        const float width = measureTextWidth(text, font.metrics(weight), command.style);
        const UiPoint origin = resolveTextOrigin(command, font, weight, width);

        glyphs.clear();
        font.layoutLine(text, weight, command.style, origin, glyphs);
        for (const UiPositionedGlyph &glyph : glyphs) {
          UiInstance glyphInstance = instance;
          writeRect(glyphInstance.bounds, glyph.bounds);
          writeRect(glyphInstance.atlas, glyph.atlas);
          glyphInstance.params[2] = static_cast<float>(UiInstanceKind::Glyph);
          // Raio e contorno pertencem à caixa, não ao glifo: herdá-los faria o
          // "o" de um botão arredondado ganhar cantos.
          glyphInstance.params[0] = 0.0f;
          glyphInstance.params[1] = 0.0f;
          // O alcance do campo viaja com a instância porque é o que converte
          // "texels por pixel de tela" na largura da transição do fragmento.
          glyphInstance.params[3] = font.spreadPixels();
          // O degradê também não atravessa: um rótulo dentro de um botão com
          // degradê deve manter a própria cor.
          glyphInstance.colors[1] = command.color;
          push(glyphInstance);
        }
        break;
      }
    }
  }
  return result;
}

} // namespace ae::ui
