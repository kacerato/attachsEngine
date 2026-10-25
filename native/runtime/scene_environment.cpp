#include "runtime/scene_environment.h"
#include "renderer/normal_matrix.h"
#include "runtime/transform_math.h"
#include "scene/environment.h"

#include <algorithm>

namespace ae::runtime {
bool collectSceneEnvironmentVolumes(const SceneGraph &graph,
                                    std::vector<renderer::SceneEnvironmentVolume> &out,
                                    std::span<const resources::EnvironmentProfile> profiles) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::vector<renderer::SceneEnvironmentVolume> candidate;
  for (ObjectId id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    const auto *component = static_cast<const scene::Environment *>(
        object->components.find(scene::Environment::descriptor));
    if (!component || !component->values.active || !component->valid()) continue;
    if(!graph.activeInHierarchy(id)) continue;
    renderer::SceneEnvironmentVolume volume;
    volume.environment=component->values;
    volume.overrides=component->volumeOverrides();
    if(component->profile.valid()) {
      const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto &profile) {
        return !profile.skybox&&profile.guid==component->profile;
      });
      // PK-4 (V-01): o perfil é o Volume Profile compartilhado, lido do
      // recurso a cada coleta: editar o perfil muda todo volume que o usa.
      // Recurso ausente conserva a cópia gravada na instância; o GUID fica, e
      // quando o recurso reaparecer volta a resolver sem remapeamento. Uma
      // cena anterior à v19 cuja cópia divergia do perfil (o "Atualizar
      // perfil" pendente) continua com a cópia até o editor clonar o perfil.
      const bool divergedLegacy=found!=profiles.end()&&component->legacyProfileCopy&&
          !resources::sameEnvironmentProfileData(component->values,volume.overrides,*found);
      if(found!=profiles.end()&&!divergedLegacy) {
        volume.environment=resources::applyEnvironmentProfile(component->values,*found);
        volume.overrides=found->overrides;
      }
    }
    // PK-2: Material do céu compartilhado, lido do recurso a cada coleta (editar
    // o material muda todo Ambiente que o usa). Ausente: o céu da instância.
    if(volume.environment.skybox.valid()) {
      const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto &profile) {
        return profile.skybox&&profile.guid==volume.environment.skybox;
      });
      if(found!=profiles.end()) volume.environment=resources::applySkyboxMaterial(volume.environment,*found);
    }
    volume.shape=component->shape;
    // PK-4 (V-04): a camada é a do objeto, como o GameObject.layer do Volume.
    volume.layer=object->layer;volume.stableId=id;volume.weight=component->weight;
    volume.blendDistance=component->blendDistance;volume.sphereRadius=component->sphereRadius;
    std::copy(component->boxSize,component->boxSize+3,volume.boxSize);
    if(!worldMatrix(graph,id,volume.localToWorld)) return false;
    float normal[12];
    if(!renderer::buildNormalMatrix(volume.localToWorld,normal)) return false;
    std::fill(std::begin(volume.worldToLocal),std::end(volume.worldToLocal),0.0f);
    volume.worldToLocal[15]=1;
    for(u32 row=0;row<3;++row) for(u32 column=0;column<3;++column)
      volume.worldToLocal[column*4+row]=normal[row*4+column];
    for(u32 row=0;row<3;++row) for(u32 column=0;column<3;++column)
      volume.worldToLocal[12+row]-=volume.worldToLocal[column*4+row]*volume.localToWorld[12+column];
    if(!volume.valid()) return false;
    candidate.push_back(volume);
  }
  out=std::move(candidate);
  return true;
}

bool collectSceneEnvironment(const SceneGraph &graph, renderer::SceneEnvironment &out,
                             std::span<const resources::EnvironmentProfile> profiles) {
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  if(!collectSceneEnvironmentVolumes(graph,volumes,profiles)) return false;
  const float origin[3]{};
  out=renderer::resolveSceneEnvironment(volumes,origin);
  return true;
}
} // namespace ae::runtime
