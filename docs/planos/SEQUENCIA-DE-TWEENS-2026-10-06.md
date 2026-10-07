# Sequência de tweens (bloco D, F010)

Componente `astra.tween.sequence` v1 ("Sequência de tweens", família Lógica, subfamília Tempo). Encadeia Transform Tweens de outros objetos em até 8 etapas, sem script.

Referência: Godot 4.5 [Tween](https://docs.godotengine.org/en/4.5/classes/class_tween.html) (`chain`, `parallel`, `set_loops`). A Unity 6000.0 não tem sequência nativa; o modelo de etapas anexadas e paralelas é o mesmo.

## Contrato

| Propriedade | Efeito |
|---|---|
| `step_0`…`step_7` | Objeto com Transform Tween (o seletor filtra por esse tipo). Referência vazia é pulada |
| `join_1`…`join_7` | "Junto da anterior": a etapa começa com a anterior em vez de esperar o término |
| `interval_0`…`interval_7` | Espera antes de a etapa (ou o grupo que ela abre) começar; soma-se à espera do próprio tween |
| `loops` | Quantas vezes a lista inteira é percorrida (0 = infinito) |
| `autoplay`, `enabled`, `ignore_time_scale` | Início no Play, desligamento preservando dados, escala de tempo dos intervalos |

Métodos: `play` (recomeça da etapa 1), `cancel`, `pause`, `resume` e `step` (inteiro, etapa em andamento ou 0). Eventos: `step_started` (payload: número da etapa) e `completed` (uma vez, após as repetições finitas). Os quatro métodos sem retorno e os dois eventos entraram no catálogo da Conexão de evento (eventos 11–12, métodos 16–19). Em C#, a fachada gerada `TweenSequence` expõe `Play/Cancel/Pause/Resume/Step`, `OnStepStarted` e `OnCompleted`.

## Execução

```
SceneTweenSequences (runtime/scene_tween_sequences.h), antes de SceneTweens no quadro
  início: cancela os tweens de todas as etapas (a sequência é dona deles)
  grupo = etapa + seguintes "junto"; espera o intervalo → reinicia cada tween (SceneTweens::command 1)
  lê o término dos tweens do grupo → próximo grupo no quadro seguinte
```

Não existe segundo interpolador: pausa, retomada e cancelamento chegam aos mesmos estados do Transform Tween. Falhas são explícitas e param a sequência com o motivo no Inspector: objeto sem Transform Tween, tween com repetição infinita (nunca terminaria), tween cancelado fora da sequência, pose sob autoridade da física ou de outro escritor.

## Diferenças

| Aspecto | Astra | Classificação |
|---|---|---|
| Etapas | 8 etapas fixas apontando objetos com Transform Tween | Adaptação explícita (mesmo padrão de LOD Group e Receita de colisão) |
| Callback por etapa | Evento `step_started`, consumido por script ou Conexão de evento | Equivalente ao `tween_callback` encadeado |
| Etapa seguinte | Começa no quadro seguinte ao término da anterior | Adaptação explícita: o Godot reaproveita o tempo restante no mesmo quadro |
| NumberTween em etapa | Não: NumberTween é trilha criada por script, sem identidade autoral | Pendente; compor por script com `NumberTween.State` |
| Tween de propriedade arbitrária por etapa | Só pose (Transform Tween) | Pendente |

## Inspector

A captura inicial (`docs/validacao/evidencias/tween-sequence-20261006/`) mostrou o Inspector genérico agrupando por tipo de campo: "Etapa 2 junto da anterior" aparecia antes da Etapa 1, os intervalos iam para o fim, e o telefone paginava 9 vezes. A aba Etapas passou a usar uma linha por etapa, na ordem de execução: número (destacado quando paralela), "Primeira / Depois da anterior / Junto da anterior", objeto com seletor, Espera e Junto. No tablet as três etapas cabem numa página; no telefone, 4 páginas (uma por etapa) em vez de 9. A linha de estado mostra "N etapas · começa no Play" na edição e "Etapa em andamento · etapa N de M", "Concluída" ou o motivo da falha no Play. Ícone novo `component/tween-sequence` no atlas.

## Validação executada (06/10/2026)

- Host C++: `tween_sequence_*` 2/2: gravação v1 e recusa de intervalo negativo; ordem, espera, etapa paralela no mesmo quadro, duas passagens com `step_started` 1,2,3,1,2,3 e um único `completed`; falha explícita sem tween e com tween infinito; `play/pause/resume/cancel/step` pela mesma porta da ABI e recusa `NotRunning` sem o avaliador. Suíte 1414/1418; as 4 falhas são anteriores à branch.
- C#: suíte 521/521 com a fachada regenerada.
- UI executável: capturas 853×394 e 1200×700 (Etapas, Execução, Adicionar).
- Aparelho (APK instalado com `install -r`, projeto `Sequencia-20261006` gerado por `aether_ui_preview write-sequence-project`): sonda `SequenceProbe` registrou etapa 2 em 0,84 s depois da 1 em 0,09 s (0,5 s de tween + 0,3 s de espera), etapa 3 no mesmo instante da 2, duas passagens, porta relativa em 2,00 e luz/placa em 1,00, `completed ... PASS`, 0 erros Vulkan (`aparelho-logcat.txt`). Inspector do Play no aparelho com "Concluída" e as etapas (`aparelho-03-inspector-play.png`). Os objetos do teste não têm malha: a vista de jogo não mostra movimento.
