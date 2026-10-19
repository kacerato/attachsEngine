# Animator: modos de mistura 2D e camadas sincronizadas — validação (16/10/2026)

Revisão de fonte: commit `2b443b35` (Animator v6, `.aeanimator` v4). Aparelho: Astra Dev code 93 (`dev.aether.editor.u07`), projeto "Kyle - Carregar caixa".

## Host

| Suíte | Resultado |
|---|---|
| `aether_tests` | 1572/1572 |
| `aether_gui_tests` | 126/126 |
| `aether_animation_clip_editor_tests` | 73/73 |

Cenários novos em `tests/native/test_animator.cpp`: quatro modos 2D sobre a mesma cruz de cinco clipes, arquivo v6 e `animator_sync_layer_mirrors_structure_and_keeps_own_motions`.

## Aparelho (tudo pela UI)

| Passo | Captura | Observado |
|---|---|---|
| Criar › Gameplay › Animator na cena do Kyle | `aparelho/animator.png` | Componente criado, Inspector com "1 estado · 0 parâmetros" e botão Abrir grafo |
| Grafo › estado Parado › Ajustes | `aparelho/state.png` | Painel do estado com Clipe / Mistura 1D / Mistura 2D |
| Mistura 2D | `aparelho/blend2d.png` | Linha **Modo 2D · Triangulação** abaixo dos parâmetros X/Y |
| Toque em Modo 2D ×3 | `aparelho/modo2d_1..3.png` | Direcional simples → Direcional livre → Cartesiano livre (volta a Triangulação no quarto toque) |
| "+" de camada | `aparelho/layer.png` | Camada nova com Composição, Peso, **Sincronizar com · –**, Máscara |
| Toque em Sincronizar com | `aparelho/sync.png` | Valor **Base**; o grafo da camada passa a mostrar o estado Parado (Mistura 2D) copiado da Base, na mesma posição |
| +Estado na camada sincronizada | `aparelho/objmenu.png` (barra de estado) | Nada criado; aviso "Camada sincronizada: edite a estrutura na camada de origem" |
| Excluir o Animator de teste | — | Projeto volta ao estado anterior, "Salvo" |

Fora desta rodada: Blend Tree aninhada e Sync timing (ver `docs/planos/ANIMATOR-2026-10-07.md`).
