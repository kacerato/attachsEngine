#include "runtime/scene_paths.h"
#include "scene/path.h"
#include "scene/path_follow.h"
#include "resources/curve3d_transform.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
namespace ae::runtime {
namespace {
using V=std::array<float,3>;
bool equalCurve(const resources::Curve3D&a,const resources::Curve3D&b){return a.closed==b.closed&&a.up==b.up&&a.points==b.points;}
bool enabled(const scene::ComponentValue&c){for(const auto &p:c.type().booleans)if(p.id=="enabled")return p.read(c);return true;}
bool competing(const SceneGraph&graph,ObjectId object){
 const auto *owner=graph.find(object);if(!owner)return true;
 for(usize i=0;i<owner->components.size();++i){const auto &c=*owner->components.at(i);const auto id=c.type().id;
  if(id=="astra.physics2d.body"||id=="astra.physics.body"||id=="astra.physics.character")return true;
  if(id=="astra.tween.transform"||id=="astra.camera.follow"||id=="astra.camera.look"||id=="astra.constraint.position"||id=="astra.constraint.rotation"||id=="astra.constraint.scale"||id=="astra.constraint.aim"||id=="astra.constraint.parent"||id=="astra.constraint.look_at"){
   if(!enabled(c))continue;
   // Zero-weight constraints still publish a baseline pose in their consumer.
   return true;
  }
 }
 // Animation can target descendants by imported identity or name. Conservatively
 // reject the entire enabled animation subtree rather than overwrite a channel.
 for(auto id=object;id;){const auto *o=graph.find(id);if(!o)break;for(usize i=0;i<o->components.size();++i){const auto &c=*o->components.at(i);if(c.type().id=="astra.animation"&&enabled(c))return true;}id=o->parent;}
 return false;
}
}
const char *ScenePaths::issueText(Issue issue){switch(issue){case Issue::MissingPath:return "Caminho ausente ou inativo";case Issue::InvalidCurve:return "Curva vazia, degenerada ou excede limite de bake";case Issue::Cycle:return "Ciclo entre caminho, seguidor e hierarquia";case Issue::Authority:return "Pose controlada pela física";case Issue::CompetingWriter:return "Pose possui outro escritor ativo";case Issue::VerticalTangent:return "Tangente paralela à vertical +Y";case Issue::InvalidPose:return "Transformação mundial não representável";case Issue::DependencyLimit:return "Dependência excede 256 seguidores";}return "Caminho inválido";}
void ScenePaths::reset(){curves_.clear();followers_.clear();candidates_.clear();diagnostics_.clear();world_=0;revision_=0;}
bool ScenePaths::prepare(GameWorld&world,ObjectId path){
 const auto handle=world.handle(path);
 const auto *o=world.find(handle);
 if(!o){curves_.erase(path);return false;}
 if(!world.activeInHierarchy(handle))return false;
 const auto *component=o->components.find(scene::Path::descriptor);
 if(!component){curves_.erase(path);return false;}
 const auto &curve=static_cast<const scene::Path&>(*component).curve;
 Transform pose;if(world.worldTransform(handle,pose)!=WorldStatus::Ok)return false;std::array<float,16>matrix;transformMatrix(pose,matrix.data());
 auto &cache=curves_[path];
 // Cache failure as well as success: unchanged invalid curves must not spend
 // the adaptive subdivision budget on every follower, frame and Inspector query.
 if(cache.attempted&&cache.matrix==matrix&&equalCurve(cache.source,curve))return cache.ready;
 const auto transformed=resources::transformedCurve3D(curve,matrix);
 cache.source=curve;
 cache.matrix=matrix;
 cache.attempted=true;
 cache.ready=false;
 if(!cache.baked.bake(transformed,.01)||cache.baked.length()<1e-7)return false;
 cache.ready=true;
 return true;
}
bool ScenePaths::sample(GameWorld&world,ObjectId path,double distance,V&position,V&tangent,bool wrap){if(!world.running()||!std::isfinite(distance))return false;if(world_!=world.worldId()){reset();world_=world.worldId();}return prepare(world,path)&&curves_.at(path).baked.sampleDistance(distance,position,tangent,wrap);}
bool ScenePaths::sampleFrame(GameWorld&world,ObjectId path,double distance,resources::BakedCurve3D::Frame&frame,bool wrap){if(!world.running()||!std::isfinite(distance))return false;if(world_!=world.worldId()){reset();world_=world.worldId();}return prepare(world,path)&&curves_.at(path).baked.sampleFrame(distance,frame,wrap);}
ScenePaths::FollowState *ScenePaths::ensureFollower(GameWorld &world,ObjectId object){
 if(!world.running())return nullptr;
 if(world_!=world.worldId()){reset();world_=world.worldId();}
 const auto *owner=world.find(world.handle(object));
 const auto *component=owner?owner->components.find(scene::PathFollow::descriptor):nullptr;
 if(!component)return nullptr;
 const auto &c=static_cast<const scene::PathFollow&>(*component);
 auto &state=followers_[object];
 const bool changed=state.initialized&&(state.instance!=c.instanceId()||state.target!=c.target||state.initial!=c.progressDistance);
 if(!state.initialized||changed){
  state={c.instanceId(),c.target,c.progressDistance,c.progressDistance,c.autoplay,true,changed};
 }
 return &state;
}
bool ScenePaths::restart(GameWorld&world,ObjectId object){auto *state=ensureFollower(world,object);if(!state)return false;state->distance=state->initial;state->playing=true;state->pendingReset=false;return true;}
bool ScenePaths::stop(GameWorld&world,ObjectId object){auto *state=ensureFollower(world,object);if(!state)return false;state->playing=false;return true;}
bool ScenePaths::length(GameWorld&world,ObjectId path,double&out){V position,tangent;if(!sample(world,path,0,position,tangent,false))return false;out=curves_.at(path).baked.length();return true;}
bool ScenePaths::progress(GameWorld &world,ObjectId object,double &distance){
 auto *state=ensureFollower(world,object);
 if(!state)return false;
 distance=state->distance;
 return true;
}
bool ScenePaths::playing(GameWorld &world,ObjectId object,bool &value){
 auto *state=ensureFollower(world,object);
 if(!state)return false;
 value=state->playing;
 return true;
}
bool ScenePaths::advance(GameWorld &world,double seconds){
 if(!world.running()||!std::isfinite(seconds)||seconds<0)return false;
 if(world_!=world.worldId()){reset();world_=world.worldId();}
 diagnostics_.clear();
 if(revision_!=world.structuralRevision()){
  candidates_.clear();
  std::vector<ObjectId> objects;
  world.graph().collectSubtree(world.graph().root(),objects);
  for(auto id:objects){
   const auto *object=world.graph().find(id);
   if(object&&object->components.find(scene::PathFollow::descriptor))candidates_.push_back(id);
  }
  revision_=world.structuralRevision();
  std::set<ObjectId> alive(candidates_.begin(),candidates_.end());
  for(auto it=followers_.begin();it!=followers_.end();){
   if(!alive.contains(it->first))it=followers_.erase(it);
   else ++it;
  }
  for(auto it=curves_.begin();it!=curves_.end();){
   const auto *object=world.graph().find(it->first);
   if(!object||!object->components.find(scene::Path::descriptor))it=curves_.erase(it);
   else ++it;
  }
 }
 std::map<ObjectId,u32> states;
 u32 depth=0;
 std::function<bool(ObjectId)> visit=[&](ObjectId object){
  if(states[object]==2)return true;
  if(states[object]==3)return false;
  auto fail=[&](Issue issue){
   diagnostics_.push_back({object,issue});
   states[object]=3;
   return false;
  };
  if(states[object]==1)return fail(Issue::Cycle);
  if(depth>=256)return fail(Issue::DependencyLimit);
  const auto *owner=world.graph().find(object);
  const auto *component=owner?owner->components.find(scene::PathFollow::descriptor):nullptr;
  if(!component){states[object]=2;return true;}
  const auto &c=static_cast<const scene::PathFollow&>(*component);
  auto &state=followers_[object];
  const bool changed=state.initialized&&(state.instance!=c.instanceId()||
                     state.target!=c.target||state.initial!=c.progressDistance);
  const bool retargeted=state.pendingReset||changed;
  if(!state.initialized||changed){
   state={c.instanceId(),c.target,c.progressDistance,c.progressDistance,c.autoplay,true,retargeted};
  }
  if(!c.enabled||!world.activeInHierarchy(world.handle(object))){states[object]=2;return true;}
  states[object]=1;
  ++depth;
  struct Depth {u32 &value;~Depth(){--value;}} guard{depth};
  const auto target=static_cast<ObjectId>(c.target);
  if(target==object||world.graph().isDescendantOf(target,object))return fail(Issue::Cycle);
  auto dependencies=[&](ObjectId id){
   for(auto ancestor=id;ancestor;){
    const auto *value=world.graph().find(ancestor);
    if(!value)break;
    if(value->components.find(scene::PathFollow::descriptor)&&!visit(ancestor))return false;
    ancestor=value->parent;
   }
   return true;
  };
  if(!dependencies(owner->parent)||!dependencies(target)){states[object]=3;return false;}
  if(world.authorityOf(world.handle(object))!=TransformAuthority::Free)return fail(Issue::Authority);
  if(competing(world.graph(),object))return fail(Issue::CompetingWriter);
  const auto *path=world.graph().find(target);
  if(!path||!path->components.find(scene::Path::descriptor)||
     !world.activeInHierarchy(world.handle(target)))return fail(Issue::MissingPath);
  if(!prepare(world,target))return fail(Issue::InvalidCurve);
  const auto &baked=curves_.at(target).baked;
  const double length=baked.length();
  double distance=state.distance;
  if(state.playing&&!retargeted){
   const double speed=c.mode==scene::PathFollowMode::Speed?c.speed:length/c.duration;
   distance+=(c.backwards?-1:1)*seconds*speed;
  }
  if(!std::isfinite(distance))return fail(Issue::InvalidPose);
  if(c.loop){distance=std::fmod(distance,length);if(distance<0)distance+=length;}
  else distance=std::clamp(distance,0.0,length);
  resources::BakedCurve3D::Frame frame;
  if(!baked.sampleFrame(distance,frame))return fail(Issue::InvalidCurve);
  const V position=frame.position,up=frame.up;
  V tangent=frame.tangent,right=frame.right;
  if(c.backwards){for(auto &value:tangent)value=-value;for(auto &value:right)value=-value;}
  Transform pose;
  if(world.worldTransform(world.handle(object),pose)!=WorldStatus::Ok)return fail(Issue::InvalidPose);
  for(u32 axis=0;axis<3;++axis){
   pose.position[axis]=position[axis]+right[axis]*c.offset[0]+
                       up[axis]*c.offset[1]+tangent[axis]*c.offset[2];
  }
  if(c.orient){
   float matrix[16]{},identity[16]{};
   matrix[15]=1;
   for(u32 axis=0;axis<4;++axis)identity[axis*5]=1;
   for(u32 axis=0;axis<3;++axis){
    matrix[axis]=right[axis];
    matrix[4+axis]=up[axis];
    matrix[8+axis]=tangent[axis];
   }
   Transform rotation;
   if(!localTransformForWorld(matrix,identity,rotation))return fail(Issue::InvalidPose);
   for(u32 axis=0;axis<3;++axis)pose.rotationDegrees[axis]=rotation.rotationDegrees[axis];
  }
  const auto status=world.setWorldTransform(world.handle(object),pose);
  if(status!=WorldStatus::Ok){
   return fail(status==WorldStatus::TransformOwnedByPhysics?Issue::Authority:Issue::InvalidPose);
  }
  state.distance=distance;
  state.pendingReset=false;
  states[object]=2;
  return true;
 };
 for(auto id:candidates_)visit(id);
 return true;
}
}
