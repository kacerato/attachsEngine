#pragma once
#include "resources/animation_clip_asset.h"
#include <functional>

namespace ae::resources {
struct AnimationBakeSettings {
  u32 sampleRate=60,verificationSteps=4,maximumFrames=16384;
  double tolerance=.01;
  // 3 preserves the current representation. 0/1/2 use AnimationRotationMode.
  u32 rotation=3;
  bool reduce=true;
  // Explicit initial XYZ branch (Rz*Ry*Rx). Later poses lift to the nearest
  // continuous branch. No orientation-only representation invents lost turns.
  bool eulerReferenceExplicit=false;
  float eulerReference[3]{};
};
struct AnimationBakeReport {
  u32 inputKeys=0,outputKeys=0,sampledFrames=0,verifiedSamples=0;
  double maximumError=0;
};
// Converts one real channel through the shared runtime sampler, transactionally.
// Error is maximum component error (translation/scale/weights) or degrees
// (rotation). Report is measured on the stated verification lattice, not a
// proof of continuous-time error. Original source/bindings/other tracks stay
// unchanged. Cancellation/budget/precision/invalid poses never publish a part.
bool bakeAnimationClipTrack(AnimationClipAsset &asset,u64 track,
    const AnimationBakeSettings &settings,AnimationBakeReport &report,std::string &diagnostic,
    const std::function<bool()> &cancelled={});
// Publishes no files: generates a distinct single-base resource using the real
// composed sampler, including Mute/Solo and additive references. The editor
// publishes the whole result in one creation/history transaction.
bool consolidateAnimationClip(const AnimationClipAsset &source,AssetGuid guid,std::string_view name,
    const AnimationBakeSettings &settings,AnimationClipAsset &out,AnimationBakeReport &report,
    std::string &diagnostic,const std::function<bool()> &cancelled={});
}
