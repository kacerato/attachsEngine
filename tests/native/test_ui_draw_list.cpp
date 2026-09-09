#include "harness.h"
#include "ui/ui_draw_list.h"
#include "ui/ui_text.h"

#include <cmath>

using namespace ae;
using namespace ae::ui;

namespace {

const UiRect kScreen{0, 0, 1600, 900};

UiDrawList beginList() {
  UiDrawList list;
  list.begin(kScreen, fallbackFontMetrics());
  return list;
}

} // namespace

AE_TEST(draw_list_records_commands_in_submission_order) {
  // A ordem é a profundidade: o painel é desenhado antes do rótulo que vive
  // dentro dele, e o consumidor não reordena nada.
  UiDrawList list = beginList();
  const UiTheme &theme = defaultTheme();
  list.addRect({0, 0, 400, 100}, theme.color.surface, theme.radius.card);
  list.addText({12, 12, 200, 20}, "HIERARCHY", theme.color.text, theme.type.label);
  AE_EXPECT_EQ(list.commandCount(), 2u, "");
  AE_EXPECT_TRUE(list.commands()[0].kind == UiPrimitive::Rect, "");
  AE_EXPECT_TRUE(list.commands()[1].kind == UiPrimitive::Text, "");
  AE_EXPECT_TRUE(list.textOf(list.commands()[1]) == "HIERARCHY", "");
}

AE_TEST(draw_list_clips_extreme_projected_lines_before_gpu_interpolation) {
  UiDrawList list=beginList();list.pushClip({100,100,300,200});
  AE_EXPECT_TRUE(list.addLine({-10000000,200},{10000000,200},0xffffffff,1),"crossing line accepted");
  const auto &line=list.commands().back();
  AE_EXPECT_TRUE(line.atlas.x>=98 && line.atlas.width<=402,"GPU endpoints stay near viewport");
  AE_EXPECT_TRUE(!list.addLine({-10000000,0},{10000000,0},0xffffffff,1),"fully outside line rejected");
}

AE_TEST(draw_list_clip_is_baked_into_each_command) {
  UiDrawList list = beginList();
  AE_EXPECT_TRUE(list.pushClip({100, 100, 200, 200}), "");
  list.addRect({0, 0, 1000, 1000}, 0xFF112233);
  list.popClip();
  AE_EXPECT_EQ(list.commandCount(), 1u, "");
  const UiDrawCommand &command = list.commands()[0];
  AE_EXPECT_TRUE(command.clip.x == 100.0f && command.clip.width == 200.0f,
                 "o consumidor nao precisa reconstruir a pilha");
}

AE_TEST(draw_list_nested_clips_intersect_and_never_widen) {
  // Um filho não pode desenhar fora do pai: senão qualquer erro de layout vira
  // painel vazando por cima da cena.
  UiDrawList list = beginList();
  AE_EXPECT_TRUE(list.pushClip({100, 100, 200, 200}), "");
  AE_EXPECT_TRUE(list.pushClip({0, 0, 1600, 900}), "");
  AE_EXPECT_TRUE(list.currentClip().x == 100.0f && list.currentClip().width == 200.0f,
                 "o recorte interno nao pode alargar o externo");
  list.popClip();
  AE_EXPECT_TRUE(list.currentClip().width == 200.0f, "");
  list.popClip();
  AE_EXPECT_TRUE(list.currentClip().width == 1600.0f, "voltou ao viewport");
}

AE_TEST(draw_list_pop_below_the_root_is_harmless) {
  UiDrawList list = beginList();
  list.popClip();
  list.popClip();
  AE_EXPECT_TRUE(list.currentClip().width == 1600.0f,
                 "desbalancear a pilha nao pode apagar o recorte da tela");
}

AE_TEST(draw_list_culls_what_falls_entirely_outside_the_clip) {
  UiDrawList list = beginList();
  list.pushClip({0, 0, 100, 100});
  list.addRect({500, 500, 10, 10}, 0xFFFFFFFF);
  list.addRect({50, 50, 10, 10}, 0xFFFFFFFF);
  list.popClip();
  AE_EXPECT_EQ(list.commandCount(), 1u, "so o que intersecta o recorte vira geometria");
  AE_EXPECT_EQ(list.culledCommandCount(), 1u,
               "e o descartado e contado: lista inteira recortada e erro de layout");
}

AE_TEST(draw_list_border_and_fill_are_separate_commands) {
  // O asset selecionado do mockup tem fundo escuro e contorno lima. São dois
  // comandos porque a borda não presume que alguém pintou o fundo.
  UiDrawList list = beginList();
  const UiTheme &theme = defaultTheme();
  list.addRect({0, 0, 120, 120}, theme.color.raised, theme.radius.thumb);
  list.addBorder({0, 0, 120, 120}, theme.color.accent, 2.0f, theme.radius.thumb);
  AE_EXPECT_EQ(list.commandCount(), 2u, "");
  AE_EXPECT_EQ(list.commands()[1].borderWidth, 2.0f, "");
  AE_EXPECT_EQ(list.commands()[1].color, 0u, "a borda nao pinta o interior");
}

AE_TEST(draw_list_gradient_reports_both_stops) {
  // O slider de temperatura mostra a propria escala que edita.
  UiDrawList list = beginList();
  list.addGradientRect({0, 0, 200, 8}, 0xFF4488FF, 0xFFFF8844, 4.0f);
  const UiDrawCommand &command = list.commands()[0];
  AE_EXPECT_EQ(command.color, 0xFF4488FFu, "");
  AE_EXPECT_EQ(command.gradientEnd, 0xFFFF8844u, "");
}

AE_TEST(draw_list_solid_rect_reports_identical_stops) {
  // Sem sinalizador separado: quem desenha compara os dois e sabe o que fazer.
  UiDrawList list = beginList();
  list.addRect({0, 0, 10, 10}, 0xFF123456);
  AE_EXPECT_EQ(list.commands()[0].color, list.commands()[0].gradientEnd, "");
}

AE_TEST(draw_list_rejects_malformed_input_instead_of_drawing_it) {
  UiDrawList list = beginList();
  AE_EXPECT_TRUE(!list.addRect({0, 0, 10, 10}, 0xFFFFFFFF, -1.0f), "raio negativo");
  AE_EXPECT_TRUE(!list.addBorder({0, 0, 10, 10}, 0xFFFFFFFF, 0.0f), "borda de espessura zero");
  AE_EXPECT_TRUE(!list.addImage({0, 0, 10, 10}, kUiNoImage),
                 "id zero desenharia o primeiro slot do atlas por acidente");
  UiTypeStyle broken{};
  broken.size = 0.0f;
  AE_EXPECT_TRUE(!list.addText({0, 0, 10, 10}, "x", 0xFFFFFFFF, broken), "corpo zero");
  AE_EXPECT_EQ(list.commandCount(), 0u, "nada malformado entrou na lista");
}

AE_TEST(draw_list_empty_text_is_accepted_and_emits_nothing) {
  // Um rótulo vazio é um estado normal da interface (campo sem valor), não um
  // erro do chamador.
  UiDrawList list = beginList();
  AE_EXPECT_TRUE(list.addText({0, 0, 10, 10}, "", 0xFFFFFFFF, defaultTheme().type.body), "");
  AE_EXPECT_EQ(list.commandCount(), 0u, "");
}

AE_TEST(draw_list_begin_resets_everything) {
  UiDrawList list = beginList();
  list.pushClip({0, 0, 10, 10});
  list.addRect({0, 0, 5, 5}, 0xFFFFFFFF);
  list.addRect({500, 500, 5, 5}, 0xFFFFFFFF);
  list.begin(kScreen, fallbackFontMetrics());
  AE_EXPECT_EQ(list.commandCount(), 0u, "");
  AE_EXPECT_EQ(list.culledCommandCount(), 0u, "");
  AE_EXPECT_TRUE(list.currentClip().width == 1600.0f, "a pilha de recorte volta ao viewport");
}

AE_TEST(draw_list_text_arena_survives_many_commands) {
  UiDrawList list = beginList();
  const UiTypeStyle &style = defaultTheme().type.body;
  list.addText({0, 0, 100, 20}, "Position", 0xFFFFFFFF, style);
  list.addText({0, 20, 100, 20}, "Rotation", 0xFFFFFFFF, style);
  list.addText({0, 40, 100, 20}, "Scale", 0xFFFFFFFF, style);
  AE_EXPECT_TRUE(list.textOf(list.commands()[0]) == "Position", "");
  AE_EXPECT_TRUE(list.textOf(list.commands()[1]) == "Rotation", "");
  AE_EXPECT_TRUE(list.textOf(list.commands()[2]) == "Scale",
                 "fatias diferentes do mesmo arena nao se misturam");
}

AE_TEST(text_measure_scales_with_body_and_adds_tracking) {
  const UiFontMetrics &metrics = fallbackFontMetrics();
  UiTypeStyle style{};
  style.size = 10.0f;
  style.tracking = 0.0f;
  // A métrica de reserva é meio em por glifo: quatro glifos de corpo 10 = 20.
  AE_EXPECT_TRUE(std::fabs(measureTextWidth("Play", metrics, style) - 20.0f) < 0.01f, "");
  style.size = 20.0f;
  AE_EXPECT_TRUE(std::fabs(measureTextWidth("Play", metrics, style) - 40.0f) < 0.01f,
                 "dobrar o corpo dobra a largura");
  style.size = 10.0f;
  style.tracking = 0.1f;
  AE_EXPECT_TRUE(std::fabs(measureTextWidth("Play", metrics, style) - 24.0f) < 0.01f,
                 "tracking e fracao do corpo, somada por glifo");
}

AE_TEST(text_measure_of_an_unknown_codepoint_uses_the_declared_fallback) {
  // Um nome de objeto acentuado não pode fazer a caixa medir zero e o painel
  // colapsar. Ele mede pelo fallback, que é um número honesto e não o certo.
  UiFontMetrics metrics = fallbackFontMetrics();
  UiTypeStyle style{};
  style.size = 10.0f;
  const float known = measureTextWidth("aa", metrics, style);
  const char accented[] = {static_cast<char>(0xC3), static_cast<char>(0xA1), '\0'};
  const float unknown = measureTextWidth(accented, metrics, style);
  AE_EXPECT_TRUE(known > 0.0f && unknown > 0.0f, "nenhuma das duas mede zero");
}

AE_TEST(text_truncation_returns_the_largest_prefix_that_fits) {
  const UiFontMetrics &metrics = fallbackFontMetrics();
  UiTypeStyle style{};
  style.size = 10.0f;
  // Cinco pixels por glifo: 22 pixels comportam quatro.
  AE_EXPECT_EQ(truncateToWidth("Concrete Block", metrics, style, 22.0f), usize{4}, "");
  AE_EXPECT_EQ(truncateToWidth("abc", metrics, style, 1000.0f), usize{3}, "cabendo tudo, tudo");
  AE_EXPECT_EQ(truncateToWidth("abc", metrics, style, 0.0f), usize{0}, "sem largura, nada");
}

AE_TEST(text_baseline_places_the_cap_height_where_the_master_measured_it) {
  UiFontMetrics metrics = fallbackFontMetrics();
  metrics.capHeight = 0.72f;
  UiTypeStyle style{};
  style.size = 100.0f;
  AE_EXPECT_TRUE(std::fabs(baselineForCapTop(metrics, style, 10.0f) - 82.0f) < 0.01f,
                 "a linha de base fica uma altura de caixa alta abaixo do topo pedido");
}

AE_TEST(geometry_touch_target_grows_around_the_centre) {
  const UiRect drawn{100, 100, 20, 20};
  const UiRect target = expandToMinimumTouchTarget(drawn, 48.0f);
  AE_EXPECT_TRUE(std::fabs(target.width - 48.0f) < 0.01f, "");
  AE_EXPECT_TRUE(std::fabs(target.x - 86.0f) < 0.01f, "cresce igual dos dois lados");
  const UiRect large{0, 0, 60, 60};
  AE_EXPECT_TRUE(expandToMinimumTouchTarget(large, 48.0f).width == 60.0f,
                 "o que ja e grande o bastante nao encolhe nem cresce");
}

AE_TEST(geometry_rect_edges_belong_to_exactly_one_rect) {
  // Dois painéis encostados não podem disputar o mesmo pixel de toque.
  const UiRect left{0, 0, 100, 100};
  const UiRect right{100, 0, 100, 100};
  const UiPoint seam{100.0f, 50.0f};
  AE_EXPECT_TRUE(!left.contains(seam), "a borda direita nao pertence ao da esquerda");
  AE_EXPECT_TRUE(right.contains(seam), "e pertence ao da direita");
}

AE_TEST(geometry_disjoint_intersection_is_empty_not_negative) {
  const UiRect result = intersect({0, 0, 10, 10}, {100, 100, 10, 10});
  AE_EXPECT_TRUE(result.isEmpty(), "");
  AE_EXPECT_TRUE(result.width >= 0.0f && result.height >= 0.0f,
                 "largura negativa desenharia ao contrario em algum estagio");
}

AE_TEST(geometry_deflate_never_inverts_a_small_rect) {
  const UiRect result = deflate({0, 0, 10, 10}, UiInsets::all(20));
  AE_EXPECT_TRUE(result.width == 0.0f && result.height == 0.0f,
                 "painel mais estreito que o proprio padding tem conteudo de largura zero");
}
