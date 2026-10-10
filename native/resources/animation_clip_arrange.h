#pragma once
#include "resources/animation_clip_selection.h"

namespace ae::resources {
enum class AnimationArrange : u32 { AlignStart, AlignEnd, AlignCenter, Distribute };
// Alignment translates each selected track as a block; selected cues form one
// block. Distribution maps shared original instants to shared new instants.
// Authored values, handles and IDs survive. Collisions refuse the whole edit.
bool arrangeAnimationSelection(AnimationClipAsset &,std::span<const AnimationKeyAddress>,
    std::span<const u64> cues,AnimationArrange,float first,float last,std::string &);
// C0 loop closure: copy each raw layer/property's start pose onto its end.
// This does not promise matching velocity or retarget/root-motion correction.
bool closeAnimationRange(AnimationClipAsset &,float start,float end,std::string &);
// Insert additional contiguous closed cycles, all layers and cues together.
// Cues use [start,end); the existing end cue moves to the final boundary.
// Tolerance is absolute property units, degrees for quaternion orientation.
bool repeatAnimationRange(AnimationClipAsset &,float start,float end,u32 copies,
    double tolerance,std::string &);
}
