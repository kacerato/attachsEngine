// Alocação de índice para o registro bindless (item 2.1.4 do plano). Separado de
// bindless_registry.h de propósito: este arquivo é lógica pura, sem nenhuma chamada Vulkan real,
// testável no host sem GPU/loader (mesma separação de responsabilidade de descriptor_cache.h
// versus device.cpp) — se estivesse na mesma unidade de compilação de BindlessTextureRegistry
// (que chama vkCreateDescriptorPool etc.), o linker arrastaria símbolos vk* reais para qualquer
// alvo que testasse só esta parte, quebrando o build do host sem loader Vulkan.
#pragma once

#include "core/base.h"

#include <vector>

namespace ae::rhi {

// Índice inválido — mesmo valor sentinela de PhysicsBodyHandle::Invalid (0xFFFFFFFF).
inline constexpr u32 kBindlessIndexInvalid = 0xFFFFFFFFu;

// Aloca/libera índices dentro de um array bindless de tamanho fixo `capacity`. Free-list simples
// (pilha de índices livres) — mesma estrutura de qualquer pool de handle já usado no projeto
// (ex. PhysicsBodyHandle do lado nativo, item 4.1.1). Não toca em nenhum recurso Vulkan: só
// decide QUAL número um novo registro recebe.
class BindlessIndexAllocator {
public:
  explicit BindlessIndexAllocator(u32 capacity);

  u32 capacity() const { return capacity_; }
  u32 allocatedCount() const { return capacity_ - static_cast<u32>(freeIndices_.size()); }

  // Devolve kBindlessIndexInvalid se a capacidade foi esgotada — nunca cresce silenciosamente
  // (o array de descritores no lado Vulkan tem tamanho fixo definido na criação do pool/set;
  // crescer exigiria recriar o descriptor set inteiro, fora do escopo desta fatia).
  u32 allocate();

  // Devolve o índice à free-list para reuso futuro. Chamar duas vezes para o mesmo índice sem
  // uma allocate() entre as duas é uso indevido do chamador (corromperia a free-list) — não
  // detectado aqui por custo (mesma disciplina de handles do projeto: o contrato é do chamador).
  void release(u32 index);

private:
  u32 capacity_;
  std::vector<u32> freeIndices_; // pilha: topo = próximo índice a devolver
};

} // namespace ae::rhi
