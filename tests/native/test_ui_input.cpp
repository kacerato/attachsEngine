#include "harness.h"
#include "ui/ui_input.h"

using namespace ae;
using namespace ae::ui;

namespace {

constexpr u32 kPlayButton = 101;
constexpr u32 kMoveTool = 202;

UiPointerEvent down(u32 pointerId, float x, float y) {
  return {pointerId, UiPointerPhase::Down, {x, y}, 0.0};
}
UiPointerEvent move(u32 pointerId, float x, float y) {
  return {pointerId, UiPointerPhase::Move, {x, y}, 0.0};
}
UiPointerEvent up(u32 pointerId, float x, float y) {
  return {pointerId, UiPointerPhase::Up, {x, y}, 0.0};
}

// A forma do frame do editor: dock com um botão, e o resto é cena.
void buildEditorFrame(UiInputRouter &router) {
  router.beginFrame();
  router.addRegion({700, 820, 120, 60}, kPlayButton);
  router.addRegion({20, 200, 64, 64}, kMoveTool);
}

} // namespace

AE_TEST(input_touch_outside_every_region_belongs_to_the_scene) {
  UiInputRouter router;
  buildEditorFrame(router);
  const UiPointerRouting routing = router.route(down(1, 640, 400));
  AE_EXPECT_TRUE(routing.target == UiPointerTarget::Viewport,
                 "o vazio da tela e da cena, nao da interface");
  AE_EXPECT_EQ(router.viewportPointerCount(), 1u, "");
}

AE_TEST(input_touch_on_a_widget_is_captured_by_it) {
  UiInputRouter router;
  buildEditorFrame(router);
  const UiPointerRouting routing = router.route(down(1, 760, 850));
  AE_EXPECT_TRUE(routing.target == UiPointerTarget::Widget, "");
  AE_EXPECT_EQ(routing.widgetId, kPlayButton, "");
  u32 pressed = 0;
  AE_EXPECT_TRUE(router.pressedWidget(pressed) && pressed == kPlayButton,
                 "o estado visual de pressionado segue a captura");
}

AE_TEST(input_topmost_region_wins_when_two_overlap) {
  // O card do Inspector fica por cima do viewport e, no mockup do Add, por cima
  // do proprio rail. A ordem de desenho e a ordem de profundidade.
  UiInputRouter router;
  router.beginFrame();
  router.addRegion({0, 0, 500, 500}, 1);
  router.addRegion({100, 100, 100, 100}, 2);
  const UiPointerRouting routing = router.route(down(1, 150, 150));
  AE_EXPECT_EQ(routing.widgetId, 2u, "a ultima registrada esta por cima");
}

AE_TEST(input_touch_expansion_never_steals_a_touch_inside_a_neighbour) {
  // Linhas de 24 com alvo mínimo de 44, como a aba Estrutura do importador:
  // a expansão da linha de baixo cobria a metade de baixo da de cima, e o
  // toque no meio de uma linha pegava a vizinha (visto no aparelho).
  UiInputRouter router;
  router.beginFrame();
  for (u32 row = 0; row < 3; ++row)
    router.addRegion({0, static_cast<float>(row) * 24.0f, 200, 24}, 10 + row, 44.0f);
  for (u32 row = 0; row < 3; ++row)
    for (const float offset : {2.0f, 12.0f, 22.0f})
      AE_EXPECT_EQ(router.hitTest({100, static_cast<float>(row) * 24.0f + offset}).widgetId, 10 + row,
                   "o toque dentro da linha é da própria linha");
  // Fora de todas as linhas, a expansão continua ajudando o dedo.
  AE_EXPECT_EQ(router.hitTest({100, 80}).widgetId, 12u, "logo abaixo da última linha, a expansão dela vale");

  // Um bloqueador por cima continua encerrando a busca: o que está embaixo
  // dele não recebe o toque, nem pela área exata.
  router.addBlocker({0, 0, 200, 30});
  AE_EXPECT_TRUE(router.hitTest({100, 12}).target == UiPointerTarget::None, "o bloqueador por cima vence");
}

AE_TEST(input_a_drag_that_starts_on_a_widget_never_becomes_a_camera_orbit) {
  // Este é o defeito que o módulo existe para impedir: arrastar um slider e ver
  // a câmera girar assim que o dedo sai do painel.
  UiInputRouter router;
  buildEditorFrame(router);
  AE_EXPECT_TRUE(router.route(down(1, 760, 850)).target == UiPointerTarget::Widget, "");

  buildEditorFrame(router); // proximo frame, mesmas regioes
  const UiPointerRouting dragged = router.route(move(1, 300, 400));
  AE_EXPECT_TRUE(dragged.target == UiPointerTarget::Widget,
                 "o dedo saiu do botao mas continua pertencendo a ele");
  AE_EXPECT_EQ(dragged.widgetId, kPlayButton, "");
  AE_EXPECT_TRUE(dragged.dragging, "passou do limiar, entao e arraste");
  AE_EXPECT_EQ(router.viewportPointerCount(), 0u, "a cena nunca recebeu este dedo");
}

AE_TEST(input_a_drag_that_starts_on_the_scene_is_not_stolen_by_a_panel) {
  // O caminho inverso: orbitar a câmera e passar o dedo por cima do Inspector
  // não pode interromper a órbita.
  UiInputRouter router;
  buildEditorFrame(router);
  AE_EXPECT_TRUE(router.route(down(1, 400, 400)).target == UiPointerTarget::Viewport, "");
  buildEditorFrame(router);
  const UiPointerRouting routing = router.route(move(1, 760, 850));
  AE_EXPECT_TRUE(routing.target == UiPointerTarget::Viewport, "a orbita continua sendo da cena");
}

AE_TEST(input_tap_requires_no_drag_and_a_release_inside_the_widget) {
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  buildEditorFrame(router);
  const UiPointerRouting tapped = router.route(up(1, 762, 852));
  AE_EXPECT_TRUE(tapped.tapped, "toque curto dentro do botao e clique");
  AE_EXPECT_TRUE(tapped.released, "");
  AE_EXPECT_EQ(router.activePointerCount(), 0u, "o ponteiro foi liberado");

  // Sair da area antes de levantar e o gesto universal de desistir.
  buildEditorFrame(router);
  router.route(down(2, 760, 850));
  buildEditorFrame(router);
  const UiPointerRouting cancelled = router.route(up(2, 300, 300));
  AE_EXPECT_TRUE(!cancelled.tapped, "soltar fora do botao nao clica");
  AE_EXPECT_TRUE(cancelled.released, "mas encerra a captura");
}

AE_TEST(input_a_finger_that_wandered_and_came_back_is_still_a_drag) {
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  buildEditorFrame(router);
  AE_EXPECT_TRUE(router.route(move(1, 900, 850)).dragging, "");
  buildEditorFrame(router);
  const UiPointerRouting back = router.route(up(1, 760, 850));
  AE_EXPECT_TRUE(!back.tapped,
                 "voltar ao ponto inicial nao desfaz o arraste: o slider ja se moveu");
}

AE_TEST(input_slop_separates_a_tap_from_a_drag) {
  UiInputRouter router;
  router.setDragSlop(10.0f);
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  buildEditorFrame(router);
  AE_EXPECT_TRUE(!router.route(move(1, 765, 850)).dragging, "cinco pixels ainda e toque");
  buildEditorFrame(router);
  AE_EXPECT_TRUE(router.route(move(1, 780, 850)).dragging, "vinte pixels e arraste");
}

AE_TEST(input_total_delta_is_measured_from_the_touch_not_from_the_last_step) {
  // Um arraste de gizmo aplica o deslocamento total sobre a pose inicial. Somar
  // passos acumularia erro de arredondamento ao longo de centenas de frames.
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 100, 100));
  buildEditorFrame(router);
  const UiPointerRouting first = router.route(move(1, 130, 110));
  AE_EXPECT_TRUE(first.totalDelta.x == 30.0f && first.totalDelta.y == 10.0f, "");
  AE_EXPECT_TRUE(first.stepDelta.x == 30.0f && first.stepDelta.y == 10.0f, "");
  buildEditorFrame(router);
  const UiPointerRouting second = router.route(move(1, 150, 90));
  AE_EXPECT_TRUE(second.totalDelta.x == 50.0f && second.totalDelta.y == -10.0f,
                 "total continua contado desde o Down");
  AE_EXPECT_TRUE(second.stepDelta.x == 20.0f && second.stepDelta.y == -20.0f,
                 "e o passo, desde o evento anterior");
}

AE_TEST(input_a_blocker_absorbs_the_touch_without_giving_it_to_the_scene) {
  // O espaço vazio de um painel opaco: nada acontece, e principalmente a câmera
  // não orbita por trás da interface.
  UiInputRouter router;
  router.beginFrame();
  router.addBlocker({1200, 56, 400, 772});
  router.addRegion({1220, 100, 100, 40}, 7);
  AE_EXPECT_TRUE(router.route(down(1, 1300, 400)).target == UiPointerTarget::None,
                 "o painel absorve o toque");
  AE_EXPECT_EQ(router.viewportPointerCount(), 0u, "");
  router.route(up(1, 1300, 400));

  router.beginFrame();
  router.addBlocker({1200, 56, 400, 772});
  router.addRegion({1220, 100, 100, 40}, 7);
  AE_EXPECT_EQ(router.route(down(2, 1250, 120)).widgetId, 7u,
               "um widget dentro do painel continua recebendo");
}

AE_TEST(input_minimum_touch_target_grows_the_area_around_the_drawing) {
  // O olho de visibilidade da hierarquia é desenhado pequeno; a área que
  // responde ao dedo tem de ser a mínima de acessibilidade.
  UiInputRouter router;
  router.beginFrame();
  router.addRegion({100, 100, 20, 20}, 5, 48.0f);
  AE_EXPECT_EQ(router.route(down(1, 90, 102)).widgetId, 5u,
               "fora do desenho, dentro do alvo minimo");
  router.route(up(1, 90, 102));
  router.beginFrame();
  router.addRegion({100, 100, 20, 20}, 5, 48.0f);
  AE_EXPECT_TRUE(router.route(down(2, 60, 102)).target == UiPointerTarget::Viewport,
                 "bem longe continua sendo cena");
}

AE_TEST(input_two_fingers_on_the_scene_are_both_reported) {
  // Pinçar e deslocar precisam saber que há dois dedos na cena; interpretar o
  // gesto é da câmera, contar é daqui.
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 400, 400));
  router.route(down(2, 600, 500));
  AE_EXPECT_EQ(router.viewportPointerCount(), 2u, "");
  router.route(up(1, 400, 400));
  AE_EXPECT_EQ(router.viewportPointerCount(), 1u, "levantar um nao cancela o outro");
  AE_EXPECT_TRUE(router.isPointerActive(2), "");
}

AE_TEST(input_events_for_an_unknown_pointer_are_ignored) {
  UiInputRouter router;
  buildEditorFrame(router);
  const UiPointerRouting orphan = router.route(move(9, 400, 400));
  AE_EXPECT_TRUE(orphan.target == UiPointerTarget::None,
                 "inventar um alvo aqui moveria a camera sem que ninguem tocasse nela");
  AE_EXPECT_EQ(router.activePointerCount(), 0u, "");
}

AE_TEST(input_a_repeated_down_replaces_the_stale_capture) {
  // Acontece quando o sistema perde um Up (troca de superfície, por exemplo).
  // Manter as duas capturas deixaria uma orfã que nunca mais seria liberada.
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  buildEditorFrame(router);
  const UiPointerRouting again = router.route(down(1, 400, 400));
  AE_EXPECT_TRUE(again.target == UiPointerTarget::Viewport, "");
  AE_EXPECT_EQ(router.activePointerCount(), 1u, "so uma captura para o mesmo dedo");
}

AE_TEST(input_cancel_releases_without_clicking) {
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  buildEditorFrame(router);
  const UiPointerRouting cancelled =
      router.route({1, UiPointerPhase::Cancel, {760.0f, 850.0f}, 0.0});
  AE_EXPECT_TRUE(cancelled.released, "");
  AE_EXPECT_TRUE(!cancelled.tapped, "cancelamento do sistema nunca vira clique");
  AE_EXPECT_EQ(router.activePointerCount(), 0u, "");
}

AE_TEST(input_cancel_all_clears_captures_on_rotation_or_focus_loss) {
  UiInputRouter router;
  buildEditorFrame(router);
  router.route(down(1, 760, 850));
  router.route(down(2, 400, 400));
  router.cancelAllPointers();
  AE_EXPECT_EQ(router.activePointerCount(), 0u,
               "um arraste nao pode atravessar a rotacao da tela");
}

AE_TEST(input_regions_are_rejected_when_malformed) {
  UiInputRouter router;
  router.beginFrame();
  AE_EXPECT_TRUE(!router.addRegion({0, 0, 0, 40}, 1), "regiao sem area nunca receberia toque");
  AE_EXPECT_TRUE(!router.addRegion({0, 0, -10, 40}, 1), "");
  AE_EXPECT_TRUE(router.addRegion({0, 0, 10, 40}, 1), "");
}
