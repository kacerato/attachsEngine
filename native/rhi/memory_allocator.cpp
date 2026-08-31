#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1001000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_NULLABLE
#define VMA_NOT_NULL
#define VMA_NULLABLE_NON_DISPATCHABLE
#define VMA_NOT_NULL_NON_DISPATCHABLE
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
// VMA is vendored third-party code. Keep -Werror for our implementation while
// preventing compiler-version-specific warnings inside the single-header
// implementation from breaking the engine build.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <vk_mem_alloc.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "rhi/memory_allocator.h"

#include <utility>

namespace ae::rhi {

namespace {
constexpr u64 kBudgetSafetyNumerator = 3;
constexpr u64 kBudgetSafetyDenominator = 4;
}

MemoryBudgetConfig deriveMobileMemoryBudget(VkPhysicalDevice physicalDevice) {
  if (physicalDevice == VK_NULL_HANDLE) return MemoryBudgetConfig::unlimited();

  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
  u64 deviceLocalBytes = 0;
  for (u32 i = 0; i < properties.memoryHeapCount; ++i) {
    if ((properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
      deviceLocalBytes += properties.memoryHeaps[i].size;
    }
  }
  if (deviceLocalBytes == 0) return MemoryBudgetConfig::unlimited();

  // Não promete o heap inteiro à engine: Android, compositor e driver também
  // o utilizam. O limite é uma política configurável e pode ser substituído
  // por DeviceProfile/ProjectSettings quando esses consumidores existirem.
  const u64 usableBytes = (deviceLocalBytes / kBudgetSafetyDenominator) * kBudgetSafetyNumerator;
  return MemoryBudgetConfig::fromTotalBytes(usableBytes);
}

VulkanBuffer::~VulkanBuffer() {
  reset();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer &&other) noexcept {
  *this = std::move(other);
}

VulkanBuffer &VulkanBuffer::operator=(VulkanBuffer &&other) noexcept {
  if (this == &other) return *this;
  reset();
  owner_ = other.owner_;
  buffer_ = other.buffer_;
  allocation_ = other.allocation_;
  mappedData_ = other.mappedData_;
  sizeBytes_ = other.sizeBytes_;
  accountedBytes_ = other.accountedBytes_;
  memoryClass_ = other.memoryClass_;
  other.owner_ = nullptr;
  other.buffer_ = VK_NULL_HANDLE;
  other.allocation_ = VK_NULL_HANDLE;
  other.mappedData_ = nullptr;
  other.sizeBytes_ = 0;
  other.accountedBytes_ = 0;
  return *this;
}

void VulkanBuffer::reset() {
  if (owner_ != nullptr) owner_->destroyBuffer(*this);
}

VulkanImage::~VulkanImage() {
  reset();
}

VulkanImage::VulkanImage(VulkanImage &&other) noexcept {
  *this = std::move(other);
}

VulkanImage &VulkanImage::operator=(VulkanImage &&other) noexcept {
  if (this == &other) return *this;
  reset();
  owner_ = other.owner_;
  image_ = other.image_;
  view_ = other.view_;
  allocation_ = other.allocation_;
  width_ = other.width_;
  height_ = other.height_;
  mipLevels_ = other.mipLevels_;
  format_ = other.format_;
  usage_ = other.usage_;
  aspectMask_ = other.aspectMask_;
  uploadSubmitted_ = other.uploadSubmitted_;
  accountedBytes_ = other.accountedBytes_;
  memoryClass_ = other.memoryClass_;
  other.owner_ = nullptr;
  other.image_ = VK_NULL_HANDLE;
  other.view_ = VK_NULL_HANDLE;
  other.allocation_ = VK_NULL_HANDLE;
  other.width_ = 0;
  other.height_ = 0;
  other.mipLevels_ = 1;
  other.format_ = VK_FORMAT_UNDEFINED;
  other.usage_ = 0;
  other.aspectMask_ = 0;
  other.uploadSubmitted_ = false;
  other.accountedBytes_ = 0;
  return *this;
}

void VulkanImage::reset() {
  if (owner_ != nullptr) owner_->destroyImage(*this);
}

VulkanMemoryAllocator::~VulkanMemoryAllocator() {
  shutdown();
}

bool VulkanMemoryAllocator::initialize(VkInstance instance, VkPhysicalDevice physicalDevice,
                                       VkDevice device,
                                       const MemoryBudgetConfig &budgetConfig) {
  if (allocator_ != VK_NULL_HANDLE || instance == VK_NULL_HANDLE ||
      physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
    return false;
  }

  VmaAllocatorCreateInfo info{};
  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  info.instance = instance;
  info.physicalDevice = physicalDevice;
  info.device = device;
  info.vulkanApiVersion = VK_API_VERSION_1_1;
  info.pVulkanFunctions = &functions;
  if (vmaCreateAllocator(&info, &allocator_) != VK_SUCCESS) return false;
  device_ = device;
  budget_.configure(budgetConfig);
  return true;
}

void VulkanMemoryAllocator::shutdown() {
  if (allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(allocator_);
    allocator_ = VK_NULL_HANDLE;
  }
  device_ = VK_NULL_HANDLE;
}

bool VulkanMemoryAllocator::createBuffer(const BufferDesc &desc, VulkanBuffer *outBuffer) {
  if (allocator_ == VK_NULL_HANDLE || outBuffer == nullptr || outBuffer->isReady() ||
      desc.sizeBytes == 0 || desc.usage == 0) {
    return false;
  }

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = desc.sizeBytes;
  bufferInfo.usage = desc.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocationInfo{};
  allocationInfo.usage = desc.preferDeviceMemory ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                                                 : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
  if (desc.cpuAccess == CpuAccess::SequentialWrite) {
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT;
  } else if (desc.cpuAccess == CpuAccess::Random) {
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo createdInfo{};
  if (vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &buffer, &allocation,
                      &createdInfo) != VK_SUCCESS) {
    return false;
  }

  const u64 allocationBytes = static_cast<u64>(createdInfo.size);
  if (!budget_.tryReserve(desc.memoryClass, allocationBytes)) {
    vmaDestroyBuffer(allocator_, buffer, allocation);
    return false;
  }

  outBuffer->owner_ = this;
  outBuffer->buffer_ = buffer;
  outBuffer->allocation_ = allocation;
  outBuffer->mappedData_ = createdInfo.pMappedData;
  outBuffer->sizeBytes_ = desc.sizeBytes;
  outBuffer->accountedBytes_ = allocationBytes;
  outBuffer->memoryClass_ = desc.memoryClass;
  return true;
}

void VulkanMemoryAllocator::destroyBuffer(VulkanBuffer &buffer) {
  if (buffer.owner_ != this) return;
  if (buffer.buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator_, buffer.buffer_, buffer.allocation_);
  }
  if (buffer.accountedBytes_ > 0) {
    budget_.release(buffer.memoryClass_, buffer.accountedBytes_);
  }
  buffer.owner_ = nullptr;
  buffer.buffer_ = VK_NULL_HANDLE;
  buffer.allocation_ = VK_NULL_HANDLE;
  buffer.mappedData_ = nullptr;
  buffer.sizeBytes_ = 0;
  buffer.accountedBytes_ = 0;
}

bool VulkanMemoryAllocator::flushBuffer(VulkanBuffer &buffer, u64 offsetBytes, u64 sizeBytes) {
  if (!isReady() || buffer.owner_ != this || buffer.allocation_ == VK_NULL_HANDLE ||
      buffer.mappedData_ == nullptr ||
      offsetBytes > buffer.sizeBytes_) {
    return false;
  }
  const u64 available = buffer.sizeBytes_ - offsetBytes;
  const u64 flushBytes = sizeBytes == 0 ? available : sizeBytes;
  if (flushBytes > available) return false;
  return vmaFlushAllocation(allocator_, buffer.allocation_, offsetBytes, flushBytes) == VK_SUCCESS;
}

bool VulkanMemoryAllocator::createImage(const ImageDesc &desc, VulkanImage *outImage) {
  if (allocator_ == VK_NULL_HANDLE || outImage == nullptr || outImage->isReady() ||
      !isImageDescValid(desc)) {
    return false;
  }

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = {desc.width, desc.height, 1};
  imageInfo.mipLevels = desc.mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = desc.format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = desc.usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocationInfo{};
  allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo createdInfo{};
  if (vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &image, &allocation,
                     &createdInfo) != VK_SUCCESS) {
    return false;
  }

  const u64 allocationBytes = static_cast<u64>(createdInfo.size);
  if (!budget_.tryReserve(desc.memoryClass, allocationBytes)) {
    vmaDestroyImage(allocator_, image, allocation);
    return false;
  }

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = desc.format;
  viewInfo.subresourceRange.aspectMask = desc.aspectMask;
  viewInfo.subresourceRange.levelCount = desc.mipLevels;
  viewInfo.subresourceRange.layerCount = 1;
  VkImageView view = VK_NULL_HANDLE;
  if (vkCreateImageView(device_, &viewInfo, nullptr, &view) != VK_SUCCESS) {
    budget_.release(desc.memoryClass, allocationBytes);
    vmaDestroyImage(allocator_, image, allocation);
    return false;
  }

  outImage->owner_ = this;
  outImage->image_ = image;
  outImage->view_ = view;
  outImage->allocation_ = allocation;
  outImage->width_ = desc.width;
  outImage->height_ = desc.height;
  outImage->mipLevels_ = desc.mipLevels;
  outImage->format_ = desc.format;
  outImage->usage_ = desc.usage;
  outImage->aspectMask_ = desc.aspectMask;
  outImage->uploadSubmitted_ = false;
  outImage->accountedBytes_ = allocationBytes;
  outImage->memoryClass_ = desc.memoryClass;
  return true;
}

void VulkanMemoryAllocator::destroyImage(VulkanImage &image) {
  if (image.owner_ != this) return;
  if (image.view_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, image.view_, nullptr);
  }
  if (image.image_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
    vmaDestroyImage(allocator_, image.image_, image.allocation_);
  }
  if (image.accountedBytes_ > 0) {
    budget_.release(image.memoryClass_, image.accountedBytes_);
  }
  image.owner_ = nullptr;
  image.image_ = VK_NULL_HANDLE;
  image.view_ = VK_NULL_HANDLE;
  image.allocation_ = VK_NULL_HANDLE;
  image.width_ = 0;
  image.height_ = 0;
  image.mipLevels_ = 1;
  image.format_ = VK_FORMAT_UNDEFINED;
  image.usage_ = 0;
  image.aspectMask_ = 0;
  image.uploadSubmitted_ = false;
  image.accountedBytes_ = 0;
}

} // namespace ae::rhi
