#pragma once

#include "renderer/scene_environment.h"
#include "resources/asset_registry.h"

#include <string>
#include <string_view>

namespace ae::resources {

// Aparência compartilhada por volumes. Forma, prioridade, camada e peso ficam
// na instância; o recurso guarda somente o conteúdo visual reutilizável.
//
// PK-2 (A-06): o mesmo formato guarda o Material do céu (`skybox`), o Skybox
// Material da Unity escolhido na janela Iluminação: só os campos do céu valem
// (shader Procedural/Panorâmico/Físico e os parâmetros dele). Arquivo .skybox,
// tipo de recurso SkyboxMaterial, cabeçalho ASTRA_SKYBOX_MATERIAL.
struct EnvironmentProfile final {
  static constexpr u32 FormatVersion = 14;
  AssetGuid guid{};
  u32 revision = 1;
  std::string name;
  bool skybox = false;
  renderer::SceneEnvironment values{};
  AssetType assetType() const { return skybox ? AssetType::SkyboxMaterial : AssetType::EnvironmentProfile; }

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

// O céu do Material do céu sobre a instância: shader, mapa HDRI e parâmetros
// do céu, sol e lua. Ambiente, neblina, pós e volume continuam da instância.
renderer::SceneEnvironment applySkyboxMaterial(const renderer::SceneEnvironment &instance,
                                               const EnvironmentProfile &material);
// Só o céu de `values`, para criar um Material do céu a partir de um Ambiente.
renderer::SceneEnvironment skyboxMaterialValues(const renderer::SceneEnvironment &values);

} // namespace ae::resources
