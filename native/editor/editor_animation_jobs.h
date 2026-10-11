#pragma once
#include "resources/animation_clip_bake.h"
#include <memory>

namespace ae::editor {
enum class AnimationJobState : u32 {Queued,Running,Ready,Cancelled,Failed,Published,Stale};
enum class AnimationJobKind : u32 {BakeTrack,Consolidate};
// A job never retains a SceneGraph, component, Session or renderer pointer.
// Publication belongs to the editor thread after the complete ticket is checked.
struct AnimationJobTicket {
  std::string project;
  u64 epoch=0,owner=0,sceneRevision=0;
  resources::AssetGuid source;
  u32 sourceRevision=0;
  friend bool operator==(const AnimationJobTicket &,const AnimationJobTicket &)=default;
};
struct AnimationJobRequest {
  AnimationJobTicket ticket;
  AnimationJobKind kind=AnimationJobKind::BakeTrack;
  resources::AnimationClipAsset source;
  u64 track=0;
  resources::AssetGuid destination;
  std::string name,path;
  resources::AnimationBakeSettings settings;
};
struct AnimationJobStatus {
  u64 handle=0;
  AnimationJobState state=AnimationJobState::Stale;
  AnimationJobKind kind=AnimationJobKind::BakeTrack;
  resources::AnimationBakeReport report;
  std::string diagnostic;
};
struct AnimationJobResult {
  AnimationJobTicket ticket;
  AnimationJobKind kind;
  resources::AnimationClipAsset candidate;
  std::string path;
  resources::AnimationBakeReport report;
};
// One worker on mobile. At most four owned requests and 16 MiB of serialized
// input; the running bake additionally obeys its frame/evaluation/key budgets.
// Scope destruction/project invalidation cancels work without publishing files.
class EditorAnimationJobs final {
public:
  static constexpr usize MaximumJobs=4,MaximumSourceBytes=16u<<20;
  EditorAnimationJobs();
  ~EditorAnimationJobs();
  EditorAnimationJobs(const EditorAnimationJobs &)=delete;
  EditorAnimationJobs &operator=(const EditorAnimationJobs &)=delete;
  u64 enqueue(AnimationJobRequest,std::string &error);
  bool status(u64,AnimationJobStatus &) const;
  bool cancel(u64);
  // A ready result remains owned until successful publication or release. A
  // failed disk CAS can therefore be reported without discarding the candidate.
  std::shared_ptr<const AnimationJobResult> result(u64,const AnimationJobTicket &,std::string &error);
  bool published(u64);
  bool release(u64);
  void invalidate();
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace ae::editor
