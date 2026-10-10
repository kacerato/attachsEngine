#pragma once
#include "resources/unity_yaml.h"
#include <array>

namespace ae::resources {
struct UnityPrefabObject {
  i64 fileId=0,transformId=0,parentTransform=0;
  std::string name,tag;
  std::array<float,3> position{},scale{1,1,1};
  std::array<float,4> rotation{0,0,0,1};
  bool active=true,renderer=false,rendererEnabled=true,castShadow=true,receiveShadow=true;
  u32 layer=0;
  UnityObjectReference mesh;
  std::vector<UnityObjectReference> materials;
};
struct UnityPrefabImport {
  i64 rootFileId=0;
  std::vector<UnityPrefabObject> objects;
  std::vector<std::string> transformations;
};
// Complete standalone GameObject/Transform/MeshFilter/MeshRenderer prefabs.
// Variant/instance patches, MonoBehaviours and unknown components are refused
// as a whole. Their source remains in the package with exact references.
bool convertUnityPrefab(const UnityYamlDocument &,UnityPrefabImport &,std::string &diagnostic);
} // namespace ae::resources
