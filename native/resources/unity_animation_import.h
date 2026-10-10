#pragma once
#include "resources/animation_clip_asset.h"
#include "resources/unity_yaml.h"

namespace ae::resources {
// The mesh's real, ordered morph names are required. Unity stores blendShape
// curves by name, while the native renderer consumes weight array indices.
struct UnityAnimationMorphBinding {
  std::string path;
  std::vector<std::string> names;
  std::vector<float> defaults;
};
struct UnityAnimationImport {
  AnimationClipAsset clip;
  AssetGuid unityGuid{};
  std::string sourceHash;
  i64 fileId=0;
  bool loop=false;
  std::vector<std::string> transformations;
};
// Generic Transform and named blendShape curves only. Humanoid muscle curves,
// compressed clips, object-reference curves, callbacks and Euler rotation
// orders without an exact native representation fail before publishing.
bool convertUnityAnimation(const UnityYamlDocument &document,AssetGuid nativeGuid,AssetGuid unityGuid,
    std::string_view sourceHash,std::span<const UnityAnimationMorphBinding> morphs,
    UnityAnimationImport &out,std::string &diagnostic);
} // namespace ae::resources
