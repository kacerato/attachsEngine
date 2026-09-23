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

## Formato AEEN v4

O cabeçalho continua little-endian:

| Offset | Tipo | Conteúdo |
|---:|---|---|
| 0 | `u32` | magic `AEEN` |
| 4 | `u32` | versão (`4`) |
| 8 | `u32` | tamanho total (`320`) |
| 12 | `u32` | reservado |
| 16 | 8 x `vec4` | payload estável de iluminação (128 bytes) |
| 144 | 8 x `u32` | projeção, largura/altura/mips especulares, largura/altura/mips BRDF e flags |
| 176 | 9 x `vec4` | irradiância difusa SH L2, RGB por coeficiente e `w` reservado igual a zero |

AEEN v1, v2 e v3 continuam legíveis. V1/v2 resolvem explicitamente para o
panorama equiretangular legado; v3 preserva a radiância especular e mantém o
hemisfério difuso anterior. Nenhuma dessas versões recebe coeficientes
inventados durante a leitura. Arquivos v4 inválidos falham com log de contexto;
coeficiente não finito ou padding diferente de zero também é recusado.

O cooker projeta a imagem linear que efetivamente alimenta o Environment Map em
nove harmônicos esféricos reais, ortonormais e com Y para cima. Cada texel é
ponderado pelo ângulo sólido exato da sua faixa e cada banda recebe a convolução
do cosseno lambertiano (`π`, `2π/3`, `π/4`). Assim, uma radiância constante `L`
produz irradiância `πL`; não há cor de ambiente sintética gravada como se viesse
da HDRI. Fontes HDR/EXR preservam a radiância acima de 1 no cálculo; uma fonte
LDR continua sendo tratada honestamente como radiância linear limitada da
própria imagem.

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

A irradiância SH9 não cria textura nem binding. O loader valida os coeficientes
no AEEN e os copia como nove `vec4` no fim do UBO do quadro, preservando todos os
offsets anteriores. O shader avalia SH somente quando o recurso declara a
capacidade, o perfil mantém ambiente hemisférico e o céu resolvido é HDRI. Céu
atmosférico, perfil constante e AEEN legado continuam no caminho hemisférico.
O resultado SH é irradiância e entra no BRDF difuso dividido por `π`; força do
ambiente, escala física do céu e override Difuso indireto continuam no ganho
comum já resolvido pelo runtime.

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

## Referência de recurso em C# durante Play

O mesmo binding declarado no schema do componente atende Inspector e scripts. Para um
`Component` de tipo `ComponentIds.Environment`, `GetResource("profile")` e
`GetResource("environment_map")` devolvem o `AssetGuid` de 128 bits; `SetResource`
altera apenas o mundo em Play. Exemplo mínimo:

```csharp
var environment = Object.GetComponent(ComponentIds.Environment)
    ?? throw new InvalidOperationException("Ambiente ausente");
var previous = environment.GetResource("environment_map");
environment.SetResource("environment_map", new AssetGuid(0x0123456789abcdef, 0xfedcba9876543210));
```

O runtime valida mundo, geração, instância, propriedade, slot, existência no registro de
assets e tipo (`EnvironmentMap` ou `EnvironmentProfile`). GUID desconhecido ou recurso
de outro tipo é recusado; GUID vazio remove a referência. A mutação não grava o
documento autoral e Stop descarta o mundo de execução.

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

## Atmosfera física autorável

O componente `Environment` v7 e o `EnvironmentProfile` v3 preservam os modos
HDRI e Atmosfera existentes e acrescentam `PhysicalAtmosphere` como seleção
opt-in. O mesmo contrato de volumes mistura intensidade, densidades de ar e
aerossóis, anisotropia, raio do planeta, altura do observador, escalas de altura,
altura da atmosfera e albedo médio do solo. O modo de alta qualidade é discreto
e troca quando a influência do volume alcança metade.

O sky pass integra espalhamento simples Rayleigh/Mie numa esfera, com densidade
exponencial, sombra do planeta e extinção do disco solar. A distribuição cúbica
dos segmentos concentra amostras na baixa atmosfera: 8×4 amostras no modo móvel
e 16×8 no modo alto. A intensidade física usa a intensidade direcional já
resolvida pelo renderer; `skyScale` continua exclusivo dos caminhos HDRI e
Atmosfera legado, evitando aplicar a escala solar duas vezes.

A referência de autoria é o Physically Based Sky do HDRP, mas a implementação
é deliberadamente menor para Vulkan mobile. O HDRP pré-calcula espalhamento em
texturas 3D e oferece múltiplos bounces, ozônio/tints e ambiente derivado; este
passe calcula single scattering por pixel e ainda não inclui ozônio, múltiplo
espalhamento ou perspectiva aérea volumétrica. A iluminação indireta dos
materiais também não é recalculada a partir da atmosfera física nesta etapa:
ela continua no fallback hemisférico/IBL existente.

Referências oficiais consultadas: [Unity HDRP Physically Based Sky](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@10.4/manual/Override-Physically-Based-Sky.html)
e [comparação de recursos dos pipelines Unity 6](https://docs.unity3d.com/6000.0/Documentation/Manual/render-pipelines-feature-comparison.html),
que classifica Physical Sky como recurso do HDRP e Android como alvo fora do HDRP.

### Caminho no editor e API de jogo

No Inspector de **Ambiente**, abra **Atmosfera → Céu → Atmosfera física**.
A aba **Atmosfera física** passa a oferecer as propriedades condicionais.
Elas usam o mesmo descriptor em edição, perfis e scripts; alterar um mundo em
Play não reescreve o documento autoral ao parar.

Com `target` sendo um `Astra.GameObject` válido que já contém Ambiente:

```csharp
var sky = target.GetComponent(Astra.ComponentIds.Environment)
    ?? throw new System.InvalidOperationException("O objeto precisa de Ambiente.");
sky.SetEnum("sky", 2); // PhysicalAtmosphere; selecionar antes dos campos condicionais.
sky.SetFloat("physical_sky_intensity", 1.5f);
sky.SetFloat("aerosol_density", 0.8f);
sky.SetBool("physical_atmosphere_high_quality", false);
```

A influência final depende de peso, prioridade, camada e overrides do volume.
Esta API de componente não configura os níveis globais de `rendering.astra`;
a API C# dessa política de projeto permanece pendente.

## Pesquisa de implementação e estado em 21/09/2026

As referências abaixo delimitam caminhos reutilizáveis; consulta ou licença
compatível não significa que a dependência já foi integrada:

- [Playdead Temporal Reprojection](https://github.com/playdeadgames/temporal),
  commit `4795aa0007d464371abe60b7b28a1cf893a4e349`, licença MIT: somente a
  conversão RGB/YCoCg e o clipping AABB dirigido ao centro foram adaptados para
  GLSL. A cópia local da licença e a revisão ficam em
  `native/third_party/playdead_temporal/`; não há dependência do runtime Unity;
- [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h), dual
  public domain/MIT: `stbi_loadf` preserva Radiance RGBE `.hdr` como floats
  lineares. É um candidato pequeno para a fatia de importação HDR, mas não
  decodifica OpenEXR e ainda não foi incorporado ao loader da Astra;
- [Bruneton, Precomputed Atmospheric Scattering](https://github.com/ebruneton/precomputed_atmospheric_scattering),
  licença BSD: referência com LUTs pré-computadas, documentação e testes para
  espalhamento múltiplo. É material de comparação para uma evolução futura,
  não o algoritmo executado pelo sky pass atual;
- [Hillaire, Production Ready Atmosphere Rendering](https://sebh.github.io/publications/egsr2020.pdf):
  referência técnica para LUTs escaláveis, múltiplo scattering e perspectiva
  aérea. O artigo não foi tratado como biblioteca incorporável nem como licença
  de código.

Nesta rodada, a reprojeção TAA foi corrigida em código para ancorar a consulta
do histórico resolvido pelo jitter atual, medir movimento em pixels da saída e
recortar o histórico em YCoCg no mesmo domínio display-linear do quadro atual.
O caminho diferencia swapchain sRGB, cuja amostragem decodifica em hardware, de
UNORM com codificação manual. O subset Playdead, a revisão fixa e a licença MIT
estão empacotados no repositório. Um Release com essa correção compilou, foi
instalado no Xiaomi 25053PC47G/Adreno 825 e o log temporário confirmou modo 3
com feedback 0,88; o log foi removido depois da medição. A causa raiz do bypass
era o shell publicar clip/FOV zero e logo depois os valores reais em todo quadro,
invalidando o histórico antes do pós. O shell agora publica a câmera efetiva uma
vez por quadro.

Em captura estática da vista Exterior de `GraphicsStage0921`, a 85%, Catmull–Rom,
TAA e 120 Hz solicitados, vídeos H.264 de 4 s/20 Mbps foram medidos após 1 s, por
2 s, no crop `x=850, y=650, w=780, h=430`. O vídeo anterior teve 120 frames,
`edgeStd=7,6314783` e diferença entre frames `10,4111462`; o Release final teve
118 frames, `edgeStd=2,1526747` e diferença `1,9744731`, cerca de 81% menos
diferença entre frames. Os dois vídeos usam a mesma máscara de 7.341 pixels de
borda derivada do baseline, evitando que máscaras distintas favoreçam um deles.
São níveis de cinza de vídeo 8-bit comprimido, úteis para esta vista estática;
não medem tempo de GPU, não cobrem movimento nem generalizam para outras cenas.
As evidências ficam em `build/graphics-stability/`, incluindo
`taa-exterior-final.mp4`, `exterior-final.png` e `final-device.log`.
O sharpen existente continua antes do resolve temporal por manter o consumidor
da opção atual; movê-lo para RCAS ou outro passe posterior exige separar o
recurso de histórico. O film grain também continua presente na imagem copiada
para o histórico e precisa dessa separação para ficar realmente fora da
acumulação.

A atmosfera física desta rodada permanece single scattering Rayleigh/Mie. A
irradiância hemisférica incidente no solo agora é calculada e cacheada na CPU
por `PhysicalAtmosphereGroundIrradianceCache`, somente quando atmosfera ou sol
mudam; a unidade `renderer/physical_atmosphere.cpp` integra o alvo no CMake. O
frame UBO passou a 3328 bytes e publica o `vec4` no offset 3312; o sky
pass consome esse valor em vez de executar três integrações atmosféricas extras
por pixel do hemisfério inferior. O filtro de host `physical_atmosphere` passou
5/5. O Release foi instalado e `build/graphics-stability/exterior-after.png`
mostra o solo antes preto agora iluminado. Isso otimiza o modelo compacto; não
implementa os modelos completos de Bruneton ou Hillaire. Importação HDR
autorável por GUID, OpenEXR, múltiplo scattering, perspectiva aérea, probes
locais e medição do custo GPU da atmosfera continuam abertos; a passada ADB
validou o efeito visual, sem medir desempenho.

O sampler de texturas glTF também passou a preservar separadamente minificação,
magnificação, uso de mip e filtro entre mips. Cooker, autoria e pacote usam o
mesmo decode, mantendo a leitura dos flags legados conforme a
[especificação glTF 2.0 da Khronos](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-sampler).
A regressão dos seis modos
glTF e o caso de pacote passaram em host; a validação visual Android continua
pendente. A regressão de reabertura do editor e três filtros focados também
passaram; o código chegou ao Release instalado. A revisão do cache de importação
subiu de 4 para 5 para reconstruir automaticamente os derivados na próxima
abertura, sem apagar a fonte. Pacotes `.aemap` já cozidos com os bits antigos
precisam ser recozidos da fonte, pois não contêm informação para recuperar a
escolha original.
O teste focado do cache passou 1/1 e o Release com a revisão 5 compilou em 24 s;
foi instalado com `Success` e reabriu com o processo vivo (PID 19040). O código
gráfico medido no vídeo final não mudou nesse rebuild posterior.

Depois dessa medição foi implementado o fechamento para quadros sem histórico:
corte de câmera é decidido antes do jitter e frames invalidados usam jitter zero
com FXAA espacial, copiando uma base estável para o histórico seguinte. Isso evita
que cenas dinâmicas sem motion vectors exibam a sequência Halton crua. O Release
final compilou em 13 s, foi instalado e passou por Play/Stop; o céu foi observado
nos dois modos e o aplicativo permaneceu vivo (PID 15010). Essa passada não moveu
objeto para validar o fallback dinâmico. Motion vectors por objeto/skinning e
máscaras reativas continuam pendentes.

## Próximos gates

### Integração de recursos e histórico — 2026-09-21

O recurso `EnvironmentMap` separa a fonte Radiance HDR equiretangular 2:1 dos
derivados: panorama RGBA16F com mips, reflexão GGX octaédrica, LUT BRDF e SH9.
O importador C++ usa o `stb_image` já presente no projeto, limites de memória,
cancelamento e cache por hash/receita/revisão. OpenEXR não está implementado.
O orçamento de trabalho padrão é 128 MiB e inclui a coexistência da alocação do
decoder com o vetor de saída; exceder o limite produz recusa explícita. Em host
O2, duas fontes reais locais (`hausdorf_clear_sky_1k.hdr` e
`kloppenheim_03_puresky_2k.hdr`) passaram por importação e round-trip do cache:
686,3 ms e 752,9 ms de preparo, respectivamente, com a receita padrão. Cada
cache ocupou 6.422.948 bytes. Esses tempos não representam custo no Android.
A versão 8 de `Environment` e a versão 4 de `EnvironmentProfile` preservam GUID,
rotação em graus e exposição HDRI em EV. O grupo Céu escolhe o panorama de modo
discreto no limiar de influência 0,5; volumes que só alteram neblina ou pós não
trocam o recurso. Isso não implementa mistura entre dois panoramas.

Referência: [Unity 6, skyboxes](https://docs.unity3d.com/cn/6000.0/Manual/skyboxes-using.html).
Astra deriva céu, SH9 e reflexão da mesma fonte. A exposição da HDRI importada
não muda as luzes diretas nem acompanha a intensidade do sol. Importação,
reimportação, reabertura, seleção no Inspector e binding por API usam o mesmo
registro de recursos. A prévia permite escolher resolução do panorama, reflexão
e BRDF, além das amostras GGX/BRDF; alterar a receita prepara novamente os
derivados antes de habilitar a publicação. O teste de commit/reopen/binding
passou em host. No Xiaomi/Adreno 825, a cópia `GraphicsApi0921` importou a fonte
Radiance de 1K, alterou GGX para 256 amostras na prévia, selecionou o recurso e
mostrou efeito de rotação/exposição no céu e na iluminação. A reabertura
recuperou o cache sem falhas. Em Play, o script trocou a rotação para 45°; Stop
preservou a autoria em 90°/4 EV. Evidência: `build/graphics-api-validation/`.
Isso não é uma comparação física isolada de SH9/reflexo nem validação de probes.

O TAA agora possui uma saída de resolve própria, anterior ao grão e à nitidez.
Um segundo attachment do passe de pós alimenta o histórico; a imagem apresentada
recebe os efeitos de acabamento. A prévia de câmera mantém attachments
compatíveis. O histórico deixa de depender de `TRANSFER_SRC` da swapchain.
Grão e nitidez são aplicados em display-linear, antes da transferência sRGB,
sem contaminar o histórico. As dependências de profundidade publicam escritas
de EARLY e LATE fragment tests para pós, sombras e HZB. Os dois shaders passaram
por `spirv-val`, e o APK Release compilou para arm64. Isso não substitui validação visual ou medição de custo:
existe um attachment adicional de resolução de saída enquanto TAA estiver ativo.

Os itens abaixo continuam sendo gates de fechamento, não afirmações de suporte.

1. comparar numericamente/visualmente irradiância e reflexão da fonte importada,
   inclusive orientação e exposição em outras poses; a importação, seleção por
   GUID e reabertura já foram observadas na cópia Android indicada acima;
2. reflection probes locais, blending, parallax correction e orçamento de updates;
3. cache persistente e orçamento de variantes, aquecimento fora do frame;
4. regressão visual multipose (SSIM/FLIP), incluindo a orientação da irradiância
   SH, rota de 60 s, soak de 30 min e matriz
   Adreno/Mali;
5. captura AGI para separar bandwidth, textura, ALU e register pressure do passe
   opaco antes da próxima otimização.

SH9 global descreve somente a iluminação distante da mesma HDRI. Ele não mede
oclusão dentro de cômodos nem substitui Light Probes, Reflection Probes locais,
lightmaps ou bake espacial. Esses recursos continuam gates separados.

