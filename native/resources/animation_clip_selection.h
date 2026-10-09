#pragma once
#include "resources/animation_clip_asset.h"

namespace ae::resources {
struct AnimationKeyAddress {
  u64 track=0;
  u32 component=0;
  u64 key=0;
  bool operator==(const AnimationKeyAddress &) const=default;
};
// Selection is editor state, addressed by persistent IDs, never curve indices.
// Quaternion/progressive poses expand to their complete synchronized group.
bool expandAnimationKeySelection(const AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                 std::vector<AnimationKeyAddress> &out,std::string &diagnostic);
// In-memory atomic operations. The caller publishes through ClipAsset::edit /
// EditorSession's journal. Failure preserves values, IDs and allocation frontier.
// Time transform must retain key order; moving through an unselected key is an
// explicit conflict, rather than silently replacing another authored pose.
bool transformAnimationKeyTimes(AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                double pivot,double scale,double offset,std::string &diagnostic);
bool eraseAnimationKeySelection(AnimationClipAsset &clip,std::span<const AnimationKeyAddress> selection,
                                std::string &diagnostic);
} // namespace ae::resources
