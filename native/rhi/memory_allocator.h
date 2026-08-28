#pragma once

#include "rhi/memory_budget.h"

#include <vulkan/vulkan.h>

// Mantém o header de terceiros fora da API pública do RHI. Além de reduzir o
// custo de compilação, isto impede warnings/anotações específicas do VMA de
// vazarem para todos os consumidores de device.h.
struct VmaAllocator_T;
struct VmaAllocation_T;
using VmaAllocator = VmaAllocator_T *;
using VmaAllocation = VmaAllocation_T *;

namespace ae::rhi {

enum class CpuAccess : u32 {
  None = 0,
  SequentialWrite,
  Random,
};

struct BufferDesc {
  u64 sizeBytes = 0;
  VkBufferUsageFlags usage = 0;
  MemoryClass memoryClass = MemoryClass::Buffer;
  CpuAccess cpuAccess = CpuAccess::None;
  bool preferDeviceMemory = true;
};

struct ImageDesc {
  u32 width = 0;
  u32 height = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkImageUsageFlags usage = 0;
  VkImageAspectFlags aspectMask = 0;
  MemoryClass memoryClass = MemoryClass::Texture;
  u32 mipLevels = 1;
};

bool isImageDescValid(const ImageDesc &desc);
bool isRgba8UploadValid(const ImageDesc &desc, u64 sourceSizeBytes);
// Tightly packed complete mip chains, supported RGBA8/RGBA16F/ASTC6x6 formats.
u64 sampledMipByteSize(VkFormat format, u32 width, u32 height);
u64 sampledChainByteSize(const ImageDesc &desc);

// Recurso move-only com ownership explícito. A destruição passa pelo allocator
// que o criou, mantendo VkBuffer e VmaAllocation inseparáveis.
class VulkanBuffer final {
public:
  VulkanBuffer() = default;
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;
  VulkanBuffer(VulkanBuffer &&other) noexcept;
  VulkanBuffer &operator=(VulkanBuffer &&other) noexcept;

  void reset();
  bool isReady() const { return buffer_ != VK_NULL_HANDLE; }
  VkBuffer handle() const { return buffer_; }
  void *mappedData() const { return mappedData_; }
  u64 sizeBytes() const { return sizeBytes_; }

private:
  friend class VulkanMemoryAllocator;
  friend class VulkanUploadContext;
  class VulkanMemoryAllocator *owner_ = nullptr;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  void *mappedData_ = nullptr;
  u64 sizeBytes_ = 0;
  u64 accountedBytes_ = 0;
  MemoryClass memoryClass_ = MemoryClass::Buffer;
};

class VulkanImage final {
public:
  VulkanImage() = default;
  ~VulkanImage();

  VulkanImage(const VulkanImage &) = delete;
  VulkanImage &operator=(const VulkanImage &) = delete;
  VulkanImage(VulkanImage &&other) noexcept;
  VulkanImage &operator=(VulkanImage &&other) noexcept;

  void reset();
  bool isReady() const { return image_ != VK_NULL_HANDLE && view_ != VK_NULL_HANDLE; }
  VkImage handle() const { return image_; }
  VkImageView view() const { return view_; }
  VkFormat format() const { return format_; }
  u32 width() const { return width_; }
  u32 height() const { return height_; }
  ImageDesc description() const { return {width_, height_, format_, usage_, aspectMask_, memoryClass_, mipLevels_}; }

private:
  friend class VulkanMemoryAllocator;
  friend class VulkanUploadContext;
  class VulkanMemoryAllocator *owner_ = nullptr;
  VkImage image_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  u32 width_ = 0;
  u32 height_ = 0;
  u32 mipLevels_ = 1;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkImageUsageFlags usage_ = 0;
  VkImageAspectFlags aspectMask_ = 0;
  bool uploadSubmitted_ = false;
  u64 accountedBytes_ = 0;
  MemoryClass memoryClass_ = MemoryClass::Texture;
};

class VulkanMemoryAllocator final {
public:
  VulkanMemoryAllocator() = default;
  ~VulkanMemoryAllocator();

  VulkanMemoryAllocator(const VulkanMemoryAllocator &) = delete;
  VulkanMemoryAllocator &operator=(const VulkanMemoryAllocator &) = delete;

  bool initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                  const MemoryBudgetConfig &budgetConfig);
  void shutdown();

  bool createBuffer(const BufferDesc &desc, VulkanBuffer *outBuffer);
  void destroyBuffer(VulkanBuffer &buffer);
  bool flushBuffer(VulkanBuffer &buffer, u64 offsetBytes = 0, u64 sizeBytes = 0);
  bool createImage(const ImageDesc &desc, VulkanImage *outImage);
  void destroyImage(VulkanImage &image);

  bool isReady() const { return allocator_ != VK_NULL_HANDLE; }
  VkDevice device() const { return device_; }
  MemoryBudgetSnapshot budgetSnapshot() const { return budget_.snapshot(); }

private:
  VmaAllocator allocator_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  MemoryBudgetTracker budget_{};
};

MemoryBudgetConfig deriveMobileMemoryBudget(VkPhysicalDevice physicalDevice);

} // namespace ae::rhi
