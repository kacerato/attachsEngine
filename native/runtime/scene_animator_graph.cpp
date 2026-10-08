#include "runtime/scene_animator_graph.h"
#include "runtime/scene_physics.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {
namespace animator_detail {

void blend1D(const std::vector<scene::AnimatorMotion> &motions,float value,std::vector<WeightedMotion> &out) {
  out.clear();
  if(motions.empty()) return;
  std::vector<u32> order(motions.size());
  for(u32 i=0;i<order.size();++i) order[i]=i;
  std::stable_sort(order.begin(),order.end(),[&](u32 a,u32 b){return motions[a].threshold<motions[b].threshold;});
  if(value<=motions[order.front()].threshold) {out.push_back({order.front(),1});return;}
  if(value>=motions[order.back()].threshold) {out.push_back({order.back(),1});return;}
  for(usize i=0;i+1<order.size();++i) {
    const float a=motions[order[i]].threshold,b=motions[order[i+1]].threshold;
    if(value>=a&&value<=b) {
      const float t=b>a?(value-a)/(b-a):0;
      if(t<1) out.push_back({order[i],1-t});
      if(t>0) out.push_back({order[i+1],t});
      return;
    }
  }
}

namespace {
struct P {float x,y;};
float cross(P o,P a,P b) {return (a.x-o.x)*(b.y-o.y)-(a.y-o.y)*(b.x-o.x);}
// Segmento a→b: parâmetro do ponto mais próximo de q, limitado a [0,1].
float projection(P a,P b,P q,float &distance) {
  const float dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
  const float t=length>1e-12f?std::clamp(((q.x-a.x)*dx+(q.y-a.y)*dy)/length,0.f,1.f):0;
  const float px=a.x+dx*t-q.x,py=a.y+dy*t-q.y;distance=px*px+py*py;return t;
}
} // namespace

void blend2D(const std::vector<scene::AnimatorMotion> &motions,float x,float y,std::vector<WeightedMotion> &out) {
  out.clear();
  // Pontos distintos (o primeiro de cada posição repetida vale).
  std::vector<u32> index;std::vector<P> points;
  for(u32 i=0;i<motions.size();++i) {
    const P p{motions[i].x,motions[i].y};bool repeated=false;
    for(const auto &q:points) repeated=repeated||(std::fabs(q.x-p.x)<1e-6f&&std::fabs(q.y-p.y)<1e-6f);
    if(!repeated) {index.push_back(i);points.push_back(p);}
  }
  const P q{x,y};
  if(points.empty()) return;
  if(points.size()==1) {out.push_back({index[0],1});return;}
  // Delaunay por força bruta (até 8 pontos): triângulo cujo círculo não contém outro ponto.
  struct Tri {u32 a,b,c;};std::vector<Tri> triangles;
  const usize n=points.size();
  for(u32 a=0;a<n;++a) for(u32 b=a+1;b<n;++b) for(u32 c=b+1;c<n;++c) {
    const P A=points[a],B=points[b],C=points[c];
    const float d=2*(A.x*(B.y-C.y)+B.x*(C.y-A.y)+C.x*(A.y-B.y));
    if(std::fabs(d)<1e-9f) continue;
    const float ux=((A.x*A.x+A.y*A.y)*(B.y-C.y)+(B.x*B.x+B.y*B.y)*(C.y-A.y)+(C.x*C.x+C.y*C.y)*(A.y-B.y))/d;
    const float uy=((A.x*A.x+A.y*A.y)*(C.x-B.x)+(B.x*B.x+B.y*B.y)*(A.x-C.x)+(C.x*C.x+C.y*C.y)*(B.x-A.x))/d;
    const float r=(A.x-ux)*(A.x-ux)+(A.y-uy)*(A.y-uy);
    bool empty=true;
    for(u32 k=0;k<n&&empty;++k) if(k!=a&&k!=b&&k!=c) {
      const float dk=(points[k].x-ux)*(points[k].x-ux)+(points[k].y-uy)*(points[k].y-uy);
      empty=dk>=r*(1-1e-5f);
    }
    if(empty) triangles.push_back({a,b,c});
  }
  for(const auto &t:triangles) {
    const P A=points[t.a],B=points[t.b],C=points[t.c];
    const float area=cross(A,B,C);
    const float wa=cross(q,B,C)/area,wb=cross(A,q,C)/area,wc=cross(A,B,q)/area;
    if(wa>=-1e-5f&&wb>=-1e-5f&&wc>=-1e-5f) {
      const float s=std::max(0.f,wa)+std::max(0.f,wb)+std::max(0.f,wc);
      for(const auto &[i,w]:{std::pair{t.a,wa},std::pair{t.b,wb},std::pair{t.c,wc}})
        if(w>1e-6f) out.push_back({index[i],w/s});
      return;
    }
  }
  // Fora da triangulação (ou pontos colineares): a aresta mais próxima. Arestas
  // da casca são as que pertencem a um só triângulo; sem triângulos, todos os pares.
  std::vector<std::pair<u32,u32>> edges;
  if(triangles.empty()) {for(u32 a=0;a<n;++a) for(u32 b=a+1;b<n;++b) edges.push_back({a,b});}
  else {
    std::vector<std::pair<std::pair<u32,u32>,u32>> counted;
    for(const auto &t:triangles) for(auto e:{std::pair{t.a,t.b},std::pair{t.b,t.c},std::pair{t.a,t.c}}) {
      if(e.first>e.second) std::swap(e.first,e.second);
      auto f=std::find_if(counted.begin(),counted.end(),[&](const auto &c){return c.first==e;});
      if(f==counted.end()) counted.push_back({e,1});else ++f->second;
    }
    for(const auto &[e,count]:counted) if(count==1) edges.push_back(e);
  }
  float best=1e30f,bestT=0;std::pair<u32,u32> bestEdge{0,0};
  for(const auto &e:edges) {
    float distance=0;const float t=projection(points[e.first],points[e.second],q,distance);
    // Em pontos colineares, o segmento mais curto que contém a projeção vence o empate.
    if(distance<best-1e-9f||(std::fabs(distance-best)<=1e-9f&&t>0&&t<1)) {best=distance;bestT=t;bestEdge=e;}
  }
  if(bestT<1) out.push_back({index[bestEdge.first],1-bestT});
  if(bestT>0) out.push_back({index[bestEdge.second],bestT});
}

bool crossed(float previous,float current,float mark,bool loop) {
  if(current<=previous) return false;
  if(loop&&mark<1) return std::floor(current-mark)>std::floor(previous-mark);
  return previous<mark&&current>=mark;
}

} // namespace animator_detail

namespace {
using V=scene::ComponentOperationValue;
const scene::Animator *animatorOf(const GameWorld &world,ObjectId owner,u64 instance) {
  const auto *o=world.graph().find(owner);if(!o) return nullptr;
  const auto *c=o->components.findInstance(instance);
  return c&&&c->type()==&scene::Animator::descriptor?static_cast<const scene::Animator*>(c):nullptr;
}
float clipDuration(const AnimationLibrary &library,const resources::AssetGuid &clip) {
  AnimationClipView view;return library.findClip(clip,view)&&view.clip->duration>0?view.clip->duration:0;
}
} // namespace

const SceneAnimatorGraphs::Instance *SceneAnimatorGraphs::find(ObjectId owner,u64 instance) const {
  for(const auto &i:instances_) if(i.owner==owner&&i.instance==instance) return &i;
  return nullptr;
}
const scene::Animator *SceneAnimatorGraphs::configuration(const GameWorld &world,ObjectId owner,u64 instance) const {
  const auto *authored=animatorOf(world,owner,instance);if(!authored) return nullptr;
  if(!authored->controller.valid()) return authored;
  const auto *runtime=find(owner,instance);
  return runtime&&runtime->controllerAvailable&&runtime->controller==authored->controller?&runtime->resolved:nullptr;
}

void SceneAnimatorGraphs::sync(Instance &runtime,const scene::Animator &authored) {
  // Parâmetros por identidade: os novos entram com o padrão, os removidos saem.
  std::vector<u64> ids;std::vector<float> values;std::vector<scene::AnimatorParameterType> types;
  for(const auto &p:authored.parameters) {
    ids.push_back(p.id);
    const auto f=std::find(runtime.parameterIds.begin(),runtime.parameterIds.end(),p.id);
    const auto index=static_cast<usize>(f-runtime.parameterIds.begin());types.push_back(p.type);
    values.push_back(f!=runtime.parameterIds.end()&&index<runtime.parameterTypes.size()&&runtime.parameterTypes[index]==p.type?
      runtime.values[index]:p.type==scene::AnimatorParameterType::Trigger?0:p.value);
  }
  runtime.parameterIds=std::move(ids);runtime.values=std::move(values);
  runtime.parameterTypes=std::move(types);
  std::vector<LayerState> layers;
  for(const auto &l:authored.layers) {
    const auto f=std::find_if(runtime.layers.begin(),runtime.layers.end(),[&](const LayerState &s){return s.layer==l.id;});
    LayerState s=f!=runtime.layers.end()?*f:LayerState{};
    s.layer=l.id;
    if(!l.state(s.current)) {s.current=l.defaultState;s.time=0;s.transitioning=false;s.entered=false;s.currentFresh=true;}
    if(s.transitioning&&!l.state(s.next)) s.transitioning=false;
    layers.push_back(s);
  }
  runtime.layers=std::move(layers);
}

SceneAnimatorGraphs::Instance *SceneAnimatorGraphs::ensure(GameWorld &world,ObjectId owner,u64 instance,const scene::Animator **component) {
  const auto *a=animatorOf(world,owner,instance);if(component) *component=nullptr;
  if(!a) return nullptr;
  Instance *runtime=nullptr;
  for(auto &i:instances_) if(i.owner==owner&&i.instance==instance) {runtime=&i;break;}
  if(!runtime) {Instance created;created.owner=owner;created.instance=instance;instances_.push_back(std::move(created));runtime=&instances_.back();}
  if(!a->controller.valid()) {
    if(runtime->controller.valid()) {runtime->parameterIds.clear();runtime->layers.clear();}
    runtime->controller={};runtime->controllerRevision=0;runtime->controllerAvailable=true;runtime->controllerDiagnostic.clear();
    sync(*runtime,*a);if(component) *component=a;return runtime;
  }
  const auto *asset=resources::findAnimatorController(controllers_,a->controller);
  if(!asset) {runtime->controllerAvailable=false;runtime->controllerDiagnostic="Controller ausente ou inválido";return runtime;}
  bool changed=runtime->controller!=a->controller||runtime->controllerRevision!=asset->revision||!runtime->controllerAvailable;
  const auto &cached=runtime->resolved;
  changed=changed||cached.target!=a->target||cached.motionSource!=a->motionSource||cached.enabled!=a->enabled||
    cached.speed!=a->speed||cached.unscaledTime!=a->unscaledTime||cached.clipOverrides!=a->clipOverrides;
  for(const auto &layer:cached.layers) {const auto *local=a->layer(layer.id);if(layer.mask!=(local?local->mask:0)) changed=true;}
  if(changed) {
    if(runtime->controller!=a->controller) {runtime->parameterIds.clear();runtime->layers.clear();}
    runtime->controllerAvailable=resources::resolveAnimatorController(*a,controllers_,runtime->resolved,runtime->controllerDiagnostic);
    runtime->controller=a->controller;runtime->controllerRevision=asset->revision;
    if(runtime->controllerAvailable) sync(*runtime,runtime->resolved);
  }
  if(runtime->controllerAvailable&&component) *component=&runtime->resolved;
  return runtime;
}

void SceneAnimatorGraphs::emit(GameWorld &world,const Instance &runtime,std::string_view event,std::initializer_list<V> values) {
  if(events_) events_->emit(world,runtime.owner,runtime.instance,scene::Animator::descriptor,event,std::span(values.begin(),values.size()));
}

bool SceneAnimatorGraphs::advance(GameWorld &world,const AnimationLibrary &library,float scaled,float unscaled,
                                  std::vector<SceneAnimator::ExternalSample> &samples) {
  if(!std::isfinite(scaled)||!std::isfinite(unscaled)||scaled<0||unscaled<0) return false;
  std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
  std::vector<std::pair<ObjectId,u64>> live;
  for(const auto id:ids) if(const auto *o=world.graph().find(id))
    if(const auto *c=o->components.find(scene::Animator::descriptor)) live.emplace_back(id,c->instanceId());
  std::erase_if(instances_,[&](const Instance &i){return std::find(live.begin(),live.end(),std::pair(i.owner,i.instance))==live.end();});

  std::vector<animator_detail::WeightedMotion> weights;
  for(const auto &[owner,instance]:live) {
    if(!world.activeInHierarchy(world.handle(owner))) continue;
    const scene::Animator *a=nullptr;auto *runtime=ensure(world,owner,instance,&a);
    if(!runtime||!a||!a->enabled) continue;
    const ObjectId motionOwner=a->motionSource?static_cast<ObjectId>(a->motionSource):owner;
    bool bound=false;for(const auto &p:a->parameters) bound=bound||p.source!=scene::AnimatorParameterSource::Manual;
    if(bound) {
      float velocity[3]{};bool grounded=false,available=false,hasSupportState=false;
      physics::CharacterMotor::RuntimeState character;
      ScenePhysics::DynamicMotorState motor;
      if(physics_&&physics_->ownsWorld(world)&&world.graph().exists(motionOwner)&&world.activeInHierarchy(world.handle(motionOwner))) {
        if(physics_->characterState(world,motionOwner,character)==WorldStatus::Ok&&character.hasMeasuredStep) {
          velocity[0]=character.motorVelocity.x;velocity[1]=character.motorVelocity.y;velocity[2]=character.motorVelocity.z;
          grounded=character.groundState==AetherCharacterGroundState::OnGround;available=true;hasSupportState=true;
        } else if(physics_->dynamicMotorState(motionOwner,motor)&&motor.hasMeasuredStep&&physics_->getBodyVelocity(motionOwner,velocity)) {
          grounded=motor.grounded;available=true;hasSupportState=true;
          if(grounded) for(u32 axis=0;axis<3;++axis) velocity[axis]-=motor.supportVelocity[axis];
        } else available=physics_->getBodyVelocity(motionOwner,velocity);
      }
      runtime->motionAvailable=available;
      runtime->motionDiagnostic=available?"":"Fonte física ausente, inativa ou sem passo";
      for(const auto &p:a->parameters) if(available&&!hasSupportState&&p.source==scene::AnimatorParameterSource::Grounded)
        runtime->motionDiagnostic="Apoio exige Personagem ou Motor; velocidades disponíveis";
      for(usize k=0;k<a->parameters.size();++k) {
        const auto &p=a->parameters[k];if(p.source==scene::AnimatorParameterSource::Manual) continue;
        float target=0;
        if(available) switch(p.source) {
          case scene::AnimatorParameterSource::PlanarSpeed: target=std::hypot(velocity[0],velocity[2]);break;
          case scene::AnimatorParameterSource::VerticalSpeed: target=velocity[1];break;
          case scene::AnimatorParameterSource::Grounded: target=grounded?1.f:0.f;break;
          case scene::AnimatorParameterSource::Speed: target=std::sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]+velocity[2]*velocity[2]);break;
          default: break;
        }
        if(p.source!=scene::AnimatorParameterSource::Grounded) target*=p.scale;
        const float alpha=p.response>0&&available&&p.source!=scene::AnimatorParameterSource::Grounded? -std::expm1(-scaled/p.response):1.f;
        runtime->values[k]+=alpha*(target-runtime->values[k]);
      }
    } else {runtime->motionAvailable=false;runtime->motionDiagnostic.clear();}
    const float delta=(a->unscaledTime?unscaled:scaled)*a->speed;
    const ObjectId root=a->target&&world.graph().exists(static_cast<ObjectId>(a->target))?static_cast<ObjectId>(a->target):owner;
    const auto value=[&](u64 parameter,float fallback){
      const auto f=std::find(runtime->parameterIds.begin(),runtime->parameterIds.end(),parameter);
      return f==runtime->parameterIds.end()?fallback:runtime->values[f-runtime->parameterIds.begin()];
    };
    // Pesos de cada clipe de um estado e a duração ponderada (Unity: média dos filhos).
    const auto motionsOf=[&](const scene::AnimatorState &s,float &duration){
      weights.clear();
      if(s.kind==scene::AnimatorMotionKind::Clip) {if(!s.motions.empty()) weights.push_back({0,1});}
      else if(s.kind==scene::AnimatorMotionKind::Blend1D) animator_detail::blend1D(s.motions,value(s.blendX,0),weights);
      else animator_detail::blend2D(s.motions,value(s.blendX,0),value(s.blendY,0),weights);
      duration=0;float known=0;
      for(const auto &w:weights) if(const float d=clipDuration(library,s.motions[w.motion].clip);d>0) {duration+=w.weight*d;known+=w.weight;}
      duration=known>0?duration/known:1;
      return weights;
    };
    const auto stateSpeed=[&](const scene::AnimatorState &s){return s.speed*(s.speedParameter?value(s.speedParameter,1):1);};
    for(u32 li=0;li<a->layers.size();++li) {
      const auto &layer=a->layers[li];auto &ls=runtime->layers[li];
      const auto *current=layer.state(ls.current);if(!current) continue;
      if(!ls.entered) {ls.entered=true;emit(world,*runtime,"state_entered",{V::makeInteger(li),V::makeInteger(static_cast<i64>(current->id))});}
      // Avanço de tempo dos estados ativos (e eventos que o tempo atravessa).
      const auto advanceState=[&](const scene::AnimatorState &s,float &time,bool &justEntered){
        float duration=1;motionsOf(s,duration);
        const float previous=justEntered?-1e-6f:time;justEntered=false;
        time+=delta*stateSpeed(s)/std::max(duration,1e-4f);
        for(const auto &e:s.events) if(animator_detail::crossed(previous,time,e.time,s.loop))
          emit(world,*runtime,"state_event",{V::makeInteger(li),V::makeInteger(static_cast<i64>(s.id)),V::makeInteger(e.tag)});
        return previous;
      };
      const float previous=advanceState(*current,ls.time,ls.currentFresh);
      if(ls.transitioning) {
        const auto *next=layer.state(ls.next);
        if(next) advanceState(*next,ls.nextTime,ls.nextEntered);
        ls.elapsed+=std::max(0.f,a->unscaledTime?unscaled:scaled);
        if(!next||ls.elapsed>=ls.duration) {ls.current=ls.next;ls.time=ls.nextTime;ls.transitioning=false;current=layer.state(ls.current);}
      } else {
        // Transições: Qualquer estado primeiro, depois as do estado atual.
        const scene::AnimatorTransition *chosen=nullptr;
        for(u32 pass=0;pass<2&&!chosen;++pass) for(const auto &t:layer.transitions) {
          if(pass==0?t.from!=0:t.from!=current->id) continue;
          if(pass==0&&t.to==current->id) continue;
          if(t.conditions.empty()&&!t.hasExitTime) continue;
          if(t.hasExitTime&&!animator_detail::crossed(previous,ls.time,t.exitTime,current->loop)) continue;
          bool pass_=true;
          for(const auto &c:t.conditions) {
            const float v=value(c.parameter,0);
            switch(c.mode) {
              case scene::AnimatorConditionMode::If: pass_=pass_&&v!=0;break;
              case scene::AnimatorConditionMode::IfNot: pass_=pass_&&v==0;break;
              case scene::AnimatorConditionMode::Greater: pass_=pass_&&v>c.threshold;break;
              case scene::AnimatorConditionMode::Less: pass_=pass_&&v<c.threshold;break;
              case scene::AnimatorConditionMode::Equals: pass_=pass_&&std::lround(v)==std::lround(c.threshold);break;
              case scene::AnimatorConditionMode::NotEqual: pass_=pass_&&std::lround(v)!=std::lround(c.threshold);break;
            }
          }
          if(pass_) {chosen=&t;break;}
        }
        if(chosen) {
          // Gatilhos usados pela transição são consumidos.
          for(const auto &c:chosen->conditions) if(const auto *p=a->parameter(c.parameter);p&&p->type==scene::AnimatorParameterType::Trigger)
            for(usize k=0;k<runtime->parameterIds.size();++k) if(runtime->parameterIds[k]==p->id) runtime->values[k]=0;
          emit(world,*runtime,"state_entered",{V::makeInteger(li),V::makeInteger(static_cast<i64>(chosen->to))});
          if(chosen->duration<=0) {ls.current=chosen->to;ls.time=0;ls.transitioning=false;ls.currentFresh=true;current=layer.state(ls.current);}
          else {ls.next=chosen->to;ls.nextTime=0;ls.elapsed=0;ls.duration=chosen->duration;ls.transitioning=true;ls.nextEntered=true;}
        }
      }
      // Amostras: estado atual (1−b) e próximo (b), cada um com sua mistura.
      const float layerWeight=li==0?1.f:std::clamp(layer.weight,0.f,1.f);
      const ObjectId mask=layer.mask&&world.graph().exists(static_cast<ObjectId>(layer.mask))?static_cast<ObjectId>(layer.mask):kInvalidObject;
      const float blend=ls.transitioning&&ls.duration>0?std::clamp(ls.elapsed/ls.duration,0.f,1.f):0;
      const auto sample=[&](const scene::AnimatorState &s,float normalized,float weight){
        if(weight<=0) return;
        float duration=1;const auto chosenWeights=motionsOf(s,duration);
        const float fraction=s.loop?normalized-std::floor(normalized):std::clamp(normalized,0.f,1.f);
        for(const auto &w:chosenWeights) {
          const auto &m=s.motions[w.motion];const float d=clipDuration(library,m.clip);
          if(!m.clip.valid()) continue;
          if(d<=0) {runtime->controllerDiagnostic="Clipe ausente ou sem duração";continue;}
          samples.push_back({root,m.clip,fraction*d,weight*w.weight*layerWeight,li,mask});
        }
      };
      if(current) sample(*current,ls.time,1-blend);
      if(ls.transitioning) if(const auto *next=layer.state(ls.next)) sample(*next,ls.nextTime,blend);
    }
  }
  return true;
}

SceneAnimatorGraphs::Status SceneAnimatorGraphs::parameter(GameWorld &world,ObjectId owner,u64 instance,std::string_view name,
                                                           ParameterOperation operation,float value,float &result) {
  result=0;const scene::Animator *a=nullptr;auto *runtime=ensure(world,owner,instance,&a);
  if(!runtime) return Status::UnknownComponent;
  if(!a) return Status::MissingController;
  const auto *p=a->parameter(name);if(!p) return Status::UnknownParameter;
  if(operation!=ParameterOperation::Get&&p->source!=scene::AnimatorParameterSource::Manual) return Status::BoundParameter;
  usize k=0;while(k<runtime->parameterIds.size()&&runtime->parameterIds[k]!=p->id) ++k;
  if(k==runtime->parameterIds.size()) return Status::UnknownParameter;
  using T=scene::AnimatorParameterType;
  const auto expect=[&](T type){return p->type==type;};
  switch(operation) {
    case ParameterOperation::Get: result=runtime->values[k];return Status::Ok;
    case ParameterOperation::SetFloat: if(!expect(T::Float)) return Status::WrongType;if(!std::isfinite(value)) return Status::InvalidArgument;break;
    case ParameterOperation::SetInt: if(!expect(T::Int)) return Status::WrongType;value=std::round(value);break;
    case ParameterOperation::SetBool: if(!expect(T::Bool)) return Status::WrongType;value=value!=0?1.f:0.f;break;
    case ParameterOperation::SetTrigger: if(!expect(T::Trigger)) return Status::WrongType;value=1;break;
    case ParameterOperation::ResetTrigger: if(!expect(T::Trigger)) return Status::WrongType;value=0;break;
    default: return Status::InvalidArgument;
  }
  runtime->values[k]=value;result=value;return Status::Ok;
}

SceneAnimatorGraphs::Status SceneAnimatorGraphs::play(GameWorld &world,ObjectId owner,u64 instance,u32 layerIndex,std::string_view stateName,float crossFade) {
  const scene::Animator *a=nullptr;auto *runtime=ensure(world,owner,instance,&a);
  if(!runtime) return Status::UnknownComponent;
  if(!a) return Status::MissingController;
  if(layerIndex>=a->layers.size()) return Status::UnknownLayer;
  if(!std::isfinite(crossFade)||crossFade<0||crossFade>60) return Status::InvalidArgument;
  const auto &layer=a->layers[layerIndex];auto &ls=runtime->layers[layerIndex];
  const scene::AnimatorState *target=nullptr;for(const auto &s:layer.states) if(s.name==stateName) target=&s;
  if(!target) return Status::UnknownState;
  emit(world,*runtime,"state_entered",{V::makeInteger(layerIndex),V::makeInteger(static_cast<i64>(target->id))});
  if(crossFade<=0||target->id==ls.current) {ls.current=target->id;ls.time=0;ls.transitioning=false;ls.entered=true;ls.currentFresh=true;}
  else {ls.next=target->id;ls.nextTime=0;ls.elapsed=0;ls.duration=crossFade;ls.transitioning=true;ls.nextEntered=true;ls.entered=true;}
  return Status::Ok;
}

SceneAnimatorGraphs::Status SceneAnimatorGraphs::info(const GameWorld &world,ObjectId owner,u64 instance,u32 layerIndex,Info &out) const {
  out={};const auto *a=configuration(world,owner,instance);const auto *runtime=find(owner,instance);
  if(runtime&&!runtime->controllerAvailable) return Status::MissingController;
  if(!a||!runtime) return Status::UnknownComponent;
  if(layerIndex>=a->layers.size()||layerIndex>=runtime->layers.size()) return Status::UnknownLayer;
  const auto &layer=a->layers[layerIndex];const auto &ls=runtime->layers[layerIndex];
  out.state=ls.current;out.normalizedTime=ls.time;out.transitioning=ls.transitioning;
  if(const auto *s=layer.state(ls.current)) out.name=s->name;
  if(ls.transitioning) {out.next=ls.next;out.progress=ls.duration>0?std::clamp(ls.elapsed/ls.duration,0.f,1.f):1;if(const auto *s=layer.state(ls.next)) out.nextName=s->name;}
  return Status::Ok;
}

} // namespace ae::runtime
