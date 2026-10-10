# Aceite Android de eventos e marcadores — 10/10/2026

Bloco de cues de clipe aceito no POCO F7 (25053PC47G/onyx), Astra Dev
0.3.1-dev.cues.20261010.1, versionCode 33, pacote dev.aether.editor.u07.
Origem funcional: attachsEngine, commit 5e7df0db20e0678305b7dc46e550aa3b1e7b75df.
O público dev.aether.editor permanece 0.3.0-preview.20261009/code 28.
Esta evidência não declara IK, retargeting, toda a animação ou toda a engine concluídos.

## Gates executados

| Camada | Resultado | Evidência |
| --- | --- | --- |
| Clipes/editor nativo | 42/42 | animation-cues-native-recheck.log |
| Regressões nativas existentes: Animator, skeletal, arquivos atômicos e física integrada | 36/36 | animation-cues-runtime-accepted.log |
| C# completo com bibliotecas nativas reais obrigatórias | 594/594, zero pulados | animation-cues-managed-accepted.log |
| C# SDK de autoria / ponte nativa real | 1/1, zero pulados | animation-cues-sdk-device-round.log |
| Android Release + testes Java | BUILD SUCCESSFUL; 17/17 (ProjectSceneSource 10, ProjectStore 7) | animation-cues-android-build.log |
| Pacote/instalação | SHA do APK instalado igual ao build; SDK/atlas/fonte iguais aos gerados | package-audit.json |
| Android CoreCLR + runtime | 6 checks PASS, Animation=6/Animator=6, repetidos após autoria e reabertura | runtime-cues.log |
| Toque + persistência + SDK | Conferência por comando compilado no IDE e executado depois de force-stop/reabertura | sdk-cold-pass.png; tools/validation/ClipCuesAuthoring.cs |
| Vídeo | 1.538/1.538 frames inspecionados, 52 folhas | runtime-review/VISUAL-REVIEW.md |

Os 24 casos de recursos executados anteriormente se sobrepõem à família de 42;
não são somados como cobertura independente. Não se executou toda a suíte nativa
de famílias alheias à alteração; a suíte gerenciada completa foi executada.

## Cenário real

Gerar um projeto vazio com `build/editor-host/aether_ui_preview.exe write-animation-cues-project <diretorio-novo>`.
Usa Mechanism.glb procedural CC0 e os serializadores/importadores reais, sem jogador
hardcoded. Dois donos reproduzem o mesmo AECLIP 4: Animation e Animator. O probe
tests/fixtures/animator/ClipCuesProbe.cs é compilado pelo IDE Android e executado no Play.
Conexões de evento reais alternam receptores somente no código 7, uma vez.

Checks: payload forward e isolamento de marcadores; filtro da conexão; peso zero;
reinício/travessia; pausa; reverso. Ambos recebem apenas tags 7/8, double correto,
identidade de cue e instância do emissor. Após acrescentar cues desabilitados pela
UI/SDK e reabrir o processo, os mesmos seis checks passam novamente.

Na interface física: selecionar, editar nome/código/valor, criar evento e marcador,
duplicar, excluir, arrastar, desfazer/refazer. ToqueAndroid foi salvo em 0,25 s,
código 99, valor 3,75, forward=false, reverse=true, enabled=false. O arraste de
Trava e o marcador temporário foram desfeitos, preservando Trava em 0,25 s.

SDK: criar evento em 0,6 s, código 16777215, double 1e100, reverse=true e
forward/enabled=false; marcador em 1,4 s. Conferir draft, commit, encerrar app,
reabrir, executar Conferir cues SDK salvos sem recriar dados. Resultado: seis cues
exatos e todas as propriedades preservadas. sdk-event-cold.png mostra o recurso
persistido. O source do comando está em tools/validation/ClipCuesAuthoring.cs.

13 arquivos editor.aescene preexistentes conservaram seus hashes após atualização
e testes. Listas de nomes de projetos privados não são publicadas. Só o projeto
isolado de aceite foi criado/modificado. Atualização Dev por cima, sem desinstalação.

## Ocorrências tratadas

- A primeira sonda procurava receptores depois de desativados; FindInWorld só
  retorna objetos ativos. A fixture foi corrigida para conservar referências no
  Start. Não foi mascarada como correção da engine.
- Uma primeira rodada nativa teve acesso negado do Windows na publicação atômica
  do journal de pose. O rollback preservou os arquivos; reprodução isolada passou
  e a repetição da família inteira passou 42/42. A causa externa não foi reproduzida.
- A suíte C# revelou expectativa antiga que recusava corpos com translação travada
  e rotação livre. Jolt 5.6 já suporta esse contrato, coberto também no nativo; teste
  e comentário foram corrigidos. Nenhuma regra de física do runtime foi alterada.
- Warnings preexistentes CS8981 (math/quaternion) não são falhas de execução.

## Reproduzir no host

Compilar targets aether_animation_clip_editor_tests, aether_animation_runtime_tests,
aether_animation_authoring_sdk_bridge, aether_physics_shared, aether_resources_shared
e aether_transform em build/editor-host. Não executar Ninja concorrente nesse diretório.
Executar os dois binários de família. Para o C#, adicionar build/editor-host ao PATH,
definir AETHER_REQUIRE_NATIVE=1 e ASTRA_AUTHORING_SDK_BRIDGE para a DLL real, então
`dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release`.

Android: `:app:assembleRelease :app:testReleaseUnitTest`, applicationId Dev,
versionCode 33 e versionName acima. Executável, SDK e atlas são um conjunto;
AECLIP 4 não abre no APK público antigo. Preserve backup antes de migrar.

## Limites

Aceite de um aparelho ARM64/Vulkan; não é matriz de todos os fabricantes, soak
térmico ou desempenho. O vídeo contém startup e uma lacuna de PTS documentada.
Seleção múltipla/clipboard de cues e regiões de marcadores não são anunciados.
IK, retargeting, drivers e bake FK/IK/root motion permanecem outros blocos.
Referências versionadas e decisões estão em docs/planos/ANIMATION-CLIP-CUES-2026-10-10.md.
