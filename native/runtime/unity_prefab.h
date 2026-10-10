#pragma once
#include "runtime/prefab.h"
#include "resources/unity_prefab_import.h"
#include <functional>

namespace ae::runtime {
struct UnityPrefabIdentity {i64 fileId=0;ObjectId object=0;};
struct UnityPrefabIdentityMap {
  ObjectId nextObject=2;
  std::vector<UnityPrefabIdentity> entries;
};
// Mesh reference resolves to its native submeshes, in source material order;
// material references resolve to exactly one shared material identity.
using UnityPrefabResolver=std::function<bool(const resources::UnityObjectReference &,resources::AssetType,
    std::vector<resources::AssetGuid> &,std::string &)>;
bool buildUnityPrefab(const resources::UnityPrefabImport &,resources::AssetGuid guid,const UnityPrefabResolver &,
    const UnityPrefabIdentityMap *previous,Prefab &out,UnityPrefabIdentityMap &identities,std::string &diagnostic);
} // namespace ae::runtime
