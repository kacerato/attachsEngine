#pragma once
#include "renderer/authoring_geometry.h"
#include "scene/primitive.h"
#include <cmath>
#include <numbers>

namespace ae::renderer {
// Library roles, never shading modes. Cube keeps its old resource bit and slot.
inline constexpr u32 PrimitiveAuthoringMask=7u<<16;
inline scene::PrimitiveType primitiveFromFlags(u32 flags) {
  const auto type=(flags&PrimitiveAuthoringMask)>>16;
  if(type>0 && type<6) return static_cast<scene::PrimitiveType>(type);
  return flags&BoxAuthoringResource?scene::PrimitiveType::Cube:scene::PrimitiveType::Count;
}
inline bool appendPrimitiveGeometry(scene::PrimitiveType type,u32 stride,std::vector<u8> &vertices,
    std::vector<u32> &indices,std::vector<MapDrawRecord> &draws,std::vector<MapMaterialRecord> &materials) {
  if(!scene::validPrimitive(type) || stride!=MapVertexStride || vertices.size()%stride) return false;
  if(type==scene::PrimitiveType::Cube) return appendBoxAuthoringGeometry(stride,vertices,indices,draws,materials);
  struct Vertex {float position[3];i16 normal[4];i16 tangent[4];float uv[2];float metadata[2];u32 color;};
  static_assert(sizeof(Vertex)==MapVertexStride);
  std::vector<Vertex> points;std::vector<u32> triangles;
  points.reserve(640);triangles.reserve(3200);
  const auto add=[&](float x,float y,float z,float nx,float ny,float nz,float tx,float ty,float tz,float sign,float u,float v) {
    const auto pack=[](float n) {return static_cast<i16>(std::lround(std::clamp(n,-1.f,1.f)*32767));};
    points.push_back({{x,y,z},{pack(nx),pack(ny),pack(nz),0},{pack(tx),pack(ty),pack(tz),pack(sign)},
                      {u,v},{0,0},0xffffffffu});
  };
  constexpr u32 segments=32;constexpr float pi=std::numbers::pi_v<float>;
  if(type==scene::PrimitiveType::Sphere || type==scene::PrimitiveType::Capsule) {
    const bool capsule=type==scene::PrimitiveType::Capsule;const u32 rows=capsule?18:17;
    for(u32 row=0;row<rows;++row) {
      const float angle=capsule?(row<9?row*pi/16:pi/2+(row-9)*pi/16):row*pi/16;
      const float radial=(row==0 || row+1==rows)?0:std::sin(angle);
      const float ny=std::cos(angle),y=.5f*ny+(capsule?(row<9?.5f:-.5f):0);
      for(u32 col=0;col<=segments;++col) {
        const float angleU=(col==segments?0:col)*2*pi/segments;
        const float x=std::cos(angleU),z=std::sin(angleU),u=static_cast<float>(col)/segments;
        add(.5f*radial*x,y,.5f*radial*z,radial*x,ny,radial*z,-z,0,x,1,u,capsule?(1-y)/2:static_cast<float>(row)/16);
      }
    }
    for(u32 row=0;row+1<rows;++row) for(u32 col=0;col<segments;++col) {
      const u32 a=row*(segments+1)+col,b=a+segments+1;
      if(row) triangles.insert(triangles.end(),{a,a+1,b});
      if(row+2<rows) triangles.insert(triangles.end(),{a+1,b+1,b});
    }
  } else if(type==scene::PrimitiveType::Cylinder) {
    for(u32 row=0;row<2;++row) for(u32 col=0;col<=segments;++col) {
      const float a=(col==segments?0:col)*2*pi/segments,x=std::cos(a),z=std::sin(a);
      add(.5f*x,row?-1.f:1.f,.5f*z,x,0,z,-z,0,x,1,static_cast<float>(col)/segments,static_cast<float>(row));
    }
    for(u32 col=0;col<segments;++col) {const u32 b=col+segments+1;triangles.insert(triangles.end(),{col,col+1,b,col+1,b+1,b});}
    for(u32 cap=0;cap<2;++cap) {
      const float sign=cap?-1.f:1.f;const u32 center=static_cast<u32>(points.size());
      add(0,sign,0,0,sign,0,1,0,0,-sign,.5f,.5f);
      for(u32 col=0;col<segments;++col) {
        const float a=col*2*pi/segments,x=.5f*std::cos(a),z=.5f*std::sin(a);
        add(x,sign,z,0,sign,0,1,0,0,-sign,x+.5f,z+.5f);
      }
      for(u32 col=0;col<segments;++col) {
        const u32 a=center+1+col,b=center+1+(col+1)%segments;
        if(cap) triangles.insert(triangles.end(),{center,a,b});else triangles.insert(triangles.end(),{center,b,a});
      }
    }
  } else {
    const bool plane=type==scene::PrimitiveType::Plane;const u32 divisions=plane?10:1;const float size=plane?10.f:1.f;
    for(u32 row=0;row<=divisions;++row) for(u32 col=0;col<=divisions;++col) {
      const float u=static_cast<float>(col)/divisions,v=static_cast<float>(row)/divisions;
      add((u-.5f)*size,plane?0:(v-.5f)*size,plane?(v-.5f)*size:0,0,plane?1:0,plane?0:1,1,0,0,plane?-1:1,u,v);
    }
    for(u32 row=0;row<divisions;++row) for(u32 col=0;col<divisions;++col) {
      const u32 a=row*(divisions+1)+col,b=a+divisions+1;
      if(plane) triangles.insert(triangles.end(),{a,b,a+1,a+1,b,b+1});
      else triangles.insert(triangles.end(),{a,a+1,b,a+1,b+1,b});
    }
  }
  if(vertices.size()/stride>std::numeric_limits<u32>::max()-points.size() || indices.size()>std::numeric_limits<u32>::max()-triangles.size()) return false;
  MapDrawRecord draw{};draw.firstIndex=static_cast<u32>(indices.size());draw.vertexOffset=static_cast<u32>(vertices.size()/stride);
  draw.indexCount=static_cast<u32>(triangles.size());draw.materialIndex=static_cast<u32>(materials.size());draw.lodGroupId=static_cast<u32>(draws.size());
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;
  for(const auto &point:points) {const auto *p=point.position;draw.boundsRadius=std::max(draw.boundsRadius,std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]));}
  const auto offset=vertices.size();vertices.resize(offset+points.size()*stride);std::memcpy(vertices.data()+offset,points.data(),points.size()*stride);
  indices.insert(indices.end(),triangles.begin(),triangles.end());
  MapMaterialRecord material{};std::fill(std::begin(material.textureIndices),std::end(material.textureIndices),InvalidMapTexture);
  material.baseColorFactor[0]=material.baseColorFactor[1]=material.baseColorFactor[2]=.55f;material.baseColorFactor[3]=1;
  material.roughness=.65f;material.normalScale=1;
  material.flags=(static_cast<u32>(type)<<16)|MapMaterialCullBackFaces;
  materials.push_back(material);draws.push_back(draw);return true;
}
inline bool appendPrimitiveLibrary(u32 stride,std::vector<u8> &vertices,std::vector<u32> &indices,
    std::vector<MapDrawRecord> &draws,std::vector<MapMaterialRecord> &materials) {
  const auto v=vertices.size(),i=indices.size(),d=draws.size(),m=materials.size();
  for(u32 type=0;type<6;++type) if(!appendPrimitiveGeometry(static_cast<scene::PrimitiveType>(type),stride,vertices,indices,draws,materials)) {
    vertices.resize(v);indices.resize(i);draws.resize(d);materials.resize(m);return false;
  }
  return true;
}
}
