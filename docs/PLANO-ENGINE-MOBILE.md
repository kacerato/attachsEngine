# PROJETO **AETHER** — Engine de Jogos Nativa Mobile
### Plano Técnico Completo, Arquitetura e Roadmap

> **Versão:** 1.0 — 24/08/2026
> **Premissa central:** o editor **roda no próprio celular**. Não é um engine de desktop que exporta para mobile — é uma suíte completa de criação (cena, modelagem, script visual, build e publicação) que vive dentro de um aparelho Android/iOS, desenhada para toque, tela pequena e orçamento térmico limitado.
> **Escopo do documento:** plano técnico "estado da arte", sem restrição de tamanho de equipe. Onde há um caminho mais barato, ele está marcado como **[atalho viável]**.

---

## SUMÁRIO

| # | Parte | Conteúdo |
|---|-------|----------|
| 0 | [Resumo executivo](#parte-0--resumo-executivo) | O que é, por que existe, apostas centrais |
| 1 | [Análise competitiva e tese do produto](#parte-1--análise-competitiva-e-tese-do-produto) | Godot, Unity, Unreal, Blender, Dreams, Roblox |
| 2 | [Usuários, jobs-to-be-done e requisitos](#parte-2--usuários-e-requisitos) | Personas, requisitos funcionais e não-funcionais |
| 3 | [Arquitetura geral](#parte-3--arquitetura-geral-do-sistema) | Camadas, decisão de linguagens, módulos |
| 4 | [Núcleo (Core Layer)](#parte-4--núcleo-core-layer) | Memória, jobs, ECS, reflection, serialização |
| 5 | [Renderização Vulkan](#parte-5--renderização--vulkan-de-última-geração) | RHI, render graph, GPU-driven, GI, upscaling |
| 6 | [Simulação](#parte-6--simulação-física-animação-áudio-ia) | Física, animação, áudio, IA, partículas |
| 7 | [Pipeline de assets](#parte-7--pipeline-de-assets-e-armazenamento) | Import, compressão, streaming, virtual texturing |
| 8 | [O Editor Mobile](#parte-8--o-editor-mobile--layout-e-ux) | Layout, gestos, painéis, gizmos, modos |
| 9 | [Sistema No-Code](#parte-9--sistema-no-code-aetherflow) | AetherFlow, VM, compilação, IA assistida |
| 10 | [Ferramentas de criação de conteúdo](#parte-10--ferramentas-de-criação-de-conteúdo-integradas) | Modelagem, sculpt, UV, terreno, materiais |
| 11 | [Scripting em C#](#parte-11--scripting-e-camada-de-código-c) | .NET no dispositivo, hot reload, API |
| 12 | [Multiplayer e serviços](#parte-12--multiplayer-backend-e-serviços) | Netcode, backend, cloud |
| 13 | [Build, publicação e distribuição](#parte-13--build-publicação-e-distribuição) | Compilar um APK/IPA de dentro do celular |
| 14 | [Colaboração e versionamento](#parte-14--colaboração-versionamento-e-nuvem) | VCS visual, co-edição |
| 15 | [Inovações diferenciais](#parte-15--inovações-diferenciais-o-que-ninguém-tem) | As 15 apostas que definem o produto |
| 16 | [ROADMAP COMPLETO](#parte-16--roadmap-completo) | Fases 0→9, etapas, sub-etapas, critérios |
| 17 | [Riscos e mitigações](#parte-17--riscos-e-mitigações) | Matriz de risco técnico e de produto |
| 18 | [Métricas e critérios de qualidade](#parte-18--métricas-e-critérios-de-qualidade) | KPIs técnicos e de produto |
| 19 | [Estrutura de repositório e engenharia](#parte-19--estrutura-de-repositório-e-processo-de-engenharia) | Monorepo, CI, testes |
| 20 | [Modelo de negócio e ecossistema](#parte-20--modelo-de-negócio-e-ecossistema) | Monetização, asset store, comunidade |
| A | [Apêndices](#apêndice-a--registro-de-decisões-arquiteturais-adrs) | ADRs, glossário, alvos de hardware |

---

# PARTE 0 — RESUMO EXECUTIVO

## 0.1 O que é o Aether

Uma **engine de jogos completa cujo ambiente de autoria é um aplicativo mobile nativo**. O usuário abre o app no celular ou tablet e tem, num único produto:

1. **Editor de cena 3D/2D** com viewport em tempo real rodando o mesmo renderizador do jogo final.
2. **Modelagem e escultura 3D integradas** (papel do Blender) — sem sair do app.
3. **Programação visual no-code** (AetherFlow) como cidadão de primeira classe, com paridade funcional real com código.
4. **Scripting em C#** com hot reload, para quem quer descer de nível.
5. **Renderizador Vulkan** de última geração, com GI dinâmica, GPU-driven rendering e upscaling temporal.
6. **Build e publicação** do jogo final (APK/AAB/IPA/WebGPU) diretamente do dispositivo, via serviço de build remoto.

## 0.2 Por que isso não existe ainda

| Barreira | Por que derrubá-la agora é possível |
|---|---|
| GPU móvel fraca demais para editor + jogo | SoCs de 2024-2026 (Snapdragon 8 Gen 3/4, Dimensity 9300+, Apple A17/A18/M-series em iPad) entregam Vulkan 1.3 completo, ray query em hardware e ~2-4 TFLOPs |
| Vulkan fragmentado no Android | Android Baseline Profile 2022/2023 + Vulkan 1.3 como piso removeram 90% do inferno de capabilities |
| Interface de engine é intrinsecamente desktop | É um **dogma não testado**. Ninguém desenhou uma engine touch-first de verdade — só portaram menus de desktop |
| Compilar o jogo exige toolchain pesada | Build remoto em nuvem + AOT incremental resolvem; o celular só precisa gerar o pacote de dados |
| Memória: editor + runtime na mesma RAM | Aparelhos flagship têm 12-24 GB; arquitetura de processo duplo e streaming agressivo cabem confortavelmente |

## 0.3 As cinco apostas centrais

1. **Aposta de UX:** a interação por toque não é um downgrade do mouse — é uma linguagem diferente. Com gestos bimanuais, menus radiais contextuais e gizmos de "arrasto com trava", editar uma cena no celular pode ser *mais rápido* que no desktop para 80% das operações comuns.
2. **Aposta de arquitetura:** núcleo nativo enxuto (C++20) + **toda a camada de engine, editor e gameplay em C#** com .NET moderno. C# dá hot reload, reflexão para o editor e segurança de memória; C++ fica só onde o profiler exige.
3. **Aposta gráfica:** GPU-driven rendering com *render graph* automático e GI dinâmica por sondas — o mesmo pipeline no editor e no jogo, escalando de "modo bateria" a "modo showcase" sem trocar de renderizador.
4. **Aposta no-code:** o AetherFlow não é um brinquedo paralelo ao código. Ele **compila para o mesmo IL** que o C#, com o mesmo desempenho, e qualquer grafo pode ser convertido para C# e vice-versa (round-trip).
5. **Aposta de produto:** o criador **nunca precisa de um PC**. Da primeira caixa até a loja publicada.

## 0.4 Contrato global de desempenho — tick, chunks e renderização

O alvo da engine não é “60 FPS na cena de teste”. O runtime e todo jogo publicado
usam a mesma arquitetura, com cadências de **60, 90 e 120 Hz** e qualidade visual
preservada. Cada cena de amostra é somente um benchmark; nenhuma regra pode consultar
nome de mapa, asset ou modelo de aparelho. O plano executável e as medições ficam em
[`PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md`](PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md).

| Cadência | Frame | CPU (lane) | GPU (lane) | Uso |
|---:|---:|---:|---:|---|
| 120 Hz | 8,33 ms | ≤ 6,50 ms | ≤ 7,33 ms | alvo de desempenho em painel 120 Hz |
| 90 Hz | 11,11 ms | ≤ 8,67 ms | ≤ 9,78 ms | fallback de cadência, não de gráficos |
| 60 Hz | 16,67 ms | ≤ 13,00 ms | ≤ 14,67 ms | piso sustentado do perfil A |

CPU e GPU são lanes sobrepostas, portanto seus números não são somados. A margem
restante pertence a compositor, jitter do SO e variação térmica. Trocar cadência não
autoriza reduzir resolução, materiais, iluminação, geometria ou distância; qualquer
degradação visual é outra política, explícita e opt-in.

O frame global segue obrigatoriamente esta sequência:

1. Amostrar input e relógio uma vez por frame apresentado.
2. Executar zero ou mais passos de **fixed tick** (padrão 60 Hz), com teto de catch-up;
   render a 90/120 Hz interpola estados, sem acelerar física ou gameplay.
3. Consultar versões dos chunks ECS e extrair apenas componentes alterados. Sistemas
   sem alteração não percorrem o mundo e o caminho estável aloca zero bytes.
4. Atualizar uma cena GPU persistente por lotes; não reconstruir listas, buffers e
   descriptors por entidade a cada frame.
5. Fazer visibilidade conservadora (frustum → HZB → LOD), compactação e geração de
   comandos indiretos na GPU. A CPU envia poucos pacotes grandes e bounded.
6. Executar Render Graph, apresentar na taxa escolhida pelo painel e registrar tempos
   CPU, GPU, espera de acquire/present, draws visíveis e descartes de tick.

“Chunk” nunca é um termo genérico: **chunk ECS** é armazenamento SoA de 16 KiB;
**render chunk** é um lote espacial/material persistente voltado a culling/indirect;
**streaming chunk** é uma unidade assíncrona de I/O. Converter um mesh em centenas de
draws de CPU não é chunking válido. Subdivisão só entra quando a compactação/indirect
impede que a granularidade aumente linearmente o custo de CPU.

### 0.5 Contrato global de perfis e validação em hardware

Nenhuma cena possui preset de desempenho próprio. A engine resolve uma única política
global, serializável e versionada, combinando: capabilities reais, calibração curta de
desempenho, Project Settings, override Auto/S/A/B/C/Custom e estado térmico com
histerese. Materiais e luzes descrevem intenção artística; LOD por erro de tela,
sombras, AO/GI, streaming, render scale, memória e cadência consomem budgets do perfil.

Feature detection e desempenho são eixos separados. Uma GPU não vira perfil A apenas
por expor mesh shader/VRS, nem perfil C por pertencer a uma marca. A classificação
final registra GPU, driver, memória, resolução, benchmark e revisão do banco de
dispositivos. Overrides de teste são identificados na telemetria e jamais falsificam
as capabilities reais.

O AVD `Aether-C-Synthetic` planejado (2 cores, 4 GB e resolução da faixa alvo) valida
fallback, lifecycle, memória, imagem e seleção de política no Android Studio. Como o
Emulator usa GPU/CPU do host ou software renderer, ele não certifica FPS, bandwidth,
driver ou térmica de um Galaxy A32/Mali. Gates de desempenho exigem aparelho físico e
o percurso determinístico da mesma cena; Test Lab/Game Loop amplia a matriz. O contrato,
as limitações e a ordem de execução ficam em
[`PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md`](PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md).

## 0.6 Programa global de margem gráfica

O teto exibido de 120 FPS não mede sozinho a capacidade do renderer: em um painel de
120 Hz a aplicação nunca apresenta mais de 120 quadros úteis. A margem para acrescentar
sombras, vegetação, clima, partículas e UI deve ser medida principalmente em **tempo
GPU**, percentis e potência sustentada. A cena-base de floresta é um benchmark global
do renderer, não uma cena autorizada a possuir regras próprias.

No aparelho forte de referência, a rota completa está em 9,13 ms de GPU média e o
hotspot em aproximadamente 10,38 ms. Para deixar de apenas alcançar 120 Hz e passar a
financiar um pico gráfico superior, o contrato adicional desta cena-base é:

| Medida no aparelho forte | Atual | Gate de margem da cena-base |
|---|---:|---:|
| GPU média da rota | 9,13 ms | **≤ 6,20 ms** |
| GPU média do hotspot | ~10,38 ms | **≤ 6,50 ms** |
| GPU p95 da rota | variável | **≤ 7,00 ms** |
| GPU p99 após aquecimento | variável | **≤ 8,00 ms** |
| apresentação média da rota | 95,16/s | **≥ 110/s** |
| pior janela móvel de 600 frames | 84,18/s | **≥ 100/s** |
| CPU p95 do frame | já abaixo do limite | **≤ 3,00 ms** |

Os gates reservam aproximadamente 1,3–2,1 ms dentro do frame de 8,33 ms para qualidade
adicional e variação do sistema. Não são promessas para todo hardware: perfis A/B/C
mantêm seus próprios budgets. São também gates conjuntos; uma média boa não compensa
p99 ruim, imagem divergente, throttling ou estouro de memória.

A redução necessária de aproximadamente 3 ms no hotspot não será atribuída a uma única
técnica. O plano principal abre as seguintes frentes, todas globais, serializáveis e
resolvidas por capabilities/budget:

1. **Verdade de GPU:** marcadores por pass/draw class, captura AGI/APA, counters de
   tiler, fragment, texture, bandwidth, early-Z, cache, occupancy e sincronização.
2. **Visibilidade e geometria:** cena GPU persistente, BVH, LOD/HLOD, impostors,
   HZB same-frame e compactação/indirect inteiramente na GPU; zero readback por frame.
3. **Vegetação e coverage:** semântica explícita de foliage, mips que preservam
   cobertura, mapa de overdraw, agrupamento espacial e representação distante opaca ou
   impostor quando visualmente equivalente.
4. **Pipeline móvel:** Render Graph consumidor, pass culling, attachments transient,
   load/store por uso real, render passes/subpasses nativos e ausência de targets
   intermediários não consumidos. O prepass permanece seletivo e guiado por A/B.
5. **Assets, materiais e shaders:** ASTC/mips por semântica, streaming/residency,
   compressão de vértices/índices, material LOD e redução de registers/fetches apenas
   onde counters provarem retorno.
6. **Iluminação financiada:** probes SH/IBL, luz estática cacheada, CSM cacheada e
   Forward+ entram consumindo a margem comprovada, nunca antes dela existir.
7. **Pacing e sustentação:** apresentação via timing real do Android, recursos isolados
   por frame, ADPF/thermal headroom, perfis S/A/B/C e soak de 30 minutos. Pacing pode
   reduzir jitter, mas não será contabilizado como redução do trabalho GPU.

A ordem de integração é: instrumentar e capturar → cortar tráfego/passes inúteis →
reduzir o conjunto visível e sua representação → atacar overdraw/bandwidth dominante →
validar pacing/térmica → reinvestir a margem em qualidade. Cada etapa só é promovida
após A/B na rota em movimento, hotspot, diff visual, Adreno e Mali; hipóteses de ganho
não são somadas antes da medição.

---

# PARTE 1 — ANÁLISE COMPETITIVA E TESE DO PRODUTO

## 1.1 O que roubar de cada engine

### Godot — a arquitetura conceitual
| Elemento | Adotar? | Observação |
|---|---|---|
| Árvore de nós (Node/Scene) com composição | **Sim, adaptado** | Modelo mental mais fácil de ensinar que ECS puro. Vira uma *fachada* sobre o ECS |
| Cenas aninhadas / instanciamento de cena | **Sim** | Melhor sistema de reuso de qualquer engine. Base para "prefabs vivos" |
| Sinais (signals) para desacoplamento | **Sim** | Casam perfeitamente com no-code: um sinal é um pino de saída |
| Servidores (RenderingServer, PhysicsServer) | **Sim** | Camada de indireção que permite trocar backends e rodar headless |
| Tudo é um recurso (.tres) | **Sim, com binário** | Texto para diff, binário para runtime |
| GDScript | Não | C# + AetherFlow cobrem os dois públicos |
| Editor em modo "imediato" próprio | Parcial | A ideia de "o editor é um jogo feito na engine" é ouro para dogfooding |

### Unity — o modelo de trabalho
| Elemento | Adotar? |
|---|---|
| Componentes plugáveis com inspector gerado por reflexão | **Sim** — pilar do editor |
| Prefabs com overrides e variantes | **Sim** — versão melhorada, com merge visual |
| Asset Store / ecossistema | **Sim** — desde o dia 1 do beta |
| Timeline / Cinemachine (cinemática e câmera) | **Sim** — a timeline é o elemento de UI que melhor funciona em touch |
| ScriptableObjects (dados como asset) | **Sim** — essencial para design orientado a dados |
| Burst / Job System / DOTS | **Sim, como padrão e não como opção** |
| Fragmentação de pipelines (Built-in/URP/HDRP) | **Não** — um único pipeline escalável. É o maior erro estratégico da Unity |

### Unreal Engine — a régua técnica
| Elemento | Adotar? | Adaptação mobile |
|---|---|---|
| Blueprints | **Sim, superado** | AetherFlow: compilação para IL nativo, não interpretação |
| Nanite (geometria virtualizada) | **Sim, reduzido** | "MicroMesh": clusterização + LOD contínuo por *meshlets*, sem software rasterizer completo |
| Lumen (GI dinâmica) | **Sim, reduzido** | "GlowField": DDGI + SDF tracing, sem HW ray tracing obrigatório |
| Virtual Shadow Maps | **Sim** | Versão em cache com atlas esparso |
| Material Editor por nós | **Sim** | Um dos melhores usos de nós que existe; ótimo em touch |
| World Partition / streaming de mundo | **Sim** | Obrigatório num aparelho com orçamento de RAM |
| Sequencer | **Sim** | Unificado com a Timeline |
| MetaHuman / Chaos | Fase tardia | Não é diferencial no mobile |

### Blender — a criação de conteúdo
| Elemento | Adotar? | Adaptação touch |
|---|---|---|
| Modelagem poligonal (extrude, bevel, loop cut, knife) | **Sim** | Operadores por gesto + "modo de precisão" numérico |
| Modificadores não-destrutivos empilháveis | **Sim, central** | O empilhamento de modificadores é *perfeito* para uma lista tocável |
| Geometry Nodes | **Sim** | Procedural = menos trabalho manual num celular. Diferencial enorme |
| Sculpt com dynamic topology | **Sim** | O dedo é um pincel melhor que o mouse. Aqui o mobile **ganha** do desktop |
| UV unwrap (marcação de costuras + unwrap angular) | **Sim** | Auto-UV por IA como padrão, manual como opção |
| Shader/Compositor por nós | **Sim** | Compartilha o editor de nós com material e AetherFlow |
| Rigging + weight painting | **Sim** | Auto-rig como padrão (estilo Mixamo/Rignet) |
| Sistema de "modos" (Object/Edit/Sculpt/Paint) | **Sim, essencial** | Resolve o problema de densidade de UI no celular |

### Fontes fora do óbvio
| Produto | Lição |
|---|---|
| **Dreams (Media Molecule)** | Prova que autoria complexa com controle impreciso funciona. Menus radiais, "grab and pull", tudo com feedback contínuo |
| **Roblox Studio** | O loop social publicar→jogar→remixar é mais motivador que qualquer feature técnica |
| **Procreate / Nomad Sculpt** | O padrão-ouro de UI criativa em touch. Nomad Sculpt prova que escultura profissional no iPad é viável |
| **Figma** | Co-edição em tempo real e componentes; o modelo de "multiplayer para ferramentas" |
| **Scratch / Blockly** | Ensino de lógica: encaixe físico impede erro de sintaxe. Nível de entrada do AetherFlow |
| **Shadertoy / Bevy / wgpu** | Arquitetura moderna de render graph e ECS de referência |

## 1.2 Posicionamento

```
                     PODER TÉCNICO
                          ▲
              Unreal ●    │    ● Unity
                          │
         Godot ●          │        ◆ AETHER
                          │       (alvo)
   ───────────────────────┼───────────────────────►
   Difícil                │                 Fácil
                          │                ACESSIBILIDADE
        Blender ●         │   ● Roblox Studio
                          │
                          │   ● Dreams
                          │   ● GDevelop / Buildbox
```

**Tese:** o quadrante superior direito (alto poder + alta acessibilidade) está vazio porque ninguém aceitou o custo de reprojetar a interface do zero. O mobile obriga a esse redesenho — e é exatamente por isso que ele destrava o quadrante.

## 1.3 Anti-objetivos (o que o Aether NÃO será)

- Não será uma engine AAA de console. O alvo é *mobile-native*, com exportação secundária para desktop/web.
- Não terá múltiplos pipelines de renderização incompatíveis.
- Não terá uma linguagem de script proprietária.
- Não separará "versão para iniciantes" e "versão profissional" — é o mesmo produto com camadas de profundidade progressiva.
- Não exigirá conexão permanente. Tudo funciona offline exceto build em nuvem e colaboração.

---

# PARTE 2 — USUÁRIOS E REQUISITOS

## 2.1 Personas

| # | Persona | Contexto | Precisa de | Métrica de sucesso |
|---|---|---|---|---|
| **P1** | **O Iniciante Absoluto** (13-20 anos, só tem celular) | Nunca programou. Descobriu o app numa rede social | Templates jogáveis, no-code por blocos, tutorial interativo, publicação em 1 toque | Publica algo jogável em **< 45 min** na primeira sessão |
| **P2** | **O Criador de Conteúdo Solo** (20-35) | Já usou Unity/Godot no PC, quer prototipar no ônibus | Paridade de conceitos, importação de FBX/glTF, C#, Git | Consegue continuar no celular um projeto começado no PC |
| **P3** | **O Artista 3D** | Usa Blender/Nomad, quer ver a arte no jogo | Sculpt, retopologia, UV, material por nós, preview PBR fiel | Modela → texturiza → coloca na cena sem exportar nada |
| **P4** | **O Estúdio Pequeno** (3-10 pessoas) | Produção comercial | Colaboração, VCS, profiler, build reproduzível, source access | Ship de um título comercial de médio porte |
| **P5** | **O Educador** | Ensina lógica/jogos numa escola sem laboratório | Modo sala de aula, projetos compartilháveis, sandbox seguro | Turma de 30 alunos só com celulares |
| **P6** | **O Modder / Remixador** | Quer pegar o jogo de outro e mudar | Projetos abertos, fork em 1 toque, sandbox de execução | Fork → alteração → republicação em minutos |

## 2.2 Requisitos funcionais (visão macro)

| ID | Requisito | Prioridade |
|---|---|---|
| RF-01 | Editar cena 3D/2D com viewport em tempo real no dispositivo | P0 |
| RF-02 | Renderizar via Vulkan 1.3 com paridade editor↔runtime | P0 |
| RF-03 | Programação visual completa (AetherFlow) | P0 |
| RF-04 | Scripting C# com hot reload no dispositivo | P0 |
| RF-05 | Importar glTF/FBX/OBJ/USD, PNG/JPG/EXR/HDR, WAV/OGG | P0 |
| RF-06 | Física 3D rígida + 2D + character controller | P0 |
| RF-07 | Animação esquelética, blend trees, state machine | P0 |
| RF-08 | Áudio espacial com mixer e efeitos | P0 |
| RF-09 | UI de jogo (canvas, layout responsivo, input) | P0 |
| RF-10 | Build e publicação para Android/iOS a partir do dispositivo | P0 |
| RF-11 | Modelagem poligonal e escultura integradas | P1 |
| RF-12 | Materiais por nós + shaders customizados | P1 |
| RF-13 | Nós de geometria procedural | P1 |
| RF-14 | GI dinâmica e sombras de alta qualidade | P1 |
| RF-15 | Multiplayer (netcode autoritativo + relay) | P1 |
| RF-16 | Colaboração em tempo real e versionamento | P1 |
| RF-17 | Terreno, vegetação, mundo aberto com streaming | P2 |
| RF-18 | Asset store integrada | P2 |
| RF-19 | XR (AR/VR) | P2 |
| RF-20 | Assistente de IA integrado (geração de lógica, assets, debug) | P1 |

## 2.3 Requisitos não-funcionais (as restrições que definem tudo)

| ID | Requisito | Alvo | Por quê |
|---|---|---|---|
| RNF-01 | **Tamanho do app** | < 350 MB base, módulos sob demanda | Instalação em rede móvel |
| RNF-02 | **Tempo de abertura a frio** | < 3 s até a tela de projetos | Sessões mobile são curtas |
| RNF-03 | **Abrir um projeto médio** | < 5 s | Idem |
| RNF-04 | **Taxa do editor** | 60 fps estáveis com cena de 500k triângulos | Sensação de fluidez |
| RNF-05 | **Orçamento térmico** | < 4 W sustentados no editor; sem throttle antes de 20 min | Celular esquenta e cai de clock |
| RNF-06 | **RAM do editor** | < 1.5 GB para projeto médio | Android mata processos gulosos |
| RNF-07 | **Consumo de bateria** | < 12%/hora em edição típica | Sessão de 4h sem carregador |
| RNF-08 | **Latência de toque** | < 30 ms toque→pixel | Limiar de "responde ao meu dedo" |
| RNF-09 | **Hot reload de script** | < 1.5 s | Loop de iteração |
| RNF-10 | **Salvamento** | Contínuo, incremental, à prova de kill do SO | Nunca perder trabalho |
| RNF-11 | **Piso de hardware** | Vulkan 1.1 + ASTC + 6 GB RAM (modo reduzido) | Alcance de mercado |
| RNF-12 | **Alvo de hardware** | Vulkan 1.3, 8+ GB RAM, GPU de 2023+ | Onde o pipeline completo roda |
| RNF-13 | **Acessibilidade** | Alvos de toque ≥ 44 dp, escala de fonte, alto contraste, modo de uma mão | Inclusão real |
| RNF-14 | **Offline-first** | 100% das funções de edição sem rede | Conectividade instável |
| RNF-15 | **Determinismo** | Simulação determinística por fixed-step | Netcode e replays |

## 2.4 Alvos de hardware

| Nível | Exemplo de SoC | Perfil gráfico | Comportamento |
|---|---|---|---|
| **S — Showcase** | SD 8 Gen 3/4, Dimensity 9300+, A17+/M2+ | GI dinâmica, sombras virtuais, MicroMesh, upscaling temporal, 120 Hz | Tudo ligado |
| **A — Alvo** | SD 8 Gen 1/2, Dimensity 8200, A15+ | GI por sondas em cache, sombras em cascata, upscaling, 60 Hz | Padrão |
| **B — Base** | SD 7 Gen 1, Dimensity 7050, A13 | Forward+, lightmaps assados, sem GI dinâmica, 60 Hz | Reduzido |
| **C — Mínimo** | SD 680, Helio G99 | Forward simples, sem pós-processamento pesado, 30 Hz, viewport com resolução dinâmica | Apenas edição leve e 2D |

---

# PARTE 3 — ARQUITETURA GERAL DO SISTEMA

## 3.1 Decisão de linguagens (a escolha mais importante do projeto)

O usuário pediu **C#**. Isso é viável e desejável — mas com uma fronteira bem desenhada.

### Estratégia escolhida: **"C# em cima, C++ embaixo, fronteira fina"**

```
┌──────────────────────────────────────────────────────────────┐
│  CAMADA DE PRODUTO — 100% C#                                 │
│  Editor, ferramentas, inspector, AetherFlow, gameplay API,   │
│  importadores, UI, lógica de asset pipeline                  │
│  .NET 10 · NativeAOT (iOS) / Mono-AOT + interpretador (Android)│
└───────────────────────────┬──────────────────────────────────┘
                            │  fronteira: structs blittable,
                            │  ponteiros, zero marshalling
┌───────────────────────────┴──────────────────────────────────┐
│  CAMADA DE ENGINE — C# de alto desempenho                    │
│  ECS, cena, animação, cull, render graph, gerenciamento      │
│  Span<T>, ref struct, SIMD (System.Numerics.Vector<T>),      │
│  unsafe, alocação em arenas                                  │
└───────────────────────────┬──────────────────────────────────┘
                            │
┌───────────────────────────┴──────────────────────────────────┐
│  NÚCLEO NATIVO — C++20 (~15% do código total)                │
│  RHI Vulkan · compilação de shaders · física (solver) ·      │
│  mixer de áudio · codecs · job scheduler · alocadores ·      │
│  geometria pesada (booleanas, remesh, unwrap)                │
└──────────────────────────────────────────────────────────────┘
```

### Justificativa técnica

| Preocupação | Resposta |
|---|---|
| "C# tem GC, vai engasgar" | O .NET moderno tem GC em modo servidor/concorrente, mas a regra do projeto é **zero alocação no frame loop**: `struct`, `Span<T>`, `stackalloc`, pools e arenas. Bevy e Unity DOTS provam o modelo; a diferença é disciplina, não linguagem |
| "iOS não permite JIT" | NativeAOT do .NET compila C# para binário nativo estático. Já é a rota oficial de .NET no iOS |
| "Hot reload sem JIT?" | No Android: Mono com interpretador para código de usuário → hot reload real. No iOS: hot reload via **interpretador de IL embarcado** ou via AetherFlow (que é dado, não código) |
| "Vulkan em C#?" | Bindings gerados do `vk.xml` (padrão Silk.NET/Vortice). Funciona, mas o *hot path* de submissão de comandos fica em C++ para evitar overhead de P/Invoke por draw call |
| "Por que não 100% C++?" | Perderíamos: reflexão para gerar inspectors, hot reload, segurança de memória, produtividade, e a extensibilidade por plugins do usuário |
| "Por que não 100% C#?" | **[atalho viável]** É possível — e deve ser reavaliado na Fase 3. Custo: overhead de interop com Vulkan por comando e ausência de bibliotecas maduras de física/codec |

### Regras de fronteira (obrigatórias)

1. Nenhuma chamada nativa **por objeto** ou **por draw call**. Chamadas nativas são sempre **em lote** (`SubmitDrawList(Span<DrawCommand>)`).
2. Todo dado que cruza a fronteira é `blittable` (sem marshalling, sem `string` — usa `Utf8Span` + IDs).
3. O nativo **nunca** chama de volta para C# em loop; usa filas de eventos.
4. Toda API nativa é declarada num IDL próprio (`.aidl`) que **gera** os dois lados — não se escreve binding à mão.
5. Orçamento: máximo **200 chamadas nativas por frame**.

## 3.2 Diagrama de módulos

```
╔═══════════════════════════════════════════════════════════════════════╗
║                        APLICAÇÃO AETHER (mobile)                      ║
╠═══════════════════════════════════════════════════════════════════════╣
║  PROCESSO EDITOR                    ║  PROCESSO PLAY (sandbox)        ║
║  ────────────────────               ║  ──────────────────────         ║
║  Aether.Editor.Shell    (UI)        ║  Aether.Runtime                 ║
║  Aether.Editor.Viewport             ║  (mesma engine, sem tooling)    ║
║  Aether.Editor.Inspector            ║  Isolado: crash não derruba     ║
║  Aether.Editor.Flow     (no-code)   ║  o editor                       ║
║  Aether.Editor.Modeling             ║                                 ║
║  Aether.Editor.Assets               ║  ◄── IPC: memória compartilhada ║
╠═════════════════════════════════════╩═════════════════════════════════╣
║                         CAMADA DE ENGINE (C#)                         ║
║ ┌───────────┬───────────┬───────────┬───────────┬───────────┐         ║
║ │  Scene    │ Rendering │  Physics  │ Animation │   Audio   │         ║
║ │  (ECS +   │ (RenderG. │ (proxy)   │ (skel.,   │ (graph,   │         ║
║ │   nós)    │  cull,    │           │  blend,   │  buses)   │         ║
║ │           │  batch)   │           │  IK)      │           │         ║
║ ├───────────┼───────────┼───────────┼───────────┼───────────┤         ║
║ │   Input   │    UI     │  Network  │   Asset   │  Scripting│         ║
║ │ (gestos)  │ (retido+  │ (replic., │ (import,  │  (C# +    │         ║
║ │           │ imediato) │  predição)│  stream)  │  Flow VM) │         ║
║ └───────────┴───────────┴───────────┴───────────┴───────────┘         ║
╠═══════════════════════════════════════════════════════════════════════╣
║                    NÚCLEO / SERVIÇOS (C# + C++)                       ║
║  Job System · Memória (arenas, pools) · Reflection · Serialização     ║
║  Math/SIMD · Log/Telemetria · Eventos · Plugins · Sandbox             ║
╠═══════════════════════════════════════════════════════════════════════╣
║                   CAMADA DE PLATAFORMA (C++)                          ║
║  RHI Vulkan · Compilação de shaders · Áudio (AAudio/AudioUnit) ·      ║
║  Arquivos · Sensores · Toque · Rede · Térmica/Energia · Codecs        ║
╠═══════════════════════════════════════════════════════════════════════╣
║        ANDROID (NDK, Vulkan, AAudio)  │  iOS/iPadOS (Vulkan/MoltenVK) ║
╚═══════════════════════════════════════════════════════════════════════╝
```

## 3.3 Modelo de dados: ECS por baixo, Nós por cima

O maior erro possível seria escolher entre "árvore de nós amigável" e "ECS rápido". O Aether usa os **dois**, em camadas:

```
  Como o usuário vê             Como a engine executa
  ─────────────────             ─────────────────────
  Player (Node)          ───►   Entity #4821
   ├ Transform                   ├ TransformComponent   (arquétipo A)
   ├ MeshRenderer                ├ MeshComponent        (arquétipo A)
   ├ RigidBody                   ├ RigidBodyComponent   (arquétipo A)
   ├ PlayerController (Flow)     └ FlowScriptComponent  (arquétipo A)
   └ Camera (Node filho)  ───►   Entity #4822 [Parent=#4821]
```

- **Node** é uma *fachada* — um `readonly struct` com um `EntityId` + métodos de conveniência. Não existe em memória como objeto.
- **Componentes** vivem em *chunks* contíguos por arquétipo (16 KB, alinhados a cache line).
- **Hierarquia** é um componente (`Parent`, `Children` como span em arena), não uma estrutura de ponteiros.
- **Sistemas** rodam em *jobs* paralelos com dependências declaradas (leitura/escrita por tipo de componente).
- **Consultas** são compiladas e cacheadas: `Query<Transform, Mesh>.Without<Hidden>()`.

**Benefício direto no mobile:** iteração linear em memória contígua reduz *cache miss* — o gargalo dominante em CPUs ARM com cache L2 pequeno. Ganho medido típico: 3-6× sobre um grafo de objetos com ponteiros.

## 3.4 Modelo de threads e energia

```
Thread Principal (UI)   ─── input, gestos, animação de UI, orquestração ─── 60/120 Hz
Thread de Simulação     ─── ECS, física (fixed 60 Hz), animação, scripts
Thread de Render        ─── construção do render graph, gravação de command buffers
Pool de Workers (N-2)   ─── jobs: cull, batch, import, compressão, IA, geometria
Thread de I/O           ─── streaming de assets, salvamento incremental (nunca bloqueia)
Thread de Áudio         ─── prioridade real-time, buffer curto, NUNCA aloca
```

**Governador térmico (subsistema `PowerGovernor`) — diferencial mobile:**

| Estado térmico | Ações automáticas |
|---|---|
| Nominal | Tudo no alvo; 120 Hz se a tela suportar |
| Leve (fair) | Cap de 60 Hz, escala de resolução 0.85, reduz sondas de GI |
| Moderado | Cap de 45 Hz, escala 0.7, sombras em meio-resolução, desliga SSR |
| Severo | Cap de 30 Hz, escala 0.6, GI congelada, viewport só redesenha em mudança |
| Crítico | Modo "papel": viewport estático, editor continua responsivo, aviso ao usuário |

O `PowerGovernor` lê `thermalStatus` (Android) / `ProcessInfo.thermalState` (iOS), nível de bateria, e se está carregando. **É um sistema de primeira classe, não um remendo.**

## 3.5 Arquitetura de processos (Editor vs Play)

Rodar o jogo do usuário dentro do processo do editor é como a Unity faz — e é a razão de metade dos crashes de editor. No mobile isso é fatal (o SO mata o app inteiro).

```
  ┌──────────────┐   memória compartilhada (ashmem/mmap)   ┌──────────────┐
  │   EDITOR     │◄──────────────────────────────────────► │  PLAY (child)│
  │              │   · snapshot da cena (zero-copy)        │              │
  │  UI, tooling │   · fila de comandos                    │  runtime puro│
  │              │   · canal de telemetria/log             │  sandbox     │
  │              │   · surface compartilhada (AHardware-   │  sem escrita │
  │              │     Buffer) para exibir o jogo dentro    │  em disco    │
  │              │     do viewport do editor               │              │
  └──────────────┘                                         └──────────────┘
```

- Crash no jogo do usuário → só o processo filho morre → editor mostra o stack trace e o estado.
- Permite **play-in-editor sem sair do editor** e **múltiplas instâncias** (para testar multiplayer local).
- No iOS, onde múltiplos processos são restritos, degrada para **isolamento por domínio de aplicação .NET (AssemblyLoadContext) + guarda de exceções**.

---

# PARTE 4 — NÚCLEO (CORE LAYER)

## 4.1 Gestão de memória

Num celular, a diferença entre uma engine boa e ruim é quase toda alocação de memória.

| Subsistema | Estratégia |
|---|---|
| **Arena por frame** | Buffer linear resetado a cada frame. Todo dado temporário (listas de cull, comandos de draw) vive aqui. Custo de free: zero |
| **Pools por tipo** | Objetos de vida longa (entidades, recursos) em pools de tamanho fixo com free-list |
| **Chunks de ECS** | Blocos de 16 KB alinhados, alocados de um slab dedicado |
| **Alocador de GPU** | VMA (Vulkan Memory Allocator) com sub-alocação, budgets por categoria e desfragmentação em background |
| **Cache de assets** | LRU com orçamento explícito e resposta a `onTrimMemory`/`didReceiveMemoryWarning` |
| **GC do .NET** | Modo *concurrent workstation*, `GCSettings.LatencyMode = SustainedLowLatency` no play mode. Alvo: **zero Gen0 durante gameplay** |
| **Detector de alocação** | Em builds de debug, um analisador Roslyn falha o build se um método marcado `[NoAlloc]` alocar |

**Orçamento de memória (projeto médio, aparelho classe A — 8 GB):**

| Categoria | Orçamento |
|---|---|
| Código + runtime .NET | 180 MB |
| UI do editor + fontes/ícones | 120 MB |
| Cena carregada (ECS + metadados) | 250 MB |
| Texturas (GPU) | 500 MB |
| Malhas (GPU) | 200 MB |
| Buffers de render (G-buffer, sombras, GI) | 350 MB |
| Cache de assets em RAM | 300 MB |
| Undo/histórico | 100 MB |
| **Total** | **~2.0 GB** (com folga sobre o RNF-06 para projetos grandes) |

## 4.2 Job System

```csharp
// Modelo declarativo com dependências automáticas por tipo de componente
var cullJob   = Jobs.Schedule<CullSystem>(dependsOn: transformJob);
var batchJob  = Jobs.Schedule<BatchSystem>(dependsOn: cullJob);
var animJob   = Jobs.ScheduleParallel<SkinningSystem>(batchSize: 64);
Jobs.Complete(batchJob, animJob);
```

- **Work-stealing** com filas por thread.
- **Afinidade de núcleo**: em ARM big.LITTLE, jobs de latência crítica vão para os núcleos *big*; import/compressão vão para os *little* (economiza bateria e evita throttle).
- **Detecção de corrida em tempo de compilação**: sistemas declaram `[ReadOnly]`/`[WriteOnly]` por componente; o scheduler valida e o analisador Roslyn avisa.
- **Job graph visualizável** no profiler (uma linha por thread, timeline tocável).

## 4.3 Reflexão, serialização e a espinha dorsal do editor

Todo o editor — inspector, undo, salvamento, no-code, diff, colaboração — depende de **um único sistema de metadados**.

```csharp
[Component]
public partial struct Light {
    [Range(0, 20)] [Tooltip("Intensidade em EV")]
    public float Intensity;

    [ColorHDR] public Color Color;

    [ShowIf(nameof(Type), LightType.Spot)] [Range(1, 179)]
    public float SpotAngle;

    public LightType Type;
}
```

Um **gerador de código (Source Generator)** produz em tempo de compilação:
1. Serializador binário (sem reflexão em runtime → rápido e AOT-safe).
2. Serializador de texto (para diff/merge no VCS).
3. Descritor de UI para o inspector (widgets, faixas, condições).
4. Registro de propriedades para undo/redo e para o AetherFlow (pinos de get/set).
5. Interpolador para animação e para replicação de rede.

**Regra de ouro:** nenhuma característica do editor pode exigir código de UI escrito à mão para um componente novo. Adicionar um componente = adicionar um struct.

## 4.4 Undo/Redo e persistência

- **Modelo de comandos**: toda mutação passa por `IEditCommand` com `Do`/`Undo` e um *patch* mínimo.
- **Compressão de comandos**: arrastar um gizmo gera um comando, não 300 (coalescência por janela de tempo + gesto).
- **Undo transacional**: uma operação de modelagem (ex.: bevel em 40 arestas) é uma transação atômica.
- **Persistência contínua**: um WAL (write-ahead log) em disco grava cada comando. Se o Android matar o app, na reabertura o projeto volta exatamente ao último toque. **Nunca existe "você perdeu seu trabalho".**
- **Snapshots**: a cada N comandos ou 60 s, um snapshot completo comprimido (para truncar o WAL).

## 4.5 Matemática e SIMD

- Tipos: `float2/3/4`, `quaternion`, `float4x4`, `AffineTransform`, `Bounds`, `Ray`, `Plane`, `half`, `fixed16`.
- Implementação com `System.Runtime.Intrinsics` (NEON no ARM) e fallback escalar.
- **Transformes em precisão dupla opcional** para mundos grandes (origem flutuante como padrão: a câmera é sempre a origem, o mundo se move).
- Biblioteca de *swizzle* estilo shader (`v.xzy`) via geração de código.

---

# PARTE 5 — RENDERIZAÇÃO — VULKAN DE ÚLTIMA GERAÇÃO

## 5.1 Princípio norteador: TBDR não é desktop

GPUs móveis (Adreno, Mali, PowerVR, Apple) são **Tile-Based Deferred Renderers**. Escrever um renderizador de desktop e "otimizar depois" é a causa raiz de 90% das engines mobile ruins.

| Regra móvel | Consequência de projeto |
|---|---|
| Largura de banda de memória é o gargalo, não FLOPs | Minimizar leitura/escrita de render targets acima de tudo |
| Cada `vkCmdBeginRenderPass` é uma **resolução de tile** | Fundir passes agressivamente; usar **subpasses** e `VK_KHR_dynamic_rendering` com atenção |
| `LOAD_OP_DONT_CARE` e `STORE_OP_DONT_CARE` são grátis, `LOAD` custa caro | Nunca carregar um alvo que será totalmente sobrescrito |
| Anexos *memoryless* (`LAZILY_ALLOCATED`) nunca tocam a DRAM | G-buffer inteiro pode viver só na memória de tile |
| Deferred clássico (G-buffer em DRAM) é péssimo | Usar **deferred em tile** (subpass) ou **Forward+ clusterizado** |
| Alternância de pipeline state é cara | Ordenar draws por PSO, usar pipeline cache persistido em disco |
| Precisão `mediump` (float16) é ~2× mais rápida | Shaders escritos com precisão explícita desde o início |

## 5.2 RHI (Render Hardware Interface)

Camada fina em C++ sobre Vulkan 1.3, com abstração para futuro WebGPU/Metal direto.

**Recursos Vulkan obrigatórios (piso Vulkan 1.3 / Android Baseline Profile 2023):**

| Extensão / feature | Uso |
|---|---|
| `dynamic_rendering` | Elimina objetos de render pass/framebuffer; simplifica o render graph |
| `synchronization2` | Barreiras mais precisas e baratas |
| `timeline_semaphore` | Sincronização CPU↔GPU multi-fila sem fences manuais |
| `descriptor_indexing` (bindless) | **Base do GPU-driven rendering**: um array global de texturas/buffers |
| `buffer_device_address` | Ponteiros na GPU; estruturas de dados encadeadas em shader |
| `push_descriptor` | Descriptors por draw sem alocar sets |
| `shader_float16_int8` | Meia precisão em compute e fragment |
| `subgroup` ops | Reduções e prefix-sum rápidos em cull/light binning |
| `fragment_shading_rate` (VRS) | Reduz shading em áreas de baixa frequência / periferia |
| `sampler_ycbcr` | Vídeo e câmera (AR) |
| **Opcionais (detectados)** | `ray_query` (RT em hardware: Adreno 7xx/Mali-G7xx/Apple), `mesh_shader`, `fragment_density_map`, `EXT_shader_object` |

**Design do RHI:**
- Objetos imutáveis criados a partir de descritores hasheados → cache global (PSO, layouts, samplers).
- **Bindless por padrão**: shaders acessam `textures[materialIndex]`, sem descriptor set por objeto.
- Command buffers gravados em paralelo por *secondary command buffers* por thread.
- Pipeline cache serializado em disco e **pré-aquecido** na primeira execução (evita stutter de compilação de shader).
- Modo de validação com camadas Vulkan ativável em runtime (essencial e frequentemente esquecido).

## 5.3 Render Graph (o coração do renderizador)

Passes declaram entradas/saídas; o grafo resolve barreiras, aliasing de memória e fusão de passes automaticamente.

```csharp
graph.AddPass("Depth Prepass", pass => {
    pass.WriteDepth(depth, LoadOp.Clear);
    pass.Execute = ctx => ctx.DrawIndirect(opaqueList);
});

graph.AddPass("GBuffer", pass => {
    pass.ReadDepth(depth);
    pass.Write(albedo, normal, material, Memoryless: true); // vive só no tile
    pass.MergeWith("Lighting");                             // fusão de subpass
    pass.Execute = ctx => ctx.DrawIndirect(opaqueList);
});

graph.AddPass("Lighting", pass => {
    pass.ReadSubpassInput(albedo, normal, material);
    pass.Read(shadowAtlas, giVolume, lightClusters);
    pass.Write(hdrColor);
    pass.Execute = ctx => ctx.FullscreenQuad(deferredLightingShader);
});
```

**O que o render graph faz automaticamente:**
1. Ordena topologicamente e **elimina passes cujo resultado ninguém lê**.
2. Insere barreiras `synchronization2` mínimas.
3. Faz **aliasing de memória transitória** (dois alvos com vidas disjuntas dividem a mesma alocação) — economia típica de 30-40% de VRAM.
4. Escolhe `LOAD_OP`/`STORE_OP` corretos por análise de uso.
5. Marca anexos como *memoryless* quando só são lidos dentro do mesmo render pass.
6. Funde passes compatíveis em subpasses (crítico no TBDR).
7. Produz um **diagrama visual navegável no editor** — o usuário vê o próprio frame.

## 5.4 O pipeline de frame completo

```
┌─ CPU ────────────────────────────────────────────────────────────────┐
│ 1. Coleta de visibilidade grosseira (BVH em CPU, frustum + oclusão   │
│    por HZB do frame anterior)                                        │
│ 2. Preenche buffers de instância (bindless: matriz + índice de mat.) │
└──────────────────────────────────────────────────────────────────────┘
┌─ GPU ────────────────────────────────────────────────────────────────┐
│ 3. COMPUTE: Cull por meshlet (frustum, backface de cluster, HZB)     │
│    → gera lista de draws indiretos (GPU-driven, zero CPU por objeto) │
│ 4. Depth Prepass (só posição, early-Z alimenta o HZB do próximo)     │
│ 5. COMPUTE: Construção do HZB (mip chain de profundidade)            │
│ 6. COMPUTE: Clusterização de luzes (froxels 3D, 16×16×24)            │
│ 7. Sombras: atlas virtual esparso — só re-renderiza tiles inválidos  │
│ 8. GBuffer + Lighting FUNDIDOS em subpass (memoryless)               │
│    · BRDF: GGX multiscatter, Burley diffuse, clear coat, anisotropia │
│    · Sheen, subsurface, iridescência, transmissão                     │
│ 9. GI: GlowField (sondas DDGI) + SSAO/GTAO + reflexões               │
│10. Forward transparente (ordenado, com OIT opcional por weighted-blend)│
│11. Volumétricos (froxel: fog, god rays, nuvens) — meia resolução     │
│12. TAA + Upscaling (AetherSR / FSR / MetalFX / Snapdragon GSR)       │
│13. Pós: motion blur, DOF (bokeh separável), bloom (dual filter),     │
│    exposição automática, tonemapping (AgX), color grading (LUT 3D),  │
│    vinheta, aberração, film grain                                    │
│14. UI + composição final → swapchain (HDR10 se disponível)           │
└──────────────────────────────────────────────────────────────────────┘
```

## 5.5 GPU-Driven Rendering + MicroMesh (geometria virtualizada)

**Problema:** artistas trazem modelos de 2M de triângulos do Blender/scan. Celular não aguenta e nem toda LOD manual salva.

**Solução — MicroMesh (Nanite reduzido para mobile):**

| Etapa | Descrição |
|---|---|
| **Pré-processo (import)** | Malha é particionada em **meshlets** de 64-128 triângulos. Meshlets são agrupados hierarquicamente e simplificados (meshoptimizer + quadric error), formando um DAG de LOD com erro de tela conhecido |
| **Runtime — seleção** | Um compute shader percorre o DAG e escolhe, por cluster, o nível cujo erro projetado < 1 px. Resultado: densidade de triângulos constante na tela, independente da malha original |
| **Runtime — cull** | Por cluster: frustum, cone de normais (backface), oclusão por HZB de dois passos |
| **Runtime — desenho** | `vkCmdDrawIndexedIndirectCount` ou **mesh shaders** onde houver. Zero draw calls por objeto na CPU |
| **Diferença para Nanite** | Sem rasterizador em software (caro demais em ARM) e sem streaming de páginas de geometria na v1 — usa-se residência por nível com prefetch |

**Impacto:** o usuário importa o modelo pesado e **simplesmente funciona**. Isso remove a maior barreira de conteúdo para criadores amadores — que é exatamente a persona P1/P3.

## 5.6 Iluminação global — "GlowField"

Estratégia em três níveis, escolhida automaticamente pelo perfil de hardware:

| Nível | Técnica | Custo | Quando |
|---|---|---|---|
| **1 — Assado** | Lightmaps + probes irradiance, assados **em nuvem** (o celular não assa) | ~0 ms | Perfil B/C, cenas estáticas |
| **2 — Sondas dinâmicas (padrão)** | **DDGI**: grade de sondas cascateada centrada na câmera, cada sonda com octaedro de irradiância 8×8 + visibilidade. Atualização amortizada (N sondas/frame) com ray marching em **SDF global** da cena | 1.5-3 ms | Perfil A |
| **3 — Traçado** | Mesmas sondas, mas raios via `ray_query` em BVH de hardware + reflexões traçadas com denoise temporal | 4-7 ms | Perfil S |

**Componentes de suporte:**
- **SDF global da cena**: volume esparso (bricks 8³) gerado em compute, atualizado incrementalmente quando geometria muda. Serve GI, reflexões, sombras de contato, colisão de partículas e efeitos.
- **Reflexões**: híbrido — SSR (screen-space) → falha para *reflection probes* paralax-corrigidas → falha para GI de sondas. Em perfil S, ray query.
- **Oclusão de ambiente**: GTAO em meia resolução com upsample bilateral + oclusão de "bent normal" alimentando a especular.
- **Sombras**: **Virtual Shadow Map** — atlas esparso de páginas 128×128, com cache: uma luz estática sobre geometria estática **nunca re-renderiza**. Filtro PCSS para penumbra suave.

## 5.7 Materiais e shaders

- **Shader Graph por nós** (compartilha o editor de nós com o AetherFlow), compilando para um único arquivo `.aether-shader` intermediário.
- **Compilação**: Slang/HLSL → SPIR-V → cache de variantes. Compilação em nuvem para variantes pesadas; no dispositivo só o que muda.
- **Permutações controladas**: sistema de *shader features* com contagem máxima por material — impede a explosão combinatória que travou a Unity.
- **Modelo de material (uber shader parametrizado):** base metálica-rugosa + camadas opcionais: clear coat, anisotropia, sheen (tecido), subsurface (pele/folhagem), transmissão (vidro), iridescência, emissão HDR, detalhe triplanar, POM.
- **Precisão explícita**: todo shader declara `mediump`/`highp` por variável. Um validador avisa quando `highp` é desnecessário.
- **Preview no editor**: esfera/plano/malha do usuário renderizada com o pipeline real, não um preview aproximado.

## 5.8 Upscaling e resolução dinâmica

- **Resolução dinâmica** guiada pelo tempo de GPU do frame anterior, com histerese (evita oscilação visível).
- **AetherSR**: reconstrução temporal própria (render em 50-70% → saída nativa), com rejeição de histórico por profundidade+normal+motion vector, e *anti-ghosting* por clamp de variância no espaço YCoCg.
- Integração com upscalers do fabricante quando presentes (Snapdragon GSR, MetalFX, FSR 2/3 mobile).
- **VRS**: taxa reduzida na periferia, atrás de DOF, em superfícies de baixa frequência (dirigido por um buffer de "importância" gerado no frame anterior).

## 5.9 Renderização 2D (não é um cidadão de segunda classe)

- Sprite batching automático por atlas + material, com ordenação por camada/Y.
- **Tilemaps** com chunking em GPU, autotile, tiles animados e colisão gerada.
- Iluminação 2D com normal maps e sombras 2D por shadow casters.
- Sistema de partículas 2D compartilhado com o 3D.
- Suporte a esqueletos 2D (estilo Spine/Live2D) com deformação de malha.
- **O mesmo pipeline HDR e pós-processamento** — jogos 2D também merecem bloom e color grading decentes.

---

# PARTE 6 — SIMULAÇÃO (FÍSICA, ANIMAÇÃO, ÁUDIO, IA)

## 6.1 Física

**Decisão:** integrar **Jolt Physics** (C++, licença MIT, multithread, determinístico, usado em Horizon Forbidden West) em vez de escrever do zero, com uma fachada C# própria. Escrever um solver competitivo custa 2-3 anos-pessoa e não é diferencial.

| Recurso | Detalhe |
|---|---|
| Corpos | Estático, cinemático, dinâmico; dormência agressiva (economia de bateria) |
| Formas | Esfera, caixa, cápsula, cilindro, convex hull, malha (estático), heightfield, composta |
| Juntas | Fixa, hinge, slider, cone, distância, 6DOF, mola, motor |
| Solver | Sequential impulse com fixed timestep de 60 Hz e interpolação para render |
| **Character Controller** | Componente pronto: passos, rampas, plataformas móveis, agachar, nadar, escalar. **A ausência de um bom CC pronto é a reclamação nº 1 de usuários de Godot/Unity** |
| Queries | Raycast, shapecast, overlap, com filtros por camada e máscara |
| Física 2D | Jolt em modo 2D restrito **ou** Box2D v3 — decisão na Fase 4 por benchmark |
| Extras | Ragdoll com auto-configuração a partir do esqueleto, veículos (raycast + suspensão), corda/cloth por PBD, água por altura |
| Determinismo | Modo de ponto fixo opcional para netcode com rollback |
| Escalonamento térmico | Sub-stepping reduzido e islands adormecidos sob pressão térmica |

## 6.2 Animação

```
Malha + Esqueleto → [Grafo de Animação] → Poses → Skinning (compute) → Render
                         ↑
              Blend trees · State machine · IK · Root motion
```

| Sistema | Detalhe |
|---|---|
| **Skinning** | Em compute shader, com *dual quaternion* opcional. Buffer de poses bindless |
| **Grafo de animação** | Nós: clip, blend 1D/2D (freeform cartesian), aditivo, layer com máscara, state machine hierárquica, sub-grafos reutilizáveis |
| **Compressão de clips** | Curvas em ACL (Animation Compression Library) — ~10× menor que raw. Crítico para tamanho de app |
| **IK** | Two-bone (mãos/pés), FABRIK (cauda/tentáculo), look-at com limites, **foot placement** com raycast automático |
| **Root motion** | Com extração e reaplicação, integrado ao character controller |
| **Blend shapes / morph targets** | Para expressão facial, com compute skinning |
| **Retargeting** | Mapa de humanoide automático — importar animação de qualquer esqueleto compatível (Mixamo, etc.) |
| **Auto-rigging** | Modelo de ML no dispositivo que gera esqueleto + pesos a partir de uma malha humanoide. **Diferencial: rigar é a etapa que mais afasta iniciantes** |
| **Timeline/Sequencer** | Edição de cutscene, câmeras, eventos, áudio, tudo numa timeline tocável |
| **Captura por câmera** | Usar a câmera do celular + modelo de pose 3D para gravar animação. **Só possível porque estamos no celular** |

## 6.3 Áudio

| Camada | Detalhe |
|---|---|
| **Backend** | AAudio (Android, low-latency) / AudioUnit (iOS). Thread de prioridade real-time, buffer alvo de 5-10 ms |
| **Grafo de áudio** | Nós: fonte, mixer, bus, efeito, submix. Roteamento visual no editor |
| **Espacialização** | HRTF binaural (essencial: usuário mobile usa fone), atenuação por curva, cone direcional, oclusão por raycast, ambisonics para ambiente |
| **Efeitos** | Reverb por convolução + algorítmico, EQ paramétrico, compressor, limiter, delay, distorção, filtro passa-baixa dinâmico (abafamento) |
| **Reverb por zona** | Volumes de reverb com blend por posição, com preset automático a partir do tamanho da sala (usando o SDF da cena) |
| **Áudio adaptativo** | Camadas musicais com transição sincronizada por compasso, stingers, parâmetros expostos ao AetherFlow. Modelo estilo Wwise/FMOD, embutido |
| **Compressão** | Opus para música/diálogo, ADPCM/PCM para SFX curtos |
| **Gravação** | Usar o microfone do dispositivo direto no editor para gravar SFX e voz. **Vantagem mobile** |
| **Voz procedural** | Síntese de "gibberish" estilo Animal Crossing para diálogos sem custo de dublagem |

## 6.4 IA de jogo

- **Navmesh**: geração no dispositivo (Recast/Detour portado) com *tiles* incrementais — só regenera a área alterada.
- **Pathfinding**: A* com funil de corredor, evitação local por RVO/ORCA, flow fields para multidões.
- **Behavior Trees** e **Máquinas de Estado** editáveis visualmente (compartilham o editor de nós).
- **Utility AI** e **GOAP** como módulos opcionais.
- **Percepção**: visão (cone + raycast), audição (eventos com raio), memória de estímulos.
- **Steering behaviors** prontos: seguir, fugir, patrulhar, formação, wander.

## 6.5 Partículas e VFX

- Sistema **GPU-driven**: simulação em compute, até ~1M de partículas em perfil S.
- Editor por nós (mesmo grafo): emissão, forças, colisão (contra o SDF/depth buffer), sub-emissores, trilhas, ribbons.
- Renderização: billboard, mesh, ribbon, com iluminação e sombras recebidas.
- **VFX Graph** compartilhado com Shader Graph — o artista aprende uma linguagem só.
- Decals (projeção em espaço de tela com clustering) e sistema de "destruição" simples por pré-fratura.

---

# PARTE 7 — PIPELINE DE ASSETS E ARMAZENAMENTO

## 7.1 Fluxo de importação

```
Arquivo do usuário (glTF/FBX/PNG/WAV/USD/blend)
        │
        ▼
[1] Detecção de tipo + hash de conteúdo
        │
        ▼
[2] Importer (C#, roda em job de background nos núcleos "little")
        │   · glTF 2.0 (nativo, com extensões KHR)
        │   · FBX (via biblioteca própria, sem SDK proprietário)
        │   · OBJ, STL, PLY, USD/USDZ, VRM, Collada
        │   · Imagens: PNG/JPG/WebP/EXR/HDR/TGA/PSD (camadas)
        │   · Áudio: WAV/OGG/MP3/FLAC
        │   · Vídeo: MP4/WebM (decodificação por hardware)
        │
        ▼
[3] Processamento
        │   · Malhas: otimização de cache, geração de meshlets/LOD, tangentes,
        │             normais suavizadas, quantização (pos 16-bit, normal oct)
        │   · Texturas: mip chain, compressão ASTC (Android) / ASTC+PVRTC (iOS),
        │               canal packing automático (ORM), detecção de sRGB
        │   · Áudio: resample, normalização, escolha de codec por duração
        │
        ▼
[4] Cache de artefato (endereçado por hash: conteúdo + versão do importer + config)
        │
        ▼
[5] Asset lógico (.aether) — metadados, GUID estável, referências
```

**Princípios inegociáveis:**
1. **O arquivo original nunca é modificado.** Importação é uma projeção.
2. **Import determinístico e cacheável por hash** — reimportar não repete trabalho.
3. **Import em background com preview progressivo** — a malha aparece em baixa qualidade em 200 ms e refina.
4. **GUID estável** — mover/renomear um asset nunca quebra referências.
5. **Import incremental** — mudar uma configuração reprocessa só a etapa afetada.

## 7.2 Compressão de texturas — a decisão crítica no mobile

| Formato | Onde | Uso |
|---|---|---|
| **ASTC** (4×4 a 12×12) | Android moderno + Apple | **Padrão**. Bloco variável permite trocar qualidade por tamanho por textura |
| **ASTC HDR** | Perfis A/S | Skybox, lightmaps HDR |
| **ETC2/EAC** | Fallback Android antigo | Perfil C |
| **BC7/BC5** | Exportação para desktop | Só no build de desktop |
| **Basis Universal / KTX2** | Distribuição | Transcodifica no dispositivo para o formato nativo — **um único arquivo serve todos os alvos** |

**Compressão no dispositivo:** o compressor ASTC roda em **compute shader** (não em CPU), o que torna a importação de uma textura 4K uma questão de ~200 ms em vez de 30 s. Este é um requisito de viabilidade, não uma otimização.

## 7.3 Streaming e virtual texturing

- **Streaming de texturas por mip**: só os mips necessários residem na GPU; feedback buffer indica o que a tela realmente pede.
- **Virtual Texturing** (perfil A/S) para terreno e cenas grandes: páginas de 128×128 num atlas físico, com indireção.
- **Streaming de cena (World Partition)**: mundo dividido em células; carga/descarga por distância e por *data layers*, em jobs de I/O que nunca bloqueiam o frame.
- **Bundles**: agrupamento de assets por cena/nível para carga sequencial (I/O de flash móvel prefere leituras grandes e contíguas).
- **Orçamento explícito**: o usuário vê uma barra "Memória de texturas: 420/500 MB" no editor, em tempo real.

## 7.4 Formato de projeto e arquivos

```
MeuJogo.aetherproj/
├── project.aether            # config, versão da engine, perfis de build
├── assets/
│   ├── models/hero.glb       # ORIGINAL, intocado
│   ├── models/hero.glb.meta  # GUID + config de import (texto, versionável)
│   ├── textures/...
│   └── audio/...
├── scenes/
│   ├── level01.ascene        # texto (TOML-like) para diff/merge
│   └── level01.ascene.bin    # binário derivado, para carga rápida
├── scripts/                  # .cs
├── flows/                    # .aflow (grafos no-code, formato texto)
├── materials/  shaders/  prefabs/  animations/
├── .aether/
│   ├── cache/                # artefatos importados (não versionado)
│   ├── wal/                  # log de edição para recuperação de crash
│   └── index.db              # SQLite: índice de assets, busca, dependências
└── .gitignore / .aetherignore
```

- **Formato dual texto+binário**: texto é a verdade (versionável, mergeável); binário é derivado e descartável.
- **Banco de dados de dependências** em SQLite: "quem usa esta textura?" é uma consulta instantânea — base para o "Localizador de Referências" e para builds mínimos.

---

# PARTE 8 — O EDITOR MOBILE — LAYOUT E UX

> Esta é a parte mais original do projeto e a que decide se ele vence ou fracassa. Uma engine com renderizador medíocre e UI genial ganha de uma com renderizador genial e UI de desktop portada.

## 8.1 Os cinco princípios de design

| # | Princípio | Implicação concreta |
|---|---|---|
| **1** | **O conteúdo é o rei; a UI se retrai** | Viewport ocupa 100% da tela por padrão. Painéis são camadas invocadas, não molduras permanentes |
| **2** | **Modos, não menus** | Como no Blender: o *modo* atual redefine gestos, gizmos e barra de ferramentas. Elimina o problema de densidade |
| **3** | **Todo gesto é reversível e previsualizado** | Nada é confirmado até soltar o dedo. Feedback contínuo, sempre |
| **4** | **Duas mãos, com polegares** | Zonas de alcance de polegar (o centro da tela é a zona *ruim*, não a boa) |
| **5** | **Profundidade progressiva** | 3 níveis de UI: Essencial → Padrão → Completo. O mesmo produto, sem versões separadas |

## 8.2 Anatomia da tela — landscape, fullscreen

**Decisão de produto:** o editor é **exclusivamente landscape e fullscreen**. Sem barra de status,
sem barra de navegação, sem retrato. Isso não é uma limitação — é o que torna o produto viável:

| Por que landscape trancado | Consequência |
|---|---|
| O viewport 3D tem a proporção do jogo (16:9, 19.5:9), não uma janela vertical estreita | O que você vê editando é o que o jogador vê |
| Duas mãos seguram o aparelho, os dois polegares alcançam as bordas | As zonas de comando ficam nas laterais, onde os polegares repousam — não no topo, que exige soltar o aparelho |
| Um único layout para telefone e tablet | Metade do trabalho de UI, e nenhuma divergência de comportamento |
| Fullscreen imersivo (`WindowInsets` / `prefersHomeIndicatorAutoHidden`) | Mais ~8% de área útil e nenhum gesto do sistema roubando um arrasto do editor |
| Nenhuma reflow por rotação | Elimina uma classe inteira de bugs de layout e de recriação de swapchain |

O preço é honesto e assumido: teclado virtual em landscape come metade da tela. Por isso a entrada
de texto é sempre **inline e curta** (campos numéricos com teclado compacto próprio), e a escrita
longa — só o editor de código — abre em **overlay dedicado** com o teclado ancorado.

```
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║ ◀  MeuJogo    ▸ Play  ⏸  ⏹        ⟲  ⟳              ⬚ 60fps  3.2W  ▓▓▓░  ⚙               ║ ← BARRA (40 dp)
╠═════╦════════════════════════════════════════════════════════════════════════╦═══════════╣
║     ║                                                                        ║           ║
║  ⬚  ║                                                                        ║ ▣ Player  ║
║ Sel ║                                            ╭─ gimbal ─╮                ║ ───────── ║
║     ║                                            │    Y     │                ║ Transform ║
║  ✥  ║                        ▲ Y                 │  ◀ ● ▶   │                ║  X  1.20 ⇔║
║ Mov ║                        ┃                   │    -Y    │                ║  Y  0.00 ⇔║
║     ║                   ┌────╂────┐              ╰──────────╯                ║  Z -3.40 ⇔║
║  ⟳  ║                   │ ▣  ┃ ▣  │                                          ║ ───────── ║
║ Gir ║              ━━━━━╋━━━━╋━━━━╋━━━━► X                                    ║ Mesh    ⌄ ║
║     ║                   │ ▣  ┃ ▣  │        V I E W P O R T                   ║ Material⌄ ║
║  ⤢  ║                   └────╂────┘                                          ║ RigidBody⌄║
║ Esc ║                        ┃                                               ║           ║
║     ║                        ▼                                               ║           ║
║  ✎  ║                                                                        ║           ║
║ Edt ║   ╭──────────────╮                                                      ║           ║
║     ║   │  X: +1.24 m  │ ← HUD numérico, do lado oposto ao dedo               ║           ║
║  ⋯  ║   ╰──────────────╯                                                      ║           ║
╠═════╩════════════════════════════════════════════════════════════════════════╩═══════════╣
║ ▾ Cena  ▸Mundo  ▸Player  ▸Chão  ▸Luzes            │  Assets · Console · Flow · Timeline   ║ ← TRILHO (44 dp)
╚══════════════════════════════════════════════════════════════════════════════════════════╝
   ↑ doca de modos                                    ↑ trilho inferior: hierarquia (esq)
     (polegar esquerdo, 64 dp)                          + abas de painel (dir, polegar direito)
```

### As quatro regiões

| Região | Posição | Conteúdo | Regra |
|---|---|---|---|
| **Barra superior** | topo, 40 dp | projeto, transporte de play, undo/redo, telemetria ao vivo (fps, watts, memória), config | Nunca cresce. Telemetria sempre visível — o criador precisa sentir o custo do que constrói |
| **Doca de modos** | borda esquerda, 64 dp | modos (Seleção, Mover, Girar, Escala, Edição, Mais) | Zona de repouso do polegar esquerdo. Toque longo num modo abre suas opções em radial |
| **Viewport** | todo o resto | a cena, com o pipeline real | **Sempre o maior elemento da tela.** Os painéis flutuam sobre ele, nunca o encolhem abaixo de 60% da largura |
| **Trilho inferior** | base, 44 dp | breadcrumb de hierarquia (esquerda) + abas de painel (direita) | Toque numa aba desliza o painel correspondente por cima do viewport |

### Painéis laterais (o que substitui o Bottom Sheet em landscape)

Em landscape, folhas que sobem da base roubam a dimensão errada. O padrão vira **painéis laterais deslizantes**:

| Estado | Largura | Comportamento |
|---|---|---|
| **Oculto** | 0 | Fora da tela |
| **Espiada** | 20% | Só cabeçalho + propriedade favorita. Toque para expandir |
| **Trabalho** | 32% | Padrão. Viewport mantém 68% e continua interativo |
| **Foco** | 55% | Para editar um grafo do AetherFlow ou uma curva |
| **Cheio** | 100% | Só para editor de código e escultura, com um gesto de voltar sempre visível |

- Arraste horizontal na borda do painel move entre estados, com mola e snap.
- **Dois painéis simultâneos** (esquerdo e direito) em telas ≥ 7". Em telefone, um por vez.
- Empilhamento: abrir um sub-painel (escolher textura dentro do inspector) empurra o anterior; swipe para a direita volta.
- O painel **nunca cobre a seleção**: se o objeto selecionado ficaria atrás do painel, a câmera faz um pan curto e animado para trazê-lo à área visível. Detalhe pequeno, diferença enorme na sensação de controle.

### Zonas de polegar (o que decide onde cada coisa mora)

```
   ┌──────────────────────────────────────────────────────────┐
   │▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▓▓▓▓▓│
   │▓▓▓▓▓░░░░                                     ░░░░░▓▓▓▓▓▓│   ▓ alcance natural do polegar
   │▓▓▓▓▓▓░░              zona "fria"               ░░▓▓▓▓▓▓▓│   ░ exige mover a mão
   │▓▓▓▓▓▓▓░        (conteúdo, não comando)        ░░▓▓▓▓▓▓▓▓│
   │▓▓▓▓▓▓░░                                      ░░░▓▓▓▓▓▓▓│
   │▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▓▓▓▓▓▓│
   └──────────────────────────────────────────────────────────┘
        ↑ polegar esquerdo                    polegar direito ↑
          modos, undo/redo                    painéis, confirmar
```

Regra derivada: **todo comando frequente mora numa das duas faixas laterais**; o centro é território
do conteúdo. É o oposto de uma engine de desktop, onde os comandos ficam no topo.

## 8.3 Estados de painel e empilhamento

Detalhamento do sistema de painéis laterais descrito em 8.2. Em tablets grandes segurados na horizontal, uma folha inferior ainda faz sentido para timeline e console — estes são os estados verticais, usados só nesses dois casos:

| Estado | Altura | Comportamento |
|---|---|---|
| **Oculto** | 0 | Fora da tela |
| **Espiada (peek)** | ~15% | Só o cabeçalho + valor principal. Viewport totalmente visível |
| **Meio** | 50% | Trabalho normal. Viewport ainda visível e interativo acima |
| **Cheio** | 92% | Trabalho focado (ex.: timeline de cutscene em tablet) |

- Arraste vertical move entre estados com física de mola e *snap*.
- **Empilhamento**: abrir um sub-painel (ex.: escolher uma textura dentro do inspector) empurra o anterior para trás com um card recuado. Voltar = swipe para a direita.
- **Fixação (pin)**: em tablet, uma folha pode ser fixada como coluna.
- **Divisão**: gesto de duas folhas lado a lado em tablets ≥ 9".

## 8.4 Linguagem de gestos completa

### Navegação de câmera (sempre disponível, em qualquer modo)

| Gesto | Ação | Nota |
|---|---|---|
| 1 dedo arrastando **no vazio** | Orbitar | Se tocar num objeto, é seleção — por isso o *hit test* acontece no `down` |
| 2 dedos arrastando | Pan | |
| 2 dedos pinça | Zoom (dolly) | Zoom para o ponto entre os dedos, não para o centro |
| 2 dedos rotação | Roll da câmera | Com trava de snap a 0° |
| Toque duplo em objeto | Enquadrar (frame) | Animação suave |
| Toque duplo no vazio | Enquadrar tudo | |
| **Arrasto no gimbal de eixos** | Rotação livre com snap a vistas ortográficas | Canto superior direito |
| 3 dedos arrastando | Modo "voo" (FPS) | Para percorrer níveis grandes |
| Girar o aparelho | **Modo giroscópio**: a câmera segue o aparelho | Opcional; incrível para inspecionar cenas |

### Manipulação de objetos

| Gesto | Ação |
|---|---|
| Toque | Selecionar |
| Toque e segurar (300 ms) | **Menu radial contextual** (ver 8.5) |
| Arrastar objeto selecionado | Mover no plano da tela |
| Arrastar **seta do gizmo** | Mover travado no eixo |
| Arrastar **quadrado do gizmo** | Mover travado no plano |
| Pinça sobre seleção | Escala uniforme |
| Rotação de dois dedos sobre seleção | Girar no eixo da câmera |
| Arrastar **anel do gizmo** | Girar travado no eixo, com HUD de ângulo |
| **Segundo dedo tocando enquanto arrasta** | **Modo de precisão**: sensibilidade ÷10 |
| Swipe com 2 dedos ← | Desfazer |
| Swipe com 2 dedos → | Refazer |
| Segurar e arrastar para fora | Duplicar (com feedback háptico no "descolar") |
| Laço com 1 dedo (modo seleção) | Seleção múltipla |

### Gizmos redesenhados para o dedo

O gizmo de translação da Unity tem alvos de ~8 px. Inútil com um dedo de 9 mm. Redesenho:

```
        ▲ Y              · Hastes com 44 dp de área de toque (invisível)
        ┃                · Ponta com esfera grande
   ┌────╂────┐           · Quadrados de plano nos quadrantes
   │ ▣  ┃ ▣  │           · Anel externo = rotação no eixo da câmera
   ┃━━━━╋━━━━┫━━► X      · Centro = movimento livre no plano da tela
   │ ▣  ┃ ▣  │           · O gizmo ESCALA com a distância (tamanho de
   └────╂────┘             tela constante) e AFASTA-SE do dedo enquanto
        ┃                   você arrasta, para não ser encoberto
        ▼
   ╭──────────────╮     ← HUD flutuante durante o arrasto, ACIMA do dedo:
   │ X: +1.24 m   │       valor numérico ao vivo, tocável para digitar
   ╰──────────────╯
```

**Recursos anti-oclusão (o dedo cobre o que está editando):**
- *Lupa flutuante*: durante arrasto de precisão, uma janela circular acima do dedo mostra a região ampliada (padrão do iOS de seleção de texto).
- *Offset de arrasto*: o objeto move-se com um deslocamento vertical configurável.
- HUD numérico sempre posicionado no lado oposto ao dedo.

## 8.5 Menu radial contextual (Pie Menu)

O elemento de UI mais importante do editor. Toque e segure em qualquer lugar abre um radial cujo conteúdo depende do contexto:

```
              [Duplicar]
                  ▲
    [Material] ◀──╋──▶ [Adicionar Filho]
                  ▼
              [Excluir]
        ╲               ╱
    [Ocultar]      [Foco]

  · 4-8 setores (8 é o máximo com precisão de dedo)
  · Selecionável por DIREÇÃO, não por posição → memória muscular:
    depois de 2 dias, o usuário faz "segurar + flick para cima"
    sem olhar. Isso é o que torna a edição RÁPIDA no touch.
  · Sub-menus abrem como um segundo anel (segurar no setor)
  · Totalmente configurável pelo usuário, com perfis por modo
  · Vibração háptica por setor
```

Este é o mecanismo que substitui os atalhos de teclado do desktop — e a razão pela qual editar no celular pode chegar perto da velocidade do desktop.

## 8.6 Sistema de modos

Assim como no Blender, o modo redefine toda a interação. Trocar de modo é um toque na doca ou um gesto de swipe na barra inferior.

| Modo | Gestos redefinidos | Painéis padrão |
|---|---|---|
| **Objeto** | Selecionar/mover/girar/escalar objetos | Hierarquia, Inspector |
| **Edição de Malha** | Selecionar vértice/aresta/face; extrude por arrasto de normal; loop cut por swipe | Ferramentas de malha, Modificadores |
| **Escultura** | Dedo = pincel; pressão (se houver) = força; dois dedos = orbitar | Pincéis, Máscaras, Camadas |
| **Pintura de Textura** | Dedo pinta na malha; camadas estilo Photoshop | Camadas, Pincéis, Paleta |
| **UV** | Manipulação de ilhas UV | Ferramentas de UV |
| **Rigging** | Criar/editar ossos, pintar pesos | Esqueleto, Pesos |
| **Animação** | Timeline, keyframes, curvas | Timeline, Dope Sheet, Curvas |
| **Terreno** | Dedo esculpe altura; pinta camadas | Pincéis de terreno, Camadas |
| **Flow (no-code)** | Canvas de nós com pan/zoom | Paleta de nós, Variáveis, Debug |
| **Play** | Input do jogo; overlay de debug | Console, Profiler, Inspetor ao vivo |
| **Cinema** | Composição de câmera, timeline de cutscene | Sequencer, Câmeras |

**Transição entre modos:** o viewport nunca "recarrega". A troca é uma transição animada de 150 ms; o estado da câmera e da seleção persiste. Esta continuidade é o que dá a sensação de "um app só".

## 8.7 Inspector adaptativo

Gerado 100% por reflexão, mas com **widgets desenhados para o dedo**:

| Tipo | Widget touch |
|---|---|
| `float` | **Slider-arrasto horizontal na própria linha** (arraste em qualquer lugar da linha muda o valor, com aceleração não-linear). Toque no número = teclado numérico |
| `float` com `[Range]` | Slider com trilha, marcas e snap |
| `Vector3` | Três linhas de arrasto + botão "arrastar no viewport" |
| `Color` | Roda de cor + campo HDR + conta-gotas que amostra do viewport |
| `bool` | Switch grande |
| `enum` | Chips horizontais (se ≤ 5) ou folha de seleção |
| Referência de asset | Miniatura tocável → abre navegador em folha; arrastar do navegador para cá também funciona |
| Curva | Editor de curva em tela cheia com pontos de 44 dp |
| Lista/array | Reordenável por arrasto, swipe para excluir |
| Textura | Preview + canal picker |

**Recursos:**
- **Busca por propriedade** no topo ("digite 'sombra'") — encontra em todos os componentes.
- **Favoritos**: fixar as 3 propriedades que você mais mexe no cartão contextual do viewport.
- **Comparação com prefab**: propriedades sobrescritas em azul, com swipe para reverter.
- **Edição múltipla**: selecionar 10 objetos e editar todos de uma vez, com indicador de valores mistos.

## 8.8 Navegador de assets

- **Grade de miniaturas** com preview real (malha 3D renderizada, não ícone genérico).
- **Preview interativo**: segurar numa malha gira o preview; segurar num áudio toca.
- **Busca fuzzy** + filtros por tipo, tag, "não usado", "recém-modificado".
- **Arrastar-e-soltar para o viewport** com preview fantasma e snap ao chão.
- **Coleções inteligentes**: pastas virtuais definidas por consulta ("todas as texturas > 2K").
- **Modo "importar"**: integração com a galeria do celular, câmera, Drive/Dropbox, e **captura por fotogrametria** (ver Parte 15).

## 8.9 Acessibilidade e conforto

- Todos os alvos de toque ≥ 44 dp (48 dp no Android).
- **Modo de uma mão**: comprime a UI para o lado do polegar dominante.
- Escala de UI de 80% a 200%, independente da escala do sistema.
- Alto contraste, daltonismo (3 paletas), redução de movimento.
- **Modo escuro por padrão** (OLED: economia real de bateria) + modo claro para ambientes externos.
- Leitor de tela para navegação de hierarquia e inspector.
- **Controle externo**: suporte completo a teclado Bluetooth, mouse, trackpad, caneta (Apple Pencil, S Pen) e gamepad. Quem tiver um teclado ganha atalhos de desktop.
- **Caneta com pressão e inclinação** para escultura e pintura — no iPad e Galaxy isso equipara-se a uma mesa digitalizadora.

---

# PARTE 9 — SISTEMA NO-CODE (AETHERFLOW)

## 9.1 A tese

Blueprints (Unreal) e Bolt (Unity) falham em três pontos: são lentos, não convertem para código, e o canvas de nós é hostil em tela pequena. O AetherFlow ataca os três.

| Problema | Solução do AetherFlow |
|---|---|
| Interpretação lenta | **Compila para IL .NET** — mesmo desempenho de C# escrito à mão |
| "Blueprint spaghetti" | Layout automático, colapso em sub-grafos, e **modo Lista** (ver 9.4) |
| Canvas ruim no celular | Três representações da MESMA lógica: Blocos, Grafo e Código |
| Não migra para código | **Round-trip real**: grafo ⇄ C# em ambas as direções |
| Difícil depurar | Depuração visual com fluxo animado e inspeção de valores nos fios |

## 9.2 Três representações, uma lógica

Esta é a inovação estrutural mais importante do produto. O usuário escolhe **como ver** a mesma lógica, e pode alternar a qualquer momento.

```
   NÍVEL 1 — BLOCOS              NÍVEL 2 — GRAFO              NÍVEL 3 — C#
   (persona P1)                  (personas P1/P2)             (personas P2/P4)

  ┌──────────────────┐          ┌────────┐                  void Update() {
  │ Quando ▸ tocar   │          │OnTouch ├──┐                 if (Input.Touched)
  ├──────────────────┤          └────────┘  ▼                   rb.AddForce(
  │  ▸ Pular         │  ◄──►    ┌──────────────┐  ◄──►            Vector3.up * 5f,
  │    força: [5.0]  │          │ AddForce     │                   ForceMode.Impulse);
  ├──────────────────┤          │ dir: ↑ 5.0   │                }
  │  ▸ Tocar som     │          └──────┬───────┘
  │    som: [pulo]   │                 ▼
  └──────────────────┘          ┌──────────────┐
                                │ PlaySound    │
    Encaixe físico              └──────────────┘
    (estilo Scratch)             Nós com pinos            Texto real, editável
    Impossível errar             de dado e execução        com IntelliSense
```

**Como funciona:** as três são *vistas* de uma única AST (árvore sintática) persistida em `.aflow`. A conversão é bidirecional e sem perda para o subconjunto suportado. Quando o usuário escreve C# que o grafo não representa (ex.: LINQ complexo, genéricos avançados), o nó vira uma **"caixa de código"** — um nó opaco com pinos declarados. Isso significa que **nunca há um muro**: você começa em blocos aos 13 anos e termina escrevendo C# no mesmo arquivo.

## 9.3 Compilação

```
.aflow (AST em texto)
   │
   ▼ [Validação semântica: tipos, ciclos, pinos obrigatórios]
   │
   ▼ [Baixa para C# gerado] ──► arquivo .g.cs legível (o usuário PODE ver)
   │
   ▼ [Roslyn compila para IL]
   │
   ├──► Android: Mono/CoreCLR com JIT ou interpretador → hot reload em < 1 s
   └──► iOS / build final: AOT via ILCompiler → binário nativo
```

- **Modo editor**: interpretação da AST para iteração instantânea (< 100 ms), com transição transparente para IL compilado após 2 s de inatividade.
- **Modo build**: sempre IL/AOT. Zero penalidade de desempenho versus C#.
- **Análise estática**: detecta laços infinitos, referências nulas, alocação em loop, e avisa **enquanto o usuário monta o grafo**.

## 9.4 Modo Lista — o layout no-code que funciona no celular

Um canvas 2D de nós exige pan/zoom constante numa tela de 6". A solução: uma **representação linear indentada** que é o padrão em telefones (o grafo continua disponível, e é o padrão em tablets).

```
┌─────────────────────────────────────────┐
│ ▣ PlayerController        [Grafo] [C#]  │
├─────────────────────────────────────────┤
│ ⚡ Quando o jogo começar                │
│  ├─ ▸ Definir  velocidade = 5.0         │
│  └─ ▸ Travar cursor                     │
│                                         │
│ ⚡ A cada quadro                         │
│  ├─ ◆ Se  [ está no chão? ]             │
│  │   ├─ Sim ─┬ ▸ Mover  ( entrada × 5 ) │
│  │   │       └ ▸ Animar  "correr"       │
│  │   └─ Não ── ▸ Aplicar gravidade      │
│  └─ ▸ Girar câmera ( olhar )            │
│                                         │
│ ⚡ Quando colidir com [ Inimigo ]        │
│  ├─ ▸ Tirar vida  ( 10 )                │
│  └─ ◆ Se  [ vida ≤ 0 ] → ▸ Morrer       │
│                                         │
│            [ + Adicionar evento ]       │
└─────────────────────────────────────────┘
```

- Cada linha é um alvo de toque grande; arrastar reordena; swipe exclui.
- Toque num parâmetro entre parênteses abre um mini-editor inline.
- Toque no ⚡ colapsa o evento inteiro.
- **É legível como pseudo-código em português** — o que resolve o problema de "não sei ler código" sem esconder a estrutura.

## 9.5 Biblioteca de nós

| Categoria | Exemplos |
|---|---|
| **Eventos** | Início, A cada quadro, Física, Colisão, Gatilho, Input, Sinal customizado, Timer, Rede |
| **Fluxo** | Se/Senão, Trocar (switch), Para cada, Enquanto, Sequência, Portão, Fazer uma vez, Retardo, Paralelo |
| **Async / Tempo** | Esperar segundos, Esperar até, Esperar quadro, Corrotina, Tween/Interpolar, Curva no tempo |
| **Transform** | Mover, Girar, Escalar, Olhar para, Seguir, Orbitar, Snap, Converter espaço |
| **Física** | Aplicar força/impulso/torque, Raycast, Overlap, Definir velocidade, Character move |
| **Renderização** | Definir material, Trocar cor, Animar propriedade de shader, Ligar/desligar |
| **Animação** | Tocar, Cruzar (crossfade), Definir parâmetro, Aguardar fim, Evento de animação |
| **Áudio** | Tocar, Parar, Fade, Definir parâmetro, Tocar em posição |
| **UI** | Mostrar/ocultar, Definir texto, Animar, Vincular a variável (data binding) |
| **Dados** | Variáveis (local/objeto/global/salvas), Listas, Dicionários, Matemática, Aleatório, Texto |
| **Cena** | Instanciar, Destruir, Encontrar, Carregar cena, Sinal, Enviar mensagem |
| **IA** | Ir para, Perseguir, Fugir, Patrulhar, Ver alvo, Behavior tree |
| **Salvamento** | Salvar/carregar valor, Perfil, Nuvem |
| **Rede** | Chamada remota (RPC), Replicar variável, É servidor?, Entrar em sala |
| **Sistema** | Log, Vibrar, Abrir URL, Compartilhar, Anúncio, Compra no app, Conquista |
| **Sensores** | Acelerômetro, Giroscópio, Bússola, GPS, Câmera, Microfone, Toque multi-ponto |

**Macros e sub-grafos**: qualquer seleção de nós vira um nó reutilizável com um toque. Sub-grafos podem ser publicados na Asset Store.

## 9.6 Depuração visual

- **Fluxo animado**: partículas correm pelos fios de execução em tempo real durante o play.
- **Valores nos fios**: cada fio de dado mostra o valor atual sobre ele.
- **Breakpoint tocável**: toque num nó para pausar quando ele executar.
- **Time-travel**: uma barra de tempo permite rebobinar os últimos 10 s de execução e ver os valores de cada nó em cada frame (gravação circular de estado). **Recurso que nenhuma engine mainstream tem, e que é transformador para iniciantes.**
- **Mapa de calor**: nós coloridos por custo de CPU.
- **Watch**: fixar valores num overlay durante o play.

## 9.7 IA assistente (integrada, não parafernália)

| Recurso | Descrição |
|---|---|
| **Descrever → Grafo** | "quando o jogador pegar a moeda, tocar som, somar 10 pontos e destruir a moeda" → gera o grafo, o usuário revisa e aceita |
| **Explicar este grafo** | Descrição em português do que a lógica faz |
| **Consertar isto** | Analisa o erro/comportamento e propõe alteração diffada |
| **Sugestão de próximo nó** | Autocompletar contextual enquanto monta |
| **Converter para C#** | Refatoração assistida ao migrar de nível |
| **Gerar variações** | "faça 5 variações deste inimigo" |

**Regras:** a IA sempre propõe um *diff* revisável, nunca altera direto. Funciona offline em modelo pequeno no dispositivo para sugestões, e em nuvem para geração complexa (opt-in explícito).

---

# PARTE 10 — FERRAMENTAS DE CRIAÇÃO DE CONTEÚDO INTEGRADAS

> O papel do Blender, dentro do app. Ninguém deveria precisar de um PC para fazer uma caixa, um personagem ou um terreno.

## 10.1 Modelagem poligonal

| Ferramenta | Gesto touch |
|---|---|
| Seleção de vértice/aresta/face | Toque; arrasto = laço; toque duplo em aresta = loop |
| **Extrude** | Arrastar a face na direção da normal (com HUD numérico) |
| **Inset** | Pinça para dentro na face |
| **Bevel** | Arrastar perpendicular à aresta; segundo dedo controla segmentos |
| **Loop cut** | Swipe perpendicular à malha; o preview do loop segue o dedo |
| **Knife** | Traçar com o dedo/caneta |
| **Bridge, Merge, Dissolve, Solidify** | Da barra de ferramentas do modo + radial |
| **Espelho / Simetria** | Ligado por padrão em X — modelar metade de um personagem |
| **Snap** | Vértice, aresta, face, grade, com feedback háptico |
| **Proportional editing** | Toque e segure = raio de influência com falloff visual |
| **Booleanas** | União/diferença/interseção com preview em tempo real |

**Modificadores não-destrutivos empilháveis** (a melhor ideia do Blender, e a que melhor se traduz para uma lista tocável):

```
┌────────────────────────────────┐
│ MODIFICADORES        [ + ]     │
├────────────────────────────────┤
│ ⣿ Espelho          X ▣  ●━━━  │  ← arrastar para reordenar
│ ⣿ Subdivisão       Níveis: 2   │  ← toque expande parâmetros
│ ⣿ Bevel            0.02 m      │
│ ⣿ Solidificar      0.05 m      │
│ ⣿ Array            × 6 radial  │
└────────────────────────────────┘
```

## 10.2 Escultura (onde o mobile ganha do desktop)

- **Dyntopo** (topologia dinâmica): a malha subdivide sob o pincel.
- **Multiresolução**: níveis de detalhe esculpíveis independentemente.
- **Voxel remesh** com um toque (limpa topologia ruim).
- Pincéis: Draw, Clay Strips, Inflate, Smooth, Grab, Snake Hook, Pinch, Crease, Flatten, Mask, Trim (corte por traçado).
- **Pressão da caneta** → força; **inclinação** → ângulo do pincel.
- **Simetria radial e por eixo**.
- **Camadas de escultura** (como camadas de imagem, mescláveis).
- **Retopologia**: automática (quad remesh) e manual (traçar quads sobre a superfície com o dedo — surpreendentemente natural em touch).

## 10.3 UV e pintura de textura

- **Unwrap automático** por padrão (xatlas), com marcação manual de costuras por traçado.
- Editor de UV com ilhas manipuláveis por gestos.
- **Pintura direta na malha 3D**, com camadas, máscaras, modos de mesclagem, e pincéis com textura.
- **Pintura por camadas procedurais** (estilo Substance Painter, simplificado): camada base + máscaras por curvatura, oclusão, altura, sujeira.
- **Baking**: normal map de high-poly para low-poly, AO, curvatura, position — em compute shader no dispositivo.
- **Câmera como fonte**: fotografar uma textura e transformá-la em material PBR (ver Parte 15).

## 10.4 Terreno e mundo

- Terreno por heightmap com **chunking** e LOD contínuo (CDLOD).
- Escultura de terreno com pincéis (levantar, abaixar, suavizar, platô, erosão hidráulica simulada).
- **Pintura de camadas** com splat maps (até 16 camadas com triplanar).
- **Espalhamento de vegetação** com regras (inclinação, altura, camada, densidade, ruído) e **renderização por GPU instancing + impostor** para distância.
- Rios e água com simulação de ondas (Gerstner + FFT em perfil S).
- **Geração procedural**: ruído, erosão, biomas — com preview ao vivo.

## 10.5 Geometry Nodes (nós de geometria)

Sistema procedural que compartilha o editor de nós. Diferencial gigante no mobile: **gerar conteúdo com regras é muito mais eficiente que modelar à mão num celular**.

- Nós: primitivas, distribuição em superfície, instanciar em pontos, transformar, booleana, extrude, ruído, curvas, campos de atributo.
- Casos de uso: cidades procedurais, escadas paramétricas, cercas, vegetação, dungeons.
- Resultado é uma malha real, cacheada, ou avaliada em runtime.

## 10.6 Editor de partículas, materiais e pós-processamento

Todos compartilham a mesma infraestrutura de nós e o mesmo canvas — o usuário aprende **um** editor de nós e o reusa em cinco contextos (material, VFX, geometria, lógica, animação). Isso reduz drasticamente a carga cognitiva.

---

# PARTE 11 — SCRIPTING E CAMADA DE CÓDIGO (C#)

## 11.1 Runtime .NET no dispositivo

| Plataforma | Estratégia |
|---|---|
| **Android (editor)** | CoreCLR ou Mono com JIT → compilação e hot reload rápidos |
| **Android (jogo publicado)** | Mono AOT + LLVM, ou NativeAOT quando estável para o alvo |
| **iOS (editor)** | NativeAOT para a engine + **interpretador de IL** para o código do usuário (permitido pelas regras da App Store para código interpretado dentro do app) |
| **iOS (jogo publicado)** | NativeAOT completo |

## 11.2 Editor de código no celular

Escrever código num teclado virtual é doloroso. Mitigações:

- **Barra de símbolos** acima do teclado: `{ } ( ) ; . = < > + - * / " _ →` com deslize horizontal.
- **Autocompletar agressivo** (Roslyn completo rodando local) — o usuário digita 2-3 letras e escolhe.
- **Snippets tocáveis**: `if`, `for`, `foreach`, `class`, `component` como chips.
- **Ditado por voz com gramática de código** ("se jogador ponto vida menor que zero abre chaves").
- **Navegação estrutural**: colapsar métodos, mapa de arquivo, ir para definição por toque longo.
- **Diagnóstico inline** com quick fixes tocáveis.
- Suporte a **teclado Bluetooth** com atalhos completos (para quem tem).

## 11.3 Desenho da API de gameplay

Princípios: descoberta fácil, poucos conceitos, nomes previsíveis, e **paridade exata com o AetherFlow**.

```csharp
using Aether;

public class PlayerController : Behavior
{
    [Inspector] public float Speed = 5f;
    [Inspector] public float JumpForce = 8f;
    [Inspector] public SoundAsset JumpSound;

    private CharacterBody body;

    protected override void Start() => body = Get<CharacterBody>();

    protected override void Update(float dt)
    {
        var move = Input.Stick("Move");
        body.Move(new float3(move.x, 0, move.y) * Speed * dt);

        if (Input.Pressed("Jump") && body.IsGrounded)
        {
            body.Jump(JumpForce);
            Audio.PlayAt(JumpSound, Transform.Position);
        }
    }

    // Corrotinas com async/await nativo do C#
    private async Task Respawn()
    {
        await Time.Delay(2f);
        Transform.Position = Scene.Find("SpawnPoint").Position;
    }
}
```

Para desempenho máximo, o usuário desce para ECS puro:

```csharp
[System(Stage.Simulation)]
public partial struct MovementSystem : IJobSystem
{
    public void Execute(ref Transform t, in Velocity v, [Time] float dt)
        => t.Position += v.Value * dt;   // vetorizado e paralelizado automaticamente
}
```

**Três níveis de API**, do mais amigável ao mais rápido, sem cliff: `Behavior` (OO familiar) → `Component + System` → `IJobSystem` (SIMD/paralelo).

## 11.4 Hot reload

- Salvar → Roslyn compila incremental → novo `AssemblyLoadContext` → estado dos componentes migrado por serialização → **< 1.5 s**.
- Mudanças de assinatura/estrutura são detectadas e resolvidas com migração automática ou aviso.
- No iOS, o código do usuário roda no interpretador durante a edição, permitindo o mesmo loop.

---

# PARTE 12 — MULTIPLAYER, BACKEND E SERVIÇOS

## 12.1 Netcode

| Camada | Detalhe |
|---|---|
| **Transporte** | UDP confiável próprio (estilo ENet/QUIC) com canais, ordenação opcional, agregação de pacotes, e fallback WebSocket |
| **Modelos** | Cliente-servidor autoritativo (padrão), host-cliente (P2P com um host), lockstep determinístico (para RTS/fighting) |
| **Replicação** | Componentes marcados `[Replicated]` sincronizam automaticamente. Delta encoding + quantização + interest management por distância |
| **Predição** | Predição de cliente + reconciliação por rollback para o jogador local |
| **Interpolação** | Buffer de interpolação para entidades remotas, com extrapolação limitada |
| **RPC** | `[ServerRpc]` / `[ClientRpc]` com validação de autoridade — expostos como nós no AetherFlow |
| **Lag compensation** | Histórico de posições para hit validation no servidor |
| **Servidor dedicado** | Build headless (Linux, sem renderizador) gerado do mesmo projeto |

**Adaptação mobile:** perfis de rede que assumem 4G instável — tolerância a jitter alto, reconexão transparente, e modo de baixo consumo quando o app está em segundo plano.

## 12.2 Serviços de backend (Aether Cloud)

| Serviço | Descrição |
|---|---|
| **Contas e perfis** | Login social, anônimo, cross-device |
| **Salvamento na nuvem** | Sincronização de saves e de projetos |
| **Matchmaking + relay** | Salas, lobbies, skill-based, com relay para NAT difícil |
| **Leaderboards e conquistas** | Prontos, com nós no AetherFlow |
| **Analytics** | Funil, retenção, eventos customizados, mapas de calor de morte |
| **Remote config / A-B** | Alterar balanceamento sem republicar |
| **Build farm** | Compilar APK/AAB/IPA/desktop/web |
| **Lightmap baking** | Assar GI pesada na nuvem |
| **Asset processing** | Compressão e otimização de assets grandes |
| **Crash/erro** | Relatório com stack trace simbolizado |

Todos com **fallback local** — nada é obrigatório para desenvolver.

---

# PARTE 13 — BUILD, PUBLICAÇÃO E DISTRIBUIÇÃO

## 13.1 O desafio

Gerar um APK assinado exige toolchain Android (SDK, NDK, Gradle, keystore). Um IPA exige macOS e certificados Apple. Nada disso cabe num celular.

## 13.2 Arquitetura de build em três modos

| Modo | Como funciona | Quando |
|---|---|---|
| **1. Play no dispositivo (instantâneo)** | Nenhum build. O processo Play carrega o projeto e roda. Latência: < 1 s | 99% das iterações |
| **2. Build local de pacote de dados** | O celular gera o `.aetherpack` (assets otimizados + IL/AST) que roda no **Aether Player** — um app leve que qualquer pessoa instala e usa para jogar projetos compartilhados. Latência: 10-60 s | Compartilhar com amigos, testar em outro aparelho, salas de aula |
| **3. Build nativo em nuvem** | O projeto é enviado (delta) para a build farm, que compila com AOT, funde com o runtime nativo, assina e devolve o APK/AAB/IPA. Latência: 3-15 min | Publicação nas lojas |

O modo 2 é estrategicamente decisivo: ele dá **distribuição imediata e viral** sem passar por loja, do mesmo jeito que o Roblox faz. E o modo 3 continua disponível para quem quer um produto próprio na Play Store.

## 13.3 Assinatura e chaves

- Keystore gerado no dispositivo e armazenado no **Android Keystore / iOS Keychain** (chave privada nunca sai do hardware seguro).
- Assinatura remota via protocolo de desafio, ou assinatura local do artefato devolvido pela nuvem (preferível — a nuvem nunca vê a chave).
- Fluxo guiado para Apple Developer Program e Google Play Console, com checklist do que falta.

## 13.4 Otimização de build

- **Stripping**: análise de alcançabilidade (IL trimming) remove código não usado. Alvo: runtime base < 12 MB.
- **Build mínimo de assets**: o índice de dependências garante que só entra o que é referenciado.
- **Texture streaming + Play Asset Delivery / On-Demand Resources**: app inicial pequeno, resto baixa depois.
- **Split por ABI e por densidade**.
- **Pré-compilação de shaders** e empacotamento do pipeline cache (elimina stutter no primeiro jogo).

---

# PARTE 14 — COLABORAÇÃO, VERSIONAMENTO E NUVEM

## 14.1 Versionamento visual (Git por baixo, humano por cima)

O Git é essencial e é incompreensível para 90% do público-alvo. Solução: **uma camada visual sobre Git real**.

```
┌─────────────────────────────────────────┐
│ HISTÓRICO                     [Salvar]  │
├─────────────────────────────────────────┤
│ ● agora     Você — "inimigos mais rápidos"│
│ │            3 cenas, 1 script            │
│ ● 2h atrás  Ana — "novo nível de floresta"│
│ │            [ver diferenças] [reverter]  │
│ ● ontem     Você — "arte do personagem"   │
│ │            ⟲ restaurar este ponto       │
└─────────────────────────────────────────┘
```

- Commit = "Salvar ponto", com título obrigatório e captura automática do viewport como thumbnail.
- **Diff visual de cena**: lista "3 objetos adicionados, 1 movido, textura X trocada" — com preview lado a lado, não texto.
- **Merge de cena assistido**: conflitos apresentados objeto a objeto ("Ana moveu a Porta, você mudou a cor da Porta → mesclar ambos?").
- Branches como "Experimentos" com nomes livres.
- **LFS automático** para binários.

## 14.2 Co-edição em tempo real

- **CRDT** (Conflict-free Replicated Data Type) sobre o grafo de cena → múltiplos usuários editando a mesma cena ao vivo, offline-tolerante.
- Cursores/seleções dos outros visíveis no viewport com nome.
- Bloqueio suave: quem está arrastando um objeto tem prioridade; os outros veem em fantasma.
- Chat de voz e anotações espaciais ("comentário fixado neste ponto da cena").
- Modo **Sala de Aula**: um professor projeta, alunos acompanham e enviam soluções.

---

# PARTE 15 — INOVAÇÕES DIFERENCIAIS (O QUE NINGUÉM TEM)

Estas são as apostas que fazem do Aether um produto novo, e não uma engine a mais.

| # | Inovação | Por que só é possível aqui | Impacto |
|---|---|---|---|
| **1** | **Três representações da lógica (Blocos ⇄ Grafo ⇄ C#) com round-trip real** | Exige projetar a AST como fonte da verdade desde o dia 1 | Elimina o teto do no-code. Uma criança e um profissional trabalham no mesmo arquivo |
| **2** | **Menu radial com memória muscular direcional** | Só faz sentido em touch | Torna a edição mobile competitiva em velocidade |
| **3** | **Captura fotogramétrica in-app** | O celular tem câmera, LiDAR (iPhone Pro), giroscópio e NPU | Andar em volta de um objeto e ter a malha texturizada na cena em 2 min |
| **4** | **Material a partir de uma foto** | Câmera + modelo de ML que estima albedo/normal/rugosidade de uma superfície | Fotografar um muro e ter um material PBR tileável |
| **5** | **Animação por captura de movimento com a câmera** | Modelo de pose 3D no dispositivo | Gravar o próprio movimento e aplicar num personagem, sem estúdio |
| **6** | **Debug com viagem no tempo** | Gravação circular de estado do ECS | Rebobinar 10 s de gameplay e inspecionar qualquer valor em qualquer frame |
| **7** | **Governador térmico como sistema de primeira classe** | Restrição exclusiva do mobile virada em recurso | Sessões longas sem throttle; o jogo publicado herda o mesmo sistema |
| **8** | **Aether Player + compartilhamento por link** | Modo 2 de build | Distribuição viral sem loja; o loop do Roblox num produto aberto |
| **9** | **Assistente de IA com diffs revisáveis** | Modelo local + nuvem | Reduz a curva de aprendizado sem tirar o controle |
| **10** | **Modo Lista para lógica** | Adaptação obrigatória à tela pequena que acabou sendo melhor | Lógica legível como pseudo-código em português |
| **11** | **Modo giroscópio de câmera** | Sensores do aparelho | Inspecionar uma cena movendo o telefone é natural e rápido |
| **12** | **Auto-rig e retargeting por ML no dispositivo** | NPU dos SoCs modernos | Remove a etapa que mais desiste iniciantes |
| **13** | **MicroMesh (geometria virtualizada mobile)** | Cull por meshlet em compute | "Importa e funciona" para modelos pesados |
| **14** | **Render graph inspecionável pelo usuário** | Arquitetura de grafo | O criador vê e entende o custo do próprio frame |
| **15** | **WAL de edição — trabalho impossível de perder** | Necessidade mobile (SO mata processos) | Confiança total; nunca existe "salvou?" |
| **16** | **Modo Sala de Aula + co-edição CRDT** | Colaboração móvel | Abre o mercado educacional que não tem laboratório de PC |
| **17** | **Um editor de nós para cinco domínios** | Decisão de unificação | Aprende uma vez, usa em material, VFX, geometria, lógica e animação |

---

# PARTE 16 — ROADMAP COMPLETO

## 16.0 Como ler este roadmap

- **10 fases**, cada uma com **etapas** e **sub-etapas**.
- Cada fase tem um **critério de saída demonstrável** — algo que se pode filmar. Se não dá para filmar, não está pronto.
- As durações assumem um time bem dimensionado (**~25-40 engenheiros** distribuídos nas trilhas). Para um time menor, multiplique e corte o que está marcado **[opcional]**.
- **Trilhas paralelas** correm o tempo todo: Desempenho, Testes, Documentação, Comunidade.
- **Regra de ouro do projeto:** *dogfooding desde a Fase 3*. A partir do momento em que o editor abre, a equipe faz jogos com ele toda sexta-feira. Nenhuma engine sobrevive sem isso.

### Visão macro

```
ANO 1 ─────────────────────────── ANO 2 ─────────────────────── ANO 3 ────────────
F0  F1        F2          F3            F4      F5      F6      F7     F8    F9
├──┼─────────┼──────────┼─────────────┼───────┼───────┼──────┼─────┼────┼──────┤
PoC Núcleo    Renderizador Editor       Simu-   No-code  Cria-  Gráf. Build  Lança-
              Vulkan       Mobile       lação   + C#     ção    avanç. Publ.  mento
    ▲          ▲            ▲            ▲       ▲        ▲      ▲      ▲     ▲
   M0         M1           M2           M3      M4       M5     M6     M7    M8
```

| Marco | Nome | O que prova |
|---|---|---|
| **M0** | *Triângulo no celular* | Vulkan + .NET + toque funcionam juntos |
| **M1** | *Mundo vivo* | ECS + jobs sustentam 100k entidades a 60 fps |
| **M2** | *Cena bonita* | Renderizador PBR com sombras roda a 60 fps num aparelho classe A |
| **M3** | **ALPHA INTERNA** — *Eu editei uma cena com o dedo* | A UX touch funciona; a equipe faz dogfooding |
| **M4** | *O jogo tem vida* | Física, animação e áudio integrados; um platformer jogável |
| **M5** | **ALPHA PÚBLICA** — *Fiz um jogo sem escrever código* | AetherFlow completo; primeiros usuários externos |
| **M6** | *Modelei um personagem no celular* | Suíte de criação integrada |
| **M7** | **BETA** — *Isso parece um jogo de 2026* | GI dinâmica, MicroMesh, upscaling |
| **M8** | **1.0** — *Publiquei na Play Store direto do celular* | Ciclo completo fechado |

---

## FASE 0 — FUNDAÇÃO E PROVA DE CONCEITO
### Duração: 2-3 meses · Objetivo: **matar os riscos fatais antes de investir**

Esta fase existe para responder cinco perguntas que, se respondidas "não", mudam o projeto inteiro.

### Etapa 0.1 — Espinha dorsal técnica *(3 semanas)*
- **0.1.1** Monorepo, sistema de build (CMake para nativo + .NET SDK), CI com dispositivos físicos.
- **0.1.2** Farm de dispositivos de teste: mínimo 12 aparelhos cobrindo Adreno, Mali, PowerVR, Apple, dos 4 perfis (S/A/B/C).
- **0.1.3** Shell nativo Android (`NativeActivity` / `GameActivity`) + iOS, com loop de aplicação, surface Vulkan e ciclo de vida correto (pausa/retomada/perda de surface — **o erro nº 1 em apps Vulkan mobile**).
- **0.1.4** Integração .NET no processo nativo: carregar CoreCLR/Mono, chamar C# do C++ e vice-versa.
- **0.1.5** Telemetria e logging desde o dia 1 (nada de `printf`).

### Etapa 0.2 — Provas de conceito de risco *(6 semanas, em paralelo)*

| PoC | Pergunta que responde | Critério de sucesso |
|---|---|---|
| **PoC-A: Vulkan + .NET** | O overhead de interop mata o desempenho? | 5.000 objetos renderizados a 60 fps com < 3 ms de CPU |
| **PoC-B: Gestos de edição** | Dá para mover/girar/escalar um objeto no dedo com precisão? | 10 testadores completam uma tarefa de posicionamento em < 30 s |
| **PoC-C: Térmica** | O editor pode rodar 30 min sem throttle? | < 4 W sustentados, sem queda abaixo de 55 fps |
| **PoC-D: Hot reload de C#** | O loop de iteração é viável no dispositivo? | Editar → ver mudança em < 2 s no Android e no iOS |
| **PoC-E: Compressão ASTC em GPU** | Importar uma textura 4K é instantâneo? | < 300 ms para 4096×4096 |

### Etapa 0.3 — Pesquisa de UX *(4 semanas, paralelo)*
- **0.3.1** Estudo com 20 usuários das personas P1/P2/P3 (entrevistas + teste de protótipo em papel/Figma).
- **0.3.2** Protótipo interativo de alta fidelidade da navegação de câmera, gizmos e menu radial (feito em Flutter/nativo, sem engine).
- **0.3.3** Teste de usabilidade do protótipo com métricas de tempo-para-tarefa.
- **0.3.4** Definição do sistema de design (tokens, tipografia, espaçamento, ícones, hápticos).

### Etapa 0.4 — Decisões arquiteturais registradas *(contínuo)*
- **0.4.1** ADRs para: linguagem, ECS vs cena, formato de arquivo, estratégia de build, backend gráfico.
- **0.4.2** Especificação do IDL de fronteira C#↔C++.
- **0.4.3** Definição dos orçamentos (memória, energia, frame time) como **testes automatizados**, não como documento.

> **✅ CRITÉRIO DE SAÍDA (M0):** um app instalável que desenha um cubo texturizado com Vulkan, que você gira com o dedo, cujo shader você edita em C# e vê atualizar em 2 s, rodando 30 min sem esquentar. Todos os 5 PoCs verdes ou com plano de mitigação aprovado.

> **⚠️ Se PoC-A ou PoC-C falharem:** reconsiderar a fronteira de linguagem (mais C++) ou o perfil de hardware alvo. Melhor descobrir aqui do que no mês 18.

---

## FASE 1 — NÚCLEO DA ENGINE
### Duração: 4 meses · Objetivo: **a fundação sobre a qual tudo é construído**

### Etapa 1.1 — Camada de plataforma *(4 semanas)*
- **1.1.1** Abstração de sistema de arquivos (assets, escopo de armazenamento do Android, iCloud/Files no iOS).
- **1.1.2** Entrada: toque multi-ponto com histórico e predição, caneta (pressão/inclinação), teclado, mouse/trackpad, gamepad, sensores.
- **1.1.3** Ciclo de vida robusto: pausa, retomada, perda/recriação de surface, mudança de configuração, memória baixa.
- **1.1.4** Janela/display: taxa de atualização variável, notch/safe area, multi-janela, display externo.
- **1.1.5** Energia e térmica: leitura de estado, API do `PowerGovernor`.

### Etapa 1.2 — Memória e concorrência *(5 semanas)*
- **1.2.1** Alocadores: arena de frame, pools por tipo, slab para chunks, alocador de rastreamento (debug).
- **1.2.2** Job system com work-stealing, dependências e afinidade big.LITTLE.
- **1.2.3** Primitivas sem trava (filas SPSC/MPMC, contadores atômicos).
- **1.2.4** Instrumentação: rastreador de alocações com atribuição por subsistema; visualizador de jobs.
- **1.2.5** Analisador Roslyn `[NoAlloc]`.

### Etapa 1.3 — ECS *(6 semanas)*
- **1.3.1** Armazenamento por arquétipo em chunks, com versionamento de componentes (para *change detection*).
- **1.3.2** Sistema de consultas compiladas e cacheadas, com filtros.
- **1.3.3** Buffers de comando estruturais (criar/destruir/mudar arquétipo) aplicados em pontos de sincronização.
- **1.3.4** Hierarquia como componente + sistema de propagação de transform (paralelo, por nível).
- **1.3.5** Fachada `Node` sobre o ECS, com API amigável.
- **1.3.6** Benchmark: 100k entidades com transform + hierarquia a 60 fps na CPU classe A.

### Etapa 1.4 — Reflexão, serialização e recursos *(5 semanas)*
- **1.4.1** Source generator de metadados de componente.
- **1.4.2** Serializador binário (rápido) + texto (versionável), com migração de versão de esquema.
- **1.4.3** Sistema de recursos: GUID, referência fraca/forte, carregamento assíncrono, contagem de uso.
- **1.4.4** Sistema de comandos de edição (undo/redo) + WAL de recuperação.
- **1.4.5** Índice de dependências em SQLite.

### Etapa 1.5 — Matemática, tempo e utilitários *(2 semanas)*
- **1.5.1** Biblioteca math com SIMD NEON e testes de precisão.
- **1.5.2** Tempo: fixed step, interpolação, escala de tempo, pausa.
- **1.5.3** Eventos e sinais tipados.
- **1.5.4** Sistema de configuração/preferências.

> **✅ CRITÉRIO DE SAÍDA (M1):** benchmark headless com **100.000 entidades** hierarquizadas, animadas por um sistema simples, rodando a 60 fps num aparelho classe A com **zero alocações de GC por frame** e < 6 ms de CPU. Salvar e recarregar o estado é bit-exato.

---

## FASE 2 — RENDERIZADOR VULKAN
### Duração: 5 meses · Objetivo: **o pipeline gráfico base, projetado para TBDR desde a primeira linha**

### Etapa 2.1 — RHI Vulkan *(6 semanas)*
- **2.1.1** Inicialização: instância, seleção de dispositivo, filas, swapchain com recriação robusta.
- **2.1.2** Alocação de memória com VMA + budgets por categoria.
- **2.1.3** Objetos: buffers, imagens, samplers, pipelines, com cache hasheado.
- **2.1.4** **Bindless** via `descriptor_indexing`: tabelas globais de texturas/buffers/samplers.
- **2.1.5** Gravação de command buffers multi-thread; timeline semaphores.
- **2.1.6** Camadas de validação, marcadores de debug (RenderDoc/AGI), captura de frame.
- **2.1.7** Detecção de capabilities e perfis de dispositivo (base de dados de GPUs conhecidas + fallbacks).
- **2.1.8** Política global resolvida: separar capabilities, calibração de desempenho,
  Project Settings e estado térmico; presets versionados e override de teste explícito.
- **2.1.9** Laboratório reproduzível: AVD C sintético para correção/fallback e matriz
  física Adreno+Mali+C para FPS/GPU/potência, sempre com cena/câmera/hash registrados.

> **Fatia integrada em 29/08/2026:** `Aether-C-Synthetic` já possui definição JSON e
> criador PowerShell versionados; o runtime/runner já vinculam sceneId, fingerprint,
> câmera e contagens ao perfil. O baseline físico Release do mapa real foi fixado no
> Xiaomi SM8735/Adreno, 2772×1280 e painel efetivamente em 120 Hz: câmera
> `0,160,-100,0,0.08`, fingerprint `dd907ec34bbebc21`, 78,12 presents/s, CPU média
> 1,41 ms e GPU média/pior p95 de janela 11,40/18,41 ms. A captura prova gargalo GPU,
> não fecha ganho A/B nem soak. O coletor SurfaceFlinger passou a rodar concorrentemente
> para não perder a janela circular, mantém invalidação conservadora e registra sua janela
> independente das janelas nativas. Um experimento global de backface culling foi retirado
> porque ainda removeu terreno/folhagem (9,42% dos pixels divergiram do baseline) e o A/B
> físico regrediu 97,46→86,34 presents/s, com GPU média 9,04→9,98 ms. A decisão
> evita transformar uma hipótese em preset global; uma nova tentativa depende de captura
> AGI e semântica de cobertura versionada no AEMAP. Em 30/08, o runner também ganhou
> isolamento GPU diagnóstico global por variantes de pipeline (`full`, `no-normal`,
> `no-ibl`, `base-color`), registrado junto da cena. O A/B especializado
> `full → base-color → full` mediu GPU média 9,72→10,08→11,04 ms e SurfaceFlinger
> 93,45→88,51→91,61/s: retirar o fragment PBR não provou ganho, logo não virou preset
> nem redução gráfica. O próximo gate é atribuição física de vertex/raster/tiles,
> visibilidade e frame pacing. A primeira resposta foi o AEMAP v2 global: stride de
> vértice 72→48 bytes com posição/UV float32, direções SNORM16, cor UNORM8 e decoder v1
> preservado. O asset/APK caíram 10,20 MB; o gate visual teve máximo 1/255 e dois runs
> Adreno mediram GPU 8,89/7,42 ms e SurfaceFlinger 110,20/111,85/s. A matriz Mali,
> Game Loop, percurso automatizado e política C forçada por interface de teste continuam
> abertos; portanto 2.1.9 permanece parcial.

### Etapa 2.2 — Compilação de shaders *(4 semanas)*
- **2.2.1** Pipeline Slang/HLSL → SPIR-V, com reflexão automática de bindings.
- **2.2.2** Sistema de variantes com orçamento e cache em disco.
- **2.2.3** Pipeline cache persistido + pré-aquecimento na primeira execução.
- **2.2.4** Compilação em background (nunca no frame) com material de fallback rosa.
- **2.2.5** Biblioteca de shaders base com precisão explícita (`mediump`/`highp`).

### Etapa 2.3 — Render Graph *(6 semanas)*
- **2.3.1** Declaração de passes, recursos transitórios, dependências.
- **2.3.2** Compilação do grafo: ordenação topológica, poda de passes mortos.
- **2.3.3** Inserção automática de barreiras `synchronization2`.
- **2.3.4** **Aliasing de memória transitória** com verificação de correção.
- **2.3.5** **Fusão de passes em subpasses** e marcação de anexos *memoryless* — a otimização mais importante do mobile.
- **2.3.6** Visualizador do grafo (base do recurso de inspeção do usuário).

### Etapa 2.4 — Pipeline de renderização direta *(6 semanas)*
- **2.4.1** Depth prepass + Forward+ com clusterização de luzes (froxels em compute).
  - Fatura parcial validada em hardware: prepass seletivo de cobertura para materiais
    alpha-mask, global e orientado por flags do material. Preserva o PBR e reduziu
    21,6–25,4% do tempo GPU no hotspot; o Forward+ e o depth prepass geral continuam
    pendentes, portanto o item não está concluído.
- **2.4.2** BRDF PBR completo (GGX multiscatter, Burley, Fresnel), com validação contra referência offline.
- **2.4.3** Sombras: cascaded shadow maps com PCF/PCSS, sombras de spot e point (cubemap com atlas).
- **2.4.4** IBL: skybox HDR, pré-filtragem de especular, SH de irradiância, reflection probes.
- **2.4.5** Transparência ordenada + partículas básicas.
- **2.4.6** Pós-processamento: exposição automática, bloom (dual-filter), tonemapping AgX, LUT de grading, FXAA→TAA.

### Etapa 2.5 — Culling e batching *(4 semanas)*
- **2.5.1** BVH de cena com atualização incremental.
- **2.5.2** Frustum + occlusion culling (HZB de dois passos).
- **2.5.3** Instancing automático por malha+material; GPU instancing bindless.
- **2.5.4** Sistema de LOD com transição por dither temporal.
- **2.5.5** Ordenação de draws por PSO e por profundidade (front-to-back para opacos).
- **2.5.6** Telemetria de visibilidade por perfil: draws/triângulos submetidos,
  visíveis e ocluídos, LOD por erro projetado e motivo de fallback, sem regra por cena.

> **Fatia integrada em 30/08/2026:** o runtime Vulkan agora constrói um frustum
> backend-independent uma vez por frame e filtra bounds esféricos dos draw packets
> antes de gravar comandos. A política é conservadora (`radius*1,05 + 0,5`), falha
> aberta para câmera/bounds inválidos, reutiliza listas scratch sem alocação e mantém
> a ordenação opaca front-to-back/transparente back-to-front somente entre visíveis.
> A telemetria registra draws candidatos, visíveis, descartados, chamadas realmente
> submetidas e triângulos visíveis/submetidos. Isso entrega a primeira metade de
> 2.5.2, 2.5.5 e 2.5.6 sem reduzir resolução, materiais, texturas, iluminação ou
> distância. Ainda não conclui a etapa: os 27 draws atuais são agrupados por material
> e possuem bounds grandes; a sequência obrigatória é visualizador de bounds → render
> chunks espaciais persistentes + `DrawIndexedIndirectCount` → HZB com histerese → LOD
> por erro projetado. Subdividir em centenas de draws CPU continua proibido.
> Na primeira execução física, a câmera em movimento variou entre 27/27 e 18/27
> draw packets visíveis (341.109→325.827 triângulos lógicos). O mecanismo funciona,
> mas a pequena redução de triângulos confirma que os grupos pesados ainda precisam
> de render chunks/indirect antes de HZB.
>
> O benchmark de aceite passa a ser uma rota móvel determinística de 60 s, não FPS
> parado. O relato atual (picos de 120, faixa de 60–80 e quedas a ~47 FPS) será
> capturado com GPU/CPU p95, térmica, ADPF e counters de visibilidade. A estabilização
> observada com gravador de tela será tratada como experimento A/B de DVFS/compositor;
> a engine não gera carga artificial nem usa ganchos privados para forçar clocks.
>
> **Segunda fatia integrada em 30/08/2026:** a medição física separou ocupação de CPU
> de throughput. Aproximadamente 7,3% de CPU coexistiu com espera de `acquire` de
> 9–22 ms e GPU acima do budget de 8,33 ms, portanto elevar carga/clock da CPU não é
> a correção do gargalo atual. O vertex shader passou a consumir linhas world-to-view
> calculadas uma vez por frame; o PBR removeu trigonometria, `pow` e normalizações
> redundantes sem trocar materiais ou iluminação. O mapa agora é repartido de forma
> determinística em 58 render chunks (máximo 8.192 triângulos), preservando todos os
> índices/triângulos e a ordem de blend, e os chunks visíveis são compactados por
> material em multi-draw indirect quando a capability existe. No Xiaomi, o caminho
> ativo reduziu 82 draws CPU da primeira versão para 34 no ponto fixo e elevou a mesma
> câmera de 85,86 FPS/10,25 ms GPU (v7) para 99,46 FPS/9,23 ms (v10), com imagem
> equivalente. Em movimento, as cinco primeiras janelas ficaram em média 98,34 FPS,
> mas uma janela com 57/58 chunks e 525.704 triângulos ainda caiu a 64,39 FPS/13,73 ms
> GPU. Assim, render chunks/indirect estão entregues, porém 2.5 continua parcial.
> A próxima dependência passa a ser rota determinística → HZB conservador com
> histerese → LOD por erro projetado com coverage/dither; frames em voo só serão
> ampliados depois de recursos per-frame e latência toque→pixel estarem no gate.
> A política Android também consulta suporte público a sustained performance; o
> Xiaomi respondeu `false`, mantendo fallback sem hooks privados ou carga artificial.

> **Revisão/medição de 31/08/2026:** a rota móvel deixou de ser pendência: o
> formato `.aeroute` foi validado no Android e `forest-walk-v1` contém 6.611 poses,
> 55,09 s e 487,16 unidades percorridas. O baseline Release completo ficou em
> 95,16 presents/s, pior janela 84,18, CPU 1,34 ms, GPU 9,13 ms e pior GPU-p95
> 12,93 ms; um intervalo isolado equivaleu a 53,9 FPS. Cada janela agora carrega
> o ordinal da rota e o volume visível para localizar o hotspot, e o runner recusa
> benchmark da floresta sem pose explícita ou Record/Replay. HZB foi corrigido
> (depth normalizado, store, pré-rotação e capability), porém o readback CPU só é
> elegível acima do orçamento global de candidatos e câmera móvel falha aberta;
> same-frame GPU-driven continua a próxima arquitetura. O agrupador LOD agora
> preserva múltiplos chunks por nível e o dither é complementar, mas o asset atual
> é AEMAP v2/zero grupos, logo LOD continua sem ganho físico declarado. O A/B de
> coverage em 2.048 triângulos/chunk foi rejeitado; 8.192 permanece o default. O
> prepass seletivo `MASK` foi então isolado na mesma rota: ligado 93,61 → desligado
> 80,89 → ligado 92,45 presents/s, com GPU média 9,32→11,01 ms (+18,1%) e CPU
> praticamente invariável. Ele permanece habilitado globalmente; a próxima
> intervenção partiu da pose fixa do `route_frame≈1734`: full 83,97–85,92 FPS/
> 10,37–10,39 ms GPU, base-color 120,08/6,08, no-normal 94,41/9,14 e no-IBL
> 87,47/9,98. Como primeira correção sem alteração visual, os descritores bindless
> por lote deixaram de ser marcados incorretamente como não uniformes. O v15 físico
> ficou em 83,05–83,65 FPS/~10,38 ms GPU, sem ganho contra v14; o driver já otimizava
> esse caso. Próxima fatia: separar fetch/TBN/folhagem, integrar compressão e filtragem
> semântica de normal + material LOD por erro projetado, depois AGI, HZB
> same-frame/GPU-driven e recook AEMAP v3/LOD.

> **Correção Android integrada na mesma fatia:** `sensorLandscape` agora trata a
> troca física entre os dois lados como mudança real de display. Gestos ativos são
> cancelados na fronteira de configuração, uma Surface Vulkan nova é criada quando
> `Display.getRotation()` muda (a dimensão WxH pode permanecer idêntica em 180°) e o
> HUD usa a transformação de eixos do display sem reaplicar o sinal já tratado pelo
> compositor. Configurações sem rotação continuam no caminho barato de recriação da
> swapchain.

### Etapa 2.6 — Renderizador 2D *(3 semanas)*
- **2.6.1** Sprite batcher, atlas dinâmico, ordenação por camada.
- **2.6.2** Tilemap com chunking em GPU.
- **2.6.3** Iluminação 2D e sombras.

> **✅ CRITÉRIO DE SAÍDA (M2):** o **Sponza** (ou uma cena equivalente com ~500k triângulos, 30 luzes dinâmicas, materiais PBR e sombras) roda a **60 fps estáveis** num aparelho classe A, consumindo < 3.5 W, com o render graph provando fusão de subpasses e G-buffer memoryless. Roda também a **30 fps sustentados** no perfil C, com meta evolutiva de 45 fps, sem remover conteúdo: algoritmos/budgets globais podem variar apenas dentro do gate visual automatizado. O AVD C não fecha este critério; exige Mali físico e relatório reproduzível.

---

## FASE 3 — O EDITOR MOBILE *(a fase que define o produto)*
### Duração: 6 meses · Objetivo: **provar que editar no celular é bom, não apenas possível**

### Etapa 3.1 — Framework de UI *(6 semanas)*
- **3.1.1** Sistema de UI retido com layout flex/constraint, virtualização de listas, e renderização por GPU (SDF para texto e formas).
- **3.1.2** Sistema de design implementado: tokens, tema claro/escuro, escala, ícones vetoriais.
- **3.1.3** Animação e física de UI (molas, momentum, snap) — a "sensação" de qualidade vem daqui.
- **3.1.4** Feedback háptico com vocabulário definido (seleção, snap, erro, confirmação).
- **3.1.5** Acessibilidade: leitor de tela, escala, alto contraste, alvos mínimos.
- **3.1.6** **Bottom Sheet Stack** com estados, empilhamento e navegação por gesto.

### Etapa 3.2 — Reconhecimento de gestos *(5 semanas)*
- **3.2.1** Máquina de estados de gestos com resolução de conflito (o problema mais subestimado: distinguir "orbitar" de "arrastar objeto" de "laço").
- **3.2.2** Predição de toque para reduzir latência percebida.
- **3.2.3** Gestos bimanuais e modificadores (segundo dedo = precisão).
- **3.2.4** **Menu radial** com seleção direcional, sub-anéis e configuração.
- **3.2.5** Suporte a caneta com pressão/inclinação/hover.
- **3.2.6** Camada de atalhos para teclado/mouse externos.

### Etapa 3.3 — Viewport *(6 semanas)*
- **3.3.1** Controle de câmera completo (todos os gestos da seção 8.4) com inércia e limites.
- **3.3.2** Gimbal de eixos, grade adaptativa, overlays (wireframe, normais, colisores, luzes).
- **3.3.3** **Gizmos touch-first** com áreas ampliadas, HUD numérico, anti-oclusão e lupa.
- **3.3.4** Seleção: toque, laço, por hierarquia, por material; realce com contorno.
- **3.3.5** Snap: grade, vértice, superfície, ângulo, com háptico.
- **3.3.6** Modos de visualização de debug (overdraw, complexidade de shader, mipmaps, densidade de textura).

> **Fatia runtime integrada em 30/08/2026 (3.3.1 + 4.1.5, ainda parcial):** o
> benchmark da floresta possui `FirstPersonController` desacoplado da câmera,
> joystick flutuante multi-touch, look por pointer ID, cápsula `CharacterVirtual`,
> gravidade em fixed step, malha estática mundial e HUD de FPS Vulkan. A conversão
> AEMAP→colisão vive no módulo `renderer`, aplica a matriz de cada draw e compacta
> apenas vértices referenciados. Transparência visual e física são contratos
> separados: BLEND permanece colidível; alpha-mask usa default não físico e aceita
> `NoCollision`/`ForceCollision` como metadata de importação. No Xiaomi, a revisão
> da estrada registrou 188.681 vértices/156.119 triângulos físicos e posições Y
> estáveis com `ground=0`. O HUD final corrige ordem e bitmap dos dígitos, mas seu
> APK v5 aguarda inspeção no aparelho porque a bateria encerrou a conexão ADB.

### Etapa 3.4 — Painéis principais *(8 semanas)*
- **3.4.1** **Hierarquia**: árvore virtualizada, arrastar para reparentar, busca, multi-seleção, favoritos.
- **3.4.2** **Inspector adaptativo** gerado por reflexão, com todos os widgets touch da seção 8.7.
- **3.4.3** **Navegador de assets** com previews reais, busca fuzzy, coleções, drag-and-drop.
- **3.4.4** **Console** com filtros, agrupamento e navegação para a origem.
- **3.4.5** Cartão contextual do viewport com propriedades favoritas.
- **3.4.6** Barra superior, doca de modos, e o sistema de troca de modos.

### Etapa 3.5 — Fluxos de projeto *(4 semanas)*
- **3.5.1** Tela inicial: projetos recentes, templates, tutoriais, comunidade.
- **3.5.2** Criação de projeto com templates jogáveis (plataforma 3D, top-down 2D, FPS, corrida, puzzle).
- **3.5.3** Sistema de prefabs com overrides, variantes e merge visual.
- **3.5.4** Salvamento contínuo + recuperação de crash (WAL).
- **3.5.5** Importação de arquivos: galeria, câmera, nuvem, ZIP.

### Etapa 3.6 — Play in Editor *(4 semanas)*
- **3.6.1** Processo Play separado com IPC por memória compartilhada.
- **3.6.2** Exibição do jogo dentro do viewport (AHardwareBuffer/IOSurface compartilhado).
- **3.6.3** Inspeção ao vivo durante o play (editar valores e ver o efeito).
- **3.6.4** Pausa, avanço quadro a quadro, câmera livre durante o play.
- **3.6.5** Isolamento de crash com stack trace apresentável.

### Etapa 3.7 — Onboarding *(3 semanas)*
- **3.7.1** Tutorial interativo integrado (não vídeo — o app guia com destaques e gestos).
- **3.7.2** Três níveis de UI (Essencial/Padrão/Completo) com revelação progressiva.
- **3.7.3** Dicas contextuais e "por que isso está assim?".

> **✅ CRITÉRIO DE SAÍDA (M3 — ALPHA INTERNA):** um membro da equipe que nunca usou o editor monta uma cena com 20 objetos, luzes, materiais e um prefab, em menos de 15 minutos, **usando apenas os dedos**, e aperta Play. Toda a equipe passa a fazer dogfooding semanal.

> **📊 Teste de usabilidade obrigatório:** 15 usuários externos das personas P1-P3. Tempo-para-primeira-cena e SUS (System Usability Scale) medidos. **Meta: SUS ≥ 72.** Se não atingir, a Fase 3 não terminou.

---

## FASE 4 — SIMULAÇÃO
### Duração: 4 meses · Objetivo: **o mundo se move, soa e reage**

### Etapa 4.1 — Física *(6 semanas)*
- **4.1.1** Integração do Jolt: mundo, corpos, formas, dormência, camadas de colisão.
- **4.1.2** Fachada C# com componentes (`RigidBody`, `Collider`, `Trigger`) e sincronização ECS↔Jolt sem cópias.
- **4.1.3** Juntas e motores, com gizmos de edição no viewport.
- **4.1.4** Queries (raycast/shapecast/overlap) expostas a script e a nós.
- **4.1.5** **Character Controller** de alta qualidade (o diferencial): degraus, rampas, plataformas móveis, agachar, correr, deslizar, escalar, nadar.
- **4.1.6** Física 2D (benchmark Jolt-2D vs Box2D v3 → decisão).
- **4.1.7** Geração automática de colisores (convex decomposition) na importação.
- **4.1.8** Determinismo em ponto fixo (modo opcional) e testes de reprodutibilidade.
- **4.1.9** Escalonamento térmico (sub-steps, islands).

### Etapa 4.2 — Animação *(7 semanas)*
- **4.2.1** Esqueleto, poses, skinning em compute (linear + dual quaternion).
- **4.2.2** Compressão de clips (ACL) e amostragem otimizada.
- **4.2.3** **Grafo de animação**: state machine hierárquica, blend 1D/2D, camadas com máscara, aditivos, sub-grafos.
- **4.2.4** Editor de grafo de animação (reusa o canvas de nós).
- **4.2.5** IK: two-bone, FABRIK, look-at, **foot placement** automático.
- **4.2.6** Root motion + integração com character controller.
- **4.2.7** Blend shapes e animação facial.
- **4.2.8** **Retargeting humanoide** automático.
- **4.2.9** Eventos de animação (disparam nós/scripts em frames específicos).
- **4.2.10** **Timeline/Sequencer** touch: trilhas, keyframes arrastáveis, curvas, preview de scrub.

### Etapa 4.3 — Áudio *(5 semanas)*
- **4.3.1** Backend AAudio/AudioUnit com thread real-time e buffer curto.
- **4.3.2** Grafo de áudio: fontes, buses, submixes, roteamento visual.
- **4.3.3** Espacialização HRTF, atenuação, cones, doppler.
- **4.3.4** Efeitos: reverb (convolução + algorítmico), EQ, compressor, limiter, delay, filtro de oclusão.
- **4.3.5** Zonas de reverb com blend, com preset automático a partir do SDF da cena.
- **4.3.6** Sistema de música adaptativa (camadas, transições sincronizadas, stingers).
- **4.3.7** Gravação por microfone dentro do editor + edição básica (trim, normalizar, fade).
- **4.3.8** Streaming de áudio longo e gerenciamento de vozes com prioridade.

### Etapa 4.4 — Partículas e VFX *(4 semanas)*
- **4.4.1** Simulação GPU em compute com pools de partículas.
- **4.4.2** Editor de VFX por nós (emissão, forças, colisão com depth/SDF, sub-emissores).
- **4.4.3** Renderização: billboard, mesh, ribbon/trail, com iluminação.
- **4.4.4** Decals com clustering.

### Etapa 4.5 — IA e navegação *(4 semanas)*
- **4.5.1** Geração de navmesh no dispositivo (Recast) com tiles incrementais.
- **4.5.2** Pathfinding + funil + evitação local (ORCA).
- **4.5.3** Behavior trees e state machines com editor visual.
- **4.5.4** Percepção (visão/audição) e steering behaviors prontos.

### Etapa 4.6 — UI de jogo *(4 semanas)*
- **4.6.1** Canvas com layout responsivo (anchors, flex, safe area), sprites 9-slice, texto com SDF e fontes dinâmicas.
- **4.6.2** Componentes: botão, slider, toggle, scroll, input, lista virtualizada.
- **4.6.3** Sistema de navegação por gamepad/teclado.
- **4.6.4** Data binding (vincular um texto a uma variável) — exposto ao AetherFlow.
- **4.6.5** Editor de UI touch com preview em múltiplas resoluções.
- **4.6.6** Localização (tabelas de string, pluralização, RTL).

> **✅ CRITÉRIO DE SAÍDA (M4):** um **platformer 3D jogável** feito inteiramente no editor: personagem animado com blend de andar/correr/pular, física, inimigos com navmesh, áudio espacial, partículas, HUD e menu. Roda a 60 fps no perfil A e 30 fps no perfil C.

---

## FASE 5 — AETHERFLOW (NO-CODE) E SCRIPTING C#
### Duração: 5 meses · Objetivo: **qualquer pessoa consegue programar; quem sabe programar não é limitado**

### Etapa 5.1 — Fundação da linguagem *(6 semanas)*
- **5.1.1** Definição da AST do AetherFlow (a fonte da verdade das três representações).
- **5.1.2** Sistema de tipos, inferência, coerção segura, genéricos limitados.
- **5.1.3** Formato de arquivo `.aflow` (texto versionável) + serialização.
- **5.1.4** Validador semântico (ciclos, tipos, pinos obrigatórios, alcançabilidade).
- **5.1.5** Compilador AST → C# gerado legível.
- **5.1.6** Interpretador de AST para iteração instantânea no editor.

### Etapa 5.2 — Round-trip C# ⇄ Grafo *(6 semanas)*
- **5.2.1** Parser C# (Roslyn) → AST do Flow para o subconjunto suportado.
- **5.2.2** Regras de preservação: comentários, formatação, nomes.
- **5.2.3** Nó "caixa de código" para construções fora do subconjunto.
- **5.2.4** Testes de ida-e-volta (property-based: gerar grafos aleatórios, converter e comparar).

### Etapa 5.3 — Canvas de nós *(7 semanas)*
- **5.3.1** Canvas com pan/zoom fluido, virtualização (grafos de 1000+ nós), e níveis de detalhe (nós distantes viram caixas).
- **5.3.2** Criação de nós: paleta com busca, arrastar de um pino para o vazio abre paleta filtrada por tipo.
- **5.3.3** Conexão de fios por arrasto, com magnetismo, validação de tipo e conversão automática.
- **5.3.4** Layout automático (dagre/sugiyama) e "arrumar" com um toque.
- **5.3.5** Grupos, comentários, cores, colapso em sub-grafo.
- **5.3.6** Reroute nodes e organização de fios.

### Etapa 5.4 — Modo Lista e Modo Blocos *(6 semanas)*
- **5.4.1** **Modo Lista**: renderização indentada da AST, reordenação por arrasto, edição inline.
- **5.4.2** **Modo Blocos**: encaixe físico estilo Scratch para a persona P1, com subconjunto guiado.
- **5.4.3** Transição animada entre as três representações (a mesma lógica, morfando).
- **5.4.4** Escolha automática de representação por tamanho de tela e nível do usuário.

### Etapa 5.5 — Biblioteca de nós *(8 semanas, paralelo)*
- **5.5.1** Implementação de todas as categorias da seção 9.5 (~400 nós).
- **5.5.2** **Geração automática de nós a partir de C#**: qualquer método marcado `[FlowNode]` vira nó com pinos derivados da assinatura. Garante paridade permanente entre código e no-code.
- **5.5.3** Documentação inline de cada nó (descrição, exemplo, vídeo curto).
- **5.5.4** Macros e sub-grafos reutilizáveis, publicáveis.

### Etapa 5.6 — Depuração visual *(5 semanas)*
- **5.6.1** Fluxo animado nos fios de execução; valores exibidos nos fios de dado.
- **5.6.2** Breakpoints, step, watch.
- **5.6.3** **Time-travel debugging**: gravação circular do estado do ECS + valores de nós, com scrubbing.
- **5.6.4** Mapa de calor de custo por nó.

### Etapa 5.7 — Scripting C# completo *(6 semanas)*
- **5.7.1** Compilação Roslyn no dispositivo, incremental.
- **5.7.2** Hot reload com migração de estado (`AssemblyLoadContext`).
- **5.7.3** Editor de código touch: barra de símbolos, autocompletar, snippets, diagnósticos inline, navegação estrutural.
- **5.7.4** Ditado por voz com gramática de código **[opcional]**.
- **5.7.5** Depurador de C# (breakpoints, variáveis, call stack) no dispositivo.
- **5.7.6** API de gameplay completa nos três níveis (`Behavior` → `System` → `IJobSystem`).
- **5.7.7** Sandbox de segurança para código de projetos baixados da comunidade.

### Etapa 5.8 — Assistente de IA *(5 semanas)*
- **5.8.1** Modelo local pequeno para autocompletar de nós e sugestões.
- **5.8.2** Serviço em nuvem para "descrever → grafo", "explicar", "consertar".
- **5.8.3** Apresentação sempre como **diff revisável**, nunca aplicação direta.
- **5.8.4** Controles de privacidade explícitos e opt-in.

> **✅ CRITÉRIO DE SAÍDA (M5 — ALPHA PÚBLICA):** um testador sem experiência em programação cria, em 2 horas, um jogo completo (menu, gameplay, pontuação, game over) usando apenas o AetherFlow. Um desenvolvedor experiente pega o mesmo projeto, converte um grafo para C#, otimiza e volta para o grafo — **sem perder nada**. Programa de alpha pública com 500-2000 usuários aberto.

---

## FASE 6 — FERRAMENTAS DE CRIAÇÃO DE CONTEÚDO
### Duração: 6 meses · Objetivo: **o papel do Blender, dentro do celular**

### Etapa 6.1 — Pipeline de assets completo *(6 semanas)*
- **6.1.1** Importadores: glTF 2.0 (com extensões), FBX, OBJ, USD/USDZ, VRM, Collada, STL/PLY.
- **6.1.2** Imagens: PNG/JPG/WebP/EXR/HDR/TGA/PSD com camadas.
- **6.1.3** Processamento de malha: otimização de cache, geração de LOD, tangentes, quantização.
- **6.1.4** **Compressão ASTC/ETC2 em compute shader** + KTX2/Basis.
- **6.1.5** Cache endereçado por hash, import incremental, preview progressivo.
- **6.1.6** Streaming de texturas por mip com feedback buffer.

### Etapa 6.2 — Modelagem poligonal *(8 semanas)*
- **6.2.1** Estrutura half-edge com edição paralela e histórico.
- **6.2.2** Modo de edição de malha: seleção de vértice/aresta/face, loops, rings, crescer/encolher.
- **6.2.3** Operadores por gesto: extrude, inset, bevel, loop cut, knife, bridge, merge, dissolve.
- **6.2.4** Snap, simetria, proportional editing.
- **6.2.5** **Pilha de modificadores** não-destrutivos com preview ao vivo.
- **6.2.6** Booleanas robustas (biblioteca em C++).
- **6.2.7** Primitivas paramétricas editáveis.

### Etapa 6.3 — Escultura *(7 semanas)*
- **6.3.1** Motor de escultura com dyntopo e multiresolução.
- **6.3.2** ~15 pincéis com curvas de falloff, alpha e textura.
- **6.3.3** Suporte a pressão/inclinação de caneta.
- **6.3.4** Máscaras, camadas de escultura, simetria radial.
- **6.3.5** Voxel remesh e quad remesh (retopologia automática) em compute.
- **6.3.6** Retopologia manual por traçado.

### Etapa 6.4 — UV e texturização *(6 semanas)*
- **6.4.1** Unwrap automático (xatlas) + marcação manual de costuras.
- **6.4.2** Editor de UV touch com manipulação de ilhas, packing, escala consistente.
- **6.4.3** Pintura 3D direta na malha com camadas, máscaras e modos de mesclagem.
- **6.4.4** Camadas procedurais com máscaras por curvatura/AO/altura/inclinação.
- **6.4.5** Baking (normal, AO, curvatura, position, ID) em GPU.

### Etapa 6.5 — Editor de materiais e shaders *(5 semanas)*
- **6.5.1** Shader Graph no canvas de nós compartilhado.
- **6.5.2** Biblioteca de nós de shader (~150) + funções customizadas em HLSL.
- **6.5.3** Preview em tempo real com o pipeline real, em objeto escolhido.
- **6.5.4** Sistema de instâncias de material com overrides.
- **6.5.5** Validação de custo (contagem de instruções, avisos de precisão).

### Etapa 6.6 — Terreno, vegetação e mundo aberto *(6 semanas)*
- **6.6.1** Terreno com CDLOD, chunking e streaming.
- **6.6.2** Pincéis de escultura de terreno + erosão simulada.
- **6.6.3** Pintura de camadas com splat + triplanar.
- **6.6.4** Espalhamento de vegetação por regras, com instancing e impostores.
- **6.6.5** Água com Gerstner/FFT e interação de flutuação.
- **6.6.6** **World Partition**: células, data layers, streaming por distância, HLOD.

### Etapa 6.7 — Geometry Nodes *(5 semanas)*
- **6.7.1** Sistema de campos de atributo e avaliação de grafo geométrico.
- **6.7.2** ~100 nós procedurais.
- **6.7.3** Cache e avaliação em runtime.
- **6.7.4** Exemplos prontos: cidade, escada, cerca, dungeon, floresta.

### Etapa 6.8 — Inovações de captura *(6 semanas)*
- **6.8.1** **Fotogrametria in-app**: captura guiada (o app diz onde andar), SfM + MVS na NPU/GPU, malha + textura, com limpeza automática.
- **6.8.2** **LiDAR/Depth scan** onde disponível (iPhone Pro, alguns Android) para captura de ambiente.
- **6.8.3** **Material a partir de foto**: estimativa de albedo/normal/rugosidade por ML + tileamento automático.
- **6.8.4** **Mocap por câmera**: pose 3D em tempo real → clip de animação, com limpeza e retargeting.
- **6.8.5** **Auto-rigging por ML**: esqueleto + skin weights a partir de uma malha humanoide.

> **✅ CRITÉRIO DE SAÍDA (M6):** um artista modela, esculpe, faz UV, texturiza, riga e anima um personagem **inteiramente no celular**, e o coloca jogável numa cena — sem tocar num PC. Um segundo teste: escanear um objeto real com a câmera e tê-lo na cena com material PBR em < 5 min.

---

## FASE 7 — GRÁFICOS DE ÚLTIMA GERAÇÃO
### Duração: 5 meses · Objetivo: **a diferença visível entre "jogo mobile" e "jogo de verdade"**

### Etapa 7.1 — GPU-Driven Rendering e MicroMesh *(8 semanas)*
- **7.1.1** Geração de meshlets e DAG de LOD no import (particionamento + simplificação por erro quadrático).
- **7.1.2** Cull hierárquico por cluster em compute (frustum, cone de normais, HZB dois passos).
- **7.1.3** Seleção de nível por erro projetado em pixels.
- **7.1.4** Desenho indireto (`DrawIndexedIndirectCount`) + caminho de **mesh shaders** onde disponível.
- **7.1.5** Residência e prefetch de níveis de geometria.
- **7.1.6** Fallback completo para perfis B/C (LOD discreto tradicional).

### Etapa 7.2 — Iluminação global (GlowField) *(9 semanas)*
- **7.2.1** **SDF global da cena**: geração esparsa em compute, atualização incremental.
- **7.2.2** **DDGI**: grades cascateadas de sondas, octaedros de irradiância + visibilidade, atualização amortizada.
- **7.2.3** Ray marching no SDF para os raios das sondas; filtragem temporal e rejeição de luz vazada.
- **7.2.4** Caminho com `ray_query` (hardware RT) para perfil S.
- **7.2.5** **Lightmap baking em nuvem** para o perfil B/C (path tracing no servidor, entrega de atlas comprimido).
- **7.2.6** Reflexões híbridas: SSR → probes paralax-corrigidas → GI de sondas → RT.
- **7.2.7** GTAO com bent normals e upsample bilateral.

### Etapa 7.3 — Sombras avançadas *(5 semanas)*
- **7.3.1** **Virtual Shadow Maps**: atlas esparso de páginas, tabela de indireção, marcação de páginas visíveis.
- **7.3.2** Cache: páginas de geometria estática + luz estática nunca re-renderizam.
- **7.3.3** PCSS com penumbra por tamanho de fonte de luz.
- **7.3.4** Sombras de contato (ray march em screen space) para detalhe próximo.
- **7.3.5** Sombras de transparências e de vegetação (alpha-test otimizado).

### Etapa 7.4 — Volumétricos e atmosfera *(4 semanas)*
- **7.4.1** Froxel volumétrico (fog, luz volumétrica) em meia resolução com reprojeção temporal.
- **7.4.2** Atmosfera fisicamente baseada (Bruneton) com LUTs pré-computadas.
- **7.4.3** Nuvens volumétricas **[opcional para perfil S]**.
- **7.4.4** Ciclo dia/noite com atualização eficiente de GI e sombras.

### Etapa 7.5 — Anti-aliasing, upscaling e pós *(5 semanas)*
- **7.5.1** **TAA** robusto (rejeição por profundidade/normal/motion, clamp de variância em YCoCg, anti-ghosting).
- **7.5.2** **AetherSR**: reconstrução temporal com upscaling 50-70% → nativo.
- **7.5.3** Integração com GSR/MetalFX/FSR quando presentes.
- **7.5.4** **Resolução dinâmica** com histerese, guiada por tempo de GPU.
- **7.5.5** **VRS** dirigido por buffer de importância.
- **7.5.6** Pós avançado: DOF com bokeh separável, motion blur por objeto, aberração, film grain, HDR10 de saída.

### Etapa 7.6 — Escalabilidade e perfis *(4 semanas)*
- **7.6.1** Sistema de perfis gráficos (S/A/B/C) com detecção automática e override do usuário.
- **7.6.2** Base de dados de dispositivos com configurações conhecidas-boas.
- **7.6.3** Benchmark de calibração na primeira execução (5 s).
- **7.6.4** Integração completa com o `PowerGovernor`.
- **7.6.5** **Comparador visual** no editor: ver a cena nos 4 perfis lado a lado.
- **7.6.6** `ResolvedRenderingPolicy` global e versionada: budgets de cadência,
  visibilidade, LOD, sombras, GI/AO, pós, streaming, memória e trabalho assíncrono.
- **7.6.7** Percursos determinísticos de benchmark (incluindo exterior com vegetação),
  manifestos de APK/cena/câmera/GPU/driver e gates p50/p95/p99 por perfil.
- **7.6.8** Harness Android: AVD C sintético para regressão e Game Loop/laboratório
  físico para desempenho; resultado emulado nunca é publicado como FPS de aparelho.

> A infraestrutura inicial compartilhada com 2.1.9 foi integrada em 29/08/2026.
> O gate M7 continua exigindo hardware físico e os demais itens de escalabilidade.

> **✅ CRITÉRIO DE SAÍDA (M7 — BETA):** uma cena de demonstração (interior arquitetônico + exterior com vegetação) com GI dinâmica, sombras virtuais, MicroMesh e upscaling roda a **60 fps num aparelho classe A** com < 4.5 W. Testadores externos não conseguem distinguir capturas do Aether de capturas de uma engine de desktop com configuração média.

---

## FASE 8 — BUILD, PUBLICAÇÃO E SERVIÇOS
### Duração: 4 meses · Objetivo: **fechar o ciclo: da ideia à loja, sem PC**

### Etapa 8.1 — Aether Player *(5 semanas)*
- **8.1.1** App leve (< 60 MB) que carrega `.aetherpack`.
- **8.1.2** Geração do pacote no dispositivo, com stripping e compressão.
- **8.1.3** Compartilhamento por link/QR, com preview e instalação em um toque.
- **8.1.4** Sandbox de segurança para projetos de terceiros.
- **8.1.5** Feed de projetos da comunidade dentro do Player.

### Etapa 8.2 — Build farm em nuvem *(7 semanas)*
- **8.2.1** Infraestrutura de build (Android em Linux, iOS em macOS), com fila e cache.
- **8.2.2** Upload delta do projeto; build incremental.
- **8.2.3** AOT (NativeAOT/Mono-LLVM), IL trimming, empacotamento.
- **8.2.4** **Assinatura sem expor a chave**: keystore no hardware seguro do dispositivo, assinatura local do artefato.
- **8.2.5** Alvos: APK, AAB, IPA, Windows, macOS, Linux, **WebGPU**.
- **8.2.6** Logs de build legíveis e diagnóstico de falhas em português.

### Etapa 8.3 — Publicação assistida *(4 semanas)*
- **8.3.1** Checklist guiado (ícone, splash, descrição, classificação, política de privacidade).
- **8.3.2** Geração automática de assets de loja (ícones em todos os tamanhos, screenshots do jogo rodando, vídeo de preview).
- **8.3.3** Integração com Google Play Console e App Store Connect API.
- **8.3.4** Gestão de versões e canais (interno, teste fechado, aberto, produção).

### Etapa 8.4 — Serviços de jogo *(6 semanas)*
- **8.4.1** Contas, saves em nuvem, perfis.
- **8.4.2** Leaderboards, conquistas, estatísticas — com nós no Flow.
- **8.4.3** Analytics com funil, retenção e mapas de calor.
- **8.4.4** Remote config e A/B testing.
- **8.4.5** Monetização: anúncios (mediação), compras no app, assinaturas — com nós prontos e conformidade.
- **8.4.6** Relatórios de crash simbolizados.

### Etapa 8.5 — Multiplayer *(7 semanas)*
- **8.5.1** Transporte UDP confiável + fallback WebSocket.
- **8.5.2** Replicação de componentes com delta, quantização e interest management.
- **8.5.3** RPCs com autoridade, expostos ao Flow.
- **8.5.4** Predição de cliente + reconciliação; interpolação de entidades remotas.
- **8.5.5** Lag compensation no servidor.
- **8.5.6** Matchmaking, salas e relay.
- **8.5.7** Build de servidor dedicado headless.
- **8.5.8** Ferramentas: simulador de latência/perda, visualizador de tráfego, múltiplas instâncias locais para teste.

### Etapa 8.6 — Profiler e ferramentas de diagnóstico *(5 semanas)*
- **8.6.1** Profiler de CPU com timeline por thread e captura de jobs.
- **8.6.2** Profiler de GPU com tempos por pass do render graph.
- **8.6.3** Profiler de memória por categoria, com detecção de vazamento.
- **8.6.4** Monitor térmico e de energia com histórico.
- **8.6.5** **Assistente de otimização**: analisa a captura e sugere ações concretas em linguagem simples ("sua textura de fundo tem 4K mas ocupa 40 px na tela").
- **8.6.6** Captura de frame para RenderDoc/AGI.

> **✅ CRITÉRIO DE SAÍDA (M8):** um membro da equipe publica um jogo real na Google Play **usando apenas o celular**, do primeiro toque até a listagem aprovada. Um segundo jogo é compartilhado por link e jogado por 100 pessoas via Aether Player em 24 h.

---

## FASE 9 — COLABORAÇÃO, ECOSSISTEMA E LANÇAMENTO
### Duração: 5 meses · Objetivo: **transformar uma ferramenta num ecossistema**

### Etapa 9.1 — Versionamento e colaboração *(7 semanas)*
- **9.1.1** Git embutido com camada visual ("pontos de salvamento").
- **9.1.2** Diff visual de cena e de prefab; merge assistido objeto a objeto.
- **9.1.3** LFS automático; políticas de armazenamento.
- **9.1.4** **Co-edição CRDT** em tempo real com presença, cursores e bloqueio suave.
- **9.1.5** Comentários espaciais e chat de voz.
- **9.1.6** **Modo Sala de Aula** (professor/alunos, distribuição e coleta de projetos).

### Etapa 9.2 — Asset Store e comunidade *(6 semanas)*
- **9.2.1** Loja integrada com preview interativo dentro do editor.
- **9.2.2** Publicação de assets, sub-grafos, templates e plugins por usuários; repartição de receita.
- **9.2.3** Curadoria, avaliações, moderação e verificação de segurança de código.
- **9.2.4** Pacotes iniciais gratuitos de alta qualidade (fundamentais para o primeiro dia).
- **9.2.5** **Remix**: fork de qualquer projeto público com um toque, com atribuição.

### Etapa 9.3 — Extensibilidade *(5 semanas)*
- **9.3.1** SDK de plugins em C# (ferramentas de editor, importadores, nós, inspetores customizados).
- **9.3.2** API de automação/scripting do editor.
- **9.3.3** Sandbox e permissões de plugin.
- **9.3.4** Documentação de arquitetura interna e política de código aberto (definir: núcleo aberto? licença?).

### Etapa 9.4 — XR **[opcional/paralelo]** *(6 semanas)*
- **9.4.1** AR: ARCore/ARKit, âncoras, oclusão por profundidade, iluminação estimada.
- **9.4.2** VR mobile (Quest via Android): estéreo, foveated rendering, controles.
- **9.4.3** Modo de **edição em AR**: posicionar objetos da cena no espaço real.

### Etapa 9.5 — Documentação e aprendizado *(contínuo, intensifica aqui)*
- **9.5.1** Documentação completa da API, gerada e revisada, em PT-BR e EN.
- **9.5.2** Tutoriais interativos dentro do app (20+ trilhas).
- **9.5.3** Cursos em vídeo e projetos de exemplo comentados.
- **9.5.4** Templates de jogos completos e jogáveis (10+ gêneros).
- **9.5.5** Programa de embaixadores e conteúdo da comunidade.

### Etapa 9.6 — Endurecimento e lançamento *(8 semanas)*
- **9.6.1** Beta aberta ampla; triagem sistemática de bugs por severidade.
- **9.6.2** Compatibilidade: matriz de 100+ dispositivos, com correções específicas de driver.
- **9.6.3** Otimização final guiada por telemetria real de uso.
- **9.6.4** Auditoria de segurança e privacidade (LGPD/GDPR, dados de menores — o público P1/P5 exige rigor).
- **9.6.5** Localização (PT-BR, EN, ES, ZH, HI, ID, RU, JA).
- **9.6.6** Infraestrutura de suporte e SLA.
- **9.6.7** Lançamento 1.0 com campanha, showcase de jogos criados na beta e programa de criadores.

> **✅ CRITÉRIO DE SAÍDA (1.0):** 10.000 usuários ativos mensais na beta, 500 jogos publicados por usuários, crash-free rate > 99.5%, SUS ≥ 80, e pelo menos **3 jogos comerciais** feitos com o Aether nas lojas.

---

## 16.10 Trilhas contínuas (correm durante todas as fases)

| Trilha | Atividade permanente |
|---|---|
| **Desempenho** | Benchmarks automatizados em dispositivos reais a cada commit; orçamentos como testes que **quebram o build** quando estourados; sessão semanal de profiling |
| **Testes** | Unitários (núcleo), integração (subsistemas), **testes de imagem** (renderizador, com comparação perceptual), testes de UI automatizados em dispositivo, fuzzing de importadores e de serialização |
| **Compatibilidade** | Farm de dispositivos rodando a suíte todas as noites; base de dados de quirks de driver |
| **Documentação** | Toda API pública documentada no mesmo PR que a introduz. Sem exceção |
| **Dogfooding** | Game jam interna a cada 6 semanas, **obrigatoriamente feita no celular**. Os bugs encontrados aqui têm prioridade máxima |
| **Comunidade** | Devlog público quinzenal desde a Fase 2; Discord desde a Fase 3; alpha fechada na Fase 5 |
| **Acessibilidade** | Auditoria a cada fase; testes com usuários de tecnologias assistivas a partir da Fase 3 |

## 16.11 Grafo de dependências entre fases

```
F0 ──► F1 ──┬──► F2 ──┬──► F3 ──┬──► F4 ──┬──► F5 ──┬──► F8 ──► F9
            │         │         │         │         │
            │         └─────────┴────────►F7 (precisa de F2+F3)
            │                             ▲
            └────────────────► F6 ─────────┘ (F6 precisa de F1+F3;
                                              alimenta F7 com meshlets)

Paralelizável de verdade:
  · F4 (simulação) e F5 (no-code) podem correr juntas com times separados
  · F6 (criação de conteúdo) pode começar assim que F3 entregar o framework de UI
  · F7 (gráficos avançados) pode começar assim que F2 entregar o render graph
  · Serviços de nuvem (F8) podem ser construídos desde a F2, em paralelo total
```

---

# PARTE 17 — RISCOS E MITIGAÇÕES

## 17.1 Riscos técnicos

| # | Risco | Prob. | Impacto | Mitigação |
|---|---|---|---|---|
| RT-1 | **Overhead de interop C#↔Vulkan mata o desempenho** | Média | Crítico | PoC-A na Fase 0; regra de lote obrigatório; orçamento de 200 chamadas/frame testado no CI; plano B: mover mais do renderizador para C++ |
| RT-2 | **Throttle térmico torna o editor inutilizável** | **Alta** | Crítico | `PowerGovernor` como sistema de primeira classe desde a Fase 1; teste de 30 min no CI; viewport com render sob demanda quando ocioso |
| RT-3 | **Fragmentação de drivers Vulkan no Android** | **Alta** | Alto | Piso em Android Baseline Profile; base de dados de quirks; farm com 100+ aparelhos; caminho de fallback para cada extensão opcional; camada de validação em produção (opt-in) |
| RT-4 | **GC do .NET causa stutter** | Média | Alto | Regra de zero alocação no frame loop com analisador que quebra o build; pools e arenas; `SustainedLowLatency`; teste de "0 Gen0 em 60 s de gameplay" |
| RT-5 | **Restrições da App Store sobre execução de código** | Média | **Fatal para iOS** | Interpretador de IL (permitido pelas regras 2.5.2 para código dentro do app); AetherFlow é *dado interpretado*, não código baixado; consulta jurídica na Fase 0; lançar Android primeiro |
| RT-6 | **MicroMesh não cabe no orçamento de compute mobile** | Média | Médio | Escopo reduzido (sem software raster, sem streaming de páginas na v1); fallback para LOD discreto é sempre funcional |
| RT-7 | **GI dinâmica custa caro demais** | Média | Médio | Três níveis (assado/sondas/traçado) — o produto funciona bem no nível 1 |
| RT-8 | **Compilação de shaders causa engasgo** | Alta | Médio | Pipeline cache pré-aquecido e empacotado; compilação assíncrona com material temporário; compilação de variantes em nuvem |
| RT-9 | **Round-trip Flow⇄C# perde informação** | Média | Alto | AST como fonte única; testes property-based; "caixa de código" como escape hatch garantido |
| RT-10 | **Memória insuficiente em aparelhos B/C** | Média | Médio | Orçamentos explícitos testados; streaming agressivo; perfil C limitado a projetos 2D/pequenos, comunicado com honestidade |
| RT-11 | **Complexidade do projeto excede a capacidade de execução** | **Alta** | Crítico | Fases com critérios de saída demonstráveis; nada avança sem M anterior verde; itens **[opcional]** cortáveis sem quebrar o produto |

## 17.2 Riscos de produto e mercado

| # | Risco | Mitigação |
|---|---|---|
| RP-1 | **"Ninguém quer fazer jogos no celular"** — a hipótese central pode estar errada | Validar na Fase 0 com pesquisa real; alpha pública na Fase 5 (mês ~20) é o teste de mercado antes do maior investimento |
| RP-2 | **Unity/Godot lançam um editor mobile** | A vantagem é a arquitetura touch-first; um port de desktop não compete. Manter o ritmo e construir comunidade cedo |
| RP-3 | **Ecossistema vazio no lançamento** | Asset store com pacotes gratuitos de qualidade; templates completos; programa de criadores pago durante a beta |
| RP-4 | **Curva de aprendizado ainda alta** | Os três níveis de UI e as três representações de lógica; tutoriais interativos; métrica de "tempo até o primeiro jogo publicado" como KPI de produto |
| RP-5 | **Monetização insuficiente** | Modelo freemium com nuvem paga (build farm, colaboração, baking) — o custo marginal recai sobre quem usa |
| RP-6 | **Moderação e segurança de conteúdo** (público jovem) | Sandbox de execução, revisão de conteúdo publicado, controles parentais, conformidade com COPPA/LGPD desde o design |

## 17.3 Os três pontos de decisão "vai/não vai"

| Momento | Pergunta | Se a resposta for não |
|---|---|---|
| **Fim da Fase 0** | Vulkan + .NET + térmica funcionam juntos? | Mudar fronteira de linguagem ou perfil de hardware. Não avançar às cegas |
| **Fim da Fase 3 (M3)** | Editar com o dedo é bom o bastante (SUS ≥ 72)? | **Parar e reprojetar a UX.** Este é o risco existencial do produto — não é aceitável "seguir e melhorar depois" |
| **Fim da Fase 5 (M5)** | A alpha pública mostra retenção e criação real? | Reavaliar posicionamento antes de gastar as Fases 6-9 |

---

# PARTE 18 — MÉTRICAS E CRITÉRIOS DE QUALIDADE

## 18.1 KPIs técnicos (testados automaticamente no CI, em dispositivos reais)

| Métrica | Alvo | Falha o build se |
|---|---|---|
| Frame time do editor (cena de referência, classe A) | ≤ 16.6 ms | > 20 ms |
| Frame time do runtime (demo de referência, classe A) | ≤ 16.6 ms | > 16.6 ms |
| Alocações de GC durante 60 s de gameplay | 0 Gen0 | > 0 |
| Chamadas nativas por frame | ≤ 200 | > 300 |
| Memória do editor (projeto de referência) | ≤ 1.5 GB | > 2.0 GB |
| Potência sustentada (30 min de edição) | ≤ 4 W | > 5 W |
| Tempo até primeira interação (cold start) | ≤ 3 s | > 4 s |
| Hot reload de script | ≤ 1.5 s | > 3 s |
| Latência toque→pixel | ≤ 30 ms | > 45 ms |
| Tamanho do runtime base (jogo vazio) | ≤ 12 MB | > 18 MB |
| Cobertura de testes do núcleo | ≥ 80% | < 70% |
| Testes de imagem do renderizador | 0 regressões | qualquer regressão não aprovada |

## 18.2 KPIs de produto

| Métrica | Alvo no 1.0 |
|---|---|
| **Tempo até o primeiro jogo jogável** (usuário novo, P1) | < 45 min |
| **Tempo até a primeira publicação** | < 3 dias |
| Retenção D1 / D7 / D30 | 45% / 22% / 12% |
| SUS (System Usability Scale) | ≥ 80 |
| Crash-free sessions | > 99.5% |
| Projetos criados por usuário ativo/mês | ≥ 2 |
| Taxa de conclusão do tutorial | > 70% |
| NPS | ≥ 40 |

## 18.3 Barra de qualidade não-negociável

Cinco coisas que **nunca** podem ser sacrificadas por prazo:

1. **Nunca perder trabalho do usuário** (WAL + salvamento contínuo).
2. **Nunca travar o thread principal** por mais de 100 ms (tudo pesado vai para job).
3. **Nunca crashar o editor por culpa do jogo do usuário** (isolamento de processo).
4. **Nunca mostrar um erro que o usuário não entenda** (mensagens em linguagem simples, com ação sugerida).
5. **Nunca esquentar o aparelho a ponto de desconfortar** (governador térmico com limite rígido).

---

# PARTE 19 — ESTRUTURA DE REPOSITÓRIO E PROCESSO DE ENGENHARIA

## 19.1 Monorepo

```
aether/
├── native/                       # C++20
│   ├── core/                     # alocadores, jobs, plataforma
│   ├── rhi/                      # Vulkan
│   ├── shaderc/                  # compilação de shaders
│   ├── physics/                  # integração Jolt
│   ├── audio/                    # mixer, DSP
│   ├── geometry/                 # booleanas, remesh, unwrap, meshlets
│   ├── codecs/                   # ASTC, imagem, áudio, vídeo
│   └── platform/{android,ios,desktop}/
│
├── managed/                      # C# (.NET 10)
│   ├── Aether.Core/              # ECS, math, serialização, recursos
│   ├── Aether.Rendering/         # render graph, cull, batching
│   ├── Aether.Scene/             # nós, prefabs, hierarquia
│   ├── Aether.Physics/  .Animation/  .Audio/  .UI/  .Net/
│   ├── Aether.Assets/            # importadores, pipeline
│   ├── Aether.Scripting/         # Roslyn, hot reload, sandbox
│   ├── Aether.Flow/              # AST, compilador, VM, round-trip
│   ├── Aether.Editor.*/          # shell, viewport, inspector, modeling...
│   └── Aether.Runtime/           # runtime puro para o jogo publicado
│
├── shaders/                      # Slang/HLSL + biblioteca comum
├── interop/                      # IDL .aidl + gerador dos dois lados
├── tools/                        # build farm, gerador de docs, benchmarks
├── samples/                      # projetos de exemplo e templates
├── tests/                        # unit, integração, imagem, device
├── docs/                         # ADRs, arquitetura, API
└── ci/                           # pipelines, farm de dispositivos
```

## 19.2 Processo

| Prática | Detalhe |
|---|---|
| **Trunk-based** com feature flags | Nada de branches longos; recursos incompletos ficam atrás de flag |
| **ADRs obrigatórios** | Toda decisão arquitetural registrada com contexto, alternativas e consequências |
| **Orçamentos como testes** | Desempenho e memória não são "a gente otimiza depois" — são testes que quebram o build |
| **Farm de dispositivos no CI** | Cada PR roda a suíte em 6 aparelhos; o build noturno roda em 40+ |
| **Testes de imagem** | Renderizador validado por comparação perceptual contra referências aprovadas |
| **Dogfooding obrigatório** | Game jam interna a cada 6 semanas, feita no celular |
| **Devlog público** | Transparência constrói a comunidade antes do produto existir |
| **Revisão de UX** | Toda mudança de interface passa por revisão de design e teste com 3 usuários |

---

# PARTE 20 — MODELO DE NEGÓCIO E ECOSSISTEMA

## 20.1 Monetização

| Camada | Preço | Conteúdo |
|---|---|---|
| **Gratuito** | R$ 0 | Editor completo, todas as features de criação, publicação via Aether Player, 3 builds em nuvem/mês, 2 GB de nuvem. Splash "Feito com Aether" |
| **Criador** | ~R$ 30/mês | Builds ilimitados, sem splash, 50 GB, baking em nuvem, colaboração até 3 pessoas |
| **Estúdio** | ~R$ 120/usuário/mês | Colaboração ilimitada, servidores dedicados, analytics avançado, suporte prioritário, source access |
| **Educação** | Gratuito | Modo Sala de Aula, licenças de turma, currículo pronto |
| **Asset Store** | 70/30 | Repartição favorável ao criador (contra os 88/12 da Unity, mas com audiência mobile enorme) |

**Princípio:** cobrar por **custo marginal de nuvem**, nunca por features de criação. Um adolescente com um celular emprestado precisa conseguir fazer e publicar um jogo por R$ 0.

## 20.2 Estratégia de ecossistema

1. **Devlog público desde a Fase 2** — a comunidade acompanha a construção e se apega ao projeto.
2. **Alpha pública na Fase 5** (~mês 20) com programa de criadores pago para gerar conteúdo de referência.
3. **Aether Player como vetor viral** — cada jogo compartilhado por link é um anúncio jogável.
4. **Núcleo aberto** (a decidir na Fase 9): abrir o runtime e o núcleo sob licença permissiva, mantendo os serviços de nuvem como produto. Constrói confiança e evita o "medo do Runtime Fee" que abalou a Unity.
5. **Foco geográfico inicial**: Brasil, Índia, Indonésia, Nigéria, Filipinas — mercados onde o celular **é** o computador. Esta é a razão de ser do produto e onde a competição é zero.

---

# APÊNDICE A — REGISTRO DE DECISÕES ARQUITETURAIS (ADRs)

| # | Decisão | Alternativas descartadas | Razão |
|---|---|---|---|
| ADR-01 | C# para engine/editor, C++ para núcleo | 100% C++; 100% C#; Rust | Reflexão e hot reload são exigências do editor; C++ só onde o profiler exige |
| ADR-02 | ECS por arquétipos com fachada de nós | Só grafo de objetos; só ECS puro | Desempenho de cache no ARM + modelo mental acessível |
| ADR-03 | Vulkan 1.3 como único backend | OpenGL ES; WebGPU; Metal nativo | Bindless e controle de memória são pré-requisitos do pipeline moderno; MoltenVK cobre iOS |
| ADR-04 | Render graph com aliasing e fusão de subpasses | Pipeline fixo codificado | Único jeito de respeitar o TBDR sem código espalhado |
| ADR-05 | Jolt Physics em vez de solver próprio | Escrever do zero; PhysX; Bullet | 2-3 anos-pessoa economizados; não é diferencial competitivo |
| ADR-06 | AST única com três representações | Grafo e código separados | Elimina o teto do no-code — é a inovação central do produto |
| ADR-07 | Formato dual texto+binário | Só binário; só texto | Diff/merge viável e carga rápida ao mesmo tempo |
| ADR-08 | Processo separado para o Play | Mesmo processo com try/catch | Crash do usuário não pode derrubar o editor no mobile |
| ADR-09 | Build nativo em nuvem, dados no dispositivo | Toolchain completa no celular | Impossível hoje; o modo Player cobre 95% das necessidades |
| ADR-10 | Um único pipeline de renderização escalável | URP/HDRP separados | O maior erro estratégico da Unity; fragmenta assets e conhecimento |
| ADR-11 | Menu radial como mecanismo primário de comando | Barras de ferramentas; menus hierárquicos | Memória muscular direcional é o substituto touch dos atalhos de teclado |
| ADR-12 | WAL de edição com recuperação total | Salvamento manual/autosave periódico | O SO mata processos mobile a qualquer momento |
| ADR-013 | Backend 2D decidido por benchmark físico | Escolher Jolt restrito ou Box2D só por arquitetura/host | CPU, memória e qualidade equivalente precisam ser medidas em perfis B/C |
| ADR-014 | Política global de renderização orientada por budgets | Tuning por cena/modelo; capabilities como velocidade; AVD como certificação | Uma fonte de verdade escala qualidade/desempenho sem fragmentar renderer e projetos |

---

# APÊNDICE B — GLOSSÁRIO

| Termo | Significado |
|---|---|
| **TBDR** | Tile-Based Deferred Rendering — arquitetura das GPUs móveis; renderiza a tela em blocos pequenos que cabem em memória rápida no chip |
| **Bindless** | Técnica em que shaders acessam recursos por índice num array global, sem descriptor set por objeto |
| **Meshlet** | Grupo de 64-128 triângulos tratado como unidade de cull e desenho |
| **DDGI** | Dynamic Diffuse Global Illumination — GI por grade de sondas atualizadas em tempo real |
| **SDF** | Signed Distance Field — volume que armazena a distância até a superfície mais próxima; útil para traçar raios barato |
| **HZB** | Hierarchical Z-Buffer — pirâmide de profundidade usada para occlusion culling |
| **VRS** | Variable Rate Shading — reduzir a taxa de sombreamento em regiões pouco importantes |
| **ASTC** | Formato de compressão de textura com bloco de tamanho variável, padrão em mobile |
| **CRDT** | Estrutura de dados que permite edição concorrente sem conflitos |
| **WAL** | Write-Ahead Log — registro de operações antes de aplicá-las, para recuperação |
| **AOT / JIT** | Compilação antecipada / em tempo de execução |
| **SUS** | System Usability Scale — questionário padronizado de usabilidade (0-100) |
| **Round-trip** | Converter A→B→A sem perda de informação |

---

# APÊNDICE C — RESUMO DO ESFORÇO POR FASE

| Fase | Duração | Trilhas ativas | Foco de contratação |
|---|---|---|---|
| F0 — Fundação | 2-3 meses | Plataforma, PoCs, UX | Engenheiro de plataforma, gráfico sênior, designer de UX |
| F1 — Núcleo | 4 meses | Core, ECS, ferramentas | Engenheiros de sistemas, C# de alto desempenho |
| F2 — Renderizador | 5 meses | Gráfico | Engenheiros gráficos (Vulkan/mobile) |
| F3 — Editor | 6 meses | UI/UX, ferramentas | Engenheiros de UI, designers de interação, pesquisa de usuário |
| F4 — Simulação | 4 meses | Física, animação, áudio | Especialistas por domínio |
| F5 — No-code + C# | 5 meses | Linguagens, ferramentas | Engenheiros de compiladores, ferramentas de desenvolvedor |
| F6 — Criação | 6 meses | DCC, assets | Engenheiros de geometria, artistas técnicos |
| F7 — Gráficos avançados | 5 meses | Gráfico | Engenheiros gráficos sênior, pesquisa |
| F8 — Build/Serviços | 4 meses | Backend, DevOps | Engenheiros de nuvem, netcode |
| F9 — Ecossistema | 5 meses | Tudo + comunidade | Suporte, conteúdo, comunidade, QA |
| **Total** | **~46 meses** (com paralelismo real: **30-36 meses**) | | |

---

## PRÓXIMOS PASSOS IMEDIATOS (as primeiras 4 semanas)

| Semana | Ação |
|---|---|
| **1** | Montar o monorepo e o CI; adquirir os primeiros 6 aparelhos de teste (um por perfil, GPUs diferentes) |
| **1-2** | Shell nativo Android com surface Vulkan + ciclo de vida correto; "olá, triângulo" |
| **2-3** | Carregar .NET no processo; provar a fronteira C#↔C++ com o gerador de IDL |
| **3-4** | **PoC-B (gestos)** em paralelo, num protótipo separado — porque se essa não funcionar, nada mais importa |
| **4** | Iniciar as entrevistas de UX com as personas P1/P2/P3 |

---

*Documento vivo. Atualizar a cada marco alcançado, com os aprendizados que mudarem as premissas.*
