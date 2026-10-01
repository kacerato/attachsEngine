#pragma once
#include "runtime/game_world.h"
#include <map>
#include <string>

namespace ae::runtime {
// Runtime-created tracks. Handles bind to a world, component and persistent PropertyId.
class SceneNumberTweens final {
public:
  enum class Status : u32 {Running,Completed,Cancelled,Failed};
  struct State {Status status=Status::Running;WorldStatus failure=WorldStatus::Ok;double elapsed=0;float value=0,duration=0;bool paused=false,active=false,enabled=false;};
  static constexpr u32 MaximumTracks=256;
  void reset(){tracks_.clear();world_=0;next_=0;}
  WorldStatus create(GameWorld &world,ComponentHandle target,std::string_view property,float destination,float duration,u32 easing,bool unscaled,u64 &id) {
    id=0;if(!world.running())return WorldStatus::NotRunning;
    if(property.empty()||property.size()>127||property.find('\0')!=std::string_view::npos||!std::isfinite(duration)||duration<.001f||duration>36000||easing>3)return WorldStatus::InvalidArgument;
    float initial=0;const auto result=world.validateTweenNumber(target,property,destination,initial);if(result!=WorldStatus::Ok)return result;
    if(world_!=world.worldId()){reset();world_=world.worldId();}
    if(tracks_.size()>=MaximumTracks||next_==std::numeric_limits<u32>::max())return WorldStatus::LimitReached;
    for(const auto &[key,track]:tracks_){(void)key;if(track.state.status==Status::Running&&track.target.object==target.object&&track.target.instance==target.instance&&track.property==property)return WorldStatus::PropertyAlreadyTweening;}
    Track track;track.target=target;track.property=property;track.initial=initial;track.destination=destination;track.easing=easing;track.unscaled=unscaled;track.state.value=initial;track.state.duration=duration;observe(world,track);
    id=(static_cast<u64>(world_)<<32)|++next_;tracks_.emplace(id,std::move(track));return WorldStatus::Ok;
  }
  // 0 snapshot, 1 pause, 2 resume, 3 cancel, 4 release retained state.
  WorldStatus command(GameWorld &world,u64 id,u32 operation,State &out) {
    if(!world.running())return WorldStatus::NotRunning;
    if(static_cast<u32>(id>>32)!=world.worldId()||world_!=world.worldId())return WorldStatus::ForeignWorld;
    if(operation>4)return WorldStatus::InvalidArgument;
    const auto it=tracks_.find(id);if(it==tracks_.end())return WorldStatus::UnknownElement;
    auto &track=it->second;observe(world,track);
    if(track.state.status==Status::Running){float value=0;const auto valid=world.validateTweenNumber(track.target,track.property,track.destination,value);if(valid!=WorldStatus::Ok)fail(track,valid);}
    if(operation==1)track.state.paused=true;
    else if(operation==2)track.state.paused=false;
    else if(operation==3&&track.state.status==Status::Running)track.state.status=Status::Cancelled;
    out=track.state;if(operation==4)tracks_.erase(it);return WorldStatus::Ok;
  }
  bool advance(GameWorld &world,double scaled,double unscaled) {
    if(!world.running()||!std::isfinite(scaled)||!std::isfinite(unscaled)||scaled<0||scaled>.25||unscaled<0||unscaled>.25)return false;
    if(world_!=world.worldId()){reset();world_=world.worldId();}
    for(auto &[id,track]:tracks_) {
      (void)id;if(track.state.status!=Status::Running)continue;
      const auto valid=world.validate(track.target.object);
      if(valid!=WorldStatus::Ok){fail(track,valid);continue;}
      float current=0;const auto eligible=world.validateTweenNumber(track.target,track.property,track.destination,current);
      if(eligible!=WorldStatus::Ok){fail(track,eligible);continue;}
      observe(world,track);if(track.state.paused||!track.state.active||!track.state.enabled)continue;
      if(current!=track.state.value){fail(track,WorldStatus::PropertyWrittenExternally);continue;}
      const double delta=track.unscaled?unscaled:scaled;if(delta==0)continue;
      const double elapsed=std::min(track.state.elapsed+delta,static_cast<double>(track.state.duration));
      const float t=static_cast<float>(elapsed/track.state.duration);
      const float blend=track.easing==1?t*t*(3-2*t):track.easing==2?t*t:track.easing==3?1-(1-t)*(1-t):t;
      const float value=t>=1?track.destination:track.initial+(track.destination-track.initial)*blend;
      const auto applied=world.setTweenNumber(track.target,track.property,value);
      if(applied!=WorldStatus::Ok){fail(track,applied);continue;}
      track.state.elapsed=elapsed;track.state.value=value;if(t>=1)track.state.status=Status::Completed;
    }
    return true;
  }
private:
  struct Track {ComponentHandle target;std::string property;float initial=0,destination=0;u32 easing=0;bool unscaled=false;State state;};
  static void fail(Track &track,WorldStatus result){track.state.status=Status::Failed;track.state.failure=result;}
  static void observe(const GameWorld &world,Track &track){track.state.active=world.activeInHierarchy(track.target.object);const auto*value=world.readComponent(track.target);track.state.enabled=value!=nullptr;if(value)for(const auto &p:value->type().booleans)if(p.id=="enabled")track.state.enabled=p.read(*value);}
  std::map<u64,Track> tracks_;u32 world_=0,next_=0;
};
}
