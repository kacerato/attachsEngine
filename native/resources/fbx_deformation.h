#pragma once
#include "resources/gltf_import.h"
struct ufbx_mesh;
struct ufbx_scene;
namespace ae::resources {
// Native preparation attributes, not a new runtime representation: these
// attributes enter GLB before smoothing/LOD duplicate the imported vertices.
struct FbxDeformationPolicy {
  bool reduceWeightsToFour=false;
  bool allowStepApproximation=false;
  bool sampleMorph=false;
  u32 bakeRate=30;
  u32 maximumTakes=128;
  u64 maximumKeys=262144;
  u64 maximumBytes=128ull<<20;
};
struct FbxSkinAttributes {
  std::vector<u32> jointNodes;
  std::vector<float> inverseBinds;
  // Four entries per control vertex, indexed by mesh.vertex_indices[index].
  std::vector<u16> joints;
  std::vector<float> weights;
  bool reducedWeights=false;
  // Skin marcado como dual quaternion (ou misto) no FBX, deformado com os mesmos
  // pesos em mistura linear, como o SkinnedMeshRenderer da Unity faz ao importar.
  bool linearFromDualQuaternion=false;
};
struct FbxBakedTake {
  std::string name;
  float duration=0;
  double sourceStart=0;
  std::vector<AnimationChannel> channels;
  bool approximatedSteps=false;
};
bool prepareFbxSkin(const ufbx_mesh &,const ufbx_scene &,const FbxDeformationPolicy &,
                    const GltfImportProgress &,FbxSkinAttributes &,std::string &diagnostic);
bool prepareFbxTakes(const ufbx_scene &,const FbxDeformationPolicy &,
                     const GltfImportProgress &,std::vector<FbxBakedTake> &,std::string &diagnostic);
}
