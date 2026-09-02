# ADR-016 — Compute como capacidade do RHI e do Render Graph

- **Estado:** aceita e implementada na fundação
- **Data:** 01/09/2026

## Problema

Compute era usado por um probe ASTC isolado, com criação manual de pipeline,
descriptors, command pool e fila. Repetir esse padrão para HZB, Forward+,
culling, skinning e partículas criaria APIs diferentes, sincronização frágil e
dependência direta de Vulkan em cada sistema.

## Decisão

Compute é uma capacidade transversal do RHI. Um kernel declara seu contrato de
shader e recursos; a reflexão SPIR-V valida esse contrato; um contexto reutiliza
command buffer/fence e compõe submissões por semáforos. O Render Graph declara o
tipo do passe, os acessos e a preferência por async compute, ficando responsável
por derivar barreiras e ownership de filas.

O backend Vulkan prefere uma família compute-only quando disponível e usa a
família gráfica como fallback. Consumidores não podem assumir async compute.
Dispatch indireto exige usage apropriado e recursos precisam estar totalmente
vinculados antes de despachar.

## Alternativas rejeitadas

- Manter wrappers específicos por efeito: duplica lifecycle, descriptors e
  sincronização e torna profiling/compatibilidade inconsistentes.
- Executar sempre na fila gráfica: é um fallback válido, mas impediria explorar
  sobreposição em aparelhos que expõem fila dedicada.
- Forçar fila dedicada: quebra dispositivos sem essa topologia e não é
  mobile-first.
- Esperar a fila ficar ociosa após cada dispatch: simplifica o probe, porém
  serializa o frame inteiro e inviabiliza composição no Render Graph.

## Consequências

HZB, culling, Forward+, skinning, partículas e ferramentas compartilham o mesmo
contrato, capability gate e instrumentação. A implementação ganha complexidade
explícita de ownership entre famílias. O grafo ainda precisa evoluir para um
executor integral de passes e para `synchronization2`; a fundação atual usa
barreiras Vulkan 1.1 compatíveis.

## Validação

- testes host de reflexão, contrato, dispatch e compilação/mapeamento do grafo;
- shader generation + `spirv-val`;
- builds Android Debug e Release;
- probe ASTC 4096 x 4096 em Adreno, incluindo transferência compute -> graphics.

