# Auditoria "full Unity" — renderização, ambiente, luzes, pós, materiais, partículas e fluxo do editor

Data: 19/10/2026. Base: `codex/ui-play-performance-apk-export` @ `e097da3a`. Só leitura de código; nada foi
executado no aparelho. Tudo abaixo vem de leitura estática: onde diz "confirmar", é hipótese forte que um teste
curto deve validar antes de mexer.

Decisão do dono (19/10/2026): tudo segue o funcionamento prático da Unity 6 (URP/HDRP). O que existe e não faz
sentido com a Unity pode mudar, inclusive cena salva (com migração).

Partículas: o código de partículas **não está nesta base**; foi auditado a partir do branch
`claude/g5-particles-p1-4cf3a2` (`native/scene/particle_system.h`, `docs/planos/G5-P1-PARTICULAS-2026-10-17.md`).
Materiais: o branch G3a (`claude/g3a-material-audit-a6b1f4`, roughness_source) também não está na base.

Legenda: **P0** corrompe a verdade do modelo (dois donos, controle sem efeito, resultado diferente do que a
Unity entrega com o mesmo ajuste); **P1** lacuna de fluxo/capacidade que o usuário Unity sente no primeiro dia;
**P2** acabamento, nomenclatura ou capacidade avançada. Esforço: S ≤ 1 dia, M 2–4 dias, L 1–2 semanas, XL > 2 semanas.

---

## 1. Resumo — o que mais pesa

1. **Sol, céu, ambiente e fog ainda têm acoplamentos "inteligentes" que a Unity não tem** (céu HDRI e ambiente
   escalados pela intensidade do sol; fog escurecido automaticamente; ambiente = média fixa de cores). O
   commit `e097da3a` resolveu a direção; falta resolver o brilho (A-01..A-03).
2. **Existe uma segunda fonte da verdade escondida**: a tela "Ambiente da cena" (oficina Lighting) edita 4
   multiplicadores gravados na raiz (`editor_properties.h:41–44`) que se somam ao componente Ambiente e à Luz
   (A-04). Pelo menos "Sol potência ×" é sobrescrito quando existe luz direcional.
3. **Cena sem Ambiente mostra um panorama fixo do mapa de teste ("dirt road")** em vez de Skybox procedural +
   ambiente de Skybox + Volume padrão (A-05). A Unity sempre tem esses padrões.
4. **Volume/Perfil não segue o Volume Profile da Unity**: perfil é *copiado* para a instância e precisa de
   "Atualizar perfil"; overrides são por grupo (céu/neblina/pós/indireto), não por parâmetro; sem Volume
   padrão de projeto; Volume Mask da câmera é só "todos ou uma camada" (V-01..V-05).
5. **Não há Reflection Probe nem Light Probe nem GI assada** (capacidades `render.probe.*` Planned). O modelo de
   exposição de interior é um multiplicador de ambiente por volume (P-01).
6. **Luz**: faltam Mode (Realtime/Mixed/Baked), Culling Mask, Cookie, sombra do sol por luz, Sun Source explícito,
   segundo direcional descartado, teto global de 8 locais (L-01..L-06).
7. **Câmera**: sem Clear Flags/Background, Culling Mask, HDR/MSAA, Physical Camera, Post Processing por câmera,
   Viewport/Render Target (C-01..C-04).
8. **MeshRenderer só tem "Renderizar"**: sem Cast/Receive Shadows, sondas, Contribute GI, Rendering Layer (M-01).
9. **Material**: sem escolha de shader, sem Height/Detail/Clear Coat, rugosidade em vez de Smoothness no
   vocabulário, modos de mistura só Opaco/Recorte/Mistura (T-01..T-05).
10. **Partículas (G5 P1)**: componente monolítico sem módulos ligáveis, material embutido (sem Material de
    projeto), 6 formas e ~7 módulos dos 25 da Unity (X-01..X-05).

---

## 2. Tabela priorizada

| ID | P | Área | Divergência (resumo) | Esforço | Pacote |
|---|---|---|---|---|---|
| A-01 | P0 | Ambiente | Brilho do céu HDRI e do ambiente escalado pela intensidade do sol | M | PK-1 |
| A-02 | P0 | Ambiente | Ambiente do céu procedural = 0,4 céu + 0,6 equador (média fixa), sem SH nem fonte Gradient/Color | M | PK-1 |
| A-03 | P1 | Ambiente | Neblina escurece sozinha pela luz do céu | S | PK-1 |
| A-04 | P0 | Editor | Multiplicadores legados da raiz ("Sol potência ×", "Ambiente ×", "Exposição ×", "Rotação do céu") | M | PK-1 |
| A-05 | P0 | Ambiente | Sem Ambiente na cena ⇒ panorama fixo de teste; sem Skybox/Ambiente/Volume padrão | M | PK-2 |
| A-06 | P1 | Ambiente | Skybox não é um recurso (Material de céu); só enum HDRI/Atmosfera/Física | L | PK-2 |
| A-07 | P1 | Ambiente | Sol não tem Sun Source; escolha implícita "mais brilhante, o do TimeOfDay primeiro" | S | PK-3 |
| A-08 | P2 | Ambiente | Sol: tamanho em graus e intensidade do disco próprios; Unity: Sun Size / Convergence; sem Exposure do Skybox | S | PK-2 |
| V-01 | P0 | Volume | Perfil copiado para a instância; precisa "Atualizar perfil" | M | PK-4 |
| V-02 | P1 | Volume | Override por grupo, não por parâmetro; Ambiente é um componente monolítico | XL | PK-4 |
| V-03 | P1 | Volume | Sem Volume padrão (global, prioridade mínima); sem Criar › Volume | M | PK-2 |
| V-04 | P2 | Volume | Camada própria "Ambiente 0..7" e máscara da câmera de 1 bit; Unity usa layers de objeto e Volume Mask | M | PK-4 |
| V-05 | P2 | Volume | Parâmetros discretos trocam em 0,5 (Unity: t>0) | S | PK-4 |
| V-06 | P2 | Editor | Inspetor de perfil pagina por "< 1/5 >" | M | PK-4 |
| P-01 | P1 | Luz indireta | Sem Reflection Probe, Light Probe, GI assada; reflexo/ambiente único global | XL | PK-6 |
| P-02 | P1 | Pós | Tonemapping: sem "None" nem Neutral real; AgX/Reinhard no lugar | S | PK-5 |
| P-03 | P1 | Pós | Faltam DoF, Motion Blur, Chromatic Aberration, Lens Distortion, Color Adjustments (Filter/Hue), Color Curves | XL | PK-5 |
| P-04 | P1 | Pós | Exposição: só EV fixo + automática; sem modos (Fixed/Automatic/Curve/Physical Camera) | M | PK-5 (G2 em curso) |
| P-05 | P2 | Pós | Bloom sem Scatter/Tint/Clamp/Dirt; liga/desliga em bool + intensidade | S | PK-5 |
| L-01 | P1 | Luz | Sombra do sol não é da Luz (Shadow Type/Strength/Bias no Light) | M | PK-3 |
| L-02 | P1 | Luz | 2º direcional descartado; teto global de 8 locais | M | PK-3 |
| L-03 | P1 | Luz | Sem Mode, Culling Mask, Rendering Layers, Cookie, Área | L | PK-3 |
| L-04 | P2 | Luz | Cone em meio-ângulo; Unity: Spot Angle/Inner Spot Angle completos | S | PK-3 |
| L-05 | P1 | Luz | Valores padrão incoerentes entre "Luz direcional" (10 000 lux), "Sol dia/noite" (3 motor) e modelo (100 000 lux) | S | PK-3 |
| L-06 | P2 | Luz | Cor "linear" no inspetor vs seletor sRGB da Unity | S | PK-3 |
| C-01 | P1 | Câmera | Sem Clear Flags/Background, Culling Mask, Viewport, Target Texture | L | PK-7 |
| C-02 | P1 | Câmera | Sem Post Processing/Render Shadows por câmera, HDR, MSAA | M | PK-7 |
| C-03 | P2 | Câmera | Sem Physical Camera (Sensor, Focal Length, Gate Fit) | M | PK-7 |
| C-04 | P2 | Câmera | "Prioridade" ≡ Depth; empilhamento Base/Overlay ausente | L | PK-7 |
| M-01 | P1 | MeshRenderer | Só "Renderizar": sem Cast/Receive Shadows, Light/Reflection Probes, Contribute GI, Rendering Layer | M | PK-8 |
| T-01 | P1 | Material | Sem escolha de shader/modelo (Lit, Unlit, Simple Lit); Astra = um PBR | L | PK-9 |
| T-02 | P1 | Material | Vocabulário Roughness vs Smoothness | S | PK-9 |
| T-03 | P1 | Material | Faltam Height/Parallax, Detail Maps, Clear Coat, Specular workflow | L | PK-9 |
| T-04 | P2 | Material | Mistura: sem Premultiply/Additive/Multiply; sem Render Queue/Priority | M | PK-9 |
| T-05 | P2 | Material | Importação Unity recusa PARALLAXMAP, DETAIL, alfa premultiplicado | M | PK-9 |
| X-01 | P1 | Partículas | Componente único sem módulos ligáveis (Unity: checkbox por módulo) | M | PK-10 |
| X-02 | P1 | Partículas | Renderer + Material embutidos; sem Material de projeto nem atlas/Texture Sheet | L | PK-10 |
| X-03 | P1 | Partículas | Módulos ausentes: Velocity/Force/Noise/Collision/Sub Emitters/Trails/Lights/Texture Sheet | XL | PK-10 |
| X-04 | P2 | Partículas | Formas 6 de 11; sem Mesh/Sprite/Donut; sem Rate over Distance | M | PK-10 |
| X-05 | P2 | Partículas | Teto por nível do aparelho (C 1 000) diferente de Max Particles; sem Culling Mode | S | PK-10 |
| E-01 | P1 | Editor | Criar › só Luzes; sem Volume, Sondas, Skybox/Nuvens; Ambiente só por "Adicionar componente" | M | PK-2 |
| E-02 | P2 | Qualidade | Qualidade = política por nível de aparelho (A/B/C/S), não Quality Levels | — | decisão |

---

## 3. Detalhe por item

### Ambiente, céu e sol

**A-01 (P0) — Brilho do céu e do ambiente depende da intensidade do sol.**
- Unity: o Skybox/Procedural tem `Exposure` próprio (`shader-skybox-procedural`); mudar a intensidade da luz
  direcional não clareia o céu nem o ambiente. O ambiente é "Environment Lighting › Intensity Multiplier".
- Astra: `instanced_renderer.cpp:1672–1675` calcula `skyScale = sol.intensidade / referência` e `:1735–1738`
  multiplica `sceneAoDetail[2..3]` (céu HDRI e reflexo) e o ambiente difuso por ele; `dirt_road_sky.frag:248`
  aplica no céu HDRI. Para HDRI com mapa autoral só vale `exp2(hdriExposureEv)` (`:1734`), então o mesmo botão
  (intensidade do sol) tem efeito diferente por tipo de céu.
- Impacto: ajustar a intensidade da luz pra equilibrar uma cena mexe no céu; scripts que fazem "sol apagando"
  apagam o céu HDRI. O físico usa outra regra (irradiância integrada).
- Proposta: `skyScale` deixa de existir. Céu procedural: `Exposure` (novo, padrão 1,3 como a Unity). HDRI:
  só `hdriExposureEv`. Ambiente: `indirectDiffuse` (já é o Intensity Multiplier). Calibrar os padrões para a
  cena nova e **migrar** cenas v≤15: ao ler, gravar `hdriExposureEv += log2(skyScaleHistórico)` quando a
  cena tem sol fotométrico e HDRI (um passo de migração único na versão 16 do Ambiente, com teste de
  comparação de imagem antes/depois).
- Dependências: G2 (exposição automática) usa o mesmo caminho de brilho; coordenar com a sessão de G2.

**A-02 (P0) — Ambiente é uma média fixa, não "Environment Lighting Source".**
- Unity (`lighting-window`): Source = Skybox | Gradient (sky/equator/ground) | Color, `Intensity Multiplier`,
  Skybox → harmônicos esféricos (SH L2) direcionais.
- Astra: procedural `instanced_renderer.cpp:1687–1698` usa `sky*0,4 + equador*0,6` (cor plana) e chão
  separado; fallback HDRI-sem-mapa `:1620–1623` usa `horizonte*0,65 + zênite*0,35`; só mapa HDRI tem SH
  (`:1609–1613`). `punctual/ambiente` hemisférico é a qualidade `Hemispheric` (`rendering_policy.h:AmbientQuality`).
- Impacto: sem direcionalidade do céu (pôr do sol não tinge um lado), sem modo Gradient/Color para estilizados
  e para UI de jogo mobile (comum na Unity).
- Proposta: enum `Ambiente › Fonte` (Skybox, Gradiente, Cor) + `Intensidade`; SH L1/L2 calculado na CPU do
  céu procedural (já existe `proceduralSkyAmbient`) e re-enviado quando o sol mexe (3 vec4 por quadro).
  Migração: cena antiga → Fonte = Skybox (comportamento atual aproximado).
- Esforço M. Depende de A-01.

**A-03 (P1) — Neblina escurece com o dia/noite sozinha.**
- Unity URP: a cor da neblina é só a que o autor define (ou HDRP "Color Mode = Sky Color", opcional).
- Astra: `instanced_renderer.cpp:1700–1712` e `:1735–1738` multiplicam a cor por um `daylight` calculado.
- Proposta: `Fog › Modo de cor` = Cor / Cor do céu (opção explícita); padrão novo = Cor; cenas antigas ficam
  "Cor do céu" (migração por versão, sem mudar a imagem). S.

**A-04 (P0) — Segunda fonte da verdade: multiplicadores legados da raiz.**
- Unity: não existe "sol × N" global; intensidade é da Luz, exposição é do Volume, rotação do céu é da
  Skybox Material (`_Rotation`).
- Astra: `editor_properties.h:41–44` (Sol potência ×, Ambiente potência ×, Exposição ×, Rotação do céu),
  mostrados em `editor_screen.cpp:9362–9366` (oficina Lighting), aplicados por
  `renderer::adjustEnvironmentLighting` (`environment_lighting.h:21–28`) via `instanced_renderer.cpp:1606`.
  `sunDirectionIntensity[3]` volta a valer `sun->intensity` absoluto em `:1681`, então **"Sol potência ×" é
  sobrescrito quando existe Luz direcional** (confirmar com teste), embora `skyScale` use a referência já
  multiplicada (`:1673`) — efeito indireto confuso. "Exposição ×" duplica `exposure_ev`.
- Impacto: controle sem efeito ou com efeito escondido; viola "propriedade sem comportamento real".
- Proposta: remover a oficina "Ambiente da cena" como edição dos 4 números e substituí-la por uma janela
  **Iluminação** (A-05/A-06/E-01) que edita o Ambiente padrão da cena; migrar `Rotação do céu` para
  `hdri_rotation_degrees` do Ambiente global criado na migração; `Exposição ×` → `exposure_ev` (log2);
  `Sol ×` e `Ambiente ×` → intensidade da luz/`indirect_diffuse` quando ≠ 1. Cena v≤N com os 4 em 1,0 (a maioria)
  migra sem ação.

**A-05 (P0) — Cena sem Ambiente.**
- Unity: toda cena tem Skybox (Default-Skybox procedural), ambiente de Skybox e Volume padrão (URP: perfil
  default do projeto, prioridade mais baixa — `urp/Volumes`).
- Astra: sem componente, `SceneEnvironment{}` (`active=false`), a cena usa o panorama `dirt_road` embutido
  (`dirt_road_resources.cpp:631`, sol 2 do motor) e `sceneSky[0]=0`; no editor a vista usa
  `defaultSceneViewEnvironment()` (`scene_environment.cpp:153`), que **não é** o que o Play usa.
- Impacto: Scene View e Game View diferem; um projeto novo parece "paisagem de Vila" mesmo vazio.
- Proposta: o documento da cena guarda um **Ambiente global implícito** (valores da janela Iluminação; mesmo
  schema do componente); Play e editor leem o mesmo; panorama `dirt_road` vira apenas HDRI de exemplo. Migração:
  cena sem Ambiente → global implícito = Atmosfera procedural + sol 1 + pós neutro; cenas existentes que
  dependiam do `dirt_road` ganham um HDRI explícito (guardar a escolha na migração). M.

**A-06 (P1) — Skybox não é um recurso.**
- Unity: Skybox Material (Procedural, Panoramic, Cubemap, 6 Sided) escolhido na janela Iluminação ou na câmera.
- Astra: `SkyModel` {Hdri, Atmosfera, Física} dentro do Ambiente (`scene_environment.h:14`, `environment.h:244`);
  cubemap e 6-faces inexistentes; HDRI vem de `EnvironmentMap` (`environment.h:280`).
- Proposta: tipo de recurso `Skybox` (material de céu: Procedural, Panorâmico, Cubemap, Físico) referenciado
  pela janela Iluminação e por câmera (override). Cubemap 6 faces como importador de LDR. L. Depende de A-05.

**A-07 (P1) — Sun Source.**
- Unity: `Sun Source` explícito; None ⇒ direcional mais brilhante (`lighting-window`).
- Astra: `selectDirectionalLight` (`punctual_lights.h:266`) escolhe a mais brilhante, com TimeOfDay primeiro.
- Proposta: campo `Fonte do sol` (referência a objeto) no Ambiente global; vazio = regra atual. S.

**A-08 (P2) — Disco do sol.**
- Unity Procedural: Sun (None/Simple/High Quality), Sun Size, Sun Size Convergence, Atmosphere Thickness,
  Sky Tint, Ground, Exposure. Astra: `sun_disk_degrees`, `sun_disk_intensity`, `atmosphere`, tint, ground;
  sem `Sun Size Convergence` nem `Exposure`. Proposta: renomear/adicionar na janela Iluminação (parte de A-01). S.

### Volume

**V-01 (P0) — Perfil copiado.**
- Unity: o Volume referencia o Volume Profile; editar um parâmetro com perfil compartilhado edita o recurso na
  hora (todos os volumes mudam); "Clone" cria perfil independente (`urp/Volumes`).
- Astra: `applyEnvironmentProfile` copia valores para `Environment.values` (`environment.h:34–41` grava
  também o GUID); editar a instância não altera o recurso; é preciso `updateEnvironmentProfile`
  (`editor_session.cpp:7916`) e há sincronização inversa `synchronizeEnvironmentProfile` (`:7857`) que
  sobrescreve as cópias.
- Proposta: com `profile` ≠ vazio, a instância **não guarda valores**: o inspetor edita o recurso (histórico em
  nome do recurso); sem perfil, valores locais ("Profile" vs "Shared Profile" da Unity). Migração: cenas com
  perfil cuja cópia diverge do recurso (`sameEnvironmentAppearance`==false) ganham um perfil novo "Clone de …".
  M. Dependência: nenhuma; destrava V-02.

**V-02 (P1) — Overrides por grupo.** Unity: cada efeito é um *Volume Override* (Bloom, Vignette, Tonemapping,
Color Adjustments, Lift Gamma Gain …) com checkbox por parâmetro e botões All/None; fog e skybox são da janela
Iluminação, não de Volume (URP). Astra: `EnvironmentOverrideSky/Fog/Post/Indirect` (`scene_environment.h:18–27`)
e mistura manual em `scene_environment.cpp:~78–150`; um único componente com ~170 números e 4 bools de override.
Proposta: componentes de Volume por efeito (`astra.volume.bloom` …) que compartilham o `VolumeProfile`, com
`active` por parâmetro e lista "Adicionar override"; o Ambiente atual vira migração para vários overrides.
XL; fazer depois de V-01 e em paralelo com PK-5 (cada efeito novo já nasce como override).

**V-03 (P1)** Sem Volume padrão de projeto nem *Criar › Volume › Global/Caixa/Esfera* (ver E-01). Proposta: o
Ambiente global implícito de A-05 é o Volume padrão (prioridade mínima); criação por catálogo. M.

**V-04 (P2)** Unity: `Volume Mask` da câmera = LayerMask de layers de objeto e `Volume Trigger` (transform).
Astra: `Environment.layer` 0..7 (`environment.h:18,27`), `Camera.environmentMask` 1 bit ou todos
(`camera.h:17,23`), e já existe `runtime/gameplay_layers.h` para layers de objeto. Proposta: unificar nas
layers de gameplay (máscara de 32 bits, multi-seleção) + `Volume Trigger` opcional. Migração: layer N → layer
de objeto equivalente criada com nome "Ambiente N". M.

**V-05 (P2)** `scene_environment.cpp:95,113,141` troca valores discretos em `amount>=0,5`. Unity usa `t>0`
(`VolumeParameter.Interp`). Proposta: seguir a Unity ou documentar; S.

**V-06 (P2)** `editor_screen.cpp:2678–2698` pagina grupos do perfil com `< 1/5 >`. Regra do projeto
(memória "Interface universal"): painéis empilhados com foldouts, sem botões paginados. M.

### Luz indireta

**P-01 (P1) — Sondas e GI.** Unity: `Reflection Probe` (Realtime/Baked/Custom, caixa de influência, blend,
box projection), `Light Probe Group`, Mixed/Baked lighting, Lightmap (`class-ReflectionProbe`, `LightProbes`).
Astra: `render.probe.irradiance` e `render.probe.reflection` **Planned** (`engine_capability.h:92,94`);
lightmap externo por submalha `DeviceLimited` (`lightmap_binding.h`), sem serviço de bake. Um único
ambiente global (`environment_lighting.glsl`); o modelo de cena usa `indirect_diffuse` por Volume como
compensação (`editor_scene_template_types.h:40–50`).
Impacto: interiores e metais sem reflexo local; é a maior distância visual para uma cena Unity importada.
Proposta (em fatias): (1) Reflection Probe capturada em tempo de edição (cubemap pequeno por sonda, BC/ASTC),
blend entre 2 sondas por objeto, `Skybox` como fallback; (2) Light Probe Group com SH L1 por tetraedro,
interpolação por objeto; (3) bake de lightmap só depois. Migração: nenhuma (componentes novos); os
`indirect_*` por Volume continuam válidos. XL. Depende de A-02 (SH) e M-01 (campos do MeshRenderer).

### Pós-processamento

**P-02 (P1)** Unity URP Tonemapping: None, Neutral, ACES. Astra: `ToneMapper {Reinhard, ACES, AgX}`
(`scene_environment.h:14`, `environment.h:245`); o comentário admite que "Neutral" é Reinhard histórico.
Proposta: enum {Nenhum, Neutral (Hable/Khronos PBR Neutral), ACES}; AgX permanece como extra; migração
Reinhard → Neutral (aproximação) com aviso no relatório. S.

**P-03 (P1)** URP Volume: Depth of Field, Motion Blur, Chromatic Aberration, Lens Distortion, Panini,
Color Adjustments (Post Exposure, Contrast, Color Filter, Hue Shift, Saturation), Color Curves, Channel
Mixer, Split Toning, White Balance. Astra tem exposição, bloom, vinheta, grão, AO, saturação/contraste,
LUT/gradação (Lift/Gamma/Gain, SMH, WB) — `engine_capability.h:153–164`. Faltam os restantes, e **nem
aparecem no registro de capacidades como Planned**. Proposta: registrar todos; priorizar para mobile:
Color Adjustments (filtro, matiz — baratos, entram na LUT 32³), Motion Blur por objeto (já há vetores),
DoF de bokeh simples, Chromatic Aberration/Lens Distortion (custo mínimo). XL em pacotes por efeito.

**P-04 (P1)** Exposição. Unity URP: Color Adjustments › Post Exposure; HDRP: Exposure (Fixed, Automatic,
Automatic Histogram, Curve Mapping, Use Physical Camera, Compensation). Astra: `exposure_ev` + `auto_exposure_*`
(histograma). Sessão G2 "exposição automática" está em andamento; esta auditoria não deve duplicá-la.
Proposta: apenas garantir que o resultado seja um *override de Volume* (V-02) com `Modo` explícito. M.

**P-05 (P2)** Bloom: faltam Scatter, Tint, Clamp, High Quality Filtering, Dirt; `bloom` é bool separado do
`intensity` (Unity: intensidade 0 = desligado). S.

### Luz

**L-01 (P1) — Sombra do sol na Luz.** Unity (URP `light-component`): Shadow Type, Strength, Bias (Custom),
Normal Bias, Near Plane **na Luz**, inclusive direcional; cascatas, distância e resolução vêm do URP Asset/Volume
(Shadows). Astra: `light.h:25–28` declara que a direcional não tem esses campos ("dois donos para um valor") e
`lightHasLocalShadow` exclui a direcional; sombra do sol = política global (`shadow_cascades.h`,
`rendering_policy.h`). Impacto: não dá para o sol "sem sombra" ou com força 0,6 por cena/objeto.
Proposta: mostrar `Tipo de sombra/Força/Bias/Normal bias` na Luz direcional; a política passa a ser o *teto*
(Unity: o URP Asset também limita). Migração: padrões = valores atuais da política (sem mudança visual). M.

**L-02 (P1)** Segunda direcional é descartada (`punctual_lights.h:200–206`, `directionalDropped`); teto de 8
locais por quadro (`:19`, `engine_capability.h:68`). Unity URP: 1 principal + até 8 adicionais por objeto
(Forward+ sem teto por objeto no Unity 6 padrão). Proposta: segunda direcional vira "adicional" sem sombra
(lua, preenchimento); revisar teto 8 → ligado ao nível do aparelho (política). M. Dependência: custo do laço
de luzes (medição G0).

**L-03 (P1)** Light URP: Type (inclui Area), **Mode** (Realtime/Mixed/Baked), Culling Mask, Cookie, Rendering
Layers (`urp/light-component`). Astra: `render.light.cookie` e `render.light.area` Planned
(`engine_capability.h:74–76`); sem Mode/culling/layers. Proposta: Culling Mask via layers de gameplay (V-04);
Cookie 2D para spot/direcional (uma textura por luz no atlas de sombras); Mode só depois de P-01. L.

**L-04 (P2)** Cone: Astra meio-ângulo (`light.h:11`, campos `inner_angle`/`outer_angle`); Unity: Spot Angle e
Inner Spot Angle completos. Proposta: apresentar em ângulo completo no inspetor e API C#; guardar como hoje.
S (sem migração de arquivo).

**L-05 (P1)** Padrões: criação "Luz direcional" = `intensity 10000` na unidade LuxLumen (`editor_creation_catalog.h:94`),
"Sol com dia e noite" = 3 na unidade do motor (`:103`), modelo de referência = 100 000 lux
(`editor_scene_template.h:74`). Unity: 1 (URP) ou 3,14 (HDRP, 100 000 lux com exposição). Proposta: uma
tabela única de "luzes padrão" ligada ao Ambiente global (A-05), com a exposição coerente. S.

**L-06 (P2)** `lightTriples` marca `color` como `LinearColor` (`light.h:152`): o inspetor mostra o valor linear;
na Unity o seletor é sRGB com conversão para linear. Confirmar a apresentação no seletor de cor e usar sRGB
em todas as cores de autor (Luz, Ambiente, Material). S.

### Câmera

**C-01/C-02/C-03/C-04** — `camera.h` tem FOV vertical, near/far, prioridade, projeção ortográfica e máscara de
ambiente. Unity (URP `camera-component-reference`): Render Type Base/Overlay, **Background Type** (Skybox,
Solid Color, Uninitialized), Culling Mask, Volumes (Mask, Trigger), Post Processing, Render Shadows, Anti-aliasing
(MSAA/FXAA/SMAA/TAA), HDR, Output Texture, Viewport Rect, Physical Camera, Priority (depth). Proposta: campos
em ordem; `Background` (Skybox/Cor) é o mais pedido por quem faz UI/2D; `Culling Mask` depende de V-04. Antialias
já é política global (`render.aa.*`); expor por câmera como override. Camada Overlay/Stack só se houver UI 3D.
Cena: `astra.camera` v3 → v4 com campos novos no fim (padrões = comportamento atual). Esforço total L.

### MeshRenderer

**M-01 (P1)** `class-MeshRenderer`: Cast Shadows (On/Off/Two-sided/Shadows Only), Receive Shadows, Contribute
GI, Receive GI, Light Probes, Reflection Probes, Anchor Override, Motion Vectors, Dynamic Occlusion, Rendering
Layer Mask. Astra: `mesh_renderer_meshRendererBooleans.inc` só `enabled` ("Renderizar"). Proposta: adicionar
`cast_shadows` (4 valores), `receive_shadows`, `motion_vectors`, `probe_usage`, `rendering_layer` (campos novos
no fim, padrões = hoje; `astra.render.mesh` v10 → v11, coordenar com a versão do G3a). M. Depende do
extrator de desenho (`render_instance.h`) e do passe de sombra (flag por instância).

### Materiais (Standard / URP Lit)

**T-01 (P1)** Unity: o Material escolhe um *shader* (Lit, Simple Lit, Unlit, Particles/*, custom). Astra:
`MaterialAsset` v6 (`material_asset.h`) = um PBR metal/rugosidade fixo (`render.material.pbr`); variantes de
pipeline "por material" `DeviceLimited` (`engine_capability.h:108`). Proposta: campo `Modelo` no MaterialAsset:
Lit (atual), Unlit (cor/emissão), Simple Lit (Blinn-Phong barato para mobile), Particula (liga a X-02). M–L,
versão 7 do recurso (já ocupada pelo G3a: usar 8 ou integrar).

**T-02 (P1)** Standard/URP Lit usam *Smoothness*; Astra usa *rugosidade* no modelo e na UI
(`material_parameters.h:roughness`). G3a acrescentou `roughness_source` (conversão). Proposta: **UI e API C#
exibem Smoothness** (1 − roughness) com o campo "Smoothness Source" (Metallic alpha / Albedo alpha) igual ao
Standard; o arquivo continua em rugosidade. Sem migração de arquivo. S.

**T-03 (P1)** Faltam Height/Parallax, Detail Maps (Albedo×2, Normal), Clear Coat (`render.material.clearcoat`
Planned, `:110`), workflow Specular, Occlusion já existe, Emission com cor HDR existe. Proposta: Height (POM
barato, só nível A/B), Detail (segunda UV, mapa de detalhe já tem peso por distância no shader
`dirt_road_shading.glsl:~303`), Clear Coat. L.

**T-04 (P2)** Alfa: Astra {Opaco, Recorte, Mistura} (`material_parameters.h:12`); Unity Lit: Opaque/Transparent
com Blending Mode Alpha/Premultiply/Additive/Multiply, Preserve Specular, Render Face, Priority.
Proposta: acrescentar Aditivo/Premultiplicado/Multiplicar (reaproveita pipeline de partículas), `Priority`
de ordenação. M.

**T-05 (P2)** `unity_material_import.cpp:114–138` recusa `_PARALLAXMAP`, `_DETAIL_MULX2`, alfa premultiplicado.
Resolve-se com T-03/T-04; o relatório de importação já lista os itens. M.

### Partículas (branch G5, `particle_system.h`)

**X-01 (P1)** Unity: cada módulo (Emission, Shape, Velocity over Lifetime, …) tem checkbox; só o Main é fixo.
Astra: componente único com 97 números/9 bools por grupos "Principal/Emissão/Forma/…". Proposta: bool `enabled`
por módulo e foldout correspondente; migração: módulos já usados ficam ligados. M.

**X-02 (P1)** Unity: `ParticleSystem` + `ParticleSystemRenderer` + **Material** de projeto. Astra:
renderer e material no mesmo componente (decisão registrada no cabeçalho do arquivo), texturas embutidas
(8 formas/3 ruídos/3 máscaras) e "textura do projeto" só no P2. Proposta: referência a Material de projeto
(modelo Partícula, T-01) e Texture Sheet Animation; o material embutido atual vira um Material pré-feito.
Migração: sistemas v1 ganham um Material criado dos campos atuais. L.

**X-03 (P1)** Módulos da Unity ausentes: Velocity/Limit/Inherit Velocity over Lifetime, Force over Lifetime,
External Forces, Noise, Collision, Triggers, Sub Emitters, Texture Sheet Animation, Lights, Trails, Rotation by
Speed, Size by Speed, Custom Data. G5 já registra `render.particles.trails/collision/mesh/compute` como Planned.
Proposta: ordem por valor em mobile: Velocity/Force/Noise → Texture Sheet → Sub Emitters → Trails →
Collision (mundo simples) → Lights. XL (vários pacotes).

**X-04 (P2)** Formas: 6 (esfera, hemisfério, cone, caixa, círculo, aresta) vs 11 (+ Mesh, Sprite, Donut,
Rectangle, MeshRenderer, SkinnedMeshRenderer). Faltam Rate over Distance, Simulation Space Custom, Scaling Mode,
Ring Buffer. M.

**X-05 (P2)** Teto por nível do aparelho (C 1 000, B 8 000, A 30 000, S 100 000) reparte sem alterar o valor
autoral — razoável para mobile; expor `Culling Mode` (Pause/Always Simulate) e reportar no console quando o teto
corta. S.

### Editor

**E-01 (P1)** Catálogo de criação (`editor_creation_catalog.h:239`): categorias Básicos, Geometria, Água, Física,
Luzes, Gameplay, Áudio, Física 2D, Navegação, Personagem. Unity: *GameObject › Light, Volume (Global/Box/Sphere/
Cylinder), Effects, Camera, Audio, UI*. Falta: **Volume**, **Sondas** (P-01), **Céu/Skybox** (A-06),
**Efeitos** (G5 acrescenta Partículas), **Nuvens/Clima** (G4). O Ambiente só nasce por "Adicionar componente"
ou pelo modelo de referência (`editor_session.cpp:5894`). Proposta: nova categoria "Ambiente e efeitos"
(ícones novos conforme AGENTS.md) com Volume global/caixa/esfera, Sondas, Partículas, e receita de clima da G4.
M. Depende de V-03/A-05.

**E-02 (P2, decisão)** Quality Settings da Unity = níveis nomeados por plataforma. Astra =
`ResolvedRenderingPolicy` por eixos e nível do aparelho A/B/C/S (`rendering_policy.h`). É um *desenho melhor
para mobile*; sugestão: manter, mas oferecer "Níveis de qualidade" no menu do projeto como presets nomeados
sobre os eixos (vocabulário Unity). Decisão do dono.

---

## 4. Já alinhado (não refazer)

- Direção do sol: `TimeOfDay` gira o transform da direcional e o céu lê o ângulo (`scene_time_of_day.h`,
  `e097da3a`); céu procedural = O'Neil do Skybox/Procedural (`sky_procedural.glsl`), Sky Tint = zênite 0,5.
- Sol por luz direcional "mais brilhante" como Sun Source implícito (A-07 só torna explícito).
- Unidades fotométricas lux/candela/lúmen e temperatura de cor (como HDRP).
- Sombra local com Shadow Type/Resolution/Strength/Bias/Normal Bias/Near Plane no Light (`light.h`).
- Prioridade, peso e distância de mistura de Volume (`scene_environment.cpp:55–70`); perfis embutidos de look
  (G2 item 7); LUT 32³ (Lift/Gamma/Gain, SMH, WB) como o URP.
- Material: canais, oclusão, UV por binding, transformação de UV (KHR), lightmap externo — completo para PBR básico.

## 5. Não verificado nesta auditoria

- Comportamento exato de "Sol potência ×" com luz direcional (A-04) — precisa de teste de host.
- Cena sem Ambiente em Play vs editor (A-05): lido por código; confirmar com captura.
- Ordem real de mistura de parâmetros discretos vs. Unity (V-05).
- Branches G2 (exposição automática) e G4 (nuvens/clima) estavam em andamento; pontos P-04 e E-01 podem
  mudar com o que elas entregarem. Nuvens/clima: nada no sky além de campos `cloud*` herdados do formato AEEN
  (`environment_lighting.h`); nenhuma implementação em `sky_procedural.glsl`.

---

## 6. Ordem de pacotes de correção

Cada pacote cita a referência oficial (Unity 6000.0). Antes de cada um: verificar o item "Não verificado".
Links marcados (†) foram abertos em 19/10/2026; os demais foram localizados por busca e devem ser reabertos.

| Pacote | Conteúdo | Itens | Referência |
|---|---|---|---|
| **PK-1** Brilho, ambiente e a segunda verdade | Tirar `skyScale`; Exposure do céu; Fonte de ambiente (Skybox/Gradiente/Cor) + SH; fog por cor; remover multiplicadores legados e migrar | A-01..A-04 | [Lighting window](https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html), [Procedural Skybox†](https://docs.unity3d.com/6000.0/Documentation/Manual/shader-skybox-procedural.html) |
| **PK-2** Janela Iluminação, Ambiente global e Criar | Ambiente global implícito; Skybox como recurso (cubemap); Criar › Volume/Ambiente | A-05..A-06, A-08, V-03, E-01 | [Using skyboxes](https://docs.unity3d.com/Manual/skyboxes-using.html), [URP Volumes†](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Volumes.html) |
| **PK-3** Luz como na Unity | Sun Source; sombra do sol no Light; adicionais; Mode/Culling/Cookie; padrões únicos; ângulo completo; cor sRGB | A-07, L-01..L-06 | [URP Light component†](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/light-component.html), [Light](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html) |
| **PK-4** Volume Profile de verdade | Perfil compartilhado editável; layers/Volume Mask; paginação → foldouts; Interp | V-01, V-02 (início), V-04..V-06 | [URP Volumes†](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Volumes.html) |
| **PK-5** Pós como overrides | Tonemapping None/Neutral/ACES; Color Adjustments; Bloom completo; DoF/Motion Blur/CA/Lens | P-02..P-05, V-02 | [URP Tonemapping](https://docs.unity3d.com/Manual/urp/post-processing-tonemapping.html) (confirmar caminho 6000.0) |
| **PK-6** Sondas | Reflection Probe → Light Probe Group → (depois) bake | P-01 | [Reflection Probe†](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ReflectionProbe.html), [Light Probes†](https://docs.unity3d.com/6000.0/Documentation/Manual/LightProbes.html) |
| **PK-7** Câmera | Background, Culling Mask, Post/Shadows/HDR/MSAA por câmera, Physical Camera | C-01..C-04 | [URP camera reference](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/camera-component-reference.html) (confirmar) |
| **PK-8** MeshRenderer | Cast/Receive Shadows, probes, rendering layer, motion vectors | M-01 | [Mesh Renderer†](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html) |
| **PK-9** Material | Modelo (Lit/Unlit/Simple Lit), Smoothness, Height/Detail/Clear Coat, mistura | T-01..T-05 | [URP Lit shader†](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html) |
| **PK-10** Partículas | Módulos ligáveis, Material de projeto, módulos de velocidade/força/ruído → sheet → sub-emissores → trilhas | X-01..X-05 | [Particle System modules](https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystemModules.html) |

Sequência recomendada e paralelismo:
1. **PK-1 → PK-2** em série (mesmo código de ambiente; migração de cena na versão 16 do Ambiente + raiz).
   Conflito direto com as sessões G2 (exposição) e G4 (nuvens/clima): combinar antes de editar
   `instanced_renderer.cpp:1600–1760` e `environment.h`.
2. **PK-3** e **PK-8** podem andar em paralelo com PK-1 se ficarem em `light.h`/`mesh_renderer.h`/`punctual_lights.h`.
3. **PK-4** antes de **PK-5** (cada efeito novo já nasce como override).
4. **PK-9** antes de PK-10 (X-02 depende do modelo Partícula).
5. **PK-6** isolado e longo; começar por Reflection Probe depois de PK-2.
6. **PK-7** quando a janela Iluminação (PK-2) existir (Background = Skybox).

Cada pacote: reservar versão de componente/recurso no momento do merge (mesh v10→v11 e MaterialAsset v7→v8 colidem
com o G3a; Ambiente v15→v16; câmera v3→v4), regenerar contratos (`tools/component_contracts`, C# gerado),
registrar capacidade em `engine_capability.h`, nota em `AstraDocs/data/updates.json` e captura no aparelho,
conforme `AGENTS.md`.
