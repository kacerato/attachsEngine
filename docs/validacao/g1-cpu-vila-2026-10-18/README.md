# G1 · CPU da Vila: animador sem alocação por canal (18/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, G1, segundo alvo (CPU
17–24 ms na linha de base). POCO F7, Astra Dev `dev.aether.editor.u07`, Vila Astra
em Play (11 personagens com Animator), carregador ligado.

## 1. Perfil (simpleperf, APK profileable)

`baseline/` (plano, 20 s) e `callgraph/` (pilha por frame pointer, 15 s):

- **98% da CPU do app numa única thread** (o laço principal). Essa thread aparece com
  595 nomes "Thread-N" em 20 s: ela se anexa e se desanexa da JVM uma vez por quadro.
  Ainda não corrigido; registrado como próximo alvo.
- **Alocação ~31% da CPU** (CAS atômico 11,8%, mutex do scudo 6,9%, alloc/free).
- **`SceneAnimator::advance` = 51% inclusivo**: para cada canal de cada personagem,
  um `std::vector<float>` novo, um nó de `unordered_map`, e 6–8 vetores temporários por
  propriedade na composição — da ordem de 15 mil alocações por quadro.
- Outros: `EditorMapScene::extract` 9,9% (`deformedPose` 8,1%), `worldMatrix` 5–6,7%,
  `resolveSceneCamera` 3,2–4,3%, `collectSubtree` 3,9%.

## 2. Mudança (`native/runtime/scene_animation.cpp`)

Mesma lógica de amostragem e mistura; só os contêineres mudaram:

- Valores de canal num vetor pequeno com 4 floats em linha (`SmallVector<float,4>`);
  só pesos de blend shape longos usam o heap.
- Rascunho preso ao `SceneAnimator` (`AdvanceScratch`) e reutilizado entre quadros:
  lista de vivos, propriedades (com a capacidade das listas de contribuições), índice
  de propriedades, poses e ligações externas. Os índices são hash de endereçamento
  aberto (`FlatIndex`), limpos sem liberar memória.
- `externalBindings_` troca de buffer com o rascunho em vez de ser recriado.

Ainda alocam por quadro: as poses de camada publicadas (`LayerPose`, compartilhadas com
o Animator) e os caminhos aditivos.

Testes: `aether_animation_runtime_tests` **84/84** (animator, esqueleto, IK, root motion,
locomoção). Clang Android sem avisos.

## 3. Medição

O FPS da Vila é limitado pela GPU (~30) e o clock da CPU variou entre rodadas, então
ms de CPU por quadro não separam o efeito (`ab/`: pares de mesmo clock −1,9%, par
quente invertido). A medida usada é **instruções e ciclos de usuário por quadro**
(`simpleperf stat` durante a janela de FPS; `tools/measure-cpu-instructions.ps1`),
A/B/B/A alternando APKs de mesmo versionCode (A = 0.3.0-dev.100-g1c,
B = 0.3.0-dev.100-g1d, SHA-256 `809E1408138BE195A7F1C8B4A35BCD5BBE453DDFDB6617A94F9D47DCC2F12163`):

| Rodada | FPS | Instruções/quadro | Ciclos/quadro |
|---|---|---|---|
| 1 A | 20,5 | 32,65 M | 17,52 M |
| 2 B | 20,5 | 22,50 M | 12,85 M |
| 3 B | 20,2 | 23,01 M | 12,85 M |
| 4 A | 20,3 | 33,13 M | 17,82 M |

**Instruções −30,8%, ciclos −27,3% por quadro.** Pelo perfil do B (`callgraph-B/`),
`SceneAnimator::advance` caiu de 51% para 30% da CPU e malloc/scudo saiu do topo.

Comportamento no aparelho: `play-B-dois-quadros.png` mostra os moradores em poses de
caminhada e em movimento entre dois quadros a 3 s de distância.

## 4. Limites e próximos alvos

- O ganho é de CPU; o FPS desta vista continua limitado pela GPU (opaco ~15 ms). O ganho
  aparece como folga de CPU, menor aquecimento e mais espaço para o nível C, onde a CPU
  também é mais fraca.
- Próximos na CPU: anexar/desanexar JNI por quadro; `deformedPose`/`extract`;
  `worldMatrix` sem cache por quadro; `resolveSceneCamera` e `collectSubtree` por quadro.

## 5. Segunda rodada: anexação JNI por quadro e `worldMatrix` com cache

- **JNI:** `updateAndroidAudioFocus` rodava todo quadro e anexava/desanexava a thread
  do jogo para duas chamadas Java (cada anexação cria um objeto `Thread` e lixo para o
  GC; daí os 595 nomes). A thread do `android_main` agora fica anexada do início ao fim
  (guarda RAII declarado antes do shell, nome "AstraGame"), e `setPlayAudioDemand` só
  cruza o JNI quando a demanda muda. O estado do foco continua lido por quadro.
- **`worldMatrix`:** subia até a raiz convertendo Euler -> matriz em cada ancestral, para
  cada osso de cada personagem. Agora memoriza a matriz de mundo por objeto num cache
  `thread_local` chaveado por (identidade do grafo, revisão). O `SceneGraph` ganhou uma
  identidade única por instância, renovada em cópia, atribuição e `reset()`, para que
  duas cópias com a mesma revisão nunca compartilhem entradas. Qualquer mutação aceita
  sobe a revisão e invalida o cache.

A/B/B/A por instruções (A = g1d, só o animador; B = 0.3.0-dev.100-g1e):

| Rodada | FPS | Instruções/quadro | Ciclos/quadro |
|---|---|---|---|
| 1 A | 34,8 | 22,00 M | 11,65 M |
| 2 B | 30,5 | 19,91 M | 7,56 M |
| 3 B | 30,5 | 19,76 M | 10,51 M |
| 4 A | 30,0 | 22,62 M | 11,92 M |

**Instruções −11,1%** (ciclos variaram mais). Acumulado do dia na Vila:
**32,9 M -> 19,8 M instruções por quadro (−40%)**.

Perfil do B (`callgraph-C/`): uma thread só, "AstraGame" (97,3%); `deformedPose`
12,2% -> 3,9%; `extract` 15% -> 6,8%; `worldMatrix` fora do topo; 4.860 amostras em 15 s
contra 8.367 no início (mesmo FPS, limitado pela GPU). Restam no topo:
`SceneAnimator::advance` 37%, `resolveSceneCamera` 7% e `collectSubtree` 6% por quadro.

Testes de host após a mudança: `aether_animation_runtime_tests` 84/84 e `aether_tests`
1574/1574. Imagem: `play-A01-B02-jni-world.png` (cena e moradores corretos em B).
