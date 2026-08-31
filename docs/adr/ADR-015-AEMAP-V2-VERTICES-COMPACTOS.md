# ADR-015 — AEMAP v2 com vértices compactos

- **Estado:** aceito para o pipeline atual; validação longa e matriz Mali pendentes
- **Data:** 30/08/2026

## Problema

O AEMAP v1 armazenava posição, normal, tangente, dois UVs e cor integralmente em
`float32`, com stride de 72 bytes. A floresta usa 424.849 vértices; eram 29,17 MiB de
atributos lidos pela entrada de vértices, apesar de normal/tangente serem normalizados
e a cor desta cena já ser representável exatamente em 8 bits. Isolar todo o fragment
PBR não reduziu o tempo GPU, enquanto os bounds grandes não permitem descarte útil por
draw na câmera do hotspot.

## Decisão

Criar AEMAP v2 com stride de 48 bytes:

| Atributo | Formato Vulkan | Offset | Motivo |
|---|---|---:|---|
| posição | `R32G32B32_SFLOAT` | 0 | preserva extensão e precisão espacial |
| normal | `R16G16B16A16_SNORM` | 12 | direção normalizada; W reservado |
| tangente | `R16G16B16A16_SNORM` | 20 | preserva direção e sinal W |
| UV0 | `R32G32_SFLOAT` | 28 | UVs chegam a −75,9; float16 alteraria fase em até 0,031 |
| UV1 | `R32G32_SFLOAT` | 36 | mesma regra de UV0 |
| cor | `R8G8B8A8_UNORM` | 44 | corpus atual é representado exatamente |

O decoder continua aceitando AEMAP v1/stride 72 e rejeita pares mistos de
versão/stride. O cooker produz v2 por padrão. O shader não ganha variante: a conversão
para `vec` ocorre na entrada fixa do pipeline. Não há regra por cena, aparelho ou
preset de qualidade.

## Alternativas rejeitadas

- **Float16 para UV:** economizaria mais 8 bytes, mas o erro máximo medido de 0,0312
  pode deslocar texturas repetidas visivelmente.
- **10:10:10:2 para normal/tangente:** menor ainda, porém traz mais erro e benefício
  não atribuído; SNORM16 já remove 33% do stride.
- **Frustum culling por draw agora:** as 27 esferas conservadoras intersectam o
  frustum do hotspot; zero dos 341.109 triângulos seria descartado.
- **Subdividir CPU em centenas de draws:** a tentativa anterior com 396 draws caiu
  para 25,64 FPS. Granularidade fina depende de cena persistente e indirect.

## Evidência e impacto

- `scene.aemap`: 34.688.380 → 24.491.996 bytes (−10.196.384; −29,4%).
- APK Release assinado: 402.443.803 → 392.244.763 bytes.
- erro de quantização máximo: 1,529e−5 em normal/tangente; cor sem erro.
- comparação v1/v2: 523/3.548.160 pixels diferentes (0,0147%), máximo 1/255 e
  erro RGB médio 0,000049.
- dois runs v2: GPU média 8,887/7,420 ms, engine 99,395/117,294 presents/s e
  SurfaceFlinger 110,202/111,855/s. Os controles v1 imediatamente anteriores ficaram
  em GPU 9,715/11,040 ms, engine 92,148/82,124 e SurfaceFlinger 93,446/91,612/s.

Os runs são evidência inicial em um Adreno, não substituem A/B longo, soak nem a matriz
Mali/C. A compatibilidade v1 e o gate visual são permanentes; compactação adicional só
entra com erro visual e counters medidos.
