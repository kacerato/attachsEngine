// O consumidor Vulkan da interface: pega as instâncias que ui_instance_builder
// produziu e desenha a tela inteira em uma chamada.
//
// Ele é deliberadamente magro. Toda decisão — o que desenhar, onde, de que cor,
// como o texto vira glifos — já aconteceu na camada pura. Aqui só existe o que
// precisa de um `VkDevice`: pipeline, um buffer de instâncias por frame e as
// duas texturas de atlas.
//
// **Uma pipeline, um bind, um `vkCmdDraw`.** As duas texturas ficam ligadas o
// tempo todo e o recorte viaja na instância, então não há nada que force uma
// quebra de lote entre um painel e o rótulo dentro dele.
#pragma once

#include "core/base.h"
#include "rhi/memory_allocator.h"
#include "rhi/resource.h"
#include "rhi/surface_transform.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include "ui/ui_instance_builder.h"

#include <span>
#include <vulkan/vulkan.h>

namespace ae::rhi {

class VulkanUploadContext;

class VulkanUiRenderer final {
public:
  VulkanUiRenderer() = default;
  ~VulkanUiRenderer();
  VulkanUiRenderer(const VulkanUiRenderer &) = delete;
  VulkanUiRenderer &operator=(const VulkanUiRenderer &) = delete;

  // Os atlas são enviados uma vez, aqui. Depois disso `font` e `icons` só são
  // consultados pelo lado puro, e os bytes deles podem ser liberados — a GPU
  // ficou com a própria cópia.
  bool initialize(VkDevice device, VulkanMemoryAllocator &allocator,
                  VulkanUploadContext &upload, VkRenderPass renderPass, u32 subpass,
                  VkPipelineCache pipelineCache, const ui::UiFont &font,
                  const ui::UiIconAtlas &icons, u32 maximumInstances);
  void shutdown();
  bool isReady() const noexcept { return pipeline_ != VK_NULL_HANDLE; }
  u32 capacity() const noexcept { return capacity_; }

  // Grava o desenho dentro de um render pass já aberto. `surfaceWidth/Height`
  // são pixels LÓGICOS — os mesmos em que as instâncias foram construídas —, e a
  // pré-rotação leva o resultado ao espaço do display.
  bool record(VkCommandBuffer commandBuffer, std::span<const ui::UiInstance> instances,
              float surfaceWidth, float surfaceHeight, const SurfaceTransform &surfaceTransform, bool srgbTarget=false);
  // R4: troca o atlas de prévia (texturas). Cria imagem nova, envia e atualiza o
  // descritor; chamar só fora de gravação e com o quadro anterior concluído.
  bool setPreviewAtlas(VulkanUploadContext &upload, std::span<const u8> rgba, u32 width, u32 height);
  // Borrowed view. Caller keeps it alive until previous UI submissions finish.
  void setCameraPreview(VkImageView view);

private:
  struct PushConstants final {
    float surface[4]{};
    float surfaceTransform[4]{};
    float atlasSizes[4]{};
    float outputFlags[4]{};
  };

  bool createAtlas(VulkanMemoryAllocator &allocator, VulkanUploadContext &upload,
                   const ui::UiFont &font, const ui::UiIconAtlas &icons);
  bool createPipeline(VkRenderPass renderPass, u32 subpass, VkPipelineCache pipelineCache);

  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VulkanBuffer instanceBuffer_{};
  VulkanImage fontAtlas_{};
  VulkanImage iconAtlas_{};
  VulkanImage previewAtlas_{};
  VkImageView cameraPreviewView_=VK_NULL_HANDLE;
  float previewAtlasSize_[2]{};
  VulkanSampler sampler_{};
  VulkanMemoryAllocator *allocator_ = nullptr;
  u32 capacity_ = 0;
  float fontAtlasSize_[2]{};
  float iconAtlasSize_[2]{};
};

} // namespace ae::rhi
