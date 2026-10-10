#pragma once
#include "resources/fbx_deformation.h"
struct ufbx_mesh;
namespace ae::resources {
// Decoded attributes. Each in-between shape becomes an independent
// target; ufbx effective weights retain its actual combination at the bind pose.
// Vertex duplication must happen after these control-vertex arrays are expanded.
struct FbxMorphTarget {
  std::string name;
  u32 channel=0,keyframe=0;
  float defaultWeight=0;
  std::vector<float> positions,normals;
};
bool prepareFbxMorph(const ufbx_mesh &,const FbxDeformationPolicy &,
                     const GltfImportProgress &,std::vector<FbxMorphTarget> &,
                     std::string &diagnostic);
}
