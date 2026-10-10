# Eventos e marcadores — evidência de 10/10/2026

**42/42 cenários nativos de animação.** Integração C# com ponte nativa real:
**1 passou, 0 falhas, 0 pulados**, prefixos ABI 1–5 e comandos ABI 6.
SDK Release compilado sem erros, com dois avisos CS8981 preexistentes.
Dois exemplos completos do guia público compilaram sem erros/avisos.

O cenário de mecanismo usa os avaliadores reais Animation/Animator, GameWorld,
ComponentEventQueue e SceneEventConnections: código 7 alterna a atividade de
outro objeto, código 8 não repete a ação. O cursor Scripts recebe código,
payload double e instância exatos. Peso zero suprime callbacks; um avanço de
200 s entrega 256 eventos e informa 144 suprimidos por avaliador. Isso é
evidência de runtime nativo e entrega à fila, não aceite Android de um Behavior.

As capturas PNG vêm do UI executável com fonte/atlas de produção, rasterizado
no host. Cobrem a tela anterior, ponto selecionado, opções, arraste e paisagem
655×300 com propriedades acessíveis. Não incluem renderer 3D/Vulkan, personagem
ou atmosfera. Não houve ADB, instalação ou APK novo nesta rodada.

Comandos executados a partir da raiz:

```powershell
cmake --build build/editor-host --target aether_animation_clip_editor_tests aether_animation_authoring_sdk_bridge aether_component_contract_probe -j 2
build/editor-host/aether_component_contract_probe.exe --write-api managed/Astra.Scripting/Generated/Components.g.cs
$env:AE_CLIP_CAPTURE_DIR="$PWD/build/animation-cues-ui"
$env:AE_ANIMATION_CHARACTER_LIBRARY="$PWD/build/animation-character-library"
build/editor-host/aether_animation_clip_editor_tests.exe
$env:ASTRA_AUTHORING_SDK_BRIDGE="$PWD/build/editor-host/libaether_animation_authoring_sdk_bridge.dll"
dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release -- AnimationAuthoringSdkTests
```

Os arquivos GLB da biblioteca de aceite são privados e não acompanham esta
evidência pública. `hashes.json` registra os artefatos finais; os logs separam
build, cenários, SDK e exemplos. O primeiro build encontrou um aviso de
indentação no fixture e foi corrigido; os logs finais registram os passes.

Contrato, referências, migração e limites:
`../../planos/ANIMATION-CLIP-CUES-2026-10-10.md`.
