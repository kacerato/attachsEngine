# G3c — lightmap assado e UV2 automática (23/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, seção G3 › G3c, itens 6 a 8.
Branch `claude/agitated-hodgkin-0a80d5`, base aee32953.
Referências (Unity 6000.0 e Godot 4.5):
[Progressive Lightmapper](https://docs.unity3d.com/6000.0/Documentation/Manual/progressive-lightmapper.html),
[Lighting window](https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html),
[Lighting Settings asset](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightingSettings.html),
[Generating lightmap UVs](https://docs.unity3d.com/6000.0/Documentation/Manual/LightingGiUvs-GeneratingLightmappingUVs.html),
[Light Mode](https://docs.unity3d.com/6000.0/Documentation/Manual/LightModes.html),
[Mesh Renderer › Lighting](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html),
[LightmapGI (Godot)](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html),
[xatlas](https://github.com/jpcy/xatlas) (MIT, commit f700c77).
Frames de vídeo da Unity: [`referencia-unity/`](referencia-unity/README.md).

## Como funciona na Unity, na prática, e como ficou na Astra

**UV2 (item 6).** Na Unity o autor liga *Generate Lightmap UVs* no Model Import Settings (Hard Angle 88°, Pack
Margin 4 px) e reimporta. Na Astra: importação do modelo › **Gerar UV de lightmap** com **Ângulo vivo** e
**Margem entre ilhas** (perfil de importação schema 13). O xatlas desembrulha cada malha estática em ilhas sem
sobreposição; as arestas acima do ângulo viram costura. Malhas com skin ou blend shape ficam de fora.

**Generate Lighting (item 7).** Na Unity: objetos *Static* com *Contribute Global Illumination*, luzes em modo
*Baked* ou *Mixed*, e *Window › Rendering › Lighting › Scene › Generate Lighting*; a barra de progresso corre em
segundo plano e *Clear Baked Data* desfaz. Na Astra: janela **Iluminação › Lightmaps** com o cartão do estado
(em dia, desatualizado, sem bake), a contagem da cena (Static com GI, recebem lightmap, sem UV2, luzes
assadas/mistas), **Gerar iluminação** / **Cancelar** / **Limpar** e as **Configurações de iluminação** da cena
(Resolução do lightmap, Espaçamento, Tamanho máximo, Amostras diretas/indiretas/do ambiente, Rebotes, Remover
ruído, Comprimir, Modo direcional Não direcional, Intensidade indireta, Reforço de albedo). O bake roda numa
thread própria com trabalhadores de prioridade baixa; o editor continua editável. O resultado vai para
`Iluminação/<cena> Lightmap.aelightmap` e as sondas de luz da cena são assadas logo depois, com os lightmaps já
no lugar (os objetos que se movem recebem a luz assada pelas sondas do PK-6, como no Generate Lighting da Unity).

**Mesh Renderer e Luz.** Mesh Renderer v12: **Contribuir para iluminação global** e **Receber iluminação global**
(Lightmaps / Sondas de luz). Luz: **Modo** Tempo real / Misto / Assado com efeito real — Assado grava luz e
rebote e sai do tempo real nos desenhos com lightmap; Misto assa só o rebote; Tempo real não entra no bake.

**Dia e noite (item 8).** O lightmap guarda, por texel, a irradiância das luzes locais assadas (RGB) e a
visibilidade do céu (A, céu branco de radiância 1 com os rebotes dele). O sol e a lua da Hora do dia nunca são
assados. No fragmento, o ambiente difuso vira `RGB/π + ambiente_do_céu_da_hora(n) × V`, e o reflexo do céu é
ocluído por `V`: debaixo do beiral e dentro da arcada o céu não acende nada, e a cor do céu de cada hora continua
mandando — o lightmap não congela o meio-dia.

## O que roda onde

| Peça | Arquivo | Onde roda |
|---|---|---|
| UV2 automática (xatlas) | `resources/lightmap_uv_generate.cpp`, `third_party/xatlas` | Importação (host e aparelho) |
| BVH, rasterização em UV2, direta, rebotes, céu, denoise, dilatação, compressão | `renderer/lightmap_baker.cpp` | CPU, thread do bake + trabalhadores |
| Recurso `.aelightmap` e consulta (objeto, slot) | `renderer/lightmap_data.cpp` | CPU |
| Cena do bake (malha, pose, material, cor base) | `editor/editor_map_scene.cpp` (`collectLightmapScene`) | Editor |
| Job, gravação, Limpar, estado da janela | `editor/editor_lightmaps.cpp` | Editor |
| Array de páginas (binding 26) e registro por desenho | `platform/android/instanced_lightmaps.inl`, `instanced_probes.inl` | GPU do aparelho |
| Amostragem e reescala pelo céu da hora | `rhi/shaders/probe_lighting.glsl`, `dirt_road_shading.glsl` | Fragmento PBR |
| Luz Assada fora do tempo real nos desenhos com lightmap | `renderer/punctual_lights.h` (`PunctualLightFilter::baked`) | CPU por desenho |

## Testes no host

`aether_lightmap_tests` (também dentro de `aether_tests` e `aether_environment_authoring_tests`): UV2 do cubo sem
sobreposição e com as costuras do Hard Angle, compactação, perfil schema 13 e leitura do 12, ida e volta do
recurso nos dois formatos, lei do inverso do quadrado e sombra de uma luz assada, céu sob um telhado, Misto ×
Assado, emissivo, cancelamento, filtro da luz Assada e migração do Mesh Renderer v11 e das Configurações v1.

## Validação no aparelho

(preenchido na validação no POCO F7)
