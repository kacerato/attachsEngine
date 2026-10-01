# Evidências — segundo pacote de 50, campos físicos

Pacote: `docs/planos/PACOTE-50-CAMPOS-2026-10-01.md`.
Contagem: 4 tipos, 40 entradas de propriedades (10 em cada tipo) e 6 APIs = 50.
As oito propriedades comuns repetidas nos quatro tipos contam como entradas
distintas por tipo; vetores não são recontados por eixo.

| Evidência | Resultado / limite |
|---|---|
| `native-final.log` | 4/4 cenários; editor/archive/hierarquia/Jolt/bridge/lifecycle reais |
| `prior-regressions.log` | 4/4 cenários do primeiro pacote; inclui CCD |
| `managed-abi.log` | 1/1 novo contrato gerenciado |
| `managed-body-abi.log` | 1/1 preservação do contrato de corpo |
| `host-build.log` | Executáveis e bibliotecas nativas compilados |
| `android-build.log` | `:app:assembleDebug` passou |
| `script-build.log` | SDK e Fields50Probe compilados; 0 avisos, 0 erros |
| `fixture-export.log` | Projeto editável exportado e archive reaberto |
| `benchmark.log` | 96 corpos, 32 campos, 60 passos; full pipeline Debug host |
| `ui-captures.log` | 5 capturas UI reais; zero descarte/fonte ausente/recorte |
| `package-check.log`, `package-manifest.json` | SHA256 APK/assets; SDK/Rendering/atlas iguais aos assets gerados |
| `field-icons-concept.png` | Hipótese de ícones gerada por imagem; não prova UI |
| `field-volume.png`, `field-sphere.png` | Inspector condicional e gizmo de volume |
| `field-wind.png`, `field-drag.png`, `field-scope.png` | Efeito e alcance na superfície contextual |

As capturas usam `aether_ui_preview` com rasterização de UI software, fonte e
atlas do produto. O fundo não é uma cena Vulkan. Capturas foram inspecionadas;
o enquadramento e rótulos vetoriais foram ajustados e capturados novamente.

Jolt e o ScriptBridge são reais nos cenários nativos; o export gerenciado é
capturado por harness. O script C# foi compilado, mas seu READY/PASS não foi
executado no Android neste pacote. Não houve instalação, toque ou captura nova
do aparelho. Evidência física do pacote anterior não certifica os quatro campos.

Projeto de aceite final: `build/acceptance/Fields50-20261001-final`.
Ele contém script, quatro campos, quatro esferas físicas, câmera e luz direcional.
APK: `android/app/build/outputs/apk/debug/app-debug.apk`, ABI33.

Comandos direcionados executados, sem suíte geral:

```powershell
cmake --build build/editor-host --target aether_fields50_tests aether_bulk50_tests aether_ui_preview -j3
build/editor-host/aether_fields50_tests.exe
build/editor-host/aether_bulk50_tests.exe
build/editor-host/aether_fields50_tests.exe --benchmark-fields
dotnet build/fields50-managed/bin/Aether.Tests/release/Aether.Tests.dll PhysicsFieldsAbi
dotnet build/fields50-managed/bin/Aether.Tests/release/Aether.Tests.dll Bulk50Abi
dotnet build build/fields50-probe-check/Fields50Probe.csproj -c Release --artifacts-path build/fields50-probe-artifacts
build/editor-host/aether_fields50_tests.exe --write-project build/acceptance/Fields50-20261001-final
# Na pasta android:
.\gradlew.bat :app:assembleDebug --console=plain
```

Benchmark: média 2,709 ms, p95 2,968 ms e máximo 3,127 ms por passo completo.
Cinco passos de aquecimento precedem a medição; 60 passos não são 60 testes.
O custo inclui solver e sincronização, e não estima FPS/temperatura no Android.
