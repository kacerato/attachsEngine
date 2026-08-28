#pragma once

#include "rhi/memory_allocator.h"

namespace ae::rhi {

// Contexto síncrono de upload usado na criação/importação de recursos, nunca
// no caminho de frame. O contrato já centraliza staging e transições; a versão
// assíncrona futura poderá conservar a mesma API de recursos e trocar apenas a
// política de submissão/fences.
// Single-thread: o caller serializa acesso à fila gráfica e mantém device/
// allocator vivos. Somente imagens novas, ainda não usadas por outro comando;
// termina em SHADER_READ_ONLY_OPTIMAL para leitura no fragment shader.
class VulkanUploadContext final {
public:
  VulkanUploadContext() = default;
  ~VulkanUploadContext();

  VulkanUploadContext(const VulkanUploadContext &) = delete;
  VulkanUploadContext &operator=(const VulkanUploadContext &) = delete;

  bool initialize(VkDevice device, u32 queueFamilyIndex);
  void shutdown();

  // Primeira operação tipada de upload: uma camada/mip, linhas compactas e
  // quatro canais de 8 bits. O nome estreito evita prometer suporte implícito
  // a formatos comprimidos, mips ou depth antes de esses contratos existirem.
  bool uploadRgba8ToSampledImage(VulkanMemoryAllocator &allocator, const void *sourceBytes,
                                 u64 sourceSizeBytes, VulkanImage &destination);

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
};

} // namespace ae::rhi
