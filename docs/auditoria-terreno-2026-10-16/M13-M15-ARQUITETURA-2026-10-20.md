# M13–M15: arquitetura proposta para revisão

Preparação isolada em 20/10/2026. Nenhum runtime ativo alterado, nenhum build.
M11/M12 continua congelado durante sua aceitação. Este documento não marca linhas
do plano como concluídas e não congela nomes públicos antes do inventário fonte.

## Escopo e contrato fonte já confirmado

M13: wetness/poças, pintura FX, transições, altura/inclinação, gotas/ripples.
M14: streams com mapas/normal/refração, fluxo animado, autoridade local/global.
M15: lava com fluxo, crosta, aquecimento/resfriamento visual e emissão.
Não confundir esses materiais com volume de água, erosão hidráulica ou fluido3D.

Review leu Streams interno3.9.0, Unity2019.4, dependência Core. `_StreamControl`
linear Clamp RGBA significa wetness/puddle/stream/lava. NÃO é peso de materiais
e NÃO contém vetor de direção. A textura dinâmica é outra RGBA: wet trail,
burn trail, water, lava. Shader combina Z/W estáticos com saturate(dynamicZW*3).
Direção do fluxo vem da normal geométrica/slope, com convenções específicas do
fonte; não inventar direção pintada RG. Time/fase precisa relógio runtime real.
Inventário integral de mapas, vetores, flags, per-layer, assets, Manager/Emitter/
Collider está em `docs/auditoria-terreno-2026-10-16/PROXIMO-M13-M15-2026-10-20.md`,
preparado pelo review. Essa tabela inteira é checklist obrigatório do modelo e
consumidores; este documento complementa arquitetura, não substitui nem reduz campos.

Já confirmados: Flow usa fase frac(time*speed) e offset direction*intensity;
StreamFlowParams(.4,.5,.3,.2)=refração/velocidade/intensidade do ciclo/depth damping.
LavaParams(2.7,1,.5,.275)=blend/max/amplitude/frequência temporal. DistanceWetness
(100,1,200,0) contém start/near/length/far; salvar end=start+length explicitamente.
Stream/LavaFades(0,10,1000,1500) são quatro alturas reais, não fade de distância.
Globals substituem limites wet/puddle/stream e rain intensity: só expor quando
autoridade tem autoria, persistência, binding e consumidor. Extras sem consumidor
ficam inventariados como pendentes, com proveniência integral, sem flags falsas.

## Pintura FX independente e dados persistentes

Proposta de arquivos: resources/terrain_fx_field.{h,cpp}, terrain_fx_asset.{h,cpp},
terrain_fx_import.{h,cpp}; renderer/terrain_fx_binding.h; shader terrain_flows.glsl.
`TerrainFxChannel {Wetness,Puddle,Stream,Lava}` tem canais independentes [0,1].
Não normalizar soma e nunca escrever `TerrainField.controlWeights`, vertex weights,
controlCells ou material RGBA. Pintar poça1 sobre wet1 é um estado válido.

`TerrainFxRecipe` contém enabled/resolução explícita, dados importados opcionais,
lista de `TerrainFxStroke`, IDs estáveis e versão do algoritmo. `TerrainFxStroke`
possui brush2D completo (posição local, shape círculo/box, raio, aspect, rotação,
hardness, opacity, target, flow, sampleSeconds, pressure, erase, gesture)
e canal explícito. Cada propriedade exposta deve participar do rasterizador.
Não reutilizar TerrainStroke.layer como canal FX nem TerrainTool.Paint ambiguamente.
Compartilhar funções geométricas de brush com adaptador tipado, sem reinterpretar
campos exclusivos de sculpt. O fonte confirmado usa Target: lerp(current,target,
clamp(falloff*deltaTime*flow*pressure,0,1)); paintValue0 apaga. Persistir duração e
pressão de cada amostra, não reproduzir com deltaTime do frame de reabertura.
Defaults fonte flow8/falloff1/target1/pressure1; tamanho fonte1 precisa conversão
explícita para raio em metros Astra, sem copiar limite GUI .01..30 como unidade
universal. SlopeRange atua em1-clamp(dot(normal,up),0,1), não em graus. Erase/Restore
afetam só canais FX selecionados. Accumulate seria extensão Astra, não requisito
fonte: só expor se implementado/testado, sem campo extra morto nesta primeira versão.
Raycast Wetness é operação assíncrona real de exposição a cobertura: cena/colliders,
range/seed/progresso/cancel/result snapshot e undo; não substituir por altura/slope.

Primeira capacidade é heightfield, como o domínio do fonte. Voxel/overhang precisa
domínio por superfície/volume distinto; rejeitar a combinação não suportada em vez
de fingir autoria única por face com um mapaXZ. Resolução FX independente da malha
e do controle PBR, com limite de bytes antes de alocar, sem redução automática.
Domínio local é {-size/2,-size/2,size,size}, incluindo transform do objeto no shader.

Fonte importada preserva PNG original e GUID, resolução e política Original/Cap;
bitmap editável é estado autoral distinto. Strokes são persistentes e reprodutíveis;
checkpoint opcional guarda bitmap + índice/revision/hash do prefixo, nunca substitui
histórico sem regra de migração. Snapshot derivado de material usa RGBA8 linear
Clamp sem normalização, e CPU amostra exatamente os bytes quantizados publicados.
Assim consulta CPU e GPU não divergem por float de brush versus textura8bit.

## Histórico, jobs e regiões sujas

Integrar ao estado de edição de um terreno por GUID, usando transações de gesto
existentes. Um gesto=uma entrada undo com receita/revision antes/depois, incluindo
canal, target e brush. Cancelar restaura bitmap e receita; redo reproduz mesmo hash.
Salvar/fechar/reabrir preserva strokes, fontes e resolução; Play não escreve de volta
o estado simulado sem comando explícito Bake/Save snapshot com consumidor real.

Worker captura immutable recipe + source images + terrain GUID + authorRevision +
generationToken. Cancelamento cooperativo por tile; resultado só entra se todos
esses tokens ainda coincidem. Resposta velha nunca substitui gesto novo/outro terreno.
Separar chaves de geometria, PBR e FX: pintura FX não deve invalidar triangulação,
colisão ou plantas; atualizar também o código que hoje compara receita serializada
inteira para compatibilidade. Mudar altura pode invalidar campos derivados de slope,
mas preserva os valores pintados em domínio local.

`TerrainFxDirtyRect` inteiro, inclusivo/exclusivo documentado, separado de dirty
chunks de malha. União por tiles, halo de filtro, mip ancestry calculada por nível.
Publicação pode começar com texture rebuild completo correto, mas API de dirty
rect e contagem de bytes deve existir desde a primeira entrega. Caminho parcial
precisa upload das regiões e mips afetados com proteção de fences; não sobrescrever
imagem lida por frame em voo. Mudança de size/resolução reprojeta por regra explícita
com confirmação concreta no editor, ou recria sem fingir que coordenadas persistiram.

## Persistência, GLB e cache

Proposta `AETERRAIN9`: preserva corpo8 e acrescenta cauda FX version1. Leitores1..8
criam FX desabilitado/neutro; writer novo declara9. Evitar parser implícito de versão
de um dígito para ampliações futuras. `AETERRAINSURFACE4`: corpo legado + extensões
V2/V3 intactas + cauda Flows version1, per-layer por GUID, mapas e políticas completos.
Leitores1..3 permanecem provados com fixtures produzidos pelos serializers antigos.
Não simplesmente trocar texto da versão para fabricar fixture histórico.

GLB deve ter metadados próprios FX: versão, domínio, sourceGUID, snapshot identity,
width/height, channel semantics e imagem linear independente dos maps PBR. `fxTexture`
não ocupa TerrainControlBinding.textureIndex. Guardar receitaFX na metadata editável
e snapshot necessário para renderer externo/Astra cold import. Dados runtime
dinâmicos não entram no GLB por acidente; se snapshot solicitado, gravar explicitamente
tempo, tick, seed e semântica, mantendo parâmetros/emitters originais separadamente.

Cache recebe seção opcional versionada própria, packing/Clamp/cap/revision/domain,
associada ao terreno/material alinhado. Chave inclui algoritmo FX e fontes reais.
Byte budgets, counts/strides/índices, PNG integrity e cobertura completa são validados
antes de mutar saída. Mips/teto respeitam minimumResidentMip; nunca converter fonte
FX em sRGB ou aplicar filtro normal. Snapshot vivo normalmente é não-streamable,
pois uma simulação escreve em coordenadas da grade inteira; custo declarado.

## ABI extensível e renderer

Preservar base2+40N, advancedv1 e todos os registros M05–M12. Sem Flows, produzir
exatamente rootv1/v2 e layout anteriores. Proposta com Flows: feature256 e rootv3
com quarta row de diretório; rows0..2 mantêm significados, inclusive domínio e Snow.
Row3=[directoryAbsPlus1,count,entryStride4,endExclusive]. moduleLayerRows2 e todos
os payloads existentes mantêm formatos; ponteiros absolutos acomodam deslocamento.

Diretório ordenado por typeID: entradas[typeID,recordVersion,recordAbs,recordRows].
Tipos iniciais FX binding, Wetness, Puddles, Streams, Lava e Authority. Cada tipo
tem stride fixo/versionado e presença validada contra settings. Reserva não é uma
flag com efeito prometido. Rejeitar tipo/version desconhecido nesta versão; extensão
futura deve definir regra explícita, não ler dados arbitrários como registros atuais.
Validar não sobreposição, extents, valores exatosfloat, descritores e budgets antes
de relocação. Dedup usa parte canônica zero, cópia é relocada atomicamente; nunca
mutar chave canônica ou substituir descritor faltando por branco.

`TerrainFxBinding` alinhado por material/terreno, publica static texture, dynamic
texture opcional, domain, authorityGUID e revision. Publicação de texturas, tabela,
controle PBR e FX usa a mesma transação de renderer e lifetime/fences. Tempo pertence
ao frame/scene runtime, não a relógio de parede no serializer. Renderer pausado usa
tick/tempo estável; editor scrub só existe com API e consumidor correspondentes.

## Autoridade e dinâmica

`TerrainFlowController` por cena: GUID persistente, enabled, timeScale, chuva/limites
globais, seed e settings de simulação efetivamente suportados. Terreno referencia
GUID explicitamente ou usa valores locais. Resolver no scene lifecycle: criação,
load, duplicação, remoção, reload, Play e pausa; referência faltante é diagnóstico,
sem singleton inventado. Source/global flags só habilitam a substituição autorizada.

Manager/Emitter/Collider devem ganhar modelo/componentes, criação, persistência,
consumidor e testes juntos se necessários ao pacote. Apenas adicionar seus nomes
ao menu não implementa streams. Dinâmica pode usar solver2D de tick fixo e grade
limitada, CPU como autoridade compartilhada com queries, depois uploaddirty; solver
GPU exige readback/versioning antes de consultaCPU alegar exatidão. Nenhuma promessa
de equivalência física ao solver proprietário antes de validar suas propriedades.
Separar fluxo UV visual de transporte de quantidade: velocidade do shader não deve
movimentar material pintado implicitamente. Inventário fonte decide flags reais.

## Ordem material e Snow/CPU

Separar amostragem de avaliação. Calcular UV/world gradients antes de variantes,
branches e fases de Flow; fetchGrad/frame explícitos evitam derivadas indefinidas
ao cruzar fase frac/time/células. Todos os papéis de um fluido usam a mesma seleção,
transform e fase. Normais decoded/world são normalizadas; scalar/texturasRGBA têm
roles declarados, linear/sRGB independentes e GA/RG/RGB quando fonte exigir.

Ordem proposta a conferir no fragmento completo: PBR+M05–M10 -> produzir estado
FX wet/puddle/stream/lava com filtros -> aplicar wet/poça/flow/lava -> calcular Snow
com o mesmo clearingFX -> M12 emission/transmission -> iluminação. A ordem exata
e interações rain/ripple/lava devem seguir fonte ou documentar diferença intencional.
Fonte retorna clearingSnow=max(puddle,stream,lava,wetness); flag wetnessMaskSnow soma
1-FX.R. Não substituir simplesmente pelo canal Puddle nem recalcular com outro UV.

Criar helper puro `evaluateTerrainFxState(inputs,settings,state)` usado como contrato
CPU/GPU. Inputs incluem mesmos bytesFX publicados, dinâmica/tick, height/slope,
normal/material e autoridade resolvida. `evaluateTerrainSnowCoverage` recebe clearing
calculado por esse produtor e prova de correspondência/revision. O campo legado
puddleHeight isolado permanece Estimate para valor nãozero até um caminho tipado
identificar o produtor M13–M15 compartilhado; não retirar a proteção só porque existe
um shader novo. CPU sem textura/dinâmica/material correspondentes retorna Estimate
com motivos, ou erro para fonte obrigatória ausente; nunca Exact por aritmética só.

## Qualidade, assets e cenários de aceite

Preservar todos os assets originais de Streams/Lava/rain/ripple: paths, GUIDs, hashes,
dimensões, canais/packing, cor e licença/proveniência. Contagem fica pendente do review;
não reutilizar normal Snow para inventar asset de água. Derivados lossless podem ter
papéis separados e samplerClamp; Original permanece padrão, caps só explícitos.

Custo: staticFX1 fetch adicional; dynamicFX+1; Flow normalmente duas fases por papel;
triplanar multiplica projeções; rain/ripple/distância adicionam seus mapas. Enumerar
samplers, bytes/resident/source/decodepeak, resoluções e update bytes por tick antes
de habilitar perfil mobile. Sem downres escondido, truncação de emitters/colliders,
ou desligamento automático de efeitos sem política persistente e diagnóstico.

Testes necessários: FX.RGBA independentes e soma>1; paint/erase/target/hardness/shape;
histórico de gesto, cancel/redo/save/cold; jobs antigos/outro terreno descartados;
FX-only preserva hashes de malha/colisão e pesosPBR; dirtyrect/mips/fences/bordas;
fixtures8->9 e surface3->4; GLB/cachecold e budgets atômicos; Clamp em UV0/1/mip;
tempo/pause/phase boundaries e direction slope; cada propriedade fonte com efeito;
authority local/global/load/remove; emissor/collider sem truncação; Snow clearing
CPU/GPU em mesmos bytes/tick e motivos Estimate quando incompleto; material normal,
wet/dry/cold/hot, luz/sem luz e assets originais. Captura UI antes/depois e cena real
lit de todos os módulos, separada do fixture sintético e do conceito gerado.

## Referências oficiais verificadas

Unity6000.0 Texture2D.SetPixelData (dados de textura/mips explícitos):
https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Texture2D.SetPixelData.html
ShaderGraph17.0.4 Time (tempo de shader separado de persistência):
https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Time-Node.html
ShaderGraph17.0 Sample Texture2D/Normal Blend já usados em M05–M12 continuam referência
de capacidade para projeção/fetch/normal. A arquitetura adapta capacidades, não copia
classes/visual Unity nem publica fragmentos privados. Doc CustomRenderTextures6000.0
não foi acessível nesta pesquisa; não citá-la como evidência confirmada.

## Fechamento do inventário recebido do review

Per-layer: porosity0..1 (slotfonte3.B) e foam0..2 (3.A). Definir precedência porosity
global versus camada, reaproveitando semanticamente snowPorosity sem duplicar duas
propriedades visíveis que disputem o mesmo valor; migração deve preservar o valor
M11 existente e só converter quando a autoridade nova for explícita.

Quatro assets reais obrigatórios, sem alteração nesta preparação:
- streamfoam512²RGB, GUID04426e150438d48e19ee055632fedf02: RG normalXY, B foam.
- rain256²RGBA, GUIDaca95408547244a2b8760b8425b5b0fc: R shape/lifetime, GB normal
  direction, A phase. Normal view é GB, exige decode/packing próprio, não RG/GA falso.
- lava01 1024²RGBA GUIDa6a8d659d589c4119a3afbd07e10a776 e lava02 GUID
  2d2978ad3535f45649b30977cb570eb8: RG normal/distortion, B heat lerp, A crust.
Todos lineares. Separar mips normalizados de XY e cadeias escalares brutas R/G/B/A;
filtrar normal não pode destruir foam, calor, crosta, phase ou lifecycle. Fonte PNG
e TGA originais/hashes completos estão na tabela do review; runtimePNG é derivado
lossless com hash próprio, preservando dimensão/canais e política Original default.

Manager defaults executáveis: evaporation(.01,.01),strength(1,1),speed(1,1),
resistance(.1,.1),wetnessEvaporation.01,burnEvaporation.01. Esses valores prevalecem
sobre defaults distintos do shader isolado. Solver depende de heightmap real e
buffers ping-pong; definir equivalência/adaptação da evaporação quantizada~.004
com timers persistentes no checkpoint, tick fixo e pausa. Guardar unidade original,
sem chamar strength de litros/segundo. SourceEmitter força multiplica raio, não
volume; não implementar multiplicação de vazão e alegar que é o mesmo parâmetro.

Emitters: tipoWater0/Lava1, defaultWater, strength1 [0,1], transform.position e
lossyScale.x definem centro/raio. Colliders: Water0/Lava1/Both2, defaultBoth, mesmo
domínio transformado. Limite fonte64 de cada é capacidade explícita e validada;
65º recusa criação/publicação atomicamente ou exige política nova documentada,
nunca corta os arrays. Remoção de objeto, referência de terreno, registro/unregister,
duplicação e reload devem atualizar consumidor sem ponteiro solto ou buffer órfão.

Adjacentes preservados como pendentes: TraxFXThresholds(0,0,0,0),
tessLavaDislacementScale1, specfade e integração mesh/gravity direction. Arquivo
de proveniência conserva esses campos/keywords; opções ativas exigem diagnóstico
até consumidor existir. Isso não permite chamar o pacote proprietário inteiro de
portado quando só M13–M15 do recorte original estiver aceito.
