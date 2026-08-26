# Propagação de transforms

## Responsabilidades

`TransformPropagationPlanCache` pertence ao runtime gerenciado. Ele descobre entidades com
`LocalTransform` + `WorldTransform`, resolve `Parent`, quebra ciclos inválidos de forma
determinística e compila uma ordem topológica. O plano é reconstruído quando muda:

- a estrutura do `World` (criação, destruição ou migração de arquétipo);
- a versão de hierarquia (`Parent`, `FirstChild` ou `NextSibling`);
- a dirty version de qualquer coluna `Parent`, cobrindo também escrita direta por span.

`World` continua sendo a fonte de verdade. O nativo não cria entidades, não guarda handles do ECS
e não decide parentesco.

## Memória e fronteira nativa

Buffers de `Chunk` são arrays de longa duração alocados no Pinned Object Heap. O plano mantém os
`Chunk`s vivos e compila um array blittable com endereço de local, endereço de world e índice do
pai. Em cada frame, `AetherTransform_PropagatePlan` recebe esse array em uma única chamada
síncrona e escreve diretamente os `WorldTransform`s em ordem topológica.

A ABI está em `native/transform/transform_bridge.h`. `AetherTransform` possui 40 bytes e o layout é
verificado por `static_assert`; somente POD cru atravessa P/Invoke. Não há chamada nem cópia por
entidade. O plano não pode ser executado simultaneamente com alterações estruturais no mesmo
`World`; essa é a mesma regra de sincronização já exigida por qualquer iteração de chunks.

Se `aether_transform` não existir, tiver arquitetura incompatível ou não exportar a função, a
detecção ocorre uma vez e `TransformSystem` usa o algoritmo C# equivalente. O backend da última
execução fica exposto em `TransformSystem.ActiveBackend` para profiling.

## Build e validação

O alvo CMake `aether_transform` produz `aether_transform.dll` no Windows e
`libaether_transform.so` no Android/Linux. O Gradle inclui o alvo no APK ARM64.

O benchmark canônico é
`HierarchyTests.Benchmark_CemMilTransformsEmHierarquiaRealista_SemGcEComPercentis`: árvore de
100 mil entidades, fator 8, profundidade 7, animação de todos os locais, 60 amostras e zero
alocação por frame. Resultados e ressalvas de DVFS ficam registrados em `ESTADO.md`; `taskset`
restringe afinidade, mas não trava a frequência da CPU.
