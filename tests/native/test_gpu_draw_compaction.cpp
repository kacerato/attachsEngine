#include "harness.h"
#include "renderer/gpu_draw_compaction.h"

#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {

// Simulação fiel do que draw_compact.comp faz: varredura em blocos de
// kGpuCompactionGroupSize com soma de prefixo exclusiva por bloco e um
// acumulador costurando os blocos.
//
// Ela existe porque a referência de CPU percorre o lote sequencialmente e, por
// isso, não passa por nenhuma fronteira de bloco. O risco real do kernel está
// exatamente aí: um acumulador atualizado cedo demais, ou uma barreira faltando,
// produz uma lista que continua com a contagem certa e a ordem errada. Comparar
// as duas implementações prova que a decomposição em blocos é equivalente à
// compactação sequencial — que é a afirmação que o shader faz sobre si mesmo.
bool simulateBlockwiseCompaction(const std::vector<u32> &instanceCounts,
                                 const std::vector<GpuCompactBatch> &batches,
                                 std::vector<u32> &outSourceIndices,
                                 std::vector<u32> &outCounts) {
  const u32 group = kGpuCompactionGroupSize;
  for (usize batchIndex = 0; batchIndex < batches.size(); ++batchIndex) {
    const GpuCompactBatch &batch = batches[batchIndex];
    u32 emitted = 0;
    for (u32 blockStart = 0; blockStart < batch.commandCount; blockStart += group) {
      std::vector<u32> scan(group, 0);
      for (u32 lane = 0; lane < group; ++lane) {
        const u32 offset = blockStart + lane;
        const bool withinBatch = offset < batch.commandCount;
        scan[lane] = (withinBatch && instanceCounts[batch.firstCommand + offset] != 0) ? 1u : 0u;
      }
      // Hillis-Steele inclusivo, na mesma ordem de leitura e escrita do kernel:
      // toda a linha lê antes de qualquer um escrever, que é o papel das duas
      // barreiras por passo.
      for (u32 stride = 1; stride < group; stride <<= 1) {
        std::vector<u32> carried(group, 0);
        for (u32 lane = stride; lane < group; ++lane) carried[lane] = scan[lane - stride];
        for (u32 lane = stride; lane < group; ++lane) scan[lane] += carried[lane];
      }
      for (u32 lane = 0; lane < group; ++lane) {
        const u32 offset = blockStart + lane;
        if (offset >= batch.commandCount) continue;
        const u32 sourceIndex = batch.firstCommand + offset;
        if (instanceCounts[sourceIndex] == 0) continue;
        const u32 flag = 1;
        const u32 exclusive = scan[lane] - flag;
        const u32 destination = batch.compactedBase + emitted + exclusive;
        if (destination >= outSourceIndices.size()) return false;
        outSourceIndices[destination] = sourceIndex;
      }
      emitted += scan[group - 1];
    }
    outCounts[batchIndex] = emitted;
  }
  return true;
}

GpuCompactBatch batchOf(u32 firstCommand, u32 commandCount, u32 compactedBase) {
  GpuCompactBatch batch{};
  batch.firstCommand = firstCommand;
  batch.commandCount = commandCount;
  batch.compactedBase = compactedBase;
  return batch;
}

// Sentinela distinta de qualquer índice válido: uma posição que a compactação
// não deveria tocar tem de continuar com ela.
constexpr u32 kUntouched = 0xdeadbeefu;

} // namespace

AE_TEST(compaction_removes_holes_and_preserves_order) {
  const std::vector<u32> instanceCounts{1, 0, 1, 1, 0, 0, 1};
  const std::vector<GpuCompactBatch> batches{batchOf(0, 7, 0)};
  std::vector<u32> indices(7, kUntouched);
  std::vector<u32> counts(1, kUntouched);

  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, batches, indices, counts),
                 "a compactacao devia aceitar um lote que cabe na lista");
  AE_EXPECT_EQ(counts[0], 4u, "quatro comandos sobreviveram");
  AE_EXPECT_EQ(indices[0], 0u, "primeiro sobrevivente e o comando 0");
  AE_EXPECT_EQ(indices[1], 2u, "segundo sobrevivente e o comando 2");
  AE_EXPECT_EQ(indices[2], 3u, "terceiro sobrevivente e o comando 3");
  AE_EXPECT_EQ(indices[3], 6u, "quarto sobrevivente e o comando 6");
  // Além da contagem, nada foi escrito: o consumidor lê apenas `counts[0]`
  // entradas, e sujar o resto esconderia um erro de contagem.
  AE_EXPECT_EQ(indices[4], kUntouched, "a cauda alem da contagem nao e escrita");
  AE_EXPECT_EQ(indices[6], kUntouched, "a cauda alem da contagem nao e escrita");
}

AE_TEST(compaction_of_fully_visible_batch_is_the_identity) {
  const std::vector<u32> instanceCounts(32, 1);
  const std::vector<GpuCompactBatch> batches{batchOf(0, 32, 0)};
  std::vector<u32> indices(32, kUntouched);
  std::vector<u32> counts(1, kUntouched);

  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, batches, indices, counts), "");
  AE_EXPECT_EQ(counts[0], 32u, "nada foi ocluido");
  for (u32 index = 0; index < 32; ++index)
    AE_EXPECT_EQ(indices[index], index, "lote inteiro visivel preserva a lista original");
}

AE_TEST(compaction_of_fully_occluded_batch_publishes_zero) {
  const std::vector<u32> instanceCounts(16, 0);
  const std::vector<GpuCompactBatch> batches{batchOf(0, 16, 0)};
  std::vector<u32> indices(16, kUntouched);
  std::vector<u32> counts(1, kUntouched);

  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, batches, indices, counts), "");
  AE_EXPECT_EQ(counts[0], 0u, "lote inteiro ocluido submete zero comandos");
  AE_EXPECT_EQ(indices[0], kUntouched, "nenhum comando foi copiado");
}

AE_TEST(compaction_of_empty_batch_publishes_zero) {
  // Um material pode ficar sem nenhum draw visível ao frustum neste frame. O
  // lote continua existindo — e o contador dele precisa ser escrito, porque a
  // submissão vai lê-lo de qualquer jeito.
  const std::vector<u32> instanceCounts{1, 1};
  const std::vector<GpuCompactBatch> batches{batchOf(0, 0, 0), batchOf(0, 2, 0)};
  std::vector<u32> indices(2, kUntouched);
  std::vector<u32> counts(2, kUntouched);

  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, batches, indices, counts), "");
  AE_EXPECT_EQ(counts[0], 0u, "lote vazio publica contagem zero");
  AE_EXPECT_EQ(counts[1], 2u, "o lote seguinte nao e afetado pelo vazio");
}

AE_TEST(compaction_keeps_batches_in_their_own_destination) {
  // Três lotes com destinos deslocados: o segundo não pode escrever no espaço
  // do primeiro nem do terceiro, mesmo quando sobra lugar por oclusão.
  const std::vector<u32> instanceCounts{1, 0, 1, /* lote 1 */ 0, 0, 1, /* lote 2 */ 1, 1};
  const std::vector<GpuCompactBatch> batches{batchOf(0, 3, 0), batchOf(3, 3, 3), batchOf(6, 2, 6)};
  std::vector<u32> indices(8, kUntouched);
  std::vector<u32> counts(3, kUntouched);

  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, batches, indices, counts), "");
  AE_EXPECT_EQ(counts[0], 2u, "lote 0 manteve dois");
  AE_EXPECT_EQ(counts[1], 1u, "lote 1 manteve um");
  AE_EXPECT_EQ(counts[2], 2u, "lote 2 manteve dois");
  AE_EXPECT_EQ(indices[0], 0u, "");
  AE_EXPECT_EQ(indices[1], 2u, "");
  AE_EXPECT_EQ(indices[2], kUntouched, "o buraco do lote 0 nao e preenchido pelo lote 1");
  AE_EXPECT_EQ(indices[3], 5u, "lote 1 escreve na propria base");
  AE_EXPECT_EQ(indices[6], 6u, "");
  AE_EXPECT_EQ(indices[7], 7u, "");
}

AE_TEST(compaction_matches_the_blockwise_kernel_across_block_boundaries) {
  // Lote deliberadamente maior que o bloco do kernel, com um padrão de oclusão
  // que cai fora de fase com a fronteira: se o acumulador entre blocos estiver
  // errado, o segundo bloco sobrescreve o primeiro ou deixa um buraco.
  const u32 group = kGpuCompactionGroupSize;
  const u32 commandCount = group * 3 + 7;
  std::vector<u32> instanceCounts(commandCount, 0);
  for (u32 index = 0; index < commandCount; ++index)
    instanceCounts[index] = (index % 3 == 0 || index % 7 == 1) ? 1u : 0u;

  const std::vector<GpuCompactBatch> batches{batchOf(0, commandCount, 0)};
  std::vector<u32> referenceIndices(commandCount, kUntouched);
  std::vector<u32> referenceCounts(1, kUntouched);
  std::vector<u32> kernelIndices(commandCount, kUntouched);
  std::vector<u32> kernelCounts(1, kUntouched);

  AE_EXPECT_TRUE(
      compactDrawCommandsReference(instanceCounts, batches, referenceIndices, referenceCounts), "");
  AE_EXPECT_TRUE(simulateBlockwiseCompaction(instanceCounts, batches, kernelIndices, kernelCounts),
                 "a simulacao do kernel nao pode sair dos limites");
  AE_EXPECT_EQ(kernelCounts[0], referenceCounts[0], "as duas contagens tem de coincidir");
  for (u32 index = 0; index < commandCount; ++index)
    AE_EXPECT_EQ(kernelIndices[index], referenceIndices[index],
                 "o kernel em blocos produz a MESMA lista, na mesma ordem, da referencia");
}

AE_TEST(compaction_matches_the_blockwise_kernel_for_many_batches) {
  // Vários lotes de tamanhos diferentes, alguns menores e outros maiores que o
  // bloco, cada um com sua própria base. É a forma que o frame real tem.
  const u32 group = kGpuCompactionGroupSize;
  const u32 sizes[] = {1, group - 1, group, group + 1, 0, group * 2 + 5, 3};
  std::vector<GpuCompactBatch> batches;
  u32 cursor = 0;
  for (u32 size : sizes) {
    batches.push_back(batchOf(cursor, size, cursor));
    cursor += size;
  }
  std::vector<u32> instanceCounts(cursor, 0);
  for (u32 index = 0; index < cursor; ++index)
    instanceCounts[index] = (index % 5 == 2 || index % 11 == 0) ? 1u : 0u;

  std::vector<u32> referenceIndices(cursor, kUntouched);
  std::vector<u32> referenceCounts(batches.size(), kUntouched);
  std::vector<u32> kernelIndices(cursor, kUntouched);
  std::vector<u32> kernelCounts(batches.size(), kUntouched);

  AE_EXPECT_TRUE(
      compactDrawCommandsReference(instanceCounts, batches, referenceIndices, referenceCounts), "");
  AE_EXPECT_TRUE(simulateBlockwiseCompaction(instanceCounts, batches, kernelIndices, kernelCounts),
                 "");
  for (usize batchIndex = 0; batchIndex < batches.size(); ++batchIndex)
    AE_EXPECT_EQ(kernelCounts[batchIndex], referenceCounts[batchIndex],
                 "contagem por lote tem de coincidir");
  for (u32 index = 0; index < cursor; ++index)
    AE_EXPECT_EQ(kernelIndices[index], referenceIndices[index], "lista compacta tem de coincidir");
}

AE_TEST(compaction_rejects_batches_that_leave_the_source_list) {
  const std::vector<u32> instanceCounts(4, 1);
  std::vector<u32> indices(4, kUntouched);
  std::vector<u32> counts(1, kUntouched);

  const std::vector<GpuCompactBatch> pastEnd{batchOf(2, 3, 0)};
  AE_EXPECT_TRUE(!compactDrawCommandsReference(instanceCounts, pastEnd, indices, counts),
                 "um lote que passa do fim da lista de origem e recusado");
  AE_EXPECT_EQ(counts[0], kUntouched, "recusa nao escreve nada");

  const std::vector<GpuCompactBatch> wrapping{batchOf(0xffffffffu, 2, 0)};
  AE_EXPECT_TRUE(!compactDrawCommandsReference(instanceCounts, wrapping, indices, counts),
                 "a soma que daria a volta em u32 e recusada, nao imitada");
}

AE_TEST(compaction_rejects_batches_that_share_a_destination) {
  const std::vector<u32> instanceCounts(6, 1);
  std::vector<u32> indices(6, kUntouched);
  std::vector<u32> counts(2, kUntouched);

  // Dois lotes cujas reservas se cruzam: sem oclusão nenhuma eles se
  // sobrescreveriam, e com oclusão o resultado dependeria da ordem de execução.
  const std::vector<GpuCompactBatch> overlapping{batchOf(0, 3, 0), batchOf(3, 3, 2)};
  AE_EXPECT_TRUE(!compactDrawCommandsReference(instanceCounts, overlapping, indices, counts),
                 "destinos sobrepostos sao recusados");

  const std::vector<GpuCompactBatch> adjacent{batchOf(0, 3, 0), batchOf(3, 3, 3)};
  AE_EXPECT_TRUE(compactDrawCommandsReference(instanceCounts, adjacent, indices, counts),
                 "destinos encostados, mas disjuntos, sao legais");
}

AE_TEST(compaction_rejects_undersized_outputs) {
  const std::vector<u32> instanceCounts(4, 1);
  const std::vector<GpuCompactBatch> batches{batchOf(0, 4, 0)};

  std::vector<u32> shortIndices(3, kUntouched);
  std::vector<u32> counts(1, kUntouched);
  AE_EXPECT_TRUE(!compactDrawCommandsReference(instanceCounts, batches, shortIndices, counts),
                 "lista compacta menor que o pior caso do lote e recusada");

  std::vector<u32> indices(4, kUntouched);
  std::vector<u32> noCounts;
  AE_EXPECT_TRUE(!compactDrawCommandsReference(instanceCounts, batches, indices, noCounts),
                 "sem lugar para a contagem, o consumidor leria lixo: recusa");
}

AE_TEST(compaction_batch_validation_matches_the_kernel_guard) {
  // O kernel repete a mesma guarda por conta própria porque falhar fechado na
  // GPU é o que impede uma escrita fora dos limites caso a CPU erre. Este teste
  // fixa a fronteira exata que as duas implementações têm de concordar.
  const std::vector<GpuCompactBatch> exact{batchOf(0, 4, 0)};
  AE_EXPECT_TRUE(validateGpuCompactBatches(exact, 4, 4), "lote que preenche a capacidade e legal");

  const std::vector<GpuCompactBatch> oneOver{batchOf(0, 5, 0)};
  AE_EXPECT_TRUE(!validateGpuCompactBatches(oneOver, 4, 8), "um comando alem da origem e ilegal");

  const std::vector<GpuCompactBatch> destinationOver{batchOf(0, 4, 5)};
  AE_EXPECT_TRUE(!validateGpuCompactBatches(destinationOver, 4, 8),
                 "a reserva do destino e do pior caso, nao do numero de visiveis");

  const std::vector<GpuCompactBatch> empties{batchOf(4, 0, 4), batchOf(4, 0, 4)};
  AE_EXPECT_TRUE(validateGpuCompactBatches(empties, 4, 4),
                 "lotes vazios na mesma base nao se sobrepoem");
}
