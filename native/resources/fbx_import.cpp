#include "resources/fbx_import.h"
#include "resources/fbx_scene_import.h"
namespace ae::resources {
bool importStaticFbx(std::span<const u8> bytes,const GltfImportLimits &limits,
                     const GltfImportProgress &progress,GltfImport &out) {
  FbxSceneImportPolicy policy;policy.deformation=false;
  return importFbxScene(bytes,limits,progress,policy,out);
}
}