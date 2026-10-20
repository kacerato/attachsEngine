# Catálogo e materiais PBR — 20/10/2026

Incremento na branch `codex/terrain-voxel-generator`, dentro do plano integral de 71 itens. O primeiro bloco permanece aberto. Sem ADB e sem APK novo nesta etapa.

## Capacidade integrada

O catálogo original de 82 materiais pode substituir individualmente os quatro canais atuais de pintura. A primeira troca migra explicitamente as quatro camadas legadas, com identidades persistentes e suas texturas originais. Os GUIDs pertencem aos canais autorais, não às posições transitórias da GPU. Propriedades, packing e proveniência JSON do catálogo são preservados integralmente.

Os sete papéis — albedo, normal, altura, AO, roughness, metallic e emissão — chegam do PNG original ao importador, cache, biblioteca de fontes, remapeamento de descritores e shader. Cor/dados têm visualizações sRGB/linear distintas; normal RGB/RG tem geração de mips própria, reconstrução de Z e força zero neutra. Remapeamento escalar/cor, canal, inversão, fatores, tint e saturação possuem consumidores reais. Valores constantes são usados quando um mapa está ausente.

Projeções UV, triplanar local e mundial usam escala e deslocamento em metros e rotação em graus. Mistura por altura tem contraste, largura, influência e três qualidades de amostragem. Camadas desativadas e projeções inúteis do modo UV evitam trabalho sem reduzir a qualidade. Superfícies PBR não executam primeiro a avaliação legada que seria descartada.

## Editor e assets

**Pintar → Biblioteca/Camada/Mapas e canais/Pincel** organiza seleção e propriedades. Ajustes contínuos usam barras e entrada precisa. Controles dependentes só ficam ativos quando têm consumidor; mapas normais permanecem lineares. Ícones reutilizam os conceitos existentes no atlas Astra. A biblioteca usa uma grade com miniaturas reais dos 82 materiais, faixa compacta de quatro canais e navegação de uma linha. O atlas e o enum foram regenerados juntos: 479 entradas. O alfa empacotado do albedo é tratado como opaco somente na prévia UI derivada; os 58 PNGs de miniaturas utilizados e as 146 texturas runtime permanecem intactos. Busca e filtros ainda não foram implementados.

O preparo do APK inclui catálogo, texturas e miniaturas originais, com leitor Android via AAssetManager. PSDs de origem ficam no repositório. Isso é integração de empacotamento, não prova de APK distribuído. Alterações de cor/escala/fatores reutilizam pixels decodificados imutáveis quando as referências e os espaços de cor não mudam; gestos de geometria também reutilizam o material. O histórico e a reabertura transportam os PNGs dentro da fonte GLB sem depender da pasta externa do catálogo. Uma sessão nova reconstrói a biblioteca por registro/fontes e confirma o PBR da revisão visível. Reaplicar um hash já existente republica a biblioteca canônica antes de resolver os slots; prévias temporárias não ficam incorporadas aos índices dos chunks.

## Qualidade, limites e integridade

Nenhuma redução destrutiva dos assets. O nível original e a cadeia completa de mips são conservados. Falta de orçamento produz diagnóstico e recusa de publicação; o estado aplicado é preservado. Mapas PBR ficam fixos em resolução integral enquanto suas projeções não participam do planejador de streaming. O seletor opcional de redução próprio desses mapas ainda não existe. O preparo de materiais conserva os limites atuais de memória; não há garantia de todos os materiais 4K simultâneos nem medição Android.

PNGs são validados estruturalmente e por CRC antes de decodificar, exportar ou ler/gravar o cache. Imagens codificadas e mips compartilhados são contabilizados sem duplicar o mesmo bloco retido entre fontes. O renderer recusa falta de descritores para materiais PBR, em vez de trocar silenciosamente os mapas por branco.

## Persistência e renderer

- Receita AETERRAIN8; leitores 1–8 preservados. AETERRAINSURFACE1 conserva todos os campos do contrato.
- GLB astraTerrainSurface 3 com surfaceRecipe, imagens originais por GUID e escala local; controle v2 continua transportado.
- Cache revisão 9, seção opcional ASTRATS1; derivados anteriores são reconstruídos.
- Tabela std430 própria no binding 18: cabeçalho de duas linhas, 40 por camada. A extensão do material permanece com 15 linhas; linha 14.z aponta para a tabela.
- Publicação e deduplicação remapeiam todos os índices. A prévia de câmera reserva o descritor adicional. Limite de tabela 4 MiB e referências de material até 16.384 entradas.

## Referências oficiais e escopo restante

Unity **6000.0**: [Terrain Layers](https://docs.unity3d.com/6000.0/Documentation/Manual/class-TerrainLayer.html) e [TerrainLayer](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainLayer.html), consultadas nesta revisão. Referências de capacidade para separação de camada, textura/normal/máscara e transformação em metros; implementação e identidade visual próprias da Astra.

M01 conserva 32 camadas no contrato, mas o controle e o shader atuais usam quatro. M02–M04 têm consumidores de canais PBR, altura e projeção neste incremento, sujeitos ao aceite completo. M05–M08 (ruídos, anti-tile, resampling, clusters) conservam dados, sem seus consumidores completos no shader. M09–M18 (estratos, composição global, neve e demais módulos), vegetação, grafos e demais itens seguem abertos. Não há paridade de plugins declarada.

## Validação

- **102/102 cenários integrados**: build e execução em `terrain-pbr-thumbnails-final.log`; repetição com o atlas final 2048×2452 em `terrain-pbr-atlas-acceptance.log`. Incluem seleção/toque, barras versus rolagem vertical, propriedade no PBR visível após Undo/Redo, sessão nova com restauração de registro/fontes e biblioteca externa indisponível. A prova de cache/GLB também conserva canais e PNGs; nenhum span transitório é anunciado como persistente.
- Duas asserções condicionais de symlink não executaram porque o Windows recusou o privilégio. As demais verificações de corrupção/recuperação foram executadas.
- **30 modos GPU reais**: `terrain-pbr-gpu-final.log`, NVIDIA RTX 3050, leitura de pixels após serializar/desserializar a autoria e usar publicador/tabela/GLSL reais. Incluem canais/remaps, normais RGB/RG/força zero, tint, altura, projeções, escala/offset em metros e compatibilidade. A tabela do harness tem dez descritores fixos; não prova o alocador completo, iluminação final, desempenho Android ou cena em aparelho.
- Todos os shaders embutidos regenerados e validados por SPIR-V: `terrain-pbr-shaders.log`.
- UI executável/rasterizada: seis capturas `pbr-final-{library,layer,maps}-{1280,655}.png`, com dimensões 1280×720 e 655×300; controles alcançáveis e rodapé fixo conferidos. Viewport vazio é fixture UI, não prova de uma cena 3D. A proposta `terrain-pbr-proposal.png` foi gerada a partir das capturas iniciais; busca/filtros e outros detalhes conceituais não foram implementados.
- **16/16 unidades Android ARM64**, incluindo sessão/publicação, assets, shell e renderer: `terrain-pbr-android.log`. Compilação sintática; não é link, APK ou runtime.
- **146 PNGs runtime, 302.147.871 bytes**, hashes/CRC/dimensões originais conferidos: `terrain-pbr-assets.json`. São 37 arquivos em textures e 109 PNGs em sources. Miniaturas originais utilizadas não foram alteradas. Atlas final tem 479 entradas de 96 px em 2048×2452, sem eixo acima de 4096; apenas a prévia UI derivada força alfa opaco.

Logs e capturas em `build/terrain-evidence/`. O APK 110 permanece anterior a este código; nenhum ADB foi usado. Publicação do checkpoint no site/branch deve ser registrada abaixo após confirmação remota.


## Publicação confirmada

Engine `ebd31aec` publicada em `codex/terrain-voxel-generator`, sem merge na main. AstraDocs `5f1029ea9c790ba001c67a6c9a41da08e61cc436` publicada na main; nota remota de exposição preservada durante a integração. Vercel `dpl_D2hCpAUknMfJhLwie63EsbHRGnzY` READY, alias astraengine.com.br confirmado. Guia HTML, Markdown, feed JSON e Atualizações responderam HTTP 200 com o conteúdo desta revisão. Build local: 2437 páginas; verificador: 2436 páginas, 594931 links, zero erros. Revisão em desktop e largura 390 px sem overflow DOM. Sem APK novo.

Continuação M05 em implementação após este checkpoint; suas mudanças locais não fazem parte da evidência 102/30/16 acima.
