#pragma once
#include "runtime/rig_instance.h"

namespace ae::editor {
// Pure authoring operations: no project files, renderer, History or hidden
// scene writes. The caller publishes the resulting typed candidate together
// with its other resources, after checking the recorded resource revisions.
bool prepareRigCapture(const runtime::SceneGraph &,runtime::ObjectId root,
                       resources::AssetGuid,std::string_view name,
                       resources::RigAsset &,std::string &error);
// id=0 creates a control at the monotonic frontier. Existing IDs retain their
// kind and joint; moving a control is removal+creation, never identity reuse.
bool prepareRigControl(const resources::RigAsset &,u32 expectedRevision,
                       resources::RigControl value,resources::RigAsset &,
                       u64 &assignedId,std::string &error);
bool prepareRigRemoveControl(const resources::RigAsset &,u32 expectedRevision,
                             u64 control,resources::RigAsset &,std::string &error);
bool prepareRigMirrorPair(const resources::RigAsset &,u32 expectedRevision,
                          u64 left,u64 right,resources::RigAsset &,std::string &error);
bool prepareRigMirrorPlane(const resources::RigAsset &,u32 expectedRevision,
                           std::array<float,3> normal,float offset,
                           resources::RigAsset &,std::string &error);
bool prepareRigChain(const resources::RigAsset &,u32 expectedRevision,u64 id,
                     std::string_view name,std::span<const u64> joints,
                     resources::RigAsset &,u64 &assignedId,std::string &error);
bool prepareRigReference(const resources::RigAsset &,u32 expectedRevision,
                         std::span<const resources::RigTransform> local,
                         std::span<const u8> channels,resources::RigAsset &,
                         std::string &error);
bool prepareRigPose(const resources::CompiledRig &,resources::AssetGuid,
                    std::string_view name,std::span<const resources::RigTransform>,
                    std::span<const u8> channels,resources::RigPose &,
                    std::string &error);
// Supplied map and policies are explicit. No bone-name, humanoid or axis guess.
// Both expected revisions are validated before the compiled evaluator accepts
// the candidate, so the eventual publication can revalidate the same pair.
bool prepareRigRetarget(const resources::RigAsset &source,u32 expectedSource,
                        const resources::RigAsset &target,u32 expectedTarget,
                        const resources::RetargetProfile &,resources::RetargetProfile &,
                        std::string &error);
// Edit of an existing profile: CAS revision is the profile's OWN revision;
// source/target revisions remain independently validated dependencies.
bool prepareRetargetEdit(const resources::RetargetProfile &base,u32 expectedProfile,
                         const resources::RigAsset &source,u32 expectedSource,
                         const resources::RigAsset &target,u32 expectedTarget,
                         const resources::RetargetProfile &draft,
                         resources::RetargetProfile &,std::string &error);
} // namespace ae::editor
