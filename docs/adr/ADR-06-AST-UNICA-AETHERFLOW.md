# ADR-06 — AST única com três representações (AetherFlow)

- **Estado:** aceita; fundação implementada, representações visuais não iniciadas
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 5.1–5.6
- **Decisores:** arquitetura, AetherFlow

## Contexto

Todo sistema no-code tradicional trata "grafo visual" e "código" como
representações separadas e sincronizadas manualmente (ou pior, apenas uma
delas é editável). Isso cria um teto: o usuário avançado esbarra em uma
parede quando o grafo não expressa o que o código faria, e o usuário
iniciante nunca consegue ler o código gerado como uma segunda fonte de
verdade confiável.

## Decisão

Uma única AST (Abstract Syntax Tree) é a fonte única de verdade. Blocos,
Grafo (Canvas de nós) e C# são apenas três **vistas** sobre a mesma AST —
editar em qualquer uma reflete nas outras, porque todas leem/escrevem a
mesma estrutura de dados subjacente, não uma cópia sincronizada.

## Alternativas descartadas

1. **Grafo e código como representações separadas.** É o modelo de quase
   todo concorrente (Blueprint da Unreal não gera C++ legível e editável;
   Bolt da Unity não é a fonte de verdade do C#). Elimina o teto do no-code
   é a inovação central do produto — abrir mão disso remove a razão de ser
   do AetherFlow.

## Evidência

- `managed/Aether.Flow/Ast/FlowGraph.cs`: comentário de cabeçalho confirma
  a intenção — "a fonte única da verdade da qual Blocos, Grafo e C# são
  apenas vistas".
- AST: `FlowNode.cs`, `FlowGraph.cs`, `FlowConnection.cs`, `FlowPin.cs`,
  `FlowVariable.cs`, `FlowType.cs`.
- `Validation/FlowValidator.cs`: validador semântico com capabilities
  (rejeita nó que exige um serviço não injetado no contexto de execução).
- `CodeGen/FlowToCSharp.cs`: AST → C# gerado legível.
- `CodeGen/CSharpToFlow.cs`: parser Roslyn C# → AST (round-trip parcial).
- `Interpreter/FlowInterpreter.cs`: interpretador direto da AST, sem passar
  por geração de código.
- `Serialization/FlowSerializer.cs`: formato `.aflow` (ver ADR-07, formato
  dual texto+binário).
- `Runtime/FlowExecutionContext.cs`: contexto explícito de
  serviços/capabilities injetado (fecha `GAP-FLOW-02` — nenhum singleton
  global).
- **Testes:** `tests/Aether.Tests/FlowTests.cs` — 48 testes, incluindo
  testes diferenciais que executam o C# de referência, o interpretador e o
  C# regenerado e comparam efeitos sobre um mundo determinístico (fecha
  `GAP-FLOW-01`, controle de fluxo `return`/`break`/`continue` divergente).

## Limitação atual

A fundação (AST + interpretador + gerador + validador + serializador) está
implementada e testada, mas nenhuma das interfaces visuais existe ainda:
sem Canvas de nós (item 5.3), sem Modo Lista/Modo Blocos (item 5.4). O
catálogo de nós é mínimo (cerca de 12-13 nós — `log.message` é o slice
vertical completo citado em `docs/PLANO-FECHAMENTO-LACUNAS.md`), não os
~400 nós especificados no plano de produto.

## Consequências

- Toda nova capability de execução (acesso a física, tempo, input) deve
  entrar via `FlowExecutionContext`, nunca como singleton ou estado global
  — é o contrato que fechou `GAP-FLOW-02` e não pode ser reaberto por
  atalho.
- As três interfaces visuais (itens 5.3–5.6 do plano principal) devem ser
  construídas como vistas sobre a AST existente, nunca como um modelo de
  dados paralelo — isso quebraria a premissa central desta decisão.
