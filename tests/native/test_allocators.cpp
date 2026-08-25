#include "harness.h"
#include "core/allocators.h"

using namespace ae;
using namespace ae::test;

AE_TEST(LinearAllocator_alinhamento_respeitado) {
  LinearAllocator arena(1024, 16);
  void *p1 = arena.allocate(3, 16);
  void *p2 = arena.allocate(5, 16);
  AE_EXPECT_TRUE(p1 != nullptr && p2 != nullptr, "alocacoes nao deveriam falhar dentro da capacidade");
  AE_EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(p1) % 16 == 0, "p1 precisa estar alinhado a 16");
  AE_EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(p2) % 16 == 0, "p2 precisa estar alinhado a 16");
}

AE_TEST(LinearAllocator_reset_volta_ao_inicio) {
  LinearAllocator arena(64, 8);
  arena.allocate(32, 8);
  AE_EXPECT_TRUE(arena.used() == 32, "used deveria refletir a alocacao");
  arena.reset();
  AE_EXPECT_TRUE(arena.used() == 0, "reset precisa zerar o cursor");
  void *p = arena.allocate(64, 8);
  AE_EXPECT_TRUE(p != nullptr, "apos reset a arena inteira deveria estar livre de novo");
}

AE_TEST(LinearAllocator_esgotamento_retorna_nulo) {
  LinearAllocator arena(16, 8);
  void *p1 = arena.allocate(16, 8);
  void *p2 = arena.allocate(1, 8);
  AE_EXPECT_TRUE(p1 != nullptr, "primeira alocacao deveria caber exatamente");
  AE_EXPECT_TRUE(p2 == nullptr, "segunda alocacao deveria falhar: arena esgotada");
}

AE_TEST(PoolAllocator_aloca_e_libera_reciclando_bloco) {
  PoolAllocator pool(sizeof(void *) * 2, 4);
  void *a = pool.allocate();
  void *b = pool.allocate();
  AE_EXPECT_TRUE(a != nullptr && b != nullptr && a != b, "blocos distintos devem ter enderecos distintos");
  AE_EXPECT_TRUE(pool.freeCount() == 2, "dois blocos ainda livres de quatro");
  pool.deallocate(a);
  AE_EXPECT_TRUE(pool.freeCount() == 3, "liberar devolve o bloco a free-list");
  void *c = pool.allocate();
  AE_EXPECT_TRUE(c == a, "bloco reciclado deveria ser o ultimo liberado (free-list LIFO)");
}

AE_TEST(PoolAllocator_esgotamento_retorna_nulo) {
  PoolAllocator pool(sizeof(void *), 2);
  void *a = pool.allocate();
  void *b = pool.allocate();
  void *c = pool.allocate();
  AE_EXPECT_TRUE(a != nullptr && b != nullptr, "os dois blocos do pool deveriam ser alocados");
  AE_EXPECT_TRUE(c == nullptr, "terceira alocacao deveria falhar: pool esgotado");
}

AE_TEST(PoolAllocator_todos_blocos_alinhados) {
  PoolAllocator pool(sizeof(void *) * 3, 8, 32);
  for (int i = 0; i < 8; ++i) {
    void *p = pool.allocate();
    AE_EXPECT_TRUE(p != nullptr, "capacidade nao deveria esgotar antes do esperado");
    AE_EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(p) % 32 == 0, "bloco precisa respeitar alinhamento pedido");
  }
}
