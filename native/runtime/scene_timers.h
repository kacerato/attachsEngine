#pragma once
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
  struct State {double remaining=0;float interval=0;bool enabled=false;bool completed=false;};
  void reset() {states_.clear();timers_.clear();knownRevision_=0;}
  const State *state(ObjectId object,u64 instance) const {
    const auto it=states_.find({object,instance});return it==states_.end()?nullptr:&it->second;
  }
  template<class Callback> bool advance(GameWorld &world,double delta,Callback fire) {
    if(!world.running() || !std::isfinite(delta) || delta<0 || delta>0.25) return false;
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
        auto [it,created]=states_.try_emplace(key,State{timer.intervalSeconds,timer.intervalSeconds,timer.enabled,false});
        auto &state=it->second;
        if(!created && state.interval!=timer.intervalSeconds) {
          state.interval=timer.intervalSeconds;state.remaining=timer.intervalSeconds;
        }
        if(!state.enabled && timer.enabled) {state.remaining=timer.intervalSeconds;state.completed=false;}
        state.enabled=timer.enabled;
        if(!world.activeInHierarchy(handle) || !timer.enabled || state.completed || delta==0) continue;
        state.remaining-=delta;
        if(state.remaining>0) continue;
        // Preserve elapsed ticks after a long frame; one callback carries the
        // count instead of flooding the managed boundary with identical events.
        const auto count=timer.repeat ?
            static_cast<u32>(1+std::floor(-state.remaining/static_cast<double>(timer.intervalSeconds))) : 1u;
        if(timer.repeat) state.remaining+=static_cast<double>(count)*timer.intervalSeconds;
        else {state.remaining=0;state.completed=true;}
        if(!fire(key.first,timer.instanceId(),count)) return false;
    }
    return true;
  }
private:
  using Key=std::pair<ObjectId,u64>;
  std::map<Key,State> states_;
  std::vector<Key> timers_;
  u64 knownRevision_=0;
};
}
