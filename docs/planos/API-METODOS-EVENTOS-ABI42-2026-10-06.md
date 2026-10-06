# Bloco A — métodos, eventos e famílias da ABI42

Branch `claude/api-componentes`. Primeiro bloco do [roadmap de API e componentes](ROADMAP-API-COMPONENTES-2026-10-06.md).

## O que passou a ser possível

- Um tipo de componente declara **métodos** (argumentos e retorno tipados) e **eventos** (payload tipado) no próprio `ComponentType`. O runtime executa os métodos com a função real, enfileira os eventos e recusa o que não foi declarado.
- Scripts C# chamam esses métodos pela fachada gerada (`timer.Start(0.5)`, `audio.Seek(2)`, `follower.Progress()`) e assinam eventos (`timer.OnElapsed(this, e => ...)`, `collider.OnTriggerEnter(this, e => ...)`).
- A ABI de scripts deixa de crescer por versão: o núcleo `ScriptSceneAccess` fica congelado na **v42**, cujo último campo é o resolvedor `extension`. Funções novas entram como **famílias nomeadas**, com versão e tamanho próprios. Um host sem uma família recusa só as operações dela.

## Cadeia

```text
scene::ComponentMethod / ComponentEvent (descritor no ComponentType)
  → runtime::invokeComponentMethod (valida mundo, handle, tipo, método e argumentos)
  → função ligada em runtime/component_operations.cpp (SceneTimers, SceneTweens, SceneAudio, ScenePaths)
  → resultado conferido contra o tipo de retorno declarado

emissores reais (timer, tween concluído, contato Jolt, contato Box2D)
  → runtime::ComponentEventQueue (cursor por consumidor, capacidade 4096, perda informada)
  → família astra.component.operations: pollEvents / eventName / declaresEvent
  → BehaviorWorld: drena no início de FixedUpdate, Update e LateUpdate → assinaturas do Behavior
```

`auditComponentOperations()` exige que todo método declarado tenha exatamente uma função e que toda função corresponda a um método declarado. O gerador C# (`componentCSharpApi`) e a matriz (`componentMatrixMarkdown`) leem os mesmos descritores.

## Catálogo desta entrega

| Tipo | Métodos | Eventos |
|---|---|---|
| `astra.time.timer` | `start(interval)`, `stop`, `pause`, `resume`, `remaining → número`, `running → booleano` | `elapsed(count: inteiro)` |
| `astra.tween.transform` | `restart`, `cancel`, `pause`, `resume`, `elapsed → número` | `completed` |
| `astra.audio.source` | `play`, `pause`, `resume`, `stop`, `seek(seconds)` | — |
| `astra.path.follow` | `restart`, `stop`, `progress → número`, `playing → booleano` | — |
| `astra.physics.collider` | — | `trigger_enter/exit(other)`, `collision_enter/exit(other)` |
| `astra.physics2d.collider` | — | `trigger_enter/exit(other)`, `collision_enter/exit(other)` |

Os métodos reutilizam os serviços já chamados pelos callbacks específicos (`timerCommand`, `tweenCommand`, `audioCommand`, `pathRuntimeCommand`); as duas portas produzem o mesmo efeito. Esses callbacks continuam no núcleo por compatibilidade.

## Diferenças em relação à referência

| Referência | Astra | Classificação |
|---|---|---|
| Godot 4.5 [ClassDB](https://github.com/godotengine/godot/blob/4.5/core/object/class_db.h): métodos e sinais registrados junto das propriedades | Descritores estáticos no `ComponentType`, função efetiva no runtime e auditoria cruzada | Adaptação explícita |
| Godot 4.5 [signals](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html): `connect` desfaz a ligação quando o alvo é liberado | Assinatura pertence ao Behavior; termina com `Dispose`, retirada do dono ou Stop | Equivalente |
| Unity 6000.0 [UnityEvent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html): invocação síncrona no emissor | Eventos entregues no próximo despacho de script (FixedUpdate, Update ou LateUpdate), em ordem de emissão | Adaptação explícita: o emissor nativo não executa C# no meio do passo físico |
| Unity `OnTriggerStay` / `OnCollisionStay` | Stay continua só nos callbacks de Behavior; não vira evento de fila | Adaptação explícita: um evento por par e passo encheria a fila sem informação nova |
| Contato 3D por forma | Instância zero: o Jolt informa o objeto, não qual colisor repetível tocou. O Box2D informa o colisor | Pendente no 3D |

## Limites

- Métodos existem só no mundo de Play; fora dele respondem `NotRunning`.
- Tipos de valor: booleano, inteiro, número, vetor e objeto. String, recurso e cor ficam para quando um método real exigir.
- Payload até três valores por evento; fila de 4096 eventos por sessão. Estouro descarta os mais antigos e informa `Lost` ao consumidor, nunca em silêncio.
- AudioSource não emite `finished`: o `SceneAudio` não expõe o fim da voz como acontecimento. Fica pendente até existir esse produtor.
- As conexões autoradas sem script (bloco B) vão consumir a mesma fila pelo cursor `Connections`, que já existe e é testado.

## Mudança de ABI e compatibilidade

- `ScriptSceneAccess.version` 41 → **42**, acrescentando `extension` no fim. Runtime gerenciado e nativo precisam ser publicados juntos, como em toda versão anterior. A partir daqui, uma função nova **não** sobe essa versão: ela entra como família em `native/scene/script_extensions.h`.
- `WorldStatus.UnknownOperation` acrescentado no fim dos dois enums.
- Quatro testes gerenciados afirmavam que o campo da própria família era o último do núcleo. Essa premissa deixou de valer por contrato: os testes continuam conferindo a posição exata relativa aos vizinhos, e o tamanho total do núcleo passa a ser conferido em um único lugar (`ComponentOperationsTests`).

## Uso

```csharp
using Astra;
using Astra.Components;

[ComponentId("exemplo.porta")]
public sealed class Porta : Behavior
{
    public override void Start()
    {
        var sensor = Object.GetComponent<Collider>().Value;
        var som = Object.GetComponent<AudioSource>().Value;
        sensor.OnTriggerEnter(this, e =>
        {
            Scene.Log(ObjectId, $"entrou {e.GetObject(0).ObjectId}");
            som.Play();
        });
    }
}
```

## Validação executada (06/10/2026)

- Host C++ (MinGW 16.1, Debug): `aether_tests` completo **1397/1407** antes da correção abaixo; as 10 falhas eram idênticas às da base `d12c2ea2` compilada no worktree `atchengine-base` (1388/1398). Depois de trocar versões fixas da ABI (38, 24) pela constante do núcleo em cinco testes antigos, esses cinco passaram; restam 5 falhas, todas presentes na base e fora deste escopo.
- Testes novos: `component_operations_*` 3/3 e `component_event_queue_*` 1/1 (auditoria, ABI42 com Timer real, tween e contato Jolt reais, estouro/perda da fila).
- C# (`Aether.Tests`, Release): **515/516**; `ComponentOperationsTests` 4/4. A falha restante (`PhysicsQueries_TypedHits…`) também falha na base, que tinha 4 falhas: as outras três eram as asserções de tamanho do núcleo tratadas acima.
- Android: `libaether_android.so` arm64 compilado com clang do NDK 27.1 numa pasta de build própria, sem avisos. APK não gerado, nada instalado; sem ADB nesta rodada.
- `aether_tests` passou a usar `-Wa,-mbig-obj` no MinGW: a própria base falhava com "file too big" em `test_bulk50.cpp`, sem relação com este bloco.
