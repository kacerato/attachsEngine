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
