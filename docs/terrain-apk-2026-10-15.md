# APK de terreno — 15/10/2026

Branch: `codex/terrain-voxel-generator`. Fonte funcional: `b5c887a`; correções de empacotamento: `83f5a5b` e `f3abcbc`. Branch publicada no GitHub; sem merge na main da engine.

## Artefato

- Arquivo local: `build/artifacts/Astra-Dev-Terrain-75-arm64.apk`.
- 108.753.809 bytes; ARM64; mínimo Android API 26, alvo API 35.
- Pacote `dev.aether.editor.u07`, nome **Astra Dev**, código 75, versão `0.3.1-dev.terrain.20261014`.
- SHA-256: `db04b34304e4feb3f0557829f086aa4810725f497181aa65c4408dfadb0b627c`.
- Assinatura APK v2 verificada. Certificado debug local SHA-256: `68a37f8803e5f27155676328e8dc97385b41930bd878ad4168b1c962026599be`.

O certificado local é diferente do histórico documentado `9dd308380ab1606fa4e328a1d62fb9edda5abdf6cf7e5696bc8ccbf7c83bc66e`. Não presumir atualização compatível sobre uma instalação com a chave antiga. Não desinstalar nem apagar projetos para contornar isso; para uma atualização preservando a instalação, usar a chave original.

## Compilação

Gradle 8.10.2 / AGP 8.7.3, Java 17, NDK 27.1.12297006, CMake 3.22.1. No diretório `android`:

```powershell
./gradlew.bat :app:assembleRelease '-PastraApplicationId=dev.aether.editor.u07' '-PastraVersionCode=75' '-PastraVersionName=0.3.1-dev.terrain.20261014' --console=plain --max-workers=4
```

Primeira execução retomada após interrupção; objetos compilados foram reutilizados. A compilação C++ ARM64 concluiu. O fechamento incremental após corrigir Java terminou com `BUILD SUCCESSFUL`, 53 tarefas (21 executadas, 32 reutilizadas).

Correções preexistentes necessárias: removida dependência de `:export-core`, que não está no checkout/settings, e callback `onProjectExport`, que não faz parte de `ShellView.Listener` e apontava para `GameExportActivity` ausente. Nenhum caminho de exportação de jogos foi implementado ou validado. Licença e proveniência das texturas agora acompanham o APK.

## Auditoria

`apksigner verify --verbose --print-certs`: válido. `aapt2 dump badging`: identidade/versão/ABI/entrada conferidas. `zipalign -c -P 16 -v 4`: passou. CRC do ZIP passou. Biblioteca ARM64 contém o marcador da receita, menu do terreno e as quatro texturas incorporadas. Atlas é idêntico ao da branch; módulos C# são idênticos aos produzidos pelo build. Licença/manifesto presentes. Camada Vulkan de debug ausente.

**Limitação de páginas de 16 KB:** o alinhamento ZIP passou, porém isso não prova alinhamento dos segmentos ELF. Engine, libc++ e OpenSSL usam 16.384 bytes; libhostfxr e seis bibliotecas do runtime .NET usam 4.096 bytes. Este APK não foi validado para aparelhos de 16 KB e não tem conformidade ELF integral para essa configuração.

Evidências locais em `build/terrain-evidence/apk-{audit.json,signature.txt,badging.txt,manifest.txt,zipalign.txt}` e logs `build/terrain-apk-{resume,final}.log`.

Sem instalação, ADB, captura ou interação no aparelho. Os nove testes nativos do terreno no host são evidência separada. Este artefato é de desenvolvimento, sem atualização do download público.
