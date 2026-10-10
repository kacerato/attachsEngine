#pragma once
#include "resources/material_asset.h"
#include <vector>

namespace ae::resources {
// Deliberately bounded reader for Unity's text Material asset, not a generic
// YAML loader or a shader translator. Unknown fields survive in the source.
struct UnityMaterialTexture {
  std::string property;
  AssetGuid unityGuid;
  u32 binding=0;
  scene::MaterialSampling sampling;
};
struct UnityMaterialImport {
  MaterialAsset material;
  std::vector<UnityMaterialTexture> textures;
  std::vector<std::string> unsupported;
  std::string diagnostic;
  bool supported() const {return diagnostic.empty() && unsupported.empty();}
};
bool readUnityMaterial(std::string_view text,UnityMaterialImport &out);
} // namespace ae::resources
