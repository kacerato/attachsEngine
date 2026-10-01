#pragma once
#include "core/base.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <set>
namespace ae::resources {
using CurveVector3=std::array<float,3>;
struct CurvePoint3D {u64 id=0;CurveVector3 position{},in{},out{};float rollDegrees=0;bool operator==(const CurvePoint3D&)const=default;};
struct Curve3D {
 static constexpr usize MaximumPoints=128;
 std::vector<CurvePoint3D> points;bool closed=false;CurveVector3 up{0,1,0};
 bool valid()const {
  if(points.size()>MaximumPoints)return false;
  for(float n:up)if(!std::isfinite(n)||std::abs(n)>100000)return false;
  if(std::hypot(up[0],up[1],up[2])<1e-7)return false;
  std::set<u64>ids;for(const auto&p:points){
   if(!p.id||!ids.insert(p.id).second||!std::isfinite(p.rollDegrees)||std::abs(p.rollDegrees)>3600)return false;
   for(const auto*v:{&p.position,&p.in,&p.out})for(float n:*v)if(!std::isfinite(n)||std::abs(n)>100000)return false;
  }return true;
 }
};
class BakedCurve3D {
public:
 struct Sample {CurveVector3 position{},tangent{};double distance=0;CurveVector3 up{};float rollDegrees=0;};
 struct Frame {CurveVector3 position{},tangent{},up{},right{};float rollDegrees=0;};
 const std::vector<Sample>&samples()const{return samples_;}
 double length()const{return samples_.empty()?0:samples_.back().distance;}
 bool bake(const Curve3D&curve,double tolerance=.01){
  if(!curve.valid()||!std::isfinite(tolerance)||tolerance<=0)return false;
  std::vector<Sample> result;
  if(curve.points.empty()){samples_.clear();return true;}
  result.push_back({curve.points.front().position,{},0,{},curve.points.front().rollDegrees});
  const usize segments=curve.points.size()>1?curve.points.size()-1+(curve.closed?1:0):0;
  for(usize i=0;i<segments;++i){
   const auto&a=curve.points[i],&b=curve.points[(i+1)%curve.points.size()];CurveVector3 c1{},c2{};
   for(u32 k=0;k<3;++k){c1[k]=a.position[k]+a.out[k];c2[k]=b.position[k]+b.in[k];}
   const usize start=result.size()-1;const double begin=result.back().distance;
   result.back().rollDegrees=a.rollDegrees;
   if(!subdivide(result,a.position,c1,c2,b.position,tolerance,0))return false;
   const double span=result.back().distance-begin;
   for(usize n=start+1;n<result.size();++n){const double t=span>1e-12?(result[n].distance-begin)/span:0;result[n].rollDegrees=float(a.rollDegrees+(b.rollDegrees-a.rollDegrees)*t);}
  }
  for(usize i=0;i<result.size();++i){CurveVector3 direction{};const usize next=i+1<result.size()?i+1:i,prev=i?i-1:0;for(u32 k=0;k<3;++k)direction[k]=result[next].position[k]-result[prev].position[k];if(!normalize(direction)){for(u32 k=0;k<3;++k)direction[k]=result[next].position[k]-result[i].position[k];if(!normalize(direction)){for(u32 k=0;k<3;++k)direction[k]=result[i].position[k]-result[prev].position[k];normalize(direction);}}result[i].tangent=direction;}
  if(curve.closed&&result.size()>2){
   CurveVector3 seam{};for(u32 k=0;k<3;++k)seam[k]=curve.points.front().out[k]-curve.points.front().in[k];
   if(!normalize(seam)){for(u32 k=0;k<3;++k)seam[k]=result[1].position[k]-result[result.size()-2].position[k];normalize(seam);}
   if(norm(seam)>1e-12){result.front().tangent=seam;result.back().tangent=seam;}
  }
  if(result.back().distance>1e-12){
   if(!projectUp(curve.up,result.front().tangent,result.front().up))return false;
   for(usize i=1;i<result.size();++i){
    auto transported=transport(result[i-1].up,result[i-1].tangent,result[i].tangent);
    if(!projectUp(transported,result[i].tangent,result[i].up))return false;
   }
   // Distribute geometric holonomy, preserving the authored first frame and seam.
   if(curve.closed&&result.front().tangent==result.back().tangent){
    const auto&axis=result.front().tangent;const auto&first=result.front().up;const auto&last=result.back().up;
    const double correction=std::atan2(dot(cross(last,first),axis),std::clamp(dot(last,first),-1.,1.));
    for(auto&s:result)s.up=rotate(s.up,s.tangent,correction*s.distance/result.back().distance);
    result.back().up=result.front().up;result.back().rollDegrees=result.front().rollDegrees;
   }
  }
  samples_.swap(result);return true;
 }
 bool sampleDistance(double offset,CurveVector3&position,CurveVector3&tangent,bool wrap=false)const{
  usize hi=0;double t=0;if(!interval(offset,wrap,hi,t))return false;
  if(hi==0){position=samples_.front().position;tangent=samples_.front().tangent;return true;}
  const auto&a=samples_[hi-1],&b=samples_[hi];
  for(u32 k=0;k<3;++k)position[k]=float(a.position[k]+(b.position[k]-a.position[k])*t);
  if(t<=0)tangent=a.tangent;else if(t>=1)tangent=b.tangent;else{for(u32 k=0;k<3;++k)tangent[k]=b.position[k]-a.position[k];if(!normalize(tangent))tangent=b.tangent;}
  return true;
 }
 bool sampleFrame(double offset,Frame&out,bool wrap=false)const{
  usize hi=0;double t=0;if(!interval(offset,wrap,hi,t)||length()<=1e-12)return false;
  Frame frame;if(!sampleDistance(offset,frame.position,frame.tangent,wrap)||!normalize(frame.tangent))return false;
  const auto&a=samples_[hi?hi-1:0],&b=samples_[hi];CurveVector3 up{};
  for(u32 k=0;k<3;++k)up[k]=float(a.up[k]+(b.up[k]-a.up[k])*t);
  if(norm(up)<1e-7)up=transport(a.up,a.tangent,frame.tangent);
  if(!projectUp(up,frame.tangent,frame.up))return false;
  frame.rollDegrees=float(a.rollDegrees+(b.rollDegrees-a.rollDegrees)*t);
  frame.up=rotate(frame.up,frame.tangent,double(frame.rollDegrees)*3.14159265358979323846/180.);
  frame.right=cross(frame.up,frame.tangent);if(!normalize(frame.right))return false;
  frame.up=cross(frame.tangent,frame.right);normalize(frame.up);out=frame;return true;
 }
private:
 std::vector<Sample>samples_;
 static double dot(const CurveVector3&a,const CurveVector3&b){return double(a[0])*b[0]+double(a[1])*b[1]+double(a[2])*b[2];}
 static CurveVector3 cross(const CurveVector3&a,const CurveVector3&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
 static double norm(const CurveVector3&v){return std::hypot(double(v[0]),double(v[1]),double(v[2]));}
 static bool normalize(CurveVector3&v){const double n=norm(v);if(n<1e-12||!std::isfinite(n))return false;for(auto&x:v)x=float(x/n);return true;}
 static CurveVector3 rotate(const CurveVector3&v,const CurveVector3&axis,double angle){const double c=std::cos(angle),s=std::sin(angle),d=dot(v,axis);const auto cv=cross(axis,v);CurveVector3 r;for(u32 k=0;k<3;++k)r[k]=float(v[k]*c+cv[k]*s+axis[k]*d*(1-c));return r;}
 static bool projectUp(CurveVector3 hint,const CurveVector3&tangent,CurveVector3&up){
  if(norm(tangent)<1e-12||!normalize(hint))return false;
  double d=dot(hint,tangent);for(u32 k=0;k<3;++k)hint[k]-=float(d*tangent[k]);
  if(norm(hint)<1e-7){
   // Explicit frame convention for parallel hints: least-aligned positive axis.
   u32 axis=0;for(u32 k=1;k<3;++k)if(std::abs(tangent[k])<std::abs(tangent[axis]))axis=k;
   hint={};hint[axis]=1;d=dot(hint,tangent);for(u32 k=0;k<3;++k)hint[k]-=float(d*tangent[k]);
  }
  if(!normalize(hint))return false;
  up=hint;return true;
 }
 static CurveVector3 transport(const CurveVector3&up,const CurveVector3&from,const CurveVector3&to){
  auto axis=cross(from,to);const double sine=norm(axis),cosine=std::clamp(dot(from,to),-1.,1.);
  if(sine>1e-7){normalize(axis);return rotate(up,axis,std::atan2(sine,cosine));}
  // A 180 degree cusp has no unique minimal axis; preserve up, reverse right.
  return up;
 }
 bool interval(double offset,bool wrap,usize&hi,double&t)const{
  if(samples_.empty()||!std::isfinite(offset))return false;
  const double total=length();if(total<=1e-12){hi=0;t=0;return true;}
  if(wrap){offset=std::fmod(offset,total);if(offset<0)offset+=total;}else offset=std::clamp(offset,0.,total);
  if(offset<=0){hi=0;t=0;return true;}if(offset>=total){hi=samples_.size()-1;t=1;return true;}
  const auto at=std::lower_bound(samples_.begin(),samples_.end(),offset,[](const Sample&s,double d){return s.distance<d;});hi=usize(at-samples_.begin());
  const double span=samples_[hi].distance-samples_[hi-1].distance;t=span>1e-12?(offset-samples_[hi-1].distance)/span:0;return true;
 }
 static double distance(const CurveVector3&a,const CurveVector3&b){return std::hypot(double(a[0])-b[0],double(a[1])-b[1],double(a[2])-b[2]);}
 static CurveVector3 midpoint(const CurveVector3&a,const CurveVector3&b){CurveVector3 v{};for(u32 k=0;k<3;++k)v[k]=(a[k]+b[k])*.5f;return v;}
 static bool subdivide(std::vector<Sample>&out,const CurveVector3&a,const CurveVector3&b,const CurveVector3&c,const CurveVector3&d,double tolerance,u32 depth){
  const double polygon=distance(a,b)+distance(b,c)+distance(c,d),chord=distance(a,d);
  if(polygon-chord<=tolerance||depth==12){if(out.size()>=32768)return false;const double step=distance(out.back().position,d);if(step>1e-12)out.push_back({d,{},out.back().distance+step});return true;}
  const auto ab=midpoint(a,b),bc=midpoint(b,c),cd=midpoint(c,d),abc=midpoint(ab,bc),bcd=midpoint(bc,cd),m=midpoint(abc,bcd);
  return subdivide(out,a,ab,abc,m,tolerance,depth+1)&&subdivide(out,m,bcd,cd,d,tolerance,depth+1);
 }
};
}
