#pragma once
#include "core/base.h"
#include <cmath>
#include <vector>

namespace ae::renderer {
struct MeshVertex {
  float position[3];
  float normal[3];
  float tangent[4];
  float uv[2];
};
static_assert(sizeof(MeshVertex) == 48);
struct MeshData { std::vector<MeshVertex> vertices; std::vector<u32> indices; };

// Seam duplication preserves UV continuity; pole triangles omit zero-area faces.
inline bool makeUvSphere(u32 segments, u32 rings, MeshData &result) {
  if (segments < 3 || rings < 2 || segments > 1024 || rings > 512) return false;
  MeshData mesh;
  mesh.vertices.reserve(static_cast<usize>(segments+1)*(rings+1));
  mesh.indices.reserve(static_cast<usize>(segments)*(rings-1)*6);
  constexpr float pi = 3.14159265358979323846f;
  for (u32 y=0; y<=rings; ++y) {
    const float v=static_cast<float>(y)/rings, theta=v*pi;
    for (u32 x=0; x<=segments; ++x) {
      const float u=static_cast<float>(x)/segments, phi=(x==segments?0.0f:u*2*pi);
      const float s=(y==0 || y==rings)?0.0f:std::sin(theta);
      const float px=s*std::cos(phi), py=std::cos(theta), pz=s*std::sin(phi);
      mesh.vertices.push_back({{px,py,pz},{px,py,pz},{-std::sin(phi),0,std::cos(phi),1},{u,v}});
    }
  }
  for (u32 y=0; y<rings; ++y) for (u32 x=0; x<segments; ++x) {
    const u32 a=y*(segments+1)+x, b=a+segments+1;
    if (y!=0) mesh.indices.insert(mesh.indices.end(),{a,a+1,b});
    if (y+1!=rings) mesh.indices.insert(mesh.indices.end(),{a+1,b+1,b});
  }
  result=std::move(mesh);
  return true;
}
} // namespace ae::renderer
