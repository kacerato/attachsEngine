#pragma once
#include "resources/terrain_fx_field.h"
#include "renderer/terrain_surface_binding.h"
#include "renderer/authoring_texture.h"
namespace ae::resources {
// Immutable derived snapshot. Recipe::fx remains the sole authoring owner.
bool makeTerrainStaticFxTexture(const TerrainFxField &,u32 textureIndex,
  renderer::SharedAuthoringTexture &,renderer::TerrainStaticFxBinding &,std::string &,
  const TerrainFxLimits & = {});
bool validateTerrainStaticFxTexture(const renderer::TerrainStaticFxBinding &,
  const renderer::AuthoringTexture &,std::array<float,4> terrainDomain,std::string &);
}
