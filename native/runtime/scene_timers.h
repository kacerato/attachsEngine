#pragma once
#include "runtime/object_activation_connection.h"
#include "runtime/game_world.h"
#include "scene/timer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace ae::runtime {
// Estado de execução separado dos dados autorados. Identidade de instância
// permite vários timers no mesmo objeto e impede reusar contagem após remoção.
class SceneTimers final {
public:
  enum class ConnectionStatus {Disconnected,Ready,MissingTarget,Rejected};
  struct State {double remaining=0;float interval=0;bool enabled=false;bool completed=false;ConnectionStatus connection=ConnectionStatus::Disconnected;bool running=false,paused=false;};
  void reset() {states_.clear();timers_.clear();knownRevision_=0;}
  const State *state(ObjectId object,u64 instance) const {
    const auto it=states_.find({object,instance});return it==states_.end()?nullptr:&it->second;
  }
  WorldStatus command(GameWorld &world,ComponentHandle component,u32 operation,float seconds,State &output) {
    const auto valid=world.validate(component.object);if(valid!=WorldStatus::Ok)return valid;
    const auto *value=world.readComponent(component);if(!value)return WorldStatus::ComponentMissing;
    if(&value->type()!=&scene::Timer::descriptor)return WorldStatus::InvalidArgument;
    if(operation>4 || !std::isfinite(seconds) || (operation!=1&&seconds!=0) || seconds<0 ||
       (seconds>0&&(seconds<.05f||seconds>3600.f)))return WorldStatus::InvalidArgument;
    auto timer=*static_cast<const scene::Timer*>(value);
    if(operation==1&&seconds>0) {
      const auto status=world.setProperty(component,"interval_seconds",seconds);if(status!=WorldStatus::Ok)return status;
      timer.intervalSeconds=seconds;
    }
    auto &state=ensure({component.object.id,component.instance},timer);
    if(operation==1){state.interval=timer.intervalSeconds;state.remaining=timer.intervalSeconds;state.running=true;state.completed=false;}
    else if(operation==2){state.remaining=0;state.running=false;state.completed=false;}
    else if(operation==3)state.paused=true;
    else if(operation==4)state.paused=false;
    output=state;return WorldStatus::Ok;
  }
  template<class Callback> bool advance(GameWorld &world,double delta,Callback fire) {
    return advance(world,delta,delta,fire);
  }
  template<class Callback> bool advance(GameWorld &world,double delta,double unscaledDelta,Callback fire) {
    if(!world.running() || !std::isfinite(delta) || delta<0 || delta>0.25 ||
       !std::isfinite(unscaledDelta) || unscaledDelta<0 || unscaledDelta>0.25) return false;
    if(knownRevision_!=world.structuralRevision()) {
      std::vector<ObjectId> objects;world.graph().collectSubtree(world.graph().root(),objects);
      timers_.clear();std::set<Key> seen;
      for(const auto object:objects) {
        const auto *node=world.graph().find(object);if(!node) continue;
        for(usize i=0;i<node->components.size();++i) {
          const auto *value=node->components.at(i);
          if(&value->type()!=&scene::Timer::descriptor) continue;
          const Key key{object,value->instanceId()};timers_.push_back(key);seen.insert(key);
        }
      }
      for(auto it=states_.begin();it!=states_.end();) {
        if(!seen.contains(it->first)) it=states_.erase(it);else ++it;
      }
      knownRevision_=world.structuralRevision();
    }
    for(const auto &key:timers_) {
        const auto handle=world.handle(key.first);
        const auto *value=world.readComponent({handle,key.second});
        if(!value || &value->type()!=&scene::Timer::descriptor) continue;
        const auto timer=*static_cast<const scene::Timer *>(value);
        auto &state=ensure(key,timer);
        if(state.interval!=timer.intervalSeconds) {
          state.interval=timer.intervalSeconds;state.remaining=state.running?timer.intervalSeconds:0;
        }
        if(!state.enabled && timer.enabled && timer.autoStart) {state.remaining=timer.intervalSeconds;state.completed=false;state.running=true;}
        state.enabled=timer.enabled;
        const auto target=world.handle(static_cast<ObjectId>(timer.elapsedTarget));
        state.connection=timer.elapsedAction==0 ? ConnectionStatus::Disconnected :
            world.alive(target)?ConnectionStatus::Ready:ConnectionStatus::MissingTarget;
        const double tick=timer.ignoreTimeScale?unscaledDelta:delta;
        if(!world.activeInHierarchy(handle) || !timer.enabled || !state.running || state.paused || state.completed || tick==0) continue;
        state.remaining-=tick;
        if(state.remaining>0) continue;
        // Preserve elapsed ticks after a long frame; one callback carries the
        // count instead of flooding the managed boundary with identical events.
        const auto count=timer.repeat ?
            static_cast<u32>(1+std::floor(-state.remaining/static_cast<double>(timer.intervalSeconds))) : 1u;
        if(timer.repeat) state.remaining+=static_cast<double>(count)*timer.intervalSeconds;
        else {state.remaining=0;state.completed=true;state.running=false;}
        // No retained callable or raw object pointer. Resolve every receiver by the
        // current world handle; pending removals cannot receive an activation.
        if(state.connection==ConnectionStatus::Ready) {
          if(applyObjectActivationConnection(world,static_cast<ObjectId>(timer.elapsedTarget),timer.elapsedAction,count)!=WorldStatus::Ok)
            state.connection=ConnectionStatus::Rejected;
        }
        if(!fire(key.first,timer.instanceId(),count)) return false;
    }
    return true;
  }
private:
  using Key=std::pair<ObjectId,u64>;
  State &ensure(const Key &key,const scene::Timer &timer) {
    auto [it,created]=states_.try_emplace(key);
    if(created) {auto &state=it->second;state.remaining=timer.autoStart?timer.intervalSeconds:0;
      state.interval=timer.intervalSeconds;state.enabled=timer.enabled;state.running=timer.autoStart;}
    return it->second;
  }
  std::map<Key,State> states_;
  std::vector<Key> timers_;
  u64 knownRevision_=0;
};
}
