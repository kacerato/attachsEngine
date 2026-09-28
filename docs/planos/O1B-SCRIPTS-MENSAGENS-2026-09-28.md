# O1 — Scripts dinâmicos, destruição, descoberta e mensagens

28/09/2026 · etapas **2 e 3** do [bloco ampliado](BLOCO-OBJETOS-SCRIPTS-PREFABS-2026-09-28.md).

## Capacidade e arquitetura

A criação de scripts passa pelo catálogo compilado da sessão, constrói a
instância C#, cria o componente nativo com identidade própria e o registra no
mesmo dispatcher dos scripts autorais. O objeto real, seu componente, o
lifecycle e o Inspector compartilham a identidade. Não há um registro paralelo
de scripts sem componente no mundo. A ABI nativa/gerenciada passa de 15 para
**16**; carregamento exige versões correspondentes.

`GameObject.AddBehavior<T>()` aceita tipos publicados no assembly ativo e
preserva os valores de construção, inclusive Enabled. Awake/Enable ocorrem
quando o objeto está ativo; Start antecede o primeiro despacho de execução.
Criar dentro de Awake ou outro callback não invalida a iteração em curso.
`GetBehavior<T>()` e `GetBehaviors<T>()` consultam por classe ou interface no
objeto. `Behavior.Remove()` remove somente aquela instância; referências
retidas passam a reportar `IsAlive == false`. Disable, Stop e Destroy seguem o
estado que a instância alcançou; Destroy é entregue às instâncias que receberam
Awake. A compactação do dispatcher acontece fora dos callbacks.

`GameObject.Destroy(segundos)` usa tempo de simulação do mundo: Pause congela
o prazo, Step avança 1/60 s. Zero usa a destruição existente; negativos, NaN e
infinito são recusados. Repetição conserva o prazo mais cedo. A fila pertence
ao mundo, não ao comportamento solicitante, e é descartada ao encerrar Play.
O vencimento invalida handles e agenda a remoção física pelo ponto seguro
existente. Teto de 4.096 pedidos pendentes; fila cheia produz erro explícito.

O Inspector usa a hierarquia de execução em Play/Inspect. A validação no
aparelho encontrou uma seleção apagada a cada quadro por consultar a cena
autoral; a correção agora valida contra o documento que está sendo mostrado.
Stop volta à autoria e descarta seleções exclusivamente runtime.

A repetição no aparelho também reproduziu uma busca que falhava no intervalo
entre invalidar um objeto destruído e retirar seu slot da hierarquia. Busca
global e mensagens descendentes agora ignoram slots mortos antes do flush;
o teste gerenciado mantém esse estado intermediário para proteger a regressão.

## Contratos das consultas e mensagens

- `FindInWorld(nome)` busca nome exato entre objetos ativos, em pré-ordem da
  hierarquia; o primeiro duplicado vence. A raiz sintética não participa.
  Não interpreta caminhos. `Find(nome)` conserva a busca em subárvore existente.
- `SendMessage`, `BroadcastMessage` e `SendMessageUpwards` operam no objeto,
  nele e seus descendentes, ou nele e seus ancestrais, respectivamente.
  Descendentes seguem pré-ordem; ancestrais começam no alvo. Scripts de cada
  objeto seguem a ordem de anexação.
- Receptores são métodos de instância `void`, públicos ou privados, não
  genéricos, com zero argumento ou um payload compatível. Não há conversão
  numérica implícita, `ref`, `out`, múltiplos argumentos ou despacho assíncrono.
  A classe derivada tem precedência; sobrecargas compatíveis ambíguas são erro.
- Objetos inativos são excluídos. Um Behavior desabilitado ainda recebe uma
  mensagem explícita; uma instância desativada por falha não recebe mais.
- A lista de receptores é capturada antes da primeira chamada. Removidos antes
  de sua vez são ignorados; novos receptores não entram na mensagem corrente.
  Exceções do receptor são registradas com objeto, instância e fase e não
  interrompem os demais receptores. O retorno conta chamadas iniciadas.
- Ausência lança `MissingMethodException`, salvo `DontRequireReceiver`.
  Recursão de mensagens/criação é limitada a 32 níveis. Cache de métodos é
  limitado a 4.096 entradas e liberado com o assembly da sessão.

```csharp
var alvo = Object.FindInWorld("Alvo");
if (alvo is not null)
{
    var movimento = alvo.AddBehavior<Movimento>(); // tipo publicado do projeto
    var contrato = alvo.GetBehavior<IMovimento>();
    alvo.BroadcastMessage("Impulso", 3.0f, MessageOptions.DontRequireReceiver);
    movimento.Remove();
    alvo.Destroy(2.0);
}
```

## Referências efetivamente estudadas — Unity 6000.0

- [AddComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.AddComponent.html)
  e [GetComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.GetComponent.html):
  anexação e descoberta por tipo. A Astra separa fachadas nativas de Behavior
  gerenciado, mantendo o componente real por trás dos dois caminhos.
- [Execution Order](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html):
  construção/anexação, Awake, ativação e Start têm fases distintas. A Astra
  fixa o conjunto de despacho para suportar criação/remoção reentrantes.
- [Destroy](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Object.Destroy.html):
  remoção individual e agendamento pertencem ao runtime. A adaptação explicita
  relógio, prazo mais cedo, descarte de sessão e ponto seguro da Astra.
- [SendMessage](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.SendMessage.html),
  [BroadcastMessage](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.BroadcastMessage.html)
  e [SendMessageUpwards](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.SendMessageUpwards.html):
  mensagem local ou encaminhada pela hierarquia, payload e receptor opcional.
  A Astra acrescenta retorno de contagem e rejeita resolução ambígua.
- [Find](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.Find.html):
  busca global considera atividade. A Astra usa nome próprio `FindInWorld`
  para preservar a API preexistente e documenta ordem de duplicados.
- [GameObject.bindings.cs, branch 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Scripting/GameObject.bindings.cs):
  API gerenciada encaminha identidade/tipo ao mundo nativo. Na Astra a ponte
  ABI 16 encaminha a criação nativa, enquanto o assembly e seus métodos ficam
  sob ownership do BehaviorWorld da sessão.

## Persistência, limites e escopo real

Metadados dos scripts usam o serializer existente (tipo, arquivo, instância,
Enabled e propriedades), sem novo formato de cena. Criações e remoções de
Play são transitórias; salvar a cena autoral não as incorpora. Filas de atraso
e estado estático não são serializados nem sobrevivem ao novo Play.

O Inspector permite editar os campos serializados pelo caminho existente.
Ele **não faz leitura automática de todos os campos C# modificados em runtime**:
esse limite preexistente permanece; não confundir valores de autoria exibidos
com um debugger de memória gerenciada. Não foi implementada captura de estado
runtime para transformar objetos em prefabs; isso depende das próximas etapas.

Este pacote não acrescenta tipos de componentes ao atlas. Entrega sete itens
do inventário (125, 132–135, 139, 140) e amplia a criação do item 119.
Clonagem, primitivas, prefabs e Static continuam pendentes. Mensagens por
reflexão são uma ferramenta de comunicação explícita, sem alegação de custo
adequado para hot paths. Não houve benchmark nem aceite em portrait.

## Validação

- Build C++ `aether_tests` e APK Android debug concluídos.
- **100 testes nativos distintos aprovados** nos filtros `dynamic`, `runtime_`,
  `play_`, `script_abi`, `archive`, `component_contracts` e `inspector_in_play`.
  Incluem identidade, serializer, pausa/Step, descarte de sessão e edição de
  Play. A regressão de seleção foi reproduzida antes do patch e passou depois.
- **28 testes C# aprovados:** 19 de Behavior e 9 de World. O cenário compila
  scripts reais com o compilador do projeto e verifica uma única falha
  deliberada de receptor, sem interromper os demais.
- Aparelho **25053PC47G**, landscape 2772×1280, projeto isolado
  `O1Dynamic0928`: duas execuções completas no APK final, antes e após
  Stop → salvar → encerrar aplicativo → reabrir. Logs READY/PASS às 14:41:59
  e 14:43:35. Nested conserva seleção e mostra os dois scripts criados em
  Awake; desligar um preserva o outro. Stop descarta os objetos runtime e
  a cena salva continua contendo somente Driver além da raiz.
- SHA-256 do APK instalado:
  `B9A612DDA019E619D1911D3CF30F3AE0DEC622ADA16667793B317C6AD6994C4E`.

[Capturas e logs](../validacao/evidencias/o1bc-scripts-mensagens-20260928/README.md)
são do editor executável, sem imagens conceituais. O viewport vazio é
intencional: o cenário testa scripts e hierarquia, sem geometria.

Próxima etapa: clonagem de hierarquias com remapeamento de referências, seguida
pelas primitivas. O bloco ampliado permanece em execução; etapas 4–12 pendentes.
