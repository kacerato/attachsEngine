#include "runtime/component_operations.h"

#include "runtime/scene_audio.h"
#include "runtime/scene_paths.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_tweens.h"
#include "runtime/scene_tween_sequences.h"
#include "runtime/scene_physics_queries.h"
#include "runtime/scene_virtual_cameras.h"
#include "scene/audio_mixer.h"
#include "runtime/scene_animator_graph.h"
#include "scene/animator.h"
#include "scene/audio.h"
#include "scene/component_schema.h"
#include "scene/path_follow.h"
#include "scene/timer.h"
#include "scene/transform_tween.h"
#include "scene/property_tween.h"
#include "scene/tween_sequence.h"
#include "scene/virtual_camera.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {
namespace {
using Value=scene::ComponentOperationValue;
using Arguments=std::span<const Value>;
WorldStatus animatorLayerOperation(const ComponentOperationServices &s,ComponentHandle h,Arguments args,Value &result,u32 operation,u32 field=0) {
  if(!s.animators||!s.world) return WorldStatus::NotRunning;
  if(args.empty()||args[0].integer<0||args[0].integer>std::numeric_limits<u32>::max()) return WorldStatus::InvalidArgument;
  const auto status=s.world->validate(h.object);if(status!=WorldStatus::Ok) return status;
  float value=0;resources::AssetGuid reference{};
  if(operation==1||operation==3) value=static_cast<float>(args[1].number);
  if(operation==2) value=static_cast<float>(args[1].integer);
  if(operation==4) reference={static_cast<u64>(args[1].integer),static_cast<u64>(args[2].integer)};
  SceneAnimatorGraphs::LayerSettings info;
  const auto changed=s.animators->layerControl(*s.world,h.object.id,h.instance,static_cast<u32>(args[0].integer),operation,value,reference,info);
  if(changed!=WorldStatus::Ok) return changed;
  if(field==1) result=Value::makeNumber(info.weight);
  if(field==2) result=Value::makeInteger(static_cast<i64>(info.blend));
  if(field==3) result=Value::makeNumber(info.referenceTime);
  if(field==4) result=Value::makeInteger(static_cast<i64>(info.reference.high));
  if(field==5) result=Value::makeInteger(static_cast<i64>(info.reference.low));
  return WorldStatus::Ok;
}

// Cada função reutiliza o mesmo serviço chamado pela ABI específica do
// componente; as duas portas produzem o mesmo efeito por construção.
WorldStatus timerOperation(const ComponentOperationServices &s,ComponentHandle h,u32 operation,float seconds,Value *result,int field) {
  if(!s.timers) return WorldStatus::NotRunning;
  SceneTimers::State state;
  const auto status=s.timers->command(*s.world,h,operation,seconds,state);
  if(status!=WorldStatus::Ok) return status;
  if(field==1) *result=Value::makeNumber(state.running?std::max(0.0,state.remaining):0.0);
  else if(field==2) *result=Value::makeBoolean(state.running);
  return WorldStatus::Ok;
}
WorldStatus tweenOperation(const ComponentOperationServices &s,ComponentHandle h,u32 operation,Value *result) {
  if(!s.tweens) return WorldStatus::NotRunning;
  SceneTweens::State state;
  const auto status=s.tweens->command(*s.world,h,operation,state);
  if(status==WorldStatus::Ok && result) *result=Value::makeNumber(state.elapsed);
  return status;
}
WorldStatus sequenceOperation(const ComponentOperationServices &s,ComponentHandle h,u32 operation,Value *result) {
  if(!s.sequences || !s.tweens) return WorldStatus::NotRunning;
  SceneTweenSequences::State state;
  const auto status=s.sequences->command(*s.world,*s.tweens,h,operation,state);
  if(status==WorldStatus::Ok && result) {
    using S=SceneTweenSequences::Status;
    const bool active=state.status==S::Interval||state.status==S::Running;
    *result=Value::makeInteger(active?state.step+1:0);
  }
  return status;
}
WorldStatus queryOperation(const ComponentOperationServices &s,ComponentHandle h,std::string_view method,Value &result) {
  if(!s.queries || !s.physics) return WorldStatus::NotRunning;
  return s.queries->command(*s.world,*s.physics,h,method,result);
}
WorldStatus audioOperation(const ComponentOperationServices &s,ComponentHandle h,SceneAudio::Command command,double seconds=0) {
  if(!s.audio) return WorldStatus::NotRunning;
  return s.audio->command(*s.world,h,command,seconds);
}
WorldStatus pathOperation(const ComponentOperationServices &s,ComponentHandle h,u32 operation,Value &result) {
  if(!s.paths) return WorldStatus::NotRunning;
  bool ok=false;
  if(operation==0) ok=s.paths->restart(*s.world,h.object.id);
  else if(operation==1) ok=s.paths->stop(*s.world,h.object.id);
  else if(operation==2) {double distance=0;ok=s.paths->progress(*s.world,h.object.id,distance);if(ok)result=Value::makeNumber(distance);}
  else {bool playing=false;ok=s.paths->playing(*s.world,h.object.id,playing);if(ok)result=Value::makeBoolean(playing);}
  return ok?WorldStatus::Ok:WorldStatus::Rejected;
}

WorldStatus cameraOperation(const ComponentOperationServices &s,ComponentHandle h,std::string_view method,Value &result) {
  if(!s.cameras) return WorldStatus::NotRunning;
  return s.cameras->command(*s.world,h,method,result);
}

const std::array<ComponentMethodBinding,64> bindings{{
  {&scene::Timer::descriptor,"start",[](const ComponentOperationServices &s,ComponentHandle h,Arguments a,Value &){
    const double seconds=a[0].number;
    if(!std::isfinite(seconds) || seconds<0) return WorldStatus::InvalidArgument;
    return timerOperation(s,h,1,static_cast<float>(seconds),nullptr,0);}},
  {&scene::Timer::descriptor,"stop",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return timerOperation(s,h,2,0,nullptr,0);}},
  {&scene::Timer::descriptor,"pause",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return timerOperation(s,h,3,0,nullptr,0);}},
  {&scene::Timer::descriptor,"resume",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return timerOperation(s,h,4,0,nullptr,0);}},
  {&scene::Timer::descriptor,"remaining",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return timerOperation(s,h,0,0,&r,1);}},
  {&scene::Timer::descriptor,"running",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return timerOperation(s,h,0,0,&r,2);}},
  {&scene::TransformTween::descriptor,"restart",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,1,nullptr);}},
  {&scene::TransformTween::descriptor,"cancel",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,2,nullptr);}},
  {&scene::TransformTween::descriptor,"pause",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,3,nullptr);}},
  {&scene::TransformTween::descriptor,"resume",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,4,nullptr);}},
  {&scene::TransformTween::descriptor,"elapsed",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return tweenOperation(s,h,0,&r);}},
  {&scene::PropertyTween::descriptor,"restart",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,1,nullptr);}},
  {&scene::PropertyTween::descriptor,"cancel",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,2,nullptr);}},
  {&scene::PropertyTween::descriptor,"pause",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,3,nullptr);}},
  {&scene::PropertyTween::descriptor,"resume",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return tweenOperation(s,h,4,nullptr);}},
  {&scene::PropertyTween::descriptor,"elapsed",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return tweenOperation(s,h,0,&r);}},
  {&scene::RayCast::descriptor,"colliding",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"colliding",r);}},
  {&scene::RayCast::descriptor,"collider",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"collider",r);}},
  {&scene::RayCast::descriptor,"point",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"point",r);}},
  {&scene::RayCast::descriptor,"normal",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"normal",r);}},
  {&scene::RayCast::descriptor,"distance",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"distance",r);}},
  {&scene::RayCast::descriptor,"update",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"update",r);}},
  {&scene::ShapeCast::descriptor,"colliding",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"colliding",r);}},
  {&scene::ShapeCast::descriptor,"collider",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"collider",r);}},
  {&scene::ShapeCast::descriptor,"point",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"point",r);}},
  {&scene::ShapeCast::descriptor,"normal",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"normal",r);}},
  {&scene::ShapeCast::descriptor,"distance",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"distance",r);}},
  {&scene::ShapeCast::descriptor,"update",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"update",r);}},
  {&scene::SpringArm::descriptor,"hit_length",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return queryOperation(s,h,"hit_length",r);}},
  {&scene::TweenSequence::descriptor,"play",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return sequenceOperation(s,h,1,nullptr);}},
  {&scene::TweenSequence::descriptor,"cancel",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return sequenceOperation(s,h,2,nullptr);}},
  {&scene::TweenSequence::descriptor,"pause",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return sequenceOperation(s,h,3,nullptr);}},
  {&scene::TweenSequence::descriptor,"resume",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return sequenceOperation(s,h,4,nullptr);}},
  {&scene::TweenSequence::descriptor,"step",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return sequenceOperation(s,h,0,&r);}},
  {&scene::AudioSource::descriptor,"play",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return audioOperation(s,h,SceneAudio::Command::Play);}},
  {&scene::AudioSource::descriptor,"pause",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return audioOperation(s,h,SceneAudio::Command::Pause);}},
  {&scene::AudioSource::descriptor,"resume",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return audioOperation(s,h,SceneAudio::Command::Resume);}},
  {&scene::AudioSource::descriptor,"stop",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){return audioOperation(s,h,SceneAudio::Command::Stop);}},
  {&scene::AudioSource::descriptor,"seek",[](const ComponentOperationServices &s,ComponentHandle h,Arguments a,Value &){
    if(!std::isfinite(a[0].number) || a[0].number<0) return WorldStatus::InvalidArgument;
    return audioOperation(s,h,SceneAudio::Command::Seek,a[0].number);}},
  {&scene::AudioSource::descriptor,"is_virtual",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){
    if(!s.audio) return WorldStatus::NotRunning;
    const auto status=s.world->validate(h.object);if(status!=WorldStatus::Ok) return status;
    r=Value::makeBoolean(s.audio->isVirtual(h.object.id,h.instance));return WorldStatus::Ok;}},
  {&scene::AudioBus::descriptor,"peak_db",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){
    if(!s.audio) return WorldStatus::NotRunning;
    const auto status=s.world->validate(h.object);if(status!=WorldStatus::Ok) return status;
    const auto *meter=s.audio->busMeter(h.object.id);
    r=Value::makeNumber(meter&&meter->peak>1e-6f?20*std::log10(meter->peak):-120);return WorldStatus::Ok;}},
  {&scene::AudioCompressor::descriptor,"reduction_db",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){
    if(!s.audio) return WorldStatus::NotRunning;
    const auto status=s.world->validate(h.object);if(status!=WorldStatus::Ok) return status;
    r=Value::makeNumber(s.audio->compressorReduction(h.object.id,h.instance));return WorldStatus::Ok;}},
  {&scene::Animator::descriptor,"in_transition",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){
    if(!s.animators) return WorldStatus::NotRunning;
    const auto status=s.world->validate(h.object);if(status!=WorldStatus::Ok) return status;
    SceneAnimatorGraphs::Info info;
    if(s.animators->info(*s.world,h.object.id,h.instance,0,info)!=SceneAnimatorGraphs::Status::Ok) return WorldStatus::ComponentUnavailable;
    r=Value::makeBoolean(info.transitioning);return WorldStatus::Ok;}},
  {&scene::Animator::descriptor,"get_layer_weight",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,0,1);}},
  {&scene::Animator::descriptor,"set_layer_weight",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,1);}},
  {&scene::Animator::descriptor,"get_layer_blend",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,0,2);}},
  {&scene::Animator::descriptor,"set_layer_blend",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,2);}},
  {&scene::Animator::descriptor,"get_layer_reference_time",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,0,3);}},
  {&scene::Animator::descriptor,"set_layer_reference_time",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,3);}},
  {&scene::Animator::descriptor,"set_layer_reference",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,4);}},
  {&scene::Animator::descriptor,"get_layer_reference_high",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,0,4);}},
  {&scene::Animator::descriptor,"get_layer_reference_low",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,0,5);}},
  {&scene::Animator::descriptor,"reset_layer_overrides",[](const auto &s,ComponentHandle h,Arguments a,Value &r){return animatorLayerOperation(s,h,a,r,5);}},
  {&scene::AudioSnapshot::descriptor,"transition_to",[](const ComponentOperationServices &s,ComponentHandle h,Arguments a,Value &){
    if(!s.audio) return WorldStatus::NotRunning;
    return s.audio->transitionSnapshot(*s.world,h,a[0].number);}},
  {&scene::AudioSnapshot::descriptor,"apply",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &){
    if(!s.audio) return WorldStatus::NotRunning;
    return s.audio->transitionSnapshot(*s.world,h,0);}},
  {&scene::PathFollow::descriptor,"restart",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return pathOperation(s,h,0,r);}},
  {&scene::PathFollow::descriptor,"stop",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return pathOperation(s,h,1,r);}},
  {&scene::PathFollow::descriptor,"progress",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return pathOperation(s,h,2,r);}},
  {&scene::PathFollow::descriptor,"playing",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return pathOperation(s,h,3,r);}},
  {&scene::VirtualCamera::descriptor,"prioritize",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return cameraOperation(s,h,"prioritize",r);}},
  {&scene::VirtualCamera::descriptor,"snap",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return cameraOperation(s,h,"snap",r);}},
  {&scene::VirtualCamera::descriptor,"is_live",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return cameraOperation(s,h,"is_live",r);}},
  {&scene::CameraBrain::descriptor,"live_camera",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return cameraOperation(s,h,"live_camera",r);}},
  {&scene::CameraBrain::descriptor,"blending",[](const ComponentOperationServices &s,ComponentHandle h,Arguments,Value &r){return cameraOperation(s,h,"blending",r);}},
}};

const ComponentMethodBinding *findBinding(const scene::ComponentType &type,std::string_view method) {
  for(const auto &binding:bindings) if(binding.type==&type && binding.method==method) return &binding;
  return nullptr;
}
bool payloadMatches(std::span<const scene::ComponentParameter> declared,std::span<const Value> values) {
  if(declared.size()!=values.size()) return false;
  for(usize i=0;i<declared.size();++i) {
    if(values[i].valueKind()!=declared[i].kind) return false;
    if(declared[i].kind==scene::ComponentValueKind::Number && !std::isfinite(values[i].number)) return false;
    if(declared[i].kind==scene::ComponentValueKind::Vector3 &&
       !(std::isfinite(values[i].vector[0])&&std::isfinite(values[i].vector[1])&&std::isfinite(values[i].vector[2]))) return false;
    if(declared[i].kind==scene::ComponentValueKind::Boolean && values[i].boolean>1) return false;
  }
  return true;
}
} // namespace

std::span<const ComponentMethodBinding> componentMethodBindings() {return bindings;}

WorldStatus invokeComponentMethod(const ComponentOperationServices &services,ComponentHandle component,
                                  std::string_view method,std::span<const Value> arguments,Value &result) {
  result=Value{};
  if(!services.world || !services.world->running()) return WorldStatus::NotRunning;
  const auto valid=services.world->validate(component.object);
  if(valid!=WorldStatus::Ok) return valid;
  const auto *value=services.world->readComponent(component);
  if(!value) return WorldStatus::ComponentMissing;
  const auto &type=value->type();
  const auto *declared=scene::findComponentMethod(type,method);
  if(!declared) return WorldStatus::UnknownOperation;
  if(!payloadMatches(declared->parameters,arguments)) return WorldStatus::InvalidArgument;
  const auto *binding=findBinding(type,method);
  if(!binding || !binding->invoke) return WorldStatus::UnknownOperation;
  const auto status=binding->invoke(services,component,arguments,result);
  if(status!=WorldStatus::Ok) {result=Value{};return status;}
  // O retorno também é contrato: uma função que devolve outro tipo é defeito.
  if(result.valueKind()!=declared->result) {result=Value{};return WorldStatus::Rejected;}
  return WorldStatus::Ok;
}

std::vector<std::string> auditComponentOperations() {
  std::vector<std::string> issues;
  for(const auto &schema:scene::componentSchemas) {
    const auto &type=*schema.type;
    for(usize i=0;i<type.methods.size();++i) {
      const auto &method=type.methods[i];
      if(method.id.empty()) issues.push_back(std::string(type.id)+": método sem identidade");
      for(usize j=0;j<i;++j) if(type.methods[j].id==method.id) issues.push_back(std::string(type.id)+"."+std::string(method.id)+": repetido");
      usize found=0;
      for(const auto &binding:bindings) if(binding.type==&type && binding.method==method.id) ++found;
      if(found!=1) issues.push_back(std::string(type.id)+"."+std::string(method.id)+": "+std::to_string(found)+" implementações");
    }
    for(usize i=0;i<type.events.size();++i) {
      const auto &event=type.events[i];
      if(event.id.empty()) issues.push_back(std::string(type.id)+": evento sem identidade");
      if(event.payload.size()>scene::kComponentEventPayloadLimit) issues.push_back(std::string(type.id)+"."+std::string(event.id)+": payload acima do limite");
      for(usize j=0;j<i;++j) if(type.events[j].id==event.id) issues.push_back(std::string(type.id)+"."+std::string(event.id)+": repetido");
    }
  }
  for(const auto &binding:bindings) {
    bool registered=false;
    for(const auto &schema:scene::componentSchemas) registered=registered||schema.type==binding.type;
    if(!registered || !scene::findComponentMethod(*binding.type,binding.method))
      issues.push_back(std::string(binding.type->id)+"."+std::string(binding.method)+": implementação sem método declarado");
  }
  return issues;
}

void ComponentEventQueue::reset() {
  records_.clear();next_=1;dropped_=0;cursor_={1,1};attached_={};
}
void ComponentEventQueue::attach(Consumer consumer,bool attached) {
  attached_[index(consumer)]=attached;
  // Um consumidor que chega agora só vê o que acontecer depois.
  if(attached) cursor_[index(consumer)]=next_;
  trim();
}
bool ComponentEventQueue::emit(GameWorld &world,ObjectId object,u64 instance,const scene::ComponentType &type,
                               std::string_view event,std::span<const Value> payload) {
  if(!attached_[0] && !attached_[1]) return true;
  const auto handle=world.handle(object);
  if(world.validate(handle)!=WorldStatus::Ok) return false;
  usize eventIndex=type.events.size();
  for(usize i=0;i<type.events.size();++i) if(type.events[i].id==event) {eventIndex=i;break;}
  if(eventIndex==type.events.size() || !payloadMatches(type.events[eventIndex].payload,payload)) return false;
  if(records_.size()>=Capacity) {records_.pop_front();++dropped_;}
  ComponentEventRecord record;
  record.sequence=next_++;record.object=handle;record.instance=instance;record.type=&type;
  record.event=static_cast<u32>(eventIndex);record.count=static_cast<u32>(payload.size());
  std::copy(payload.begin(),payload.end(),record.values.begin());
  records_.push_back(record);
  return true;
}
usize ComponentEventQueue::pending(Consumer consumer) const noexcept {
  return next_>cursor_[index(consumer)]?static_cast<usize>(next_-cursor_[index(consumer)]):0;
}
void ComponentEventQueue::trim() {
  u64 floor=next_;
  for(usize i=0;i<attached_.size();++i) if(attached_[i]) floor=std::min(floor,cursor_[i]);
  while(!records_.empty() && records_.front().sequence<floor) records_.pop_front();
}

} // namespace ae::runtime
