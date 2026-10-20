#include "harness.h"
#include "resources/gltf_import.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_textures.generated.h"
#include "resources/import_cache.h"
#include "resources/terrain_surface_import.h"
#include "resources/terrain_png_integrity.h"
#include <algorithm>
#include <cmath>
#include <cstring>
using namespace ae;
AE_TEST(terrain_surface_v3_recipe_glb_cache_and_role_views_are_lossless) {
  using namespace resources;std::string error;
  TerrainRecipe r;r.cells=8;r.controlCells=32;r.height=0;r.textureScale=7;
  const auto imageId=assetGuidFromSeed("terrain-surface-original");
  TerrainSurfaceLayer layer;layer.identity=assetGuidFromSeed("terrain-surface-layer");layer.name="Layer with provenance";
  layer.sourceCatalogId="catalog/source";layer.sourcePropertiesJson="{\"gain\":0.37}";
  layer.sourcePackingJson="{\"normal\":\"RGB\"}";layer.sourceResolvedMapsJson="{\"original\":true}";
  layer.maps[0].texture=imageId;layer.maps[0].channel=TerrainSurfaceChannel::RGB;layer.maps[0].linear=false;
  layer.maps[1].texture=imageId;layer.maps[1].channel=TerrainSurfaceChannel::NormalRGB;layer.maps[1].linear=true;
  layer.maps[4].texture=imageId;layer.maps[4].linear=true;
  layer.detailNoise.enabled=true;layer.detailNoise.map.texture=imageId;layer.detailNoise.strength=.37f;
  layer.resampleAlbedo=true;layer.resampleScale=.7f;layer.tint={.7f,.8f,.9f,1};
  r.surface.layers={layer};r.surface.normalNoise[2].enabled=true;r.surface.normalNoise[2].map=layer.maps[1];
  TerrainRecipe reopened;const auto recipe=serializeTerrainRecipe(r);
  AE_EXPECT_TRUE(deserializeTerrainRecipe(recipe,reopened,error),error.c_str());
  AE_EXPECT_EQ(serializeTerrainRecipe(reopened),recipe,"recipe eight preserves every nested surface property");
  const std::array<std::vector<u8>,4> legacy{{
    {std::begin(terrainTexture0),std::end(terrainTexture0)},
    {std::begin(terrainTexture1),std::end(terrainTexture1)},
    {std::begin(terrainTexture2),std::end(terrainTexture2)},
    {std::begin(terrainTexture3),std::end(terrainTexture3)}}};
  const std::vector<TerrainSurfaceImage> images{{imageId,legacy[0]}};
  TerrainBuild build;AE_EXPECT_TRUE(updateTerrain(r,nullptr,build,error),error.c_str());
  std::vector<u8> glb;AE_EXPECT_TRUE(writeTerrainGlb(build,legacy,images,glb,error),error.c_str());
  GltfImport imported;GltfImportLimits limits;limits.maximumTextureDimension=32;limits.textureCompression=8;
  AE_EXPECT_TRUE(importGlb(glb,limits,{},imported),imported.diagnostic.c_str());
  AE_EXPECT_EQ(imported.terrainSurfaces.size(),imported.materials.size(),"surface binding includes neutral slot");
  const auto &binding=imported.terrainSurfaces[0];
  AE_EXPECT_EQ(binding.localMetersPerUv,7.f,"local meter conversion persists");
  std::string originalSurface,actualSurface;
  AE_EXPECT_TRUE(serializeTerrainSurface(r.surface,originalSurface,error),error.c_str());
  AE_EXPECT_TRUE(serializeTerrainSurface(binding.surface,actualSurface,error),error.c_str());
  AE_EXPECT_EQ(originalSurface,actualSurface,"GLB keeps unsupported properties and provenance");
  AE_EXPECT_EQ(binding.textures.size(),usize(3),"one original supplies independent sRGB, linear and normal views");
  for(const auto &entry:binding.textures) {
    AE_EXPECT_TRUE(entry.textureIndex<imported.textures.size(),"resolved role index valid");
    if(entry.textureIndex>=imported.textures.size())return;
    const auto &texture=*imported.textures[entry.textureIndex];
    AE_EXPECT_TRUE(texture.width>32 && texture.levels>1,"PBR keeps original dimensions and full mips");
    AE_EXPECT_TRUE(texture.srgb!=entry.linear,"role selects correct color space");
    AE_EXPECT_EQ(texture.format,renderer::AuthoringTextureRgba8,"quality profile cannot ASTC-compress source views");
  }
  AE_EXPECT_EQ(imported.terrainSurfaceImages.size(),usize(1),"original GUID PNG retained once");
  AE_EXPECT_TRUE(*imported.terrainSurfaceImages[0].encoded==*images[0].encoded,"original PNG bytes survive import");
  std::vector<u8> cache;const auto key=importCacheKey("terrain-surface-v3",limits);
  AE_EXPECT_TRUE(writeImportCache(imported,key,cache),"surface cache write");
  GltfImport cached;AE_EXPECT_TRUE(readImportCache(cache,key,cached),"surface cache read");
  AE_EXPECT_TRUE(*cached.terrainSurfaceImages[0].encoded==*images[0].encoded,"cache preserves original PNG");
  std::string cachedSurface;AE_EXPECT_TRUE(serializeTerrainSurface(cached.terrainSurfaces[0].surface,cachedSurface,error),error.c_str());
  AE_EXPECT_EQ(cachedSurface,originalSurface,"cache preserves full typed schema");
  AE_EXPECT_TRUE(validTerrainPng(images[0].bytes()),"original PNG structure and CRC valid");
  auto corruptBytes=*images[0].encoded;corruptBytes[29]^=1; // IHDR CRC only; decoded pixels would be unchanged.
  AE_EXPECT_TRUE(!validTerrainPng(corruptBytes),"CRC-only corruption rejected");
  std::vector<TerrainSurfaceImage> corruptImages{{imageId,corruptBytes}};
  std::vector<renderer::SharedAuthoringTexture> protectedTextures=imported.textures;
  renderer::TerrainSurfaceBinding protectedBinding=binding;
  AE_EXPECT_TRUE(!importTerrainSurface(r.surface,corruptImages,7,256ull<<20,protectedTextures,protectedBinding,error),"decoder refuses PNG with bad CRC");
  std::vector<u8> protectedGlb{1,2,3};
  AE_EXPECT_TRUE(!writeTerrainGlb(build,legacy,corruptImages,protectedGlb,error),"export refuses corrupt original PNG");
  AE_EXPECT_TRUE(protectedGlb==std::vector<u8>({1,2,3}),"corrupt original leaves export atomic");
  auto corruptedCache=cache;
  const auto originalPng=std::search(corruptedCache.begin(),corruptedCache.end(),images[0].bytes().begin(),images[0].bytes().end());
  AE_EXPECT_TRUE(originalPng!=corruptedCache.end(),"cache contains original PNG payload");
  if(originalPng!=corruptedCache.end()) {
    *(originalPng+29)^=1;
    GltfImport rejected;AE_EXPECT_TRUE(!readImportCache(corruptedCache,key,rejected),"cache reader rejects corrupted original PNG CRC");
  }
  auto corruptModel=imported;corruptModel.terrainSurfaceImages=corruptImages;
  std::vector<u8> rejectedCache;AE_EXPECT_TRUE(!writeImportCache(corruptModel,key,rejectedCache),"cache writer rejects PNG CRC corruption");
  corruptImages[0]=TerrainSurfaceImage(imageId,std::vector<u8>(images[0].bytes().begin(),images[0].bytes().begin()+8));
  AE_EXPECT_TRUE(!validTerrainPng(corruptImages[0].bytes()),"signature-only truncated PNG rejected");
  AE_EXPECT_TRUE(!writeTerrainGlb(build,legacy,corruptImages,protectedGlb,error),"export refuses signature-only PNG");
  auto noIend=*images[0].encoded;noIend.resize(noIend.size()-12);
  AE_EXPECT_TRUE(!validTerrainPng(noIend),"missing terminal IEND rejected");
  auto trailing=*images[0].encoded;trailing.push_back(0);
  AE_EXPECT_TRUE(!validTerrainPng(trailing),"trailing bytes after IEND rejected");
  auto invalid=imported;invalid.terrainSurfaces[0].textures[0].textureIndex=renderer::InvalidMapTexture;
  AE_EXPECT_TRUE(!writeImportCache(invalid,key,cache),"cache rejects invalid surface texture reference");
  std::vector<renderer::SharedAuthoringTexture> unchanged=imported.textures;
  renderer::TerrainSurfaceBinding unchangedBinding=binding;
  AE_EXPECT_TRUE(!importTerrainSurface(r.surface,images,7,1,unchanged,unchangedBinding,error),"full quality fails explicit insufficient budget");
  AE_EXPECT_EQ(unchanged.size(),imported.textures.size(),"decoder failure preserves texture output");
  TerrainSurfaceAsset rgSurface;TerrainSurfaceLayer rgLayer;
  rgLayer.identity=assetGuidFromSeed("normal-rg-layer");rgLayer.maps[1].texture=imageId;
  rgLayer.maps[1].channel=TerrainSurfaceChannel::NormalRG;rgSurface.layers={rgLayer};
  std::vector<renderer::SharedAuthoringTexture> rgTextures;renderer::TerrainSurfaceBinding rgBinding;
  AE_EXPECT_TRUE(importTerrainSurface(rgSurface,images,1,256ull<<20,rgTextures,rgBinding,error),error.c_str());
  DecodedImage raw;AE_EXPECT_TRUE(decodeImageRgba8(images[0].bytes(),{},raw,error),error.c_str());
  AE_EXPECT_TRUE(std::equal(raw.rgba.begin(),raw.rgba.end(),rgTextures[0]->mipChain.begin()),"RG-normal mip derivation preserves original level zero channels");
  AE_EXPECT_TRUE(rgBinding.textures[0].normal && rgBinding.textures[0].linear,"RG normal view is explicitly linear");
}

AE_TEST(terrain_high_resolution_control_png_survives_real_glb_import) {
  resources::TerrainRecipe r;r.cells=8;r.controlCells=128;r.height=0;r.solidBase=true;
  resources::TerrainStroke stroke;stroke.tool=resources::TerrainTool::Paint;
  stroke.x=3;stroke.z=3;stroke.radius=1;stroke.layer=3;stroke.hardness=1;r.strokes={stroke};
  resources::TerrainField field;resources::TerrainMesh mesh;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_TRUE(resources::meshTerrainField(r,field,mesh,error),error.c_str());
  bool top=false,closed=false;float vertexPath=0;
  for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
    if(v.uv1[1]>=0){top=true;vertexPath=std::max(vertexPath,v.weights[3]);}
    else {closed=true;AE_EXPECT_TRUE(std::abs(-v.uv1[1]-1-r.macroVariation)<.00001f,"closed surface macro variation preserved");}
  }
  AE_EXPECT_TRUE(top&&closed,"top eligibility distinguishes closed faces");
  AE_EXPECT_TRUE(vertexPath<.001f,"narrow paint lies between coarse geometry vertices");
  AE_EXPECT_TRUE(resources::terrainFieldWeights(field,3,3)[3]>.99f,"independent map retains narrow paint");
  const std::array<std::vector<u8>,4> textures{{
    {std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},
    {std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},
    {std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},
    {std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
  std::vector<u8> glb;AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,textures,glb,error),error.c_str());
  resources::TerrainBuild prepared;prepared.recipe=r;prepared.field=field;prepared.mesh=mesh;
  std::vector<u8> reused;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(prepared,textures,reused,error),error.c_str());
  AE_EXPECT_TRUE(reused==glb,"prepared field export equals replay export byte for byte");
  prepared.field.controlWidth=9;reused={1,2,3};
  AE_EXPECT_TRUE(!resources::writeTerrainGlb(prepared,textures,reused,error),"incompatible prepared field rejected");
  AE_EXPECT_TRUE(reused==std::vector<u8>({1,2,3}),"prepared export failure preserves output");
  resources::GltfImport imported;resources::GltfImportLimits limits;
  limits.maximumTextureDimension=32;limits.textureCompression=8;
  AE_EXPECT_TRUE(resources::importGlb(glb,limits,{},imported),imported.diagnostic.c_str());
  AE_EXPECT_EQ(imported.terrainControls.size(),imported.materials.size(),"bindings align materials including neutral");
  const auto &binding=imported.terrainControls[0];
  AE_EXPECT_TRUE(binding.textureIndex<imported.textures.size(),"dedicated control binding resolves");
  if(binding.textureIndex>=imported.textures.size())return;
  const auto &texture=*imported.textures[binding.textureIndex];
  AE_EXPECT_EQ(texture.width,129u,"resident quality policy never downsizes control data");
  AE_EXPECT_EQ(texture.levels,1u,"single level pins control resolution");
  AE_EXPECT_TRUE(!texture.srgb&&texture.format==renderer::AuthoringTextureRgba8,"control remains linear uncompressed data");
  const auto sampler=renderer::decodeTextureSampler(texture.samplerFlags);
  AE_EXPECT_TRUE(sampler.minLinear&&sampler.magLinear&&!sampler.mipEnabled,"linear sampling without mip bias");
  AE_EXPECT_TRUE(!(texture.samplerFlags&(renderer::AuthoringTextureRepeatU|renderer::AuthoringTextureRepeatV)),"control clamps at boundaries");
  renderer::SharedAuthoringTexture expected;AE_EXPECT_TRUE(resources::makeTerrainControlTexture(field,expected,error),error.c_str());
  AE_EXPECT_TRUE(texture.mipChain==expected->mipChain,"lossless PNG preserves every control byte");
  const float first=(-r.size*.5f/r.textureScale)*binding.uvScale[0]+binding.uvOffset[0];
  AE_EXPECT_TRUE(std::abs(first-.5f/texture.width)<.000001f,"first sample maps to texel center");
  for(u32 i=1;i<4;++i)AE_EXPECT_EQ(imported.terrainControls[i].textureIndex,binding.textureIndex,"materials share one control texture");
  AE_EXPECT_EQ(imported.terrainControls.back().textureIndex,renderer::InvalidMapTexture,"neutral material has no control");
}

AE_TEST(terrain_all_modes_export_and_import_real_parser) {
  for (u32 mode = 0; mode < 3; ++mode) {
    resources::TerrainRecipe r;
    r.mode = resources::TerrainMode(mode);
    r.cells = 16;
    resources::TerrainMesh mesh;
    std::string error;
    AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
    std::vector<u8> bytes;
    AE_EXPECT_TRUE(resources::writeTerrainGlb(r, mesh, {}, bytes, error),
                   error.c_str());
    resources::GltfImport model;
    AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, model),
                   model.diagnostic.c_str());
    AE_EXPECT_EQ(model.draws.size(), mesh.parts.size(),
                 "all chunk layers imported");
    resources::TerrainRecipe loaded;
    AE_EXPECT_TRUE(resources::terrainRecipeFromGlb(bytes, loaded, error),
                   error.c_str());
    AE_EXPECT_EQ(resources::serializeTerrainRecipe(r),
                 resources::serializeTerrainRecipe(loaded), "recipe in GLB");
    for (const auto &part : mesh.parts)
      for (const auto &v : part.vertices) {
        float n = 0;
        for (float value : v.normal)
          n += value * value;
        AE_EXPECT_TRUE(std::isfinite(n) && std::abs(n - 1) < .01f,
                       "finite unit normals");
      }
  }
}

AE_TEST(terrain_revision3_blended_weights_and_material_bindings_survive_import) {
  resources::TerrainRecipe r;r.cells=16;r.height=0;
  r.strokes={{resources::TerrainTool::Paint,0,4,0,20,.5f,3}};
  resources::TerrainMesh mesh;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  bool blended=false,unpainted=false;
  for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
    float sum=0;for(float w:v.weights){AE_EXPECT_TRUE(w>=0&&w<=1,"finite normalized channel");sum+=w;}
    AE_EXPECT_TRUE(std::abs(sum-1)<1e-5f,"splat weights preserve energy");
    blended|=v.weights[3]>0&&v.weights[3]<.51f;unpainted|=v.weights[3]==0;
  }
  AE_EXPECT_TRUE(blended&&unpainted,"soft painting has mixed border and untouched exterior");
  const std::array<std::vector<u8>,4> textures{{
    {std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},
    {std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},
    {std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},
    {std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
  std::vector<u8> bytes;resources::GltfImport imported;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,textures,bytes,error),error.c_str());
  AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},imported),imported.diagnostic.c_str());
  for(const auto &draw:imported.draws) {
    const auto &material=imported.materials[draw.materialIndex];
    AE_EXPECT_TRUE(material.flags&renderer::MapMaterialTerrain,"terrain shader contract survives GLB loader");
    AE_EXPECT_TRUE(!(material.flags&renderer::MapMaterialNormalMap),"rock albedo is not interpreted as a normal map");
  }
  AE_EXPECT_TRUE(imported.textures.size()>=4,"all four layers decoded");
  for(const auto &texture:imported.textures)AE_EXPECT_TRUE(texture&&texture->srgb,"all terrain layer bindings decode as sRGB albedo");
}

AE_TEST(terrain_indexed_export_preserves_triangles_and_supports_locked_border_lods) {
  resources::TerrainRecipe r;r.cells=32;r.height=0;r.lodLevels=3;
  resources::TerrainMesh mesh;std::string error;std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,{},bytes,error),error.c_str());
  // No caller flags: a cold import must recover the authored policy itself.
  resources::GltfImportLimits limits;
  resources::GltfImport model;AE_EXPECT_TRUE(resources::importGlb(bytes,limits,{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.vertices.size()/renderer::MapVertexStride<mesh.triangles*3/2,"lossless sharing reduces resident vertex data by more than half");
  AE_EXPECT_TRUE(!model.meshLods.empty(),"indexed topology supports actual border-locked LOD reduction");
  for(const auto &level:model.meshLods)AE_EXPECT_TRUE(level.level<r.lodLevels,"authored level count bounds the cold import");
  for(usize part=0;part<mesh.parts.size();++part) {
    const auto &draw=model.draws[part];
    AE_EXPECT_EQ(draw.indexCount,mesh.parts[part].vertices.size(),"all source triangles retained in level zero");
    for(u32 i=0;i<draw.indexCount;++i) {
      const auto index=draw.vertexOffset+model.indices[draw.firstIndex+i];
      float position[3];std::memcpy(position,model.vertices.data()+index*renderer::MapVertexStride,12);
      for(u32 axis=0;axis<3;++axis)AE_EXPECT_EQ(position[axis],mesh.parts[part].vertices[i].position[axis],"indexed export preserves exact triangle positions");
    }
  }
  // A saved one-level terrain must also override an enabling generic profile.
  // This catches policy application in the wrong order in the real importer.
  r.lodLevels=1;limits.generateLods=true;limits.maximumLodLevels=4;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,{},bytes,error),error.c_str());
  AE_EXPECT_TRUE(resources::importGlb(bytes,limits,{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.meshLods.empty(),"authored one-level policy wins over generic import profile");
}

AE_TEST(terrain_texture_sharing_preserves_sampler_contract_and_lifetime) {
  resources::TerrainRecipe r;r.cells=8;r.height=0;
  resources::TerrainMesh mesh;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  const std::vector<u8> png{std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)};
  const std::array<std::vector<u8>,4> textures{png,png,png,png};
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,textures,bytes,error),error.c_str());
  std::weak_ptr<const renderer::AuthoringTexture> lifetime;
  {
    resources::GltfImport first,second,clamped;
    AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},first),first.diagnostic.c_str());
    AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},second),second.diagnostic.c_str());
    AE_EXPECT_TRUE(!first.textures.empty(),"actual PNG decoded");
    AE_EXPECT_TRUE(first.textures.front()==second.textures.front(),"separate imports share identical immutable pixels");
    lifetime=first.textures.front();
    const std::string repeat="10497",clamp="33071";
    auto found=std::search(bytes.begin(),bytes.end(),repeat.begin(),repeat.end());
    AE_EXPECT_TRUE(found!=bytes.end(),"export contains explicit repeat sampler");
    std::copy(clamp.begin(),clamp.end(),found);
    AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},clamped),clamped.diagnostic.c_str());
    AE_EXPECT_TRUE(first.textures.front()!=clamped.textures.front(),"different wrap behavior never aliases despite identical pixels");
    AE_EXPECT_TRUE(first.textures.front()->mipChain==clamped.textures.front()->mipChain,"no mip, resolution or pixel quality change");
  }
  AE_EXPECT_TRUE(lifetime.expired(),"interning never retains pixels after their owners are released");
}
