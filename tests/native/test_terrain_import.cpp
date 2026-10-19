#include "harness.h"
#include "resources/gltf_import.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_textures.generated.h"
#include <cmath>
#include <cstring>
using namespace ae;

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
