# Prefabs — evidências de componentes

`native-ownership.log`: **24/24 cenários**, incluindo cinco novos cenários integrados de composição, execução de Timer no GameWorld real, conflitos, archive, undo/redo, dependências e referências externas. `native.log` registra a execução inicial 23/24 e o fixture legado sem Câmera; `native-final.log` é a passagem anterior ao último reforço de Undo. O resultado final é `native-ownership.log`.

`host-final-build.log`: compilação do alvo dedicado e preview executável. `android-build.log`: `:app:assembleDebug` final com a proteção de Undo, sucesso em 51 s. `package-manifest.json` registra SHA256 do APK, fonte da reconciliação, bibliotecas arm64 e assets byte a byte.

`added-component.png` e `component-order.png`: UI host real, documento/registro reais, fonte/atlas existentes, raster por software. O Apply habilitado corresponde ao endereço real aprovado pelo preflight. Dependências ausentes desabilitam a publicação individual e informam a necessidade de publicar a cadeia; o contrato em lote aceita uma composição válida completa.

Sem instalação, execução ou captura física Android. F005 continua parcial para alterações da árvore de objetos, nested prefabs, variantes e mudanças estruturais em coleções de subelementos. Ver `docs/planos/PREFAB-COMPONENTES-2026-10-01.md`.
