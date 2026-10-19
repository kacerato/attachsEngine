#include "harness.h"
#include "resources/terrain_lod.h"
#include "resources/terrain_vertex_index.h"
#include "renderer/lod_selection.h"
#include "renderer/static_collision_mesh.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
using namespace ae;

AE_TEST(terrain_authored_lod_roundtrip_borders_selection_and_collision) {
  resources::TerrainRecipe recipe;recipe.cells=32;recipe.height=2;recipe.lodLevels=4;
  resources::TerrainPath path;path.shape=false;path.width=2;path.shoulderWidth=4;
  path.points={{-28,4,-4,false},{28,4,-4,false}};path.surfaceLayer=path.shoulderLayer=3;recipe.paths={path};
  resources::TerrainMesh mesh;std::string error;std::vector<u8> glb;
  AE_EXPECT_TRUE(resources::generateTerrain(recipe,mesh,error),error.c_str());
  AE_EXPECT_TRUE(resources::writeTerrainGlb(recipe,mesh,{},glb,error),error.c_str());
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(resources::terrainRecipeFromGlb(glb,loaded,error),error.c_str());
  resources::MeshLodSettings settings;
  AE_EXPECT_TRUE(resources::applyTerrainLodRecipe(resources::serializeTerrainRecipe(loaded),settings,error),error.c_str());
  AE_EXPECT_TRUE(settings.generate && settings.maximumLevels==4,"cold authored policy enables real generation");
  AE_EXPECT_EQ(settings.maximumRelativeError,recipe.lodRelativeError,"error budget persists");

  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  for(const auto &part:mesh.parts) {
    std::vector<resources::TerrainVertex> indexed;std::vector<u32> local;
    resources::indexTerrainVertices(part.vertices,indexed,local);
    renderer::MapDrawRecord draw{};draw.firstIndex=u32(indices.size());draw.indexCount=u32(local.size());
    draw.vertexOffset=u32(vertices.size()/renderer::MapVertexStride);draw.lodGroupId=u32(draws.size());
    for(u32 axis=0;axis<4;++axis)draw.model[axis*5]=1;
    for(const auto &v:indexed) {
      const usize offset=vertices.size();vertices.resize(offset+renderer::MapVertexStride);
      std::memcpy(vertices.data()+offset,v.position,sizeof(v.position));
      i16 normal[4]{};for(u32 k=0;k<3;++k)normal[k]=i16(std::lround(std::clamp(v.normal[k],-1.f,1.f)*32767));
      std::memcpy(vertices.data()+offset+12,normal,sizeof(normal));
      for(u32 k=0;k<4;++k)vertices[offset+44+k]=u8(std::lround(std::clamp(v.weights[k],0.f,1.f)*255));
    }
    indices.insert(indices.end(),local.begin(),local.end());draws.push_back(draw);
  }
  const auto original=indices;const auto originalVertices=vertices;
  resources::MeshLodReport report;std::vector<renderer::MeshLodLevel> levels;
  AE_EXPECT_TRUE(resources::buildMeshLods(vertices,indices,draws,{},settings,levels,report,error),error.c_str());
  AE_EXPECT_TRUE(!levels.empty(),"actual terrain topology produces simplified levels");
  AE_EXPECT_EQ(vertices,originalVertices,"LOD shares source vertices");
  AE_EXPECT_TRUE(std::equal(original.begin(),original.end(),indices.begin()),"level zero is untouched");
  // All open edges of each source chunk must survive at every level, not
  // just its outer bounding-box corners. Adjacent chunks may choose any LOD.
  for(const auto &level:levels) {
    const auto &draw=draws[level.draw];
    using Edge=std::pair<u32,u32>;
    std::map<Edge,u32> counts;std::set<Edge> reduced;
    const auto edges=[&](u32 first,u32 count,auto &&emit) {
      for(u32 i=first;i<first+count;i+=3)for(u32 j=0;j<3;++j) {
        auto a=indices[i+j],b=indices[i+(j+1)%3];if(a>b)std::swap(a,b);emit(Edge{a,b});
      }
    };
    edges(draw.firstIndex,draw.indexCount,[&](Edge e){++counts[e];});
    edges(level.firstIndex,level.indexCount,[&](Edge e){reduced.insert(e);});
    for(const auto &[edge,count]:counts)if(count==1)AE_EXPECT_TRUE(reduced.contains(edge),"every source border edge survives simplification");
    AE_EXPECT_TRUE(level.indexCount<draw.indexCount,"real reduction, no duplicate named level");
    u8 before=0,after=0;
    for(u32 i=draw.firstIndex;i<draw.firstIndex+draw.indexCount;++i)
      before=std::max(before,vertices[(usize(draw.vertexOffset)+indices[i])*renderer::MapVertexStride+47]);
    for(u32 i=level.firstIndex;i<level.firstIndex+level.indexCount;++i)
      after=std::max(after,vertices[(usize(draw.vertexOffset)+indices[i])*renderer::MapVertexStride+47]);
    AE_EXPECT_TRUE(after+3>=before,"narrow painted path is retained at every reduced level");
  }
  std::vector<renderer::LodLevelInfo> selection{{0,0}};
  for(const auto &level:levels)if(level.draw==levels.front().draw)selection.push_back({level.level,level.geometricError});
  renderer::LodHysteresisState state;
  const auto far=renderer::selectLodLevel(selection.data(),u32(selection.size()),10000,1,720,1,.8f,state);
  AE_EXPECT_TRUE(far.level>0,"distant terrain selects a real reduced level");
  const auto near=renderer::selectLodLevel(selection.data(),u32(selection.size()),.001f,1,720,1,.8f,state);
  AE_EXPECT_EQ(near.level,0u,"approaching immediately restores detail");
  renderer::MapMaterialRecord material{};
  renderer::MapPackageView package;package.header.vertexCount=u32(vertices.size()/renderer::MapVertexStride);
  package.header.vertexStride=renderer::MapVertexStride;package.header.indexCount=u32(indices.size());
  package.vertices=vertices;package.indices=indices;package.draws=draws;package.materials={&material,1};
  renderer::StaticCollisionMesh collision;
  AE_EXPECT_TRUE(renderer::buildStaticCollisionMesh(package,collision),"production collision converter accepts LOD buffers");
  AE_EXPECT_EQ(collision.indices.size(),original.size(),"render LODs never duplicate or simplify collision");
  // Invalid/truncated authored data cannot partly overwrite a valid policy.
  const auto saved=settings;
  AE_EXPECT_TRUE(!resources::applyTerrainLodRecipe("AETERRAIN6 0",settings,error),"truncated authoring is rejected");
  AE_EXPECT_EQ(settings.maximumLevels,saved.maximumLevels,"failed read preserves prior policy");
  recipe.lodLevels=1;recipe.lodRelativeError=.02f;
  AE_EXPECT_TRUE(resources::applyTerrainLodRecipe(resources::serializeTerrainRecipe(recipe),settings,error),error.c_str());
  AE_EXPECT_TRUE(!settings.generate,"explicit one-level recipe disables the caller's generation flag");
}
