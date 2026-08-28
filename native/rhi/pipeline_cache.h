// Item 2.1.3 do plano: cache real de render pass / pipeline layout / pipeline
// gráfico, generalizando o DescriptorCache<Desc,Handle> já testado headless
// (descriptor_cache.h) para os três objetos que hoje cada renderer Android
// recria do zero em TODA troca de tela — rotação, resize, background/
// foreground disparam TERM_WINDOW→INIT_WINDOW, e cada um desses ciclos
// chamava vkCreateGraphicsPipelines de novo mesmo quando a configuração
// resultante era idêntica à anterior (ver docs/ESTADO.md, item 2.1.3, para a
// evidência de hardware do reuso real).
//
// Design: as descrições não tentam modelar toda a superfície do Vulkan
// (viraria um sistema de reflection de shader, fora de escopo) — em vez
// disso, cada Desc carrega um `extraHash` calculado pelo chamador sobre os
// bytes crus das sub-estruturas variáveis (vertex input layout, push
// constant ranges) que não cabem como campos escalares fixos. hashDesc()
// (descriptor_cache.h) já cobre os campos escalares por igualdade de bytes;
// extraHash amplia isso sem exigir uma struct por combinação possível de
// vertex layout.
//
// Ancoragem: os três caches vivem em VulkanDevice (não em cada renderer),
// porque o VkPipeline cacheado precisa sobreviver ao shutdown()/initialize()
// do renderer que o criou — é exatamente esse ciclo (destruição e recriação
// a cada troca de tela) que o cache existe para evitar. Múltiplos renderers
// (TriangleRenderer, InstancedRenderer) compartilham a mesma instância.
#pragma once

#include "core/base.h"
#include "rhi/descriptor_cache.h"

#include <vulkan/vulkan.h>

namespace ae::rhi {

// Estende RenderPassDesc (descriptor_cache.h) só o suficiente para produzir
// um VkRenderPass real: load/store op de cor e presença de depth attachment
// não entravam na descrição de teste original, mas mudam o VkRenderPass de
// verdade.
struct RenderPassCacheDesc {
  VkFormat colorFormat = VK_FORMAT_UNDEFINED;
  VkFormat depthFormat = VK_FORMAT_UNDEFINED; // VK_FORMAT_UNDEFINED = sem depth attachment
  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

  bool operator==(const RenderPassCacheDesc &other) const = default;
};

// Um VkDescriptorSetLayout já é, no projeto, um objeto de longa duração
// criado uma vez (ex.: BindlessTextureRegistry::layout()) — por isso entra
// aqui por HANDLE, não por descrição recursiva; dois layouts com o mesmo
// handle são trivialmente o mesmo layout, e comparar por handle evita
// duplicar a lógica de "descrever um descriptor set layout" que já não
// existe em lugar nenhum do projeto hoje.
struct PipelineLayoutCacheDesc {
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE; // VK_NULL_HANDLE = pipeline sem descriptor set
  VkShaderStageFlags pushConstantStageFlags = 0;
  u32 pushConstantSize = 0;

  bool operator==(const PipelineLayoutCacheDesc &other) const = default;
};

// Ver comentário de arquivo sobre extraHash. renderPass/layout entram por
// HANDLE (já cacheados por RenderPassCache/PipelineLayoutCache acima) — um
// VkRenderPass recriado com a mesma RenderPassCacheDesc sempre resolve para
// o mesmo handle cacheado, então comparar por handle aqui é equivalente a
// comparar por descrição, sem duplicar os campos.
struct GraphicsPipelineCacheDesc {
  u64 vertexShaderHash = 0;
  u64 fragmentShaderHash = 0;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  bool depthTestEnable = false;
  bool depthWriteEnable = false;
  bool blendEnable = false;
  // Hash FNV-1a (hashDesc, descriptor_cache.h) calculado pelo chamador sobre
  // VkVertexInputBindingDescription[]/VkVertexInputAttributeDescription[] —
  // dois renderers com vertex input diferente (ex.: TriangleRenderer sem
  // buffer vs. InstancedRenderer com binding por instância) só colidem na
  // chave se também colidirem aqui.
  u64 vertexInputHash = 0;

  bool operator==(const GraphicsPipelineCacheDesc &other) const = default;
};

// Dono real dos três caches — ver comentário de arquivo sobre por que vive
// no VulkanDevice, não em cada renderer. Não testável sem VkDevice real
// (mesma nota de device.h/device.cpp); a lógica de identidade de chave
// (DescriptorCache<Desc,Handle>) já é testada headless separadamente.
class PipelineCache final {
public:
  PipelineCache() = default;
  ~PipelineCache();

  PipelineCache(const PipelineCache &) = delete;
  PipelineCache &operator=(const PipelineCache &) = delete;

  void initialize(VkDevice device);
  void shutdown();

  // getOrCreate* retornam VK_NULL_HANDLE em falha de criação (nunca lançam);
  // o caller decide como reportar/abortar, mesma convenção do resto do RHI.
  VkRenderPass getOrCreateRenderPass(const RenderPassCacheDesc &desc);
  VkPipelineLayout getOrCreatePipelineLayout(const PipelineLayoutCacheDesc &desc);
  VkPipeline getOrCreateGraphicsPipeline(const GraphicsPipelineCacheDesc &desc,
                                        const VkGraphicsPipelineCreateInfo &createInfoTemplate);

  usize renderPassCount() const;
  usize pipelineLayoutCount() const;
  usize pipelineCount() const;

private:
  VkDevice device_ = VK_NULL_HANDLE;

  // unique_ptr porque DescriptorCache não é copiável/movível de forma
  // trivial (guarda um std::function e um unordered_map) e PipelineCache
  // precisa ser construído em duas fases (initialize() depois do device
  // existir) — mesma razão de BindlessTextureRegistry ser inicializado à
  // parte do construtor.
  struct Impl;
  Impl *impl_ = nullptr;
};

} // namespace ae::rhi
