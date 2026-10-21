# M14/M15 e ondulações: integração em validação

Continuação de [M13 estático](FX-ESTATICO-2026-10-20.md), na branch
`codex/terrain-voxel-generator`. Documento de trabalho: integração ativa ainda
sem aceite completo, commit próprio, APK novo ou publicação pública. Não encerra
M13/M14/M15 nem o plano71. Propriedades comprometidas permanecem no
[contrato integral](PROXIMO-M13-M15-2026-10-20.md).

## Integrado, aguardando verificações finais

- Correnteza: máscara B independente, fonte RG normal/B espuma, máximo/blend,
  refração, fase, ciclo UV, profundidade, normal/espuma, tint RGB, UV, transições
  por altura e espuma por camada. Fonte tipada e busca com opção Todos.
- Lava: máscara A, mapas RG distorção/B calor/A crosta, emissão e três cores RGB,
  frio/quente, distorção, velocidade, UV, transições e amostragem estocástica
  opcional. Duas fontes originais selecionáveis.
- Chuva local: fonte R forma/GB direção normal/A fase, quatro fases temporais,
  intensidade e repetição UV; requer poças ativas e fonte real. Não cria chuva
  atmosférica, partículas ou um controlador global implícito.
- Transição de material por altura mundial: atua em metallic/smoothness após
  neve; independente da máscara FX, com início/fim inclusive coincidentes.
- Barras com valores precisos, grupos contextuais, retorno explícito, fontes
  packed lineares sem controles falsos de sRGB/canal. Remaps/fator/inversão
  afetam o consumidor. Propriedades reservadas permanecem serializadas.
- Catálogo156 texturas/82 materiais; quatro originais lossless preservados em
  512²,256² e1024². Atlas646 preserva todos640 IDs anteriores, acrescenta quatro
  miniaturas e dois ícones vetoriais/384px de chuva/transição por altura.
- ABI root5 mantém root3/root4 quando os recursos correspondentes estão
  desligados. Caso sem máscara usa descritor ausente protegido; nunca um índice
  inventado. Relocação e recusas de dados inválidos permanecem atômicas.

## Transformações e evidência

A direção calculada no espaço mundial é convertida para o domínio local antes
de deslocar UV. Misturar WORLD.xz e LOCAL.xz funcionava apenas no terreno sem
transformação. O pullback usa derivadas horizontais, preserva amplitude e evita
normalização duplicada; caso degenerado vertical produz deslocamento zero
finito. Não implica simulação física ou equivalência numérica com outra engine.

Overlay inicial exato integrado:310/310 cenários GPU passaram, incluindo os284 anteriores,
18 de chuva/transição e oito de transformações. Prova algébrica24/24 contempla
oito transformações e três parametrizações. São fixtures32×32 em GPU do host;
não comprovam qualidade de uma cena natural ou desempenho Android. Asserções
anteriores foram preservadas. SHA dos54 arquivos integrados conferidos após
cópia; pacote imutável lido por completo antes de escrever os ativos.

Correção adicional em validação: o filtro de altura da refração agora usa worldY,
como a cobertura e os rótulos em metros. Altura PBR permanece apenas no cálculo
de profundidade/gap. É divergência intencional da peculiaridade da fonte que
misturava os dois domínios. Os312/312 cenários GPU passaram, incluindo os310
anteriores e os dois novos casos de altura. As44 famílias de shaders de produção
foram compiladas e seus SPIR-V validados; dois headers executáveis atualizados.

Auditoria do caminho Android encontrou que `prepareRows` deslocava os mapas PBR
ao concatenar biblioteca após texturas do pacote, mas esquecia o índice StaticFX.
O helper de deslocamento agora valida todos os índices antes de mutar e inclui
a máscara; sentinela desligada continua ausente. Quatro testes preparados
incluem prefixo4 e descritores77/203 distintos, equivalência da tabela offset0
com a tabela completa e recusa atômica de overflow/índice inválido. Correção
integrada; os quatro testes passaram dentro dos112/112 cenários Core.

Dois testes Python ativos de fontes e ícones passaram. A primeira execução de
ícones esperava quatro conceitos; foi atualizada para os seis conceitos reais,
preservando verificações de dimensões, alfa, hashes, IDs e rerun sem regravação.
Core112/112 aprovado. A primeira execução integrada passou218/224: dois fixtures
desatualizados (catálogo152→156 e teto128 inválido→256 válido) foram corrigidos,
mantendo validações de fonte/canais e acrescentando recusa explícita de128. As
quatro falhas UI novas identificaram busca SHA64 recusada por63, preset Stream
com metadado RGB incompatível com NormalRG e percurso de teste Lava tentando
acessar Fonte pela página UV. Busca até128 tem buffer transitório próprio ao
diálogo, sem alterar os nomes de cena; seleção/preset Stream preservam NormalRG
com upload raw sem mips de normal. O teste Lava passa por Geral e conserva todas
as propriedades/asserções. Reexecução integrada final: **229/229 cenários**.
Corrigidos também o percurso Chuva→Poças→Criar máscara e a ordem real de desenho
do diálogo de busca: Cancelar/Aplicar agora recebem toque acima do painel.
Não houve remoção de asserções para obter o aceite.

Capturas executáveis:96 painéis (incluindo rolagem),8 telas completas e2 diálogos
de busca em1280×720 e655×300. Conferência visual da versão compacta e ampla:
grupos contextuais, barras com valor preciso, rodapé fixo e teclado de busca
com SHA64 legível. São capturas do rasterizador da UI no host, sem cena3D;
não substituem a conferência Android. APK111 e aparelho ainda em validação.
ADB liberado pelo proprietário somente para esta entrega antes da
próxima nota pública; após isso o restante do plano prossegue sem ADB.

## Prévia contínua e espaço do APK

Revisão adicional21/10 antes do aparelho: **230/230 cenários integrados**.
A igualdade estrita do token de prévia descartava malhas concluídas enquanto
o dedo continuava acrescentando amostras. A publicação agora aceita um prefixo
verificado do mesmo gesto, com base autoral, cena/revisão/alvo, ponteiro,
época FX, orçamento e todos os campos das amostras correspondentes. Cancelar
continua restaurando a geometria aplicada e rejeitando conclusões tardias.
O teste usa movimento contínuo real sobre terreno planoY0, exige elevação
publicada antes de Up e verifica receita/histórico/cancelamento íntegros.

Empacotamento: a primeira compilação111 passou com649.627.581 bytes.
A tentativa inicial de excluir sources/ foi recusada pela auditoria ANTES da
instalação:111 entradas runtime usam diretamente PNGs originais nessa pasta.
O critério correto é a união exata de runtimePath+thumbnail do catálogo, não o
nome da pasta. O Sync agora envia313 arquivos necessários (catálogo,156 imagens
runtime e156 miniaturas), sem conversões/cópias não referenciadas. Todas as156
imagens foram comparadas por bytes/SHA e nenhuma foi reduzida ou alterada.
APK final448.089.078 bytes; economia201.538.503 bytes, cerca192,2MiB.
SHA2560702325979bd0435f49c40c9071cbf67ea6bb186dddd49711cfd0139aebf4074.
AssinaturaV2/certificado anterior, ARM64, ZIPCRC/alinhamento e nativo exato passaram.
Instalado por atualização, code111/0.3.1-dev.terrain.20261021.1, sem apagar dados.
Aparelho usa páginas4096. Há bibliotecas ELF que não têm alinhamento16KiB;
não anunciar compatibilidade16KiB integral. Teste de interação ainda em andamento
no novo projeto isolado TerrainQA111; os projetos antigos não foram editados.

## Referências concretas

Unity Shader Graph17.0.4: [Normal Vector](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Vector-Node.html)
e [Transform](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Transform-Node.html)
para distinguir espaço e tipo de vetor; [Time](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Time-Node.html),
[Sample Texture2D](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Sample-Texture-2D-Node.html)
e [Normal Blend](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Normal-Blend-Node.html)
para amostragem, animação e composição de normais. Referências consultadas
em20/10/2026; orientam capacidades, não autorizam afirmar paridade de editor,
material proprietário ou comportamento físico.

## Ainda pendente

Conferência visual adicional21/10: vídeo oficial Unity2019,
[Build Beautiful Terrains](https://www.youtube.com/watch?v=aExdxF4OKBo), canal
Unity verificado. Inspecionados146,331s e161,331s e três quadros consecutivos
pausados em161,331200/161,364533/161,397866s. A sequência curta conserva a
projeção violeta da forma sobre a superfície inclinada e o Inspector com máscara,
força e tamanho no mesmo contexto. Entre os trechos há um plano inicial e um
relevo já esculpido; isso sozinho não mede latência nem independência dos eventos.
Capturas de estudo ficam privadas em build/terrain-evidence/unity-video.
Princípios para a aceitação Astra: projeção aderente ao relevo, propriedades
acessíveis sem sair da ferramenta, base plana e navegação que não concorra com
pintura. Esta análise cobre trechos específicos, não o vídeo inteiro quadro a
quadro; também não prova execução equivalente no Android.

Controles globais, simulação dinâmica, três componentes de cena, upload por
instância/fences, tracks, interações adjacentes, import/rebase da máscara e
aceite integral do plano. Solver/cold-height/autoridade assíncrona existem em
staging com cenários preparados; não estão habilitados por um menu antecipado.
Nenhuma propriedade ou função foi removida do compromisso.
