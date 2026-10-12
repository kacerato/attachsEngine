#pragma once
// Implementacao do SceneIk: incluida so por scene_ik.cpp e testes do solver.
#include "runtime/scene_ik.h"
// IK em Play: pose amostrada -> mistura -> constraints -> IK -> pose final.
// Usa os mesmos solvers do estudio (runtime/ik_solvers.h). Peso combina com a
// pose de ENTRADA do quadro, guardada aqui, para nao acumular entre quadros.
#include "runtime/game_world.h"
#include "runtime/ik_solvers.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "scene/ik_constraints.h"
#include "resources/rig_asset.h"
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ae::runtime {
class SceneIkSolver : public SceneIkTypes {
public:
  const std::vector<Diagnostic> &diagnostics() const {return diagnostics_;}
  void reset() {limitRest_.clear();cache_.clear();diagnostics_.clear();candidates_.clear();knownRevision_=0;knownWorld_=0;}

  bool advance(GameWorld &world,const GroundQuery &ground={}) {
    diagnostics_.clear();if(!world.running())return false;
    if(knownWorld_!=world.worldId()) {reset();knownWorld_=world.worldId();}
    if(knownRevision_!=world.structuralRevision()) {
      candidates_.clear();std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
      for(const auto id:ids)if(const auto *o=world.graph().find(id))
        for(const auto *type:types())if(o->components.find(*type)) {candidates_.push_back(id);break;}
      knownRevision_=world.structuralRevision();
    }
    std::map<ObjectId,bool> touched;
    // Ordem fixa e documentada: corpo/pelve -> membros e pes -> mira -> olhar ->
    // dedos -> torcao -> limites. Cada etapa parte do resultado da anterior.
    grounderBodies(world,ground,touched);
    for(const auto id:candidates_) {
      const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
      if(const auto *c=static_cast<const scene::TwoBoneIk*>(o->components.find(scene::TwoBoneIk::descriptor));c&&c->enabled&&c->weight>0)
        twoBone(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::ChainIk*>(o->components.find(scene::ChainIk::descriptor));c&&c->enabled&&c->weight>0)
        chain(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::FootGrounding*>(o->components.find(scene::FootGrounding::descriptor));c&&c->enabled&&c->weight>0)
        grounding(world,id,*c,ground,touched);
    }
    const auto each=[&](const scene::ComponentType &type,const auto &run) {
      for(const auto id:candidates_) {
        const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
        if(const auto *c=o->components.find(type))run(id,*c);
      }
    };
    each(scene::AimIk::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::AimIk&>(v);if(c.enabled&&c.weight>0)aim(world,id,c,touched);});
    each(scene::LookAtIk::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::LookAtIk&>(v);if(c.enabled&&c.weight>0)lookAt(world,id,c,touched);});
    each(scene::FingerCurl::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::FingerCurl&>(v);if(c.enabled&&c.weight>0)finger(world,id,c,touched);});
    each(scene::TwistRelaxer::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::TwistRelaxer&>(v);if(c.enabled&&c.weight>0)twist(world,id,c,touched);});
    limits(world,touched);
    // Ossos que deixaram de ser escritos voltam a seguir a pose de entrada.
    for(auto it=cache_.begin();it!=cache_.end();) {
      if(!touched.count(it->first)) {
        const auto *o=world.graph().find(it->first);
        if(o&&same(o->transform,it->second.output))world.setLocalTransform(world.handle(it->first),it->second.input);
        it=cache_.erase(it);
      } else ++it;
    }
    return true;
  }

private:
  static std::array<const scene::ComponentType*,10> types() {
    return {&scene::TwoBoneIk::descriptor,&scene::ChainIk::descriptor,&scene::FootGrounding::descriptor,&scene::LookAtIk::descriptor,
            &scene::AimIk::descriptor,&scene::RotationLimit::descriptor,&scene::GrounderBody::descriptor,&scene::FingerCurl::descriptor,
            &scene::IkOffset::descriptor,&scene::TwistRelaxer::descriptor};
  }
  struct Cache {Transform input{},output{};};
  std::map<ObjectId,Cache> cache_;
  std::vector<Diagnostic> diagnostics_;
  std::vector<ObjectId> candidates_;
  u64 knownRevision_=0,knownWorld_=0;

  static bool same(const Transform &a,const Transform &b) {
    for(u32 i=0;i<3;++i)if(std::abs(a.position[i]-b.position[i])>1e-6f||std::abs(a.rotationDegrees[i]-b.rotationDegrees[i])>1e-4f||std::abs(a.scale[i]-b.scale[i])>1e-6f)return false;
    return true;
  }
  // Restaura a entrada do quadro: se o osso ainda tem o que o IK escreveu, a
  // animacao nao o tocou neste quadro, entao a entrada guardada continua valendo.
  void beginBone(GameWorld &world,ObjectId id,std::map<ObjectId,bool> &touched) {
    if(touched.count(id))return;
    const auto *o=world.graph().find(id);if(!o)return;
    auto found=cache_.find(id);
    if(found!=cache_.end()&&same(o->transform,found->second.output))world.setLocalTransform(world.handle(id),found->second.input);
    else cache_[id].input=o->transform;
    touched[id]=true;
  }
  bool writeBone(GameWorld &world,ObjectId id,const float solvedWorld[16],float weight) {
    float parent[16];Transform solved;
    if(!parentWorldMatrix(world.graph(),id,parent)||!localTransformForWorld(solvedWorld,parent,solved))return false;
    // Rotacao combinada a partir da entrada; posicao/escala atuais (ex.: pelve
    // ja descida pelo grounder neste quadro) sao preservadas.
    auto &cache=cache_[id];Transform result=world.graph().find(id)->transform;
    float a[4],b[4],q[4];transformRotationQuaternion(cache.input,a);transformRotationQuaternion(solved,b);
    if(!interpolateRotationArc(a,b,weight,q)||!rotationEulerXYZ(q,result.rotationDegrees))return false;
    if(world.setLocalTransform(world.handle(id),result)!=WorldStatus::Ok)return false;
    cache.output=world.graph().find(id)->transform;return true;
  }
  bool targetPosition(GameWorld &world,u64 target,float out[3]) {
    if(!target||!world.graph().find(static_cast<ObjectId>(target))||!world.graph().activeInHierarchy(static_cast<ObjectId>(target)))return false;
    float m[16];if(!worldMatrix(world.graph(),static_cast<ObjectId>(target),m))return false;
    out[0]=m[12];out[1]=m[13];out[2]=m[14];return true;
  }
  bool solveLimb(GameWorld &world,ObjectId end,const float target[3],const float *pole,float weight,std::map<ObjectId,bool> &touched,float *endWorldOut=nullptr) {
    const auto *e=world.graph().find(end);const auto *m=e?world.graph().find(e->parent):nullptr;const auto *r=m?world.graph().find(m->parent):nullptr;
    if(!r||m->id==world.graph().root()||r->id==world.graph().root()) {diagnostics_.push_back({end,Issue::ShortChain});return false;}
    beginBone(world,r->id,touched);beginBone(world,m->id,touched);
    float root[16],mid[16],tip[16];
    if(!worldMatrix(world.graph(),r->id,root)||!worldMatrix(world.graph(),m->id,mid)||!worldMatrix(world.graph(),end,tip)) {diagnostics_.push_back({end,Issue::InvalidPose});return false;}
    // Peso: alvo interpolado entre a ponta de entrada e o alvo; uma unica solucao.
    const float goal[3]{tip[12]+(target[0]-tip[12])*weight,tip[13]+(target[1]-tip[13])*weight,tip[14]+(target[2]-tip[14])*weight};
    TwoBoneIkResult solved;
    if(!solveTwoBoneIk(root,mid,tip,goal,solved,pole)) {diagnostics_.push_back({end,Issue::Degenerate});return false;}
    // Raiz primeiro; o meio e escrito relativo ao pai ja resolvido.
    if(!writeBone(world,r->id,solved.root,1)||!writeBone(world,m->id,solved.mid,1)) {diagnostics_.push_back({end,Issue::InvalidPose});return false;}
    if(solved.clamped)diagnostics_.push_back({end,Issue::NotConverged,0});
    if(endWorldOut)worldMatrix(world.graph(),end,endWorldOut);
    return true;
  }
  void twoBone(GameWorld &world,ObjectId id,const scene::TwoBoneIk &c,std::map<ObjectId,bool> &touched) {
    float target[3],pole[3];
    if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    applyOffset(world.graph(),id,target);
    const bool hasPole=c.pole&&targetPosition(world,c.pole,pole);
    if(!solveLimb(world,id,target,hasPole?pole:nullptr,c.weight,touched))return;
    if(c.matchTargetRotation) {
      beginBone(world,id,touched);
      float targetWorld[16],tipWorld[16];
      if(worldMatrix(world.graph(),static_cast<ObjectId>(c.target),targetWorld)&&worldMatrix(world.graph(),id,tipWorld)) {
        for(u32 i=12;i<15;++i)targetWorld[i]=tipWorld[i];
        if(!writeBone(world,id,targetWorld,c.weight))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
  }
  void chain(GameWorld &world,ObjectId id,const scene::ChainIk &c,std::map<ObjectId,bool> &touched) {
    float target[3];if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    applyOffset(world.graph(),id,target);
    const u32 bones=static_cast<u32>(c.chainLength);
    std::vector<ObjectId> joints{id};
    for(u32 i=0;i<bones;++i) {
      const auto *o=world.graph().find(joints.back());
      if(!o||o->parent==world.graph().root()||!world.graph().find(o->parent)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
      joints.push_back(o->parent);
    }
    std::reverse(joints.begin(),joints.end());
    for(usize i=0;i+1<joints.size();++i)beginBone(world,joints[i],touched);
    std::vector<std::array<float,16>> worlds(joints.size()),solved;
    for(usize i=0;i<joints.size();++i)if(!worldMatrix(world.graph(),joints[i],worlds[i].data())) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    const float goal[3]{worlds.back()[12]+(target[0]-worlds.back()[12])*c.weight,worlds.back()[13]+(target[1]-worlds.back()[13])*c.weight,worlds.back()[14]+(target[2]-worlds.back()[14])*c.weight};
    ChainIkResult result;
    if(!solveChainIk(c.solver==1?ChainIkSolver::Fabrik:ChainIkSolver::Ccd,worlds,goal,static_cast<u32>(c.iterations),c.tolerance,solved,result)) {diagnostics_.push_back({id,Issue::Degenerate});return;}
    for(usize i=0;i+1<joints.size();++i)if(!writeBone(world,joints[i],solved[i].data(),1)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    if(!result.converged)diagnostics_.push_back({id,Issue::NotConverged,result.residual});
  }
  void grounding(GameWorld &world,ObjectId id,const scene::FootGrounding &c,const GroundQuery &ground,std::map<ObjectId,bool> &touched) {
    if(!ground) {diagnostics_.push_back({id,Issue::NoGround});return;}
    float foot[16];if(!worldMatrix(world.graph(),id,foot)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    ObjectId owner=id;for(const auto *o=world.graph().find(id);o&&o->parent!=world.graph().root();o=world.graph().find(o->parent))owner=o->parent;
    const float origin[3]{foot[12],foot[13]+c.rayHeight,foot[14]},direction[3]{0,-(c.rayHeight+c.rayDistance),0};
    float point[3],normal[3];
    if(!ground(origin,direction,owner,point,normal)) {diagnostics_.push_back({id,Issue::NoGround});return;}
    const float desired=point[1]+c.footOffset,correction=std::clamp(desired-foot[13],-c.maxCorrection,c.maxCorrection);
    const float target[3]{foot[12],foot[13]+correction,foot[14]};
    float footAfter[16];
    if(!solveLimb(world,id,target,nullptr,c.weight,touched,footAfter))return;
    if(c.alignToSurface) {
      // Gira o pe para o "cima" dele seguir a normal da superficie.
      beginBone(world,id,touched);
      float up[3]{footAfter[4],footAfter[5],footAfter[6]};const float n=std::sqrt(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
      if(n>1e-6f) {
        for(float &v:up)v/=n;
        float axis[3]{up[1]*normal[2]-up[2]*normal[1],up[2]*normal[0]-up[0]*normal[2],up[0]*normal[1]-up[1]*normal[0]};
        const float s=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]),co=std::clamp(up[0]*normal[0]+up[1]*normal[1]+up[2]*normal[2],-1.f,1.f);
        if(s>1e-6f) {
          for(float &v:axis)v/=s;
          const float angle=std::atan2(s,co),cs=std::cos(angle),sn=std::sin(angle),t=1-cs;
          const float x=axis[0],y=axis[1],z=axis[2];
          const float r[9]{t*x*x+cs,t*x*y-sn*z,t*x*z+sn*y,t*x*y+sn*z,t*y*y+cs,t*y*z-sn*x,t*x*z-sn*y,t*y*z+sn*x,t*z*z+cs};
          float aligned[16];std::copy(footAfter,footAfter+16,aligned);
          for(u32 col=0;col<3;++col) {const float v0=footAfter[col*4],v1=footAfter[col*4+1],v2=footAfter[col*4+2];
            aligned[col*4]=r[0]*v0+r[1]*v1+r[2]*v2;aligned[col*4+1]=r[3]*v0+r[4]*v1+r[5]*v2;aligned[col*4+2]=r[6]*v0+r[7]*v1+r[8]*v2;}
          if(!writeBone(world,id,aligned,c.weight))diagnostics_.push_back({id,Issue::InvalidPose});
        }
      }
    }
  }
  // ---- Pacote A ----
  std::map<ObjectId,Transform> limitRest_;
  static const scene::IkOffset *offsetOf(const SceneGraph &graph,ObjectId id) {
    const auto *o=graph.find(id);if(!o)return nullptr;
    const auto *c=static_cast<const scene::IkOffset*>(o->components.find(scene::IkOffset::descriptor));
    return c&&c->enabled?c:nullptr;
  }
  static void applyOffset(const SceneGraph &graph,ObjectId id,float target[3]) {
    if(const auto *c=offsetOf(graph,id))for(u32 i=0;i<3;++i)target[i]+=c->offset[i];
  }
  // Escreve a pose mundial completa (posicao e rotacao) de um osso.
  bool writeWorld(GameWorld &world,ObjectId id,const float solvedWorld[16]) {
    float parent[16];Transform local;
    if(!parentWorldMatrix(world.graph(),id,parent)||!localTransformForWorld(solvedWorld,parent,local))return false;
    if(world.setLocalTransform(world.handle(id),local)!=WorldStatus::Ok)return false;
    cache_[id].output=world.graph().find(id)->transform;return true;
  }
  bool writeLocal(GameWorld &world,ObjectId id,const Transform &local) {
    if(world.setLocalTransform(world.handle(id),local)!=WorldStatus::Ok)return false;
    cache_[id].output=world.graph().find(id)->transform;return true;
  }
  // Ossos acima de `tip` (count ossos), da base para a ponta, sem a raiz da cena.
  bool ancestors(const SceneGraph &graph,ObjectId tip,u32 count,std::vector<ObjectId> &out) {
    out.clear();ObjectId at=tip;
    for(u32 i=0;i<count;++i) {
      const auto *o=graph.find(at);if(!o||o->parent==graph.root()||!graph.find(o->parent))return false;
      at=o->parent;out.push_back(at);
    }
    std::reverse(out.begin(),out.end());return true;
  }
  void grounderBodies(GameWorld &world,const GroundQuery &ground,std::map<ObjectId,bool> &touched) {
    for(const auto id:candidates_) {
      const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
      const auto *g=static_cast<const scene::GrounderBody*>(o->components.find(scene::GrounderBody::descriptor));
      if(!g||!g->enabled||g->weight<=0)continue;
      if(!ground) {diagnostics_.push_back({id,Issue::NoGround});continue;}
      const ObjectId pelvis=static_cast<ObjectId>(g->pelvis);
      if(!pelvis||!world.graph().find(pelvis)) {diagnostics_.push_back({id,Issue::MissingTarget});continue;}
      // Correcao de cada pe (negativa = o chao esta abaixo do pe).
      std::vector<ObjectId> subtree;world.graph().collectSubtree(id,subtree);float lowest=0;bool any=false;
      for(const auto foot:subtree) {
        const auto *f=world.graph().find(foot);if(!f)continue;
        const auto *fg=static_cast<const scene::FootGrounding*>(f->components.find(scene::FootGrounding::descriptor));
        if(!fg||!fg->enabled)continue;
        float m[16];if(!worldMatrix(world.graph(),foot,m))continue;
        const float origin[3]{m[12],m[13]+fg->rayHeight,m[14]},direction[3]{0,-(fg->rayHeight+fg->rayDistance),0};
        float point[3],normal[3];if(!ground(origin,direction,id,point,normal))continue;
        const float correction=std::clamp(point[1]+fg->footOffset-m[13],-fg->maxCorrection,fg->maxCorrection);
        lowest=any?std::min(lowest,correction):correction;any=true;
      }
      if(!any||lowest>=0)continue;
      // A pelve desce o necessario para o pe mais baixo alcancar o chao.
      beginBone(world,pelvis,touched);
      float m[16];if(!worldMatrix(world.graph(),pelvis,m))continue;
      m[13]+=std::max(lowest,-g->maxPelvisOffset)*g->weight;
      if(!writeWorld(world,pelvis,m))diagnostics_.push_back({id,Issue::InvalidPose});
    }
  }
  void aim(GameWorld &world,ObjectId id,const scene::AimIk &c,std::map<ObjectId,bool> &touched) {
    float target[3];if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    applyOffset(world.graph(),id,target);
    std::vector<ObjectId> bones;
    if(!ancestors(world.graph(),id,static_cast<u32>(c.chainLength),bones)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    for(const auto b:bones)beginBone(world,b,touched);
    const u32 iterations=static_cast<u32>(c.iterations);
    for(u32 it=0;it<iterations;++it)for(usize k=bones.size();k-->0;) {
      // Peso maior no osso proximo da ponta; a base recebe (1-falloff).
      const float t=bones.size()>1?float(k)/float(bones.size()-1):1.f;
      const float boneWeight=c.weight*(1-c.falloff*(1-t));
      float tip[16],bone[16];if(!worldMatrix(world.graph(),id,tip)||!worldMatrix(world.graph(),bones[k],bone))return;
      float axis[3];ikAxisWorld(tip,c.axis,axis);
      const float desired[3]{target[0]-tip[12],target[1]-tip[13],target[2]-tip[14]};
      float r[9];ikWeightedFromTo(axis,desired,boneWeight,-1,r);(void)it;
      float next[16];ikRotateWorld(bone,r,next);
      if(!writeWorld(world,bones[k],next)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    }
  }
  void lookAt(GameWorld &world,ObjectId id,const scene::LookAtIk &c,std::map<ObjectId,bool> &touched) {
    float target[3];if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    std::vector<ObjectId> chain;
    const u32 body=static_cast<u32>(c.chainLength)>1?static_cast<u32>(c.chainLength)-1:0;
    if(!ancestors(world.graph(),id,body,chain)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    chain.push_back(id);
    for(const auto b:chain)beginBone(world,b,touched);
    for(usize k=0;k<chain.size();++k) {
      const bool head=k+1==chain.size();
      const float share=head?c.headWeight:c.bodyWeight/float(std::max<usize>(1,chain.size()-1));
      float headWorld[16],bone[16];if(!worldMatrix(world.graph(),id,headWorld)||!worldMatrix(world.graph(),chain[k],bone))return;
      float forward[3];ikAxisWorld(headWorld,c.axis,forward);
      const float desired[3]{target[0]-headWorld[12],target[1]-headWorld[13],target[2]-headWorld[14]};
      // O limite vale para o giro total da cabeca; cada osso usa sua parte.
      const float dotValue=forward[0]*desired[0]+forward[1]*desired[1]+forward[2]*desired[2];
      const float len=std::sqrt(desired[0]*desired[0]+desired[1]*desired[1]+desired[2]*desired[2]);
      if(len<1e-6f)return;
      const float angle=std::acos(std::clamp(dotValue/len,-1.f,1.f))*180/3.14159265f;
      const float allowed=angle>c.clampAngle&&angle>0?c.clampAngle/angle:1.f;
      float r[9];ikWeightedFromTo(forward,desired,std::clamp(share*c.weight*allowed,0.f,1.f),-1,r);
      float next[16];ikRotateWorld(bone,r,next);
      if(!writeWorld(world,chain[k],next)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    }
  }
  void finger(GameWorld &world,ObjectId id,const scene::FingerCurl &c,std::map<ObjectId,bool> &touched) {
    ObjectId at=id;
    for(u32 i=0;i<static_cast<u32>(c.chainLength)&&at;++i) {
      beginBone(world,at,touched);
      Transform local=cache_[at].input;
      // Giro no eixo local de dobra, somado a pose de entrada.
      const float angle=c.curl*c.maxAngle*c.weight;float delta[3]{};delta[c.axis%3]=(c.axis>=3?-angle:angle);
      float qIn[4],qDelta[4],q[4];transformRotationQuaternion(local,qIn);rotationQuaternionXYZ(delta,qDelta);
      q[0]=qIn[3]*qDelta[0]+qIn[0]*qDelta[3]+qIn[1]*qDelta[2]-qIn[2]*qDelta[1];
      q[1]=qIn[3]*qDelta[1]-qIn[0]*qDelta[2]+qIn[1]*qDelta[3]+qIn[2]*qDelta[0];
      q[2]=qIn[3]*qDelta[2]+qIn[0]*qDelta[1]-qIn[1]*qDelta[0]+qIn[2]*qDelta[3];
      q[3]=qIn[3]*qDelta[3]-qIn[0]*qDelta[0]-qIn[1]*qDelta[1]-qIn[2]*qDelta[2];
      if(!rotationEulerXYZ(q,local.rotationDegrees)||!writeLocal(world,at,local)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
      const auto children=world.graph().childrenOf(at);at=children.empty()?0:children.front();
    }
  }
  void twist(GameWorld &world,ObjectId id,const scene::TwistRelaxer &c,std::map<ObjectId,bool> &touched) {
    const auto children=world.graph().childrenOf(id);if(children.empty()) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    const ObjectId child=children.front();
    beginBone(world,id,touched);beginBone(world,child,touched);
    Transform childLocal=world.graph().find(child)->transform,local=world.graph().find(id)->transform;
    float axis[3]{childLocal.position[0],childLocal.position[1],childLocal.position[2]};
    const float n=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);if(n<1e-6f)return;
    for(float &v:axis)v/=n;
    float qc[4];transformRotationQuaternion(childLocal,qc);
    // Torcao do filho em torno do eixo do osso (decomposicao swing-twist).
    const float d=qc[0]*axis[0]+qc[1]*axis[1]+qc[2]*axis[2];
    float tw[4]{axis[0]*d,axis[1]*d,axis[2]*d,qc[3]};
    if(!normalizeRotationQuaternion(tw))return;
    const float identity[4]{0,0,0,1};float part[4];
    if(!interpolateRotationArc(identity,tw,c.weight,part))return;
    float qm[4],qmNew[4],qcNew[4];transformRotationQuaternion(local,qm);
    const auto mul=[](const float a[4],const float b[4],float o[4]) {
      o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
      o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];};
    const float inverse[4]{-part[0],-part[1],-part[2],part[3]};
    mul(qm,part,qmNew);mul(inverse,qc,qcNew);
    if(!rotationEulerXYZ(qmNew,local.rotationDegrees)||!rotationEulerXYZ(qcNew,childLocal.rotationDegrees)||
       !writeLocal(world,id,local)||!writeLocal(world,child,childLocal))diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void limits(GameWorld &world,const std::map<ObjectId,bool> &touched) {
    for(const auto &[id,flag]:touched) {
      (void)flag;const auto *o=world.graph().find(id);if(!o)continue;
      const auto *c=static_cast<const scene::RotationLimit*>(o->components.find(scene::RotationLimit::descriptor));
      if(!c||!c->enabled||c->kind==0)continue;
      auto rest=limitRest_.find(id);
      if(rest==limitRest_.end())rest=limitRest_.emplace(id,cache_[id].input).first;
      resources::RigAngularLimit limit;
      limit.kind=c->kind==1?resources::RigLimitKind::Hinge:resources::RigLimitKind::ConeTwist;
      limit.axis={c->axis==0?1.f:0.f,c->axis==1?1.f:0.f,c->axis==2?1.f:0.f};
      limit.minimum=c->minimum;limit.maximum=c->maximum;limit.cone=c->cone;
      float ref[4],now[4];transformRotationQuaternion(rest->second,ref);Transform local=o->transform;transformRotationQuaternion(local,now);
      std::array<float,4> reference{ref[0],ref[1],ref[2],ref[3]},rotation{now[0],now[1],now[2],now[3]};
      if(!resources::constrainRigRotation(limit,reference,rotation))continue;
      const float q[4]{rotation[0],rotation[1],rotation[2],rotation[3]};
      if(rotationEulerXYZ(q,local.rotationDegrees))writeLocal(world,id,local);
    }
  }
};
} // namespace ae::runtime
