#pragma once
#include "core/base.h"
#include "renderer/terrain_control.h"
#include "renderer/authoring_texture.h"
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
enum class TerrainBrushShape : u32 { Circle, Box, Hemisphere, Cone, RoundedBox };
enum class TerrainBrushApplication : u32 { Legacy, Accumulate, Target };
struct TerrainStroke {
  TerrainTool tool = TerrainTool::Raise;
  float x = 0, y = 0, z = 0, radius = 4, strength = 1;
  u32 layer = 0;
  TerrainBrushShape shape = TerrainBrushShape::Circle;
  float hardness = 0, rotation = 0, aspect = 1;
  u32 gesture = 0;
  // Local ellipsoid axes. Legacy radius/aspect retain their exact meaning.
  float verticalAspect = 1, depthAspect = 1, pitch = 0, roll = 0;
  float roundness = .25f, opacity = 1;
  u32 filterRadius = 1;
  TerrainBrushApplication application = TerrainBrushApplication::Legacy;
  bool inverted = false, restoreGeometry = true, restoreWeights = true;
  float surfaceDepth = 0;
  bool lockHeight = false, dimensionsLinked = true;
};
struct TerrainCurvePoint {
  float x = 0, y = 0;
  bool operator==(const TerrainCurvePoint &) const = default;
};
struct TerrainPathPoint {
  float x = 0, y = 0, z = 0;
  bool curved = false;
  bool operator==(const TerrainPathPoint &) const = default;
};
enum class TerrainPathCap : u32 { Round, Flat, Square };
struct TerrainPath {
  u32 id = 1;
  std::string name = "Caminho";
  std::vector<TerrainPathPoint> points;
  float width = 4, shoulderWidth = 8, elevation = 0, slopeLimit = 45;
  float uniformRamp = 0, definedHeight = 0, textureOpacity = 1, shoulderOpacity = 1;
  float textureTransition = 1, vegetationTransition = 2;
  u32 surfaceLayer = 3, shoulderLayer = 2;
  bool enabled = true, shape = true, texture = true, keepWidthRatio = true;
  bool autoRamp = true, useDefinedHeight = false, reverseRamp = false, smartPaint = true;
  bool followTerrain = true;
  bool clearGrass = false, clearTrees = false, showVegetation = true;
  TerrainPathCap cap = TerrainPathCap::Round;
  // All profiles use normalized distance from lane edge to shoulder edge.
  std::vector<TerrainCurvePoint> profile{{0,1},{1,0}};
  std::vector<TerrainCurvePoint> textureProfile{{0,1},{1,0}};
  std::vector<TerrainCurvePoint> vegetationProfile{{0,1},{1,0}};
  bool operator==(const TerrainPath &) const = default;
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
  // 0 preserves AETERRAIN1..6 vertex painting. Explicit maps apply to heightfields.
  u32 controlCells = 0;
  // Level 0 remains the collision surface. Additional render levels lock
  // chunk borders and use the renderer's existing screen-space error budget.
  u32 lodLevels = 1;
  float lodRelativeError = .01f;
  std::vector<TerrainStroke> strokes;
  std::vector<TerrainPath> paths;
};
struct TerrainVertex {
  float position[3]{}, normal[3]{}, uv[2]{};
  float uv1[2]{}, weights[4]{1,0,0,0};
};
// Shared by mesh generation and the viewport footprint. Coordinates are local.
float terrainBrushDistance(const TerrainStroke &, float dx, float dy, float dz, bool volume);
float terrainBrushWeight(const TerrainStroke &, float normalizedDistance);
// The cursor and density operators share the same transformed shape boundary.
std::array<float, 3> terrainBrushOutline(const TerrainStroke &, u32 ring, float phase);
bool validTerrainStroke(const TerrainStroke &, std::string &);
struct TerrainPart {
  u32 x = 0, y = 0, z = 0, layer = 0;
  std::vector<TerrainVertex> vertices;
};
struct TerrainMesh {
  std::vector<TerrainPart> parts;
  u32 samples = 0, triangles = 0, chunks = 0;
  u32 rebuiltChunks = 0, reusedChunks = 0;
};
// Authoring samples are independent of triangulation. A mesher can rebuild a
// region without reconstructing the rest of the mesh or changing saved data.
struct TerrainField {
  u32 width = 0, depth = 0, height = 0;
  float step = 0, verticalStep = 0, bottom = 0, solidBottom = 0;
  std::vector<float> heights, density;
  std::vector<u8> legacyLayers, holes;
  std::vector<TerrainStroke> paintSamples;
  u32 controlWidth = 0;
  float controlStep = 0;
  std::vector<std::array<float,4>> controlWeights;
};
// Exactly the same curve tessellation used by path deformation and painting.
std::vector<std::array<float,3>> terrainPathPolyline(const TerrainPath &, float step);
float terrainFieldHeight(const TerrainField &, float x, float z);
std::array<float,4> terrainFieldWeights(const TerrainField &, float x, float z);
bool makeTerrainControlTexture(const TerrainField &, renderer::SharedAuthoringTexture &, std::string &);
renderer::TerrainControlBinding terrainControlBinding(const TerrainRecipe &, const TerrainField &, u32 textureIndex);
struct TerrainChunkKey {
  u32 x = 0, y = 0, z = 0;
  bool operator==(const TerrainChunkKey &) const = default;
};
bool generateTerrainField(const TerrainRecipe &, TerrainField &, std::string &,
                          const std::function<bool(float)> &progress = {});
bool meshTerrainField(const TerrainRecipe &, const TerrainField &, TerrainMesh &, std::string &,
                      const std::function<bool(float)> &progress = {},
                      const TerrainMesh *previous = nullptr,
                      std::span<const TerrainChunkKey> dirty = {});
struct TerrainBuild {
  TerrainRecipe recipe;
  TerrainField field;
  TerrainMesh mesh;
  std::vector<TerrainChunkKey> dirty;
};
bool updateTerrain(const TerrainRecipe &, const TerrainBuild *previous, TerrainBuild &,
                   std::string &, const std::function<bool(float)> &progress = {});
bool validTerrainRecipe(const TerrainRecipe &, std::string &);
std::string serializeTerrainRecipe(const TerrainRecipe &);
bool deserializeTerrainRecipe(std::string_view, TerrainRecipe &, std::string &);
// Cooperative cancellation and strict output budget. Failure never changes out.
bool generateTerrain(const TerrainRecipe &, TerrainMesh &out, std::string &,
                     const std::function<bool(float)> &progress = {});
bool writeTerrainGlb(const TerrainRecipe &, const TerrainMesh &,
                     const std::array<std::vector<u8>, 4> &textures,
                     std::vector<u8> &, std::string &);
bool writeTerrainGlb(const TerrainBuild &, const std::array<std::vector<u8>, 4> &textures,
                     std::vector<u8> &, std::string &);
bool terrainRecipeFromGlb(std::span<const u8>, TerrainRecipe &, std::string &);
} // namespace ae::resources
