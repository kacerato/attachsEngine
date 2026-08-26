# ADR-02 — ECS por arquétipos com fachada de nós

- **Estado:** aceita e implementada
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 1.3
- **Decisores:** arquitetura, núcleo

## Contexto

Duas linguagens de modelo de dados competiam: a árvore de nós amigável que
todo criador de conteúdo já entende (Godot/Unity clássico) e o ECS por
arquétipos que dá desempenho de cache em CPUs móveis com L2 pequena. Escolher
uma só custaria produtividade do usuário (ECS puro) ou desempenho (grafo de
objetos com ponteiros).

## Decisão

As duas camadas coexistem: o usuário edita uma árvore de `Node`, a engine
executa um ECS por arquétipos por baixo. `Node` é uma fachada — um `readonly
struct` com `EntityId` e métodos de conveniência, sem existência própria em
memória além disso.

## Alternativas descartadas

1. **Só grafo de objetos com ponteiros.** Modelo mental mais simples, mas
   iteração não-contígua é o gargalo dominante em CPUs ARM com cache L2
   pequena — ganho medido típico de 3-6× ao trocar por dados contíguos.
2. **Só ECS puro, sem fachada.** Desempenho igual, mas expõe `EntityId`s e
   componentes crus ao usuário do editor — modelo mental incompatível com
   "arrastar um objeto e editar suas propriedades".

## Evidência

- `managed/Aether.Core/ECS/Archetype.cs`: chunks SoA de 16 KB, alinhados
  para caber na L1 de núcleos "little" de um ARM big.LITTLE.
- `managed/Aether.Core/ECS/World.cs`: `Read<T>`/`Write<T>` como APIs
  distintas — a leitura não marca a coluna como escrita, fechando
  `GAP-ECS-01` (change detection exata por chunk/coluna, não por entidade).
- `managed/Aether.Core/ECS/CompiledQuery.cs`: consultas compiladas e
  cacheadas, com filtros `With`/`Without`/`Changed`.
- `managed/Aether.Core/ECS/EntityCommandBuffer.cs`: buffers de comando
  estruturais (criar/destruir/mudar arquétipo), aplicados em pontos de
  sincronização, não imediatamente.
- `managed/Aether.Core/ECS/Node.cs`: a fachada — `readonly struct Node`
  sobre `World` + `EntityId`, API de parent/children sem alocação.
- `managed/Aether.Core/ECS/Hierarchy.cs`,
  `managed/Aether.Core/ECS/TransformPropagationPlan.cs`,
  `native/physics` (via `NativeTransformKernel.cs`): hierarquia como
  componente, plano topológico O(N), kernel nativo de propagação de
  transform em lote.
- **Testes:** `tests/Aether.Tests/EcsTests.cs` (29 testes),
  `tests/Aether.Tests/HierarchyTests.cs` (20 testes).
- **Benchmark de desempenho** (critério de saída do Gate M1): 100 mil
  entidades hierarquizadas, validado em hardware real (Xiaomi SM8735,
  Snapdragon SM8735) — p50 entre 0,59 ms e 2,29 ms, zero alocação de GC
  (`docs/ESTADO.md`, item 1.3.4-1.3.6).

## Consequências

- Todo componente novo precisa de metadados de arquétipo (item 1.4.1,
  source generator) para participar de serialização/Inspector — não é
  opcional, é o contrato do sistema.
- A fachada `Node` nunca deve ganhar estado próprio (campo além de
  `EntityId`+referência ao `World`) — isso reintroduziria o custo de
  alocação por objeto que a decisão existe para evitar.
