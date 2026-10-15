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
  Path,
  Noise,
  Terrace,
  Restore,
  Sharpen,
  Hole
};
enum class TerrainBrushShape : u32 { Circle, Box, Hemisphere, Cone };
struct TerrainStroke {
  TerrainTool tool = TerrainTool::Raise;
  float x = 0, y = 0, z = 0, radius = 4, strength = 1;
  u32 layer = 0;
  TerrainBrushShape shape = TerrainBrushShape::Circle;
  float hardness = 0, rotation = 0, aspect = 1;
  u32 gesture = 0;
};
struct TerrainRecipe {
  TerrainMode mode = TerrainMode::Heightfield;
  u32 seed = 1337, cells = 32, verticalCells = 24, octaves = 4;
  float size = 64, height = 16, base = 4, frequency = 3, textureScale = 4;
  bool island = false, collision = true;
  // Heightfields may close their perimeter and underside. The horizontal floor
  // sits baseDepth below the lowest sculpted height; voxel modes ignore this.
  // Legacy AETERRAIN1 recipes remain open surfaces when loaded.
  bool solidBase = false;
  float baseDepth = 8;
  bool blendedSurface = true;
  float macroVariation = .3f;
  std::vector<TerrainStroke> strokes;
};
struct TerrainVertex {
  float position[3]{}, normal[3]{}, uv[2]{};
  float uv1[2]{}, weights[4]{1,0,0,0};
};
// Shared by mesh generation and the viewport footprint. Coordinates are local.
float terrainBrushDistance(const TerrainStroke &, float dx, float dy, float dz, bool volume);
float terrainBrushWeight(const TerrainStroke &, float normalizedDistance);
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
