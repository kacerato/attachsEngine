#pragma once
#include "renderer/map_package.h"
#include "renderer/authoring_geometry.h"
#include "renderer/water_grid.h"
#include <algorithm>
#include <limits>

namespace ae::renderer {
// Runtime-only library resource marker. Never instantiated by package import.
inline constexpr u32 WaterAuthoringResource = 1u << 11;
inline constexpr u32 WaterRouteResource = 1u << 12;

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
  const auto boxMaterialIndex=materials.size();
  if(!appendBoxAuthoringGeometry(vertexStride,vertices,indices,draws,materials)) return false;
  auto &boxMaterial=materials[boxMaterialIndex];
  boxMaterial.flags|=WaterAuthoringResource;
  boxMaterial.baseColorFactor[1]=.29f;boxMaterial.baseColorFactor[2]=.12f;
  return true;
}
}
