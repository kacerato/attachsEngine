# Plano de céu, nuvens e iluminação coerente

> **Criado em 29/08/2026**, depois de um ciclo revertido (`b2b94ca` → `dfe6945`).
> Este documento **não substitui** `PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md`: ele
> detalha os itens **O1** (correção de materiais/visibilidade) e **O2** (céu e
> iluminação) daquele plano, que estavam descritos em uma linha cada e se
> mostraram grandes demais para uma fatia só. A ordem global de execução, os
> gates de cadência e as decisões proibidas continuam sendo daquele documento.
>
> Alinhamento com `PLANO-ENGINE-MOBILE.md`: itens **2.2.5, 2.4.1–2.4.6, 7.2.7,
> 7.3.5, 7.4.2–7.4.3, 7.5.1**. Nenhum item é declarado concluído aqui, e **M2
> continua aberto**.

---

## 1. Evidência medida — a base deste plano

Tudo nesta tabela foi medido nesta sessão, no aparelho de referência
(Xiaomi `25053PC47G`, Adreno, Android 16, 2772×1280) ou diretamente nos assets
cozidos do repositório. Nada aqui é hipótese.

| # | Achado | Como foi medido | Consequência |
|---|---|---|---|
| 1 | **Normal não é invertida na face traseira.** Inverter com `gl_FrontFacing` devolveu cor natural à folhagem | Build instalado no aparelho, captura antes/depois | **Causa dominante do branco.** Confirmada por correção |
| 2 | **Nenhum dos 26 materiais tem `doubleSided`** (bit 32 nunca presente), mas o pipeline desenha tudo com `VK_CULL_MODE_NONE` | Leitura dos 26 `MapMaterialRecord` de `scene.aemap`; `instanced_renderer.cpp:275` | Faces traseiras que o autor nunca previu são sombreadas. É o mecanismo por trás do achado 1 |
| 3 | **Especular de ambiente não é amostrado na folhagem.** O branch `metal>.01 \|\| rough<.65` não dispara para folhagem (`rough≈1.0`) nem estrada (`0.81`); dispara só em materiais tipo poça (`rough 0.42`, 100% dos texels) | Decodificação dos canais G/B das texturas MR | **Especular descartado como causa do branco.** O HDRI de 89,5 MB serve hoje quase só de plano de fundo |
| 4 | **Cobertura alpha erode nos mips**: 26,1% → 22,6% (mip 0→4); texels de alpha parcial crescem 22,1% → 57,2% | Decodificação da cadeia de mips de `texture_000-fallback.aetex` | Afinamento e aliasing de silhueta à distância. Real, mas **secundário** |
| 5 | **Sangramento de RGB para branco: descartado.** Texels transparentes do atlas guardam `[79,82,46]` (verde-oliva escuro), corretamente dilatado | Média de RGB onde `alpha < 8`, todos os mips | Hipótese eliminada. **Registrado para não ser reinvestigada** |
| 6 | **Não existe oclusão assada.** O canal R de toda textura MR é constante `1.0` | Estatística do canal R | AO precisa ser de runtime; não há AO grátis a recuperar |
| 7 | **A cena é essencialmente difusa**: `metallic = 0` em todos os materiais, `roughness` entre 0,78 e 1,0 | Registros de material | Iluminação = sol `N·L` + ambiente constante. **Sem sombra e sem AO, achatamento é estrutural, não de calibração** |
| 8 | **O céu atual é uma foto de pôr do sol amostrada direto**, enquanto o sol da cena é um sol de dia | `dirt_road_sky.frag` (pós-revert) faz `textureLod(environmentMap,…,0)` | É a incoerência relatada: céu e luz vêm de fontes diferentes |

### 1.1 Diagnóstico consolidado

O branco na folhagem tem **duas causas independentes**, em ordem de peso:

1. **Face traseira sombreada com a normal errada** (achados 1+2) — dominante,
   correção já validada em hardware.
2. **Erosão de cobertura nos mips** (achado 4) — afina a silhueta ao longe.

E a sensação de "escuro e apagado" **não é** falta de brilho: é ausência de
**contraste direcional**. Com tudo difuso (achado 7), sem sombra e sem AO, a
única forma de "clarear" é subir o ambiente — que é exatamente o que lava a
imagem. **Foi esse o erro do ciclo revertido.**

---

## 2. A lição do ciclo revertido — regra de processo

O ciclo `b2b94ca` trocou, na mesma fatia: fonte de IBL, modelo de ambiente
(SH9), operador de tonemap (AgX) **e** a calibração de sol/ambiente. A correção
de material funcionou; o conjunto ficou pior; e não havia como saber qual dos
quatro eixos causou a piora sem desfazer tudo. Daí três regras obrigatórias:

1. **Um eixo por vez.** Modelo físico, calibração artística e operador de
   tonemap são mudanças separadas, em commits separados, cada uma com captura
   antes/depois no aparelho.
2. **Calibração nunca é palpite.** Enquanto sol e céu não estiverem na mesma
   escala física, "aumentar o sol" e "diminuir o ambiente" são números mágicos
   que se desequilibram a cada mudança de modelo.
3. **Nenhuma mudança de aparência entra sem medição objetiva** das poses fixas
   (§3.1). Impressão visual entra como veto, nunca como aprovação.

---

## 3. Fundação: `EnvironmentState` em unidades físicas

A coerência tem que ser **derivável**, não ajustada. Hoje `sunIntensity=2.1` e
`ambientStrength=0.28` são números sem escala comum. A proposta:

| Grandeza | Unidade | Referência |
|---|---|---|
| Iluminância do sol | lux | ~100.000 (meio-dia claro), ~20.000 (dourada) |
| Luminância do céu | cd/m² | derivada do modelo atmosférico, não autorada |
| Exposição da câmera | EV100 | de abertura/obturador/ISO |

Um único `EnvironmentState` (hora, latitude, turbidez, albedo do solo) passa a
derivar **sol, céu visível, irradiância difusa, reflexão especular e exposição**.
Mudar a hora move tudo junto, por construção. Esse é o item 1 do O2 do plano de
otimização, e é pré-requisito de todo o resto — sem ele, cada estágio abaixo
reintroduz o desequilíbrio que causou o revert.

**Compatibilidade:** `EnvironmentLighting` (hoje 64 bytes, AEEN v1) ganha versão
nova com `structSize`/`apiVersion`, conforme §3.3 do plano de fechamento de
lacunas. A ferramenta de HDRI autoral (`cook-hdri-environment.py`) precisa
acompanhar a versão nova, ou passa a recusar explicitamente — nunca ficar
escrevendo um formato que o runtime não aceita mais.

---

## 4. Estágios

Cada estágio é entregável e verificável sozinho. A ordem é dependência técnica.

### E1 — Correção do branco *(primeiro, é regressão reportada)*

**Alinha:** 2.4.1, 2.4.5, 7.3.5, O1 do plano de otimização.

1. **Variantes de pipeline por estado de rasterização**, não por asset:
   `opaque-single`, `opaque-double`, `mask-single`, `mask-double`, `blend`.
   Consome o `doubleSided` que o importador já grava e o renderer ignora
   (achado 2). Elimina o `CULL_MODE_NONE` global.
2. **Face traseira correta em material double-sided**: inverter a normal
   (`gl_FrontFacing`) — correção já validada em hardware (achado 1).
3. **Política de folhagem no import, dirigida por dados**: cartões que a
   heurística de recorte já identifica como cutout são promovidos a
   double-sided mesmo quando o glTF os declara single-sided (achado 2: nenhum
   material do mapa tem a flag, e cortá-los deixaria as árvores ocas). A decisão
   fica gravada no material, nunca em `if` por nome de asset.
4. **Mips que preservam cobertura alpha** (rescale por nível para manter a
   fração acima do cutoff do mip 0). Ataca o achado 4.
5. **Anti-aliasing de cobertura**: alpha-to-coverage quando houver MSAA;
   dither temporal caso contrário.

**Aceite:** zero folhagem branca nas poses fixas; silhueta e densidade
preservadas de perto e a 100 m; backface culling reduz fragmentos sombreados
sem remover geometria válida (contagem por captura, não por impressão).

### E0 — Instrumentação de aparência *(antes de qualquer mudança de luz)*

**Alinha:** O0 do plano de otimização, 7.6.

1. **Poses determinísticas** nomeadas (clareira, sob copa, contraluz, close de
   material, vista aberta), acionáveis por parâmetro de lançamento.
2. **Carta de referência global**: cartão cinza 18%, esferas branca/preta e
   color checker, opcionais por opção de debug, para que exposição e tonemap
   sejam validados numericamente. Um cinza 18% sob sol de meio-dia tem que cair
   em ~0,18 linear na saída.
3. **Métricas objetivas por captura**: média/desvio por região, histograma de
   luminância, % de pixels saturados em 0 e em 1, SSIM/FLIP contra referência.
4. **Comparador A/B** que roda as poses antes/depois e falha o gate fora do
   orçamento.

**Aceite:** a regressão do ciclo revertido teria sido detectada automaticamente
(azul dominante, contraste caindo, nuvens sumindo são todos mensuráveis).

### E2 — Céu com nuvens

**Alinha:** 2.4.4, 7.4.2, 7.4.3, O2 itens 2/3/9.

1. **Atmosfera analítica** (Hosek-Wilkie ou Preetham) no lugar do gradiente
   fixo e da foto. Fórmula fechada, sem LUT, com cor do céu e do sol derivadas
   da elevação solar e da turbidez do `EnvironmentState`. Bruneton (7.4.2) entra
   depois **atrás da mesma interface**, sem reescrever consumidores.
2. **Céu assado num cubemap pequeno** (~128², RGBA16F), regerado só quando o
   `EnvironmentState` muda — não por pixel, por frame. Isso resolve custo e
   coerência de uma vez: o mesmo cubemap alimenta a cúpula visível, o SH difuso
   e o especular pré-filtrado. Trocar hora do dia atualiza os três juntos.
3. **Nuvens em camadas, escalonadas por perfil:**
   - **Todos os perfis:** fBm procedural na cúpula com **cobertura, densidade e
     detalhe separados** (parametrização estilo Nubis), iluminada por Beer +
     fase Henyey-Greenstein + termo *powder* para a borda prateada. Animada em
     coordenadas de mundo. Assada no mesmo cubemap → custo por pixel ~zero.
   - **Perfil A+:** camada 2.5D com ray march curto em ¼ de resolução e
     reprojeção temporal.
   - **Perfil S:** volumétrico completo (7.4.3), já previsto no plano principal.
4. **Disco solar** com tamanho angular correto (~0,53°) e escurecimento de
   limbo, coerente com a direção que projeta as sombras.

**Nota de calibração:** o threshold atual de nuvem é
`smoothstep(.79, 1.17, cloudNoise)`, mas o ruído tem média 0,749 e máximo ~1,44
(medido) — ou seja, as nuvens quase nunca atingem opacidade plena. É por isso
que "não tem nuvem". Cobertura passa a ser um parâmetro do `EnvironmentState`
em fração de céu coberto, não um threshold de ruído.

**Aceite:** nuvens visíveis e legíveis, sem cintilação ao girar a câmera
(diff quadro a quadro num giro fixo dentro do orçamento); céu custa **menos**
memória e banda que o HDRI de 89,5 MB atual; sol visível coincide com a direção
das sombras.

### E3 — Iluminação coerente

**Alinha:** 2.4.2, 2.4.3, 2.4.4, 2.4.6, 7.2.7, 7.3.5.

Ordem interna importa — cada item depende do anterior:

1. **Sombras direcionais (CSM)** — item 2.4.3, hoje **não iniciado**, e o maior
   ganho visual isolado disponível. Sem sombra de copa não existe floresta
   crível, e é a ausência dela que força o ambiente para cima (achado 7). 4
   cascatas estabilizadas (snap por texel), PCF, **alpha-test da vegetação no
   shadow pass** (7.3.5), cache de casters estáticos.
2. **Oclusão de contato (GTAO)** — item 7.2.7. Escurece base de tronco,
   vãos e sub-copa. Meia resolução + upsample bilateral + filtro temporal.
   Necessário porque não há AO assado (achado 6).
3. **Transmissão de folhagem** — lobo de translucência para material de folha
   (wrap diffuse + transmissão por trás). É o que faz folha acender em
   contraluz, e é a resposta *correta* para face dupla — de onde E1 item 2 sai
   de "inverter a normal" para "modelo de dois lados de verdade".
4. **IBL derivado do céu** — SH9 difuso + especular pré-filtrado, ambos do
   cubemap de E2. É o trabalho revertido, refeito sobre unidades físicas e
   depois de existirem sombra e AO. Observação do achado 3: nesta cena o que
   pesa é o **difuso**; o especular só aparece em poças e material molhado.
5. **Exposição e tonemap** — item 2.4.6. EV100 → AgX, calibrado contra a carta
   de E0. Auto-exposição por histograma com taxa de adaptação limitada, para
   não bombear ao atravessar copa/clareira.

**Aceite:** sob copa preserva contraste sem preto esmagado nem branco lavado;
verdes lêem como verdes (saturação medida, não impressão); nenhuma cor de
material foi falsificada para compensar iluminação.

### E4 — O custo é pago, não acumulado

Requisito explícito do projeto: desempenho, gráfico e complexidade andam juntos.
Cada item acima entra com sua fonte de financiamento:

| Custo novo | Financiamento |
|---|---|
| Sombras (CSM) | Frustum culling + cache de cascatas estáticas + prepass de cobertura já existente |
| GTAO | Meia resolução + temporal |
| Nuvens | Assadas no cubemap; ray march só em perfil A+, em ¼ |
| Céu analítico | Deixa de ser amostragem por pixel de textura 4K |
| — | **Remoção do HDRI de 89,5 MB**: ganho direto de memória e banda (achado 3: hoje é quase só plano de fundo) |

Orçamentos por perfil (**alvos a validar, não medições**) entram em
`metrics/budgets.v1.json` antes da implementação, e a cadência continua regida
pela tabela 60/90/120 Hz de `PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md` §3.2.

---

## 5. Gates

Cada estágio só fecha com, no aparelho, **nos dois caminhos** (bindless e
fallback), debug e release, sem VUID:

- capturas antes/depois das poses fixas de E0, com as métricas objetivas;
- orçamento de GPU/CPU respeitado, medido com o `VulkanGpuFrameTimer` já
  existente — nunca com tempo de acquire;
- `ESTADO.md` e `MATRIZ-MARCOS.md` atualizados **na mesma mudança**;
- nenhuma regra condicionada a nome de mapa, asset ou modelo de aparelho.

Matriz mínima (Adreno + Mali + perfil C) continua sendo requisito de M0/M2 e
**não é** atendida por um aparelho só. Nenhum estágio aqui fecha `GAP-HW-01`.

---

## 6. Decisões proibidas

Além das de `PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md` §8:

- Trocar modelo de iluminação e calibração na mesma mudança (foi o erro do ciclo revertido).
- Subir ambiente para compensar ausência de sombra ou AO.
- Aceitar mudança de aparência por impressão visual sem as métricas de E0.
- Tratar céu, irradiância difusa e reflexão especular como fontes independentes.
- Assar sol/nuvens no IBL difuso (conta a luz do sol duas vezes).

---

## 7. O que este plano não entrega

- Não fecha M2 nem nenhum gate M0–M9.
- Não entrega Bruneton, DDGI/GlowField, VSM, TAA, AetherSR nem VRS — todos
  continuam na Fase 7 do plano principal, atrás da fundação que este plano cria.
- Não resolve a matriz de aparelhos (`GAP-HW-01`).
- Não trata streaming, LOD nem occlusion culling — são O3/O4 do plano de
  otimização e independem deste trabalho.
