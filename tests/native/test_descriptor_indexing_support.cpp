#include "harness.h"
#include "rhi/descriptor_indexing_support.h"

using namespace ae::rhi;

namespace {
VkPhysicalDeviceDescriptorIndexingProperties limits() {
  VkPhysicalDeviceDescriptorIndexingProperties p{};
  p.maxDescriptorSetUpdateAfterBindSamplers = 100;
  p.maxDescriptorSetUpdateAfterBindSampledImages = 100;
  p.maxPerStageDescriptorUpdateAfterBindSamplers = 100;
  p.maxPerStageDescriptorUpdateAfterBindSampledImages = 100;
  p.maxPerStageUpdateAfterBindResources = 100;
  p.maxUpdateAfterBindDescriptorsInAllPools = 100;
  return p;
}
}

AE_TEST(Bindless_exige_todas_subfeatures_do_layout) {
  auto f = enabledBindlessTextureFeatures(true);
  const auto p = limits();
  VkBool32 *required[] = {&f.shaderSampledImageArrayNonUniformIndexing,
      &f.descriptorBindingPartiallyBound, &f.runtimeDescriptorArray,
      &f.descriptorBindingSampledImageUpdateAfterBind};
  for (auto *feature : required) {
    *feature = VK_FALSE;
    AE_EXPECT_EQ(bindlessTextureCapacity(true, f, p), 0u, "sub-feature ausente deve escolher fallback");
    *feature = VK_TRUE;
  }
  AE_EXPECT_EQ(bindlessTextureCapacity(false, f, p), 0u, "sem extensao na instancia 1.1");
}

AE_TEST(Bindless_respeita_cada_limite_de_imagem_sampler_e_pool) {
  auto p = limits();
  const auto f = enabledBindlessTextureFeatures(true);
  uint32_t *values[] = {&p.maxDescriptorSetUpdateAfterBindSamplers,
      &p.maxDescriptorSetUpdateAfterBindSampledImages,
      &p.maxPerStageDescriptorUpdateAfterBindSamplers,
      &p.maxPerStageDescriptorUpdateAfterBindSampledImages,
      &p.maxPerStageUpdateAfterBindResources, &p.maxUpdateAfterBindDescriptorsInAllPools};
  for (auto *value : values) {
    *value = 7;
    AE_EXPECT_EQ(bindlessTextureCapacity(true, f, p), 7u, "usar menor limite");
    *value = 0;
    AE_EXPECT_EQ(bindlessTextureCapacity(true, f, p), 0u, "consulta ausente nao autoriza bindless");
    *value = 100;
  }
}

AE_TEST(Bindless_nao_solicita_features_nao_utilizadas) {
  const auto f = enabledBindlessTextureFeatures(true);
  AE_EXPECT_EQ(f.descriptorBindingUpdateUnusedWhilePending, VK_FALSE, "feature nao exigida pelo layout");
  AE_EXPECT_EQ(bindlessTextureCapacity(true, enabledBindlessTextureFeatures(false), limits()), 0u,
               "modo de diagnostico realmente desabilita subfeatures");
}
