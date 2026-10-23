# Cortes de custo da medição de 22–23/10 (23/10/2026)

Branch `claude/friendly-bell-de1e43`, base `17c96512` (codex/ui-play-performance-apk-export).
APKs Astra Dev profileable: `0.3.0-dev.261023999-cortes-1722` (perfil de CPU) e `-cortes-1809`
(teto repassado pelo shell + corte de animação), conferidos com `dumpsys`.

## O que mudou (modelo: Project Settings › Quality → URP Asset por nível)

O nível escolhido pelo projeto agora é **teto** para o que a cena pede, como os campos do URP Asset
que cada nível de qualidade da Unity aponta. O eixo cortado fica registrado com o motivo `quality`
no log `[RenderPolicy]` e aparece no painel Qualidade no próprio valor ("4 → 2 no nível").

| Nível | Render Scale | Sombra do sol | Luzes por objeto | AO (SSAO) | Bloom |
|---|---|---|---|---|---|
| S | ≤ 1,0 | ≤ 4 cascatas @4096, PCF 25 | 8 | cheio | passe único |
| A | ≤ 1,0 | ≤ 4 @2048, PCF 25 | 8 | cheio | passe único |
| B | ≤ 1,0 | ≤ 3 @2048, PCF 9 | 8 | 1/2 resolução | cadeia de mips |
| C | ≤ 0,85, **resolução dinâmica ligada** (alvo = orçamento do quadro) | ≤ 2 @1024, PCF 9 (gather) | **4** | **desligado** | cadeia de mips |

- Máscara de luzes por desenho ligada por padrão em todos os níveis.
- `aether.quality_ceiling=false` reproduz o "antes" no mesmo binário; `aether.post_half_ao` e
  `aether.post_bloom_chain` só sobrepõem o nível quando enviados.

Referências Unity 6 (6000.0): URP Asset — Quality (Render Scale), Lighting (Per Object Limit), Shadows
(Max Distance, Cascade Count, Shadow Resolution):
https://docs.unity3d.com/6000.0/Documentation/Manual/urp/universalrp-asset.html ·
SSAO (Downsample): https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-ssao.html ·
Quality settings: https://docs.unity3d.com/6000.0/Documentation/Manual/class-QualitySettings.html ·
Dynamic Resolution: https://docs.unity3d.com/6000.0/Documentation/Manual/DynamicResolution-landing.html

## Vila Astra, nível C (POCO F7, 1600×720 interno, Play, no carregador)

| Métrica (ms) | Antes | Depois | Δ |
|---|---|---|---|
| FPS interno | 45,92 | 59,23 (preso em 60) | +13,31 |
| **GPU quadro** | **18,91** | **14,33** | **−4,59 (−24%)** |
| CPU processo | 12,77 | 11,20 | −1,57 |
| Opaco | 6,16 | 5,67 | −0,49 |
| Pós | 4,71 | 2,80 | −1,91 |
| Sombra | 2,94 | 0,90 | −2,04 |
| EASU + RCAS | 4,18 | 4,15 | −0,03 |

Uma rodada de 40 s de cada lado com o mesmo APK (`cortes-1809`); antes = `-NoQualityCeiling`
(4 cascatas @4096 PCF 25, AO cheio, bloom antigo, 8 luzes/objeto, sem máscara), depois =
`-ProductDefaults`. Bateria 39,9 °C (antes) e 35,5 °C (depois). Prancha: `prancha-vila-c.jpg`.

Imagem: composição, cor e luz da praça iguais. A borda da sombra fica mais borrada (penumbra ~4×)
e com serrilhado leve no contorno; sem AO os cantos (bancos, base do muro) ficam mais claros.
É a troca esperada para o nível do celular fraco.

## CPU (simpleperf, Vila S, `cpu-vila-s/`)

20 s, 2000 Hz, pilha por frame pointer. 90% das amostras na thread `AstraGame`. Na thread do jogo:

1. **Animação: ~42%** — `EditorPlayScene::animate` 37,5% (`SceneAnimator::advance` 33,4%, 13,4% próprio),
   pose deformada dos personagens 4,2%.
2. **Envio de desenhos: ~28%** — `InstancedRenderer::drawFrame` 20,7% (passe de sombra 4,5%,
   `vkCmdDrawIndexed` 3,6%), extração dos desenhos 7,6%.
3. **Scripts .NET: ~6%** — libcoreclr 2,8%, código JIT 2,2%, ponte ~1%. Física 4,4% e navegação 2,3% vêm depois.

Corte no maior (animação, commit `cb97d956`): ponteiro do repouso por propriedade em vez de
`unordered_map` por osso e quadro (2,1%), e decomposição da transformação atual só quando o clipe
não escreve T, R e S (sincos/atan por osso). Corte seguinte (envio): a varredura de Lens Dirt
(1,6%) passou a olhar só o mundo de Play e a pular quadros sem mutação.

## Pendente

- Vila S antes × depois; Sponza S e C (a G3c ainda está com o Sponza numa variante de teste).
- A/B de CPU do corte de animação no aparelho (`cortes-1722` × `cortes-1809`).
- Rodada do C com resolução dinâmica ligada (`-DynamicResolution`).
