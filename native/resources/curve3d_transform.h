#pragma once
#include "resources/curve3d.h"
namespace ae::resources {
// Authoring geometry becomes a world-space bake input. Both runtime and editor
// use this before deriving frames; transforming an already rolled local frame
// would disagree under nonuniform scale.
inline Curve3D transformedCurve3D(const Curve3D&source,const std::array<float,16>&matrix){
 Curve3D result=source;
 for(u32 a=0;a<3;++a)result.up[a]=matrix[a]*source.up[0]+matrix[4+a]*source.up[1]+matrix[8+a]*source.up[2];
 for(auto&point:result.points){
  const auto position=point.position,in=point.in,out=point.out;
  for(u32 a=0;a<3;++a){
   point.position[a]=matrix[12+a]+matrix[a]*position[0]+matrix[4+a]*position[1]+matrix[8+a]*position[2];
   point.in[a]=matrix[a]*in[0]+matrix[4+a]*in[1]+matrix[8+a]*in[2];
   point.out[a]=matrix[a]*out[0]+matrix[4+a]*out[1]+matrix[8+a]*out[2];
  }
 }
 return result;
}
}
