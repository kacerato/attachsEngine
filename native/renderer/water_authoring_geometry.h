#pragma once
#include "renderer/map_package.h"
#include "renderer/water_grid.h"
#include <algorithm>
#include <limits>

namespace ae::renderer {
// Runtime-only library resource marker. Never instantiated by package import.
inline constexpr u32 WaterAuthoringResource = 1u << 11;
inline constexpr u32 WaterRouteResource = 1u << 12;
inline constexpr u32 BoxAuthoringResource = 1u << 13;

// Append reusable geometry before upload. Instances share these buffers; their
// transforms provide finite dimensions. Spectral displacement stays in the GPU.
inline bool appendWaterAuthoringGeometry(u32 vertexStride, u32 segments,
    std::vector<u8> &vertices, std::vector<u32> &indices,
    std::vector<MapDrawRecord> &draws, std::vector<MapMaterialRecord> &materials) {
  if(vertexStride!=MapVertexStride || vertices.size()%vertexStride ||
     vertices.size()/vertexStride>std::numeric_limits<u32>::max()) return false;
  const WaterGridSettings grid{segments,384,8000};
  if(!validateWaterGrid(grid)) return false;
  std::vector<WaterGridVertex> generated(waterGridVertexCount(grid));
  std::vector<u32> topology(waterGridIndexCount(grid));
  if(!buildWaterGrid(grid,generated.data(),generated.size(),topology.data(),topology.size())) return false;
  const usize firstWater=draws.size();
  struct Vertex {float position[3]; i16 normal[4]; i16 tangent[4];float uv[2];float metadata[2];u32 color;};
  static_assert(sizeof(Vertex)==MapVertexStride);
  for(u32 mode=0;mode<2;++mode) {
    MapDrawRecord draw{};
    draw.firstIndex=static_cast<u32>(indices.size());draw.indexCount=static_cast<u32>(topology.size());
    draw.vertexOffset=static_cast<u32>(vertices.size()/vertexStride);
    draw.materialIndex=static_cast<u32>(materials.size());draw.lodGroupId=static_cast<u32>(draws.size());
    draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;
    draw.boundsRadius=mode?11314.0f:14.142136f;
    const usize start=vertices.size();vertices.resize(start+generated.size()*sizeof(Vertex));
    for(u32 i=0;i<generated.size();++i) {
      const auto &source=generated[i];
      Vertex vertex{};vertex.normal[1]=32767;vertex.tangent[0]=vertex.tangent[3]=32767;
      vertex.color=0xffffffffu;
      std::copy(source.uv,source.uv+2,vertex.uv);
      if(mode) {
        std::copy(source.position,source.position+3,vertex.position);
        vertex.metadata[0]=source.bandLimit;vertex.metadata[1]=grid.farExtent;
      } else {
        vertex.position[0]=(float(i%(segments+1))/segments-.5f)*20;
        vertex.position[2]=(float(i/(segments+1))/segments-.5f)*20;
        vertex.metadata[0]=20.0f/segments;vertex.metadata[1]=10;
      }
      std::memcpy(vertices.data()+start+i*sizeof(Vertex),&vertex,sizeof(vertex));
    }
    indices.insert(indices.end(),topology.begin(),topology.end());
    MapMaterialRecord material{};
    std::fill(std::begin(material.textureIndices),std::end(material.textureIndices),InvalidMapTexture);
    material.baseColorFactor[0]=.04f;material.baseColorFactor[1]=.22f;material.baseColorFactor[2]=.28f;material.baseColorFactor[3]=1;
    material.roughness=.14f;material.specular=.5f;material.normalScale=1;
    material.flags=MapMaterialWater|MapMaterialNoCollision|WaterAuthoringResource|
        (mode?MapMaterialWaterCameraGrid:0);
    materials.push_back(material);draws.push_back(draw);
  }
  auto route=draws[firstWater];route.materialIndex=static_cast<u32>(materials.size());
  route.lodGroupId=static_cast<u32>(draws.size());
  auto material=materials[draws[firstWater].materialIndex];material.flags|=WaterRouteResource;
  materials.push_back(material);draws.push_back(route);
  MapDrawRecord box{};box.firstIndex=static_cast<u32>(indices.size());box.vertexOffset=static_cast<u32>(vertices.size()/vertexStride);
  box.indexCount=36;box.materialIndex=static_cast<u32>(materials.size());box.lodGroupId=static_cast<u32>(draws.size());
  box.model[0]=box.model[5]=box.model[10]=box.model[15]=1;box.boundsRadius=.8660254f;
  for(u32 face=0;face<6;++face) {
    const u32 axis=face/2,u=(axis+1)%3,v=(axis+2)%3;const float sign=face%2?1.0f:-1.0f;
    for(u32 corner=0;corner<4;++corner) {
      Vertex vertex{};vertex.position[axis]=sign*.5f;
      vertex.position[u]=(corner==1 || corner==2)?.5f:-.5f;vertex.position[v]=corner>=2?.5f:-.5f;
      vertex.normal[axis]=static_cast<i16>(sign*32767);vertex.tangent[u]=vertex.tangent[3]=32767;
      vertex.uv[0]=vertex.position[u]+.5f;vertex.uv[1]=vertex.position[v]+.5f;vertex.color=0xffffffffu;
      const usize offset=vertices.size();vertices.resize(offset+sizeof(Vertex));std::memcpy(vertices.data()+offset,&vertex,sizeof(Vertex));
    }
    const u32 a=face*4;
    if(sign>0) indices.insert(indices.end(),{a,a+1,a+2,a,a+2,a+3});
    else indices.insert(indices.end(),{a,a+2,a+1,a,a+3,a+2});
  }
  MapMaterialRecord boxMaterial{};std::fill(std::begin(boxMaterial.textureIndices),std::end(boxMaterial.textureIndices),InvalidMapTexture);
  boxMaterial.baseColorFactor[0]=.55f;boxMaterial.baseColorFactor[1]=.29f;boxMaterial.baseColorFactor[2]=.12f;boxMaterial.baseColorFactor[3]=1;
  boxMaterial.roughness=.65f;boxMaterial.normalScale=1;boxMaterial.flags=WaterAuthoringResource|BoxAuthoringResource;
  materials.push_back(boxMaterial);draws.push_back(box);
  return true;
}
}
