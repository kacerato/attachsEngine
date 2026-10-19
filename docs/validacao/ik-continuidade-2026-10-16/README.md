# IK em Play: continuidade quadro a quadro (16/10/2026)

Origem: o usuário relatou a animação do Kyle "toda bugada, só olhar frame a frame" no projeto `KyleRevisao` do aparelho (Astra Dev code 93). O projeto foi puxado por `adb pull` e tocado no host com o modo novo `aether_ui_preview … play-project <projeto> <personagem> [clipe] [quadros] [retirar] [ossos]`, que mede o maior giro local de cada osso entre quadros consecutivos (1/60 s) e aceita retirar componentes de IK para isolar causas.

## O que o projeto tinha

- `Robot Kyle`: Animação (clipe "Clipe", aceno do braço direito, 2 s) + Sistema de interação (carregar a Caixa, agachar até 0,41 m) + Root motion.
- Sobras das validações de 15–16/10 (receitas Criar › Personagem aplicadas no projeto e nunca removidas): Olhar para alvo na cabeça **e** no próprio objeto "Olhar · Head", Mãos ao alvo (dois ossos) e Mirar nos dois punhos, Pés no chão nos dois tornozelos. Três solvers no mesmo punho, com alvos fixos, por cima do clipe.
- "Clipe 2" (Aceno), o clipe tocado automaticamente, só tem chaves no objeto raiz com os valores de repouso: não move nada.

## Medição no host (antes → depois)

| Configuração | Saltos > 20° num quadro (180 quadros) | Pior |
|---|---|---|
| Tudo como no aparelho | 9 → 2 | antebraço 161° → 59° (só na volta do loop em 2,0 s) |
| Só o Sistema de interação (sem sobras) | 6 → 1 | pelve 27 cm num quadro → nenhum; resta a volta do loop |
| Carregar como desenhado (interação + pés + olhar) | 6 → 1 | joelho 74° → nenhum; resta a volta do loop |
| Dois ossos sozinho / Mirar sozinho / Pés sozinho / Olhar sozinho | 1 → 1 | só a volta do loop (chaves de 0 s e 2 s diferentes no clipe do usuário) |

## Causas e correções (`native/runtime/scene_ik_impl.h`)

1. **Agachamento do Sistema de interação oscilava.** O pedido de descer a pelve era tudo-ou-nada em `flat == reach` (alvo na borda do alcance horizontal): a mão animada cruzava a borda e a pelve caía 27 cm num quadro e voltava no seguinte. Agora o pedido cai em rampa entre 80% e 100% do alcance e o corpo desce/sobe a no máximo 2 m/s (3,3 cm por quadro), com estado por sistema (`crouches_`).
2. **A inclinação e o agachamento mediam a mão com o IK do quadro anterior.** O braço volta à pose animada (`beginBone`) antes de medir, como o pé no apoio.
3. **Dois ossos sem polo trocava o lado do cotovelo.** A dobra vinha só da pose animada; com a cadeia quase reta, 1 mm decidia o lado. O repouso no referencial do osso raiz dá o lado (`restBendPole`), e a pose animada continua mandando quando dobra de verdade. Referência: Final IK 2.2, LimbIK "bend normal" relativo ao osso raiz; Unity Animation Rigging, TwoBoneIKConstraint com hint opcional.

Cenários novos em `tests/native/test_ik_constraints.cpp`: `ik_interaction_crouch_never_moves_the_body_more_than_a_step_per_frame` e `ik_two_bone_without_pole_keeps_the_rest_bend_side_when_the_chain_is_straight`.

## Aparelho (Astra Dev code 95)

| Passo | Captura |
|---|---|
| Cena como o usuário a viu no code 93: gizmos de IK nos punhos e na cabeça, sobras das receitas | `aparelho/cena-93-sobras.png` |
| Rajada de 16 capturas no code 93 (edição, parado) | `aparelho/cena-93-antes.png` |
| Dev 95 instalado; `scenes/editor.aescene` do projeto recebeu a cópia limpa (sem os 8 componentes de IK das receitas; Sistema de interação e Caixa mantidos) | — |
| Play: 16 capturas em ~8 s, Kyle segura a caixa com as duas mãos, sem bobbing nem salto de cotovelo | `aparelho/play-95-rajada.png`, `aparelho/play-95-zoom.png` |
| Parar o Play: sem gizmos de IK nos punhos e na cabeça | `aparelho/cena-95-apos-play.png` |
| Pastas "Olhar · Head", "Alvo IK · …" e "Mira · …" excluídas pelo menu do objeto; "Salvo" | `aparelho/cena-95-limpa.png` |

Fora desta rodada: a volta do loop em 2,0 s do "Clipe" do usuário (chaves de 0 s e 2 s diferentes) continua um salto de autoria; o estúdio não oferece ainda "copiar primeira chave para o fim".
