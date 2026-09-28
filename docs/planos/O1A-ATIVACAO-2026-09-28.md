# O1a — estado ativo e ciclo de vida

Registro histórico da entrega de ativação. Depois dela, [tags foram entregues](O1A-TAGS-2026-09-28.md)
e a continuação foi ampliada para [O1 + O2 + O4 + P completo](BLOCO-OBJETOS-SCRIPTS-PREFABS-2026-09-28.md).
As pendências e contagens abaixo descrevem o momento desta entrega.

Entrega de 28/09/2026, na branch `codex/gameplay-runtime`. **A parte de ativação está implementada e validada. O1a continua parcial:** catálogo de tags, consultas por tag e habilitação uniforme dos componentes ainda não foram entregues. Nenhum tipo novo foi adicionado ao catálogo.

## Problema e implementação

O documento já guardava `SceneObject.active`, e o Inspector já alterava esse estado local. Faltava expor `GameObject.ActiveSelf` aos scripts. A investigação encontrou dois defeitos associados: a ponte descartava os scripts de objetos inicialmente inativos, e o gerenciador de callbacks consultava apenas `Behavior.Enabled`, sem considerar a hierarquia.

O caminho agora é `Inspector / SetActive → SceneObject.active → GameWorld → ScriptSceneAccess v14 → GameObject.ActiveSelf / ActiveInHierarchy → BehaviorWorld`. O arquivo de cena continua na versão 13: o dado já existia e não precisava de outra representação. A ABI de scripts ganhou um ponteiro no fim, com versão, tamanho e completude verificados nos dois lados; código aplicado de uma compilação anterior precisa ser recompilado pelo editor.

- Scripts de todos os objetos da cena entram na sessão, com seus campos e identidade. Um objeto inicialmente inativo aguarda a primeira ativação para receber `Awake`; `Enabled=false` sozinho não adia `Awake`.
- A execução exige instância sem falha, `Enabled=true`, handle vivo e objeto ativo na hierarquia. Isso vale para Update, FixedUpdate, LateUpdate, contatos, triggers, timers e eventos do aplicativo, por meio do mesmo ponto de despacho.
- Desativar um ancestral preserva o estado local do filho e o `Enabled` do comportamento. Reativar preserva a instância e seus campos; `Awake` e `Start` não se repetem.
- O estado é reavaliado depois de Awake, Enable e Start, porque esses callbacks podem desativar ou destruir seu próprio objeto. Falhas continuam isoladas por instância.
- `SetActive` com o valor já aplicado não altera revisões nem pede reconstrução dos consumidores.
- Leituras de estado por handle vencido falham; a ABI recusa IDs maiores que o identificador nativo, sem truncá-los para outro objeto.

## Referência concreta

Unity **6000.0**: [activeSelf](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject-activeSelf.html), [SetActive](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.SetActive.html), [Awake](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.Awake.html) e [GameObject.bindings.cs](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Scripting/GameObject.bindings.cs). O princípio adotado é separar a intenção local do estado efetivo herdado e preservar a instância durante a inatividade.

A adaptação mantém o contrato de despacho da Astra: Enable/Disable são reconciliados antes do próximo callback, sem chamar código do usuário de forma reentrante dentro de `SetActive`. Um desligar/religar inteiro entre dois despachos não produz um par de callbacks. Não se afirma equivalência de timing com Unity, nem suporte a coroutines.

## Validação

- Build nativo `aether_tests` e APK `:app:assembleDebug`: passaram.
- C#: `AstraBehaviorTests` **15/15** e `AstraWorldTests` **9/9**. Compilam scripts reais com o compilador do projeto; os acessos de cena destes testes são duplos.
- Nativo: filtros `runtime_world` **14/14**, `play_` **27/27**, `skinned` **1/1** e `script_abi_v14` **1/1**. Os filtros têm um teste em comum. Incluem arquivo de cena → reabertura → Play → ponte de scripts, recusa de handle vencido e regressão da ABI de animação.
- Aparelho **25053PC47G**, APK Debug atualizado, projeto isolado **O1Ativacao0928**: compilação/publicação dos scripts no próprio editor, Play, desativação pelo pai, reativação, desativação local do filho e retomada. Resultado: **Awake=1, Start=1, Enable=3, Disable=2**, com a mesma instância e nenhum Update no intervalo inativo.
- Stop manteve o pai inativo e o filho localmente ativo na autoria; Salvar, encerrar o aplicativo e reabrir preservaram esses valores. Uma segunda execução completou o mesmo cenário.

Fixture reproduzível: [activation-lifecycle](../../tests/fixtures/activation-lifecycle/README.md). Evidências em [capturas e log](../capturas/o1a-ativacao/). Esta entrega usa o controle de ativação já existente; não cria painel, controle ou conceito visual novo. A captura do Play vazio comprova o diagnóstico do script, não uma comparação visual de renderer/física.

## Continuação real

1. **Restante de O1a:** catálogo persistente de tags, atribuição no Inspector, `CompareTag` e buscas. Decidir a identidade e a política de remoção/renomeação antes de conectar o editor; conservar a separação entre dados do projeto e estado de Play.
2. **Habilitação por componente:** rastrear o consumidor de cada `enabled` e unificar a API apenas onde o runtime oferece esse comportamento. O item 121 permanece parcial.
3. **O1b:** acesso a comportamento por tipo em `GameObject`, criação/remoção de instâncias de script durante Play e destruição com atraso no ponto seguro. A correção atual cobre as instâncias carregadas no início da sessão; não implementa esse ciclo de vida dinâmico.
4. Depois, **O1c com P mínimo**, para clonagem/instanciação com referências remapeadas; então O2 e O4, conforme o inventário.

No inventário de 219 itens, apenas o **113** muda nesta entrega: **103 existentes, 9 parciais, 100 ausentes e 7 adaptações**.
