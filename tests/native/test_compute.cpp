#include "harness.h"
#include "rhi/compute.h"
#include "renderer/gpu_draw_culling.h"
#include "rhi/shaders/astc_encode_spirv.h"
#include "rhi/shaders/draw_cull_spirv.h"
#include "rhi/shaders/water_spectrum_evolve_spirv.h"
#include "rhi/shaders/water_fft_inverse_spirv.h"
#include "rhi/shaders/water_foam_update_spirv.h"

AE_TEST(Compute_water_spectral_shaders_match_resource_contract) {
  ae::rhi::ComputeShaderReflection reflection{};
  AE_EXPECT_TRUE(ae::rhi::reflectComputeShader(ae::rhi::shaders::kWater_Spectrum_EvolveCompSpirv,
      ae::rhi::shaders::kWater_Spectrum_EvolveCompSpirvSize,reflection),"evolution reflection");
  AE_EXPECT_EQ(reflection.bindingCount,2u,"initial and output");
  AE_EXPECT_EQ(reflection.localSize[0],64u,"evolution group");
  for(ae::u32 i=0;i<2;++i)
    AE_EXPECT_TRUE(reflection.bindings[i].binding==i && reflection.bindings[i].type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,"storage contract");
  AE_EXPECT_TRUE(reflection.hasPushConstants,"evolution parameters");
  AE_EXPECT_TRUE(ae::rhi::reflectComputeShader(ae::rhi::shaders::kWater_Fft_InverseCompSpirv,
      ae::rhi::shaders::kWater_Fft_InverseCompSpirvSize,reflection),"inverse reflection");
  AE_EXPECT_EQ(reflection.bindingCount,1u,"in-place output");
  AE_EXPECT_EQ(reflection.localSize[0],128u,"line group");
  AE_EXPECT_TRUE(reflection.hasPushConstants,"axis and size");
  AE_EXPECT_TRUE(ae::rhi::reflectComputeShader(ae::rhi::shaders::kWater_Foam_UpdateCompSpirv,
      ae::rhi::shaders::kWater_Foam_UpdateCompSpirvSize,reflection),"foam reflection");
  AE_EXPECT_EQ(reflection.bindingCount,1u,"surface and history share buffer");
  AE_EXPECT_EQ(reflection.localSize[0],64u,"foam group");
  AE_EXPECT_TRUE(reflection.hasPushConstants,"foam rates and reset");
}

using namespace ae;
using namespace ae::rhi;
using namespace ae::test;

namespace {
ComputeLimits limits() {
  ComputeLimits result;
  result.supported = true;
  result.maximumWorkGroupCount[0] = 65535;
  result.maximumWorkGroupCount[1] = 65535;
  result.maximumWorkGroupCount[2] = 64;
  result.maximumWorkGroupSize[0] = 1024;
  result.maximumWorkGroupSize[1] = 1024;
  result.maximumWorkGroupSize[2] = 64;
  result.maximumWorkGroupInvocations = 1024;
  result.maximumPushConstantBytes = 128;
  return result;
}
}

AE_TEST(Compute_kernel_desc_valida_spirv_bindings_e_push_constants) {
  const u32 spirv[5] = {0x07230203u, 0, 0, 0, 0};
  const ComputeBindingDesc bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };
  ComputeKernelDesc desc;
  desc.spirv = spirv; desc.spirvBytes = sizeof(spirv);
  desc.bindings = bindings; desc.bindingCount = 2; desc.pushConstantBytes = 16;
  AE_EXPECT_TRUE(isComputeKernelDescValid(desc, limits()), "descriptor compute valido deve ser aceito");
}

AE_TEST(Compute_kernel_desc_recusa_binding_duplicado) {
  const u32 spirv[5] = {0x07230203u, 0, 0, 0, 0};
  const ComputeBindingDesc bindings[] = {
      {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
  };
  ComputeKernelDesc desc;
  desc.spirv = spirv; desc.spirvBytes = sizeof(spirv);
  desc.bindings = bindings; desc.bindingCount = 2;
  AE_EXPECT_TRUE(!isComputeKernelDescValid(desc, limits()), "dois bindings com o mesmo indice devem falhar antes do Vulkan");
}

AE_TEST(Compute_dispatch_respeita_limites_do_device) {
  ComputeLimits deviceLimits = limits();
  AE_EXPECT_TRUE(isComputeDispatchValid({65535, 1, 1}, deviceLimits), "dispatch no limite deve ser valido");
  AE_EXPECT_TRUE(!isComputeDispatchValid({65536, 1, 1}, deviceLimits), "dispatch acima do limite deve falhar");
  AE_EXPECT_TRUE(!isComputeDispatchValid({0, 1, 1}, deviceLimits), "grupo de trabalho nulo deve falhar");
}

AE_TEST(Compute_local_size_refletido_respeita_eixos_e_total_do_device) {
  ComputeShaderReflection reflection{};
  reflection.localSize[0] = 8;
  reflection.localSize[1] = 8;
  reflection.localSize[2] = 1;
  AE_EXPECT_TRUE(isComputeLocalSizeValid(reflection, limits()),
                 "grupo local 8x8 deve caber no perfil de teste");
  ComputeLimits narrow = limits();
  narrow.maximumWorkGroupSize[0] = 4;
  AE_EXPECT_TRUE(!isComputeLocalSizeValid(reflection, narrow),
                 "eixo local acima do limite deve falhar antes do Vulkan");
  narrow = limits();
  narrow.maximumWorkGroupInvocations = 32;
  AE_EXPECT_TRUE(!isComputeLocalSizeValid(reflection, narrow),
                 "produto de invocacoes acima do limite deve falhar");
}

AE_TEST(Compute_dispatch_indireto_exige_alinhamento_e_comando_completo) {
  AE_EXPECT_TRUE(isComputeIndirectOffsetValid(4, 16), "offset alinhado com 12 bytes restantes deve ser valido");
  AE_EXPECT_TRUE(!isComputeIndirectOffsetValid(2, 16), "offset indireto precisa ser multiplo de quatro");
  AE_EXPECT_TRUE(!isComputeIndirectOffsetValid(8, 16), "buffer truncado nao contem VkDispatchIndirectCommand completo");
}

AE_TEST(Compute_reflection_descobre_layout_do_kernel_astc_real) {
  ComputeShaderReflection reflection{};
  AE_EXPECT_TRUE(reflectComputeShader(shaders::kAstc_EncodeCompSpirv,
                                     shaders::kAstc_EncodeCompSpirvSize, reflection),
                 "SPIR-V compute real deve ser refletido sem manifest duplicado");
  AE_EXPECT_TRUE(reflection.localSize[0] == 8 && reflection.localSize[1] == 8 &&
                 reflection.localSize[2] == 1, "local size 8x8x1 deve vir do binario");
  AE_EXPECT_TRUE(reflection.bindingCount == 2, "ASTC declara imagem e buffer");
  AE_EXPECT_TRUE(reflection.bindings[0].binding == 0 &&
                 reflection.bindings[0].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 "binding zero deve refletir storage image");
  AE_EXPECT_TRUE(reflection.bindings[1].binding == 1 &&
                 reflection.bindings[1].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 "binding um deve refletir storage buffer");
  AE_EXPECT_TRUE(reflection.hasPushConstants, "bloco de push constants deve ser detectado");
}

AE_TEST(Compute_reflection_confirma_contrato_do_kernel_de_oclusao) {
  // O kernel de oclusão é o consumidor do HZB construído em compute. O renderer
  // declara este mesmo conjunto de bindings ao criar o VulkanComputeKernel; se
  // o shader e a declaração divergirem, a criação falha em runtime no aparelho.
  // Refletir o binário aqui move essa falha para o build do host.
  ComputeShaderReflection reflection{};
  AE_EXPECT_TRUE(reflectComputeShader(shaders::kDraw_CullCompSpirv,
                                     shaders::kDraw_CullCompSpirvSize, reflection),
                 "SPIR-V do culling deve ser refletido");
  AE_EXPECT_TRUE(reflection.localSize[0] == ae::renderer::GpuCullLocalSizeX &&
                 reflection.localSize[1] == 1 && reflection.localSize[2] == 1,
                 "local size do binario deve ser o mesmo que dimensiona o dispatch");
  AE_EXPECT_EQ(reflection.bindingCount, 10u,
               "quatro buffers mais os seis niveis da piramide");
  for (u32 index = 0; index < 4; ++index) {
    AE_EXPECT_TRUE(reflection.bindings[index].binding == index &&
                   reflection.bindings[index].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                   "bindings zero a tres sao storage buffers");
  }
  for (u32 index = 4; index < 10; ++index) {
    AE_EXPECT_TRUE(reflection.bindings[index].binding == index &&
                   reflection.bindings[index].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   "bindings quatro a nove sao niveis amostrados do HZB");
  }
  AE_EXPECT_TRUE(reflection.hasPushConstants, "bloco de push constants deve ser detectado");

  ComputeKernelDesc desc{};
  desc.spirv = shaders::kDraw_CullCompSpirv;
  desc.spirvBytes = shaders::kDraw_CullCompSpirvSize;
  desc.bindings = reflection.bindings;
  desc.bindingCount = reflection.bindingCount;
  desc.pushConstantBytes = sizeof(ae::renderer::GpuCullParameters);
  AE_EXPECT_TRUE(isComputeKernelDescValid(desc, limits()),
                 "contrato refletido deve caber nos limites minimos do perfil");
  AE_EXPECT_TRUE(isComputeLocalSizeValid(reflection, limits()),
                 "grupo local do culling deve caber nos limites minimos");
}
