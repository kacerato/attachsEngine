# Pendências do G1b e do G3c, e giro do Personagem (26/10/2026)

Branch `claude/lucid-jang-4a0b85`, base `a06d4e30` (codex/ui-play-performance-apk-export).
APKs Astra Dev (versionCode 261026999 em todos, regra de 27/10): `pend-1141` (medições A),
`pend-1446`/`pend-0856` (giro), `pend-0925`/`pend-1052`/`pend-1213` (bordas, emenda e lightmap),
conferidos com `dumpsys` no início de cada rodada. POCO F7, rodadas sozinhas com a guarda de
ligação/primeiro plano e devolução da Vila (2e2e9968) e da Sponza (91ce9ac9, cache cbd58d30)
conferidas por md5.

## A1. Sponza S e C, antes × depois dos cortes de 23–25/10

Roteiro do G0 (`tools/measure-feature-ab.ps1`, Edit na pose fixa -6,101; 5,220; -8,918; yaw 0,6;
pitch 0,45), A/B/A/B/A no mesmo binário: antes = `-NoQualityCeiling` (sem teto por nível, sem
máscara de luzes, AO cheio, bloom antigo), depois = `-ProductDefaults`. 40 s por rodada, no
carregador, bateria 39,5–41,9 °C. Pastas `sponza-s/` e `sponza-c/`.

| Nível | Configuração | FPS interno | GPU quadro (ms) | Opaco | Pós |
|---|---|---|---|---|---|
| S | antes (3 rodadas) | 58,33 | 14,63 | 9,94 | 4,57 |
| S | depois (2) | 59,35 | 14,32 | 9,72 | 4,48 |
| C | antes (3) | 59,78 | 14,02 | 7,51 | 6,35 |
| C | depois (2) | 56,46 | 14,40 | 8,07 | 6,16 |

- **S:** o teto do nível S não corta nada (`teto=on` com os mesmos eixos); as duas linhas são a
  mesma configuração e diferem no ruído (0,3 ms).
- **C:** o teto desliga o AO e passa o bloom para a cadeia de mips (`ao=off ao_meia=1
  bloom_cadeia=1`, 4 luzes por objeto). O pós cai 0,2 ms; o total não muda. A Sponza fica presa em
  60 FPS (a GPU trabalha abaixo do vsync e o clock cai), não tem luz pontual nem sombra do sol na
  pose, e a rodada 4 saiu fora (54 FPS, GPU 15,1). **Os cortes não têm custo a tirar na Sponza;
  o ganho deles é na Vila** (C: GPU 18,91 → 14,33 ms, `2026-10-23-cortes`).

## A2. Resolução dinâmica no nível C

Modelo: Dynamic Resolution da Unity 6000.0 (escala entre mínimo e máximo pelo tempo de GPU,
`FrameTimingManager`, latência de quadros) —
https://docs.unity3d.com/6000.0/Documentation/Manual/DynamicResolution-landing.html ·
https://docs.unity3d.com/6000.0/Documentation/Manual/frame-timing-manager.html

O controlador (`native/renderer/dynamic_resolution.cpp`) já existia e já vinha ligado no nível C
(piso 0,55, teto = escala do nível, 0,85); as medições anteriores o desligavam. Mudou:

- decide pela **mediana** das últimas `overloadFrames` amostras de GPU: um pico isolado não
  derruba a escala, e a GPU oscilando em volta do orçamento desce (antes, cada quadro abaixo
  zerava a contagem de quadros seguidos e a escala nunca descia);
- **descarta 3 amostras depois de cada troca**: o tempo lido ainda é da escala velha;
- **degrau proporcional** ao excesso (custo ~ área ~ escala²), entre o degrau mínimo e 0,15;
- histerese igual: desce acima do orçamento, sobe só abaixo de 0,72 × orçamento por 120 quadros;
- o log `[DynamicResolution]` traz a mediana que decidiu a troca.

Teste de host (`tests/native/test_dynamic_resolution.cpp`, 8/8): pico isolado, oscilação no
limite, amostras da escala velha e degrau proporcional — os três últimos falham no controlador
anterior (5/8).

**Vila C no painel inteiro** (`-NativePanel`: com a referência 1600×720 a escala já sai 0,57 e a
faixa dinâmica [0,55; 0,57] não tem onde trabalhar). Play, A/B/A/B/A, 40 s, pasta
`vila-c-dinamica/`, linha do tempo em `vila-c-dinamica/escala.json`.

| Configuração | FPS interno | GPU quadro (ms) | Opaco | Pós | CPU processo |
|---|---|---|---|---|---|
| Escala travada 0,85 (3 rodadas) | 36,63 | 24,66 | 15,58 | 7,79 | 14,21 |
| **Dinâmica 0,55–0,85** (2) | **55,37** | **16,08** | 8,10 | 6,69 | 10,40 |

Trocas nas duas rodadas dinâmicas (idênticas): 0,85 → 0,78 (mediana 16,4 ms, orçamento 14,67) →
0,73 (15,6) → 0,68 (15,4) → 0,55 (21,5), em ~1 s, e fica no piso sem oscilar: mesmo a 0,55 o
quadro custa 16 ms (o pós escala pouco com a resolução). A última decisão viu 21,5 ms a 0,68, acima
do que a área prevê (~16 ms): as primeiras amostras depois do redimensionamento custam mais. O piso
seria alcançado de qualquer modo; fica anotado para quando a Vila couber no orçamento acima do
piso. Capturas: `vila-c-travada-085.jpg`, `vila-c-dinamica-055.jpg`.

## B. Lightmap: tempo real × assado à noite

Pendência do G3c: à mesma exposição o assado saía com ~metade da luminância do tempo real
(Sponza às 21 h, 7 lanternas Assadas com sombra + 1 Mista, EV100 fixo −6). Variantes geradas por
`g3c_lightmap_scene_author` (commits cb73f46a, 1473cf2b), assadas **no POCO** pelo editor
(Iluminação › Lightmaps › Gerar, ~2 min). Luminância linear média da vista 3D (`lightmap/`):

| Variante | Tempo real (sem lightmap) | Assado | Assado / tempo real |
|---|---|---|---|
| V2 — ambiente normal | 0,0917 | 0,0574 | **0,63** (a "metade" do G3c) |
| V0 — ambiente zerado (`ambiente=0`) | 0,0703 | 0,0534 | **0,76** |

**O céu explica só uma parte.** Sem céu a razão sobe de 0,63 para 0,76; o resto não é do céu.

**A referência do host estava errada, não o aparelho.** O A/B do G3c dizia "assado = 2,29× o
tempo real sem céu" (`g3c_lightmap_host_bake`). A ferramenta importava a fonte em pasta sem
nenhuma textura — as imagens chegam por `GltfImportProgress::externalFile` e ela respondia
falso — e assava com albedo médio **0,94** (`lightmap/host-v0-sem-textura.txt`): rebotes com o
dobro da energia. Corrigida (c572a643, mesmo leitor do editor): 24/24 texturas, albedo **0,21**, e
o lightmap do host **bate com o do POCO** (RGB médio 0,000406 × 0,000402; pico 0,0614 × 0,0617;
`lightmap/host-v0-albedo-real.txt`). Formato (RGBA8 linear com raiz e faixa por página),
decodificação e unidades das luzes conferem; o bake do aparelho está certo.

Paridade do host com o albedo real (vista "G3c arcada norte"): assado / difuso de tempo real com
sombra = **1,29**; com o especular GGX das luzes = **1,07**; difuso Burley do shader = 0,85 × Lambert.
Como na Unity, **o lightmap não guarda o especular direto nem o difuso Burley** (guarda irradiância
Lambert + rebote).

**O que falta para o 0,76 do aparelho: sombra de tempo real.** O atlas de sombra local tem
`MaximumLocalShadowTiles = 16` e uma pontual usa 6 (uma por face do cubo): em tempo real só **2 das
8** lanternas fazem sombra; a luz das outras 6 atravessa paredes e colunas. O assado faz a sombra
das 8. A rodada V3 compara só a energia: ambiente zerado, lanternas **sem sombra dos dois lados**
(`sombras=0`) e o tempo real **sem o especular direto** (`aether.gpu_isolation 6`, novo
`GpuCostIsolation::NoDirectSpecular`, 1473cf2b).

| V3 — sem céu, lanternas sem sombra dos dois lados, tempo real sem especular direto | 0,0590 | 0,0620 | **1,05** |

**Fechado: com o mesmo tratamento de sombra dos dois lados e sem o especular que o lightmap não
guarda, o assado fica 5% acima do tempo real**, como o host prevê (1,07) e como a Unity se comporta.
A "metade" do G3c somava três coisas que não são defeito do bake: o céu sem oclusão no tempo real
sem lightmap, o especular direto e a luz de 6 lanternas sem sombra no tempo real. Prancha com a
Unity (vídeo KJ4fl-KBDR8, 1:00 antes do bake e 0:00 assado):
`lightmap/prancha-sponza-tempo-real-x-assado.jpg`.

Limite que fica registrado (não é do lightmap): com 16 tiles no atlas local, só 2 pontuais por
quadro fazem sombra em tempo real; cenas com muitas lanternas em Tempo real deixam luz atravessar
parede. Em Assado as 8 fazem sombra no lightmap.

**Unity como referência de comportamento**: com Baked Global Illumination, objeto Static com
lightmap recebe o ambiente pelo lightmap (céu × visibilidade assada) mesmo com luzes Realtime; a
Astra já faz isso (`dirt_road_shading.glsl`, `astraLightmapActive`). Sem lightmap nenhum, o
tempo real da Unity também vê o céu sem oclusão. Nada a mudar no código por causa do céu.

**Privacidade.** Duas capturas da variante V1 saíram com a cortina de notificações do dono por
cima (mensagens, nomes e um número de telefone) e foram apagadas sem entrar no repositório. A
guarda das rodadas passou a tratar a cortina aberta como aparelho ocupado.

## C. Personagem: girar para a direção do movimento

Referência: Unity Asset Store, **First Person + Third Person | Character Controllers** (Starter
Assets) 2.0.1, 17/09/2026, URP 6000.0/6000.3 —
https://assetstore.unity.com/packages/essentials/starter-assets-thirdperson-updates-in-new-charactercontroller-pa-196526 ·
`ThirdPersonController.RotationSmoothTime` (0,12 s): o alvo é `atan2(comando) + yaw da câmera` e o
corpo chega lá por `Mathf.SmoothDampAngle`. A classe da Unity não foi copiada; o comportamento é
estudado e refeito sobre o motor da Astra.

- **Personagem v7**, grupo **Direção**: *Girar para a direção do movimento* (padrão desligado nos
  componentes avulsos), *Velocidade do giro* (720 °/s), *Suavização do giro* (0,12 s) — os dois
  só aparecem com o giro ligado — e *Relativo à câmera* (ligado: frente é para onde a câmera olha;
  desligado: frente é +Z do mundo). Cenas v6 abrem sem o giro, como antes.
- Runtime (`native/physics/character_facing.h`, `runtime/scene_physics.cpp`): depois de cada passo
  fixo do motor, com comando, o yaw do corpo segue o alvo pelo caminho curto; parado (ou girado por
  script) a direção volta a ser a do transform. No modo Dirigir o giro continua sendo o do veículo.
- Ligado nas receitas de player: modelo de cena *Personagem jogável* e *Converter em Personagem*.
- API C#: `Character.FaceMovement`, `CameraRelative`, `FaceTurnSpeed`,
  `RotationSmoothTime` (`--sync-api`).

Testes de host (`aether_animation_runtime_tests` 14/14, `aether_tests` character 34/34,
`aether_animation_clip_editor_tests` modelos 1/1): primeiro passo suavizado (2,7°, sem salto),
direção relativa à câmera, mantém a direção ao soltar, cruza 180° pelo caminho curto, segue um
círculo a 90 °/s com atraso < 20°, desligado não gira, sem câmera anda em +Z, leitura v6/v7, e o
modelo *Personagem jogável* cria o Personagem com o giro ligado.

**Aparelho** (APK pend-0856, projeto "Giro do Personagem" gerado por
`aether_gui_preview write-playable-facing`, aberto em Play por opção de lançamento; joystick
flutuante em círculo por `input motionevent`): `giro/giro-circulo.mp4`. A marca escura na cabeça
(a frente do modelo) dá a volta enquanto o corpo anda: de costas, de lado, de frente para a
câmera e do outro lado. Prancha com a Unity (Starter Assets overview, Unity, 8:15 e 8:20 —
https://www.youtube.com/watch?v=4QuPlKzdq14&t=495s): `prancha-giro-unity-x-poco.jpg`.
Inspector no aparelho: `giro/inspector-direcao.jpg` (grupo Direção com o giro ligado pela receita,
a seta de restaurar e os dois campos que só aparecem com ele ligado).

Observação da captura: com a câmera seguidora que olha o alvo, o círculo do joystick vira espiral
(o "frente" relativo à câmera gira quando o personagem anda de lado); nas duas primeiras rodadas,
com 4 m/s e voltas de 7 s, o personagem saiu do chão. A terceira usou meio curso (~2 m/s) e voltas
de 3 s.

## D. Bordas brancas e emenda sob a barra do editor (achado do dono, 27/10)

O dono viu bordas brancas na Sponza. Duas causas distintas, as duas corrigidas:

1. **Alvos em 1/2 lidos fora do retângulo escrito** (3cc15717). Com a resolução abaixo do teto
   (dinâmica ou escala do painel), bloom, DoF e AO em 1/2 só escrevem o canto `ceil(interna/2)`;
   o pós prendia a leitura com meio texel da cena (¼ do texel do alvo) e puxava o texel não escrito
   na última linha e coluna, e o bloom somava uma faixa clara. Agora `clampHalfUv` prende ao
   retângulo escrito. Teste de host: a borda não muda com a escala (o limite antigo dava >1,5×).
   No nível automático do POCO a dinâmica vem desligada, por isso não apareceu nas rodadas.
2. **Emenda sob a barra do editor** (3f308cab; a0ac3049 e f61868ce foram tentativas que a
   medição derrubou). A borda da vista caía em fração de pixel depois da rotação da superfície; o
   fundo da câmera e a geometria cobriam linhas diferentes e a linha logo abaixo da barra saía com
   o fundo misturado — escura de dia, branca com o céu da noite. Agora `physicalSceneViewport`
   arredonda para pixels inteiros, viewport e scissor usam os mesmos inteiros, e o clamp do pós
   fica como defesa. POCO, Sponza, linha 101 (logo abaixo da barra) × 102:

   | APK | Play | Editor |
   |---|---|---|
   | pend-0856 (antes) | 53,62,81 × 93,106,127 | 57,66,82 × 95,108,129 |
   | pend-1052 (a0ac3049) | 7,7,23 × 94,107,127 | 12,14,18 × 96,109,130 |
   | **pend-1213 (pixels inteiros)** | **95,108,128 × 95,108,129** | **97,110,131 × 97,111,132** |

   `[ViewportEdge]` (`emenda/viewport-edge-log.txt`): bordas inteiras em texels (Play
   `x=[0,1179]`; editor `x=[47,1179] y=[677,2012]`). Prancha: `emenda/emenda-antes-depois.jpg`.

O traço claro curvo no canto superior esquerdo das capturas **não é da engine**: é a alça
flutuante do Game Turbo da Xiaomi (`dumpsys window`: duas janelas
`com.miui.securitycenter/.FloatingWindow` sobre a Astra), que aparece porque o manifesto declara
`appCategory="game"` (usado pelo Game Mode). Some desligando o Game Turbo para o app no aparelho.

## E. Suíte e entrega

- Host em 26/10 (depois do giro): `aether_tests` 1724/1724 e as 24 famílias de teste verdes;
  gerenciada 523 passaram, 0 falharam, 71 pulados (`dotnet` Release).
- Depois disso mudaram o pós (shader + `post_sample_bounds.h`), o renderer Android, o isolamento 6
  e as ferramentas do G3c: testes de host afetados verdes (`test_dynamic_resolution.cpp` 11/11 em
  compilação isolada, `test_normal_matrix.cpp` 5/5); SPIR-V `-All -Check` limpo; APK arm64 sem
  aviso.
- Commits: 16034207, cb73f46a, 706d8e5d, 7d37800e, 3cc15717, 0aa62bdf, f61868ce, a0ac3049,
  3f308cab, 1473cf2b, c572a643.
- Pronto para mesclar na base quando o dono pedir.
