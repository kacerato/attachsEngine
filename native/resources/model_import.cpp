#include "resources/model_import.h"
#include "resources/fbx_import.h"
#include "resources/fbx_scene_import.h"
#include <cstring>
namespace ae::resources {
bool isFbxSource(std::span<const u8> bytes) {
  if(bytes.empty())return false;
  if(bytes.size()>=4&&!std::memcmp(bytes.data(),"glTF",4))return false;
  constexpr char binary[]="Kaydara FBX Binary  \x00\x1a\x00";
  if(bytes.size()>=sizeof(binary)-1&&!std::memcmp(bytes.data(),binary,sizeof(binary)-1))return true;
  const std::string_view prefix(reinterpret_cast<const char*>(bytes.data()),std::min<usize>(bytes.size(),4096));
  const auto first=prefix.find_first_not_of(" \t\r\n");
  return first!=prefix.npos&&(prefix[first]==';'||prefix[first]=='F')&&prefix.find("FBXHeaderExtension:",first)!=prefix.npos;
}
bool importModelSource(std::span<const u8> bytes,const GltfImportLimits &limits,
                       const GltfImportProgress &progress,GltfImport &out) {
  if(!isFbxSource(bytes))return importGlb(bytes,limits,progress,out);
  FbxSceneImportPolicy policy;policy.deformation=limits.fbxDeformation;policy.bake.reduceWeightsToFour=limits.fbxReduceWeights;
  policy.bake.allowStepApproximation=limits.fbxApproximateSteps;policy.bake.bakeRate=limits.fbxBakeRate;
  return importFbxScene(bytes,limits,progress,policy,out);
}
}
