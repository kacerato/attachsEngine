// Included inside jolt_bridge.cpp's extern C. One validated body lock per
// command; no backend types escape the versioned ABI.

ae::i32 AetherPhysics_GetBodyFieldStateV1(AetherPhysicsWorld *world,AetherBodyHandle handle,float *mass,float *gravityFactor,AetherVec3 *gravity){
 if(!world||!mass||!gravityFactor||!gravity)return 0;
 JPH::BodyLockRead lock(world->physicsSystem.GetBodyLockInterface(),JPH::BodyID(handle));
 if(!lock.Succeeded()||!lock.GetBody().IsDynamic())return 0;
 const auto *m=lock.GetBody().GetMotionProperties();const float inverse=m->GetInverseMass();
 if(!std::isfinite(inverse)||inverse<=0)return 0;
 *mass=1/inverse;*gravityFactor=m->GetGravityFactor();*gravity=FromJolt(world->physicsSystem.GetGravity());return 1;
}
ae::i32 AetherPhysics_ConfigureBodySimulationV1(AetherPhysicsWorld *world,AetherBodyHandle handle,const AetherBodySimulationV1 *s) {
 if(!world||!s||s->size!=sizeof(*s)||s->version!=1||s->continuousCollision>1||s->velocitySteps>255||!std::isfinite(s->maxLinearVelocity)||!std::isfinite(s->maxAngularVelocity)||s->maxLinearVelocity<=0||s->maxAngularVelocity<=0)return 0;
 const JPH::BodyID id(handle);
 {JPH::BodyLockWrite lock(world->physicsSystem.GetBodyLockInterface(),id);if(!lock.Succeeded()||lock.GetBody().IsStatic())return 0;
  auto *m=lock.GetBody().GetMotionProperties();m->SetMaxLinearVelocity(s->maxLinearVelocity);m->SetMaxAngularVelocity(s->maxAngularVelocity);m->SetNumVelocityStepsOverride(s->velocitySteps);}
 world->physicsSystem.GetBodyInterface().SetMotionQuality(id,s->continuousCollision?JPH::EMotionQuality::LinearCast:JPH::EMotionQuality::Discrete);return 1;
}
ae::i32 AetherPhysics_BodyCommandV1(AetherPhysicsWorld *world,AetherBodyHandle handle,ae::u32 op,AetherVec3 value,AetherVec3 point,AetherBodyStateV1 *out) {
 if(!world||handle==AetherBodyHandle_Invalid||op>9||!out||out->size!=sizeof(*out)||out->reserved)return 0;
 for(float n:{value.x,value.y,value.z,point.x,point.y,point.z})if(!std::isfinite(n))return 0;
 const JPH::BodyID id(handle);AetherBodyStateV1 result;bool wake=false,sleep=false;
 {JPH::BodyLockWrite lock(world->physicsSystem.GetBodyLockInterface(),id);if(!lock.Succeeded())return 0;
  auto &body=lock.GetBody();
  if(op!=0&&op!=9&&body.IsStatic())return 0;
  if((op==3||op==4||op==5)&&!body.IsDynamic())return 0;
  if(op==3&&!body.GetAllowSleeping())return 0;
  switch(op){
   case 1:body.SetAngularVelocityClamped(ToJolt(value));wake=true;break;
   case 2:wake=true;break;
   case 3:sleep=true;break;
   case 4:body.AddForce(ToJolt(value),JPH::RVec3(point.x,point.y,point.z));wake=true;break;
   case 5:body.AddImpulse(ToJolt(value),JPH::RVec3(point.x,point.y,point.z));wake=true;break;
   case 6:body.SetLinearVelocityClamped(ToJolt(value));body.SetAngularVelocityClamped(ToJolt(point));wake=true;break;
   case 7:body.SetLinearVelocityClamped(body.GetLinearVelocity()+ToJolt(value));wake=true;break;
   case 8:body.SetAngularVelocityClamped(body.GetAngularVelocity()+ToJolt(value));wake=true;break;
  }
  const auto center=body.GetCenterOfMassPosition();result.centerOfMass={float(center.GetX()),float(center.GetY()),float(center.GetZ())};
  result.linear=FromJolt(op==9?body.GetPointVelocity(JPH::RVec3(value.x,value.y,value.z)):body.GetLinearVelocity());result.angular=FromJolt(body.GetAngularVelocity());
  result.flags=(body.IsActive()?1u:0u)|(body.IsDynamic()?2u:0u)|(body.IsKinematic()?4u:0u)|(!body.IsStatic()&&!body.IsActive()?8u:0u);
 }
 // BodyInterface locks internally. Never call it inside BodyLockWrite.
 auto &bi=world->physicsSystem.GetBodyInterface();if(wake)bi.ActivateBody(id);if(sleep)bi.DeactivateBody(id);
 if(wake){result.flags|=1;result.flags&=~8u;}if(sleep){result.flags&=~1u;result.flags|=8;result.linear=result.angular={0,0,0};}
 *out=result;return 1;
}
