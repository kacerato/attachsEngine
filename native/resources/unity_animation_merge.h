#pragma once
#include "resources/animation_clip_asset.h"

namespace ae::resources {
enum class UnityReimportPolicy : u8 { RefuseConflict,KeepLocal,ReceiveSource };
struct UnityAnimationMergeReport {u32 updated=0,preserved=0,added=0,removed=0;std::vector<std::string> conflicts;};
// Source tracks are identified by canonical path/property. Authoring IDs and
// allocator frontiers survive source reorder, deletion, Undo and local edits.
// Layers, cues, regions, properties and drivers authored locally are retained.
bool mergeUnityAnimation(const AnimationClipAsset &baseline,const AnimationClipAsset &local,
    const AnimationClipAsset &incoming,UnityReimportPolicy policy,AnimationClipAsset &out,
    AnimationClipAsset &nextBaseline,UnityAnimationMergeReport &,std::string &diagnostic);
} // namespace ae::resources
