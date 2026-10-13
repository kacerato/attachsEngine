#pragma once
// Matematica do IK de plano rotativo (rotate-plane). Mesma sequencia do solver
// do UMotion Pro 1.29 (Rotate Plane Solver, manual "Inverse Kinematics"), em
// matrizes 3x3 por linha aplicadas a vetor coluna, como ikWeightedFromTo.
#include <cmath>

namespace ae::runtime::rotate_plane {
struct V {float x=0,y=0,z=0;};
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(V a){return std::sqrt(dot(a,a));}
inline V normalized(V a){const float n=length(a);return n>1e-9f?a*(1/n):V{};}
inline V position(const float m[16]){return {m[12],m[13],m[14]};}
// Direcao de um vetor local pela rotacao do mundo (colunas normalizadas: a
// escala do pai nao entra), como Transform.TransformDirection.
inline V direction(const float m[16],V v) {
  V c[3];for(int i=0;i<3;++i)c[i]=normalized(V{m[i*4],m[i*4+1],m[i*4+2]});
  return c[0]*v.x+c[1]*v.y+c[2]*v.z;
}
// Perpendicular escolhida pelo eixo dominante (zero do angulo do plano).
inline V perpendicular(V v) {
  const V a{std::fabs(v.x),std::fabs(v.y),std::fabs(v.z)};
  if(a.z>a.x&&a.z>a.y)return {-v.z,0,v.x};
  return {-v.y,v.x,0};
}
// Componente de v perpendicular ao eixo unitario.
inline V reject(V v,V axis){return v-axis*dot(v,axis);}
inline void identity(float r[9]){for(int i=0;i<9;++i)r[i]=(i%4==0)?1.f:0.f;}
inline void axisAngle(V axis,float degrees,float r[9]) {
  axis=normalized(axis);identity(r);if(length(axis)<.5f)return;
  const float a=degrees*3.14159265358979f/180.f,c=std::cos(a),s=std::sin(a),t=1-c,x=axis.x,y=axis.y,z=axis.z;
  r[0]=t*x*x+c;r[1]=t*x*y-s*z;r[2]=t*x*z+s*y;
  r[3]=t*x*y+s*z;r[4]=t*y*y+c;r[5]=t*y*z-s*x;
  r[6]=t*x*z-s*y;r[7]=t*y*z+s*x;r[8]=t*z*z+c;
}
inline V apply(const float r[9],V v){return {r[0]*v.x+r[1]*v.y+r[2]*v.z,r[3]*v.x+r[4]*v.y+r[5]*v.z,r[6]*v.x+r[7]*v.y+r[8]*v.z};}
// Base com z=frente e y o mais perto possivel de "cima" (LookRotation).
inline bool look(V forward,V up,float m[9]) {
  const V z=normalized(forward);V x=normalized(cross(up,z));
  if(length(z)<.5f||length(x)<.5f)return false;
  const V y=cross(z,x);
  m[0]=x.x;m[1]=y.x;m[2]=z.x;m[3]=x.y;m[4]=y.y;m[5]=z.y;m[6]=x.z;m[7]=y.z;m[8]=z.z;return true;
}
// Rotacao que leva a base de `from` para a de `to`, com o mesmo "cima".
inline bool lookDelta(V to,V from,V up,float r[9]) {
  float a[9],b[9];if(!look(to,up,a)||!look(from,up,b))return false;
  for(int i=0;i<3;++i)for(int j=0;j<3;++j)r[i*3+j]=a[i*3]*b[j*3]+a[i*3+1]*b[j*3+1]+a[i*3+2]*b[j*3+2];
  return true;
}
// Quaternion (x,y,z,w) de uma rotacao 3x3 por linha.
inline void quaternion(const float r[9],float q[4]) {
  const float trace=r[0]+r[4]+r[8];
  if(trace>0) {const float s=std::sqrt(trace+1)*2;q[3]=.25f*s;q[0]=(r[7]-r[5])/s;q[1]=(r[2]-r[6])/s;q[2]=(r[3]-r[1])/s;}
  else if(r[0]>r[4]&&r[0]>r[8]) {const float s=std::sqrt(1+r[0]-r[4]-r[8])*2;q[3]=(r[7]-r[5])/s;q[0]=.25f*s;q[1]=(r[1]+r[3])/s;q[2]=(r[2]+r[6])/s;}
  else if(r[4]>r[8]) {const float s=std::sqrt(1+r[4]-r[0]-r[8])*2;q[3]=(r[2]-r[6])/s;q[0]=(r[1]+r[3])/s;q[1]=.25f*s;q[2]=(r[5]+r[7])/s;}
  else {const float s=std::sqrt(1+r[8]-r[0]-r[4])*2;q[3]=(r[3]-r[1])/s;q[0]=(r[2]+r[6])/s;q[1]=(r[5]+r[7])/s;q[2]=.25f*s;}
}
// Rotacao de uma matriz de mundo (colunas normalizadas e ortogonalizadas).
// Falso com escala nula ou espelho: o chamador usa o caminho por matriz.
inline bool basisQuaternion(const float m[16],float q[4]) {
  V x=normalized(V{m[0],m[1],m[2]}),y=V{m[4],m[5],m[6]};
  y=normalized(reject(y,x));const V z=cross(x,y);
  if(length(x)<.5f||length(y)<.5f||dot(z,V{m[8],m[9],m[10]})<=0)return false;
  const float r[9]{x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z};quaternion(r,q);return true;
}
inline bool fromTo(V a,V b,float r[9]) {
  a=normalized(a);b=normalized(b);identity(r);if(length(a)<.5f||length(b)<.5f)return false;
  const V axis=cross(a,b);const float s=length(axis),c=dot(a,b);
  if(s<1e-7f) {
    if(c>0)return true;
    V ortho=perpendicular(a);if(length(ortho)<1e-6f)ortho={0,-a.z,a.y};
    axisAngle(ortho,180,r);return true;
  }
  axisAngle(axis,std::atan2(s,c)*180.f/3.14159265358979f,r);return true;
}
} // namespace ae::runtime::rotate_plane
