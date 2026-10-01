# Expansão da API de runtime — 30/09/2026

Este pacote expande capacidades da API C# sobre consumidores já existentes. **Zero tipos novos de componente**. Não declara suporte a áudio, partículas, UI de jogo, navegação, prefabs variants ou bake de lightmap. A referência é Unity **6000.0** e Godot **4.5-stable**, conforme os links abaixo.

## Caminhos reais

| Família | Cadeia implementada | Limite |
|---|---|---|
| Pose e espaço | GameObject → ISceneAccess → NativeBehaviorRuntime → ScriptBridge → GameWorld → SceneGraph | Operações de pose respeitam autoridade física e rejeição nativa; nenhum bypass |
| Matrizes affine | TRS local de cada ancestral → matriz completa → transformação/inversão | Preserva shear de escala não uniforme com rotação; inversão singular é recusada |
| Direções | Quaternions locais dos ancestrais → composição → direção | Não inclui escala ou translação; +Z é eixo do objeto, não promessa sobre eixo óptico da câmera |
| Consulta de composição | Fachada gerada → identidade de componente → composição e hierarquia nativas | Repetíveis permanecem instâncias separadas; componentes desabilitados são consultáveis |
| Câmera follow | Offset tipado → SetVector3 → ABI SetTriple → validação do candidato → publicação única | Elimina as três escritas parciais da fachada CameraFollowRig |
| Matemática | Delta explícito + velocidade do usuário → interpolação/amortecimento → resultado | Radianos; não cria Time global nem altera clock nativo |
| Aleatoriedade | Seed + stream → PCG32 → faixa/distribuição → snapshot versionado | Sequência Astra estável, não compatível com sequências Unity/Godot; não criptográfico |

### Pose e consultas

`LocalRotation`, `LocalScale`, `WorldPosition`, `WorldRotation`, matrizes `LocalToWorldMatrix` e `WorldToLocalMatrix`, conversão de ponto/vetor/direção e inversas, eixos `Right/Up/Forward`, `Translate`, `Rotate`, `RotateAround` e `LookAt` usam os objetos reais. A matriz inclui shear; o getter/setter nativo de **WorldTransform continua exigindo TRS representável**. Uma escrita world sob pais não uniformes pode, portanto, recusar corretamente a operação. Ler uma matriz completa não significa que o modelo agora armazene shear como autoria.

`LookAt` orienta +Z do objeto, recebe up como referência e recusa alvo coincidente/up collinear. Rotação/ângulos usam radianos de System.Numerics. Entradas inválidas são recusadas antes da escrita; RotateAround publica posição e rotação juntas. NativeWorld ainda pode recusar pelo ownership de física.

`GetComponents<T>`, sobrecarga com `List<T>`, `GetComponentsInChildren<T>`, sobrecarga com lista, `GetComponentsInParent<T>` e `TryGetComponent<T>` reutilizam `IComponentFacade<T>`. Children percorre profundidade primeiro, self antes dos filhos, ordem nativa preservada. Self é consultado mesmo inativo; descendentes inativos são excluídos por padrão. Slots de objetos já destruídos e ainda não drenados são ignorados pelo primitive existente `PushAliveChildren`. Lista fornecida é limpa antes de receber resultados.

Todas as fachadas geradas passam a expor `Object`, `IsAlive` e `Remove` pela mesma identidade nativa. `Remove` não promete sucesso quando o componente é dependência de outro sistema; a recusa existente é propagada.

**Custo:** matrizes e direções percorrem ancestrais e alocam um conjunto para detectar ciclos. Consultas de hierarquia usam stack e conjunto; as variantes List evitam o array de resultado, não prometem zero alocações. Em lotes de pontos, capture `LocalToWorldMatrix` uma vez e use `Vector3.Transform`. Não são API de hot path GPU nem cache de transforms; resultados acompanham estado real no momento da chamada.

### Matemática e streams

`Mathf` oferece clamp, lerp, inverse lerp, smoothstep, move towards, repeat, ping-pong, delta/lerp/move de ângulos, damp por meia-vida e SmoothDamp/SmoothDampAngle com velocidade por referência e delta explícito. DeltaAngle e o spring usam intermediários double para evitar overflow da diferença de floats finitos extremos. SmoothDamp com delta zero conserva posição/velocidade. Resultado de spring que não cabe em float é recusado antes de alterar a velocidade.

`RandomStream` é por dono, com seed de 64 bits e stream de 63 bits, estados capturáveis/restauráveis e algoritmo versão 1. Integer range é `[min,max)` com rejection sampling sem viés de módulo; float range é finito e também `[min,max)`. Sphere/circle amostram volume/área, e não raio uniforme. Normal usa Box-Muller; Fisher-Yates usa índices uniformes. Snapshot incompatível é recusado. A captura pode ser serializada pelo código de jogo, mas este pacote **não cria uma persistência automática de snapshots no documento da cena**.

## Uso em comportamento

```csharp
using Astra;
using Astra.Components;
using System.Numerics;

[ComponentId("examples.patrol")]
public sealed class Patrol : Behavior
{
    private readonly RandomStream random = new(42, 7);
    private float speed, velocity;
    public override void Start()
    {
        var colliders = Object.GetComponentsInChildren<Collider>(includeInactive: true);
        Scene.Log(ObjectId, $"Colisores reais: {colliders.Length}");
        speed = random.Range(1f, 3f);
    }
    public override void Update(float deltaTime)
    {
        speed = Mathf.SmoothDamp(speed, 2, ref velocity, .25f, deltaTime);
        Object.Translate(Vector3.UnitZ * speed * deltaTime, TransformSpace.Local);
    }
}
```

Use Translate somente com objetos cujo transform não seja comandado pela física. Em um corpo dinâmico, use as APIs existentes de força/velocidade, conforme seu ownership. Mudanças de pose feitas em Play continuam dados de runtime; este pacote não muda o contrato de descarte de autoria ao sair de Play.

## Referências concretas

- Unity 6000.0 [TransformPoint](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Transform.TransformPoint.html), [TransformDirection](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Transform.TransformDirection.html) e [LookAt](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Transform.LookAt.html): separar ponto, vetor e direção e documentar orientação de objeto.
- Unity 6000.0 [GetComponentsInChildren](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.GetComponentsInChildren.html) e [GetComponentsInParent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.GetComponentsInParent.html): self primeiro e inclusão opcional de inativos, preservando composição por instância.
- Unity 6000.0 [SmoothDamp](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Mathf.SmoothDamp.html): spring com velocidade persistida. O código oficial [Mathf.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Math/Mathf.cs) foi consultado como arquitetura; master é móvel, e a régua de contrato é a documentação versionada. Astra exige delta explícito e usa radianos.
- Godot 4.5 [RandomNumberGenerator](https://docs.godotengine.org/en/4.5/classes/class_randomnumbergenerator.html) e [random_pcg.h em 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/core/math/random_pcg.h): streams locais e estado reproduzível, em vez de estado global implícito.

## Validação e limites de evidência

Build gerenciado Release e quatro cenários dirigidos `AstraExpandedApiTests` passaram: affine com shear/inversão singular; escrita de pose única/recusa de ownership/LookAt; composição repetível e inatividade; PCG com vetor de referência/replay, distribuições limitadas, spring e floats extremos. Os testes de cena usam um duplo do contrato C#, portanto não constituem validação de ScriptBridge, consumo nativo nem aparelho. ADB não foi usado.

O build Debug inicial esbarrou em uma DLL de Aether.Rendering aberta por outro processo; Release evitou esse arquivo sem interromper processos do usuário. Regeneração da fachada é feita exclusivamente pelo gerador `aether_tests --write-component-api`, junto ao schema vigente, incluindo os campos lightmap do pacote gráfico.

## Segundo pacote: Awaitable do Behavior — API transversal §8

O §8 do plano vigente pede **Awaitable (NextFrame, Seconds, FixedUpdate) no lugar de corrotinas**. A entrega principal desta etapa é `await Awaitable.NextFrame()`, `await Awaitable.Seconds(...)`, `await Awaitable.SecondsRealtime(...)` e `await Awaitable.FixedUpdate()`, vinculados ao Behavior e à sessão. O scheduler também suporta IEnumerator como API adicional; isso não substitui a decisão do plano nem cria uma família nova de componentes.

### Fluxo executado

`NativeBehaviorRuntime.Update → BehaviorWorld.Update → CoroutineScheduler.BeginFrame → callbacks Update → scheduler.Tick → continuação async` usa o caminho já chamado pelo ScriptBridge no Android. `FixedUpdate` usa `BeginFixedStep → callbacks FixedUpdate → TickFixed`. O contador avança **antes** dos callbacks: um await iniciado dentro de FixedUpdate aguarda o próximo despacho físico, não o final do callback atual. Essa fase continua sendo a do callback nativo existente; não se adicionou uma nova fase depois da integração Jolt.

`Seconds` acumula o delta real recebido pelo callback Update, sem usar FixedUpdate duas vezes nem incrementar na passagem de LateUpdate. É tempo da simulação entregue pelo host; este pacote **não cria TimeScale nem presume que uma escala global já esteja exposta**. `SecondsRealtime` usa Stopwatch monotônico de verdade, separado do delta. Quando a sessão/app não despacha Update, o relógio realtime continua correndo, mas a continuação só é entregue quando o despacho volta. Nenhuma espera executa em background.

Todas as primitivas fornecidas retomam no thread que criou a sessão. Guards recusam agendamento pelo worker antes de consultar a cena ou mudar contadores. Await é de consumidor único; ao completar ou cancelar, a referência à continuação é removida. Token é inspecionado em cada pump, antes de deadline e tipo de espera, portanto cancelar uma espera longa não aguarda seu vencimento. Token cancelado na criação completa em estado cancelado, com o host/sessão/thread ainda verificados.

`StartAsync(Func<CancellationToken,Task>)` fornece token e observa a Task no mesmo scheduler. Erro vira BehaviorFailure com objeto, instância e fase; a irmã continua rodando. OperationCanceledException é cancelamento, não uma falha de script. Passe o token às primitivas. Parar o handle retorna controle cooperativamente no próximo pump do cancelamento por token; parar/destruir o objeto ou encerrar a sessão cancela diretamente os awaits e executa os finally antes de retirar as raízes de script.

**Fronteira:** a garantia de thread vale para Awaitable da Astra, não para `Task.Run`, `Task.Delay`, rede ou Tasks arbitrárias. Não existe SynchronizationContext que transplante qualquer tarefa para o thread de cena. Overrides `async void` não fazem parte desta capacidade: use override síncrono chamando `StartAsync`, com uma função `async Task`, para que erros possam ser observados. Trabalho externo deve respeitar o token e não acessar a cena no worker; o motor não pode preemptar nem descarregar uma Task externa que o jogo manteve em execução. Uma continuação estática observa faults de uma Task externa ainda pendente após cancelamento, sem capturar Behavior/contexto, mas não transforma isso em execução segura de gameplay após Stop.

### Lifecycle e IEnumerator adicional

Objeto inativo cancela os trabalhos do Behavior quando a transição é observada no despacho; reativar não reinicia enumeradores. Remoção é observada pelo sweep do runtime. Não se promete cancelamento síncrono no momento de toda escrita de estado nativo. Apenas desligar `Behavior.Enabled` mantém trabalhos já iniciados, seguindo o contrato de coroutine da Unity. Retire, falha no callback do Behavior e Dispose da sessão bloqueiam novos agendamentos antes de cancelar. Descarte de enumeradores também bloqueia reinícios vindos de finally. Os awaits são cancelados antes dos observadores de Task, permitindo que finally de async Task finalize no mesmo thread e que faults ainda sejam observados.

`StartCoroutine`, `StopCoroutine(handle)`, `StopCoroutine(root IEnumerator)` e `StopAllCoroutines` são adicionais. Start avança imediatamente até o primeiro yield. Yield null espera o próximo quadro; são aceitos IEnumerator aninhado, espera de tempo, predicado, handle de coroutine e passo físico. Instâncias de IEnumerator têm ownership exclusivo; compartilhar um enumerador, await de outra sessão ou ciclo de handles é recusado. Unsupported yield produz diagnóstico explícito. Stop durante MoveNext marca cancelamento e adia Dispose até o enumerador sair do seu passo, evitando reentrância em Dispose. Cada pilha é descartada de dentro para fora; exceção de Dispose não impede descartar os demais enumeradores.

Handles terminados carregam só status, IDs, string de erro e um token de escopo sem referência ao mundo. O scheduler remove listas e índices de enumeradores terminados; não mantém referência global a scripts nem ao AssemblyLoadContext.

### Uso autorizado pelo contrato

```csharp
using Astra;
using System.Threading;
using System.Threading.Tasks;

[ComponentId("examples.delayed-action")]
public sealed class DelayedAction : Behavior
{
    public override void Start() => StartAsync(Run);
    private async Task Run(CancellationToken cancellation)
    {
        try
        {
            await Awaitable.NextFrame(cancellation);
            await Awaitable.Seconds(.5, cancellation);
            await Awaitable.FixedUpdate(cancellation);
            Scene.Log(ObjectId, "Continuou no próximo passo físico da sessão");
        }
        finally { Scene.Log(ObjectId, "Fim ou cancelamento da ação"); }
    }
}
```

### Custo e evidência

Há alocação por operação/awaiter/pilha/estado async; não se declara zero GC. Pump normal percorre operações ativas, sem criar wrappers GameObject por trabalho/quadro. Esperar um handle consulta a lista de operações e detectar ciclos acrescenta buscas; isso não é um scheduler para dezenas de milhares de agentes. Limites explícitos: 4096 operações vivas por sessão (incluindo awaits e observadores), profundidade aninhada 64, reentrância Start 32, até 1024 passos imediatos por avanço. Um MoveNext ou função do usuário que bloqueie internamente ainda bloqueia o main thread.

Arquivos: `Behavior.cs`, `Coroutines.cs`, `BehaviorAwaitables.cs`, `Runtime/BehaviorWorld.cs`, `Runtime/CoroutineScheduler.cs`. Nenhum ABI, arquivo nativo ou controle de editor foi adicionado. `AstraAwaitableTests` contém três cenários integrados com o ProjectCompiler e o BehaviorWorld reais, incluindo thread, tempo da simulação, realtime monotônico, FixedUpdate iniciado dentro do callback, nesting, reentrância, dispose, erro isolado, token cancelado antes de deadline, objetos inativos, Stop e handles entre sessões. A cena é um duplo de contrato; isso não prova Android. A verificação centralizada confirmou **build C# com zero avisos/erros e 3/3 cenários host passando**. A publicação gerenciada Android `publishManagedCore` também passou com esses arquivos; isso é evidência de compilação/publicação, não de execução no aparelho nem de APK nativo completo. ADB permanece sem uso.

### Referências desta etapa

Consolidação do executor principal: filtro gerenciado **Astra 47/47**, sem falhas, incluindo os três cenários novos; `assembleDebug` Android passou em 37 s. APK de 242798906 bytes, SHA256 `D29B8F00C256D0E980AD1BCAC2C42D83421AAF1A6BACD9B94D1F0DBA72D15B42`. Este hash identifica a compilação integrada; nenhum aparelho foi usado para validar execução ou aparência.

- Unity 6000.0 [Awaitable](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Awaitable.html), [NextFrameAsync](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Awaitable.NextFrameAsync.html) e [FixedUpdateAsync](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Awaitable.FixedUpdateAsync.html): awaits ligados à fase do player loop e continuação síncrona de awaitables. Astra vincula cada espera ao Behavior em vez de depender de contexto global.
- Código oficial [Awaitable.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Scripting/Awaitable.cs) e [Coroutines.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Scripting/Coroutines.cs), master consultado como arquitetura móvel; a régua de contrato é Unity6000.0 versionada. A Astra usa ownership gerenciado por sessão e não handles nativos de coroutine.
- Unity 6000.0 [StartCoroutine](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.StartCoroutine.html), [WaitForSeconds](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/WaitForSeconds.html) e [WaitForSecondsRealtime](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/WaitForSecondsRealtime.html): avanço inicial, nesting, cancelamento por lifecycle e separação de relógios. Na Astra, duração é calculada ao agendar a espera, não depois de um marcador separado de fim do frame Unity; a retomada continua quantizada pelo próximo pump.
