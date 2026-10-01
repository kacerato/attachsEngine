# F076 — curvas e gradientes com avaliação coerente

O campo autoral já tinha editor, bibliotecas, histórico e ponte para o Behavior. A lacuna desta revisão era o SDK usar tangentes antigas ao avaliar chaves editadas por script, além de alocar duas cópias ordenadas em cada amostra de gradiente. Esta revisão conecta os modos ao conteúdo atual e preserva a precisão do arquivo. O fechamento exige o aceite host/APK identificado no registro; o pacote físico Android continua separado.

## Dados, avaliação e limites

`ScriptCurve`/`AnimationCurve` usam até 256 chaves em tempos estritamente crescentes, valor, tangente de entrada/saída, modo de cada lado e Broken. Free alinhado usa a tangente de entrada nos dois lados; Broken permite tangentes livres diferentes. Linear deriva a inclinação do vizinho atual. Auto deriva a inclinação entre vizinhos, com extremos planos. ClampedAuto zera extremos locais e limita a inclinação pela menor secante adjacente. Constant segura o valor anterior até a chave seguinte. Auto Astra é recalculado após mudança; isto difere da opção Auto legada da Unity descrita no manual, que pode manter a tangente até reselecionar o modo. Não se promete igualdade bit a bit com o solver Unity.

Hermite opera com intermediários double. Clamp, Loop e PingPong usam o intervalo real entre as pontas; não transbordam porque a diferença de dois floats finitos excedeu float. Um resultado matemático fora da representação float é saturado ao limite finito, sem produzir NaN/Inf. Curva vazia vale zero; uma chave vale seu valor. Dados inválidos são recusados: `tryEvaluateScriptCurve` retorna false sem escrever o destino; a conveniência nativa devolve NaN, nunca um sucesso falso. O SDK lança erro de argumento para tempo não finito e erro de estado para dados/modos inválidos. Factories recusam intervalos degenerados antes de criar a curva.

Gradientes têm até oito paradas de cor e oito de alfa, independentes. RGB é linear, inclusive HDR até 65504; alfa está em [0,1]. Blend interpola RGB linear; PerceptualBlend usa Oklab, sem transformar alfa; Fixed mantém o contrato existente da primeira parada com tempo maior ou igual à amostra. A posição da amostra é clamp em [0,1]. Paradas fora do alcance e modos desconhecidos são erros. O SDK conserva arrays mutáveis e aceita sua ordem arbitrária, resolvendo os vizinhos sem ordenar/alocar; tempos iguais mantêm a ordem estável anterior. Arrays vazios no SDK conservam a regra anterior branco/alfa um; campos autorais exigem pelo menos uma parada de cada tipo.

Os dois avaliadores usam o mesmo algoritmo de tangentes e convenções. O arquivo textual conserva `max_digits10` de cada float: duas chaves com tempos adjacentes representáveis não colapsam após salvar/reabrir. O parser valida antes de publicar o valor. Os limites não são controles decorativos: arquivo, Inspector e SDK os verificam.

Curvas e gradientes são valores inteiros de um campo, reutilizáveis nas bibliotecas `.astra/libraries/curves.astra` e `gradients.astra`. Nenhum track ou override endereça uma chave interna; por isso este contrato não fabrica IDs de chave. Publicar o campo inteiro mantém identidade do campo/componente. Referências duráveis a pontos de Path e elementos Animation são outros contratos, com IDs reais.

## Autoria, runtime e edição

`EditorSession` abre a superfície contextual pelo campo real do ScriptBehavior. Curva permite adicionar/mover/remover chaves, tempo/valor, tangentes, modos e repetição; gradiente permite paradas de cor/alfa, posição, modo e picker de cor. O draft não escreve no documento. Aplicar publica pelo histórico existente em um passo; cancelar descarta o draft. Bibliotecas preservam os valores do projeto. `ScriptBridge::attachments` entrega as propriedades tipadas à instância compilada; `BehaviorWorld` faz o consumo. Arrays alterados pelo script passam a ser avaliados com seus valores atuais, sem usar estado da UI.

NÃO IREI SER SIMPLISTA NO DESIGN.

As capturas executáveis 853×394 mostram gráfico dominante, alças e chave selecionada na curva, faixa de cores com alfa e seleção no gradiente; Aplicar/Cancelar permanecem acessíveis. Não houve novo conceito visual ou ícone, nem alteração cosmética para este conserto de runtime. Capturas de layout usam drafts tipados do executável host; os cenários separados de EditorSession verificam interação, biblioteca, aplicação e histórico. Nenhuma captura host é frame Vulkan Android.

## Referências concretas

- [Unity 6000.0 AnimationCurve.Evaluate](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationCurve.Evaluate.html), [TangentMode](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.TangentMode.html) e [Editing Curves](https://docs.unity3d.com/6000.0/Documentation/Manual/EditingCurves.html): relação entre vizinhos, lados de tangente, interpolação e edição. A Astra adapta o workflow ao toque e declara sua diferença do Auto legado.
- [Unity 6000.0 Gradient](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Gradient.html): cor e alfa separados; o limite e os modos são contratos do valor, não flags de um painel.
- [Godot 4.5 Curve](https://docs.godotengine.org/en/4.5/classes/class_curve.html) e [fonte 4.5-stable curve.cpp](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/curve.cpp): dados autorais, sampling e cache derivados separados; validação explícita das entradas. A Astra mantém Hermite e seus modos próprios, sem importar o objeto Godot.
- A pesquisa de uso localizou [tutorial oficial Unity de curvas/eventos, 2014](https://www.youtube.com/watch?v=8VG2aK2AGSk) e [Unity Learn 2019.4](https://learn.unity.com/tutorial/working-with-animations-and-animation-curves?uv=2019.4). O provedor não abriu vídeo/transcrição nesta rodada; não se atribuem observações de frames ou cliques a esses vídeos. As decisões de workflow são sustentadas pelo manual acessível e pela inspeção da UI executável.

## Aceite direcionado

O alvo `aether_curve_gradient_family_tests` seleciona cinco cenários: dois contratos anteriores, um novo cenário de edição/precisão/extremos/erro e dois fluxos reais de EditorSession com biblioteca e aplicação em um Undo. O C# acrescenta dois cenários de amostragem mutável/zero alocação e reutiliza dois de compilação e consumo de campo pelo BehaviorWorld; o cenário de curva agora também altera o valor recebido e verifica o resultado seguinte. Logs, capturas e manifesto do pacote ficam em `docs/validacao/evidencias/families-curves-gradients-20261001/`. A suíte integral não é necessária para esse recorte.
