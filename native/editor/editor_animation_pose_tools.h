#pragma once
#include "resources/animation_clip_asset.h"
#include <array>

namespace ae::editor {
// Authoring state only. It never changes a runtime channel or the source scene.
struct AnimationPoseRecordingPolicy {
  bool createChannels=true,rangeArmed=false;
  float start=0,end=0;
};
struct AnimationPoseClipboard {
  bool world=false;
  resources::AnimationPath path=resources::AnimationPath::Translation;
  resources::AnimationRotationMode rotation=resources::AnimationRotationMode::Quaternion;
  std::vector<float> values;
  std::array<float,16> worldMatrix{};
};
// Rotation masks address local XYZ angles, never quaternion components.
// Weights/TRS masks address their native property components.
bool applyAnimationPoseLocks(const resources::AnimationClipTrack &,float time,u64 mask,
                             std::span<const float> requested,std::vector<float> &out,std::string &error);
bool sampleAnimationPoseChannel(const resources::AnimationClipTrack &,float time,std::vector<float> &out);
}
