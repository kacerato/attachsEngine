// Registro bindless de texturas (item 2.1.4 do plano: "descriptor_indexing — base do
// GPU-driven rendering: um array global de texturas/buffers").
//
// Design: UM único VkDescriptorSet, com UM binding cujo tipo é um array grande de
// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER (VK_EXT_descriptor_indexing / core no Vulkan 1.2+).
// Shaders acessam por índice (`textures[nonuniformEXT(materialIndex)]`, plano §5.2) em vez de
// receber um descriptor set por objeto — elimina o custo de alocar/vincular um novo set a cada
// draw call, pré-requisito citado pelo plano para GPU-driven rendering.
//
// A alocação de ÍNDICE (headless, testável sem GPU) fica em bindless_index_allocator.h — este
// arquivo só contém a parte que exige VkDevice real (criação de descriptor set/pool/layout),
// exercitável apenas em hardware/Android. Ver o comentário de bindless_index_allocator.h para o
// motivo da separação em unidades de compilação distintas.
#pragma once

#include "rhi/bindless_index_allocator.h"

#include <vulkan/vulkan.h>

namespace ae::rhi {

// Dono do VkDescriptorSetLayout/Pool/Set único com o array de combined-image-samplers. Cada slot
// começa apontando para uma textura/sampler "dummy" (evita erro de validação por descriptor não
// inicializado quando `partiallyBound` não está disponível em todo hardware-alvo) até uma
// textura real ser registrada nele.
class BindlessTextureRegistry final {
public:
  BindlessTextureRegistry() = default;
  ~BindlessTextureRegistry();

  BindlessTextureRegistry(const BindlessTextureRegistry &) = delete;
  BindlessTextureRegistry &operator=(const BindlessTextureRegistry &) = delete;

  // `dummyImageView`/`dummySampler` preenchem todo slot não utilizado (ver comentário de classe).
  // `capacity` é o tamanho do array bindless — fixo pelo tempo de vida do registro.
  bool initialize(VkDevice device, u32 capacity, VkImageView dummyImageView, VkSampler dummySampler);
  void shutdown();

  bool isReady() const { return descriptorSet_ != VK_NULL_HANDLE; }
  VkDescriptorSetLayout layout() const { return layout_; }
  VkDescriptorSet descriptorSet() const { return descriptorSet_; }
  u32 capacity() const { return allocator_.capacity(); }

  // Aloca um índice livre e escreve a textura nesse slot do array bindless. Devolve
  // kBindlessIndexInvalid se a capacidade foi esgotada.
  u32 registerTexture(VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout);

  // Libera o índice — o slot correspondente do array Vulkan NÃO é revertido para o dummy
  // automaticamente (custo de uma escrita de descriptor extra por release, e o slot só volta a
  // ser lido depois de um registerTexture novo sobrescrevê-lo) — documentado aqui para quem for
  // depurar um frame que ainda lê um índice recém-liberado antes do próximo registro: a leitura
  // devolve a última textura escrita naquele slot, não lixo nem a dummy.
  void unregisterTexture(u32 index);

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  BindlessIndexAllocator allocator_{0};
};

} // namespace ae::rhi
