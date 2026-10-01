# Interpolação numérica de propriedades — ABI30

Referências: [Godot 4.5 PropertyTweener](https://docs.godotengine.org/en/4.5/classes/class_propertytweener.html), [Tween 4.5](https://docs.godotengine.org/en/4.5/classes/class_tween.html) e [implementação oficial 4.5](https://raw.githubusercontent.com/godotengine/godot/4.5/scene/animation/tween.cpp). O princípio extraído é a interpolação de propriedades identificáveis com ownership e lifecycle de mundo. Astra captura o valor inicial na criação, como `from_current`; rejeita dois escritores de tween no mesmo campo, enquanto Godot documenta prioridade do último tween. Não representa paridade com sequência, paralelo, Callable ou todos os tipos Variant.

## Cadeia funcional

`Component.TweenFloat(PropertyId, destino, duração, easing, ignoreTimeScale)` → ABI30 → ScriptBridge → SceneNumberTweens → validação de ComponentHandle e contrato numérico → GameWorld.setTweenNumber → componente existente e invalidação existente → extração de luz / resolução de câmera / Inspector do Play.

Seis campos elegíveis: Light `intensity`, `color.r`, `color.g`, `color.b`; Camera `vertical_fov` quando perspectiva; `orthographic_half_height` quando ortográfica. O opt-in está na reflexão e na matriz de propriedades. Campos estruturais, recursos, física e propriedades não qualificadas são recusados; não existe flag de suporte universal. A intensidade conserva a unidade autoral e passa pela conversão física já usada pelo renderer.

O serviço existe antes de Start e avança no frame após os tweens de transformação, antes das constraints. Relógio escalado ou não escalado, passos aceitos até 0.25 s, duração 0.001–36000 s, easing linear/smoothstep/quadrático in/out. Inatividade na hierarquia, componente desabilitado e pausa local congelam o avanço. Retomar não recupera uma execução terminal. Conclusão escreve exatamente o destino; cancelamento conserva o último valor escrito.

O token inclui o mundo e um contador monotônico. Remoção do alvo ou mudança que retire a elegibilidade produz Failed com WorldStatus. Outro escritor detectado antes da próxima escrita produz PropertyWrittenExternally e conserva seu valor. Snapshot expõe estado, falha, tempo, último valor escrito, duração, pausa/atividade/habilitação. O valor do snapshot não se apresenta como leitura corrente do componente após uma falha.

Limite de 256 tracks inclui estados terminais retidos. Dispose/Release libera armazenamento; Stop limpa tudo. O wrapper C# exige a thread da cena e descarta token de sessão encerrada sem despachar para novo mundo. A escrita por frame usa a instância real sem clone de componente a cada tick; nenhuma propriedade acoplada ou que exige reconstrução pesada foi qualificada. Medições de host não são desempenho Android.

## Autoria e persistência

Esta API cria execução temporária por script, não um novo componente de autoria. Os scripts e os valores iniciais usam ScriptBehavior/arquivo de cena existentes; os tracks não são serializados. Encerrar Play descarta os valores interpolados junto com o mundo de execução e conserva o documento autoral. O Inspector existente mostra os valores do componente real no Play. Não há novo botão decorativo, menu de tipos ou ícone independente neste pacote.

## Aceite e estado

Projeto independente NumberTweens-20261001: Light e NumberTweenProbe persistentes, TimeProbe existente; pausa inicial → retomada em tempo não escalado → intensidade chega a 8 → segundo track avança → Cancel conserva o valor → Dispose libera snapshot. O script registra NUMBER TWEEN PASS somente depois da sequência real. [Resultados e limites](../validacao/evidencias/number-tweens-abi30-20261001/README.md).

ABI30; cena16/prefab3/Timer4/TransformTween3. 34 schemas,33 fachadas,57 receitas,229 ícones; seis propriedades qualificadas, nenhuma fachada ou tipo de autoria novo. P06 e o plano completo permanecem parciais: sequência/paralelo, timeline de propriedades, callbacks gerais e outros campos exigem contratos próprios.
