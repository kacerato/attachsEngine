# Campos físicos 2D — terceiro pacote de 50

Entrega: quatro componentes Box2D, dez propriedades semânticas em cada tipo
e seis consultas da SDK. Oito propriedades são comuns aos quatro tipos;
Vector2 é contado uma vez, sem contar X/Y. Infraestrutura auxiliar não entra
na contagem. Detalhes, referências Godot 4.5 e contratos estão no plano
`docs/planos/PACOTE-50-CAMPOS-2D-2026-10-01.md`.

## Evidências obtidas

| Verificação | Resultado | Evidência |
|---|---|---|
| Compilação host e atualização final | Passou | host-build.log, host-final-build.log |
| Novos cenários integrados | 4/4 | native-final.log |
| Campos 3D anteriores | 4/4 | prior-fields3d.log |
| Molas/comandos anteriores | 4/4 | prior-bulk50.log |
| Protocolo gerenciado XY | 1/1 | managed-protocol.log |
| ABI campos/corpos | 2/2 | managed-fields-abi.log, managed-bulk-abi.log |
| Script C# editável | 0 avisos, 0 erros | probe-build.log |
| Cena exportada/reaberta + GameWorld + passo Box2D | Passou | fixture-export.log |
| Native Android arm64 + APK | Passou | android-native-build.log, android-assemble.log |
| Conteúdo SDK/Rendering/atlas + bibliotecas APK | Passou | package-check.log, package-manifest.json |
| Custo de campos/solver/sync host Debug | médio 1,269 ms; p95 1,871 ms; máximo 2,254 ms | benchmark.log |
| Execução física Android deste pacote | Pendente | Sem instalação/ADB nesta rodada |

Os cenários nativos executam Box2D real. O teste ScriptBridge captura a
interface gerenciada em harness; o teste C# usa recorder de protocolo.
Nenhum desses dois é apresentado como execução completa do script Android.
`Fields2D50Probe.cs` foi compilado, mas seu log PASS em tempo de execução ainda
não foi observado no aparelho.

`native-first.log` preserva a primeira execução (3/4): a expectativa de undo
assumia um offset alterado diretamente fora do histórico antes de desfazer e
refazer a criação. A verificação final compara ambos os canais com o estado
real imediatamente anterior à edição de teclado; o cenário passou.

## Capturas e conceito

Cinco PNGs `field2d-*.png` vieram do executável `aether_ui_preview`, em
1200×560, com o código de UI, fonte e atlas reais. Mostram autoria XY,
retângulo/círculo, efeitos de vento/arrasto e alcance radial. Foram
inspecionados quanto a legibilidade, controles condicionais e espaço do
viewport. Não são capturas Vulkan nem evidência física Android.

`field-icons-concept.png` é uma hipótese gerada de quatro ícones.
Os ícones executáveis são SVG/PNG integrados ao enum e atlas pelo pipeline
real; o conceito não é usado como prova de implementação.

## Reprodução

Da raiz do repositório, após compilar os targets dedicados:

```powershell
cmake --build build/editor-host --target aether_fields2d50_tests aether_fields50_tests aether_bulk50_tests aether_ui_preview -j3
& build/editor-host/aether_fields2d50_tests.exe
& build/editor-host/aether_fields50_tests.exe
& build/editor-host/aether_bulk50_tests.exe
dotnet build tests/Aether.Tests/Aether.Tests.csproj -c Release --artifacts-path build/fields2d50-managed
dotnet build/fields2d50-managed/bin/Aether.Tests/release/Aether.Tests.dll PhysicsFields2DProtocol
dotnet build/fields2d50-managed/bin/Aether.Tests/release/Aether.Tests.dll PhysicsFieldsAbi
dotnet build/fields2d50-managed/bin/Aether.Tests/release/Aether.Tests.dll Bulk50Abi
dotnet build build/fields2d50-probe-check/Fields2D50Probe.csproj -c Release --artifacts-path build/fields2d50-probe-artifacts
& build/editor-host/aether_fields2d50_tests.exe --benchmark-fields
& build/editor-host/aether_fields2d50_tests.exe --write-project build/acceptance/Fields2D50-20261001-repro
& build/editor-host/aether_ui_preview.exe build/field2d-gravity.ppm 1200 560 field2d-gravity-volume
```

O exportador exige diretório vazio; não sobrescreve projetos existentes.
O projeto entregue está em `build/acceptance/Fields2D50-20261001`, com quatro
campos, quatro malhas reais Body2D/Collider2D, câmera ortográfica, luz e script.

Em `android`, executar `./gradlew.bat :app:assembleDebug --console=plain`.
O verificador de embalagem reproduzível fica em
`build/fields2d50-package-check.py`; a cópia nesta pasta registra o código usado.
Benchmark inclui 5 passos de aquecimento e 60 medidos, com 96 corpos e
32 campos. O loop é O(corpos × campos); host Debug não estima custo térmico
ou FPS Android.

## Limite da entrega

Implementados: quatro tipos novos e a cadeia de autoria, persistência,
Box2D, lifecycle, consultas, Inspector e ícones. Tipos novos parciais ou stubs
expostos: zero. Verificação física Android continua pendente.
Cena em paisagem e IDE em retrato preservam o contrato existente; o pacote
não altera orientação nem transforma capturas host em confirmação no aparelho.
