#pragma once
#include "resources/animator_controller.h"
#include "resources/unity_yaml.h"
#include <functional>

namespace ae::resources {
struct UnityControllerClip {AssetGuid guid{};bool loop=false;};
using UnityControllerClipResolver=std::function<bool(const UnityObjectReference &,UnityControllerClip &,std::string &)>;
struct UnityControllerImport {
  AnimatorControllerAsset controller;
  std::vector<std::pair<i64,u64>> identities;
  std::vector<std::string> transformations;
};
// Flat state machines, scalar conditions, clip motions and 1D blend trees.
// Humanoid IK, scripts, Avatar masks, synchronized layers and blend algorithms
// lacking equivalent native semantics are explicit conversion errors.
bool convertUnityController(const UnityYamlDocument &,AssetGuid nativeGuid,
    const UnityControllerClipResolver &,UnityControllerImport &,std::string &diagnostic);
// Reuses exact fileIDs and named parameter/layer identities. Deleted IDs stay
// reserved; source ordering never becomes an identity heuristic.
bool rebaseUnityController(UnityControllerImport &,const AnimatorControllerAsset &baseline,
    const AnimatorControllerAsset &local,std::span<const std::pair<i64,u64>> previous,std::string &diagnostic);
} // namespace ae::resources
