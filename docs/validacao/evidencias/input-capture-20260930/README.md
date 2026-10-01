# Captura de input — execução de 30/09–01/10/2026

Autoria interativa de vínculo de teclado, tecla negativa, botão e eixo de gamepad, conectada ao mapa existente. [Contrato e referências Unity Input System 1.11.2](../../../planos/INPUT-CAPTURE-2026-09-30.md). Não encerra P06 ou P00–P20. O número de componentes permanece **32 schemas / 31 fachadas C# / 54 receitas**; o atlas passou a **223 ícones**.

## Host e APK

Build nativo passou. **38/38 testes selecionados por input_**, incluindo três cenários novos de captura; **2/2 testes shipped_icon**. Os novos cenários verificam pressão/release/repeat, fonte incompatível, cancelamento, invalidação por mudança de documento, eixo neutro/direção, tecla negativa distinta, UI em landscape/portrait, Undo/Redo, serialização e consumo por InputService real. O teste input_workspace também passou isoladamente; já está incluído nos 38.

`:app:assembleDebug` passou, e a repetição após a última alteração confirmou fontes atualizadas. APK: **253308489 bytes**, SHA-256 **8565cb37129a065ceb97b812ad3adfb99f7b950bbc3228359ad94afdea418004**. SDK empacotado igual à saída do build, SHA-256 **3ecb7308fdc572ab0e1a74e8d2d53b2dc15d5619f21b2003b70d9752523ffe11**. [Manifesto](manifest.json). A ABI permanece 24.

## UI executável e conceito

NÃO IREI SER SIMPLISTA NO DESIGN.

[Landscape](after-landscape.png) e [vínculo](binding-landscape.png) têm zero descartes, glifos ausentes e recortes na rasterização host. [Portrait](after-portrait.png) registra duas primitivas recortadas no total do draw list; os controles da captura estão íntegros e legíveis. O fundo host não prova Vulkan.

[Antes](before.png) foi a captura executável usada como referência de edição pelo imagegen. [Conceito](concept.png) é hipótese de design, produzida em modo edit, fundo opaco, uma imagem de referência. Resumo do prompt: manter identidade Astra e toolbar preta, branco/lima e superfície escura; destacar captura de Saltar, instrução central, vínculo atual, fonte, feedback e Cancelar; usar composição aberta, sem cards, timeline, propriedades ou diagnósticos sem efeito. Arquivo original: `C:/Users/donod/.codex/generated_images/01a0f3b0-d099-7430-91c0-38b9f7409303/exec-e57fd234-1093-495a-8c98-940efbca2223.png`. A imagem não foi integrada ao runtime; a UI foi redesenhada em código. Ícones novos de teclado/gamepad foram integrados via SVG → PNG → atlas/enum reais.

## Android observado

O APK foi instalado com sucesso no aparelho **25053PC47G / onyx**, pelo ADB. Um projeto independente novo, Time-20261001, preserva os projetos anteriores. [Editor](device-editor.png), [captura](device-capturing.png), [resultado tecla 30](device-captured.png), [Undo para 0](device-undo.png), [Redo](device-redo.png) e [cancelamento por Escape](device-cancelled.png) são capturas reais do dispositivo. A tecla foi injetada por `adb shell input keyevent 30`; isso verifica o caminho Android de eventos, não um teclado físico ou gamepad conectado. O [arquivo salvo](device-saved.aescene) contém Saltar/Key/código 30. Qualificação de eixo e botão de gamepad continua sendo host.

O probe TimeProbe compilou no editor Android e executou pela ponte CLR/nativa: [captura TIME PASS](device-play.png) e [log Astra.Script](device-script.txt) mostram TIME READY → TIME FROZEN (3 disparos não escalados) → TIME PASS (1 disparo escalado, 11 FixedUpdate, 0,200096 s simulados em 0,400192 s não escalados). Isso amplia a evidência anterior de ABI24; não prova movimento visual ou shaders em cena com conteúdo.

O [projeto InputCapture-20261001](../../../../tests/fixtures/input/project/InputCapture-20261001/project.json) foi exportado pelo EditorSession/serializer reais. Seu vínculo inicial Saltar/Key/62 foi capturado para 30 pela UI no aparelho, salvo e reaberto. A [captura após reabrir](device-reopened.png) conserva 30. Em Play, `adb shell input keyevent --longpress 30` produziu INPUT READY → INPUT DOWN → **INPUT PASS**: uma pressão e uma soltura chegaram ao InputAccess C# pela ABI24. O [log](device-script.txt) registra também TIME PASS no mesmo Play. Isso não mede latência e não prova teclado/gamepad externos.

Perfis do jogador, mouse, rebind em gameplay, grupos e conexões autoráveis de eventos não foram implementados por esta captura. Alterações locais anteriores preservadas; sem commit/push.
