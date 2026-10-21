# Viewport, Play e Player — revisão obrigatória de 21/10/2026

Este pacote complementa o plano de 71 itens; não substitui itens nem reduz propriedades.
Estado: implementação local em andamento, sem aceite visual ou ganho de FPS confirmado.

## Contrato de execução

| Contexto | Autoridade de qualidade | Relógio | Exportação |
|---|---|---|---|
| Editor / autoria | preferência local Baixo, Médio, Alto; teto30FPS | simulação congelada; prévia de materiais opcional | preferências excluídas |
| Play no editor | configurações gráficas do jogo; teto50FPS | simulação da sessão Play | mesmos eixos e recursos do jogo |
| Player / APK do jogo | `.astra/rendering.astra`; teto50FPS | execução do jogo | mantém overrides gráficos; teto temporário50FPS |

Compartilhar código, caches imutáveis e assets é desejável. Compartilhar a escolha de
qualidade e o relógio de simulação entre autoria e jogo não é. Não serão mantidos
três mundos simultaneamente em execução apenas para produzir três visualizações.
A política resolvida é imutável por configuração; a troca ocorre no ponto seguro de
reconstrução já existente. Play/Stop devem invalidar o conteúdo temporal e devolver
a câmera, a cena e as preferências ao seu dono.

A primeira revisão local usa escala interna de 50%, 75% e 100% nos presets do editor,
com texturas originais completas em todos eles. Não reimporta nem diminui os assets.
Autoria não solicita reconstrução temporal Arm ASR/FSR2/TAA; o painel do jogo conserva
essas escolhas e FXAA, sujeitos às capacidades reais do backend. O desenho da UI
continua na resolução da tela. O custo do material também precisa ser corrigido:
baixar a escala de autoria não constitui otimização do Player.

## Evidência do problema

- O painel Qualidade gravava o projeto e alterava imediatamente o renderer editorial.
- Materiais de terreno recebiam o relógio congelado da cena fora do Play.
- Câmera em movimento no APK 111 anterior: 4,71 FPS e 3,59 FPS em dois percursos;
  pausas máximas de 2,03 s e 2,05 s. Apresentações em repouso reutilizavam a cena e
  não mediam o custo de redesenhá-la.
- Profiler: passe opaco chegou a 1,41 s; CPU e submit não explicam esse custo.
- Primeiro candidato de shader continuou próximo de 5 FPS. Não aprovado.

A otimização local subsequente só omite camadas quando prova peso zero em TODOS os
texels da máscara e TODAS as cores de vértices referenciadas pelo material, incluindo
laterais, base e voxels. Um byte não zero mantém a camada. Conteúdo desconhecido ou
inválido desliga essa otimização. Não modifica a receita, a biblioteca ou as texturas;
uma nova pintura restaura a camada na publicação seguinte.

## Referências e comparação visual

- Unity **6000.0**, [Scene view / View Options](https://docs.unity3d.com/6000.0/Documentation/Manual/ViewModes.html):
  efeitos locais e Always Refresh para materiais animados.
- Unity **6000.0**, [Game view](https://docs.unity3d.com/6000.0/Documentation/Manual/GameView.html):
  vista das câmeras do jogo, distinta da ferramenta de autoria.
- Unity **6000.0**, [Quality Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/class-QualitySettings.html):
  escolhas de qualidade do jogo e plataforma.
- Jason Booth, [MicroSplat — Puddles, Streams, Lava, Wetness module](https://www.youtube.com/watch?v=8nOYoPoqRec),
  publicado em 19/08/2017, **Unity 5.6.1p2** visível na gravação. Referência visual
  histórica do autor; não é evidência de paridade com MicroSplat 3.9.41 ou Unity 6.
  Frames efetivamente inspecionados: 205,693 s, 205,727 s, 411,387 s, 617,080 s,
  617,113 s, 622,113 s e 1028,467 s. O frame imediato após um seek pode ainda ser o anterior; só os frames
  decodificados foram utilizados. Não foi analisado o vídeo inteiro.

O frame de 617 s mostra rocha molhada, resposta especular e relevo sob água; o de
1028 s permite observar a distribuição no relevo e controles de resistência/velocidade.
A máscara da Astra não é aceita como água apenas por mudar a cor. O aceite exige
bordas, transição de umidade, normais em movimento, reflexo/refração coerentes,
controle de profundidade/espuma onde o módulo o prevê e persistência das propriedades.
Registrar comparação na mesma escala, iluminação e câmera, incluindo o movimento.

## Portões de aceite ainda abertos

1. Compilar e testar isolamento de configuração, persistência e exportação; exercer
   Editor → Play → Stop e reabertura, sem vazamento de preferências nem scripts.
2. Capturar a UI executável nos três presets; julgar toque, hierarquia e área útil;
   refinar se inadequada. Proposta conceitual não vale como captura de implementação.
3. Medir GPU e câmera em movimento no editor e no Play; medir APK exportado separado.
   Informar p95, maior pausa, resolução, efeitos, dispositivo e temperatura.
4. Corrigir e comparar água, poças e lava com os frames do autor; só aceitar depois
   da validação visual/temporal e de Undo, Cancelar, Aplicar e reabrir no aparelho.
5. Publicar a revisão real na aba Atualizações do site após a janela de validação
   autorizada; depois continuar o restante do plano sem ADB até sua conclusão.

## Revisão local subsequente, ainda sem aceite móvel

- 46/46 testes da política gráfica: arquivos do jogo preservados integralmente,
  três escolhas de autoria, temporal fora da autoria, restauração tardia de scripts.
- 237/237 testes terreno/editor: controles reais roteados por toque, preferências
  persistidas/reabertas e ausência de pedido de alteração gráfica do projeto.
- Shader reorganizado para guardar quatro alturas escalares e acumular somente um
  material completo de cada vez; noise/global mantêm todos os seus efeitos, refração
  conserva cor/altura originais e não introduz branches por cobertura nas derivadas.
  Os312 modos GPU passaram; isto prova as verificações de pixels cobertas, não FPS Android.
- Início automático do Play e retorno após falha de publicação revalidam o contexto
  antes de executar/desenhar o primeiro frame. Restauração de um mundo encerrado é
  consumida sem criar outra configuração gráfica de autoria.
- Pipelines de cena passam a usar o cache de compilação já pertencente ao VkDevice;
  os handles continuam no renderer. Sem duplicar mundo ou renderer. Ganho de troca de
  modo e custo no dispositivo ainda precisam ser medidos.
- Experimento Java de tela de carregamento retirado: a captura real continuou preta;
  não entregar um conceito ou um build aprovado como prova de que a tela funciona.
- Compilação geral host encontrou cache antigo com headers CRT Android expostos ao
  compilador Windows. O cache local foi corrigido para headers Vulkan isolados; a
  família focada de política gráfica compilou e passou. Não declarar suíte geral
  aether_tests aprovada, pois ela não foi concluída nesta passagem.

## Execução e revisão de interface subsequentes

- 239/239 cenários terreno/editor e46/46 política passaram. Teste novo usa Canvas
  persistido real, inicia o mundo pelo mesmo `extractPlayMap` do backend e roteia
  Down/Up no Toggle vivo, preservando o documento autoral. Player omite chrome;
  prévia mantém Stop. Atualização de Inspector/miniaturas/buscas fica fora do
  caminho comum de Play/Player; inspeção de Play conserva seu caminho completo.
- `astra.player` determina Player mesmo com o carregador compartilhado de sessão.
  Autocompilação autoral fica fora do Player; carregamento do código do jogo é
  preservado. Ainda precisa de APK exportado instalado e executado separado.
- Gráficos do jogo são salvos para o próximo Play/APK, inclusive se editados no
  Play atual. A UI diferencia configuração salva de execução efetiva, e o rodapé
  identifica Editor/Play/Jogo. ASR salvo não aparece como falha por estar na autoria.
- Raster executável655×300 revelou menu coberto pela navegação inferior. A ordem
  de desenho/roteamento foi corrigida; teste confere os centros de Céu/Névoa/Pós
  em655×300 e1280×720. Captura posterior e aceitação visual ainda em andamento.
- Vídeo oficial Unity `fxJR2pHPyTQ`: quadros78s/208s inspecionados para seleção,
  gizmos e isolamento no Scene View; não é evidência de orçamento ou FPS. A
  referência versionada de capacidades continua a documentação Unity6000.0.
- ADB sem aparelho na última consulta. A suíte host não substitui comparação visual
  móvel de água nem medição de desempenho. Nenhuma nova aceitação foi publicada.

## Ajuste solicitado: 30 FPS na autoria e 50 FPS na execução

Pedido explícito subsequente do proprietário: editor limitado a30FPS para bateria;
Play50FPS, aplicado também ao Player para manter o mesmo contrato. Ainda sem seletor
para alterar esses limites. Overrides gráficos autorais permanecem salvos; apenas o
limite efetivo por contexto é temporariamente fixo. O painel mostra o valor real como
somente leitura, e o popover identifica30FPS. Implementação da política movida para
.cpp para evitar recompilar todo o editor em futuras mudanças de orçamento.

53/53 testes de política/cadência passaram. O contrato antes arredondava50 para30;
agora50 é uma taxa reconhecida. O filtro Android preserva fase ideal entre callbacks:
em120Hz alterna intervalos físicos de2/3VSYNC, mantendo média50, em vez de transformar
cada20ms em25ms/40FPS. Testes cobrem30/50 em60/90/120Hz, pausas longas e ausência de
rajada para recuperar frames perdidos. Física conserva fixed tick independente.

O orçamento ativo agora sincroniza admissão Android, voto da Surface, Swappy opcional
e alvo ADPF na troca de contexto; antes o frameBudget do shell era fixado na abertura.
Isto define um teto, não prova que a GPU consegue alcançá-lo.

## Latência: espera de GPU antes de capturar a cena

A árvore usa uma única fence de frame. O caminho anterior capturava câmera/cena e
então esperava essa fence infinitamente na mesma thread do ALooper/input. O novo
poll de fence ocorre antes do snapshot; acquire usa timeout zero. NotReady devolve
controle ao ALooper sem reset de fence, índice adquirido, contagem de frame ou
recriação. O acquire confirma a readiness de novo; reset só após sucesso/suboptimal.

6/6 testes com PFNs instrumentadas passaram: fence ocupada, compositor sem imagem,
retomada/recriação, erro de dispositivo, suboptimal, argumentos inválidos e retry.
O handler não perde eventos do pincel. Instrumentação adicional apenas no profiling
registra idade do MOVE ao despacho/snapshot e retorno do present. Esse retorno é
fila de apresentação, não prova de instante visível na tela. Present/Swappy e uploads
explícitos ainda podem bloquear e precisam de medição separada.

Referências primárias das APIs Vulkan1.0+:
- https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html
- https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
- https://docs.vulkan.org/spec/latest/chapters/synchronization.html

## Seleção de pipelines por capacidades presentes

Owner real no InstancedRenderer especializa a união conservadora dos módulos de
TODAS as tabelas publicadas. Não elimina um recurso por intensidade/um pixel/um peso
momentaneamente zero. Tabela desconhecida usa genérico completo. Mantém famílias,
constantes anteriores, ordenação de transparência, coverage e lotes indiretos.
Receitas e módulos retidos têm ciclo de vida explícito; falha de publicação retorna
ao genérico; mesmo conjunto de recursos reutiliza resultados positivos e negativos.

10/10 testes nativos passaram. Teste Vulkan real dos312 modos originais, com variantes
especializadas e fallback desconhecido comparados por readback, passou em238,25s;
44 famílias SPIR-V passaram. Nenhum ganho Android é inferido desses números. O primeiro
conjunto novo pode compilar até aproximadamente50 pipelines; log de profiling mede
esse custo. Assets/propriedades continuam completos. Integração Control32/Dynamic
futura exige nova prova de ABI e permanece explicitamente genérica até lá.

APK privado111 `.20261021.3` em compilação, ainda NÃO instalado/testado. Revisões
intermediárias canceladas não contam como entregas. A/B móvel, toque, água e Player
exportado continuam pendentes; a janela ADB excepcional segue aberta.
