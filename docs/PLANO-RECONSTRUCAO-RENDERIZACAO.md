# Reconstrução coerente da renderização mobile

## Decisão

A linha de renderização passa a partir de `a796f5a`, último ponto anterior ao
Arm ASR e ao lote experimental de GTAO, luz local, sombra local e exposição
automática. O estado experimental foi preservado em
`codex/pre-render-rebuild-20260903`; ele não é mais a base de produção.

Arm ASR fica descartado. Antialiasing e reconstrução de resolução pertencem à
Aether e devem possuir fallback próprio, sem tornar a engine dependente de um
fornecedor.

## Zero medido

Dispositivo físico: Xiaomi 25053PC47G, Android 16, SM8735/Adreno, painel 120 Hz.
Cena e pose fixas: `dirt-road`, `-15.71,145.27,-25.72,2.75,0.11`, Release.

- 115,56 presents/s na captura de 15 s;
- GPU p95 entre 7,08 e 7,11 ms para orçamento de 7,33 ms;
- CPU p95 entre 2,25 e 2,36 ms para orçamento de 6,50 ms;
- pós-processamento: aproximadamente 1,33 ms;
- opacos: aproximadamente 5,76 ms;
- escala dinâmica estabilizada em 0,65;
- imagem sem a lavagem de exposição e sem os cards claros gigantes do lote
  experimental.

Esse zero é o teste de regressão. Uma feature não entra por parecer correta em
uma captura parada: precisa passar imagem, movimento, GPU e validação Vulkan.

## Contratos

1. Cada recurso visual é um eixo global configurável; preset apenas escolhe
   valores iniciais.
2. Nenhum algoritmo depende do nome da cena, de material específico ou do
   perfil nominal dentro do shader.
3. Qualidade próxima não é reduzida para pagar pixels distantes. LOD, mip e
   sombra usam erro projetado, histerese e transição explícita.
4. A configuração Auto respeita o orçamento da cadência. Em 120 Hz, 7,33 ms é
   teto de GPU, não objetivo aproximado.
5. Startup não pode concentrar uploads, compilação ou todas as faces de sombra
   no primeiro frame visível. Trabalho assíncrono recebe orçamento por frame.
6. Falha de capability produz fallback documentado, nunca shader indefinido.

## Ordem de implementação

### R0 — restaurar estabilidade e observabilidade

- manter a base anterior ao ASR;
- preservar a branch experimental somente para consulta;
- medir sempre Release no aparelho, com pose e rota reproduzíveis;
- registrar p50/p95/p99 por passe, escala interna e espera de apresentação;
- bloquear regressão de startup e VUIDs.

### R1 — vegetação e textura sem artefatos

- gerar mips de cutout com RGB premultiplicado por alpha;
- preservar cobertura sem transformar 2x2/1x1 em placa opaca;
- limitar `maxLod` ao último nível que representa a cobertura autoral;
- isolar tiles de atlas e impedir sangramento entre árvores/vistas;
- corrigir proporção do billboard e guardar normal/profundidade coerentes;
- cruzar LOD por histerese e dither estável, com máscara responsiva para AA;
- validar aproximação e afastamento em rota, não só screenshots paradas.

### R2 — antialiasing nativo

- substituir o booleano FXAA por `AntiAliasingMode` (`Off`, `Fxaa`,
  `Temporal`) serializável e inspecionável;
- manter FXAA como fallback universal;
- implementar TAA da Aether com jitter de baixa discrepância, reprojeção por
  profundidade, rejeição de disocclusion, clamp de vizinhança e peso responsivo
  para alpha-tested/transparência;
- separar nitidez de antialiasing e de escala dinâmica;
- invalidar histórico em corte de câmera, resize, mudança de escala e reload;
- aceitar TAA em Auto somente se substituir o custo do filtro anterior dentro
  do orçamento físico.

### R3 — sombras direcionais bonitas e estáveis

- conservar CSM estável por texel e cache de caster estático;
- usar blend entre cascatas e fade no alcance final;
- calibrar bias constante, slope e normal offset por texel;
- aplicar alpha-tested no passe de sombra de vegetação;
- oferecer PCF próximo e kernel menor distante como eixos independentes;
- atualizar cascatas sob orçamento e nunca produzir pico de abertura.

### R4 — iluminação e cor

- começar com sol, céu hemisférico e IBL calibrados da base;
- separar energia direta, ambiente e emissivo;
- manter exposição manual/fixa como referência determinística;
- reintroduzir exposição automática apenas em HDR, com histograma recortado,
  adaptação assimétrica, limites e compensação autoral;
- reintroduzir luzes locais sem sombra antes do atlas de sombra local;
- atlas local só entra com cache, prioridade, invalidação e faces por frame.

### R5 — profundidade visual opcional

- GTAO meia resolução somente depois de depth/normal terem contrato estável;
- upsample bilateral e rejeição temporal obrigatórios;
- probes de reflexão precisam de prioridade, volume e blend previsível;
- bloom, volumetria e atmosfera ficam desligáveis e com orçamento individual;
- nenhuma dessas etapas é agrupada em um único commit ou medição.

## Portas de aceitação

Cada etapa precisa passar, nesta ordem:

1. testes puros e validação de shaders;
2. build Android Debug e Release;
3. zero VUID no Debug físico;
4. três poses fixas, incluindo o hotspot;
5. rota de aproximação e afastamento da vegetação;
6. comparação visual com mesma exposição e mesmo frame;
7. GPU p95 <= 7,33 ms e CPU p95 <= 6,50 ms em 120 Hz;
8. sem pico de primeiro frame causado pelo recurso;
9. soak térmico antes de tornar o recurso padrão em Auto.

Se uma etapa falhar, ela permanece opt-in e o Auto continua no último ponto
aprovado. Código existir não significa que o aparelho precise pagá-lo.

## Checkpoint da reconstrução

Implementado e validado no host nesta etapa:

- R1 usa a mesma função de dither no prepass de cobertura e no shading com
  `depth EQUAL`; aproximação e afastamento retornam o mesmo par fino/grosso e
  a mesma transição complementar;
- o impostor distante recebe uma normal volumétrica de copa, estável em relação
  ao yaw da câmera, em vez de iluminar a árvore inteira como uma placa;
- a saturação da irradiância de ambiente do AEEN v3 passou a ser realmente
  consumida pelo shader; o recurso expõe saturação e cor de rebatimento do solo
  sem alterar a cor do sol ou do material;
- R2 possui FXAA espacial como fallback universal e TAA próprio opt-in, com
  jitter Halton, reprojeção por profundidade, corte de câmera, clipping local,
  rejeição por movimento e invalidação em mudança de escala;
- o TAA só é ativado no pipeline que implementa o contrato completo de câmera e
  jitter. Falta generalizar matrizes atual/anterior antes de habilitá-lo nos
  previews de cena e material;
- a pressão térmica reduz apenas detalhe distante (erro de LOD e alcance de
  variantes de material), e reconfigurar a política não restaura a resolução
  dinâmica ao máximo no instante em que os clocks já estão caindo.

As portas de host estão verdes: shaders reproduzíveis e válidos, 329 testes
nativos e APK Debug compilado. R1/R2 continuam sem aprovação física nesta
revisão: ainda são obrigatórias a rota de aproximação/afastamento, as poses da
mancha branca/sombra roxa, zero VUID e o soak térmico com ADB reconectado.
