#include "harness.h"
#include "resources/tga_validation.h"
#include "resources/image_decode.h"
#include "editor/editor_import_transaction.h"
#include <cstdlib>
#include <cstdio>
using namespace ae;
AE_TEST(tga_preflight_accepts_authored_orientation_and_refuses_truncated_rle) {
  std::vector<u8> bytes(18);bytes[2]=2;bytes[12]=2;bytes[14]=1;bytes[16]=24;bytes[17]=0x10;
  bytes.insert(bytes.end(),{0,0,255,0,255,0});resources::TgaImageInfo info;std::string error;
  AE_EXPECT_TRUE(resources::readTgaInfo(bytes,info)&&info.rightOrigin&&!info.hasAlpha,"right-origin RGB24 metadata");
  AE_EXPECT_TRUE(resources::validateTgaPixels(bytes,info,error),error.c_str());
  resources::DecodedImage decoded;
  AE_EXPECT_TRUE(resources::decodeImageRgba8(bytes,{},decoded,error),error.c_str());
  AE_EXPECT_TRUE(decoded.rgba==std::vector<u8>({0,255,0,255,255,0,0,255}),"right-origin RGB normalized into top-left RGBA");
  bytes.pop_back();AE_EXPECT_TRUE(!resources::validateTgaPixels(bytes,info,error),"raw truncation refused before stb allocation");
  bytes.resize(18);bytes[2]=10;bytes.insert(bytes.end(),{0x81,0,0,255});
  AE_EXPECT_TRUE(resources::readTgaInfo(bytes,info)&&resources::validateTgaPixels(bytes,info,error),"bounded RLE run");
  AE_EXPECT_TRUE(resources::decodeImageRgba8(bytes,{},decoded,error)&&decoded.rgba==std::vector<u8>({255,0,0,255,255,0,0,255}),"actual RLE pixels");
  bytes[18]=0x82;AE_EXPECT_TRUE(!resources::validateTgaPixels(bytes,info,error),"oversized run refused");
  bytes[18]=0x81;bytes.pop_back();AE_EXPECT_TRUE(!resources::validateTgaPixels(bytes,info,error),"truncated run refused");
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(bytes,{},decoded,error)&&decoded.rgba.empty(),"decoder fails closed on permissive stb truncation");
  bytes.resize(18);bytes[2]=2;bytes[16]=32;bytes[17]=0x28;bytes.insert(bytes.end(),{0,0,255,7,0,255,0,240});
  AE_EXPECT_TRUE(resources::decodeImageRgba8(bytes,{},decoded,error)&&decoded.rgba==std::vector<u8>({255,0,0,7,0,255,0,240}),"declared alpha preserved with top origin");
  bytes[17]=0x20;
  AE_EXPECT_TRUE(resources::decodeImageRgba8(bytes,{},decoded,error)&&decoded.rgba[3]==255&&decoded.rgba[7]==255,"undeclared attribute bytes treated as opaque");
  resources::ImageDecodeLimits limited;limited.maximumPixels=1;
  AE_EXPECT_TRUE(!resources::decodeImageRgba8(bytes,limited,decoded,error)&&decoded.rgba.empty(),"pixel budget checked before decoding");
  bytes.resize(18);bytes[2]=2;bytes[16]=16;bytes[17]=1;
  AE_EXPECT_TRUE(!resources::readTgaInfo(bytes,info),"16bit truecolor alpha not silently discarded by stb");
  // Private acceptance corpus is opt-in; no original asset is embedded in
  // this test or redistributed with the engine. The log separates it from
  // the portable synthetic contract above.
  if(const char *sourceRoot=std::getenv("ASTRA_TGA_SOURCE_ROOT")) {
    u32 decodedSources=0;std::error_code scanError;
    std::filesystem::recursive_directory_iterator entry(sourceRoot,scanError),end;
    AE_EXPECT_TRUE(!scanError,"private source root available");
    for(;entry!=end;entry.increment(scanError)) {
      AE_EXPECT_TRUE(!scanError,"private corpus enumeration");
      auto extension=entry->path().extension().string();
      for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".tga")continue;
      std::vector<u8> original;
      AE_EXPECT_TRUE(editor::EditorImportTransaction::read(entry->path(),original),"private TGA readable");
      AE_EXPECT_TRUE(resources::decodeImageRgba8(original,{},decoded,error),error.c_str());
      ++decodedSources;
    }
    AE_EXPECT_EQ(decodedSources,7u,"all supplied TGA sources decoded with actual native backend");
    std::printf("TGA_SOURCE_ACCEPTANCE: %u supplied sources decoded\n",decodedSources);
  } else std::printf("TGA_SOURCE_ACCEPTANCE: private corpus not requested\n");
}
