#include "harness.h"
#include "fbx_fixture.h"
#include "resources/model_import.h"
#include "resources/fbx_import.h"
#include "editor/editor_import_transaction.h"
#include <cstdlib>
#include <cstdio>
#include <map>
using namespace ae;
AE_TEST(fbx_static_native_decoder_produces_existing_typed_geometry_and_refuses_bad_sources) {
  const std::string source=test::FbxTriangle;const std::span bytes{reinterpret_cast<const u8*>(source.data()),source.size()};
  AE_EXPECT_TRUE(resources::isFbxSource(bytes),"ASCII FBX decoder selected by actual header");
  resources::GltfImport model;resources::GltfImportLimits limits;
  AE_EXPECT_TRUE(resources::importModelSource(bytes,limits,{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.draws.size()==1&&model.indices.size()==3&&!model.vertices.empty()&&model.nodes.size()>=2,"same renderable typed IR as GLB");
  const auto node=model.drawNodes.front();
  AE_EXPECT_TRUE(node<model.nodes.size()&&model.nodes[node].name=="Triangle","editable source hierarchy retained");
  AE_EXPECT_TRUE(std::abs(model.nodes[node].localMatrix[12]-2)<.0001f&&std::abs(model.nodes[node].localMatrix[13]-3)<.0001f&&std::abs(model.nodes[node].localMatrix[14]-4)<.0001f,"meters and authored local translation preserved");
  limits.maximumVertices=2;
  AE_EXPECT_TRUE(!resources::importStaticFbx(bytes,limits,{},model)&&model.draws.empty()&&!model.diagnostic.empty(),"budget failure leaves no partial geometry");
  limits={};resources::GltfImportProgress cancelled;cancelled.cancelled=[](void*){return true;};
  AE_EXPECT_TRUE(!resources::importStaticFbx(bytes,limits,cancelled,model)&&model.cancelled&&model.draws.empty(),"cancellation before native parser");
  std::vector<u8> malformed{1,2,3,4,5};
  AE_EXPECT_TRUE(!resources::importStaticFbx(malformed,limits,{},model)&&model.draws.empty(),"unknown source cannot masquerade as FBX geometry");
  if(const auto *root=std::getenv("ASTRA_FBX_SOURCE_ROOT")) {
    std::error_code error;std::filesystem::recursive_directory_iterator file(root,error),end;u32 count=0;
    AE_EXPECT_TRUE(!error,"private static corpus root");
    for(;file!=end;file.increment(error)) {
      AE_EXPECT_TRUE(!error,"private static corpus enumeration");auto extension=file->path().extension().string();
      for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".fbx")continue;
      std::vector<u8> encoded;AE_EXPECT_TRUE(editor::EditorImportTransaction::read(file->path(),encoded),"private FBX source readable");
      AE_EXPECT_TRUE(resources::importModelSource(encoded,limits,{},model),model.diagnostic.c_str());
      AE_EXPECT_TRUE(!model.draws.empty()&&!model.vertices.empty(),"private static FBX consumed by real backend");++count;
    }
    AE_EXPECT_EQ(count,520u,"complete supplied POLYGON static model family");std::printf("FBX_SOURCE_ACCEPTANCE: %u static originals decoded\n",count);
  } else std::printf("FBX_SOURCE_ACCEPTANCE: private corpus not requested\n");
  if(const auto *root=std::getenv("ASTRA_FBX_DEFORMATION_ROOT")) {
    std::error_code error;std::filesystem::recursive_directory_iterator file(root,error),end;
    u32 files=0,accepted=0,skinned=0,morphed=0,animated=0,meshless=0;std::map<std::string,u32> refused;
    limits.fbxDeformation=true;limits.fbxReduceWeights=true;limits.fbxApproximateSteps=true;
    AE_EXPECT_TRUE(!error,"private deformation corpus root");
    for(;file!=end;file.increment(error)) {
      AE_EXPECT_TRUE(!error,"private deformation corpus enumeration");auto extension=file->path().extension().string();
      for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".fbx")continue;
      ++files;std::vector<u8> encoded;AE_EXPECT_TRUE(editor::EditorImportTransaction::read(file->path(),encoded),"private deforming source readable");
      if(!resources::importModelSource(encoded,limits,{},model)) {
        AE_EXPECT_TRUE(!model.diagnostic.empty()&&model.draws.empty()&&model.animations.empty(),"unsupported source refuses whole candidate with actionable diagnostic");
        ++refused[model.diagnostic];continue;
      }
      ++accepted;skinned+=!model.skins.empty();morphed+=!model.morphs.empty();animated+=!model.animations.empty();meshless+=model.draws.empty();
      AE_EXPECT_TRUE(!model.nodes.empty()&&(!model.draws.empty()||!model.animations.empty())&&!model.skippedSkins&&!model.unsupportedAnimationChannels,"accepted source enters the real typed consumer without deformation fallback");
      for(const auto &clip:model.animations)for(const auto &channel:clip.channels)
        AE_EXPECT_TRUE(resources::validAnimationChannel(channel)&&channel.node<model.nodes.size(),"actual corpus samplers reference valid imported hierarchy");
    }
    AE_EXPECT_EQ(files,151u,"complete supplied deforming FBX corpus classified");
    AE_EXPECT_TRUE(skinned&&morphed&&animated&&meshless,"all deformation families have real imported originals");
    std::printf("FBX_DEFORMATION_ACCEPTANCE: originals=%u accepted=%u refused=%u skinned=%u morph=%u takes=%u meshless=%u\n",files,accepted,files-accepted,skinned,morphed,animated,meshless);
    for(const auto &[reason,count]:refused)std::printf("FBX_DEFORMATION_REFUSAL: %u %s\n",count,reason.c_str());
  }
}
