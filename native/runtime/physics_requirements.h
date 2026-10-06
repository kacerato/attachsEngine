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
  // Primitives require TRS; meshes receive their full affine transform.
  const bool mesh=collider.shape==scene::ColliderShape::Mesh;
  // Mesh vertices receive the full affine matrix below, including shear from
  // local rotation under nonuniform object scale. No TRS approximation.
  if(mesh){pose=Transform{};return nullptr;}
  Transform local;
  if(!mesh) {
    local.position[0]=collider.centerX;local.position[1]=collider.centerY;local.position[2]=collider.centerZ;
    local.rotationDegrees[0]=collider.rotationX;local.rotationDegrees[1]=collider.rotationY;local.rotationDegrees[2]=collider.rotationZ;
  }
  float localMatrix[16],objectMatrix[16],world[16];transformMatrix(local,localMatrix);
  if(!worldMatrix(graph,id,objectMatrix)) return "Transformação global do colisor inválida";
  multiplyMatrix(objectMatrix,localMatrix,world);
  if(!localTransformForWorld(world,frame,pose)) return "Colisor rotacionado sob escala não uniforme produz shear";
  const float x=std::abs(pose.scale[0]),y=std::abs(pose.scale[1]),z=std::abs(pose.scale[2]);
  if((collider.shape==scene::ColliderShape::Sphere||collider.shape==scene::ColliderShape::Capsule) &&
     (std::abs(x-y)>1e-4f*x||std::abs(x-z)>1e-4f*x))
    return "Esfera e cápsula requerem escala global uniforme";
  if(collider.shape==scene::ColliderShape::Cylinder&&std::abs(x-z)>1e-4f*x)
    return "Cilindro requer escalas X e Z iguais";
  return nullptr;
}
inline bool colliderMeshMatrixForPhysics(const SceneGraph &graph,ObjectId id,const scene::Collider &c,
                                        const float frame[16],float out[16]) {
  Transform local;if(c.meshLocalPose){local.position[0]=c.centerX;local.position[1]=c.centerY;local.position[2]=c.centerZ;
    local.rotationDegrees[0]=c.rotationX;local.rotationDegrees[1]=c.rotationY;local.rotationDegrees[2]=c.rotationZ;}
  float localMatrix[16],object[16],world[16];transformMatrix(local,localMatrix);
  if(!worldMatrix(graph,id,object))return false;
  multiplyMatrix(object,localMatrix,world);
  // frame is rigid: inverse rotation is transpose; retain all affine columns.
  for(u32 column=0;column<4;++column)for(u32 row=0;row<3;++row) {
    out[column*4+row]=0;for(u32 k=0;k<3;++k)out[column*4+row]+=frame[row*4+k]*(world[column*4+k]-(column==3?frame[12+k]:0));
    if(!std::isfinite(out[column*4+row]))return false;
  }
  out[3]=out[7]=out[11]=0;out[15]=1;return true;
}
// O que a forma Malha exige além da pose. Um recurso de colisão explícito vence
// o Renderizador de malha do objeto; sem nenhum dos dois não há forma, e não se
// inventa uma caixa no lugar.
// Não convexa em corpo dinâmico é recusada pela mesma razão da Unity: triângulos
// soltos não têm volume, logo não têm massa nem inércia.
inline const char *colliderMeshForPhysics(const SceneGraph &graph,ObjectId id,const scene::Collider &collider,ObjectId owner) {
  if(collider.shape!=scene::ColliderShape::Mesh) return nullptr;
  const auto *entity=graph.find(id);const auto *render=entity?meshRenderer(*entity):nullptr;
  if(!collider.collisionMesh.valid()&&(!render||!render->mesh))
    return "Colisor de malha requer uma Malha de colisão ou um Renderizador de malha neste objeto";
  const auto *object=graph.find(owner);const auto *body=object?physicsBody(*object):nullptr;
  if(!collider.convex&&body&&body->motion==scene::BodyMotion::Dynamic)
    return "Malha não convexa só em corpo estático ou cinemático; ligue Convexo para corpo dinâmico";
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
