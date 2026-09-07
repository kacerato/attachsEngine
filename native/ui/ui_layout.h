// Layout retido da interface: uma árvore de caixas em linha ou coluna, com
// tamanho fixo, ajustado ao conteúdo ou proporcional à sobra.
//
// É deliberadamente um subconjunto do flexbox, não uma reimplementação dele. O
// editor precisa de barra superior fixa, viewport ocupando a sobra, painel
// lateral de largura fixa e listas que crescem — e de nada mais. Cada regra a
// mais aqui é uma regra que teria de ser testada, medida no frame e explicada
// no Inspector; `wrap`, ordem e bases percentuais ficam de fora até existir uma
// tela que os exija.
//
// **A árvore é reconstruída todo frame e resolvida em duas varreduras lineares.**
// Não há recursão: um nó só pode ser criado depois do pai, então percorrer os
// índices ao contrário mede filhos antes de pais, e percorrê-los na ordem
// posiciona pais antes de filhos. Isso troca profundidade de pilha por uma
// invariante verificável, o que importa numa engine que roda com -fno-exceptions.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"

#include <vector>

namespace ae::ui {

enum class UiAxis : u8 { Horizontal, Vertical };

enum class UiSizing : u8 {
  // Tamanho em pixels lógicos, vindo do token ou do desenho.
  Fixed,
  // Tamanho vem do conteúdo: soma dos filhos no eixo principal, maior filho no
  // eixo cruzado, mais o padding. Numa folha, é `contentWidth`/`contentHeight`.
  Hug,
  // Divide a sobra do eixo principal do pai proporcionalmente ao peso. No eixo
  // cruzado, `Grow` se comporta como `Hug` — não existe "sobra" transversal.
  Grow,
};

struct UiLength final {
  UiSizing mode = UiSizing::Hug;
  // Pixels quando Fixed, peso quando Grow, ignorado quando Hug.
  float value = 0.0f;

  static constexpr UiLength fixed(float pixels) noexcept { return {UiSizing::Fixed, pixels}; }
  static constexpr UiLength hug() noexcept { return {UiSizing::Hug, 0.0f}; }
  static constexpr UiLength grow(float weight = 1.0f) noexcept { return {UiSizing::Grow, weight}; }
};

struct UiBoxDesc final {
  // Eixo em que os FILHOS deste nó são empilhados. Uma folha ignora o campo.
  UiAxis axis = UiAxis::Horizontal;
  UiLength width{};
  UiLength height{};
  UiInsets padding{};
  float gap = 0.0f;
  // Onde o bloco de filhos fica quando sobra espaço no eixo principal e nenhum
  // filho é Grow. Com um filho Grow não sobra nada e o campo não tem efeito.
  UiAlign mainAlign = UiAlign::Start;
  UiAlign crossAlign = UiAlign::Stretch;
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  // Zero significa sem teto. Um máximo menor que o mínimo é entrada inválida e
  // faz `resolve` falhar, em vez de escolher em silêncio qual dos dois vence.
  float maxWidth = 0.0f;
  float maxHeight = 0.0f;
  // Tamanho intrínseco de uma folha: texto já medido, ícone, miniatura. Um nó
  // com filhos ignora estes campos — o conteúdo dele são os filhos.
  float contentWidth = 0.0f;
  float contentHeight = 0.0f;
  // Repassado intacto para quem consome o resultado (roteamento de toque e
  // desenho). O layout nunca o interpreta.
  u32 userData = 0;
};

class UiLayoutTree final {
public:
  static constexpr u32 kInvalidNode = 0xffffffffu;
  // Teto de nós por frame. Existe para que um laço de construção com defeito
  // falhe em vez de crescer até o alocador reclamar.
  static constexpr u32 kMaximumNodes = 4096;

  void reset() noexcept;

  // Devolve o índice do novo nó, ou kInvalidNode quando a descrição é inválida,
  // o pai não existe ou a árvore está cheia. O pai precisa já existir: é essa
  // regra que garante `pai < filho` e, com ela, as duas varreduras lineares.
  u32 addNode(u32 parent, const UiBoxDesc &desc);

  // Resolve todos os retângulos dentro de `viewport`. Falso quando não há raiz,
  // quando o viewport não é finito ou quando alguma descrição é contraditória;
  // nesse caso os retângulos ficam como estavam e nada é desenhado com lixo.
  bool resolve(const UiRect &viewport) noexcept;

  u32 nodeCount() const noexcept { return static_cast<u32>(nodes_.size()); }
  bool isResolved() const noexcept { return resolved_; }
  // Retângulo final em coordenadas da superfície. Nó inexistente devolve vazio.
  UiRect rect(u32 node) const noexcept;
  // Tamanho medido antes da distribuição da sobra. Útil para diagnóstico e para
  // decidir se uma lista precisa rolar; não é o tamanho final.
  UiPoint measuredSize(u32 node) const noexcept;
  u32 userData(u32 node) const noexcept;
  u32 parent(u32 node) const noexcept;

private:
  struct Node final {
    UiBoxDesc desc{};
    u32 parent = kInvalidNode;
    u32 firstChild = kInvalidNode;
    u32 lastChild = kInvalidNode;
    u32 nextSibling = kInvalidNode;
    u32 childCount = 0;
    UiPoint measured{};
    UiRect rect{};
  };

  void measureNode(Node &node) noexcept;
  void arrangeChildren(const Node &node) noexcept;

  std::vector<Node> nodes_;
  bool resolved_ = false;
};

} // namespace ae::ui
