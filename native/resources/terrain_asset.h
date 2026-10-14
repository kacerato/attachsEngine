#pragma once
#include "core/base.h"
#include <array>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ae::resources {
enum class TerrainMode : u32 { Heightfield, SmoothVoxel, BlockVoxel };
enum class TerrainTool : u32 {
  Raise,
  Lower,
  Flatten,
  Smooth,
  Dig,
  Add,
  Paint,
  Path
};
struct TerrainStroke {
  TerrainTool tool = TerrainTool::Raise;
  float x = 0, y = 0, z = 0, radius = 4, strength = 1;
  u32 layer = 0;
};
struct TerrainRecipe {
  TerrainMode mode = TerrainMode::Heightfield;
  u32 seed = 1337, cells = 32, verticalCells = 24, octaves = 4;
  float size = 64, height = 16, base = 4, frequency = 3, textureScale = 4;
  bool island = false, collision = true;
  std::vector<TerrainStroke> strokes;
};
struct TerrainVertex {
  float position[3]{}, normal[3]{}, uv[2]{};
};
struct TerrainPart {
  u32 x = 0, y = 0, z = 0, layer = 0;
  std::vector<TerrainVertex> vertices;
};
struct TerrainMesh {
  std::vector<TerrainPart> parts;
  u32 samples = 0, triangles = 0, chunks = 0;
};
bool validTerrainRecipe(const TerrainRecipe &, std::string &);
std::string serializeTerrainRecipe(const TerrainRecipe &);
bool deserializeTerrainRecipe(std::string_view, TerrainRecipe &, std::string &);
// Cooperative cancellation and strict output budget. Failure never changes out.
bool generateTerrain(const TerrainRecipe &, TerrainMesh &out, std::string &,
                     const std::function<bool(float)> &progress = {});
bool writeTerrainGlb(const TerrainRecipe &, const TerrainMesh &,
                     const std::array<std::vector<u8>, 4> &textures,
                     std::vector<u8> &, std::string &);
bool terrainRecipeFromGlb(std::span<const u8>, TerrainRecipe &, std::string &);
} // namespace ae::resources
