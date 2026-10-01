# P15a: API de curvas e seguidores — ABI 22

Referência funcional: [Godot 4.5 Curve3D](https://docs.godotengine.org/en/4.5/classes/class_curve3d.html) e [PathFollow3D](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html). A Astra usa pontos Bézier locais, amostragem por distância e progresso separado dos dados autorais. Não expõe tilt ou frames transportados sem consumidor.

## Caminho real

`Component.Curve()` cria acesso à instância `astra.path`. `CurvePath.Count`, `At(index)` e `ById(id)` leem a coleção real; `Insert`, `Set`, `Remove` e `Move` publicam um candidato validado no GameWorld. `CurvePoint3D.Id` é persistente, não um índice. Posição e handles In/Out são vetores locais; os handles são offsets relativos ao ponto. A API rejeita NaN/Inf, valores fora de ±100000, elemento ausente, capacidade de 128 pontos, mundo alheio e geração obsoleta. Reordenar conserva identidade; modificar os nove canais por `Set` é atômico. Remover todos os pontos é um rascunho válido; amostrar uma curva sem comprimento útil falha explicitamente.

`CurvePath.Sample(worldDistance, wrap)` retorna posição e tangente mundiais e comprimento mundial, consumindo o mesmo cache que o seguidor. Escala da hierarquia participa do comprimento. `Closed` é uma propriedade real de fechamento geométrico; wrap permite repetir a amostragem, inclusive numa curva aberta, onde a passagem do fim ao começo é um salto explícito. O recurso deste pacote é um valor Curve3D pertencente à cena; não existe arquivo de recurso compartilhado por GUID ou editor de extrusão entregue.

`Component.FollowPath()` aceita somente `astra.path.follow`. `PathFollower.Restart()` volta à distância inicial e toca; `Stop()` conserva progresso e interrompe o avanço; `Progress` lê a distância real, e `IsPlaying` lê o pedido de reprodução. Este último pode permanecer verdadeiro num extremo ou enquanto um diagnóstico impede movimento: não é uma medida de velocidade nem garantia de pose publicada. Os métodos funcionam antes do primeiro frame. A configuração de velocidade/duração, orientação e alvo permanece na fachada reflexiva gerada `Astra.Components.PathFollow`.

```csharp
var component = pathObject.GetComponent(ComponentIds.Path)
    ?? throw new InvalidOperationException("Objeto sem Path");
var curve = component.Curve();
var point = curve.At(0);
curve.Set(point.Id, point.Position, point.In, point.Out);
var sample = curve.Sample(2.0); // duas unidades no mundo

var followComponent = followerObject.GetComponent(ComponentIds.PathFollow)
    ?? throw new InvalidOperationException("Objeto sem PathFollow");
var follower = followComponent.FollowPath();
follower.Restart();
```

A fachada do Path chama-se `Astra.Components.PathComponent` para não introduzir ambiguidade com `System.IO.Path` nos scripts existentes. Ela mantém `TypeId = astra.path`; a nomenclatura C# não muda o arquivo da cena. As duas fachadas são geradas do registro único. A API dedicada por identidade evita guardar endereços por slot para coleções reordenáveis.

## ABI e ownership

`ScriptSceneAccess` e `NativeBehaviorRuntime.SceneAccess` acrescentam dois ponteiros no fim da ABI, versão **22**, com tamanho e disponibilidade estritos. Nenhum campo anterior foi deslocado.

`pathPointCommand`: operações 0 count, 1 leitura por índice, 2 leitura por ID, 3 insert, 4 edit por ID, 5 remove por ID, 6 move por ID ao índice final. Buffers de valores contêm exatamente nove floats: Position XYZ, In XYZ, Out XYZ; a identidade é saída u64 separada. Count pode devolver zero; falha devolve -1 com WorldStatus. Insert aceita índice igual ao tamanho para append e devolve o ID alocado. Os acessos gerenciados validam os tamanhos antes de entrar na ABI.

`pathRuntimeCommand`: 0 sample (seis floats e comprimento double), 1 restart, 2 stop, 3 progress e 4 playing. Todas as chamadas validam mundo, geração e instância de componente, inclusive queries. Falha devolve zero com WorldStatus; o consumidor ScenePaths indisponível devolve NotRunning. Sample/length recompõem apenas o cache afetado; não há fallback para uma curva antiga após erro. Mutação é na thread do mundo, como a coleção de clips existente. Remoção estrutural continua respeitando o ponto seguro.

O documento autoral não recebe mutações de Play. O editor permite editar pontos em Edit com Undo/Redo e protege resultados de teclado com objeto, instância, revisão e ID do ponto. A UI de pontos fica bloqueada em Play; a API de gameplay mantém mutação explícita do mundo de execução. Cache, distância e pedido de reprodução não são serializados na cena.

## Verificação

O cenário `path_script_abi_points_identity_sampling_runtime_control_and_stale_handles` usa o GameWorld, EditorPlayScene e ScenePaths reais, capturando a ABI por um serviço CLR de teste. Ele verifica mutação atômica, reorder/ID, comprimento/amostragem, avanço da pose, Stop, foreign world e destruição. Isso não prova a execução física de um jogo C# Android. Os resultados executados, capturas e APK são consolidados na [auditoria da rodada](EXECUCAO-AUDITORIA-UNITY-ASTRA-2026-09-30.md).
