#pragma once
#include "renderer/water_surface.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::renderer {
inline constexpr u32 MaximumWaterRoutePoints=16;
struct WaterRoutePoint {
  float position[3]{};
  float width=6,depth=3,speed=1,foam=1,tension=0;
};
struct WaterRoute {
  std::array<WaterRoutePoint,MaximumWaterRoutePoints> points{};
  u32 count=0;
};
struct WaterRouteSample {
  WaterVec3 center{},tangent{0,0,1};
  float width=0,depth=0,speed=0,foam=0,distance=0;
};
inline bool validateWaterRoute(const WaterRoute &route) {
  if(route.count<2 || route.count>MaximumWaterRoutePoints) return false;
  for(u32 i=0;i<route.count;++i) {
    const auto &p=route.points[i];
    for(float v:p.position) if(!std::isfinite(v) || std::abs(v)>1e7f) return false;
    if(!std::isfinite(p.width) || p.width<.1f || p.width>10000 ||
       !std::isfinite(p.depth) || p.depth<.1f || p.depth>10000 ||
       !std::isfinite(p.speed) || std::abs(p.speed)>100 ||
       !std::isfinite(p.foam) || p.foam<0 || p.foam>10 ||
       !std::isfinite(p.tension) || p.tension<0 || p.tension>1) return false;
    if(i && std::hypot(p.position[0]-route.points[i-1].position[0],p.position[2]-route.points[i-1].position[2])<.01f) return false;
  }
  return true;
}
inline WaterRouteSample evaluateWaterRoute(const WaterRoute &r,u32 segment,float t) {
  const auto &a=r.points[segment],&b=r.points[segment+1];
  const auto &before=r.points[segment?segment-1:0],&after=r.points[std::min(segment+2,r.count-1)];
  WaterRouteSample out;
  float point[3],derivative[3];
  const float t2=t*t,t3=t2*t;
  for(u32 axis=0;axis<3;++axis) {
    const float ma=(b.position[axis]-before.position[axis])*.5f*(1-a.tension);
    const float mb=(after.position[axis]-a.position[axis])*.5f*(1-b.tension);
    point[axis]=(2*t3-3*t2+1)*a.position[axis]+(t3-2*t2+t)*ma+(-2*t3+3*t2)*b.position[axis]+(t3-t2)*mb;
    derivative[axis]=(6*t2-6*t)*a.position[axis]+(3*t2-4*t+1)*ma+(-6*t2+6*t)*b.position[axis]+(3*t2-2*t)*mb;
  }
  const float length=std::hypot(derivative[0],derivative[2]);
  if(length<1e-6f) {derivative[0]=b.position[0]-a.position[0];derivative[1]=b.position[1]-a.position[1];derivative[2]=b.position[2]-a.position[2];}
  const float norm=std::max(1e-6f,std::hypot(derivative[0],derivative[2]));
  out.center={point[0],point[1],point[2]};out.tangent={derivative[0]/norm,derivative[1]/norm,derivative[2]/norm};
  out.width=a.width+(b.width-a.width)*t;out.depth=a.depth+(b.depth-a.depth)*t;
  out.speed=a.speed+(b.speed-a.speed)*t;out.foam=a.foam+(b.foam-a.foam)*t;
  return out;
}
// Both geometry and queries use this same piecewise approximation of the curve.
inline constexpr u32 WaterRouteSteps=16;
inline bool sampleWaterRoute(const WaterRoute &route,WaterVec2 position,WaterRouteSample &out) {
  if(!validateWaterRoute(route)) return false;
  float best=1e30f;WaterRouteSample selected;u32 selectedSegment=0;float selectedT=0;
  for(u32 segment=0;segment+1<route.count;++segment) {
    auto a=evaluateWaterRoute(route,segment,0);
    for(u32 step=1;step<=WaterRouteSteps;++step) {
      const auto b=evaluateWaterRoute(route,segment,float(step)/WaterRouteSteps);
      const float dx=b.center.x-a.center.x,dz=b.center.z-a.center.z,length=dx*dx+dz*dz;
      const float t=length>1e-12f?std::clamp(((position.x-a.center.x)*dx+(position.y-a.center.z)*dz)/length,0.0f,1.0f):0;
      const float distance=std::hypot(position.x-a.center.x-t*dx,position.y-a.center.z-t*dz);
      if(distance<best) {best=distance;selectedSegment=segment;selectedT=(step-1+t)/WaterRouteSteps;selected=evaluateWaterRoute(route,segment,selectedT);}
      a=b;
    }
  }
  selected.distance=best;out=selected;
  const float along=(position.x-selected.center.x)*selected.tangent.x+(position.y-selected.center.z)*selected.tangent.z;
  if((selectedSegment==0 && selectedT==0 && along<0) ||
     (selectedSegment+2==route.count && selectedT==1 && along>0)) return false;
  return best<=selected.width*.5f;
}
struct WaterRouteVertex {float position[3],normal[3],uv[2],flow[2],foam,spacing;};
inline bool buildWaterRouteMesh(const WaterRoute &route,std::vector<WaterRouteVertex> &vertices,std::vector<u32> &indices) {
  if(!validateWaterRoute(route)) return false;
  std::vector<WaterRouteVertex> result;std::vector<u32> topology;
  constexpr u32 across=8;
  const u32 sections=(route.count-1)*WaterRouteSteps;
  result.reserve((sections+1)*(across+1));topology.reserve(sections*across*6);
  for(u32 row=0;row<=sections;++row) {
    const u32 segment=std::min(row/WaterRouteSteps,route.count-2);
    const auto sample=evaluateWaterRoute(route,segment,float(row-segment*WaterRouteSteps)/WaterRouteSteps);
    const float normalLength=std::sqrt(1+sample.tangent.y*sample.tangent.y);
    for(u32 column=0;column<=across;++column) {
      const float side=(float(column)/across-.5f)*sample.width;
      WaterRouteVertex vertex{};
      vertex.position[0]=sample.center.x+sample.tangent.z*side;vertex.position[1]=sample.center.y;
      vertex.position[2]=sample.center.z-sample.tangent.x*side;
      vertex.normal[0]=-sample.tangent.x*sample.tangent.y/normalLength;vertex.normal[1]=1/normalLength;
      vertex.normal[2]=-sample.tangent.z*sample.tangent.y/normalLength;
      // The second coordinate carries authored depth; flow uses world metres.
      vertex.uv[0]=float(column)/across;vertex.uv[1]=sample.depth;
      vertex.flow[0]=sample.tangent.x*sample.speed;vertex.flow[1]=sample.tangent.z*sample.speed;
      vertex.foam=sample.foam;vertex.spacing=std::max(sample.width/across,1.0f);
      result.push_back(vertex);
    }
    if(row<sections) for(u32 column=0;column<across;++column) {
      const u32 a=row*(across+1)+column,b=a+1,c=a+across+1,d=c+1;
      topology.insert(topology.end(),{a,c,b,b,c,d});
    }
  }
  vertices=std::move(result);indices=std::move(topology);return true;
}
}
