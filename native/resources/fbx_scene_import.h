#pragma once
#include "resources/fbx_deformation.h"
namespace ae::resources {
// Existing scene/deformation IR; no second skeleton or animation runtime.
// Static legacy profiles keep the strict previous gate.
struct FbxSceneImportPolicy {
  bool deformation=true;
  FbxDeformationPolicy bake{};
};
bool importFbxScene(std::span<const u8>,const GltfImportLimits &,
                    const GltfImportProgress &,const FbxSceneImportPolicy &,GltfImport &);
}
