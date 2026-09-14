# M09.1/M09.2 — texturas e PBR importados (Entrega 3)

14/09/2026 · branch `codex/gameplay-runtime`, sobre o commit `bda54e91`. Terceira entrega do [pacote delegado](DELEGACAO-M08-M09-IMPORTACAO-AUTORAL.md). **M08 e M09 continuam abertos**: dependências externas (`.gltf` + `.bin` + imagens), Draco, meshopt, KTX2/BasisU e escala negativa/cisalhamento são da Entrega 4. Nenhuma expansão além do plano foi incluída.

## O que foi implementado

| Contrato do plano | Implementação | Onde |
|---|---|---|
| Decodificar imagens embutidas no aparelho, igual no host | stb_image v2.30 vendorizado (domínio público/MIT, commit `2c980bb5…`, SHA-256 `594c2fe3…`), só PNG e JPEG. `AImageDecoder` não serve: só existe no Android 11 e o mínimo é o 8 | `native/third_party/stb/`, `native/resources/image_decode.*` |
| Limites antes de alocar | Cabeçalho lido primeiro; recusa acima de 8192 px por lado, 16 MP ou 64 MB codificados, com diagnóstico | `decodeImageRgba8`, `readImageDimensions` |
| Espaço de cor por uso | Cor base e emissiva em sRGB; normal e metálico/rugosidade lineares. A mesma imagem usada nos dois papéis vira duas texturas. Mipmaps da cor são médios em espaço linear | `buildMipChain`, `resolveTexture` |
| Amostrador do arquivo | `magFilter`/`minFilter` (nearest, mip nearest) e `wrapS`/`wrapT` (repeat, clamp, mirror) viram flags da textura e um sampler Vulkan próprio | `samplerFlags`, `DirtRoadResources` |
| Slots PBR do material | Base (slot 0), normal (1, com `scale`), metálico/rugosidade (2), emissiva (3); `texCoord` 0 ou 1 por slot; `alphaMode` MASK com `alphaCutoff`, BLEND e dupla face continuam nos flags do material | `readMaterials`, `assignTexture` |
| Orçamento de memória residente | Limite de 2048 px por textura (descarta os mips de cima, não a textura). Antes de decodificar, um planejamento lê só os cabeçalhos de todas as texturas referenciadas; se o conjunto passa de 256 MB, o limite cai pela metade **para todas** até 256 px, e só então o orçamento descarta. Descartar em ordem de declaração deixava o fim do arquivo sem textura | `planTextureResolution` |
| Tangentes ausentes | Com mapa normal e sem `TANGENT`, a importação gera tangentes (derivadas de UV por triângulo, ortogonalizadas contra a normal, orientação no `w`). O shader omite o detalhe do mapa normal se a tangente for degenerada, em vez de produzir NaN | `gltf_import.cpp`, `dirt_road_shading.glsl` |
| Publicação no renderer | Texturas importadas sobem como imagens `R8G8B8A8_SRGB/UNORM` com a cadeia de mips, entram no registro bindless depois das texturas do pacote; índices de textura de cada fonte são deslocados na biblioteca combinada. Falha de GPU volta à biblioteca anterior | `dirt_road_resources.cpp`, `instanced_renderer.cpp`, `editor_session.cpp` |
| Prévia honesta | A preparação lista texturas aplicadas (MB), reduzidas (e até quantos px), não aplicadas com motivo, transformações de UV e oclusão não aplicadas | `EditorSession::showImportPreview` |

## Limitações declaradas

- **Oclusão e `KHR_texture_transform` não são aplicadas.** O bloco de push constants do material (8 × vec4 = 128 bytes, o mínimo garantido pelo Vulkan) já está cheio; são contadas e mostradas na prévia, nunca aproximadas.
- **KTX2/BasisU, WebP e imagens externas ou em data URI não são lidas** (CarConcept: 25 texturas só em KTX2 ficam de fora). Entrega 4.
- **Materiais avançados** (`KHR_materials_clearcoat`, `specular`, `transmission`…) continuam listados como extensões preservadas na fonte, não reproduzidas.
- **Dupla face não muda o culling:** todas as pipelines usam `VK_CULL_MODE_NONE`, então materiais de face única também mostram o verso.
- **Caminho sem bindless** (aparelho sem descriptor indexing) desenha os materiais importados sem textura, com os fatores; o log avisa.
- Tangentes geradas não são MikkTSpace bit a bit; mapas normais assados com MikkTSpace podem ter pequenas diferenças nas costuras de UV.
- O orçamento de 256 MB é por importação; várias fontes texturizadas somam na GPU.
- Decodificação e mipmaps são em CPU no worker; o cancelamento vale entre materiais, não dentro de uma textura grande.

## Evidências

Host (`build/editor-host`, depuração), [testes M09.1](../validacao/evidencias/m091-20260914/host-m091.log): PNG decodificado e recusa de cabeçalho acima do limite antes de alocar; mips em espaço linear para cor e diretos para dados; slots, espaço de cor, UV e amostrador vindos do GLB; redução ao limite residente em vez de descarte; orçamento reduzindo a resolução de todas antes de descartar (e o piso segurando); tangentes geradas unitárias e perpendiculares à normal; índices de textura deslocados por fonte na publicação. [Suíte](../validacao/evidencias/m091-20260914/host-suite.log): **885/887**, com as mesmas duas falhas antigas de console/barra do IDE.

Arquivos reais, `aether_tests --reimport-glb` ([primeira medição](../validacao/evidencias/m091-20260914/real-textures-primeira-medicao.log), [com limite residente e tangentes](../validacao/evidencias/m091-20260914/real-textures-limite-residente.log)):

| Arquivo | Antes | Depois |
|---|---|---|
| Ford Lotus Cortina | 18 texturas, 148,1 MB | 18 texturas, 84,1 MB, 1 reduzida |
| Porsche 911 930 | 52 aplicadas, 21 descartadas, 255,0 MB | 73 aplicadas, 0 descartadas, 167,0 MB (limite 1024 px), 46 primitivas com tangentes geradas |
| CarConcept | 25 texturas só KTX2 | igual (Entrega 4) |

Reimportação e identidade continuam como na Entrega 1 em todos (conteúdo igual, reabertura idêntica).

### Aparelho

Projeto `M08Recursos0913k`, instalado sem apagar dados, capturas só com o editor em primeiro plano. [Log completo](../validacao/evidencias/m091-20260914/aparelho-adb.log).

1. **Primeira abertura com texturas revelou um defeito.** APK `5AD6E9A8…D011`: Ford com 18 texturas em 18 slots bindless e texturizado; Porsche com 73 texturas publicadas (91 no total), mas **preto**. Antes da Entrega 3 ele aparecia amarelo. Causa: 46 primitivas com mapa normal sem `TANGENT`, tangente zero e NaN na base TBN. [Captura](../validacao/evidencias/m091-20260914/porsche-preto-sem-tangentes-adb.png).
2. **Com tangentes geradas.** APK `15F7FECB…D303` (SPIR-V da família `dirt_road` regenerado e conferido com `-All -Check`): Porsche amarelo com grade, rodas e detalhes texturizados, Ford igual. Decodificar o Porsche levou cerca de 35 s no aparelho, no worker. [Captura](../validacao/evidencias/m091-20260914/ford-porsche-texturizados-adb.png).
3. **Play.** Cena em execução com as mesmas texturas. [Captura](../validacao/evidencias/m091-20260914/play-texturizado-adb.png).
4. **Stop, segundo plano e volta.** Mesmo processo (pid 27421), superfície recriada em 24 ms e as 91 texturas republicadas da memória em cerca de 2 s, sem decodificar de novo; nada de `FATAL` ou `DEVICE_LOST`. [Captura](../validacao/evidencias/m091-20260914/retomado-texturizado-adb.png).

Durante a abertura do Porsche o `lowmemorykiller` encerrou apps de outros pacotes em segundo plano; o editor sobreviveu. Causa provável: a importação guardava todas as imagens RGBA cheias decodificadas até o fim. Depois desta conferência, o planejamento passou a contar os usos de cada imagem e a imagem cheia é liberada no último uso, então o pico fica em uma imagem cheia mais as cadeias já reduzidas. Essa mudança tem cobertura de host (a mesma imagem usada como cor e como dado continua gerando as duas texturas), e no aparelho (APK `103DAA00…E51F`) a abertura a frio publicou as mesmas 91 texturas, com PSS máximo amostrado de cerca de 742 MB e 3 encerramentos de outros apps pelo `lowmemorykiller`. Não há amostra de PSS da abertura anterior, então **não é um ganho medido**. Gerar a cadeia inteira antes de cortar os mips de cima continua dobrando a memória da textura corrente.
