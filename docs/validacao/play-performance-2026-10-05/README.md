# Play da Sponza: composição e plano, 2026-10-05

Pedido: melhorar FPS durante Play sem reduzir gráficos. O pacote específico e a ordem dos próximos trabalhos estão em `docs/planos/PERFORMANCE-POCO-F7-2026-10-05.md`, seção “Play preservando a imagem”. O roadmap fornecido pelo usuário é referência de engenharia, não evidência de sistemas concluídos.

A melhoria de editor estático M7 não atua em Play. No aceite anterior, a vista inicial gastava aproximadamente 17,8 ms de GPU, contra 3,8 ms de CPU ativa. Cerca de 2,43 milhões de triângulos eram visíveis. Esse diagnóstico indica priorizar GPU; não prova, isoladamente, que triângulos são o gargalo em vez de fragmentos/banda.

Primeira candidata: GUI autoral do Play dentro do passe de pós-processamento final compatível. Preserva ordem da interface, cor, input, simulação, geometria e efeitos. TAA, FSR1 e HUD separado do controlador global usam a rota original. A GUI fica fora de históricos temporais. O recorte do pós usado pelo editor não é aplicado em Play: a reconstrução continua cobrindo a saída inteira.

Código: `native/platform/android/instanced_renderer.cpp/.h`, opção de diagnóstico em `android_main.cpp`, encaminhamento pela lista explícita do launcher `AstraShellActivity.java`. `aether.play_post_ui_fusion` é opt-in; o caminho normal permanece anterior até comprovação de eficácia. Coletor: `tools/measure-play-composition.ps1`, comparação A/B/B/A com configuração/cadência fixas e verificação do caminho efetivamente executado.

Referência: [Khronos Vulkan Guide: Tile Based Rendering](https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html), evitar leitura/escrita externa entre passes compatíveis. Diagnóstico: [Godot 4.5 GPU optimization](https://docs.godotengine.org/en/4.5/tutorials/performance/gpu_optimization.html), separar custos de geometria, fragmentos e banda.

A primeira série `build/play-performance-20261005/abba/` foi **rejeitada**: o launcher não encaminhava a opção e a variante B registrava `post_ui_fused=false`. Não houve execução da candidata; não usar esses números como A/B. O encaminhamento foi corrigido, e o coletor agora recusa a variante quando a telemetria não confirma seu caminho.

Nova série: `build/play-performance-20261005/abba-forwarded/`. Resolução 0,75 fixa nos dois caminhos, alvo 120 FPS, qualidade autoral fixada para comparação; a proteção térmica do Android permanece ativa. A escala fixa é instrumento de comparação, não uma redução promovida como ganho. O aparelho está carregando: não há aceite de potência ou endurance nesta rodada.

Essa segunda série também foi rejeitada como comparação completa: a primeira janela B tinha nove frames reutilizados de editor, e seu contexto ainda identificava a câmera do editor. O profiler não abria uma época nova em Play quando a composição permanecia fundida. As janelas posteriores tinham zero reutilização, mas o contexto inicial não provava a câmera do jogo. Nenhum ganho foi declarado a partir delas.

Correção de M0: `profileSceneId` distingue `authored-project-play` de `authored-project`, reiniciando contexto/janelas em Play/Stop independentemente do estado da fusão. O coletor usa a opção existente `aether.start_play`, que inicia Play depois da publicação real, e recusa modo errado, ausência da candidata e frames reutilizados. Isso corrige a validade da medição, não é contabilizado como aumento de FPS.

## Resultado da composição

Série `abba-play-context` no APK `982F891C419CC75BACB0D5898F5C605EC88438CBD8751243465439B1DFCF7A2C`: A/B/B/A, 3.600 frames por variante, escala fixa 0,75, mesma qualidade e cadência solicitada, zero frames estáticos reutilizados em todas as janelas selecionadas. Apresentação contínua nas quatro rodadas: A 48,28/48,62 FPS; B 48,15/48,16 FPS. Capturas B idênticas a A, zero pixels diferentes.

GPU média A 18,6120 ms, B 18,5704 ms: diferença observada de 0,2234%. FPS interno agregado 48,35→48,16. **Não há eficácia demonstrada: candidata rejeitada como solução e mantida opt-in.** Não declarar economia térmica/potência. Os percentis são o pior percentil de janela, não percentis globais.

Limitação adicional descoberta nessa série: no autostart, o modo do frame tinha sido lido antes de iniciar Play. O primeiro contexto B usava a câmera anterior do editor, embora as janelas posteriores tivessem zero reutilização e as capturas finais fossem idênticas. Isso limita a prova de identidade do contexto inicial; os números não são usados para promover a candidata. Corrigido o snapshot do modo após autostart, antes da publicação do mapa, e novamente antes da seleção da câmera após possíveis falhas. A validação posterior de transição é registrada separadamente. Não reinterpretar a diferença de 0,2234% como ganho aceito.

O principal alvo continua no opaco, aproximadamente 15 ms nesta vista. A etapa seguinte é separar custo de geometria/banda de custo de fragmentos/material e alterar o caminho dominante, preservando imagem. HZB e chunks anteriores foram medidos em outra vista; só uma nova comparação no Play pode autorizar promoção nessa vista. Alvo de 120 FPS requer orçamento total de 8,33 ms e não foi atingido nem prometido por esta rodada.

## Estado final instalado

APK final `build/play-performance-20261005/play-performance-final.apk`, SHA-256 `C128B72470C81288E45C682E943EC47C1466A1746A58641E7B3493D9D925D766`, build Android Release aprovado e atualizado por `adb install -r`. Contém as correções de medição/transição; fusão do Play continua false por padrão. Não existe aumento de FPS promovido nesta rodada. A revisão inclui trabalho anterior do workspace, que permanece sujo; não é validação de todos esses outros sistemas.

Validação posterior no mesmo POCO: `transition-validation/`, modo `authored-project-play`, câmera inicial do player `x=-9; y=1,729999; z=0,2; yaw=1,570796 rad`, fusão solicitada e executada, todas as janelas com zero reutilização. SurfaceFlinger contínuo por aproximadamente 30 s, 48,41 FPS apresentados; não é novo A/B nem aceite de potência. A cena e a iluminação da captura são idênticas às capturas anteriores. Ao encerrar, o coletor reabriu Sponza normalmente, sem profiler, autostart ou quality lock (`ready.png`).

Hashes autorais conferidos após todas as rodadas: cena `10F98AC40F3F4B5CB02CFD960059498BB57FBF8828F9F3BB8AA719961ED468BC`, configuração gráfica `0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`. Ambas preservadas desde a entrega do player. Fontes/texturas não modificadas.
