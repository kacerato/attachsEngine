#pragma once
#include "resources/gltf_import.h"
namespace ae::resources {
// No extension-based success: signatures select a real decoder. Folder glTF
// still uses gltf_package and its controlled dependency reader before this.
bool isFbxSource(std::span<const u8> bytes);
bool importModelSource(std::span<const u8> bytes,const GltfImportLimits &limits,
                       const GltfImportProgress &progress,GltfImport &out);
}
