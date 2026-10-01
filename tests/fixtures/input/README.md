# Captura de vínculo → script no Android

`aether_ui_preview write-input-capture-project <diretório-novo-vazio>` usa o exportador de tempo e acrescenta InputCaptureProbe no mesmo objeto com duas instâncias reais de ScriptBehavior. Autora Saltar como Button/Key/62 no InputActionMap existente e verifica leitura/regravação da cena exportada. Recusa saída não vazia e symlink. Fontes são copiadas byte a byte.

Abrir o projeto → Cena / Configurações do projeto → Entrada → Saltar → Capturar → tecla B (código Android 30) → salvar → fechar/reabrir → conferir Tecla 30 → Play → pressionar/soltar B. Console deve registrar INPUT READY → INPUT DOWN → INPUT PASS. Repetição não gera nova borda até soltar. TimeProbe verifica os relógios no mesmo Play; input continua disponível enquanto Scale é zero.

O evento pode ser injetado pela bancada ADB para verificar a plataforma: `adb shell input keyevent --longpress 30`. Isso não é evidência de teclado físico conectado. O projeto exportado permanece editável; o código 30 só aparece depois de capturar pela UI, não vem hardcoded na cena inicial. [Evidência Android](../../../docs/validacao/evidencias/input-capture-20260930/README.md).
