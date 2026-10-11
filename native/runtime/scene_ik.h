#pragma once
// IK em Play: pose amostrada -> mistura -> constraints -> IK -> pose final.
// Usa os mesmos solvers do estudio (runtime/ik_solvers.h). Peso combina com a
// pose de ENTRADA do quadro, guardada aqui, para nao acumular entre quadros.
#include "runtime/game_world.h"
#include "runtime/ik_solvers.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "scene/ik_constraints.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ae::runtime {
class SceneIk final {
public:
  enum class Issue { MissingTarget, ShortChain, Degenerate, NotConverged, NoGround, InvalidPose };
  struct Diagnostic {ObjectId object;Issue issue;float residual=0;};
  // Raio de apoio: origem, direcao com comprimento; devolve ponto e normal.
  using GroundQuery=std::function<bool(const float origin[3],const float direction[3],ObjectId ignore,float point[3],float normal[3])>;
  const std::vector<Diagnostic> &diagnostics() const {return diagnostics_;}
  static const char *issueText(Issue issue) {
    switch(issue) {
    case Issue::MissingTarget:return "Alvo do IK ausente ou inativo";
    case Issue::ShortChain:return "Cadeia mais curta que o pedido";
    case Issue::Degenerate:return "Osso sem comprimento na cadeia";
    case Issue::NotConverged:return "IK não convergiu na tolerância";
    case Issue::NoGround:return "Sem chão no alcance do raio";
    case Issue::InvalidPose:return "Pose resultante inválida";
    }
    return "IK";
  }
  void reset() {cache_.clear();diagnostics_.clear();candidates_.clear();knownRevision_=0;knownWorld_=0;}

  bool advance(GameWorld &world,const GroundQuery &ground={}) {
    diagnostics_.clear();if(!world.running())return false;
    if(knownWorld_!=world.worldId()) {reset();knownWorld_=world.worldId();}
    if(knownRevision_!=world.structuralRevision()) {
      candidates_.clear();std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
      for(const auto id:ids)if(const auto *o=world.graph().find(id))
        if(o->components.find(scene::TwoBoneIk::descriptor)||o->components.find(scene::ChainIk::descriptor)||o->components.find(scene::FootGrounding::descriptor))candidates_.push_back(id);
      knownRevision_=world.structuralRevision();
    }
    std::map<ObjectId,bool> touched;
    for(const auto id:candidates_) {
      const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
      if(const auto *c=static_cast<const scene::TwoBoneIk*>(o->components.find(scene::TwoBoneIk::descriptor));c&&c->enabled&&c->weight>0)
        twoBone(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::ChainIk*>(o->components.find(scene::ChainIk::descriptor));c&&c->enabled&&c->weight>0)
        chain(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::FootGrounding*>(o->components.find(scene::FootGrounding::descriptor));c&&c->enabled&&c->weight>0)
        grounding(world,id,*c,ground,touched);
    }
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
    auto &cache=cache_[id];Transform result=cache.input;
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
};
} // namespace ae::runtime
