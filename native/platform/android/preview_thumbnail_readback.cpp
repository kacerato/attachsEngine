#include "platform/android/preview_thumbnail_readback.h"
#include <cstring>

namespace ae::platform::android {
namespace {
bool supported(VkFormat format) {
  return format==VK_FORMAT_R8G8B8A8_UNORM||format==VK_FORMAT_R8G8B8A8_SRGB||
      format==VK_FORMAT_B8G8R8A8_UNORM||format==VK_FORMAT_B8G8R8A8_SRGB;
}
}
bool PreviewThumbnailReadback::record(rhi::VulkanMemoryAllocator &allocator,VkCommandBuffer command,
    const rhi::VulkanImage &image,const renderer::RenderViewSnapshot &lease,std::string &error) {
  error.clear();
  if(recorded_) {error="Captura anterior ainda possui uma submissão pendente.";return false;}
  const u64 pixels=u64{lease.width}*lease.height;
  if(command==VK_NULL_HANDLE||!lease.valid()||!lease.sceneEpoch||!image.isReady()||
      image.width()!=lease.width||image.height()!=lease.height||!supported(image.format())||
      !(image.description().usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)||lease.width>MaximumSide||
      lease.height>MaximumSide||!pixels||pixels>MaximumPixels) {
    error="Target de miniatura exige preview RGBA8 com cópia habilitada e no máximo512×512.";return false;
  }
  rhi::BufferDesc desc;desc.sizeBytes=pixels*4;desc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  desc.memoryClass=rhi::MemoryClass::RenderTarget;desc.cpuAccess=rhi::CpuAccess::Random;desc.preferDeviceMemory=false;
  rhi::VulkanBuffer next;
  if(!allocator.createBuffer(desc,&next)||!next.mappedData()) {error="Memória de leitura da miniatura indisponível.";return false;}
  VkImageMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image=image.handle();barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  barrier.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={lease.width,lease.height,1};
  vkCmdCopyImageToBuffer(command,image.handle(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,next.handle(),1,&copy);
  barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  VkBufferMemoryBarrier host{};host.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=next.handle();host.size=VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
  buffer_=std::move(next);lease_=lease;format_=image.format();recorded_=deliver_=true;fence_=VK_NULL_HANDLE;return true;
}
bool PreviewThumbnailReadback::submitted(VkFence fence) {
  if(!recorded_||fence_!=VK_NULL_HANDLE||fence==VK_NULL_HANDLE)return false;
  fence_=fence;return true;
}
bool PreviewThumbnailReadback::abandonUnsubmitted() {
  if(!recorded_||fence_!=VK_NULL_HANDLE)return false;
  resetAfterDeviceIdle();return true;
}
ThumbnailReadbackStatus PreviewThumbnailReadback::take(VkDevice device,rhi::VulkanMemoryAllocator &allocator,
    PreviewThumbnailPixels &out,std::string &error) {
  error.clear();if(!recorded_)return ThumbnailReadbackStatus::Idle;
  if(fence_==VK_NULL_HANDLE)return ThumbnailReadbackStatus::Pending;
  if(device==VK_NULL_HANDLE) {error="Device da captura não está disponível.";return ThumbnailReadbackStatus::Failed;}
  const auto status=vkGetFenceStatus(device,fence_);
  if(status==VK_NOT_READY)return ThumbnailReadbackStatus::Pending;
  if(status!=VK_SUCCESS) {deliver_=false;error="Submissão da miniatura não completou com sucesso.";return ThumbnailReadbackStatus::Failed;}
  return consume(allocator,out,error);
}
ThumbnailReadbackStatus PreviewThumbnailReadback::takeAfterSubmissionWait(VkFence completed,
    rhi::VulkanMemoryAllocator &allocator,PreviewThumbnailPixels &out,std::string &error) {
  error.clear();
  if(!recorded_)return ThumbnailReadbackStatus::Idle;
  if(completed==VK_NULL_HANDLE||fence_==VK_NULL_HANDLE||fence_!=completed) {
    error="Entrega da miniatura não corresponde à submissão esperada.";return ThumbnailReadbackStatus::Failed;
  }
  return consume(allocator,out,error);
}
ThumbnailReadbackStatus PreviewThumbnailReadback::consume(rhi::VulkanMemoryAllocator &allocator,
    PreviewThumbnailPixels &out,std::string &error) {
  if(!deliver_) {resetAfterDeviceIdle();return ThumbnailReadbackStatus::Cancelled;}
  const u64 size=u64{lease_.width}*lease_.height*4;
  if(!buffer_.mappedData()||size>buffer_.sizeBytes()||!allocator.invalidateBuffer(buffer_)) {
    resetAfterDeviceIdle();error="Leitura coerente dos pixels da miniatura falhou.";return ThumbnailReadbackStatus::Failed;
  }
  PreviewThumbnailPixels pixels;pixels.lease=lease_;pixels.rgba.resize(static_cast<usize>(size));
  std::memcpy(pixels.rgba.data(),buffer_.mappedData(),pixels.rgba.size());
  if(format_==VK_FORMAT_B8G8R8A8_UNORM||format_==VK_FORMAT_B8G8R8A8_SRGB)
    for(usize i=0;i<pixels.rgba.size();i+=4)std::swap(pixels.rgba[i],pixels.rgba[i+2]);
  resetAfterDeviceIdle();out=std::move(pixels);return ThumbnailReadbackStatus::Ready;
}
void PreviewThumbnailReadback::resetAfterDeviceIdle() {
  buffer_.reset();lease_={};fence_=VK_NULL_HANDLE;format_=VK_FORMAT_UNDEFINED;recorded_=deliver_=false;
}
} // namespace ae::platform::android
