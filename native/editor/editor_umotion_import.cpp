// Importar projeto UMotion Pro (.asset) para um personagem da cena, como rig
// editavel da Astra: um clipe por clipe do projeto (FK com as curvas do UMotion
// chave por chave), alvos de IK e objetos auxiliares que nao existem no modelo,
// IK de dois ossos em Plano rotativo (mesma sequencia do solver do UMotion),
// Filho de com pai animado, mistura FK/IK e rotacao do polo animadas, camadas,
// eventos e root motion. A conversao Unity -> Astra (espelho, escala, eixos e
// o referencial do pai) e estimada pela pose de repouso de cada osso e e linear
// nas chaves, entao as curvas nao sao reamostradas.
#include "editor/editor_umotion_import.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_animation_pose_selection.h"
#include "core/rotation_math.h"
#include "runtime/ik_rotate_plane.h"
#include "resources/umotion_project.h"
#include "resources/animation_clip_asset.h"
#include "runtime/transform_math.h"
#include "scene/animation.h"
#include "scene/ik_constraints.h"
#include "scene/ik_effects.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <set>

namespace ae::editor {
namespace {
using namespace resources;
// ---- Matematica: matrizes coluna-maior (como runtime/transform_math) ----
struct M4 {float m[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};};
M4 mul(const M4 &a,const M4 &b) {M4 r;runtime::multiplyMatrix(a.m,b.m,r.m);return r;}
M4 inverse(const M4 &a) {
  // Inversa afim: [L|t]^-1 = [L^-1|-L^-1 t].
  const float *m=a.m;M4 r;
  const float a00=m[0],a01=m[4],a02=m[8],a10=m[1],a11=m[5],a12=m[9],a20=m[2],a21=m[6],a22=m[10];
  const float c00=a11*a22-a12*a21,c01=a02*a21-a01*a22,c02=a01*a12-a02*a11;
  const float c10=a12*a20-a10*a22,c11=a00*a22-a02*a20,c12=a02*a10-a00*a12;
  const float c20=a10*a21-a11*a20,c21=a01*a20-a00*a21,c22=a00*a11-a01*a10;
  const float det=a00*c00+a01*c10+a02*c20;const float k=std::fabs(det)>1e-20f?1/det:0;
  r.m[0]=c00*k;r.m[4]=c01*k;r.m[8]=c02*k;r.m[1]=c10*k;r.m[5]=c11*k;r.m[9]=c12*k;r.m[2]=c20*k;r.m[6]=c21*k;r.m[10]=c22*k;
  for(int i=0;i<3;++i)r.m[12+i]=-(r.m[i]*m[12]+r.m[4+i]*m[13]+r.m[8+i]*m[14]);
  return r;
}
void quatMatrix(const float q[4],float r[9]) { // linha-maior
  const float x=q[0],y=q[1],z=q[2],w=q[3];
  r[0]=1-2*(y*y+z*z);r[1]=2*(x*y-z*w);r[2]=2*(x*z+y*w);
  r[3]=2*(x*y+z*w);r[4]=1-2*(x*x+z*z);r[5]=2*(y*z-x*w);
  r[6]=2*(x*z-y*w);r[7]=2*(y*z+x*w);r[8]=1-2*(x*x+y*y);
}
void matrixQuat(const float r[9],float q[4]) { // r linha-maior, rotacao propria
  const float t=r[0]+r[4]+r[8];
  if(t>0) {const float s=std::sqrt(t+1)*2;q[3]=s/4;q[0]=(r[7]-r[5])/s;q[1]=(r[2]-r[6])/s;q[2]=(r[3]-r[1])/s;}
  else if(r[0]>r[4]&&r[0]>r[8]) {const float s=std::sqrt(1+r[0]-r[4]-r[8])*2;q[3]=(r[7]-r[5])/s;q[0]=s/4;q[1]=(r[1]+r[3])/s;q[2]=(r[2]+r[6])/s;}
  else if(r[4]>r[8]) {const float s=std::sqrt(1+r[4]-r[0]-r[8])*2;q[3]=(r[2]-r[6])/s;q[0]=(r[1]+r[3])/s;q[1]=s/4;q[2]=(r[5]+r[7])/s;}
  else {const float s=std::sqrt(1+r[8]-r[0]-r[4])*2;q[3]=(r[3]-r[1])/s;q[0]=(r[2]+r[6])/s;q[1]=(r[5]+r[7])/s;q[2]=s/4;}
  normalizeRotationQuaternion(q);
}
M4 trs(const float p[3],const float q[4],const float s[3]) {
  float r[9];quatMatrix(q,r);M4 o;
  for(int c=0;c<3;++c)for(int k=0;k<3;++k)o.m[c*4+k]=r[k*3+c]*s[c];
  o.m[12]=p[0];o.m[13]=p[1];o.m[14]=p[2];return o;
}
// Euler da Unity (Quaternion.Euler): Z, depois X, depois Y.
void unityEuler(const float e[3],float q[4]) {
  const auto axis=[](int i,float deg,float out[4]){const float h=deg*3.14159265358979f/360.f;out[0]=out[1]=out[2]=0;out[i]=std::sin(h);out[3]=std::cos(h);};
  const auto mulq=[](const float a[4],const float b[4],float o[4]){
    o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
    o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];};
  float qx[4],qy[4],qz[4],t[4];axis(0,e[0],qx);axis(1,e[1],qy);axis(2,e[2],qz);
  mulq(qy,qx,t);mulq(t,qz,q); // R = Ry * Rx * Rz (aplica Z, X, Y)
}
// Parte linear: escala uniforme e matriz ortogonal (det +-1), linha-maior.
bool similarity(const M4 &a,float &scale,float o[9]) {
  float l[9];for(int r=0;r<3;++r)for(int c=0;c<3;++c)l[r*3+c]=a.m[c*4+r];
  const float det=l[0]*(l[4]*l[8]-l[5]*l[7])-l[1]*(l[3]*l[8]-l[5]*l[6])+l[2]*(l[3]*l[7]-l[4]*l[6]);
  scale=std::cbrt(std::fabs(det));if(scale<1e-12f)return false;
  for(int i=0;i<9;++i)o[i]=l[i]/scale;
  // Ortogonalidade (tolerancia de exportadores): O*O^T ~ I.
  for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
    float d=0;for(int k=0;k<3;++k)d+=o[r*3+k]*o[c*3+k];
    if(std::fabs(d-(r==c?1.f:0.f))>2e-3f)return false;
  }
  return true;
}
float det3(const float o[9]){return o[0]*(o[4]*o[8]-o[5]*o[7])-o[1]*(o[3]*o[8]-o[5]*o[6])+o[2]*(o[3]*o[7]-o[4]*o[6]);}
// Matriz 4x4 que leva q_unity (x,y,z,w) ao q_astra: q = mD(qL* (x) q) (x) qR.
struct QuatMap {float k[16]{};};
void leftMul(const float a[4],float m[16]) { // a (x) q
  const float x=a[0],y=a[1],z=a[2],w=a[3];
  const float v[16]{w,-z,y,x, z,w,-x,y, -y,x,w,z, -x,-y,-z,w};std::copy(v,v+16,m);
}
void rightMul(const float b[4],float m[16]) { // q (x) b
  const float x=b[0],y=b[1],z=b[2],w=b[3];
  const float v[16]{w,z,-y,x, -z,w,x,y, y,-x,w,z, -x,-y,-z,w};std::copy(v,v+16,m);
}
void mat4mul(const float a[16],const float b[16],float o[16]) {for(int r=0;r<4;++r)for(int c=0;c<4;++c){float s=0;for(int k=0;k<4;++k)s+=a[r*4+k]*b[k*4+c];o[r*4+c]=s;}}
void applyQ(const float k[16],const float q[4],float o[4]) {for(int r=0;r<4;++r)o[r]=k[r*4]*q[0]+k[r*4+1]*q[1]+k[r*4+2]*q[2]+k[r*4+3]*q[3];}
// Conversao de um objeto: L_astra = G^-1 * L_unity * Psi.
struct LocalMap {
  float position[9]{},offset[3]{};   // p_A = P*p_U + offset
  QuatMap rotation;                   // q_A = K*q_U
  // A mesma conversao como produto: q_A = antes (x) q_U (x) depois. Euler do
  // UMotion entra com as proprias chaves e este referencial na trilha.
  float before[4]{0,0,0,1},after[4]{0,0,0,1};
  // Camada aditiva: o delta da Unity vale no espaco do osso, entao so o
  // referencial do osso (Psi) o conjuga: delta_A = Psi^-1 delta_U Psi.
  QuatMap additiveRotation;float additiveBefore[4]{0,0,0,1},additiveAfter[4]{0,0,0,1};
  float scale=1;int permutation[3]{0,1,2};bool scaleAligned=true;
};
bool makeMap(const M4 &g,const M4 &psi,LocalMap &out) {
  float gs,go[9],ps,po[9];
  if(!similarity(g,gs,go)||!similarity(psi,ps,po))return false;
  if((det3(go)>0)!=(det3(po)>0))return false; // um espelho de cada lado ou nenhum
  const M4 gi=inverse(g);
  for(int r=0;r<3;++r){for(int c=0;c<3;++c)out.position[r*3+c]=gi.m[c*4+r];out.offset[r]=gi.m[12+r];}
  // Rotacao: com espelho D=diag(-1,1,1), G_O=Q_G*D e Psi_O=D*Q_Psi.
  const bool mirrored=det3(go)<0;float qg[9],qp[9];
  for(int i=0;i<9;++i){qg[i]=go[i];qp[i]=po[i];}
  if(mirrored) {for(int r=0;r<3;++r)qg[r*3]=-qg[r*3];for(int c=0;c<3;++c)qp[c]=-qp[c];}
  float qG[4],qP[4];matrixQuat(qg,qG);matrixQuat(qp,qP);
  const float qGc[4]{-qG[0],-qG[1],-qG[2],qG[3]};
  float l[16],r[16],t[16];leftMul(qGc,l);rightMul(qP,r);
  if(mirrored) {const float d[16]{1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1};mat4mul(d,l,t);std::copy(t,t+16,l);}
  mat4mul(r,l,out.rotation.k);
  {
    const auto product=[](const float a[4],const float b[4],float o[4]) {
      o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
      o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];};
    // Espelho D: conjugar por D e conjugar pela meia volta em X (q -> X q X*).
    const float x[4]{1,0,0,0},xc[4]{-1,0,0,0},qPc[4]{-qP[0],-qP[1],-qP[2],qP[3]};
    if(mirrored) {
      product(x,qGc,out.before);product(xc,qP,out.after);
      product(qPc,x,out.additiveBefore);product(xc,qP,out.additiveAfter);
    } else {
      std::copy(qGc,qGc+4,out.before);std::copy(qP,qP+4,out.after);
      std::copy(qPc,qPc+4,out.additiveBefore);std::copy(qP,qP+4,out.additiveAfter);
    }
    float al[16],ar[16];leftMul(out.additiveBefore,al);rightMul(out.additiveAfter,ar);mat4mul(ar,al,out.additiveRotation.k);
  }
  out.scale=ps/gs;
  // Escala nao uniforme so atravessa Psi se ele for permutacao de eixos.
  for(int c=0;c<3;++c) {
    int best=0;for(int k=1;k<3;++k)if(std::fabs(po[k*3+c])>std::fabs(po[best*3+c]))best=k;
    out.permutation[c]=best;if(std::fabs(std::fabs(po[best*3+c])-1)>1e-3f)out.scaleAligned=false;
  }
  return true;
}
bool fail(std::string &error,std::string text){error=std::move(text);return false;}
// Tangentes explicitas: Linear vira a inclinacao ate a chave vizinha (como a
// Unity); automaticas e livres usam o valor gravado pelo UMotion.
std::vector<UMotionKey> explicitTangents(const std::vector<UMotionKey> &in) {
  auto out=in;
  for(usize k=0;k<out.size();++k) {
    if(out[k].incoming()==UMotionTangent::Linear)out[k].inTangent=k?(in[k].value-in[k-1].value)/float(in[k].frame-in[k-1].frame):0.f;
    if(out[k].outgoing()==UMotionTangent::Linear)out[k].outTangent=k+1<in.size()?(in[k+1].value-in[k].value)/float(in[k+1].frame-in[k].frame):0.f;
    const bool constant=out[k].incoming()==UMotionTangent::Constant||out[k].outgoing()==UMotionTangent::Constant;
    if(!constant) {
      const u32 broken=out[k].inTangent!=out[k].outTangent?0x100u:0u;
      out[k].mode=(out[k].mode&0xF000u)|0x44u|broken; // Free nos dois lados
    }
  }
  return out;
}
std::string leaf(const std::string &path){const auto s=path.rfind('/');return s==std::string::npos?path:path.substr(s+1);}
std::string parentPath(const std::string &path){const auto s=path.rfind('/');return s==std::string::npos?std::string():path.substr(0,s);}
AnimationTangentMode tangent(UMotionTangent t) {
  switch(t){case UMotionTangent::Linear:return AnimationTangentMode::Linear;case UMotionTangent::Constant:return AnimationTangentMode::Constant;default:return AnimationTangentMode::Free;}
}
// Chave do UMotion (quadros) como chave da Astra (segundos).
AnimationCurveKey key(const UMotionKey &k,i32 start,float rate,u64 &nextId) {
  AnimationCurveKey o;o.id=nextId++;o.time=float(k.frame-start)/rate;o.value=k.value;
  o.inSlope=std::isfinite(k.inTangent)?k.inTangent*rate:0;o.outSlope=std::isfinite(k.outTangent)?k.outTangent*rate:0;
  o.incoming=tangent(k.incoming());o.outgoing=tangent(k.outgoing());o.broken=k.broken()||o.incoming!=o.outgoing;
  o.weightedIn=k.weightedIn();o.weightedOut=k.weightedOut();o.inWeight=k.inWeight;o.outWeight=k.outWeight;
  if(o.incoming==AnimationTangentMode::Constant||o.outgoing==AnimationTangentMode::Constant)o.broken=true;
  return o;
}
} // namespace

bool EditorSession::importUMotionProject(std::string_view relative,EditorEntityId owner,UMotionImportReport &report,std::string &error) {
  report={};error.clear();
  if(isPlaying()||history_.isOpen()) {error="Pare o Play e conclua a edição antes de importar";return false;}
  const auto *ownerEntity=document_.find(owner);
  if(!ownerEntity||owner==document_.root()) {error="Selecione o personagem que recebe a animação";return false;}
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());std::filesystem::path path;std::vector<u8> bytes;
  if(!EditorImportTransaction::safePath(root,std::string(relative),path)||!EditorImportTransaction::read(path,bytes,64u<<20)) {error="Projeto UMotion não encontrado no projeto";return false;}
  UMotionProject project;
  if(!readUMotionProject(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()),project,error))return false;
  {
    const auto prepare=[](UMotionFloatChannel &channel){for(auto &clip:channel.clips)for(auto &layer:clip)layer=explicitTangents(layer);};
    for(auto &o:project.objects) {
      for(auto &c:o.position)prepare(c);
      for(auto &c:o.rotation)prepare(c);
      for(auto &c:o.scale)prepare(c);
      prepare(o.fkIkBlend);prepare(o.poleRotation);prepare(o.childOfPinning);for(auto &p:o.customProperties)prepare(p.channel);
    }
  }
  const auto &objects=project.objects;const usize count=objects.size();
  // ---- 1) Objetos do UMotion -> nos da cena ----
  std::vector<EditorEntityId> node(count,kInvalidEntity);std::vector<bool> created(count,false);
  std::set<std::string> firstSegments;
  for(const auto &o:objects)if(!o.path.empty())firstSegments.insert(o.path.substr(0,o.path.find('/')));
  std::vector<EditorEntityId> subtree;document_.collectSubtree(owner,subtree);
  EditorEntityId container=kInvalidEntity;usize best=0;
  for(const auto id:subtree) {
    usize hits=0;for(const auto child:document_.childrenOf(id))if(firstSegments.count(document_.find(child)->name))++hits;
    if(hits>best){best=hits;container=id;}
  }
  if(!container) {error="Nenhum osso do projeto UMotion existe neste personagem";return false;}
  const auto childNamed=[&](EditorEntityId parent,const std::string &name)->EditorEntityId {
    EditorEntityId found=kInvalidEntity;
    for(const auto child:document_.childrenOf(parent))if(document_.find(child)->name==name){if(found)return kInvalidEntity;found=child;}
    return found;
  };
  std::vector<usize> order(count);for(usize i=0;i<count;++i)order[i]=i;
  std::stable_sort(order.begin(),order.end(),[&](usize a,usize b){return std::count(objects[a].path.begin(),objects[a].path.end(),'/')<std::count(objects[b].path.begin(),objects[b].path.end(),'/');});
  std::vector<i32> unityParent(count,-1);
  for(const auto i:order) {
    const auto &o=objects[i];if(o.path.empty())continue;
    const auto parent=parentPath(o.path);unityParent[i]=parent.empty()?project.find(""):project.find(parent);
    EditorEntityId at=container;std::string walk;
    for(usize s=0,e;s<=o.path.size();s=e+1) {
      e=o.path.find('/',s);if(e==std::string::npos)e=o.path.size();
      at=at?childNamed(at,o.path.substr(s,e-s)):kInvalidEntity;
    }
    node[i]=at;
  }
  // ---- 2) Repouso na Unity (espaco da raiz animada) ----
  std::vector<M4> unityWorld(count);std::vector<bool> worldDone(count,false);
  const auto unityLocal=[&](const UMotionObject &o) {
    float p[3],q[4],s[3];for(int c=0;c<3;++c){p[c]=o.position[c].reference;s[c]=o.scale[c].reference;}
    for(int c=0;c<4;++c)q[c]=o.rotation[c].reference;
    if(!normalizeRotationQuaternion(q)) {const float e[3]{o.rotation[0].reference,o.rotation[1].reference,o.rotation[2].reference};unityEuler(e,q);}
    for(float &v:s)if(std::fabs(v)<1e-9f)v=1;
    return trs(p,q,s);
  };
  for(const auto i:order) {
    if(objects[i].path.empty()){unityWorld[i]=M4{};worldDone[i]=true;continue;}
    const i32 p=unityParent[i];
    unityWorld[i]=p>=0&&!objects[p].path.empty()?mul(unityWorld[p],unityLocal(objects[i])):unityLocal(objects[i]);worldDone[i]=true;
  }
  // ---- 3) Referencial: Phi (espelho/permutacao + escala + translacao) em F ----
  const auto worldOf=[&](EditorEntityId id){M4 w;runtime::worldMatrix(document_,id,w.m);return w;};
  struct Fit {EditorEntityId frame=0;float scale=1,perm[9]{},offset[3]{};double relative=1e30;};Fit fit;
  std::vector<EditorEntityId> frames;for(auto at=container;at&&at!=document_.root();at=document_.find(at)->parent){frames.push_back(at);if(at==owner)break;}
  for(const auto frame:frames) {
    const M4 inv=inverse(worldOf(frame));
    std::vector<std::array<float,3>> a,u;
    for(usize i=0;i<count;++i)if(node[i]&&!objects[i].path.empty()) {
      const M4 w=mul(inv,worldOf(node[i]));a.push_back({w.m[12],w.m[13],w.m[14]});
      u.push_back({unityWorld[i].m[12],unityWorld[i].m[13],unityWorld[i].m[14]});
    }
    if(a.size()<3)continue;
    double ca[3]{},cu[3]{};for(usize k=0;k<a.size();++k)for(int c=0;c<3;++c){ca[c]+=a[k][c];cu[c]+=u[k][c];}
    for(int c=0;c<3;++c){ca[c]/=a.size();cu[c]/=a.size();}
    static constexpr int perms[6][3]{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for(const auto &pm:perms)for(int signs=0;signs<8;++signs) {
      float P[9]{};for(int r=0;r<3;++r)P[r*3+pm[r]]=(signs>>r)&1?-1.f:1.f;
      double num=0,den=0;
      for(usize k=0;k<a.size();++k)for(int r=0;r<3;++r) {
        double pu=0;for(int c=0;c<3;++c)pu+=P[r*3+c]*(u[k][c]-cu[c]);
        num+=(a[k][r]-ca[r])*pu;den+=pu*pu;
      }
      if(den<1e-12||num<=0)continue;
      const double s=num/den;double err=0,size=0;
      for(usize k=0;k<a.size();++k)for(int r=0;r<3;++r) {
        double pu=0;for(int c=0;c<3;++c)pu+=P[r*3+c]*(u[k][c]-cu[c]);
        const double d=(a[k][r]-ca[r])-s*pu;err+=d*d;size+=(a[k][r]-ca[r])*(a[k][r]-ca[r]);
      }
      const double rel=std::sqrt(err/std::max(size,1e-12));
      if(rel<fit.relative) {
        fit.relative=rel;fit.frame=frame;fit.scale=static_cast<float>(s);std::copy(P,P+9,fit.perm);
        for(int r=0;r<3;++r){double pu=0;for(int c=0;c<3;++c)pu+=P[r*3+c]*cu[c];fit.offset[r]=static_cast<float>(ca[r]-s*pu);}
      }
    }
  }
  if(!fit.frame||fit.relative>2e-3) {error="A pose de repouso do personagem não corresponde ao rig do projeto UMotion (erro "+std::to_string(fit.relative*100)+"%)";return false;}
  report.restError=fit.relative;
  M4 phi;for(int r=0;r<3;++r){for(int c=0;c<3;++c)phi.m[c*4+r]=fit.perm[r*3+c]*fit.scale;phi.m[12+r]=fit.offset[r];}
  const M4 frameInverse=inverse(worldOf(fit.frame));
  const auto astraInFrame=[&](EditorEntityId id){return mul(frameInverse,worldOf(id));};
  // Psi dos objetos existentes: W_A = Phi*W_U*Psi.
  std::vector<M4> psi(count);
  // Auxiliares (alvos, caixa): so a permutacao de eixos e a escala que leva o
  // tamanho ao mundo. O referencial do ajuste pode ser a armadura (escala 0,01
  // no Kyle) com s = 100; dividir so por s deixava a caixa 100x menor.
  const float frameScale=[&]{const M4 f=worldOf(fit.frame);float s,o[9];return similarity(f,s,o)?s:1.f;}();
  M4 helperPsi;for(int r=0;r<3;++r)for(int c=0;c<3;++c)helperPsi.m[c*4+r]=fit.perm[c*3+r]/(fit.scale*frameScale); // (|F|*s*P)^-1
  for(usize i=0;i<count;++i) {
    if(objects[i].path.empty()||!node[i]){psi[i]=helperPsi;continue;}
    psi[i]=mul(inverse(mul(phi,unityWorld[i])),astraInFrame(node[i]));
    float s,o[9];
    if(!similarity(psi[i],s,o)) {error="Osso '"+objects[i].path+"' com escala não uniforme ou cisalhamento no repouso";return false;}
    const float t=std::sqrt(psi[i].m[12]*psi[i].m[12]+psi[i].m[13]*psi[i].m[13]+psi[i].m[14]*psi[i].m[14]);
    if(t>1e-3f*s*100) {error="Origem do osso '"+objects[i].path+"' difere do rig UMotion";return false;}
    psi[i].m[12]=psi[i].m[13]=psi[i].m[14]=0;
  }
  // Mapa local de um objeto quando seu pai e `parent` (indice UMotion, -1 = raiz).
  const auto mapFor=[&](usize i,i32 parent,EditorEntityId astraParent,LocalMap &out) {
    M4 g;
    if(parent>=0&&!objects[parent].path.empty()&&node[parent]==astraParent)g=psi[parent];
    else {const M4 unityParentWorld=parent>=0&&!objects[parent].path.empty()?unityWorld[parent]:M4{};g=mul(inverse(mul(phi,unityParentWorld)),astraInFrame(astraParent));}
    return makeMap(g,psi[i],out);
  };
  const auto convertPoint=[](const LocalMap &m,const float p[3],float out[3]){for(int r=0;r<3;++r)out[r]=m.position[r*3]*p[0]+m.position[r*3+1]*p[1]+m.position[r*3+2]*p[2]+m.offset[r];};
  // ---- 4) Cena: auxiliares, IK e Filho de numa transacao ----
  if(!history_.begin("Importar projeto UMotion")) {error="Não foi possível abrir a edição";return false;}
  const auto cancel=[&](std::string text){history_.cancel(document_);error=std::move(text);return false;};
  for(const auto i:order) {
    const auto &o=objects[i];if(o.path.empty()||node[i])continue;
    const i32 p=unityParent[i];const EditorEntityId parent=p>=0&&!objects[p].path.empty()?node[p]:fit.frame;
    if(!parent)return cancel("Pai de '"+o.path+"' ausente");
    const auto id=history_.createEntity(document_,parent,EditorEntityKind::Folder,leaf(o.path));
    if(!id)return cancel("Não foi possível criar '"+o.path+"'");
    node[i]=id;created[i]=true;++report.helpers;
    LocalMap m;if(!mapFor(i,p,parent,m))return cancel("Referencial inválido para '"+o.path+"'");
    auto values=*document_.find(id);
    float pu[3]{o.position[0].reference,o.position[1].reference,o.position[2].reference};convertPoint(m,pu,values.transform.position);
    float qu[4]{o.rotation[0].reference,o.rotation[1].reference,o.rotation[2].reference,o.rotation[3].reference},qa[4];
    if(!normalizeRotationQuaternion(qu)){const float e[3]{qu[0],qu[1],qu[2]};unityEuler(e,qu);}
    applyQ(m.rotation.k,qu,qa);normalizeRotationQuaternion(qa);rotationEulerXYZ(qa,values.transform.rotationDegrees);
    for(int c=0;c<3;++c)values.transform.scale[c]=std::fabs(o.scale[m.permutation[c]].reference)*m.scale;
    for(float &v:values.transform.scale)if(!(v>0))v=1;
    if(!history_.applyValues(document_,id,values))return cancel("Não foi possível posicionar '"+o.path+"'");
    // Objeto de cena do exemplo (cubo, caixa): visual de 1 m da Unity.
    const auto name=leaf(o.path);
    if(o.type!=2&&(name.find("Cube")!=std::string::npos||name.find("Box")!=std::string::npos)) {
      const u32 slot=boxAssetSlot()?boxAssetSlot():(mapScene_.assetCount()?1u:0u);
      if(slot) {
        // Nasce na origem do objeto (a posicao local e zerada logo abaixo).
        float at[16];editorWorldMatrix(document_,id,at);const float origin[3]{at[12],at[13],at[14]};
        EditorAssetInstantiation options;options.name=name+" · visual";options.rigidBody=false;
        const auto visual=instantiateAssetInTransaction(slot-1,id,origin,&options);
        if(visual) {
          auto v=*document_.find(visual);for(int c=0;c<3;++c){v.transform.position[c]=0;v.transform.rotationDegrees[c]=0;v.transform.scale[c]=1;}
          // So aparencia: a caixa e animada pelo clipe (e pelo Filho de), nao
          // simulada. Um corpo na subarvore tomaria a pose do objeto animado.
          v.components.remove(scene::Collider::descriptor);v.components.remove(scene::PhysicsBody::descriptor);
          history_.applyValues(document_,visual,v);
        }
      }
    }
  }
  // Repouso dos auxiliares no referencial: Psi = (s*P)^-1 por construcao.
  std::map<EditorEntityId,u64> ikComponent,childOfComponent;
  for(usize i=0;i<count;++i) {
    const auto &o=objects[i];
    if(o.ik&&o.ikTarget>=0) {
      const auto target=node[o.ikTarget];if(!target)return cancel("Alvo do IK de '"+o.path+"' ausente");
      auto values=*document_.find(target);
      // Dois ossos: IK de dois ossos; cadeia maior: IK de cadeia, ambos em Plano rotativo.
      const auto &type=o.ikChainLength==2?scene::TwoBoneIk::descriptor:scene::ChainIk::descriptor;
      auto *component=values.components.find(type)?values.components.edit(type):values.components.add(type);
      if(!component)return cancel("Não foi possível adicionar IK em '"+objects[o.ikTarget].path+"'");
      const u64 pole=o.ikPole>=0?node[o.ikPole]:0;const u32 reference=o.ikReferenceMode==0?1u:0u;const float weight=std::clamp(o.fkIkBlend.reference,0.f,1.f);
      // Rotacao do alvo: a mao recebe a do alvo no referencial do osso dela
      // (Psi do alvo -> Psi da ponta), nao no referencial do auxiliar.
      float offset[3]{};
      if(o.ikTargetRotationMode==1) {
        const M4 relative=mul(inverse(psi[i]),psi[o.ikTarget]);float r[9];
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)r[row*3+col]=relative.m[col*4+row];
        for(int col=0;col<3;++col){const float n=std::sqrt(r[col]*r[col]+r[3+col]*r[3+col]+r[6+col]*r[6+col]);if(n>1e-9f)for(int row=0;row<3;++row)r[row*3+col]/=n;}
        float q[4];runtime::rotate_plane::quaternion(r,q);
        if(normalizeRotationQuaternion(q))rotationEulerXYZ(q,offset);
      }
      if(&type==&scene::TwoBoneIk::descriptor) {
        auto *c=static_cast<scene::TwoBoneIk*>(component);c->target=node[i];c->pole=pole;c->solver=1;c->reference=reference;
        c->matchTargetRotation=o.ikTargetRotationMode==1;c->weight=weight;c->enabled=true;std::copy(offset,offset+3,c->targetOffset);
      } else {
        auto *c=static_cast<scene::ChainIk*>(component);c->target=node[i];c->pole=pole;c->solver=2;c->reference=reference;
        c->matchTargetRotation=o.ikTargetRotationMode==1;c->weight=weight;c->enabled=true;c->chainLength=static_cast<float>(std::clamp<u32>(o.ikChainLength,2,32));
        std::copy(offset,offset+3,c->targetOffset);
      }
      if(!history_.applyValues(document_,target,values))return cancel("IK recusado em '"+objects[o.ikTarget].path+"'");
      ikComponent[target]=document_.find(target)->components.find(type)->instanceId();++report.ikChains;
    }
    if(o.childOf&&node[i]) {
      auto values=*document_.find(node[i]);
      if(!values.components.find(scene::ChildOf::descriptor)&&!values.components.add(scene::ChildOf::descriptor))return cancel("Filho de recusado em '"+o.path+"'");
      if(!history_.applyValues(document_,node[i],values))return cancel("Filho de recusado em '"+o.path+"'");
      childOfComponent[node[i]]=document_.find(node[i])->components.find(scene::ChildOf::descriptor)->instanceId();++report.childOf;
    }
  }
  // Zero do plano: a perpendicular do UMotion (osso do meio no repouso, no
  // espaco do osso de cima da Unity) levada ao espaco do osso de cima da Astra.
  // Um angulo fixo entre as duas perpendiculares so valeria no repouso: com
  // eixos locais diferentes (FBX x GLB) o erro crescia com a pose.
  std::map<usize,std::array<float,3>> planeZero;const float mirrorSign=det3(fit.perm)<0?-1.f:1.f;
  for(usize i=0;i<count;++i) {
    const auto &o=objects[i];if(!o.ik||o.ikTarget<0)continue;
    // Cadeia: raiz `top` (ikChainLength acima da ponta) e o segundo osso `mid`.
    std::vector<i32> chainU{o.ikTarget};for(u32 k=0;k<o.ikChainLength&&chainU.back()>=0;++k)chainU.push_back(unityParent[chainU.back()]);
    if(chainU.back()<0)continue;
    const i32 tip=o.ikTarget,top=chainU.back(),mid=chainU[chainU.size()-2];
    if(mid<0||top<0||objects[top].path.empty())continue;
    namespace rp=runtime::rotate_plane;
    const auto perpendicular=[](const float v[3],float out[3]){const float a[3]{std::fabs(v[0]),std::fabs(v[1]),std::fabs(v[2])};
      if(a[2]>a[0]&&a[2]>a[1]){out[0]=-v[2];out[1]=0;out[2]=v[0];}else{out[0]=-v[1];out[1]=v[0];out[2]=0;}};
    // Unity: zero = rotacao do osso de cima * perp(posicao local do meio), no espaco da raiz.
    float midU[3]{objects[mid].position[0].reference,objects[mid].position[1].reference,objects[mid].position[2].reference},zU[3];perpendicular(midU,zU);
    const M4 topU=unityWorld[top];float dirU[3];for(int r=0;r<3;++r){float s=0;for(int c=0;c<3;++c)s+=topU.m[c*4+r]*zU[c];dirU[r]=s;}
    float axisU[3]{unityWorld[tip].m[12]-topU.m[12],unityWorld[tip].m[13]-topU.m[13],unityWorld[tip].m[14]-topU.m[14]};
    // Para o referencial F (so direcoes: P*v).
    rp::V zUF{},axisF{};
    for(int r=0;r<3;++r){float s=0,a=0;for(int c=0;c<3;++c){s+=fit.perm[r*3+c]*dirU[c];a+=fit.perm[r*3+c]*axisU[c];}(&zUF.x)[r]=s;(&axisF.x)[r]=a;}
    (void)axisF;
    // Espaco local do osso de cima da Astra: colunas normalizadas da rotacao.
    const M4 topA=astraInFrame(node[top]);std::array<float,3> zero{};
    for(int c=0;c<3;++c) {const rp::V column=rp::normalized(rp::V{topA.m[c*4],topA.m[c*4+1],topA.m[c*4+2]});zero[c]=rp::dot(column,rp::normalized(zUF));}
    planeZero[i]=zero;
  }
  for(usize i=0;i<count;++i) {
    const auto &o=objects[i];if(!o.ik||o.ikTarget<0)continue;
    auto values=*document_.find(node[o.ikTarget]);
    const float angle=mirrorSign*o.ikPlaneAngle;
    const float poleRotation=std::clamp(mirrorSign*o.poleRotation.reference,-360.f,360.f);
    const std::array<float,3> zero=planeZero.count(i)?planeZero[i]:std::array<float,3>{};
    if(auto *c=static_cast<scene::TwoBoneIk*>(values.components.edit(scene::TwoBoneIk::descriptor));c&&o.ikChainLength==2){
      c->planeAngle=std::remainder(angle,360.f);c->poleRotation=poleRotation;std::copy(zero.begin(),zero.end(),c->planeZero);}
    else if(auto *k=static_cast<scene::ChainIk*>(values.components.edit(scene::ChainIk::descriptor))){
      k->planeAngle=std::remainder(angle,360.f);k->poleRotation=poleRotation;std::copy(zero.begin(),zero.end(),k->planeZero);}
    if(!history_.applyValues(document_,node[o.ikTarget],values))return cancel("Plano do IK recusado");
  }
  history_.end();
  // ---- 5) Um clipe da Astra por clipe do projeto ----
  std::vector<AssetGuid> clipGuids;
  for(usize clipIndex=0;clipIndex<project.clips.size();++clipIndex) {
    const auto &uc=project.clips[clipIndex];const float rate=static_cast<float>(uc.framerate);
    AnimationClipAsset clip;
    std::string clipPath="Clipes/"+(uc.name.empty()?std::string("UMotion"):uc.name)+".aeclip";
    for(u32 n=2;files_.exists(clipPath)||assets_.findByPath(clipPath);++n)clipPath="Clipes/"+uc.name+" "+std::to_string(n)+".aeclip";
    clip.guid=assetGuidFromSeed("umotion:"+files_.rootPath()+":"+clipPath+":"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
    clip.name=clipPath.substr(7,clipPath.size()-7-7);clip.displayRate=static_cast<u32>(uc.framerate);
    clip.duration=std::max(1.f/rate,float(uc.end-uc.start)/rate);
    // Camadas acima da base.
    std::vector<u64> layerIds{0};
    for(const auto &l:uc.layers) {
      AnimationClipLayer layer;layer.id=clip.nextId++;layer.name=l.name.empty()?"Camada":l.name;
      layer.blend=l.overrideBlend?AnimationAuthorBlend::Override:AnimationAuthorBlend::Additive;layer.weight=std::clamp(l.weight,0.f,1.f);layer.muted=l.mute;
      clip.layers.push_back(layer);layerIds.push_back(layer.id);++report.layers;
    }
    std::map<EditorEntityId,u64> bindings;
    const auto bindingOf=[&](EditorEntityId id)->u64 {
      if(auto it=bindings.find(id);it!=bindings.end())return it->second;
      AnimationClipBinding b;b.id=clip.nextId++;std::string e;
      if(id!=owner&&!animationPoseTargetPath(document_,owner,id,b.path,e))return 0;
      b.name=document_.find(id)->name;clip.bindings.push_back(b);bindings[id]=b.id;return b.id;
    };
    // Pai ativo de um objeto com Filho de num quadro.
    const auto parentAt=[&](usize i,i32 frame,usize layer)->i32 {
      const auto &o=objects[i];if(!o.childOf)return unityParent[i];
      const std::vector<UMotionReferenceKey> *keys=clipIndex<o.childOfParent.clips.size()&&layer<o.childOfParent.clips[clipIndex].size()?&o.childOfParent.clips[clipIndex][layer]:nullptr;
      i32 parent=o.childOfParent.reference;
      if(keys)for(const auto &k:*keys)if(k.frame<=frame)parent=k.object;
      return parent>=0?parent:unityParent[i];
    };
    for(usize i=0;i<count;++i) {
      const auto &o=objects[i];if(o.path.empty()||!node[i])continue;
      for(usize layer=0;layer<layerIds.size();++layer) {
        const bool additive=layer>0&&!uc.layers[layer-1].overrideBlend;
        const auto mapAt=[&](i32 frame,LocalMap &m) {
          const i32 parent=parentAt(i,frame,layer);
          const EditorEntityId astraParent=o.childOf&&parent>=0&&parent!=unityParent[i]?node[parent]:document_.find(node[i])->parent;
          if(!mapFor(i,parent,astraParent,m))return false;
          if(additive) {
            m.offset[0]=m.offset[1]=m.offset[2]=0;m.rotation=m.additiveRotation;
            std::copy(m.additiveBefore,m.additiveBefore+4,m.before);std::copy(m.additiveAfter,m.additiveAfter+4,m.after);
          }
          return true;
        };
        // Troca de pai (Filho de): o UMotion avalia quadros inteiros, entao entre a
        // ultima chave no espaco antigo e a primeira no novo nao ha interpolacao.
        // Degrau no lado de saida da chave anterior reproduz isso em tempo continuo.
        const auto stepAcrossSwitches=[&](AnimationClipTrack &track) {
          if(!o.childOf)return;
          const bool progressive=track.path==AnimationPath::Rotation&&track.rotationMode==AnimationRotationMode::ProgressiveQuaternion;
          for(usize c=0;c<track.curves.size();++c) {
            if(progressive&&c<4)continue;
            auto &keys=track.curves[c].keys;
            for(usize n=0;n+1<keys.size();++n) {
              const i32 f0=uc.start+static_cast<i32>(std::lround(keys[n].time*rate)),f1=uc.start+static_cast<i32>(std::lround(keys[n+1].time*rate));
              if(parentAt(i,f0,layer)!=parentAt(i,f1,layer)){keys[n].outgoing=AnimationTangentMode::Constant;keys[n].broken=true;}
            }
          }
        };
        // Posicao: chaves unificadas entre x,y,z quando o mapa mistura eixos.
        {
          std::array<const std::vector<UMotionKey>*,3> k{};bool any=false;
          for(int c=0;c<3;++c){k[c]=o.position[c].keys(clipIndex,layer);any=any||(k[c]&&!k[c]->empty());}
          if(any) {
            std::set<i32> frames;for(int c=0;c<3;++c)if(k[c])for(const auto &x:*k[c])frames.insert(x.frame);
            AnimationClipTrack track;track.id=clip.nextId++;track.binding=bindingOf(node[i]);track.path=AnimationPath::Translation;track.layer=layerIds[layer];track.curves.resize(3);
            if(!track.binding)return fail(error,"Caminho de '"+o.path+"' fora do personagem");
            for(const i32 f:frames) {
              float v[3],sIn[3],sOut[3];UMotionKey shape;bool shapeSet=false;
              for(int c=0;c<3;++c) {
                const auto *list=k[c];const float fallback=additive?0.f:o.position[c].reference;
                v[c]=list&&!list->empty()?sampleUMotionChannel(*list,f,fallback):fallback;
                const auto exact=list?std::find_if(list->begin(),list->end(),[&](const UMotionKey &x){return x.frame==f;}):std::vector<UMotionKey>::const_iterator{};
                if(list&&exact!=list->end()){sIn[c]=exact->inTangent;sOut[c]=exact->outTangent;if(!shapeSet){shape=*exact;shapeSet=true;}}
                else if(list&&list->size()>1) {
                  const double h=.01;sIn[c]=sOut[c]=(sampleUMotionChannel(*list,f+h,fallback)-sampleUMotionChannel(*list,f-h,fallback))/float(2*h);
                } else sIn[c]=sOut[c]=0;
              }
              LocalMap m;if(!mapAt(f,m))return fail(error,"Referencial inválido para '"+o.path+"'");
              float out[3];convertPoint(m,v,out);
              for(int r=0;r<3;++r) {
                UMotionKey uk=shape;uk.frame=f;uk.value=out[r];
                uk.inTangent=m.position[r*3]*sIn[0]+m.position[r*3+1]*sIn[1]+m.position[r*3+2]*sIn[2];
                uk.outTangent=m.position[r*3]*sOut[0]+m.position[r*3+1]*sOut[1]+m.position[r*3+2]*sOut[2];
                track.curves[r].keys.push_back(key(uk,uc.start,rate,clip.nextId));
              }
            }
            stepAcrossSwitches(track);clip.tracks.push_back(std::move(track));++report.tracks;
          }
        }
        // Rotacao.
        {
          const auto mode=clipIndex<o.rotationMode.size()?o.rotationMode[clipIndex]:UMotionRotationMode::ProgressiveQuaternion;
          std::array<const std::vector<UMotionKey>*,5> k{};bool any=false;
          for(int c=0;c<5;++c){k[c]=o.rotation[c].keys(clipIndex,layer);if(c<4)any=any||(k[c]&&!k[c]->empty());}
          if(any) {
            AnimationClipTrack track;track.id=clip.nextId++;track.binding=bindingOf(node[i]);track.path=AnimationPath::Rotation;track.layer=layerIds[layer];
            if(!track.binding)return fail(error,"Caminho de '"+o.path+"' fora do personagem");
            std::set<i32> frames;for(int c=0;c<(mode==UMotionRotationMode::Euler?3:4);++c)if(k[c])for(const auto &x:*k[c])frames.insert(x.frame);
            // Euler com referencial fixo no clipe (sem troca de pai): trilha Euler ZXY
            // com as chaves, tangentes e pesos originais; o referencial absorve a
            // troca de espaco. Com troca de pai, o referencial muda no tempo e a
            // rotacao e amostrada por quadro.
            // Canal Euler sem chaves: o angulo da pose de referencia (guardada em
            // quaternion, decomposta como eulerAngles da Unity, ZXY).
            float referenceEuler[3]{};
            if(mode==UMotionRotationMode::Euler) {
              float q[4]{o.rotation[0].reference,o.rotation[1].reference,o.rotation[2].reference,o.rotation[3].reference};
              if(normalizeRotationQuaternion(q))rotationEuler(RotationEulerOrder::ZXY,q,referenceEuler);
              else for(int c=0;c<3;++c)referenceEuler[c]=o.rotation[c].reference;
              if(additive)std::fill(referenceEuler,referenceEuler+3,0.f);
            }
            bool fixedFrame=mode==UMotionRotationMode::Euler;LocalMap first;
            if(fixedFrame&&!mapAt(uc.start,first))return fail(error,"Referencial inválido para '"+o.path+"'");
            for(i32 f=uc.start;fixedFrame&&f<=uc.end;++f) {
              LocalMap m;if(!mapAt(f,m))return fail(error,"Referencial inválido para '"+o.path+"'");
              float a=0,b=0;for(int c=0;c<4;++c){a+=m.before[c]*first.before[c];b+=m.after[c]*first.after[c];}
              fixedFrame=std::fabs(std::fabs(a)-1)<1e-6f&&std::fabs(std::fabs(b)-1)<1e-6f;
            }
            if(fixedFrame) {
              track.rotationMode=AnimationRotationMode::Euler;track.curves.resize(3);
              track.euler.order=RotationEulerOrder::ZXY;
              std::copy(first.before,first.before+4,track.euler.before);std::copy(first.after,first.after+4,track.euler.after);
              normalizeRotationQuaternion(track.euler.before);normalizeRotationQuaternion(track.euler.after);
              for(int c=0;c<3;++c) {
                if(k[c]&&!k[c]->empty())for(const auto &x:*k[c])track.curves[c].keys.push_back(key(x,uc.start,rate,clip.nextId));
                else {UMotionKey uk;uk.frame=uc.start;uk.value=referenceEuler[c];uk.mode=0x122;track.curves[c].keys.push_back(key(uk,uc.start,rate,clip.nextId));}
              }
            } else if(mode==UMotionRotationMode::Euler) {
              // Euler (Z,X,Y) amostrado nos quadros do clipe e gravado como quaternion linear.
              track.rotationMode=AnimationRotationMode::Quaternion;track.curves.resize(4);
              for(i32 f=uc.start;f<=uc.end;++f) {
                float e[3];for(int c=0;c<3;++c) {const float fallback=referenceEuler[c];e[c]=k[c]&&!k[c]->empty()?sampleUMotionChannel(*k[c],f,fallback):fallback;}
                float qu[4],qa[4];unityEuler(e,qu);LocalMap m;if(!mapAt(f,m))return fail(error,"Referencial inválido para '"+o.path+"'");
                applyQ(m.rotation.k,qu,qa);normalizeRotationQuaternion(qa);
                for(int c=0;c<4;++c){UMotionKey uk;uk.frame=f;uk.value=qa[c];uk.mode=0x122;track.curves[c].keys.push_back(key(uk,uc.start,rate,clip.nextId));}
              }
            } else {
              const bool progressive=mode==UMotionRotationMode::ProgressiveQuaternion;
              track.rotationMode=progressive?AnimationRotationMode::ProgressiveQuaternion:AnimationRotationMode::Quaternion;
              track.curves.resize(progressive?5:4);
              float previous[4]{};double cumulative=0;i32 previousFrame=0;bool first=true;
              const std::vector<UMotionKey> *progress=progressive?k[4]:nullptr;
              for(const i32 f:frames) {
                float qu[4],sIn[4],sOut[4];UMotionKey shape;bool shapeSet=false;
                for(int c=0;c<4;++c) {
                  const auto *list=k[c];const float fallback=additive?(c==3?1.f:0.f):o.rotation[c].reference;
                  qu[c]=list&&!list->empty()?sampleUMotionChannel(*list,f,fallback):fallback;
                  const auto exact=list?std::find_if(list->begin(),list->end(),[&](const UMotionKey &x){return x.frame==f;}):std::vector<UMotionKey>::const_iterator{};
                  if(list&&exact!=list->end()){sIn[c]=exact->inTangent;sOut[c]=exact->outTangent;if(!shapeSet){shape=*exact;shapeSet=true;}}
                  else sIn[c]=sOut[c]=0;
                }
                LocalMap m;if(!mapAt(f,m))return fail(error,"Referencial inválido para '"+o.path+"'");
                float qa[4],ia[4],oa[4];applyQ(m.rotation.k,qu,qa);applyQ(m.rotation.k,sIn,ia);applyQ(m.rotation.k,sOut,oa);
                for(int c=0;c<4;++c) {
                  UMotionKey uk=shape;uk.frame=f;uk.value=qa[c];uk.inTangent=ia[c];uk.outTangent=oa[c];
                  if(progressive){uk.mode=0x022;uk.inTangent=uk.outTangent=0;} // componentes lineares do modo progressivo
                  track.curves[c].keys.push_back(key(uk,uc.start,rate,clip.nextId));
                }
                if(progressive) {
                  float qn[4]{qa[0],qa[1],qa[2],qa[3]};normalizeRotationQuaternion(qn);
                  const double travel=first?0.0:rotationDistanceDegrees(previous,qn);
                  // Progressao do UMotion: mesma forma da curva, escala da distancia convertida.
                  UMotionKey pk;pk.frame=f;pk.mode=0x022;
                  if(progress) {
                    const auto exact=std::find_if(progress->begin(),progress->end(),[&](const UMotionKey &x){return x.frame==f;});
                    if(exact!=progress->end())pk=*exact;
                  }
                  double sourceTravel=0;
                  if(!first&&progress&&!progress->empty())sourceTravel=sampleUMotionChannel(*progress,f,0)-sampleUMotionChannel(*progress,previousFrame,0);
                  const double ratio=sourceTravel>1e-6?travel/sourceTravel:1.0;
                  cumulative+=travel;pk.value=static_cast<float>(cumulative);pk.inTangent=static_cast<float>(pk.inTangent*ratio);pk.outTangent=static_cast<float>(pk.outTangent*ratio);
                  track.curves[4].keys.push_back(key(pk,uc.start,rate,clip.nextId));
                  std::copy(qn,qn+4,previous);previousFrame=f;first=false;
                }
              }
            }
            stepAcrossSwitches(track);clip.tracks.push_back(std::move(track));++report.tracks;
          }
        }
        // Escala.
        {
          std::array<const std::vector<UMotionKey>*,3> k{};bool any=false;
          for(int c=0;c<3;++c){k[c]=o.scale[c].keys(clipIndex,layer);any=any||(k[c]&&!k[c]->empty());}
          if(any) {
            LocalMap m;if(!mapAt(uc.start,m)||!m.scaleAligned)return fail(error,"Escala animada de '"+o.path+"' não passa pelo referencial do osso");
            AnimationClipTrack track;track.id=clip.nextId++;track.binding=bindingOf(node[i]);track.path=AnimationPath::Scale;track.layer=layerIds[layer];track.curves.resize(3);
            for(int c=0;c<3;++c) {
              const auto *list=k[m.permutation[c]];const float fallback=additive?0.f:o.scale[m.permutation[c]].reference;
              if(list&&!list->empty())for(auto uk:*list){uk.value*=m.scale;uk.inTangent*=m.scale;uk.outTangent*=m.scale;track.curves[c].keys.push_back(key(uk,uc.start,rate,clip.nextId));}
              else {UMotionKey uk;uk.frame=uc.start;uk.value=fallback*m.scale;track.curves[c].keys.push_back(key(uk,uc.start,rate,clip.nextId));}
            }
            stepAcrossSwitches(track);clip.tracks.push_back(std::move(track));++report.tracks;
          }
        }
        // Mistura FK/IK e rotacao do polo (no objeto da ponta do IK).
        if(o.ik&&o.ikTarget>=0&&layer==0) {
          const auto tipNode=node[o.ikTarget];const u64 instance=ikComponent.count(tipNode)?ikComponent[tipNode]:0;
          for(int which=0;which<2;++which) {
            const auto *list=(which==0?o.fkIkBlend:o.poleRotation).keys(clipIndex,0);if(!list||list->empty()||!instance)continue;
            AnimationPropertyTrack track;track.id=clip.nextId++;track.binding=bindingOf(tipNode);
            track.address.domain=AnimationPropertyDomain::Component;track.address.kind=AnimationPropertyKind::Number;track.address.component=instance;
            const auto &ikType=o.ikChainLength==2?scene::TwoBoneIk::descriptor:scene::ChainIk::descriptor;
            track.address.version=ikType.version;track.address.type=std::string(ikType.id);
            track.address.property=which==0?"weight":"pole_rotation";track.data.kind=AnimationPropertyKind::Number;track.data.curves.resize(1);
            for(auto uk:*list) {
              if(which==1){uk.value*=mirrorSign;uk.inTangent*=mirrorSign;uk.outTangent*=mirrorSign;}
              else uk.value=std::clamp(uk.value,0.f,1.f);
              track.data.curves[0].keys.push_back(key(uk,uc.start,rate,clip.nextId));
            }
            clip.properties.push_back(std::move(track));++report.propertyTracks;
          }
        }
        // Pai do Filho de (referencia discreta).
        if(o.childOf&&layer==0&&childOfComponent.count(node[i])) {
          const auto *keys=clipIndex<o.childOfParent.clips.size()&&!o.childOfParent.clips[clipIndex].empty()?&o.childOfParent.clips[clipIndex][0]:nullptr;
          if(keys&&!keys->empty()) {
            AnimationPropertyTrack track;track.id=clip.nextId++;track.binding=bindingOf(node[i]);
            track.address.domain=AnimationPropertyDomain::Component;track.address.kind=AnimationPropertyKind::Object;track.address.component=childOfComponent[node[i]];
            track.address.version=scene::ChildOf::descriptor.version;track.address.type=std::string(scene::ChildOf::descriptor.id);track.address.property="parent";
            track.data.kind=AnimationPropertyKind::Object;
            for(const auto &rk:*keys) {
              AnimationPropertyDiscreteKey dk;dk.id=clip.nextId++;dk.time=float(rk.frame-uc.start)/rate;
              dk.value=AnimationPropertyObject{rk.object>=0&&node[rk.object]?static_cast<u64>(node[rk.object]):0u};
              if(!track.data.keys.empty()&&track.data.keys.back().time==dk.time)track.data.keys.back()=dk;else track.data.keys.push_back(dk);
            }
            clip.properties.push_back(std::move(track));++report.propertyTracks;
          }
        }
      }
    }
    // Eventos do clipe como cues.
    for(const auto &event:uc.events)for(const auto &fn:event.functions) {
      AnimationCue cue;cue.id=clip.nextId++;cue.time=std::clamp(float(event.frame-uc.start)/rate,0.f,clip.duration);cue.kind=AnimationCueKind::Event;
      cue.name=fn.name;char *end=nullptr;const double v=std::strtod(fn.parameter.c_str(),&end);
      if(end&&*end==0&&!fn.parameter.empty())cue.value=v;else if(!fn.parameter.empty())cue.name+=":"+fn.parameter;
      clip.cues.push_back(cue);++report.cues;
    }
    // Chaves fora do intervalo do clipe: recorta com as curvas intactas no intervalo.
    float last=0;for(const auto &t:clip.tracks)for(const auto &c:t.curves)if(!c.keys.empty())last=std::max(last,c.keys.back().time);
    const float keep=clip.duration;clip.duration=std::max(keep,last);
    std::string diag;
    if(clip.duration>keep&&!clip.crop(0,keep,diag))return fail(error,"Clipe '"+uc.name+"': "+diag);
    if(!clip.valid(&diag))return fail(error,"Clipe '"+uc.name+"' inválido: "+diag);
    if(!createAnimationClipAsset(clip,clipPath,error))return false;
    clipGuids.push_back(clip.guid);report.clips.push_back(clipPath);
  }
  // ---- 6) Componente Animation no personagem ----
  if(!history_.begin("Clipes UMotion no personagem")) {error="Não foi possível abrir a edição";return false;}
  auto values=*document_.find(owner);
  auto *animation=static_cast<scene::Animation*>(values.components.find(scene::Animation::descriptor)?values.components.edit(scene::Animation::descriptor):values.components.add(scene::Animation::descriptor));
  if(!animation) {history_.cancel(document_);error="Componente Animation recusado";return false;}
  for(const auto guid:clipGuids)animation->appendClip(guid);
  animation->clip=clipGuids.front();animation->playAutomatically=true;
  if(!history_.applyValues(document_,owner,values)) {history_.cancel(document_);error="Animation recusado no personagem";return false;}
  // Outras animacoes do personagem (o take embutido de um FBX, por exemplo)
  // tocariam os mesmos ossos ao mesmo tempo: param de iniciar sozinhas. O
  // componente e os clipes ficam, so a reproducao automatica e desligada.
  {
    std::vector<EditorEntityId> subtree;document_.collectSubtree(owner,subtree);
    for(const auto id:subtree) {
      if(id==owner)continue;
      const auto *entity=document_.find(id);
      const auto *other=entity?static_cast<const scene::Animation*>(entity->components.find(scene::Animation::descriptor)):nullptr;
      if(!other||!other->playAutomatically)continue;
      auto silenced=*entity;static_cast<scene::Animation*>(silenced.components.edit(scene::Animation::descriptor))->playAutomatically=false;
      if(!history_.applyValues(document_,id,silenced)) {history_.cancel(document_);error="Não foi possível pausar a animação de '"+std::string(entity->name)+"'";return false;}
      report.silencedAnimations.push_back(entity->name);
    }
  }
  history_.end();
  report.bones=0;for(usize i=0;i<count;++i)if(node[i]&&!created[i]&&!objects[i].path.empty())++report.bones;
  state_.status="UMotion: "+std::to_string(report.clips.size())+" clipe(s), "+std::to_string(report.bones)+" ossos, "+std::to_string(report.ikChains)+" IK, "+std::to_string(report.childOf)+" Filho de";
  if(!report.silencedAnimations.empty())state_.status+=" · animação do modelo pausada ("+report.silencedAnimations.front()+")";
  return true;
}
} // namespace ae::editor
