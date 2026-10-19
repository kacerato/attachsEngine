# G4 — Dia e noite no modelo da Unity (19/10/2026)

Aparelho: POCO (Android, Vulkan). APKs `0.3.0-dev.113-unity-ceu` e
`0.3.0-dev.114-unity-poente`, canal `dev.aether.editor.u07`. Cena: Vila Astra
(céu físico), com um objeto de teste “Sol com dia e noite” (Luz direcional +
Hora do dia), apagado depois das capturas.

## Como funciona (igual à Unity)

| Unity | Astra |
|---|---|
| Directional Light como *Sun Source* | Luz direcional; o céu lê a direção do transform dela |
| Script `[ExecuteAlways]` de ciclo de dia girando o transform | Componente **Hora do dia** gira a rotação do objeto no editor e no Play (gizmo e Inspector mostram o ângulo real) |
| Skybox/Procedural (O'Neil, GPU Gems 2 cap. 16) | `sky_procedural.glsl` / `procedural_sky.cpp`, com Tom do céu, Chão, espessura e exposição |
| Environment Lighting Source: Skybox | Ambiente tirado do próprio céu no ângulo atual do sol |
| Physically Based Sky (HDRP) | Neblina e ambiente escalados pela irradiância integrada do céu físico (agora ÷ sol a pino) |

Referências: Unity 6 Manual, *Skybox/Procedural shader* e *Lighting window —
Environment*; HDRP 17 *Physically Based Sky*; O'Neil, GPU Gems 2 cap. 16.

## Capturas

| Arquivo | O que mostra |
|---|---|
| `01-vila-ceu-unity.png` | APK 111, céu procedural antes da calibração da exposição (escuro demais) |
| `02-sol-10h-inspector.png` | Hora 10 → rotação do transform X 41,6 / Y −37,9 no Inspector |
| `03-hora-17h.png` | Editor, luz quente e rasante |
| `04-hora-22h.png` | Editor sem sol |
| `06-noite-22h.png` | APK 113, Play: noite escura com estrelas, sem faixa clara de neblina |
| `07-por-do-sol-17h.png` | APK 114, Play às 17,4 h (sol ~2° acima do horizonte): horizonte avermelhado, sem estrelas, luz rasante nas paredes |
| `08-editor-12h.png` | Editor ao meio-dia: sombras curtas, Hora = 12 |
| `09-meio-dia-12h.png` | APK 114, Play ao meio-dia: céu azul, ambiente cheio |

## Defeitos achados e corrigidos no aparelho

- APK 113, poente: estrelas acesas com o sol acima do horizonte e praça clara sob céu
  preto. Causa: o ambiente do céu físico usava a curva do céu procedural. Correção
  (114): razão da irradiância do próprio céu físico; estrelas só depois do pôr do
  sol (acendem até −6°, crepúsculo civil).

## Lacunas

- Zênite do poente muito escuro com exposição fixa; a Unity/HDRP resolve com
  exposição automática (*Exposure* do Volume) — pendente no plano de pós.
- Sem luz da lua: a noite fica escura como na Unity sem uma segunda direcional.
- O viewport de edição só desenha o céu com o modo de efeitos.
- Postes da Vila não conferidos à noite.
