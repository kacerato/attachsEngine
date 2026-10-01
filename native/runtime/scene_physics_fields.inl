// Included in scene_physics.cpp, inside ae::runtime. No parallel physical world.
WorldStatus ScenePhysics::fieldQuery(const GameWorld &world,ObjectHandle owner,u64 instance,u32 operation,const float *point,u32 layer,PhysicsFieldSample &out) const {
  if(!world.running()||!world_)return WorldStatus::NotRunning;
  if(ownerWorldId_!=world.worldId())return WorldStatus::ForeignWorld;
  const auto status=world.validate(owner);if(status!=WorldStatus::Ok)return status;
  if(operation>2||layer>=32||!point||out.size!=sizeof(out)||out.reserved)return WorldStatus::InvalidArgument;
  for(u32 a=0;a<3;++a)if(!std::isfinite(point[a]))return WorldStatus::InvalidArgument;
  const auto *component=world.find(owner)->components.findInstance(instance);
  if(!component||scene::physicsFieldKind(*component)<0)return WorldStatus::ComponentMissing;
  PhysicsFieldFrame field;if(!makePhysicsFieldFrame(world.graph(),owner.id,*component,field))return WorldStatus::Rejected;
  out=samplePhysicsField(field,point,layer);
  if(operation!=2)return WorldStatus::Ok;
  // Statistics are current solver membership, not last frame's cached counter.
  for(const auto &b:bindings_){
    AetherBodyStateV1 body;if(!AetherPhysics_BodyCommandV1(world_,b.body,0,{},{},&body)||!(body.flags&2))continue;
    const auto *object=world.graph().find(b.id);if(!object||!world.graph().activeInHierarchy(b.id))continue;
    const float p[3]{body.centerOfMass.x,body.centerOfMass.y,body.centerOfMass.z};const auto value=samplePhysicsField(field,p,object->layer);
    if(value.weight<=0||(!(body.flags&1)&&!(value.flags&4)))continue;
    float mass,factor;AetherVec3 gravity;if(!AetherPhysics_GetBodyFieldStateV1(world_,b.body,&mass,&factor,&gravity))return WorldStatus::Rejected;
    ++out.affectedBodies;out.affectedMass+=mass;
  }
  return WorldStatus::Ok;
}

bool ScenePhysics::applyPhysicsFields(GameWorld &world,float dt){
  if(fieldRevision_!=world.structuralRevision()){
    fieldCandidates_.clear();std::vector<ObjectId> ids;world.graph().collectSubtree(world.graph().root(),ids);
    for(auto id:ids){const auto *o=world.graph().find(id);for(usize n=0;n<o->components.size();++n){const auto *c=o->components.at(n);if(scene::physicsFieldKind(*c)>=0)fieldCandidates_.push_back({id,c->instanceId()});}}
    fieldRevision_=world.structuralRevision();
  }
  fieldFrames_.clear();
  for(const auto &[id,instance]:fieldCandidates_){
    const auto *o=world.graph().find(id);if(!o||!world.graph().activeInHierarchy(id))continue;
    const auto *c=o->components.findInstance(instance);if(!c||!static_cast<const scene::PhysicsFieldProperties&>(*c).enabled)continue;
    PhysicsFieldFrame frame;if(!makePhysicsFieldFrame(world.graph(),id,*c,frame)){error_="Campo físico: transformação inválida";return false;}fieldFrames_.push_back(frame);
  }
  if(fieldFrames_.empty())return true;
  for(const auto &b:bindings_){
    const auto *o=world.graph().find(b.id);if(!o||!world.graph().activeInHierarchy(b.id))continue;
    const auto *settings=physicsBody(*o);if(!settings||settings->motion!=scene::BodyMotion::Dynamic)continue;
    AetherBodyStateV1 state;if(!AetherPhysics_BodyCommandV1(world_,b.body,0,{},{},&state))return false;
    const float point[3]{state.centerOfMass.x,state.centerOfMass.y,state.centerOfMass.z};
    double acceleration[3]{},wind[3]{},windCoefficient=0,linearDrag=0,angularDrag=0,replaceWeight=0;bool affected=false;
    for(const auto &field:fieldFrames_){const auto sample=samplePhysicsField(field,point,o->layer);
      if(sample.weight<=0||(!(state.flags&1)&&!(sample.flags&4)))continue;
      affected=true;replaceWeight=std::max(replaceWeight,double(sample.overrideWeight));
      for(u32 a=0;a<3;++a){acceleration[a]+=sample.acceleration[a];wind[a]+=double(sample.windVelocity[a])*sample.windDrag;}
      windCoefficient+=sample.windDrag;linearDrag+=sample.linearDrag;angularDrag+=sample.angularDrag;
    }
    if(!affected)continue;
    float mass,factor;AetherVec3 gravity;if(!AetherPhysics_GetBodyFieldStateV1(world_,b.body,&mass,&factor,&gravity))return false;
    // Solve combined wind + linear drag analytically; strong drag cannot reverse
    // velocity or explode at a fixed step. Superposed fields commute.
    const double rate=linearDrag+windCoefficient/mass;
    const double decay=std::exp(-rate*dt),angularDecay=std::exp(-angularDrag*dt);
    const float linear[3]{state.linear.x,state.linear.y,state.linear.z},angular[3]{state.angular.x,state.angular.y,state.angular.z},g[3]{gravity.x,gravity.y,gravity.z};
    float newLinear[3],newAngular[3],force[3];bool change=false;
    for(u32 a=0;a<3;++a){newLinear[a]=float(linear[a]*decay+(rate>1e-12?wind[a]/mass*(-std::expm1(-rate*dt))/rate:0));newAngular[a]=float(angular[a]*angularDecay);force[a]=float(mass*(acceleration[a]-g[a]*factor*replaceWeight));
      if(!std::isfinite(newLinear[a])||!std::isfinite(newAngular[a])||!std::isfinite(force[a])){error_="Campo físico: efeito excede faixa numérica";return false;}
      change=change||newLinear[a]!=linear[a]||newAngular[a]!=angular[a]||force[a]!=0;
    }
    if(!change)continue; // An enabled zero-effect field must not keep a body awake.
    if(!AetherPhysics_BodyCommandV1(world_,b.body,6,{newLinear[0],newLinear[1],newLinear[2]},{newAngular[0],newAngular[1],newAngular[2]},&state)||
       !AetherPhysics_ApplyBodyForceV1(world_,b.body,{force[0],force[1],force[2]},AetherBodyForceKind::Force))return false;
  }
  return true;
}
