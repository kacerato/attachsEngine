#include "editor/editor_animation_ik.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {
struct V {float x=0,y=0,z=0;};
V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator*(V a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(V a){return std::sqrt(dot(a,a));}
bool normalize(V &a){const float l=length(a);if(!(l>1e-7f)||!std::isfinite(l))return false;a=a*(1/l);return true;}
V position(const float m[16]){return {m[12],m[13],m[14]};}
// Rotation matrix (row-major 3x3) taking direction a onto b.
void fromTo(V a,V b,float r[9]) {
  normalize(a);normalize(b);V axis=cross(a,b);const float s=length(axis),c=std::clamp(dot(a,b),-1.f,1.f);
  if(s<1e-6f) {
    if(c>0){r[0]=r[4]=r[8]=1;r[1]=r[2]=r[3]=r[5]=r[6]=r[7]=0;return;}
    axis=std::abs(a.x)<.9f?cross(a,V{1,0,0}):cross(a,V{0,1,0});normalize(axis);
  } else axis=axis*(1/s);
  const float angle=std::atan2(s,c),cs=std::cos(angle),sn=std::sin(angle),t=1-cs;
  const float x=axis.x,y=axis.y,z=axis.z;
  r[0]=t*x*x+cs;  r[1]=t*x*y-sn*z;r[2]=t*x*z+sn*y;
  r[3]=t*x*y+sn*z;r[4]=t*y*y+cs;  r[5]=t*y*z-sn*x;
  r[6]=t*x*z-sn*y;r[7]=t*y*z+sn*x;r[8]=t*z*z+cs;
}
V apply(const float r[9],V v){return {r[0]*v.x+r[1]*v.y+r[2]*v.z,r[3]*v.x+r[4]*v.y+r[5]*v.z,r[6]*v.x+r[7]*v.y+r[8]*v.z};}
void multiply(const float a[9],const float b[9],float out[9]) {
  for(int i=0;i<3;++i)for(int j=0;j<3;++j){float s=0;for(int k=0;k<3;++k)s+=a[i*3+k]*b[k*3+j];out[i*3+j]=s;}
}
// Rotates the basis columns of a world matrix and places its origin.
void rotated(const float in[16],const float r[9],V origin,float out[16]) {
  for(int column=0;column<3;++column) {
    const V axis=apply(r,{in[column*4],in[column*4+1],in[column*4+2]});
    out[column*4]=axis.x;out[column*4+1]=axis.y;out[column*4+2]=axis.z;out[column*4+3]=in[column*4+3];
  }
  out[12]=origin.x;out[13]=origin.y;out[14]=origin.z;out[15]=in[15];
}
} // namespace

bool solveTwoBoneIk(const float rootWorld[16],const float midWorld[16],const float endWorld[16],
                    const float targetValue[3],TwoBoneIkResult &out,const float *pole) {
  const V a=position(rootWorld),b=position(midWorld),c=position(endWorld),t{targetValue[0],targetValue[1],targetValue[2]};
  for(float v:{a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z,t.x,t.y,t.z})if(!std::isfinite(v))return false;
  const float upper=length(b-a),lower=length(c-b);
  if(upper<1e-5f||lower<1e-5f)return false;
  V toTarget=t-a;float distance=length(toTarget);
  if(distance<1e-5f)return false;
  const float maximum=(upper+lower)*.9999f,minimum=std::abs(upper-lower)*1.0001f+1e-5f;
  out.clamped=distance>maximum||distance<minimum;
  distance=std::clamp(distance,minimum,maximum);
  V u=toTarget;normalize(u);
  // Bend direction: the current knee offset projected off the root->target
  // axis. Falls back to the bend plane normal when the chain is straight.
  // Alvo de direcao (pole): quando presente, define para onde a dobra aponta.
  V bend=pole?(V{pole[0],pole[1],pole[2]}-a)-u*dot(V{pole[0],pole[1],pole[2]}-a,u):(b-a)-u*dot(b-a,u);
  if(!normalize(bend)) {
    V normal=cross(b-a,c-b);
    if(!normalize(normal))normal=std::abs(u.y)<.9f?cross(u,V{0,1,0}):cross(u,V{1,0,0});
    bend=cross(normal,u);if(!normalize(bend))return false;
  }
  const float cosRoot=std::clamp((upper*upper+distance*distance-lower*lower)/(2*upper*distance),-1.f,1.f);
  const float sinRoot=std::sqrt(std::max(0.f,1-cosRoot*cosRoot));
  const V knee=a+(u*cosRoot+bend*sinRoot)*upper;
  const V tip=a+u*distance;
  float r1[9];fromTo(b-a,knee-a,r1);
  const V rotatedLower=apply(r1,c-b);
  float r2[9];fromTo(rotatedLower,tip-knee,r2);
  float r21[9];multiply(r2,r1,r21);
  rotated(rootWorld,r1,a,out.root);
  rotated(midWorld,r21,knee,out.mid);
  out.reached[0]=tip.x;out.reached[1]=tip.y;out.reached[2]=tip.z;
  for(float v:out.root)if(!std::isfinite(v))return false;
  for(float v:out.mid)if(!std::isfinite(v))return false;
  return true;
}
} // namespace ae::editor
