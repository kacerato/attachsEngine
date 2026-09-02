# Orçamento de 120 Hz — CPU, GPU, RAM e VRAM medidos

Tudo neste documento foi medido no Xiaomi `25053PC47G` / SM8735 / Adreno,
Android 16, Release assinado, cena `dirt-road`, **1280×2772 nativo, escala
1,00**, pose fixa do hotspot `-15.71,145.27,-25.72,2.75,0.11`, 30 s por rodada,
`Thermal Status 0`. Nenhum número aqui é estimativa; o que é estimativa está
marcado como tal.

A pose é o **pior caso documentado** da cena, não a média. Poses médias mediram
95–110 FPS em sessões anteriores. O gate de produto é a rota inteira, que exige
uma rota gravada — ainda não disponível neste repositório.

## 1. O alvo, em milissegundos

| Alvo | Frame | Orçamento da engine |
|---|---:|---|
| 120 FPS | 8,333 ms | CPU ≤ 6,500 ms, GPU ≤ 7,333 ms, ~0,5 ms de compositor |
| 115 FPS | 8,696 ms | — |

`FramePolicy` já publica exatamente esses limites em runtime:
`render=120 Hz (8.333 ms), fixed tick=60 Hz, CPU<=6.500 ms, GPU<=7.333 ms`.

## 2. Onde o frame está hoje

Estado atual, três execuções reproduzindo em **0,02 ms**:

| Região de GPU | ms | Observação |
|---|---:|---|
| Shadow (atlas) | 0,001 | cache estático; o atlas quase nunca é redesenhado |
| Culling | 0,001 | oclusão GPU desligada (medida e rejeitada, §6) |
| Opaque + Coverage + Sky + Transparent + UI | 9,05 | um único balde: numa GPU TBDR os timestamps internos ao render pass resolvem no fim do tile |
| Post | 1,10 | passe dedicado, tonemap + FXAA em 3,55 Mpx |
| HZB | 0,001 | produtor desligado |
| **Frame** | **10,16** | **≈ 98 FPS de teto de GPU** |

Faltam **1,83 ms** para 120 Hz nesta pose.

### Decomposição do passe principal

Obtida com dois pontos de resolução e as variantes de `gpu_cost_isolation`, na
mesma pose:

| Termo | ms | Como foi medido |
|---|---:|---|
| Fixo (geometria, binning) | ≈ 4,8 | dois pontos de resolução: 13,927 a 1,00 e 9,932 a 0,75 |
| Por pixel a 1280×2772 | ≈ 9,1 | o mesmo par |
| ├─ material acima de `base-color` | ≈ 5,1 | `full` 13,93 contra `base-color` 8,81 |
| │  ├─ IBL | ≈ 2,1 | `full` contra `no-ibl` 11,86 |
| │  └─ normal/MR/emissivo | ≈ 3,0 | diferença |
| └─ base color, depth, overdraw | ≈ 4,0 | resto |
| Sombra por fragmento | 2,15 | `sombra on` contra `sombra off`, no build atual |

**A restrição dura:** o passe opaco **só com base color** custa 8,811 ms nesta
pose, contra 8,333 ms de frame inteiro a 120 Hz. Nenhum ajuste de shading chega
a 120 Hz aqui. O que resta é reduzir **quantos fragmentos são gerados e
sombreados**, não o custo de cada amostra.

## 3. CPU — sobra 74% do orçamento

| Métrica | ms | Orçamento |
|---|---:|---|
| CPU de processo, média | 1,64 | 6,500 |
| CPU de processo, p95 | 2,51 | — |
| Gravação + submissão | 0,52 | — |
| Present | 0,31 | — |
| **`acquire_wall`** | **10,80** | bloqueada esperando a GPU |

`acquire_wall_ms` acompanha `gpu_frame_ms` quase exatamente. A CPU trabalha
~2,5 ms e passa ~10,8 ms parada: **está ociosa 84% do frame**.

Consequência prática, contra uma recomendação comum: **aumentar frames em voo
não ajudaria**. Sobrepor CPU e GPU só cria vazão quando existe trabalho de CPU
para sobrepor; aqui não existe. Dois ou três `FrameContext` adicionariam
latência toque→pixel sem tocar no tempo de frame. Enquanto `acquire_wall` for
essencialmente `gpu_frame`, esse eixo está fechado.

O que **converte** CPU em FPS é pré-computação offline: cozimento, LOD,
impostores, baking. Isso não aparece como uso de CPU em runtime — aparece como
GPU rasterizando menos.

## 4. Memória — 173 MiB usados, e é aí que está a folga

Memória unificada; não há VRAM separada neste SoC.

Desde o FrameProfile schema 7 essa afirmação deixa de depender de uma leitura
pontual do log: cada janela registra RSS/RAM disponível e o heap Vulkan como
memória unificada ou dedicada, com budget/uso do driver quando
`VK_EXT_memory_budget` existe e uso/pico/limite da engine por classe sempre.
Isso permite verificar crescimento e pressão ao longo da rota/soak; os 173 MiB
abaixo continuam sendo a medição histórica anterior a esse contrato.

| Categoria | MiB | Origem |
|---|---:|---|
| Texturas do mapa residentes | 112,6 | 146 imagens, ASTC 6×6 com cadeias completas de mip |
| Buffers de geometria | ~34,7 | AEMAP v3, vértices de 48 bytes |
| Render targets | ~14,3 | cor offscreen, depth, HZB quando ligado |
| Atlas de sombra | 8,0 | 2 cascatas 1024² D32 |
| Ambiente (céu + especular + BRDF) | 3,46 | 1024×512 + 256² + 128² |
| **Total** | **≈ 173** | de vários GiB disponíveis |

**A folga de memória é enorme e está sendo desperdiçada.** Mas memória só vira
FPS quando **substitui computação**. Alocar mais sem trocar por trabalho não
acelera nada. As trocas possíveis, com o que já foi medido sobre cada uma:

| Troca | Custo de memória | Estado |
|---|---:|---|
| Impostores octaédricos de árvore distante | ~50–100 MiB | **não implementado** — ataca os 4,8 ms fixos e o overdraw de folhagem ao mesmo tempo; é o maior item restante |
| Lightmap / AO assado para estático | ~30–60 MiB | **não implementado** — trocaria os 2,15 ms de sombra por fragmento por uma busca de textura |
| Atlas de sombra maior com menos amostras | +25 MiB | **medido, não paga** (§6) |
| Cadeia de LOD residente | já pago | ativa por padrão |

## 5. O que foi aceito e está ligado

| Mudança | Ganho medido | Como foi validado |
|---|---:|---|
| Ordem de profundidade nos lotes indiretos + PCF de hardware com 4 buscas | **−2,14 ms (−17,4%)** | A/B intercalado, controles reproduzindo em 0,049 e 0,062 ms, confirmado numa segunda pose (12,987 → 10,52) |
| `game_mode_config` declarado | 0 ms de média, **variância eliminada** | `game_mode=2` passou a ser determinístico em três execuções; antes alternava com STANDARD, e a diferença entre os dois modos era 7,284 contra 10,204 ms |
| Early-out do hemisfério oposto em `directLight` | dentro do ruído | entra por correção semântica |
| Projeção do ambiente como specialization constant | dentro do ruído | remove o ramo equirect morto do binário |

O `game_mode_config` merece destaque: ele não subiu a média, mas **7,284 ms
(114 FPS) foi medido nesta cena, nesta pose, em resolução nativa e qualidade
completa**. O teto existe; o que falta é o SoC permanecer no estado de clock que
o produz.

## 6. O que foi medido e rejeitado

Registrado para que ninguém gaste a semana seguinte nisso.

| Tentativa | Resultado |
|---|---|
| Oclusão GPU-driven (HZB + culling em compute) | Kernel funciona (244 testados, 28 ocluídos, imagem idêntica), mas o produtor custa +0,99 ms e o consumidor não devolve: frame +1,28 ms |
| Corte de cards alfa no cooker | Rodado sobre o GLB real: mediana de ganho 9,1% por card, média ponderada 4,83%, **1,8% de área removida na malha**. Os cards já vêm justos e 41.362 quads compartilham vértices |
| Prepass de cobertura seletivo por área projetada | Limiar não move nada: 12,978 ms com zero contra 12,921 ms com 16.384 px |
| Atlas de sombra 2048² com 1 amostra | 7,479 contra 7,284 do controle no mesmo estado de clock; e 10,234 contra 10,204 no outro. Eixo esgotado |
| Laço de PCF com limites dinâmicos | Pior que o 5×5 predicado: sem contagem de voltas conhecida o compilador não desenrola e as buscas serializam |
| Remover o passe de pós (tonemap inline) | 21,222 contra 15,239 ms. Desenhar direto no swapchain é **pior**; hipótese é pré-rotação da surface, exige AGI |
| Cadência adaptativa | 8,90 → 22,11 ms, com o aparelho mais frio |
| VRS | Indisponível: o device reporta `vrs=0` |

## 7. Higiene de medição — o que invalida uma rodada

Aprendido do jeito caro nesta sessão.

- **Overlay GameTurbo do Xiaomi.** Abriu sobre o app com "Wild Boost" ativo. A
  mesma configuração passou a variar 6 ms e `sombra off` chegou a medir mais
  lento que `sombra on`, o que é fisicamente impossível. A captura de tela
  mostrou o painel; 78% dos pixels divergiram do controle por causa dele.
- **Game Mode.** STANDARD contra PERFORMANCE valia 40% do tempo de frame. Agora
  é determinístico, mas o runner ainda não recusa uma janela por modo divergente.
- **Estados de clock.** O mesmo trabalho mediu 7,28 / 10,16 / 12,95 ms em
  momentos diferentes. Só comparações intercaladas dentro da mesma sessão, com
  controle nos dois lados, sustentam uma conclusão.
- **Primeira execução após `adb install`** é sistematicamente pior.

O runner precisa gravar GameTurbo, Game Mode e estado de gravação de tela como
contexto e recusar a janela quando divergirem — hoje ele produz números que
parecem válidos.

## 8. Impostores de folhagem — implementados e medidos (02/09/2026)

O item 1 da lista anterior saiu do papel: `tools/bake-foliage-impostors.py`
rasteriza offline cada grupo de LOD alpha-tested num tile e o emenda no AEMAP v3
como o nível mais grosseiro do grupo. `MapMaximumLodLevels` subiu de 3 para 4
(três níveis do cooker mais o impostor) e `dirt_road.vert` gira posição, normal e
tangente em torno de Y para o quad encarar a câmera.

**O que está no pacote:** 212 impostores, atlas 2048×2048 R8G8B8A8_SRGB com 12
mips (21,33 MiB) mais fallback 512² (1,33 MiB); +212 draws, +848 vértices,
+1 textura, +1 material. RGBA8 e não ASTC porque `astcenc` não está disponível
nesta máquina; trocar divide esses bytes por ~4 e nada mais muda.

**A primeira versão não funcionava, e o motivo importa.** Ela cobrava o raio do
grupo como erro geométrico — o quad achata tudo num plano, então "erra por isso".
Medido, esse número torna o impostor **inalcançável**: com raio mediano de 32,9
num mapa de 551 unidades, a distância de troca cai em 2.470 unidades e **zero
impostores são selecionados em qualquer pose deste mapa**. E é cobrar errado, não
só cobrar caro: o erro que a seleção de LOD modela é desvio de silhueta, e um
quad que gira para encarar a câmera reproduz exatamente a silhueta assada. O
achatamento produz parallax errada quando a câmera anda de lado — um erro que não
escala com 1/distância e que essa métrica não representa. O que ela representa é
a resolução do impostor: um texel do tile vale N unidades de mundo. O erro passou
a ser 1,25× o nível anterior com piso no texel do atlas (min 1,417, p50 3,843,
máx 19,616), e aí o impostor entra.

**Onde ele demonstravelmente age:** numa pose de borda o pool de triângulos
selecionado por LOD cai de 229.502 para 172.921 (**−24,7%**), com 665 candidatos
contra 651.

**Onde ele não age, e por quê:** na pose do hotspot, **nenhum** impostor é
selecionado — a cadeia de cobertura inteira fica no nível 0 ali. O nível 1
exigiria 44–76 px de erro projetado contra 24 px de orçamento efetivo. Não é o
orçamento que está apertado: a folhagem do hotspot está **perto** (distância
mediana do ponto mais próximo: 77 unidades). Trocar a distância da esfera pela
distância da AABB do grupo foi verificado e não muda um único grupo. A hipótese
da revisão anterior — "impostores atacam os 4,8 ms fixos do hotspot" — está
**medida e refutada**: o que enche aquela tela é folhagem de perto, que nenhum
impostor pode substituir.

**A/B em hardware,** pose de vista de floresta `307.9,160,-308.1,5.236,0`, oito
rodadas intercaladas de 30 s: GPU p95/budget com impostores {1,313 1,316 1,700
1,169} contra {1,519 1,124 1,165} sem. Os dois controles discordam entre si em
2,9 ms — maior que o efeito procurado. **O ganho de triângulos é real e medido; a
conversão em tempo de GPU fica dentro do ruído de estado de clock nesta sessão.**
`aether.disable_foliage_impostors` (e `-DisableFoliageImpostors` no runner)
devolvem o pacote sem impostores no mesmo binário, para o A/B não exigir dois
APKs de 334 MiB.

## 9. Onde os 120 Hz fecham hoje

Pose do hotspot, mesma sessão, Release assinado, `game_mode=performance`,
`Thermal Status 0`:

| Configuração | FPS apresentado | GPU p95 | Escala |
|---|---:|---:|---:|
| Escala nativa 1,00 | 84,7 | 11,22 ms | 1,00 |
| Resolução dinâmica, piso do perfil (0,58) | 97,6 | 8,67 ms | 0,58 |
| **Resolução dinâmica, piso 0,50** | **111,1** | **7,68 ms** | **0,50** |
| Piso 0,50 + orçamento de cobertura 64 px | 111,1 | 7,68 ms | 0,50 |

Decomposição no piso 0,50: opaco **5,68 ms**, pós **1,72 ms**, frame **7,41 ms**
de média contra 7,333 de orçamento. O pós custa 23% do frame e **não encolhe com
a escala** — ele lê o alvo reduzido e escreve o swapchain inteiro (3,55 Mpx). Sem
FXAA nem bloom nem sharpen, é uma busca de textura por pixel: o custo é banda,
não ALU. Remover o passe já foi medido e é pior (§6).

Subir o orçamento de LOD de cobertura de 32 para 64 px não move nada
(1,0476 contra 1,0145): a folhagem perto continua sendo desenhada.

**Conclusão medida: 120 Hz nesta pose não existe em 1280×2772.** O passe opaco só
com base color custa 8,811 ms contra 8,333 ms de frame inteiro. O que existe é
110–111 FPS com a resolução interna em 0,50, e é isso que uma política
`Mobile120` deve resolver sozinha — não uma flag de linha de comando.

## 10. Caminho restante

1. **Piso de resolução por perfil de dispositivo.** O controlador trava o piso em
   0,50 (`dynamic_resolution.cpp`); o perfil deste aparelho pede 0,58 e mede 97,6
   FPS, contra 111,1 no piso 0,50. É o único eixo que mediu ganho de dois dígitos.
2. **Rota gravada e medição de média.** O alvo do produto é a média, e hoje só
   existe a pior pose. Sem isso não há gate.
3. **Baking de iluminação estática.** Troca os 2,15 ms de sombra por fragmento
   por uma busca.
4. **Budget `Mobile120` no CI**, reprovando commit que ultrapasse GPU p95 de
   7,2 ms — para não perder o que já foi ganho.
5. **ASTC no atlas de impostores**, quando `astcenc` estiver disponível: 21,33
   MiB viram ~5,3 MiB sem mudar mais nada.
