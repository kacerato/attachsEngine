#pragma once
#include "resources/rig_evaluation.h"
#include "runtime/scene_graph.h"

namespace ae::runtime {
struct RigBindingIssue {u64 joint=0;std::string message;};
struct RigBindings {
  resources::AssetGuid rig;
  u32 revision=0;
  ObjectId root=0;
  // Same order as CompiledRig. Zero invalidates only that joint's binding.
  std::vector<ObjectId> objects;
  std::vector<RigBindingIssue> issues;
};
bool rigTransformFromScene(const Transform &,resources::RigTransform &);
bool rigTransformToScene(const resources::RigTransform &,const Transform &near,Transform &);
bool bindRigToScene(const resources::RigAsset &,const resources::CompiledRig &,const SceneGraph &,ObjectId root,RigBindings &);
bool readRigScenePose(const resources::CompiledRig &,const RigBindings &,const SceneGraph &,
                      std::span<resources::RigTransform> local,std::string &error);
// Captures the real selected hierarchy, with reference initially equal to bind.
// No skin, hidden character, camera or extra scene transform is created.
bool captureRig(const SceneGraph &,ObjectId root,resources::AssetGuid,std::string_view name,
                resources::RigAsset &,std::string &error);
struct RigSceneChange {ObjectId object=0;Transform before,after;};
// Builds a complete atomic candidate; editor commits these through History.
// Missing unmodified bindings do not prevent applying an unrelated pose.
bool planRigScenePose(const resources::CompiledRig &,const RigBindings &,const SceneGraph &,
                      std::span<const resources::RigTransform> local,std::span<const u8> channels,
                      std::vector<RigSceneChange> &,std::string &error);
bool applyRigSceneChanges(SceneGraph &,std::span<const RigSceneChange>,std::string &error);

// Stateful runtime consumer. Revisions/targets checked on every frame, immutable
// rig and clip ownership; no editor window or project JSON in the hot path.
class RetargetInstance {
public:
  bool configure(resources::CompiledRetarget,resources::CompiledRigAnimation,RigBindings target,std::string &error);
  // Source clip sampled at the exact supplied local time; source scene untouched.
  bool evaluate(SceneGraph &target,float localTime,std::string &error);
  void reset();
  const std::vector<resources::RigTransform> &targetPose() const {return targetLocal_;}
  const resources::RetargetWorkspace &diagnostics() const {return scratch_;}
private:
  resources::CompiledRetarget program_;
  resources::CompiledRigAnimation clip_;
  RigBindings targetBindings_;
  resources::RetargetWorkspace scratch_;
  std::vector<resources::RigTransform> sourceLocal_,targetLocal_;
  std::vector<Transform> targetValues_;
  bool ready_=false;
};
} // namespace ae::runtime
