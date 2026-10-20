# M13: pintura independente, umidade e poças

Revisão com aceite local na branch `codex/terrain-voxel-generator`. Este documento
registra implementação e evidências por camada; não encerra M13–M15, o primeiro
bloco nem os71 itens. Sem APK/ADB. A revisão anterior de neve está publicada em
`a3d27687`/`8f402594`; o pacote deste documento ainda depende de commit/push
próprios e publicação nas Atualizações do site.

## Implementado no código

Recipe9 é o único dono autoral do FX: bitmap linear RGBA e histórico completo de
gestos. R umidade, G poça, B correnteza e A lava são quantidades independentes,
sem normalização por soma e sem confusão com direção de fluxo. Checkpoints
vinculam domínio, resolução, fonte, prefixo de strokes e pixels por SHA256.
Encoding raw/RLE é sem perda e escolhe RLE somente quando menor.

Pincel tem alvo, raio em metros, fluxo, falloff, pressão capturada, tempo de
amostra, dureza, opacidade, círculo/caixa, proporção, rotação e filtro de
inclinação mundial. A API de toque atual não informa pressão: o editor captura1,
sem fabricar sensibilidade. Modo apagar usa alvo0 sem perder o alvo preparado.
Editar, pintar e transformar são intenções exclusivas. Bitmap e cursor são
consumidores reais; preview do material recebe o campo transitório durante o
gesto, e somente soltar confirma a autoria. Cancelamento restaura a prévia.

Controles contínuos usam barras e entrada precisa. O mapa compacto foi revisto
após captura: seletores reais de canal/modo liberam espaço para mostrar todo o
bitmap, mantendo tamanhos de toque e botão principal. Ícones vetoriais novos de
umidade/poça/correnteza/lava entram no atlas real:640 entradas, masters384px e
células96px, sem renumerar as636 entradas anteriormente publicadas.

O orçamento de edição96/192/256MiB é uma preferência explícita da sessão. A
estimativa agregada56bytes/texel recusa2048 em96/192 antes da alocação;256 permite
esse perfil. Não é medição de RSS. O mínimo interno do raster20bytes/texel é
distinto. Não há downsize automático, substituição do mapa existente ou perda de
strokes para fazer caber. Importação de fonte FX e rebase explícito ainda precisam
do fluxo completo; não estão representados por botões sem ação.

O shader usa os bytes RGBA8 realmente publicados, com filtro linear Clamp e um
único nível original. Isso impede o streaming de mudar a base por baixo de uma
consulta CPU que confirme esses bytes. Os originais autorais permanecem intactos;
um orçamento insuficiente recusa a publicação. Mips FX com consultas equivalentes
exigem integração futura própria. Slots, dimensões, domínio, hash, GUID derivado
e revisão são conferidos no importador/cache antes de habilitar o consumidor.

Umidade e poças têm consumidores de BRDF, normal e remoção de neve. Conservam
mínimo/máximo, altura/onda, fade por distância, contraste/máximo de poça e
porosidade global ou por camada. `snowPorosity` continua sendo a propriedade
canônica compartilhada; não existe cópia concorrente no material Flow. O relógio
material usa uma faixa transitória reservada do UBO, preenchida por quadro e
independente do relógio autoral da água. Não alterou offsets/push constants.

SurfaceV4 preserva integralmente corpo legado e caudas V2/V3 antes da extensão
Flow. Recipe9 preserva o corpo anterior antes do FX length-prefixed. Arquivos
antigos continuam legíveis, com novos efeitos neutros. Cache derivado revisão12
será regenerado. Leitura/exportação GLB e recovery/history usam cotas coordenadas;
Scene/registro guardam referências, sem duplicar o bitmap na cena.

## Evidências locais já obtidas

- Core ativo:103/103 cenários, incluindo correção de LOD ao reabrir Recipe9,
  fixtures reais SurfaceV1/V2/V3 e Recipe8, persistência e relocação foam1.5.
- GPU:252 modos0..251 na RTX3050, produção shader/tabela/serialização e samplers
  reais. A fixture offscreen32×32 não demonstra cena natural nem desempenho mobile.
- Pipeline GLB/import/cache:11/11 cenários, incluindo todos os16MiB do mapa2048²
  depois de decode/cold cache, apesar do teto genérico1. Fontes empacotadas mantêm
  canais B/A originais e vistas raw lineares; normal-mips não podem destruir espuma.
- Persistência/Recipe/RLE em snapshot:14/14. Controller/cobertura:9/9; fundação9/9
  e SurfaceV4 4/4 isolados. São conjuntos parcialmente compartilhados, não uma
  contagem somada de recursos ou promessa de integração visual.
- Editor integrado final:213/213, incluindo rolagem, modos/gizmo/câmera, pintura
  vertical, cancelamento/undo/redo, material RG, Apply/reabrir, publicação ao Move,
  rodapé aplicado, diagnósticos e estado Prévia sem alterar autoria.
- Android ARM64 sintático:26/26; repetição1/1 de editor_terrain_screen após o
  último ajuste visual.44 shaders de produção validados após ligar o relógio
  material. Sem link Android/APK/aparelho.
- 42 capturas finais:16 da tela completa com documento/seleção/publicação real de
  materiais e26 de páginas nos dois tamanhos, incluindo sliders, mapa, fontes
  vazias, modos e rodapé após Aplicar. Raster de UI, sem render3D/aparelho.
  Revisão estrutural e hipótese raster estão em UI-FX-2026-10-20.md.
- Gerador de ícones:1/1 teste, originais384px, no-op/mtime e636 IDs anteriores
  preservados. O atlas continua2048×3236; nenhuma fonte foi reduzida.

Logs finais: `terrain-fx-active-host-final.log`, `terrain-fx-android-final.log`,
`terrain-fx-android-screen-final.log` e manifests de `fx-full-final`/`fx-panels-final`,
em build/terrain-evidence. Este aceite é do recorte estático, não do M13 integral.

## Desempenho e correções verificáveis

A fila, recepção e rodapé passaram a verificar um token leve antes de copiar ou
serializar FX. O token inclui estado nãoFX, revisões/contexto, orçamento e todos
os campos relevantes dos strokes geométricos transitórios. O teste integrado
executado verifica64 quadros reais com2048, sem aumentar encodes/snapshots/jobs,
e uma edição geométrica como controle positivo. Não mede RSS nem tempo no Android.
Gestos geométricos legados ainda têm
serializações por evento; não se anuncia refatoração geral desse caminho.

Preview FX reutiliza biblioteca de geometria e troca textura/binding no máximo20Hz;
o publisher pode reconstruir pacote/upload internamente. Custo no aparelho e
upload GPU apenas da região suja não são alegados. Dirty rectangles/mips têm
contrato de raster, mas isso sozinho não prova upload parcial no renderer.

Foram corrigidos o gate de LOD que esquecia Recipe9, a guarda de relocação que
recusava foam válido acima1, o material neutro no cache e o include do verificador
GPU que carregava um shader antigo. A compilação das44 famílias detectou e
corrigiu a ausência do relógio no push constant real; o teste GPU isolado usava
um relógio explícito e não provava esse transporte. Uma leitura cp1252 no merge
local foi identificada e o arquivo restaurado de UTF8 com os limites de
persistência preservados.

## Próximas capacidades e limites

Integração inicial após a fundação:202/208 (`terrain-fx-active-host.log`). As falhas
foram corrigidas e a execução final passou213/213. A primeira revisão encontrou
cinco caminhos de UI
(rolagem avançada, liberação do campo ao fechar, cancelar gesto, ligar material
molhado e aplicar/reabrir RG) e uma expectativa antiga de writerV3. Esta última
foi revisada para conferir o writerV4 atual e o GLB com payload V3 congelado,
ambos recusando domínio ausente; a regressão passou na execução final.
O teste de64 quadros reais parados com seed2048 passou sem novo encode/snapshot/job.

O modo Transformar recebeu guardas para mostrar a barra e o gizmo reais apenas
nesse modo; Editar/Pintar os ocultam. Câmera cancela o gesto FX e retorna a Editar,
sem interferir nas abas geométricas/PBR. Fixture de tela completa foi integrada
para16 capturas em655×300/1280×720, incluindo Aplicado; build/revisão passaram.

Streams/Lava possuem overlay separado, não aplicado: registros tipados, mapas raw
e284 modos GPU executados/aprovados. Chuva/specularFade preparados em outro overlay
passaram302 modos GPU; faltam integração/UI/host/produção dos pacotes seguintes.
Dinâmica tem produtor CPU e6/6 cenários isolados em staging; scene/controller,
publicação e GPU dinâmica ainda não estão integrados. Não há menus alegando essas
capacidades prontas. A remoção de neve deve combinar coberturas por máximo: ligar
lava com quantidade zero não pode apagar a contribuição da poça.

Autoridades globais, chuva, emissores/colisores de fluxo, tracks, outras capacidades
do plano e validação Android real continuam abertos. Quatro canais PBR pintados
continuam o limite visual atual;32 camadas autoradas não equivalem a32 pintadas.

## Referências versionadas

Unity6000.0: [TerrainPaintTool](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintTool_1.html),
[CalculateBrushTransform](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainTools.TerrainPaintUtility.CalculateBrushTransform.html)
e [Time.time](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Time-time.html).
Shader Graph17.0.4: [Sample Texture2D](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Sample-Texture-2D-Node.html)
e [Blend](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/Blend-Node.html).
Fonte fornecida: MicroSplat Streams3.9.0/Unity2019.4 na coleção3.9.41; contratos
integrais M13–M15 e proveniência permanecem versionados neste diretório.

Conferência adicional em20/10: Unity6000.0,
[Custom Render Texture Inspector](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CustomRenderTexture.html).
A referência separa tamanho, formato, espaço de cor, mips e Clamp. Na Astra,
o snapshot FX é dado linear RGBA independente; uma escolha de qualidade de
textura PBR não pode mudar silenciosamente sua resolução ou seus canais.
