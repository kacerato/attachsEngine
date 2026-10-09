#pragma once
#include "resources/animation_clip_selection.h"

namespace ae::resources {
enum class AnimationPasteMode : u8 { Replace, InsertTracks, InsertAllTracks };
// In-process, typed authoring clipboard. It owns only copied keys, binding
// identities and channel metadata; it never retains a library/editor pointer.
class AnimationKeyClipboard {
public:
  bool copy(const AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,std::string &diagnostic);
  bool paste(AnimationClipAsset &clip,float at,AnimationPasteMode mode,
             std::vector<AnimationKeyAddress> &selection,std::string &diagnostic) const;
  bool empty() const {return entries_.empty();}
  usize size() const {return count_;}
private:
  struct Key {AnimationCurveKey value;double inSpan=0,outSpan=0;};
  struct Entry {
    u64 track=0,layer=0;std::string layerName;AnimationAuthorBlend blend=AnimationAuthorBlend::Override;AnimationClipBinding binding;
    AnimationPath path=AnimationPath::Translation;
    AnimationRotationMode rotation=AnimationRotationMode::Quaternion;
    u32 weightCount=0;std::vector<std::vector<Key>> curves;
  };
  AssetGuid clip_,source_;float first_=0,last_=0;usize count_=0;
  std::vector<Entry> entries_;
};
}
