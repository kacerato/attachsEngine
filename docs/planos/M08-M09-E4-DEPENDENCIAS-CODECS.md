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

## Limitações declaradas

- KTX2 é transcodificado para RGBA8 na CPU. Não há negociação de formato comprimido de GPU (ASTC/ETC2) com o aparelho: a memória de GPU de uma textura KTX2 é a mesma de um PNG da mesma resolução.
- KTX2 HDR, cubemap e array são recusados com motivo; só o nível 0 é transcodificado e os mips são refeitos.
- `gltfpack` quantiza UV por padrão e compensa com `KHR_texture_transform`, que continua não aplicado (Entrega 3). Arquivos assim importam com a geometria certa e as texturas desalinhadas, e a prévia conta as transformações não aplicadas.
- O orçamento de expansão é por importação. A decodificação Draco aloca internamente antes de o orçamento da saída ser conferido.
- A reflexão duplica a geometria de primitivas usadas por nós refletidos e não refletidos ao mesmo tempo.

## Evidências

Host (`build/editor-host`, depuração): [testes da Entrega 4](../validacao/evidencias/m09e4-20260914/host-m09e4.log), [suíte](../validacao/evidencias/m09e4-20260914/host-suite.log), [arquivos reais](../validacao/evidencias/m09e4-20260914/real-codecs.log). Fixtures dos codecs geradas pelas ferramentas oficiais das mesmas versões em `tests/native/fixtures/codecs/` (com SHA-256 e comando).

Suíte do host com as três trilhas: **893/895**, com as mesmas duas falhas antigas de console/barra do IDE. Testes da Entrega 4: meshopt (sem perda e com filtro, entradas hostis), Draco (quadrado quantizado com orientação, contagem, truncamento, orçamento), KTX2 ETC1S e UASTC, textura só em `KHR_texture_basisu`, reflexão (mundo, normais e face da frente iguais ao arquivo; cisalhamento recusado) e empacotamento (`.gltf` + `.bin` + imagem, recusas de rede, `..`, caminho absoluto, ambiguidade, limite e `data:`).

### Aparelho

**Pendente.** O APK com as três trilhas compila para arm64 (`CE4B7CD2…4F`; as mensagens dos três codecs estão na biblioteca nativa). A conferência foi interrompida antes de começar: outro aplicativo passou ao primeiro plano, a regra de captura só com o editor em primeiro plano recusou a imagem, e a interação com o aparelho foi suspensa até ele estar livre. Estão preparados em `Fontes/` do projeto de conferência `esfera-draco.glb`, `ford-lotus-meshopt-noq.glb`, `CarConcept.glb` e `ford-lotus-espelho.glb`; o `.gltf` separado do Ford está em `build/glb-real/ford-gltf/` para o teste do seletor com seleção múltipla.
