#pragma once
#include "resources/gltf_import.h"

namespace ae::resources {
// Native ufbx decoder -> bounded GLB interchange -> existing typed import IR.
// Initial static family. Deformers/animation/constraints are refused rather
// than discarded; external materials/textures carry explicit loss diagnostics.
// The library and model picker feed this same decoder on host and Android.
bool importStaticFbx(std::span<const u8> bytes,const GltfImportLimits &limits,
                     const GltfImportProgress &progress,GltfImport &out);
}
