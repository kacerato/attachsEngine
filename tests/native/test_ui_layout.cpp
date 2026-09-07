#include "harness.h"
#include "ui/ui_layout.h"

#include <cmath>

using namespace ae;
using namespace ae::ui;

namespace {

bool nearlyEqual(float first, float second, float tolerance = 0.01f) {
  return std::fabs(first - second) <= tolerance;
}

bool rectEquals(const UiRect &rect, float x, float y, float width, float height) {
  return nearlyEqual(rect.x, x) && nearlyEqual(rect.y, y) && nearlyEqual(rect.width, width) &&
         nearlyEqual(rect.height, height);
}

UiBoxDesc row(UiLength width, UiLength height) {
  UiBoxDesc desc{};
  desc.axis = UiAxis::Horizontal;
  desc.width = width;
  desc.height = height;
  return desc;
}

UiBoxDesc column(UiLength width, UiLength height) {
  UiBoxDesc desc = row(width, height);
  desc.axis = UiAxis::Vertical;
  return desc;
}

UiBoxDesc leaf(float contentWidth, float contentHeight) {
  UiBoxDesc desc{};
  desc.width = UiLength::hug();
  desc.height = UiLength::hug();
  desc.contentWidth = contentWidth;
  desc.contentHeight = contentHeight;
  return desc;
}

} // namespace

AE_TEST(layout_root_grow_fills_the_viewport) {
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::grow(), UiLength::grow()));
  AE_EXPECT_TRUE(root == 0, "a raiz e sempre o no zero");
  AE_EXPECT_TRUE(tree.resolve({0, 0, 1600, 900}), "");
  AE_EXPECT_TRUE(rectEquals(tree.rect(root), 0, 0, 1600, 900), "raiz elastica ocupa o viewport");
}

AE_TEST(layout_root_hug_keeps_its_measured_size) {
  // Um painel flutuante — o card do Inspector do mockup — é uma raiz Hug: ele
  // não deve crescer até a tela só porque foi resolvido contra ela.
  UiLayoutTree tree;
  UiBoxDesc desc = column(UiLength::hug(), UiLength::hug());
  desc.padding = UiInsets::all(12);
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, desc);
  tree.addNode(root, leaf(200, 40));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 1600, 900}), "");
  AE_EXPECT_TRUE(rectEquals(tree.rect(root), 0, 0, 224, 64), "conteudo mais o padding dos dois lados");
}

AE_TEST(layout_splits_leftover_between_equal_weights) {
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::grow(), UiLength::grow()));
  const u32 first = tree.addNode(root, row(UiLength::grow(), UiLength::grow()));
  const u32 second = tree.addNode(root, row(UiLength::grow(), UiLength::grow()));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 1000, 100}), "");
  AE_EXPECT_TRUE(rectEquals(tree.rect(first), 0, 0, 500, 100), "metade para o primeiro");
  AE_EXPECT_TRUE(rectEquals(tree.rect(second), 500, 0, 500, 100), "metade para o segundo");
}

AE_TEST(layout_splits_leftover_proportionally_to_weight) {
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::grow(), UiLength::grow()));
  const u32 first = tree.addNode(root, row(UiLength::grow(3.0f), UiLength::grow()));
  const u32 second = tree.addNode(root, row(UiLength::grow(1.0f), UiLength::grow()));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 800, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(first).width, 600), "peso 3 leva tres quartos");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(second).width, 200), "peso 1 leva um quarto");
}

AE_TEST(layout_editor_shell_places_viewport_between_bars) {
  // A forma real da tela do editor: barra superior fixa, corpo elástico e dock
  // inferior fixa. É o caso que qualquer regressão de distribuição quebraria
  // primeiro, e é o que decide onde o toque da câmera começa a valer.
  UiLayoutTree tree;
  UiBoxDesc shell = column(UiLength::grow(), UiLength::grow());
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, shell);
  const u32 topBar = tree.addNode(root, row(UiLength::grow(), UiLength::fixed(56)));
  const u32 body = tree.addNode(root, row(UiLength::grow(), UiLength::grow()));
  const u32 dock = tree.addNode(root, row(UiLength::grow(), UiLength::fixed(72)));

  const u32 hierarchy = tree.addNode(body, column(UiLength::fixed(320), UiLength::grow()));
  const u32 viewport = tree.addNode(body, row(UiLength::grow(), UiLength::grow()));
  const u32 inspector = tree.addNode(body, column(UiLength::fixed(400), UiLength::grow()));

  AE_EXPECT_TRUE(tree.resolve({0, 0, 1600, 900}), "");
  AE_EXPECT_TRUE(rectEquals(tree.rect(topBar), 0, 0, 1600, 56), "barra no topo");
  AE_EXPECT_TRUE(rectEquals(tree.rect(body), 0, 56, 1600, 772), "corpo recebe a sobra vertical");
  AE_EXPECT_TRUE(rectEquals(tree.rect(dock), 0, 828, 1600, 72), "dock encostada no fundo");
  AE_EXPECT_TRUE(rectEquals(tree.rect(hierarchy), 0, 56, 320, 772), "hierarquia a esquerda");
  AE_EXPECT_TRUE(rectEquals(tree.rect(viewport), 320, 56, 880, 772),
                 "viewport fica exatamente com o que sobra entre os dois paineis");
  AE_EXPECT_TRUE(rectEquals(tree.rect(inspector), 1200, 56, 400, 772), "inspector a direita");
}

AE_TEST(layout_gap_and_padding_consume_main_axis_space) {
  UiLayoutTree tree;
  UiBoxDesc desc = row(UiLength::fixed(400), UiLength::fixed(100));
  desc.padding = UiInsets::symmetric(20, 10);
  desc.gap = 12;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, desc);
  const u32 first = tree.addNode(root, row(UiLength::grow(), UiLength::grow()));
  const u32 second = tree.addNode(root, row(UiLength::grow(), UiLength::grow()));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 400, 100}), "");
  // 400 - 40 de padding - 12 de intervalo = 348, dividido em dois.
  AE_EXPECT_TRUE(rectEquals(tree.rect(first), 20, 10, 174, 80), "");
  AE_EXPECT_TRUE(rectEquals(tree.rect(second), 206, 10, 174, 80),
                 "o segundo comeca depois do primeiro mais o intervalo");
}

AE_TEST(layout_hug_column_sums_children_and_gaps) {
  UiLayoutTree tree;
  UiBoxDesc desc = column(UiLength::hug(), UiLength::hug());
  desc.gap = 8;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, desc);
  tree.addNode(root, leaf(120, 30));
  tree.addNode(root, leaf(200, 30));
  tree.addNode(root, leaf(90, 30));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 1000, 1000}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(root).height, 106), "tres alturas mais dois intervalos");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(root).width, 200), "largura e a do filho mais largo");
}

AE_TEST(layout_main_align_moves_the_block_when_nothing_grows) {
  UiLayoutTree tree;
  UiBoxDesc desc = row(UiLength::fixed(500), UiLength::fixed(100));
  desc.mainAlign = UiAlign::Center;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, desc);
  const u32 child = tree.addNode(root, leaf(100, 40));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 500, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(child).x, 200), "o bloco fica centrado na sobra");

  // A dock inferior do mockup é exatamente isto: cinco itens de largura própria,
  // centrados numa barra que ocupa a tela inteira.
  UiLayoutTree endTree;
  desc.mainAlign = UiAlign::End;
  const u32 endRoot = endTree.addNode(UiLayoutTree::kInvalidNode, desc);
  const u32 endChild = endTree.addNode(endRoot, leaf(100, 40));
  AE_EXPECT_TRUE(endTree.resolve({0, 0, 500, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(endTree.rect(endChild).x, 400), "encostado no fim");
}

AE_TEST(layout_cross_align_stretch_is_the_default_and_center_respects_measure) {
  UiLayoutTree tree;
  UiBoxDesc stretched = row(UiLength::fixed(200), UiLength::fixed(100));
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, stretched);
  const u32 child = tree.addNode(root, leaf(50, 20));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 200, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(child).height, 100), "Stretch preenche o eixo cruzado");

  UiLayoutTree centered;
  UiBoxDesc desc = stretched;
  desc.crossAlign = UiAlign::Center;
  const u32 centeredRoot = centered.addNode(UiLayoutTree::kInvalidNode, desc);
  const u32 centeredChild = centered.addNode(centeredRoot, leaf(50, 20));
  AE_EXPECT_TRUE(centered.resolve({0, 0, 200, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(centered.rect(centeredChild).height, 20), "Center mantem o medido");
  AE_EXPECT_TRUE(nearlyEqual(centered.rect(centeredChild).y, 40), "e o posiciona no meio");
}

AE_TEST(layout_fixed_cross_size_is_not_stretched) {
  // O rail de ferramentas do mockup tem largura própria dentro de uma coluna
  // esticada: `Fixed` no eixo cruzado tem de vencer o `Stretch` do pai.
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                column(UiLength::fixed(300), UiLength::fixed(400)));
  const u32 child = tree.addNode(root, column(UiLength::fixed(64), UiLength::grow()));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 300, 400}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(child).width, 64), "largura fixa sobrevive ao Stretch");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(child).height, 400), "e o eixo principal ainda cresce");
}

AE_TEST(layout_minimum_and_maximum_clamp_the_result) {
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::fixed(1000), UiLength::fixed(100)));
  UiBoxDesc capped = row(UiLength::grow(), UiLength::grow());
  capped.maxWidth = 300;
  const u32 first = tree.addNode(root, capped);
  UiBoxDesc floored = row(UiLength::fixed(10), UiLength::grow());
  floored.minWidth = 120;
  const u32 second = tree.addNode(root, floored);
  AE_EXPECT_TRUE(tree.resolve({0, 0, 1000, 100}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(first).width, 300), "o teto vence a sobra distribuida");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(second).width, 120), "o piso vence o tamanho fixo menor");
}

AE_TEST(layout_overflow_does_not_shrink_children) {
  // Conteúdo maior que o painel é caso de rolagem e de recorte, não de encolher
  // em silêncio: um rótulo espremido seria indistinguível de um layout correto.
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::fixed(100), UiLength::fixed(50)));
  const u32 first = tree.addNode(root, leaf(80, 20));
  const u32 second = tree.addNode(root, leaf(80, 20));
  AE_EXPECT_TRUE(tree.resolve({0, 0, 100, 50}), "");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(first).width, 80), "primeiro mantem o tamanho medido");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(second).width, 80), "segundo tambem");
  AE_EXPECT_TRUE(nearlyEqual(tree.rect(second).x, 80), "e transborda o pai, para ser recortado");
}

AE_TEST(layout_rejects_contradictory_and_malformed_descriptions) {
  UiLayoutTree tree;
  UiBoxDesc impossible = row(UiLength::hug(), UiLength::hug());
  impossible.minWidth = 200;
  impossible.maxWidth = 100;
  AE_EXPECT_TRUE(tree.addNode(UiLayoutTree::kInvalidNode, impossible) ==
                     UiLayoutTree::kInvalidNode,
                 "teto abaixo do piso nao tem resposta certa: recusa");

  UiBoxDesc negativeGap = row(UiLength::hug(), UiLength::hug());
  negativeGap.gap = -4;
  AE_EXPECT_TRUE(tree.addNode(UiLayoutTree::kInvalidNode, negativeGap) ==
                     UiLayoutTree::kInvalidNode,
                 "intervalo negativo sobreporia irmaos");

  UiBoxDesc notFinite = row(UiLength::fixed(std::nanf("")), UiLength::hug());
  AE_EXPECT_TRUE(
      tree.addNode(UiLayoutTree::kInvalidNode, notFinite) == UiLayoutTree::kInvalidNode,
      "NaN entraria em todo retangulo derivado");

  UiBoxDesc negativeWeight = row(UiLength{UiSizing::Grow, -1.0f}, UiLength::hug());
  AE_EXPECT_TRUE(
      tree.addNode(UiLayoutTree::kInvalidNode, negativeWeight) == UiLayoutTree::kInvalidNode,
      "peso negativo tiraria espaco dos irmaos");
}

AE_TEST(layout_rejects_a_second_root_and_an_unknown_parent) {
  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::grow(), UiLength::grow()));
  AE_EXPECT_TRUE(root == 0, "");
  AE_EXPECT_TRUE(tree.addNode(UiLayoutTree::kInvalidNode,
                              row(UiLength::grow(), UiLength::grow())) ==
                     UiLayoutTree::kInvalidNode,
                 "uma segunda raiz nunca seria posicionada");
  AE_EXPECT_TRUE(tree.addNode(99, row(UiLength::grow(), UiLength::grow())) ==
                     UiLayoutTree::kInvalidNode,
                 "pai inexistente quebraria a invariante pai<filho");
}

AE_TEST(layout_resolve_fails_closed_on_an_empty_tree_or_bad_viewport) {
  UiLayoutTree empty;
  AE_EXPECT_TRUE(!empty.resolve({0, 0, 100, 100}), "sem raiz nao ha o que resolver");

  UiLayoutTree tree;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode,
                                row(UiLength::grow(), UiLength::grow()));
  AE_EXPECT_TRUE(!tree.resolve({0, 0, std::nanf(""), 100}), "viewport nao finito e recusado");
  AE_EXPECT_TRUE(!tree.isResolved(), "e a arvore continua marcada como nao resolvida");
  AE_EXPECT_TRUE(tree.rect(root).isEmpty(), "ninguem desenha com retangulo de lixo");
}

AE_TEST(layout_user_data_survives_the_resolution) {
  // O roteamento de toque e o desenho encontram o widget pelo userData; o
  // layout nunca o interpreta, mas tem de devolvê-lo intacto.
  UiLayoutTree tree;
  UiBoxDesc desc = row(UiLength::grow(), UiLength::grow());
  desc.userData = 0xABCDEF01u;
  const u32 root = tree.addNode(UiLayoutTree::kInvalidNode, desc);
  AE_EXPECT_TRUE(tree.resolve({0, 0, 10, 10}), "");
  AE_EXPECT_EQ(tree.userData(root), 0xABCDEF01u, "");
  AE_EXPECT_EQ(tree.userData(999), 0u, "no inexistente devolve zero, nao lixo");
}
