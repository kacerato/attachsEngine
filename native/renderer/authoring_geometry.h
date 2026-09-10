#pragma once
#include "renderer/map_package.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace ae::renderer {
inline constexpr u32 BoxAuthoringResource = 1u << 13;
// Stable built-in unit cube: resource only, never an authored scene object.
inline bool appendBoxAuthoringGeometry(u32 vertexStride,std::vector<u8> &vertices,
    std::vector<u32> &indices,std::vector<MapDrawRecord> &draws,
    std::vector<MapMaterialRecord> &materials) {
  if(vertexStride!=MapVertexStride || vertices.size()%vertexStride ||
     vertices.size()/vertexStride>std::numeric_limits<u32>::max()-24 ||
     indices.size()>std::numeric_limits<u32>::max()-36) return false;
  struct Vertex {float position[3];i16 normal[4];i16 tangent[4];float uv[2];float metadata[2];u32 color;};
  static_assert(sizeof(Vertex)==MapVertexStride);
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
  boxMaterial.baseColorFactor[0]=.55f;boxMaterial.baseColorFactor[1]=.55f;boxMaterial.baseColorFactor[2]=.55f;boxMaterial.baseColorFactor[3]=1;
  boxMaterial.roughness=.65f;boxMaterial.normalScale=1;boxMaterial.flags=BoxAuthoringResource;
  materials.push_back(boxMaterial);draws.push_back(box);
  return true;
}
}
