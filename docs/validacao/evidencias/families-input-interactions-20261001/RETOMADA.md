# Pausa solicitada pelo usuário

Atualização: após o pedido explícito "instale", o APK final foi instalado via ADB no 25053PC47G com Success. SHA-256 do base.apk instalado corresponde ao APK local. Evidência: adb-install.log e adb-install-manifest.json. A instalação ocorreu; os cenários finais de input e a qualificação física de runtime continuam pendentes. O objetivo de implementação das demais famílias permanece pausado.

Em 2026-10-01 o usuário autorizou instalação por ADB na próxima retomada, pediu continuar depois e especificou "mas nao agora". Não instalar nem executar no aparelho durante esta pausa. Retomar a conclusão de todas as famílias quando solicitado, começando pela verificação do bloco de input e instalação do pacote atualizado.

## Estado confirmado

- ABI36, cena17, mapa de ações2/perfil2 implementados; interações Press/Hold/Tap, habilitação, filtros de dispositivo, política runtime, persistência e edição conectadas aos consumidores.
- native-final.log: 24/24 cenários passaram antes da separação final do cancelamento por fonte. O teste foi ampliado para proteger teclado mantido após perda do mouse. Essa ampliação ainda precisa ser executada no binário recompilado.
- SDK: managed-transport-final.log e três regressões dirigidas de ABI/cena passaram. O SDK de input não foi alterado desde esses resultados.
- UI: response-initial.png é captura host executável revisada em 853×394 sem cortes. Falta gerar/revisar response-final.png com o binário final.
- Primeiro APK ABI36 compilou. A recompilação final abaixo inclui o ajuste de clearMouse para cancelar interações em remoção/cancelamento de ponteiro. Não houve instalação desta revisão.

## Processos iniciados antes da pausa

- Sessão exec 32327: build host dos alvos aether_input_family_tests e aether_ui_preview. Saída redirecionada para host-build-final.log. No último acompanhamento ainda não havia conclusão.
- Sessão exec 7694: Android :app:assembleDebug após ajuste final clearMouse. Saída redirecionada para android-build.log. No último acompanhamento ainda não havia conclusão.
- Não iniciar builds concorrentes sobre esses diretórios. Primeiro confirmar conclusão por sessão/log. Como android_game_input.h recebeu o ajuste final durante o build host, repetir incrementalmente o alvo de input se necessário antes da execução final.

## Pendências para fechar F046

Confirmar os dois builds, executar os 24 cenários no binário final, gerar captura final e convertê-la para PNG, revisar layout, rodar build/input-interactions-package-check.py e registrar hashes/evidências. Atualizar INPUT-INTERACOES-2026-10-01.md, escrever README das evidências e fechar F046 no ledger apenas depois dos resultados. Ledger permanece em 10 encerradas/77 obrigatórias sem encerramento, sem contabilizar F046 antecipadamente. Preserve os manifests históricos: não rerodar checkers dos blocos anteriores sobre um APK novo.

Na retomada, a autorização nova permite instalação e verificação por ADB. Separar essa evidência dos resultados host e SDK; não presumir captura host como prova Android.

## F043 preparado, ainda sem implementação final

Rastreamento de consultas físicas encontrou consumidores Jolt reais e lacunas de contratos para dados inválidos/callback depois de Stop/sobreposição inicial. Foram acrescentados test_query_contracts.cpp, query_family_main.cpp, PhysicsQueryContractsTests.cs e CONSUL­TAS-FISICAS-2026-10-01.md como preparação. Esses testes não foram executados, ainda exigem mudanças de produção e alvo CMake. Não declarar F043 concluída. O nome do documento no filesystem é CONSULTAS-FISICAS-2026-10-01.md.

Não há solicitação de commit/push. Preservar todas as mudanças anteriores e os arquivos não rastreados.
