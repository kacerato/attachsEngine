#pragma once
#include "resources/terrain_asset.h"
#include "resources/terrain_surface_catalog.h"
#include "resources/terrain_surface_import.h"
#include <functional>
namespace ae::editor {
using TerrainLibraryReader =
    std::function<bool(std::string_view, std::vector<u8> &)>;
struct TerrainMaterialCache {
  std::string key;
  std::vector<resources::TerrainSurfaceImage> images;
  std::vector<renderer::SharedAuthoringTexture> textures;
  renderer::TerrainSurfaceBinding binding;
};
std::shared_ptr<const TerrainMaterialCache> prepareTerrainMaterial(
    const resources::TerrainRecipe &,
    const std::shared_ptr<const resources::TerrainSurfaceCatalog> &,
    const TerrainLibraryReader &,
    const std::shared_ptr<const TerrainMaterialCache> &, std::string &);
} // namespace ae::editor
