# Camadas de autoria — bloco vertical B4

Estado: implementação, host e aceite físico concluídos em 09/10/2026. Publicação do guia e da nota registrada ao final deste relatório. O recorte fecha camadas de autoria; B4 completo e B5/B6 continuam abertos.

## Recorte

Camadas não destrutivas em clipes independentes para posição, rotação, escala e morphs. Máscara esparsa por propriedade, ordem, criação/duplicação/remoção, nome, peso, Override/Additive, referência temporal/neutra, mute e solo. Mesmo avaliador no Studio, Animation e Animator; edição C# sem painel, persistência e histórico.

AECLIP 3 lê AECLIP 1/2 como uma camada Base permanente. IDs de camada compartilham o alocador monotônico de bindings/canais/chaves. ABI 3 preserva os prefixos 1/2; Sample continua sendo o canal cru, SampleComposed retorna a propriedade final. Formato e ABI são atualizados juntos com o SDK do APK.

## Semântica

Base fixa em primeiro lugar. Toda propriedade de correção possui uma base correspondente. Override interpola sobre o resultado anterior; Additive soma diferenças em posição/morph, aplica razão na escala e multiplica à direita o delta local de quaternion. Peso 0..1. Referência neutra T=0/Q=identidade/S=1/W=0 ou pose da própria camada num tempo do clipe. Override não aceita referência sem efeito. Só canais presentes na camada participam. Solo exclui movimento das demais camadas; uma Base excluída mantém sua pose inicial. Mute vence Solo. Flags são autoradas e persistidas, portanto afetam também runtime.

Campos de Pose/Curvas editam a camada selecionada. Viewport mostra composição final. Selecionar camada vazia não inventa canais: Objetos → alvo → propriedade cria um canal neutro ou inicial, na mesma transação que uma base ausente. Cópia da Base é explícita e reversível. Renomear camada não renomeia clipe.

Corte recusa remover uma referência ativa; retime/reverse/inserção global reposicionam referências. Inserção parcial recusa deslocar referência compartilhada com canais não deslocados. Colar entre clipes resolve Base ou nome/modo únicos da camada; ambiguidade recusa em vez de escolher a última trilha.

Compilação publica um canal por binding/propriedade, com fontes planas ordenadas (máximo 32 camadas). Avaliação usa buffers fixos, sem alocação por amostra. Caches glTF recusam canais autorados/compostos. Fonte importada não é reescrita.

## Design

NÃO IREI SER SIMPLISTA NO DESIGN.

Uma faixa de edição substitui temporariamente o transporte. Cena e gráfico permanecem visíveis; seleção direta em lista paginada evita atravessar 32 camadas. Modo, peso e referência ficam junto dos controles de audição; canais exibidos pertencem somente à camada selecionada. Os novos conceitos Mudo/Solo possuem SVGs editáveis, PNGs e integração no enum/atlas de produção. Capturas reais e comparação nos tamanhos 853×394 e 655×300 estão em `host/`; `device-ui.png` mostra o resultado Android. A primeira captura revelou que o inspector vertical de Pose ficava cortado no viewport baixo: substituído nesse contexto por uma faixa horizontal com XYZ simultaneamente acessíveis; morphs mantêm paginação de três componentes.

## Referências e decisões

- Unity 6000.0: https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationLayers.html — máscara, ordem e Override/Additive. Aqui a máscara é um conjunto tipado de canais do clipe, separado das camadas do controller já existentes.
- Godot 4.5: https://docs.godotengine.org/en/4.5/classes/class_animationnodeblend2.html e https://docs.godotengine.org/en/4.5/classes/class_animationnodeadd2.html — pesos e filtros devem alterar avaliação, não só apresentação.
- Source Godot 4.5-stable: https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/animation_blend_tree.cpp — composição no grafo, não estado de UI como runtime.
- UMotion Pro 1.29p04 fornecido: Manual/Layers.html, imagens LayersView/BlendWeightBar/AdditiveLabel/Mute e workflow InPractice2. Máscara por canais realmente autorados, base permanente, separar canal selecionado e pose final. A imagem local LayersView foi inspecionada. Vídeo oficial https://www.youtube.com/watch?v=TPzCp6Ezy4o consultado no navegador: telas em 1:11, 1:36 e 1:51 mostram a criação da camada aditiva e a chave de correção separada das chaves originais. A transcrição localizou o trecho 1:16–2:17. Observação de trechos de referência, não revisão integral desse vídeo.

Não é entrega de IK, retargeting, root motion, eventos/drivers nem biblioteca convertida dos pacotes. Esses pertencem aos demais blocos do roadmap; não são dependências fictícias desta composição.

## Portões de aceite

- Recursos: matemática TRS/morph, ordem/mute/solo/referência, migração, orçamento/IDs e recusas atômicas.
- Sessão/editor: fluxo de toque, propriedades com efeito, preview isolado, histórico e reabertura.
- C#: comandos cruzam ponte nativa real, versões ABI 1/2/3, nenhuma API decorativa.
- Android: build/hash/instalação separados de uso físico. Criar camada/canal, editar, ouvir/isolar, histórico, reprodução/Play e cold reopen.
- Todos os quadros dos vídeos usados no aceite: decoder, PTS/hashes, folhas e inspeção visual.
- Guia e nota nova na aba Atualizações do site; build/links/deploy READY.

## Evidência host — 09/10/2026

- Recursos: 21/21 cenários direcionados. TRS, seis morphs, referência, ordem, mudo/solo, formatos independentes, clipboard entre camadas e edições de tempo atômicas.
- Sessão/UI/runtime: 31/31 cenários na compilação final, incluindo criação de camada vazia por toque no router real, canal, peso, nome, audição, referência, duplicação/ordem/remoção, histórico, lista direta, copiar Base e reabertura a frio. XYZ alcançáveis a 655×300. SceneAnimator consome a composição no mesmo teste; documento fonte não muda.
- SDK: build Release sem erros/avisos; integração C# com DLL nativa real passou (1 cenário, zero skips), incluindo prefixos ABI 1/2/3, metadados, amostragem crua/composta, cópia de Base, transação e Undo/Redo.
- Primeira execução integrada: 30/31, uma expectativa de exclusão de chave falhou após cancelamento de arraste. A execução isolada e duas execuções integradas após nova compilação passaram. A seleção preservada passou a ter assert explícito; não há causa de runtime reproduzida para declarar uma correção específica desse incidente.
- Captura inicial de Pose cortada foi usada para corrigir a UI executável: faixa horizontal condicional substitui o inspector vertical no viewport baixo. Captura posterior examinada; todos os três campos ficam visíveis.
- `CamadasUniversais.cs` compilou em projeto isolado; um aviso de resolução de System.Collections.Immutable no projeto de sonda. A compilação/publicação/execução embarcada posterior passou sem erros/avisos, conforme aceite abaixo.

Estas evidências não são aceite físico. A área de cena vazia nos rasters host não é renderização Vulkan.

## APK e aceite físico — POCO F7

Dev `dev.aether.editor.u07`, versão `0.2.10-dev.layers.20261009.1`, code 22,
ARM64 Release. Build final 15m18s, 53 tasks; APK 104466046 bytes.
SHA-256 local e instalado:
`6df9a641f65eabe9520e35dbe9be9ff0a2453b8d3eeb9991a5e1682a4ecb0439`.
O build inclui a revisão final da faixa horizontal de Pose. O público 0.2.3 não
foi alterado. Log: `build/animation-layers-android-build.log`.

Projeto isolado `AnimatorClips-20261008`, cópia anterior guardada em
`build/animation-layers-device-before/AnimatorClips-20261008`. Dois mecanismos
compartilham controller, com override local somente em A; nenhum preset de player.

- Pela UI: criar camada vazia e canal de posição, Y=40, peso 0,5; Mudo/Solo,
  referência neutra/temporal, Additive/Override, duplicar/mover/remover,
  copiar Base, Undo/Redo e nomes separados de camada/clipe. Clip `Camadas UI`,
  camada `Correcao UI`. Captura `device-ui.png`.
- IDE: `CamadasUniversais.cs` compilado/publicado/executado pelo SDK embarcado.
  Verificações de canal cru 40, composição Y=20/escala 1,5, Mudo, Solo,
  referência, duplicação/ordem/cópia/remoção e readback. `device-sdk.png`.
- Encerrar/reabrir app e executar `Conferir camadas salvas`: metadados e
  composição persistidos passaram. `device-cold-sdk.png` e `device-console.txt`.
- Animator Play: os cinco asserts de `tools/validation/CamadasRuntime.cs`
  passaram — posição Y=20, escala 1,5, isolamento da outra instância,
  estado/override Fechada e rotação da Base avançando sob as correções.
  `device-runtime-assertions.png`, `runtime-final.mp4`, `device-console.txt`.
- Fonte GLB, licença e controller compartilhado permaneceram byte idênticos:
  `source-preservation.json`. Scene e script anteriores foram restaurados após
  o aceite, hashes conferidos; clipes produzidos continuam na biblioteca.
  A compilação embarcada do script restaurado foi publicada sem erro.

Não é validação física de skin/morph, IK ou caminhada; skin/morph foram exercitados
nos cenários host. Screenrecord não é benchmark de FPS, latência ou térmica.

## Revisão integral dos vídeos

`video-review.json` e `*/human-review.json` registram intervalo, hashes,
observações e folhas efetivamente examinadas. Cada quadro decodificado aparece
em ordem, sem amostragem temporal; `frames.csv` mantém PTS/hash RGB24.

| Vídeo | Quadros revisados | Folhas | Uso |
|---|---:|---:|---|
| controls.mp4 | 477/477 | 16 | Mudo/Solo, referência e modo |
| preview.mp4 | 296/296 | 10 | Reprodução no Studio |
| runtime-final.mp4 | 136/136 | 5 | Play com sonda LAYERS |
| runtime.mp4 | 291/291 | 10 | Suplementar; sonda Hierarchy anterior |
| mix.mp4 | 597/597 | 20 | Rejeitado: captura estática fora das ações |

Aceite funcional: **909/909 quadros, 31 folhas**. Total examinado incluindo
suplementar/rejeitado: 1797 quadros, 61 folhas. A primeira automação serializou
as chamadas de execução e não gravou os controles; foi substituída por captura
e ações no mesmo processo PowerShell. O vídeo final tem uma lacuna de captura
entre PTS 0 e 2,595s durante a preparação; os cinco asserts exportados e a
reprodução posterior são a prova de runtime, sem alegação de fluidez contínua.

## Publicação

Guia e nota publicados no AstraDocs; a capacidade do app está no Dev instalado,
e a distribuição pública permanece 0.2.3. Commit enviado à `main` do AstraDocs:
`884f72cd261d295fcfce3a17b95a18be31ed5657`.

- `npm run build`: passou; 952 páginas HTML, 951 Markdown, índices JSON e exemplos.
- `npm run check:links`: 951 páginas, 218859 links, zero erros.
- Cobertura de atualizações: 28 notas; entrada nova
  `2026-10-09-animation-clips-authoring-layers`, sem reescrever notas anteriores.
- Vercel: deploy `dpl_ND3RfppoQfdVrhMZv4u2aMRYYqUk`, estado **READY**, produção,
  commit acima e alias `astraengine.com.br`, sem erro de alias.
- Conferência ao vivo: nota visível em Atualizações, navegação para o guia,
  texto de disponibilidade/migração e imagens carregadas. Viewports 390×844 e
  1280×800 sem overflow horizontal; capturas `docs-live-mobile.png` e
  `docs-live-desktop.png`. Override de viewport restaurado ao final.

Guia: https://astraengine.com.br/pt-br/snapshot-2026-10-07/sistemas/animation-clips/

Nota: https://astraengine.com.br/atualizacoes/#2026-10-09-animation-clips-authoring-layers

As alterações da engine permanecem no checkout compartilhado e no APK Dev
instalado; não foi realizado commit/push da engine nesta entrega. Alterações
anteriores de desempenho/renderização do proprietário foram preservadas.
