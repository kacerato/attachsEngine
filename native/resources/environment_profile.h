#pragma once

#include "renderer/scene_environment.h"
#include "resources/asset_registry.h"

#include <string>
#include <string_view>

namespace ae::resources {

// Aparência compartilhada por volumes. Forma, prioridade, camada e peso ficam
// na instância; o recurso guarda somente o conteúdo visual reutilizável.
struct EnvironmentProfile final {
  static constexpr u32 FormatVersion = 2;
  AssetGuid guid{};
  u32 revision = 1;
  std::string name;
  renderer::SceneEnvironment values{};

  bool valid() const;
  std::string serialize() const;
  static bool deserialize(std::string_view text, EnvironmentProfile &out);
};

// Aplica só a aparência. Estado ativo e prioridade continuam pertencendo ao
// componente Volume, como no contrato de Volume/Profile consultado.
renderer::SceneEnvironment applyEnvironmentProfile(const renderer::SceneEnvironment &instance,
                                                    const EnvironmentProfile &profile);
bool sameEnvironmentAppearance(const renderer::SceneEnvironment &instance,
                               const EnvironmentProfile &profile);

} // namespace ae::resources
