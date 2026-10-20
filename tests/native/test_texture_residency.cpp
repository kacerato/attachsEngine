#include "harness.h"
#include "renderer/texture_streaming.h"
#include "resources/texture_budget.h"
#include "renderer/texture_level_reader.h"
#include <chrono>
#include <thread>
using namespace ae;
using namespace ae::renderer;
namespace {
TextureStreamingTexture cappedTexture() {
  TextureStreamingTexture t;t.width=4096;t.height=2048;t.levels=13;
  t.minimumResidentMip=3;return t;
}
}
AE_TEST(texture_residency_author_ceiling_survives_manual_requests_camera_and_streaming_off) {
  auto manual=cappedTexture();manual.requestedMip=0;
  auto fixed=cappedTexture();fixed.streamable=false;
  const std::vector textures{manual,fixed,cappedTexture()};
  TextureStreamingUse usage;usage.texture=2;usage.metersPerUv=0;
  TextureStreamingView view;view.orthographicPixelsPerMeter=1e6f;
  TextureStreamingSettings settings;settings.budgetBytes=~0ull;settings.maxLevelReduction=0;
  const std::vector<u32> loaded{0,TextureStreamingNotLoaded,0};
  TextureStreamingPlan plan;
  AE_EXPECT_TRUE(planTextureStreaming(textures,std::span(&usage,1),std::span(&view,1),settings,loaded,plan),"plan explicit author ceilings");
  for(auto mip:plan.targetMip) AE_EXPECT_EQ(mip,3u,"manual request, nonstreamed and near camera all honor ceiling");
  AE_EXPECT_EQ(plan.nonStreamingBytes,textureStreamingChainBytes(fixed,3),"pinned author map counts only permitted resident chain");
  AE_EXPECT_EQ(plan.desiredBytes,3*textureStreamingChainBytes(manual,3),"requested GPU bytes reflect explicit cap");
  AE_EXPECT_TRUE(plan.desiredBytes<plan.totalBytes/60,"512 cap reduces a 4096 chain without changing source dimensions");
  AE_EXPECT_TRUE(plan.loads.size()==3,"old overly detailed allocation is scheduled for replacement");
  settings.minimumMip=5;
  AE_EXPECT_TRUE(planTextureStreaming(textures,std::span(&usage,1),std::span(&view,1),settings,loaded,plan),"global quality and author ceiling combine");
  for(auto mip:plan.targetMip) AE_EXPECT_EQ(mip,5u,"stricter global quality still applies");
}
AE_TEST(texture_residency_invalid_ceiling_does_not_replace_a_plan) {
  auto texture=cappedTexture();texture.minimumResidentMip=texture.levels;
  TextureStreamingSettings settings;TextureStreamingPlan plan;plan.targetBytes=9182;plan.targetMip={7};
  const u32 loaded=TextureStreamingNotLoaded;
  AE_EXPECT_TRUE(!planTextureStreaming(std::span(&texture,1),{}, {},settings,std::span(&loaded,1),plan),"invalid ceiling rejected");
  AE_EXPECT_EQ(plan.targetBytes,9182ull,"failed planning leaves previous allocation plan intact");
  AE_EXPECT_EQ(plan.targetMip[0],7u,"failed planning preserves previous target");
}
AE_TEST(texture_residency_async_reader_refuses_finer_mips_and_preserves_raw_cache_access) {
  auto texture=std::make_shared<AuthoringTexture>();texture->width=texture->height=16;texture->levels=5;
  texture->minimumResidentMip=2;texture->mipChain.resize(texture->expectedBytes(),73);
  AE_EXPECT_TRUE(texture->valid(),"source retains complete chain with a separate residency ceiling");
  std::vector<u8> scratch;std::span<const u8> raw;
  AE_EXPECT_TRUE(readAuthoringTextureLevels(*texture,0,scratch,raw),"cache/export retains byte-exact access to complete original derivatives");
  AE_EXPECT_EQ(raw.size(),texture->expectedBytes(),"residency does not destroy source data");
  TextureLevelReader reader;reader.request(41,texture,0);reader.request(42,texture,2);
  const auto wait=[&](u64 key,u32 mip,std::vector<u8>&bytes){
    auto status=TextureLevelReader::Status::Pending;
    for(u32 retry=0;retry<2000 && status==TextureLevelReader::Status::Pending;++retry){
      status=reader.take(key,mip,bytes);
      if(status==TextureLevelReader::Status::Pending)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }return status;
  };
  std::vector<u8> denied{19},allowed;
  AE_EXPECT_TRUE(wait(41,0,denied)==TextureLevelReader::Status::Failed,"streaming read cannot bypass author ceiling");
  AE_EXPECT_TRUE(denied==std::vector<u8>{19},"refused request leaves output intact");
  AE_EXPECT_TRUE(wait(42,2,allowed)==TextureLevelReader::Status::Ready,"permitted resident mip is recoverable");
  AE_EXPECT_EQ(allowed.size(),texture->chainBytesFrom(2),"only permitted chain is sent for GPU upload");
  auto invalid=*texture;invalid.minimumResidentMip=5;
  AE_EXPECT_TRUE(!invalid.valid(),"out-of-range ceiling invalidates source before upload");
}

AE_TEST(texture_residency_library_budget_counts_caps_and_never_simplifies_protected_originals) {
  const auto source=[](u32 size,u32 ceiling){
    auto t=std::make_shared<AuthoringTexture>();t->width=t->height=size;t->levels=1;
    for(auto side=size;side>1;side/=2)++t->levels;
    t->minimumResidentMip=ceiling;t->mipChain.resize(t->expectedBytes(),97);return t;
  };
  SharedAuthoringTexture capped=source(64,2);
  std::vector<SharedAuthoringTexture> duplicates{capped,capped};
  const auto capBytes=capped->chainBytesFrom(2);
  auto report=resources::applyTextureBudget(duplicates,capBytes,1);
  AE_EXPECT_TRUE(report.withinBudget() && report.textures==1 && report.reducedTextures==0,"explicit cap fits and identical source is counted once");
  AE_EXPECT_EQ(report.requestedBytes,capBytes,"GPU budget counts permitted chain, not full CPU source");
  AE_EXPECT_TRUE(duplicates[0]==capped && duplicates[1]==capped && capped->width==64,"cap does not replace or resize authored originals");
  report=resources::applyTextureBudget(duplicates,capBytes-1,1);
  AE_EXPECT_TRUE(!report.withinBudget() && report.reducedTextures==0,"an explicit cap that cannot fit fails without further hidden quality loss");
  SharedAuthoringTexture original=source(32,0),ordinary=source(32,0);
  std::vector<SharedAuthoringTexture> mixed{original,ordinary};
  const std::vector<SharedAuthoringTexture> protectedMaps{original};
  report=resources::applyTextureBudget(mixed,original->expectedBytes()+ordinary->chainBytesFrom(1),1,protectedMaps);
  AE_EXPECT_TRUE(report.withinBudget() && report.reducedTextures==1,"other project textures retain established budget policy");
  AE_EXPECT_TRUE(mixed[0]==original && mixed[0]->width==32 && mixed[1]->width==16,"terrain Original is preserved while unrelated reducible texture can fit");
}
