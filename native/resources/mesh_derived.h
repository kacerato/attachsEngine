#pragma once
#include "resources/asset_registry.h"
#include "renderer/map_package.h"
#include <span>
#include <string>
#include <vector>

namespace ae::resources {

// Receita autoral de uma malha física derivada. `identity` não muda quando os
// parâmetros mudam: regenerar substitui o conteúdo do mesmo recurso e preserva
// referências de cenas/presets. Perfis schema 6 recebem a identidade legada na
// leitura; receitas novas usam a identidade estável derivada apenas da fonte.
struct CollisionMeshRecipe {
  AssetGuid source;
  u8 trianglePercent = 25;
  float maximumError = 0.02f;
  AssetGuid identity{};
  friend bool operator==(const CollisionMeshRecipe &, const CollisionMeshRecipe &) = default;
};

bool validCollisionMeshRecipe(const CollisionMeshRecipe &recipe) noexcept;
AssetGuid collisionMeshGuid(const CollisionMeshRecipe &recipe);

struct CollisionMeshBuild {
  std::vector<u32> indices;
  float resultingError = 0;
};

// Simplifica uma primitiva importada conservando seu buffer de vértices e a
// indexação local. O resultado é outro recurso; a malha visual não é alterada.
bool buildCollisionMesh(std::span<const u8> vertices, std::span<const u32> indices,
                        const renderer::MapDrawRecord &source,
                        const CollisionMeshRecipe &recipe, CollisionMeshBuild &out,
                        std::string &diagnostic);

} // namespace ae::resources
