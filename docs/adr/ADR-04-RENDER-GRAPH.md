# ADR-04 — Render graph com aliasing e fusão de subpasses

- **Estado:** aceita e implementada (headless — nunca executada contra GPU real)
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 2.3
- **Decisores:** arquitetura, renderer

## Contexto

GPUs móveis são Tile-Based Deferred Renderers (TBDR): o gargalo dominante é
largura de banda de memória, não FLOPs, e cada `vkCmdBeginRenderPass` é uma
resolução de tile. Um pipeline fixo codificado à mão (como se faz em
desktop) obriga o programador a lembrar manualmente de fundir passes,
marcar recursos como memoryless e escolher `LOAD_OP`/`STORE_OP` corretos em
cada ponto do código — um erro de descuido custa uma resolução de tile
inteira, silenciosamente.

## Decisão

Um render graph que recebe passes declarativos (recursos lidos/escritos) e
compila automaticamente: ordenação topológica, poda de passes cujo
resultado ninguém lê, aliasing de memória para recursos com tempos de vida
disjuntos, inserção mínima de barreiras, escolha de `LOAD_OP`/`STORE_OP` por
uso real, marcação memoryless quando possível, e fusão de passes compatíveis
em subpasses.

## Alternativas descartadas

1. **Pipeline fixo codificado.** Único jeito de respeitar a disciplina TBDR
   sem um render graph seria replicar manualmente, em cada ponto do código,
   as mesmas decisões que um grafo automatiza — código espalhado e frágil a
   erro humano em cada novo pass adicionado.

## Evidência

- `native/rendergraph/render_graph.h`/`.cpp`: compilador puro — topológico,
  poda, aliasing, barreiras, `loadOp`/`storeOp`, memoryless, fusão de
  subpasses.
- **Testes:** `tests/native/test_render_graph.cpp` — 18 testes, cobrindo
  grafo vazio, passe único, passes independentes sem ordem obrigatória,
  ordenação topológica respeitando dependência de recurso, poda de passe
  morto e de passe cujo resultado ninguém lê, tempo de vida do primeiro ao
  último toque, aliasing economizando memória com lifetimes disjuntos,
  recursos com vidas sobrepostas não compartilhando slot, barreira write→read
  inserida e read→read não inserida, alvo totalmente sobrescrito recebendo
  `LOAD_DONT_CARE`, alvo não lido depois recebendo `STORE_DONT_CARE`,
  recurso importado sempre recebendo `STORE`, fusão de G-buffer+lighting com
  G-buffer ficando memoryless, pass que amostra textura arbitrária não
  fundindo, detecção de ciclo com mensagem clara, exportação de texto legível
  do grafo.

## Limitação atual

Toda a implementação é testada apenas headless (CPU/lógica pura) — nunca foi
executada contra uma GPU real. O visualizador do grafo (item 2.3.6, base do
recurso de inspeção do usuário no editor) não existe. O Gate M2 do plano
principal continua fechado: nenhuma cena real usa o render graph em conjunto
com o RHI (ADR-03) ainda.

## Consequências

- Nenhum pass de renderização deve ser escrito com `vkCmdBeginRenderPass`
  manual fora do render graph — todo novo pass do pipeline direto (itens
  2.4.x) entra como declaração de recursos lidos/escritos, deixando o grafo
  decidir barreiras e fusão.
- A primeira execução contra GPU real (quando M2 começar de fato) é o
  próximo ponto de validação crítico: a lógica está provada, mas nunca
  mediu o comportamento real do driver Adreno/Mali com aliasing de memória.
