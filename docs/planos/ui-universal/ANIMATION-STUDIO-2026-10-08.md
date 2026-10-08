# Animation Studio — evolução universal a partir dos pacotes fornecidos

Data: 08/10/2026. Repositório confirmado: `kacerato/attachsEngine`, base `b900826a`.
Estado: B2 entregue e validado em desenvolvimento; B3–B6 são plano de implementação.
O relato de aceite está em `docs/validacao/animator-additive-2026-10-08/REPORT.md`.

NÃO IREI SER SIMPLISTA NO DESIGN.

## Universo e rastreabilidade

Fontes locais: `Downloads/analise-plugins/analise-plugins/FinalIK-2.2` e
`UMotionPro-1.29p04`. BoZo fica excluído da integração solicitada.
Os manifestos fornecidos registram 503 e 280 arquivos, respectivamente, sem
contar .meta e previews auxiliares. São 47 FBX, 20 .anim, 43 .mat, cinco
prefabs, 27 controllers Unity; UMotion inclui 75 páginas HTML e três DLLs.
Contagem de presença não comprova importação, execução ou portabilidade.

Final IK fornece 245 scripts, incluindo algoritmos, utilitários, demos e
inspectors. Os solvers dependem de UnityEngine.Transform/Quaternion, lifecycle
Unity e, conforme a função, Animator, física e editor. Portar um algoritmo
significa adaptar matemática, ownership da pose e consumidor; copiar o arquivo
C# não cria uma integração. UMotion fornece seis scripts de apoio/API e núcleo
compilado: não afirmar que existe fonte do editor no pacote. Suas DLLs Unity e
seu exportador Windows não são backend Android da Astra.

Cada seção, tabela de propriedades e imagem das 75 páginas deve entrar no
inventário de pesquisa com origem, uso, dependências, API prevista, estado real
e cenário de aceite. Manter GUID/hash e dependências de assets e miniaturas.
Inventário de pesquisa e biblioteca utilizável são estados distintos. Não
publicar os pacotes, manuais ou fontes de terceiros no portal público.

## Arquitetura e fluxo proposto

Um workspace de animação com destinos Grafo, Clipes, Pose e Rig. Seleção,
tempo e binding compartilhados; destinos preservam seu estado de navegação.
O viewport segue dominante. A faixa temporal aparece ao editar/scrubbar um
clipe; lista de canais e propriedades ocupam uma superfície contextual
substituível, expansível temporariamente. Nada de quatro docks fixos mobile.

A comparação Origem/Resultado permite estudar o que retargeting, camadas ou
constraints alteraram. É proposta, não controle entregue. Antes de expô-la,
precisa de avaliação isolada, restauração ao fechar/cancelar, estado da física
separado e medição de custo. Diagnósticos devem indicar estágio, propriedade,
origem e motivo; painel textual permanente não substitui feedback no gizmo.

Pipeline pretendido: recurso/rig → máquina de estados → sampling tipado →
composição por propriedade → retargeting configurado no binding → constraints
ordenadas → extração/publicação. Root motion deve entregar trajetória ao motor
com colisão, nunca mover somente a malha. A ordem precisa ser resolvida conforme
o contrato de cada estágio; não usar essa lista como promessa de pipeline pronto.

## Blocos verticais e critérios

### B2 — composição aditiva — fechado em desenvolvimento

Pose inicial ou clipe/tempo explícito de referência; translação por diferença,
quaternion relativo normalizado e escala por razão. Morph por diferença.
Peso, máscara, transição, remoção, relógio e autoridade física usam consumidores
existentes. Camadas superiores de substituição atenuam as inferiores por
propriedade; aditivas não consomem o peso da pose base. Referência ausente ou
incompatível exige diagnóstico, sem substituir silenciosamente por outra pose.
Persistir autoria e migrar cenas/controllers antigos para Override. Controles
reais no grafo, Undo/Redo de recurso/local e API de execução por índice de camada,
preservando ajustes runtime pela identidade interna da camada,
com validação. Aceite: mecanismo não humano e modelo com skin; peso zero/meio/um,
referência não identidade, máscara, transição, salvar/reabrir e Play.

Implementado: Animator v4/AEANIMATOR 2 com leitura dos formatos anteriores,
compositor tipado de posição/rotação/escala/morph, referência inicial ou clipe/tempo,
diagnóstico e restauração de canais, autoridade física e máscara, consumidor
de skin, editor contextual, histórico e dez operações de composição no SDK.
16/16 cenários host; SDK e APK compilados; oito checks C# no POCO F7/Android 16,
autoria de peso 0,65 e referência 0,25 s salva e reaberta; todos os 417 frames
da captura revisados. Dev 0.2.7/code 15 instalado, hash igual ao pacote local.
Não comprova desempenho de multidões, retargeting, IK ou root motion. O public
0.2.3 permanece separado. O preview/Studio completo continua nos blocos seguintes.

### B3 — submáquinas e interrupções

IDs estáveis para máquina, estado, ligação e entrada/saída; caminhos em debug e
SDK, limites de profundidade e recusa de ciclos estruturais. Sem duplicar o
grafo ao entrar. Breadcrumbs, criar/mover grupo, seleção e Undo transacional.
Transição interrompida começa na pose composta capturada; gatilhos consumidos
uma vez, eventos com regra documentada e ordem determinística. Entrada,
saída, Qualquer estado local/global e destino externo precisam de contratos
explícitos. Aceite com três níveis, retorno, interrupção no meio do fade,
controller compartilhado, prefab e archive round-trip.

### B4 — clipes, curvas e autoria de poses

Recurso editável separado da fonte importada, vínculo de origem e versão;
reimportação preserva personalizações. Canais de transform, quaternion e morph
tipados, identidade estável, unidades, taxa de exibição versus tempo em segundos.
Criar/excluir/mover/duplicar chaves, seleção de intervalo, copiar/colar, snap,
trim, retime, reverse, loop, marcadores/eventos, tangentes e redução com erro
medido. Não converter quaternion para Euler destrutivamente. Auto-key com
transação única; alteração sem chave é preview, nunca edição oculta do asset.

Dopesheet e curvas compartilham seleção; scrub determinístico não dispara
eventos de gameplay por padrão. Mute/solo, camadas de autoria, referência,
mirror/copy pose e bake FK/IK dependem de consumidores reais. Preview pode
cancelar/restaurar a cena; salvar/exportar é publicação transacional. API deve
criar recursos e editar canais sem janela aberta. Aceite em mecanismo, rig,
morph, rotação contínua, importação existente e cancelamento após erro.

### B5 — rig, retargeting e constraints/IK

Rig genérico primeiro: nós/parentesco, transform de repouso, eixos, comprimento,
escala, papéis opcionais. Humanoide é perfil, não anatomia obrigatória. Mapping
manual, sugestões verificáveis, duplicatas/ausências e preview A/B. Compensar
repouso e orientação; translações têm política explícita por canal/papel.
Não confundir correspondência por nome existente com retargeting de proporções.

Constraints ordenadas com IDs, alvos locais/mundo, pesos, limites, ativação e
ownership. Pacotes: analítico de dois segmentos, CCD, FABRIK, mira/olhar,
limites e múltiplas cadeias; depois interação, corpo inteiro e dedos. Grounder
depende de consulta física, apoio móvel, contato/fase, pelvis e limites de
correção. VR exige tracking/calibração externo e não nasce de um enum VRIK.
Escolha de solver documenta custo, tolerância e casos inalcançáveis. Gizmos,
seleção de ossos/alvos, inspector e bake usam a mesma matemática do runtime.
Aceite em braço mecânico, quadrúpede e dois humanoides de proporções/eixos
diferentes; degrau, alvo inalcançável, singularidade, teleporte e referência
removida, sem produzir NaN ou disputar transforms com física.

### B6 — biblioteca de pacotes e importação

Cartão de pacote com miniatura, conteúdo, formatos, dependências, origem e
disponibilidade real. Modelos/clipes/materiais são recursos independentes,
endereçados por GUID, com criação de instância editável, não cenas hardcoded.
Pré-visualização do que será convertido e relatório do que não será preservado.

FBX: estudar ufbx/Assimp e contratos de pivô, eixos, unidades, skin, morph,
bind pose, take, curvas e materiais; converter para IR já consumido pela Astra.
Unity YAML: resolver .meta/GUID/fileID, variantes/prefabs/hierarquia, materiais
e texturas. .anim exige binding/caminho e curvas reais; .controller exige
parâmetros/transições/subassets. MonoBehaviour/shaders Unity não se tornam
comportamento Astra automaticamente; mapear suportados e preservar/diagnosticar
desconhecidos. PSD/TIF/TGA exigem codec/conversão e tratamento de cor explícitos.
Modelo fonte e overrides ficam separados; journal, cancelamento, dependência
ausente e reimportação entram no aceite. Importar material com textura não
equivale a portar um shader. Nunca exibir como utilizável asset só catalogado.

## API com liberdade real

Recursos de controller/clipe/rig, edição transacional, IDs/caminhos estáveis,
enumeração e diagnóstico. Separar API de autoria da de execução; parâmetros,
pesos, relógios, seek, bindings e alvos têm estado próprio por instância.
Operações de UI devem chamar as mesmas operações de documento disponíveis ao
SDK. Recurso e avaliação não exigem abrir painel. API não é Dictionary<string,
object>: canais, eventos, argumentos e retornos são tipados. Valores inválidos,
recurso ausente e conflito de revisão retornam erro e preservam o documento.
Expor todos os campos efetivos do bloco entregue; não publicar setters que
sejam sobrescritos pela resolução de controller sem produzir efeito.

## Inspeção do processo dentro da engine

Biblioteca → importar → conferir bindings → criar instância → configurar rig
→ criar/editar clipe → scrub/cancelar → compor camadas → grafo/submáquina → IK
→ Play → alterar pela API → salvar → fechar/reabrir → prefab/reimportar.
Executar em objeto genérico, humanoide e quadrúpede. Cada etapa registra autoria,
pose observável, diagnóstico e valores preservados. Conferir capturas reais e
todo frame dos vídeos usados no aceite; vídeo de referência não assistido não
vira alegação de workflow observado. Builds, host, pacote, instalação e
aceite físico são evidências separadas. Medir muitas instâncias em Release
somente quando a funcionalidade correspondente existir.

## Referências concretas

- UMotion Pro 1.29p04: `Manual/ClipEditor.html`, `PoseEditor.html`, `Curves.html`,
  `Layers.html`, `InverseKinematics.html`, `Configuration.html`, `ImportExport.html`
  e suas imagens locais. https://soxware.com/umotion-manual/Curves.html
- Final IK 2.2: `IKSolverTrigonometric.cs`, `IKSolverCCD.cs`, `IKSolverFABRIK.cs`,
  `Shared Scripts/SolverManager.cs`, Grounding, inspectors e Baker fornecidos.
- Unity 6000.0, camadas: https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationLayers.html
- Godot 4.5, rigs/repouso: https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/retargeting_3d_skeletons.html
- Godot 4.5, máquina: https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/animation_node_state_machine.cpp

Este plano incorpora dependências, mas não promete eliminar para sempre a
necessidade de novas capacidades. Só marcar bloco fechado depois do seu aceite.
