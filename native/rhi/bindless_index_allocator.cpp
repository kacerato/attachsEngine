#include "rhi/bindless_index_allocator.h"

namespace ae::rhi {

BindlessIndexAllocator::BindlessIndexAllocator(u32 capacity) : capacity_(capacity) {
  freeIndices_.reserve(capacity_);
  // Empilha do maior para o menor para que allocate() devolva 0, 1, 2... na ordem natural na
  // primeira rodada de alocações — não é uma exigência do formato, só previsibilidade para quem
  // lê um log/teste esperando índices sequenciais no caso comum sem reciclagem.
  for (u32 i = capacity_; i > 0; --i) {
    freeIndices_.push_back(i - 1);
  }
}

u32 BindlessIndexAllocator::allocate() {
  if (freeIndices_.empty()) return kBindlessIndexInvalid;
  u32 index = freeIndices_.back();
  freeIndices_.pop_back();
  return index;
}

void BindlessIndexAllocator::release(u32 index) {
  if (index >= capacity_) return; // índice fora de faixa: uso indevido do chamador, ignorado (nunca corrompe a free-list com lixo)
  freeIndices_.push_back(index);
}

} // namespace ae::rhi
