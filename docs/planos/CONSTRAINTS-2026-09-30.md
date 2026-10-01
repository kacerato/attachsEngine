# Transform Constraints — 2026-09-30

## Capacidade e referências

Quatro tipos com dados tipados, consumidor runtime, lifecycle, persistência versionada e propriedades reflexivas: PositionConstraint, RotationConstraint, ScaleConstraint e AimConstraint. Implementação validada no host: cinco testes nativos passaram, incluindo criação pelo editor, undo, save, clone, gizmo e GameWorld. Capturas reais do executável host foram inspecionadas em 853×394 e 1200×700. Nenhuma execução em aparelho ou validação visual Android foi feita. Não há LookAt alias: Aim já cobre orientação para um alvo com seis eixos locais de mira.

Referências oficiais Unity 6.0 (6000.0):

- [Position Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html)
- [Rotation Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html)
- [Scale Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html)
- [Aim Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AimConstraint.html)
- [Código oficial dos bindings, branch 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Modules/Animation/ScriptBindings/Constraint.bindings.cs)

O modelo de fonte, influência e seleção de eixos vem dessas referências. Unity oferece múltiplas fontes, pesos por fonte, captura de repouso/offset, Lock e controles de editor próprios. Astra adapta para uma fonte explícita, peso global e transformação de mundo; não promete equivalência integral da API Unity. Não há lista de Sources falsa. ParentConstraint, LookAtConstraint distinto, múltiplas fontes, Lock/Activate e up-object são pesquisados, não entregues: quatro implementados no pacote, zero stubs expostos.

## Caminho vertical

`ComponentSchema → ComponentValue → propriedades/reflection → arquivo de cena/prefab → GameWorld → SceneConstraints → setWorldTransform → renderer/physics consumers`.

Cada propriedade usa o caminho existente de edição/undo, API refletida e serialização; referências aparecem em `ComponentType.references`, por isso cloneSubtree/prefab remapeiam alvos internos pela implementação comum. Alvos externos conservam identidade. IDs do grafo não reciclam; remover fonte não reaponta para outro objeto. Fonte inválida/inativa é diagnosticada e não consumida; seus canais preservam a pose corrente. Draft sem fonte continua salvável; readiness bloqueia sua apresentação como executável quando ativo com peso positivo.

O pass público `SceneConstraints::advance(GameWorld&)` não depende do editor. EditorPlayScene executa Update, timers, animação, física, LateUpdate, constraints e CameraFollow também no avanço manual pausado. Begin/stop limpam o cache. Animação fornece a pose de entrada; constraint altera apenas seus canais, depois da animação. Corpos/characters conservam TransformAuthority: constraint não sobrescreve suas poses. CameraFollow conserva prioridade final sobre posição da câmera quando coexiste; uma câmera como source lê a pose anterior de follow, pelo contrato de ordem atual. Evitar usar CameraFollow em cadeias que exigem a posição posterior de follow.

## Semântica

- Position: posição de mundo da fonte mais offset em unidades de cena.
- Rotation: Euler de mundo da fonte mais offset em graus, interpolação pelo menor arco por canal. Esta é adaptação da convenção Euler Rz Ry Rx da Astra, não mistura quaternion Unity. Gimbal singularities seguem a transformação existente.
- Scale: escala de mundo da fonte multiplicada pelo fator por eixo (default 1, mínimo 0); peso mistura esse resultado com a entrada. Transformação que introduz shear/reflexão ou não pode ser decomposta pelo grafo é recusada com InvalidPose.
- Aim: orientação local +X/+Y/+Z/-X/-Y/-Z em direção à posição de mundo da fonte. World-up seleciona X/Y/Z. Offset é angular Euler após construir a base. Fonte coincidente ou mira paralela a world-up preservam pose de entrada e diagnosticam DegenerateAim.
- Eixos X/Y/Z selecionam os canais **de mundo** consumidos. Em Aim são canais Euler resultantes, não o eixo de mira.
- Peso [0,1] é influência, não taxa por frame: o cache conserva `restLocal` e `outputLocal`. Se a pose local de entrada ainda é o último output, recompõe o repouso de mundo usando o transform atual do pai, sem capturar novamente o resultado constrangido. Se animation/script publica uma pose local diferente, essa pose vira a base atual. Mover o pai altera o repouso recomposto, sem acumular peso entre frames. Peso zero restaura a base dos canais controlados. Desabilitar/remover deixa pose atual e elimina cache quando não há outra constraint ativa no objeto.
- Ordem entre modos no mesmo objeto: Position, Rotation, Scale, Aim. Aim ganha prioridade sobre Rotation nos canais que habilita e lê a posição depois de Position. Sem múltiplas instâncias de um mesmo modo.

## Ordem e diagnóstico

DFS determinístico por ObjectId resolve fontes e ancestrais constrangidos primeiro. Ciclos diretos, indiretos e dependências por hierarquia interrompem a cadeia afetada; demais objetos continuam. Uma fonte descendente do próprio objeto é recusada. Profundidade de dependência limitada a 256 protege a pilha; limite gera DependencyLimit. Diagnósticos públicos por frame: MissingSource, Cycle, DependencyLimit, Authority, DegenerateAim e InvalidPose; EditorPlayScene expõe `constraints().diagnostics()`; o Inspector apresenta os diagnósticos reais do pass.

Candidatos vivos são coletados apenas quando worldId/structuralRevision muda. Por frame são lidos IDs/propriedades/referências atuais dos candidatos, sem armazenar ponteiros entre frames. A ordenação é reavaliada entre candidatos para captar mudança de target sem depender de invalidation consumida pela física. Custo atual: O(C log C + caminhos de ancestrais) com maps/sets temporários proporcionais a C; não há varredura dos N objetos sem constraint em cada frame. Não foram medidos tempo ou alocações: pooling/ordenação cacheada por assinaturas continua oportunidade se C alto for cenário real.

## Editor e aceite

NÃO IREI SER SIMPLISTA NO DESIGN.

Add → Lógica → Restrições → escolher tipo → Fonte/Peso/Ativo em Influência → Offset/fatores/eixos/world-up em Ajustes. Inputs, object picker, reset e undo vêm das primitives existentes; quatro ícones próprios, quatro recipes de criação e quatro gizmos estão integrados. O gizmo mostra a ligação à fonte usando dados reais do grafo. Modelo/reflexão não exige setter especial. As capturas host são evidência da UI executável; imagem conceitual não é evidência de implementação, e nenhuma captura host prova funcionamento em aparelho.

Cenário de aceite: criar fonte e restrição → mudar fonte/offset/eixos/peso → Play → peso 0.5 estável em 10 frames → mover fonte → observar novo resultado → desabilitar/remover → salvar/reabrir → clonar grupo/prefab e conferir alvo interno remapeado → ciclo entre fontes diagnosticado → fonte destruída segura → corpo físico mantém autoridade. Aim deve apontar corretamente todos os seis eixos locais; testar coincidência/world-up paralelo.

Cinco testes nativos passaram: os quatro cenários de runtime (peso/cadeia/autoridade, ciclo/clone/serialização, aim/scale/degen e seis eixos de mira) e o cenário integrado Editor → recipe → undo → save → clone → gizmo → GameWorld. A regressão de pai móvel verifica pai X=4 → resultado de mundo X=7 e pai X=6 → resultado X=8, protegendo a mistura a partir do repouso local sem acumulação de peso.

As 19 fachadas C# do registro final foram regeneradas. O build C# terminou com zero erros e dois avisos CS8981 preexistentes; 49/49 testes Astra passaram, incluindo 2/2 Time. Separadamente, SaveStore passou 3/3: 52 testes distintos no total. Essas contagens abrangem o bloco integrado, não representam 19 novos tipos de constraints.

Capturas reais host inspecionadas em 853×394 e 1200×700 estão em `docs/validacao/evidencias/inspector-components-20260930/`, com sufixo `wave2`: `constraint-search-wave2.png`, `constraint-missing-wave2.png`, `constraint-influence-wave2.png` e `constraint-influence-wide-wave2.png`. A compilação Android final passou na validação central; APK e hash estão na auditoria consolidada. Nenhum ADB, instalação ou aceite visual em aparelho foi executado.
