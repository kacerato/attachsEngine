#include "harness.h"
#include "rhi/render_graph_vulkan.h"

using namespace ae::rhi;
using namespace ae::test;

AE_TEST(RenderGraphVulkan_mapeia_compute_storage_para_flags_reais) {
  VkPipelineStageFlags stage = 0;
  VkAccessFlags access = 0;
  AE_EXPECT_TRUE(graphStageToVulkan("ComputeShader", stage) &&
                 stage == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 "compute stage deve mapear sem heuristica do backend");
  AE_EXPECT_TRUE(graphAccessToVulkan("ShaderStorageWrite", access) &&
                 access == VK_ACCESS_SHADER_WRITE_BIT,
                 "storage write deve produzir acesso Vulkan real");
}

AE_TEST(RenderGraphVulkan_mapeia_indirect_e_layout_general) {
  VkPipelineStageFlags stage = 0;
  VkAccessFlags access = 0;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  AE_EXPECT_TRUE(graphStageToVulkan("DrawIndirect", stage) &&
                 stage == VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                 "draw indirect exige stage especifico");
  AE_EXPECT_TRUE(graphAccessToVulkan("IndirectCommandRead", access) &&
                 access == VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                 "argumentos indiretos exigem access especifico");
  AE_EXPECT_TRUE(graphLayoutToVulkan(ae::rendergraph::ResourceLayout::General, layout) &&
                 layout == VK_IMAGE_LAYOUT_GENERAL,
                 "storage image deve permanecer em General");
}

AE_TEST(RenderGraphVulkan_recusa_nome_desconhecido) {
  VkPipelineStageFlags stage = 0;
  VkAccessFlags access = 0;
  AE_EXPECT_TRUE(!graphStageToVulkan("StageInventado", stage),
                 "backend nao pode aceitar stage sem mapeamento");
  AE_EXPECT_TRUE(!graphAccessToVulkan("AccessInventado", access),
                 "backend nao pode aceitar access sem mapeamento");
}
