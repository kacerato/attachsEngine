#include "rhi/bindless_registry.h"

#include <vector>

namespace ae::rhi {

BindlessTextureRegistry::~BindlessTextureRegistry() {
  shutdown();
}

bool BindlessTextureRegistry::initialize(VkDevice device, u32 capacity, VkImageView dummyImageView,
                                         VkSampler dummySampler) {
  if (device == VK_NULL_HANDLE || capacity == 0 || dummyImageView == VK_NULL_HANDLE ||
      dummySampler == VK_NULL_HANDLE) {
    return false;
  }
  device_ = device;
  allocator_ = BindlessIndexAllocator(capacity);

  // VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT: permite deixar slots do array sem gravação válida
  // sem violar a spec — mesmo assim inicializamos todo slot com a dummy abaixo (defesa em
  // profundidade: um hardware/driver que não respeite partially-bound direito não deveria ler
  // lixo). VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT: permite atualizar o set enquanto comandos
  // que o referenciam já foram gravados (mas ainda não submetidos) — sem isso, registrar uma
  // textura nova exigiria esperar o device ficar ocioso, inviável para import de asset em tempo
  // de edição.
  VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
  bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlagsInfo.bindingCount = 1;
  bindingFlagsInfo.pBindingFlags = &bindingFlags;

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = capacity;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = &bindingFlagsInfo;
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &binding;
  if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = capacity;
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout_;
  if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_) != VK_SUCCESS) {
    shutdown();
    return false;
  }

  // Preenche todo slot com a dummy antes de qualquer registro real — ver comentário do header.
  std::vector<VkDescriptorImageInfo> dummyInfos(capacity);
  for (u32 i = 0; i < capacity; ++i) {
    dummyInfos[i].sampler = dummySampler;
    dummyInfos[i].imageView = dummyImageView;
    dummyInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorCount = capacity;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = dummyInfos.data();
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

  return true;
}

void BindlessTextureRegistry::shutdown() {
  if (device_ == VK_NULL_HANDLE) return;
  // descriptorSet_ não precisa de destroy explícito: é liberado junto com o pool (não foi
  // alocado com VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, então vkFreeDescriptorSets
  // não se aplica — mesma disciplina do resto do RHI, ver createDescriptors() de InstancedRenderer).
  descriptorSet_ = VK_NULL_HANDLE;
  if (pool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device_, pool_, nullptr); pool_ = VK_NULL_HANDLE; }
  if (layout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device_, layout_, nullptr); layout_ = VK_NULL_HANDLE; }
  device_ = VK_NULL_HANDLE;
}

u32 BindlessTextureRegistry::registerTexture(VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout) {
  if (!isReady()) return kBindlessIndexInvalid;
  u32 index = allocator_.allocate();
  if (index == kBindlessIndexInvalid) return kBindlessIndexInvalid;

  VkDescriptorImageInfo imageInfo{};
  imageInfo.sampler = sampler;
  imageInfo.imageView = imageView;
  imageInfo.imageLayout = imageLayout;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_;
  write.dstBinding = 0;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

  return index;
}

void BindlessTextureRegistry::unregisterTexture(u32 index) {
  allocator_.release(index);
}

} // namespace ae::rhi
