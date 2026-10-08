# Animator: recursos compartilhados e overrides por instância

Repositório `kacerato/attachsEngine`. **Bloco B1 fechado em 08/10/2026:** recurso compartilhado e overrides por instância, runtime, persistência, histórico, editor, APIs tipadas e aceite POCO F7. Evidências e limites em [REPORT](../../validacao/animator-controller-2026-10-08/REPORT.md). Não encerra todos os blocos U03/U06/U08.

## Problema e decisão

O grafo anterior pertencia a cada componente. Reutilizar a mesma lógica exigia duplicá-la e corrigir cada cópia. O recurso `.aeanimator` passa a possuir parâmetros, camadas, estados, movimentos, transições e eventos. O componente referencia seu GUID e conserva raiz animada, fonte física, velocidade global, relógio, ativação e máscaras locais. A tabela de overrides associa GUID de clipe original a GUID substituto; todo uso daquele original no grafo é substituído na instância.

O recurso nunca armazena IDs de objetos de cena. Sua revisão e o próximo ID do grafo são monotônicos, inclusive no Undo/Redo. Estados e parâmetros conservam IDs estáveis. Cada objeto mantém relógio, valores e transição próprios no runtime. Substituir um recurso por outro começa sua máquina e seus parâmetros; reatribuir o mesmo recurso conserva overrides. Desvincular resolve e incorpora o grafo efetivo e seus clipes locais, conservando a identidade do componente.

## Cadeia funcional

Editor / Inspector → arquivo + registro transacional → biblioteca de controllers → snapshot no início do Play → configuração resolvida e cacheada por componente → parâmetros / transições → amostras de clipes reais → pose. Arquivo ausente ou inválido interrompe a avaliação com diagnóstico; não toca o antigo grafo inline como fallback. Overrides que perderam sua origem numa nova revisão permanecem registrados e podem ser removidos explicitamente.

O Play usa snapshot em memória; recarregar arquivos é uma operação de autoria fora do Play. Não há leitura de disco por quadro. O cache é invalidado pela revisão, GUID, overrides, controles locais e máscaras. Alterações compartilhadas são visíveis a todos os consumidores na próxima execução. O estado de playback nunca é gravado no recurso.

## Workflow e design

NÃO IREI SER SIMPLISTA NO DESIGN.

A gaveta Recurso ocupa a mesma superfície contextual dos parâmetros e ajustes. Criar publica o grafo atual; Escolher atribui recurso existente; Editar recurso muda explicitamente a autoridade para a definição compartilhada. Na autoridade Instância, o grafo permanece legível e sua topologia protegida. A origem de cada clipe fica acima do campo substituto; reset local remove o override. Corpo e máscara continuam locais mesmo enquanto a definição compartilhada está aberta. Undo/Redo diferencia edição do recurso e edição do objeto; arrastar um estado compartilhado confirma uma única operação.

A alternativa estrutural adotada evita abrir outro dock permanente: recurso e instância compartilham o workspace com autoridade explícita. O navegador abre o grafo de um consumidor do recurso; um recurso ainda não atribuído abre como arquivo textual, sem fabricar objeto de cena. Para autoria visual de recurso não atribuído, escolha-o primeiro em um Animator real. Não há editor visual autônomo de recurso nesta entrega.

Criar mantém o arquivo como recurso disponível depois de Undo; Undo desfaz sua atribuição. Copiar recurso publica outro GUID. Edição compartilhada recusa arquivo externo divergente, exigindo recarregar. A publicação usa o journal existente para arquivo e registro; falha não deixa um registro apontando para conteúdo incompleto.

Dois ícones novos, controller e override, usam recorte angular branco/verde e entram no SVG, raster e atlas reais. A captura conceitual não é aceite: comparar captura prévia com workspace executável e conferir legibilidade, área do grafo, toque, rolagem, estados de autoridade e erro.

## Persistência, APIs e referências

Animator v3 acrescenta GUID de controller e pares de clipes. V1 e v2 permanecem legíveis e locais; suas origens físicas e defaults são preservados. O recurso usa `AEANIMATOR 1`, GUID interno, revisão e grafo portátil; tamanho, contagens, IDs, referência de cena e dados extras são validados. APKs anteriores não leem v3: preservar backup antes de salvar no Dev. Presets, clones e prefabs conservam GUIDs de recursos e remapeiam referências locais, incluindo máscaras. Reimportar fonte não reescreve o controller nem o override; clipe removido/sem duração é diagnosticado na avaliação.

O descriptor do componente expõe Controller como recurso tipado; o C# gerado usa `GetController` / `SetController`. Setter valida tipo no registro e disponibilidade no consumidor antes do commit. Limpar o controller incorpora a configuração resolvida. A API de parâmetros e estado usa a definição efetiva. Autoria dos pares de overrides é pelo workspace; não se anuncia uma API de edição aninhada completa do grafo.

## Aceite deste bloco

Poucos cenários focados: poses diferentes em dois mecanismos pelo mesmo grafo; parâmetros independentes; payload e recurso; missing/corrupt sem fallback; nova revisão e órfãos; prefab/máscaras; UI real de criar, escolher, compartilhar, Undo/Redo e recusar conflito; salvar/reabrir. SDK Release e Android build são gates distintos. No aparelho: criar/copiar/atribuir, substituir e resetar clipe, editar definição, Undo/Redo, Play, API de troca e persistência. Gravação curta da pose deve ser revista quadro a quadro, com PTS e hashes de todos os quadros, sem usar vídeo como prova apenas por existir.

## Referências estudadas

- [Unity 6000.0 AnimatorOverrideController](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimatorOverrideController.html): lógica compartilhada, tabela original/substituto e saída em tempo normalizado. Astra conserva o tempo normalizado existente e mantém pares na instância.
- [Unity 6000.0 ApplyOverrides](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimatorOverrideController.ApplyOverrides.html): alteração por conjunto de pares, evitando reconstrução de bindings por setter individual.
- [UnityCsReference, branch 6000.0, Inspector](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Inspector/AnimatorOverrideControllerInspector.cs): escolha da fonte, pares de clipes, busca, alteração compartilhada. Astra adapta a escolha à gaveta de toque; não copia o dock desktop.
- [Godot 4.5 AnimationTree](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_tree.html): clipes e árvore são responsabilidades distintas; recursos de árvore compartilhados exigem parâmetros de execução por instância.
- [Unity tutorial oficial: Animator Controller](https://www.youtube.com/watch?v=JeZkctmoBPw) localizado como referência de workflow; leitura do vídeo indisponível nesta sessão. Não se afirma análise dos seus quadros. O workflow foi decidido pela documentação e pelo código oficial, e será conferido no próprio APK.

## Pendências fora deste bloco

Submáquinas, interrupções e pose capturada, aditivas e pose de referência, retargeting, root motion com autoridade física, IK e contato de pés, edição de clipes e Timeline, tracks de outras propriedades, API aninhada de autoria e escala de muitas instâncias continuam blocos próprios. Limites atuais do grafo: 32 parâmetros, 4 camadas, 24 estados por camada, 48 transições por camada, 8 movimentos e 8 eventos por estado. Reutilizar o grafo não transforma esses limites em suporte ilimitado.
