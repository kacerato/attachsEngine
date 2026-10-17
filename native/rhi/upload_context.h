#pragma once

#include "rhi/memory_allocator.h"
#include "core/buffer_updates.h"

namespace ae::rhi {
struct BufferRangeUpload {
  std::span<const u8> source;
  std::span<const BufferUpdateRange> ranges;
  VulkanBuffer *destination = nullptr;
  VkPipelineStageFlags stage = 0;
  VkAccessFlags access = 0;
};

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
  bool uploadSampledMipChain(VulkanMemoryAllocator &allocator, const void *sourceBytes,
                            u64 sourceSizeBytes, VulkanImage &destination);
  // Static geometry upload. Destination is device-local and already created
  // with TRANSFER_DST plus its final usage; completion is synchronous because
  // this context belongs to initialization/import, never the frame loop.
  // Only sourceSizeBytes are initialized; any extra destination capacity must
  // remain outside the caller's draw/dispatch ranges until explicitly uploaded.
  bool uploadBuffer(VulkanMemoryAllocator &allocator, const void *sourceBytes,
                    u64 sourceSizeBytes, VulkanBuffer &destination,
                    VkPipelineStageFlags destinationStage,
                    VkAccessFlags destinationAccess);
  // Caller has retired every reader of this buffer. Validate and stage all
  // ranges before one submission; unchanged bytes and allocation stay intact.
  // source is the logical extent and may be smaller than destination capacity.
  bool uploadBufferRanges(VulkanMemoryAllocator &allocator, std::span<const BufferRangeUpload> updates);

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
};

} // namespace ae::rhi
