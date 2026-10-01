# Grupos — evidência ABI25 / 01-10-2026

[Contrato e referências](../../../planos/GROUPS-2026-10-01.md).

Host: groups 8/8 (cinco cenários novos e três regressões existentes); prefab 16/16; archive 8/8; regressões direcionadas 10/10. Logs nesta pasta. Astra gerenciado 52/52, conforme build/groups-managed-astra.txt; não equivale a executar GroupsProbe no CLR Android.

host-phone.png mostra UI executável 853x394 sem clipping/descarte/ícone ausente reportado. host-tablet.png teve três clips do drawlist. O cenário novo usa a rota real do Inspector, IME, criação/renomeação/remoção, Undo e rejeição de rascunho obsoleto. Também verifica arquivo/legado/prefab/clone; snapshots, ramo inativo e destruição; reimportação preservando associação local. As consultas da ABI usam GameWorld nativo.

android-build-final.txt registra build concluído. ABI25, cena15, prefab3, 32 schemas, 31 fachadas e 224 ícones. manifest.json registra hashes e igualdade do SDK e atlas empacotados. APK instalado no onyx; projeto Groups-20261001 enviado isoladamente.

**Android de grupos pendente:** a tela de bloqueio cobriu a atividade antes de abrir o editor. device-editor.png é captura preta dessa tentativa, não prova da função. Não houve GROUPS PASS nem fluxo save/reopen observado no aparelho neste pacote. Input/Time têm evidências anteriores próprias.

Fixture: tests/fixtures/groups/project/Groups-20261001 e [GroupsProbe.cs](../../../../tests/fixtures/groups/GroupsProbe.cs). Exige nomes autorais, ordem pai/filho, snapshots, add/remove idempotente e filtro de ativos. Execute em Play usando SDK/nativo ABI25 juntos. Nenhum projeto do usuário foi substituído.
