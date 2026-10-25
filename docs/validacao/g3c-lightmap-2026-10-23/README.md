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
Assado, emissivo, cancelamento, filtro da luz Assada, migração do Mesh Renderer v11 e das Configurações v1,
instância espelhada, enrolamento contra a normal, normal por inversa-transposta e malha fechada double-sided com
normais para dentro (13 cenários).

## Validação no aparelho

POCO F7, APK `0.3.0-dev.261024999-lightmap-0924` (commit 0daadc5a), 24/10/2026.
Prancha: [`prancha-unity-x-poco.png`](prancha-unity-x-poco.png); capturas em [`poco/`](poco/).

**Vila Astra (variante de teste, devolvida ao original depois).** 140 objetos Static com GI, 15 sem UV2 válida,
14 luzes locais em Misto, Configurações 8 texels/m, 1024, 2 rebotes, 128/128/16 amostras.

| Medida | Resultado |
|---|---|
| Importação com UV2 automática ao abrir | ~30 s para os 21 modelos |
| Bake no aparelho | 32,3 s, 125 receptores, 1,0 M texels, 282 M raios, 3 páginas de 1024 (12,0 MB, RGBA8 comprimido) |
| Editor durante o bake | editável; barra "Luz indireta e céu · rebote 2 de 2 · 71%" e tempo restante (V4) |
| Desenhos com lightmap | 125 |
| Visual (V1 × V2) | com lightmap, a fachada sob o beiral e as quinas perdem o céu (V baixo) e o pátio aberto continua igual; nenhum objeto acende na sombra |

**Sponza (23/10, APK lightmap-0203).** Bake de 307 objetos em ~47–50 s no aparelho (0,7 M texels, ~195 M raios,
2 páginas, 8 MB). As manchas escuras do Sponza vinham dos decalques transparentes do térreo e do 1º andar
tapando o céu: slots com material Blend não ocluem nem recebem (fbe930e8).

**Defeito achado e corrigido no aparelho.** A primeira geração da Vila (APK 0203) deixou as copas das 14 árvores
com V≈0: a malha da copa na Vila.glb é fechada, com enrolamento e normais para dentro e material double-sided,
então o texel integrava o interior da copa. Correção 0daadc5a: em material double-sided, uma sondagem de 16 raios
inverte o lado do texel quando o lado da normal está fechado pelo próprio objeto e o outro lado vê claramente mais
céu. Depois dela a copa ficou com V 0,2–0,3.

**Lacunas abertas (não validadas).**
- As copas aparecem pretas **também sem lightmap** (pixel 0,0,0 em V1): o caminho de tempo real não sombreia a
  face de trás de material double-sided com normais para dentro. É defeito anterior ao G3c e fica de fora.
- A prancha da Vila é de dia. O rebote das lanternas Mistas aparece à noite, e a caminhada de 24 h sem salto
  de luz ainda não foi capturada no aparelho.
- Sponza lado a lado com e sem lightmap depois da correção dos decalques não foi recapturada: a Sponza original
  foi devolvida ao G1b.
- Custo por quadro do lightmap (A/B com Limpar) não foi medido no profiler.

### Rodada automática da Sponza (25/10, APK lightmap-0443, commits a95528c6..ae5c40f9)

Roteiro `rodada2.sh` sem a sessão: 5 min sem ligação (telephony e modo de áudio) antes da trava, foco conferido a
cada toque, vista salva "G3c arcada norte", exposição Fixa no EV100 que a automática escolheu em tempo real (-3,56)
e reaproveitamento do quadro desligado (`aether.disable_editor_scene_reuse`) para a GPU desenhar a cena.
Cena: 7 luzes pontuais Assadas com sombra (3 coloridas, diante de estandartes) + 1 Mista, todas a 1200 lm (U1).

| Medida | Tempo real | Com lightmap |
|---|---|---|
| GPU por quadro (média de 6 janelas de 120 quadros) | 17,25 ms | 15,56 ms (−1,7 ms, −10%) |
| Desenhos / triângulos | 200 / 731,8 mil | 200 / 731,8 mil |
| Desenhos com lightmap | 0 | 307 |

- Manchas brancas serrilhadas (prancha anterior) eram os decalques transparentes fora do bake: agora recebem o
  lightmap da parede embaixo sem bloquear nem rebater; sumiram.
- "0 desenhos · 0 triângulos" era o quadro reaproveitado com a vista parada, não o contador; a medida anterior de
  GPU (13–14 ms dos dois lados) media esse quadro reaproveitado e não vale.
- Com a mesma exposição a imagem assada é mais escura: o céu entra ocluído pela visibilidade assada debaixo da
  arcada (fisicamente certo), e 1200 lm ilumina pouco perto do ambiente sem oclusão do tempo real.

**Defeito aberto.** Objetos que recebem pelas sondas (esfera móvel, estandartes sem UV2) saem claros demais ao lado
do entorno assado: as sondas são assadas sem a luz Assada e não acompanham a oclusão do céu do lightmap.

### Rodada noturna com sondas assadas (25/10 06:17, APK lightmap-0538, commit 99aa9ce0)

Mesma cena às 21 h (céu quase apagado: a luz das lanternas domina), EV100 fixo -6 (a automática bateu no
limite; mediu -8,05). Prancha: [`prancha-sponza-noite-unity-x-poco.png`](prancha-sponza-noite-unity-x-poco.png).

| Medida | Tempo real | Com lightmap + sondas |
|---|---|---|
| GPU por quadro | 18,58 ms | 13,18 ms (−5,4 ms, −29%) |
| Bake no aparelho | — | 156 s (8 luzes com sombra suave, 50 sondas, 0,69 M texels, 304 M raios) |
| Luminância média da imagem (0–255) | 64 | 34 |

- Teste de host de paridade: 1 luz pontual de 1200 lm, assado × tempo real = −1,5% / +2,2% (compressão e filtro
  ligados); plano aberto V = 1,000. Lúmen→intensidade, π e codificação estão certos.
- As cores das luzes aparecem nas paredes nos dois lados; a esfera móvel recebe a luz assada pelas sondas (amarela,
  não mais estourada).
- **Ainda aberto:** à mesma exposição o assado fica com ~metade da luminância média. Não é escala (paridade de
  host; o lightmap do aparelho chega a 0,06, a faixa esperada a ~1,5 m de 1200 lm). Candidatas não isoladas:
  o especular GGX das luzes em tempo real (o bake não guarda especular, como na Unity), vazamento de sombra das
  8 luzes pontuais no atlas em tempo real, e a oclusão (AO) que multiplica só a parte assada.

### A/B de paridade no host (25/10, commit f64861d6)

`g3c_lightmap_host_bake` lança 117×100 raios pela vista "G3c arcada norte" sobre a geometria da Sponza (21 h,
7 Assadas + 1 Mista a 1200 lm) e soma, por pixel, o que cada caminho entrega. Uma variável por vez:

| Variável | Efeito |
|---|---|
| Assado (lightmap + direta da Mista) ÷ direta difusa de tempo real com sombra | 2,29× (o assado soma o rebote) |
| Sombras das pontuais desligadas no tempo real | +7% |
| Especular GGX das luzes no tempo real (rugosidade 0,6) | +5% |
| AO no assado | nenhum: os 28 materiais da Sponza não têm occlusionTexture |
| Difuso Burley do shader ÷ Lambert do lightmap (rugosidade 0,8) | 0,85× (escurece o tempo real) |
| Parede esquerda, canal verde: tempo real × assado | 0,0051 × 0,0086 |

Nenhuma variável do material ou da luz deixa o tempo real mais claro que o assado: a cor verde e o rebote
estão no bake. A metade de luminância vista no aparelho à noite vem do céu: com a exposição no limite (EV100 −6)
o pouco céu noturno pesa, e no tempo real ele entra sem oclusão debaixo da arcada, enquanto o assado o multiplica
pela visibilidade (0,03–0,07 no chão da arcada). A prova no aparelho com o ambiente zerado nos dois lados
(`ambiente=0`) foi interrompida pela queda do adb às 07:02 e fica pendente.

### Fechamento

- Base 818433ca mesclada (4f513f12): atlas de ícones reempacotado (414), SPIR-V regenerado, 77 capacidades.
- `aether_tests` 1714/1714, `aether_environment_authoring_tests` 100/100, `aether_lightmap_tests` 15/15
  (executáveis de 25/10 08:18).
- Roteiro do aparelho: guarda de ligação (telephony e modo de áudio) e primeiro plano antes de cada toque; a trava só
  sai com a Sponza conferida por md5.
