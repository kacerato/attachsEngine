#include "editor/editor_scene_physics.h"
#include "editor/editor_physics_body.h"
#include "editor/editor_character.h"
#include "editor/editor_map_scene.h"
#include <cmath>
#include <unordered_map>
#include "scene/joint.h"
#include "editor/editor_component_references.h"
namespace ae::editor {
namespace {
AetherQuat physicsRotation(const EditorTransform &t) {
  const float x=t.rotationDegrees[0]*.00872664626f,y=t.rotationDegrees[1]*.00872664626f,z=t.rotationDegrees[2]*.00872664626f;
  const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
  return {sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz};
}
void multiplyPhysicsMatrix(const float *a,const float *b,float *out) {
  for(u32 c=0;c<4;++c) for(u32 r=0;r<4;++r) {out[c*4+r]=0;for(u32 k=0;k<4;++k) out[c*4+r]+=a[k*4+r]*b[c*4+k];}
}
bool activePhysicsObject(const EditorDocument &document,EditorEntityId id) {
  for(auto p=id;p;) {const auto *e=document.find(p);if(!e||!e->active) return false;p=e->parent;}return true;
}
AetherVec3 transformPhysicsPoint(const float *m,const float *v,bool direction=false) {
  return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+(direction?0:m[12]),
    m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+(direction?0:m[13]),
    m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+(direction?0:m[14])};
}
}
void EditorScenePhysics::stop() {
  characters_.clear();
  if(world_) AetherPhysics_DestroyWorld(world_);
  world_=nullptr;bindings_.clear();objects_.clear();events_.clear();accumulated_=0;jointCount_=0;
}
bool EditorScenePhysics::start(const EditorDocument &document) {
  stop();error_="Falha ao iniciar a física da cena";
  std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
  AetherPhysicsWorldDescV2 config{};
  config.structSize=sizeof(config);config.apiVersion=AetherPhysicsWorldApiVersionV2;
  config.gravity={0,-9.81f,0};config.maxBodies=1024;config.maxBodyPairs=4096;
  config.maxContactConstraints=4096;config.maxBroadPhasePairs=4096;
  config.overflowPolicy=AetherPhysicsOverflowPolicy::Warning;
  world_=AetherPhysics_CreateWorldV2(&config);if(!world_) return false;
  struct ColliderSource {EditorEntityId object;const scene::Collider *value;};
  std::unordered_map<EditorEntityId,std::vector<ColliderSource>> colliders;
  const auto fail=[&](const EditorEntity &entity,const std::string &reason) {error_=std::string(entity.name)+": "+reason;stop();return false;};
  // Resolve ownership before creating anything; no nearest-parent inference.
  for(auto id:ids) {
    const auto &entity=*document.find(id);if(!activePhysicsObject(document,id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Collider::descriptor) continue;
      const auto &c=static_cast<const scene::Collider &>(*v);if(!c.enabled) continue;
      if(!c.valid()||!editorReferenceAccepts(document,id,scene::colliderReferences[0],c.owner,true))
        return fail(entity,"colisor #"+std::to_string(c.instanceId())+" requer um corpo neste objeto ou em um ancestral explícito");
      const auto owner=c.owner?static_cast<EditorEntityId>(c.owner):id;
      if(!activePhysicsObject(document,owner)) return fail(entity,"corpo proprietário inativo");
      // A collider may not cross a separately simulated body in the hierarchy.
      for(auto p=id;p!=owner;p=document.find(p)->parent)
        if(physicsBody(*document.find(p))) return fail(entity,"colisor não pode atravessar outro corpo físico até seu proprietário");
      auto &parts=colliders[owner];if(parts.size()>=256) return fail(entity,"limite de 256 colisores por corpo");
      parts.push_back({id,&c});
    }
  }
  for(auto id:ids) {
    const auto &entity=*document.find(id);const auto *body=physicsBody(entity);
    if(!body||!activePhysicsObject(document,id)) continue;
    if(!body->valid()||characterComponent(entity)) return fail(entity,"corpo inválido ou incompatível com Personagem");
    for(auto p=entity.parent;p;p=document.find(p)->parent) {
      const auto &ancestor=*document.find(p);const auto *parentBody=physicsBody(ancestor);
      if(characterComponent(ancestor)||(parentBody&&parentBody->motion!=scene::BodyMotion::Static))
        return fail(entity,"corpos independentes devem ficar fora da hierarquia de um corpo móvel; conecte-os por Junta");
    }
    auto found=colliders.find(id);
    if(found==colliders.end()||found->second.empty()) return fail(entity,"corpo sem colisores ativos vinculados");
    float world[16],identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;EditorTransform transform;
    if(!editorWorldMatrix(document,id,world)||!editorLocalTransformForWorld(world,identity,transform)) return fail(entity,"transformação física inválida ou com shear");
    auto rigid=transform;rigid.scale[0]=rigid.scale[1]=rigid.scale[2]=1;float bodyFrame[16];editorTransformMatrix(rigid,bodyFrame);
    std::vector<AetherCompoundPart> parts;parts.reserve(found->second.size());
    for(const auto &source:found->second) {
      const auto &c=*source.value;EditorTransform local;local.position[0]=c.centerX;local.position[1]=c.centerY;local.position[2]=c.centerZ;
      local.rotationDegrees[0]=c.rotationX;local.rotationDegrees[1]=c.rotationY;local.rotationDegrees[2]=c.rotationZ;
      float localMatrix[16],objectMatrix[16],shapeWorld[16];editorTransformMatrix(local,localMatrix);
      if(!editorWorldMatrix(document,source.object,objectMatrix)) return fail(entity,"objeto do colisor ausente");
      multiplyPhysicsMatrix(objectMatrix,localMatrix,shapeWorld);EditorTransform partTransform;
      if(!editorLocalTransformForWorld(shapeWorld,bodyFrame,partTransform)) return fail(*document.find(source.object),"colisor rotacionado sob escala não uniforme produz shear");
      const float x=std::abs(partTransform.scale[0]),y=std::abs(partTransform.scale[1]),z=std::abs(partTransform.scale[2]);
      if(c.shape!=scene::ColliderShape::Box && (std::abs(x-y)>1e-4f*x||std::abs(x-z)>1e-4f*x)) return fail(entity,"esfera e cápsula requerem escala global uniforme");
      AetherCompoundPart part{};part.position={partTransform.position[0],partTransform.position[1],partTransform.position[2]};part.rotation=physicsRotation(partTransform);
      switch(c.shape) {
        case scene::ColliderShape::Box:part.shape.kind=AetherShapeKind::Box;part.shape.boxHalfExtent={c.halfX*x,c.halfY*y,c.halfZ*z};break;
        case scene::ColliderShape::Sphere:part.shape.kind=AetherShapeKind::Sphere;part.shape.sphereRadius=c.radius*x;break;
        case scene::ColliderShape::Capsule:part.shape.kind=AetherShapeKind::Capsule;part.shape.sphereRadius=c.radius*x;part.shape.capsuleHalfHeight=c.halfHeight*y;break;
      }
      parts.push_back(part);
    }
    AetherBodyDescV2 desc{};desc.structSize=sizeof(desc);desc.apiVersion=AetherBodyApiVersionV2;desc.eventLayerMask=3;
    desc.position={world[12],world[13],world[14]};desc.rotation=physicsRotation(transform);
    desc.motionType=static_cast<AetherMotionType>(body->motion);desc.friction=body->friction;desc.restitution=body->restitution;desc.isSensor=body->sensor;
    AetherBodyDynamicsV1 dynamics{sizeof(AetherBodyDynamicsV1),1,body->linearDamping,body->angularDamping,body->gravityFactor,
      {body->angularX,body->angularY,body->angularZ},body->allowSleep?1u:0u};
    const auto handle=AetherPhysics_CreateCompoundBodyV1(world_,&desc,parts.data(),static_cast<u32>(parts.size()),&dynamics);
    if(handle==AetherBodyHandle_Invalid||(body->motion==scene::BodyMotion::Dynamic&&!AetherPhysics_SetMassV2(world_,handle,body->mass))) return fail(entity,"Jolt recusou a composição ou a massa");
    if(body->motion!=scene::BodyMotion::Static) AetherPhysics_SetLinearVelocity(world_,handle,{body->velocityX,body->velocityY,body->velocityZ});
    objects_.emplace(handle,id);
    bindings_.push_back({id,handle,{transform.scale[0],transform.scale[1],transform.scale[2]},body->motion!=scene::BodyMotion::Static});
  }
  // All bodies exist now, including static anchors and forward references.
  for(auto id:ids) {
    const auto &entity=*document.find(id);if(!activePhysicsObject(document,id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Joint::descriptor) continue;
      const auto &joint=static_cast<const scene::Joint &>(*v);if(!joint.enabled) continue;
      if(!joint.valid()||!physicsBody(entity)||!editorReferenceAccepts(document,id,scene::jointReferences[0],joint.connectedBody,true)) return fail(entity,"junta requer dois corpos distintos e campos válidos");
      const Binding *a=nullptr,*b=nullptr;for(const auto &binding:bindings_) {if(binding.id==id) a=&binding;if(binding.id==joint.connectedBody) b=&binding;}
      if(!a||!b) return fail(entity,"corpo conectado à junta está inativo");
      if(physicsBody(entity)->motion!=scene::BodyMotion::Dynamic&&physicsBody(*document.find(b->id))->motion!=scene::BodyMotion::Dynamic)
        return fail(entity,"junta requer pelo menos um corpo dinâmico");
      float ma[16],mb[16];if(!editorWorldMatrix(document,id,ma)||!editorWorldMatrix(document,b->id,mb)) return fail(entity,"referencial da junta inválido");
      AetherJointDescV2 desc{};desc.structSize=sizeof(desc);desc.apiVersion=AetherJointApiVersionV2;desc.kind=static_cast<AetherJointKind>(joint.kind);desc.space=AetherJointSpace::World;
      desc.point1=transformPhysicsPoint(ma,joint.anchorA);desc.point2=transformPhysicsPoint(mb,joint.anchorB);
      desc.axis1=transformPhysicsPoint(ma,joint.axisA,true);desc.axis2=transformPhysicsPoint(mb,joint.axisB,true);
      const float units=joint.kind==scene::JointKind::Hinge?.01745329252f:1.0f;
      desc.limitsMin=joint.limitMin*units;desc.limitsMax=joint.limitMax*units;
      desc.motor={static_cast<AetherMotorState>(joint.motor),joint.motorVelocity*units,joint.motorPosition*units,joint.motorForce,joint.frequency,joint.damping};
      if(AetherPhysics_CreateJointV2(world_,a->body,b->body,&desc)==AetherJointHandle_Invalid) return fail(entity,"Jolt recusou a junta #"+std::to_string(joint.instanceId()));
      ++jointCount_;
    }
  }
  for(auto id:ids) {
    const auto &entity=*document.find(id);const auto *character=characterComponent(entity);
    if(!character) continue;
    bool enabled=entity.active;
    for(auto p=entity.parent;p;p=document.find(p)->parent) enabled=enabled&&document.find(p)->active;
    if(!enabled) continue;
    if(physicsBody(entity)||colliderComponent(entity)||!character->valid()||characters_.size()>=32) return fail(entity,"Personagem requer cápsula própria, configuração válida e até 32 instâncias");
    for(auto p=entity.parent;p;p=document.find(p)->parent) {
      const auto &ancestor=*document.find(p);const auto *body=physicsBody(ancestor);
      if(characterComponent(ancestor)||(body&&body->motion!=scene::BodyMotion::Static)) return fail(entity,"Personagem não pode herdar pose de corpo móvel");
    }
    CharacterBinding binding{};binding.id=id;binding.eyeHeight=character->eyeHeight;
    binding.jumpSpeed=character->jumpSpeed;
    float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;EditorTransform transform;
    if(!editorWorldMatrix(document,id,binding.world)||!editorLocalTransformForWorld(binding.world,identity,transform)) {stop();return false;}
    for(float scale:transform.scale) if(std::abs(scale-1)>.0001f) {stop();return false;}
    physics::CharacterMotorSettings settings;
    settings.radius=character->radius;settings.standingHalfHeight=character->halfHeight;
    settings.eyeHeight=character->eyeHeight;settings.movementUnitsPerSecond=character->speed;
    settings.maximumSlopeRadians=character->slopeDegrees*.01745329252f;
    settings.gravityUnitsPerSecondSquared=9.81f;
    binding.motor=std::make_unique<physics::CharacterMotor>();
    if(!binding.motor->initializeInWorld(world_,{binding.world[12],binding.world[13]+binding.eyeHeight,binding.world[14]},settings)) {stop();return false;}
    characters_.push_back(std::move(binding));
  }
  error_.clear();return true;
}
bool EditorScenePhysics::setCharacterMove(EditorEntityId id,float right,float forward,float yaw) {
  if(!std::isfinite(right)||!std::isfinite(forward)||!std::isfinite(yaw)) return false;
  for(auto &c:characters_) if(c.id==id) {c.right=right;c.forward=forward;c.yaw=yaw;return true;}
  return false;
}
bool EditorScenePhysics::jumpCharacter(EditorEntityId id) {
  for(auto &c:characters_) if(c.id==id) return c.motor->jump(c.jumpSpeed);
  return false;
}
bool EditorScenePhysics::applyBodyForce(EditorEntityId id,const float *v,u32 kind) {
  if(!v||kind>3) return false;
  for(const auto &b:bindings_) if(b.id==id) return AetherPhysics_ApplyBodyForceV1(world_,b.body,{v[0],v[1],v[2]},static_cast<AetherBodyForceKind>(kind))!=0;
  return false;
}
bool EditorScenePhysics::getBodyVelocity(EditorEntityId id,float *out) const {
  if(!out) return false;
  for(const auto &b:bindings_) if(b.id==id) {AetherVec3 v;if(!AetherPhysics_TryGetBodyVelocityV1(world_,b.body,&v)) return false;out[0]=v.x;out[1]=v.y;out[2]=v.z;return true;}
  return false;
}
bool EditorScenePhysics::setBodyVelocity(EditorEntityId id,const float *v) {
  if(!v||!std::isfinite(v[0])||!std::isfinite(v[1])||!std::isfinite(v[2])) return false;
  for(const auto &b:bindings_) if(b.id==id && b.moving) {AetherPhysics_SetLinearVelocity(world_,b.body,{v[0],v[1],v[2]});return true;}
  return false;
}
bool EditorScenePhysics::moveKinematic(EditorEntityId id,const float *v) {
  if(!v) return false;
  for(u32 i=0;i<7;++i) if(!std::isfinite(v[i])) return false;
  const float n=std::sqrt(v[3]*v[3]+v[4]*v[4]+v[5]*v[5]+v[6]*v[6]);if(n<1e-8f) return false;
  for(const auto &b:bindings_) if(b.id==id)
    return AetherPhysics_MoveKinematicV2(world_,b.body,{v[0],v[1],v[2]},{v[3]/n,v[4]/n,v[5]/n,v[6]/n},1.0f/60.0f)!=0;
  return false;
}
bool EditorScenePhysics::advance(double elapsed,EditorDocument &document,bool (*beforeStep)(void *,float),void *context,bool (*trigger)(void *,EditorEntityId,EditorEntityId,u32)) {
  if(!world_ || !std::isfinite(elapsed) || elapsed<0) return false;
  constexpr double fixed=1.0/60.0;
  // Bound catch-up after surface/lifecycle stalls; never feed a large dt to Jolt.
  accumulated_+=std::min(elapsed,.25);
  while(accumulated_+1e-9>=fixed) {
    if(beforeStep&&!beforeStep(context,static_cast<float>(fixed))) return false;
    for(auto &c:characters_) if(!c.motor->update(c.right,c.forward,c.yaw,static_cast<float>(fixed))) return false;
    if(AetherPhysics_StepV2(world_,static_cast<float>(fixed),1)!=0) return false;
    accumulated_-=fixed;
    if(!synchronizePoses(document)) return false;
    if(trigger) {
      const auto count=AetherPhysics_GetTriggerEvents(world_,nullptr,0);
      if(count<0||count>1024*1024) {error_="Quantidade de eventos físicos inválida";return false;}
      events_.resize(static_cast<usize>(count));
      if(count&&AetherPhysics_GetTriggerEvents(world_,events_.data(),count)!=count) return false;
      // Native events are directed sensor -> other and aggregate compound parts.
      for(const auto &event:events_) {
        const auto sensor=objects_.find(event.sensor),other=objects_.find(event.other);
        if(sensor!=objects_.end()&&other!=objects_.end()&&!trigger(context,sensor->second,other->second,static_cast<u32>(event.type))) return false;
      }
    }
  }
  return true;
}
bool EditorScenePhysics::synchronizePoses(EditorDocument &document) {
  for(const auto &binding:bindings_) {
    if(!binding.moving) continue;
    AetherVec3 p;AetherQuat q;
    if(!AetherPhysics_TryGetBodyPoseV2(world_,binding.body,&p,&q)) return false;
    float world[16]{
      (1-2*(q.y*q.y+q.z*q.z))*binding.scale[0],2*(q.x*q.y+q.w*q.z)*binding.scale[0],2*(q.x*q.z-q.w*q.y)*binding.scale[0],0,
      2*(q.x*q.y-q.w*q.z)*binding.scale[1],(1-2*(q.x*q.x+q.z*q.z))*binding.scale[1],2*(q.y*q.z+q.w*q.x)*binding.scale[1],0,
      2*(q.x*q.z+q.w*q.y)*binding.scale[2],2*(q.y*q.z-q.w*q.x)*binding.scale[2],(1-2*(q.x*q.x+q.y*q.y))*binding.scale[2],0,p.x,p.y,p.z,1};
    const auto *entity=document.find(binding.id);if(!entity) return false;
    float parent[16]{};parent[0]=parent[5]=parent[10]=parent[15]=1;
    if(entity->parent && !editorWorldMatrix(document,entity->parent,parent)) return false;
    EditorTransform local;
    if(!editorLocalTransformForWorld(world,parent,local)||!document.setTransform(binding.id,local)) return false;
  }
  for(auto &c:characters_) {
    const auto eye=c.motor->eyePosition();c.world[12]=eye.x;c.world[13]=eye.y-c.eyeHeight;c.world[14]=eye.z;
    const auto *entity=document.find(c.id);if(!entity) return false;
    float parent[16]{};parent[0]=parent[5]=parent[10]=parent[15]=1;
    if(entity->parent&&!editorWorldMatrix(document,entity->parent,parent)) return false;
    EditorTransform local;
    if(!editorLocalTransformForWorld(c.world,parent,local)||!document.setTransform(c.id,local)) return false;
  }
  return true;
}
}
