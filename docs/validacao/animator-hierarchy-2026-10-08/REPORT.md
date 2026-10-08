# B3 — grupos aninhados e interrupções: aceite em desenvolvimento

08/10/2026. Repositório https://github.com/kacerato/attachsEngine, base
1c851bf4d314e5592ec4cf55c7bc02584878cb22, branch codex/gameplay-runtime.
Esta revisão cobre B3. A equivalência completa com FinalIK 2.2/UMotion Pro
1.29p04 continua pendente nos blocos B4–B6; BoZo excluído.

## Modelo, consumidor e ownership

Animator v5/AEANIMATOR 3 acrescentam máquinas com parentagem, destino inicial,
IDs estáveis, coordenadas e nomes. Estados pertencem a uma máquina; ligações
registram escopo, Entrada, política de interrupção, prioridade, reentrada,
offset e unidade de duração. Nomes não contêm barra e devem ser únicos entre
irmãos. Leitores aceitam Animator v1–v4/recursos 1–2; o fixture v4 é o arquivo
real da revisão B2, não um mock de wire format. Grafos legados ficam na raiz.

Entrada escolhe ligação condicional ou padrão direto, descendo por grupos.
Saída consulta rotas de máquina e pode subir vários níveis. Qualquer estado
procura do escopo mais interno até a raiz, antes das filas normais. Rotas
ausentes/cíclicas/sem estado efetivo geram diagnóstico; não inventam um padrão.
Gatilhos só são consumidos quando a rota inteira encontra destino válido.

Cinco políticas: só Qualquer estado, origem, destino, origem→destino e
destino→origem. Por prioridade limita a fila na ligação ativa. Duração fixa
usa segundos; duração normalizada considera clipe ponderado e velocidade
efetiva da origem. Offset inicia o destino em voltas sem reproduzir eventos
anteriores. Ligações de grupo/Entrada não oferecem blend: a saída iniciadora
do estado possui sua duração.

Durante fades o compositor existente captura canais por alvo/propriedade,
cobertura, peso e contribuição relativa antes de camadas superiores atenuarem
as inferiores. Uma interrupção parte dessa pose realmente exibida. Posição,
quaternion, escala e morph preservam composição parcial, máscara e aditivas.
Sem captura necessária, a troca é recusada com diagnóstico e conserva o fade
anterior e seus gatilhos. Não há captura adicional no estado estável.

Captura, parâmetros, relógios, grupos ativos e overrides pertencem à instância.
Parar Play limpa estado runtime pelo lifecycle existente e preserva autoria.
Eventos: interrupção antes de mudanças de grupo, saídas interno→externo,
entradas externo→interno, depois entrada de estado. Grupos da origem/destino
participam enquanto a mistura está ativa. IDs de eventos existentes preservados;
novos machine_entered/machine_exited/transition_interrupted são tipados.

## Editor e SDK

Grafo por níveis, viewport dominante e gaveta contextual. Breadcrumb restaura
vista/zoom sem alterar autoria. Criar grupo cria uma Entrada real na mesma
transação; mover valida ciclos; duplicar remapeia a árvore e defaults;
remover é recursivo. Histórico e publicação do controller usam o journal
existente, revisão e ownership explícito de Editar recurso. Navegar continua
disponível em instância protegida e Play.

Ligações entre os mesmos nós projetados possuem trajetos separados, inclusive
self-links. Desenho e hit-test compartilham a mesma geometria. Política aparece
logo após destino; campos condicionais evitam duração decorativa em máquina.
Ícones grupo/interrupção possuem SVG, PNG, IDs e atlas real: total 298 entradas.

Play/CrossFade aceitam caminho completo de estado/grupo; nome curto só se único.
GetCurrentState publica Path/NextPath/LeafName/IsInMachine. O transporte permite
até 1087 bytes sem truncar o caminho em 128 bytes, preservando a ABI. Fachada
Animator oferece três eventos novos com receptor e callback tipado. Essa é API
de execução/inspeção; transações de autoria de topologia/clipes em C# permanecem
pendentes e não são alegadas como entregues.

## Host, SDK e build

- 25/25 cenários focados `aether_tests.exe animator` na revisão final. Cobrem
  três níveis, conditional Entry/Exit/Any, políticas, prioridade, offset,
  duração normalizada/eventos, repetição de fades, quaternion/escala/morph,
  aditivas parcialmente ponderadas, controller compartilhado, migração v4,
  prefab, caminho longo/ABI, histórico e seleção de ligações projetadas.
- SDK Release compilado; sonda com API pública compilada pelo ProjectCompiler:
  um cenário dirigido. Contrato de componente 1/1, conexões de eventos 6/6.
  Fachada gerada conferida com o schema real. Nenhuma suíte inteira executada.
- Preview usa a UI executável e renderer real em 1100×600 e 800×450; zero
  comandos descartados e zero fontes ausentes. Capturas não são mockups.
- Android Release arm64 e APK Dev 0.2.8-dev.20261008/code 16 concluídos.
  A captura de vídeo e a autoria inicial usam o primeiro build B3, SHA-256
  `03fb430a0256cf1e913ac648f922f3d0504e59db544802891d943529f650ba83`.
  O build final só acrescenta roteamento visual/hit-test de ligações sobrepostas,
  sem mudança no runtime da animação. Hash e reaceite final registrados abaixo.

## POCO F7 / Android 16

Atualização `install -r` do desenvolvimento dev.aether.editor.u07 preserva
projetos. Público dev.aether.editor separado. Projeto editável independente
AnimatorHierarchy-20261008, criado pelo serializer/importador existente; dois
mecanismos glTF sem skin compartilham o mesmo recurso e têm poses próprias.
Sonda: `tests/fixtures/animator/HierarchyAnimatorProbe.cs`, somente API pública.

15 checks passam antes e após autoria/reabertura: origem compartilhada, padrão
raiz, entrada em três níveis e eventos, instância independente, Entrada
condicional, caminho CrossFade, primeira interrupção, continuidade repetida,
saída de grupos, transição autorada, fila do destino/gatilho único, destino
interrompido, Exit de três níveis, Any global e isolamento/restauração.
Maior passo observado na segunda execução foi 1,27° dentro da janela medida
pela sonda; isso é continuidade do cenário, não FPS sustentado.

Autoria por toque: abrir três grupos, política→destino/origem, Undo/Redo,
offset→0,20; criar grupo/Entrada, mover para Ciclo; duplicar, excluir,
Undo restaurar e Redo remover cópia. Salvar, encerrar processo e reabrir
preserva grupo original e configuração. Controller revision 14, GUID
282bb1ab8c19598d3e9d3b62adecc117, quatro grupos/quatro estados/oito ligações.
Grupo 55 possui pai 43/default 56; ligação 48 scope 44, interruption 4,
ordered 1, self 0, offset 0.200000003, fixed 1. Allocator next 59 conserva
IDs excluídos. Recurso salvo e puxado após Play têm o mesmo SHA-256:
`608d5f30f19d0678652ca4aadf4e9788e8cce7fd651937d57f7f23e53065c021`.

Capturas reais demonstram viewport útil, identidade angular, breadcrumbs
legíveis, hierarquia/contexto, condições e campos efetivos sem textos longos.
O layout só oferece propriedades utilizadas pelo modelo/runtime.

## Todos os frames revisados

Vídeo device-play.mp4: SHA-256
`f6ed6e43613f1ee5fe66d27c5cf31edd9b33263b5863e44fa82dcd293fdd986c`.
1920×886, **598/598** frames decodificados/extraídos, zero descartados,
último PTS 9.931411s. **Todas as 20 folhas** foram inspecionadas, contendo
cada quadro, além de frames completos. O ROI final 450,230,1400,740 inclui
ambos os mecanismos em todas as poses; a primeira extração estreita não é
a evidência final. FFmpeg avisou sobre DTS na extração, mas contagem/PTS
conferem integralmente; nenhum quadro foi omitido.

A permanece contínuo nos trechos CrossFade e interrupções repetidas, com
mudança de inclinação/retorno sem salto de pose. B permanece fechado e
independente. Play imediato produz os degraus intencionais da sonda; não são
fades. O perfil fino ao passar por 90° é a geometria do painel. Região e
viewport permanecem estáveis. Parte final mantém ambos fechados sem oscilação.
Não afirma desempenho de multidões, FPS sustentado ou qualidade de caminhada.
Skin/morph têm evidência host; o cenário físico é mecanismo sem skin.

## Referências e limites

- Unity 6000.0: https://docs.unity3d.com/6000.0/Documentation/Manual/NestedStateMachines.html
- Unity 6000.0: https://docs.unity3d.com/6000.0/Documentation/Manual/StateMachineTransitions.html
- Unity 6000.0: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transition.html
- Godot 4.5-stable: https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/animation_node_state_machine.cpp
- Tutorial oficial Unity 5: https://www.youtube.com/watch?v=lpekqN4_4xg;
  transcrição estudada e grafo real em 2:33 inspecionado. Não foi analisado
  integralmente quadro a quadro; não confundir referência com nosso aceite.

Adaptamos hierarquia, prioridade e arbitragem ao compositor e à navegação
mobile existente. Não copiamos arquitetura/código de um pacote para criar
wrappers sem consumidor. Limites: 32 parâmetros, quatro camadas, 24 estados,
48 ligações/32 máquinas por camada, profundidade 16, nome 63 bytes UTF-8,
oito movimentos/estado, quatro condições/ligação.

Grupo vazio pode ser editado e é recusado em execução. APK antigo não lê v5/3;
guardar backup. Root motion, editor completo de clipes/curvas/poses, retargeting,
IK, autoria SDK e biblioteca FBX/Unity YAML seguem B4–B6. Pacotes inventariados:
783 arquivos/75 páginas/788 seções/317 propriedades; assets importados: zero.
O inventário de pesquisa não comprova biblioteca utilizável ou paridade.

Evidência reproduzível: `aether_ui_preview write-animator-hierarchy-project`;
script de revisão de frames aceita tamanho/ROI explícitos e conserva PTS/hash.
Logs, capturas, controller autorado, vídeo e 20 folhas acompanham este relatório.

## Revisão final instalada

APK final: 103881574 bytes, SHA-256 local/instalado idêntico
`ad652a801fb654da5892b06ede4ff4252352cdb588923a4db3ce08cf41cc2e70`.
Build incremental concluído em 1m25s. Os 15 checks passaram novamente no APK
final (PID 30745), maior passo na janela 1,30°. A UI foi conferida após update:
criar uma segunda ligação Meia→Aberta gera dois trajetos; tocar no inferior
seleciona a ligação original (Destino→origem, 1s, offset 0,20), tocar no
superior seleciona a nova (só Qualquer estado, 0,25s, offset 0). Capturas
device-parallel-original/new mostram seleção e propriedades independentes.

A ligação temporária foi desfeita. O teste também criou/desfez um self-link
ao conferir o fluxo Ligar→origem→destino. Recurso final preserva as oito
ligações/quatro grupos/quatro estados, com revisão monotônica 18/nextId 61;
SHA-256 `0ed72a264ae13732705b1e5ccfb3a8449af8c3be2d2203077da2bed917da4bb7`.
A diferença em relação à revisão 14 é revisão/allocator do histórico,
sem alteração final da configuração da animação. Não apagar isso para
simular um arquivo nunca editado. Projeto permanece disponível no Dev.
