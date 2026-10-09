#pragma once
#include "resources/animation_clip_asset.h"

namespace ae::resources {
// Shared authoring math: rebuild angular distance from synchronized poses and
// retain normalized speed handles. Callers publish the complete clip atomically.
bool rebuildProgressiveAnimationTrack(AnimationClipTrack &track,std::string &diagnostic);
}
