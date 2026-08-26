# ADR-08 — Processo separado para o Play

- **Estado:** aceita como decisão de design; **não implementada**
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 3.6.1–3.6.5
- **Decisores:** arquitetura, editor

## Contexto

O editor roda no próprio celular do usuário (CONVENCOES.md §1). Quando o
usuário aperta "Play" para testar o jogo, um crash no código de gameplay (o
conteúdo do próprio usuário, potencialmente com bugs) não pode derrubar o
processo do editor — isso violaria a regra 3 da barra de qualidade
(CONVENCOES.md §8: "nunca crashar o editor por culpa do conteúdo do
usuário").

## Decisão

O modo Play roda em um **processo separado** do processo do editor,
comunicando por IPC via memória compartilhada. Um crash do Play é isolado e
apresentável (stack trace legível), sem afetar o editor.

## Alternativas descartadas

1. **Mesmo processo com try/catch.** Não protege contra crash nativo (uma
   violação de acesso em C++ do lado física/renderer não é capturável por
   try/catch gerenciado), nem contra memória corrompida que sobrevive ao
   catch e degrada o processo do editor de forma sutil depois.

## Estado de implementação

**Nenhuma implementação existe.** Busca completa em `managed/` e `native/`
não encontrou processo Play separado, IPC por memória compartilhada, nem
qualquer mecanismo de isolamento de execução. `docs/MATRIZ-MARCOS.md`
confirma os cinco itens da Etapa 3.6 (Play in Editor) — processo separado
(3.6.1), exibição do jogo no viewport via `AHardwareBuffer`/`IOSurface`
(3.6.2), inspeção ao vivo (3.6.3), pausa/avanço quadro a quadro (3.6.4),
isolamento de crash com stack trace (3.6.5) — todos como "não iniciado".

Não há vestígio nem no protótipo HTML (`prototype/editor.html`), que cobre
apenas câmera/gizmos/menu radial/Inspector, sem modo Play.

## Consequências

- Esta ADR existe para registrar a decisão de design **antes** de qualquer
  implementação começar — quando a Etapa 3.6 for iniciada, a arquitetura de
  processo separado já está decidida, não é uma escolha em aberto naquele
  momento.
- Qualquer protótipo futuro de "rodar o jogo dentro do editor" que **não**
  use processo separado (ex.: um atalho rápido de mesmo-processo "só para
  testar") deve ser tratado explicitamente como workaround temporário, não
  como o caminho de produto — CONVENCOES.md §8 não permite essa troca
  silenciosa de garantia.
