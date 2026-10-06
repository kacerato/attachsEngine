#pragma once
#include "core/base.h"
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

namespace ae::editor {
// Immutable local-space triangles/BVH, shared by all authored instances.
class EditorPickMesh final {
public:
  using Triangle=std::array<float,9>;
  // Match the static import index budget (12 Mi indices / 3). The old 256 Ki
  // ceiling rejected ordinary vehicle assets after successful GPU publication.
  static constexpr usize MaximumTriangles=4u*1024u*1024u;
  std::span<const Triangle> triangles() const {return triangles_;}
  bool build(std::vector<Triangle> triangles) {
    if(triangles.empty() || triangles.size()>MaximumTriangles) return false;
    for(const auto &t:triangles) for(float v:t) if(!std::isfinite(v)) return false;
    EditorPickMesh next;next.triangles_=std::move(triangles);
    // Partition source ordinals, preserving element identity and input geometry order.
    next.order_.resize(next.triangles_.size());
    for(u32 n=0;n<next.order_.size();++n)next.order_[n]=n;
    // Leaves hold up to four triangles; fewer than N nodes suffice for N>=2.
    next.nodes_.reserve(next.triangles_.size());next.buildNode(0,static_cast<u32>(next.triangles_.size()));
    *this=std::move(next);return true;
  }
  // Shared affine ray conversion for authored shapes and triangle BVHs. Keep
  // direction unnormalized so depth remains in the original world-ray units.
  static bool localRay(const float origin[3],const float direction[3],const float model[16],double o[3],double v[3]) {
    for(u32 i=0;i<16;++i) if(!std::isfinite(model[i])) return false;
    // Affine inverse, including nonuniform scale and hierarchy-induced shear.
    const double a=model[0],b=model[4],c=model[8],d=model[1],e=model[5],f=model[9],g=model[2],h=model[6],i=model[10];
    const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(determinant) || determinant==0) return false;
    const double inv[9]{(e*i-f*h)/determinant,(c*h-b*i)/determinant,(b*f-c*e)/determinant,
      (f*g-d*i)/determinant,(a*i-c*g)/determinant,(c*d-a*f)/determinant,
      (d*h-e*g)/determinant,(b*g-a*h)/determinant,(a*e-b*d)/determinant};
    std::fill(o,o+3,0);std::fill(v,v+3,0);
    for(u32 r=0;r<3;++r) for(u32 k=0;k<3;++k) {
      o[r]+=inv[r*3+k]*(double(origin[k])-model[12+k]);v[r]+=inv[r*3+k]*direction[k];
    }
    // Do not normalize local direction: t must stay a world-space distance.
    for(u32 k=0;k<3;++k) if(!std::isfinite(o[k]) || !std::isfinite(v[k])) return false;
    return v[0]!=0 || v[1]!=0 || v[2]!=0;
  }
  bool intersect(const float origin[3],const float direction[3],const float model[16],float &distance,
      float minimumDistance=0,float maximumDistance=std::numeric_limits<float>::max(),u32 *sourceTriangle=nullptr) const {
    double o[3]{},v[3]{};
    if(nodes_.empty() || !std::isfinite(minimumDistance) || !std::isfinite(maximumDistance) ||
       minimumDistance<0 || maximumDistance<minimumDistance || !localRay(origin,direction,model,o,v))return false;
    double nearest=maximumDistance;bool hit=false;u32 nearestTriangle=0;
    std::array<u32,64> stack{};u32 count=1;
    while(count) {
      const auto &node=nodes_[stack[--count]];
      double enter=minimumDistance,leave=nearest;bool inside=true;
      for(u32 axis=0;axis<3;++axis) {
        if(v[axis]==0) { if(o[axis]<node.low[axis] || o[axis]>node.high[axis]) inside=false; }
        else {
          double x=(node.low[axis]-o[axis])/v[axis],y=(node.high[axis]-o[axis])/v[axis];
          if(x>y) std::swap(x,y);
          enter=std::max(enter,x);leave=std::min(leave,y);
        }
      }
      if(!inside || leave<enter) continue;
      if(!node.count) { stack[count++]=node.left;stack[count++]=node.right;continue; }
      for(u32 n=node.begin;n<node.begin+node.count;++n) {
        const auto &t=triangles_[order_[n]];double edge1[3],edge2[3],delta[3],cross[3],q[3];
        for(u32 k=0;k<3;++k) {edge1[k]=double(t[3+k])-t[k];edge2[k]=double(t[6+k])-t[k];delta[k]=o[k]-t[k];}
        cross[0]=v[1]*edge2[2]-v[2]*edge2[1];cross[1]=v[2]*edge2[0]-v[0]*edge2[2];cross[2]=v[0]*edge2[1]-v[1]*edge2[0];
        const double det=edge1[0]*cross[0]+edge1[1]*cross[1]+edge1[2]*cross[2];
        if(det==0) continue;
        const double u=(delta[0]*cross[0]+delta[1]*cross[1]+delta[2]*cross[2])/det;
        if(u<0 || u>1) continue;
        q[0]=delta[1]*edge1[2]-delta[2]*edge1[1];q[1]=delta[2]*edge1[0]-delta[0]*edge1[2];q[2]=delta[0]*edge1[1]-delta[1]*edge1[0];
        const double w=(v[0]*q[0]+v[1]*q[1]+v[2]*q[2])/det;
        if(w<0 || u+w>1) continue;
        const double depth=(edge2[0]*q[0]+edge2[1]*q[1]+edge2[2]*q[2])/det;
        if(depth>=minimumDistance && depth<=nearest) {nearest=depth;hit=true;nearestTriangle=order_[n];}
      }
    }
    if(!hit || nearest>std::numeric_limits<float>::max()) return false;
    distance=static_cast<float>(nearest);if(sourceTriangle)*sourceTriangle=nearestTriangle;return true;
  }
private:
  struct Node {float low[3],high[3];u32 begin=0,count=0,left=0,right=0;};
  u32 buildNode(u32 begin,u32 count) {
    const u32 index=static_cast<u32>(nodes_.size());Node node{};
    for(u32 k=0;k<3;++k) {node.low[k]=std::numeric_limits<float>::infinity();node.high[k]=-node.low[k];}
    for(u32 n=begin;n<begin+count;++n) for(u32 p=0;p<3;++p) for(u32 k=0;k<3;++k) {
      node.low[k]=std::min(node.low[k],triangles_[order_[n]][p*3+k]);node.high[k]=std::max(node.high[k],triangles_[order_[n]][p*3+k]);
    }
    nodes_.push_back(node);
    if(count<=4) {nodes_[index].begin=begin;nodes_[index].count=count;return index;}
    u32 axis=0;for(u32 k=1;k<3;++k) if(node.high[k]-node.low[k]>node.high[axis]-node.low[axis]) axis=k;
    const u32 middle=begin+count/2;
    std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+begin+count,[&](u32 x,u32 y){
      const auto &a=triangles_[x],&b=triangles_[y];
      return double(a[axis])+a[3+axis]+a[6+axis]<double(b[axis])+b[3+axis]+b[6+axis];
    });
    const auto left=buildNode(begin,count/2),right=buildNode(middle,count-count/2);
    nodes_[index].left=left;nodes_[index].right=right;return index;
  }
  std::vector<Triangle> triangles_;
  std::vector<u32> order_;
  std::vector<Node> nodes_;
};
}
