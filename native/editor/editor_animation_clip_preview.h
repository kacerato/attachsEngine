#pragma once
#include "resources/animation_clip_asset.h"
#include "runtime/scene_animation.h"

namespace ae::editor {
// An isolated evaluated scene, never the source document. Preview uses the
// same clip sampler/compositor as Play but starts no scripts, events, physics
// or audio. Cancelling destroys the evaluated copy; there is no fragile list
// of properties to restore to the source scene after an error.
class AnimationClipPreview final : private runtime::AnimationLibrary {
public:
  AnimationClipPreview()=default;
  AnimationClipPreview(const AnimationClipPreview &)=delete;
  AnimationClipPreview &operator=(const AnimationClipPreview &)=delete;
  bool begin(const runtime::SceneGraph &scene,runtime::ObjectId owner,const resources::AnimationClipAsset &clip,std::string &error);
  bool seek(float seconds,std::string &error);
  void cancel();
  bool active() const {return owner_!=runtime::kInvalidObject;}
  const runtime::SceneGraph &scene() const {return scene_;}
  float time() const {return time_;}
  u64 sceneRevision() const {return sceneRevision_;}
  u32 clipRevision() const {return clipRevision_;}
  resources::AssetGuid clip() const {return source_.clipIds.empty()?resources::AssetGuid{}:source_.clipIds.front();}
private:
  bool findClip(const resources::AssetGuid &guid,runtime::AnimationClipView &out) const override;
  runtime::SceneGraph scene_;
  runtime::SourceAnimations source_;
  runtime::SceneAnimator sampler_;
  runtime::ObjectId owner_=runtime::kInvalidObject;
  u64 sceneRevision_=0;
  u32 clipRevision_=0;
  float time_=0;
};
} // namespace ae::editor
