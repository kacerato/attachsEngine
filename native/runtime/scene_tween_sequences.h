#pragma once
#include "runtime/component_operations.h"
#include "runtime/game_world.h"
#include "runtime/scene_tweens.h"
#include "scene/tween_sequence.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ae::runtime {
// Avaliador de Play das Sequências de tweens. Não interpola nada: comanda os
// estados de SceneTweens (reiniciar, pausar, cancelar) e lê o término deles.
// Roda antes de SceneTweens no quadro; uma etapa que termina libera a próxima
// no quadro seguinte.
class SceneTweenSequences final {
public:
  enum class Status {Idle,Interval,Running,Completed,Cancelled,Failed,NoSteps};
  static const char *statusText(Status s) {
    switch(s) {
    case Status::Idle:return "Aguardando Tocar";
    case Status::Interval:return "Intervalo da etapa";
    case Status::Running:return "Etapa em andamento";
    case Status::Completed:return "Concluída";
    case Status::Cancelled:return "Cancelada";
    case Status::Failed:return "Etapa falhou";
    case Status::NoSteps:return "Nenhuma etapa com tween";
    }
    return "Sequência indisponível";
  }
  struct State {
    Status status=Status::Idle;
    u32 step=0;          // slot da etapa em andamento (0..7)
    u32 groupEnd=0;      // primeiro slot depois do grupo em andamento
    u32 loopsDone=0;
    double interval=0;   // intervalo restante antes do grupo começar
    bool paused=false,enabled=true,pendingStart=false;
    std::string failure; // motivo legível quando status==Failed
    std::vector<std::pair<ObjectId,u64>> members; // tweens do grupo em andamento
  };
  using Key=std::pair<ObjectId,u64>;
  const State *state(ObjectId id,u64 instance) const {auto i=states_.find({id,instance});return i==states_.end()?nullptr:&i->second;}
  void reset() {states_.clear();world_=0;revision_=~u64{0};ids_.clear();}
  void setEvents(ComponentEventQueue *events) noexcept {events_=events;}

  // 0 leitura, 1 tocar, 2 cancelar, 3 pausar, 4 retomar.
  WorldStatus command(GameWorld &w,SceneTweens &tweens,ComponentHandle h,u32 operation,State &out) {
    const auto valid=w.validate(h.object);if(valid!=WorldStatus::Ok) return valid;
    const auto *v=w.readComponent(h);if(!v) return WorldStatus::ComponentMissing;
    if(&v->type()!=&scene::TweenSequence::descriptor || operation>4) return WorldStatus::InvalidArgument;
    sync(w);
    auto [it,created]=states_.try_emplace({h.object.id,h.instance});auto &s=it->second;
    const auto &c=scene::tweenSequence(*v);
    if(created) s.pendingStart=c.autoplay;
    if(operation==1) {stopMembers(w,tweens,s);const bool paused=s.paused;s=State{};s.paused=paused;s.pendingStart=true;}
    else if(operation==2) {stopMembers(w,tweens,s);s.status=Status::Cancelled;s.pendingStart=false;}
    else if(operation==3 && !s.paused) {s.paused=true;memberCommand(w,tweens,s,3);}
    else if(operation==4 && s.paused) {s.paused=false;memberCommand(w,tweens,s,4);}
    out=s;return WorldStatus::Ok;
  }

  bool advance(GameWorld &w,SceneTweens &tweens,double delta,double unscaledDelta) {
    if(!w.running() || !std::isfinite(delta) || delta<0 || delta>.25 || !std::isfinite(unscaledDelta) || unscaledDelta<0 || unscaledDelta>.25) return false;
    sync(w);
    std::set<Key> seen;
    for(const auto id:ids_) {
      const auto *o=w.graph().find(id);if(!o) continue;
      for(usize k=0;k<o->components.size();++k) {
        const auto *v=o->components.at(k);
        if(&v->type()!=&scene::TweenSequence::descriptor) continue;
        const auto &c=scene::tweenSequence(*v);const Key key{id,c.instanceId()};seen.insert(key);
        auto [it,created]=states_.try_emplace(key);auto &s=it->second;
        if(created) s.pendingStart=c.autoplay;
        if(!c.enabled) {if(s.enabled) {stopMembers(w,tweens,s);s.status=Status::Cancelled;}s.enabled=false;continue;}
        if(!s.enabled) {s=State{};s.pendingStart=c.autoplay;}
        if(!w.activeInHierarchy(w.handle(id)) || s.paused) continue;
        if(s.pendingStart) {
          s.pendingStart=false;
          if(c.stepCount()==0) {s.status=Status::NoSteps;continue;}
          // A sequência é dona dos tweens das etapas: nenhum começa por conta própria.
          for(const auto &step:c.steps) for(const auto &member:tweensOf(w,step.target)) {SceneTweens::State ignored;tweens.command(w,{w.handle(member.first),member.second},2,ignored);}
          s.loopsDone=0;beginGroup(c,s,firstStep(c,0));
        }
        const double dt=c.ignoreTimeScale?unscaledDelta:delta;
        if(s.status==Status::Interval) {
          s.interval-=dt;
          if(s.interval>1e-9) continue;
          startGroup(w,tweens,id,c,s);
          continue;
        }
        if(s.status!=Status::Running) continue;
        bool done=true;
        for(const auto &[member,instance]:s.members) {
          const auto *live=tweens.state(member,instance);
          if(!live) {fail(s,"Etapa "+std::to_string(s.step+1)+": tween removido");done=false;break;}
          using T=SceneTweens::Status;
          if(live->status==T::Completed) continue;
          if(live->status==T::Running || live->status==T::Delayed) {done=false;continue;}
          fail(s,"Etapa "+std::to_string(s.step+1)+": "+(live->status==T::Cancelled?std::string{"tween cancelado fora da sequência"}:std::string{SceneTweens::statusText(live->status)}));
          done=false;break;
        }
        if(s.status!=Status::Running || !done) continue;
        const u32 next=firstStep(c,s.groupEnd);
        if(next<scene::kTweenSequenceSteps) {beginGroup(c,s,next);continue;}
        ++s.loopsDone;
        if(c.loops==0 || s.loopsDone<c.loops) {beginGroup(c,s,firstStep(c,0));continue;}
        s.status=Status::Completed;s.members.clear();
        if(events_) events_->emit(w,id,c.instanceId(),scene::TweenSequence::descriptor,"completed");
      }
    }
    for(auto i=states_.begin();i!=states_.end();) if(!seen.contains(i->first)) i=states_.erase(i); else ++i;
    return true;
  }

private:
  static u32 firstStep(const scene::TweenSequence &c,u32 from) {
    for(u32 i=from;i<scene::kTweenSequenceSteps;++i) if(c.steps[i].target) return i;
    return scene::kTweenSequenceSteps;
  }
  // Grupo = etapa `first` e as seguintes marcadas "junto da anterior".
  static void beginGroup(const scene::TweenSequence &c,State &s,u32 first) {
    s.step=first;s.members.clear();s.interval=c.steps[first].interval;s.status=Status::Interval;
    u32 end=first+1;
    for(u32 next=firstStep(c,end);next<scene::kTweenSequenceSteps && c.steps[next].join;next=firstStep(c,next+1)) end=next+1;
    s.groupEnd=end;
  }
  void startGroup(GameWorld &w,SceneTweens &tweens,ObjectId owner,const scene::TweenSequence &c,State &s) {
    s.members.clear();
    for(u32 i=s.step;i<s.groupEnd;++i) {
      if(!c.steps[i].target) continue;
      const auto members=tweensOf(w,c.steps[i].target);
      if(members.empty()) {fail(s,"Etapa "+std::to_string(i+1)+": objeto sem Transform Tween nem Tween de propriedade");return;}
      for(const auto &member:members) if(loopsOf(w,member)==0) {fail(s,"Etapa "+std::to_string(i+1)+": tween com repetição infinita nunca termina");return;}
      for(const auto &member:members) {
        SceneTweens::State out;
        if(tweens.command(w,{w.handle(member.first),member.second},1,out)!=WorldStatus::Ok) {fail(s,"Etapa "+std::to_string(i+1)+": tween recusou reinício");return;}
        s.members.emplace_back(member);
      }
      if(events_) {const auto number=scene::ComponentOperationValue::makeInteger(i+1);events_->emit(w,owner,c.instanceId(),scene::TweenSequence::descriptor,"step_started",std::span(&number,1));}
    }
    s.status=Status::Running;
  }
  // Tweens persistentes do objeto apontado: o Transform Tween (um escreve a
  // pose) e todos os Tweens de propriedade, que começam juntos na etapa.
  static std::vector<std::pair<ObjectId,u64>> tweensOf(const GameWorld &w,u64 target) {
    std::vector<std::pair<ObjectId,u64>> found;if(!target) return found;
    const auto *o=w.graph().find(static_cast<ObjectId>(target));if(!o) return found;
    bool pose=false;
    for(usize k=0;k<o->components.size();++k) {
      const auto *v=o->components.at(k);
      const bool transform=&v->type()==&scene::TransformTween::descriptor;
      if((transform&&!pose)||&v->type()==&scene::PropertyTween::descriptor) found.emplace_back(static_cast<ObjectId>(target),v->instanceId());
      pose|=transform;
    }
    return found;
  }
  static u32 loopsOf(const GameWorld &w,std::pair<ObjectId,u64> member) {
    const auto *v=w.readComponent({w.handle(member.first),member.second});
    if(!v) return 1;
    return &v->type()==&scene::PropertyTween::descriptor?scene::propertyTween(*v).loops:static_cast<const scene::TransformTween&>(*v).loops;
  }
  void stopMembers(GameWorld &w,SceneTweens &tweens,State &s) {memberCommand(w,tweens,s,2);s.members.clear();}
  static void memberCommand(GameWorld &w,SceneTweens &tweens,const State &s,u32 operation) {
    for(const auto &[member,instance]:s.members) {
      if(w.validate(w.handle(member))!=WorldStatus::Ok) continue;
      SceneTweens::State ignored;tweens.command(w,{w.handle(member),instance},operation,ignored);
    }
  }
  static void fail(State &s,std::string reason) {s.status=Status::Failed;s.failure=std::move(reason);s.members.clear();}
  void sync(GameWorld &w) {
    if(world_!=w.worldId()) {reset();world_=w.worldId();}
    if(revision_!=w.structuralRevision()) {ids_.clear();w.graph().collectSubtree(w.graph().root(),ids_);revision_=w.structuralRevision();}
  }
  ComponentEventQueue *events_=nullptr;
  std::map<Key,State> states_;
  u32 world_=0;u64 revision_=~u64{0};
  std::vector<ObjectId> ids_;
};
}
