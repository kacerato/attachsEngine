#pragma once
#include "editor/editor_terrain_fx_controls.h"
#include <array>
#include <cmath>
#include <vector>
namespace ae::editor {
// The footprint uses the same local metres, aspect and rotation as rastering.
// Height is supplied by the matching retained terrain field, never by PBR normals.
template<class Height> std::vector<std::array<float,6>> terrainFxCursorSegments(
    const TerrainFxUiState &ui,const resources::TerrainFxAsset &fx,const float *world,Height height) {
  std::vector<std::array<float,6>> lines;
  if(ui.mode!=1 || !ui.cursorVisible || !fx.enabled)return lines;
  const double angle=ui.brush.rotationDegrees*3.141592653589793/180,c=std::cos(angle),sn=std::sin(angle);
  const double x=fx.domain.originX+ui.cursorU*fx.domain.sizeX,z=fx.domain.originZ+ui.cursorV*fx.domain.sizeZ;
  const auto point=[&](double u,double v){
    const double dx=u*ui.brush.radiusMeters*ui.brush.aspect,dz=v*ui.brush.radiusMeters;
    const float px=float(x+c*dx-sn*dz),pz=float(z+sn*dx+c*dz),py=height(px,pz)+.05f;
    std::array<float,3> result{};for(u32 k=0;k<3;++k)result[k]=world[12+k]+world[k]*px+world[4+k]*py+world[8+k]*pz;
    return result;
  };
  const auto segment=[&](auto a,auto b){lines.push_back({a[0],a[1],a[2],b[0],b[1],b[2]});};
  if(ui.brush.shape==resources::TerrainFxBrushShape::Circle){for(u32 i=0;i<48;++i){const double a=i*6.283185307179586/48,b=(i+1)*6.283185307179586/48;segment(point(std::cos(a),std::sin(a)),point(std::cos(b),std::sin(b)));}}
  else {
    const double corners[5][2]{{-1,-1},{1,-1},{1,1},{-1,1},{-1,-1}};
    for(u32 edge=0;edge<4;++edge)for(u32 i=0;i<16;++i){
      const auto at=[&](double t){return point(corners[edge][0]+(corners[edge+1][0]-corners[edge][0])*t,corners[edge][1]+(corners[edge+1][1]-corners[edge][1])*t);};
      segment(at(double(i)/16),at(double(i+1)/16));
    }
  }
  return lines;
}
}
