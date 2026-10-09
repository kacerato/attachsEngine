# B4 — clipes e poses: evidência de desenvolvimento

08/10/2026. Repositório `https://github.com/kacerato/attachsEngine.git`,
branch `codex/gameplay-runtime`, base `665ea805`. B4 segue em implementação.
Este relato não fecha o Studio, nem declara equivalência com UMotion/Final IK.

Atualização de 09/10: code 21 aceito para bake por trilha, API, histórico e
reabertura. Foram examinados 477/477 quadros; redução UI 605 → 15 chaves e
sonda SDK 183 → 32. As seções anteriores registram cada revisão histórica;
o estado mais recente e os limites estão no fim deste relato e em BAKE.md.

## Pacote e aparelho

APK Dev `0.2.9-dev.clipes.20261008`, versionCode 17,
applicationId `dev.aether.editor.u07`, 103972874 bytes,
SHA-256 `5e17fd1c75351aafefa26da523cbd82eba553802126ec67c0e981ab5456f080a`.
Release arm64 instalado com atualização preservando dados. O aplicativo público
permanece separado. POCO F7/Android 16, modelo `25053PC47G`, ADB por Wi-Fi.

Projeto isolado `AnimatorClips-20261008`, derivado do cenário B3 pelo fluxo de
projeto, com dois mecanismos instanciados da mesma origem. O cenário é genérico:
não exige jogador, humanoide ou motor de locomoção.

## Autoria exercitada por toque

Selecionar Mecanismo A → abrir clipes → criar recurso editável → buscar filho
Painel → rotação progressiva → tempo 1 s → editar pose Y para 90° pelo teclado
numérico da plataforma. O preview gira apenas o filho vinculado. O recurso
recebe quatro curvas de quaternion e a curva derivada de distância angular,
com chaves agrupadas no mesmo tempo. Undo restaura 0°; Redo restaura 90°.

A curva de progresso registra 0°/90°/180° em 0/1/2 s. O último valor é distância
acumulada: o painel volta à orientação original em 2 s, não termina orientado a
180°. Fechar o editor de clipe exibe novamente a pose de origem da cena.

Salvar cena → encerrar processo → abrir projeto → abrir o mesmo clipe → buscar
1 s → selecionar Painel/progressivo → abrir Pose confirma X=0°, Y=90°, Z=0°.
Fechar o clipe volta a mostrar ambos os painéis na pose original. As chaves e
bindings permanecem; não é uma restauração baseada no estado anterior da UI.

Artefatos locais em `build/animation-clip-ui/`: `device-pose90.png`,
`device-undo.png`, `device-curves.png`, `device-cold-pose.png` e
`device-cold-restored.png`. Cópias do recurso antes/depois da sequência de
histórico diferem somente na revisão (3 → 5); IDs, alocador e curvas são iguais.
Hashes SHA-256 respectivamente
`987346eb4ac6f1d616b2264e8de1c8b612666c821ed523861d5fd71b83307c75` e
`befda1bee88f44315ba92cf9e537da45070b03613f3033825d26ab731e7aa72b`.

## Reprodução: revisão visual de todos os quadros

`device-loop.mp4`, 1920×886, 1954385 bytes, SHA-256
`3eef42fda3ade40708e8ecf75de8827aae9861d16468454d4707ad53b4a52993`.
Extração por `tools/validation/review-animator-frames.py`: 598 quadros
decodificados, 598 extraídos, zero descartados; último PTS 9,926011 s.

As 20 folhas `frame-review/sheet-00.png` a `sheet-19.png` foram abertas e
inspecionadas visualmente, abrangendo todos os 598 quadros. Região examinada:
x=810, y=240, largura=1120, altura=365, contendo os dois painéis. Os quadros
integrais `frames/00200.png` e `frames/00450.png` também foram examinados para conferir transporte,
playhead, curva e viewport no mesmo instante.

Os primeiros quadros registram o estado parado antes de tocar Play. Durante a
reprodução, o filho vinculado gira e retorna à pose original em ciclos; o outro
painel permanece parado. Não foi observado desaparecimento de objetos,
desvio do binding ou cobertura do cenário nessa região. O perfil de velocidade
usado é linear; esta captura não valida tangentes suaves, skin, morph, IK,
retargeting, eventos, desempenho de multidões ou estabilidade térmica.

O JSON de extração automática conserva seu aviso de revisão humana pendente;
este relato registra a revisão humana, separadamente da contagem automática.

## Host: extração e interface da próxima revisão

A cópia editável de um clipe importado e o seletor “Extrair da fonte” passaram
na validação host da revisão seguinte: 20/20 cenários, sendo 14 de recurso e
seis de integração. `host-import-final-build.log` e `host-import-final-tests.log`
registram a compilação e a execução. `clip-imported-picker.png` vem da UI nativa
executável com fontes/atlas de produção; a área vazia não é um render Vulkan.

A sonda importa o GLB do rig, instancia seus nós e skin, extrai o clipe sem
resampling, compara 201 tempos por canal contra a fonte Step/Linear/CubicSpline,
abre o seletor real e extrai uma segunda cópia por toque simulado. Adicionar
outra propriedade conserva o binding importado. Criação/edição/Undo/Redo usam
o journal e o registro de assets; a reabertura lê o registro gravado em disco.
Os bytes GLB, hash da fonte e pose de autoria permanecem iguais.

Uma execução intermediária recusou Undo após retirar uma dependência; não houve
reprodução na execução isolada, na validação final ou em três novas repetições
direcionadas (`host-history-recheck.log`). A causa da recusa intermitente não
foi determinada. O diagnóstico do cenário foi ampliado; não afirmar que foi
corrigida uma causa que não foi identificada.

## Android: extração no code 18

APK Dev `0.2.9-dev.clipes.20261008.2`, code 18, 103979070 bytes,
SHA-256 `b986d7bed1146f3b577c52f10cccf81c95292dfdb6e7be39c7d107cd3054b4f9`.
Build Release concluído; instalado por atualização no mesmo app de desenvolvimento.
Esta revisão permanece local, sem distribuição pública.

No projeto isolado `AnimatorClips-20261008`, o seletor real lista os três
clipes de `Fontes/Mechanism.glb`. Tocar “Meia abertura” cria `Clipes/Extraído.aeclip`
e abre a cópia independente com binding importado do filho Painel. Pose Y=45°
em 0,5 s foi editada para 90°; Undo restaura 45° e Redo restaura a chave em 90°.
Após salvar, encerrar o processo e reabrir o projeto, selecionar a cópia e
buscar 0,5 s conserva Y=90°. O segundo mecanismo permanece parado.

`device-code18-source.png`, `device-code18-extracted.png`,
`device-code18-pose45.png`, `device-code18-pose90.png`,
`device-code18-undo45.png`, `device-code18-cold-catalog.png` e
`device-code18-cold-pose90.png` foram abertos e examinados. A inspeção do
arquivo extraído confirma GUID da fonte, GUID do clipe original, binding do nó,
quaternion XYZW, nova pose sincronizada e `sourceOverride`. O GLB antes/depois
mantém SHA-256 `ca06483178d84b27428605065b3709e41dc70e3756c540f2df589d386b6db86b`.
Estas capturas validam extração, edição, histórico e persistência de um objeto
genérico; não validam render de skin/IK/retargeting no aparelho.

A fundação de seleção múltipla passou na execução host integrada de 21/21
cenários (`host-selection-tests.log`). O code 18 não contém essa expansão;
a ligação de seleção por área à UI está em implementação e validação separadas.

Na revisão inicial, estavam pendentes capacidades de B4: SDK C# de autoria,
intervalos/clipboard, bake/redução, camadas e mute/solo de autoria, eventos,
drivers/propriedades de componentes, auto-key no viewport, mirror/copy pose,
merge de reimportação e workflow completo dos pacotes. B5 e B6 também continuam
pendentes. Nenhum asset dos dois pacotes foi convertido nesta expansão em
biblioteca utilizável da engine. Os aceites posteriores de seleção, SDK e bake
estão registrados nas seções seguintes; não permanecem pendentes por esse motivo.

Referências: Unity 6000.0, Animation window e extração de clipes independentes:
https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationEditorGuide.html e
https://docs.unity3d.com/6000.0/Documentation/Manual/Splittinganimations.html;
Godot 4.5, Animation e código oficial de tracks:
https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/animation.cpp;
glTF 2.0.1, sampling/interpolação:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations.

## Revisão code 19 — seleção e clipboard no aparelho

APK local de desenvolvimento `0.2.9-dev.clipes.20261008.3`, code 19,
103989854 bytes, SHA-256
`ee0c7c9ca292aec36c79286225e65edf10baa8d724648c0cd2fb32a018f1b727`.
Instalado por atualização em `dev.aether.editor.u07`, preservando o projeto
isolado `AnimatorClips-20261008`; não é distribuição pública.

Selecionar a pose quaternion por área destacou seus quatro componentes.
Copiar e substituir em 0,25 s criou IDs 15–18 sem retirar a pose original.
Inserção deslocou as chaves seguintes em 1/60 s, estendeu a duração para
1,01666665 s e preservou IDs existentes; Undo/Redo recuperaram a topologia.
Mover por -0,1 s e escalar por 2 em torno de 0,25 s colocou o grupo em
0,0500000119 s. Um deslocamento inválido de 9 s foi recusado, sem mudar o
arquivo. O APK mostrou motivo genérico nessa recusa; o diagnóstico específico
foi corrigido e validado no host posteriormente, ainda fora desse APK.

Recortar, desfazer/refazer e reproduzir foram registrados em
`device-code19-clipboard-flow.mp4` (1920×886, 964106 bytes), SHA-256
`10cc0dd22538727c3e472bef5d6326ee868a9501bc2f344fae0b53cde80c85f8`.
Todos os **417/417 quadros** foram extraídos e examinados nas 14 folhas de
contato; quadros 200 e 350 também foram examinados em resolução completa.
A seleção sai/retorna com o histórico; o cursor reproduz e volta no loop;
o painel associado gira enquanto o segundo mecanismo permanece parado.
Não é uma medição de latência, FPS, render de skin, IK ou retargeting.

Depois de salvar, encerrar o processo e reabrir, o recurso mantém revisão 16,
fronteira de IDs 27 e as poses editadas. O arquivo antes/depois da reabertura
é byte a byte igual, SHA-256
`12e02e6a12f71c54e8cfd26adce4f4dfb3f64003730af66a7920caf992b07796`.
`Fontes/Mechanism.glb` conserva SHA-256
`ca06483178d84b27428605065b3709e41dc70e3756c540f2df589d386b6db86b`.
Apenas um track existe nesse recurso físico; inserir em todos versus nos
tracks selecionados é distinguido pelo cenário host com outros tracks.

## API de autoria — evidência host posterior ao code 19

`host-author-api-tests.log`: 24/24 cenários. A nova integração cria e edita
recurso com o painel fechado, adiciona binding canônico `A%2FB`, copia/cola,
amostra curva/pose pelo avaliador real e publica um único passo no histórico.
O recurso ao vivo não muda durante o rascunho. Contextos de outro comando,
thread estrangeiro e revisão vencida são recusados. O consumidor SceneAnimator
anima proprietário e filho; Undo/Redo e reabertura a frio preservam o recurso.
Desfazer a criação e recriar no mesmo caminho produz GUID novo; o antigo
não resolve para o novo recurso. Essa correção não consta no APK code 19.

A publicação de arquivo usa diagnóstico de etapa/erro real e sincronização
de arquivo no Windows/POSIX. A verificação focada teve 6/6 cenários, incluindo
destino bloqueado, e a sonda local fez 600 publicações sem retries. Houve
recusas intermitentes em verificações anteriores do editor; a causa permanece
não identificada. As repetições isoladas posteriores e a execução integrada
passaram. Isso não demonstra imunidade a interrupção de energia nem identifica
um antivírus como causa.

SDK C# e host de ferramentas estão em implementação. B4, B5/B6 e conversão
dos assets dos pacotes permanecem pendentes conforme o plano.

## SDK C# e comandos publicados — revisão host posterior

`host-sdk-editor-tests.log`: **25/25** cenários, incluindo publicação e menu
contextual pelo roteador real. `managed-authoring-integration-tests.log`:
**1 integração passou, zero falhas, zero skips**, com `AETHER_REQUIRE_NATIVE=1`.
A DLL host é uma fixture de teste que usa o EditorSession, ABI, recursos,
histórico e SceneAnimator reais; não é um backend mock e não entra no APK.

O comando é compilado/publicado pelo ProjectCompiler/NativeCompiler real e
atravessa a tabela de callbacks C#/C++. Verifica chaves ponderadas e flags no
snapshot, clipboard liberado, transformações/exclusão, bindings escapados,
seis valores de morph, quaternion/progressiva, Euler e recusa de conversão que
exige bake, sampling, descarte e commit. O compositor aplica o recurso no objeto
e no filho; Undo/Redo restauram o comportamento. Worker/contexto encerrado e
comando async são recusados. Fonte nova não publicada não altera o catálogo.

A amostragem encontrou e corrigiu um uso indevido do overload de quatro
valores em morphs maiores; agora usa span do tamanho do canal. Compilação do
rascunho é reutilizada até a próxima edição. Catálogo evita reconstrução em
cada item. Isso é uma correção de caminho/custo identificável, sem claim de FPS.

`managed-authoring-template-tests.log`: 2/2 cenários. O novo modelo é um
arquivo de partida escolhido pelo usuário, não um projeto ou script imposto
por padrão. `API-AUTORIA-ANIMACAO-2026-10-08.md` registra limites, vida, unidades
e a distinção entre commit de recurso e transação de comando inteiro.

As capturas de menu host usam catálogo controlado pelo teste de transporte;
não são prova de execução física nem de mirror/copy pose. O code 20 está em
build/aceite separado. Essas evidências não fecham B4 nem importam os assets
dos pacotes.

## SDK no APK code 20 — aceite físico

Dev `0.2.9-dev.autoria.20261008.4`, versionCode 20, 104026450 bytes,
SHA-256 `175f0db3f3bb3e9e3642dbe974712bed35c225a7926b6856f76cb26b24bd381c`.
Instalado por atualização, sem apagar projetos. No POCO F7, o caminho real foi
selecionar Mecanismo A → IDE → Modelos → Ferramenta de animação → criar
`AutorarMecanismo.cs` → recompilar/publicar → Ferramentas do editor → Criar giro
no objeto → abrir o recurso no Animation Studio. Não exigiu personagem ou Play.

O clipe gerado tem GUID `53d01f19c7b4b8769554e0fce9d34288`, nome Giro do objeto,
3 tracks TRS e pose quaternion de 90 graus em Y no segundo 1. O preview gira
o mecanismo A, mantendo B parado. A edição pela timeline moveu o grupo inteiro
de quatro chaves para 0,75 s; revisão 3, fronteira 29. A primeira entrada
automatizada acrescentou texto ao número existente e foi recusada; substituir
explicitamente o conteúdo publicou a mudança. Não se atribui essa recusa a
corrupção nem a falha do runtime.

Salvar → encerrar processo → reabrir projeto/clipe preservou o arquivo byte a
byte, SHA-256 `c357a277954b2c90d0a31f1562bae058a56f67400109ae5466c5f69d9686b315`.
A fonte GLB conserva `ca06483178d84b27428605065b3709e41dc70e3756c540f2df589d386b6db86b`.

`device-code20-sdk-preview.mp4`: 1920×886, 2049548 bytes,
SHA-256 `dc7f612e0be7b746ea5f9067ae15c88a5707346283683ae5219678de5513d17b`.
Foram extraídos e examinados **474/474 quadros**, sem omissões, nas 16 folhas;
quadros 44 e 84 examinados também em resolução completa. Cursor avança/volta
no loop, o objeto gira/retorna, B continua estático. Avisos de timestamps
repetidos impedem usar o vídeo como medição de FPS/latência. Execução do
comando foi conferida em capturas e no recurso, separadamente desse vídeo.
`code20-frame-review/human-review.json` registra a revisão visual e os limites.

Aceite restrito ao SDK e preview genérico. B4 completo, IK, retargeting,
conversão dos pacotes e publicação pública continuam pendentes.

## Bake — code 21 instalado; histórico de 08/10 e aceite de 09/10

Backend de bake/redução, SDK ABI 2 e faixa contextual implementados; 27/27
cenários nativos e integração C#/sessão nativa passaram. Capturas executáveis
853×394 e 655×300 examinadas após retirar a folha vertical que cobria cena e
gráfico. O relatório é medido no sampler real; não é certificação de erro
contínuo. Recurso/histórico usam revisões e IDs monotônicos.

Dev `0.2.9-dev.bake.20261008.5`/code 21 compilado em Release ARM64 e instalado.
104445346 bytes; hash local e instalado iguais:
`0769ac1f2d2d329a7e69c2d8f9d7b6d8bc682616a7fdc2155e75954cd6c7c57c`.
O clipe existente mantém seu hash antes/depois da instalação. Em 08/10 o
bloqueio do Android impediu o aceite. Em 09/10, com o aparelho desbloqueado,
o code 21 passou pelo fluxo completo de bake por toque e API: Quaternion →
Progressivo, Undo/Redo, redução desligada 605 chaves / ligada 15 chaves,
compilar/publicar/executar comando C# de duas voltas (183 → 32 chaves
escalares), reprodução e salvar/encerrar/reabrir. Os recursos UI/API reabriram
byte idênticos e a fonte GLB permaneceu intacta. Revisão visual de todos os
477 quadros de duas gravações em 16 folhas; PTS/hashes e revisão humana
separados. Nenhum resultado de FPS/temperatura é inferido das gravações.

Falha de link por disco cheio foi resolvida com compactação NTFS reversível
dos caches do host e Android: cerca de 3,2 GB recuperados; no fim há cerca de
1,7 GB livres. A exclusão de frames temporários foi recusada pela ferramenta,
com razão "blocked by policy"; nenhum deles foi apagado por esta sessão.

Detalhes, limites, origem e artefatos: `BAKE.md` e device-code21-acceptance.json.
O recorte bake por trilha/UI/API tem aceite; B4 inteiro permanece aberto.
Guia e nota Dev no AstraDocs são uma entrega editorial separada. O APK
público permanece na versão 0.2.3, sem distribuição dessa revisão Dev.
Em 09/10, guia/Atualizações publicados no AstraDocs main `332e3e5`, Vercel
`dpl_4QrffurHV18w8iCjQpeeVyUNSGrx` READY no domínio astraengine.com.br.
Build, links (zero erros), navegação desktop/móvel, imagem real, Markdown e
feed conferidos; URLs e critérios estão em BAKE.md.
