#pragma once
#include "resources/rig_asset.h"
#include "resources/skeletal_animation.h"
#include <unordered_map>

namespace ae::resources {
struct CompiledRigJoint {
  u64 id=0;
  i32 parent=-1;
  RigTransform bind,reference,referenceWorld;
  RigAngularLimit limit;
};
struct CompiledRig {
  AssetGuid guid;
  u32 revision=0;
  // Parent-before-child, independent of authoring order and object IDs.
  std::vector<CompiledRigJoint> joints;
  std::unordered_map<u64,u32> index;
};
bool compileRig(const RigAsset &,CompiledRig &,std::string &error);
bool rigWorldPose(const CompiledRig &,std::span<const RigTransform> local,std::span<RigTransform> world);
bool blendRigPose(const CompiledRig &,const RigPose &,float weight,std::span<const RigTransform> input,
                  std::span<RigTransform> output,std::string &error,bool acceptRevisedRig=false);
// LocalReference values are offsets from the authored reference. RigWorld
// values are absolute in the rig root's parent space. Only enabled channels
// change; limits are measured about the reference orientation.
bool applyRigControl(const RigAsset &,const CompiledRig &,u64 control,const RigTransform &value,
                     std::span<const RigTransform> input,std::span<RigTransform> output,std::string &error);
// Reference mode reflects motion relative to each side's reference, preserving
// asymmetric rests. Absolute mode reflects positions about the configured plane.
bool mirrorRigPose(const RigAsset &,const CompiledRig &,std::span<const RigTransform> input,
                   std::span<RigTransform> output,u8 channels,bool referenceMode,std::string &error);
struct CompiledRetargetJoint {u32 source=0,target=0;RetargetJoint policy;std::array<float,4> motionFrame{0,0,0,1};};
struct CompiledRetarget {
  CompiledRig source,target;
  std::vector<CompiledRetargetJoint> mapping; // target parent-before-child
  std::vector<i32> targetMapping;
  float motionScale=1;
  bool allowUnmappedSource=false;
};
struct RetargetWorkspace {
  struct Error {float position=0,angleDegrees=0;bool limited=false;};
  std::vector<RigTransform> sourceWorld,targetWorld,idealWorld;
  std::vector<Error> errors;
  float maximumPositionError=0,maximumAngularError=0;
  u32 limitedJoints=0;
  void prepare(const CompiledRetarget &);
};
bool compileRetarget(const RigAsset &source,const RigAsset &target,const RetargetProfile &,
                     CompiledRetarget &,std::string &error);
// No allocation or scene mutation. Output is published only after every joint
// has been checked; use distinct output/scratch/input buffers.
bool evaluateRetarget(const CompiledRetarget &,std::span<const RigTransform> sourceLocal,
                      std::span<RigTransform> targetLocal,RetargetWorkspace &);
struct RigAnimationChannel {u32 rigJoint=0,clipChannel=0;};
struct CompiledRigAnimation {
  AssetGuid rig;
  u32 revision=0;
  // Retains immutable runtime data, not a pointer into an editor selection.
  AnimationClip clip;
  std::vector<RigAnimationChannel> channels;
};
// nodeJoints maps clip node indices to stable rig joint IDs. No name fallback.
// Typed properties/morph channels need their own correspondence and are refused.
bool compileRigAnimation(const CompiledRig &,const AnimationClip &,std::span<const u64> nodeJoints,
                         bool allowUnmapped,CompiledRigAnimation &,std::string &error);
bool sampleRigAnimation(const CompiledRig &,const CompiledRigAnimation &,float time,std::span<RigTransform> local);
bool validateRetargetAnimation(const CompiledRetarget &,const CompiledRigAnimation &,std::string &error);
struct RetargetBakeOptions {u32 rate=60;usize maximumKeys=262144;};
// Same evaluator as realtime; node indices of the result are target rig order.
// The editor adds portable bindings when publishing the independent clip.
bool bakeRetarget(const CompiledRetarget &,const CompiledRigAnimation &,const RetargetBakeOptions &,
                  AnimationClip &,std::string &error,const std::function<bool()> &cancelled={});
} // namespace ae::resources
