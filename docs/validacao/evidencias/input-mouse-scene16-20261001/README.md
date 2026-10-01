# Entrada mouse — 01/10/2026

Implementação e referências: [contrato](../../../planos/INPUT-MOUSE-2026-10-01.md). Artefatos e hashes: [manifest](manifest.json).

42/42 testes ligados a input passaram na rodada final (incluindo quatro novos de mouse). Regressões: timeout 3/3, limites portrait 1/1, arquivo 26/26 e atlas 2/2. 4/4 testes de mouse passaram, incluindo câmera real em Play, ausência de controle virtual duplicado, perda de foco e restauração da autoria após Stop. Compilação C# de cinco fixtures passou (tempo, tecla, mouse, grupos, timeout).

As imagens host-phone.png, host-portrait.png e host-capture.png provêm do executável aether_ui_preview. Layout portrait mantém seleção, criação de vínculo, fonte, botão nomeado, captura, avançado e remoção acessíveis. Landscape divide lista e edição; há quatro primitivas recortadas de texto secundário, sem comando descartado ou glifo ausente. Captura tem zero primitivas recortadas. Nenhuma imagem conceitual é apresentada como implementação.

Build Android passou. APK instalado com Success, SDK e atlas empacotados comparados byte a byte. A fixture independente tests/fixtures/input/project/Mouse-20261001 possui mapa salvo e MouseProbe compilável. Android permanece sem qualificação do mouse porque o aparelho está bloqueado; não houve observação de MOUSE PASS, mouse físico ou scroll físico.

Este pacote não encerra P06 nem P00–P20. Perfis de rebind persistentes do jogador e conexões gerais continuam pendentes.
