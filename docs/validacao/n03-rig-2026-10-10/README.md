# N03 — aceite do estúdio de animação, rig, IK e transferência

Data: 11/10/2026. Branch `codex/gameplay-runtime`. Aparelho: Astra Dev (`dev.aether.editor.u07`), códigos 34–43.

## Host
- `aether_animation_clip_editor_tests`: 60/60, incluindo aceites por toque:
  estúdio em painéis, rig no estúdio, espelho de membro, salvar pose/filtro/fantasma,
  transferência Raposa→Lobo com repousos distintos, IK de dois ossos (cálculo).
- `aether_rig_authoring_tests`: 2/2.

## Aparelho (projeto AnimatorFox, personagem Fox, 27 juntas)
| Captura | O que mostra |
|---|---|
| `clip-antes-aparelho.png` | Editor antigo em tela cheia (referência) |
| `aparelho-pares-sugeridos.png` | 7 pares de espelho sugeridos pelo nome |
| `aparelho-ik-pata.png` | IK levantando a pata, canais do ombro/cotovelo |
| `aparelho-espelho-membro.png` | Pata esquerda recebendo o membro espelhado |
| `aparelho-fantasma-filtro.png` | Fantasma âmbar e timeline filtrada |
| `studio-*.png` | Capturas host 1386×640 |

## Limites declarados
- Transferência por nome e só de rotações; sem mapa manual nem posição da raiz.
- IK e espelho gravam na camada Base.
- Biblioteca de poses sem miniatura (readback Vulkan preparado, não integrado).
- Fantasma com passo fixo de 0,1 s.
- Transferência validada no host; o projeto do aparelho tem um único personagem.
