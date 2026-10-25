# Desempenho + gráficos vivos — plano integrado (para análise)

Data: 17/10/2026. Estado: **ordem aprovada pelo proprietário em 17/10/2026; G0 e G5 em execução**. Nada deste
documento está implementado; cada bloco declara o que já existe no código e o que
seria novo. Ao aprovar, cada capacidade nova entra em `core/engine_capability.h`
como `Planned` e só vira `Implemented` com consumidor, teste e captura no aparelho.

## 1. Resumo em uma página

O pedido tem dois objetivos que competem pelo mesmo orçamento de GPU: o celular
fraco precisa ficar mais rápido e a imagem precisa ganhar vida (dia/noite, nuvens,
clima, partículas, VFX, pós melhor, menos aspecto “plástico brilhoso”). Por isso
o plano não trata os dois em separado. **Toda melhoria visual chega com uma
variante para cada nível de aparelho (C/B/A/S) e um custo medido em ms; o trabalho
de desempenho vem primeiro, para liberar o orçamento que os efeitos vão gastar.**

Os nove blocos, na ordem proposta:

| # | Bloco | Ganho principal | Tamanho |
|---|---|---|---|
| G0 | Bancada no POCO F7, modo nível C forçado e cenas de aceite | Medir antes de mudar | P |
| GT | Telemetria de desempenho dos testadores (opt-in) | Dados reais de aparelhos fracos | M |
| G1 | Desempenho estrutural (CPU + GPU) | FPS no aparelho fraco | G |
| G2 | Pós-processamento v2 (pilha por passes, LUT, bloom real) | Imagem melhor **e** mais barata | M |
| G3 | Luz natural: fim do “brilhoso”, sondas, lightmap | Realismo de iluminação | G |
| G4 | Dia/noite, céu, nuvens e clima (estudo UniStorm) | Mundo vivo | G |
| G5 | Sistema de partículas e VFX (estudo Shuriken/Hovl) | Efeitos, magia, chuva | G |
| G6 | Importar o RPG VFX Bundle e o clima do UniStorm | Conteúdo pronto | M |
| G7 | API C#/Flow, editor e publicação | Universal pelo script | M (contínuo) |

G7 não é uma etapa final: cada bloco já entrega componente serializado + fachada
C# gerada + nó Flow quando houver evento. G7 lista o contrato comum.

## 2. Diagnóstico do que existe hoje

Levantado no código em 17/10/2026, não em memória de sessões anteriores.

**Já existe e funciona (não refazer):**

- Renderer forward Vulkan em `platform/android/instanced_renderer.cpp` (7.037 linhas),
  culling/compactação em GPU, HZB opcional, LOD de pacote e LOD autoral, impostores,
  skinning em compute, sombras do sol em cascatas e atlas de sombras de luzes locais
  (8 luzes por quadro), streaming de texturas, ASTC.
- Ambiente (`scene/environment.h`, versão 12): céu HDRI / atmosfera simples /
  atmosfera física Rayleigh-Mie, neblina por altura, exposição automática por
  histograma, volumes de ambiente globais/caixa/esfera com camadas e perfil.
- Pós em **um único shader** (`rhi/shaders/post_process_common.glsl`): neblina, SSAO,
  FXAA, “bloom”, tonemap Reinhard/ACES/AgX, contraste/saturação, vinheta, grão,
  TAA, FSR1, Arm ASR / FSR2.
- Política de qualidade C/B/A/S por aparelho (`rhi/device_profile.h`,
  `renderer/rendering_policy.h`), resolução dinâmica, controlador térmico.
- Importação de `.unitypackage` com YAML, prefab (só GameObject/Transform/
  MeshFilter/MeshRenderer) e material (só o Standard da Unity).
- Gradientes e curvas de script (`scene/script_gradient.h`), Flow, fachadas C#
  geradas (`managed/Astra.Scripting/Generated/Components.g.cs`).

**O que explica a reclamação, com causa no código:**

| Sintoma | Causa provável observada | Bloco |
|---|---|---|
| Lento no celular fraco | Pós monolítico em resolução cheia: SSAO de 12 amostras + 8 amostras de bloom + FXAA no mesmo pixel; céu físico faz raymarch por pixel; Vila Astra mediu CPU 24,9 ms **e** GPU 32,5 ms num aparelho topo de linha (POCO F7), então o fraco sofre nos dois lados | G1, G2 |
| “Tudo brilhoso” | Reflexo especular do ambiente chega em todo lugar: não há oclusão especular, sonda de reflexão local nem sonda de irradiância (`render.probe.*` = `Planned`); materiais importados sem rugosidade coerente; sem color grading | G3, G2 |
| Sem dia/noite | Não há componente de hora do dia; sol, céu e ambiente são valores fixos; não há lua, estrelas nem transição | G4 |
| Sem vida | Não há sistema de partículas, rastro, linha, decal, nuvem, clima ou vento global (nenhum arquivo de `native/` implementa partícula) | G4, G5 |
| Bloom fraco | O “bloom” é uma vizinhança de 8 amostras a 2 px: não espalha luz, só borra o ponto | G2 |
| Sem GI como nos vídeos | Lightmap aceita textura externa (`render.gi.lightmap` limitado), mas não há bake, unwrap nem sondas | G3 |

## 3. O que os vídeos mostram e o que a Astra adota

| Vídeo | Conteúdo | Decisão para a Astra |
|---|---|---|
| [VoxelGI vs LightMapGI (LoopDev)](https://www.youtube.com/watch?v=FAk3MLd8YB4) | Godot 4: GI por voxel em tempo real comparada com lightmap pré-calculado | **Lightmap + sondas é o caminho para celular.** A documentação da Godot restringe VoxelGI ao renderer Forward+ (desktop) e recomenda LightmapGI para hardware fraco e mobile ([GI na Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/introduction_to_global_illumination.html), [LightmapGI](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html), [VoxelGI](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_voxel_gi.html)). G3 entrega bake de lightmap e sondas. |
| [VoxelGI dynamic reflection (tutor tomato)](https://youtu.be/Le0AExwsOLA) | Reflexos e luz indireta reagindo a objetos e luzes que se movem | Voxel em tempo real não cabe em C/B. Equivalente viável: **sondas de reflexão com atualização amortizada** (uma face de cubo por quadro) e **volume de sondas de irradiância reiluminado no tempo** em A/S. Fica em G3b, depois do bake. |
| [262.000× mais rápido que Minecraft (IGoByLotsOfNames)](https://youtu.be/CJ94gOzKqsM) | Otimização de engine voxel: dados de vértice compactados, malhas mescladas, menos draws, culling agressivo | Princípios aplicados em G1: **compactar vértices** (posição 16 bits quantizada, normal/tangente octaedral, UV 16 bits), mesclar estáticos por material, medir banda de vértices separada de pixels. Não adotar voxels como formato de mundo. |
| [LPVGI vs SDFGI vs VoxelGI na Godot 4.6 (NIGHTCALL)](https://youtu.be/4q4v4BNtY00) | Três técnicas de GI dinâmica em Sponza/Bistro | SDFGI e VoxelGI são de desktop. LPV (light propagation volumes) é a mais barata das três; **não encontrei LPVGI na documentação oficial da Godot**, então é tratada como pesquisa, não como referência de capacidade. Candidata a GI dinâmica de nível S depois de G3b, sujeita a medição. |

Conclusão: o visual dos vídeos vem de **luz indireta + sombra de contato + boa
exposição + pós bem calibrado**, não do algoritmo de voxel em si. No celular
isso se obtém com bake + sondas + pós eficiente, e GI dinâmica só no nível S.

## 4. Análise do UniStorm (clima, céu e nuvens)

Pacote extraído e lido: 437 arquivos, 119 C#, 39 shaders, 30 áudios, 46 perfis.
Feito para o pipeline Built-in da Unity (CommandBuffer, `OnRenderImage`) e inclui
uma cópia do Post Processing Stack v1 da Unity.

**O que ele faz e como (o que vale estudar):**

- **Hora do dia** (`UniStormSystem.cs`, 3.662 linhas): hora/minuto, duração do dia e
  da noite separadas, data/estação, ângulo do sol por latitude, fases da lua.
  Quase toda a aparência é **gradiente ou curva indexada pela hora**: cor do sol,
  da lua, do céu, ambiente céu/equador/chão, neblina, nuvem, raios de sol, estrelas,
  com uma segunda versão “tempestade” de cada gradiente.
- **Tipos de clima** (`WeatherType`, 30 perfis): nuvem (perfil), intensidade de
  sol/lua/sombra de nuvem, neblina, vento, efeito de partícula + quantidade, som,
  raios, aurora, temperatura. Transição gradual entre tipos.
- **Nuvens** (`UniStormClouds.cs`, `cloudsInclude.cginc`): renderizadas num
  **domo em textura de tamanho fixo** (1024 ou 2048), não na tela. A cada quadro só
  **1/16 dos texels** é recalculado (¼ × ¼ de resolução, deslocamento Bayer 4×4 +
  Halton) e misturado com o buffer anterior. Raymarch de 20 a 100 passos, conforme
  a qualidade, e 5 de autossombra. Modo 2D barato. **O custo não depende da
  resolução da tela**, por isso é a técnica certa para celular.
- **Sombra de nuvem**: textura 256² gerada do ruído (modo simulado) e projetada no
  mundo.
- Raios (LineRenderer + luz piscando), aurora, estrelas por mapa, sun shafts e
  neblina atmosférica como pós.

**O que a Astra não copia:** o código (licença do Asset Store, ver §11), a
arquitetura de singleton/MonoBehaviour, o Post Stack v1 embutido e os efeitos de
pós próprios dele (a Astra já tem neblina e terá sun shafts no G2).

**O que a Astra cria a partir desse estudo:** componentes `TimeOfDay`, `Weather`,
recursos `WeatherProfile`/`CloudProfile`/`SkyGradientSet`, nuvens amortizadas em
domo, sombra de nuvem, estrelas/lua, raio, vento global. Detalhe em G4.

## 5. Análise do RPG VFX Bundle 5.0 (Hovl Studio)

Pacote extraído e varrido por script: 746 arquivos, 217 prefabs, **1.187
sistemas de partícula** (5,5 por prefab, máximo 21), 211 materiais, 12 shaders,
30 FBX de malha, 402 texturas (68 MB). Pacotes: Projéteis AAA, magias de área,
círculos mágicos, lasers 3D e marcadores de mapa.

**Módulos do Shuriken realmente ligados** (base da ordem de implementação em G5):

| Módulo | Sistemas | % | Módulo | Sistemas | % |
|---|---|---|---|---|---|
| Main + Emission | 1.187 | 100 | Collision | 113 | 9,5 |
| Color over Lifetime | 1.030 | 87 | Trails | 102 | 8,6 |
| Size over Lifetime | 809 | 68 | Lights | 71 | 6,0 |
| Shape | 799 | 67 | Sub Emitters | 56 | 4,7 |
| Limit Velocity | 232 | 20 | Inherit Velocity | 56 | 4,7 |
| Force over Lifetime | 212 | 18 | Custom Data | 43 | 3,6 |
| Rotation over Lifetime | 167 | 14 | Noise | 25 | 2,1 |
| Velocity over Lifetime | 141 | 12 | Size/Color by Speed, External Forces | 4 | 0,3 |
| Texture Sheet (UV) | 126 | 11 | | | |

- Renderização: billboard 701, malha 237, esticado 165, sem render (só emissor) 84.
- Formas: cone 457, esfera 354, caixa 211, círculo 64, hemisfério 51, volume de
  cone 35, aresta 12, malha 2, rosca 1.
- Espaço: local 737, mundo 450. Máximo de partículas: mediana 1.000, máximo 16.000.
- Outros componentes: 72 Light, 11 LineRenderer, 14 AudioSource, 27 Rigidbody,
  colisores 3D/2D. Scripts: `AutoDestroyPS` (48), `ParticleCollisionInstance` (32),
  `ProjectileMover`/`2D` (54), `EGA_EffectSound` (14), `Hovl_Laser` (10).
- **Shaders**: `Blend_CenterGlow` (100 materiais) e `Add_CenterGlow` (83) cobrem
  87%. Ambos têm textura principal + ruído + fluxo + máscara com rolagem de UV,
  emissão, brilho central e **partícula suave por profundidade**. O resto:
  `Blend_TwoSides`, `AddTrail`, `Blend_Normals`, `Electricity`, `DissolveNoise`,
  `SmoothSmoke`, `LightGlow`, `Lit_CenterGlow` e **distorção** (`Distortion`,
  `BlendDistort`), que lê a cor da cena (`_GrabTexture`).

Consequência: com **10 módulos + 3 renderizadores + 2 famílias de shader**, a
Astra cobre a grande maioria do pacote. Colisão, rastro, luz e subemissor fecham o
restante. Distorção exige cópia da cor da cena e fica restrita a B/A/S.

## 6. Princípios que valem para todos os blocos

1. **Orçamento antes de efeito.** Cada nível tem um orçamento de ms por categoria
   (tabela do §7). Um recurso só é ligado por padrão num nível se couber nele no
   aparelho de referência daquele nível.
2. **Uma variante por nível, não um interruptor.** Ex.: nuvens = gradiente no
   céu (C), domo 2D (B), volumétrico amortizado (A), volumétrico com mais passos
   e sombra (S). O autor configura a aparência **uma vez**; a política escolhe a
   variante. O dado autoral nunca é apagado (regra de `DeviceLimited`).
3. **Componente primeiro, API de graça.** Todo recurso é `ComponentType` com
   descritor, serialização versionada e migração; a fachada C# sai do gerador
   existente; eventos viram nós Flow. Nada de API escrita à mão paralela ao editor.
4. **Capacidade registrada.** Novos ids em `engine_capability.h` (`render.sky.clouds`,
   `render.time_of_day`, `render.weather`, `render.particles.*`, `render.post.bloom_chain`,
   `render.post.color_grading`, `render.probe.*`, `render.gi.lightmap_bake`…).
5. **Medição com o protocolo já usado** (`PERFORMANCE-POCO-F7-2026-10-05.md`):
   A/B/B/A, mesma pose, mesma cadência, imagem comparada, p95/p99, potência quando
   disponível. Ganho abaixo de 5% ou com p99 pior não é promovido.
6. **Ganho estrutural separado de política.** Baixar resolução no nível C é uma
   decisão de política legítima, mas é registrada separadamente e nunca conta como
   otimização de shader/CPU.
7. **Regras do AGENTS.md**: referência Unity/Godot por capacidade, captura no
   aparelho, conceito visual gerado como hipótese, ícones novos no atlas real,
   nota na aba Atualizações do AstraDocs, APK público por bloco entregue.

## 7. Níveis e orçamento-alvo

Metas propostas para discussão. Os números são orçamento, não medição.

| | C (fraco) | B (médio) | A (alto) | S (topo) |
|---|---|---|---|---|
| Exemplo de GPU | Mali-G52 / Adreno 610 | Mali-G68 / Adreno 642 | Adreno 730 / Immortalis | Adreno 8xx (POCO F7) |
| Meta de quadro | **60 FPS (16,6 ms)**, com efeitos reduzidos | 60 FPS (16,6 ms) | 60 FPS | 60–120 FPS |
| Resolução interna (política) | 0,5–0,67 + FSR1 (≈ 800–1070 px de largura) | 0,67–0,75 + FSR1/ASR | 0,75–1,0 + ASR | 1,0 ou ASR qualidade |
| Opaco + sombras | ≤ 8 ms | ≤ 7 ms | ≤ 6 ms | ≤ 5 ms |
| Céu + nuvens | ≤ 0,3 ms (gradiente, sem nuvem própria) | ≤ 0,8 ms (domo 2D) | ≤ 1,2 ms (volumétrico amortizado) | ≤ 1,5 ms |
| Partículas/VFX | ≤ 1,5 ms, 1.000 partículas, ½ resolução, sem distorção, luz de partícula desligada | ≤ 2 ms, 8.000 | ≤ 2 ms, 30.000 (compute) | ≤ 2 ms, 100.000 |
| Pós | ≤ 1,2 ms (tonemap+LUT+bloom ¼, sem SSAO, sem sun shafts) | ≤ 2 ms (+AO ½) | ≤ 2,5 ms (+DoF/sun shafts) | ≤ 3 ms (+motion blur) |
| Sombras | 1 cascata curta, filtro 4, cache estática; sem sombra de luz local | 2 cascatas | 3 cascatas | 4 cascatas, sombra de nuvem |
| GI | lightmap + sondas (custo ≈ zero em tempo de execução) | lightmap + sondas | + sondas de reflexão amortizadas | + GI dinâmica (pesquisa) |

**Decisão de 17/10/2026: nível C mira 60 FPS, aceitando menos efeitos.** Soma da
GPU no C: 8 + 0,3 + 1,5 + 1,2 ≈ 11 ms; os ~5 ms restantes cobrem ampliação FSR1,
UI e variação térmica. A CPU também precisa caber em 16,6 ms (meta: ≤ 10 ms na
thread principal, com culling/animação em jobs), o que torna o trabalho de CPU do
G1 obrigatório, não opcional. Num aparelho fraco aquecido, 60 FPS pode não se
sustentar: a política térmica primeiro reduz resolução e efeitos do próprio nível;
cair para 30 FPS é o último recurso e fica registrado na telemetria. Aparência
autoral é a mesma em todos os níveis; o C mostra menos efeitos, não outro jogo.

**Decisão de 17/10/2026:** o único aparelho disponível é o POCO F7 (nível S), e
mesmo nele o FPS está baixo. Então o POCO é a bancada de todos os A/B/B/A, e o
nível C é validado de duas formas: (1) no POCO, com o **modo nível C forçado**
(G0), que mede o custo relativo de cada variante; (2) com **dados reais dos
testadores** coletados pela telemetria (GT). O POCO não reproduz banda de memória
nem ALU de uma GPU fraca. Por isso nenhum orçamento do nível C é declarado
cumprido sem dados de campo de pelo menos três modelos de GPU do nível C.

## 8. Blocos

Cada bloco lista: o que entrega, onde no código, critério de aceite. A ordem
interna de cada bloco é a ordem de implementação.

### G0 — Bancada no POCO F7 e cenas de aceite (pré-requisito)

1. POCO F7 por ADB; build Release `profileable` (já existe
   `-PastraProfileable=true`); coletor de FPS apresentado, GPU total, CPU por
   thread e potência pela HAL de bateria (todos já usados no POCO).
2. **Modo nível C forçado**: a política resolve o nível C de ponta a ponta
   (variantes de shader, sombras, pós, partículas, nuvens) e a resolução interna é
   fixada na **contagem de pixels de um celular 720p** (≈ 1600×720), não na escala
   relativa à tela do POCO. Mede o custo relativo das variantes C; não prova FPS
   de um aparelho C. Chave de diagnóstico, não opção de jogo.
3. Quatro cenas fixas com pose explícita: **Vila Astra** (cidade, 165 instâncias),
   **Sponza** (interior, GI), **Campo aberto** (nova: terreno, vegetação, céu,
   ciclo de dia, palco de G3/G4) e **Arena VFX** (nova: 20 prefabs simultâneos, G5/G6).
4. **Astra Bench**: uma rota de câmera gravada por cena (`CameraRoutePlayer` já
   existe), executável pelo menu do app em 3 minutos. É o mesmo teste que os
   testadores rodam (GT), então os números do POCO e os de campo são comparáveis.
5. Linha de base no POCO em nível S e em nível C forçado: tabela por passe
   (sombra, opaco, céu, transparente, pós, UI) e CPU por sistema (script, física,
   animação, culling, gravação de comandos, editor/ImGui). Hoje até o POCO tem FPS
   baixo, então essa tabela já basta para escolher os primeiros alvos do G1.
6. Opcional, sem comprar aparelho: rodar o Astra Bench em celulares fracos reais de
   laboratório remoto ([Firebase Test Lab, modo Game Loop](https://firebase.google.com/docs/test-lab/android/game-loop)),
   que instala o APK, executa o loop e devolve logs. Útil para ter o primeiro dado
   do nível C antes de os testadores responderem.

Aceite: quatro cenas e Astra Bench reproduzíveis; baseline A/A com variação < 3%
no POCO; tabela de onde vai o tempo, em nível S e em nível C forçado.

**Execução (17/10/2026):**
- Já existiam: forçar nível por `aether.quality_preset`, travar qualidade
  (`aether.lock_rendering_quality`), resolução dinâmica desligável, rota de câmera
  gravada/reproduzida (`platform/camera_route.h`), coletor de janelas por passe e
  apresentação do SurfaceFlinger (`tools/android-frame-profile.ps1`).
- Novo: `aether.reference_resolution=LARGURAxALTURA` converte a área de referência
  numa escala fixa sobre a superfície real (`renderer::referenceResolutionScale`,
  teste `reference_resolution_emulates_target_pixel_count`). No POCO, 1600×720 dá
  escala 0,570 (≈ 1579×729 interno).
- Novo: `tools/measure-g0-baseline.ps1` roda cada projeto em nível S nativo e nível
  C forçado, ordem S/C/C/S, e gera `baseline.md` com FPS e GPU/CPU por passe.
- O **Astra Bench pelo menu do app** passa para o GT: é o teste que os testadores
  rodam e cujo resultado é enviado; no G0 a bancada é por ADB.
- Cenas novas Campo aberto e Arena VFX entram junto de G4 e G5, que as consomem.

**Linha de base medida (17/10/2026, POCO quente e carregando, decisão do proprietário):**
relatório completo em `docs/validacao/g0-baseline-2026-10-17/README.md`.

| | FPS | GPU | CPU | Opaco | Pós | Sombra |
|---|---|---|---|---|---|---|
| Vila Play, S | 18,5 | 50,8 ms | 23,6 ms | 23,2 ms | 18,1 ms | 5,5 ms |
| Vila Play, C forçado | 30,2 | 30,8 ms | 18,0 ms | 9,9 ms | 6,0 ms | 5,5 ms |
| Sponza Edit, S | 41,3 | 22,7 ms | 4,5 ms | 12,1 ms | 10,5 ms | 0 |
| Sponza Edit, C forçado | 40,8 | 22,1 ms | 4,5 ms | 8,1 ms | 13,9 ms | 0 |

Consequências para o G1, nesta ordem: (1) **pós** (10–18 ms) — dividir o shader único
e levar SSAO/bloom a meia resolução, antecipando o G2; (2) **CPU da Vila** (17–24 ms
com 11 personagens) — simpleperf e jobs; (3) opaco. Ressalvas: eixos de qualidade
definidos no projeto vencem o preset (a Vila em C manteve sombras S); o EASU no POCO
(6,3 ms) sai na resolução nativa e não representa um celular 720p; a Sponza foi
reimportada da fonte oficial sem a autoria antiga de player/luz.

### GT — Telemetria de desempenho dos testadores

> **Estado em 18/10/2026.** O servidor saiu do Supabase (limite de 2 projetos gratuitos)
> para uma Neon Function no projeto `astra-telemetry`. O site `astraengine.com.br`
> só repassa, e a gravação foi validada ponta a ponta em produção. O app já tem
> consentimento na primeira abertura, Configurações › Telemetria de desempenho,
> janelas de 600 quadros, fila em disco, envio em lote e "Apagar meus dados".
> O jogo exportado sai sem a permissão de internet. Nada disso foi testado no
> aparelho ainda.
> Recorte desta primeira versão: FPS, intervalo, CPU da thread e do processo, GPU
> (só quando os timestamps estão ligados; senão `null`), resolução, térmica, draws,
> triângulos, modelo, GPU, RAM, tela, nível recomendado e resolvido.
> Fora deste recorte, por enquanto: bateria, ADPF, GPU por passe, falhas e ANR,
> Astra Bench e "Relatar lentidão". Detalhes em `services/telemetry/README.md`.

Objetivo: transformar cada APK distribuído numa medição. O app já calcula quase
tudo localmente (`platform/android/android_frame_profiler.h`: FPS, orçamento,
pressão térmica, ADPF, draws, triângulos, flags de cada otimização); hoje isso só
vai para o Logcat. O manifesto atual **não tem permissão de internet**.

**Consentimento e privacidade (LGPD)**
- Desligada por padrão. Na primeira abertura, uma tela explica o que é enviado e
  pede **“Enviar dados de desempenho”** (sim/não), revogável nas configurações.
- Nada pessoal: sem e-mail, nome, localização, IMEI, Android ID, lista de apps nem
  conteúdo de projeto. Identificador = UUID aleatório da instalação, apagável.
  Nomes de projeto e de cena viram hash, salvo nas cenas de bancada da Astra.
- **Jogos exportados pelos usuários não herdam a telemetria**: o exportador remove
  o módulo e a permissão, como já faz com a permissão de profiling.
- Retenção definida (proposta: 180 dias para sessões, agregados sem prazo).

**O que é coletado**
| Grupo | Campos |
|---|---|
| Aparelho | fabricante/modelo, SoC, GPU (vendor/device id/nome/driver), versão Vulkan e features (`DeviceFeatures`), RAM, Android, taxa de atualização e resolução da tela |
| Política | nível recomendado e resolvido, evidência (`DeviceQualityEvidence`), cada redução aplicada (`PolicyClampRecord`), escala de resolução, filtro de ampliação |
| Quadro | FPS apresentado; tempo de quadro p50/p95/p99; CPU ativa e GPU total; GPU por passe quando os timestamps forem válidos; quadros perdidos |
| Térmica e energia | estado térmico e headroom ao longo da sessão, ADPF, Game Mode, nível de bateria, carregando ou não, potência estimada quando a HAL expõe |
| Conteúdo | cena (hash ou id de bancada), draws, triângulos, materiais, partículas vivas, luzes, memória Vulkan e RSS |
| Estabilidade | falha nativa (assinatura, sem dados do usuário), ANR, perda de superfície, recusa de pipeline |
| Astra Bench | resultado completo de cada rota, com versão do APK e hash da cena |

Amostragem: janelas agregadas de 10 s no aparelho (não quadro a quadro), fila
local quando offline, envio em lote, de preferência no Wi-Fi, limite de tamanho
por dia.

**Transporte e servidor: Supabase (decidido em 17/10/2026)**
- **Projeto próprio** `astra-telemetry` na organização existente do proprietário,
  separado dos dois projetos de outros apps que já existem lá (dados, chaves e cota
  isolados). Consulta de custo pelo conector em 17/10/2026: US$ 0/mês. Se a cota do
  plano gratuito recusar um terceiro projeto ativo, a decisão volta ao proprietário
  (pausar um projeto ou mudar de plano); nunca reaproveitar um banco de outro app.
- **Ingestão por Edge Function** `ingest` (Deno): valida esquema JSON versionado,
  tamanho do lote, versão do APK e limite por instalação/IP; grava com a chave de
  serviço, que fica só no servidor. O app envia apenas a chave pública (publishable)
  e não tem permissão de leitura nem de escrita direta em nenhuma tabela.
- **Tabelas** (esquema `telemetry`, migrações versionadas no repositório):
  `installations` (UUID, aparelho, GPU, features), `sessions` (versão do APK,
  política resolvida, cena), `windows` (janelas de 10 s), `bench_runs` (Astra Bench),
  `crashes`, `slow_reports` (“Relatar lentidão”). RLS ligada em todas, **sem
  nenhuma política para `anon`/`authenticated`**: só a função escreve, só o
  proprietário lê pelo painel do Supabase ou por SQL.
- **Agregados** por visão materializada: FPS/tempo de quadro por GPU × nível ×
  versão × cena, atualizados por `pg_cron`. Retenção: job que apaga `windows` com
  mais de 180 dias; agregados ficam.
- **Apagar meus dados**: a função aceita `delete` com o UUID da instalação, chamado
  pelo botão nas configurações do app.
- Lado do app: módulo nativo de fila (arquivo na pasta privada, limite em disco),
  envio em lote por HTTPS com tentativas e espera crescente, permissão `INTERNET`
  só no manifesto do editor. O exportador de jogo retira o módulo e a permissão.
- Verificação antes de liberar: avisos de segurança do Supabase (advisors) sem
  pendência; teste de que a chave pública não lê nem grava nada diretamente.

**Execução (18/10/2026), parte local:** janela agregada no app
(`native/profiler/telemetry_window.{h,cpp}`, JSON `astra.telemetry.window/1`, GPU não
medida sai como null; `aether_telemetry_tests` 3/3, alvo pequeno que compila em ~13 s);
migração SQL (esquema `telemetry`, RLS sem políticas, agregado por GPU, gravação só por
funções `SECURITY DEFINER` executáveis pelo `service_role`, retenção de 180 dias via
pg_cron) e Edge Function `ingest` com validação do lote, em `services/telemetry/`.
Nada publicado: criar o projeto e publicar a função aguardam autorização. Pendentes no
app: consentimento, fila local e envio, envelope do aparelho, ligação ao laço de
quadros, "Apagar meus dados" e remoção no jogo exportado.

Referências: [Supabase Edge Functions](https://supabase.com/docs/guides/functions),
[Row Level Security](https://supabase.com/docs/guides/database/postgres/row-level-security),
[pg_cron](https://supabase.com/docs/guides/database/extensions/pg_cron).

**O que os dados realimentam**
1. **Tabela de GPUs conhecidas**: `recommendDeviceQuality` já aceita a evidência
   `RecognizedGpuIdentity`. Os dados de campo definem o nível inicial de cada GPU,
   em vez de só deduzir pelo conjunto de features.
2. **Orçamento real do §7** por modelo de GPU, substituindo as metas propostas.
3. **Regressões por versão**: cada APK publicado é comparado com o anterior no
   mesmo modelo de GPU e na mesma cena de bancada.
4. **Painel** de desempenho (artifact privado ou página interna; nunca no AstraDocs
   público): FPS por GPU, nível, versão e cena; térmica ao longo da sessão; falhas.

**Feedback dirigido**: botão “Relatar lentidão” no menu do app, que anexa a última
janela de telemetria e um comentário curto do testador.

Referências: [Android Performance Hint / ADPF](https://developer.android.com/games/optimize/adpf),
[Firebase Performance Monitoring](https://firebase.google.com/docs/perf-mon) (como
modelo de traces e amostragem; não obrigatório), [LGPD, Lei 13.709/2018](https://www.planalto.gov.br/ccivil_03/_ato2015-2018/2015/lei/l13709.htm).

Aceite: consentimento na primeira abertura e revogação funcionando; sessão do POCO
chegando ao servidor com todos os grupos; envio offline/online testado; jogo
exportado sem telemetria e sem permissão de internet adicionada por ela; painel
mostrando a primeira semana de dados; nota no AstraDocs explicando o que é coletado.

### G1 — Desempenho estrutural

Ordem decidida pela atribuição de G0. Candidatas já justificadas pelo código:

**GPU**
1. **Céu físico por LUT** (técnica de Hillaire 2020: LUT de transmitância 256×64,
   LUT de visão do céu 192×108 recalculada só quando sol/atmosfera mudam). Hoje
   cada pixel de céu faz raymarch. Com dia/noite (G4) o sol muda o tempo todo:
   recalcular a LUT em 1–2 quadros, amortizado. Referência: atmosfera física da
   Unity HDRP ([Physically Based Sky](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/create-a-physically-based-sky.html)).
2. **Compactação de vértices** (lição do vídeo voxel): posição quantizada 16 bits
   por malha, normal+tangente octaedral 2×16, UV half. Reduz banda de vértice e
   binning nos tilers. Migração de cache de malha versionada.
3. **Mescla de estáticos por material** (lote estático) com culling por bloco
   (reuso de `SpatialRenderChunks`, hoje opt-in por custo de memória).
4. **Shaders do nível C**: família sem normal map à distância, um só lookup de
   ambiente, sombra de 4 amostras. A especialização de sólidos já provou −13,8% e
   −10,2%; estender o mesmo método aos materiais restantes.
5. **Pós dividido** (entra em G2 com qualidade junto).
6. **Overdraw de transparência**: ordenação e limite por nível; base para as
   partículas de G5.

**CPU** (Vila mediu 24,9 ms de CPU no topo de linha)
7. Perfil com simpleperf na Vila em Play: separar script gerenciado, física,
   animação, atualização de cena, gravação de comandos.
8. Jobs no `core/job_system` para culling, animação e preparação de instâncias;
   gravação de comandos sem realocação por quadro.
9. Player exportado sem custo de editor (o APK de jogo hoje inclui o binário do
   editor: medir se há trabalho de ImGui/autoria rodando no Play).
10. Ritmo de quadro: pacing estável a 30/60 Hz (Android Frame Pacing / Swappy)
    para não oscilar entre 30 e 60 no nível C.

**Execução (17/10/2026) — pós, primeira entrega:** atribuição por máscara de
diagnóstico (`aether.post_isolation`) mostrou a névoa como 45% do pós, porque era
reavaliada em cada amostra de FXAA/bloom/reconstrução. Pré-passe de névoa por texel
promovido como padrão no Play: pós −38%, quadro −16,6%, FPS +19,9% no mesmo clock,
imagem equivalente (≤ 3 níveis em 99,6% dos pixels estáticos; contorno de silhuetas
mais correto). Evidência: `docs/validacao/g1-post-fog-resolve-2026-10-17/README.md`.
Próximos no pós: bloom em cadeia de mips e AO em meia resolução (G2), depois CPU da Vila.

**Bloom em cadeia e AO em 1/2 (18/10/2026):** implementados como candidatas,
desligados por padrão. Bloom em cadeia: custo neutro (pós −1,5%) e aparência mais
espalhada; decisão visual, segue para o G2. AO em 1/2: pós −13,9%, quadro −4,7%,
imagem equivalente; abaixo do limite de 5%, fica opcional até os dados de campo.
Evidência: `docs/validacao/g1-post-bloom-ao-2026-10-17/README.md`. O pós da Vila
em S caiu de 15–18 ms para 8–9,5 ms; próximos alvos do G1: CPU (18–21 ms) e opaco (~15 ms).

**CPU da Vila (18/10/2026):** perfil com simpleperf mostrou 98% da CPU numa thread,
~31% em alocação e `SceneAnimator::advance` com 51% (milhares de vetores por quadro).
O animador passou a usar valores em linha e rascunho reutilizado entre quadros, com a
mesma lógica de mistura (84/84 testes). Medido por instruções de usuário por quadro,
independente do clock: **−30,8% instruções, −27,3% ciclos**. Evidência:
`docs/validacao/g1-cpu-vila-2026-10-18/README.md`. Segunda rodada: thread do jogo
anexada à JVM uma vez (antes, anexava e desanexava a cada quadro) e `worldMatrix`
memorizada por (identidade do grafo, revisão): **−11,1% instruções**; acumulado
**−40%** (32,9 M -> 19,8 M por quadro). Terceira rodada: revisão estrutural no grafo
(câmeras da cena e animadores vivos em cache), repouso decomposto uma vez e fim da busca
quadrática nas poses de camada: **−7,7%** (8 rodadas); acumulado **−44%**. Próximo alvo
de G1: o opaco na GPU (~15 ms), que limita o FPS da Vila.

Aceite: Vila e Sponza no modo nível C forçado do POCO com GPU ≤ 11 ms e CPU ≤ 10 ms;
em campo (telemetria), mediana ≥ 58 FPS e p99 < 25 ms por 10 min em 3 GPUs do nível C;
no POCO, Vila em Play ≥ 60 FPS na escala da política. Cada item com A/B/B/A próprio.

### G2 — Pós-processamento v2

> **Estado em 19/10/2026 (itens 2 e 7).** Gradação por LUT 32³ (balanço de branco,
> lift/gama/ganho, sombras/médios/realces, vibração) e os sete perfis prontos na aba Cor
> do Ambiente, com Ajuste fino recolhido. Neutro pela LUT: ≤ 3/255 em 99,997% dos pixels
> estáticos; custo da leitura 3D no POCO ≈ +0,09 ms no mesmo clock. Mixer de canais,
> curvas e `.cube` ficam para depois. Evidências em `docs/validacao/g2-color-grading-2026-10-19/`.

Substitui o shader monolítico por uma **pilha de passes com variantes por nível**,
mantendo os campos atuais do `Environment` (migração v12→v13 preserva cenas antigas
com a mesma aparência).

1. **Bloom real** em cadeia de mips (downsample 13 amostras + upsample tenda,
   ½ → 1/64), limiar suave, intensidade, dispersão, cor, sujeira de lente opcional.
   Mais barato que o atual em resolução cheia e espalha luz de verdade. Referência:
   [Bloom da URP](https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/post-processing-bloom.html).
2. **Color grading por LUT 32³**: balanço de branco, lift/gamma/gain,
   sombras/médios/realces, mixer de canais, curvas, saturação/vibração, e
   importação de `.cube`. A LUT é montada só quando os parâmetros mudam; aplicar
   custa uma leitura de textura 3D por pixel. Referência: [Color grading da
   Godot](https://docs.godotengine.org/en/4.5/tutorials/3d/environment_and_post_processing.html) e
   [Color Lookup da URP](https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/post-processing-color-lookup.html).
3. **SSAO em meia resolução** com blur bilateral e reconstrução por profundidade,
   e uso do resultado como **oclusão especular** (ligação com G3).
4. **Sun shafts** em espaço de tela (meia/quarta resolução, 1 passe radial),
   intensidade ligada ao `TimeOfDay`. Neblina com dispersão na direção do sol
   (in-scattering) usando a cor do céu.
5. Nível A/S: profundidade de campo (bokeh em ¼), aberração cromática, motion
   blur por vetor de velocidade (já existem vetores de movimento).
6. Fusão de tonemap + LUT + vinheta + grão + UI no passe final compatível com tile
   (a técnica de fusão já foi validada).
7. **Perfis prontos** como recursos de ambiente: Neutro, Cinemático, Dia quente,
   Entardecer, Noite azul, Tempestade, Fantasia. O autor escolhe um e ajusta;
   ninguém precisa “saber criar pós” do zero.

Aceite: perfil Neutro igual à imagem atual (diferença máxima ≤ 3/255); pós total
dentro do orçamento do §7 em C e POCO; capturas de cada perfil no aparelho.

### G3 — Luz natural e fim do aspecto “brilhoso”

**G3a — barato e imediato**
1. **Oclusão especular**: reflexo do ambiente multiplicado pela oclusão (AO de
   material × SSAO × visibilidade do céu) e pela oclusão de horizonte da normal.
   Causa principal do “tudo brilha” em interiores e sob telhados.
2. **Auditoria de materiais importados**: rugosidade/metálico padrão, canais
   trocados (smoothness Unity → rugosidade), especular de não-metais fixo em 4%.
   Relatório por pacote importado.
3. **Exposição e branco**: exposição automática padrão em cenas novas com faixa
   sensata; ponto de branco por hora do dia (G4).

**G3b — sondas**
4. **Sonda de reflexão** (componente `ReflectionProbe`): captura de cubemap com o
   próprio renderer, filtro de rugosidade pré-calculado, projeção em caixa, mistura
   por volume. Modos: bake no editor, atualização sob demanda por script, e no
   A/S atualização amortizada (uma face por quadro). Referências:
   [ReflectionProbe Godot](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/reflection_probes.html),
   [Reflection Probes Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/ReflectionProbes.html).
5. **Volume de sondas de irradiância** (harmônicos esféricos L1/L2 em grade):
   bake e amostragem por instância ou por pixel. Dá luz indireta a personagens e
   objetos móveis. Referência: [Light Probes Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/LightProbes.html).

**G3c — lightmap (a capacidade do primeiro vídeo)**

> Estado (23/10/2026): implementado na branch `claude/agitated-hodgkin-0a80d5` — UV2 por xatlas na importação, baker
> progressivo em CPU, Gerar iluminação/Limpar na janela Iluminação, Modo da luz e GI do Mesh Renderer com efeito, lightmap
> reescalado pelo céu da hora. Relatório e medições: `docs/validacao/g3c-lightmap-2026-10-23/`.
6. **Desembrulho automático de UV2** (biblioteca [xatlas](https://github.com/jpcy/xatlas), MIT).
7. **Baker progressivo** por caminho de luz em CPU com jobs (o aparelho C não tem
   ray query): luz direta + 1–3 rebotes, céu, emissivos, denoise simples, dilatação,
   compressão. Roda no PC e no celular (em segundo plano, com progresso e cancelamento).
   Referências: [LightmapGI Godot](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html),
   [Progressive Lightmapper Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/progressive-lightmapper.html).
8. **Compatibilidade com dia/noite**: o lightmap guarda **luz indireta de luzes
   locais + visibilidade do céu**, não o sol. O sol continua direto e com sombra em
   tempo real. A luz indireta do céu é reescalada pela cor do céu da hora atual.
   Sem isso, o lightmap “congela” o meio-dia.

**G3d — GI dinâmica (pesquisa, só S)**
9. Volume de sondas reiluminado no tempo (estilo DDGI simplificado, sem ray tracing:
   raios contra o SDF/lightmap ou cubo de baixa resolução) ou LPV. Só entra se G0
   mostrar orçamento sobrando no nível S.

Aceite: Sponza com e sem G3 lado a lado no aparelho; nenhum objeto “acende”
sob sombra; bake de uma cena média termina no POCO; lightmap + dia/noite sem
salto de iluminação ao longo de 24 h simuladas.

### G4 — Dia/noite, céu, nuvens e clima

1. **`TimeOfDay`** (componente de cena único): hora, velocidade, duração de dia e
   noite separadas, data, latitude, fase da lua, pausa. Dirige o sol (direção,
   cor por temperatura, intensidade), a lua (luz direcional fraca e fria), o céu,
   o ambiente, a neblina e a exposição. Aparência por **gradientes pela hora**
   (estudo do UniStorm), com versão “tempestade” misturada pelo clima.
   Eventos: `HourChanged`, `Sunrise`, `Sunset`, `DayChanged` (Flow + C#).
2. **Céu**: LUT física do G1 + disco do sol e da lua com fase + **estrelas**
   (mapa estelar com cintilação, girando com a latitude) + aurora opcional.
3. **Nuvens em níveis**: C = cobertura no gradiente do céu; B = domo 2D com ruído
   rolando; A/S = **volumétrico em domo de textura fixa, recalculando 1/16 dos
   texels por quadro com reprojeção** (custo independente da tela). Perfis de
   nuvem: suave, densa, tempestade, estilizada.
4. **Sombra de nuvem**: textura de cobertura 256² projetada no sombreamento do sol.
5. **`Weather`** + recurso `WeatherProfile`: nuvem, neblina, vento, intensidade do
   sol, precipitação (sistema de partícula do G5, seguindo a câmera), som em loop
   (usa o mixer de áudio existente), raios, molhado da superfície (rugosidade e cor
   escurecidas, poças por máscara no nível A/S). Transição temporizada entre perfis.
   Perfis prontos: Limpo, Parcialmente nublado, Nublado, Neblina, Chuva leve,
   Chuva forte, Tempestade, Neve, Nevasca, Tempestade de areia.
6. **Vento global**: direção/intensidade/rajada num bloco uniforme lido por
   partículas, vegetação e água.
7. **Raio**: linha procedural ramificada + flash de luz + trovão com atraso pela
   distância.

Referências: [Sky/Environment Godot](https://docs.godotengine.org/en/4.5/tutorials/3d/environment_and_post_processing.html),
[Volumetric Clouds HDRP](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/create-realistic-clouds-volumetric-clouds.html),
[Lighting/Skybox Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/skyboxes.html).

Aceite: Campo aberto rodando 24 h em 4 min sem saltos, nas quatro variantes;
custo de céu+nuvens dentro do orçamento em C e POCO; transições de clima por script.

### G5 — Sistema de partículas e VFX

Componente `ParticleSystem` com módulos no **mesmo vocabulário da Unity** (facilita
importar e aprender), executado em CPU com jobs/SIMD nos níveis C/B e em **compute**
nos níveis A/S. Referências: [Particle System modules Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystemModules.html),
[GPUParticles3D Godot](https://docs.godotengine.org/en/4.5/tutorials/3d/particles/index.html).

Pacotes na ordem da varredura do §5 (cada pacote completo: modelo, runtime,
propriedades com efeito, persistência, criação, editor, teste):

| Pacote | Conteúdo | Cobre do Hovl |
|---|---|---|
| P1 | Main, Emission (taxa, rajadas), Shape (cone, esfera, hemisfério, caixa, círculo, aresta), Color/Size/Rotation over Lifetime, renderer billboard + esticado, 2 famílias de material de partícula (mistura/aditivo com ruído, máscara, rolagem de UV, brilho central, **partícula suave**) | ~70% |
| P2 | Velocity/Limit Velocity/Force over Lifetime, Inherit Velocity, Texture Sheet, Noise, renderer de malha, espaço local/mundo | ~90% |
| P3 | Trails, `TrailRenderer`, `LineRenderer`, Lights (respeitando o limite de 8 luzes por prioridade), Sub Emitters, Custom Data | ~97% |
| P4 | Collision (planos + consultas físicas existentes, limitada por nível), eventos de colisão para script, Distortion (B/A/S, cópia da cor em ½), Dissolve, Electricity, Two Sides | ~100% |
| P5 | Compute (A/S), ordenação, LOD/culling de emissor por distância e tela, partículas em ½ resolução no nível C com composição | desempenho |

Também: `Decal` (marca de impacto, poças) e “VFX Prefab” com auto destruir ao
terminar (substitui o `AutoDestroyPS`).

Aceite: Arena VFX com 20 efeitos simultâneos no orçamento do nível; editor de
curvas/gradientes por módulo com prévia ao vivo; play/stop/emit por script.

### G6 — Importação dos pacotes

**RPG VFX Bundle** (amplia `resources/unity_prefab_import.cpp`, hoje restrito a
GameObject/Transform/MeshFilter/MeshRenderer):
1. Classes Unity novas: ParticleSystem (198), ParticleSystemRenderer (199),
   TrailRenderer (96), LineRenderer (120), Light (108), AudioSource (82),
   colisores e Rigidbody já mapeáveis.
2. Materiais Hovl por **nome/GUID do shader** → famílias de partícula do G5
   (`unity_material_import.cpp` hoje só aceita o Standard).
3. Scripts comuns → componentes Astra: `AutoDestroyPS` → autodestruir;
   `ProjectileMover` → projétil com impacto; `ParticleCollisionInstance` → evento
   de colisão que instancia prefab; `EGA_EffectSound` → AudioSource com som aleatório;
   `Hovl_Laser` → laser por raycast.
4. **Relatório de cobertura por prefab**: suportado / parcial (lista de módulos
   ignorados) / recusado. Meta: 217/217 abrindo, ≥ 95% sem perda visível.

**UniStorm**: importar só dados úteis — 30 perfis de clima e 3 de nuvem como
`WeatherProfile`/`CloudProfile` (valores e gradientes), sons, mapas de estrelas,
partículas de chuva/neve (via G5). Scripts e shaders não são importados.

Aceite: os dois pacotes importados no aparelho pela Biblioteca de pacotes;
comparação lado a lado com as prévias do pacote; relatório publicado só com
contagens (nunca os assets).

### G7 — API universal, editor e publicação (contínuo)

**API** (gerada do descritor + métodos manuais finos onde houver comando):
```csharp
TimeOfDay.Current.Hour = 18.5f;  TimeOfDay.Current.Speed = 60;   // 1 min real = 1 h
TimeOfDay.Current.HourChanged += h => { ... };
Weather.Current.TransitionTo(rainProfile, seconds: 20);
var fx = Scene.Instantiate(fireball, pos, rot); fx.Get<ParticleSystem>().Play();
ParticleSystem.Emit(count: 30); ParticleSystem.Stop(StopBehavior.StopEmitting);
EnvironmentVolume.Get(obj).PostProfile = cinematic;  Graphics.Settings.Clouds = GraphicsFeature.Enabled;
ReflectionProbe.Get(obj).RenderNow();  Lightmaps.BakeAsync(scene, progress);
```
Nós Flow para eventos de hora, clima e colisão de partícula. Novos campos em
`GraphicsSettings` (nuvens, partículas máximas, distorção, sondas) com a política
resolvida exposta.

**Editor** (regras do AGENTS.md e da memória de UI universal): painel “Céu e
clima” com régua de 24 h arrastável e prévia ao vivo; editor de partículas com
módulos empilhados, curvas e gradientes inline, botões Reproduzir/Parar/Reiniciar
no viewport; criação por catálogo (“Efeito › Fogo / Faísca / Chuva…”). Conceito
visual gerado a partir de captura real, ícones novos no atlas, captura final no
aparelho.

**Publicação por bloco**: nota em `AstraDocs/data/updates.json`, APK público,
docs de cada componente, limites e migrações declarados.

## 9. Ordem e paralelismo

```text
G0 bancada POCO + Astra Bench ──► GT telemetria (1º APK com coleta) ─┐
                                                                    ▼ dados de campo alimentam G1 e o §7
G0 ──► G1 desempenho ──► G2 pós v2 ──► G4 dia/noite/clima ──► G6b UniStorm
                 │                 │              ▲
                 │                 └► G3a anti-brilho ─► G3b sondas ─► G3c lightmap ─► G3d (pesquisa)
                 └────────────────────► G5 partículas P1..P5 ──► G6a RPG VFX
G7 (API/editor/docs) acompanha cada bloco
```

- GT vem logo depois de G0 e antes do grosso de G1: o primeiro APK com telemetria
  sai cedo, e cada APK seguinte já chega medido nos aparelhos dos testadores.
  G1 começa no POCO em paralelo, pelos alvos que G0 mostrar.
- G3a vem cedo porque é barato e ataca diretamente o “brilhoso”.
- G5 (partículas) é independente do céu e pode correr em paralelo a G2–G4 em outra
  sessão/worktree, já que toca arquivos diferentes (novo módulo `renderer/particles`).
- G4 depende da LUT de céu (G1) e do bloom/LUT (G2) para ficar bonito; o clima com
  chuva depende de G5 P1–P2.

## 10. Arquivos e módulos previstos

Novos (nomes indicativos): `renderer/sky_luts.*`, `renderer/clouds.*`,
`renderer/time_of_day.*`, `runtime/scene_weather.*`, `renderer/particles/*`,
`rhi/shaders/particle_*.{vert,frag,comp}`, `rhi/shaders/post_bloom_*.frag`,
`rhi/shaders/color_grading_lut.comp`, `rhi/shaders/clouds_*.frag`,
`renderer/reflection_probes.*`, `renderer/irradiance_volume.*`,
`resources/lightmap_baker.*`, `resources/unity_particle_import.*`,
`scene/{time_of_day,weather,particle_system,reflection_probe,decal}.h`.

Alterados: `post_process_common.glsl` (dividido), `instanced_renderer.cpp` (passes
novos), `environment.h` (v13), `rendering_policy.*`, `device_profile.*`,
`engine_capability.h`, `unity_prefab_import.cpp`, `unity_material_import.cpp`,
gerador de contratos e fachadas C#.

## 11. Licença dos pacotes (importante)

UniStorm e RPG VFX Bundle são do Unity Asset Store. A licença permite usá-los nos
**jogos** do proprietário, não redistribuir código ou assets como parte de uma
ferramenta. Portanto:
- O código C#/shader dos pacotes é **estudado, não copiado**; a Astra implementa
  as técnicas (que são públicas: raymarch amortizado, gradientes por hora, módulos
  de partícula) com código próprio.
- Texturas, malhas, sons e prefabs **não entram no APK da engine, no repositório
  público nem no AstraDocs**. Entram só em projetos do usuário via importação.
- O UniStorm inclui o Post Processing Stack v1 da Unity: só como estudo.
- Os efeitos e perfis prontos que a Astra distribuir (G2/G4/G5) precisam de
  texturas próprias ou CC0.

## 12. Riscos

| Risco | Mitigação |
|---|---|
| Não há aparelho fraco físico (confirmado) | POCO com nível C forçado para custo relativo; telemetria dos testadores e, se preciso, laboratório remoto para o FPS real; nível C só é declarado com dados de 3 GPUs C |
| Poucos testadores aceitam enviar dados | Pedido claro e curto; Astra Bench de 3 min com resultado mostrado ao testador; botão “Relatar lentidão” |
| Dados de campo ruidosos (térmica, carregador, cenas diferentes) | Comparar só Astra Bench ou cenas de bancada; segmentar por estado térmico e carregador; mediana por GPU, não médias soltas |
| Privacidade/LGPD | Opt-in, nada pessoal, UUID apagável, retenção definida, nota pública do que é coletado, jogos exportados sem telemetria |
| Bake de lightmap lento no celular | Bake no PC como caminho principal; no celular em segundo plano e com qualidade “prévia” |
| Overdraw de partículas no nível C | Limite por nível, ½ resolução, LOD de emissor, relatório de custo no editor |
| Distorção exige cópia da cena (caro em tiler) | Só B/A/S, em ½ resolução, uma cópia por quadro compartilhada |
| Migração de cenas (Environment v13, cache de malha) | Migração versionada com teste de leitura de todas as versões antigas |
| Escopo enorme | Cada bloco entrega sozinho; a ordem permite parar após qualquer bloco com ganho real |

## 13. Perguntas para o proprietário

1. ~~Qual celular fraco temos?~~ Respondido em 17/10/2026: só o POCO F7; nível C
   via modo forçado + telemetria dos testadores.
1a. ~~Servidor da telemetria?~~ Respondido em 17/10/2026: **Supabase**, projeto
   próprio `astra-telemetry` (desenho no bloco GT). Em 18/10/2026 o Supabase recusou
   o terceiro projeto gratuito; o proprietário escolheu a Vercel, e a recepção ficou
   numa Neon Function (projeto Neon `astra-telemetry`) atrás de `astraengine.com.br`.
1b. **Laboratório remoto** (Firebase Test Lab, Game Loop) para os primeiros dados
   de aparelhos fracos: usar ou esperar os testadores?
2. ~~Meta do nível C?~~ Respondido em 17/10/2026: **60 FPS, mesmo com menos efeitos** (tabela do §7).
3. ~~Ordem?~~ Aprovada em 17/10/2026: **G0 → GT → G1 → G2/G3a → G4, com G5 em
   paralelo** (sessão própria, módulo `renderer/particles`, sem disputar arquivos com G0/G1).
4. **Lightmap**: bake no PC é suficiente para começar, ou o bake no celular é
   requisito desde o início?
5. **GI dinâmica** (G3d) entra no escopo ou fica só como pesquisa?
6. Os assets do Hovl/UniStorm podem ser usados nas **cenas de teste locais**
   (nunca publicadas)?

## 14. Evidência desta análise

- Pacotes extraídos (cópia de trabalho temporária, fora do repositório) com
  extrator próprio; varredura dos 217 prefabs por script que conta módulos
  `enabled: 1`, modos de render, formas, espaço de simulação, componentes e shaders.
- Código Astra lido: `scene/environment.h`, `core/engine_capability.h`,
  `rhi/shaders/post_process_common.glsl`, `rhi/shaders/dirt_road_sky.frag`,
  `rhi/device_profile.h`, `renderer/rendering_policy.h`,
  `resources/unity_prefab_import.cpp`, `resources/unity_material_import.cpp`,
  planos de desempenho de 05/10 e de UI/Play de 11/10.
- Títulos dos vídeos obtidos pelo oEmbed do YouTube; o conteúdo descrito no §3
  vem do título/tema, não de assistir o vídeo quadro a quadro.
