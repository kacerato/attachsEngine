#pragma once
#include "core/base.h"
#include <algorithm>
#include <cstring>
#include <span>
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
                                                       std::span<const u8> next) {
  BufferUpdate plan;
  if(next.empty() || next.size()%4 || previous.size()%4)return plan;
  plan.valid=true;plan.replace=previous.size()!=next.size();
  if(plan.replace){plan.ranges.push_back({0,next.size()});plan.uploadedBytes=next.size();return plan;}
  constexpr usize block=4096,mergeGap=256;
  for(usize base=0;base<next.size();base+=block) {
    const auto end=std::min(base+block,next.size());
    if(std::memcmp(previous.data()+base,next.data()+base,end-base)==0)continue;
    for(usize offset=base;offset<end;offset+=4) {
      if(std::memcmp(previous.data()+offset,next.data()+offset,4)==0)continue;
      if(!plan.ranges.empty() && offset<=plan.ranges.back().offset+plan.ranges.back().bytes+mergeGap) {
        auto &range=plan.ranges.back();range.bytes=offset+4-range.offset;
      } else plan.ranges.push_back({offset,4});
    }
  }
  // There are at most one range per 260 bytes; publish in one queue submission.
  for(const auto &range:plan.ranges)plan.uploadedBytes+=range.bytes;
  return plan;
}
} // namespace ae
