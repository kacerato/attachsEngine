# Edição das cenas de teste

## Fluxo implementado

O launcher ativa o editor nativo para Ocean/Boat, Forest e Empty. Empty usa a biblioteca de malhas do pacote Forest, com documento inicialmente vazio. Material Preview continua sendo um preview; não possui adaptador de documento. Backroom, Vehicle e River continuam sem cena implementada.

O documento agora deriva dos draws realmente carregados do AEMAP. Não usa a hierarquia fictícia Water Lab. A aba Assets instancia as malhas do pacote na posição original; permite reconstruir a geometria de uma cena vazia. São lotes processados do pacote, não os objetos originais de um arquivo glTF.

- Um dedo orbita; dois dedos deslocam e aproximam. Gestos pertencem ao editor em Scene e ao controlador de runtime em Play.
- Os dois controles superiores esquerdos enquadram seleção e conjunto visível. A vista começa na câmera original do pacote.
- Toque num campo numérico abre entrada exata; arraste horizontal ajusta o valor. Confirmar ou terminar um arraste cria um único passo de undo.
- Os modos Move, Rotate e Scale alteram os respectivos valores. A posição do gizmo usa a transformação mundial; translação converte o deslocamento para o espaço do pai.
- Assets ou `+` abrem a biblioteca de malhas. Menu da hierarquia/Inspector oferece duplicar subárvore, criar grupo e excluir.
- Visibilidade e atividade são herdadas dos pais. Geometria, limites de culling, normais e sombras recebem a mesma transformação.
- Duplicação compartilha geometria e cria outra instância GPU; excluir e undo/redo alteram a submissão real.

## Representação e ownership

`EditorMapScene` possui uma cópia imutável dos registros do pacote. `assetId` é o índice de draw mais um dentro desse pacote. Transformações autoradas são aplicadas ao redor do centro original de cada lote; rotação/escala iniciais no Inspector são offsets neutros sobre a matriz importada, não uma decomposição da transformação original do asset. Isso preserva matrizes importadas, inclusive orientações que não devem ser reconstruídas por Euler.

O documento possui entidades e hierarquia; o histórico captura seus valores. `MapDrawState` é um contrato do renderer sem dependência do editor ou Android. A sessão extrai esse estado quando a revisão muda. O renderer publica os buffers somente depois da fence do frame adquirido. A criação de instâncias pode realocar o buffer nesse ponto seguro. Geometria e texturas continuam pertencendo aos recursos do pacote.

Na vista autorada, LOD0 é o estado inicial visível; níveis derivados importados começam ocultos. A edição invalida os caches de sombra e temporal. A vista recortada usa seu próprio aspect ratio e viewport/scissor transformados para a superfície; a interface restaura viewport/scissor completos. HZB e reprojeção temporal que pressupõem câmera ocupando o alvo inteiro não são usados nessa vista. O caminho autorado usa submissão direta para enviar os parâmetros de material por instância; isso também evita misturar overrides diferentes no mesmo lote. A política otimizada do runtime sem editor continua disponível.

## Persistência

Formato textual `AETHER_EDITOR 2`, com leitura e migração de `AETHER_EDITOR 1`, com fingerprint do pacote, identidade, hierarquia, nome, transformação, flags e referência de malha. Floats usam precisão suficiente para roundtrip e locale clássico. O leitor limita tamanho/contagem/IDs, rejeita versões desconhecidas, truncamento, resíduos e pacote incompatível, e só substitui o documento após validação completa.

O salvamento usa arquivo temporário, flush/sync e substituição atômica pela abstração existente de plataforma. Salvar, autosave após três segundos fora de uma transação e pausa do Android persistem a cena. O caminho privado do app inclui hash do caminho do projeto e fingerprint do pacote para evitar compartilhar edições entre projetos do mesmo template. Esse arquivo ainda não é o `scenes/main.ascene` indicado no descritor Java: exportação/migração entre esses formatos permanece pendente. Arquivo inválido é preservado; novas alterações usam sufixo `.recovered`.

## Validação e limites

Host Windows: testes de gesto, entrada numérica exata, cancelamento, transações, modos de gizmo, instanciação, duplicação/undo/redo, herança, extração, corrupção e roundtrip. Testes também carregam os pacotes reais Ocean (14 draws) e Forest (1.029 draws), salvam/recarregam e verificam todos os registros e matrizes. Prévia da interface e teclado numérico foi rasterizada e inspecionada em 853×394, sem descarte de instâncias ou glifos ausentes. Essa prévia usa rasterizador de UI e não comprova a cena Vulkan.

Ainda **não atende ao critério de editar perfeitamente cada detalhe das cenas**. Faltam: edição de referências de texturas e classes de material, criação de luzes independentes, parâmetros de água, corpos físicos e scripts; metadados originais de objetos que o cozimento agregou em lotes; seleção por triângulos (hoje limites esféricos), renomeação; importação de novos recursos e exportação do projeto. Rotate/Scale usam eixos apresentados como controles lineares e valores Euler locais, não um manipulador de rotação por anéis. Play ainda usa a simulação existente da cena de teste e seus corpos, sem reconstruir o mundo físico a partir de todas as alterações autoradas.

Na primeira etapa não havia ADB. Em 08/09, o usuário informou que o aparelho estava conectado e pediu avançar antes de acessá-lo. A validação continua exclusivamente local: não houve teste por toque, captura Vulkan, validação de surface rotation no aparelho, medição GPU, soak térmico ou verificação de lifecycle real. Builds host/Android e testes não substituem essa etapa.


## Continuação de 08/09: aparência e hierarquia

`editorNumericProperties` é a tabela compartilhada de nome, localização, grupo, limites e passo de arraste. Inspector, entrada numérica e validação usam a mesma tabela. Campos de material: RGB linear, rugosidade, metalicidade, força da normal, especular, RGB emissivo e potência emissiva. Valores iniciais vêm do material importado. A primeira edição habilita override por instância, preservando referências de textura, alfa e classe de pipeline do recurso. Duplicar preserva valores e permite editar cópias independentemente. Undo restaura também a habilitação do override.

Lighting mostra controles globais relativos ao ambiente importado: multiplicadores de sol, ambiente e exposição, mais deslocamento da rotação do céu em graus. Eles ficam na raiz do documento, são serializados e passam pelo histórico. `renderer::adjustEnvironmentLighting` sempre aplica os valores ao ambiente original, sem acumular multiplicações entre frames. Não equivale a adicionar luzes pontuais ou spots.

O menu da hierarquia oferece mover acima/abaixo, mudar pai por seleção em duas etapas e mover para raiz. `reparentKeepingWorld` calcula a nova transformação local antes de alterar a árvore; rejeita ciclos, matrizes singulares e shear não representável por TRS. Mudança de pai e transformação são uma transação. Enquadrar grupo usa os limites dos descendentes visíveis.

A suíte local cobre isolamento de material entre instâncias, passagem para os parâmetros do renderer, persistência v2, migração v1, validação, iluminação sem acúmulo, campos acessíveis pelo roteamento de toque, reparent/undo/redo e recusa atômica de ciclos e shear. As abas foram rasterizadas e inspecionadas em 853×394. Efeitos finais no shader Vulkan e custo da submissão direta ainda precisam ser medidos no aparelho.


### Continuacao local: nomes, hierarquia e projeto

Renomear no menu da hierarquia abre entrada touch portavel (letras ASCII,
maiusculas, numeros, espaco e pontuacao). Aplicar gera um comando de historico;
cancelar nao altera a cena. Nomes UTF-8 existentes sao preservados e backspace
remove um codepoint inteiro; entrada de acentos via IME Android ainda pendente.
Grupos podem recolher/expandir; a rolagem conta apenas linhas expandidas e e
limitada novamente apos exclusoes ou mudancas no tamanho do painel.

A cena editada passa a ser gravada em `scenes/editor.aescene` no projeto.
Novos descritores anunciam `editorScene`; `main.ascene` permanece como entrada
do template, sem converter indevidamente o formato JSON de runtime. Projetos
antigos usam o mesmo caminho por convencao. Na primeira abertura, o arquivo
privado anterior e carregado se valido e preservado como backup de migracao.
A escrita continua atomica. Arquivo principal invalido e preservado; uma
recuperacao valida e reaberta. Se a recuperacao tambem estiver invalida, o
salvamento automatico e suspenso e o erro e registrado, preservando os dois.
Esse arquivo ainda depende do fingerprint do pacote de geometria original;
nao constitui exportacao de jogo nem substitui validacao em dispositivo.

Validacao: 681 testes host, incluindo gestos de renomeacao, rejeicao de nome
vazio, cancelamento, undo/redo e recolher/expandir. Preview rasterizado do
teclado em 853x394 inspecionado. Android Debug e Release compilados sem ADB.


### Validacao Android e correcoes de integracao (08/09)

No aparelho 25053PC47G, o encaminhamento Android cancelava a captura do editor
apos cada evento consumido. O Down era perdido antes do Move/Up, impedindo
selecao e gestos apesar dos testes isolados da sessao passarem. Agora somente
os controladores de runtime sao cancelados quando o editor possui o toque.
Cancelamento do editor permanece nos eventos de lifecycle e ACTION_CANCEL.
Selecao na hierarquia, orbita, entrada numerica de escala 1 -> 2 e Undo -> 1
foram observados no aparelho. HUD de runtime aparece somente em Play; o popup
Java de diagnostico nao sobrepoe o editor. Rotulos respeitam seu retangulo.

Foi observado crash VMA no teardown com alocacoes remanescentes: faltava
encerrar VulkanUiRenderer em InstancedRenderer::shutdown. Seus atlas e buffer
agora sao liberados antes do render pass e do allocator da superficie.

Apos a correcao de teardown, Home -> reabrir manteve PID 20736 e completou
DestroySurface/CreateSurface, com interface e selecao novamente funcionais.
Play agora usa o corpo inteiro abaixo da barra, sem Inspector, hierarquia ou
gizmos capturando input de runtime; Scene restaura os paineis. Teste de
regressao verifica essa transicao: 682/682 testes host passaram.


### Criterio de autoria a partir de cena vazia

O aceite exige reconstruir Forest e Ocean em projetos vazios, com objetos,
recursos e propriedades persistentes; reabrir e executar o resultado. Os draws
do pacote nao equivalem a objetos originais. Continuam obrigatorios: catalogo
de recursos com identidade estavel independente do pacote, preservacao dos
objetos e referencias no importador, ambiente/agua/fisica como dados da cena,
e runtime criado a partir desses dados. Nenhum teste de interface isolado
substitui esse fluxo completo.

Navegacao agora expoe Orbita/Pan/Zoom para arraste com um dedo; a pinca de dois
dedos continua disponivel. Instanciar uma malha coloca seu pivot no alvo da
camera, em vez de reutilizar coordenadas absolutas do exemplo. Selecionar via
viewport revela ancestrais recolhidos e rola a hierarquia ate o objeto. Novos
testes verificam os modos de navegacao e a revelacao numa lista extensa.
Validacao host: 684/684; builds Android Debug e Release.


### Contrato de origem e referencia de viewport

Referencias oficiais: Godot introduction_to_3d (gizmos, local/global, snapping),
Unity SceneViewNavigation (focus, navigation, aligned views), Unreal viewport
controls (transform manipulators and navigation). Aplicacao ainda em andamento:
aneis de rotacao, planos de translacao, coordenadas locais/globais, vistas
alinhadas e selecao geometrica devem substituir controles lineares aproximados.
Os botoes textuais de navegacao foram substituidos pelos icones rasterizados
ja existentes no atlas PNG do projeto.

O cooker agora respeita matrix OU TRS glTF e rejeita ciclos/parentesco duplicado.
Emite scene.authoring.json com IDs deterministas por fonte/tipo/indice, nomes,
parentesco, matrizes locais, referencias a malhas compartilhadas e lista dos
draws derivados por objeto. Os IDs sobrevivem a recook com a mesma identidade
e ordenacao da fonte; reordenacao de nos ainda exige reconciliacao de import.
O consumidor Android desse catalogo ainda nao foi integrado.
Inspecao do GLB original Forest: 56 nos e 27 malhas, versus 1029 draws derivados.
Parte da vegetacao ja esta agregada nas malhas do proprio arquivo de origem;
nao e correto prometer uma arvore por objeto apenas restaurando esses nos.
Testes cobrem TRS hierarquico, recurso compartilhado, identidade e ciclo.


### Recursos fonte independentes

`tools/export-authoring-assets.py` extrai um glTF por malha, com cena local
neutra e referencias a um buffer compartilhado enderecado por conteudo. O
catalogo conserva as instancias e suas matrizes separadamente. Dados de
material, imagens embutidas e accessors continuam referenciando o mesmo buffer.
Skeleton/skin requer outro caminho e e recusado; animacoes nao sao exportadas
por este importador estatico. Este e um artefato fonte, nao um runtime loader.

Execucao real: Forest -> 56 objetos/27 recursos; Boat -> 1 objeto/1 recurso
com 3 primitives. Comparacao de TODOS os atributos de vertices e indices dos
27 primitives Forest e 3 Boat contra os GLBs originais passou exatamente.
Teste automatizado verifica duas instancias com transforms distintos usando
um recurso, buffer compartilhado, IDs e resultado estaveis em reimportacao.
A integracao ao Asset Browser/runtime e os manipuladores geometricos de
viewport permanecem pendentes; nenhuma destas provas encerra o objetivo.


### Manipulador de rotacao geometrico

Rotate desenha tres aneis em planos mundiais. O raio do toque intersecta o
plano escolhido; atan2 determina o angulo e remainder acumula passagens em
+/-pi sem saltos. O arraste congela camera, matriz mundial e pai, aplica a
rotacao mundial ao redor do pivot e converte de volta para TRS local. Um pai
com escala nao uniforme que gere shear e recusado explicitamente, sem
corromper a transformacao. Rotacao nao usa mais pixels como graus na sessao.
O modo local e a edicao de shear continuam pendentes.

Teste integrado: arraste de .5 rad no anel Z -> 28.64789 graus, um comando de
historico e undo exato. Teste matematico cobre pontos projetados, raio paralelo
e centro indefinido. 686 testes host passaram; Android Debug/Release compilados.
Referencia de implementacao consultada: WickedEngine Editor/Translator.cpp
(https://github.com/turanszkij/WickedEngine/blob/master/Editor/Translator.cpp).
Esta implementacao foi escrita para as matrizes e input existentes, sem
copiar trechos de terceiros. Referencias Stride e Godot continuam no escopo.

No aparelho 25053PC47G, arraste no anel Z alterou a geometria e o Inspector
para -90.2 graus; captura device-ring-drag.png. Undo acionado depois para
restaurar o objeto. APK desta implementacao instalado.


### Agua configurada pela cena (primeira integracao)

A raiz possui override de 13 propriedades de agua: altura/velocidade/inclinacao
ondas, microondas, opacidade, absorcao, espuma, rugosidade, turbidez, IOR,
direcao, nivel e densidade. Settings expoe Water pelo mesmo editor numerico,
validacao e historico. O arquivo AETHER_EDITOR 3 salva valores e habilitacao;
v1/v2 continuam legiveis e nao habilitam override implicitamente. Na migracao,
os valores iniciais exibidos sao hidratados dos controles existentes.
Android aplica esses valores ao caminho de perfil/espectro/optica ja existente;
densidade tambem alimenta a simulacao naval. A comparacao de propriedades evita
reconfigurar agua em resposta a transforms de outros objetos.
Ainda e um unico perfil global da cena, nao corpos de agua independentes.
Espectro detalhado, wake, colisores e criacao da malha/volume continuam pendentes.
