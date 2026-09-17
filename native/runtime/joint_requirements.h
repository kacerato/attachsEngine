#pragma once
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"
#include "scene/joint.h"

namespace ae::runtime {
// Authored prerequisites, shared by inspection and execution. This does not
// promise that shape construction or the physics backend will succeed.
struct JointRequirementIssue {
  std::string_view code;
  ObjectId object=0;
  u64 instance=0;
  const char *message;
};
inline std::vector<JointRequirementIssue> jointRequirementIssues(
    const SceneGraph &graph,ObjectId source,const scene::Joint &joint) {
  std::vector<JointRequirementIssue> issues;
  const auto *owner=graph.find(source);
  if(!owner || !joint.enabled || !graph.activeInHierarchy(source)) return issues;
  const auto add=[&](std::string_view code,ObjectId id,u64 instance,const char *message) {
    issues.push_back({code,id,instance,message});
  };
  if(!joint.valid()) add("joint.fields",source,joint.instanceId(),"Campos da junta inválidos");
  const auto *bodyA=physicsBody(*owner);
  if(!bodyA) add("joint.source_body",source,0,"Adicione Corpo físico ao objeto da junta");
  const auto readiness=referenceReadiness(graph,source,joint,scene::jointReferences[0]);
  if(readiness!=ReferenceReadiness::Ready) {
    const char *message=readiness==ReferenceReadiness::RequiredEmpty?"Escolha o corpo conectado":
      readiness==ReferenceReadiness::Inactive?"Ative o corpo conectado e seus ancestrais":
      "Escolha outro objeto com Corpo físico";
    const auto target=joint.connectedBody<=std::numeric_limits<ObjectId>::max()?static_cast<ObjectId>(joint.connectedBody):0;
    const auto *destination=graph.find(target);
    add("joint.target",destination?target:source,0,message);
    return issues;
  }
  const auto target=static_cast<ObjectId>(joint.connectedBody);
  const auto *bodyB=physicsBody(*graph.find(target));
  if(bodyA && bodyB && bodyA->motion!=scene::BodyMotion::Dynamic && bodyB->motion!=scene::BodyMotion::Dynamic)
    add("joint.dynamic_body",source,bodyA->instanceId(),"Pelo menos um dos dois corpos deve ser dinâmico");
  for(const auto id:{source,target}) {
    float matrix[16];
    if(!worldMatrix(graph,id,matrix)) add("joint.transform",id,0,"Transformação global do corpo inválida");
  }
  return issues;
}
} // namespace ae::runtime
