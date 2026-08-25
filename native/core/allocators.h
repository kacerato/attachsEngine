// Alocadores de arena/pool sobre memória alinhada.
//
// Caminho quente da engine não pode alocar via GC/malloc por objeto (ver
// CONVENCOES.md, regra "zero alocação de GC no caminho de frame"). Estes
// alocadores pré-reservam um bloco e distribuem dele; nunca chamam
// new/delete por item.
#pragma once

#include "core/base.h"

#include <cstdlib>
#include <new>

namespace ae {

// Aloca `size` bytes alinhados a `alignment` (potência de 2). Wrapper fino
// sobre aligned_alloc/free para isolar o único ponto de alocação do sistema.
inline void *alignedAlloc(usize size, usize alignment) {
  AE_CHECK((alignment & (alignment - 1)) == 0, "alinhamento precisa ser potência de 2");
  // aligned_alloc exige que size seja múltiplo de alignment.
  usize rounded = (size + alignment - 1) & ~(alignment - 1);
  if (rounded == 0) rounded = alignment;
  return std::aligned_alloc(alignment, rounded);
}

inline void alignedFree(void *ptr) { std::free(ptr); }

// LinearAllocator: arena que só cresce (bump pointer) até ser resetada.
// Ideal para dados de um único frame — reset é O(1) e não libera memória
// para o SO, evitando fragmentação entre frames.
class LinearAllocator {
public:
  LinearAllocator() = default;

  explicit LinearAllocator(usize capacity, usize alignment = alignof(std::max_align_t))
      : alignment_(alignment) {
    AE_CHECK(capacity > 0, "capacidade da arena precisa ser > 0");
    base_ = static_cast<u8 *>(alignedAlloc(capacity, alignment));
    AE_CHECK(base_ != nullptr, "falha ao reservar memória da arena");
    capacity_ = capacity;
    cursor_ = 0;
  }

  ~LinearAllocator() {
    if (base_) alignedFree(base_);
  }

  LinearAllocator(const LinearAllocator &) = delete;
  LinearAllocator &operator=(const LinearAllocator &) = delete;

  LinearAllocator(LinearAllocator &&other) noexcept { *this = std::move(other); }
  LinearAllocator &operator=(LinearAllocator &&other) noexcept {
    if (this != &other) {
      if (base_) alignedFree(base_);
      base_ = other.base_;
      capacity_ = other.capacity_;
      cursor_ = other.cursor_;
      alignment_ = other.alignment_;
      other.base_ = nullptr;
      other.capacity_ = 0;
      other.cursor_ = 0;
    }
    return *this;
  }

  // Retorna nullptr quando a arena está esgotada — caller decide se isso é
  // fatal (AE_CHECK) ou se deve crescer para uma arena maior.
  void *allocate(usize size, usize align = alignof(std::max_align_t)) {
    usize aligned_cursor = (cursor_ + align - 1) & ~(align - 1);
    if (aligned_cursor + size > capacity_) {
      return nullptr; // esgotado
    }
    void *ptr = base_ + aligned_cursor;
    cursor_ = aligned_cursor + size;
    return ptr;
  }

  // O(1): não libera memória do SO, só volta o cursor ao início. É o motivo
  // de a arena existir — reciclar por frame sem tocar o alocador do sistema.
  void reset() { cursor_ = 0; }

  usize used() const { return cursor_; }
  usize capacity() const { return capacity_; }

private:
  u8 *base_ = nullptr;
  usize capacity_ = 0;
  usize cursor_ = 0;
  usize alignment_ = alignof(std::max_align_t);
};

// PoolAllocator: blocos de tamanho fixo em uma free-list intrusiva. Aloca e
// libera em O(1), sem fragmentação, útil para objetos de vida curta e
// contagem alta (ex.: nós de render graph, jobs).
class PoolAllocator {
public:
  PoolAllocator() = default;

  PoolAllocator(usize blockSize, usize blockCount,
                usize alignment = alignof(std::max_align_t)) {
    AE_CHECK(blockSize >= sizeof(void *),
              "bloco do pool precisa caber ao menos um ponteiro para a free-list");
    AE_CHECK(blockCount > 0, "pool precisa ter ao menos um bloco");
    blockSize_ = (blockSize + alignment - 1) & ~(alignment - 1);
    blockCount_ = blockCount;
    base_ = static_cast<u8 *>(alignedAlloc(blockSize_ * blockCount_, alignment));
    AE_CHECK(base_ != nullptr, "falha ao reservar memória do pool");

    // Monta a free-list: cada bloco livre guarda o ponteiro para o próximo.
    freeList_ = base_;
    for (usize i = 0; i < blockCount_ - 1; ++i) {
      u8 *block = base_ + i * blockSize_;
      u8 *next = base_ + (i + 1) * blockSize_;
      *reinterpret_cast<void **>(block) = next;
    }
    *reinterpret_cast<void **>(base_ + (blockCount_ - 1) * blockSize_) = nullptr;
    freeCount_ = blockCount_;
  }

  ~PoolAllocator() {
    if (base_) alignedFree(base_);
  }

  PoolAllocator(const PoolAllocator &) = delete;
  PoolAllocator &operator=(const PoolAllocator &) = delete;

  PoolAllocator(PoolAllocator &&other) noexcept { *this = std::move(other); }
  PoolAllocator &operator=(PoolAllocator &&other) noexcept {
    if (this != &other) {
      if (base_) alignedFree(base_);
      base_ = other.base_;
      blockSize_ = other.blockSize_;
      blockCount_ = other.blockCount_;
      freeList_ = other.freeList_;
      freeCount_ = other.freeCount_;
      other.base_ = nullptr;
      other.freeList_ = nullptr;
      other.blockCount_ = 0;
      other.freeCount_ = 0;
    }
    return *this;
  }

  // Retorna nullptr quando o pool está esgotado (todos os blocos em uso).
  void *allocate() {
    if (!freeList_) return nullptr;
    void *block = freeList_;
    freeList_ = *reinterpret_cast<void **>(freeList_);
    --freeCount_;
    return block;
  }

  void deallocate(void *ptr) {
    if (!ptr) return;
    *reinterpret_cast<void **>(ptr) = freeList_;
    freeList_ = ptr;
    ++freeCount_;
  }

  usize blockSize() const { return blockSize_; }
  usize capacity() const { return blockCount_; }
  usize freeCount() const { return freeCount_; }

private:
  u8 *base_ = nullptr;
  void *freeList_ = nullptr;
  usize blockSize_ = 0;
  usize blockCount_ = 0;
  usize freeCount_ = 0;
};

} // namespace ae
