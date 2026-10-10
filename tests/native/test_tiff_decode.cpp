#include "harness.h"
#include "resources/tiff_decode.h"
#include "resources/texture_asset.h"
#include "editor/editor_import_transaction.h"
#include <cstdlib>
#include <cstdio>
using namespace ae;
#include "tiff_fixture.h"
using ae::test::tiffFixture;
AE_TEST(tiff_classic_bounded_decoder_requires_explicit_alpha_precision_and_color_choices) {
  resources::ImageDecodeLimits limits;resources::TiffDecodePolicy policy;
  resources::DecodedImage image;resources::TiffInfo info;std::vector<std::string> losses;std::string error;
  const auto little=tiffFixture(true,8);const auto big=tiffFixture(false,16,6);
  resources::TextureProfile recipe,restored;
  const auto legacy="{\"schema\":4,\"interpretation\":0,\"maximumDimension\":0,\"mipmaps\":1,\"dilateEdges\":0,\"anisotropy\":1,\"invertNormalGreen\":0,\"preserveAlphaCoverage\":0,\"alphaCoverageCutoff\":500,\"streamingMipmaps\":1,\"streamingPriority\":0}";
  AE_EXPECT_TRUE(resources::parseTextureProfile(legacy,restored)&&restored.tiffUndefinedExtra==0&&!restored.tiffQuantize16&&!restored.tiffIgnoreColorProfile,"legacy profiles migrate with strict source choices");
  recipe.tiffUndefinedExtra=2;recipe.tiffQuantize16=true;
  AE_EXPECT_TRUE(resources::parseTextureProfile(resources::serializeTextureProfile(recipe),restored)&&resources::sameTextureProfile(recipe,restored)&&!resources::sameTexturePreparation(recipe,{}),"persisted TIFF policy changes prepared cache identity");
  AE_EXPECT_TRUE(resources::inspectTiff(little,limits,info,error)&&info.width==2&&info.height==1&&info.undefinedExtra,"header and undefined extra read before allocation");
  AE_EXPECT_TRUE(!resources::decodeTiffRgba8(little,limits,policy,image,losses,error)&&image.rgba.empty()&&!error.empty(),"undefined extra cannot silently become alpha");
  policy.undefinedExtra=resources::TiffUndefinedExtra::Ignore;
  AE_EXPECT_TRUE(resources::decodeTiffRgba8(little,limits,policy,image,losses,error)&&image.rgba[3]==255&&losses.size()==1,"explicit opaque interpretation consumes pixels and records loss");
  policy.undefinedExtra=resources::TiffUndefinedExtra::Alpha;
  AE_EXPECT_TRUE(resources::decodeTiffRgba8(little,limits,policy,image,losses,error)&&image.rgba[3]==64,"explicit alpha interpretation consumes fourth channel");
  AE_EXPECT_TRUE(!resources::decodeTiffRgba8(big,limits,policy,image,losses,error)&&image.rgba.empty(),"16-bit precision cannot be silently truncated");
  policy.quantize16=true;
  AE_EXPECT_TRUE(resources::decodeTiffRgba8(big,limits,policy,image,losses,error)&&image.width==1&&image.height==2&&image.rgba[0]==255&&image.rgba[2]==128&&image.rgba[3]==64&&losses.size()==2,"big endian precision conversion rounds and orientation rotates actual texels");
  auto malformed=little;malformed.resize(malformed.size()-1);
  AE_EXPECT_TRUE(!resources::decodeTiffRgba8(malformed,limits,policy,image,losses,error)&&image.rgba.empty(),"truncated strip cannot publish partial pixels");
  limits.maximumPixels=1;
  AE_EXPECT_TRUE(!resources::inspectTiff(little,limits,info,error),"pixel budget checked before output allocation");limits={};
  std::atomic<bool> cancelled=true;
  AE_EXPECT_TRUE(!resources::decodeTiffRgba8(little,limits,policy,image,losses,error,&cancelled)&&image.rgba.empty(),"cancelled decode leaves no prepared texels");
  if(const auto *root=std::getenv("ASTRA_TIFF_SOURCE_ROOT")) {
    u32 count=0;std::error_code failure;std::filesystem::recursive_directory_iterator entry(root,failure),end;
    AE_EXPECT_TRUE(!failure,"supplied TIFF source root");
    for(;entry!=end;entry.increment(failure)) {
      AE_EXPECT_TRUE(!failure,"supplied TIFF enumeration");auto extension=entry->path().extension().string();
      for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".tif"&&extension!=".tiff")continue;
      std::vector<u8> source;AE_EXPECT_TRUE(editor::EditorImportTransaction::read(entry->path(),source),"supplied TIFF original readable");
      AE_EXPECT_TRUE(resources::inspectTiff(source,limits,info,error),error.c_str());
      policy.ignoreColorProfile=false;
      if(!info.colorProfile.empty()) {
        AE_EXPECT_TRUE(info.matrixSrgb,"supplied ICC recognized by matrix/TRC, not its description");
        auto changed=source;const auto icc=static_cast<usize>(info.colorProfile.data()-source.data());changed[icc+16]='L';
        AE_EXPECT_TRUE(!resources::decodeTiffRgba8(changed,limits,policy,image,losses,error)&&image.rgba.empty(),"unchanged sRGB description cannot authorize changed ICC colorimetry");
        changed=source;changed[icc+67]=3;
        AE_EXPECT_TRUE(!resources::decodeTiffRgba8(changed,limits,policy,image,losses,error)&&image.rgba.empty(),"absolute rendering intent requires explicit color conversion policy");
      }
      policy.ignoreColorProfile=true;
      AE_EXPECT_TRUE(resources::decodeTiffRgba8(source,limits,policy,image,losses,error),error.c_str());
      resources::PreparedTextureImport cooked;resources::TextureImportLimits cooking;
      AE_EXPECT_TRUE(resources::prepareTextureImport(source,recipe,true,0,cooking,nullptr,cooked)&&cooked.valid()&&cooked.sourceTiff&&!cooked.knownLosses.empty(),cooked.diagnostic.c_str());
      AE_EXPECT_TRUE(image.width&&image.height&&!losses.empty(),"supplied texels decoded with explicit losses");++count;
    }
    AE_EXPECT_EQ(count,4u,"all supplied TIFF originals");std::printf("TIFF_SOURCE_ACCEPTANCE: %u originals decoded with explicit policy\n",count);
  }
}
