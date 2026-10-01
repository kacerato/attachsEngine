# Conexão persistente de Timer v3 — 01/10/2026

[Contrato, versões e referências](../../../planos/TIMER-CONNECTION-2026-10-01.md).

`host-tests.txt`: 9/9 testes direcionados. Três cobrem autoria por toques/IME/picker, arquivo/Undo/migração/clone, runtime/paridade/remoção; um protege o Inspector portrait de ferramentas sobrepostas; dois são regressões de timers; um confere a geração C#; dois verificam enum/atlas. `managed-astra.txt`: 52/52 anteriores; `managed-fixtures.txt`: 1/1 adicional compilando TimeProbe, InputCaptureProbe, GroupsProbe e TimerConnectionProbe contra o SDK publicado. Não se chama esse teste de execução Android.

`host-phone.png`: captura executável 853x394 sem descarte, glifo ausente ou clip reportado. `host-portrait.png` mostrou a sobreposição anterior. `concept.png` é hipótese produzida com geração de imagem a partir daquela captura; ela ainda desenhou controles sobre o cabeçalho e essa parte foi recusada. O prompt pediu conservar identidade Astra e limitar ferramentas ao viewport, retirar opções secundárias do viewport estreito e preservar somente ação/receptor reais, sem diagnósticos ou controles inventados. `host-portrait-after.png` mostra a UI implementada após correção; o drawlist total ainda reporta seis primitivas recortadas, sem descarte ou glifo ausente. O cenário de ponteiro confirma que os controles de cena não interceptam o Inspector e retornam ao viewport ampliado.

`android-build.txt` passou em 1m50 após as alterações finais. SDK e atlas empacotados coincidem com as saídas do build; hashes/tamanhos em `manifest.json`. APK ABI25 com Timer3 instalado via ADB no onyx; fixture TimerConnection-20261001 enviada como projeto independente. Permanecem 32 schemas e 31 fachadas; atlas225.

**Android visual/Play pendente:** keyguard continua cobrindo o editor. Não foram observados CONNECTION PASS nem edição da conexão no aparelho. A fixture já existe para essa execução e o teste host exercita o mesmo consumidor de ativação; isso não substitui o aceite Android. Resultados anteriores de Time/Input estão registrados nas pastas próprias com versões/hashes históricos.
