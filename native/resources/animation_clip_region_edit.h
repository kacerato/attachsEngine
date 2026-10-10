#pragma once
#include "resources/animation_clip_asset.h"
#include <algorithm>

namespace ae::resources {
inline void sortAnimationRegions(AnimationClipAsset &clip) {
  std::sort(clip.regions.begin(),clip.regions.end(),[](const auto &a,const auto &b){return a.start!=b.start?a.start<b.start:a.id<b.id;});
}
// A section's end is exclusive for ripple insertion. Inserting exactly at the
// end adds time after that section; inserting inside extends its span.
inline void insertAnimationRegionTime(AnimationClipAsset &clip,float at,double length) {
  for(auto &r:clip.regions) {
    if(r.start>=at)r.start=static_cast<float>(r.start+length);
    if(r.end>at)r.end=static_cast<float>(r.end+length);
  }
  sortAnimationRegions(clip);
}
inline void removeAnimationRegionTime(AnimationClipAsset &clip,float start,float end) {
  const auto map=[&](float t){return t<=start?t:t>=end?static_cast<float>(double(t)-(double(end)-start)):start;};
  for(auto &r:clip.regions) {r.start=map(r.start);r.end=map(r.end);}
  std::erase_if(clip.regions,[](const auto &r){return r.start>=r.end;});sortAnimationRegions(clip);
}
inline void cropAnimationRegions(AnimationClipAsset &clip,float start,float end) {
  for(auto &r:clip.regions) {r.start=std::max(r.start,start)-start;r.end=std::min(r.end,end)-start;}
  std::erase_if(clip.regions,[](const auto &r){return r.start>=r.end;});sortAnimationRegions(clip);
}
inline bool copyAnimationRegions(AnimationClipAsset &clip,std::span<const AnimationRegion> regions,double offset,std::string &error) {
  if(regions.size()>MaximumAnimationRegions-clip.regions.size()) {error="Duplicação excede o limite de regiões";return false;}
  for(auto r:regions) {
    r.id=0;r.start=static_cast<float>(r.start+offset);r.end=static_cast<float>(r.end+offset);u64 id=0;
    if(!clip.putRegion(std::move(r),id,error))return false;
  }
  return true;
}
}
