# B2 — composição aditiva: aceite em desenvolvimento

08/10/2026. Repositório `https://github.com/kacerato/attachsEngine`, base b900826a,
branch de trabalho codex/gameplay-runtime. Bloco B2 entregue; não declara
submáquinas, editor completo de clipes, retargeting, IK ou FBX implementados.

## Capacidade e fluxo real

Animator v4 e AEANIMATOR 2: camadas Override/Additive, peso efetivo, pose inicial
ou referência explícita por clipe e segundos. Posição/morph usam diferença,
escala usa razão e rotação usa delta local normalizado. Sampling, transições,
máscaras e autoridade física passam pelo compositor existente. Substituição
superior atenua aditivas por propriedade. Peso zero, troca de máscara ou estado
ativo sem canais removem contribuição antiga. Desativar mantém a última pose;
Stop restaura autoria pelo ciclo de Play existente.

Referência ausente, canal incompatível, escala de referência zero, quaternion
inválido e TRS não representável são diagnosticados. Não há referência inventada.
Editor usa seleção de camada e gaveta Ajustes, picker real, Editar recurso,
histórico e journal. Dez operações de composição estão no descriptor, consumidor
nativo e SDK C#; mudanças em Play são por instância e não alteram o recurso.
Novos ícones additive/reference-pose possuem SVG, PNG e atlas consumido pela UI.

Referência: Unity 6000.0, Animation Layers:
https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationLayers.html
O princípio de sobreposição foi adaptado ao compositor e à navegação por toque.

## Host e build

- `aether_tests.exe animator`: **16/16** cenários direcionados; cinco novos testes
  protegem matemática/referências, canais removidos/física, archive/migração e
  isolamento, autoria/histórico e importação de skin com consumidor real.
- O cenário de skin percorre GLB importado, scheduler e renderer: STEP, LINEAR e
  CUBICSPLINE; meia contribuição produz 22,5 graus/posição 1/escala 1,25, paleta
  de skin efetiva; peso zero retira o deslocamento. Morph também possui consumidor.
- SDK Release: zero erros/avisos. Fachada regenerada a partir do schema.
- Host preview e build nativo Android + Gradle APK concluídos. Nenhuma suíte inteira.
- Dev **0.2.7-dev.20261008**, versionCode **15**, arm64; tamanho **103845746 bytes**.
- SHA-256 do APK local e do `base.apk` puxado após instalação:
  `85f00c0294dc29b973362c6b3da9309941b6be155268d0704945d5a86f0bb497`.

## POCO F7 / Android 16

Atualização por `install -r` sem apagar projetos. Pacotes Astra conferidos:
`dev.aether.editor` público e `dev.aether.editor.u07` desenvolvimento.
Projeto editável independente `AnimatorAdditive-20261008`: dois mecanismos
glTF sem skin compartilham um recurso; sonda usa somente API pública C#.

Oito checks passaram antes e após personalizar/reabrir: configuração compartilhada,
peso zero removendo delta, quaternion relativo ponderado, isolamento entre instâncias,
pose inicial, modo/tempo/GUID, recusa de GUID ausente e restauração da autoria.

Autoria pelo painel real: composição, Undo/Redo, peso **0,65**, referência **Meia
abertura**, tempo **0,25 s**, escolha/undo e limpar referência/undo. Salvar,
encerrar o processo, reabrir, selecionar camada e conferir os valores. Arquivo
AEANIMATOR 2 puxado: GUID `a4a2e40ddd621986696f36bc7b6dc391`, revisão **11**,
camada ID **38**, blend **1**, peso **0.649999976**, referência
`ab187bf1fcd384e83826b3bc57855a78`, tempo **0.25**.

## Revisão de vídeo e interface

Captura 1920×886: **417/417 frames** decodificados e extraídos, zero descartados,
último PTS 6,918044 s. Todos os quadros revisados nas **14 folhas**; frames
completos 68, 113, 202 e 416 conferem contexto e toast. O recorte das folhas
não enquadra toda a largura do painel esquerdo aberto; frames completos mostram
que esse corte pertence à folha de revisão e não ao renderer.

Fases: início até 22 mantém autoria; 23–67 pose de base com peso zero; 68–112
peso um; 113–201 referência inicial/substituição; 202–416 restauração. O objeto
B permanece independente. Mudanças são degraus intencionais da sonda, sem
oscilação nos intervalos estáveis. Isso não prova FPS nem suavidade de caminhada.

Capturei a camada após reabertura: novos ícones legíveis, controles essenciais
agrupados, referência/tempo condicionais, valor curto Aditiva sem glifo ausente,
grafo com área dominante e autoria compartilhada protegida. Esta é a UI
implementada; o Studio Grafo/Clipes/Pose/Rig permanece uma proposta do roadmap.

## Migração e limites

Leitores novos aceitam Animator v1–v3 e AEANIMATOR 1. Grafos antigos migram para
Override e mantêm peso integral da base legado. Leitores antigos não leem v4/2;
preserve backup antes de salvar no Dev. Sem novo APK público nesta entrega.

Sem retargeting implícito, máscaras com pesos por osso, escalas negativas/nulas
publicadas como TRS ou performance medida de multidões. Skin/morph têm evidência
host; cenário físico é mecanismo sem skin. Submáquinas, interrupções, editor de
clipes/curvas, rig/IK/retargeting e biblioteca/importação seguem no roadmap B3–B6.

Pacotes FinalIK/UMotion: **783/783 hashes** conferidos, **75 páginas**, **788
seções**, **317 entradas de propriedades**, zero problemas de inventário. Assets
importados na engine: **zero**. O catálogo é pesquisa privada, não biblioteca
utilizável. Vídeo técnico de referência localizado, ainda não assistido.

Evidências anexas: host-tests.log, device-runtime-final.txt, extraction.json,
frames.csv, folha inicial/final, captura de reabertura/Play e recurso autorado.
Frames completos e demais folhas permanecem em build/animator-additive-acceptance.
Regeneração: `aether_ui_preview write-animator-additive-project`; sonda em
`tests/fixtures/animator/AdditiveAnimatorProbe.cs`.
