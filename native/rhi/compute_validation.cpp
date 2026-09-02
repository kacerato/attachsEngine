#include "rhi/compute.h"

#include <algorithm>
#include <vector>

namespace ae::rhi {
namespace {
bool supportedDescriptor(VkDescriptorType type) {
  return type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
         type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || type == VK_DESCRIPTOR_TYPE_SAMPLER;
}
}

bool isComputeKernelDescValid(const ComputeKernelDesc &desc, const ComputeLimits &limits) {
  if (!limits.supported || desc.spirv == nullptr || desc.spirvBytes < 20 ||
      (desc.spirvBytes & 3u) != 0 || desc.spirv[0] != 0x07230203u ||
      desc.bindingCount > 16 || (desc.bindingCount > 0 && desc.bindings == nullptr) ||
      desc.entryPoint == nullptr || desc.entryPoint[0] == '\0' ||
      desc.pushConstantBytes > limits.maximumPushConstantBytes ||
      (desc.pushConstantBytes & 3u) != 0) return false;
  for (u32 i = 0; i < desc.bindingCount; ++i) {
    const auto &binding = desc.bindings[i];
    if (!supportedDescriptor(binding.type) || binding.descriptorCount != 1) return false;
    for (u32 prior = 0; prior < i; ++prior)
      if (desc.bindings[prior].binding == binding.binding) return false;
  }
  return true;
}

bool isComputeDispatchValid(const ComputeDispatch &dispatch, const ComputeLimits &limits) {
  return limits.supported && dispatch.x > 0 && dispatch.y > 0 && dispatch.z > 0 &&
         dispatch.x <= limits.maximumWorkGroupCount[0] &&
         dispatch.y <= limits.maximumWorkGroupCount[1] &&
         dispatch.z <= limits.maximumWorkGroupCount[2];
}

bool isComputeLocalSizeValid(const ComputeShaderReflection &reflection,
                             const ComputeLimits &limits) {
  if (!limits.supported || limits.maximumWorkGroupInvocations == 0) return false;
  u64 invocations = 1;
  for (u32 axis = 0; axis < 3; ++axis) {
    if (reflection.localSize[axis] == 0 ||
        reflection.localSize[axis] > limits.maximumWorkGroupSize[axis]) return false;
    invocations *= reflection.localSize[axis];
  }
  return invocations <= limits.maximumWorkGroupInvocations;
}

bool isComputeIndirectOffsetValid(u64 offsetBytes, u64 bufferSizeBytes) {
  return (offsetBytes & 3u) == 0 && offsetBytes <= bufferSizeBytes &&
         bufferSizeBytes - offsetBytes >= sizeof(VkDispatchIndirectCommand);
}

bool reflectComputeShader(const u32 *spirv, usize spirvBytes, ComputeShaderReflection &out) {
  out = {};
  if (spirv == nullptr || spirvBytes < 20 || (spirvBytes & 3u) != 0 ||
      spirv[0] != 0x07230203u || spirv[3] == 0 || spirv[3] > (1u << 24)) return false;
  struct IdInfo {
    u16 opcode = 0;
    u32 storageClass = UINT32_MAX;
    u32 pointee = 0;
    u32 elementType = 0;
    u32 sampled = 0;
    i32 descriptorSet = -1;
    i32 binding = -1;
    bool block = false;
    bool bufferBlock = false;
  };
  std::vector<IdInfo> ids(spirv[3]);
  const usize wordCount = spirvBytes / sizeof(u32);
  u32 computeEntry = 0;
  bool localSizeFound = false;
  for (usize offset = 5; offset < wordCount;) {
    const u16 instructionWords = static_cast<u16>(spirv[offset] >> 16u);
    const u16 opcode = static_cast<u16>(spirv[offset] & 0xffffu);
    if (instructionWords == 0 || offset + instructionWords > wordCount) return false;
    auto validId = [&](u32 id) { return id > 0 && id < ids.size(); };
    if (opcode == 15 && instructionWords >= 3 && spirv[offset + 1] == 5) { // OpEntryPoint GLCompute
      computeEntry = spirv[offset + 2];
    } else if (opcode == 16 && instructionWords >= 6 && spirv[offset + 2] == 17 &&
               (computeEntry == 0 || spirv[offset + 1] == computeEntry)) { // LocalSize
      out.localSize[0] = spirv[offset + 3]; out.localSize[1] = spirv[offset + 4];
      out.localSize[2] = spirv[offset + 5]; localSizeFound = true;
    } else if (opcode == 71 && instructionWords >= 3 && validId(spirv[offset + 1])) { // OpDecorate
      IdInfo &id = ids[spirv[offset + 1]];
      const u32 decoration = spirv[offset + 2];
      if (decoration == 2) id.block = true;
      else if (decoration == 3) id.bufferBlock = true;
      else if (decoration == 33 && instructionWords >= 4) id.binding = static_cast<i32>(spirv[offset + 3]);
      else if (decoration == 34 && instructionWords >= 4) id.descriptorSet = static_cast<i32>(spirv[offset + 3]);
    } else if ((opcode == 25 || opcode == 26 || opcode == 27 || opcode == 28 ||
                opcode == 29 || opcode == 30) && instructionWords >= 2 && validId(spirv[offset + 1])) {
      IdInfo &id = ids[spirv[offset + 1]]; id.opcode = opcode;
      if (opcode == 25 && instructionWords >= 9) id.sampled = spirv[offset + 7];
      else if ((opcode == 27 || opcode == 28 || opcode == 29) && instructionWords >= 3)
        id.elementType = spirv[offset + 2];
    } else if (opcode == 32 && instructionWords >= 4 && validId(spirv[offset + 1])) { // OpTypePointer
      IdInfo &id = ids[spirv[offset + 1]]; id.opcode = opcode;
      id.storageClass = spirv[offset + 2]; id.pointee = spirv[offset + 3];
    } else if (opcode == 59 && instructionWords >= 4 && validId(spirv[offset + 2])) { // OpVariable
      IdInfo &id = ids[spirv[offset + 2]]; id.opcode = opcode;
      id.pointee = spirv[offset + 1]; id.storageClass = spirv[offset + 3];
    }
    offset += instructionWords;
  }
  if (computeEntry == 0 || !localSizeFound || out.localSize[0] == 0 ||
      out.localSize[1] == 0 || out.localSize[2] == 0) return false;

  for (u32 variableId = 1; variableId < ids.size(); ++variableId) {
    const IdInfo &variable = ids[variableId];
    if (variable.opcode != 59) continue;
    if (variable.storageClass == 9) { out.hasPushConstants = true; continue; }
    if (variable.binding < 0) continue;
    if (variable.descriptorSet != 0 || out.bindingCount >= 16 || !variable.pointee ||
        variable.pointee >= ids.size()) return false;
    const IdInfo &pointer = ids[variable.pointee];
    u32 typeId = pointer.pointee;
    if (typeId == 0 || typeId >= ids.size()) return false;
    while (ids[typeId].opcode == 28 || ids[typeId].opcode == 29) {
      typeId = ids[typeId].elementType;
      if (typeId == 0 || typeId >= ids.size()) return false;
    }
    const IdInfo &type = ids[typeId];
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    if (variable.storageClass == 12) descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    else if (variable.storageClass == 2 && type.opcode == 30)
      descriptorType = type.bufferBlock ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                        : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    else if (variable.storageClass == 0 && type.opcode == 25)
      descriptorType = type.sampled == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                         : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    else if (variable.storageClass == 0 && type.opcode == 26)
      descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    else if (variable.storageClass == 0 && type.opcode == 27)
      descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    if (!supportedDescriptor(descriptorType)) return false;
    out.bindings[out.bindingCount++] = {
        static_cast<u32>(variable.binding), descriptorType, 1};
  }
  std::sort(out.bindings, out.bindings + out.bindingCount,
            [](const ComputeBindingDesc &a, const ComputeBindingDesc &b) {
              return a.binding < b.binding;
            });
  for (u32 i = 1; i < out.bindingCount; ++i)
    if (out.bindings[i - 1].binding == out.bindings[i].binding) return false;
  return true;
}

} // namespace ae::rhi
