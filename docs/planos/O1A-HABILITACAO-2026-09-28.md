# O1a — habilitação por componente

Entrega de 28/09/2026, item 121, etapa 1 do [bloco ampliado](BLOCO-OBJETOS-SCRIPTS-PREFABS-2026-09-28.md).
Uma capacidade concluída; nenhum tipo novo. O restante do bloco continua pendente.

## Contrato e referências

NÃO VOU IMPLEMENTAR DEPENDÊNCIAS DE FORMA CENOGRÁFICA.

O estado local é propriedade do componente nativo. Inspector, `Component.Enabled`,
fachadas geradas e `Behavior.Enabled` leem/escrevem esse mesmo estado. Inatividade
do objeto ou de um ancestral continua prevalecendo sobre a habilitação local.
Tipos sem essa capacidade recusam a propriedade; não recebem uma flag decorativa.

Referências oficiais, Unity **6000.0**:

- [Behaviour.enabled](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Behaviour-enabled.html)
  e [implementação nativa exposta ao C#](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Scripting/Behaviour.bindings.cs): estado compartilhado, não uma segunda variável independente no script.
- [Collider.enabled](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider-enabled.html)
  e [Rigidbody](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html): participação da forma na colisão é distinta da simulação do corpo.
- [LODGroup.enabled](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/LODGroup-enabled.html),
  [Animation](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Animation.html)
  e [uso de componentes](https://docs.unity3d.com/6000.0/Documentation/Manual/UsingComponents.html): controles de habilitação junto ao componente. A política de retomada da Astra está explícita abaixo; não se declara paridade total de animação.

## Cobertura real

Dos 15 tipos registrados, **12 suportam habilitação**; três possuem outra semântica.
As capacidades anteriores foram preservadas e três componentes ganharam estado persistente.

| Componente | Desligamento e retomada |
|---|---|
| ScriptBehavior | Suspende callbacks de execução; transições Enable/Disable no próximo despacho. Awake/Start não se repetem. Estado sincronizado entre C# e nativo. |
| CameraLook | Ignora entrada de olhar, preservando rotação e parâmetros. Reativar aceita novos deltas. |
| Animation | Suspende avanço do tempo e avaliação, preservando estados, mistura e última pose. Reativar retoma os estados. |
| LodGroup | Libera os renderizadores do controle do grupo e descarta o crossfade corrente. Reativar calcula o nível da vista atual. |
| Collider | Remove a forma das colisões/consultas. Com todas as formas desligadas, o corpo continua simulando. |
| Joint | Deixa de criar a restrição na reconstrução física; reativar a recria. |
| Timer | Suspende disparos. A transição desligado → ligado reinicia o intervalo, conforme contrato existente. |
| CameraFollow | Suspende acompanhamento; reativar volta a seguir o alvo válido. |
| Camera | Deixa de participar da seleção da câmera ativa. |
| MeshRenderer | Deixa de emitir o desenho, preservando recursos e parâmetros. |
| Light | Deixa de contribuir como luz ativa. |
| EnvironmentVolume | Deixa de participar da composição do ambiente. |

PhysicsBody e Character continuam usando suas propriedades físicas e atividade do
objeto; SkinnedMesh é consumido pelo MeshRenderer. `Component.Enabled` nesses três
tipos é recusado, assim como qualquer propriedade inexistente.

## Implementação e correções necessárias

`CameraLook` v2, `Animation` v4 e `LodGroup` v3 persistem a flag; versões anteriores
migram com `enabled=true`. O arquivo de cena permanece v14 e a ABI v15. Fachadas
C# e matriz de propriedades foram regeneradas dos descritores.

O mundo nativo permite editar `enabled` de ScriptBehavior pela propriedade tipada;
os demais campos continuam no fluxo validado de edição de comportamento. Escrever
o mesmo estado não invalida física. A exceção de um callback que destruiu seu objeto
não provoca uma segunda exceção ao tentar desligar um handle vencido.

Desligar o último colisor antes causava falha de reconstrução. Agora um corpo com
formas vinculadas, todas desligadas, usa a forma real `EmptyShape` do **Jolt 5.6.1**
vendorizado ([contrato oficial](https://jrouwe.github.io/JoltPhysics/class_empty_shape.html)).
Não há volume de colisão substituto. O Inspector identifica o estado como “Sem colisão”.
Um corpo sem nenhuma forma vinculada continua recebendo diagnóstico de configuração.

Também foi corrigida a entrada em Play: a física era montada antes de Awake/Start,
mas as invalidações desses callbacks eram descartadas. A fila e a reconstrução agora
são aplicadas antes do primeiro Update. O teste reproduziu a falha antes do patch.

**Limite físico:** a reconstrução conserva velocidades e pose, mas recalcula centro
de massa e inércia. Sem formas, usa a inércia padrão do EmptyShape escalada pela massa
autorada. Não se promete conservação do tensor anterior, nem continuidade de todos
os caches de contato/solver ao reconstruir.

## Inspector e validação

NÃO IREI SER SIMPLISTA NO DESIGN.

Os três componentes usam o controle já existente no cabeçalho, acessível mesmo
recolhidos. Não foi acrescentado painel permanente, conceito visual ou ícone novo.
As [capturas reais](../capturas/o1a-habilitacao/) mostram estados ligados, undo/redo,
Stop e o resultado de execução. Os controles permanecem legíveis em landscape
2772×1280, distinguem atividade do objeto e do componente e preservam o viewport.
Não houve imagem conceitual nem mudança estrutural de layout nesta entrega.

VOU TESTAR O QUE PROTEGE COMPORTAMENTO REAL.

- Build nativo e APK Debug passaram. **116 testes nativos distintos**: filtros
  enabled, runtime_, lod_, animation_, imported_rig, deformation_, play_, archive,
  component_api, component_contracts e component_matrix, com sobreposição.
- **27 testes C#**: 18 AstraBehaviorTests e 9 AstraWorldTests, incluindo compilação
  da fixture pelo compilador de projetos e sincronização do estado de comportamento.
- No host: pausa/retomada de animação com GLB real, visibilidade/fade de LOD,
  entrada de câmera, corpo Jolt sem forma, raycast, migrações e mudanças em Start.
- No Android 25053PC47G, projeto isolado **O1Enabled0928**: fixture
  [component-enabled](../../tests/fixtures/component-enabled/README.md), dois
  registros **ENABLED PASS**, antes e depois de encerrar/reabrir. C#/ABI, callbacks,
  atividade do objeto, colisor e propriedades geradas foram executados no aparelho.
- Pelo toque: ligar os três componentes, salvar, undo/redo de LOD, desligar todos,
  salvar/reabrir, executar e parar. Stop conservou os três desligados. Os arquivos
  salvos foram conferidos com os valores persistidos.
- Animação com malha e troca visual de LOD foram verificadas no host; a fixture
  Android confere suas flags, não demonstra visualmente esses dois sistemas.
  Portrait e medição de desempenho no aparelho não foram realizados.
- SHA-256 do APK: `32EEC26803D6E00F563D539BFEC65148A2D29C3C01B2C793382DEA1673FC764E`.

## Continuação

Inventário: **108 existentes, 8 parciais, 96 ausentes, 7 adaptações**, total 219.
Próxima etapa: scripts dinâmicos e destruição (125, 139 e 140), com registro por
tipo, adição/remoção durante Play, lifecycle e destruição atrasada em ponto seguro.
Descoberta/mensagens, clonagem, primitivas, prefabs completos e demais etapas
seguem a ordem do bloco ampliado; não são declarados entregues por este pacote.
