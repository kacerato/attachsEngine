// Testes do item 2.1.4 (bindless via descriptor_indexing) — só a parte headless-testável:
// BindlessIndexAllocator, a lógica pura de "qual índice cada textura ocupa no array bindless".
// BindlessTextureRegistry (o VkDescriptorSet real) exige VkDevice real, exercitado no shell
// Android — ver docs/ESTADO.md para a evidência de hardware.
#include "harness.h"
#include "rhi/bindless_index_allocator.h"

using namespace ae;
using namespace ae::rhi;
using namespace ae::test;

AE_TEST(BindlessIndexAllocator_capacidadeZero_semAlocacaoPossivel) {
  BindlessIndexAllocator allocator(0);
  AE_EXPECT_TRUE(allocator.capacity() == 0, "capacidade deve refletir o valor do construtor");
  AE_EXPECT_TRUE(allocator.allocate() == kBindlessIndexInvalid, "capacidade zero nunca deve conseguir alocar");
}

AE_TEST(BindlessIndexAllocator_primeiraAlocacao_devolveIndiceValido) {
  BindlessIndexAllocator allocator(16);
  u32 index = allocator.allocate();
  AE_EXPECT_TRUE(index != kBindlessIndexInvalid, "primeira alocação em capacidade não-zero deve suceder");
  AE_EXPECT_TRUE(index < 16, "índice devolvido deve estar dentro da capacidade");
}

AE_TEST(BindlessIndexAllocator_alocacoesConsecutivas_devolvemIndicesDistintos) {
  BindlessIndexAllocator allocator(8);
  u32 seen[8];
  for (int i = 0; i < 8; ++i) {
    seen[i] = allocator.allocate();
    AE_EXPECT_TRUE(seen[i] != kBindlessIndexInvalid, "todas as 8 alocações dentro da capacidade devem suceder");
  }
  for (int i = 0; i < 8; ++i) {
    for (int j = i + 1; j < 8; ++j) {
      AE_EXPECT_TRUE(seen[i] != seen[j], "duas alocações consecutivas nunca devem devolver o mesmo índice");
    }
  }
}

AE_TEST(BindlessIndexAllocator_esgotarCapacidade_devolveInvalidoSemCrash) {
  BindlessIndexAllocator allocator(2);
  allocator.allocate();
  allocator.allocate();
  AE_EXPECT_TRUE(allocator.allocate() == kBindlessIndexInvalid, "terceira alocação além da capacidade 2 deve falhar explicitamente");
}

AE_TEST(BindlessIndexAllocator_releaseDepoisAllocate_permiteReciclarOIndice) {
  BindlessIndexAllocator allocator(1);
  u32 first = allocator.allocate();
  AE_EXPECT_TRUE(first != kBindlessIndexInvalid, "única alocação possível deve suceder");
  AE_EXPECT_TRUE(allocator.allocate() == kBindlessIndexInvalid, "capacidade 1 já esgotada antes do release");

  allocator.release(first);
  u32 second = allocator.allocate();
  AE_EXPECT_TRUE(second != kBindlessIndexInvalid, "após release, a capacidade deve estar disponível de novo");
  AE_EXPECT_TRUE(second == first, "com capacidade 1, o único índice reciclado deve ser o mesmo devolvido");
}

AE_TEST(BindlessIndexAllocator_allocatedCount_refleteAlocacoesMenosLiberacoes) {
  BindlessIndexAllocator allocator(4);
  AE_EXPECT_TRUE(allocator.allocatedCount() == 0, "nenhuma alocação feita ainda");
  u32 a = allocator.allocate();
  u32 b = allocator.allocate();
  AE_EXPECT_TRUE(allocator.allocatedCount() == 2, "duas alocações feitas");
  allocator.release(a);
  AE_EXPECT_TRUE(allocator.allocatedCount() == 1, "uma liberação deve reduzir a contagem");
  (void)b;
}

AE_TEST(BindlessIndexAllocator_releaseComIndiceForaDaFaixa_naoCrashaNemCorrompeFreeList) {
  BindlessIndexAllocator allocator(2);
  allocator.release(999); // índice nunca alocado, fora da capacidade — deve ser ignorado, não corromper o estado
  u32 first = allocator.allocate();
  u32 second = allocator.allocate();
  AE_EXPECT_TRUE(first != kBindlessIndexInvalid && second != kBindlessIndexInvalid, "as duas alocações válidas da capacidade real devem continuar funcionando");
  AE_EXPECT_TRUE(allocator.allocate() == kBindlessIndexInvalid, "capacidade real (2) deve continuar sendo 2, não inflada pelo release inválido");
}
