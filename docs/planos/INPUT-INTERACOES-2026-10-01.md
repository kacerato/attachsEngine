# Entrada: interações, habilitação e grupos de dispositivos

F046 reúne ações, vínculos, contextos, processamento de eixos, rebind/perfis e respostas temporais. O mapa é autoria da cena; InputService é o consumidor; ScriptBridge publica o estado ao SDK; o produtor Android e os ponteiros do Play alimentam o mesmo consumidor. Não há propriedades de interface usadas como fonte de verdade do gameplay.

## Contrato de execução

Cada ação possui enabled, deviceGroups (Touch=1, KeyboardMouse=2, Gamepad=4, All=7), interaction (Press, Hold, Tap) e duration de 0,01 a 60 segundos. Axes usam Press e mantêm deadzone, rescale, sensibilidade, escala/inversão por vínculo. A ação pode autorizar zero grupos, silenciando todas as fontes. Máscaras desconhecidas e valores não finitos são recusados.

Press preserva a resposta imediata. Hold começa no primeiro quadro pressionado, executa ao atingir a duração mínima e permanece pressionada até soltar; JustPressed ocorre uma vez. Tap executa um pulso ao soltar dentro do prazo máximo; pressionar além do prazo cancela. A agregação dos vínculos ocorre depois da filtragem de dispositivos e antes da interação. A duração começa na borda agregada: trocar de vínculo mantendo outro pressionado não reinicia o tempo. Não há pilha de interações, control schemes arbitrários, MultiTap ou captura de uma fila completa de eventos.

submit recebe o intervalo não escalado entre amostras. O primeiro quadro pressionado começa em zero, sem contabilizar tempo anterior à pressão. EditorSession repassa seu intervalo de quadro antes de Update, independentemente de GameWorld.timeScale. Pausa de Play não avança esse relógio. Não há resolução inferior ao período de amostragem: down/up entre quadros é retido como uma pressão de um quadro, com soltura no seguinte. Isso preserva cliques rápidos, mas não mede sua duração física subquadro. Relógio inválido aborta estado pendente, sem sucesso silencioso.

Perda de foco, captura interativa, contexto desligado, desabilitação e desconexão/cancelamento da fonte abortam interações pendentes. Um cancelamento não vira Tap. Fase e progresso vêm do consumidor real: Waiting, Started, Performed, Canceled, Disabled. Elapsed é limitado a 60 segundos; Progress reflete elapsed/duration enquanto Started e 1 quando Performed. O cancelamento do contexto também ocorre se desligar/religar entre amostras. Um novo mapa descarta políticas e estados transitórios.

O botão de toque do Play passa a publicar Down, estado mantido e Up/Cancel, com ownership por pointer ID e suporte a vários ponteiros. A pressão breve também é retida até uma amostra. Assim Hold não depende do antigo clique emitido somente ao soltar. TouchButton 0/1 são os botões de Play existentes; índices adicionais continuam disponíveis para produtores externos reais, sem criar botões fictícios.

## SDK e persistência

ABI36 acrescenta inputActionCommand ao fim do layout: query, enabled override, restore, group query, group write. Payload tipado tem 32 bytes e tamanho explícito. InputAccess.ActionState consulta enabled efetivo/autorado, interação, grupos autorados, fase, duração, elapsed e progresso; SetActionEnabled e RestoreActionEnabled alteram política runtime; DeviceGroups filtra globalmente. Nome ausente, operação inválida, máscara inválida, tamanho incorreto e mundo parado falham explicitamente. NativeBehaviorRuntime exige a versão/layout completos.

Cena17 escreve ASTRA_ACTION_MAP 2 com precisão suficiente para roundtrip float. Cenas anteriores conservam a leitura do mapa sem cabeçalho e herdam enabled=true, Press, duration=0,5, grupos=All. Arquivo antigo não aceita um payload novo disfarçado. Leitura monta um candidato antes de publicar; versão não suportada ou campo inválido preserva o mapa anterior.

Perfil de rebind2 carrega os mapas versionados; perfil1 continua legível para autoria antiga equivalente. O perfil permite mudar apenas vínculos. Enabled autorado, duração, interação, grupos, contexto, papéis e identidade entram na verificação de compatibilidade. Habilitação runtime e filtro global são políticas separadas e não são serializadas pelo perfil, nem alteram a cena. Overrides e captura anteriores continuam usando os mesmos consumidores.

## Editor

NÃO IREI SER SIMPLISTA NO DESIGN.

A captura anterior mostrava três colunas cortando conteúdo em 853×394. A implementação mantém a lista de ações em paisagem e oferece uma superfície contextual com Ação, Vínculos e Resposta. Em espaço estreito, seleção anterior/próxima substitui a lista. Não se mantêm simultaneamente todos os controles. Resposta expõe enabled, as três interações, duração condicional e três grupos, com nota derivada da configuração real. Eixos não oferecem temporização que o runtime recusa. Campos numéricos, histórico e revisão do teclado usam os caminhos existentes. O ícone input/response foi integrado por SVG, raster, catálogo e atlas reais.

A hipótese gerada usa a captura anterior como referência; não prova funcionamento. Captura posterior do executável host e interação efetiva determinam o aceite de layout. Uma captura host não comprova Vulkan Android.

## Referências e adaptação

- [Unity Input System 1.11.2 — Interactions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Interactions.html): estados da interação e timeout por duração. Astra usa uma interação por ação, sem reproduzir a pilha completa da Unity.
- [Fonte HoldInteraction.cs, tag 1.11.2](https://github.com/Unity-Technologies/InputSystem/blob/1.11.2/Packages/com.unity.inputsystem/InputSystem/Actions/Interactions/HoldInteraction.cs) e [TapInteraction.cs](https://github.com/Unity-Technologies/InputSystem/blob/1.11.2/Packages/com.unity.inputsystem/InputSystem/Actions/Interactions/TapInteraction.cs): distinção entre tempo mínimo sustentado e prazo de soltura, extraída para o consumidor existente.
- [Action Bindings 1.11.2](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html): filtragem por grupos e overrides separados da autoria. Astra usa grupos tipados de fontes físicas, sem prometer pareamento multiusuário ou control schemes nomeados.
- [Actions Editor 1.11.2](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionsEditor.html): configuração contextual de ações/vínculos. Para mobile, a Astra usa destinos alternáveis, preservando densidade e acesso por toque.
- A busca de uso real encontrou [Input System Interactions Explained, Samyam](https://www.youtube.com/watch?v=rMlcwtoui4I), mas o provedor não abriu o vídeo. Nenhuma conclusão sobre seus frames, cliques ou interação visual foi atribuída a esse conteúdo.

## Aceite

Criar/selecionar ação → configurar Hold e grupos → alterar duração → Undo/Redo → salvar/reabrir → manter pointer Down no Play → completar o intervalo mesmo com timeScale=0 → soltar/cancelar → conferir nova pressão. Outros cenários dirigidos protegem Tap na borda do prazo, cancelamento por foco/contexto/captura/desconexão, política runtime separada da autoria e migração de arquivos/perfis.

Validação em andamento. Evidências e contagens serão registradas após execução, build Android e inspeção da captura posterior. Esta revisão não será instalada/executada no aparelho por instrução do usuário; embalagem APK e host são evidências separadas.
