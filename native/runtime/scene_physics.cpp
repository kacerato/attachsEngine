#include "runtime/scene_physics.h"
#include "runtime/transform_math.h"
#include <cmath>
#include <cstring>
#include <unordered_map>
#include "scene/joint.h"
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
  if(world_) AetherPhysics_DestroyWorld(world_);
  world_=nullptr;bindings_.clear();objects_.clear();events_.clear();accumulated_=0;jointCount_=0;
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
bool collisionMesh(const CollisionGeometrySource &geometry,const scene::Collider &collider,const scene::MeshRenderer *render,const float scale[3],
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
  const bool mirrored=scale[0]*scale[1]*scale[2]<0;
  struct Key {u32 bits[3];bool operator==(const Key &o) const {return bits[0]==o.bits[0]&&bits[1]==o.bits[1]&&bits[2]==o.bits[2];}};
  struct Hash {usize operator()(const Key &k) const {return (static_cast<usize>(k.bits[0])*73856093u)^(static_cast<usize>(k.bits[1])*19349663u)^(static_cast<usize>(k.bits[2])*83492791u);}};
  std::unordered_map<Key,u32,Hash> welded;if(collider.weldVertices) welded.reserve(triangles.size()/3);
  vertices.clear();indices.clear();indices.reserve(triangles.size()/3);
  for(usize t=0;t<triangles.size();t+=9) {
    u32 corner[3];
    for(u32 v=0;v<3;++v) {
      const float p[3]{triangles[t+v*3]*scale[0],triangles[t+v*3+1]*scale[1],triangles[t+v*3+2]*scale[2]};
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
  // Resolve ownership before creating anything; no nearest-parent inference.
  for(auto id:ids) {
    const auto &entity=*document.find(id);if(!document.activeInHierarchy(id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Collider::descriptor) continue;
      const auto &c=static_cast<const scene::Collider &>(*v);if(!c.enabled) continue;
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
    if(found==colliders.end()||found->second.empty()) return fail(entity,"corpo sem colisores ativos vinculados");
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
        case scene::ColliderShape::Mesh: {
          if(const auto *error=colliderMeshForPhysics(document,source.object,c,id)) return fail(object,error);
          if(!geometry) return fail(object,"geometria de colisão indisponível neste mundo");
          if(!collisionMesh(*geometry,c,meshRenderer(object),partTransform.scale,meshVertices[index],meshIndices[index]))
            return fail(object,"malha sem triângulos válidos para colisão");
          part.geometry=c.convex?AetherPartGeometry::ConvexHull:AetherPartGeometry::TriangleMesh;
          part.cooking.flags=c.optimizeCooking?static_cast<u32>(AetherMeshCookingOptimizeRuntime):0u;
          part.cooking.hullTolerance=c.hullTolerance*std::max({x,y,z});
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
    desc.motionType=static_cast<AetherMotionType>(body->motion);desc.friction=body->friction;desc.restitution=body->restitution;desc.isSensor=body->sensor;
    AetherBodyDynamicsV1 dynamics{sizeof(AetherBodyDynamicsV1),1,body->linearDamping,body->angularDamping,body->gravityFactor,
      {body->angularX,body->angularY,body->angularZ},body->allowSleep?1u:0u};
    const auto handle=AetherPhysics_CreateCompoundBodyV3(world_,&desc,parts.data(),static_cast<u32>(parts.size()),&dynamics);
    if(handle==AetherBodyHandle_Invalid||(body->motion==scene::BodyMotion::Dynamic&&!AetherPhysics_SetMassV2(world_,handle,body->mass)))
      return fail(entity,"Jolt recusou a composição ou a massa (malha convexa plana não tem casco sólido)");
    if(body->motion!=scene::BodyMotion::Static) AetherPhysics_SetLinearVelocity(world_,handle,{body->velocityX,body->velocityY,body->velocityZ});
    objects_.emplace(handle,id);
    gameWorld.setAuthority(id,TransformAuthority::PhysicsBody);
    std::vector<u64> instances;instances.reserve(found->second.size());
    for(const auto &source:found->second) instances.push_back(source.value->instanceId());
    // A camada de gameplay do objeto vale para o corpo inteiro. Dois colisores
    // do mesmo corpo não podem estar em camadas diferentes: o Jolt filtra por
    // corpo, e prometer o contrário seria uma propriedade sem efeito.
    if(!AetherPhysics_SetBodyGameplayLayerV1(world_,handle,entity.layer%GameplayLayers::kCount))
      return fail(entity,"camada de gameplay inválida");
    bindings_.push_back({id,handle,{transform.scale[0],transform.scale[1],transform.scale[2]},
                         body->motion!=scene::BodyMotion::Static,std::move(instances)});
  }
  // All bodies exist now, including static anchors and forward references.
  for(auto id:ids) {
    const auto &entity=*document.find(id);if(!document.activeInHierarchy(id)) continue;
    for(usize i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);if(&v->type()!=&scene::Joint::descriptor) continue;
      const auto &joint=static_cast<const scene::Joint &>(*v);if(!joint.enabled) continue;
      const auto issues=jointRequirementIssues(document,id,joint);
      if(!issues.empty()) return fail(entity,issues.front().message);
      const Binding *a=nullptr,*b=nullptr;for(const auto &binding:bindings_) {if(binding.id==id) a=&binding;if(binding.id==joint.connectedBody) b=&binding;}
      if(!a||!b) return fail(entity,"corpo conectado à junta está inativo");
      float ma[16],mb[16];if(!worldMatrix(document,id,ma)||!worldMatrix(document,b->id,mb)) return fail(entity,"referencial da junta inválido");
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
    float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;Transform transform;
    if(!worldMatrix(document,id,binding.world)||!localTransformForWorld(binding.world,identity,transform)) {stop();return false;}
    for(float scale:transform.scale) if(std::abs(scale-1)>.0001f) {stop();return false;}
    physics::CharacterMotorSettings settings;
    settings.radius=character->radius;settings.standingHalfHeight=character->halfHeight;
    settings.eyeHeight=character->eyeHeight;settings.movementUnitsPerSecond=character->speed;
    settings.maximumSlopeRadians=character->slopeDegrees*.01745329252f;
    settings.gravityUnitsPerSecondSquared=9.81f;
    binding.motor=std::make_unique<physics::CharacterMotor>();
    if(!binding.motor->initializeInWorld(world_,{binding.world[12],binding.world[13]+binding.eyeHeight,binding.world[14]},settings)) {stop();return false;}
    gameWorld.setAuthority(id,TransformAuthority::Character);
    characters_.push_back(std::move(binding));
  }
  error_.clear();return true;
}
void ScenePhysics::releaseObject(ObjectId id) {
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
}
QueryHit ScenePhysics::describeHit(AetherBodyHandle body,u32 subShapeId) const {
  QueryHit hit{};
  hit.object=objectForBody(body);
  u64 part=0;
  if(AetherPhysics_GetSubShapeUserDataV1(const_cast<AetherPhysicsWorld *>(world_),body,subShapeId,&part))
    for(const auto &binding:bindings_)
      if(binding.body==body&&part<binding.colliderInstances.size()) {
        hit.colliderInstance=binding.colliderInstances[static_cast<usize>(part)];break;
      }
  return hit;
}
bool ScenePhysics::rayCast(const float origin[3],const float direction[3],const QueryFilter &filter,QueryHit &out) const {
  if(!world_||!finite3(origin)||!finite3(direction)) return false;
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
  out.hasNormal=true;
  out.isSensor=hit.isSensor!=0;
  return true;
}
u32 ScenePhysics::rayCastAll(const float origin[3],const float direction[3],const QueryFilter &filter,
                             QueryHit *out,u32 capacity) const {
  if(!world_||!finite3(origin)||!finite3(direction)) return 0;
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
    out[i].hasNormal=true;
    out[i].isSensor=hits[i].isSensor!=0;
  }
  return static_cast<u32>(total);
}
bool ScenePhysics::shapeCast(const QueryShapeDesc &shape,const float origin[3],const float direction[3],
                             const QueryFilter &filter,QueryHit &out) const {
  if(!world_||!finite3(origin)||!finite3(direction)) return false;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  const auto nativeDesc=nativeShape(shape);
  AetherShapeQueryHit hit{};u32 subShape=0,sensor=0;
  if(!AetherPhysics_ShapeCastClosestV2(const_cast<AetherPhysicsWorld *>(world_),&nativeDesc,
      {origin[0],origin[1],origin[2]},{shape.rotation[0],shape.rotation[1],shape.rotation[2],shape.rotation[3]},
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
  if(!world_||!finite3(origin)) return 0;
  AetherBodyHandle ignore=AetherBodyHandle_Invalid;
  for(const auto &binding:bindings_) if(binding.id==filter.ignore) ignore=binding.body;
  const auto native=nativeFilter(filter,ignore);
  const auto nativeDesc=nativeShape(shape);
  std::vector<AetherShapeQueryHit> hits(capacity);
  std::vector<u32> subShapes(capacity),sensors(capacity);
  const auto total=AetherPhysics_OverlapShapeV2(const_cast<AetherPhysicsWorld *>(world_),&nativeDesc,
      {origin[0],origin[1],origin[2]},{shape.rotation[0],shape.rotation[1],shape.rotation[2],shape.rotation[3]},
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
bool ScenePhysics::jumpCharacter(ObjectId id) {
  for(auto &c:characters_) if(c.id==id) return c.motor->jump(c.jumpSpeed);
  return false;
}
bool ScenePhysics::applyBodyForce(ObjectId id,const float *v,u32 kind) {
  if(!v||kind>3) return false;
  for(const auto &b:bindings_) if(b.id==id) return AetherPhysics_ApplyBodyForceV1(world_,b.body,{v[0],v[1],v[2]},static_cast<AetherBodyForceKind>(kind))!=0;
  return false;
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
bool ScenePhysics::advance(double elapsed,GameWorld &world,bool (*beforeStep)(void *,float),void *context,bool (*trigger)(void *,ObjectId,ObjectId,u32),bool (*contact)(void *,const ContactEvent &)) {
  if(!world_ || !std::isfinite(elapsed) || elapsed<0) return false;
  constexpr double fixed=1.0/60.0;
  // Bound catch-up after surface/lifecycle stalls; never feed a large dt to Jolt.
  accumulated_+=std::min(elapsed,.25);
  while(accumulated_+1e-9>=fixed) {
    if(beforeStep&&!beforeStep(context,static_cast<float>(fixed))) return false;
    for(auto &c:characters_) {
      const bool scripted=c.scriptMoveActive;
      const float right=scripted?c.scriptRight:c.right;
      const float forward=scripted?c.scriptForward:c.forward;
      const float yaw=scripted?c.scriptYaw:c.yaw;
      if(!c.motor->update(right,forward,yaw,static_cast<float>(fixed))) return false;
    }
    if(AetherPhysics_StepV2(world_,static_cast<float>(fixed),1)!=0) return false;
    accumulated_-=fixed;
    if(!synchronizePoses(world)) return false;
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
  return true;
}
bool ScenePhysics::synchronizePoses(GameWorld &world) {
  auto &document=world.poseGraph();
  for(const auto &binding:bindings_) {
    if(!binding.moving) continue;
    AetherVec3 p;AetherQuat q;
    if(!AetherPhysics_TryGetBodyPoseV2(world_,binding.body,&p,&q)) return false;
    float world[16]{
      (1-2*(q.y*q.y+q.z*q.z))*binding.scale[0],2*(q.x*q.y+q.w*q.z)*binding.scale[0],2*(q.x*q.z-q.w*q.y)*binding.scale[0],0,
      2*(q.x*q.y-q.w*q.z)*binding.scale[1],(1-2*(q.x*q.x+q.z*q.z))*binding.scale[1],2*(q.y*q.z+q.w*q.x)*binding.scale[1],0,
      2*(q.x*q.z+q.w*q.y)*binding.scale[2],2*(q.y*q.z-q.w*q.x)*binding.scale[2],(1-2*(q.x*q.x+q.y*q.y))*binding.scale[2],0,p.x,p.y,p.z,1};
    // Um objeto removido no ponto seguro já teve o corpo solto; se a ordem
    // inverter, ignorar é correto — publicar pose de quem não existe não é.
    const auto *entity=document.find(binding.id);if(!entity) continue;
    float parent[16]{};parent[0]=parent[5]=parent[10]=parent[15]=1;
    if(entity->parent && !worldMatrix(document,entity->parent,parent)) return false;
    Transform local;
    if(!localTransformForWorld(world,parent,local)||!document.setTransform(binding.id,local)) return false;
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
}
