#pragma once
#include "renderer/terrain_pipeline_capabilities_vulkan.h"
#include <memory>
namespace ae::renderer {
// The publication-wide union deliberately shares a capability mask across all
// draws. Each ORIGINAL pipeline retains its own recipe/epoch and full render
// state. No indirect batch split, sorting change or material override assumption.
class TerrainPipelinePublicationOwner final {
 struct Recipe {
  VkPipeline generic{};TerrainPipelineCapabilitiesKey key;
  VkGraphicsPipelineCreateInfo info{};
  std::vector<VkPipelineShaderStageCreateInfo> stages;
  std::vector<VkSpecializationMapEntry> specialEntries;
  std::vector<u8> specialBytes;VkSpecializationInfo special{};
  VkPipelineVertexInputStateCreateInfo vertex{};
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  VkPipelineViewportStateCreateInfo viewport{};
  std::vector<VkViewport> viewports;std::vector<VkRect2D> scissors;
  VkPipelineRasterizationStateCreateInfo raster{};
  VkPipelineMultisampleStateCreateInfo multisample{};std::vector<VkSampleMask> sampleMasks;
  VkPipelineDepthStencilStateCreateInfo depth{};
  VkPipelineColorBlendStateCreateInfo blend{};std::vector<VkPipelineColorBlendAttachmentState> attachments;
  VkPipelineDynamicStateCreateInfo dynamic{};std::vector<VkDynamicState> dynamicStates;
  bool capture(VkPipeline original,const VkGraphicsPipelineCreateInfo &source,u64 epoch){
   if(!original||source.pNext||source.pTessellationState||!source.pStages||source.stageCount!=2||
      !source.pVertexInputState||!source.pInputAssemblyState||!source.pViewportState||
      !source.pRasterizationState||!source.pMultisampleState||!source.pDepthStencilState||
      !source.pColorBlendState||!source.pDynamicState)return false;
   if(source.pVertexInputState->pNext||source.pInputAssemblyState->pNext||source.pViewportState->pNext||
      source.pRasterizationState->pNext||source.pMultisampleState->pNext||source.pDepthStencilState->pNext||
      source.pColorBlendState->pNext||source.pDynamicState->pNext)return false;
   generic=original;info=source;key.pipelineEpoch=epoch;
   stages.assign(source.pStages,source.pStages+source.stageCount);
   VkPipelineShaderStageCreateInfo *fragment=nullptr;
   for(auto &stage:stages){if(stage.pNext||!stage.pName||std::strcmp(stage.pName,"main"))return false;stage.pName="main";
    if(stage.stage==VK_SHADER_STAGE_FRAGMENT_BIT){if(fragment)return false;fragment=&stage;}
    else if(stage.stage!=VK_SHADER_STAGE_VERTEX_BIT||stage.pSpecializationInfo)return false;
   }
   if(!fragment||!fragment->pSpecializationInfo)return false;
   const auto &s=*fragment->pSpecializationInfo;
   if(s.mapEntryCount!=6||!s.pMapEntries||!s.pData||s.dataSize>65536)return false;
   specialEntries.assign(s.pMapEntries,s.pMapEntries+s.mapEntryCount);specialBytes.resize(s.dataSize);std::memcpy(specialBytes.data(),s.pData,s.dataSize);
   std::array<bool,6> seen{};
   for(const auto &m:specialEntries){if(m.constantID>=6||seen[m.constantID]||m.size!=4||u64(m.offset)+4>s.dataSize)return false;seen[m.constantID]=true;std::memcpy(&key.existingSpecialization[m.constantID],specialBytes.data()+m.offset,4);}
   vertex=*source.pVertexInputState;
   if((vertex.vertexBindingDescriptionCount&&!vertex.pVertexBindingDescriptions)||(vertex.vertexAttributeDescriptionCount&&!vertex.pVertexAttributeDescriptions))return false;
   if(vertex.vertexBindingDescriptionCount)bindings.assign(vertex.pVertexBindingDescriptions,vertex.pVertexBindingDescriptions+vertex.vertexBindingDescriptionCount);
   if(vertex.vertexAttributeDescriptionCount)attributes.assign(vertex.pVertexAttributeDescriptions,vertex.pVertexAttributeDescriptions+vertex.vertexAttributeDescriptionCount);
   assembly=*source.pInputAssemblyState;viewport=*source.pViewportState;
   if(viewport.pViewports)viewports.assign(viewport.pViewports,viewport.pViewports+viewport.viewportCount);
   if(viewport.pScissors)scissors.assign(viewport.pScissors,viewport.pScissors+viewport.scissorCount);
   raster=*source.pRasterizationState;multisample=*source.pMultisampleState;
   if(multisample.pSampleMask){u32 count=(u32(multisample.rasterizationSamples)+31)/32;if(!count||count>2)return false;sampleMasks.assign(multisample.pSampleMask,multisample.pSampleMask+count);}
   depth=*source.pDepthStencilState;blend=*source.pColorBlendState;
   if(blend.attachmentCount&&!blend.pAttachments)return false;
   if(blend.attachmentCount)attachments.assign(blend.pAttachments,blend.pAttachments+blend.attachmentCount);
   dynamic=*source.pDynamicState;
   if(dynamic.dynamicStateCount&&!dynamic.pDynamicStates)return false;
   if(dynamic.dynamicStateCount)dynamicStates.assign(dynamic.pDynamicStates,dynamic.pDynamicStates+dynamic.dynamicStateCount);
   key.family=!attachments.empty()&&attachments[0].blendEnable?TerrainPipelineFamily::Blend:
      depth.depthCompareOp==VK_COMPARE_OP_EQUAL?TerrainPipelineFamily::Coverage:TerrainPipelineFamily::Opaque;
   return true;
  }
  VkGraphicsPipelineCreateInfo view(){
   special={u32(specialEntries.size()),specialEntries.data(),specialBytes.size(),specialBytes.data()};
   for(auto &stage:stages)if(stage.stage==VK_SHADER_STAGE_FRAGMENT_BIT)stage.pSpecializationInfo=&special;
   info.pStages=stages.data();vertex.pVertexBindingDescriptions=bindings.data();vertex.pVertexAttributeDescriptions=attributes.data();
   info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;
   viewport.pViewports=viewports.empty()?nullptr:viewports.data();viewport.pScissors=scissors.empty()?nullptr:scissors.data();info.pViewportState=&viewport;
   info.pRasterizationState=&raster;multisample.pSampleMask=sampleMasks.empty()?nullptr:sampleMasks.data();info.pMultisampleState=&multisample;
   info.pDepthStencilState=&depth;blend.pAttachments=attachments.data();info.pColorBlendState=&blend;
   dynamic.pDynamicStates=dynamicStates.data();info.pDynamicState=&dynamic;return info;
  }
 };
 VkDevice device_;PFN_vkDestroyShaderModule destroyShader_;
 TerrainPipelineCapabilitiesVulkan pipelines_;
 std::map<VkPipeline,std::unique_ptr<Recipe>> recipes_;
 std::vector<VkShaderModule> modules_;u64 nextEpoch_=1;
 u32 activeMask_=TerrainCapabilityGeneric,preparedMask_=TerrainCapabilityGeneric;
public:
 TerrainPipelinePublicationOwner(VkDevice device,PFN_vkCreateGraphicsPipelines create,
    PFN_vkDestroyPipeline destroy,PFN_vkDestroyShaderModule destroyShader):device_(device),destroyShader_(destroyShader),pipelines_(device,create,destroy){}
 void retainModule(VkShaderModule module){if(module&&std::find(modules_.begin(),modules_.end(),module)==modules_.end())modules_.push_back(module);}
 bool capture(VkPipeline generic,const VkGraphicsPipelineCreateInfo &source){
  if(recipes_.contains(generic))return false;auto recipe=std::make_unique<Recipe>();
  if(!recipe->capture(generic,source,nextEpoch_++))return false;recipes_.emplace(generic,std::move(recipe));return true;
 }
 void beginPublicationAtIdle(){activeMask_=TerrainCapabilityGeneric;}
 void commitPublicationAtIdle(u32 mask,VkPipelineCache driver){
  activeMask_=TerrainCapabilityGeneric;if(mask>TerrainCapabilityKnown)return;
  // One publication-wide mask is active. At this explicit idle point prior
  // mask pipelines cannot be referenced by frames, so release them instead of
  // filling the64-entry bound with whole historical families. Unchanged paint
  // keys retain all handles, including negative results; no driver retry.
  if(mask!=preparedMask_){pipelines_.clearAtIdle();preparedMask_=mask;}
  for(auto &[generic,recipe]:recipes_){auto key=recipe->key;key.mask=mask;auto info=recipe->view();pipelines_.prepareAtIdle(key,info,generic,driver);}
  activeMask_=mask;
 }
 VkPipeline select(VkPipeline generic)const{
  if(activeMask_>TerrainCapabilityKnown)return generic;auto found=recipes_.find(generic);if(found==recipes_.end())return generic;
  auto key=found->second->key;key.mask=activeMask_;return pipelines_.select(key,generic);
 }
 void clearAtIdle(){activeMask_=preparedMask_=TerrainCapabilityGeneric;pipelines_.clearAtIdle();recipes_.clear();for(auto module:modules_)if(destroyShader_)destroyShader_(device_,module,nullptr);modules_.clear();}
 usize recipeCount()const{return recipes_.size();}
};
}
