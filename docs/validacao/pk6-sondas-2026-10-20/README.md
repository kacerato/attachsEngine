# PK-6 — Reflection Probe e Light Probe Group (20–21/10/2026)

Pacote P-01 da auditoria full Unity (`docs/planos/auditoria-full-unity-2026-10-19.md`, seção "Luz indireta").
Branch `claude/suspicious-hopper-f4e458`, base 9fbbbe57. Referências (Unity 6000.0):
[Reflection Probe](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ReflectionProbe.html),
[Using Reflection Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/AdvancedRefProbe.html),
[Light Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/LightProbes.html),
[Light Probe Group](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightProbeGroup.html),
[Mesh Renderer › Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html).
Frames de vídeo da Unity: [`referencia-unity/`](referencia-unity/README.md).

## Como funciona na Unity, na prática, e como ficou na Astra

**Reflection Probe.** Na Unity o autor cria *GameObject › Light › Reflection Probe*, arrasta as alças da caixa
amarela até as paredes do cômodo (Box Size/Offset), liga *Box Projection* num interior e, na sonda *Baked*,
clica *Bake*: o cubemap vai para o projeto. Uma sonda *Realtime* recaptura sozinha (On Awake, Every Frame ou
por `RenderProbe()`), fatiada no tempo. Cada Mesh Renderer mistura as duas sondas de maior Importance que
contêm o centro dos seus limites e cai no céu fora delas.
Na Astra: **Criar › Luzes › Sonda de reflexo** cria `astra.render.reflection_probe` v1 com o mesmo Inspector
(Tipo, Atualização, Fatiamento no tempo, Importância, Intensidade, Projeção na caixa, Distância de mistura,
Tamanho/Deslocamento da caixa, Resolução 16–256, HDR, Fundo céu/cor, Máscara de culling, Plano próximo e
distante). O viewport mostra a caixa (alinhada aos eixos do mundo, como na Unity), a caixa interna onde a mistura
termina e alças para o tamanho e a mistura. **Assar** captura no aparelho só os objetos Static e grava
`Iluminação/<nome>.aeprobe` (um passo de desfazer liga o recurso). A API C# ganha `ReflectionProbe.RenderProbe()`.

**Light Probe Group.** Na Unity o autor cria *Light › Light Probe Group* (8 sondas num cubo), liga *Edit Light
Probes*, adiciona, duplica, apaga e move sondas onde a luz muda (sol → sombra) e clica *Generate Lighting*. Cada
objeto dinâmico com *Light Probes = Blend Probes* acha o tetraedro do centro dos limites e mistura as quatro sondas.
Na Astra: **Criar › Luzes › Grupo de sondas de luz** (8 sondas num cubo), **Editar sondas** com anterior/próxima,
Adicionar, Duplicar, Apagar, posição local por eixo, toque no viewport seleciona a sonda e alças movem a
selecionada; as arestas da tetraedralização aparecem com *Mostrar arestas*. **Assar sondas** captura cada ponto de
todos os grupos da cena (como o Generate Lighting), projeta em SH L2 e grava `Iluminação/<grupo>.aelightprobes`.
*Remover anéis* tira os lóbulos negativos atrás de luz forte.

**Mesh Renderer › Sondas.** Sondas de luz (Desligado, Misturar sondas) e Sondas de reflexo (Desligado, Misturar
sondas, Misturar sondas e céu, Simples), agora editáveis e lidas por desenho.

## O que roda onde

| Peça | Arquivo | Onde roda |
|---|---|---|
| SH L2, anti-ringing, Delaunay e caminhada entre tetraedros | `renderer/light_probes.cpp` | CPU (host e aparelho) |
| Escolha das 8 sondas do quadro, mistura por objeto, box projection (espelho C++ do shader), recurso `.aeprobe` | `renderer/reflection_probes.cpp` | CPU |
| Extração da cena e `RenderProbe()` | `runtime/scene_probes.cpp`, `runtime/component_operations.cpp` | CPU |
| Atlas octaédrico 8×256² (6 mips), registro por desenho, captura de 6 faces, mips por blit, filtro GGX, leitura para o bake | `platform/android/instanced_probes.inl`, `rhi/shaders/reflection_probe_filter.comp` | GPU do aparelho |
| Box projection por pixel, mistura de 2 sondas + céu, SH no difuso | `rhi/shaders/probe_lighting.glsl` | Fragmento PBR |
| Bake, recursos, Inspector, gizmos, alças, seleção | `editor/editor_probes.cpp`, `editor_screen.cpp`, `editor_component_visuals.h`, `editor_component_handles.h` | Editor |

## Pendente (declarado)

- Lightmaps e GI assada (Mixed/Baked lighting): continuam planejados (`render.light.baked`).
- Reflection Probe tipo Custom, Shadow Distance e Occlusion Culling da captura: não expostos.
- Light Probe Proxy Volume, Anchor Override e Adaptive Probe Volumes: não existem.
- A captura usa as luzes e as cascatas de sombra escolhidas para a câmera principal; uma sonda fora da distância de
  sombra da câmera captura sem sombra do sol.
