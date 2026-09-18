// Roteamento de toque da interface do editor.
//
// O plano chama a resolução de conflito de gestos de "o problema mais
// subestimado" (3.2.1), e a razão é esta: o mesmo dedo, no mesmo pixel, pode
// querer apertar um botão, arrastar um eixo do gizmo ou orbitar a câmera. A
// regra que este módulo implementa é a única que não exige adivinhação:
//
//   **quem recebe o `Down` fica com o ponteiro até o `Up`.**
//
// A decisão acontece uma vez, no toque, contra as regiões daquele frame. Depois
// disso o alvo não muda, mesmo que o dedo saia da região, que o painel se mova
// ou que a região deixe de existir. Um roteamento que reavaliasse a cada
// movimento faria um arraste iniciado num slider "pular" para a câmera ao
// passar por cima do viewport — que é exatamente o defeito que todo editor
// mobile ruim tem.
//
// O módulo não interpreta gestos: ele diz de quem é o dedo e quanto ele andou.
// Orbitar, pinçar e arrastar eixo são decisões de quem recebe o ponteiro.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"

#include <vector>

namespace ae::ui {

enum class UiPointerPhase : u8 { Down, Move, Up, Cancel };

struct UiPointerEvent final {
  // Identidade estável do dedo, vinda do sistema. Não é o índice na lista de
  // toques: esse muda quando um dedo do meio é levantado.
  u32 pointerId = 0;
  UiPointerPhase phase = UiPointerPhase::Down;
  UiPoint position{};
  double timeSeconds = 0.0;
};

// Para onde o ponteiro foi despachado. `Viewport` é o alvo de fallback: tudo
// que não é interface pertence à cena, e é a cena que decide entre orbitar,
// selecionar e arrastar um gizmo.
enum class UiPointerTarget : u8 { None, Widget, Viewport };

struct UiPointerRouting final {
  UiPointerTarget target = UiPointerTarget::None;
  // Identificador do widget que capturou o ponteiro. Só tem sentido com
  // `target == Widget`; zero é um id válido, então nunca use ele como sentinela.
  u32 widgetId = 0;
  UiPoint start{};
  UiPoint position{};
  // Deslocamento desde o `Down`, não desde o evento anterior. É o que um
  // arraste de gizmo precisa: aplicar somas de passos acumularia erro.
  UiPoint totalDelta{};
  // Deslocamento desde o evento anterior deste mesmo ponteiro.
  UiPoint stepDelta{};
  // Verdadeiro depois que o movimento passou do limiar. Uma vez verdadeiro,
  // continua verdadeiro até o `Up`: um dedo que volta ao ponto inicial não
  // vira toque de novo.
  bool dragging = false;
  // Verdadeiro exatamente no evento `Up` que fecha um toque sem arraste dentro
  // da região do widget. É o sinal de "clique" — e ele não existe para o
  // viewport, cuja interpretação de toque curto é seleção e pertence à cena.
  bool tapped = false;
  // Verdadeiro no `Up`/`Cancel` que encerra a captura, com ou sem arraste.
  bool released = false;
};

class UiInputRouter final {
public:
  // Teto de regiões por frame. Existe pela mesma razão do teto de nós do layout:
  // um laço de construção com defeito deve falhar, não crescer sem limite.
  static constexpr u32 kMaximumRegions = 2048;

  // Zera as regiões do frame. Os ponteiros capturados NÃO são zerados: um dedo
  // que desceu no frame anterior continua pertencendo a quem o capturou.
  void beginFrame() noexcept;

  // Registra uma região clicável. A ordem é a de desenho: a última registrada
  // está por cima e é testada primeiro. `minimumTouchSide` cresce a área de
  // toque em volta do centro sem mexer no desenho — ver
  // expandToMinimumTouchTarget.
  bool addRegion(const UiRect &rect, u32 widgetId, float minimumTouchSide = 0.0f);

  // Bloqueia tudo que estiver por baixo, na área dada. Um painel opaco registra
  // isto para que um toque no espaço vazio dele não caia no viewport e vire uma
  // órbita de câmera por trás da própria interface.
  bool addBlocker(const UiRect &rect);

  UiPointerRouting route(const UiPointerEvent &event) noexcept;
  // Read-only hit test for drop targets; does not transfer pointer capture.
  UiPointerRouting hitTest(UiPoint position) const noexcept;

  // Widget que está com algum dedo em cima agora, para o estado visual de
  // pressionado. Devolve false quando nenhum widget está capturado.
  bool pressedWidget(u32 &outWidgetId) const noexcept;
  bool isPointerActive(u32 pointerId) const noexcept;
  u32 activePointerCount() const noexcept { return static_cast<u32>(pointers_.size()); }
  // Quantos dedos estão no viewport. É o que distingue orbitar (um) de pinçar
  // e deslocar (dois) sem que este módulo precise conhecer nenhum dos dois.
  u32 viewportPointerCount() const noexcept;

  void setDragSlop(float slop) noexcept;
  float dragSlop() const noexcept { return dragSlop_; }
  // Esquece todos os ponteiros. Chamado quando a janela perde o foco ou o
  // aparelho gira: manter uma captura viva atravessando isso deixaria um
  // arraste fantasma preso num widget que já não existe.
  void cancelAllPointers() noexcept;

private:
  struct Region final {
    UiRect rect{};
    // A área desenhada, antes da expansão para o alvo mínimo de toque.
    UiRect exact{};
    u32 widgetId = 0;
    bool blocker = false;
  };
  struct ActivePointer final {
    u32 pointerId = 0;
    UiPointerTarget target = UiPointerTarget::None;
    u32 widgetId = 0;
    UiPoint start{};
    UiPoint previous{};
    bool dragging = false;
  };

  const ActivePointer *find(u32 pointerId) const noexcept;
  ActivePointer *find(u32 pointerId) noexcept;
  void erase(u32 pointerId) noexcept;

  std::vector<Region> regions_;
  std::vector<ActivePointer> pointers_;
  float dragSlop_ = 10.0f;
};

} // namespace ae::ui
