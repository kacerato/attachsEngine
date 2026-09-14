# M08/M09 — codecs, reflexão e dependências externas (Entrega 4)

14/09/2026 · branch `codex/gameplay-runtime`, sobre o commit `d45dd179` (Entrega 3). Quarta entrega do [pacote delegado](DELEGACAO-M08-M09-IMPORTACAO-AUTORAL.md). **M08 e M09 continuam abertos.** Este documento cobre as trilhas já implementadas; a de dependências externas (`.gltf` + `.bin` + imagens pelo SAF) está marcada como pendente abaixo e não é apresentada como pronta.

## Trilha 1 — codecs (implementada)

Três adaptadores com versão, commit e licença fixados, atrás de uma interface nossa (`native/resources/gltf_codecs.h`) que não deixa nenhum tipo das bibliotecas vazar para o importador. Geometria e textura são trilhas separadas: decodificar Draco não diz nada sobre KTX2.

| Extensão | Biblioteca vendorizada | Onde entra no importador | Orçamento |
|---|---|---|---|
| `EXT_meshopt_compression` (e `KHR_meshopt_compression`) | meshoptimizer v1.2 (`9d9890c7`, MIT): `vertexcodec`, `indexcodec`, `vertexfilter` | por bufferView, decodificada uma vez e compartilhada por acessores, dados esparsos e imagens | `maximumExpandedBytes` (512 MB) conferido antes de alocar |
| `KHR_draco_mesh_compression` | Draco 1.5.7 (`87867400`, Apache-2.0): só os 44 `.cc` de decodificação | por primitiva; o acessor glTF manda no tipo, componentes e contagem, e contagem diferente do bloco é recusada com os dois números | mesmo orçamento |
| `KHR_texture_basisu` (KTX2) | Basis Universal v2_50 (`9bebe167`, Apache-2.0) + decompressor zstd | pela imagem da textura; transcodificada para RGBA8 e depois pelo MESMO caminho do PNG/JPEG (limite residente, espaço de cor, mips, liberação da imagem cheia) | limites de imagem pelo cabeçalho, antes de transcodificar |

- Alvo CMake `aether_codecs` entra antes das flags estritas, como Jolt/Box2D/SQLite, sem RTTI e sem exceções (nenhum `throw`, `try`, `typeid` ou `dynamic_cast` no subconjunto). Saídas do transcodificador que não usamos (DXT, BC7, PVRTC, ATC, FXT1) desligadas por definição.
- O adaptador meshopt valida modo, passo, contagem e filtro **antes** de chamar a biblioteca: a v1.2 confere essas condições com `assert`, e um arquivo hostil derrubaria o processo em build de depuração.
- `extensionsRequired` passa a aceitar as quatro extensões acima, e `KHR_texture_basisu` deixou de ser listada como omissão de aparência. Extensão exigida desconhecida continua recusada com o nome.
- A prévia da importação diz o que foi descomprimido (primitivas Draco, visões meshopt, imagens KTX2).

## Trilha 2 — reflexão e cisalhamento (implementada)

Escala negativa continua proibida no grafo de cena (`isTransformValid`), no editor e na física; nada disso foi relaxado. A reflexão é resolvida na importação, de forma exata:

- Com S = diag(-1,1,1), a pose local de cada nó vira S_pai · L · S_nó, escolhendo S_nó para o determinante sair positivo. O filho de um nó refletido herda S à esquerda.
- A geometria dos nós refletidos é uma cópia (uma por primitiva) com X negado em posição, normal e tangente, sinal `w` da tangente invertido e dois vértices trocados por triângulo, que é o que o glTF manda para determinante negativo.
- Resultado: posições e normais no mundo iguais às do arquivo, face da frente coerente com a normal, e poses locais que cabem no TRS do editor. Seleção, hierarquia e colisores derivados da malha usam a geometria já espelhada.
- Cisalhamento na pose local é **recusado** na importação com os nomes dos nós (mesma tolerância relativa do editor ao decompor), com a orientação de aplicar a transformação no editor 3D. Cisalhamento induzido pela hierarquia (escala não uniforme no pai com filho rotacionado) continua aceito, como antes.

## Trilha 3 — dependências externas pelo SAF (implementada no host; conferência no aparelho pendente)

- **Seletor:** `ModelPicker` pede seleção múltipla (`EXTRA_ALLOW_MULTIPLE`). O usuário marca o `.gltf` (ou `.glb`) e as dependências dele; o `ContentResolver` lê cada um (até 64 arquivos, 128 MB juntos) e o conteúdo atravessa a JNI com o nome declarado pelo provedor (`submitMany`). `content://` nunca vira caminho, e não há acesso à pasta.
- **Principal:** o único `.gltf`; sem `.gltf`, o único `.glb`. Dois candidatos são recusados em vez de adivinhados.
- **Resolvedor** (`native/resources/gltf_package.*`): casa cada URI pelo último segmento com os arquivos escolhidos; decodifica `data:` base64; recusa URI com esquema (http, https, file, content…), caminho absoluto, barra invertida e segmento `..`; nome que casa com dois arquivos é ambíguo e recusado; dependências faltando são listadas todas no diagnóstico, com a instrução de selecioná-las junto.
- **Staging:** buffers e imagens são copiados para um único bloco binário e as bufferViews reendereçadas (inclusive as da extensão meshopt). O resultado é um GLB autocontido, guardado em `Fontes/<nome>.glb` pela mesma transação da importação. Reabrir e reimportar leem só o projeto: funciona offline e sem a permissão temporária do seletor.
- **Manifesto:** `Fontes/<nome>.glb.deps` (`ASTRA_GLTF_DEPS 1`) registra o principal, cada dependência (URI, arquivo, tamanho, SHA-256) e o GLB empacotado; é gravado só depois da fonte, e falhar nele não desfaz a importação. A prévia diz quantas dependências foram copiadas e quantos arquivos escolhidos ficaram sem uso.

Decisão registrada: guardar o GLB empacotado em vez dos arquivos soltos. Os bytes das dependências ficam no projeto do mesmo jeito, a transação atômica continua sendo de um arquivo, e a identidade de nós (Entrega 1) e a reimportação seguem o mesmo caminho do GLB.

## Transformação de UV assada (implementada)

A Entrega 3 deixava `KHR_texture_transform` sem efeito porque o bloco de push constants do material não tem espaço para a matriz. A transformação passou a ser aplicada nas UVs da primitiva, na importação:

- A matriz é a do glTF (translação · rotação anti-horária · escala, em colunas), inclusive o `texCoord` da extensão, que troca o conjunto de UV da textura.
- Por material e por conjunto de UV, todas as texturas aplicadas precisam concordar. Se uma usa a transformação e outra no mesmo conjunto não usa (ou usa outra), nada é assado naquele conjunto e as transformações continuam contadas como não aplicadas, com a prévia dizendo isso.
- A aplicação acontece antes da geração de tangentes, que seguem as UVs que o shader amostra; espelhamento e empacotamento usam as UVs já transformadas.
- Efeito medido: o Ford comprimido pelo `gltfpack` (UV quantizada) passou de 19 transformações não aplicadas para 19 assadas; o CarConcept, de 12 para 12 assadas.

## KTX2 comprimido na GPU (implementado)

- O renderer consulta o aparelho (`vkGetPhysicalDeviceFormatProperties`) e só declara suporte quando `VK_FORMAT_ASTC_4x4_SRGB_BLOCK` e `UNORM` aceitam amostragem com filtro linear e cópia. A sessão do editor recebe isso antes de reabrir fontes e passa aos limites de importação (interativa e reabertura).
- Com suporte e KTX2 com a cadeia completa de mips, cada nível é transcodificado direto para blocos ASTC 4x4 (8 bits por texel em vez de 32), sem RGBA intermediário nem mips em CPU; o limite de resolução residente descarta níveis de cima e o planejamento de memória usa o tamanho comprimido.
- Sem suporte, ou KTX2 sem mips (não há codificador para gerá-los), o caminho RGBA8 continua, e a prévia diz o motivo.
- O RHI passou a conhecer o tamanho de ASTC 4x4; o upload usa o mesmo `uploadSampledMipChain` do pacote de mapa.

## Limitações declaradas

- KTX2 sem cadeia completa de mips, ou em aparelho sem ASTC 4x4, continua em RGBA8. ETC2 não é negociado como alternativa; PNG e JPEG continuam RGBA8 (não há codificador ASTC no aparelho para eles).
- A qualidade do ASTC vem da transcodificação do Basis (UASTC→ASTC é quase sem perda; ETC1S→ASTC herda a perda do ETC1S). No host não há decodificador ASTC: os testes conferem tamanho e níveis, e a aparência fica para a conferência no aparelho.
- KTX2 HDR, cubemap e array são recusados com motivo; só o nível 0 é transcodificado e os mips são refeitos.
- A transformação de UV é assada pelo material da FONTE. Se o usuário trocar o slot para outro material da mesma fonte com transformação diferente, as UVs continuam com a transformação do material original.
- Materiais cujas texturas discordam na transformação de UV dentro do mesmo conjunto continuam sem ela (contadas na prévia).
- O orçamento de expansão é por importação. A decodificação Draco aloca internamente antes de o orçamento da saída ser conferido.
- A reflexão duplica a geometria de primitivas usadas por nós refletidos e não refletidos ao mesmo tempo.

## Evidências

Host (`build/editor-host`, depuração): [testes da Entrega 4](../validacao/evidencias/m09e4-20260914/host-m09e4.log), [suíte](../validacao/evidencias/m09e4-20260914/host-suite.log), [arquivos reais](../validacao/evidencias/m09e4-20260914/real-codecs.log). Fixtures dos codecs geradas pelas ferramentas oficiais das mesmas versões em `tests/native/fixtures/codecs/` (com SHA-256 e comando).

Suíte do host com as três trilhas: **893/895**, com as mesmas duas falhas antigas de console/barra do IDE. Testes da Entrega 4: meshopt (sem perda e com filtro, entradas hostis), Draco (quadrado quantizado com orientação, contagem, truncamento, orçamento), KTX2 ETC1S e UASTC, textura só em `KHR_texture_basisu`, reflexão (mundo, normais e face da frente iguais ao arquivo; cisalhamento recusado) e empacotamento (`.gltf` + `.bin` + imagem, recusas de rede, `..`, caminho absoluto, ambiguidade, limite e `data:`).

### Aparelho

APK `2E0117F6…3E` (três trilhas, transformação de UV assada e caminho ASTC), instalado sem apagar dados, projeto `M08Recursos0913k`, capturas só com o editor em primeiro plano. Uma primeira tentativa foi suspensa quando outro aplicativo passou ao primeiro plano; a conferência abaixo foi feita depois de o aparelho ser liberado.

1. **Capacidade do aparelho.** O log da abertura diz `[Import] KTX2 com mips vira ASTC 4x4.`: o renderer confirmou ASTC 4x4 sRGB e UNORM com filtro linear e cópia.
2. **Draco.** `esfera-draco.glb` estava registrada e na cena (importada pelo editor enquanto a conferência estava suspensa) e reabriu do projeto: `[Import] fonte reaberta: Fontes/esfera-draco.glb`. [Captura](../validacao/evidencias/m09e4-20260914/projeto-aberto-esfera-draco-adb.png).
3. **KTX2 em ASTC.** Arquivos → Fontes → `CarConcept.glb` → Reimportar: prévia com "Texturas: 13 aplicadas (4 MB com mipmaps)" (15,6 MB em RGBA8), "Transformação de UV aplicada nas UVs em 12 referência(s)" e "12 imagem(ns) KTX2". *Importar na cena* publicou 104 texturas em 104 slots bindless e o vínculo ficou "igual à fonte". [Prévia](../validacao/evidencias/m09e4-20260914/previa-carconcept-astc-adb.png), [cena](../validacao/evidencias/m09e4-20260914/carconcept-importado-adb.png). A aparência de perto não foi avaliada: o carro ficou pequeno sob o Ford e não houve como aproximar a câmera com gesto de pinça pelo adb.
4. **Reflexão.** `ford-lotus-espelho.glb`: prévia com "Reflexão (escala negativa) resolvida em 43 nó(s)"; *Importar na cena* instanciou `EspelhoX` com a hierarquia inteira, texturizado, vínculo "igual à fonte", 122 texturas publicadas no mesmo processo. Antes desta entrega o editor recusava instanciar esse arquivo. [Prévia](../validacao/evidencias/m09e4-20260914/previa-ford-espelho-adb.png), [cena](../validacao/evidencias/m09e4-20260914/ford-espelho-importado-adb.png).
5. **meshopt.** `ford-lotus-meshopt-noq.glb`: prévia com "5 visão(ões) meshopt" e as 18 texturas. A importação foi cancelada de propósito ("Importação cancelada; projeto preservado"): mais 84 MB de texturas sobre Porsche, Ford e Ford espelhado não acrescentariam evidência de decodificação. [Prévia](../validacao/evidencias/m09e4-20260914/previa-ford-meshopt-adb.png).
6. **Ciclo de vida.** Salvar, Play, Stop, Home e volta pelo lançador no mesmo processo (pid 23222), com as 122 texturas republicadas. Na reabertura a frio (app encerrado), esfera Draco, CarConcept e Ford espelhado reabriram só a partir do projeto, sem nenhum encerramento de app pelo `lowmemorykiller`. No Play seguinte o CarConcept aparece texturizado em ASTC ao lado de Porsche, Ford e esfera, e o Stop volta à edição. [Reaberto](../validacao/evidencias/m09e4-20260914/reaberto-a-frio-adb.png), [Play](../validacao/evidencias/m09e4-20260914/play-astc-reflexao-draco-adb.png), [parado](../validacao/evidencias/m09e4-20260914/parado-depois-do-play-adb.png), [log](../validacao/evidencias/m09e4-20260914/aparelho-adb.log).
7. **Alteração local não pedida, revertida.** Depois da reabertura, `BodyUnderside` (CarConcept) mostrava "1 alteração local": posição 0 0 3,78 contra a base 0 0 0 do vínculo. A causa não foi identificada; o mais provável é um toque desta conferência no viewport ter arrastado o gizmo de mover. Foi revertida pelo menu do vínculo ("Reverter posição à fonte") e salva, e a cena volta a gravar 0 0 0 com o vínculo "igual à fonte". [Captura](../validacao/evidencias/m09e4-20260914/posicao-revertida-adb.png).
8. **Seleção múltipla de `.gltf` pelo seletor do sistema:** não conferida no aparelho. Ler a lista de arquivos do seletor exige autorização explícita; a trilha está coberta no host (empacotamento do Ford separado em 20 arquivos).
