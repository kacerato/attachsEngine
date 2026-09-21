#pragma once

#include "renderer/scene_environment.h"
#include "resources/environment_profile.h"
#include "runtime/scene_graph.h"

namespace ae::runtime {
bool collectSceneEnvironmentVolumes(const SceneGraph &graph,
                                    std::vector<renderer::SceneEnvironmentVolume> &out,
                                    std::span<const resources::EnvironmentProfile> profiles={});
bool collectSceneEnvironment(const SceneGraph &graph, renderer::SceneEnvironment &out,
                             std::span<const resources::EnvironmentProfile> profiles={});
} // namespace ae::runtime
