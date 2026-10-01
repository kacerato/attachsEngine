#pragma once
#include "runtime/game_world.h"
#include "runtime/object_activation_connection.h"
#include "scene/transform_tween.h"
#include <map>
#include <set>
namespace ae::runtime {
class SceneTweens final {
public:
 enum class Status {Idle,Delayed,Running,Completed,Cancelled,Authority,CompetingWriter,InvalidPose,NoChannels};
 static const char*statusText(Status s){switch(s){case Status::Idle:return "Aguardando início";case Status::Delayed:return "Espera inicial";case Status::Running:return "Interpolando";case Status::Completed:return "Concluído";case Status::Cancelled:return "Cancelado";case Status::Authority:return "Pose controlada pela física";case Status::CompetingWriter:return "Outro componente controla a pose";case Status::InvalidPose:return "Transformação inválida";case Status::NoChannels:return "Nenhum canal selecionado";}return "Tween indisponível";}
 struct State {Status status=Status::Idle;double elapsed=0;Transform start{},end{};bool initialized=false,enabled=true,paused=false;std::array<float,9> authoredDestination{};bool authoredRelative=false,authoredPosition=false,authoredRotation=false,authoredScale=false;bool connectionInvoked=false;WorldStatus connectionStatus=WorldStatus::Ok;};
 using Key=std::pair<ObjectId,u64>;
 const State*state(ObjectId id,u64 instance)const{auto i=states_.find({id,instance});return i==states_.end()?nullptr:&i->second;}
 void reset(){states_.clear();knownWorld_=0;cachedRevision_=~u64{0};ids_.clear();}
 WorldStatus command(GameWorld&w,ComponentHandle h,u32 operation,State&out){
  const auto valid=w.validate(h.object);if(valid!=WorldStatus::Ok)return valid;
  const auto*v=w.readComponent(h);if(!v)return WorldStatus::ComponentMissing;
  if(v->type().id!=scene::TransformTween::descriptor.id||operation>4)return WorldStatus::InvalidArgument;
  if(knownWorld_!=w.worldId()){reset();knownWorld_=w.worldId();}
  const auto&c=*static_cast<const scene::TransformTween*>(v);auto[it,created]=states_.try_emplace({h.object.id,h.instance});auto&s=it->second;
  if(created){s.status=c.autoplay?Status::Running:Status::Idle;s.enabled=c.enabled;}
  if(operation==1){const bool paused=s.paused;s=State{};s.paused=paused;s.enabled=c.enabled;s.status=Status::Running;}
  else if(operation==2)s.status=Status::Cancelled;
  else if(operation==3)s.paused=true;
  else if(operation==4)s.paused=false;
  out=s;return WorldStatus::Ok;
 }
 bool restart(GameWorld&w,ObjectId id,u64 instance){State out;return command(w,{w.handle(id),instance},1,out)==WorldStatus::Ok;}
 bool cancel(GameWorld&w,ObjectId id,u64 instance){if(knownWorld_!=w.worldId())return false;State out;return command(w,{w.handle(id),instance},2,out)==WorldStatus::Ok;}
 bool advance(GameWorld&w,double delta){return advance(w,delta,delta);}
 bool advance(GameWorld&w,double delta,double unscaledDelta){
  if(!w.running()||!std::isfinite(delta)||delta<0||delta>.25||!std::isfinite(unscaledDelta)||unscaledDelta<0||unscaledDelta>.25)return false;
  if(knownWorld_!=w.worldId()){reset();knownWorld_=w.worldId();}
  if(cachedRevision_!=w.structuralRevision()){ids_.clear();w.graph().collectSubtree(w.graph().root(),ids_);cachedRevision_=w.structuralRevision();}std::set<Key>seen;
  for(auto id:ids_){const auto*o=w.graph().find(id);if(!o)continue;
   bool writer=false;for(usize k=0;k<o->components.size();++k){const auto*v=o->components.at(k);if(v->type().id=="astra.animation"||v->type().id.starts_with("astra.constraint.")||v->type().id=="astra.path.follow"){bool enabled=true;for(const auto&p:v->type().booleans)if(p.id=="enabled")enabled=p.read(*v);if(enabled)writer=true;}}
   bool claimed=false;
   for(usize k=0;k<o->components.size();++k){const auto*v=o->components.at(k);if(v->type().id!=scene::TransformTween::descriptor.id)continue;const auto&c=*static_cast<const scene::TransformTween*>(v);Key key{id,c.instanceId()};seen.insert(key);
    auto[it,created]=states_.try_emplace(key);auto&s=it->second;
    if(created)s.status=c.autoplay?Status::Running:Status::Idle;
    if(!c.enabled){s.enabled=false;s.status=Status::Cancelled;continue;}
    if(!s.enabled){s=State{};s.status=c.autoplay?Status::Running:Status::Idle;}
    if(!w.activeInHierarchy(w.handle(id)))continue;
    if(s.status==Status::Cancelled||s.status==Status::Completed||s.status==Status::Idle||s.paused)continue;
    if(!c.position&&!c.rotation&&!c.scale){s.status=Status::NoChannels;s.initialized=false;s.elapsed=0;continue;}
    if(w.authorityOf(w.handle(id))!=TransformAuthority::Free){s.status=Status::Authority;s.initialized=false;s.elapsed=0;continue;}
    if(writer||claimed){s.status=Status::CompetingWriter;s.initialized=false;s.elapsed=0;continue;}claimed=true;
    bool retarget=s.initialized&&(s.authoredRelative!=c.relative||s.authoredPosition!=c.position||s.authoredRotation!=c.rotation||s.authoredScale!=c.scale);for(u32 a=0;a<9;++a)retarget=retarget||(s.initialized&&s.authoredDestination[a]!=c.destination[a]);
    if(retarget){s.initialized=false;s.elapsed=c.delay;}
    if(!s.initialized){s.authoredRelative=c.relative;s.authoredPosition=c.position;s.authoredRotation=c.rotation;s.authoredScale=c.scale;for(u32 a=0;a<9;++a)s.authoredDestination[a]=c.destination[a];s.start=o->transform;s.end=s.start;for(u32 a=0;a<3;++a){s.end.position[a]=c.destination[a]+(c.relative?s.start.position[a]:0);s.end.rotationDegrees[a]=c.destination[3+a]+(c.relative?s.start.rotationDegrees[a]:0);s.end.scale[a]=c.relative?s.start.scale[a]*c.destination[6+a]:c.destination[6+a];}s.initialized=true;}
    s.elapsed+=c.ignoreTimeScale?unscaledDelta:delta;const double t=s.elapsed-c.delay;if(t<0){s.status=Status::Delayed;continue;}
    const double cycleDuration=c.duration*(c.pingpong?2.:1.);const bool completed=c.loops&&t>=cycleDuration*c.loops;
    double phase=completed?(c.pingpong?0.:1.):std::fmod(t,cycleDuration)/c.duration;
    if(phase>1)phase=2-phase;
    const float x=static_cast<float>(phase);const float blend=c.easing==1?x*x*(3-2*x):c.easing==2?x*x:c.easing==3?1-(1-x)*(1-x):x;
    Transform out=o->transform;for(u32 a=0;a<3;++a){if(c.position)out.position[a]=s.start.position[a]+(s.end.position[a]-s.start.position[a])*blend;if(c.rotation)out.rotationDegrees[a]=s.start.rotationDegrees[a]+(s.end.rotationDegrees[a]-s.start.rotationDegrees[a])*blend;if(c.scale)out.scale[a]=s.start.scale[a]+(s.end.scale[a]-s.start.scale[a])*blend;}
    if(w.setLocalTransform(w.handle(id),out)!=WorldStatus::Ok)s.status=Status::InvalidPose;
    else {
      s.status=completed?Status::Completed:Status::Running;
      if(completed&&c.finishedAction){s.connectionInvoked=true;s.connectionStatus=applyObjectActivationConnection(w,static_cast<ObjectId>(c.finishedTarget),c.finishedAction);}
    }
   }
  }
  for(auto i=states_.begin();i!=states_.end();)if(!seen.contains(i->first))i=states_.erase(i);else++i;
  return true;
 }
private:std::map<Key,State>states_;u32 knownWorld_=0;u64 cachedRevision_=~u64{0};std::vector<ObjectId>ids_;
};
}
