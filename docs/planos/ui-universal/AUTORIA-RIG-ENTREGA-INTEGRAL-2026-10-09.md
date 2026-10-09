# Entrega integrada: autoria, rig e personagens dos pacotes

Pedido do proprietário: entregar os blocos citados, incluindo autoria de poses,
auto-key, copiar/espelhar, eventos/drivers, reimportação, retargeting, IK e bake
FK/IK; usar personagem real dos pacotes fornecidos. A autorização atual permite
selecionar também BoZo. Não continua válida a exclusão anterior de BoZo para a
escolha desse personagem. Não é autorização para publicar fontes dos pacotes
no site público.

Base conferida após `git fetch origin --prune`: main e codex/gameplay-runtime
em `a868c8ac518a774c867a7703482391c37df3990a`, sem divergência. Preservar o bloco
J Recast/Detour, ajustes Android/licenças e o Animation Studio já integrado.

## Cadeia e ordem

1. Converter personagens preservando rig/skin/materiais/clipes; importação
   pelo caminho real de recursos da engine, instâncias editáveis.
2. Autoria de pose isolada e compartilhada por UI/API; gesto transacional,
   preview sem chave, Auto-key e restauração por cancelamento/lifecycle.
3. Copiar/espelhar com seleção, espaços/referência e correspondência explícita;
   grupos Quaternion completos, hierarquia e repouso preservados.
4. Eventos e drivers tipados com consumidor runtime, ordem, loop/reverse/seek,
   inspeção e preview sem disparo incidental de gameplay.
5. Rig/repose/mapping, retargeting configurável e constraints ordenadas depois
   da animação, respeitando ownership da física; solvers/limites/diagnósticos.
6. Bake FK/IK e publicação pelo sampler real; reimportação com comparação
   base/local/nova fonte, conflito explícito e journal, mantendo personalizações.
7. Cenário autorável dos personagens e mecanismo: UI e C#, Play, Undo/Redo,
   salvar/encerrar/reabrir, erros e instâncias independentes; evidência host,
   pacote e aparelho separadas; vídeos de aceite revisados em todos os frames.

Nenhuma etapa é concluída por possuir somente modelo, enum, botão ou teste.
Esta lista registra a obrigação, não a implementação.

## Seleção inicial de personagens

Inspeção real via Blender 4.5.0 dos FBX fornecidos: Robot Kyle (UMotion), 49
ossos e malha com skin de 3.136 vértices; Viking (Final IK), 22 ossos, corpo,
escudo e machado, arquivos Idle/Walk/Run. Os FBX referenciam texturas de
workstations antigas; receitas explícitas devem resolver arquivos do pacote.
Escolher ambos permite conferir proporções, eixos e nomes diferentes, além
de extremidades presentes somente em um rig. O conversor de malhas estáticas
existente desparenta os meshes e não deve ser usado para personagens com skin.

## Design

NÃO IREI SER SIMPLISTA NO DESIGN.

Modo Pose temporário, viewport dominante e timeline ligada à seleção. Gizmos
e diagnóstico devem refletir o alvo/contexto atual. Auto-key distingue gravação
de preview; um gesto produz um passo de histórico. Detalhes de mapping/solver
usam superfície contextual expansível, sem manter quatro docks simultâneos.
Identidade angular aprovada e ícones reais no pipeline SVG/PNG/atlas. Comparação
Origem/Resultado só entra quando ambos os estados têm avaliação/restauração
independentes. Captura conceitual não substitui a UI executável.

## Referências concretas

- Unity 6000.0, autoria/Record/Preview:
  https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AnimatingAGameObject.html
- Godot 4.5, estágio após AnimationMixer, influência e lifecycle:
  https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html
- Godot 4.5, repouso, eixos e retargeting:
  https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/retargeting_3d_skeletons.html
- UMotion Pro 1.29p04: PoseEditor, CustomProperty, RotationModes, Layers,
  InverseKinematics e ImportExport fornecidos localmente.
- Final IK 2.2: solvers analítico/CCD/FABRIK, limites, Baker e inspectors
  fornecidos. Dependências Unity devem ser adaptadas ao runtime real Astra.

## Estado da execução

O espaço foi liberado externamente e a execução retomada. A tentativa anterior
de remover caches regeneráveis foi rejeitada pela revisão automática e não foi
contornada. Não iniciar builds concorrentes no mesmo diretório nem apagar
fontes, projetos autorais ou trabalho de outros checkouts.

- Conversor FBX → GLB com receitas explícitas: Kyle (49 juntas), Viking Idle,
  Walk e Run (22 juntas). Texturas encontradas no pacote, skin/bind/hierarquia
  e ações de todos os donos conservados; hashes das fontes conferidos. Walk e
  Run possuem repousos diferentes do Viking base: ficam como fontes completas
  separadas, exigindo retargeting explícito antes de compartilhar um rig.
- Código de autoria em implementação: draft de pose, Record/Cancel/Auto-key,
  gizmos de posição/rotação/escala na camada isolada e inspeção composta.
  Escala segue os eixos locais; translação e anéis usam o mundo com conversão
  pelo pai. Transformações que exigem shear são recusadas com diagnóstico.
- ABI de autoria 5/C#: preview, seek, StagePose, RecordPose, CancelPose e Close.
  Prefixos 1–4 preservados. StagePose edita uma propriedade selecionada e
  não equivale ainda a um clipboard de pose de rig completo.
- A primeira revisão passou 38/38 cenários direcionados (incluindo o aceite
  opt-in dos personagens) e a ponte C# ABI 1–5 passou sem cenários pulados.
  Capturas executáveis 853×394 e 655×300 foram inspecionadas e os rótulos foram
  corrigidos. Revisão posterior acrescenta descarte da inspeção isolada ao
  sair de Pose, acúmulo de propriedades pela API e arraste sob pai girado;
  essa revisão está em recompilação. Não contar a passagem anterior como
  validação desses casos posteriores.
- Projeto privado reproduzível pelo comando `aether_gui_preview
  write-animation-library <biblioteca-GLB> <destino-novo>`: quatro fontes,
  quatro clipes editáveis, materiais e hierarquias preservados. Kyle possui
  um canal autorado em seu braço; Viking conserva Idle/Walk/Run originais em
  rigs separados. O gerador passou por salvar, carregar registro/fontes/cena
  em outra sessão, comparar os clipes e avaliar o runtime. Esse projeto não
  foi incluído no APK público nem aceito visualmente no aparelho.
- ADB reconectado ao POCO F7. O aparelho estava em uso em outro aplicativo;
  foi solicitada uma janela para abrir o Dev, sem interromper esse uso.
  APK code 28 está em preparação a partir de a868c8ac (a base main/code 27 de
  navegação). Nenhum code antigo foi reinstalado ou desinstalado.
- O primeiro build ARM64 code 28 terminou, mas seu APK antecede as correções
  finais de isolamento/acúmulo de propriedades. A recompilação posterior
  falhou por ENOSPC; não instalar/distribuir esse APK como a revisão final.
  A recompilação host posterior também não terminou. Os 38/38 e o teste C#
  acima pertencem à primeira revisão, não aos casos adicionados depois.
- O guia e a nota de Atualizações ABI 5 foram preparados no AstraDocs e o build
  HTML/Markdown/JSON terminou. O check de links falhou por ENOSPC ao escrever
  sua saída. Não houve commit/push/deploy dessa documentação, nem conferência
  visual no navegador (inicialização também falhou por falta de espaço).
- A limpeza pontual de cinco cópias antigas de APK foi rejeitada pela revisão
  automática, com `blocked by policy` e sem motivo adicional. Não houve
  exclusão nem contorno. Há apenas C:; foi solicitada liberação manual de
  pelo menos 3 GB. Cópias antigas somam cerca de 495 MiB e os objetos do alvo
  antigo `build/editor-host/CMakeFiles/aether_tests.dir`, 0,94 GiB. Preservar
  fontes/projetos e os alvos atuais de autoria. Repetir build/testes finais,
  checks/publicação docs e aceite Android somente após espaço disponível.

Retargeting, espelho/cópia de rig completo, eventos/drivers, IK, bake FK/IK/root
motion e merge de reimportação continuam obrigações deste pedido. Nenhum desses
blocos é declarado concluído pelo conversor ou pela autoria de uma propriedade.

## Retomada após liberação de espaço — aceite code 28

O proprietário liberou espaço manualmente. A conferência não identificou fonte
crítica rastreada removida; as exclusões de caches/bin/obj do usuário foram
preservadas. O helper CMake antes não rastreado foi reposto em
`tools/cmake/host-validation-symbols.cmake`, com configuração concluída.

As correções finais foram recompiladas: host 38/38 cenários direcionados e
integração C# nativa ABI 1–5 com 1 passagem, zero falhas e zero pulados. Essa
passagem inclui acúmulo de propriedades, descarte da inspeção isolada e pai
girado. O conversor final repetiu Kyle com fontes preservadas e o mesmo GLB.

Android Release terminou. Code 28, versão `0.3.1-dev.pose.20261009.1`, instalado
no POCO F7 com SHA-256 igual ao APK local:
`9d93256d2cd27638093b2c3e1ce9dbd1c0d68a0e125b215eeebb4a4267111350`.
O app público foi preservado. Aceite físico cobriu draft numérico sem escrita,
Cancelar, translação por gizmo, Gravar, Undo/Redo, Auto-key numérico e reabertura
após encerrar o processo. O projeto privado de quatro personagens foi aberto,
renderizado e reproduzido em Play. As fontes dos pacotes não foram publicadas.

Todos os 887 quadros de duas gravações foram inspecionados em 30 folhas, com
PTS/hashes e zero omissões na extração. Os trechos não mostraram malha quebrada;
não constituem medição prolongada, aceite de IK ou qualidade geral de locomoção.
Registro: `build/animation-pose-ui/acceptance-code28.md`; capturas, vídeos e
relatórios estão nessa pasta. Rotação/escala/morphs e SDK desta revisão continuam
com evidência host, sem aceite físico equivalente.

Guia e nota `2026-10-09-animation-pose-draft-gizmos` no AstraDocs atualizados para
esse estado real. O APK code 28 é uma instalação local Dev, não download público.
Retargeting, cópia/espelho de rig completo, eventos/drivers, IK, bake FK/IK/root
motion, merge de reimportação e biblioteca integral permanecem em aberto.

A publicação do AstraDocs foi concluída: commit `8f5773149775d43c4c597c365917e759c996ee7e`
na main; deploy `dpl_FYrkBV8j6Ek1CQGo44Wrx2BTafHE` READY com alias
`astraengine.com.br`. Build: 952 páginas HTML; verificador: 951 páginas,
218.868 links, zero erros. Guia e Atualizações foram inspecionados em desktop
e celular; o link real da nota abre o guia. Feed JSON e Markdown públicos
confirmam code 28/ABI 5 e o registro de 887 quadros. Captura pública:
`build/animation-pose-ui/docs-live-code28.png`. O aceite persistente está em
`docs/validacao/animation-pose-2026-10-09/acceptance.md`.
