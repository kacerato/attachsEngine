#pragma once
#include "renderer/terrain_surface_binding.h"
#include "renderer/authoring_texture.h"
#include <span>
namespace ae::resources {
struct TerrainSurfaceImage {
  AssetGuid identity;
  std::shared_ptr<const std::vector<u8>> encoded;
  TerrainSurfaceImage()=default;
  TerrainSurfaceImage(AssetGuid id,std::vector<u8> bytes):identity(id),encoded(std::make_shared<const std::vector<u8>>(std::move(bytes))){}
  std::span<const u8> bytes() const {return encoded?std::span<const u8>(*encoded):std::span<const u8>{};}
};
bool importTerrainSurface(const TerrainSurfaceAsset &,std::span<const TerrainSurfaceImage>,float localMetersPerUv,
                          u64 maximumTextureBytes,std::vector<renderer::SharedAuthoringTexture> &,
                          renderer::TerrainSurfaceBinding &,std::string &,
                          std::array<float,4> terrainDomain={-32,-32,64,64});
}
