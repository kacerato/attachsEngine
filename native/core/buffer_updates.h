#pragma once
#include "core/base.h"
#include <algorithm>
#include <cstring>
#include <span>
#include <limits>
#include <vector>

namespace ae {
struct BufferUpdateRange { u64 offset = 0, bytes = 0; };
struct BufferUpdate {
  bool valid = false, replace = false;
  u64 uploadedBytes = 0;
  std::vector<BufferUpdateRange> ranges;
};
// Vulkan buffer copies require 4-byte alignment. Merge nearby changed words
// to bound command count without staging the unchanged rest of the library.
inline BufferUpdate planBufferUpdate(std::span<const u8> previous,
                                    std::span<const u8> next, u64 capacity) {
  BufferUpdate plan;
  if(next.empty() || next.size()%4 || previous.size()%4 || capacity%4 || previous.size()>capacity)return plan;
  plan.valid=true;plan.replace=next.size()>capacity;
  if(plan.replace){plan.ranges.push_back({0,next.size()});plan.uploadedBytes=next.size();return plan;}
  constexpr usize block=4096,mergeGap=256;
  const auto common=std::min(previous.size(),next.size());
  const auto append=[&](u64 offset,u64 bytes) {
    if(!plan.ranges.empty() && offset-plan.ranges.back().offset-plan.ranges.back().bytes<=mergeGap) {
      auto &range=plan.ranges.back();range.bytes=offset+bytes-range.offset;
    } else plan.ranges.push_back({offset,bytes});
  };
  for(usize base=0;base<common;base+=block) {
    const auto end=std::min(base+block,common);
    if(std::memcmp(previous.data()+base,next.data()+base,end-base)==0)continue;
    for(usize offset=base;offset<end;offset+=4) {
      if(std::memcmp(previous.data()+offset,next.data()+offset,4)==0)continue;
      append(offset,4);
    }
  }
  // Bytes outside the previous logical extent are unknown, including after
  // shrink/regrow. Always upload them; unused capacity is never a CPU mirror.
  if(next.size()>common)append(common,next.size()-common);
  // There are at most one range per 260 bytes; publish in one queue submission.
  for(const auto &range:plan.ranges)plan.uploadedBytes+=range.bytes;
  return plan;
}
// Existing exact-size users keep their original replacement policy.
inline BufferUpdate planBufferUpdate(std::span<const u8> previous,std::span<const u8> next) {
  auto plan=planBufferUpdate(previous,next,previous.size());
  if(plan.valid && previous.size()!=next.size()) {
    plan.replace=true;plan.ranges={{0,next.size()}};plan.uploadedBytes=next.size();
  }
  return plan;
}
// Small authoring edits retain bounded headroom. Large shrink releases memory;
// optional headroom never exceeds 8 MiB per buffer, including the minimum.
inline u64 authoringBufferCapacity(u64 required,u64 previousCapacity) {
  constexpr u64 maximumSlack=8ull<<20;
  if(!required || required%4 || previousCapacity%4)return 0;
  if(previousCapacity>=required && previousCapacity-required<=maximumSlack)return previousCapacity;
  const u64 slack=std::min(maximumSlack,std::max(u64(4096),required/4))&~u64(3);
  if(required>std::numeric_limits<u64>::max()-slack)return required;
  return required+slack;
}
} // namespace ae
