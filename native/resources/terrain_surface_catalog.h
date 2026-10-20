#pragma once
#include "resources/terrain_surface_asset.h"

namespace ae::resources {
struct TerrainCatalogTexture {
  std::string id, sourcePath, runtimePath, thumbnailPath, originalAssetPath;
  std::string sourceHash, runtimeHash, archiveHash;
  AssetGuid identity;
  u32 width = 0, height = 0;
};
struct TerrainCatalogMaterial {
  std::string id, package, sourcePath;
  TerrainSurfaceLayer layer;
  // Exact JSON substrings from the input, including whitespace and number text.
  std::string sourcePropertiesJson, packingJson, resolvedMapsJson;
  std::vector<std::string> clusterIds, requiredTextureIds, diagnostics;
};
struct TerrainSurfaceCatalog {
  std::vector<TerrainCatalogTexture> textures;
  std::vector<TerrainCatalogMaterial> materials;
};
struct TerrainSurfaceSelection {
  TerrainSurfaceAsset surface;
  // Full metadata for explicit project import; no image data is copied.
  std::vector<TerrainCatalogTexture> requiredTextures;
  std::vector<std::string> diagnostics;
};
// The material GUID seed is exactly "astra.terrain.catalog.material:" + id.
// Texture identity always uses the catalog import GUID, not a pixel hash.
bool loadTerrainSurfaceCatalog(std::string_view json, TerrainSurfaceCatalog &,
                               std::string &error);
bool makeTerrainSurface(const TerrainSurfaceCatalog &,
                        std::span<const std::string> selectedIds,
                        TerrainSurfaceSelection &, std::string &error);
} // namespace ae::resources
