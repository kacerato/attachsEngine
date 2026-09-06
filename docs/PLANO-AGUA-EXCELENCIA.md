# Água global — execução por dependências

Estado: em execução, não representa paridade concluída com as referências.
Validação Android somente após consulta e autorização do usuário.

## 1. Contrato compartilhado de água
1.1 Auditar WaterField, perfil, espelho espectral e consumidores físicos existentes.
1.2 Correntes uniformes e locais combináveis, com unidades, limites e testes.
1.3 Bathymetry amostrada, domínio finito, prioridade entre volumes e exclusões.
1.4 Unificar tempo, orientação e cascatas das consultas com a superfície desenhada.
Aceite: consultas em lote determinísticas; sem GPU readback síncrono ou alocação
por consulta; nenhuma afirmação de equivalência CPU/GPU sem teste numérico.

## 2. Física reutilizável
2.1 Preservar volume submerso exato de caixas/esferas e integração Jolt existente.
2.2 Corpos compostos e pontos de aplicação; arrasto direcional e torque por casco.
2.3 Correntes, velocidade orbital, massa adicionada e limites por timestep fixo.
2.4 Eventos de entrada/saída, deslocamento, esteira e impulso de impacto.
2.5 Testes de equilíbrio, dissipação, estabilidade 30/60/120 e repouso energético.
Aceite: força não depende do FPS; repouso não mantém corpos acordados sem motivo;
aproximações do casco são explícitas, sem classe de barco no núcleo.

## 3. Renderização e óptica
3.1 Validar cadeia de normais espectrais filtradas, formatos e orçamento mobile.
3.2 Mips/variância de inclinação e AA especular, sem apagar ondas geométricas.
3.3 Scene color e água no mesmo HDR; refração RGB com absorção por distância.
3.4 Céu/reflexo/exposição coerentes; eliminar faixa do horizonte pela causa.
3.5 Reflexos ambiente/SSR/planar por capacidade, resolução e cadência independentes.
Aceite: sem quadrados, costuras ou clareamento artificial; cada passe mensurável;
fallback explícito, nunca selecionar um modo ainda não implementado.

## 4. Interação, costa e volume
4.1 Campo local de ondas com timestep fixo, fronteiras absorventes e orçamento.
4.2 Fontes genéricas: toque, chuva, impactos e corpos; entrada limitada por energia.
4.3 Espuma de quebra, costa e esteira; transporte e dissipação configuráveis.
4.4 Spray com emissão compactada; limites de partículas e distância próprios.
4.5 Câmera submersa, transição na linha d'água, exclusão de interiores e caustics.
Aceite: desligar módulo libera trabalho/recursos; mar não é carregado na floresta.

## 5. Authoring e painel
5.1 Inventário de propriedades: unidades, intervalos, default e custo/rebuild.
5.2 Recursos versionados; round-trip e migração; identidade estável dos volumes.
5.3 Mesma API para painel, runtime e futuros Inspector/script/NoCode.
5.4 Painel touch por categorias: espectro/cascatas, óptica, física, interação,
volumes, efeitos e diagnóstico; edição transacional e restauração de valores.
5.5 Diferenciar solicitado/resolvido; não apresentar controle sem efeito real.

## 6. Integração e provas
6.1 Testes nativos, ferramentas, SPIR-V e build Android sem usar o telefone.
6.2 Demo com rasos/profundos, correntes e corpos de densidades/geometrias distintas.
6.3 Consultar usuário antes de qualquer instalação/captura/teste no aparelho.
6.4 Depois da autorização: regressão floresta, lifecycle, movimento e soak térmico;
publicar FPS, CPU/GPU p95, escala efetiva, memória e custo individual dos módulos.
Não prometer 120 FPS nativos a partir de resultados com render scale reduzido.

## Referências e limites
- https://github.com/krautdev/GodotOceanWaves: fork com consultas de altura;
  README registra limitações de readback e alternativas de buoyancy. Não tratar
  propostas no README como funcionalidades prontas a transplantar.
- Projeto local KWS: referência para flowmaps, costa, reflexos, volume e painéis;
  integrar conceitos na arquitetura Aether, não importar dependências Unity.
- https://www.youtube.com/watch?v=8_NSuJBELus: acesso falhou nesta análise;
  conteúdo audiovisual ainda não verificado.

## Primeiro incremento desta execução
Correntes no WaterField: uniforme, radial e vórtice, soma limitada e transição
suave de alcance. Alimentam o resultado de consulta; advecção visual e controles
de authoring ainda precisam ser conectados. Não equivalem a um solver de fluidos.

Contrato atual: até 16 fontes copiadas por configuração, nenhuma alocação em
consulta. Velocidade uniforme e escalas locais em m/s; raio em metros; direção
unitária para fonte direcional. Radial/vórtice usam núcleo linear e envelope
smoothstep no alcance: `speed` é escala, não velocidade máxima do vórtice.
O limite de velocidade atua depois da soma, preservando sua direção. A física
deve somar `flow` à velocidade orbital uma única vez. A configuração ainda é
nativa: persistência, painel e advecção visual não foram implementados nesta etapa.

## Incremento seguinte — campo espacial e ligação física
- Branch `codex/render-rebuild` avançada ao histórico já existente de
  `claude/render-rebuild`; alterações não commitadas preservadas.
- Tile de bathymetry até 32x32, copiado na configuração; interpolação bilinear.
  Fora do tile a profundidade é desconhecida, salvo fallback plano explícito.
- Domínio de consulta finito circular/retangular rotacionado e exclusões
  validadas. Fundo acima do nível de repouso marca terra seca; inundação por
  ondas/shoreline dinâmica ainda não é simulada.
- Adaptador nativo `makeWaterBodySample` entrega plano e velocidade à entrada
  existente de forças Jolt, rejeitando consultas inválidas/secas. Teste cobre
  consulta -> conversão -> força de arrasto. Agendamento runtime e authoring
  desse adaptador continuam pendentes.
- Caminho interno não revalida fontes/grade por ponto: valida uma vez ao
  configurar; API pública avulsa permanece defensiva. Sem alocações por consulta.
- Domínio/bathymetry ainda não modificam a malha ou o shader. Não há paridade
  visual/física completa até integração do mesmo domínio com o renderer.

## Rodada ampliada — volumes, runtime e gráficos

Implementados nesta rodada (contratos em `water-world-runtime.md`):

- WaterWorld: 16 volumes por valor, ID estável fornecido pela cena, camadas,
  prioridade e desempate determinístico; remoção e edição transacional.
- WaterRuntime: 128 registros, consulta/forças em lote, relógio monotônico,
  histerese de contato e eventos de entrada/saída; módulo separado de integração.
- Leitura segura de pose do corpo com retorno de erro, sob lock, e controle
  explícito de permissão de sono; sem alterar comportamento default de repouso.
- WaterField preserva o campo anterior ao rejeitar edições; consultas inválidas
  não escrevem resultados parciais. Status lê o tempo corrente do espelho.
- Filtro especular e largura da espuma de contato ligados do painel ao shader,
  sem passes/texturas adicionais, com desligamento independente.
- Alvo host compartilhado inclui o código de buoyancy usado pela fronteira Jolt.

Integração física validada por testes com corpos Jolt, não por mocks; o demo
Android ainda não registra corpos nesse módulo. Não declarar multiágua visual,
serialização completa, HDR/refração ou equivalência CPU/GPU concluídos. Aparelho
continua reservado para depois de consulta ao usuário.

## Agendador de simulação

- WaterSimulation conecta consultas/forças e StepV2; timestep fixo, pausa,
  time scale e limites de recuperação configuráveis, sem alocação por frame.
- Eventos entregues por subpasso; provedor pode recusar atualização antes de
  qualquer força; falha do solver preserva sua máscara e impede repetição.
- Testes de relógio independente do FPS, câmera lenta, pausa, delta inválido,
  orçamento de catch-up e recuperação de provedor com Jolt real.
- Corpos visíveis no demo continuam pendentes: o caminho de mapa ainda não
  oferece atualização dinâmica atômica de matriz/bounds/culling/sombras.
