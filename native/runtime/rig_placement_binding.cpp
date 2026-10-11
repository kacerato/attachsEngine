#include "runtime/rig_placement_binding.h"
#include "runtime/rig_placement.h"

namespace ae::runtime {
namespace {
bool fail(std::string &error,const char *message) {error=message;return false;}
}
bool prepareRigInstancePlacement(const resources::RigAsset &asset,
                                 const resources::CompiledRig &rig,
                                 const SceneGraph &scene,ObjectId root,
                                 scene::RigInstance &out,std::string &error) {
  error.clear();
  const auto *object=scene.find(root);
  if(!object || root==scene.root() || !object->parent ||
     asset.guid!=rig.guid || asset.revision!=rig.revision ||
     rig.joints.empty() || rig.joints.front().parent>=0)
    return fail(error,"Raiz ou revisão do rig inválida para estabelecer a instância.");
  RigBindings bindings;
  if(!bindRigToScene(asset,rig,scene,root,bindings,RigBindingMode::PreparePlacement)) {
    error=bindings.issues.empty()?"Não foi possível vincular a hierarquia real do rig.":bindings.issues.front().message;
    return false;
  }
  resources::RigTransform rootTransform,placement;
  if(!rigTransformFromScene(object->transform,rootTransform) ||
     !makeRigPlacement(rootTransform,rig.joints.front().reference,placement))
    return fail(error,"Placement exige escala uniforme positiva e raiz representável.");
  scene::RigInstance candidate=out;candidate.rig=rig.guid;
  candidate.rigRevision=rig.revision;candidate.rootJoint=rig.joints.front().id;candidate.placementParent=object->parent;
  candidate.placement=placement;candidate.placed=true;
  if(!candidate.ready())return fail(error,"Placement da instância não é representável.");
  out=std::move(candidate);return true;
}
bool validateRigInstancePlacement(const resources::CompiledRig &rig,
                                  const SceneGraph &scene,ObjectId root,
                                  const scene::RigInstance &instance,std::string &error) {
  error.clear();
  if(!instance.ready() || instance.rig!=rig.guid || instance.rigRevision!=rig.revision ||
     rig.joints.empty() || instance.rootJoint!=rig.joints.front().id)
    return fail(error,"Instância exige vínculo explícito com a revisão atual do rig.");
  const auto *object=scene.find(root);
  if(!object || root==scene.root() || object->parent!=instance.placementParent)
    return fail(error,"Raiz removida ou reparentada: restabeleça o placement da instância.");
  return true;
}
bool prepareRigInstanceRevalidation(const resources::RigAsset &asset,
                                    const resources::CompiledRig &rig,
                                    const SceneGraph &scene,ObjectId root,
                                    const scene::RigInstance &previous,
                                    scene::RigInstance &out,RigBindings &bindingsOut,
                                    std::string &error) {
  error.clear();const auto *object=scene.find(root);
  if(!previous.ready() || previous.rig!=rig.guid || asset.guid!=rig.guid ||
     asset.revision!=rig.revision || rig.revision<previous.rigRevision || rig.joints.empty() ||
     previous.rootJoint!=rig.joints.front().id || !object ||
     root==scene.root() || previous.placementParent!=object->parent)
    return fail(error,"Raiz, pai ou identidade do rig mudou; revalidação não pode inferir outro placement.");
  RigBindings bindings;
  bindRigToScene(asset,rig,scene,root,bindings,RigBindingMode::PreparePlacement);
  if(bindings.objects.empty() || bindings.objects.front()!=root)
    return fail(error,"A raiz persistida não foi resolvida para a nova revisão.");
  auto candidate=previous;candidate.rigRevision=rig.revision;
  if(!candidate.ready())return fail(error,"Revisão da instância não é representável.");
  bindings.placement=candidate.placement;bindings.persistent=true;
  out=std::move(candidate);bindingsOut=std::move(bindings);return true;
}
} // namespace ae::runtime
