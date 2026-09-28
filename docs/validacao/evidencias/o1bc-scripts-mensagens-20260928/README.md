# Aceite O1 — etapas 2 e 3, 28/09/2026

Relatório: [scripts, destruição e mensagens](../../../planos/O1B-SCRIPTS-MENSAGENS-2026-09-28.md).

Capturas reais via ADB, sem retoque, aparelho 25053PC47G, 2772×1280 landscape:

1. [Runtime e scripts](01-runtime-scripts.png): Nested criado em Play, dois
   DynamicReceiver e confirmação DYNAMIC PASS.
2. [Instância desabilitada](02-disabled-instance.png): primeiro script desligado
   pelo Inspector, segundo preservado, indicação de alteração transitória.
3. [Stop e salvar](03-stop-saved.png): apenas Driver na cena autoral.
4. [Reabrir e repetir](04-reopened-pass.png): novo processo e sessão, novo PASS.

[Log Android](android-acceptance.log) contém somente mensagens do cenário no
APK final. [Testes nativos](native-tests.log) têm filtros sobrepostos; com a
[regressão de seleção](selection-tests.log), são 100 testes distintos.
[Behavior](managed-tests.log): 19; [World](world-tests.log): 9.

APK SHA-256: `B9A612DDA019E619D1911D3CF30F3AE0DEC622ADA16667793B317C6AD6994C4E`.
Fixture: `tests/fixtures/dynamic-scripts/`. Uma exceção de receptor é intencional;
o teste verifica seu isolamento. Capturas não substituem as asserções de runtime.
