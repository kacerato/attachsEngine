# Animation Studio — evolução universal a partir dos pacotes fornecidos

Data inicial: 08/10/2026; atualizado em 09/10. Repositório confirmado: `kacerato/attachsEngine`, base inicial `b900826a`.
Estado: B2 e B3 entregues e validados em desenvolvimento; B4 em implementação; B5–B6 seguem no plano.
Em 09/10 o recorte bake por trilha/UI/API do code 21 foi aceito no aparelho,
com 477/477 quadros revisados. Guia e Atualizações publicados no AstraDocs main
`332e3e5`, Vercel READY; APK público 0.2.3 preservado. Relatório atual:
`docs/validacao/animation-clips-2026-10-08/BAKE.md`.
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

B3 implementado e aceito na revisão Dev 0.2.8/code 16: Animator v5/AEANIMATOR 3; três níveis, IDs/caminhos estáveis, entrada condicional, saída encadeada e Qualquer estado por escopo. Cinco políticas de interrupção e captura por propriedade, offset/unidade; transporte SDK sem truncar caminho. Editor cria/move/duplica/remove árvores com histórico, navegação com vista preservada, breadcrumbs profundos e ligações selecionáveis separadamente. Ícones machine/interruption são SVG/PNG/atlas efetivos. Host 25/25, SDK/sonda compilados, Android Release; 15 checks C# no aparelho antes/depois da autoria e novamente no APK final. Toque, histórico, salvar/encerrar/reabrir preservam grupo movido e política/offset; 598/598 frames revisados nas 20 folhas. Relatório: `docs/validacao/animator-hierarchy-2026-10-08/REPORT.md`. Guia e Atualizações publicados separadamente no AstraDocs. Isto fecha B3; B4–B6 e a equivalência completa dos pacotes continuam pendentes. API de autoria de topologia/clipes por C# pertence à próxima expansão; não é presumida pela API runtime deste bloco.

Referências desta revisão: Unity 6000.0 `NestedStateMachines.html`, `StateMachineTransitions.html` e `class-Transition.html`; código Godot 4.5-stable `animation_node_state_machine.cpp`. Tutorial oficial Unity: https://www.youtube.com/watch?v=lpekqN4_4xg (Unity 5), transcrição estudada e quadro do grafo inspecionado em 2:33; não declarar o vídeo inteiro analisado quadro a quadro. Entrada, saída e prioridade foram confrontadas com o runtime; ligação de máquina mostra apenas destino/condições/prioridade, pois o blend pertence à saída iniciadora.

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

O universo de B4 também inclui os três modos descritos em `RotationModes.html`:
Euler autorável, quaternion e quaternion progressivo com curva de velocidade.
Não confundir editar quatro curvas escalares com oferecer esses três workflows.
`CustomProperty.html` exige drivers com remapeamento/limites, propriedades
tipadas de componentes (incluindo bool) e parâmetros do Animator, com preview
restaurável. `ImportExport.html` exige importação de múltiplos clipes, conversão
FK/IK, redução e relatório dos formatos exportados; parte depende de B5/B6.
Essas dependências continuam na meta integral dos pacotes, mesmo atravessando
a fronteira entre blocos. Os controles só entram como disponíveis após consumidor
real e aceite. Um bloco de infraestrutura não fecha o Studio completo.

Implementação B4 em andamento: recurso `AECLIP` independente da fonte; IDs de
binding/canal/chave e revisão; curvas por componente, tangentes ponderadas,
subdivisão preservando o segmento, retime/reverse/crop; sampling pelo consumidor
existente, caminhos relativos sem fallback ambíguo, publicação pelo journal do
projeto, histórico de edição do recurso e preview em cópia isolada da cena.
Há uma superfície nativa executável de clipes/curvas, scrub, transporte,
seleção/arraste, propriedades numéricas, tangentes, paginação, Undo/Redo e
criação de quaternion, Euler ou progressivo. Os três ícones curve/key/tangent
foram integrados como SVG/PNG/atlas ao pipeline real. A nova proposta visual é
hipótese; as capturas host usam as instâncias reais da UI, mas não provam cena
Vulkan ou uso no aparelho.

Euler conserva voltas e tempos independentes por eixo usando a convenção
Rz·Ry·Rx da cena Astra; ela não imita a ordem Z·X·Y da Unity. Quaternion linear
usa o menor arco. Progressivo possui poses sincronizadas e distância angular
acumulada, com handles de velocidade ponderados e overshoot; a distância deriva
das poses. Edição de pose recalcula a distância e ajusta os handles. Corte
progressivo subdivide a curva exata quando o limite cair em uma volta/overshoot;
não é uma aproximação por amostras. Conversão sem perda entre quaternion linear
e progressivo linear é explícita; trocar Euler ou remover curva de velocidade
exige bake explícito. O backend, a faixa contextual e o SDK ABI 2 de bake estão
validados no host e aceitos no code 21 em 09/10, conforme o registro abaixo.

20 cenários passaram no host em 08/10: 14 de recurso e seis de integração.
A superfície executável possui dopesheet com várias linhas, curvas, seleção de
chave, handles, teclado numérico/nome, picker de hierarquia/propriedades e edição
de pose completa. Posição, escala, rotação nos três modos e pesos de morph usam
operações tipadas; selecionar um filho cria binding relativo canônico, com
recusa de irmãos ambíguos. Editar a pose gera uma chave agrupada no tempo atual,
sem modificar o transform salvo. Criar/remover canais limpa bindings sem
reutilizar IDs. Criação, edição e remoção de canais possuem journal e histórico;
Undo/Redo da criação remove/restaura arquivo, registro e biblioteca juntos.
Excluir um recurso arbitrário pelo Asset Browser ainda exige ampliar o histórico.

As capturas em `build/animation-clip-ui/` são da UI nativa executada com
fontes/atlas de produção. A área vazia de cena dessa rasterização não é prova
Vulkan nem Android. APK Dev 0.2.9/code 17 instalado: autoria de pose progressiva
em um filho genérico, Undo/Redo, reprodução, fechar preview e encerrar/reabrir
projeto conferidos no aparelho. Todos os 598 quadros da reprodução foram
extraídos e examinados nas 20 folhas. Relato e limitações:
`docs/validacao/animation-clips-2026-10-08/REPORT.md`. Esse era o aceite do code 17;
o aceite do bake/code 21 e a publicação editorial posterior são registrados abaixo.

A expansão seguinte extrai um clipe já importado para recurso editável separado,
mantendo GUID/hash da origem e sampling Step/Linear/CubicSpline sem resampling.
O picker distingue recursos editáveis e fontes importadas. A integração inclui
preview, edição, journal, histórico e carga do registro real em disco. A sonda
usa um rig GLB real com skin e compara 201 tempos por canal; não é evidência
de renderização desse rig no aparelho. A extração não constitui merge de
reimportação. A revisão code 18 foi compilada e instalada: extrair “Meia abertura”,
editar a pose em 0,5 s, Undo/Redo e reabrir após encerrar o processo foram
conferidos no aparelho. O GLB original mantém seu hash. O aceite não é
evidência de skin/IK/retargeting. Capturas e identidade do APK estão no relatório.

Seleção por área/IDs, grupos quaternion completos, transformação/exclusão
atômicas e clipboard tipado estão conectados à timeline e ao histórico.
O APK de desenvolvimento code 19 passou no aparelho por copiar, substituir,
inserir, mover, escalar, recortar, Undo/Redo, preview e reabertura a frio.
Foram examinados todos os 417 quadros da gravação do fluxo. A cópia editada
reabriu byte a byte igual; a fonte GLB manteve seu hash. O cenário físico contém
um track de rotação; a distinção entre inserir nos tracks selecionados e em
todos os tracks também possui cenário host com tracks não selecionados.
Essa entrega não declara concluídas todas as ferramentas de intervalo de B4.

A API nativa de autoria funciona sem o painel: catálogo real, criação/extração,
rascunhos isolados, snapshot versionado, bindings, curvas/poses, clipboard,
amostragem pelo runtime e commit único no histórico. Revisão vencida, contexto
de outro comando/thread e handles encerrados são recusados. A integração passou
em 25/25 cenários host, incluindo a rota contextual de ferramentas do IDE.
SDK C# e host de comandos publicados foram conectados ao backend. Uma
verificação C#/DLL nativa real passou sem skips: rascunhos, curvas/poses,
morphs com seis valores, grupos de rotação, sampling, commit, compositor,
Undo/Redo, descarte, vida/thread do contexto e compilação publicada. O modelo
“Ferramenta de animação” faz parte do caminho de criação e compila com os
demais modelos. Não constam no APK code 19. O code 20 foi compilado, instalado
e aceito no aparelho: criar modelo no IDE, publicar, executar sobre objeto
genérico, abrir/editar o recurso, preview e reabertura a frio. Todos os 474
quadros da gravação foram examinados; arquivo reabriu igual e fonte permaneceu
intacta. A revisão não foi distribuída publicamente.

A expansão de bake usa grade configurável, extremos/tempos de descontinuidade,
refinamento, redução opcional e comparação pelo sampler real. Conserva IDs
existentes no mesmo componente/tempo, bindings/fonte e trilhas não selecionadas;
publicação/Undo/Redo seguem o journal e alocadores monotônicos. A UI substitui
temporariamente o transporte por ajustes/resultados, sem folha cobrindo a cena
ou o gráfico. Capturas 853×394 e 655×300 foram examinadas. Host 27/27 e SDK C#
com ABI 1/2 passaram; o APK code 21 foi compilado e instalado com hash igual ao
pacote local. Em 09/10 foi aceito no aparelho: conversão por toque, redução
605 → 15 chaves, Undo/Redo e sonda SDK compilada/publicada no IDE com duas
voltas Euler → Quaternion (183 → 32 chaves escalares). Todos os 477 quadros
das duas gravações foram examinados nas 16 folhas; recursos UI/API reabriram
byte idênticos, fonte GLB preservada. Esse aceite fecha o recorte de bake,
não o B4 inteiro nem a biblioteca dos pacotes.
Nessa revisão, Quaternion/progressivo → Euler era recusado; erro medido em amostras não é
certificação contínua. Relatório específico: `docs/validacao/animation-clips-2026-10-08/BAKE.md`.

Camadas de autoria não destrutivas entregues na revisão Dev 0.2.10/code 22:
AECLIP 3, ABI 3, Base permanente, máscara esparsa por propriedade, ordem,
duplicação/remoção/nome, Override/Additive, peso, referência e Mudo/Solo.
Mesma composição no Studio, Animation e Animator, com API C# tipada, snapshots,
transações, Undo/Redo, migração e recusas explícitas. Faixa contextual e novos
ícones integrados ao atlas; Pose horizontal em viewport baixo. Host 31/31 e
ponte C# ABI 1/2/3; APK instalado/hash conferido. No POCO F7: autoria por toque,
comandos C# embarcados, cold reopen e cinco verificações de composição/isolamento
no Animator. 909/909 quadros do aceite examinados em 31 folhas. Relatório:
`docs/validacao/animation-layers-2026-10-09/LAYERS.md`. Fecha o recorte de camadas,
sem declarar todo o B4 ou a equivalência com os pacotes.

Conversão e consolidação (09/10, revisão Dev code 23): Quaternion/Progressivo
→ Euler XYZ com referência inicial explícita, levantamento equivalente próximo
e tratamento de gimbal; não oferece outras ordens nem recupera voltas perdidas.
Consolidação amostra a composição real de todas as propriedades, publica outro
GUID/caminho com Base única, preserva fonte e tem Undo/Redo de recurso completo.
ABI 4/C# preservam prefixos 1/2/3, rascunhos e publicação independentes do painel.
UI usa duas linhas contextuais, alternância XYZ, confirmação do ramo, Canal/Clipe
novo e relatório; ícones novos percorrem SVG/PNG/atlas. Host 35/35, ponte real
C#/nativa com ABI 1/2/3/4 passou. Aceite físico/publicação detalhados em
`docs/validacao/animation-consolidation-2026-10-09/REPORT.md`.

Ainda faltam demais ferramentas de intervalo e bake FK/IK/root motion,
eventos, propriedades arbitrárias/drivers, auto-key por
manipulação no viewport, mirror/copy pose, reimportação e workflow completo dos
pacotes. `sourceOverride` está persistido, mas ainda não é uma política de merge
de reimportação. Não há equivalência declarada com o B4 completo. Os pacotes
continuam catalogados, com zero assets importados como biblioteca utilizável
nesta expansão.

Referências B4: Unity 6000.0
https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-UsingAnimationEditor.html;
Godot 4.5-stable `Animation::track_set_path`, `position_track_insert_key` e
`track_remove_key`: https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/animation.cpp;
glTF 2.0.1, interpolação e recusa de quaternions nulos:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations.
Unity 6000.0
https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Keyframe.html e
https://docs.unity3d.com/6000.0/Documentation/Manual/EditingCurves.html;
https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationEulerCurveImport.html
e https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.SetEditorCurve.html;
Godot 4.5-stable `scene/resources/animation.cpp`. Manual local UMotion 1.29p04:
`DopesheetCurves.html`, `Curves.html`, `Layers.html`, `PoseEditor.html`,
`RotationModes.html`, `CustomProperty.html` e `ImportExport.html`.
Tutorial oficial https://www.youtube.com/watch?v=6jsyZifFtGQ: transcrição integral
lida, quadro real em 1:55 examinado. Isso não declara o vídeo todo analisado
quadro a quadro. As imagens locais de ClipEditor, CurveView e PoseEditor foram
examinadas para estabelecer o vínculo entre seleção, propriedade e preview.

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
