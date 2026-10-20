#include "harness.h"
#include "resources/terrain_normal_mips.h"
#include "resources/texture_compression.h"
#include "resources/terrain_asset.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/terrain_textures.generated.h"
#include <cmath>
using namespace ae::resources;
AE_TEST(terrain_snow_packing_and_cap_views_survive_real_glb_and_cold_cache){
  TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=8;recipe.size=48;recipe.height=0;
  TerrainSurfaceLayer layer;layer.identity=assetGuidFromSeed("snow-cold-layer");
  const AssetGuid imageId=assetGuidFromSeed("snow-shared-packed-source");
  layer.maps[1].texture=imageId;layer.maps[1].channel=TerrainSurfaceChannel::NormalRGB;
  layer.maps[1].textureResidency={TerrainTextureResidencyMode::Original,0};recipe.surface.layers={layer};
  auto &global=recipe.surface.globalMaps[1];global.enabled=true;global.map.texture=imageId;global.map.channel=TerrainSurfaceChannel::NormalRG;
  global.map.textureResidency={TerrainTextureResidencyMode::Cap,512};
  recipe.surface.snow.normal.texture=imageId;recipe.surface.snow.normal.textureResidency={TerrainTextureResidencyMode::Cap,256};
  recipe.surface.snow.smoothness.texture=imageId;recipe.surface.snow.smoothness.channel=TerrainSurfaceChannel::R;
  recipe.surface.snow.ao.texture=imageId;recipe.surface.snow.ao.channel=TerrainSurfaceChannel::B;
  recipe.surface.snow.mask.texture=imageId;recipe.surface.snow.mask.channel=TerrainSurfaceChannel::R;
  std::vector<TerrainSurfaceImage> images;images.emplace_back(imageId,std::vector<ae::u8>{std::begin(terrainTexture0),std::end(terrainTexture0)});
  const std::array<std::vector<ae::u8>,4> legacy{{{std::begin(terrainTexture0),std::end(terrainTexture0)},{std::begin(terrainTexture1),std::end(terrainTexture1)},{std::begin(terrainTexture2),std::end(terrainTexture2)},{std::begin(terrainTexture3),std::end(terrainTexture3)}}};
  TerrainBuild build;std::string error;AE_EXPECT_TRUE(updateTerrain(recipe,nullptr,build,error),error.c_str());
  std::vector<ae::u8> glb;AE_EXPECT_TRUE(writeTerrainGlb(build,legacy,images,glb,error),error.c_str());
  GltfImport imported;AE_EXPECT_TRUE(importGlb(glb,{},{},imported),imported.diagnostic.c_str());
  auto check=[&](const GltfImport &model){
    AE_EXPECT_TRUE(!model.terrainSurfaces.empty(),"GLB publishes actual surface binding");const auto &binding=model.terrainSurfaces[0];
    bool rgb=false,rg=false,ga=false,scalar=false,clamp=false;
    for(const auto &entry:binding.textures){if(entry.identity!=imageId)continue;
      rgb|=entry.normalChannel==5&&entry.residencyCap==0;rg|=entry.normalChannel==6&&entry.residencyCap==512;ga|=entry.normalChannel==7&&entry.residencyCap==256;scalar|=!entry.normal;
      AE_EXPECT_TRUE(entry.textureIndex<model.textures.size(),"independent view index in bounds");
      const auto &texture=*model.textures[entry.textureIndex];ae::u32 minimum=99;TerrainTextureResidency policy{entry.residencyCap?TerrainTextureResidencyMode::Cap:TerrainTextureResidencyMode::Original,entry.residencyCap};
      if(entry.clampSampling){clamp=true;AE_EXPECT_EQ(texture.samplerFlags&(ae::renderer::AuthoringTextureRepeatU|ae::renderer::AuthoringTextureRepeatV|ae::renderer::AuthoringTextureMirrorU|ae::renderer::AuthoringTextureMirrorV),0u,"Snow mask actual decoded sampler clamps at all mips");}
      AE_EXPECT_TRUE(terrainTextureFirstAllowedMip(texture.width,texture.height,texture.levels,policy,{},minimum),"resolved complete chain policy");AE_EXPECT_EQ(texture.minimumResidentMip,minimum,"actual decoded texture carries authored ceiling");
    }
    AE_EXPECT_TRUE(rgb&&rg&&ga&&scalar&&clamp,"one GUID retains RGB/RG/GA, scalar and Clamp views without collisions");
  };
  check(imported);const auto key=importCacheKey("snow-cold",{});std::vector<ae::u8> cache;
  AE_EXPECT_TRUE(writeImportCache(imported,key,cache),"surface v3 cache writes independent views");GltfImport cold;
  AE_EXPECT_TRUE(readImportCache(cache,key,cold),"surface v3 cold cache restores packing and caps");check(cold);
}
AE_TEST(terrain_snow_ga_normal_view_keeps_scalar_source_independent){
  DecodedImage image;image.width=2;image.height=2;
  image.rgba={16,128,32,192, 64,128,80,64, 128,128,160,192, 240,128,224,64};
  const auto original=image.rgba;std::vector<ae::u8> normals,scalar;ae::u32 levels=0,scalarLevels=0;
  AE_EXPECT_TRUE(buildTerrainGaNormalMipChain(image,normals,levels),"GA normal filtering succeeds");
  AE_EXPECT_EQ(levels,2u,"2x2 normal view has complete tail");
  AE_EXPECT_TRUE(image.rgba==original,"packed source is immutable");
  for(ae::usize i=0;i<original.size();i+=4){
    AE_EXPECT_EQ(normals[i+1],original[i+1],"level0 G is exact");
    AE_EXPECT_EQ(normals[i+3],original[i+3],"level0 A is exact");
  }
  AE_EXPECT_TRUE(std::abs(int(normals[19])-128)<=2,"opposed GA X normals average near flat");
  AE_EXPECT_TRUE(std::abs(int(normals[17])-128)<=2,"GA Y remains near flat");
  AE_EXPECT_TRUE(buildMipChain(image,false,scalar,scalarLevels),"independent linear scalar view");
  AE_EXPECT_TRUE(scalar[16]>100&&scalar[16]<120,"R smoothness averages original R, not normal X");
  AE_EXPECT_TRUE(scalar[18]>120&&scalar[18]<128,"B AO averages original B, not reconstructed Z");
  DecodedImage malformed=image;malformed.rgba.pop_back();const auto applied=normals;levels=99;
  AE_EXPECT_TRUE(!buildTerrainGaNormalMipChain(malformed,normals,levels),"truncated source rejected");
  AE_EXPECT_TRUE(normals==applied&&levels==99,"GA failure is atomic");
}

AE_TEST(terrain_texture_residency_survives_compression_and_rebases_in_preview) {
  ae::renderer::AuthoringTexture original;original.width=original.height=16;original.levels=5;
  original.minimumResidentMip=2;original.mipChain.resize(original.expectedBytes(),128);
  ae::renderer::AuthoringTexture compressed,preview;std::string error;ae::u32 first=99;
  AE_EXPECT_TRUE(compressTexture(original,TextureCompression::Astc4x4,compressed,error),error.c_str());
  AE_EXPECT_TRUE(compressed.valid() && compressed.minimumResidentMip==2 && compressed.width==16,"ASTC retains authored ceiling and original dimensions");
  AE_EXPECT_TRUE(previewTexture(original,8,preview,first,error),error.c_str());
  AE_EXPECT_TRUE(preview.valid() && first==1 && preview.minimumResidentMip==1 && preview.width==8,"preview ceiling rebases relative to its derived dimensions");
  AE_EXPECT_TRUE(previewTexture(compressed,4,preview,first,error),error.c_str());
  AE_EXPECT_TRUE(preview.valid() && first==2 && preview.minimumResidentMip==0 && preview.width==4,"decoded preview at ceiling needs no further relative restriction");
  AE_EXPECT_EQ(original.mipChain.size(),original.expectedBytes(),"preview and compression preserve source chain");
}
