#include "runtime/scene_probes.h"
#include "runtime/transform_math.h"
#include "scene/light_probe_group.h"
#include "scene/reflection_probe.h"

#include <algorithm>

namespace ae::runtime {
namespace {
bool activeInHierarchy(const SceneGraph &graph, const SceneObject *object) {
  for (const auto *ancestor = object; ancestor; ancestor = graph.find(ancestor->parent))
    if (!ancestor->active) return false;
  return true;
}
} // namespace

bool collectSceneReflectionProbes(const SceneGraph &graph, std::vector<renderer::SceneReflectionProbe> &out) {
  out.clear();
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    const auto *probe = static_cast<const scene::ReflectionProbe *>(object->components.find(scene::ReflectionProbe::descriptor));
    if (!probe || !probe->enabled || !probe->valid() || !activeInHierarchy(graph, object)) continue;
    float world[16];
    if (!worldMatrix(graph, id, world)) return false;
    renderer::SceneReflectionProbe entry{};
    entry.objectId = id;
    entry.instance = probe->instanceId();
    entry.type = static_cast<renderer::ReflectionProbeType>(probe->probeType);
    entry.refresh = static_cast<renderer::ReflectionProbeRefresh>(probe->refreshMode);
    entry.slicing = static_cast<renderer::ReflectionProbeSlicing>(probe->timeSlicing);
    for (u32 k = 0; k < 3; ++k) {
      entry.position[k] = world[12 + k];
      // A caixa da Unity segue a posição do objeto, não a rotação nem a escala.
      const float center = world[12 + k] + probe->boxOffset[k];
      entry.boxMin[k] = center - probe->boxSize[k] * 0.5f;
      entry.boxMax[k] = center + probe->boxSize[k] * 0.5f;
      entry.background[k] = scene::srgbChannelToLinear(probe->background[k]);
    }
    entry.blendDistance = probe->blendDistance;
    entry.importance = static_cast<i32>(probe->importance);
    entry.intensity = probe->intensity;
    entry.boxProjection = probe->boxProjection;
    entry.resolution = probe->resolution;
    entry.hdr = probe->hdr;
    entry.nearClip = probe->nearClip;
    entry.farClip = probe->farClip;
    entry.cullingMask = probe->cullingMask;
    entry.solidBackground = probe->clearFlags == 1;
    entry.baked = probe->bakedTexture;
    entry.captureKey = renderer::reflectionProbeCaptureKey(entry);
    out.push_back(entry);
  }
  return true;
}

bool collectSceneLightProbeGroups(const SceneGraph &graph, std::vector<SceneLightProbeGroup> &out) {
  out.clear();
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    const auto *group = static_cast<const scene::LightProbeGroup *>(object->components.find(scene::LightProbeGroup::descriptor));
    if (!group || !group->enabled || !group->valid() || !activeInHierarchy(graph, object)) continue;
    float world[16];
    if (!worldMatrix(graph, id, world)) return false;
    SceneLightProbeGroup entry;
    entry.objectId = id;
    entry.instance = group->instanceId();
    entry.removeRinging = group->removeRinging;
    entry.baked = group->bakedData;
    for (const auto &p : group->points) {
      std::array<float, 3> position{};
      for (u32 k = 0; k < 3; ++k)
        position[k] = world[k] * p.position[0] + world[4 + k] * p.position[1] + world[8 + k] * p.position[2] + world[12 + k];
      entry.pointIds.push_back(p.id);
      entry.positions.push_back(position);
    }
    out.push_back(std::move(entry));
  }
  return true;
}

bool buildSceneLightProbeSet(std::span<const SceneLightProbeGroup> groups, std::span<const LightProbeBakeLookup> bakes,
                             renderer::LightProbeSet &out) {
  out = {};
  for (const auto &group : groups) {
    if (!group.baked.valid()) continue;
    const auto found = std::find_if(bakes.begin(), bakes.end(), [&](const auto &b) { return b.guid == group.baked; });
    if (found == bakes.end() || !found->data || !found->data->valid()) continue;
    const auto &data = *found->data;
    for (usize i = 0; i < data.positions.size(); ++i) {
      if (out.positions.size() >= renderer::MaximumLightProbes) break;
      auto sh = data.sh[i];
      if (group.removeRinging) renderer::deringShL2(sh);
      out.positions.push_back(data.positions[i]);
      out.sh.push_back(sh);
    }
  }
  return out.tetrahedralize();
}

} // namespace ae::runtime
