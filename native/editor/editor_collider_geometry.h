#pragma once
#include "scene/collider.h"
#include "editor/editor_view.h"
#include <array>
namespace ae::editor {
// Analytic authored primitive surfaces, transformed by the same full affine
// pose as their outlines. These are editor hits, not simulated Jolt contacts.
inline bool intersectColliderPrimitive(const scene::Collider &c,const EditorRay &ray,
    const float pose[16],float &distance) {
  double o[3],v[3];
  if(!c.valid() || c.shape==scene::ColliderShape::Mesh || !ray.valid ||
     !std::isfinite(ray.minimumDistance) || !std::isfinite(ray.maximumDistance) ||
     ray.minimumDistance<0 || ray.maximumDistance<ray.minimumDistance ||
     !EditorPickMesh::localRay(ray.origin,ray.direction,pose,o,v))return false;
  double nearest=ray.maximumDistance;bool hit=false;
  const auto accept=[&](double t) {
    if(std::isfinite(t) && t>=ray.minimumDistance && t<=nearest){nearest=t;hit=true;}
  };
  if(c.shape==scene::ColliderShape::Box) {
    const double half[]{c.halfX,c.halfY,c.halfZ};
    double enter=-std::numeric_limits<double>::infinity(),leave=std::numeric_limits<double>::infinity();
    for(u32 axis=0;axis<3;++axis) {
      if(v[axis]==0) {if(o[axis]<-half[axis] || o[axis]>half[axis])return false;}
      else {double a=(-half[axis]-o[axis])/v[axis],b=(half[axis]-o[axis])/v[axis];
        if(a>b)std::swap(a,b);
        enter=std::max(enter,a);leave=std::min(leave,b);}
    }
    if(leave<enter)return false;
    accept(enter);accept(leave);
  } else {
    // Stable roots also cover a tangent. Callers constrain curved parts to
    // their exposed hemispheres/finite cylinder, avoiding internal seam hits.
    const auto roots=[](double a,double b,double d,const auto &visit) {
      if(a==0)return;
      const double discriminant=b*b-4*a*d;if(discriminant<0 || !std::isfinite(discriminant))return;
      const double q=-.5*(b+std::copysign(std::sqrt(discriminant),b));
      if(q==0)visit(-b/(2*a));else {visit(q/a);visit(d/q);}
    };
    const auto sphere=[&](double center,int hemisphere) {
      const double y=o[1]-center;
      roots(v[0]*v[0]+v[1]*v[1]+v[2]*v[2],2*(o[0]*v[0]+y*v[1]+o[2]*v[2]),
          o[0]*o[0]+y*y+o[2]*o[2]-double(c.radius)*c.radius,[&](double t) {
        const double at=o[1]+t*v[1];
        if(!hemisphere || (hemisphere>0?at>=center:at<=center))accept(t);
      });
    };
    if(c.shape==scene::ColliderShape::Sphere)sphere(0,0);
    else {
      roots(v[0]*v[0]+v[2]*v[2],2*(o[0]*v[0]+o[2]*v[2]),
          o[0]*o[0]+o[2]*o[2]-double(c.radius)*c.radius,[&](double t) {
        if(std::abs(o[1]+t*v[1])<=c.halfHeight)accept(t);
      });
      if(c.shape==scene::ColliderShape::Capsule){sphere(c.halfHeight,1);sphere(-c.halfHeight,-1);}
      else if(c.shape==scene::ColliderShape::Cylinder && v[1]!=0)for(double side:{-1.,1.}) {
        const double t=(side*c.halfHeight-o[1])/v[1],x=o[0]+t*v[0],z=o[2]+t*v[2];
        if(x*x+z*z<=double(c.radius)*c.radius)accept(t);
      }
    }
  }
  if(hit)distance=static_cast<float>(nearest);
  return hit;
}
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
