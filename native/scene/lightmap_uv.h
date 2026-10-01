#pragma once
#include "core/base.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace ae::scene {
enum class LightmapUvStatus { MissingGeometry, InvalidCoordinates, DegenerateTriangle, Overlap, AnalysisLimit, Valid };
inline const char *lightmapUvDiagnostic(LightmapUvStatus status) {
  switch(status) {
  case LightmapUvStatus::MissingGeometry:return "Malha indisponível";
  case LightmapUvStatus::InvalidCoordinates:return "UV1 não finita ou fora de 0..1";
  case LightmapUvStatus::DegenerateTriangle:return "UV1 ausente ou triângulo degenerado";
  case LightmapUvStatus::Overlap:return "UV1 sobreposta";
  case LightmapUvStatus::AnalysisLimit:return "UV1 excede orçamento de análise";
  case LightmapUvStatus::Valid:return "UV1 sem sobreposição de área";
  }
  return "UV1 inválida";
}
using LightmapUvTriangle=std::array<float,6>;
namespace lightmap_uv_detail {
struct Point {double x,y;};
inline double cross(Point a,Point b,Point c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
// Clip convex triangles in double precision. Shared edges/vertices have zero
// intersection area and are valid: adjacent faces inside a UV chart must touch.
inline bool overlaps(const LightmapUvTriangle &a,const LightmapUvTriangle &b) {
  std::array<Point,8> input{},output{};u32 count=3;
  for(u32 k=0;k<3;++k) input[k]={a[k*2],a[k*2+1]};
  const Point p0{b[0],b[1]},p1{b[2],b[3]},p2{b[4],b[5]};
  const double orientation=cross(p0,p1,p2)>0?1:-1;
  for(u32 edge=0;edge<3 && count>=3;++edge) {
    const Point start{b[edge*2],b[edge*2+1]},end{b[((edge+1)%3)*2],b[((edge+1)%3)*2+1]};
    u32 written=0;
    for(u32 k=0;k<count;++k) {
      const Point current=input[k],previous=input[(k+count-1)%count];
      const double dc=orientation*cross(start,end,current),dp=orientation*cross(start,end,previous);
      const bool inside=dc>=0,previousInside=dp>=0;
      if(inside!=previousInside) {
        const double t=dp/(dp-dc);
        output[written++]={previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t};
      }
      if(inside) output[written++]=current;
    }
    count=written;input=output;
  }
  if(count<3) return false;
  double area=0;for(u32 k=1;k+1<count;++k) area+=cross(input[0],input[k],input[k+1]);
  return std::abs(area)>1e-14;
}
}
// Import-time bounded analysis. No allocations, sorting or pair tests on the
// render hot path. Unknown is explicit and fails closed for lightmap sampling.
inline LightmapUvStatus auditLightmapUv(std::span<const LightmapUvTriangle> input) {
  constexpr usize MaximumTriangles=65536,MaximumPairs=1000000;
  if(input.empty()) return LightmapUvStatus::MissingGeometry;
  if(input.size()>MaximumTriangles) return LightmapUvStatus::AnalysisLimit;
  struct Triangle {LightmapUvTriangle uv;float lowX,highX,lowY,highY;};
  std::vector<Triangle> triangles;triangles.reserve(input.size());
  for(const auto &uv:input) {
    for(float c:uv) if(!std::isfinite(c)||c<0||c>1) return LightmapUvStatus::InvalidCoordinates;
    const double area=(double(uv[2])-uv[0])*(double(uv[5])-uv[1])-(double(uv[3])-uv[1])*(double(uv[4])-uv[0]);
    if(std::abs(area)<1e-12) return LightmapUvStatus::DegenerateTriangle;
    triangles.push_back({uv,std::min({uv[0],uv[2],uv[4]}),std::max({uv[0],uv[2],uv[4]}),
                            std::min({uv[1],uv[3],uv[5]}),std::max({uv[1],uv[3],uv[5]})});
  }
  std::sort(triangles.begin(),triangles.end(),[](const auto &a,const auto &b){return a.lowX<b.lowX;});
  usize pairs=0;
  for(usize i=0;i<triangles.size();++i) for(usize j=i+1;j<triangles.size() && triangles[j].lowX<triangles[i].highX;++j) {
    if(++pairs>MaximumPairs) return LightmapUvStatus::AnalysisLimit;
    if(triangles[i].lowY>=triangles[j].highY || triangles[j].lowY>=triangles[i].highY) continue;
    if(lightmap_uv_detail::overlaps(triangles[i].uv,triangles[j].uv)) return LightmapUvStatus::Overlap;
  }
  return LightmapUvStatus::Valid;
}
}
