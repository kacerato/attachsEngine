#include "resources/animation_curve.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ae::resources {
namespace {
using Mode=AnimationTangentMode;
constexpr double MaximumFloat=std::numeric_limits<float>::max();
bool finiteFloat(double x) {return std::isfinite(x)&&std::abs(x)<=MaximumFloat;}
bool validMode(Mode m) {return static_cast<u8>(m)<=static_cast<u8>(Mode::NextConstant);}
Mode reversed(Mode m) {return m==Mode::Constant?Mode::NextConstant:m==Mode::NextConstant?Mode::Constant:m;}
double lerp(double a,double b,double u) {return a+(b-a)*u;}
struct Point {double x,y;};
Point lerp(Point a,Point b,double u) {return {lerp(a.x,b.x,u),lerp(a.y,b.y,u)};}
struct Segment {Point p[4];};
Segment segment(const AnimationCurve &curve,usize i) {
  const auto &a=curve.keys[i],&b=curve.keys[i+1];
  const double dt=double(b.time)-a.time;
  const double wa=a.weightedOut?a.outWeight:1.0/3,wb=b.weightedIn?b.inWeight:1.0/3;
  return {{{a.time,a.value},{a.time+wa*dt,a.value+wa*dt*animationCurveSlope(curve,i,false)},
           {b.time-wb*dt,b.value-wb*dt*animationCurveSlope(curve,i+1,true)},{b.time,b.value}}};
}
Point point(const Segment &s,double u) {
  const auto a=lerp(s.p[0],s.p[1],u),b=lerp(s.p[1],s.p[2],u),c=lerp(s.p[2],s.p[3],u);
  return lerp(lerp(a,b,u),lerp(b,c,u),u);
}
double parameter(const Segment &s,double time) {
  double lo=0,hi=1;
  // Monotonic Bezier time. Bisection remains stable at vertical/zero handles
  // and at the stationary midpoint of weights (1,1), unlike Newton alone.
  for(u32 i=0;i<48;++i) {const double u=(lo+hi)/2;if(point(s,u).x<time)lo=u;else hi=u;}
  return (lo+hi)/2;
}
bool isStep(const AnimationCurveKey &a,const AnimationCurveKey &b,bool &next) {
  const bool left=a.outgoing==Mode::Constant||b.incoming==Mode::Constant;
  next=a.outgoing==Mode::NextConstant||b.incoming==Mode::NextConstant;
  return left||next;
}
bool freezeTangents(AnimationCurve &curve) {
  auto next=curve;
  for(usize i=0;i<curve.keys.size();++i) {
    auto &k=next.keys[i];
    const double in=animationCurveSlope(curve,i,true),out=animationCurveSlope(curve,i,false);
    if(!finiteFloat(in)||!finiteFloat(out)) return false;
    k.inSlope=static_cast<float>(in);k.outSlope=static_cast<float>(out);k.broken=true;
    if(k.incoming!=Mode::Constant&&k.incoming!=Mode::NextConstant)k.incoming=Mode::Free;
    if(k.outgoing!=Mode::Constant&&k.outgoing!=Mode::NextConstant)k.outgoing=Mode::Free;
  }
  curve=std::move(next);return true;
}
}
bool validAnimationCurve(const AnimationCurve &curve) {
  if(curve.keys.empty()||curve.keys.size()>AnimationCurve::MaximumKeys)return false;
  // IDs are checked at resource boundaries, not during sampling.
  for(usize i=0;i<curve.keys.size();++i) {
    const auto &k=curve.keys[i];
    if(!k.id||!std::isfinite(k.time)||k.time<0||!std::isfinite(k.value)||!std::isfinite(k.inSlope)||
       !std::isfinite(k.outSlope)||!std::isfinite(k.inWeight)||!std::isfinite(k.outWeight)||
       k.inWeight<0||k.inWeight>1||k.outWeight<0||k.outWeight>1||!validMode(k.incoming)||!validMode(k.outgoing)||
       (i&&k.time<=curve.keys[i-1].time))return false;
    if(!k.broken&&(k.incoming!=k.outgoing||
       (k.incoming==Mode::Free&&k.inSlope!=k.outSlope)))return false;
    if(i) {bool next=false;if(isStep(curve.keys[i-1],k,next)&&next&&
        (curve.keys[i-1].outgoing==Mode::Constant||k.incoming==Mode::Constant))return false;}
  }
  return true;
}
double animationCurveSlope(const AnimationCurve &curve,usize i,bool incoming) {
  const auto &k=curve.keys[i];const auto mode=incoming?k.incoming:k.outgoing;
  if(mode==Mode::Free)return incoming?k.inSlope:k.outSlope;
  if(mode==Mode::Flat||mode==Mode::Constant||mode==Mode::NextConstant)return 0;
  const bool previous=i>0,next=i+1<curve.keys.size();
  const double h0=previous?double(k.time)-curve.keys[i-1].time:0,h1=next?double(curve.keys[i+1].time)-k.time:0;
  const double d0=previous?(double(k.value)-curve.keys[i-1].value)/h0:0,
               d1=next?(double(curve.keys[i+1].value)-k.value)/h1:0;
  if(mode==Mode::Linear)return incoming?(previous?d0:d1):(next?d1:d0);
  if(!previous)return d1;
  if(!next)return d0;
  if(mode==Mode::Auto)return (h0*d0+h1*d1)/(h0+h1);
  if(d0*d1<=0)return 0;
  // Monotone piecewise cubic slope: weighted harmonic mean of the secants.
  // Opposite secants flatten extrema, avoiding ClampedAuto overshoot.
  const double w0=2*h1+h0,w1=h1+2*h0;
  return (w0+w1)/(w0/d0+w1/d1);
}
bool sampleValidatedAnimationCurve(const AnimationCurve &curve,double time,AnimationCurveSample &out) {
  if(!std::isfinite(time)||curve.keys.empty())return false;
  const auto &keys=curve.keys;
  if(keys.size()==1||time<=keys.front().time) {out={keys.front().value,0};return true;}
  if(time>=keys.back().time) {out={keys.back().value,0};return true;}
  const auto upper=std::upper_bound(keys.begin(),keys.end(),time,[](double t,const auto &k){return t<k.time;});
  const usize i=static_cast<usize>(upper-keys.begin())-1;const auto &a=keys[i],&b=keys[i+1];
  if(time==a.time) {out={a.value,animationCurveSlope(curve,i,false)};return finiteFloat(out.derivative);}
  bool next=false;if(isStep(a,b,next)) {out={next?b.value:a.value,0};return true;}
  const auto s=segment(curve,i);
  const double u=!a.weightedOut&&!b.weightedIn?(time-a.time)/(double(b.time)-a.time):parameter(s,time),v=1-u;
  const auto p=point(s,u);
  const double dx=3*(v*v*(s.p[1].x-s.p[0].x)+2*v*u*(s.p[2].x-s.p[1].x)+u*u*(s.p[3].x-s.p[2].x));
  const double dy=3*(v*v*(s.p[1].y-s.p[0].y)+2*v*u*(s.p[2].y-s.p[1].y)+u*u*(s.p[3].y-s.p[2].y));
  // Value remains well-defined for a vertical tangent. Consumers needing a
  // finite derivative (curve splitting/baking) must reject that singularity.
  const double derivative=dx>1e-30?dy/dx:(std::abs(dy)<1e-30?0:std::copysign(std::numeric_limits<double>::infinity(),dy));
  if(!finiteFloat(p.y))return false;
  out={p.y,derivative};return true;
}
bool sampleAnimationCurve(const AnimationCurve &curve,double time,AnimationCurveSample &out) {
  return validAnimationCurve(curve)&&sampleValidatedAnimationCurve(curve,time,out);
}
bool animationCurveExtremaTimes(const AnimationCurve &curve,std::vector<float> &times) {
  if(!validAnimationCurve(curve))return false;
  std::vector<float> result;
  for(usize i=0;i+1<curve.keys.size();++i) {
    bool next=false;if(isStep(curve.keys[i],curve.keys[i+1],next))continue;
    const auto s=segment(curve,i);
    const double a=-s.p[0].y+3*s.p[1].y-3*s.p[2].y+s.p[3].y;
    const double b=2*(s.p[0].y-2*s.p[1].y+s.p[2].y),c=s.p[1].y-s.p[0].y;
    const auto add=[&](double u) {
      if(u>0&&u<1&&std::isfinite(u)) {
        const auto t=static_cast<float>(point(s,u).x);
        if(t>curve.keys[i].time&&t<curve.keys[i+1].time)result.push_back(t);
      }
    };
    if(a==0) {if(b!=0)add(-c/b);}
    else {
      const double discriminant=b*b-4*a*c;
      if(discriminant>=0&&std::isfinite(discriminant)) {
        const double q=-.5*(b+std::copysign(std::sqrt(discriminant),b));
        if(q!=0) {add(q/a);add(c/q);}else add(-b/(2*a));
      }
    }
  }
  std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());
  times=std::move(result);return true;
}
bool putAnimationCurveKey(AnimationCurve &curve,AnimationCurveKey key,u64 &nextId,u64 &resultId) {
  if(!std::isfinite(key.time)||key.time<0||!nextId||nextId==std::numeric_limits<u64>::max())return false;
  auto candidate=curve;u64 frontier=nextId;
  if(key.id) {
    const auto found=std::find_if(candidate.keys.begin(),candidate.keys.end(),[&](const auto &k){return k.id==key.id;});
    if(found==candidate.keys.end())return false;
    *found=key;
  } else {
    const auto same=std::find_if(candidate.keys.begin(),candidate.keys.end(),[&](const auto &k){return k.time==key.time;});
    if(same!=candidate.keys.end()) {key.id=same->id;*same=key;}
    else {key.id=frontier++;candidate.keys.push_back(key);}
  }
  std::sort(candidate.keys.begin(),candidate.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
  if(!validAnimationCurve(candidate))return false;
  std::unordered_set<u64> ids;
  for(const auto &k:candidate.keys)if(k.id>=frontier||!ids.insert(k.id).second)return false;
  curve=std::move(candidate);nextId=frontier;resultId=key.id;return true;
}
bool eraseAnimationCurveKeys(AnimationCurve &curve,std::span<const u64> ids) {
  if(ids.empty()||!validAnimationCurve(curve))return false;
  auto candidate=curve;
  for(const auto id:ids) {
    const auto found=std::find_if(candidate.keys.begin(),candidate.keys.end(),[&](const auto &k){return k.id==id;});
    if(found==candidate.keys.end())return false;
    candidate.keys.erase(found);
  }
  if(!candidate.keys.empty()&&!validAnimationCurve(candidate))return false;
  curve=std::move(candidate);return true;
}
bool retimeAnimationCurve(AnimationCurve &curve,double scale,double offset) {
  if(!validAnimationCurve(curve)||!std::isfinite(scale)||scale<=0||!std::isfinite(offset))return false;
  auto candidate=curve;
  for(auto &k:candidate.keys) {
    const double t=k.time*scale+offset,in=k.inSlope/scale,out=k.outSlope/scale;
    if(t<0||!finiteFloat(t)||!finiteFloat(in)||!finiteFloat(out))return false;
    k.time=static_cast<float>(t);k.inSlope=static_cast<float>(in);k.outSlope=static_cast<float>(out);
  }
  if(!validAnimationCurve(candidate))return false;
  curve=std::move(candidate);return true;
}
bool reverseAnimationCurve(AnimationCurve &curve,double start,double end) {
  if(!validAnimationCurve(curve)||!std::isfinite(start)||!std::isfinite(end)||start<0||end<start||
     curve.keys.front().time<start||curve.keys.back().time>end)return false;
  auto candidate=curve;
  for(auto &k:candidate.keys) {
    const double t=start+end-k.time;if(!finiteFloat(t))return false;
    k.time=static_cast<float>(t);std::swap(k.inSlope,k.outSlope);k.inSlope=-k.inSlope;k.outSlope=-k.outSlope;
    std::swap(k.incoming,k.outgoing);k.incoming=reversed(k.incoming);k.outgoing=reversed(k.outgoing);
    std::swap(k.inWeight,k.outWeight);std::swap(k.weightedIn,k.weightedOut);
  }
  std::reverse(candidate.keys.begin(),candidate.keys.end());
  if(!validAnimationCurve(candidate))return false;
  curve=std::move(candidate);return true;
}
bool splitAnimationCurve(AnimationCurve &curve,float time,u64 &nextId,u64 &resultId) {
  if(!validAnimationCurve(curve)||!std::isfinite(time)||time<curve.keys.front().time||time>curve.keys.back().time||
     !nextId||nextId==std::numeric_limits<u64>::max())return false;
  const auto upper=std::lower_bound(curve.keys.begin(),curve.keys.end(),time,[](const auto &k,float t){return k.time<t;});
  if(upper!=curve.keys.end()&&upper->time==time) {resultId=upper->id;return true;}
  const usize i=static_cast<usize>(upper-curve.keys.begin())-1;
  bool holdNext=false;const bool step=isStep(curve.keys[i],curve.keys[i+1],holdNext);
  const auto original=segment(curve,i);const double u=parameter(original,time);
  auto candidate=curve;if(!freezeTangents(candidate))return false;
  AnimationCurveKey key;key.id=nextId;key.time=time;key.broken=true;key.incoming=key.outgoing=Mode::Free;
  if(step) {
    key.value=holdNext?curve.keys[i+1].value:curve.keys[i].value;
    key.incoming=key.outgoing=holdNext?Mode::NextConstant:Mode::Constant;
  } else {
    // De Casteljau subdivision retains the complete weighted curve on both
    // sides. Merely inserting Evaluate(time) and recomputing Auto changes it.
    const auto a=lerp(original.p[0],original.p[1],u),b=lerp(original.p[1],original.p[2],u),c=lerp(original.p[2],original.p[3],u);
    const auto d=lerp(a,b,u),e=lerp(b,c,u),p=lerp(d,e,u);
    const double left=time-original.p[0].x,right=original.p[3].x-time;
    auto setHandle=[](float &slope,float &weight,bool &weighted,Point anchor,Point handle,double length,bool incoming) {
      const double dx=handle.x-anchor.x,dy=handle.y-anchor.y;
      if(std::abs(dx)<1e-30&&std::abs(dy)>1e-30)return false;
      const double s=std::abs(dx)>1e-30?dy/dx:0,w=(incoming?-dx:dx)/length;
      if(!finiteFloat(s)||!std::isfinite(w)||w<-1e-6||w>1+1e-6)return false;
      slope=static_cast<float>(s);weight=static_cast<float>(std::clamp(w,0.0,1.0));weighted=true;return true;
    };
    auto &first=candidate.keys[i],&last=candidate.keys[i+1];
    if(!finiteFloat(p.y)||!setHandle(first.outSlope,first.outWeight,first.weightedOut,original.p[0],a,left,false)||
       !setHandle(key.inSlope,key.inWeight,key.weightedIn,p,d,left,true)||
       !setHandle(key.outSlope,key.outWeight,key.weightedOut,p,e,right,false)||
       !setHandle(last.inSlope,last.inWeight,last.weightedIn,original.p[3],c,right,true))return false;
    key.value=static_cast<float>(p.y);
  }
  candidate.keys.insert(candidate.keys.begin()+i+1,key);
  if(!validAnimationCurve(candidate))return false;
  for(const auto &k:curve.keys)if(k.id>=nextId)return false;
  curve=std::move(candidate);resultId=nextId++;return true;
}
bool cropAnimationCurve(AnimationCurve &curve,float start,float end,u64 &nextId) {
  if(!validAnimationCurve(curve)||!std::isfinite(start)||!std::isfinite(end)||start<0||end<start||!nextId)return false;
  for(const auto &k:curve.keys)if(k.id>=nextId)return false;
  auto candidate=curve;u64 frontier=nextId;
  if(!freezeTangents(candidate))return false;
  auto boundary=[&](float time) {
    if(time>=candidate.keys.front().time&&time<=candidate.keys.back().time) {
      u64 id=0;return splitAnimationCurve(candidate,time,frontier,id);
    }
    if(frontier==std::numeric_limits<u64>::max())return false;
    AnimationCurveKey key;key.id=frontier++;key.time=time;key.broken=true;
    key.incoming=key.outgoing=Mode::Constant;
    if(time<candidate.keys.front().time) {
      key.value=candidate.keys.front().value;candidate.keys.front().incoming=Mode::Constant;candidate.keys.front().broken=true;
      candidate.keys.insert(candidate.keys.begin(),key);
    } else {
      key.value=candidate.keys.back().value;candidate.keys.back().outgoing=Mode::Constant;candidate.keys.back().broken=true;
      candidate.keys.push_back(key);
    }
    return validAnimationCurve(candidate);
  };
  if(!boundary(start)||!boundary(end))return false;
  std::erase_if(candidate.keys,[&](const auto &k){return k.time<start||k.time>end;});
  for(auto &k:candidate.keys)k.time-=start;
  if(!validAnimationCurve(candidate))return false;
  curve=std::move(candidate);nextId=frontier;return true;
}
} // namespace ae::resources
