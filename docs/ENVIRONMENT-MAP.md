# Environment Map

## Objetivo

`EnvironmentMap` é um resource de renderização global, independente de cena e de
backend. Ele separa três usos que não devem compartilhar formato ou custo por
conveniência:

1. panorama visível do céu;
2. radiância especular pré-filtrada por roughness;
3. integração BRDF do modelo split-sum.

A floresta `dirt-road` é somente o primeiro consumidor e o teste de stress. O
renderer não consulta nome de cena, preset ou aparelho para escolher o caminho.

## Pipeline offline

`tools/cook-sky-panorama.py` executa o trabalho caro na importação:

- preserva `environment.aetex` como panorama equiretangular RGBA8 sRGB para o céu;
- converte a radiância para `environment-specular.aetex`, octaédrico RGBA16F
  256x256 com 9 mips GGX (roughness crescente por mip);
- integra `environment-brdf.aetex`, RGBA16F 128x128, com 512 amostras por texel;
- grava dimensões, projeção, níveis e flags no trailer AEEN v3;
- atualiza hashes e metadados do manifesto.

O runtime apenas valida, carrega e amostra. Ele não reconverte panorama, não
convolui GGX e não constrói LUT durante abertura da cena.

## Formato AEEN v3

O cabeçalho continua little-endian:

| Offset | Tipo | Conteúdo |
|---:|---|---|
| 0 | `u32` | magic `AEEN` |
| 4 | `u32` | versão (`3`) |
| 8 | `u32` | tamanho total (`176`) |
| 12 | `u32` | reservado |
| 16 | 8 x `vec4` | payload estável de iluminação (128 bytes) |
| 144 | 8 x `u32` | projeção, largura/altura/mips especulares, largura/altura/mips BRDF e flags |

AEEN v1 e v2 continuam legíveis e resolvem explicitamente para o panorama
equiretangular legado. Arquivos v3 inválidos falham com log de contexto; não há
fallback silencioso para dados corrompidos.

## Contrato de runtime

`renderer/environment_map.*` contém apenas descrição portátil, validação/migração
e a chave compacta de features de material. Paths e import settings pertencem ao
Asset Database; imagens, samplers, descritores e ownership Vulkan permanecem no
backend Android atual.

No descriptor set global de ambiente:

- binding 0: UBO de ambiente/frame;
- binding 1: panorama visível/legado;
- binding 2: atlas de sombra;
- binding 3: radiância especular octaédrica;
- binding 4: LUT BRDF.

A projeção octaédrica remove `atan` e `acos` do caminho PBR quente. O panorama
equiretangular mantém essa trigonometria somente no fallback legado/sky. O LOD da
radiância é derivado de roughness e limitado pelo número de mips serializado.

## Configuração global e variantes

Os recursos disponíveis e a política de qualidade são dimensões separadas:

- `ambient.specularProbe` decide se a sonda participa da iluminação;
- `ambient.splitSumBrdf` escolhe LUT split-sum ou aproximação analítica;
- `materialShaderVariants` permite pipelines especializados para presença de
  normal, metallic-roughness e emissive;
- as distâncias de normal, MR, emissivo e sonda continuam independentes e com fade.

Os oito valores da chave de material são um limite explícito desta primeira
família, não autorização para multiplicação irrestrita de pipelines. Alpha mode,
double-sided, backend, MSAA e shadow mode são dimensões separadas. O cache/orçamento
persistente de variantes ainda é um gate de produto.

No Adreno de referência, especializar todas as combinações piorou o perfil B. Por
isso variantes ficam desligadas nos perfis B/C e configuráveis em Project Settings;
S/A podem habilitá-las. Uma variante só pode virar default de um perfil depois de
A/B no hardware-alvo.

## Lifecycle, memória e fallback

- imagens e samplers pertencem a `DirtRoadResources` nesta integração e são
  destruídos no mesmo lifecycle dos demais recursos do pacote;
- o upload respeita cancelamento, limites de dimensão e budgets separados;
- ausência dos novos assets em AEEN v1/v2 usa o caminho legado;
- AEEN v3 que anuncia uma feature exige o asset correspondente e falha de forma
  explícita se ele não puder ser carregado;
- capability/política podem desligar o uso, mas nunca reinterpretar formato.

## Validação de referência (Xiaomi 25053PC47G)

Release assinado, pose `-15.71,145.27,-25.72,2.75,0.11`, 120 Hz, escala fixa
0,58, resolução dinâmica desligada, LOD ligado e variantes de material desligadas:

| Caminho | FPS exibido | GPU média/p95 | Opaco médio | CPU média/p95 |
|---|---:|---:|---:|---:|
| sem IBL (controle) | 108,58 | 7,79 / 7,99 ms | 6,50 ms | 1,40 / 2,12 ms |
| octaédrico + BRDF analítica | 107,03 | 8,04 / 8,31 ms | 6,75 ms | 1,40 / 2,42 ms |
| octaédrico + LUT split-sum | 105,04 | 8,04 / 8,38 ms | 6,76 ms | 1,34 / 2,23 ms |

O custo observado da iluminação especular é aproximadamente 0,25 ms de GPU nesta
pose. A diferença média entre as duas BRDFs ficou abaixo do piso de ruído. As três
execuções foram classificadas como GPU-bound, Thermal Status 0, sem pressão térmica.
Isso não fecha 120 FPS sustentados nem a matriz Mali.

## Próximos gates

1. mover o resource do loader do sample para Asset Database/Resource lifecycle
   genérico, com import settings e referências por AssetId;
2. adicionar irradiância difusa SH9 derivada da mesma fonte;
3. reflection probes locais, blending, parallax correction e orçamento de updates;
4. cache persistente e orçamento de variantes, aquecimento fora do frame;
5. regressão visual multipose (SSIM/FLIP), rota de 60 s, soak de 30 min e matriz
   Adreno/Mali;
6. captura AGI para separar bandwidth, textura, ALU e register pressure do passe
   opaco antes da próxima otimização.

