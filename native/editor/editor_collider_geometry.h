#pragma once
#include "scene/collider.h"
#include <array>
namespace ae::editor {
template<class Segment> void editorColliderSegments(const scene::Collider &c,Segment segment) {
  using Point=std::array<float,3>;constexpr float pi=3.14159265359f;
  // Malha: a forma É a geometria desenhada do objeto; repetir o contorno em
  // linhas não acrescenta nada e custaria um segmento por aresta.
  if(c.shape==scene::ColliderShape::Mesh) return;
  if(c.shape==scene::ColliderShape::Box) {
    const float half[]{c.halfX,c.halfY,c.halfZ};
    for(u32 corner=0;corner<8;++corner) for(u32 axis=0;axis<3;++axis) if(!(corner&(1u<<axis))) {
      Point a{},b{};for(u32 k=0;k<3;++k) a[k]=b[k]=(corner&(1u<<k))?half[k]:-half[k];b[axis]=half[axis];segment(a,b);
    }
  } else if(c.shape==scene::ColliderShape::Sphere) {
    for(u32 axis=0;axis<3;++axis) for(u32 i=0;i<32;++i) {
      Point a{},b{};const u32 x=(axis+1)%3,y=(axis+2)%3;
      a[x]=c.radius*std::cos(i*pi/16);a[y]=c.radius*std::sin(i*pi/16);
      b[x]=c.radius*std::cos((i+1)*pi/16);b[y]=c.radius*std::sin((i+1)*pi/16);segment(a,b);
    }
  } else {
    for(float sign:{-1.0f,1.0f}) for(u32 i=0;i<32;++i) {
      Point a{c.radius*std::cos(i*pi/16),sign*c.halfHeight,c.radius*std::sin(i*pi/16)};
      Point b{c.radius*std::cos((i+1)*pi/16),sign*c.halfHeight,c.radius*std::sin((i+1)*pi/16)};segment(a,b);
    }
    for(u32 axis:{0u,2u}) {
      for(float sign:{-1.0f,1.0f}) {
        if(c.shape==scene::ColliderShape::Capsule) for(u32 i=0;i<16;++i) {
          Point a{},b{};a[axis]=c.radius*std::cos(i*pi/16);b[axis]=c.radius*std::cos((i+1)*pi/16);
          a[1]=sign*(c.halfHeight+c.radius*std::sin(i*pi/16));b[1]=sign*(c.halfHeight+c.radius*std::sin((i+1)*pi/16));segment(a,b);
        }
        Point a{},b{};a[axis]=b[axis]=sign*c.radius;a[1]=-c.halfHeight;b[1]=c.halfHeight;segment(a,b);
      }
    }
  }
}
}
