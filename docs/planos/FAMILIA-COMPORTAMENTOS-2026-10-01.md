# F007 — Comportamentos C#

Encerramento dos requisitos F007 do catálogo: fonte/tipo/instância, ativação, campos expostos, ordem, callbacks e erros. A revisão usa as evidências existentes autorizadas pelo usuário; não registra novos testes ou execução física.

## Cadeia conferida

`EditorCodeWorkspace` compila e publica o catálogo real da sessão. `scene::ScriptBehavior` conserva tipo, fonte, enabled e propriedades tipadas por identidade de membro; versões/tipos ausentes continuam como dados autorais. A criação pelo editor escolhe um tipo publicado e anexa uma instância real. `GameObject.AddBehavior<T>` cria o componente nativo e a instância gerenciada no dispatcher ativo; não publica um Behavior sem objeto/componente.

`ScriptBridge` transmite as instâncias ao `BehaviorWorld`; esse mundo possui assembly, estado, despachos e diagnóstico. Scripts autorais seguem a ordem de anexação dentro do objeto, conforme a composição persistida; criações em execução entram após o conjunto corrente. A implementação usa conjuntos de despacho limitados e aposentadoria fora do callback. Essa é a semântica de ordem Astra, sem prioridade numérica adicional por classe.

Awake, Enable, Start, fases de atualização, Disable, Stop, Destroy e eventos dos consumidores existentes têm lifecycle documentado. Falhas identificam objeto, instância e fase, isolam o receptor e não apagam os dados autorais. Remoção individual, objetos inativos, destruição pendente e novo Play não reutilizam instâncias/fila da sessão encerrada. As [regras de lifecycle e mensagens](O1B-SCRIPTS-MENSAGENS-2026-09-28.md) registram também limites e diferenças deliberadas de assinatura.

O Inspector lê e edita os membros publicados na instância C# viva durante Play. Snapshot tipado, nulo, ausência, erro de getter e valores fora do formato têm diagnósticos distintos; captura parcial nunca é publicada. Edição de Play não escreve a cena autoral. Fonte ausente ou tipo desconhecido continua identificado como ausente; não é execução bem-sucedida. [Contrato de inspeção](INSPECAO-SCRIPTS-PLAY-2026-09-28.md).

## Aceite e origem

Os pacotes [scripts dinâmicos/mensagens](../validacao/evidencias/o1bc-scripts-mensagens-20260928/README.md) e [inspeção viva](../validacao/evidencias/inspecao-scripts-play-20260928/README.md) contêm logs, capturas e cenários autorais, host e Android históricos. Cobrem fonte/compilação, anexação, campos, callbacks, reentrância, erro, remoção, instanciar/remapear e Stop/save/reopen. O [pacote ABI34](../validacao/evidencias/families-prefab-20261001/package-manifest.json) verifica o SDK/nativo atuais em Android; não substitui a execução histórica por uma execução nova.

Referências versionadas: [Unity 6000.0, ordem de execução](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html), [AddComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.AddComponent.html), [bindings oficiais 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Scripting/GameObject.bindings.cs), [Godot 4.5, depuração remota](https://docs.godotengine.org/en/4.5/tutorials/scripting/debug/overview_of_debugging_tools.html#remote-in-scene-dock) e [Inspector remoto, fonte 4.5](https://github.com/godotengine/godot/blob/4.5/editor/debugger/editor_debugger_inspector.cpp). Princípios adaptados: fonte e instância separadas, lifecycle por elegibilidade, erros por receptor e inspeção por identidade. Não se afirma compatibilidade binária, API MonoBehaviour completa ou despacho geral persistente F008.

Qualificação física da revisão ABI34 permanece separada. O build atual não foi instalado/executado para este encerramento.
