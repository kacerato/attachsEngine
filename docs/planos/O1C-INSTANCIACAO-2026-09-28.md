# O1c — Instanciação de hierarquias

28/09/2026. Etapa 4 do bloco ampliado, item 138. Implementada e validada no
runtime; não equivale ao recurso prefab, que permanece nas etapas 6–9.

## Contrato

`source.Instantiate(parent)` cria novas identidades para a subárvore viva,
incluindo filhos inativos. Conserva transformação local, atividade e componentes;
o pai é explícito. Raiz sintética, objetos vencidos e outro mundo são recusados.
Objetos já destruídos, ainda aguardando remoção física, não ressuscitam na cópia.

Referências internas de objetos e componentes apontam para a cópia; externas
continuam apontando para os mesmos destinos. Recursos permanecem compartilhados.
A identidade local de componente é conservada: o par objeto/instância é novo.
O mesmo remapeamento nativo é usado pela duplicação do editor.

Scripts copiam os valores atuais dos membros publicados pelo schema, inclusive
campos privados herdados com SerializeField/PropertyId. Listas e curvas recebem
novos valores independentes. Caches privados sem PropertyId, estáticos e estado
de lifecycle não são clonados. A resolução de membros agora percorre as classes
bases pelo ID estável, também na carga e na edição de campos.

Toda a estrutura e os campos são preparados antes de Awake/Enable. Construtores
e captura ocorrem antes da criação nativa; falha ao aplicar campos remove a
subárvore inteira antes de publicar comportamentos. Falhas de callbacks seguem
o isolamento existente do BehaviorWorld. Awake pode destruir a própria cópia;
nesse caso Instantiate retorna um GameObject vencido, verificável por IsAlive.

A ponte ABI 17 separa preparação/publicação e permite rollback imediato.
Mantém os limites existentes de objetos e 4096 comportamentos; recursão de
despacho é limitada a 32. IDs consumidos não são reutilizados.

## Referências observadas

- [Unity 6000.0, Object.Instantiate](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Object.Instantiate.html):
  hierarquia, componentes, atividade e callbacks. A Astra exige pai explícito e
  preserva a pose local; não oferece nesta entrega todos os overloads da Unity.
- [Unity 6000.0, serialização](https://docs.unity3d.com/6000.0/Documentation/Manual/script-serialization.html):
  distinção entre valores serializados e referências a objetos. A Astra usa seu
  schema tipado com PropertyId e mapa completo de identidades antes de ativar.
- [UnityCsReference, branch 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Scripting/UnityEngineObject.bindings.cs):
  entrada gerenciada encaminha a clonagem ao runtime nativo. Esse arquivo não
  contém a implementação C++ interna da Unity. Na Astra o grafo pertence ao
  GameWorld e as instâncias C# ao BehaviorWorld da mesma sessão.

## Validação e limites

Build nativo, C# e APK debug passaram. 90 execuções de testes nativos nos filtros
runtime_, duplicate, composition_archive, play_ e script_abi passaram; 20 testes
de comportamentos C# passaram. O cenário usa o compilador real do projeto e
inclui referências, campo herdado, valores mutáveis, falha de preparação,
destruição dentro de Awake e independência da fonte.

No aparelho 25053PC47G, landscape 2772×1280, projeto isolado BlockObjects0928:
PASS às 15:22:43 e 15:26:14, antes/depois de Stop → salvar → encerrar → reabrir.
A hierarquia mostra Copy/Child após a fonte ser destruída. A cena autoral
reaberta contém somente Driver além da raiz.

[Evidências](../validacao/evidencias/o1c-instanciacao-20260928/README.md).
O cenário não possui malha: valida grafo e scripts, não rendering ou colisão de
primitivas. Não houve benchmark de escala nem validação em portrait.

**Correção posterior no mesmo dia:** a lacuna de leitura dos campos C# no
Inspector foi tratada em [inspeção em Play](INSPECAO-SCRIPTS-PLAY-2026-09-28.md).
As evidências acima continuam correspondendo ao APK original da clonagem.

Nenhum tipo de componente novo foi contado. Inventário: 116 existentes,
6 parciais, 90 ausentes e 7 adaptações, total 219. Etapas 5–12 continuam abertas.
