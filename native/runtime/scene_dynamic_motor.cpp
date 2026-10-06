#include "runtime/scene_physics.h"
#include "scene/dynamic_body_motor.h"
#include <algorithm>
#include <cmath>
namespace ae::runtime {
bool ScenePhysics::setDynamicMotorMove(ObjectId id,float right,float forward,float yaw) {
  if(!std::isfinite(right)||!std::isfinite(forward)||!std::isfinite(yaw)||std::abs(right)>1||std::abs(forward)>1)return false;
  for(auto &m:dynamicMotors_)if(m.id==id){m.right=right;m.forward=forward;m.yaw=yaw;return true;}
  return false;
}
bool ScenePhysics::jumpDynamicMotor(ObjectId id) {
  for(auto &m:dynamicMotors_)if(m.id==id){if(m.pendingJump)return false;m.pendingJump=true;return true;}
  return false;
}
bool ScenePhysics::setDynamicMotorScriptMove(ObjectId id,float right,float forward,float yaw) {
  if(!std::isfinite(right)||!std::isfinite(forward)||!std::isfinite(yaw)||std::abs(right)>1||std::abs(forward)>1)return false;
  for(auto &m:dynamicMotors_)if(m.id==id){m.scriptRight=right;m.scriptForward=forward;m.scriptYaw=yaw;m.scriptMoveActive=true;return true;}
  return false;
}
bool ScenePhysics::dynamicMotorState(ObjectId id,DynamicMotorState &out) const {
  for(const auto &m:dynamicMotors_)if(m.id==id){out=m.state;return true;}
  return false;
}
bool ScenePhysics::applyDynamicMotors(GameWorld &world,float dt) {
  for(auto &m:dynamicMotors_) {
    const float inputRight=m.scriptMoveActive?m.scriptRight:m.right,inputForward=m.scriptMoveActive?m.scriptForward:m.forward,yaw=m.scriptMoveActive?m.scriptYaw:m.yaw;
    m.state={};m.state.move[0]=inputRight;m.state.move[1]=inputForward;
    const auto *entity=world.graph().find(m.id);
    const auto *component=entity?entity->components.findInstance(m.instance):nullptr;
    if(!component||!world.activeInHierarchy(world.handle(m.id))){m.pendingJump=false;continue;}
    const auto &c=static_cast<const scene::DynamicBodyMotor&>(*component);
    if(!c.enabled){m.pendingJump=false;continue;}
    const auto *body=physicsBody(*entity);
    if(!body||body->motion!=scene::BodyMotion::Dynamic||body->sensor||!c.valid()||body->freezePosition[0]||body->freezePosition[2]||(c.jumpSpeed>0&&body->freezePosition[1])) {
      error_=std::string(entity->name)+": motor requer corpo dinâmico sólido e eixos livres";return false;
    }
    const auto binding=std::find_if(bindings_.begin(),bindings_.end(),[&](const auto &b){return b.id==m.id;});
    AetherVec3 position,velocity;AetherQuat rotation;
    if(binding==bindings_.end()||!AetherPhysics_TryGetBodyPoseV2(world_,binding->body,&position,&rotation)||!AetherPhysics_TryGetBodyVelocityV1(world_,binding->body,&velocity)) {
      error_="Motor dinâmico perdeu o corpo do solver";return false;
    }
    QueryFilter filter;filter.ignore=m.id;filter.gameplayLayerMask=0;
    m.state.hasMeasuredStep=true;
    for(u32 layer=0;layer<GameplayLayers::kCount;++layer)if(world.graph().layers().interacts(entity->layer,layer))filter.gameplayLayerMask|=1u<<layer;
    AetherVec3 probes[AetherBodyGroundProbeCapacityV1];u32 probeCount=0;
    if(c.automaticSupport) {
      if(!AetherPhysics_TryGetBodyGroundProbesV1(world_,binding->body,probes,&probeCount)){error_="Motor perdeu a geometria de apoio do solver";return false;}
      for(u32 n=0;n<probeCount;++n)probes[n].y+=.02f;
    } else {
      const float offsets[5][2]{{0,0},{c.supportRadius,0},{-c.supportRadius,0},{0,c.supportRadius},{0,-c.supportRadius}};
      probeCount=c.supportRadius>0?5u:1u;
      for(u32 n=0;n<probeCount;++n)probes[n]={position.x+offsets[n][0],position.y,position.z+offsets[n][1]};
    }
    QueryHit support;float distance=c.probeHeight+c.probeDistance+1;
    const float direction[3]{0,-((c.automaticSupport?.02f:c.probeHeight)+c.probeDistance),0};
    const float minNormal=std::cos(c.maxSlopeDegrees*.01745329252f);
    for(u32 n=0;n<probeCount;++n) {
      const float origin[3]{probes[n].x,probes[n].y,probes[n].z};QueryHit hit;
      if(!rayCast(origin,direction,filter,hit))continue;
      // A settled solver shape may penetrate the floor by the contact tolerance.
      // An inside ray has no surface normal: retry above the configured probe
      // range and keep the original lower endpoint instead of inventing one.
      if(!hit.hasNormal&&hit.fraction==0) {
        const float lift=std::max(.02f,c.probeDistance);
        const float raised[]{origin[0],origin[1]+lift,origin[2]},extended[]{0,direction[1]-lift,0};
        if(!rayCast(raised,extended,filter,hit))continue;
        hit.distance=std::max(0.f,hit.distance-lift);
      }
      if(hit.hasNormal&&hit.normal[1]>=minNormal&&hit.distance<distance){support=hit;distance=hit.distance;}
    }
    if(support.object) {
      float ground[3]{};
      for(const auto &b:bindings_)if(b.id==support.object) {
        // Ask the solver for velocity at the contact point: compound shapes
        // can have a centre of mass different from the authored body origin.
        AetherBodyStateV1 state;
        if(!AetherPhysics_BodyCommandV1(world_,b.body,9,{support.point[0],support.point[1],support.point[2]},{},&state)){
          error_="Motor dinâmico perdeu a plataforma de suporte";return false;
        }
        ground[0]=state.linear.x;ground[1]=state.linear.y;ground[2]=state.linear.z;break;
      }
      // Ascending after jump is not support, even while a probe still reaches
      // the floor. A steep wall never grants a new jump either.
      if(velocity.y-ground[1]<=.5f){m.state.grounded=true;m.state.support=support.object;std::copy(support.point,support.point+3,m.state.point);std::copy(support.normal,support.normal+3,m.state.normal);std::copy(ground,ground+3,m.state.supportVelocity);}
    }
    if(m.pendingJump&&m.state.grounded&&c.jumpSpeed>0) {
      const float impulse=body->mass*std::max(0.f,c.jumpSpeed-(velocity.y-m.state.supportVelocity[1]));
      if(impulse>0&&!AetherPhysics_ApplyBodyForceV1(world_,binding->body,{0,impulse,0},AetherBodyForceKind::Impulse))return false;
      m.state.grounded=false;m.state.support=0;
    }
    m.pendingJump=false;
    float right=inputRight,forward=inputForward;const float length=std::hypot(right,forward);
    if(length>1){right/=length;forward/=length;}
    float target[3]{(right*std::cos(yaw)+forward*std::sin(yaw))*c.speed,0,(forward*std::cos(yaw)-right*std::sin(yaw))*c.speed};
    if(m.state.grounded) {
      const auto *normal=m.state.normal;const float dot=target[0]*normal[0]+target[2]*normal[2];
      for(u32 n=0;n<3;++n)target[n]-=dot*normal[n];
      const float tangent=std::sqrt(target[0]*target[0]+target[1]*target[1]+target[2]*target[2]);
      if(tangent>1e-6f){const float scale=c.speed*std::min(length,1.f)/tangent;for(auto &n:target)n*=scale;}
      if(c.inheritPlatformVelocity)for(u32 n=0;n<3;++n)target[n]+=m.state.supportVelocity[n];
    }
    float delta[3]{target[0]-velocity.x,0,target[2]-velocity.z};
    if(m.state.grounded){delta[1]=target[1]-velocity.y;const auto *normal=m.state.normal;const float dot=delta[0]*normal[0]+delta[1]*normal[1]+delta[2]*normal[2];for(u32 n=0;n<3;++n)delta[n]-=dot*normal[n];}
    const float magnitude=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
    const float limit=(length>.0001f?c.acceleration:c.braking)*(m.state.grounded?1.f:c.airControl)*dt;
    const float scale=magnitude>1e-6f?body->mass/dt*std::min(1.f,limit/magnitude):0;
    const AetherVec3 force{delta[0]*scale,delta[1]*scale,delta[2]*scale};
    if((force.x!=0||force.y!=0||force.z!=0)&&!AetherPhysics_ApplyBodyForceV1(world_,binding->body,force,AetherBodyForceKind::Force)){error_="Solver recusou força do motor dinâmico";return false;}
  }
  return true;
}
}
