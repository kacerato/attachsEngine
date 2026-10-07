#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include "scene/joint.h"
#include "scene/constant_force.h"
#include "scene/physics_event_connection.h"
#include "runtime/joint_requirements.h"
#include "runtime/physics_requirements.h"
namespace ae::runtime {
namespace {
AetherQuat physicsRotation(const Transform &t) {
  const float x=t.rotationDegrees[0]*.00872664626f,y=t.rotationDegrees[1]*.00872664626f,z=t.rotationDegrees[2]*.00872664626f;
  const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
  return {sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz};
}
AetherVec3 transformPhysicsPoint(const float *m,const float *v,bool direction=false) {
  return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+(direction?0:m[12]),
    m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+(direction?0:m[13]),
    m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+(direction?0:m[14])};
}
}
void ScenePhysics::stop() {
  characters_.clear();
  dynamicMotors_.clear();
  if(world_) AetherPhysics_DestroyWorld(world_);
  world_=nullptr;ownerWorldId_=0;bindings_.clear();objects_.clear();events_.clear();accumulated_=0;jointCount_=0;joints_.clear();
  fieldCandidates_.clear();fieldFrames_.clear();fieldRevision_=std::numeric_limits<u64>::max();
}
ObjectId ScenePhysics::objectForBody(AetherBodyHandle body) const {
  const auto found=objects_.find(body);
  return found==objects_.end()?kInvalidObject:found->second;
}
namespace {
// A malha do objeto inteiro (todos os slots, como o Mesh Collider que recebe a
// malha do MeshFilter) no referencial da parte, com a escala aplicada. Vértices
// iguais são soldados: a malha de desenho repete o vértice por triângulo, e a
// de colisão precisa das arestas compartilhadas para não prender em costuras.
bool collisionMesh(const CollisionGeometrySource &geometry,const scene::Collider &collider,const scene::MeshRenderer *render,const float matrix[16],
                   std::vector<AetherVec3> &vertices,std::vector<u32> &indices) {
  std::vector<float> triangles;
  if(collider.collisionMesh.valid()) {
    if(!geometry.meshTriangles(collider.collisionMesh,triangles)) return false;
  } else if(render) {
    for(u32 slot=0;slot<=render->submeshes.size();++slot) {
      const u32 mesh=render->slotMesh(slot);if(!mesh) continue;
      // A mesma malha em dois slots é a mesma forma: contar duas vezes só
      // duplicaria triângulos sobrepostos.
      bool repeated=false;for(u32 earlier=0;earlier<slot;++earlier) repeated=repeated||render->slotMesh(earlier)==mesh;
      if(!repeated&&!geometry.meshTriangles(mesh,triangles)) return false;
    }
  }
  if(triangles.empty()||triangles.size()%9) return false;
  // Escala espelhada inverte a ordem dos vértices; a face continua para fora.
  const bool mirrored=matrix[0]*(matrix[5]*matrix[10]-matrix[9]*matrix[6])-matrix[4]*(matrix[1]*matrix[10]-matrix[9]*matrix[2])+matrix[8]*(matrix[1]*matrix[6]-matrix[5]*matrix[2])<0;
  struct Key {u32 bits[3];bool operator==(const Key &o) const {return bits[0]==o.bits[0]&&bits[1]==o.bits[1]&&bits[2]==o.bits[2];}};
  struct Hash {usize operator()(const Key &k) const {return (static_cast<usize>(k.bits[0])*73856093u)^(static_cast<usize>(k.bits[1])*19349663u)^(static_cast<usize>(k.bits[2])*83492791u);}};
  std::unordered_map<Key,u32,Hash> welded;if(collider.weldVertices) welded.reserve(triangles.size()/3);
  vertices.clear();indices.clear();indices.reserve(triangles.size()/3);
  for(usize t=0;t<triangles.size();t+=9) {
    u32 corner[3];
    for(u32 v=0;v<3;++v) {
      const auto transformed=transformPhysicsPoint(matrix,triangles.data()+t+v*3);
      const float p[3]{transformed.x,transformed.y,transformed.z};
      if(!std::isfinite(p[0])||!std::isfinite(p[1])||!std::isfinite(p[2])) return false;
      if(collider.weldVertices) {
        Key key{};std::memcpy(key.bits,p,sizeof(key.bits));
        const auto [found,inserted]=welded.emplace(key,static_cast<u32>(vertices.size()));
        if(inserted) vertices.push_back({p[0],p[1],p[2]});
        corner[v]=found->second;
      } else {
        corner[v]=static_cast<u32>(vertices.size());vertices.push_back({p[0],p[1],p[2]});
      }
    }
    if(corner[0]==corner[1]||corner[1]==corner[2]||corner[0]==corner[2]) continue; // degenerado após soldar
    indices.push_back(corner[0]);indices.push_back(corner[mirrored?2:1]);indices.push_back(corner[mirrored?1:2]);
  }
  return !indices.empty();
}
}
bool ScenePhysics::start(GameWorld &gameWorld,const CollisionGeometrySource *geometry) {
  stop();gameWorld.clearAuthorities();error_="Falha ao iniciar a física da cena";
  // Juntas quebradas valem para a execução de Play inteira, não para uma reconstrução.
  if(brokenWorldId_!=gameWorld.worldId()){brokenJoints_.clear();brokenWorldId_=gameWorld.worldId();}
  const auto &document=gameWorld.graph();
  std::vector<ObjectId> ids;document.collectSubtree(document.root(),ids);
  AetherPhysicsWorldDescV2 config{};
  config.structSize=sizeof(config);config.apiVersion=AetherPhysicsWorldApiVersionV2;
  config.gravity={0,-9.81f,0};config.maxBodies=1024;config.maxBodyPairs=4096;
  config.maxContactConstraints=4096;config.maxBroadPhasePairs=4096;
  config.overflowPolicy=AetherPhysicsOverflowPolicy::Warning;
  world_=AetherPhysics_CreateWorldV2(&config);if(!world_) return false;
  if(!AetherPhysics_SetLayerInteractionV1(world_,document.layers().matrix(),GameplayLayers::kCount)) {
    error_="Matriz de camadas do projeto não é recíproca";stop();return false;
  }
  struct ColliderSource {ObjectId object;const scene::Collider *value;};
  std::unordered_map<ObjectId,std::vector<ColliderSource>> colliders;
  const auto fail=[&](const SceneObject &entity,const std::string &reason) {error_=std::string(entity.name)+": "+reason;stop();return false;};
  for(const auto id:ids) {
    const auto &entity=*document.find(id);
    if(const auto *motor=entity.components.find(scene::DynamicBodyMotor::descriptor);motor&&document.activeInHierarchy(id)) {
      const auto &settings=static_cast<const scene::DynamicBodyMotor&>(*motor);const auto *body=physicsBody(entity);
      if(!settings.valid()||!body||characterComponent(entity)||
         (settings.enabled&&(body->motion!=scene::BodyMotion::Dynamic||body->sensor||body->freezePosition[0]||body->freezePosition[2]||(settings.jumpSpeed>0&&body->freezePosition[1]))))
        return fail(entity,"Motor dinâmico requer Body dinâmico sólido e eixos de locomoção livres; não combine com Character");
    }
    const auto *force=entity.components.find(scene::ConstantForce::descriptor);
    if(!force || !document.activeInHierarchy(id)) continue;
    const auto &value=static_cast<const scene::ConstantForce&>(*force);
    const auto *body=physicsBody(entity);
    if(!value.valid() || !body || (value.enabled && body->motion!=scene::BodyMotion::Dynamic))
      return fail(entity,"Força constante ativa requer Corpo físico dinâmico no mesmo objeto");
  }
  for(const auto id:ids) {
    const auto &entity=*document.find(id);
    if(!document.activeInHierarchy(id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *value=entity.components.at(i);
      if(&value->type()!=&scene::PhysicsEventConnection3D::descriptor) continue;
      const auto &connection=static_cast<const scene::PhysicsEventConnection3D&>(*value);
      if(!connection.enabled || connection.action==0) continue;
      const auto *body=physicsBody(entity);const auto *collider=colliderComponent(entity);
      if(!body || !collider || (collider->owner && collider->owner!=id))
        return fail(entity,"Conexão física requer corpo e colisor próprio no emissor");
      if((connection.event<3)!=body->sensor)
        return fail(entity,"Evento de sensor requer Sensor ligado; evento de contato requer Sensor desligado no Corpo físico");
    }
  }
  // Resolve ownership before creating anything; no nearest-parent inference.
  for(auto id:ids) {
    const auto &entity=*document.find(id);
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Collider::descriptor) continue;
      const auto &c=static_cast<const scene::Collider &>(*v);
      if(!c.enabled || !document.activeInHierarchy(id)) {
        // Conserve o corpo quando todas as suas formas estão desligadas.
        // Vínculo inválido continua diagnosticável quando a forma for ligada.
        if(referenceAccepts(document,id,scene::colliderReferences[0],c.owner,true)) {
          const ObjectId owner=c.owner?static_cast<ObjectId>(c.owner):id;
          if(document.activeInHierarchy(owner)) colliders.try_emplace(owner);
        }
        continue;
      }
      ObjectId owner=0;
      if(const auto *error=colliderOwnerForPhysics(document,id,c,owner)) return fail(entity,error);
      auto &parts=colliders[owner];if(parts.size()>=256) return fail(entity,"limite de 256 colisores por corpo");
      parts.push_back({id,&c});
      // O objeto que contribui a forma também tem a pose congelada pelo corpo:
      // movê-lo por script deslocaria a parte do composto sem a física saber.
      gameWorld.setAuthority(id,TransformAuthority::PhysicsBody);
    }
  }
  for(auto id:ids) {
    const auto &entity=*document.find(id);const auto *body=physicsBody(entity);
    if(!body||!document.activeInHierarchy(id)) continue;
    if(const auto *error=bodyHierarchyForPhysics(document,id)) return fail(entity,error);
    auto found=colliders.find(id);
    if(found==colliders.end()) return fail(entity,"corpo sem colisores vinculados");
    if(const auto *motor=entity.components.find(scene::DynamicBodyMotor::descriptor);motor&&static_cast<const scene::DynamicBodyMotor&>(*motor).enabled&&found->second.empty())
      return fail(entity,"motor sem colisores ativos vinculados ao corpo");
    float world[16],bodyFrame[16];Transform transform;
    if(const auto *error=bodyFrameForPhysics(document,id,world,transform,bodyFrame)) return fail(entity,error);
    std::vector<AetherCompoundPartV3> parts;parts.reserve(found->second.size());
    // Geometria das partes Malha; vive até a criação do corpo, que a copia.
    std::vector<std::vector<AetherVec3>> meshVertices(found->second.size());
    std::vector<std::vector<u32>> meshIndices(found->second.size());
    for(usize index=0;index<found->second.size();++index) {
      const auto &source=found->second[index];
      const auto &c=*source.value;Transform partTransform;const auto &object=*document.find(source.object);
      if(const auto *error=colliderPoseForPhysics(document,source.object,c,bodyFrame,partTransform)) return fail(object,error);
      const float x=std::abs(partTransform.scale[0]),y=std::abs(partTransform.scale[1]),z=std::abs(partTransform.scale[2]);
      AetherCompoundPartV3 part{};part.base.position={partTransform.position[0],partTransform.position[1],partTransform.position[2]};part.base.rotation=physicsRotation(partTransform);
      part.cooking=AetherMeshCookingDefaultsV1;
      switch(c.shape) {
        case scene::ColliderShape::Box:part.base.shape.kind=AetherShapeKind::Box;part.base.shape.boxHalfExtent={c.halfX*x,c.halfY*y,c.halfZ*z};break;
        case scene::ColliderShape::Sphere:part.base.shape.kind=AetherShapeKind::Sphere;part.base.shape.sphereRadius=c.radius*x;break;
        case scene::ColliderShape::Capsule:part.base.shape.kind=AetherShapeKind::Capsule;part.base.shape.sphereRadius=c.radius*x;part.base.shape.capsuleHalfHeight=c.halfHeight*y;break;
        case scene::ColliderShape::Cylinder:part.base.shape.kind=AetherShapeKind::Cylinder;part.base.shape.sphereRadius=c.radius*x;part.base.shape.capsuleHalfHeight=c.halfHeight*y;break;
        case scene::ColliderShape::Mesh: {
          if(const auto *error=colliderMeshForPhysics(document,source.object,c,id)) return fail(object,error);
          if(!geometry) return fail(object,"geometria de colisão indisponível neste mundo");
          float meshMatrix[16];if(!colliderMeshMatrixForPhysics(document,source.object,c,bodyFrame,meshMatrix))return fail(object,"Matriz de colisão não finita");
          if(!collisionMesh(*geometry,c,meshRenderer(object),meshMatrix,meshVertices[index],meshIndices[index]))
            return fail(object,"malha sem triângulos válidos para colisão");
          part.geometry=c.convex?AetherPartGeometry::ConvexHull:AetherPartGeometry::TriangleMesh;
          part.cooking.flags=c.optimizeCooking?static_cast<u32>(AetherMeshCookingOptimizeRuntime):0u;
          part.cooking.hullTolerance=c.hullTolerance*std::max({std::hypot(meshMatrix[0],meshMatrix[1],meshMatrix[2]),std::hypot(meshMatrix[4],meshMatrix[5],meshMatrix[6]),std::hypot(meshMatrix[8],meshMatrix[9],meshMatrix[10])});
          part.cooking.activeEdgeAngleDegrees=c.activeEdgeAngle;
          part.vertices=meshVertices[index].data();part.vertexCount=static_cast<u32>(meshVertices[index].size());
          part.indices=meshIndices[index].data();part.indexCount=static_cast<u32>(meshIndices[index].size());
          break;
        }
      }
      parts.push_back(part);
    }
    AetherBodyDescV2 desc{};desc.structSize=sizeof(desc);desc.apiVersion=AetherBodyApiVersionV2;desc.eventLayerMask=3;
    desc.position={world[12],world[13],world[14]};desc.rotation=physicsRotation(transform);
    u32 allowed=0;for(u32 a=0;a<3;++a){if(!body->freezePosition[a])allowed|=1u<<a;if(!body->freezeRotation[a])allowed|=1u<<(a+3);}
    desc.allowedDOFs=allowed==63?AetherAllowedDOFs::All:static_cast<AetherAllowedDOFs>(allowed);
    desc.motionType=static_cast<AetherMotionType>(body->motion);desc.friction=body->friction;desc.restitution=body->restitution;desc.isSensor=body->sensor;
    AetherBodyDynamicsV1 dynamics{sizeof(AetherBodyDynamicsV1),1,body->linearDamping,body->angularDamping,body->gravityFactor,
      {body->angularX,body->angularY,body->angularZ},body->allowSleep?1u:0u};
    const auto handle=AetherPhysics_CreateCompoundBodyV3(world_,&desc,parts.data(),static_cast<u32>(parts.size()),&dynamics);
    if(handle==AetherBodyHandle_Invalid||(body->motion==scene::BodyMotion::Dynamic&&!AetherPhysics_SetMassV2(world_,handle,body->mass)))
      return fail(entity,"Jolt recusou a composição ou a massa (malha convexa plana não tem casco sólido)");
    if(body->motion==scene::BodyMotion::Dynamic&&(!body->automaticCenterOfMass||!body->automaticInertia)) {
      // Centro de massa autoral em unidades locais do objeto; a escala vai junto.
      const AetherVec3 center{body->centerOfMass[0]*transform.scale[0],body->centerOfMass[1]*transform.scale[1],body->centerOfMass[2]*transform.scale[2]};
      const AetherVec3 inertia{body->inertia[0],body->inertia[1],body->inertia[2]};
      if(!AetherPhysics_SetBodyMassPropertiesV1(world_,handle,body->mass,body->automaticCenterOfMass?nullptr:&center,body->automaticInertia?nullptr:&inertia))
        return fail(entity,"Jolt recusou o centro de massa ou a inércia");
    }
    if(body->motion!=scene::BodyMotion::Static){
      AetherBodySimulationV1 settings;settings.maxLinearVelocity=body->maxLinearVelocity;settings.maxAngularVelocity=body->maxAngularVelocity;settings.continuousCollision=body->continuousCollision;settings.velocitySteps=static_cast<u32>(body->solverVelocitySteps);
      if(!AetherPhysics_ConfigureBodySimulationV1(world_,handle,&settings))return fail(entity,"Configuração avançada do corpo recusada");
    }
    if(body->motion!=scene::BodyMotion::Static) AetherPhysics_SetLinearVelocity(world_,handle,{body->velocityX,body->velocityY,body->velocityZ});
    objects_.emplace(handle,id);
    gameWorld.setAuthority(id,TransformAuthority::PhysicsBody);
    std::vector<Binding::ColliderIdentity> instances;instances.reserve(found->second.size());
    for(const auto &source:found->second) instances.push_back({source.object,source.value->instanceId()});
    // A camada de gameplay do objeto vale para o corpo inteiro. Dois colisores
    // do mesmo corpo não podem estar em camadas diferentes: o Jolt filtra por
    // corpo, e prometer o contrário seria uma propriedade sem efeito.
    if(!AetherPhysics_SetBodyGameplayLayerV1(world_,handle,entity.layer%GameplayLayers::kCount))
      return fail(entity,"camada de gameplay inválida");
    if((body->frictionCombine||body->restitutionCombine)&&
       !AetherPhysics_SetBodyMaterialCombineV1(world_,handle,body->frictionCombine,body->restitutionCombine))
      return fail(entity,"combinação do material físico recusada");
    // Material próprio por forma: a tabela segue a ordem das partes criadas.
    std::vector<AetherPartMaterialV1> partMaterials;bool ownMaterial=false;
    for(const auto &source:found->second) {
      const auto &c=*source.value;ownMaterial|=c.ownMaterial;
      partMaterials.push_back({c.friction,c.restitution,c.frictionCombine,c.restitutionCombine,c.ownMaterial?1u:0u});
    }
    if(ownMaterial&&!AetherPhysics_SetBodyPartMaterialsV1(world_,handle,partMaterials.data(),static_cast<u32>(partMaterials.size())))
      return fail(entity,"material próprio de forma recusado");
    bindings_.push_back({id,handle,{transform.scale[0],transform.scale[1],transform.scale[2]},
                         body->motion!=scene::BodyMotion::Static,std::move(instances),
                         {body->velocityX,body->velocityY,body->velocityZ},
                         {body->angularX,body->angularY,body->angularZ},body->instanceId(),body->motion==scene::BodyMotion::Dynamic});
    bindings_.back().interpolation=body->motion==scene::BodyMotion::Static?0u:body->interpolation;
    if(const auto *motor=entity.components.find(scene::DynamicBodyMotor::descriptor))dynamicMotors_.push_back({id,motor->instanceId()});
  }
  // All bodies exist now, including static anchors and forward references.
  for(auto id:ids) {
    const auto &entity=*document.find(id);if(!document.activeInHierarchy(id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Joint::descriptor) continue;
      const auto &joint=static_cast<const scene::Joint &>(*v);if(!joint.enabled||jointBroken(id,joint.instanceId())) continue;
      const auto issues=jointRequirementIssues(document,id,joint);
      if(!issues.empty()) return fail(entity,issues.front().message);
      const Binding *a=nullptr,*b=nullptr;for(const auto &binding:bindings_) {if(binding.id==id) a=&binding;if(binding.id==joint.connectedBody) b=&binding;}
      if(!a||!b) return fail(entity,"corpo conectado à junta está inativo");
      float ma[16],mb[16];if(!worldMatrix(document,id,ma)||!worldMatrix(document,b->id,mb)) return fail(entity,"referencial da junta inválido");
      AetherJointDescV3 extended{};extended.structSize=sizeof(extended);extended.apiVersion=3;auto &desc=extended.base;desc.structSize=sizeof(desc);desc.apiVersion=AetherJointApiVersionV2;desc.kind=static_cast<AetherJointKind>(joint.kind);desc.space=AetherJointSpace::World;
      desc.point1=transformPhysicsPoint(ma,joint.anchorA);desc.point2=transformPhysicsPoint(mb,joint.anchorB);
      desc.axis1=transformPhysicsPoint(ma,joint.axisA,true);desc.axis2=transformPhysicsPoint(mb,joint.axisB,true);
      const float units=joint.kind==scene::JointKind::Hinge?.01745329252f:1.0f;
      desc.limitsMin=joint.limitMin*units;desc.limitsMax=joint.limitMax*units;
      desc.motor={static_cast<AetherMotorState>(joint.motor),joint.motorVelocity*units,joint.motorPosition*units,joint.motorForce,joint.frequency,joint.damping};
      extended.normal1=transformPhysicsPoint(ma,joint.normalA,true);extended.normal2=transformPhysicsPoint(mb,joint.normalB,true);
      constexpr float radians=.01745329252f;
      extended.swingY=joint.swingY*radians;extended.swingZ=joint.swingZ*radians;extended.twistMin=joint.twistMin*radians;extended.twistMax=joint.twistMax*radians;
      for(u32 axis=0;axis<6;++axis) {
        const auto &input=joint.axes[axis];auto &output=extended.axes[axis];const float unit=axis<3?1.f:radians;
        output.motion=input.motion;output.minimum=input.minimum*unit;output.maximum=input.maximum*unit;output.friction=input.friction;
        output.motor={static_cast<AetherMotorState>(input.motor),(input.motor==1||input.motor==3)?input.velocity*unit:0.f,input.position*unit,input.force,input.frequency,input.damping};
      }
      auto first=a->body,second=b->body;
      if(joint.kind==scene::JointKind::SixDOF) {
        // Jolt drives body 2 relative to body 1. Authoring drives the OWNER
        // relative to connectedBody; swap complete frames, never just signs
        // (asymmetric limits and rotated frames must keep their meaning).
        std::swap(first,second);std::swap(desc.point1,desc.point2);
        std::swap(desc.axis1,desc.axis2);std::swap(extended.normal1,extended.normal2);
      }
      const auto jointHandle=AetherPhysics_CreateJointV3(world_,first,second,&extended);
      if(jointHandle==AetherJointHandle_Invalid) return fail(entity,"Jolt recusou a junta #"+std::to_string(joint.instanceId()));
      joints_.push_back({id,joint.instanceId(),jointHandle,joint.breakForce,joint.breakTorque});
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
    CharacterBinding binding{};binding.id=id;binding.instance=character->instanceId();binding.eyeHeight=character->eyeHeight;
    binding.shapeFingerprint[0]=character->radius;binding.shapeFingerprint[1]=character->halfHeight;binding.shapeFingerprint[2]=character->eyeHeight;binding.shapeFingerprint[3]=character->slopeDegrees;
    binding.jumpSpeed=character->jumpSpeed;
    float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;Transform transform;
    if(!worldMatrix(document,id,binding.world)||!localTransformForWorld(binding.world,identity,transform)) {stop();return false;}
    for(float scale:transform.scale) if(std::abs(scale-1)>.0001f) {stop();return false;}
    physics::CharacterMotorSettings settings;
    settings.radius=character->radius;settings.standingHalfHeight=character->halfHeight;
    settings.eyeHeight=character->eyeHeight;settings.movementUnitsPerSecond=character->speed;
    settings.maximumSlopeRadians=character->slopeDegrees*.01745329252f;
    settings.gravityUnitsPerSecondSquared=character->gravity;settings.stepHeight=character->stepHeight;settings.floorSnapLength=character->floorSnapLength;settings.inheritPlatformHorizontal=character->inheritPlatformHorizontal;
    binding.motor=std::make_unique<physics::CharacterMotor>();
    if(!binding.motor->initializeInWorld(world_,{binding.world[12],binding.world[13]+binding.eyeHeight,binding.world[14]},settings)) {stop();return false;}
    gameWorld.setAuthority(id,TransformAuthority::Character);
    characters_.push_back(std::move(binding));
  }
  ownerWorldId_=gameWorld.worldId();error_.clear();return true;
}
bool ScenePhysics::rebuild(GameWorld &world,const CollisionGeometrySource *geometry) {
  if(ownerWorldId_!=world.worldId())return start(world,geometry);
  struct Motion {ObjectId id;u64 instance;AetherVec3 linear{},angular{};AetherBodyMomentumV1 momentum;float authored[3],authoredAngular[3];};
  std::vector<Motion> motions;motions.reserve(bindings_.size());
  for(const auto &binding:bindings_) {
    if(!binding.moving) continue;
    Motion motion{binding.id,binding.instance,{},{},{},{binding.authoredVelocity[0],binding.authoredVelocity[1],binding.authoredVelocity[2]},
                  {binding.authoredAngular[0],binding.authoredAngular[1],binding.authoredAngular[2]}};
    if(!AetherPhysics_TryGetBodyVelocityV1(world_,binding.body,&motion.linear)) continue;
    AetherPhysics_TryGetBodyAngularVelocityV1(world_,binding.body,&motion.angular);
    AetherPhysics_TryGetBodyMomentumV1(world_,binding.body,&motion.momentum);
    motions.push_back(motion);
  }
  struct CharacterMotion {ObjectId id;u64 instance;physics::CharacterMotor::MotionState motor;float pose[16],shape[4],input[6];bool scripted;};
  std::vector<CharacterMotion> characterMotions;characterMotions.reserve(characters_.size());
  for(const auto&c:characters_) {
    CharacterMotion motion{c.id,c.instance,c.motor->motionState(),{},{},{c.right,c.forward,c.yaw,c.scriptRight,c.scriptForward,c.scriptYaw},c.scriptMoveActive};
    std::copy(c.world,c.world+16,motion.pose);std::copy(c.shapeFingerprint,c.shapeFingerprint+4,motion.shape);characterMotions.push_back(motion);
  }
  const double accumulated=accumulated_;
  const auto dynamicInput=dynamicMotors_;
  if(!start(world,geometry)) return false;
  for(auto &m:dynamicMotors_)for(const auto &old:dynamicInput)if(m.id==old.id&&m.instance==old.instance){m.right=old.right;m.forward=old.forward;m.yaw=old.yaw;m.scriptRight=old.scriptRight;m.scriptForward=old.scriptForward;m.scriptYaw=old.scriptYaw;m.scriptMoveActive=old.scriptMoveActive;break;}
  accumulated_=accumulated;
  for(const auto &motion:motions) for(const auto &binding:bindings_) {
    if(binding.id!=motion.id||binding.instance!=motion.instance||!binding.moving) continue;
    u32 mask=0;
    if(std::equal(binding.authoredVelocity,binding.authoredVelocity+3,motion.authored))mask|=1;
    if(std::equal(binding.authoredAngular,binding.authoredAngular+3,motion.authoredAngular))mask|=2;
    if(mask&&binding.dynamic&&(motion.momentum.flags&1u)) {
      if(!AetherPhysics_RestoreBodyMomentumV1(world_,binding.body,&motion.momentum,mask)){error_="Não foi possível conservar momentum do Body recriado";return false;}
    } else {
      if(mask&1u)AetherPhysics_SetLinearVelocity(world_,binding.body,motion.linear);
      if(mask&2u)AetherPhysics_SetBodyAngularVelocityV1(world_,binding.body,motion.angular);
    }
    break;
  }
  // Preserve motion only for the same component, shape and world pose. An
  // explicit placement or capsule edit retains the existing reset policy.
  // Body velocities are restored first, then support is queried in the NEW world.
  for(const auto&motion:characterMotions)for(auto&c:characters_) {
    if(c.id!=motion.id||c.instance!=motion.instance)continue;
    bool unchanged=true;for(u32 n=0;n<16;++n)if(std::abs(c.world[n]-motion.pose[n])>1e-4f)unchanged=false;
    for(u32 n=0;n<4;++n)if(c.shapeFingerprint[n]!=motion.shape[n])unchanged=false;
    if(!unchanged)break;
    if(!c.motor->restoreMotionState(motion.motor)){error_="Não foi possível restaurar o movimento do personagem";return false;}
    c.right=motion.input[0];c.forward=motion.input[1];c.yaw=motion.input[2];c.scriptRight=motion.input[3];c.scriptForward=motion.input[4];c.scriptYaw=motion.input[5];c.scriptMoveActive=motion.scripted;break;
  }
  return true;
}
void ScenePhysics::releaseObject(ObjectId id) {
  std::erase_if(dynamicMotors_,[&](const auto &m){return m.id==id;});
  for(auto i=bindings_.begin();i!=bindings_.end();++i) if(i->id==id) {
    if(world_) AetherPhysics_DestroyBody(world_,i->body);
    objects_.erase(i->body);bindings_.erase(i);break;
  }
  for(auto i=characters_.begin();i!=characters_.end();++i) if(i->id==id) {characters_.erase(i);break;}
}
namespace {
AetherQueryFilterV1 nativeFilter(const QueryFilter &filter,AetherBodyHandle ignore) {
  AetherQueryFilterV1 native{};
  native.structSize=sizeof(native);
  native.apiVersion=AetherQueryFilterApiVersionV1;
  const u32 mask=(filter.includeStatic?static_cast<u32>(AetherQueryLayerMask::Static):0u)|
                 (filter.includeDynamic?static_cast<u32>(AetherQueryLayerMask::Dynamic):0u);
  native.layerMask=static_cast<AetherQueryLayerMask>(mask);
  native.ignoreBody=ignore;
  native.includeSensors=filter.includeSensors?1u:0u;
  native.gameplayLayerMask=filter.gameplayLayerMask;
  return native;
}
AetherShapeDesc nativeShape(const QueryShapeDesc &shape) {
  AetherShapeDesc native{};
  native.kind=static_cast<AetherShapeKind>(shape.kind);
  native.boxHalfExtent={shape.halfExtent[0],shape.halfExtent[1],shape.halfExtent[2]};
  native.sphereRadius=shape.radius;
  native.capsuleHalfHeight=shape.halfHeight;
  return native;
}
bool finite3(const float *v) {return v&&std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
float length3(const float *v) {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
AetherQuat queryRotation(const QueryShapeDesc &shape) {
  const float *q=shape.rotation;
  const float inv=1/std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
  return {q[0]*inv,q[1]*inv,q[2]*inv,q[3]*inv};
}
}
bool validPhysicsQueryVector(const float *v) {return finite3(v);}
bool validPhysicsQueryRay(const float *origin,const float *translation) {
  return finite3(origin)&&finite3(translation)&&std::isfinite(length3(translation))&&length3(translation)>0;
}
bool validPhysicsQueryShape(const QueryShapeDesc &s) {
  const auto kind=static_cast<u32>(s.kind);
  if(kind>3||!finite3(s.halfExtent)||!finite3(s.rotation)||!std::isfinite(s.rotation[3])||
     !std::isfinite(s.radius)||!std::isfinite(s.halfHeight)) return false;
  const float q=s.rotation[0]*s.rotation[0]+s.rotation[1]*s.rotation[1]+s.rotation[2]*s.rotation[2]+s.rotation[3]*s.rotation[3];
  return std::isfinite(q)&&q>0&&(kind==0?(s.halfExtent[0]>0&&s.halfExtent[1]>0&&s.halfExtent[2]>0):s.radius>0)&&
         (kind<2||s.halfHeight>0);
}
QueryHit ScenePhysics::describeHit(AetherBodyHandle body,u32 subShapeId) const {
  QueryHit hit{};
  hit.object=objectForBody(body);
  u64 part=0;
  if(AetherPhysics_GetSubShapeUserDataV1(const_cast<AetherPhysicsWorld *>(world_),body,subShapeId,&part))
    for(const auto &binding:bindings_)
      if(binding.body==body&&part<binding.colliders.size()) {
        const auto &identity=binding.colliders[static_cast<usize>(part)];
        hit.colliderInstance=identity.instance;hit.colliderObject=identity.object;break;
      }
  return hit;
}
bool ScenePhysics::rayCast(const float origin[3],const float direction[3],const QueryFilter &filter,QueryHit &out) const {
  if(!world_||!validPhysicsQueryRay(origin,direction)) return false;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  AetherRayQueryHitV1 hit{};
  if(!AetherPhysics_RayCastClosestV2(const_cast<AetherPhysicsWorld *>(world_),{origin[0],origin[1],origin[2]},
      {direction[0],direction[1],direction[2]},&native,&hit)) return false;
  out=describeHit(hit.body,hit.subShapeId);
  if(!out.object) return false;
  out.fraction=hit.fraction;
  out.distance=hit.fraction*length3(direction);
  out.point[0]=hit.point.x;out.point[1]=hit.point.y;out.point[2]=hit.point.z;
  out.normal[0]=hit.normal.x;out.normal[1]=hit.normal.y;out.normal[2]=hit.normal.z;
  out.hasNormal=hit.fraction>0&&length3(out.normal)>1e-6f;
  out.isSensor=hit.isSensor!=0;
  return true;
}
u32 ScenePhysics::rayCastAll(const float origin[3],const float direction[3],const QueryFilter &filter,
                             QueryHit *out,u32 capacity) const {
  if(!world_||!validPhysicsQueryRay(origin,direction)) return 0;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  std::vector<AetherRayQueryHitV1> hits(capacity);
  const auto total=AetherPhysics_RayCastAllV2(const_cast<AetherPhysicsWorld *>(world_),{origin[0],origin[1],origin[2]},
      {direction[0],direction[1],direction[2]},&native,capacity?hits.data():nullptr,static_cast<i32>(capacity));
  if(total<=0) return 0;
  const u32 copied=std::min<u32>(capacity,static_cast<u32>(total));
  const float range=length3(direction);
  for(u32 i=0;i<copied&&out;++i) {
    out[i]=describeHit(hits[i].body,hits[i].subShapeId);
    out[i].fraction=hits[i].fraction;
    out[i].distance=hits[i].fraction*range;
    out[i].point[0]=hits[i].point.x;out[i].point[1]=hits[i].point.y;out[i].point[2]=hits[i].point.z;
    out[i].normal[0]=hits[i].normal.x;out[i].normal[1]=hits[i].normal.y;out[i].normal[2]=hits[i].normal.z;
    out[i].hasNormal=hits[i].fraction>0&&length3(out[i].normal)>1e-6f;
    out[i].isSensor=hits[i].isSensor!=0;
  }
  return static_cast<u32>(total);
}
bool ScenePhysics::shapeCast(const QueryShapeDesc &shape,const float origin[3],const float direction[3],
                             const QueryFilter &filter,QueryHit &out) const {
  if(!world_||!validPhysicsQueryRay(origin,direction)||!validPhysicsQueryShape(shape)) return false;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  const auto nativeDesc=nativeShape(shape);
  AetherShapeQueryHit hit{};u32 subShape=0,sensor=0;
  if(!AetherPhysics_ShapeCastClosestV2(const_cast<AetherPhysicsWorld *>(world_),&nativeDesc,
      {origin[0],origin[1],origin[2]},queryRotation(shape),
      {direction[0],direction[1],direction[2]},&native,&hit,&subShape,&sensor)) return false;
  out=describeHit(hit.body,subShape);
  if(!out.object) return false;
  out.fraction=hit.fraction;
  out.distance=hit.fraction*length3(direction);
  out.point[0]=hit.contactPointOnHit.x;out.point[1]=hit.contactPointOnHit.y;out.point[2]=hit.contactPointOnHit.z;
  // O eixo de penetração do Jolt não é normalizado; normalizá-lo aqui é o que
  // torna o valor utilizável como direção sem que cada chamador refaça a conta.
  const float axis[3]{hit.penetrationAxis.x,hit.penetrationAxis.y,hit.penetrationAxis.z};
  const float size=length3(axis);
  if(size>1e-6f) {
    for(u32 i=0;i<3;++i) out.normal[i]=axis[i]/size;
    out.hasNormal=true;
  }
  out.isSensor=sensor!=0;
  return true;
}
u32 ScenePhysics::overlap(const QueryShapeDesc &shape,const float origin[3],const QueryFilter &filter,
                          QueryHit *out,u32 capacity) const {
  if(!world_||!finite3(origin)||!validPhysicsQueryShape(shape)) return 0;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  const auto nativeDesc=nativeShape(shape);
  std::vector<AetherShapeQueryHit> hits(capacity);
  std::vector<u32> subShapes(capacity),sensors(capacity);
  const auto total=AetherPhysics_OverlapShapeV2(const_cast<AetherPhysicsWorld *>(world_),&nativeDesc,
      {origin[0],origin[1],origin[2]},queryRotation(shape),
      &native,capacity?hits.data():nullptr,capacity?subShapes.data():nullptr,capacity?sensors.data():nullptr,
      static_cast<i32>(capacity));
  if(total<=0) return 0;
  const u32 copied=std::min<u32>(capacity,static_cast<u32>(total));
  for(u32 i=0;i<copied&&out;++i) {
    out[i]=describeHit(hits[i].body,subShapes[i]);
    out[i].point[0]=hits[i].contactPointOnHit.x;out[i].point[1]=hits[i].contactPointOnHit.y;out[i].point[2]=hits[i].contactPointOnHit.z;
    out[i].isSensor=sensors[i]!=0;
    // Sobreposição parada não tem direção ao longo de quê: fração e normal
    // continuam ausentes em vez de zeradas.
  }
  return static_cast<u32>(total);
}
bool ScenePhysics::setCharacterMove(ObjectId id,float right,float forward,float yaw) {
  if(!std::isfinite(right)||!std::isfinite(forward)||!std::isfinite(yaw)) return false;
  for(auto &c:characters_) if(c.id==id) {c.right=right;c.forward=forward;c.yaw=yaw;return true;}
  return false;
}
void ScenePhysics::beginScriptInputFrame() {
  for(auto &c:characters_) c.scriptMoveActive=false;
  for(auto &m:dynamicMotors_)m.scriptMoveActive=false;
}
bool ScenePhysics::setCharacterScriptMove(ObjectId id,float right,float forward,float yaw) {
  if(!std::isfinite(right)||!std::isfinite(forward)||!std::isfinite(yaw)||
     right<-1||right>1||forward<-1||forward>1) return false;
  for(auto &c:characters_) if(c.id==id) {
    c.scriptRight=right;c.scriptForward=forward;c.scriptYaw=yaw;c.scriptMoveActive=true;
    return true;
  }
  return false;
}
bool ScenePhysics::jumpCharacter(ObjectId id,const GameWorld *world) {
  for(auto &c:characters_) if(c.id==id) {
    const auto*entity=world?world->graph().find(id):nullptr;const auto*settings=entity?characterComponent(*entity):nullptr;
    return c.motor->jump(settings?settings->jumpSpeed:c.jumpSpeed);
  }
  return false;
}
bool ScenePhysics::applyBodyForce(ObjectId id,const float *v,u32 kind) {
  if(!v||kind>3) return false;
  for(const auto &b:bindings_) if(b.id==id) return AetherPhysics_ApplyBodyForceV1(world_,b.body,{v[0],v[1],v[2]},static_cast<AetherBodyForceKind>(kind))!=0;
  return false;
}
WorldStatus ScenePhysics::bodyCommand(const GameWorld &world,ObjectHandle handle,u64 instance,u32 op,AetherVec3 value,AetherVec3 point,AetherBodyStateV1 &out) {
  if(!world.running()||!world_)return WorldStatus::NotRunning;
  if(ownerWorldId_!=world.worldId())return WorldStatus::ForeignWorld;
  const auto status=world.validate(handle);if(status!=WorldStatus::Ok)return status;
  const auto *owner=world.find(handle);const auto *component=owner->components.findInstance(instance);
  if(!component)return WorldStatus::ComponentMissing;
  if(!world.activeInHierarchy(handle))return WorldStatus::Rejected;
  if(op>=100&&op<=103) {
    if(component->type().id!=scene::DynamicBodyMotor::descriptor.id)return WorldStatus::ComponentMissing;
    if(!static_cast<const scene::DynamicBodyMotor&>(*component).enabled)return WorldStatus::ComponentUnavailable;
    auto found=std::find_if(dynamicMotors_.begin(),dynamicMotors_.end(),[&](const auto &m){return m.id==handle.id&&m.instance==instance;});
    if(found==dynamicMotors_.end())return WorldStatus::ComponentUnavailable;
    if(op==100&&!setDynamicMotorScriptMove(handle.id,value.x,value.y,value.z))return WorldStatus::InvalidArgument;
    if(op==101&&!jumpDynamicMotor(handle.id))return WorldStatus::Rejected;
    if(op==102)found->scriptMoveActive=false;
    op=0; // Same real body snapshot as PhysicsBodyRuntime; no repacked fake state.
  } else if(component->type().id!=scene::PhysicsBody::descriptor.id)return WorldStatus::ComponentMissing;
  for(const auto &binding:bindings_)if(binding.id==handle.id)return AetherPhysics_BodyCommandV1(world_,binding.body,op,value,point,&out)?WorldStatus::Ok:WorldStatus::Rejected;
  return WorldStatus::Rejected;
}

bool ScenePhysics::getBodyVelocity(ObjectId id,float *out) const {
  if(!out) return false;
  for(const auto &b:bindings_) if(b.id==id) {AetherVec3 v;if(!AetherPhysics_TryGetBodyVelocityV1(world_,b.body,&v)) return false;out[0]=v.x;out[1]=v.y;out[2]=v.z;return true;}
  return false;
}
bool ScenePhysics::setBodyVelocity(ObjectId id,const float *v) {
  if(!v||!std::isfinite(v[0])||!std::isfinite(v[1])||!std::isfinite(v[2])) return false;
  for(const auto &b:bindings_) if(b.id==id && b.moving) {AetherPhysics_SetLinearVelocity(world_,b.body,{v[0],v[1],v[2]});return true;}
  return false;
}
bool ScenePhysics::moveKinematic(ObjectId id,const float *v) {
  if(!v) return false;
  for(u32 i=0;i<7;++i) if(!std::isfinite(v[i])) return false;
  const float n=std::sqrt(v[3]*v[3]+v[4]*v[4]+v[5]*v[5]+v[6]*v[6]);if(n<1e-8f) return false;
  for(const auto &b:bindings_) if(b.id==id)
    return AetherPhysics_MoveKinematicV2(world_,b.body,{v[0],v[1],v[2]},{v[3]/n,v[4]/n,v[5]/n,v[6]/n},1.0f/60.0f)!=0;
  return false;
}
#include "runtime/scene_physics_fields.inl"
bool ScenePhysics::jointBroken(ObjectId owner,u64 instance) const {
  for(const auto &broken:brokenJoints_) if(broken.first==owner&&broken.second==instance) return true;
  return false;
}
bool ScenePhysics::advance(double elapsed,GameWorld &world,bool (*beforeStep)(void *,float),void *context,bool (*trigger)(void *,ObjectId,ObjectId,u32),bool (*contact)(void *,const ContactEvent &),bool (*jointBroken)(void *,ObjectId,u64,float),bool (*characterHit)(void *,const CharacterHit &)) {
  if(!world_ || !std::isfinite(elapsed) || elapsed<0) {error_="Mundo físico ausente ou delta de tempo inválido";return false;}
  constexpr double fixed=1.0/60.0;
  // Bound catch-up after surface/lifecycle stalls; never feed a large dt to Jolt.
  accumulated_+=std::min(elapsed,.25);
  while(accumulated_+1e-9>=fixed) {
    if(beforeStep&&!beforeStep(context,static_cast<float>(fixed))) {if(error_.empty())error_="Callback FixedUpdate, reconciliação ou Physics2D recusou o passo";return false;}
    if(!applyDynamicMotors(world,static_cast<float>(fixed))||!applyContinuousForces(world)||!applyPhysicsFields(world,static_cast<float>(fixed))) return false;
    for(auto &c:characters_) {
      const auto*entity=world.graph().find(c.id);const auto*settings=entity?characterComponent(*entity):nullptr;
      if(!settings||!c.motor->configureMotion(settings->speed,settings->gravity,settings->stepHeight,settings->floorSnapLength,settings->inheritPlatformHorizontal))return false;
      c.jumpSpeed=settings->jumpSpeed;
      const bool scripted=c.scriptMoveActive;
      const float right=scripted?c.scriptRight:c.right;
      const float forward=scripted?c.scriptForward:c.forward;
      const float yaw=scripted?c.scriptYaw:c.yaw;
      if(!c.motor->update(right,forward,yaw,static_cast<float>(fixed))) return false;
      if(!characterHit) continue;
      const auto total=AetherPhysics_GetCharacterContactsV1(c.motor->physicsWorld(),c.motor->handle(),nullptr,0);
      if(total<=0) continue;
      characterContacts_.resize(static_cast<usize>(total));
      const auto written=AetherPhysics_GetCharacterContactsV1(c.motor->physicsWorld(),c.motor->handle(),characterContacts_.data(),total);
      std::vector<ObjectId> reported;
      for(ae::i32 k=0;k<std::min(written,total);++k) {
        const auto &hit=characterContacts_[static_cast<usize>(k)];
        if(!(hit.flags&1u)||(hit.flags&2u)) continue;
        const auto other=objects_.find(hit.body);if(other==objects_.end()) continue;
        if(std::find(reported.begin(),reported.end(),other->second)!=reported.end()) continue;
        reported.push_back(other->second);
        CharacterHit event{c.id,c.instance,other->second,{hit.point.x,hit.point.y,hit.point.z},{hit.normal.x,hit.normal.y,hit.normal.z}};
        if(!characterHit(context,event)) return false;
      }
    }
    if(const auto result=AetherPhysics_StepV2(world_,static_cast<float>(fixed),1);result!=0) {error_="Solver físico recusou o passo: "+std::to_string(result);return false;}
    accumulated_-=fixed;
    if(!synchronizePoses(world)) return false;
    // Quebra: a força média do passo é o impulso dividido pelo passo.
    for(auto &joint:joints_) {
      if(joint.handle==AetherJointHandle_Invalid||(joint.breakForce<=0&&joint.breakTorque<=0)) continue;
      float linear=0,angular=0;
      if(!AetherPhysics_GetJointImpulseV1(world_,joint.handle,&linear,&angular)) continue;
      const float force=linear/static_cast<float>(fixed),torque=angular/static_cast<float>(fixed);
      const bool byForce=joint.breakForce>0&&force>joint.breakForce,byTorque=joint.breakTorque>0&&torque>joint.breakTorque;
      if(!byForce&&!byTorque) continue;
      AetherPhysics_DestroyJoint(world_,joint.handle);joint.handle=AetherJointHandle_Invalid;
      brokenJoints_.emplace_back(joint.owner,joint.instance);
      if(jointBroken&&!jointBroken(context,joint.owner,joint.instance,byForce?force:torque)) return false;
    }
    if(trigger) {
      const auto count=AetherPhysics_GetTriggerEvents(world_,nullptr,0);
      if(count<0||count>1024*1024) {error_="Quantidade de eventos físicos inválida";return false;}
      events_.resize(static_cast<usize>(count));
      if(count&&AetherPhysics_GetTriggerEvents(world_,events_.data(),count)!=count) return false;
      // Native events are directed sensor -> other and aggregate compound parts.
      for(const auto &event:events_) {
        const auto sensor=objects_.find(event.sensor),other=objects_.find(event.other);
        if(sensor==objects_.end()||other==objects_.end()) continue;
        // Area3D do Godot: sensor sem monitoring não publica; corpo sem monitorable não é visto.
        const auto *sensorObject=world.graph().find(sensor->second);const auto *otherObject=world.graph().find(other->second);
        const auto *sensorBody=sensorObject?physicsBody(*sensorObject):nullptr;const auto *otherBody=otherObject?physicsBody(*otherObject):nullptr;
        if((sensorBody&&!sensorBody->monitoring)||(otherBody&&!otherBody->monitorable)) continue;
        if(!trigger(context,sensor->second,other->second,static_cast<u32>(event.type))) return false;
      }
    }
    if(contact) {
      // Contatos sólidos são a outra metade dos eventos: o par onde NENHUM dos
      // dois é sensor. Também saem por passo físico, agregados por par de
      // corpos, e a normal só acompanha Enter/Stay — o Jolt não informa
      // geometria quando o contato termina.
      const auto count=AetherPhysics_GetContactEventsV1(world_,nullptr,0);
      if(count<0||count>1024*1024) {error_="Quantidade de contatos inválida";return false;}
      contacts_.resize(static_cast<usize>(count));
      if(count&&AetherPhysics_GetContactEventsV1(world_,contacts_.data(),count)!=count) return false;
      for(const auto &event:contacts_) {
        const auto first=objects_.find(event.first),second=objects_.find(event.second);
        if(first==objects_.end()||second==objects_.end()) continue;
        ContactEvent value{};
        value.first=first->second;value.second=second->second;
        value.phase=static_cast<u32>(event.type);
        value.hasNormal=event.hasNormal!=0;
        value.normal[0]=event.normal.x;value.normal[1]=event.normal.y;value.normal[2]=event.normal.z;
        if(!contact(context,value)) return false;
      }
    }
  }
  return publishInterpolatedPoses(world);
}
bool ScenePhysics::applyContinuousForces(GameWorld &world) {
  // Iterate existing body bindings, not every schema/object and not a new
  // allocation per substep. Edits/removals are read after FixedUpdate's flush.
  for(const auto &binding:bindings_) {
    const auto *entity=world.graph().find(binding.id);
    if(!entity || !world.activeInHierarchy(world.handle(binding.id))) continue;
    const auto *component=entity->components.find(scene::ConstantForce::descriptor);
    if(!component) continue;
    const auto &f=static_cast<const scene::ConstantForce&>(*component);
    if(!f.enabled) continue;
    const auto *body=physicsBody(*entity);
    if(!body || body->motion!=scene::BodyMotion::Dynamic || !f.valid()) {
      error_=std::string(entity->name)+": Força constante ativa requer Corpo físico dinâmico";return false;
    }
    const bool local=f.relativeForceX!=0 || f.relativeForceY!=0 || f.relativeForceZ!=0 ||
                     f.relativeTorqueX!=0 || f.relativeTorqueY!=0 || f.relativeTorqueZ!=0;
    AetherQuat q{0,0,0,1};
    if(local) {
      AetherVec3 position;
      if(!AetherPhysics_TryGetBodyPoseV2(world_,binding.body,&position,&q)) {
        error_="Não foi possível ler a orientação física da Força constante";return false;
      }
    }
    const auto rotate=[&](float x,float y,float z) {
      const float tx=2*(q.y*z-q.z*y),ty=2*(q.z*x-q.x*z),tz=2*(q.x*y-q.y*x);
      return AetherVec3{x+q.w*tx+q.y*tz-q.z*ty,y+q.w*ty+q.z*tx-q.x*tz,z+q.w*tz+q.x*ty-q.y*tx};
    };
    auto force=rotate(f.relativeForceX,f.relativeForceY,f.relativeForceZ);
    force.x+=f.forceX;force.y+=f.forceY;force.z+=f.forceZ;
    auto torque=rotate(f.relativeTorqueX,f.relativeTorqueY,f.relativeTorqueZ);
    torque.x+=f.torqueX;torque.y+=f.torqueY;torque.z+=f.torqueZ;
    // Zero force must not wake a resting body, whereas a nonzero force must.
    if((force.x!=0 || force.y!=0 || force.z!=0) &&
       !AetherPhysics_ApplyBodyForceV1(world_,binding.body,force,AetherBodyForceKind::Force)) {
      error_="O solver recusou a força contínua";return false;
    }
    if((torque.x!=0 || torque.y!=0 || torque.z!=0) &&
       !AetherPhysics_ApplyBodyForceV1(world_,binding.body,torque,AetherBodyForceKind::Torque)) {
      error_="O solver recusou o torque contínuo";return false;
    }
  }
  return true;
}
bool ScenePhysics::synchronizePoses(GameWorld &world) {
  auto &document=world.poseGraph();
  for(auto &binding:bindings_) {
    if(!binding.moving) continue;
    AetherVec3 p;AetherQuat q;
    if(!AetherPhysics_TryGetBodyPoseV2(world_,binding.body,&p,&q)) {error_="Não foi possível ler a pose do corpo "+std::to_string(binding.id);return false;}
    binding.previousPosition=binding.hasPose?binding.currentPosition:p;binding.previousRotation=binding.hasPose?binding.currentRotation:q;
    binding.currentPosition=p;binding.currentRotation=q;binding.hasPose=true;
    // Interpolados publicam uma vez por quadro, em publishInterpolatedPoses.
    if(binding.interpolation) continue;
    if(!publishBodyPose(world,binding,p,q)) return false;
  }
  for(auto &c:characters_) {
    const auto eye=c.motor->eyePosition();c.world[12]=eye.x;c.world[13]=eye.y-c.eyeHeight;c.world[14]=eye.z;
    const auto *entity=document.find(c.id);if(!entity) continue;
    float parent[16]{};parent[0]=parent[5]=parent[10]=parent[15]=1;
    if(entity->parent&&!worldMatrix(document,entity->parent,parent)) return false;
    Transform local;
    if(!localTransformForWorld(c.world,parent,local)||!document.setTransform(c.id,local)) return false;
  }
  return true;
}
// Unity Rigidbody.interpolation: a pose desenhada fica entre o passo anterior
// e o último (Interpolar) ou é projetada pela velocidade (Extrapolar), pela
// fração do passo que sobrou no acumulador. A física continua no último passo.
bool ScenePhysics::publishInterpolatedPoses(GameWorld &world) {
  const float alpha=static_cast<float>(std::clamp(accumulated_*60.0,0.0,1.0));
  for(const auto &binding:bindings_) {
    if(!binding.moving||!binding.interpolation||!binding.hasPose) continue;
    AetherVec3 p=binding.currentPosition;AetherQuat q=binding.currentRotation;
    if(binding.interpolation==1) {
      const auto &a=binding.previousPosition;const auto &qa=binding.previousRotation;const auto &qb=binding.currentRotation;
      p={a.x+(p.x-a.x)*alpha,a.y+(p.y-a.y)*alpha,a.z+(p.z-a.z)*alpha};
      // Nlerp pelo caminho curto: entre passos de 1/60 s a diferença é pequena.
      const float sign=qa.x*qb.x+qa.y*qb.y+qa.z*qb.z+qa.w*qb.w<0?-1.f:1.f;
      q={qa.x+(sign*qb.x-qa.x)*alpha,qa.y+(sign*qb.y-qa.y)*alpha,qa.z+(sign*qb.z-qa.z)*alpha,qa.w+(sign*qb.w-qa.w)*alpha};
    } else {
      AetherVec3 v{},w{};const float dt=alpha/60.f;
      AetherPhysics_TryGetBodyVelocityV1(world_,binding.body,&v);AetherPhysics_TryGetBodyAngularVelocityV1(world_,binding.body,&w);
      p={p.x+v.x*dt,p.y+v.y*dt,p.z+v.z*dt};
      // q' = q + ½·(ω,0)·q·dt
      const AetherQuat r=q;
      q={r.x+.5f*dt*(w.x*r.w+w.y*r.z-w.z*r.y),r.y+.5f*dt*(w.y*r.w+w.z*r.x-w.x*r.z),r.z+.5f*dt*(w.z*r.w+w.x*r.y-w.y*r.x),r.w-.5f*dt*(w.x*r.x+w.y*r.y+w.z*r.z)};
    }
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(!(length>1e-6f)) continue;
    q={q.x/length,q.y/length,q.z/length,q.w/length};
    if(!publishBodyPose(world,binding,p,q)) return false;
  }
  return true;
}
bool ScenePhysics::publishBodyPose(GameWorld &world,const Binding &binding,AetherVec3 p,AetherQuat q) {
  auto &document=world.poseGraph();
  {
    float world[16]{
      (1-2*(q.y*q.y+q.z*q.z))*binding.scale[0],2*(q.x*q.y+q.w*q.z)*binding.scale[0],2*(q.x*q.z-q.w*q.y)*binding.scale[0],0,
      2*(q.x*q.y-q.w*q.z)*binding.scale[1],(1-2*(q.x*q.x+q.z*q.z))*binding.scale[1],2*(q.y*q.z+q.w*q.x)*binding.scale[1],0,
      2*(q.x*q.z+q.w*q.y)*binding.scale[2],2*(q.y*q.z-q.w*q.x)*binding.scale[2],(1-2*(q.x*q.x+q.y*q.y))*binding.scale[2],0,p.x,p.y,p.z,1};
    // Um objeto removido no ponto seguro já teve o corpo solto; se a ordem
    // inverter, ignorar é correto — publicar pose de quem não existe não é.
    const auto *entity=document.find(binding.id);if(!entity) return true;
    float parent[16]{};parent[0]=parent[5]=parent[10]=parent[15]=1;
    if(entity->parent && !worldMatrix(document,entity->parent,parent)) {error_=std::string(entity->name)+": transformação do pai físico inválida";return false;}
    Transform local;
    if(!localTransformForWorld(world,parent,local)) {error_=std::string(entity->name)+": pose física não pode ser representada no referencial do pai";return false;}
    if(!document.setTransform(binding.id,local)) {error_=std::string(entity->name)+": publicação da pose física recusada";return false;}
  }
  return true;
}
}

namespace ae::runtime {
WorldStatus ScenePhysics::characterState(const GameWorld &world,ObjectId id,physics::CharacterMotor::RuntimeState &out) const {
  if(!world_||!world.running())return WorldStatus::NotRunning;
  if(ownerWorldId_!=world.worldId())return WorldStatus::ForeignWorld;
  const auto handle=world.handle(id);const auto status=world.validate(handle);if(status!=WorldStatus::Ok)return status;
  const auto *entity=world.find(handle);const auto *component=entity->components.find(scene::Character::descriptor);
  if(!component)return WorldStatus::ComponentMissing;
  if(!world.activeInHierarchy(handle))return WorldStatus::ComponentUnavailable;
  for(const auto &binding:characters_)if(binding.id==id&&binding.instance==component->instanceId())
    return binding.motor->runtimeState(out)?WorldStatus::Ok:WorldStatus::ComponentUnavailable;
  return WorldStatus::ComponentUnavailable;
}
}
