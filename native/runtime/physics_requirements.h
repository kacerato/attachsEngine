#pragma once
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"

namespace ae::runtime {
inline const char *bodyFrameForPhysics(const SceneGraph &graph,ObjectId id,
    float world[16],Transform &pose,float frame[16]) {
  float identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;
  if(!worldMatrix(graph,id,world)||!localTransformForWorld(world,identity,pose))
    return "Transformação física inválida ou com shear";
  auto rigid=pose;rigid.scale[0]=rigid.scale[1]=rigid.scale[2]=1;
  transformMatrix(rigid,frame);return nullptr;
}
inline const char *colliderPoseForPhysics(const SceneGraph &graph,ObjectId id,
    const scene::Collider &collider,const float frame[16],Transform &pose) {
  Transform local;local.position[0]=collider.centerX;local.position[1]=collider.centerY;local.position[2]=collider.centerZ;
  local.rotationDegrees[0]=collider.rotationX;local.rotationDegrees[1]=collider.rotationY;local.rotationDegrees[2]=collider.rotationZ;
  float localMatrix[16],objectMatrix[16],world[16];transformMatrix(local,localMatrix);
  if(!worldMatrix(graph,id,objectMatrix)) return "Transformação global do colisor inválida";
  multiplyMatrix(objectMatrix,localMatrix,world);
  if(!localTransformForWorld(world,frame,pose)) return "Colisor rotacionado sob escala não uniforme produz shear";
  const float x=std::abs(pose.scale[0]),y=std::abs(pose.scale[1]),z=std::abs(pose.scale[2]);
  if(collider.shape!=scene::ColliderShape::Box && (std::abs(x-y)>1e-4f*x||std::abs(x-z)>1e-4f*x))
    return "Esfera e cápsula requerem escala global uniforme";
  return nullptr;
}
inline const char *colliderOwnerForPhysics(const SceneGraph &graph,ObjectId id,const scene::Collider &value,ObjectId &owner) {
  if(!value.valid()) return "Campos do colisor inválidos";
  if(!referenceAccepts(graph,id,scene::colliderReferences[0],value.owner,true))
    return "Colisor requer corpo próprio ou ancestral explícito, sem atravessar outro corpo";
  owner=value.owner?static_cast<ObjectId>(value.owner):id;
  if(!graph.activeInHierarchy(owner)) return "Corpo proprietário inativo";
  return nullptr;
}
inline const char *bodyHierarchyForPhysics(const SceneGraph &graph,ObjectId id) {
  const auto *entity=graph.find(id);const auto *body=entity?physicsBody(*entity):nullptr;
  if(!body||!body->valid()||characterComponent(*entity)) return "Corpo inválido ou incompatível com Personagem";
  for(auto p=entity->parent;p;p=graph.find(p)->parent) {
    const auto &ancestor=*graph.find(p);const auto *parentBody=physicsBody(ancestor);
    if(characterComponent(ancestor)||(parentBody&&parentBody->motion!=scene::BodyMotion::Static))
      return "Corpo independente não pode herdar pose de corpo móvel ou Personagem";
  }
  return nullptr;
}
} // namespace ae::runtime
