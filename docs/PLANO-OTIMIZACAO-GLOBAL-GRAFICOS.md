# Plano global de desempenho gráfico, iluminação e estabilidade visual

> **Estado inicial em 28/08/2026:** plano criado a partir da inspeção do APK em
> hardware físico e alinhado aos itens 2.1–2.5, 7.1–7.6 e aos KPIs da Parte 18 de
> `PLANO-ENGINE-MOBILE.md`. Este documento ordena trabalho já previsto no roadmap
> para atacar o frame completo; não cria um renderer paralelo nem declara M2/M7
> concluídos antecipadamente.

> **Baseline real, mesma câmera escolhida pelo usuário:** 28,68 FPS antes da
> ordenação; **48,42 FPS** depois de opacos sólidos antes de alpha-mask, com diferença
> média de imagem de 0,005. Um experimento de subdivisão em 396 draws caiu para
> **25,64 FPS** e foi removido. Isso fixa a ordem arquitetural: cena GPU persistente e
> indirect/compactação antes de granularidade espacial fina.

> **Cadência medida em 29/08/2026:** o painel e o Choreographer entregam 120 Hz,
> mas esta carga apresentou 30,86 FPS ao votar 120. Com Surface/pacer em 60 Hz,
> a segunda janela aquecida apresentou **54,61 FPS**, CPU média 2,64 ms e espera
> de acquire média 11,97 ms. O padrão permanece 60 até timestamp GPU + múltiplos
> frames em voo demonstrarem ganho; 90/120 continuam metas configuráveis, não
> números declarados sem sustentação.

## 1. Objetivo e regra de qualidade

Elevar o desempenho de cenas reais para **60 FPS sustentados como piso**, com 90 e
120 Hz como alvos de primeira classe quando o painel permitir, enquanto melhora —
ou no mínimo preserva — a imagem. O ganho deve vir de eliminar trabalho invisível, redundante ou incorreto,
de melhor organização do frame e de algoritmos mais eficientes. Reduzir resolução,
distância, sombras, texturas, iluminação ou geometria sem equivalência visual não
conta como otimização deste plano.

Regras obrigatórias:

1. Toda mudança compara a mesma câmera, cena, resolução de saída, conteúdo e estado
   térmico antes/depois.
2. Nenhum ganho é aceito se testes de imagem mostrarem perda não aprovada.
3. Correção visual vem antes da otimização do caminho incorreto.
4. Sistemas são globais e dirigidos por dados de cena/material; não entram hacks
   por nome de asset, mapa, fabricante ou aparelho.
5. Perfis S/A/B/C selecionam algoritmos e budgets globais. O perfil A permanece a
   referência visual e de 60 FPS; degradação térmica é fallback explícito, não o
   mecanismo principal de ganho.
6. CPU, GPU, compositor, memória, energia e estabilidade de frame são medidos
   separadamente. FPS médio sozinho não fecha nenhum gate.

## 2. Evidência observada no aparelho

Inspeção no Xiaomi `25053PC47G`, Android 16, resolução 2772×1280:

| Evidência | Resultado |
|---|---|
| Cena visual inspecionada | estrada/floresta com 341.109 triângulos, 27 draws, 26 materiais e 70 texturas |
| Carga da cena | 3.847 ms até `ready`; primeiro frame após ativação em ~4.342 ms |
| Memória reportada | ~150,7 MB em texturas, ~34,7 MB em buffers e ~14,3 MB em render targets |
| Ambiente atual | HDRI 4096×2048 RGBA16F, ~89,5 MB, usado simultaneamente como céu visível e fonte de reflexão |
| Defeito visual reproduzido | grande conjunto de árvores/folhagem opaco e esbranquiçado à direita da imagem |
| Visibilidade atual | todos os 27 draws são enviados; não há frustum culling, occlusion culling/HZB ou LOD |
| Perfil de 30 s | ~59,90 FPS exibidos, mas o runner ativou `poc-a-5000-textured-cubes`, não a cena de floresta |

O resultado de ~59,90 FPS não invalida os ~45 FPS percebidos: hoje o perfilador
troca a carga e rotula a captura como PoC-A. Antes de otimizar, a cena real precisa
ser perfilável sem mudar de modo.

### 2.1 Causas já demonstráveis no código

- O importador distingue somente `OPAQUE` e `BLEND`. `alphaMode=MASK` é ignorado,
  embora `alphaCutoff` seja serializado. O shader só descarta alpha quase zero para
  materiais marcados como blend. Folhagem `MASK` acaba desenhando o fundo claro do
  atlas como superfície opaca — causa primária do branco observado.
- `doubleSided` também não é importado. O pipeline inteiro usa `CULL_MODE_NONE`,
  escondendo o erro de autoria às custas de shading duplicado em toda a cena.
- Transparência é separada apenas por um bit e ordenada por primitiva. Vegetação
  recortada não deve entrar no caminho blend; transparências reais precisam de
  contrato e ordenação estáveis.
- A iluminação difusa é um termo ambiente constante, sem sombra, AO ou visibilidade.
  Regiões atrás de árvores recebem luz ambiente sem oclusão, acentuando a aparência
  lavada. Exposição e tone mapping são fixos.
- O céu visível faz uma amostragem por pixel do HDRI 4K e compartilha o mesmo recurso
  pesado usado pelo IBL. Céu, irradiância difusa e reflexão especular não têm
  representações independentes.
- O renderer recria pipelines/render passes diretamente apesar da infraestrutura de
  `PipelineCache` já conectada ao `VulkanDevice`.
- O frame ainda não executa o Render Graph real na GPU e usa um command buffer único.

## 3. Orçamentos e gates

### 3.1 Gate visual

- Zero folhagem branca por erro de alpha/material.
- Zero objeto desaparecendo por bounds, culling, LOD, ordenação ou precisão.
- Vegetação mantém silhueta, densidade, normal mapping e sombras.
- Céu azul, nuvens e disco solar coerentes com a luz direcional.
- Interior/áreas sob copa preservam contraste sem preto esmagado nem branco lavado.
- Comparação automatizada por câmera usa SSIM/FLIP e máscara de pixels instáveis;
  qualquer diferença fora do orçamento exige aprovação visual.

### 3.2 Gates globais de cadência

| Alvo | Intervalo | CPU p95 | GPU p95 | Falha de sustentação |
|---:|---:|---:|---:|---:|
| 120 Hz | 8,33 ms | ≤ 6,50 ms | ≤ 7,33 ms | p95 > 8,33 ms |
| 90 Hz | 11,11 ms | ≤ 8,67 ms | ≤ 9,78 ms | p95 > 11,11 ms |
| 60 Hz | 16,67 ms | ≤ 13,00 ms | ≤ 14,67 ms | p95 > 16,67 ms |

Render e fixed tick são independentes: padrão de simulação 60 Hz, apresentação em
60/90/120 Hz e interpolação entre snapshots. Projetos podem selecionar 30/60/120 de
simulação por necessidade de gameplay; a tela nunca define a velocidade do jogo.

### 3.3 Gate sustentado do perfil A

| Métrica | Alvo | Falha |
|---|---:|---:|
| FPS exibido sustentado | ≥ 60 | < 60 |
| Frame GPU p50 / p95 / p99 | ≤ 13 / 15,5 / 16,6 ms | p99 > 16,6 ms |
| CPU de submissão p95 | ≤ 3 ms | > 4 ms |
| Stutter | nenhum frame > 33,3 ms em 10 min | qualquer ocorrência não explicada |
| Potência em 30 min | ≤ 4 W | > 5 W |
| Estado térmico | sem throttle sustentado | queda de clock/FPS não recuperada |
| Alocação no loop estável | 0 | qualquer GC induzido pelo renderer |
| Chamadas C#↔nativo | ≤ 200/frame | > 300/frame |

O frame mantém folga de GPU para UI, simulação e variação de driver. “16,6 ms exatos”
na cena vazia não é uma margem aceitável.

## 4. Ordem global de execução

Esta ordem é uma dependência técnica, não uma lista de ideias: **frame policy e
instrumentação → fixed tick consumido pelo loop → change tracking de chunks ECS →
cena GPU persistente → culling/LOD/compactação GPU → indirect → Render Graph mobile →
shader/bandwidth → governor térmico**. Pular para subdivisão antes de indirect repete
o caso medido de 396 draws e aumenta CPU em vez de reduzi-la.

**Estado do ciclo atual:** política portátil 60/90/120 implementada; Surface Android
recebe voto explícito; Choreographer filtra callbacks pela cadência do projeto; taxa
VSYNC observada é registrada; alpha-mask, céu procedural, iluminação hemisférica e
ordenação solid-first estão ativos. Timestamps GPU por pass e o prepass seletivo de
cobertura estão ativos. Próximo gate obrigatório: reduzir shader/bandwidth e executar
captura AGI; somente depois reavaliar 2–3 frames em voo sem compartilhar
depth/command/instance buffers.

**Diagnóstico GPU de 29/08/2026:** timestamps Vulkan reais mediram 22,41–22,59 ms
de GPU média no hotspot, contra 4,05–5,07 ms de CPU de processo. A espera de acquire
de 23,12–23,38 ms é consequência dessa carga GPU, não prova de falta de frames em voo.
Logo, múltiplos frames em voo fica depois da redução de GPU para evitar apenas aumentar
latência. Próxima ordem: separar timestamps por pass → depth/coverage prepass A/B para
alpha-mask → reduzir overdraw/shading oculto → compactar IBL/bandwidth → reavaliar fila.

**Ciclo GPU concluído em 29/08/2026:** o FrameProfile v3 passou a registrar
`gpu_geometry_ms`, `gpu_background_ms` e `gpu_transparent_ms`. Em GPU móvel TBDR,
checkpoints dentro do mesmo render pass podem ser resolvidos no fim do tile e não
devem ser interpretados como uma captura AGI; ainda assim, o A/B isolou o custo de
cobertura. O prepass global de materiais `MASK` (alpha+depth, seguido de PBR com
depth `EQUAL`) reduziu a GPU fresca de **17,12 para 12,71–12,78 ms** e sustentou
**60,05–60,07 FPS** na câmera do hotspot, mesma saída 2772×1280. Já aquecido, o
A/B no mesmo APK reduziu 28,94→22,68 ms (−21,6%) e elevou 30,24→35,99 FPS (+19%).
A comparação visual manteve chão, árvores, recortes e iluminação; 99,966% dos
pixels foram idênticos. O próximo custo global é shader/normal transform e
bandwidth de materiais/IBL, antes de reavaliar frames em voo.

### O0 — Instrumentação da cena real e verdade de frame

**Alinha:** 2.1.6, 7.6.2–7.6.4 e Parte 18.

1. Fazer o runner receber `sceneId` explícito e registrar no relatório a identidade,
   hash e contagens do pacote realmente renderizado. Perfil nunca pode ativar PoC-A
   implicitamente.
2. Criar percurso determinístico de câmera para exterior com vegetação, estrada,
   céu, close de material e vista com alta oclusão.
3. Adicionar timestamp queries por pass e correlação CPU/GPU/SurfaceFlinger.
4. Registrar por frame: draws submetidos/visíveis, triângulos, mudanças de PSO,
   descriptors, pixels/tiles estimados, uploads, memória, resolução e motivo de
   fallback.
5. Capturar frame AGI/RenderDoc e estabelecer mapa de custos por pass, bandwidth,
   overdraw e stalls.
6. Rodar baseline debug e release por 60 s, depois soak de 30 min no mesmo percurso.

**Aceite:** o relatório identifica a floresta, reproduz a faixa percebida e permite
atribuir cada milissegundo a CPU, GPU, espera do compositor ou thermal governor.

### O1 — Correção global de materiais e visibilidade

**Alinha:** 2.2.2, 2.2.5, 2.4.2, 2.4.5, 7.3.5.

1. Tornar `AlphaMode` enum estável (`Opaque`, `Mask`, `Blend`) no formato de material;
   preservar `alphaCutoff`, `doubleSided` e alpha do `baseColorFactor` no import.
2. Criar variantes/pipelines globais por estado de rasterização, não por asset:
   opaque single-sided, opaque double-sided, alpha-mask single/double-sided e blend.
3. Executar alpha-mask no caminho depth-writing/opaque, com `discard` pelo cutoff,
   alpha-to-coverage quando MSAA existir e preservação de cobertura nos mipmaps.
4. Usar backface culling nos materiais single-sided. Double-sided corrige a normal
   na face traseira e só paga custo onde declarado.
5. Separar blend real, ordenar de modo estável e documentar premultiplied versus
   straight alpha. Não usar blending para folhas recortadas.
6. Validar tangentes, normal map, espaço de cor, canais MR e UV por material.
7. Criar corpus de regressão com pinheiro, folhas finas, cerca, vidro, emissivo e
   materiais vistos pelos dois lados.
8. Investigar “objetos que somem” com IDs de draw, bounds e captura de câmera. Nenhum
   culling novo entra antes de provar que os bounds transformados são conservadores.

**Aceite:** a cena não possui áreas brancas; os mesmos materiais funcionam em qualquer
mapa; backface culling reduz shading sem remover geometria válida.

### O2 — Céu azul, nuvens, sol e iluminação coerente

**Alinha:** 2.4.3, 2.4.4, 2.4.6, 7.2, 7.3 e 7.4.

**Progresso em 29/08/2026 (itens 1, 5-difusa e 8-tonemap):**
`tools/cook-procedural-sky.py` bakea a especular (mip chain 128×64, antes
4096×2048/~89,5 MB) e a irradiância difusa (SH9) da mesma função analítica
que a cúpula visível usa — item 1 fechado (as três fontes agora são
literalmente a mesma função, não só "coexistem sem se destruir"). Item 5
parcial: SH de irradiância substitui o ambiente hemisférico anterior;
probes de reflexão múltiplas e BRDF multiscatter continuam pendentes. Item 8
parcial: tonemap trocado de ACES-fit ingênuo para aproximação AgX
(`native/rhi/shaders/tonemap.glsl`, compartilhado pela cúpula e pelo
shading); exposição continua um escalar estático em `EnvironmentLighting`,
sem histograma temporal nem white balance. Item 3 parcial com limitação
nova: `cook-hdri-environment.py` (HDRI autoral fotográfica) ainda escreve o
formato AEEN **v1** (80 bytes); `decodeEnvironment` agora exige v2 (224
bytes, com SH9) sem fallback, então esse tool precisa de uma atualização
equivalente (SH projetada do panorama real) antes de voltar a ser usável —
não é um caminho ativo hoje, apenas registrado para não surpreender quem
tentar cozinhar uma HDRI autoral depois. Itens 6 (CSM), 7 (GTAO/DDGI) e a
parte de nuvens/disco solar procedurais do item 2/9 permanecem no escopo
original, não tocados por esta fatia. Confirmação em hardware físico
pendente (sem aparelho ADB conectado nesta sessão); ver `docs/ESTADO.md`
§"Ambiente/IBL global — 29/08/2026".

1. Separar três conceitos: **céu visível**, **irradiância difusa** e **reflexão
   especular**. Trocar a fotografia visível não pode destruir o IBL dos materiais.
2. Substituir o HDRI visível por céu procedural analítico com gradiente atmosférico,
   horizonte, disco solar com limb/soft edge e camada de nuvens 2D/3D barata, animada
   em coordenadas de mundo e temporalmente estável.
3. Manter opção global de HDRI autoral, mas usar representação própria e adequada ao
   céu. O padrão do sample passa a ser azul/nuvens/sol.
4. Derivar luz solar, cor do céu, exposição e IBL de um único `EnvironmentState`.
   Alterar hora/sol atualiza todos os consumidores de forma coerente.
5. Substituir ambiente difuso constante por SH de irradiância; adicionar probes de
   reflexão pré-filtradas e BRDF multiscatter.
6. Implementar CSM para o sol com splits estabilizados, atlas, PCF inicialmente e
   cache de casters estáticos. Vegetação usa alpha-test no shadow pass.
7. Adicionar oclusão de contato eficiente: primeiro GTAO/bent normals temporal;
   depois DDGI/GlowField conforme 7.2. Isso corrige áreas atrás de árvores sem
   falsificar cor por material.
8. Implementar exposição temporal por histograma, tone mapping AgX e white balance.
   Limitar adaptação para evitar bombeamento ao atravessar copa/clareira.
9. Nuvens volumétricas permanecem opcionais ao perfil S; o céu procedural padrão deve
   preservar a mesma composição visual nos perfis A/B/C.

**Aceite:** sol visível coincide com direção das sombras; nenhuma copa fica lavada;
o céu custa menos memória/bandwidth que o HDRI visível atual e materiais continuam
com reflexos/irradiância de qualidade igual ou superior.

### O3 — Eliminar trabalho invisível antes de reduzir pixels

**Alinha:** 2.5.1–2.5.5 e 7.1.1–7.1.6.

1. Construir BVH incremental global com bounds por draw/instância.
2. Aplicar frustum culling conservador e ordenar opacos front-to-back.
3. Gerar HZB e occlusion culling de dois passos com histerese; objetos recém-visíveis
   e bounds incertos ficam visíveis, nunca somem por um frame.
4. Agrupar instâncias automaticamente por malha+material+PSO e emitir draw packets
   persistentes. Remover trabalho por objeto da CPU.
5. Adotar `DrawIndexedIndirectCount`; meshlets/cluster culling entram como evolução
   global, com fallback discreto completo.
6. Gerar LODs no import por erro geométrico e selecionar por erro projetado em pixels.
   A transição dither temporal evita pop; silhueta e densidade visual são o gate, não
   apenas contagem de triângulos.
7. Ordenar PSOs e materiais para reduzir binds; transparências reais continuam
   back-to-front após a fase opaca.

**Aceite:** a imagem de referência é equivalente, nenhum objeto válido desaparece e
o custo escala com o conteúdo visível, não com o total carregado.

#### Contrato de chunks e geração de trabalho

1. Chunks ECS de 16 KiB armazenam componentes e versões; mudança de versão alimenta
   extração incremental, sem varredura de entidades estáveis.
2. Render chunks são persistentes e independentes dos chunks ECS. A chave de lote é
   mesh + material + PSO; bounds espaciais são metadados de visibilidade, não uma
   ordem para emitir um draw de CPU por célula.
3. O culling grava uma lista compacta de instâncias e `DrawIndexedIndirectCount`.
   Frustum é conservador; HZB tem histerese; objetos novos/incertos começam visíveis.
4. Streaming chunks possuem budget de bytes e tempo por frame, prioridade por
   distância/necessidade e nunca bloqueiam tick ou render thread.
5. Todo produtor declara frequência (`fixed`, `per-frame`, `on-change`, assíncrona),
   budget, ownership e contador de descarte/backpressure.

### O4 — Frame Vulkan orientado a TBDR

**Alinha:** 2.1.3–2.1.5 e 2.3.1–2.3.6.

1. Migrar todos os renderers para `PipelineCache`; persistir e pré-aquecer o cache.
2. Executar o Render Graph real na GPU, com anexos transient/memoryless, aliasing e
   fusão de passes comprovados por captura.
3. Tratar depth prepass como decisão medida em TBDR: usar onde alpha-mask/occlusion ou
   complexidade de fragmento paga o custo; evitar duplicar geometria cegamente.
4. Implementar Forward+ clusterizado e listas de luz por tile/froxel.
5. Gravar command buffers em paralelo por conjuntos de draw suficientemente grandes,
   usando timeline semaphores e múltiplos frames em voo sem aumentar latência.
6. Remover espera e upload do frame estável; streaming usa fila, staging ring e
   budget. Recriar surface não deve recarregar assets imutáveis.
7. Usar load/store ops explícitos e formatos compactos adequados ao mobile.

**Aceite:** captura AGI prova menos tráfego externo de memória, nenhuma criação de
pipeline no frame e ausência de bolhas evitáveis entre CPU, GPU e apresentação.

### O5 — Shaders, texturas e pós com equivalência visual

**Alinha:** 2.2.1–2.2.5, 2.4.2/2.4.6 e 7.5.1–7.5.6.

1. Migrar para biblioteca de shaders com reflexão e precisão explícita. Usar mediump
   somente onde teste quantitativo comprovar erro imperceptível.
2. Gerar variantes por capabilities/material e impor orçamento para evitar explosão.
3. Pré-filtrar IBL offline: SH pequeno para diffuse, cubemap/lat-long especular com
   resolução e mips adequados. Não manter 89,5 MB só para desenhar o céu.
4. Escolher ASTC por semântica: normal, HDR, alpha-mask e cor têm métricas e blocos
   próprios. Mips de alpha-mask preservam coverage.
5. Implementar TAA robusto e resolução dinâmica apenas após existir tempo GPU
   confiável. AetherSR/GSR/FSR devem demonstrar equivalência temporal e não podem ser
   usados para ocultar overdraw/culling ausente.
6. VRS atua somente em regiões de baixa importância comprovada e nunca em silhuetas,
   texto, folhagem fina ou highlights.

**Aceite:** queda de bandwidth/tempo GPU sem perda aprovada em FLIP/SSIM, sem shimmer,
ghosting ou perda de detalhe fino em movimento.

### O6 — Escalabilidade, 120 Hz e governança térmica

**Alinha:** 7.6.1–7.6.5 e `PowerGovernor`.

1. Calibrar o aparelho em 5 s e registrar GPU/driver/capabilities conhecidos.
2. Definir budgets, não listas arbitrárias de features, por perfil. O mesmo renderer e
   materiais são usados em S/A/B/C.
3. Perfil S oferece 90/120 Hz quando o frame p95 e a potência sustentada permitem.
4. PowerGovernor reduz primeiro frequência de atualização de sistemas temporais,
   budgets invisíveis e trabalho assíncrono. Alteração visível é o último estágio e
   deve ter histerese e indicação ao usuário.
5. Comparador visual dos perfis impede regressões silenciosas.

**Aceite:** 60 FPS sustentados no perfil A e transições térmicas sem oscilação,
stutter ou mudança visual abrupta.

## 5. Marcos executáveis

| Marco | Entrega demonstrável | Dependências | Critério de saída |
|---|---|---|---|
| **G0 — Verdade** | perfil da floresta, câmera determinística, timestamps GPU e captura AGI | nenhuma | reproduz FPS/defeitos sem trocar de cena |
| **G1 — Correção** | pipeline Opaque/Mask/Blend/DoubleSided e corpus de materiais | G0 | zero branco e zero desaparecimento conhecido |
| **G2 — Ambiente** | céu azul/nuvens/sol desacoplado do IBL, SH, exposição AgX | G1 | imagem melhor e custo do céu menor |
| **G3 — Sol e contato** | CSM cacheada + alpha-test de vegetação + GTAO/bent normals | G1–G2 | iluminação sob copa coerente |
| **G4 — Visibilidade** | BVH, frustum, HZB, instancing, LOD conservador | G0–G1 | custo proporcional ao visível, sem pop/sumiço |
| **G5 — Frame móvel** | Render Graph GPU, cache, Forward+, memoryless e paralelismo | G0–G4 | p99 GPU ≤16,6 ms e CPU p95 ≤3 ms |
| **G6 — Sustentação** | soak, perfis, governor e caminho 90/120 Hz | G5 | 60 FPS/30 min ≤4 W no perfil A |

G1 e G2 podem avançar em paralelo depois de G0. G3 depende da semântica correta de
alpha; G4 não pode ser aprovado enquanto houver desaparecimentos sem diagnóstico.

## 6. Primeiro ciclo de implementação

Ordem recomendada para o primeiro ciclo, sem antecipar sistemas avançados antes da
medição:

1. Corrigir o runner para perfilar `dirt-road` sem habilitar PoC-A.
2. Adicionar timestamp GPU do frame atual e captura AGI marcada por pass.
3. Corrigir import/formato/shader para `MASK`, `alphaCutoff` e `doubleSided`.
4. Criar regressões de imagem da câmera que mostra a floresta branca.
5. Separar sky visual de IBL e implementar o céu azul procedural com sol e nuvens.
6. Migrar `InstancedRenderer`/consumidores para `PipelineCache`.
7. Implementar ordenação opaca front-to-back e backface culling por material.
8. Introduzir frustum culling conservador com visualizador de bounds.
9. Medir novamente; só então escolher entre HZB, batching/indirect ou shader/bandwidth
   como próximo maior ganho.
10. Fechar o ciclo com build release, lint, testes, captura visual, 60 s e soak.

## 7. Testes obrigatórios

- Unitários do importador para todos os modos alpha, cutoff, double-sided, UV e canais.
- Golden packages versionados e teste de migração do formato de material.
- Testes de imagem estáticos e percurso em movimento, incluindo folhagem e reflexos.
- Bounds/culling property-based: culling otimizado nunca remove objeto que a referência
  conservadora considera potencialmente visível.
- Perfil bindless e fallback convencional.
- Perfis debug/release e validation layers sem VUID.
- Matriz mínima Adreno + Mali + perfil C; 60 s por PR e 30 min noturno.
- Regressão de lifecycle garantindo que cache de pipeline/assets sobrevive à surface.

## 8. Decisões proibidas

- Reduzir a resolução fixa, cortar sombras, remover árvores ou baixar texturas para
  “bater 60” antes de medir o gargalo.
- Criar condição `if (dirt-road)` ou por modelo de telefone no renderer.
- Tratar `MASK` como `BLEND` ou `OPAQUE` para simplificar pipeline.
- Ativar occlusion/LOD sem debug visual e bounds conservadores.
- Chamar acquire/present/CPU de “tempo GPU”.
- Declarar ganho usando PoC-A quando o problema reportado está em outra cena.
- Somar efeitos de Fase 7 antes de fechar o pipeline base M2 e seus budgets.

## 9. Relação com o plano principal

Este plano acelera o caminho para **M2 — cena PBR com sombras a 60 FPS no perfil A**
e prepara M7 sem inverter dependências. A prioridade imediata permanece nos itens
2.1.3/2.1.5/2.1.6, 2.2, 2.3 GPU real, 2.4 e 2.5. Os itens 7.1–7.6 entram somente
quando sua fundação correspondente estiver medida e correta. Céu procedural básico,
CSM, transparência correta e culling pertencem ao pipeline base; atmosfera Bruneton,
nuvens volumétricas, DDGI, VSM, AetherSR e VRS continuam evoluções da Fase 7.
