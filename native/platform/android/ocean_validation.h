#pragma once
#include "platform/android/instanced_renderer.h"
#include "physics/water_simulation.h"

namespace ae::platform::android {
// Sample owner only. Engine physics and renderer do not know these bodies.
class OceanValidation final {
public:
  ~OceanValidation() { shutdown(); }
  void shutdown() {
    simulation_.reset();
    if (physics_) AetherPhysics_DestroyWorld(physics_);
    physics_ = nullptr;
    previous_ = -1;
    boatDraws_.clear();
  }
  bool update(InstancedRenderer &renderer, double wallTime, float &renderTime,
              float density, bool paused) {
    // Do not float bodies on an unrelated analytic approximation to FFT.
    if (renderer.waterProviderStatus() == 2) return true;
    if (!physics_) {
      physics_ = AetherPhysics_CreateWorld({0,-9.81f,0}, 16);
      if (!physics_) return false;
      const auto &mesh=renderer.staticCollisionMesh();
      if (!mesh.empty()) {
        std::vector<AetherVec3> vertices;
        vertices.reserve(mesh.vertices.size());
        for(const auto &v:mesh.vertices) vertices.push_back({v.x,v.y,v.z});
        if(AetherPhysics_CreateStaticTriangleMesh(physics_,vertices.data(),static_cast<u32>(vertices.size()),
            mesh.indices.data(),static_cast<u32>(mesh.indices.size()),.5f)==AetherBodyHandle_Invalid) return false;
      }
      physics::WaterSimulationSettings settings;
      settings.water.forces.fluidDensity = 1400;
      if (!simulation_.configure(settings)) return false;
      for (u32 i=0;i<4;++i) {
        const float widths[4]={2,3,1.5f,4.4f}, heights[4]={1.5f,1,2,2.4f}, depths[4]={2,2,1.5f,16};
        extents_[i]={widths[i]*.5f,heights[i]*.5f,depths[i]*.5f};
        AetherBodyDesc desc{};
        desc.shape.kind=AetherShapeKind::Box; desc.shape.boxHalfExtent=extents_[i];
        desc.position={static_cast<float>(static_cast<int>(i)*5-5),2,-10};
        if(i==3) desc.position={0,2,10};
        desc.rotation={0,0,0,1}; desc.motionType=AetherMotionType::Dynamic;
        bodies_[i]=AetherPhysics_CreateBody(physics_,&desc);
        // A wave-driven validation body must keep reacting to changing fluid
        // density. This sample opts out of sleep; global engine defaults stay.
        if(!AetherPhysics_SetAllowSleepingV2(physics_,bodies_[i],0)) return false;
        if (!simulation_.bind({bodies_[i],{physics::BuoyantShapeKind::Box,extents_[i]},widths[i]*depths[i]})) return false;
      }
    }
    auto setup=renderer.waterQuerySetup();
    if (!water_.setVolume(1,setup)) return false;
    physics::WaterRuntimeSettings runtimeSettings;
    runtimeSettings.forces.fluidDensity=density;
    if (!simulation_.setWaterSettings(runtimeSettings)) return false;
    const double delta=previous_<0?0:std::max(0.0,wallTime-previous_);
    previous_=wallTime;
    const auto frame=simulation_.advance(physics_,water_,delta,1,paused);
    if (frame.error!=physics::WaterSimulationError::None) return false;
    renderTime=static_cast<float>(frame.simulationTime);
    for (u32 i=0;i<4;++i) {
      AetherVec3 p{}; AetherQuat q{};
      if (!AetherPhysics_TryGetBodyPoseV2(physics_,bodies_[i],&p,&q)) return false;
      const float x=q.x,y=q.y,z=q.z,w=q.w;
      float m[16]={1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
                   2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
                   2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,p.x,p.y,p.z,1};
      const float scale[3]={i==3?1:extents_[i].x*2,i==3?1:extents_[i].y*2,i==3?1:extents_[i].z*2};
      for(u32 c=0;c<3;++c) for(u32 r=0;r<3;++r) m[c*4+r]*=scale[c];
      const float center[3]={0,0,0};
      const auto &draws=renderer.mapDraws();
      for(u32 d=0;d<draws.size();++d) if(i<3 && draws[d].materialIndex==i+2)
        if(!renderer.queueMapDrawPose(d,m,center,.866026f)) return false;
      if(i==3) {
        if(boatDraws_.empty()) {
          for(u32 d=0;d<draws.size();++d) if(draws[d].materialIndex>=5) {
            BoatDraw binding{};
            binding.index=d;
            for(u32 axis=0;axis<3;++axis) binding.center[axis]=draws[d].boundsCenter[axis];
            binding.center[1]-=2; binding.center[2]-=10;
            binding.radius=draws[d].boundsRadius;
            boatDraws_.push_back(binding);
          }
        }
        for(const auto &binding:boatDraws_)
          if(!renderer.queueMapDrawPose(binding.index,m,binding.center,binding.radius)) return false;
      }
    }
    return true;
  }
private:
  AetherPhysicsWorld *physics_=nullptr;
  physics::WaterSimulation simulation_;
  renderer::WaterWorld water_;
  std::array<AetherBodyHandle,4> bodies_{};
  std::array<AetherVec3,4> extents_{};
  struct BoatDraw { u32 index=0; float center[3]{}; float radius=0; };
  std::vector<BoatDraw> boatDraws_;
  double previous_=-1;
};
}
