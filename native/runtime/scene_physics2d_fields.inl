// Included inside scene_physics2d.cpp. Operates on its actual Box2D world.
WorldStatus ScenePhysics2D::fieldQuery(const GameWorld &world,ObjectHandle owner,u64 instance,u32 operation,const float *point,u32 layer,PhysicsFieldSample &out) const {
 const auto &p=*impl_;if(!world.running()||!b2World_IsValid(p.solver))return WorldStatus::NotRunning;
 if(world.worldId()!=p.worldId)return WorldStatus::ForeignWorld;
 const auto status=world.validate(owner);if(status!=WorldStatus::Ok)return status;
 if(operation>2||layer>=32||!finite2(point)||!std::isfinite(point[2])||point[2]!=0||out.size!=sizeof(out)||out.reserved)return WorldStatus::InvalidArgument;
 const auto *component=world.find(owner)->components.findInstance(instance);if(!component||scene::physicsField2DKind(*component)<0)return WorldStatus::ComponentMissing;
 PhysicsField2DFrame frame;if(!makePhysicsField2DFrame(world.graph(),owner.id,*component,frame))return WorldStatus::Rejected;
 out=samplePhysicsField2D(frame,point,layer);if(operation!=2)return WorldStatus::Ok;
 for(const auto &[id,body]:p.bodies){
  if(b2Body_GetType(body.id)!=b2_dynamicBody||!b2Body_IsEnabled(body.id)||!world.graph().activeInHierarchy(id))continue;
  const auto center=b2Body_GetWorldCenterOfMass(body.id);const float xy[2]{center.x,center.y};const auto sample=samplePhysicsField2D(frame,xy,world.graph().find(id)->layer);
  if(sample.weight<=0||(!b2Body_IsAwake(body.id)&&!(sample.flags&4)))continue;
  ++out.affectedBodies;out.affectedMass+=b2Body_GetMass(body.id);
 }
 return WorldStatus::Ok;
}
bool ScenePhysics2D::applyPhysicsFields(GameWorld &world,float dt){
 auto &p=*impl_;
 if(p.fieldRevision!=world.structuralRevision()){
  p.fieldCandidates.clear();std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
  for(auto id:ids){const auto *o=world.graph().find(id);for(usize n=0;n<o->components.size();++n)if(scene::physicsField2DKind(*o->components.at(n))>=0)p.fieldCandidates.push_back({id,o->components.at(n)->instanceId()});}
  p.fieldRevision=world.structuralRevision();
 }
 p.fieldFrames.clear();
 for(const auto &[id,instance]:p.fieldCandidates){
  const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
  const auto *value=o->components.findInstance(instance);if(!value||!static_cast<const scene::PhysicsFieldProperties&>(*value).enabled)continue;
  PhysicsField2DFrame frame;if(!makePhysicsField2DFrame(world.graph(),id,*value,frame)){p.error="Campo 2D: inclinação, reflexão ou transformação singular";return false;}p.fieldFrames.push_back(frame);
 }
 if(p.fieldFrames.empty())return true;
 const auto gravity=b2World_GetGravity(p.solver);
 for(const auto &[id,body]:p.bodies){
  if(b2Body_GetType(body.id)!=b2_dynamicBody||!b2Body_IsEnabled(body.id)||!world.graph().activeInHierarchy(id))continue;
  const auto *o=world.graph().find(id);const auto center=b2Body_GetWorldCenterOfMass(body.id);const float xy[2]{center.x,center.y};
  double acceleration[2]{},wind[2]{},coefficient=0,linearDrag=0,angularDrag=0,replace=0;bool affected=false;
  for(const auto &frame:p.fieldFrames){const auto sample=samplePhysicsField2D(frame,xy,o->layer);if(sample.weight<=0||(!b2Body_IsAwake(body.id)&&!(sample.flags&4)))continue;
   affected=true;replace=std::max(replace,double(sample.overrideWeight));for(u32 a=0;a<2;++a){acceleration[a]+=sample.acceleration[a];wind[a]+=double(sample.windVelocity[a])*sample.windDrag;}coefficient+=sample.windDrag;linearDrag+=sample.linearDrag;angularDrag+=sample.angularDrag;
  }
  if(!affected)continue;
  const double mass=b2Body_GetMass(body.id);if(mass<=0||!std::isfinite(mass)){p.error="Campo 2D: massa do solver inválida";return false;}
  const auto velocity=b2Body_GetLinearVelocity(body.id);const double angular=b2Body_GetAngularVelocity(body.id),rate=linearDrag+coefficient/mass,decay=std::exp(-rate*dt),factor=b2Body_GetGravityScale(body.id);
  const auto target=[&](float current,u32 axis){return float(current*decay+(rate>1e-12?wind[axis]/mass*(-std::expm1(-rate*dt))/rate:0));};
  const b2Vec2 next{target(velocity.x,0),target(velocity.y,1)},force{float(mass*(acceleration[0]-gravity.x*factor*replace)),float(mass*(acceleration[1]-gravity.y*factor*replace))};const float nextAngular=float(angular*std::exp(-angularDrag*dt));
  if(!std::isfinite(next.x)||!std::isfinite(next.y)||!std::isfinite(nextAngular)||!std::isfinite(force.x)||!std::isfinite(force.y)){p.error="Campo 2D: efeito excede faixa numérica";return false;}
  if(next.x==velocity.x&&next.y==velocity.y&&nextAngular==angular&&force.x==0&&force.y==0)continue;
  b2Body_SetAwake(body.id,true);b2Body_SetLinearVelocity(body.id,next);b2Body_SetAngularVelocity(body.id,nextAngular);
  if(force.x!=0||force.y!=0)b2Body_ApplyForceToCenter(body.id,force,false);
 }
 return true;
}
