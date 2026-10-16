#include "core/buffer_updates.h"
#include "harness.h"
using namespace ae;

AE_TEST(buffer_updates_preserve_unchanged_regions_and_reduce_upload) {
  std::vector<u8> old(4u<<20,7),next=old;
  for(usize i=700000;i<704000;++i)next[i]=u8(i*19);
  for(usize i=3500000;i<3500800;++i)next[i]=u8(i*31);
  const auto plan=planBufferUpdate(old,next);
  AE_EXPECT_TRUE(plan.valid && !plan.replace,"same capacity preserves the allocation");
  AE_EXPECT_EQ(plan.ranges.size(),usize(2),"two distant edited chunks produce two copy ranges");
  AE_EXPECT_TRUE(plan.uploadedBytes<8192,"under 8 KiB staged for edits in a 4 MiB library");
  auto applied=old;
  for(const auto &range:plan.ranges)std::memcpy(applied.data()+range.offset,next.data()+range.offset,range.bytes);
  AE_EXPECT_EQ(applied,next,"applying only scheduled ranges exactly reproduces the complete revision");
  const auto unchanged=planBufferUpdate(applied,next);
  AE_EXPECT_TRUE(unchanged.ranges.empty() && unchanged.uploadedBytes==0,"unchanged revision performs no geometry upload");
}
AE_TEST(buffer_updates_cover_unaligned_edits_and_topology_resize) {
  std::vector<u8> old(16384,0),next=old;
  for(usize i:{0u,3u,4095u,4096u,8193u,16383u})next[i]=1;
  auto plan=planBufferUpdate(old,next);auto applied=old;u64 previousEnd=0;
  for(const auto &range:plan.ranges) {
    AE_EXPECT_TRUE(range.offset%4==0 && range.bytes%4==0 && range.offset>=previousEnd,"aligned sorted nonoverlapping copies");
    std::memcpy(applied.data()+range.offset,next.data()+range.offset,range.bytes);previousEnd=range.offset+range.bytes;
  }
  AE_EXPECT_EQ(applied,next,"word and block edges preserved");
  next.resize(32768);plan=planBufferUpdate(old,next);
  AE_EXPECT_TRUE(plan.replace && plan.ranges.size()==1 && plan.uploadedBytes==next.size(),"topology growth explicitly requires replacement");
  next.resize(12);plan=planBufferUpdate(old,next);
  AE_EXPECT_TRUE(plan.replace && plan.uploadedBytes==12,"topology shrink replaces safely too");
  next.resize(11);AE_EXPECT_TRUE(!planBufferUpdate(old,next).valid,"non Vulkan-aligned buffers refused");
}
AE_TEST(buffer_updates_scattered_edits_reconstruct_exactly) {
  std::vector<u8> old(1u<<20,19),next=old;u32 random=0x93b1;
  for(u32 i=0;i<1000;++i){random=random*1664525u+1013904223u;next[random%next.size()]=u8(random>>24);}
  const auto plan=planBufferUpdate(old,next);auto applied=old;
  for(const auto &range:plan.ranges)std::memcpy(applied.data()+range.offset,next.data()+range.offset,range.bytes);
  AE_EXPECT_EQ(applied,next,"scattered vertex edits have no omitted bytes");
  AE_EXPECT_TRUE(plan.uploadedBytes<next.size()/2,"scattered edits still avoid copying the entire library");
}
