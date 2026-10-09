# Animation Studio — conversão Euler e consolidação

Data: 09/10/2026. Repositório conferido: `https://github.com/kacerato/attachsEngine.git`, checkout `C:/Users/donod/Downloads/atchengine`, branch `codex/gameplay-runtime`, base `665ea805`. As alterações locais de renderer/desempenho do proprietário foram preservadas. Este relatório fecha o bloco de conversão/consolidação; não declara B4, UMotion, IK ou a engine completos.

## Capacidade entregue

Quaternion e rotação Progressiva podem ser convertidos para Euler XYZ contínuo com referência inicial explícita em graus. A convenção é Rz·Ry·Rx. O levantamento escolhe o ramo equivalente mais próximo da referência, incluindo tratamento de gimbal, refina a amostragem, reduz chaves e compara o resultado com o sampler nativo. Referência ausente, valor inválido, revisão vencida e orçamento insuficiente são recusados; não há publicação parcial.

A consolidação avalia o compositor real: ordem, peso, mute/solo e canais esparsos das camadas. Produz outro recurso AECLIP 3, com GUID novo e Base única, preservando bindings, dependência importada e recurso de origem. TRS e morph usam o mesmo avaliador dos clipes em runtime. Preservar rotação na consolidação gera Quaternion; Euler requer referência explícita. Consolidar o rascunho não faz Commit na origem.

O histórico registra a criação como uma transação independente. Undo remove o recurso, Redo o recria. O novo recurso entra no catálogo normal, pode ser renomeado, atribuído como override local de estado do Animator, salvo e reaberto. Não existe formato paralelo exclusivo do exemplo.

## API e implementação

- `native/core/rotation_math.h`: ramo Euler XYZ próximo e verificação angular.
- `native/resources/animation_clip_bake.h/.cpp`: conversão, redução, verificação e consolidação atômica do compositor.
- `native/editor/editor_animation_clip_resources.cpp`: publicação revisionada e histórico do novo recurso.
- `native/editor/editor_animation_authoring.h/.cpp`: ABI 4 de 200 bytes, preservando prefixos ABI 1/2/3; pedido avançado de 56 bytes.
- `managed/Astra.Scripting/Editor/AnimationAuthoring.cs`: `ClipEulerReference`, `ClipBakeSettings.EulerReference`, `ClipEdit.CreateConsolidated` e `ClipConsolidation` tipados. Hosts antigos recusam a capacidade nova explicitamente.
- Editor Animation Studio: destino Canal/Clipe novo, modo de rotação, confirmação e edição da referência XYZ, criação e relatório. Os ícones `animation/consolidate` e `animation/rotation-branch` fazem parte do gerador SVG/PNG, catálogo e atlas reais.

Os caminhos acima devem ser lidos junto ao diff local: não foi feito commit/push do código da engine nesta revisão. O SDK e o APK de desenvolvimento foram compilados e instalados; o APK público não foi substituído.

## UX executável

NÃO IREI SER SIMPLISTA NO DESIGN.

O bake usa duas linhas contextuais, com uma página de parâmetros e outra de referência XYZ. O viewport e a timeline permanecem acessíveis. Euler fica indisponível até confirmar uma referência; resultado e erro aparecem junto à operação. A mesma API funciona sem abrir o painel.

Capturas host em `build/animation-consolidation-ui/` cobrem 853×394 e 655×300; são rasterização executável do editor, sem constituir prova de renderização 3D. As capturas Android `device-before.png`, `device-result.png`, `device-sdk.png` e `device-cold-reopen.png`, nesta pasta, mostram o fluxo real com viewport. Os ícones novos foram conferidos no pipeline e na interface executável. O nome automático longo fica cortado no cabeçalho; renomear pelo editor para `Consolidado UI` foi conferido. Não foi introduzida uma reorganização global da navegação.

## Validação separada por camada

### Recursos, editor e C#/nativo

- 24/24 cenários de recursos: ramos equivalentes, gimbal, duas voltas, composição TRS/morph, máscara, mute/solo, cancelamento, orçamento e serialização.
- Execução integrada final: **35/35** cenários direcionados. Inclui conversão/publicação pela UI, referência, histórico, reabertura e ABI 4 em viewport pequeno. Não somar os 24 aos 35: fazem parte da execução integrada.
- SDK Release: zero erros e dois avisos CS8981 preexistentes.
- C# contra a ponte nativa real: **1 passou, 0 falharam, 0 pulados**, cobrindo prefixos ABI 1/2/3/4, parâmetros avançados e publicação/preservação. O projeto da sonda emitiu aviso MSB3277 de versões de Immutable; não confundir com os dois avisos do SDK.
- Durante a validação foi corrigido ownership das fontes/ícones na captura de teste: o fixture liberava um vetor temporário ainda referenciado. Não era uma correção do runtime Android. Um run de depuração recusou o save de Redo; o cenário isolado e a execução final passaram. Não se reivindica correção de um bug de Redo em produção.

Logs host: `build/animation-consolidation-host-build.log`, `build/animation-consolidation-resources-results.log`, `build/animation-consolidation-editor-results.log`, `build/animation-consolidation-managed-build.log`, `build/animation-consolidation-sdk-results.log`.

### APK e aparelho

Android ARM64 Release: 53 tarefas, 27 executadas e 26 atualizadas. APK de desenvolvimento `dev.aether.editor.u07`, code **23**, versão **0.2.11-dev.consolidation.20261009.1**, instalado no POCO F7 (onyx_global). SHA-256 do arquivo local e do APK instalado idênticos:

`201c8af81e28bbf1e5f137f723e0ad38cc5c7019cf144de4be2af0ea9b6fd836`

Projeto isolado `AnimatorClips-20261008`. O comando C# `ConsolidacaoUniversal.cs` converteu com referência explícita, comparou **401 poses** e publicou `Consolidado SDK · mecanismo`, mantendo a origem. Pela UI foram conferidos destino novo, Y=360°, criação, Undo/Redo, catálogo e renomeação para `Consolidado UI`. O relatório da criação UI mostrou 18 chaves, 1.443 pontos de verificação e erro máximo 0,0001262 u/°.

O recurso UI foi ligado a um override local do estado Fechada de Mecanismo A. O controller compartilhado ficou idêntico. Em Play, a sonda temporária `ConsolidacaoRuntime.cs` passou nas cinco verificações: posição Y=20, escala 1,5, Mecanismo B independente, estado Fechada e rotação avançando entre dois instantes. `runtime-device.log` registra os cinco PASS. A primeira tentativa usava uma geração antiga do script; o resultado aceito é a execução posterior à recompilação, também gravada. A cena é um mecanismo genérico; skin/morph possuem evidência host, não aceitação física nesta revisão.

Após encerrar e reabrir o app, o comando C# conferiu novamente **401 poses**. Sete clipes permaneceram idênticos byte a byte. GLB, licença, controller e clipe autoral ficaram preservados. Ao terminar, a cena e o script originais do projeto foram restaurados; os dois arquivos foram puxados novamente do aparelho e seus hashes comparados aos backups. Os clipes produzidos e a ferramenta de inspeção continuam disponíveis. `preservation.json` contém as comparações.

### Revisão dos vídeos

Foram examinados **1.431/1.431 frames em 48 folhas**, sem pular frames. Cada extração contém PTS e hash RGB24 em `frames.csv`; `visual-review.json` registra os intervalos e observações. `video-review.json` agrega a revisão.

| Vídeo | Frames | Folhas | Alcance real |
|---|---:|---:|---|
| controls.mp4 | 598 | 20 | Ciclo de modos e abertura/confirmação da referência; inclui períodos estáticos. Não prova sozinho a conversão nem Undo/Redo. |
| create-preview.mp4 | 476 | 16 | Destino e criação, recurso Base, relatório e preview com movimento e instância independente. |
| runtime.mp4 | 357 | 12 | Entrada em Play, cinco PASS e reprodução contínua. |

Essas gravações não medem FPS sustentado, latência, consumo ou temperatura. Erro máximo é observado por amostragem e unidade/propriedade, não uma prova matemática de equivalência em todo instante.

## Referências e adaptação

- [Unity 6000.0, Euler curve import](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationEulerCurveImport.html): representação, resampling e ambiguidades; adaptação com referência explícita, ramo contínuo e diagnóstico.
- [Godot 4.5-stable, Animation](https://github.com/godotengine/godot/blob/4.5-stable/scene/resources/animation.cpp) e [Animation.optimize](https://docs.godotengine.org/en/4.5/classes/class_animation.html#class-animation-method-optimize): avaliação efetiva e redução com precisão; adaptação ao compositor e sampler existentes.
- [Unity 6000.0, AnimationUtility.SetEditorCurve](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AnimationUtility.SetEditorCurve.html): autoria por API independente da janela; adaptação ao rascunho revisionado e histórico Astra.
- Manual local UMotion Pro 1.29p04, ImportExport/RotationModes: separação autoria/publicação, destino e precisão. [Tutorial oficial Export Animations](https://www.youtube.com/watch?v=IKjIsIJs5hM): transcrição estudada e frames dos trechos de destino/publicação vistos; não se declara revisão integral frame a frame desse vídeo de referência. Não há exportação FBX implementada por este bloco.

## Limites e sequência restante

Somente ordem Euler XYZ. O levantamento preserva a trajetória orientacional representada; não recupera voltas que já foram perdidas na fonte Quaternion. Trabalho síncrono e com orçamento: não há job com progresso/cancelamento na UI, embora o backend trate cancelamento. Não há bake FK/IK/root motion, retargeting, edição de pose/auto-key/espelho, eventos/markers/drivers, propriedades arbitrárias, merge completo de reimportação nem biblioteca dos pacotes convertida. BoZo permanece excluído. Esses itens continuam pendentes em B4 e nos blocos seguintes; os nomes dos pacotes não contam como suporte.

## Publicação

Guia público e nota nova de Atualizações publicados no AstraDocs, separados do código privado e das evidências internas. Commit/push `aefda20401e6da0b223214f85e370fd1c9863612` confirmado no remoto. Deploy Vercel `dpl_mhheKizxnN9pB27ygJqARL34aiSc`, produção, **READY**, SHA correspondente e alias `astraengine.com.br` confirmado.

Build local gerou 952 páginas HTML, 951 páginas Markdown, índices JSON e dez exemplos. Conferência de 218.864 links: zero erros. As 28 notas anteriores ficaram intactas; a nova nota identifica code 23 como desenvolvimento local e não como APK público distribuído.

No navegador executável, Atualizações exibiu a nota nova e seu link abriu o [guia público](https://astraengine.com.br/pt-br/snapshot-2026-10-07/sistemas/animation-clips/). Versão code 23, API CreateConsolidated e imagem real 2772×1280 confirmadas. Viewports 1440×1000 e 390×844 sem overflow horizontal; imagem carregada e seção nova inspecionada visualmente. Capturas `site-guide-desktop.png`, `site-guide-mobile.png`, `site-updates-desktop.png` e `site-updates-mobile.png` nesta pasta. Override de viewport restaurado ao terminar. `publication.json` registra o resumo. O código da engine continua local; somente as docs foram enviadas/publicadas nesta revisão.
