// Geometria da interface. Tudo em pixels lógicos, com a origem no canto
// superior esquerdo da superfície — a mesma convenção de coordenada de toque
// que o Android entrega, para que nenhum estágio precise inverter eixo.
//
// Sem Vulkan, sem Android, sem I/O.
#pragma once

#include "core/base.h"

#include <algorithm>
#include <cmath>

namespace ae::ui {

// Alinhamento dentro de um espaço maior que o conteúdo. Vive aqui, e não no
// layout, porque texto, ícone e caixa usam o mesmo vocabulário -- e um segundo
// enum com os mesmos quatro nomes acabaria convertido na mão em algum lugar.
enum class UiAlign : u8 { Start, Center, End, Stretch };

struct UiPoint final {
  float x = 0.0f;
  float y = 0.0f;
};

// Retângulo por canto e tamanho, nunca por dois cantos: um tamanho negativo é
// representável e detectável, enquanto `right < left` se confunde com "vazio".
struct UiRect final {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  constexpr float right() const noexcept { return x + width; }
  constexpr float bottom() const noexcept { return y + height; }
  constexpr bool isEmpty() const noexcept { return width <= 0.0f || height <= 0.0f; }
  constexpr bool contains(UiPoint point) const noexcept {
    // Borda superior/esquerda pertence ao retângulo e a inferior/direita não:
    // dois painéis encostados nunca reivindicam o mesmo pixel de toque.
    return point.x >= x && point.x < right() && point.y >= y && point.y < bottom();
  }
};

inline bool isFinite(const UiRect &rect) noexcept {
  return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) &&
         std::isfinite(rect.height);
}

// Interseção. Retângulos disjuntos devolvem um retângulo de tamanho zero na
// posição do canto do primeiro, nunca larguras negativas — quem recebe o
// resultado testa `isEmpty()` e não precisa saber como a disjunção aconteceu.
inline UiRect intersect(const UiRect &first, const UiRect &second) noexcept {
  const float left = std::max(first.x, second.x);
  const float top = std::max(first.y, second.y);
  const float right = std::min(first.right(), second.right());
  const float bottom = std::min(first.bottom(), second.bottom());
  return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
}

// Margens internas. `left/top/right/bottom` e não `horizontal/vertical` porque
// a barra do editor tem recuo assimétrico por causa do recorte da câmera.
struct UiInsets final {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  static constexpr UiInsets all(float value) noexcept { return {value, value, value, value}; }
  static constexpr UiInsets symmetric(float horizontal, float vertical) noexcept {
    return {horizontal, vertical, horizontal, vertical};
  }
  constexpr float horizontal() const noexcept { return left + right; }
  constexpr float vertical() const noexcept { return top + bottom; }
};

// Encolhe o retângulo pelas margens. Nunca devolve tamanho negativo: um painel
// mais estreito que seu próprio padding vira um conteúdo de largura zero, e não
// um retângulo invertido que a etapa seguinte desenharia ao contrário.
inline UiRect deflate(const UiRect &rect, const UiInsets &insets) noexcept {
  return {rect.x + insets.left, rect.y + insets.top,
          std::max(0.0f, rect.width - insets.horizontal()),
          std::max(0.0f, rect.height - insets.vertical())};
}

inline UiRect inflate(const UiRect &rect, float amount) noexcept {
  return {rect.x - amount, rect.y - amount, std::max(0.0f, rect.width + amount * 2.0f),
          std::max(0.0f, rect.height + amount * 2.0f)};
}

// Área mínima de toque. O plano exige 48 dp; um botão desenhado menor continua
// desenhado menor, mas a área que responde ao dedo cresce em volta do centro.
// Isso é o oposto de aumentar o desenho: a densidade visual do editor depende
// de controles pequenos, e a acessibilidade depende de alvos grandes.
inline UiRect expandToMinimumTouchTarget(const UiRect &rect, float minimumSide) noexcept {
  const float growX = std::max(0.0f, minimumSide - rect.width) * 0.5f;
  const float growY = std::max(0.0f, minimumSide - rect.height) * 0.5f;
  return {rect.x - growX, rect.y - growY, rect.width + growX * 2.0f, rect.height + growY * 2.0f};
}

} // namespace ae::ui
