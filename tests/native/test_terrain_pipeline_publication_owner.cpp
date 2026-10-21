#include "harness.h"
#include "renderer/terrain_pipeline_publication_owner.h"
#include <type_traits>
using namespace ae;using namespace ae::renderer;
namespace {
template<class T>T object(u64 n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(n);else return T(n);}
struct State {u32 creates=0,pipelinesDestroyed=0,modulesDestroyed=0;bool correct=true;std::vector<u32> families;} state;
VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice,VkPipelineCache cache,u32 count,const VkGraphicsPipelineCreateInfo *info,const VkAllocationCallbacks*,VkPipeline *out){
 ++state.creates;state.correct&=cache==object<VkPipelineCache>(9)&&count==1&&info->stageCount==2;
 state.correct&=info->layout==object<VkPipelineLayout>(4)&&info->renderPass==object<VkRenderPass>(5);
 state.correct&=info->pVertexInputState->vertexBindingDescriptionCount==1&&info->pVertexInputState->pVertexBindingDescriptions[0].stride==48;
 state.correct&=info->pVertexInputState->pVertexAttributeDescriptions[0].location==3;
 state.correct&=info->pRasterizationState->frontFace==VK_FRONT_FACE_CLOCKWISE&&info->pRasterizationState->lineWidth==1;
 state.correct&=info->pDynamicState->dynamicStateCount==3&&info->pDynamicState->pDynamicStates[2]==VK_DYNAMIC_STATE_CULL_MODE_EXT;
 const auto &s=*info->pStages[1].pSpecializationInfo;state.correct&=s.mapEntryCount==7;
 u32 specialization0=99;for(u32 i=0;i<s.mapEntryCount;++i)if(s.pMapEntries[i].constantID==0)std::memcpy(&specialization0,static_cast<const u8*>(s.pData)+s.pMapEntries[i].offset,4);
 state.correct&=specialization0==0;
 const u32 family=info->pColorBlendState->pAttachments[0].blendEnable?2u:info->pDepthStencilState->depthCompareOp==VK_COMPARE_OP_EQUAL?1u:0u;
 state.correct&=info->pDepthStencilState->depthWriteEnable==(family==0?VK_TRUE:VK_FALSE);
 state.correct&=info->subpass==(family==2?1u:0u);
 if(family==2){state.correct&=info->pColorBlendState->attachmentCount==3;for(u32 n=1;n<info->pColorBlendState->attachmentCount;++n)state.correct&=info->pColorBlendState->pAttachments[n].colorWriteMask==VK_COLOR_COMPONENT_R_BIT&&info->pColorBlendState->pAttachments[n].colorBlendOp==VK_BLEND_OP_MAX;}
 state.families.push_back(family);
 *out=object<VkPipeline>(1000+state.creates);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy(VkDevice,VkPipeline,const VkAllocationCallbacks*){++state.pipelinesDestroyed;}
VKAPI_ATTR void VKAPI_CALL destroyShader(VkDevice,VkShaderModule,const VkAllocationCallbacks*){state.correct&=state.pipelinesDestroyed==state.creates;++state.modulesDestroyed;}
struct Fixture {
 std::array<u32,6> values{0,0xffffffffu,0,0,1,0};std::array<VkSpecializationMapEntry,6> maps{};VkSpecializationInfo special{};
 std::array<VkPipelineShaderStageCreateInfo,2> stages{};
 VkVertexInputBindingDescription binding{0,48,VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attribute{3,0,VK_FORMAT_R8G8B8A8_UNORM,44};
 VkPipelineVertexInputStateCreateInfo vertex{};
 VkPipelineInputAssemblyStateCreateInfo assembly{};
 VkPipelineViewportStateCreateInfo viewport{};
 VkPipelineRasterizationStateCreateInfo raster{};
 VkPipelineMultisampleStateCreateInfo multisample{};
 VkPipelineDepthStencilStateCreateInfo depth{};
 VkPipelineColorBlendAttachmentState attachment{};VkPipelineColorBlendStateCreateInfo blend{};
 std::array<VkDynamicState,3> states{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_CULL_MODE_EXT};
 VkPipelineDynamicStateCreateInfo dynamic{};
 VkGraphicsPipelineCreateInfo info{};
 Fixture(){vertex.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;depth.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;info.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  for(u32 i=0;i<6;++i)maps[i]={i,i*4,4};special={6,maps.data(),sizeof(values),values.data()};
  for(auto &s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}
  stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=object<VkShaderModule>(2);stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=object<VkShaderModule>(3);stages[1].pSpecializationInfo=&special;
  vertex.vertexBindingDescriptionCount=vertex.vertexAttributeDescriptionCount=1;vertex.pVertexBindingDescriptions=&binding;vertex.pVertexAttributeDescriptions=&attribute;
  assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;viewport.viewportCount=viewport.scissorCount=1;raster.frontFace=VK_FRONT_FACE_CLOCKWISE;raster.lineWidth=1;multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
  depth.depthTestEnable=depth.depthWriteEnable=VK_TRUE;depth.depthCompareOp=VK_COMPARE_OP_LESS;attachment.colorWriteMask=15;blend.attachmentCount=1;blend.pAttachments=&attachment;dynamic.dynamicStateCount=3;dynamic.pDynamicStates=states.data();
  info.stageCount=2;info.pStages=stages.data();info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&multisample;info.pDepthStencilState=&depth;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;info.layout=object<VkPipelineLayout>(4);info.renderPass=object<VkRenderPass>(5);
 }
};
}
AE_TEST(terrain_publication_owner_deep_copies_expired_creation_recipe_and_routes_real_variants){
 state={};TerrainPipelinePublicationOwner owner(object<VkDevice>(1),create,destroy,destroyShader);const auto generic=object<VkPipeline>(7);
 {Fixture f;AE_EXPECT_TRUE(owner.capture(generic,f.info),"capture actual nested creation state");f.values[0]=99;f.binding.stride=0;f.attribute.location=0;f.states[2]=VK_DYNAMIC_STATE_LINE_WIDTH;f.raster.lineWidth=3;}
 AE_EXPECT_TRUE(owner.select(generic)==generic,"uncooked publication is full generic");owner.commitPublicationAtIdle(0,object<VkPipelineCache>(9));AE_EXPECT_TRUE(owner.select(generic)!=generic&&state.correct,"expired stack state cannot change pipeline recipe or six existing constants");
 const auto first=owner.select(generic);owner.beginPublicationAtIdle();AE_EXPECT_TRUE(owner.select(generic)==generic,"failed/unfinished publication cannot keep incompatible old mask");owner.commitPublicationAtIdle(0,object<VkPipelineCache>(9));AE_EXPECT_TRUE(owner.select(generic)==first&&state.creates==1,"same capabilities after paint reuse compiled handle");
 owner.beginPublicationAtIdle();owner.commitPublicationAtIdle(TerrainCapabilityStaticFx,object<VkPipelineCache>(9));AE_EXPECT_TRUE(state.creates==2&&state.pipelinesDestroyed==1,"capability change retires old mask only at idle, avoiding historical-family capacity starvation");owner.clearAtIdle();
}
AE_TEST(terrain_publication_owner_opaque_coverage_blend_remain_distinct_without_batch_rewrite){
 state={};TerrainPipelinePublicationOwner owner(object<VkDevice>(1),create,destroy,destroyShader);Fixture f;
 AE_EXPECT_TRUE(owner.capture(object<VkPipeline>(11),f.info),"opaque");f.depth.depthCompareOp=VK_COMPARE_OP_EQUAL;f.depth.depthWriteEnable=VK_FALSE;AE_EXPECT_TRUE(owner.capture(object<VkPipeline>(12),f.info),"coverage EQUAL");f.attachment.blendEnable=VK_TRUE;f.depth.depthCompareOp=VK_COMPARE_OP_LESS;f.info.subpass=1;
 std::array<VkPipelineColorBlendAttachmentState,3> attachments{};attachments[0]=f.attachment;for(u32 n=1;n<3;++n){attachments[n].colorWriteMask=VK_COLOR_COMPONENT_R_BIT;attachments[n].colorBlendOp=VK_BLEND_OP_MAX;}f.blend.attachmentCount=3;f.blend.pAttachments=attachments.data();AE_EXPECT_TRUE(owner.capture(object<VkPipeline>(13),f.info),"blend and reactive attachments in its own subpass");attachments[1]={};attachments[2]={};
 owner.commitPublicationAtIdle(TerrainCapabilitySnow,object<VkPipelineCache>(9));AE_EXPECT_TRUE(state.correct&&state.families==std::vector<u32>({0,1,2}),"retained recipes preserve all three depth/blend families");
 AE_EXPECT_TRUE(owner.select(object<VkPipeline>(11))!=owner.select(object<VkPipeline>(12))&&owner.select(object<VkPipeline>(12))!=owner.select(object<VkPipeline>(13)),"each existing generic pipeline maps to its own specialized state");AE_EXPECT_TRUE(owner.select(object<VkPipeline>(14))==object<VkPipeline>(14),"coverage depth/water/sky without recipe stay generic");owner.clearAtIdle();
}
AE_TEST(terrain_publication_owner_unknown_table_and_partial_publication_fail_open){
 state={};Fixture f;TerrainPipelinePublicationOwner owner(object<VkDevice>(1),create,destroy,destroyShader);const auto generic=object<VkPipeline>(7);AE_EXPECT_TRUE(owner.capture(generic,f.info),"capture");owner.commitPublicationAtIdle(0,object<VkPipelineCache>(9));owner.beginPublicationAtIdle();
 AE_EXPECT_TRUE(owner.select(generic)==generic,"post-idle resource failure always draws complete generic");owner.commitPublicationAtIdle(TerrainCapabilityGeneric,object<VkPipelineCache>(9));AE_EXPECT_TRUE(owner.select(generic)==generic&&state.creates==1,"unknown union does not compile guessed capabilities");
 f.vertex.pNext=&f;AE_EXPECT_TRUE(!owner.capture(object<VkPipeline>(8),f.info),"unknown pNext retained only by original generic pipeline");AE_EXPECT_EQ(owner.recipeCount(),usize(1),"invalid recipe cannot replace known routing");owner.clearAtIdle();
}
AE_TEST(terrain_publication_owner_retains_modules_once_and_releases_after_pipelines_at_idle){
 state={};Fixture f;TerrainPipelinePublicationOwner owner(object<VkDevice>(1),create,destroy,destroyShader);owner.retainModule(object<VkShaderModule>(2));owner.retainModule(object<VkShaderModule>(3));owner.retainModule(object<VkShaderModule>(2));AE_EXPECT_TRUE(owner.capture(object<VkPipeline>(7),f.info),"capture");owner.commitPublicationAtIdle(TerrainCapabilityStaticFx,object<VkPipelineCache>(9));
 AE_EXPECT_EQ(state.modulesDestroyed,0u,"shader recipes remain usable after initialization");owner.clearAtIdle();AE_EXPECT_TRUE(state.correct&&state.pipelinesDestroyed==1&&state.modulesDestroyed==2,"specialized handles released before retained modules, exactly once");owner.clearAtIdle();AE_EXPECT_EQ(state.modulesDestroyed,2u,"context/shutdown cleanup idempotent");
}
