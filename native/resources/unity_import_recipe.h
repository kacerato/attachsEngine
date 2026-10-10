#pragma once
#include "resources/unity_animation_merge.h"
#include <utility>

namespace ae::resources {
enum class UnityImportKind : u8 { Clip,Prefab,Controller };
struct UnityImportRecipe {
  static constexpr usize MaximumBytes=64u*1024u,MaximumIdentities=4096;
  UnityImportKind kind=UnityImportKind::Clip;
  AssetGuid unityGuid{};
  i64 fileId=0;
  std::string sourceHash,metaHash,baselineHash;
  bool loop=false;
  u64 nextIdentity=2;
  std::vector<std::pair<i64,u64>> identities;
  std::vector<std::string> transformations;
  std::string serialize() const;
  static bool deserialize(std::string_view,UnityImportRecipe &,std::string &diagnostic);
};
} // namespace ae::resources
