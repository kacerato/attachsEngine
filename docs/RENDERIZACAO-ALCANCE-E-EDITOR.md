# Alcance visual, detalhe e configuração do editor

## Contrato e integração existente

A configuração global pertence a `ProjectRenderingSettings` e é resolvida em
`ResolvedRenderingPolicy`. O editor deve editar esse contrato, através de comandos
reversíveis, e publicar uma nova época de configuração entre frames. Ele não deve
editar recursos Vulkan nem usar nomes de mapas como decisões do renderer.

Floresta e oceano compartilham a política. Alcance de visibilidade não é sinônimo
de densidade geométrica: uma árvore distante deve conservar cobertura, silhueta,
cor e iluminação mesmo quando folhas individuais deixam de ocupar um pixel.

O painel Android dos dois mapas já expõe erro geométrico, erro de vegetação e
margem de transição LOD. Zero herda o projeto. Esses overrides passam pelo
resolver existente antes de chegar ao renderer; são persistidos localmente em
`render_visibility_v1`. Não alteram o plano distante da câmera. Valores fora dos
limites são tratados pelo resolver e não equivalem à garantia de FPS.

| Etapa | Eixos existentes | Critério visual |
| --- | --- | --- |
| Geometria sólida | `lodPixelErrorBudget`, `lodHysteresisBandRatio` | Erro em pixels finais; aproximação refina prontamente |
| Vegetação | `coverageLodPixelErrorBudget` | Cobertura e silhueta; transição complementar entre LODs |
| Materiais | alcances de normal, metal/rugosidade, emissivo e probe | Transição pela `materialDetailFadeBandRatio` |
| Sombras | alcance, cascatas, resolução, taps e bandas de transição | Continuidade de contato próximo e iluminação distante |
| Água | `waterMesh`, espectro por bandas, micro-relevo e espuma | Geometria e detalhe de superfície usam resoluções diferentes |
| Resolução | escala, DRS, orçamento e AA | DRS não altera o erro LOD medido em pixels finais |

## Próximas integrações necessárias

1. Expor os eixos existentes em um catálogo de propriedades com ID estável,
   unidade, intervalo, valor padrão, custo e requisitos de capability. Gerar a
   interface do editor a partir dele e reutilizar a serialização da política.
2. Para a floresta, validar trajetórias de aproximação e afastamento nos limites
   de cada LOD e impostor, com câmera/FOV/resolução fixos. Não encurtar o horizonte
   para atingir o orçamento. Medir cobertura e continuidade de sombras.
3. Para o oceano, tratar separadamente deslocamento, normal, espuma persistente,
   reflexão, contato e esteira. Desligar um efeito deve preservar as demais
   camadas. A grade local de interação não define o alcance do mar.
4. Registrar pedido, valor resolvido e motivo de fallback no Inspector/profiler.
   O painel Android atual é uma integração de demonstração, não o Inspector final.

## Correção do recorte de espuma

A espuma FFT era interpolada dos vértices e multiplicada pelo filtro de geometria.
Isso imprimia o contorno da grade de menor densidade no material. Agora o fragmento
consulta o campo de espuma por cascata em coordenadas espectrais e filtra pela
pegada do pixel. O deslocamento continua filtrado pela malha. Buffers são expostos
aos dois estágios e o requisito de cinco storage buffers por estágio é verificado.

Essa correção exige validação visual em movimento e medição de custo: a leitura
bilinear da espuma por fragmento acrescenta acessos. Ainda falta uma representação
de espuma com mipmaps para conservar sua cobertura média a distâncias extremas;
o filtro atual reduz frequências não resolvidas e não encerra esse trabalho.

## Incremento naval e limites

O espelho físico valida ganhos compostos: deslocamento por cascata até 10 ×
controle global até 3, choppiness por cascata até 4 × global até 2. Validar apenas
o limite global recusava combinações legais da GPU e interrompia a simulação.
Há regressão cobrindo inicialização e consulta do conjunto nos limites compostos.

O barco do exemplo foi ampliado uniformemente 15 vezes, incluindo o proxy físico,
área de arrasto, bounds e enquadramento. `WaterWakeEmitter` injeta fontes por
distância percorrida, com força, velocidade mínima, espaçamento, largura e teto
de impulso independentes. Corpos secos não emitem esteira. O estado pertence ao
dono da simulação; atualizações não alocam memória.

Micro-relevo geométrico e elevação visual da espuma são eixos de
`WaterShadingSettings`. Seus limites entram nos bounds conservadores, inclusive
o ganho máximo de micro-ondas. A elevação da espuma não altera volume de empuxo.
Preferências do laboratório persistem localmente no Android; não representam
recursos portáveis do projeto nem possuem undo/redo do futuro editor.

Casco composto com forças distribuídas, refração espacial, subaquático, espuma
com mips, transições da área de interação e horizonte atmosférico permanecem
itens de refinamento. Não há declaração de paridade visual ou de FPS com PUBG.
