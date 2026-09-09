#pragma once
#include "editor/editor_water_world.h"
#include "physics/water_simulation.h"

namespace ae::editor {
// Runtime projection of an authored scene. The edit document is never mutated.
// One owner thread. Stop before destroying renderer mirrors or the application.
class EditorWaterPlay {
public:
  ~EditorWaterPlay(){stop();}
  void stop() {
    simulation_.reset();water_.clear();bindings_.clear();ripples_={};mirrors_={};
    if(physics_) AetherPhysics_DestroyWorld(physics_);
    physics_=nullptr;previous_=-1;simulationTime_=0;
  }
  bool active() const {return physics_!=nullptr;}
  float simulationTime() const {return static_cast<float>(simulationTime_);}
  u32 bodyCount() const {return static_cast<u32>(bindings_.size());}
  u32 volumeCount() const {return water_.volumeCount();}
  physics::WaterRuntimeStats stats() const {return simulation_.stats();}
  const renderer::WaterRippleField &ripples() const {return ripples_;}
  bool start(const EditorDocument &document,std::span<const renderer::MapDrawState> draws,
             renderer::WaterFieldSetup setup,std::span<const renderer::WaterCascadeSettings> cascades,
             const renderer::WaterSpectralControls &controls,float density) {
    stop();
    waveTimeScale_=controls.timeScale;
    std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
    const bool needsSampling=std::any_of(ids.begin(),ids.end(),[&](auto id) {
      const auto &e=*document.find(id);
      return e.active && e.rigidBodyEnabled && e.assetId && std::any_of(draws.begin(),draws.end(),[&](const auto &draw){return draw.objectId==id && draw.visible;});
    });
    if(needsSampling && !cascades.empty()) {
      renderer::WaterMirrorSetSettings settings;settings.maximumResolution=128;
      if(!mirrors_.initialize(cascades,controls,settings) || !mirrors_.update(0)) return false;
      setup.mirrorSet=&mirrors_;setup.requested=renderer::WaterFieldProvider::SpectralCpu;
    }
    renderer::WaterRippleSettings rippleSettings;rippleSettings.resolution=128;rippleSettings.areaSize=128;
    if(!ripples_.initialize(rippleSettings)) return false;
    setup.ripples=&ripples_;
    if(!extractEditorWaterWorld(document,setup,water_)) return false;
    physics_=AetherPhysics_CreateWorld({0,-9.81f,0},256);if(!physics_) return false;
    physics::WaterSimulationSettings settings;settings.water.forces.fluidDensity=density;
    if(!simulation_.configure(settings)) {stop();return false;}
    for(auto id:ids) {
      const auto &e=*document.find(id);if(!e.active || !e.rigidBodyEnabled || !e.assetId) continue;
      auto found=std::find_if(draws.begin(),draws.end(),[&](const auto &draw){return draw.objectId==id && draw.visible;});
      if(found==draws.end()) continue;
      if(bindings_.size()>=physics::WaterRuntime::Capacity) {stop();return false;}
      float world[16],identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;EditorTransform transform;
      if(!editorWorldMatrix(document,id,world) || !editorLocalTransformForWorld(world,identity,transform)) {stop();return false;}
      const float x=transform.rotationDegrees[0]*.00872664626f,y=transform.rotationDegrees[1]*.00872664626f,z=transform.rotationDegrees[2]*.00872664626f;
      const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
      AetherBodyDesc desc{};desc.position={world[12],world[13],world[14]};
      desc.rotation={sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz};
      desc.shape.kind=AetherShapeKind::Box;
      desc.shape.boxHalfExtent={e.rigidBody[2]*transform.scale[0],e.rigidBody[3]*transform.scale[1],e.rigidBody[4]*transform.scale[2]};
      desc.motionType=AetherMotionType::Dynamic;desc.friction=.5f;
      const auto body=AetherPhysics_CreateBody(physics_,&desc);
      if(body==AetherBodyHandle_Invalid || !AetherPhysics_SetMassV2(physics_,body,e.rigidBody[0]) ||
         !simulation_.bind({body,{physics::BuoyantShapeKind::Box,desc.shape.boxHalfExtent},4*desc.shape.boxHalfExtent.x*desc.shape.boxHalfExtent.z*e.rigidBody[1]})) {stop();return false;}
      Binding binding;binding.draw=static_cast<u32>(found-draws.begin());binding.body=body;
      binding.extent=desc.shape.boxHalfExtent;
      if(!binding.wake.configure(renderer::WaterWakeSettings{})) {stop();return false;}
      float rigid[16],inverse[16]{};matrix(desc.position,desc.rotation,rigid);inverse[15]=1;
      for(u32 a=0;a<3;++a) for(u32 b=0;b<3;++b) inverse[a*4+b]=rigid[b*4+a];
      for(u32 a=0;a<3;++a) inverse[12+a]=-inverse[a]*world[12]-inverse[4+a]*world[13]-inverse[8+a]*world[14];
      multiply(inverse,found->pose.draw.model,binding.local);
      bindings_.push_back(binding);
      if(bindings_.size()==1) ripples_.recenter(desc.position.x,desc.position.z);
    }
    return true;
  }
  bool update(double time,std::vector<renderer::MapDrawState> &draws) {
    if(!physics_) return false;
    const double delta=previous_<0?0:std::max(0.0,time-previous_);previous_=time;
    physics::WaterSimulationHooks hooks;hooks.context=this;
    hooks.beforeStep=[](void *context,double t) {auto &self=*static_cast<EditorWaterPlay*>(context);return !self.mirrors_.cascadeCount() || self.mirrors_.update(t*self.waveTimeScale_);};
    const auto frame=simulation_.advance(physics_,water_,delta,1,false,hooks,&ripples_);
    if(frame.error!=physics::WaterSimulationError::None) return false;
    const float elapsed=static_cast<float>(frame.simulationTime-simulationTime_);simulationTime_=frame.simulationTime;
    // beforeStep já publicou o campo do último passo fixo.
    for(auto &binding:bindings_) {
      if(binding.draw>=draws.size()) return false;
      AetherVec3 position;AetherQuat rotation;
      if(!AetherPhysics_TryGetBodyPoseV2(physics_,binding.body,&position,&rotation)) return false;
      const auto velocity=AetherPhysics_GetLinearVelocity(physics_,binding.body);
      const float forwardX=2*(rotation.x*rotation.z+rotation.w*rotation.y),forwardZ=1-2*(rotation.x*rotation.x+rotation.y*rotation.y);
      if(elapsed>0 && !binding.wake.update({{position.x,position.z},{forwardX,forwardZ},{velocity.x,velocity.z},
          binding.extent.z,binding.extent.x,simulation_.submergedFraction(binding.body),elapsed},ripples_)) return false;
      auto &draw=draws[binding.draw].pose;float rigid[16];matrix(position,rotation,rigid);
      multiply(rigid,binding.local,draw.draw.model);
      draw.draw.boundsCenter[0]=position.x;draw.draw.boundsCenter[1]=position.y;draw.draw.boundsCenter[2]=position.z;
      const float tint[]{1,1,1,1};if(!renderer::buildGpuMeshInstance(draw.draw.model,tint,&draw.instance)) return false;
    }
    ripples_.advance(elapsed);
    return true;
  }
private:
  struct Binding {u32 draw=0;AetherBodyHandle body=AetherBodyHandle_Invalid;float local[16]{};AetherVec3 extent{};renderer::WaterWakeEmitter wake;};
  static void multiply(const float *a,const float *b,float *out) {float m[16]{};for(u32 c=0;c<4;++c) for(u32 r=0;r<4;++r) for(u32 k=0;k<4;++k) m[c*4+r]+=a[k*4+r]*b[c*4+k];std::copy(m,m+16,out);}
  static void matrix(AetherVec3 p,AetherQuat q,float *m) {
    const float x=q.x,y=q.y,z=q.z,w=q.w;
    const float value[]{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,p.x,p.y,p.z,1};std::copy(value,value+16,m);
  }
  AetherPhysicsWorld *physics_=nullptr;
  physics::WaterSimulation simulation_;
  renderer::WaterWorld water_;
  renderer::WaterSpectralMirrorSet mirrors_;
  renderer::WaterRippleField ripples_;
  std::vector<Binding> bindings_;
  double previous_=-1;
  double simulationTime_=0;
  float waveTimeScale_=1;
};
}
