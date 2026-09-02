#include "rhi/render_graph_vulkan.h"

#include <cstring>

namespace ae::rhi {

bool graphStageToVulkan(const char *name, VkPipelineStageFlags &outFlags) {
  if (name == nullptr) return false;
  if (std::strcmp(name, "ColorAttachmentOutput") == 0)
    outFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  else if (std::strcmp(name, "EarlyFragmentTests") == 0)
    outFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  else if (std::strcmp(name, "LateFragmentTests") == 0)
    outFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  else if (std::strcmp(name, "FragmentShader") == 0)
    outFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  else if (std::strcmp(name, "ComputeShader") == 0)
    outFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  else if (std::strcmp(name, "Transfer") == 0)
    outFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  else if (std::strcmp(name, "DrawIndirect") == 0)
    outFlags = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
  else return false;
  return true;
}

bool graphAccessToVulkan(const char *name, VkAccessFlags &outFlags) {
  if (name == nullptr) return false;
  if (std::strcmp(name, "ColorAttachmentWrite") == 0)
    outFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  else if (std::strcmp(name, "DepthStencilAttachmentWrite") == 0)
    outFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  else if (std::strcmp(name, "ShaderRead") == 0 ||
           std::strcmp(name, "ShaderStorageRead") == 0)
    outFlags = VK_ACCESS_SHADER_READ_BIT;
  else if (std::strcmp(name, "UniformRead") == 0)
    outFlags = VK_ACCESS_UNIFORM_READ_BIT;
  else if (std::strcmp(name, "ShaderStorageWrite") == 0)
    outFlags = VK_ACCESS_SHADER_WRITE_BIT;
  else if (std::strcmp(name, "IndirectCommandRead") == 0)
    outFlags = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
  else if (std::strcmp(name, "TransferRead") == 0)
    outFlags = VK_ACCESS_TRANSFER_READ_BIT;
  else if (std::strcmp(name, "TransferWrite") == 0)
    outFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
  else return false;
  return true;
}

bool graphLayoutToVulkan(rendergraph::ResourceLayout layout, VkImageLayout &outLayout) {
  switch (layout) {
    case rendergraph::ResourceLayout::Undefined: outLayout = VK_IMAGE_LAYOUT_UNDEFINED; return true;
    case rendergraph::ResourceLayout::ColorAttachment:
      outLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; return true;
    case rendergraph::ResourceLayout::DepthStencilAttachment:
      outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; return true;
    case rendergraph::ResourceLayout::ShaderReadOnly:
      outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; return true;
    case rendergraph::ResourceLayout::General: outLayout = VK_IMAGE_LAYOUT_GENERAL; return true;
  }
  return false;
}

} // namespace ae::rhi
