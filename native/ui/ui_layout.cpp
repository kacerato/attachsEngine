#include "ui/ui_layout.h"

#include <cmath>

namespace ae::ui {
namespace {

bool isFinitePositive(float value) noexcept { return std::isfinite(value) && value >= 0.0f; }

bool isLengthValid(const UiLength &length) noexcept {
  if (!std::isfinite(length.value)) return false;
  // Peso negativo tiraria espaço dos irmãos; pixel negativo viraria retângulo
  // invertido. Os dois são erro de construção, não um caso a acomodar.
  return length.value >= 0.0f;
}

bool isDescValid(const UiBoxDesc &desc) noexcept {
  if (!isLengthValid(desc.width) || !isLengthValid(desc.height)) return false;
  if (!isFinitePositive(desc.gap)) return false;
  if (!isFinitePositive(desc.padding.left) || !isFinitePositive(desc.padding.top) ||
      !isFinitePositive(desc.padding.right) || !isFinitePositive(desc.padding.bottom))
    return false;
  if (!isFinitePositive(desc.minWidth) || !isFinitePositive(desc.minHeight)) return false;
  if (!isFinitePositive(desc.maxWidth) || !isFinitePositive(desc.maxHeight)) return false;
  if (!isFinitePositive(desc.contentWidth) || !isFinitePositive(desc.contentHeight)) return false;
  // Zero em `max` é "sem teto". Um teto abaixo do piso não tem resposta certa:
  // escolher um dos dois em silêncio esconderia o erro na tela de quem editou.
  if (desc.maxWidth > 0.0f && desc.maxWidth < desc.minWidth) return false;
  if (desc.maxHeight > 0.0f && desc.maxHeight < desc.minHeight) return false;
  return true;
}

float clampToBounds(float value, float minimum, float maximum) noexcept {
  float result = value < minimum ? minimum : value;
  if (maximum > 0.0f && result > maximum) result = maximum;
  return result < 0.0f ? 0.0f : result;
}

} // namespace

void UiLayoutTree::reset() noexcept {
  nodes_.clear();
  resolved_ = false;
}

u32 UiLayoutTree::addNode(u32 parent, const UiBoxDesc &desc) {
  if (nodes_.size() >= kMaximumNodes) return kInvalidNode;
  if (!isDescValid(desc)) return kInvalidNode;
  // A raiz é o primeiro nó e só pode haver uma: uma segunda raiz nunca seria
  // posicionada, e devolver um índice válido para algo que não aparece na tela
  // é pior do que recusar.
  if (parent == kInvalidNode) {
    if (!nodes_.empty()) return kInvalidNode;
  } else if (parent >= nodes_.size()) {
    return kInvalidNode;
  }

  const u32 index = static_cast<u32>(nodes_.size());
  Node node{};
  node.desc = desc;
  node.parent = parent;
  nodes_.push_back(node);

  if (parent != kInvalidNode) {
    Node &parentNode = nodes_[parent];
    if (parentNode.firstChild == kInvalidNode) {
      parentNode.firstChild = index;
    } else {
      nodes_[parentNode.lastChild].nextSibling = index;
    }
    parentNode.lastChild = index;
    ++parentNode.childCount;
  }
  resolved_ = false;
  return index;
}

void UiLayoutTree::measureNode(Node &node) noexcept {
  const UiBoxDesc &desc = node.desc;
  const bool horizontal = desc.axis == UiAxis::Horizontal;

  // Conteúdo dos filhos: soma no eixo principal (com os intervalos entre eles),
  // maior no eixo cruzado. Um nó sem filhos usa o tamanho intrínseco da folha.
  float childrenMain = 0.0f;
  float childrenCross = 0.0f;
  if (node.childCount > 0) {
    u32 visited = 0;
    for (u32 child = node.firstChild; child != kInvalidNode; child = nodes_[child].nextSibling) {
      const Node &childNode = nodes_[child];
      const float mainSize = horizontal ? childNode.measured.x : childNode.measured.y;
      const float crossSize = horizontal ? childNode.measured.y : childNode.measured.x;
      childrenMain += mainSize;
      if (crossSize > childrenCross) childrenCross = crossSize;
      ++visited;
    }
    if (visited > 1) childrenMain += desc.gap * static_cast<float>(visited - 1);
  }

  const float contentMain = node.childCount > 0
      ? childrenMain
      : (horizontal ? desc.contentWidth : desc.contentHeight);
  const float contentCross = node.childCount > 0
      ? childrenCross
      : (horizontal ? desc.contentHeight : desc.contentWidth);

  const UiLength &mainLength = horizontal ? desc.width : desc.height;
  const UiLength &crossLength = horizontal ? desc.height : desc.width;
  const float paddingMain = horizontal ? desc.padding.horizontal() : desc.padding.vertical();
  const float paddingCross = horizontal ? desc.padding.vertical() : desc.padding.horizontal();

  // `Grow` mede como `Hug`: o tamanho medido é o piso que ele leva para a
  // distribuição da sobra, e um filho que cresce nunca pode encolher abaixo do
  // próprio conteúdo só porque um irmão pediu mais peso.
  const float measuredMain = mainLength.mode == UiSizing::Fixed
      ? mainLength.value
      : contentMain + paddingMain;
  const float measuredCross = crossLength.mode == UiSizing::Fixed
      ? crossLength.value
      : contentCross + paddingCross;

  const float measuredWidth = horizontal ? measuredMain : measuredCross;
  const float measuredHeight = horizontal ? measuredCross : measuredMain;
  node.measured.x = clampToBounds(measuredWidth, desc.minWidth, desc.maxWidth);
  node.measured.y = clampToBounds(measuredHeight, desc.minHeight, desc.maxHeight);
}

void UiLayoutTree::arrangeChildren(const Node &node) noexcept {
  if (node.childCount == 0) return;
  const bool horizontal = node.desc.axis == UiAxis::Horizontal;
  const UiRect inner = deflate(node.rect, node.desc.padding);
  const float availableMain = horizontal ? inner.width : inner.height;
  const float availableCross = horizontal ? inner.height : inner.width;
  const float totalGap = node.desc.gap * static_cast<float>(node.childCount - 1);

  // Primeira passagem: quanto os filhos de tamanho não elástico ocupam, e qual
  // é a soma dos pesos de quem cresce.
  float fixedMain = 0.0f;
  float totalWeight = 0.0f;
  for (u32 child = node.firstChild; child != kInvalidNode; child = nodes_[child].nextSibling) {
    const Node &childNode = nodes_[child];
    const UiLength &length = horizontal ? childNode.desc.width : childNode.desc.height;
    if (length.mode == UiSizing::Grow) {
      totalWeight += length.value;
      // O medido do elástico entra como piso, não como tamanho: ele será
      // reconsiderado abaixo, mas o espaço que ele exige já está comprometido.
      fixedMain += horizontal ? childNode.measured.x : childNode.measured.y;
    } else {
      fixedMain += horizontal ? childNode.measured.x : childNode.measured.y;
    }
  }

  const float leftover = availableMain - totalGap - fixedMain;
  // Sobra negativa significa que o conteúdo não cabe. Nada é encolhido: o
  // recorte de desenho resolve o excesso visualmente e a rolagem resolve o
  // excesso de verdade. Encolher em silêncio produziria rótulos truncados que
  // ninguém pediu e que o teste de layout não teria como distinguir do certo.
  const float distributable = leftover > 0.0f ? leftover : 0.0f;

  float cursor = horizontal ? inner.x : inner.y;
  if (totalWeight <= 0.0f && distributable > 0.0f) {
    // Sem ninguém elástico, `mainAlign` decide onde o bloco inteiro fica.
    if (node.desc.mainAlign == UiAlign::Center) cursor += distributable * 0.5f;
    else if (node.desc.mainAlign == UiAlign::End) cursor += distributable;
  }

  for (u32 child = node.firstChild; child != kInvalidNode; child = nodes_[child].nextSibling) {
    Node &childNode = nodes_[child];
    const UiLength &mainLength = horizontal ? childNode.desc.width : childNode.desc.height;
    const UiLength &crossLength = horizontal ? childNode.desc.height : childNode.desc.width;

    float mainSize = horizontal ? childNode.measured.x : childNode.measured.y;
    if (mainLength.mode == UiSizing::Grow && totalWeight > 0.0f)
      mainSize += distributable * (mainLength.value / totalWeight);

    // No eixo cruzado não existe sobra a distribuir: ou o filho preenche o pai
    // (Stretch), ou fica com o tamanho que mediu e é alinhado dentro do espaço.
    float crossSize = horizontal ? childNode.measured.y : childNode.measured.x;
    const bool stretches = node.desc.crossAlign == UiAlign::Stretch &&
                           crossLength.mode != UiSizing::Fixed;
    if (stretches) crossSize = availableCross;

    const float childMinCross = horizontal ? childNode.desc.minHeight : childNode.desc.minWidth;
    const float childMaxCross = horizontal ? childNode.desc.maxHeight : childNode.desc.maxWidth;
    crossSize = clampToBounds(crossSize, childMinCross, childMaxCross);
    const float childMinMain = horizontal ? childNode.desc.minWidth : childNode.desc.minHeight;
    const float childMaxMain = horizontal ? childNode.desc.maxWidth : childNode.desc.maxHeight;
    mainSize = clampToBounds(mainSize, childMinMain, childMaxMain);

    float crossOffset = 0.0f;
    if (!stretches) {
      const float slack = availableCross - crossSize;
      if (slack > 0.0f) {
        if (node.desc.crossAlign == UiAlign::Center) crossOffset = slack * 0.5f;
        else if (node.desc.crossAlign == UiAlign::End) crossOffset = slack;
      }
    }

    if (horizontal) {
      childNode.rect = {cursor, inner.y + crossOffset, mainSize, crossSize};
    } else {
      childNode.rect = {inner.x + crossOffset, cursor, crossSize, mainSize};
    }
    cursor += mainSize + node.desc.gap;
  }
}

bool UiLayoutTree::resolve(const UiRect &viewport) noexcept {
  resolved_ = false;
  if (nodes_.empty() || !isFinite(viewport)) return false;
  if (viewport.width < 0.0f || viewport.height < 0.0f) return false;

  // Índice do filho é sempre maior que o do pai (addNode exige o pai existente),
  // então percorrer ao contrário mede toda a subárvore antes do pai. É a mesma
  // garantia que uma recursão pós-ordem daria, sem consumir pilha.
  for (usize index = nodes_.size(); index > 0; --index) measureNode(nodes_[index - 1]);

  Node &root = nodes_[0];
  // A raiz ocupa o viewport quando é elástica, e o tamanho que mediu quando é
  // fixa ou ajustada ao conteúdo — um painel flutuante é uma raiz Hug.
  const float rootWidth = root.desc.width.mode == UiSizing::Grow ? viewport.width : root.measured.x;
  const float rootHeight =
      root.desc.height.mode == UiSizing::Grow ? viewport.height : root.measured.y;
  root.rect = {viewport.x, viewport.y, rootWidth, rootHeight};

  // Na ordem direta o pai já tem retângulo final quando chega a vez dele de
  // posicionar os filhos.
  for (usize index = 0; index < nodes_.size(); ++index) arrangeChildren(nodes_[index]);
  resolved_ = true;
  return true;
}

UiRect UiLayoutTree::rect(u32 node) const noexcept {
  if (node >= nodes_.size() || !resolved_) return UiRect{};
  return nodes_[node].rect;
}

UiPoint UiLayoutTree::measuredSize(u32 node) const noexcept {
  if (node >= nodes_.size()) return UiPoint{};
  return nodes_[node].measured;
}

u32 UiLayoutTree::userData(u32 node) const noexcept {
  if (node >= nodes_.size()) return 0;
  return nodes_[node].desc.userData;
}

u32 UiLayoutTree::parent(u32 node) const noexcept {
  if (node >= nodes_.size()) return kInvalidNode;
  return nodes_[node].parent;
}

} // namespace ae::ui
