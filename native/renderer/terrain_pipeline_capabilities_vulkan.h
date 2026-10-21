#pragma once
#include "renderer/terrain_pipeline_capabilities.h"
#include <vulkan/vulkan.h>
#include <map>
#include <cstring>
namespace ae::renderer {
// Device-owned VkPipeline objects. This does NOT own/create a VkPipelineCache;
// pass the existing device.pipelineCache().driverHandle() to prepareAtIdle.
// No evictions while frames may reference an object. clearAtIdle is mandatory
// before shader/layout/renderpass/device destruction. Generic remains caller-owned.
class TerrainPipelineCapabilitiesVulkan final {
 struct Entry {VkPipeline pipeline=VK_NULL_HANDLE;};
 std::map<TerrainPipelineCapabilitiesKey,Entry> entries_;
 VkDevice device_=VK_NULL_HANDLE;
 PFN_vkCreateGraphicsPipelines create_=nullptr;
 PFN_vkDestroyPipeline destroy_=nullptr;
public:
 static constexpr usize MaximumEntries=64;
 TerrainPipelineCapabilitiesVulkan(VkDevice device,PFN_vkCreateGraphicsPipelines create,
    PFN_vkDestroyPipeline destroy):device_(device),create_(create),destroy_(destroy){}
 TerrainPipelineCapabilitiesVulkan(const TerrainPipelineCapabilitiesVulkan&)=delete;
 TerrainPipelineCapabilitiesVulkan&operator=(const TerrainPipelineCapabilitiesVulkan&)=delete;
 // Destruction intentionally performs no unsynchronized Vulkan operations.
 // Owner must call clearAtIdle after its existing vkDeviceWaitIdle safe point.
 usize entries()const{return entries_.size();}
 VkPipeline select(const TerrainPipelineCapabilitiesKey &key,VkPipeline generic)const{
  if(!key.specialized())return generic;auto found=entries_.find(key);
  return found!=entries_.end()&&found->second.pipeline?found->second.pipeline:generic;
 }
 VkPipeline prepareAtIdle(const TerrainPipelineCapabilitiesKey &key,
    const VkGraphicsPipelineCreateInfo &original,VkPipeline generic,VkPipelineCache driverCache){
  if(!key.specialized()||!generic||!device_||!create_||!destroy_)return generic;
  if(entries_.contains(key))return select(key,generic);
  if(entries_.size()>=MaximumEntries)return generic;
  // Negative entries suppress retries on every brush publication. Retry only
  // after explicit owner invalidation, never because texture pixels changed.
  auto [entry,inserted]=entries_.emplace(key,Entry{});(void)inserted;
  if(!original.pStages||!original.stageCount||original.basePipelineHandle||original.flags&VK_PIPELINE_CREATE_DERIVATIVE_BIT)return generic;
  std::vector<VkPipelineShaderStageCreateInfo> stages(original.pStages,original.pStages+original.stageCount);
  VkPipelineShaderStageCreateInfo *fragment=nullptr;
  for(auto &stage:stages)if(stage.stage==VK_SHADER_STAGE_FRAGMENT_BIT){if(fragment)return generic;fragment=&stage;}
  if(!fragment)return generic;
  std::vector<VkSpecializationMapEntry> mappings;std::vector<u8> bytes;
  if(const auto *old=fragment->pSpecializationInfo){
   if((old->mapEntryCount&&!old->pMapEntries)||(old->dataSize&&!old->pData)||old->dataSize>65536)return generic;
   if(old->mapEntryCount)mappings.assign(old->pMapEntries,old->pMapEntries+old->mapEntryCount);
   if(old->dataSize){bytes.resize(old->dataSize);std::memcpy(bytes.data(),old->pData,old->dataSize);}
  }
  std::array<bool,6> seen{};
  for(const auto &mapping:mappings){
   // Unknown existing constants or duplicate IDs cannot silently select a
   // pipeline compiled under a different key. ID6 is reserved for this owner.
   if(mapping.constantID>=6||seen[mapping.constantID]||mapping.size!=sizeof(u32)||u64(mapping.offset)+mapping.size>bytes.size())return generic;
   seen[mapping.constantID]=true;u32 value;std::memcpy(&value,bytes.data()+mapping.offset,sizeof(value));
   if(value!=key.existingSpecialization[mapping.constantID])return generic;
  }
  // Require explicit values for IDs0..5. Never guess GLSL defaults or drop a
  // diagnostic/material/distant/clipping/lighting specialization during clone.
  for(bool present:seen)if(!present)return generic;
  const u32 offset=u32(bytes.size());bytes.resize(bytes.size()+sizeof(u32));std::memcpy(bytes.data()+offset,&key.mask,sizeof(key.mask));
  mappings.push_back({6,offset,sizeof(u32)});
  VkSpecializationInfo specialization{u32(mappings.size()),mappings.data(),bytes.size(),bytes.data()};fragment->pSpecializationInfo=&specialization;
  auto info=original;info.pStages=stages.data();VkPipeline created=VK_NULL_HANDLE;
  const auto result=create_(device_,driverCache,1,&info,nullptr,&created);
  if(result==VK_SUCCESS&&created)entry->second.pipeline=created;
  else if(created)destroy_(device_,created,nullptr);
  return select(key,generic);
 }
 void clearAtIdle(){for(auto &[key,entry]:entries_)if(entry.pipeline)destroy_(device_,entry.pipeline,nullptr);entries_.clear();}
};
}
