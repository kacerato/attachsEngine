# Constraints e Transform Tween — 2026-09-30



Pacote: dois novos tipos de constraint e um tipo de tween, além das quatro constraints existentes preservadas. Os três têm descriptors, propriedades refletidas, schema/C# e consumidores nativos públicos. Constraints mantêm payload v1; TransformTween evolui para v2 com migração dos dados v1 e escolha de relógio. Não considerar esses nomes prova de validação no aparelho.



## Referências e recorte



[ParentConstraint, Unity 6000.0](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ParentConstraint.html) segue posição/rotação sem herdar escala, com offsets e máscaras separados. Astra implementa uma fonte: o offset de posição é transformado pela rotação da fonte, sem sua escala; a rotação destino compõe a rotação da fonte com o offset via matrizes. A influência mistura desde a baseline local existente, convertida ao mundo a cada avaliação. Não muda o parent da hierarquia.



[LookAtConstraint, Unity 6000.0](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LookAtConstraint.html) distingue olhar para a fonte, roll e vertical. Astra fixa o eixo de olhar em +Z, oferece vertical de mundo X/Y/Z e compõe roll local sobre a orientação calculada. Peso e máscaras de rotação têm efeito. AimConstraint continua permitindo seis eixos de mira. Coincidência e paralelismo com vertical preservam a pose anterior e produzem diagnóstico.



As seis constraints compartilham ordenação por fontes e ancestrais, detecção de ciclo, limite de dependência 256, referências refletidas remapeadas no clone e recusa de autoridade física/personagem. Remoção/desativação elimina baseline da avaliação. Não implementam múltiplas fontes, up-object, captura automática Activate/Lock ou rig de ossos; essas diferenças não são escondidas como flags sem efeito.



[Tween, Godot 4.5](https://docs.godotengine.org/en/4.5/classes/class_tween.html) fornece duração, atraso, transições, repetição e lifecycle do objeto associado. Astra adapta esses princípios a authoring persistente `astra.tween.transform`, com estado runtime separado por identidade objeto/instância; não pretende reproduzir o builder genérico de Tweener da Godot.



## Transform Tween



`SceneTweens::advance(world, delta, unscaledDelta)` recebe os intervalos de simulação e de quadro aceito, antes das constraints; ambos aceitam 0..0.25s. A sobrecarga de dois argumentos conserva os consumidores anteriores, usando o mesmo intervalo nos dois modos. `ignore_time_scale=false` usa simulação; true usa o intervalo não escalado. Pausa editorial interrompe os dois modos. Alterar o modo não reinicia progresso, remove cancelamento ou sobrepõe autoridade física. A migração v1 assume false. O Inspector, a fachada gerada e o arquivo usam a mesma propriedade. Referência específica: [Godot 4.5 set_ignore_time_scale](https://docs.godotengine.org/en/4.5/classes/class_tween.html#class-tween-method-set-ignore-time-scale). `reset`, `restart(world, object, instance)`, `cancel(object, instance)` e `state(object, instance)` são públicos. Status diferencia Idle, Delayed, Running, Completed, Cancelled, Authority, CompetingWriter, InvalidPose e NoChannels. `statusText` traduz esses estados para diagnóstico; uma UI só deve mostrar status vindo deste consumidor.



O componente seleciona posição, rotação e escala locais independentemente, conserva canais não escolhidos e captura o início ao começar. Defaults: enabled/autoplay=true, posição=true, rotação/escala=false, duração1s, delay0, Linear, um ciclo, relative/pingpong=false. Destino absoluto começa em posição/rotação zero e escala um. Relative soma posição/rotação e multiplica escala. Rotações usam interpolação Euler autorada, permitindo voltas completas; não prometem slerp nem caminho angular mínimo.



Curvas: Linear, Smoothstep, Quadrático entrada e Quadrático saída. Delay ocorre uma vez no início. Um ciclo pingpong contém ida e volta; loops0 repete indefinidamente; uma repetição finita pingpong termina no início. Grandes avanços dentro do limite são resolvidos por fase matemática, sem callbacks por ciclo. A edição de duração/curva/repetição afeta a fase viva; editar destino/canais/relative durante uma interpolação ativa recaptura a pose corrente e retarget sem repetir o atraso inicial. Completed/Cancelled continuam encerrados até restart.



Desativar o componente cancela; reativar reinicia quando autoplay está ligado. Objeto inativo pausa sem consumir tempo. Remoção limpa estado no próximo advance, mundo novo limpa todas as identidades e reset encerra a sessão. Cancel conserva a pose corrente. Autoplay=false aguarda restart explícito; a Inspeção em Play oferece Reiniciar/Cancelar no grupo Tempo, ligados ao consumidor público. Não há comando C# dedicado neste pacote.



Física/personagem conservam autoridade. Conservadoramente qualquer componente Animation ou constraint habilitado no objeto bloqueia o tween com CompetingWriter; não presume ausência de disputa por estado parcial. Ao liberar autoridade ou remover o writer, o tween recaptura a pose corrente e reinicia o atraso, evitando retornar a uma baseline anterior à disputa. Se houver vários tweens, o primeiro elegível tem a pose e os demais recebem conflito. Esse pacote não oferece arbitragem por canal entre autores distintos, sequência genérica, propriedades arbitrárias, callbacks gerenciados ou clock sem escala.



## Validação preparada



Dois cenários novos: Parent com offset rotacionado, escala independente e baseline estável, LookAt com roll; Tween com payload persistente, atraso, relativo, ponto médio, pingpong finito, restart/cancel e autoridade física. Os testes usam SceneGraph/GameWorld reais. Build e execução centralizados pelo agente principal; nenhum build/teste/ADB foi executado por este agente nesta rodada. O arquivo de teste tween é registrado no CMake pelo agente principal. O cenário também protege retarget sem salto e edição sem ressuscitar cancelamento. A lista de objetos é cacheada pela structuralRevision do GameWorld; componentes ativos são lidos a cada avaliação para consumir edição viva.



## Gizmos conectados ao authoring



Parent/LookAt mostram frame e fonte real. Tween mostra destino transformado pelo pai, com canais locais e relative conforme o documento; não pretende representar a baseline capturada da execução. Collider2D mostra offset e box/circle/capsule no plano XY; Joint2D conecta âncoras locais ou world-anchor; Force2D combina força mundial e relativa. AudioSource espacial desenha min/max distance e cones sem herdar escala; Listener mostra orientação +Z. Esses desenhos usam dados persistidos e não alteram solver/áudio. Um cenário integrado verifica offsets, destino relativo, world-anchor e alcance independente de escala. Captura no aparelho permanece pendente.



Validação central executada: constraints7/7 (inclui um cenário Joint2D pelo filtro), tween_delay1/1, gizmos integrado e componentes42/42 passaram. Inspeção recebeu ações Play reais e capturas finais em landscape/portrait stress; detalhes na auditoria e no documento de design. Android compilado, sem ADB.
