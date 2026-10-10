# N07 — próxima família de formatos

**Estado vigente:** ver `N07-saldo-real.md`. Este documento conserva a sequência
de contratos e rodadas anteriores; referências a “static-only”, 16/18 e aceite
pendente do corpus descrevem etapas históricas, sucedidas pela expansão
`N07-fbx-corpus-deformacao.md`. A baseline executada mais recente é 20/20,
520 FBX estáticos e 150/151 FBX deformáveis aceitos (uma recusa dual quaternion).
O último trecho cold Play do cenário deformável e o pacote Unity pela Biblioteca
passaram no gate 7612/41541. Não há aceite Android declarado.

## TGA: dependências e recorte verificado

Referência: [Godot 4.5 — imagens](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/importing_images.html)
e [loader oficial](https://github.com/godotengine/godot/blob/4.5-stable/modules/tga/image_loader_tga.cpp).
A Astra já fixa stb_image 2.30, commit 2c980bb59875b0d32144a71867fbdebb2f77cd20;
seu decoder TGA foi habilitado na lista STBI_ONLY. Não exige novo codec.

Inspeção dos sete arquivos TGA fornecidos: todos tipo 2, sem palette, origem
inferior esquerda; três RGB24 e quatro RGBA32 com oito bits de alpha, entre
256 e 2048 pixels por eixo. Isso cobre uma família concreta de fontes.

Cadeia necessária: preflight de cabeçalho/payload → stb_image → normalização de
origem/alpha → TextureProfile → mipmaps/compressão/cache → AssetRegistry →
material/texture viewer/UI → publicação no índice → reabertura/histórico.
Não basta adicionar extensão ao seletor. A política inicial aceita truecolor
24/32 e grayscale 8/16, sem palette, raw ou RLE; variantes ainda sem fidelidade
ficam recusadas. Origin right-to-left exige correção: stb só normaliza o eixo Y.
Truncamento RLE precisa recusa anterior à alocação, pois stb tolera fim de stream.
TGA 2.0 com tabela de correção de cor ou alpha premultiplicado exige diagnóstico.

O helper tga_validation.h está integrado ao decoder. A rodada host build-temporal-tga-resume passou com 12/12 cenários materiais/biblioteca/TGA, incluindo decodificação real das sete fontes TGA fornecidas. Abrir PNG/JPEG/KTX2/TGA na Biblioteca
aciona o worker e importador visual existentes, incluindo fontes ainda sem
registro. Escolha Color/Normal/Data vem do perfil real, com mips e compressão.
O novo cenário verifica publicação de normal TGA num material após cold reopen,
orientação, alfa, orçamento, RLE e recusa de truncamento. ASTRA_TGA_SOURCE_ROOT
habilita a aceitação explícita dos sete TGA privados; o log distingue esse
corpus dos dados sintéticos portáveis. Nenhum payload privado foi embutido.

Importar/reimportar textura publica source+registro+índice no mesmo journal.
Undo inicial retira o registro e preserva a fonte; dependências/vínculos ativos
recusam remoção. Reimportação guarda snapshots codificados imutáveis em
.astra/imports/texture-sources/<hash>.source, separados dos caches regeneráveis:
Undo/Redo restaura bytes, hash, receita e índice juntos. Snapshot corrompido ou
registro/índice/fonte alterado externamente recusa o histórico sem sobrescrever.
Fontes legadas que já mudaram sem snapshot íntegro precisam ser restauradas à
versão registrada antes da primeira reimportação; a engine não inventa os bytes
antigos. Snapshots órfãos após falha não são registros vivos; limpeza automática
desses snapshots ainda não implementada. Edição canonical de TextureProfile
também atualiza o índice na mesma transação e recusa metadados externos.

## FBX: decoder dentro da engine

[Godot 4.5 — formatos 3D](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/importing_3d_scenes/available_formats.html)
usa ufbx como importador padrão de FBX desde 4.3. O
[FBXDocument oficial](https://github.com/godotengine/godot/blob/4.5-stable/modules/fbx/fbx_document.cpp)
normaliza eixos/unidades, trata geometria/pivôs/herança e materializa os dados
através do pipeline de cena existente. A opção de conversão via software externo
não funciona como arquitetura de importação em um editor Android.

[ufbx v0.20.0](https://github.com/ufbx/ufbx/blob/v0.20.0/README.md) é C portátil,
possui progresso/cancelamento e limites de alocação. A
[licença](https://github.com/ufbx/ufbx/blob/v0.20.0/LICENSE) oferece MIT/Unlicense.
Antes de integrar: fixar commit, preservar LICENSE/version/hashes, estabelecer
limites mobile e impedir leitura automática de caminhos externos arbitrários.

O backend deve produzir o IR tipado existente de nós, draws, materiais, skins,
morphs e animações, e reaproveitar o consumidor autoral/renderer/persistência.
Nenhuma classe FBX deve duplicar Scene ou EditorDocument. Eixos/unidades,
triangulação, UV, handedness, bind poses, influências e takes exigem contratos
explícitos. Materiais Unity externos continuam resolvidos por GUID/meta do pacote.
NURBS, constraints, efeitos de shader ou canais sem equivalente geram perdas;
não se tornam propriedades ou componentes silenciosamente inativos.

O bake POLYGON existente usa Blender offline. Seus derivados são válidos para
a galeria já criada; isso não prova que o usuário importa FBX pela engine.
ufbx v0.20.0 está preservado em native/third_party/ufbx com licença, commit
a63ff0a47485328880b3300e7bcdf01413343a45 e hashes. Um backend estático foi o
primeiro pacote integrado ao CMake, seletor Android e Biblioteca. A expansão
posterior acrescenta skin linear/rigid, morph, takes e arquivos sem mesh, descritos
em `N07-fbx-corpus-deformacao.md`. Constraints e materiais/shaders sem equivalência
continuam recusados ou diagnosticados. As duas etapas usam o consumidor tipado
existente, sem um segundo renderer.


## Correcao de publicacao e importador focal (proxima rodada)

A baseline 12/12 demonstrou parser, persistencia, indice e consumidor apos cold
reopen. Nao cobria reimportar texels de textura ja vinculada: o publisher mantinha
o mesmo GUID na GPU. editor_texture_publication.cpp prepara cada binding efetivo,
publica antes de commit e usa o mesmo journal para fonte+registro+indice; falha
restaura cache/perfil/publicacao anterior. Undo/Redo usa o mesmo caminho.
O cenario existente foi ampliado para verificar texels usados e falha de publisher;
sintaxe estrita passou e build-curves-texture-publication concluiu exit0. material-texture-publication.log: 12/12 cenarios, incluindo sete TGA originais e o publisher probe de texels vivos/Undo/Redo/falha.

A captura TGA da baseline mostrou viewport vazio por uma fixture que nao chamou
beginTextureImportPreparation: NAO constitui evidencia do importador e nao deve
ser publicada com esse sentido. A fixture corrigida chama a rota real e rasteriza
o atlas de previa decodificado. A captura posterior tga-focal.655x300.png foi inspecionada: preview real 2x2 com alfa, dois campos, cinco paginas e botao Importar legiveis.
A UI focal ocupa a superficie contextual inteira: preview a esquerda, duas
propriedades de 48px a direita, cinco paginas e confirmacao no cabecalho.
Nao usa paines desktop encolhidos. Erros substituem as propriedades por diagnostico.
N07-textura-importador-conceito.png e hipotese gerada, nao captura nem prova GPU.
O raster host tambem nao demonstra execucao Vulkan; APK/aparelho continuam pendentes.


## Integração FBX e metadados múltiplos — revisão da rodada

Atualização posterior: journal4 passou os dois cenários da rodada conjunta. TIFF
passou2/2 e quatro originais; material/biblioteca16/18. Restaram duas falhas FBX:
merge de override XYZ e tangente degenerada no corpus520. Ambas foram corrigidas
em fonte. O texto abaixo descreve o pacote estático inicial; foi sucedido pela
expansão skin/morph/takes/meshless registrada em `N07-fbx-corpus-deformacao.md`.
Essa expansão está conectada a modelo/consumer/UI/history/profile12/cache, mas seu
cenário vertical19 e os corpora520/151 aguardam a rodada conjunta. Fontes com
dual quaternion, constraints/NURBS ou propriedade sem consumidor são recusadas.

Integrado, ainda fora da baseline aceita: ASTRA_IMPORT_4 amplia o journal existente
com até oito destinos tipados. LibraryIndex aceita somente o caminho fixo;
SourceProfile e NodeMap aceitam somente .astra/imports/<guid-válido>.profile/.nodes.
Companions de fonte continuam proibidos de escrever em .astra. Cada metadado tem
hash esperado, limite (índice 16 MiB, perfil 64 KiB, mapa 64 MiB), backup e recuperação.
O commit revalida fonte, registro e metadados antes da primeira escrita. Conflito
externo nessa janela abandona o preparo sem restaurar por cima da alteração externa.
Falha após começar a escrita mantém rollback e recuperação. Formatos 1–3 continuam
legíveis. Dois cenários coesos de journal passaram no alvo material.

O workflow FBX abre pela Biblioteca e pelo seletor, prepara em worker,
mostrar perdas e propriedades efetivas, publicar a fonte .fbx original sem renomear
para .glb, registrar receita/formato corretos e instanciar nos objetos reais. Reabrir
usa o dispatcher nativo. O perfil FBX inicial recusa skin/morph/takes/constraints
em vez de converter silenciosamente em estatua. Isso mantem essas familias abertas.
A preservação do payload e a galeria offline não equivalem a esse fluxo.

Histórico composto da fatia FBX estática reúne comandos de instância e a publicação
de fonte/registro/receita/mapa/índice. Snapshots de IR efetivamente retidos são
contabilizados com suas capacidades e limitados a 64 MiB agregados no histórico de
recursos; modelos que excedem esse contrato são recusados antes de publicação.
Undo preserva o original e restaura vínculos, overrides e metadados; falha de GPU
mantém documento e cursor. O callback de finalização reconcilia referências, caches
e leases depois de sucesso ou restauração. As revisões e fronteiras de IDs do
documento continuam monotônicas. Não é um limite global do heap de comandos antigos.
GLB recebe publicação atômica de perfil/mapa/índice, mas seu histórico de recurso
anterior ainda não foi migrado para esta família composta.

O decoder não acessa arquivos externos automaticamente. Cada allocator ufbx fica
limitado ao menor valor entre 128 MiB e um quarto do orçamento expandido; triangulação,
atributos e GLB intermediário também têm limites. Materiais PBR simples são convertidos;
Phong/Lambert e texturas não vinculadas geram perdas explícitas. Skin, morph, takes,
constraints e NURBS são recusados. Isso não promete fidelidade completa de FBX.

O alvo material agora prepara dezesseis cenários: os doze aceitos, um parser FBX,
dois de journal e um fluxo completo Biblioteca → receita por toque → preparo real →
publicação → instância → reimportação → Undo/Redo → falha → cold Play. O corpus
opcional solicita os 520 FBX originais POLYGON; ainda não foi executado nesta revisão.
Capturas do importador focal 655×300, build Android e aparelho permanecem pendentes.


## Referencias de workflow verificadas nesta integracao

[Unity6000.0 Model Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html):
a selecao do recurso abre suas opcoes; escala/unidades/eixos, geometria, normais e
tangentes pertencem ao preparo. Preservar hierarquia evita incompatibilidade com
arquivos de animacao separados. Na Astra, Biblioteca -> preparar ocupa a mesma
superficie contextual; nao acrescenta um painel fixo ao viewport mobile.

[Unity6000.0 Materials](https://docs.unity.com/en-us/engine/6000.0/manual/assets-and-media/asset-types/models/importing/fbximporter-materials):
subassets e materiais externos sao decisoes distintas. A busca global de texturas
pode ser nao deterministica quando ha nomes repetidos; por isso resolveremos GUID/meta
por pacote, seguindo a isolacao ja implementada para materiais Unity.

[Godot4.3 ufbx](https://godotengine.org/article/introducing-the-improved-ufbx-importer-in-godot-4-3/)
e [ufbx Nodes](https://ufbx.github.io/elements/nodes/) sustentam decoder nativo e
conversao declarada de eixos/unidades/pivos, evitando uma dependencia de Blender
no aparelho. O exemplo [ufbx viewer](https://github.com/ufbx/ufbx/blob/v0.20.0/examples/viewer/viewer.c)
mostra consumo da cena, mas nao sera copiado como segundo renderer da Astra.

Video localizado: [Gamefromscratch: FBX Godot4.3](https://www.youtube.com/watch?v=TGszmac4Y2k),
13/03/2024. Os metadados indicam comparacao FBX2glTF/nativo e uma etapa manual de
texturas. A ferramenta nao abriu o video nesta rodada; nao foi usado como evidencia
de observacao de cliques ou qualidade visual. Os contratos tecnicos acima derivam
das documentacoes oficiais e do codigo versionado.


Evidencia vigente: [importador focal executavel](../../validacao/n01-n07-2026-10-10/host/capturas/tga-focal.655x300.png)
e [resultado12/12](../../validacao/n01-n07-2026-10-10/host/material-texture-publication.log).
O probe recebe SharedAuthoringTexture efetiva pelo GeometryPublisher: isso verifica
republicacao e rollback de dados consumidos, nao prova uma imagem produzida pela GPU.
A captura usa o atlas CPU de preview da sessao, com adapter de raster somente no teste.
