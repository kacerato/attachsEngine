#pragma once
#include "runtime/scene_graph.h"
#include "resources/asset_registry.h"
#include <span>
#include <string>
#include <vector>

namespace ae::editor {
inline constexpr usize MaximumAnimationPoseSelection=256,MaximumAnimationPoseSets=1024,MaximumAnimationPoseSetsPerClip=32;
// Project authoring preferences, independent of transient object IDs. The first
// path is primary. Missing/ambiguous paths refuse recall without partial selection.
struct AnimationPoseSelectionSet {
  resources::AssetGuid clip;
  std::string name;
  std::vector<std::string> paths;
};
bool animationPoseTargetPath(const runtime::SceneGraph &,runtime::ObjectId owner,runtime::ObjectId target,std::string &,std::string &error);
bool resolveAnimationPoseTargetPath(const runtime::SceneGraph &,runtime::ObjectId owner,std::string_view path,runtime::ObjectId &,std::string &error);
std::vector<runtime::ObjectId> animationPoseSelectionRoots(const runtime::SceneGraph &,std::span<const runtime::ObjectId>);
std::string serializeAnimationPoseSets(std::span<const AnimationPoseSelectionSet>);
bool deserializeAnimationPoseSets(std::string_view,std::vector<AnimationPoseSelectionSet> &,std::string &error);
}
