#pragma once
#include "runtime/game_world.h"
#include "scene/transform_constraints.h"
#include "scene/spring_constraint.h"
#include <map>
#include <set>
#include <functional>
#include <cmath>
namespace ae::runtime {
// Runs after animation/physics/LateUpdate, before camera-follow. Animation supplies
// the unconstrained input pose each frame; physics/character retain authority.
class SceneConstraints final {
public:
 enum class Issue { MissingSource, Cycle, DependencyLimit, Authority, DegenerateAim, InvalidPose };
 struct Diagnostic {ObjectId object;Issue issue;};
 const std::vector<Diagnostic> &diagnostics() const {return diagnostics_;}
 static const char *issueText(Issue issue) {
  switch(issue){case Issue::MissingSource:return "Fonte ausente ou inativa";case Issue::Cycle:return "Ciclo entre fontes ou hierarquia";case Issue::DependencyLimit:return "Cadeia de dependência excede 256";case Issue::Authority:return "Pose controlada pela física";case Issue::DegenerateAim:return "Mira coincidente ou paralela à vertical";case Issue::InvalidPose:return "Transformação não pode ser aplicada";}return "Restrição inválida";
 }
 void reset(){baseline_.clear();springs_.clear();diagnostics_.clear();candidates_.clear();knownRevision_=0;knownWorld_=0;}
 bool advance(GameWorld &world,double elapsed=0) {
  if(!std::isfinite(elapsed)||elapsed<0)return false;
  std::set<std::pair<ObjectId,u64>> liveSprings;
  diagnostics_.clear();if(!world.running())return false;
  if(knownWorld_!=world.worldId()){reset();knownWorld_=world.worldId();}
  if(knownRevision_!=world.structuralRevision()) {
   candidates_.clear();std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
   for(auto id:ids){const auto *o=world.graph().find(id);if(!o)continue;for(const auto *type:types())if(o->components.find(*type)){candidates_.push_back(id);break;}}
   knownRevision_=world.structuralRevision();
  }
  std::map<ObjectId,std::vector<const scene::ComponentValue*>> jobs;
  for(auto id:candidates_){const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
   for(const auto *type:types()){const auto *c=o->components.find(*type);if(c&&active(*c))jobs[id].push_back(c);}}
  std::map<ObjectId,u32> state;std::set<ObjectId> live;u32 depth=0;
  std::function<bool(ObjectId)> visit=[&](ObjectId id){
   if(state[id]==2)return true;
   if(state[id]==3)return false;
   if(state[id]==1){diagnostics_.push_back({id,Issue::Cycle});return false;}
   if(depth>=256){diagnostics_.push_back({id,Issue::DependencyLimit});state[id]=3;return false;}
   ++depth;struct DepthGuard {u32 &value;~DepthGuard(){--value;}} guard{depth};
   state[id]=1;bool ready=true;
   // Every constrained ancestor affects world space, as does every source and
   // its ancestors. Reject the complete dependent branch when a cycle exists.
   auto dependency=[&](ObjectId dep){for(auto n=dep;n;){if(jobs.count(n)&&!visit(n))ready=false;const auto *o=world.graph().find(n);n=o?o->parent:0;}};
   const auto *owner=world.graph().find(id);dependency(owner->parent);
   for(const auto *c:jobs[id]){if(dispatch(*c,[](const auto &v){return v.weight==0;}))continue;const auto source=target(*c);if(source==id||world.graph().isDescendantOf(source,id)){ready=false;diagnostics_.push_back({id,Issue::Cycle});}else dependency(source);}
   if(!ready){state[id]=3;return false;}
   const auto handle=world.handle(id);
   if(world.authorityOf(handle)!=TransformAuthority::Free){diagnostics_.push_back({id,Issue::Authority});state[id]=2;return true;}
   Transform input;if(world.worldTransform(handle,input)!=WorldStatus::Ok){diagnostics_.push_back({id,Issue::InvalidPose});state[id]=3;return false;}
   auto &cache=baseline_[id];live.insert(id);
   // Compare local authoring input with the last local output. A parent moving
   // is not a new rest pose: deriving rest from last WORLD output would feed
   // last frame's constraint back into the next and make weights creep.
   if(!cache.initialized||!same(owner->transform,cache.outputLocal)){cache.restLocal=owner->transform;for(const auto *c:jobs[id])springs_.erase({id,c->instanceId()});}
   float parent[16],local[16],restWorld[16],identity[16]{};
   for(u32 axis=0;axis<4;++axis)identity[axis*5]=1;
   transformMatrix(cache.restLocal,local);
   if(!parentWorldMatrix(world.graph(),id,parent)) {diagnostics_.push_back({id,Issue::InvalidPose});state[id]=3;return false;}
   multiplyMatrix(parent,local,restWorld);
   if(!localTransformForWorld(restWorld,identity,cache.rest)) {diagnostics_.push_back({id,Issue::InvalidPose});state[id]=3;return false;}
   cache.initialized=true;Transform result=input;bool channelStarted[3][3]{};
   auto restoreChannel=[&](const scene::ComponentValue &c){
    if(c.type().id==scene::ParentConstraint::descriptor.id){const auto&v=static_cast<const scene::ParentConstraint&>(c);for(u32 a=0;a<3;++a){if(v.axis[a]&&!channelStarted[0][a]){result.position[a]=cache.rest.position[a];channelStarted[0][a]=true;}if(v.rotationAxis[a]&&!channelStarted[1][a]){result.rotationDegrees[a]=cache.rest.rotationDegrees[a];channelStarted[1][a]=true;}}return;}
    float *out=nullptr;const float *rest=nullptr;u32 channel=0;
    if(c.type().id==scene::PositionConstraint::descriptor.id||c.type().id==scene::SpringPositionConstraint::descriptor.id){out=result.position;rest=cache.rest.position;channel=0;}
    else if(c.type().id==scene::ScaleConstraint::descriptor.id||c.type().id==scene::SpringScaleConstraint::descriptor.id){out=result.scale;rest=cache.rest.scale;channel=2;}
    else {out=result.rotationDegrees;rest=cache.rest.rotationDegrees;channel=1;}
    for(u32 axis=0;axis<3;++axis)if(!channelStarted[channel][axis]&&dispatch(c,[axis](const auto &v){return v.axis[axis];})){out[axis]=rest[axis];channelStarted[channel][axis]=true;}
   };
   for(const auto *c:jobs[id]) {
    const auto source=target(*c);Transform sourcePose;
    if(dispatch(*c,[](const auto &v){return v.weight==0;})){restoreChannel(*c);continue;}
    if(!source||!world.activeInHierarchy(world.handle(source))||world.worldTransform(world.handle(source),sourcePose)!=WorldStatus::Ok){diagnostics_.push_back({id,Issue::MissingSource});continue;}
    if(c->type().id!=scene::AimConstraint::descriptor.id && c->type().id!=scene::LookAtConstraint::descriptor.id && c->type().id!=scene::ParentConstraint::descriptor.id)restoreChannel(*c);
    if(c->type().id==scene::SpringScaleConstraint::descriptor.id&&(sourcePose.scale[0]<=0||sourcePose.scale[1]<=0||sourcePose.scale[2]<=0)){diagnostics_.push_back({id,Issue::InvalidPose});continue;}
    if(isSpring(*c)) {
     liveSprings.insert({id,c->instanceId()});
     if(c->type().id==scene::SpringPositionConstraint::descriptor.id)applySpring(id,static_cast<const scene::SpringPositionConstraint&>(*c),sourcePose.position,input.position,result.position,elapsed,false,false);
     else if(c->type().id==scene::SpringRotationConstraint::descriptor.id){if(!applySpringRotation(id,static_cast<const scene::SpringRotationConstraint&>(*c),sourcePose,input,result,elapsed))diagnostics_.push_back({id,Issue::InvalidPose});}
     else applySpring(id,static_cast<const scene::SpringScaleConstraint&>(*c),sourcePose.scale,input.scale,result.scale,elapsed,false,true);
    }
    else if(c->type().id==scene::PositionConstraint::descriptor.id)apply(*static_cast<const scene::PositionConstraint*>(c),sourcePose.position,result.position,false);
    else if(c->type().id==scene::ScaleConstraint::descriptor.id)apply(*static_cast<const scene::ScaleConstraint*>(c),sourcePose.scale,result.scale,false);
    else if(c->type().id==scene::RotationConstraint::descriptor.id)apply(*static_cast<const scene::RotationConstraint*>(c),sourcePose.rotationDegrees,result.rotationDegrees,true);
    else if(c->type().id==scene::ParentConstraint::descriptor.id) {
     const auto &v=*static_cast<const scene::ParentConstraint*>(c);Transform basis=sourcePose;for(float &n:basis.scale)n=1;float m[16];transformMatrix(basis,m);Transform offsetPose;for(u32 a=0;a<3;++a)offsetPose.rotationDegrees[a]=v.rotationOffset[a];float offsetMatrix[16],combined[16],identityRotation[16]{};for(u32 a=0;a<4;++a)identityRotation[a*5]=1;transformMatrix(offsetPose,offsetMatrix);multiplyMatrix(m,offsetMatrix,combined);Transform goalRotation;if(!localTransformForWorld(combined,identityRotation,goalRotation)){diagnostics_.push_back({id,Issue::InvalidPose});continue;}
     for(u32 a=0;a<3;++a){if(v.axis[a]){if(!channelStarted[0][a])result.position[a]=cache.rest.position[a];channelStarted[0][a]=true;const float goal=m[12+a]+m[a]*v.offset[0]+m[4+a]*v.offset[1]+m[8+a]*v.offset[2];result.position[a]+=(goal-result.position[a])*v.weight;}
      if(v.rotationAxis[a]){if(!channelStarted[1][a])result.rotationDegrees[a]=cache.rest.rotationDegrees[a];channelStarted[1][a]=true;result.rotationDegrees[a]+=std::remainder(goalRotation.rotationDegrees[a]-result.rotationDegrees[a],360.f)*v.weight;}}
    } else {scene::AimConstraint aim;
     if(c->type().id==scene::AimConstraint::descriptor.id)aim=*static_cast<const scene::AimConstraint*>(c);
     else {const auto &v=*static_cast<const scene::LookAtConstraint*>(c);aim.weight=v.weight;aim.upAxis=v.upAxis;for(u32 a=0;a<3;++a)aim.axis[a]=v.axis[a];}Transform desired;
     if(!aimPose(result,sourcePose,aim,desired)){diagnostics_.push_back({id,Issue::DegenerateAim});continue;}
     if(c->type().id==scene::LookAtConstraint::descriptor.id){Transform roll;roll.rotationDegrees[2]=static_cast<const scene::LookAtConstraint*>(c)->roll;float aimMatrix[16],rollMatrix[16],combined[16],identityRoll[16]{};for(u32 a=0;a<4;++a)identityRoll[a*5]=1;transformMatrix(desired,aimMatrix);transformMatrix(roll,rollMatrix);multiplyMatrix(aimMatrix,rollMatrix,combined);if(!localTransformForWorld(combined,identityRoll,desired)){diagnostics_.push_back({id,Issue::InvalidPose});continue;}}
     restoreChannel(*c);apply(aim,desired.rotationDegrees,result.rotationDegrees,true);}
   }
   if(world.setWorldTransform(handle,result)!=WorldStatus::Ok){diagnostics_.push_back({id,Issue::InvalidPose});cache.initialized=false;for(const auto *c:jobs[id])springs_.erase({id,c->instanceId()});}
   else cache.outputLocal=world.graph().find(id)->transform;
   state[id]=2;return true;
  };
  for(const auto &[id,c]:jobs)visit(id);
  for(auto it=baseline_.begin();it!=baseline_.end();)if(!live.count(it->first))it=baseline_.erase(it);else ++it;
  for(auto it=springs_.begin();it!=springs_.end();)if(!liveSprings.count(it->first))it=springs_.erase(it);else ++it;
  // Invalid drafts are diagnosed and skipped, never abort the whole Play frame.
  return true;
 }
private:
 struct SpringState {float value[3]{},velocity[3]{},quaternion[4]{0,0,0,1};u64 target=0;bool initialized=false;};
 std::map<std::pair<ObjectId,u64>,SpringState> springs_;
 static bool isSpring(const scene::ComponentValue &c){return c.type().id==scene::SpringPositionConstraint::descriptor.id||c.type().id==scene::SpringRotationConstraint::descriptor.id||c.type().id==scene::SpringScaleConstraint::descriptor.id;}
 // Exact solution of y'' + 2*zeta*omega*y' + omega^2*y = 0, constant goal.
 // Bounded work even after a long frame; no dt-dependent Euler instability.
 static void springStep(double &y,double &v,double omega,double zeta,double dt) {
  if(dt==0)return;
  if(std::abs(zeta-1)<1e-5){const double b=v+omega*y,e=std::exp(-omega*dt);v=(v-omega*b*dt)*e;y=(y+b*dt)*e;}
  else if(zeta<1){const double a=zeta*omega,b=omega*std::sqrt(1-zeta*zeta),e=std::exp(-a*dt),sn=std::sin(b*dt),cs=std::cos(b*dt);const double old=y;y=e*(old*cs+(v+a*old)*sn/b);v=e*(v*cs-(a*v+omega*omega*old)*sn/b);}
  else {const double root=std::sqrt(zeta*zeta-1),r1=-omega/(zeta+root),r2=-omega*(zeta+root),a=(v-r2*y)/(r1-r2),b=y-a,e1=std::exp(r1*dt),e2=std::exp(r2*dt);y=a*e1+b*e2;v=r1*a*e1+r2*b*e2;}
 }
 template<class T> void applySpring(ObjectId id,const T &c,const float source[3],const float initial[3],float output[3],double dt,bool angular,bool scale) {
  auto &state=springs_[{id,c.instanceId()}];
  if(!state.initialized||state.target!=c.target){for(u32 a=0;a<3;++a){state.value[a]=initial[a];state.velocity[a]=0;}state.target=c.target;state.initialized=true;}
  // Snapshot then publish. Cap the magnitude of motion, not each axis separately.
  double next[3],vel[3],movement[3],length=0;
  for(u32 a=0;a<3;++a){
   const double desired=scale?source[a]*c.offset[a]:source[a]+c.offset[a];
   double difference=desired-output[a];if(angular)difference=std::remainder(difference,360.0);
   const double goal=output[a]+difference*c.weight;
   double y=angular?std::remainder(state.value[a]-goal,360.0):state.value[a]-goal,v=state.velocity[a];
   springStep(y,v,6.283185307179586*c.frequency,c.dampingRatio,dt);
   next[a]=goal+y;vel[a]=v;movement[a]=angular?std::remainder(next[a]-state.value[a],360.0):next[a]-state.value[a];length+=movement[a]*movement[a];
  }
  const double ratio=length>0?std::min(1.0,c.maxSpeed*dt/std::sqrt(length)):1.0;
  for(u32 a=0;a<3;++a){state.value[a]+=static_cast<float>(movement[a]*ratio);state.velocity[a]=static_cast<float>(vel[a]*ratio);if(angular)state.value[a]=std::remainder(state.value[a],360.f);if(scale&&state.value[a]<.0001f){state.value[a]=.0001f;state.velocity[a]=std::max(0.f,state.velocity[a]);}output[a]=state.value[a];}
 }
 #include "runtime/spring_rotation.inl"
 struct Baseline {Transform rest{},restLocal{},outputLocal{};bool initialized=false;};
 std::map<ObjectId,Baseline> baseline_;std::vector<Diagnostic> diagnostics_;
 std::vector<ObjectId> candidates_;u64 knownRevision_=0;u32 knownWorld_=0;
 static std::array<const scene::ComponentType*,9> types(){return {&scene::SpringPositionConstraint::descriptor,&scene::SpringRotationConstraint::descriptor,&scene::SpringScaleConstraint::descriptor,&scene::PositionConstraint::descriptor,&scene::RotationConstraint::descriptor,&scene::ScaleConstraint::descriptor,&scene::AimConstraint::descriptor,&scene::ParentConstraint::descriptor,&scene::LookAtConstraint::descriptor};}
 template<class F> static auto dispatch(const scene::ComponentValue &c,F f) -> decltype(f(static_cast<const scene::PositionConstraint&>(c))){
  if(c.type().id==scene::SpringPositionConstraint::descriptor.id)return f(static_cast<const scene::SpringPositionConstraint&>(c));
  if(c.type().id==scene::SpringRotationConstraint::descriptor.id)return f(static_cast<const scene::SpringRotationConstraint&>(c));
  if(c.type().id==scene::SpringScaleConstraint::descriptor.id)return f(static_cast<const scene::SpringScaleConstraint&>(c));
  if(c.type().id==scene::PositionConstraint::descriptor.id)return f(static_cast<const scene::PositionConstraint&>(c));
  if(c.type().id==scene::RotationConstraint::descriptor.id)return f(static_cast<const scene::RotationConstraint&>(c));
  if(c.type().id==scene::ScaleConstraint::descriptor.id)return f(static_cast<const scene::ScaleConstraint&>(c));
  if(c.type().id==scene::ParentConstraint::descriptor.id)return f(static_cast<const scene::ParentConstraint&>(c));
  if(c.type().id==scene::LookAtConstraint::descriptor.id)return f(static_cast<const scene::LookAtConstraint&>(c));
  return f(static_cast<const scene::AimConstraint&>(c));
 }
 static ObjectId target(const scene::ComponentValue &c){return dispatch(c,[](const auto &v){return static_cast<ObjectId>(v.target);});}
 static bool active(const scene::ComponentValue &c){return dispatch(c,[](const auto &v){return v.enabled;});}
 static bool same(const Transform &a,const Transform &b){for(u32 i=0;i<3;++i)if(std::abs(a.position[i]-b.position[i])>1e-5f||std::abs(a.rotationDegrees[i]-b.rotationDegrees[i])>1e-5f||std::abs(a.scale[i]-b.scale[i])>1e-5f)return false;return true;}
 template<class T> static void apply(const T &c,const float desired[3],float output[3],bool angular){for(u32 i=0;i<3;++i)if(c.axis[i]){float goal;if constexpr(std::is_same_v<T,scene::ScaleConstraint>)goal=desired[i]*c.offset[i];else goal=desired[i]+c.offset[i];float difference=goal-output[i];if(angular)difference=std::remainder(difference,360.f);output[i]+=difference*c.weight;}}
 static void cross(const float a[3],const float b[3],float out[3]){out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];}
 static bool normalize(float v[3]){const float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n<1e-6f)return false;for(u32 i=0;i<3;++i)v[i]/=n;return true;}
 static bool aimPose(const Transform &owner,const Transform &source,const scene::AimConstraint &c,Transform &out){
  float forward[3];for(u32 i=0;i<3;++i)forward[i]=source.position[i]-owner.position[i];if(!normalize(forward))return false;
  if(c.aimAxis>=3)for(float &n:forward)n=-n;
  float up[3]{0,0,0};up[c.upAxis==0?1:c.upAxis==1?2:0]=1;
  const float dot=up[0]*forward[0]+up[1]*forward[1]+up[2]*forward[2];for(u32 i=0;i<3;++i)up[i]-=dot*forward[i];
  if(!normalize(up))return false; // vertical singularity holds prior pose, diagnosed
  float columns[3][3]{};const u32 axis=c.aimAxis%3;
  for(u32 i=0;i<3;++i){columns[axis][i]=forward[i];columns[axis==1?2:1][i]=up[i];}
  if(axis==0)cross(columns[0],columns[1],columns[2]);
  else if(axis==1)cross(columns[1],columns[2],columns[0]);
  else cross(columns[1],columns[2],columns[0]);
  float matrix[16]{},identity[16]{};for(u32 i=0;i<4;++i)identity[i*5]=1;matrix[15]=1;
  for(u32 col=0;col<3;++col)for(u32 row=0;row<3;++row)matrix[col*4+row]=columns[col][row];
  return localTransformForWorld(matrix,identity,out);
 }
};
}
