#include "harness.h"
#include "renderer/terrain_pipeline_capabilities_vulkan.h"
#include <type_traits>
using namespace ae;using namespace ae::renderer;using namespace ae::resources;
namespace {
TerrainSurfaceBinding surface(){TerrainSurfaceBinding b;TerrainSurfaceLayer l;l.identity={201,1};l.name="Stable";b.surface.layers={l};return b;}
std::vector<float> table(const TerrainSurfaceBinding &b){std::vector<float> out;u32 offset=0;std::string e;if(!appendTerrainSurfaceTable(b,{},out,offset,e))return {};return out;}
template<class T>T handle(u64 n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(n);else return T(n);}
struct Calls {u32 create=0,destroy=0,mask=0;bool fail=false,partial=false,valid=true;VkPipelineCache cache{};} calls;
VKAPI_ATTR VkResult VKAPI_CALL createPipeline(VkDevice,VkPipelineCache cache,u32 count,const VkGraphicsPipelineCreateInfo *info,const VkAllocationCallbacks*,VkPipeline *out){
 ++calls.create;calls.cache=cache;calls.valid&=count==1&&info&&info->stageCount==1;
 const auto *s=info->pStages[0].pSpecializationInfo;calls.valid&=s&&s->mapEntryCount==7;
 if(s)for(u32 n=0;n<s->mapEntryCount;++n){const auto &e=s->pMapEntries[n];if(e.constantID==6)std::memcpy(&calls.mask,static_cast<const u8*>(s->pData)+e.offset,4);}
 *out=calls.fail&&!calls.partial?VK_NULL_HANDLE:handle<VkPipeline>(100+calls.create);return calls.fail?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroyPipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*){++calls.destroy;}
struct Fixture {
 std::array<u32,6> values{0,0xffffffffu,0,1,1,0};std::array<VkSpecializationMapEntry,6> maps{};
 VkSpecializationInfo specialization{};VkPipelineShaderStageCreateInfo stage{};
 VkGraphicsPipelineCreateInfo info{};
 Fixture(){stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;info.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;for(u32 i=0;i<6;++i)maps[i]={i,i*4,4};specialization={6,maps.data(),sizeof(values),values.data()};stage.stage=VK_SHADER_STAGE_FRAGMENT_BIT;stage.pSpecializationInfo=&specialization;stage.module=handle<VkShaderModule>(12);stage.pName="main";info.stageCount=1;info.pStages=&stage;}
 TerrainPipelineCapabilitiesKey key(u32 mask=0){TerrainPipelineCapabilitiesKey k;k.mask=mask;k.pipelineEpoch=1;k.existingSpecialization=values;return k;}
};
}
AE_TEST(terrain_pipeline_capabilities_derive_validated_real_table_not_transient_factors){
 auto b=surface();auto base=table(b);AE_EXPECT_TRUE(!base.empty(),"actual canonical PBR table produced");AE_EXPECT_EQ(terrainPipelineCapabilities(base),0u,"plain material has no absent modules");
 b.surface.layers[0].scaleX=17;b.surface.layers[0].maps[0].colorFactor={.2f,.5f,.8f,1};
 AE_EXPECT_EQ(terrainPipelineCapabilities(table(b)),0u,"live source/property edits keep same compatible capability");
 b.surface.layers[0].detailNoise.enabled=true;AE_EXPECT_TRUE(terrainPipelineCapabilities(table(b))&TerrainCapabilityAdvanced,"real enabled noise retains entire advanced family");
 b.surface.geo.mode=TerrainGeoMode::Albedo;AE_EXPECT_TRUE(terrainPipelineCapabilities(table(b))&TerrainCapabilityShared,"Geo/global modules retained");
 b.surface.snow.mode=TerrainSnowMode::Simple;AE_EXPECT_TRUE(terrainPipelineCapabilities(table(b))&TerrainCapabilitySnow,"Snow retained independently");
 b.surface.flow.specularFade=true;AE_EXPECT_TRUE(terrainPipelineCapabilities(table(b))&TerrainCapabilityStaticFx,"even mask-free specular fade retains complete FX family");
}
AE_TEST(terrain_pipeline_capabilities_unknown_abi_and_future_control32_are_generic){
 auto canonical=table(surface());canonical[1]=2;
 AE_EXPECT_EQ(terrainPipelineCapabilities(canonical),TerrainCapabilityGeneric,"future Control32 requires new page proof, never four-channel pruning");
 canonical=table(surface());canonical[2]=999999;
 AE_EXPECT_EQ(terrainPipelineCapabilities(canonical),TerrainCapabilityGeneric,"invalid table pointer falls back before dereference");
 Fixture f;auto key=f.key();key.abi=99;AE_EXPECT_TRUE(!key.specialized(),"unknown key ABI refuses specialization");key=f.key();key.mask=0x80000000u;AE_EXPECT_TRUE(!key.specialized(),"unknown capability bit is generic");key=f.key();key.family=TerrainPipelineFamily(99);AE_EXPECT_TRUE(!key.specialized(),"unknown draw family generic");
}
AE_TEST(terrain_pipeline_capabilities_key_separates_every_existing_variant_and_family){
 Fixture f;auto key=f.key();for(u32 n=0;n<6;++n){auto changed=key;++changed.existingSpecialization[n];AE_EXPECT_TRUE(changed!=key,"IDs0..5 including distant/coverage/detail/point variants participate");}
 auto changed=key;changed.family=TerrainPipelineFamily::Coverage;AE_EXPECT_TRUE(changed!=key,"coverage cannot reuse opaque");changed.family=TerrainPipelineFamily::Blend;AE_EXPECT_TRUE(changed!=key,"blend cannot reuse opaque");changed=key;++changed.pipelineEpoch;AE_EXPECT_TRUE(changed!=key,"shader/layout/renderpass state invalidates identity");
}
AE_TEST(terrain_pipeline_capabilities_vulkan_clone_cache_and_idle_lifetime){
 calls={};Fixture f;TerrainPipelineCapabilitiesVulkan cache(handle<VkDevice>(1),createPipeline,destroyPipeline);const auto generic=handle<VkPipeline>(2);const auto driver=handle<VkPipelineCache>(3);auto key=f.key();
 const auto first=cache.prepareAtIdle(key,f.info,generic,driver);AE_EXPECT_TRUE(first!=generic&&calls.valid,"real Vulkan create-info clone adds ID6 without dropping six existing constants");AE_EXPECT_EQ(calls.mask,0u,"plain mask forwarded");AE_EXPECT_TRUE(calls.cache==driver,"existing device-owned driver cache reused");
 AE_EXPECT_TRUE(cache.prepareAtIdle(key,f.info,generic,driver)==first&&calls.create==1,"same capability after brush/property edit does not compile again");
 AE_EXPECT_TRUE(cache.select(key,generic)==first&&calls.create==1,"draw selection never calls Vulkan compilation");
 key.mask=TerrainCapabilitySnow;AE_EXPECT_TRUE(cache.select(key,generic)==generic,"new capability cannot select old pipeline before prepare");cache.prepareAtIdle(key,f.info,generic,driver);AE_EXPECT_EQ(calls.create,2u,"one new compatible pipeline");AE_EXPECT_EQ(calls.destroy,0u,"no in-flight eviction");cache.clearAtIdle();AE_EXPECT_EQ(calls.destroy,2u,"device owner destroys only at safe idle");AE_EXPECT_EQ(cache.entries(),usize(0),"context/shader invalidation clears positive and negative entries");
}
AE_TEST(terrain_pipeline_capabilities_failure_negative_cache_and_unknown_specialization){
 calls={};calls.fail=true;calls.partial=true;Fixture f;TerrainPipelineCapabilitiesVulkan cache(handle<VkDevice>(1),createPipeline,destroyPipeline);auto key=f.key();const auto generic=handle<VkPipeline>(2);
 AE_EXPECT_TRUE(cache.prepareAtIdle(key,f.info,generic,VK_NULL_HANDLE)==generic,"driver failure retains complete generic pipeline");cache.prepareAtIdle(key,f.info,generic,VK_NULL_HANDLE);AE_EXPECT_EQ(calls.create,1u,"failed key does not recompile every publication");
 AE_EXPECT_EQ(calls.destroy,1u,"partially returned VkPipeline on creation error is destroyed immediately at idle");
 cache.clearAtIdle();calls.fail=false;f.maps[0].constantID=99;AE_EXPECT_TRUE(cache.prepareAtIdle(key,f.info,generic,VK_NULL_HANDLE)==generic&&calls.create==1,"unknown constant cannot alias a previous specialization");cache.clearAtIdle();
}
AE_TEST(terrain_pipeline_capabilities_explicit_cache_bound_never_evicts_inflight_pipeline){
 calls={};Fixture f;TerrainPipelineCapabilitiesVulkan cache(handle<VkDevice>(1),createPipeline,destroyPipeline);const auto generic=handle<VkPipeline>(2);auto key=f.key();
 for(u32 n=1;n<=cache.MaximumEntries;++n){key.pipelineEpoch=n;AE_EXPECT_TRUE(cache.prepareAtIdle(key,f.info,generic,VK_NULL_HANDLE)!=generic,"bounded cache admits each complete key");}
 key.pipelineEpoch=cache.MaximumEntries+1;AE_EXPECT_TRUE(cache.prepareAtIdle(key,f.info,generic,VK_NULL_HANDLE)==generic,"capacity uses generic, no hidden eviction or wrong-key reuse");
 AE_EXPECT_EQ(calls.create,u32(cache.MaximumEntries),"no repeated driver work after capacity refusal");AE_EXPECT_EQ(calls.destroy,0u,"all published handles retained until owner idle");cache.clearAtIdle();AE_EXPECT_EQ(calls.destroy,u32(cache.MaximumEntries),"all handles released at explicit safe point");
}
