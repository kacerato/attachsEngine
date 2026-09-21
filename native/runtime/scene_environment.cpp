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
    if(component->profile.valid()) {
      const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto &profile) {
        return profile.guid==component->profile;
      });
      // Referência ausente conserva a cópia autoral da instância. O GUID não é
      // apagado: quando o recurso reaparecer, volta a resolver sem remapeamento.
      if(found!=profiles.end()&&resources::sameEnvironmentAppearance(component->values,*found))
        volume.environment=resources::applyEnvironmentProfile(component->values,*found);
    }
    volume.shape=component->shape;
    volume.overrides=(component->overrideSky?renderer::EnvironmentOverrideSky:0u)|
                     (component->overrideFog?renderer::EnvironmentOverrideFog:0u)|
                     (component->overridePost?renderer::EnvironmentOverridePost:0u)|
                     (component->overrideIndirect?renderer::EnvironmentOverrideIndirect:0u);
    volume.layer=component->layer;volume.stableId=id;volume.weight=component->weight;
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
