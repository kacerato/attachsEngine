#pragma once
// Implementacao do SceneIk: incluida so por scene_ik.cpp e testes do solver.
#include "runtime/scene_ik.h"
// IK em Play: pose amostrada -> mistura -> constraints -> IK -> pose final.
// Usa os mesmos solvers do estudio (runtime/ik_solvers.h). Peso combina com a
// pose de ENTRADA do quadro, guardada aqui, para nao acumular entre quadros.
#include "runtime/game_world.h"
#include "runtime/ik_solvers.h"
#include "runtime/ik_rotate_plane.h"
#include "runtime/transform_math.h"
#include "core/rotation_math.h"
#include "scene/ik_effects.h"
#include "resources/rig_asset.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ae::runtime {
class SceneIkSolver : public SceneIkTypes {
public:
  const std::vector<Diagnostic> &diagnostics() const {return diagnostics_;}
  void reset() {
    limitRest_.clear();cache_.clear();diagnostics_.clear();candidates_.clear();knownRevision_=0;knownWorld_=0;
    impulses_.clear();autoStarted_.clear();springs_.clear();tilts_.clear();ampRest_.clear();interactions_.clear();holds_.clear();events_.clear();lastTime_=-1;footLocks_.clear();
  }

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
    const double now=world.elapsedSeconds();
    dt_=lastTime_<0?1.f/60:static_cast<float>(std::clamp(now-lastTime_,0.0,.1));lastTime_=now;
    ground_=&ground;
    // Ordem por etapas, documentada: amplificador -> corpo por alvos (VRIK) ->
    // grounder -> corpo inteiro -> FABRIK em arvore -> membros e pes -> interacao -> poser -> mira -> olhar -> inclinacao ->
    // inercia -> golpes e recuos -> dedos -> torcao -> limites.
    // Cada etapa parte do resultado da anterior.
    // Dentro de cada etapa, Prioridade maior resolve primeiro (empate: ordem
    // da hierarquia); quem resolve depois tem a palavra final nos ossos comuns.
    const auto each=[&](const scene::ComponentType &type,const auto &run) {
      std::vector<std::pair<float,ObjectId>> ordered;
      for(const auto id:candidates_) {
        const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
        if(const auto *c=o->components.find(type))ordered.push_back({priorityOf(*c),id});
      }
      std::stable_sort(ordered.begin(),ordered.end(),[](const auto &x,const auto &y){return x.first>y.first;});
      for(const auto &[priority,id]:ordered) {(void)priority;run(id,*world.graph().find(id)->components.find(type));}
    };
    // Filho de antes do IK: alvos de IK presos a outro pai (ou fixos no
    // personagem) ja estao no lugar quando o IK os le.
    each(scene::ChildOf::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::ChildOf&>(v);if(c.enabled)childOf(world,id,c,touched);});
    each(scene::Amplifier::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::Amplifier&>(v);if(c.enabled&&c.weight>0)amplify(world,id,c,touched);});
    each(scene::BodyByTargets::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::BodyByTargets&>(v);if(c.enabled&&c.weight>0)bodyByTargets(world,id,c,touched);});
    grounderBodies(world,ground,touched);
    each(scene::FullBodyIk::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::FullBodyIk&>(v);if(c.enabled&&c.weight>0)fullBody(world,id,c,touched);});
    each(scene::FabrikRoot::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::FabrikRoot&>(v);if(c.enabled&&c.weight>0)fabrikRoot(world,id,c,touched);});
    // Carregar: o objeto sobe ao ponto de carregar e o corpo agacha/inclina
    // antes dos membros e pes, para o apoio dobrar os joelhos.
    carry(world,touched);
    std::vector<std::pair<float,ObjectId>> limbs;
    for(const auto id:candidates_) {
      const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
      float priority=0;
      if(const auto *c=o->components.find(scene::TwoBoneIk::descriptor))priority=std::max(priority,priorityOf(*c));
      if(const auto *c=o->components.find(scene::ChainIk::descriptor))priority=std::max(priority,priorityOf(*c));
      limbs.push_back({priority,id});
    }
    std::stable_sort(limbs.begin(),limbs.end(),[](const auto &x,const auto &y){return x.first>y.first;});
    for(const auto &[limbPriority,id]:limbs) {
      (void)limbPriority;const auto *o=world.graph().find(id);
      if(const auto *c=static_cast<const scene::TwoBoneIk*>(o->components.find(scene::TwoBoneIk::descriptor));c&&c->enabled&&c->weight>0)
        twoBone(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::ChainIk*>(o->components.find(scene::ChainIk::descriptor));c&&c->enabled&&c->weight>0)
        chain(world,id,*c,touched);
      if(const auto *c=static_cast<const scene::FootGrounding*>(o->components.find(scene::FootGrounding::descriptor));c&&c->enabled&&c->weight>0)
        grounding(world,id,*c,ground,touched);
    }
    // Interacoes sem script: comecam uma vez por Play.
    each(scene::InteractionSystem::descriptor,[&](ObjectId id,const scene::ComponentValue &v){
      const auto &c=static_cast<const scene::InteractionSystem&>(v);
      if(!c.enabled||!c.autoEffector||!c.autoObject||autoStarted_.count(id))return;
      autoStarted_.insert(id);bool started=false;
      interactionStart(world,id,static_cast<ObjectId>(c.autoEffector),static_cast<ObjectId>(c.autoObject),started);
      if(c.autoSecond)interactionStart(world,id,static_cast<ObjectId>(c.autoSecond),static_cast<ObjectId>(c.autoObject),started);
    });
    interactions(world,touched);
    each(scene::HandPoser::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::HandPoser&>(v);if(c.enabled&&c.weight>0)poser(world,id,c,touched);});
    each(scene::AimIk::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::AimIk&>(v);if(c.enabled&&c.weight>0)aim(world,id,c,touched);});
    each(scene::LookAtIk::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::LookAtIk&>(v);if(c.enabled&&c.weight>0)lookAt(world,id,c,touched);});
    each(scene::BodyTilt::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::BodyTilt&>(v);if(c.enabled&&c.weight>0)tilt(world,id,c,touched);});
    each(scene::Inertia::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::Inertia&>(v);if(c.enabled&&c.weight>0)inertia(world,id,c,touched);});
    applyImpulses(world,touched);
    each(scene::FingerCurl::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::FingerCurl&>(v);if(c.enabled&&c.weight>0)finger(world,id,c,touched);});
    each(scene::TwistRelaxer::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::TwistRelaxer&>(v);if(c.enabled&&c.weight>0)twist(world,id,c,touched);});
    limits(world,touched);
    // Encaixes por ultimo: seguem a pose final, inclusive o IK.
    // Filho de outra vez, com a pose final: o objeto preso a uma mao segue a
    // mao ja resolvida pelo IK.
    each(scene::ChildOf::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::ChildOf&>(v);if(c.enabled)childOf(world,id,c,touched);});
    each(scene::Socket::descriptor,[&](ObjectId id,const scene::ComponentValue &v){const auto &c=static_cast<const scene::Socket&>(v);if(c.enabled)socket(world,id,c);});
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
  static float priorityOf(const scene::ComponentValue &value) {
    for(const auto &n:value.type().numbers)if(n.id=="priority")return n.read(value);
    return 0;
  }
  static std::array<const scene::ComponentType*,23> types() {
    return {&scene::TwoBoneIk::descriptor,&scene::ChainIk::descriptor,&scene::FootGrounding::descriptor,&scene::LookAtIk::descriptor,
            &scene::AimIk::descriptor,&scene::RotationLimit::descriptor,&scene::GrounderBody::descriptor,&scene::FingerCurl::descriptor,
            &scene::IkOffset::descriptor,&scene::TwistRelaxer::descriptor,&scene::HitReaction::descriptor,&scene::Recoil::descriptor,
            &scene::Inertia::descriptor,&scene::Amplifier::descriptor,&scene::BodyTilt::descriptor,&scene::HandPoser::descriptor,
            &scene::PenetrationAvoidance::descriptor,&scene::InteractionSystem::descriptor,&scene::FullBodyIk::descriptor,
            &scene::FabrikRoot::descriptor,&scene::BodyByTargets::descriptor,&scene::Socket::descriptor,&scene::ChildOf::descriptor};
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
public:
  void captureRest(const SceneGraph &graph) {
    rest_.clear();std::vector<ObjectId> ids;graph.collectSubtree(graph.root(),ids);
    for(const auto id:ids)if(const auto *o=graph.find(id))rest_.emplace(id,o->transform);
  }
private:
  // Pose de repouso (autoria), capturada no inicio do Play antes da animacao.
  std::map<ObjectId,Transform> rest_;
  // Plano rotativo: a sequencia do solver do UMotion Pro 1.29 (zera a cadeia
  // na referencia, resolve no plano, gira pelo polo, mistura FK->IK por osso,
  // corrige o osso do meio com peso 1 e copia a rotacao do alvo). Tres ossos
  // usam a solucao analitica; cadeias maiores, o CCD restrito ao plano.
  struct RotatePlaneSettings {u64 target=0,pole=0;bool matchTargetRotation=false;u32 reference=0;float weight=1,planeAngle=0,poleRotation=0;const float *zero=nullptr,*targetOffset=nullptr;};
  void rotatePlane(GameWorld &world,const std::vector<ObjectId> &ids,const RotatePlaneSettings &c,std::map<ObjectId,bool> &touched) {
    namespace rp=rotate_plane;using rp::V;
    const auto &g=world.graph();const usize count=ids.size();const ObjectId tip=ids.back();
    if(count<2) {diagnostics_.push_back({tip,Issue::ShortChain});return;}
    float handle[16];
    if(!c.target||!g.find(static_cast<ObjectId>(c.target))||!worldMatrix(g,static_cast<ObjectId>(c.target),handle)) {diagnostics_.push_back({tip,Issue::MissingTarget});return;}
    for(const auto id:ids)beginBone(world,id,touched);
    std::vector<Transform> fk(count);for(usize i=0;i<count;++i)fk[i]=g.find(ids[i])->transform;
    // 1) Camada IK: parte do repouso (posicao, rotacao e escala) ou do FK.
    if(c.reference==1)for(const auto id:ids)if(auto it=rest_.find(id);it!=rest_.end())world.setLocalTransform(world.handle(id),it->second);
    if(!c.matchTargetRotation) {Transform t=g.find(tip)->transform;std::copy(fk[count-1].rotationDegrees,fk[count-1].rotationDegrees+3,t.rotationDegrees);world.setLocalTransform(world.handle(tip),t);}
    const V restMid=[&]{auto it=rest_.find(ids[1]);const auto &p=it!=rest_.end()?it->second.position:fk[1].position;return V{p[0],p[1],p[2]};}();
    const auto pos=[&](ObjectId id){float m[16];worldMatrix(g,id,m);return rp::position(m);};
    // Plano: eixo raiz->ponta; zero perpendicular ao segundo osso no repouso.
    const auto plane=[&](V &start,V &axis,V &dir) {
      float rw[16],tw[16];if(!worldMatrix(g,ids[0],rw)||!worldMatrix(g,tip,tw))return false;
      start=rp::position(rw);axis=rp::position(tw)-start;
      const bool explicitZero=c.zero&&(c.zero[0]!=0||c.zero[1]!=0||c.zero[2]!=0);
      dir=rp::direction(rw,explicitZero?rp::V{c.zero[0],c.zero[1],c.zero[2]}:rp::perpendicular(restMid));
      if(count>2)dir=rp::reject(dir,rp::normalized(axis));
      float r[9];rp::axisAngle(axis,c.planeAngle,r);dir=rp::apply(r,dir);return true;
    };
    const auto abandon=[&](Issue issue){for(usize i=0;i<count;++i)writeLocal(world,ids[i],fk[i]);diagnostics_.push_back({tip,issue});};
    V start,axis,dir;
    if(!plane(start,axis,dir)) {abandon(Issue::InvalidPose);return;}
    axis=rp::normalized(axis);dir=rp::normalized(rp::reject(dir,axis));
    const V binormal=rp::cross(axis,dir);
    const V goal=rp::position(handle),toGoal=goal-start;const float distance=rp::length(toGoal);
    if(rp::length(axis)<.5f||rp::length(dir)<.5f||distance<1e-6f) {abandon(Issue::Degenerate);return;}
    float q[9];
    if(count==3) {
      const V upper=pos(ids[1])-start,lower=pos(tip)-pos(ids[1]);
      const float along=(rp::dot(upper,upper)+distance*distance-rp::dot(lower,lower))/(2*distance);
      const float side=std::sqrt(std::max(0.f,rp::dot(upper,upper)-along*along));
      if(rp::lookDelta(dir*side+axis*along,upper,binormal,q))rotateBone(world,ids[0],q);
      const V reach=start+axis*distance-pos(ids[1]);
      if(rp::lookDelta(reach,pos(tip)-pos(ids[1]),binormal,q))rotateBone(world,ids[1],q);
    } else if(count>3) {
      const auto flat=[&](V v){return rp::reject(v,binormal);};
      const auto signedAngle=[&](V a,V b){return std::atan2(rp::dot(binormal,rp::cross(a,b)),rp::dot(a,b))*57.2957795f;};
      // Juntas retas recebem uma dobra minima para o CCD ter para onde girar.
      for(usize i=2;i<count;++i) {
        const V a=flat(pos(ids[i-1])-pos(ids[i-2])),b=flat(pos(ids[i])-pos(ids[i-1]));
        if(std::fabs(signedAngle(a,b))<1){rp::axisAngle(binormal,1,q);rotateBone(world,ids[i-1],q);}
      }
      const usize last=count-1;std::vector<float> weights(count,0.f);weights[0]=1;
      for(usize l=1;l<count;++l)weights[l]=float(last-l)/float(last);
      const V point=start+axis*distance;bool done=false;
      for(u32 iteration=0;iteration<150&&!done;++iteration)
        for(i32 k=static_cast<i32>(count)-2;k>=0;--k) {
          if(weights[k]>0) {
            const V bone=pos(ids[k]);float angle=signedAngle(flat(pos(tip))-flat(bone),point-bone);
            if(weights[k]<1&&iteration<40)angle*=weights[k];
            rp::axisAngle(binormal,angle,q);rotateBone(world,ids[k],q);
          }
          if(std::fabs(rp::length(pos(tip)-start)-distance)<.005f)done=true;
        }
      // Dobra do lado contrario ao plano: espelha a cadeia em torno do eixo.
      if(signedAngle(axis,flat(pos(ids[1]))-flat(start))<0) {
        V previous=axis;
        for(usize i=0;i+1<count;++i) {
          const V v=flat(pos(ids[i+1]))-flat(pos(ids[i]));
          rp::axisAngle(binormal,-2*signedAngle(previous,v),q);rotateBone(world,ids[i],q);
          previous=rp::normalized(pos(ids[i+1])-pos(ids[i]));
        }
      }
    }
    if(rp::fromTo(axis,rp::normalized(toGoal),q))rotateBone(world,ids[0],q);
    // 2) Polo: alvo de direcao gira o plano ate ele; sem alvo, Rotacao do polo.
    if(float pw[16];c.pole&&g.find(static_cast<ObjectId>(c.pole))&&worldMatrix(g,static_cast<ObjectId>(c.pole),pw)) {
      V s,a,d;
      if(plane(s,a,d)) {
        const V toPole=rp::reject(rp::position(pw)-s,rp::normalized(a));
        if(rp::fromTo(d,toPole,q))rotateBone(world,ids[0],q);
      }
    } else if(c.poleRotation!=0) {
      rp::axisAngle(pos(tip)-pos(ids[0]),c.poleRotation,q);rotateBone(world,ids[0],q);
    }
    // 3) Rotacao do alvo copiada do objeto alvo (na camada IK).
    if(c.matchTargetRotation) {
      // Rotacao do alvo no mundo -> rotacao local da ponta (escala e posicao da ponta mantidas).
      float parent[16],hq[4],pq[4];
      if(parentWorldMatrix(g,tip,parent)&&rp::basisQuaternion(handle,hq)&&rp::basisQuaternion(parent,pq)) {
        if(c.targetOffset&&(c.targetOffset[0]!=0||c.targetOffset[1]!=0||c.targetOffset[2]!=0)) {
          float oq[4],turned[4];rotationQuaternionXYZ(c.targetOffset,oq);rotationMultiply(hq,oq,turned);std::copy(turned,turned+4,hq);
        }
        const float pc[4]{-pq[0],-pq[1],-pq[2],pq[3]};float local[4];rotationMultiply(pc,hq,local);writeLocalRotation(world,tip,local);
      }
    }
    std::vector<Transform> ik(count);for(usize i=0;i<count;++i)ik[i]=g.find(ids[i])->transform;
    // 4) Saida: posicao/escala da animacao, rotacao misturada FK->IK.
    const float w=std::clamp(c.weight,0.f,1.f);
    for(usize i=0;i<count;++i) {
      Transform out=fk[i];float a[4],b[4],r[4];transformRotationQuaternion(fk[i],a);transformRotationQuaternion(ik[i],b);
      if(interpolateRotationArc(a,b,w,r))rotationEulerXYZ(r,out.rotationDegrees);
      writeLocal(world,ids[i],out);
    }
    // 5) Com peso total, o penultimo osso aponta a ponta para o alvo se isso aproximar.
    if(w>=1) {
      const ObjectId before=ids[count-2];const V tipNow=pos(tip),beforeNow=pos(before);
      const float distanceBefore=rp::length(tipNow-goal);const Transform keep=g.find(before)->transform;
      if(rp::fromTo(tipNow-beforeNow,goal-beforeNow,q)&&rotateBone(world,before,q)&&distanceBefore<rp::length(pos(tip)-goal))writeLocal(world,before,keep);
    }
  }
  void twoBone(GameWorld &world,ObjectId id,const scene::TwoBoneIk &c,std::map<ObjectId,bool> &touched) {
    if(c.solver==1) {
      const auto *e=world.graph().find(id);const auto *m=e?world.graph().find(e->parent):nullptr;const auto *r=m?world.graph().find(m->parent):nullptr;
      if(!r||m->id==world.graph().root()||r->id==world.graph().root()) {diagnostics_.push_back({id,Issue::ShortChain});return;}
      rotatePlane(world,{r->id,m->id,id},{c.target,c.pole,c.matchTargetRotation,c.reference,c.weight,c.planeAngle,c.poleRotation,c.planeZero,c.targetOffset},touched);return;
    }
    float target[3],pole[3];
    if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    applyOffset(world.graph(),id,target);
    avoidPenetration(world,id,2,target);
    const bool hasPole=c.pole&&targetPosition(world,c.pole,pole);
    if(!solveLimb(world,id,target,hasPole?pole:nullptr,c.weight,touched))return;
    if(c.matchTargetRotation) {
      beginBone(world,id,touched);
      float targetWorld[16],tipWorld[16];
      if(worldMatrix(world.graph(),static_cast<ObjectId>(c.target),targetWorld)&&worldMatrix(world.graph(),id,tipWorld)) {
        if(c.targetOffset[0]!=0||c.targetOffset[1]!=0||c.targetOffset[2]!=0) {
          Transform offset;std::copy(c.targetOffset,c.targetOffset+3,offset.rotationDegrees);
          float turn[16],turned[16];transformMatrix(offset,turn);multiplyMatrix(targetWorld,turn,turned);std::copy(turned,turned+16,targetWorld);
        }
        for(u32 i=12;i<15;++i)targetWorld[i]=tipWorld[i];
        if(!writeBone(world,id,targetWorld,c.weight))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
  }
  void chain(GameWorld &world,ObjectId id,const scene::ChainIk &c,std::map<ObjectId,bool> &touched) {
    if(c.solver==2) {
      std::vector<ObjectId> ids;
      if(!ancestors(world.graph(),id,static_cast<u32>(c.chainLength),ids)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
      ids.push_back(id);
      rotatePlane(world,ids,{c.target,c.pole,c.matchTargetRotation,c.reference,c.weight,c.planeAngle,c.poleRotation,c.planeZero,c.targetOffset},touched);return;
    }
    float target[3];if(!targetPosition(world,c.target,target)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    applyOffset(world.graph(),id,target);
    avoidPenetration(world,id,static_cast<u32>(c.chainLength),target);
    solveChainTo(world,id,static_cast<u32>(c.chainLength),target,c.weight,touched,c.solver==1?ChainIkSolver::Fabrik:ChainIkSolver::Ccd,static_cast<u32>(c.iterations),c.tolerance);
  }
  // Cadeia de N ossos acima da ponta ate o alvo (peso interpola o alvo).
  bool solveChainTo(GameWorld &world,ObjectId id,u32 bones,const float target[3],float weight,std::map<ObjectId,bool> &touched,
                    ChainIkSolver solver=ChainIkSolver::Fabrik,u32 iterations=16,float tolerance=.001f) {
    std::vector<ObjectId> joints{id};
    for(u32 i=0;i<bones;++i) {
      const auto *o=world.graph().find(joints.back());
      if(!o||o->parent==world.graph().root()||!world.graph().find(o->parent)) {diagnostics_.push_back({id,Issue::ShortChain});return false;}
      joints.push_back(o->parent);
    }
    std::reverse(joints.begin(),joints.end());
    for(usize i=0;i+1<joints.size();++i)beginBone(world,joints[i],touched);
    std::vector<std::array<float,16>> worlds(joints.size()),solved;
    for(usize i=0;i<joints.size();++i)if(!worldMatrix(world.graph(),joints[i],worlds[i].data())) {diagnostics_.push_back({id,Issue::InvalidPose});return false;}
    const float goal[3]{worlds.back()[12]+(target[0]-worlds.back()[12])*weight,worlds.back()[13]+(target[1]-worlds.back()[13])*weight,worlds.back()[14]+(target[2]-worlds.back()[14])*weight};
    ChainIkResult result;
    if(!solveChainIk(solver,worlds,goal,iterations,tolerance,solved,result)) {diagnostics_.push_back({id,Issue::Degenerate});return false;}
    for(usize i=0;i+1<joints.size();++i)if(!writeBone(world,joints[i],solved[i].data(),1)) {diagnostics_.push_back({id,Issue::InvalidPose});return false;}
    if(!result.converged)diagnostics_.push_back({id,Issue::NotConverged,result.residual});
    return true;
  }
  // Membro de N ossos: dois ossos usa o solver analitico; mais usa FABRIK.
  bool solveMember(GameWorld &world,ObjectId tip,u32 bones,const float target[3],float weight,std::map<ObjectId,bool> &touched) {
    return bones<=2?solveLimb(world,tip,target,nullptr,weight,touched):solveChainTo(world,tip,bones,target,weight,touched);
  }
  void grounding(GameWorld &world,ObjectId id,const scene::FootGrounding &c,const GroundQuery &ground,std::map<ObjectId,bool> &touched) {
    if(!ground) {diagnostics_.push_back({id,Issue::NoGround});return;}
    // A perna volta a pose animada antes de medir o pe (trava e apoio usam o pe animado).
    if(const auto *e=world.graph().find(id))if(const auto *m=world.graph().find(e->parent)) {
      if(m->id!=world.graph().root())beginBone(world,m->id,touched);
      if(m->parent!=world.graph().root()&&world.graph().find(m->parent))beginBone(world,m->parent,touched);
    }
    float foot[16];if(!worldMatrix(world.graph(),id,foot)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    ObjectId owner=id;for(const auto *o=world.graph().find(id);o&&o->parent!=world.graph().root();o=world.graph().find(o->parent))owner=o->parent;
    const float origin[3]{foot[12],foot[13]+c.rayHeight,foot[14]},direction[3]{0,-(c.rayHeight+c.rayDistance),0};
    float point[3],normal[3];
    if(!ground(origin,direction,owner,point,normal)) {diagnostics_.push_back({id,Issue::NoGround});return;}
    const float desired=point[1]+c.footOffset,correction=std::clamp(desired-foot[13],-c.maxCorrection,c.maxCorrection);
    float target[3]{foot[12],foot[13]+correction,foot[14]};
    if(c.lockFoot) {
      auto &lock=footLocks_[id];
      const bool contact=foot[13]-desired<=c.lockHeight;
      if(!contact)lock.locked=false;
      else if(!lock.locked) {lock.locked=true;lock.point[0]=target[0];lock.point[2]=target[2];}
      if(lock.locked) {
        // Preso alem da correcao maxima esticaria a perna: solta.
        if(std::hypot(lock.point[0]-foot[12],lock.point[2]-foot[14])>c.maxCorrection)lock.locked=false;
        else {target[0]=lock.point[0];target[2]=lock.point[2];}
      }
    } else footLocks_.erase(id);
    // Plano do joelho: frente do personagem; o sentido (para frente, como o
    // joelho humano, ou para tras, como a pata traseira) vem da pose. A dobra
    // lateral de um repouso em A nao e seguida: o joelho abria para o lado.
    float pole[3]{},*hint=nullptr;
    if(const auto *e=world.graph().find(id))if(const auto *m=world.graph().find(e->parent))if(m->parent!=world.graph().root()) {
      float rw[16],mw[16],ow[16];
      if(worldMatrix(world.graph(),m->parent,rw)&&worldMatrix(world.graph(),m->id,mw)&&worldMatrix(world.graph(),owner,ow)) {
        const float axis[3]{foot[12]-rw[12],foot[13]-rw[13],foot[14]-rw[14]};const float len=length3(axis);
        if(len>1e-4f) {
          const float rel[3]{mw[12]-rw[12],mw[13]-rw[13],mw[14]-rw[14]};const float t=(rel[0]*axis[0]+rel[1]*axis[1]+rel[2]*axis[2])/(len*len);
          float bend[3]{rel[0]-axis[0]*t,rel[1]-axis[1]*t,rel[2]-axis[2]*t};
          float forward[3]{ow[8],0,ow[10]};const float fl=length3(forward);
          if(fl>1e-5f) {
            for(float &v:forward)v/=fl;
            const float bl=length3(bend),along=bl>1e-6f?(bend[0]*forward[0]+bend[2]*forward[2])/bl:0;
            const float sign=bl>=.02f*len&&along<-.3f?-1.f:1.f;
            for(u32 k=0;k<3;++k)bend[k]=forward[k]*sign;
          }
          const float b=length3(bend);
          if(b>1e-5f) {for(u32 k=0;k<3;++k)pole[k]=(k==0?mw[12]:k==1?mw[13]:mw[14])+bend[k]/b*len;hint=pole;}
        }
      }
    }
    float footAfter[16];
    if(!solveLimb(world,id,target,hint,c.weight,touched,footAfter))return;
    if(c.alignToSurface) {
      // Inclina o pe o quanto a superficie se inclina em relacao ao chao plano
      // (cima do mundo -> normal). Nao usa o eixo local do osso: em rigs
      // importados ele nao e o "cima" do pe, e o pe girava sozinho no plano.
      beginBone(world,id,touched);
      float up[3]{0,1,0};const float n=1;
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
      // A pelve volta a pose animada ANTES de medir os pes; medir com a descida
      // do quadro anterior concluiria que nao precisa descer e a pelve
      // alternaria entre quadros.
      beginBone(world,pelvis,touched);
      // Correcao de cada pe (negativa = o chao esta abaixo do pe).
      std::vector<ObjectId> subtree;world.graph().collectSubtree(id,subtree);float lowest=0;bool any=false;
      for(const auto foot:subtree) {
        const auto *f=world.graph().find(foot);if(!f)continue;
        const auto *fg=static_cast<const scene::FootGrounding*>(f->components.find(scene::FootGrounding::descriptor));
        if(!fg||!fg->enabled)continue;
        // Mede o pe na pose animada do quadro: a perna ainda guarda o IK do
        // quadro anterior, e medir por ela faria a pelve alternar entre descer
        // e nao descer a cada quadro.
        if(const auto *shin=world.graph().find(f->parent);shin&&shin->id!=world.graph().root()) {
          beginBone(world,shin->id,touched);
          if(shin->parent!=world.graph().root()&&world.graph().find(shin->parent))beginBone(world,shin->parent,touched);
        }
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
  // ---- Pacote C: efeitos, interacao e corpo inteiro ----
public:
  WorldStatus hit(GameWorld &world,ObjectId id,const float direction[3],float force) {
    if(!world.running())return WorldStatus::NotRunning;
    const auto *c=component<scene::HitReaction>(world,id);if(!c)return WorldStatus::ComponentMissing;
    if(!std::isfinite(force)||!finite3(direction))return WorldStatus::InvalidArgument;
    if(!c->enabled||force<=0||length3(direction)<1e-6f)return WorldStatus::Ok;
    Impulse impulse;impulse.id=id;impulse.recoil=false;impulse.magnitude=force;impulse.duration=c->duration;
    const float n=length3(direction);for(u32 i=0;i<3;++i)impulse.direction[i]=direction[i]/n;
    impulses_.push_back(impulse);return WorldStatus::Ok;
  }
  WorldStatus fire(GameWorld &world,ObjectId id,float magnitude) {
    if(!world.running())return WorldStatus::NotRunning;
    const auto *c=component<scene::Recoil>(world,id);if(!c)return WorldStatus::ComponentMissing;
    if(!std::isfinite(magnitude))return WorldStatus::InvalidArgument;
    if(!c->enabled||magnitude<=0)return WorldStatus::Ok;
    Impulse impulse;impulse.id=id;impulse.recoil=true;impulse.magnitude=magnitude;impulse.duration=c->duration;
    impulses_.push_back(impulse);return WorldStatus::Ok;
  }
  WorldStatus hitFront(GameWorld &world,ObjectId id,float force) {
    if(!world.running())return WorldStatus::NotRunning;
    const auto *c=component<scene::HitReaction>(world,id);if(!c)return WorldStatus::ComponentMissing;
    float m[16];if(!worldMatrix(world.graph(),id,m))return WorldStatus::Rejected;
    float front[3];ikAxisWorld(m,c->axis,front);
    const float direction[3]{-front[0],-front[1],-front[2]};
    return hit(world,id,direction,force);
  }
  void collision(GameWorld &world,ObjectId id,const float normal[3]) {
    const auto *c=component<scene::HitReaction>(world,id);
    if(!c||!c->enabled||!c->reactToCollisions||!finite3(normal)||length3(normal)<1e-6f)return;
    // A normal do contato aponta para fora deste objeto: o golpe vem contra ela.
    const float direction[3]{-normal[0],-normal[1],-normal[2]};
    hit(world,id,direction,1);
  }
  WorldStatus interactionStart(GameWorld &world,ObjectId system,ObjectId effector,ObjectId object,bool &started) {
    started=false;
    if(!world.running())return WorldStatus::NotRunning;
    const auto *s=component<scene::InteractionSystem>(world,system);if(!s)return WorldStatus::ComponentMissing;
    if(!s->enabled)return WorldStatus::Ok;
    const auto &g=world.graph();
    std::vector<ObjectId> member;
    const bool limb=g.find(effector)&&g.isDescendantOf(effector,system)&&ancestors(g,effector,static_cast<u32>(s->limbBones),member);
    if(!limb||!component<scene::InteractionObject>(world,object)) {diagnostics_.push_back({system,Issue::NotInteractable});return WorldStatus::Ok;}
    for(auto it=interactions_.begin();it!=interactions_.end();++it)
      if(it->system==system&&it->effector==effector) {dropHeld(world,*it);interactions_.erase(it);break;}
    Interaction entry;entry.system=system;entry.effector=effector;entry.object=object;entry.originalParent=g.find(object)->parent;
    for(const auto &other:interactions_)entry.second=entry.second||(other.system==system&&other.object==object&&other.effector!=effector&&other.phase<2);
    interactions_.push_back(entry);started=true;return WorldStatus::Ok;
  }
  WorldStatus interactionStop(GameWorld &world,ObjectId system,ObjectId effector) {
    if(!world.running())return WorldStatus::NotRunning;
    if(!component<scene::InteractionSystem>(world,system))return WorldStatus::ComponentMissing;
    for(auto it=interactions_.begin();it!=interactions_.end();++it) {
      if(it->system!=system||it->effector!=effector)continue;
      dropHeld(world,*it);
      // Carregando com as duas maos: larga quando nenhuma mao segura mais.
      const ObjectId object=it->object;bool holding=false;
      for(const auto &other:interactions_)holding=holding||(&other!=&*it&&other.system==system&&other.object==object&&other.phase<2);
      if(!holding)endHold(world,system,object);
      if(it->phase==3) {events_.push_back({system,"released",effector,it->object});interactions_.erase(it);}
      else if(it->phase<2) {it->phase=2;it->time=0;it->releaseFrom=it->weight;}
      return WorldStatus::Ok;
    }
    return WorldStatus::Ok;
  }
  std::vector<Event> takeEvents() {std::vector<Event> out;out.swap(events_);return out;}
  void setBodyMover(BodyMover mover) {mover_=std::move(mover);}
private:
  struct Impulse {ObjectId id=0;bool recoil=false;float direction[3]{},magnitude=1,time=0,duration=.3f;};
  struct Spring {float position[3]{},velocity[3]{};};
  struct Tilt {float yaw=0,angle=0;};
  // phase: 0 alcancando, 1 segurando, 2 soltando, 3 carregando (pego, IK solto).
  // Objeto com corpo fisico: em vez de mudar de pai, vira cinematico e segue a
  // mao pela pose relativa capturada na pegada (physical); volta ao movimento
  // original ao soltar.
  struct Interaction {ObjectId system=0,effector=0,object=0,originalParent=0;u32 phase=0,originalMotion=2;float time=0,weight=0,releaseFrom=1;bool held=false,physical=false,second=false;float offset[16]{};};
  // Objeto carregado (Ponto de carregar): filho do personagem, sobe da pose
  // da pegada ate o ponto; as maos seguem presas as pegadas.
  struct Hold {ObjectId system=0,object=0,originalParent=0;u32 originalMotion=2;float time=0;bool physical=false;Transform start;};
  std::vector<Hold> holds_;
  void beginHold(GameWorld &world,ObjectId system,ObjectId effector,ObjectId object) {
    for(const auto &h:holds_)if(h.system==system&&h.object==object)return;
    Hold h;h.system=system;h.object=object;h.originalParent=world.graph().find(object)->parent;
    float systemWorld[16],objectWorld[16];
    if(!worldMatrix(world.graph(),system,systemWorld)||!worldMatrix(world.graph(),object,objectWorld)||!localTransformForWorld(objectWorld,systemWorld,h.start)) {diagnostics_.push_back({object,Issue::InvalidPose});return;}
    // A troca de pai e enfileirada (vale no fim do quadro): h.start ja e a
    // pose relativa ao personagem, aplicada so depois que o pai mudou.
    const auto status=world.setParent(world.handle(object),world.handle(system),std::numeric_limits<u32>::max(),ReparentPosePolicy::KeepWorld);
    if(status==WorldStatus::Ok){}
    else if(status==WorldStatus::TransformOwnedByPhysics&&mover_) {
      const auto body=world.findComponent(world.handle(object),"astra.physics.body");scene::ComponentPropertyValue motion;
      if(world.getProperty(body,"motion",motion)!=WorldStatus::Ok||!std::holds_alternative<u32>(motion)||
         world.setProperty(body,"motion",scene::ComponentPropertyValue{u32{1}})!=WorldStatus::Ok) {diagnostics_.push_back({object,Issue::NotInteractable});return;}
      h.originalMotion=std::get<u32>(motion);h.physical=true;
    } else {diagnostics_.push_back({object,Issue::NotInteractable});return;}
    holds_.push_back(h);events_.push_back({system,"picked",effector,object});
  }
  void endHold(GameWorld &world,ObjectId system,ObjectId object) {
    for(auto it=holds_.begin();it!=holds_.end();++it) {
      if(it->system!=system||it->object!=object)continue;
      if(it->physical) {
        const auto body=world.findComponent(world.handle(object),"astra.physics.body");
        if(world.setProperty(body,"motion",scene::ComponentPropertyValue{it->originalMotion})!=WorldStatus::Ok)diagnostics_.push_back({object,Issue::InvalidPose});
      } else {
        const auto parent=world.graph().find(it->originalParent)?it->originalParent:world.graph().root();
        if(world.setParent(world.handle(object),world.handle(parent),std::numeric_limits<u32>::max(),ReparentPosePolicy::KeepWorld)!=WorldStatus::Ok)diagnostics_.push_back({object,Issue::InvalidPose});
      }
      holds_.erase(it);return;
    }
  }
  // Ponto da pegada desta mao (segunda mao usa a pegada dela, se houver).
  bool gripPosition(GameWorld &world,const Interaction &e,const scene::InteractionObject &object,float out[3]) {
    const u64 chosen=e.second&&object.secondGrip?object.secondGrip:object.grip;
    const ObjectId gripId=chosen&&world.graph().find(static_cast<ObjectId>(chosen))?static_cast<ObjectId>(chosen):e.object;
    float m[16];if(!worldMatrix(world.graph(),gripId,m))return false;
    out[0]=m[12];out[1]=m[13];out[2]=m[14];return true;
  }
  void carry(GameWorld &world,std::map<ObjectId,bool> &touched) {
    // 1) Objetos carregados sobem ao ponto de carregar (pose do personagem).
    for(auto it=holds_.begin();it!=holds_.end();) {
      const auto *system=component<scene::InteractionSystem>(world,it->system);
      const auto *object=component<scene::InteractionObject>(world,it->object);
      if(!system||!object) {it=holds_.erase(it);continue;}
      if(!it->physical&&world.graph().find(it->object)->parent!=it->system) {++it;continue;}
      it->time+=dt_*system->speed;
      const float w=smooth(it->time/std::max(.01f,object->liftTime));
      Transform local=it->start,hold;float systemWorld[16],holdWorld[16];
      const ObjectId point=static_cast<ObjectId>(object->holdPoint);
      const bool frame=worldMatrix(world.graph(),it->system,systemWorld);
      if(frame&&point&&world.graph().find(point)&&worldMatrix(world.graph(),point,holdWorld)&&localTransformForWorld(holdWorld,systemWorld,hold))
        for(u32 k=0;k<3;++k)local.position[k]=it->start.position[k]+(hold.position[k]-it->start.position[k])*w;
      if(it->physical) {
        float m[16],desired[16];transformMatrix(local,m);
        if(frame) {
          multiplyMatrix(systemWorld,m,desired);
          Transform pose;const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float q[4];
          if(localTransformForWorld(desired,identity,pose)) {transformRotationQuaternion(pose,q);
            const float values[7]{pose.position[0],pose.position[1],pose.position[2],q[0],q[1],q[2],q[3]};
            if(!mover_(it->object,values))diagnostics_.push_back({it->object,Issue::InvalidPose});}
        }
      } else if(world.setLocalTransform(world.handle(it->object),local)!=WorldStatus::Ok)diagnostics_.push_back({it->object,Issue::InvalidPose});
      ++it;
    }
    // 2) Corpo: o tronco inclina para as pegadas e a pelve desce o que ainda
    // falta ao braco (limitado por Agachar ate).
    std::map<ObjectId,std::vector<std::pair<ObjectId,std::array<float,4>>>> bySystem;
    for(const auto &e:interactions_) {
      if(e.weight<=0||e.held)continue;
      const auto *object=component<scene::InteractionObject>(world,e.object);float target[3];
      if(object&&gripPosition(world,e,*object,target))bySystem[e.system].push_back({e.effector,{target[0],target[1],target[2],e.weight}});
    }
    for(auto &[id,effectors]:bySystem) {
      const auto *c=component<scene::InteractionSystem>(world,id);if(!c||!c->enabled)continue;
      const ObjectId pelvis=static_cast<ObjectId>(c->pelvis),chest=static_cast<ObjectId>(c->chest);
      float weight=0;for(const auto &[tip,t]:effectors)weight=std::max(weight,t[3]);
      if(pelvis&&world.graph().find(pelvis))beginBone(world,pelvis,touched);
      if(chest&&world.graph().find(chest)&&c->lean>0) {
        beginBone(world,chest,touched);
        float chestWorld[16],tips[3]{},goals[3]{};
        if(worldMatrix(world.graph(),chest,chestWorld)) {
          for(const auto &[tip,t]:effectors){float m[16];if(worldMatrix(world.graph(),tip,m))for(u32 k=0;k<3;++k){tips[k]+=m[12+k];goals[k]+=t[k];}}
          const float n=float(effectors.size());
          const float from[3]{tips[0]/n-chestWorld[12],tips[1]/n-chestWorld[13],tips[2]/n-chestWorld[14]};
          const float to[3]{goals[0]/n-chestWorld[12],goals[1]/n-chestWorld[13],goals[2]/n-chestWorld[14]};
          float r[9];ikWeightedFromTo(from,to,c->lean*weight,60,r);
          if(!rotateBone(world,chest,r))diagnostics_.push_back({id,Issue::InvalidPose});
        }
      }
      if(!pelvis||!world.graph().find(pelvis)||c->crouch<=0)continue;
      // O corpo desce junto: quando peito e pelve sao irmaos (Kyle: Hip e
      // Ribs sob Root), desce o ancestral comum, senao o tronco esticava.
      ObjectId body=pelvis;
      if(chest&&world.graph().find(chest)&&!world.graph().isDescendantOf(chest,pelvis))
        for(const auto *o=world.graph().find(pelvis);o&&o->parent!=world.graph().root()&&o->parent!=id;o=world.graph().find(o->parent))
          if(world.graph().isDescendantOf(chest,o->parent)){body=o->parent;break;}
      beginBone(world,body,touched);
      float down=0;
      for(const auto &[tip,t]:effectors) {
        std::vector<ObjectId> member;
        if(!ancestors(world.graph(),tip,static_cast<u32>(c->limbBones),member))continue;
        member.push_back(tip);float reach=0,root[16];
        if(!worldMatrix(world.graph(),member.front(),root))continue;
        for(usize k=0;k+1<member.size();++k){float a[16],b[16];if(worldMatrix(world.graph(),member[k],a)&&worldMatrix(world.graph(),member[k+1],b)){const float d[3]{b[12]-a[12],b[13]-a[13],b[14]-a[14]};reach+=length3(d);}}
        // Desce ate o alvo ficar ao alcance (so a componente vertical).
        const float dx=t[0]-root[12],dz=t[2]-root[14],dy=t[1]-root[13],flat=std::sqrt(dx*dx+dz*dz);
        reach*=.98f;if(flat>=reach||dy>=0)continue;
        const float need=-dy-std::sqrt(std::max(0.f,reach*reach-flat*flat));
        down=std::max(down,need*t[3]);
      }
      down=std::min(down,c->crouch);
      if(down>1e-5f) {float m[16];if(worldMatrix(world.graph(),body,m)){m[13]-=down;if(!writeWorld(world,body,m))diagnostics_.push_back({id,Issue::InvalidPose});}}
    }
  }
  BodyMover mover_;
  std::vector<Impulse> impulses_;
  std::set<ObjectId> autoStarted_;
  struct FootLock {bool locked=false;float point[3]{};};
  std::map<ObjectId,FootLock> footLocks_;
  std::map<ObjectId,Spring> springs_;
  std::map<ObjectId,Tilt> tilts_;
  std::map<ObjectId,Transform> ampRest_;
  std::vector<Interaction> interactions_;
  std::vector<Event> events_;
  double lastTime_=-1;float dt_=1.f/60;
  const GroundQuery *ground_=nullptr;

  template<class T> static const T *component(const GameWorld &world,ObjectId id) {
    const auto *o=world.graph().find(id);if(!o)return nullptr;
    return static_cast<const T*>(o->components.find(T::descriptor));
  }
  static bool finite3(const float v[3]) {return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
  static float length3(const float v[3]) {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
  static void cross3(const float a[3],const float b[3],float o[3]) {o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];}
  static void axisAngle(const float axisValue[3],float degrees,float r[9]) {
    r[0]=r[4]=r[8]=1;r[1]=r[2]=r[3]=r[5]=r[6]=r[7]=0;
    const float n=length3(axisValue);if(n<1e-6f||!std::isfinite(degrees))return;
    const float x=axisValue[0]/n,y=axisValue[1]/n,z=axisValue[2]/n,a=degrees*3.14159265f/180;
    const float cs=std::cos(a),sn=std::sin(a),t=1-cs;
    r[0]=t*x*x+cs;r[1]=t*x*y-sn*z;r[2]=t*x*z+sn*y;r[3]=t*x*y+sn*z;r[4]=t*y*y+cs;r[5]=t*y*z-sn*x;r[6]=t*x*z-sn*y;r[7]=t*y*z+sn*x;r[8]=t*z*z+cs;
  }
  static float smooth(float x) {x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
  static void quatMul(const float a[4],const float b[4],float o[4]) {
    o[0]=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];o[1]=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
    o[2]=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];o[3]=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];
  }
  // Gira o osso no mundo em torno da propria posicao e grava.
  // Rotacao no mundo aplicada a rotacao local como quaternion: posicao e
  // escala locais ficam intactas e nada e decomposto de uma matriz. Decompor a
  // cada passo transformava ruido de float em escala nao uniforme, que voltava
  // como entrada do quadro seguinte e crescia ate a pose ser recusada.
  bool rotateBone(GameWorld &world,ObjectId id,const float r[9]) {
    const auto *o=world.graph().find(id);float parent[16],pq[4];
    if(!o||!parentWorldMatrix(world.graph(),id,parent))return false;
    if(!rotate_plane::basisQuaternion(parent,pq)) {
      float bone[16],next[16];if(!worldMatrix(world.graph(),id,bone))return false;
      ikRotateWorld(bone,r,next);return writeWorld(world,id,next);
    }
    float rq[4],lq[4],a[4],b[4],c[4];rotate_plane::quaternion(r,rq);transformRotationQuaternion(o->transform,lq);
    const float pc[4]{-pq[0],-pq[1],-pq[2],pq[3]};
    rotationMultiply(pc,rq,a);rotationMultiply(a,pq,b);rotationMultiply(b,lq,c);
    return writeLocalRotation(world,id,c);
  }
  // Rotacao local nova, no ramo Euler mais proximo da atual.
  bool writeLocalRotation(GameWorld &world,ObjectId id,float q[4]) {
    const auto *o=world.graph().find(id);if(!o||!normalizeRotationQuaternion(q))return false;
    Transform t=o->transform;
    if(!rotationEulerXYZNear(q,o->transform.rotationDegrees,t.rotationDegrees)&&!rotationEulerXYZ(q,t.rotationDegrees))return false;
    return writeLocal(world,id,t);
  }
  ObjectId characterOf(const SceneGraph &g,ObjectId id) const {
    ObjectId owner=id;for(const auto *o=g.find(id);o&&o->parent!=g.root();o=g.find(o->parent))owner=o->parent;
    return owner;
  }

  // Canais locais do objeto (entrada do quadro) no espaco do pai escolhido.
  void childOf(GameWorld &world,ObjectId id,const scene::ChildOf &c,std::map<ObjectId,bool> &touched) {
    const auto &g=world.graph();
    const ObjectId parent=c.pinned?characterOf(g,id):static_cast<ObjectId>(c.parent);
    if(!parent)return;
    if(!g.find(parent)||!g.activeInHierarchy(parent)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    if(parent==id||g.isDescendantOf(parent,id)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    beginBone(world,id,touched);
    float parentWorld[16],hierarchy[16];
    if(!worldMatrix(g,parent,parentWorld)||!parentWorldMatrix(g,id,hierarchy))return;
    // Composto por partes (quaternion, razao de escala, ponto): sem decompor a
    // matriz pai*local, que recusava a pose com escala local grande (caixa de
    // 10 cm sob uma armadura em 0,01) e deixava o objeto parado no quadro.
    const auto &input=cache_[id].input;float pq[4],hq[4];
    const auto scaleOf=[](const float m[16]){return std::cbrt(std::fabs(
      m[0]*(m[5]*m[10]-m[6]*m[9])-m[4]*(m[1]*m[10]-m[2]*m[9])+m[8]*(m[1]*m[6]-m[2]*m[5])));};
    const float sp=scaleOf(parentWorld),sh=scaleOf(hierarchy);
    if(sp>1e-12f&&sh>1e-12f&&rotate_plane::basisQuaternion(parentWorld,pq)&&rotate_plane::basisQuaternion(hierarchy,hq)) {
      Transform out=input;float lq[4],wq[4],q[4];transformRotationQuaternion(input,lq);
      rotationMultiply(pq,lq,wq);const float hc[4]{-hq[0],-hq[1],-hq[2],hq[3]};rotationMultiply(hc,wq,q);
      if(normalizeRotationQuaternion(q)&&(rotationEulerXYZNear(q,input.rotationDegrees,out.rotationDegrees)||rotationEulerXYZ(q,out.rotationDegrees))) {
        float p[3];for(u32 r=0;r<3;++r)p[r]=parentWorld[12+r]+parentWorld[r]*input.position[0]+parentWorld[4+r]*input.position[1]+parentWorld[8+r]*input.position[2];
        // Ponto no espaco do pai da hierarquia: base ortonormal transposta / escala.
        float x[3][3];for(u32 c=0;c<3;++c)for(u32 r=0;r<3;++r)x[c][r]=hierarchy[c*4+r]/sh;
        const float d[3]{p[0]-hierarchy[12],p[1]-hierarchy[13],p[2]-hierarchy[14]};
        for(u32 c=0;c<3;++c)out.position[c]=(x[c][0]*d[0]+x[c][1]*d[1]+x[c][2]*d[2])/sh;
        for(u32 c=0;c<3;++c)out.scale[c]=input.scale[c]*sp/sh;
        if(writeLocal(world,id,out))return;
      }
    }
    float local[16],desired[16];
    transformMatrix(input,local);multiplyMatrix(parentWorld,local,desired);
    if(!writeWorld(world,id,desired))diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void socket(GameWorld &world,ObjectId id,const scene::Socket &c) {
    const ObjectId bone=static_cast<ObjectId>(c.bone);
    if(!bone||!world.graph().find(bone)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    if(world.graph().isDescendantOf(bone,id)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    float boneWorld[16],offset[16],desired[16],parent[16];
    if(!worldMatrix(world.graph(),bone,boneWorld))return;
    if(!c.followScale)for(u32 col=0;col<3;++col) {
      const float n=std::sqrt(boneWorld[col*4]*boneWorld[col*4]+boneWorld[col*4+1]*boneWorld[col*4+1]+boneWorld[col*4+2]*boneWorld[col*4+2]);
      if(n>1e-6f)for(u32 r=0;r<3;++r)boneWorld[col*4+r]/=n;
    }
    Transform local;for(u32 i=0;i<3;++i){local.position[i]=c.position[i];local.rotationDegrees[i]=c.rotation[i];}
    // A escala do acessorio continua a dele.
    const auto *self=world.graph().find(id);if(self)for(u32 i=0;i<3;++i)local.scale[i]=self->transform.scale[i];
    transformMatrix(local,offset);multiplyMatrix(boneWorld,offset,desired);
    Transform placed;
    if(!parentWorldMatrix(world.graph(),id,parent)||!localTransformForWorld(desired,parent,placed)||
       world.setLocalTransform(world.handle(id),placed)!=WorldStatus::Ok)diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void amplify(GameWorld &world,ObjectId id,const scene::Amplifier &c,std::map<ObjectId,bool> &touched) {
    beginBone(world,id,touched);
    auto rest=ampRest_.find(id);if(rest==ampRest_.end())rest=ampRest_.emplace(id,cache_[id].input).first;
    Transform local=world.graph().find(id)->transform;
    float qr[4],qn[4];transformRotationQuaternion(rest->second,qr);transformRotationQuaternion(local,qn);
    const float inverse[4]{-qr[0],-qr[1],-qr[2],qr[3]};float delta[4];quatMul(inverse,qn,delta);
    if(delta[3]<0)for(float &v:delta)v=-v;
    const float s=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);if(s<1e-6f)return;
    // Angulo da diferenca multiplicado; eixo mantido (extrapolacao do arco).
    const float angle=2*std::atan2(s,delta[3])*(1+c.amount*c.weight);
    const float hs=std::sin(angle/2),scaled[4]{delta[0]/s*hs,delta[1]/s*hs,delta[2]/s*hs,std::cos(angle/2)};
    float q[4];quatMul(qr,scaled,q);
    if(!rotationEulerXYZ(q,local.rotationDegrees)||!writeLocal(world,id,local))diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void fullBody(GameWorld &world,ObjectId id,const scene::FullBodyIk &c,std::map<ObjectId,bool> &touched) {
    const ObjectId pelvis=static_cast<ObjectId>(c.pelvis);
    if(!pelvis||!world.graph().find(pelvis)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    // A pelve volta a pose do quadro antes de medir, senao o puxao do quadro
    // anterior esconde o excesso e o corpo oscila.
    beginBone(world,pelvis,touched);
    // 1) Quanto cada alvo esta alem do alcance do membro (raiz->ponta).
    float pull[3]{};u32 count=0;
    struct Effector {ObjectId tip;float target[3];};std::vector<Effector> effectors;
    for(u32 i=0;i<4;++i) {
      const ObjectId tip=static_cast<ObjectId>(c.tips[i]);if(!tip||!c.targets[i])continue;
      float target[3];if(!targetPosition(world,c.targets[i],target)) {diagnostics_.push_back({tip,Issue::MissingTarget});continue;}
      applyOffset(world.graph(),tip,target);
      std::vector<ObjectId> member;
      if(!ancestors(world.graph(),tip,static_cast<u32>(c.limbBones),member)) {diagnostics_.push_back({tip,Issue::ShortChain});continue;}
      member.push_back(tip);
      float rw[16];if(!worldMatrix(world.graph(),member.front(),rw))continue;
      float reach=0;
      for(usize k=0;k+1<member.size();++k) {
        float a[16],b[16];if(!worldMatrix(world.graph(),member[k],a)||!worldMatrix(world.graph(),member[k+1],b))continue;
        const float d[3]{b[12]-a[12],b[13]-a[13],b[14]-a[14]};reach+=length3(d);
      }
      reach*=.999f;
      const float toTarget[3]{target[0]-rw[12],target[1]-rw[13],target[2]-rw[14]};const float d=length3(toTarget);
      if(d>reach&&d>1e-6f) {for(u32 k=0;k<3;++k)pull[k]+=toTarget[k]/d*(d-reach);++count;}
      effectors.push_back({tip,{target[0],target[1],target[2]}});
    }
    // 2) A pelve segue a media do excesso, limitada; depois os membros.
    if(count) {
      for(float &v:pull)v=v/float(count)*c.bodyPull*c.weight;
      const float n=length3(pull);if(n>c.maxBodyOffset&&n>0)for(float &v:pull)v*=c.maxBodyOffset/n;
      float m[16];if(worldMatrix(world.graph(),pelvis,m)) {
        m[12]+=pull[0];m[13]+=pull[1];m[14]+=pull[2];
        if(!writeWorld(world,pelvis,m))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
    // 3) Tronco: o peito gira para levar o centro das pontas ao centro dos alvos.
    const ObjectId chestId=static_cast<ObjectId>(c.chest);
    if(chestId&&world.graph().find(chestId)&&c.spineBend>0&&!effectors.empty()) {
      beginBone(world,chestId,touched);
      float chestWorld[16];
      if(worldMatrix(world.graph(),chestId,chestWorld)) {
        float tips[3]{},goals[3]{};
        for(const auto &e:effectors) {float t[16];worldMatrix(world.graph(),e.tip,t);for(u32 k=0;k<3;++k){tips[k]+=t[12+k];goals[k]+=e.target[k];}}
        const float n=float(effectors.size());
        const float from[3]{tips[0]/n-chestWorld[12],tips[1]/n-chestWorld[13],tips[2]/n-chestWorld[14]};
        const float to[3]{goals[0]/n-chestWorld[12],goals[1]/n-chestWorld[13],goals[2]/n-chestWorld[14]};
        float r[9];ikWeightedFromTo(from,to,c.spineBend*c.weight,60,r);
        if(!rotateBone(world,chestId,r))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
    for(const auto &e:effectors)solveMember(world,e.tip,static_cast<u32>(c.limbBones),e.target,c.weight,touched);
  }
  // A raiz se desloca (nao gira): cada cadeia comeca no filho da raiz, com o
  // deslocamento desse filho preservado; so os ossos abaixo dele giram.
  void fabrikRoot(GameWorld &world,ObjectId id,const scene::FabrikRoot &c,std::map<ObjectId,bool> &touched) {
    const auto &g=world.graph();
    struct P {float v[3];};
    struct Chain {std::vector<ObjectId> joints;std::vector<float> lengths;std::vector<P> points;float target[3],offset[3];float reach=0;};
    std::vector<Chain> chains;
    float rootWorld[16];if(!worldMatrix(g,id,rootWorld))return;
    const P rest{{rootWorld[12],rootWorld[13],rootWorld[14]}};
    for(u32 i=0;i<4;++i) {
      const ObjectId tip=static_cast<ObjectId>(c.tips[i]);if(!tip||!c.targets[i])continue;
      Chain chain;if(!targetPosition(world,c.targets[i],chain.target)) {diagnostics_.push_back({tip,Issue::MissingTarget});continue;}
      // Caminho do filho da raiz ate a ponta.
      bool found=false;
      for(ObjectId at=tip;at;) {
        const auto *o=g.find(at);if(!o)break;
        if(o->parent==id) {chain.joints.push_back(at);found=true;break;}
        chain.joints.push_back(at);at=o->parent!=g.root()?o->parent:0;
      }
      if(!found||chain.joints.size()<2) {diagnostics_.push_back({tip,Issue::ShortChain});continue;}
      std::reverse(chain.joints.begin(),chain.joints.end());
      for(const auto j:chain.joints) {float m[16];worldMatrix(g,j,m);chain.points.push_back({{m[12],m[13],m[14]}});}
      for(u32 k=0;k<3;++k)chain.offset[k]=chain.points[0].v[k]-rest.v[k];
      for(usize n=0;n+1<chain.points.size();++n) {
        const float d[3]{chain.points[n+1].v[0]-chain.points[n].v[0],chain.points[n+1].v[1]-chain.points[n].v[1],chain.points[n+1].v[2]-chain.points[n].v[2]};
        chain.lengths.push_back(length3(d));chain.reach+=chain.lengths.back();
      }
      chains.push_back(std::move(chain));
    }
    if(chains.empty()) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    beginBone(world,id,touched);
    for(const auto &chain:chains)for(usize k=0;k+1<chain.joints.size();++k)beginBone(world,chain.joints[k],touched);
    const auto place=[](P &moved,const P &anchor,float length) {
      float d[3]{moved.v[0]-anchor.v[0],moved.v[1]-anchor.v[1],moved.v[2]-anchor.v[2]};const float n=length3(d);
      if(n<1e-7f){d[0]=0;d[1]=1;d[2]=0;}else for(float &x:d)x/=n;
      for(u32 i=0;i<3;++i)moved.v[i]=anchor.v[i]+d[i]*length;
    };
    P root=rest;
    for(u32 it=0;it<static_cast<u32>(c.iterations);++it) {
      // Passe ponta->base em cada cadeia: onde a raiz ficaria para ela.
      P average{{0,0,0}};
      for(auto &chain:chains) {
        auto &p=chain.points;for(u32 i=0;i<3;++i)p.back().v[i]=chain.target[i];
        for(usize n=p.size()-1;n-->0;)place(p[n],p[n+1],chain.lengths[n]);
        for(u32 i=0;i<3;++i)average.v[i]+=(p[0].v[i]-chain.offset[i])/float(chains.size());
      }
      for(u32 i=0;i<3;++i)root.v[i]=average.v[i]+(rest.v[i]-average.v[i])*c.rootPin;
      // Passe base->ponta a partir da raiz comum.
      for(auto &chain:chains) {auto &p=chain.points;for(u32 i=0;i<3;++i)p[0].v[i]=root.v[i]+chain.offset[i];for(usize n=0;n+1<p.size();++n)place(p[n+1],p[n],chain.lengths[n]);}
    }
    // Correcao de alcance: o que ainda ficou alem do comprimento de alguma
    // cadeia puxa a raiz (media dos excessos), contida por Fixar raiz.
    for(u32 round=0;round<32&&c.rootPin<1;++round) {
      float pull[3]{};u32 count=0;
      for(const auto &chain:chains) {
        const float d[3]{chain.target[0]-root.v[0]-chain.offset[0],chain.target[1]-root.v[1]-chain.offset[1],chain.target[2]-root.v[2]-chain.offset[2]};const float n=length3(d);
        if(n>chain.reach*.9995f&&n>1e-6f) {for(u32 i=0;i<3;++i)pull[i]+=d[i]/n*(n-chain.reach*.9995f);++count;}
      }
      if(!count)break;
      for(u32 i=0;i<3;++i)root.v[i]+=pull[i]/float(count)*(1-c.rootPin);
    }
    for(u32 i=0;i<3;++i)rootWorld[12+i]+=(root.v[i]-rest.v[i])*c.weight;
    if(!writeWorld(world,id,rootWorld)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    for(const auto &chain:chains)
      solveChainTo(world,chain.joints.back(),static_cast<u32>(chain.joints.size()-1),chain.target,c.weight,touched,ChainIkSolver::Fabrik,static_cast<u32>(c.iterations)*2,.0005f);
  }
  void bodyByTargets(GameWorld &world,ObjectId id,const scene::BodyByTargets &c,std::map<ObjectId,bool> &touched) {
    const auto &g=world.graph();
    const ObjectId pelvis=static_cast<ObjectId>(c.pelvis),head=static_cast<ObjectId>(c.head),chest=static_cast<ObjectId>(c.chest);
    if(!pelvis||!head||!g.find(pelvis)||!g.find(head)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    float headGoal[3];const bool hasHead=c.headTarget&&targetPosition(world,c.headTarget,headGoal);
    // 1) Pelve acompanha a cabeca mantendo o vetor pelve->cabeca da pose.
    if(hasHead&&c.bodyFollow>0) {
      beginBone(world,pelvis,touched);
      float p[16],h[16];
      if(worldMatrix(g,pelvis,p)&&worldMatrix(g,head,h)) {
        float shift[3]{(headGoal[0]-h[12])*c.bodyFollow*c.weight,(headGoal[1]-h[13])*c.bodyFollow*c.weight,(headGoal[2]-h[14])*c.bodyFollow*c.weight};
        const float n=length3(shift);if(n>c.maxBodyOffset&&n>0)for(float &v:shift)v*=c.maxBodyOffset/n;
        for(u32 i=0;i<3;++i)p[12+i]+=shift[i];
        if(!writeWorld(world,pelvis,p))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
    // 2) Peito curva para a cabeca alcancar o alvo.
    if(hasHead&&chest&&g.find(chest)&&c.spineBend>0) {
      beginBone(world,chest,touched);
      float ch[16],h[16];
      if(worldMatrix(g,chest,ch)&&worldMatrix(g,head,h)) {
        const float from[3]{h[12]-ch[12],h[13]-ch[13],h[14]-ch[14]},to[3]{headGoal[0]-ch[12],headGoal[1]-ch[13],headGoal[2]-ch[14]};
        float r[9];ikWeightedFromTo(from,to,c.spineBend*c.weight,75,r);
        if(!rotateBone(world,chest,r))diagnostics_.push_back({id,Issue::InvalidPose});
      }
    }
    // 3) Cabeca copia a rotacao do alvo.
    if(hasHead&&c.headRotation) {
      beginBone(world,head,touched);
      float t[16],h[16];
      if(worldMatrix(g,static_cast<ObjectId>(c.headTarget),t)&&worldMatrix(g,head,h)) {
        for(u32 i=12;i<15;++i)t[i]=h[i];
        if(!writeBone(world,head,t,c.weight))diagnostics_.push_back({head,Issue::InvalidPose});
      }
    }
    // 4) Bracos ate os alvos das maos.
    const std::pair<u64,u64> hands[2]{{c.leftHand,c.leftTarget},{c.rightHand,c.rightTarget}};
    for(const auto &[hand,target]:hands) {
      if(!hand||!target)continue;
      float goal[3];if(!targetPosition(world,target,goal)) {diagnostics_.push_back({static_cast<ObjectId>(hand),Issue::MissingTarget});continue;}
      solveLimb(world,static_cast<ObjectId>(hand),goal,nullptr,c.weight,touched);
    }
  }
  void avoidPenetration(GameWorld &world,ObjectId tip,u32 bones,float target[3]) {
    const auto *c=component<scene::PenetrationAvoidance>(world,tip);
    if(!c||!c->enabled||!ground_||!*ground_)return;
    std::vector<ObjectId> chain;if(!ancestors(world.graph(),tip,std::max(1u,bones),chain))return;
    float base[16];if(!worldMatrix(world.graph(),chain.front(),base))return;
    const float origin[3]{base[12],base[13],base[14]},direction[3]{target[0]-base[12],target[1]-base[13],target[2]-base[14]};
    if(length3(direction)<1e-5f)return;
    float point[3],normal[3];
    if(!(*ground_)(origin,direction,characterOf(world.graph(),tip),point,normal))return;
    for(u32 i=0;i<3;++i)target[i]=point[i]+normal[i]*c->radius;
  }
  void dropHeld(GameWorld &world,Interaction &entry) {
    if(!entry.held)return;
    entry.held=false;
    if(entry.physical) {
      entry.physical=false;
      const auto body=world.findComponent(world.handle(entry.object),"astra.physics.body");
      if(world.setProperty(body,"motion",scene::ComponentPropertyValue{entry.originalMotion})!=WorldStatus::Ok)diagnostics_.push_back({entry.object,Issue::InvalidPose});
      return;
    }
    const auto parent=world.graph().find(entry.originalParent)?entry.originalParent:world.graph().root();
    if(world.setParent(world.handle(entry.object),world.handle(parent),std::numeric_limits<u32>::max(),ReparentPosePolicy::KeepWorld)!=WorldStatus::Ok)
      diagnostics_.push_back({entry.object,Issue::InvalidPose});
  }
  bool carryPhysical(GameWorld &world,Interaction &e) {
    const auto body=world.findComponent(world.handle(e.object),"astra.physics.body");
    scene::ComponentPropertyValue motion;
    if(world.getProperty(body,"motion",motion)!=WorldStatus::Ok||!std::holds_alternative<u32>(motion))return false;
    float hand[16],object[16],offset[16];Transform local;
    if(!worldMatrix(world.graph(),e.effector,hand)||!worldMatrix(world.graph(),e.object,object)||!localTransformForWorld(object,hand,local))return false;
    transformMatrix(local,offset);
    if(world.setProperty(body,"motion",scene::ComponentPropertyValue{u32{1}})!=WorldStatus::Ok)return false;
    e.originalMotion=std::get<u32>(motion);e.physical=true;std::copy(offset,offset+16,e.offset);return true;
  }
  void followHand(GameWorld &world,const Interaction &e) {
    float hand[16],desired[16];if(!worldMatrix(world.graph(),e.effector,hand))return;
    multiplyMatrix(hand,e.offset,desired);
    Transform pose;const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(!localTransformForWorld(desired,identity,pose))return;
    float q[4];transformRotationQuaternion(pose,q);
    const float values[7]{pose.position[0],pose.position[1],pose.position[2],q[0],q[1],q[2],q[3]};
    if(!mover_(e.object,values))diagnostics_.push_back({e.object,Issue::InvalidPose});
  }
  void interactions(GameWorld &world,std::map<ObjectId,bool> &touched) {
    for(auto it=interactions_.begin();it!=interactions_.end();) {
      auto &e=*it;
      const auto *system=component<scene::InteractionSystem>(world,e.system);
      const auto *object=component<scene::InteractionObject>(world,e.object);
      if(!system||!object||!world.graph().find(e.effector)) {it=interactions_.erase(it);continue;}
      const float step=dt_*system->speed;
      bool reachedNow=false;
      if(e.phase==0) {
        e.time+=step;e.weight=smooth(e.time/object->reachTime);
        if(e.time>=object->reachTime) {e.weight=1;reachedNow=true;}
      } else if(e.phase==2) {
        e.time+=step;e.weight=e.releaseFrom*(1-smooth(e.time/object->releaseTime));
        if(e.time>=object->releaseTime) {
          e.weight=0;
          if(e.held) e.phase=3;
          else {events_.push_back({e.system,"released",e.effector,e.object});it=interactions_.erase(it);continue;}
        }
      }
      if(e.held&&e.physical)followHand(world,e);
      if(e.weight>0&&!e.held) {
        float target[3];
        if(gripPosition(world,e,*object,target))solveMember(world,e.effector,static_cast<u32>(system->limbBones),target,e.weight,touched);
      }
      if(object->handPose&&e.weight>0)poseHierarchy(world,e.effector,static_cast<ObjectId>(object->handPose),e.weight,false,touched);
      // A pegada acontece depois da mao chegar neste quadro: o objeto e
      // capturado na pose final da mao, sem deslocamento residual.
      if(reachedNow) {
        events_.push_back({e.system,"reached",e.effector,e.object});
        const ObjectId point=static_cast<ObjectId>(object->holdPoint);
        if(point&&world.graph().find(point)) {
          // Carregar: as maos ficam nas pegadas; o objeto sobe quando todas chegaram.
          e.phase=1;bool all=true;
          for(const auto &other:interactions_)if(other.system==e.system&&other.object==e.object&&other.phase==0)all=false;
          if(all)beginHold(world,e.system,e.effector,e.object);
        } else if(object->pickUp) {
          const auto status=world.setParent(world.handle(e.object),world.handle(e.effector),std::numeric_limits<u32>::max(),ReparentPosePolicy::KeepWorld);
          if(status==WorldStatus::Ok) {e.held=true;e.phase=2;e.time=0;e.releaseFrom=1;}
          else if(status==WorldStatus::TransformOwnedByPhysics&&mover_&&carryPhysical(world,e)) {e.held=true;e.phase=2;e.time=0;e.releaseFrom=1;}
          else {diagnostics_.push_back({e.object,Issue::NotInteractable});e.phase=1;}
        } else e.phase=1;
      }
      ++it;
    }
  }
  void poseHierarchy(GameWorld &world,ObjectId target,ObjectId reference,float weight,bool includeSelf,std::map<ObjectId,bool> &touched,u32 depth=0) {
    const auto &g=world.graph();
    if(!g.find(target)||!g.find(reference)||depth>32)return;
    if(includeSelf||depth>0) {
      beginBone(world,target,touched);
      Transform local=g.find(target)->transform;
      float a[4],b[4],q[4];transformRotationQuaternion(local,a);transformRotationQuaternion(g.find(reference)->transform,b);
      if(interpolateRotationArc(a,b,weight,q)&&rotationEulerXYZ(q,local.rotationDegrees))writeLocal(world,target,local);
    }
    const auto mine=g.childrenOf(target),theirs=g.childrenOf(reference);
    // Filhos casados pela ordem; o objeto pego na mao nao faz parte da pose.
    usize k=0;
    for(const auto child:mine) {
      if(k>=theirs.size())break;
      bool carried=false;for(const auto &e:interactions_)carried=carried||(e.held&&e.object==child);
      if(carried)continue;
      poseHierarchy(world,child,theirs[k++],weight,true,touched,depth+1);
    }
  }
  void poser(GameWorld &world,ObjectId id,const scene::HandPoser &c,std::map<ObjectId,bool> &touched) {
    const ObjectId reference=static_cast<ObjectId>(c.reference);
    if(!reference||!world.graph().find(reference)) {diagnostics_.push_back({id,Issue::MissingTarget});return;}
    if(world.graph().childrenOf(id).size()!=world.graph().childrenOf(reference).size())diagnostics_.push_back({id,Issue::ShortChain});
    poseHierarchy(world,id,reference,c.weight,c.includeSelf,touched);
  }
  void tilt(GameWorld &world,ObjectId id,const scene::BodyTilt &c,std::map<ObjectId,bool> &touched) {
    const ObjectId owner=characterOf(world.graph(),id);
    float m[16];if(!worldMatrix(world.graph(),owner,m))return;
    float forward[3];ikAxisWorld(m,c.axis,forward);
    if(std::abs(forward[0])+std::abs(forward[2])<1e-5f)return;
    const float yaw=std::atan2(forward[0],forward[2])*180/3.14159265f;
    auto found=tilts_.find(id);
    if(found==tilts_.end()) {tilts_[id]={yaw,0};return;}
    float delta=yaw-found->second.yaw;while(delta>180)delta-=360;while(delta<-180)delta+=360;
    found->second.yaw=yaw;
    const float rate=dt_>0?delta/dt_:0;
    // Gira para dentro da curva: giro positivo no eixo Y inclina para o lado do giro.
    const float desired=std::clamp(rate*c.tiltPerTurn,-c.maxAngle,c.maxAngle)*c.weight;
    const float blend=c.smoothing>0?1-std::exp(-dt_/c.smoothing):1.f;
    found->second.angle+=(desired-found->second.angle)*blend;
    if(std::abs(found->second.angle)<1e-4f)return;
    beginBone(world,id,touched);
    float r[9];axisAngle(forward,-found->second.angle,r);
    if(!rotateBone(world,id,r))diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void inertia(GameWorld &world,ObjectId id,const scene::Inertia &c,std::map<ObjectId,bool> &touched) {
    const auto children=world.graph().childrenOf(id);if(children.empty()) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    beginBone(world,id,touched);
    float bone[16],tip[16];if(!worldMatrix(world.graph(),id,bone)||!worldMatrix(world.graph(),children.front(),tip))return;
    const float animated[3]{tip[12],tip[13],tip[14]};
    auto found=springs_.find(id);
    if(found==springs_.end()) {Spring s;for(u32 i=0;i<3;++i)s.position[i]=animated[i];springs_[id]=s;return;}
    auto &s=found->second;
    // Mola criticamente parametrizada: amortecimento 1 = critico.
    const float k=c.stiffness,damp=2*c.damping*std::sqrt(k);
    const u32 steps=std::max(1u,static_cast<u32>(std::ceil(dt_*240)));const float h=dt_/float(steps);
    for(u32 n=0;n<steps;++n)for(u32 i=0;i<3;++i) {
      const float a=k*(animated[i]-s.position[i])-damp*s.velocity[i];
      s.velocity[i]+=a*h;s.position[i]+=s.velocity[i]*h;
    }
    const float current[3]{animated[0]-bone[12],animated[1]-bone[13],animated[2]-bone[14]};
    const float lagged[3]{s.position[0]-bone[12],s.position[1]-bone[13],s.position[2]-bone[14]};
    float r[9];ikWeightedFromTo(current,lagged,c.weight,c.maxAngle,r);
    if(!rotateBone(world,id,r))diagnostics_.push_back({id,Issue::InvalidPose});
  }
  void applyImpulses(GameWorld &world,std::map<ObjectId,bool> &touched) {
    for(auto it=impulses_.begin();it!=impulses_.end();) {
      auto &p=*it;p.time+=dt_;
      if(p.time>=p.duration||!world.graph().find(p.id)) {it=impulses_.erase(it);continue;}
      // Envelope: subida curta (golpe 20%, recuo 10%) e volta suave.
      const float rise=p.recoil?.1f:.2f,x=p.time/p.duration;
      const float envelope=x<rise?smooth(x/rise):1-smooth((x-rise)/(1-rise));
      if(p.recoil) {
        const auto *c=component<scene::Recoil>(world,p.id);
        if(c&&c->enabled)recoilPose(world,p.id,*c,envelope*p.magnitude,touched);
      } else {
        const auto *c=component<scene::HitReaction>(world,p.id);
        if(c&&c->enabled)hitPose(world,p.id,*c,p.direction,envelope*p.magnitude,touched);
      }
      ++it;
    }
  }
  void hitPose(GameWorld &world,ObjectId id,const scene::HitReaction &c,const float direction[3],float amount,std::map<ObjectId,bool> &touched) {
    std::vector<ObjectId> chain;
    const u32 above=static_cast<u32>(c.chainLength)>1?static_cast<u32>(c.chainLength)-1:0;
    if(!ancestors(world.graph(),id,above,chain)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    chain.push_back(id);
    for(const auto b:chain)beginBone(world,b,touched);
    float hitPoint[16];if(!worldMatrix(world.graph(),id,hitPoint))return;
    for(usize k=0;k<chain.size();++k) {
      float bone[16];if(!worldMatrix(world.graph(),chain[k],bone))return;
      // O ponto atingido e empurrado na direcao do golpe; a base reage menos.
      float lever[3]{hitPoint[12]-bone[12],hitPoint[13]-bone[13],hitPoint[14]-bone[14]};
      if(length3(lever)<1e-5f) {float y[3];ikAxisWorld(bone,1,y);for(u32 i=0;i<3;++i)lever[i]=y[i];}
      float axis[3];cross3(lever,direction,axis);
      const float share=float(k+1)/float(chain.size());
      float r[9];axisAngle(axis,c.strength*c.weight*amount*share*float(chain.size())*2/float(chain.size()+1)/float(chain.size()),r);
      if(!rotateBone(world,chain[k],r)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
      worldMatrix(world.graph(),id,hitPoint);
    }
  }
  void recoilPose(GameWorld &world,ObjectId id,const scene::Recoil &c,float amount,std::map<ObjectId,bool> &touched) {
    std::vector<ObjectId> chain;
    if(!ancestors(world.graph(),id,static_cast<u32>(c.chainLength),chain)) {diagnostics_.push_back({id,Issue::ShortChain});return;}
    for(const auto b:chain)beginBone(world,b,touched);
    float tip[16];if(!worldMatrix(world.graph(),id,tip))return;
    float forward[3];ikAxisWorld(tip,c.axis,forward);
    const float up[3]{0,1,0};float side[3];cross3(forward,up,side);
    if(length3(side)<1e-4f)return;
    // Recua a base ao longo do cano e levanta a cadeia; o angulo se divide.
    float base[16];
    if(c.kickBack>0&&worldMatrix(world.graph(),chain.front(),base)) {
      for(u32 i=0;i<3;++i)base[12+i]-=forward[i]*c.kickBack*c.weight*amount;
      if(!writeWorld(world,chain.front(),base)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    }
    for(const auto b:chain) {
      float r[9];axisAngle(side,c.kickAngle*c.weight*amount/float(chain.size()),r);
      if(!rotateBone(world,b,r)) {diagnostics_.push_back({id,Issue::InvalidPose});return;}
    }
  }
};
} // namespace ae::runtime
