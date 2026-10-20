// Geometry/recipe acceptance uses the production terrain implementation.
// These same cases remain included in aether_terrain_tests.
#include "harness.h"
#include "terrain_test_helpers.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_vertex_index.h"
#include "resources/terrain_stroke_sampler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
using namespace ae;
using ae::test::terrainDigest;

AE_TEST(terrain_stroke_sampling_preserves_polyline_exposure_and_saved_density) {
  using Point=std::array<float,3>;
  const std::array<Point,4> path{{{-4,3,-4},{-1,3,-4},{-1,4,-1},{2,4,-1}}};
  const auto sampled=[&](u32 subdivisions,std::vector<Point> &points) {
    resources::TerrainStrokeSampler sampler;sampler.reset(.8f);
    const auto emit=[&](const Point &p){points.push_back(p);return true;};
    AE_EXPECT_TRUE(sampler.sample(path.front(),true,false,emit),"first stamp");
    for(usize segment=1;segment<path.size();++segment)
      for(u32 step=1;step<=subdivisions;++step) {
        Point p;for(u32 axis=0;axis<3;++axis)
          p[axis]=std::lerp(path[segment-1][axis],path[segment][axis],float(step)/subdivisions);
        AE_EXPECT_TRUE(sampler.sample(p,true,segment+1==path.size() && step==subdivisions,emit),"sample path");
      }
  };
  std::vector<Point> sparse,dense;sampled(1,sparse);sampled(32,dense);
  AE_EXPECT_TRUE(!ae::test::currentTestFailed(),"sampler accepts input");
  AE_EXPECT_EQ(sparse.size(),dense.size(),"event frequency preserves stamp count");
  for(usize i=0;i<sparse.size();++i)for(u32 axis=0;axis<3;++axis)
    AE_EXPECT_TRUE(std::abs(sparse[i][axis]-dense[i][axis])<.00002f,"subdivision preserves position along polyline");
  std::string error;
  for(auto tool:{resources::TerrainTool::Dig,resources::TerrainTool::Add})
    for(auto application:{resources::TerrainBrushApplication::Accumulate,resources::TerrainBrushApplication::Target}) {
      resources::TerrainRecipe a;a.mode=resources::TerrainMode::SmoothVoxel;
      a.size=16;a.cells=16;a.verticalCells=16;a.height=0;a.base=4;a.baseDepth=8;
      auto b=a;
      const auto populate=[&](auto &recipe,const auto &points) {
        for(const auto &p:points) {
          resources::TerrainStroke s;s.tool=tool;s.application=application;s.gesture=1;
          s.x=p[0];s.y=p[1];s.z=p[2];s.radius=3;s.strength=.4f;s.opacity=.65f;
          recipe.strokes.push_back(s);
        }
      };
      populate(a,sparse);populate(b,dense);
      resources::TerrainRecipe reopened;
      AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(resources::serializeTerrainRecipe(b),reopened,error),error.c_str());
      resources::TerrainField first,second;
      AE_EXPECT_TRUE(resources::generateTerrainField(a,first,error),error.c_str());
      AE_EXPECT_TRUE(resources::generateTerrainField(reopened,second,error),error.c_str());
      AE_EXPECT_EQ(first.density.size(),second.density.size(),"same volume extent");
      for(usize i=0;i<first.density.size();++i)
        AE_EXPECT_TRUE(std::abs(first.density[i]-second.density[i])<.0002f,"Dig/Add density survives event subdivision and save/reopen");
    }
}

AE_TEST(terrain_stroke_sampling_corners_gaps_stationary_and_budget) {
  using Point=std::array<float,3>;
  resources::TerrainStrokeSampler sampler;sampler.reset(1);
  std::vector<Point> points;
  const auto emit=[&](const Point &p){points.push_back(p);return true;};
  AE_EXPECT_TRUE(sampler.sample({0,0,0},false,false,emit),"start");
  AE_EXPECT_TRUE(sampler.sample({.75f,0,0},false,false,emit),"short first leg");
  AE_EXPECT_TRUE(sampler.sample({.75f,0,.75f},false,false,emit),"short second leg");
  AE_EXPECT_EQ(points.size(),2u,"arc length emits across corner");
  AE_EXPECT_EQ(points.back()[0],.75f,"stamp stays on actual second leg");
  AE_EXPECT_EQ(points.back()[2],.25f,"carry distance across corner");
  for(u32 i=0;i<120;++i)AE_EXPECT_TRUE(sampler.sample({.75f,0,.75f},false,false,emit),"stationary input");
  AE_EXPECT_EQ(points.size(),2u,"stationary events add no exposure");
  AE_EXPECT_TRUE(sampler.sample({.75f,0,.75f},false,true,emit),"release flushes residual once");
  AE_EXPECT_EQ(points.size(),3u,"single endpoint stamp");
  sampler.reset(1);points.clear();
  AE_EXPECT_TRUE(sampler.sample({0,0,0},true,false,emit),"volume start");
  AE_EXPECT_TRUE(sampler.sample({0,2,0},true,false,emit),"vertical volume travel");
  AE_EXPECT_EQ(points.size(),3u,"vertical distance affects volumetric tools");
  sampler.disconnect();
  AE_EXPECT_TRUE(sampler.sample({0,100,0},true,false,emit),"reentry");
  AE_EXPECT_EQ(points.size(),4u,"missing hit never bridges the gap");
  sampler.reset(1);points.clear();
  AE_EXPECT_TRUE(sampler.sample({0,0,0},false,false,emit),"surface start");
  AE_EXPECT_TRUE(sampler.sample({0,100,0},false,true,emit),"surface height changes");
  AE_EXPECT_EQ(points.size(),1u,"heightfield spacing is horizontal");
  sampler.reset(.1f);points.clear();
  const auto bounded=[&](const Point &p){if(points.size()==8)return false;points.push_back(p);return true;};
  AE_EXPECT_TRUE(sampler.sample({0,0,0},true,false,bounded),"bounded start");
  AE_EXPECT_TRUE(!sampler.sample({1000,0,0},true,true,bounded),"abort immediately when gesture budget rejects sample");
  AE_EXPECT_EQ(points.size(),8u,"no unbounded allocation or traversal");
  sampler.reset(1);points.clear();
  AE_EXPECT_TRUE(sampler.sample({7,0,0},true,true,emit),"new gesture resets rejected state");
  AE_EXPECT_EQ(points.size(),1u,"no stale segment after reset");
}

AE_TEST(terrain_indexing_preserves_seams_and_all_vertex_attributes) {
  using namespace ae;
  resources::TerrainVertex base;
  base.position[0]=3; base.normal[1]=1; base.uv[0]=.25f;
  std::vector<resources::TerrainVertex> source(6,base);
  source[2].normal[1]=-1;
  source[3].uv1[0]=.75f;
  source[4].weights[0]=.5f; source[4].weights[1]=.5f;
  std::vector<resources::TerrainVertex> vertices;
  std::vector<u32> indices;
  resources::indexTerrainVertices(source,vertices,indices);
  AE_EXPECT_EQ(vertices.size(),4u,"only identical attributes share vertices");
  AE_EXPECT_EQ(indices.size(),source.size(),"no triangle corners removed");
  for(usize i=0;i<source.size();++i)
    AE_EXPECT_TRUE(std::memcmp(&source[i],&vertices[indices[i]],sizeof(source[i]))==0,
                   "indexed reconstruction preserves every attribute bit");
}

AE_TEST(terrain_deterministic_seed_roundtrip_and_chunks) {
  resources::TerrainRecipe r;
  resources::TerrainMesh a, b;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r, a, error), error.c_str());
  AE_EXPECT_EQ(a.chunks, 4u, "four 16x16 chunks");
  AE_EXPECT_EQ(a.triangles, 2048u, "heightfield topology");
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(
                     resources::serializeTerrainRecipe(r), loaded, error),
                 error.c_str());
  AE_EXPECT_TRUE(resources::generateTerrain(loaded, b, error), error.c_str());
  AE_EXPECT_EQ(terrainDigest(a), terrainDigest(b), "deterministic reopen");
  ++loaded.seed;
  AE_EXPECT_TRUE(resources::generateTerrain(loaded, b, error), error.c_str());
  AE_EXPECT_TRUE(terrainDigest(a) != terrainDigest(b), "seed changes terrain");
}

AE_TEST(terrain_revision3_brush_shape_falloff_rotation_and_serialization) {
  resources::TerrainRecipe r;
  r.blendedSurface=true;r.macroVariation=.7f;
  resources::TerrainStroke stroke;
  stroke.shape=resources::TerrainBrushShape::Box;stroke.radius=4;stroke.aspect=2;
  stroke.rotation=90;stroke.hardness=.6f;stroke.gesture=17;
  r.strokes.push_back(stroke);
  resources::TerrainRecipe loaded;std::string error;
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(resources::serializeTerrainRecipe(r),loaded,error),error.c_str());
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded),resources::serializeTerrainRecipe(r),"all brush and surface properties roundtrip");
  AE_EXPECT_TRUE(resources::terrainBrushDistance(stroke,0,0,6,false)<1,"rotated long axis follows viewport footprint");
  AE_EXPECT_TRUE(resources::terrainBrushDistance(stroke,6,0,0,false)>1,"rotated short axis clips footprint");
  AE_EXPECT_EQ(resources::terrainBrushWeight(stroke,.5f),1.f,"hardness keeps a fully influenced core");
  AE_EXPECT_TRUE(resources::terrainBrushWeight(stroke,.9f)<.5f,"falloff softens edge");
  stroke.shape=resources::TerrainBrushShape::Hemisphere;
  AE_EXPECT_TRUE(resources::terrainBrushDistance(stroke,0,-1,0,true)>1,"half sphere rejects lower half");
  stroke.shape=resources::TerrainBrushShape::Cone;
  AE_EXPECT_TRUE(resources::terrainBrushDistance(stroke,0,2,0,true)<1,"cone includes central mass");
  AE_EXPECT_TRUE(resources::terrainBrushDistance(stroke,0,7,3,true)>1,"cone narrows toward tip");
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe("AETERRAIN2 0 1337 16 24 4 64 16 4 3 4 0 1 1 8 0",loaded,error),error.c_str());
  AE_EXPECT_TRUE(!loaded.blendedSurface && loaded.solidBase,"v2 preserves legacy material and structure");
}

AE_TEST(terrain_revision3_sculpt_operations_have_real_effect_and_restore) {
  resources::TerrainRecipe r;r.cells=16;
  resources::TerrainMesh original,changed,restored;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,original,error),error.c_str());
  for(auto tool:{resources::TerrainTool::Noise,resources::TerrainTool::Terrace,resources::TerrainTool::Sharpen}) {
    r.strokes={{tool,0,0,0,40,2,0}};
    AE_EXPECT_TRUE(resources::generateTerrain(r,changed,error),error.c_str());
    AE_EXPECT_TRUE(terrainDigest(original)!=terrainDigest(changed),"new sculpt operation changes actual mesh");
    r.strokes.push_back({resources::TerrainTool::Restore,0,0,0,2048,128,0});
    AE_EXPECT_TRUE(resources::generateTerrain(r,restored,error),error.c_str());
    AE_EXPECT_EQ(terrainDigest(original),terrainDigest(restored),"restore returns procedural geometry and natural weights");
  }
}

AE_TEST(terrain_revision3_hole_closes_solid_rim_and_restores) {
  resources::TerrainRecipe r;r.cells=16;r.height=0;r.solidBase=true;
  resources::TerrainMesh initial,hole,restored;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,initial,error),error.c_str());
  r.strokes={{resources::TerrainTool::Hole,0,0,0,14,1,0}};
  AE_EXPECT_TRUE(resources::generateTerrain(r,hole,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(hole)!=terrainDigest(initial),"hole removes roof and floor cells");
  using Point=std::array<float,3>;using Edge=std::pair<Point,Point>;
  std::map<Edge,u32> edges;
  for(const auto &part:hole.parts)for(usize t=0;t<part.vertices.size();t+=3)
    for(u32 j=0;j<3;++j) {
      const auto *p=part.vertices[t+j].position,*q=part.vertices[t+(j+1)%3].position;
      Point a{p[0],p[1],p[2]},b{q[0],q[1],q[2]};++edges[a<b?Edge{a,b}:Edge{b,a}];
    }
  for(const auto &[edge,count]:edges){(void)edge;AE_EXPECT_EQ(count,2u,"hole rim has no open boundary");}
  r.strokes.push_back({resources::TerrainTool::Restore,0,0,0,2048,128,0});
  AE_EXPECT_TRUE(resources::generateTerrain(r,restored,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(restored),terrainDigest(initial),"restoring fills hole and removes its interior walls");
  r.mode=resources::TerrainMode::SmoothVoxel;
  AE_EXPECT_TRUE(!resources::validTerrainRecipe(r,error),"unsupported heightfield-only tool is refused in volume");
}

AE_TEST(terrain_revision3_volume_shapes_hardness_and_local_restore) {
  resources::TerrainRecipe r;r.mode=resources::TerrainMode::SmoothVoxel;
  r.size=32;r.cells=24;r.verticalCells=32;r.height=0;r.base=8;
  resources::TerrainMesh mesh;std::string error;
  std::vector<std::string> signatures;
  for(u32 shape=0;shape<4;++shape) {
    r.strokes={{resources::TerrainTool::Dig,0,7,0,6,1,0}};
    r.strokes[0].shape=resources::TerrainBrushShape(shape);
    AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
    auto hash=terrainDigest(mesh);
    AE_EXPECT_TRUE(std::find(signatures.begin(),signatures.end(),hash)==signatures.end(),"each volume shape changes the actual cavity");
    signatures.push_back(hash);
  }
  r.strokes[0].shape=resources::TerrainBrushShape::Circle;
  r.strokes[0].hardness=1;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(mesh)!=signatures[0],"hardness changes voxel influence rather than only its cursor");
  r.strokes={{resources::TerrainTool::Raise,0,0,0,20,3,0}};
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  auto original=terrainDigest(mesh);
  r.strokes.push_back({resources::TerrainTool::Restore,0,-100,0,1,1,0});
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(mesh),original,"restore outside volume does not reset the entire height column");
}

AE_TEST(terrain_sculpt_paint_and_dig_change_geometry) {
  resources::TerrainRecipe r;
  r.cells = 16;
  resources::TerrainMesh a, b;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r, a, error), error.c_str());
  r.strokes.push_back({resources::TerrainTool::Raise, 0, 0, 0, 20, 5, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  AE_EXPECT_TRUE(terrainDigest(a) != terrainDigest(b), "raise changes geometry");
  r.strokes.push_back({resources::TerrainTool::Paint, 0, 0, 0, 100, 1, 3});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  for (const auto &p : b.parts)
    AE_EXPECT_EQ(p.layer, 3u, "painting applied to material");
  r.strokes.clear();
  r.mode = resources::TerrainMode::SmoothVoxel;
  AE_EXPECT_TRUE(resources::generateTerrain(r, a, error), error.c_str());
  r.strokes.push_back({resources::TerrainTool::Dig, 0, 8, 0, 8, 1, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  AE_EXPECT_TRUE(terrainDigest(a) != terrainDigest(b), "dig changes volume");
}

AE_TEST(terrain_limits_cancel_and_invalid_input_are_atomic) {
  resources::TerrainRecipe r;
  resources::TerrainMesh mesh;
  mesh.triangles = 123;
  std::string error;
  r.size = std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!resources::generateTerrain(r, mesh, error), "NaN rejected");
  AE_EXPECT_EQ(mesh.triangles, 123u, "output untouched");
  r = {};
  r.mode = resources::TerrainMode::SmoothVoxel;
  r.cells = 128;
  r.verticalCells = 64;
  AE_EXPECT_TRUE(!resources::generateTerrain(r, mesh, error),
                 "volume budget rejected");
  r = {};
  AE_EXPECT_TRUE(
      !resources::generateTerrain(r, mesh, error, [](float) { return false; }),
      "cancelled");
  AE_EXPECT_EQ(mesh.triangles, 123u, "cancel output untouched");
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(
      !resources::deserializeTerrainRecipe("AETERRAIN1 0 2", loaded, error),
      "truncated rejected");
  AE_EXPECT_TRUE(
      !resources::deserializeTerrainRecipe(
          resources::serializeTerrainRecipe(r) + " junk", loaded, error),
      "trailing data rejected");
}

AE_TEST(terrain_path_is_continuous_and_zero_opacity_preserves_volume) {
  resources::TerrainRecipe r;
  r.cells = 32;
  r.height = 0;
  r.base = 4;
  r.strokes = {{resources::TerrainTool::Path, -24, 4, 0, 5, 1, 3},
               {resources::TerrainTool::Path, 24, 4, 0, 5, 1, 3}};
  resources::TerrainMesh mesh;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  bool middle = false;
  for (const auto &p : mesh.parts)
    if (p.layer == 3)
      for (const auto &v : p.vertices)
        if (std::abs(v.position[0]) < 2 && std::abs(v.position[2]) < 2)
          middle = true;
  AE_EXPECT_TRUE(middle, "painted path spans space between endpoints");
  r.strokes.clear();
  r.mode = resources::TerrainMode::SmoothVoxel;
  r.cells = 16;
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  auto before = terrainDigest(mesh);
  r.strokes.push_back({resources::TerrainTool::Dig, 0, 4, 0, 8, 0, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  AE_EXPECT_EQ(terrainDigest(mesh), before, "zero opacity dig is a no-op");
}

AE_TEST(terrain_solid_heightfield_is_closed_across_chunks) {
  resources::TerrainRecipe r;
  r.solidBase = true;
  r.baseDepth = 6;
  r.island = true;
  r.strokes = {{resources::TerrainTool::Lower, 0, 0, 0, 12, 32, 0},
               {resources::TerrainTool::Paint, 0, 0, 0, 128, 1, 3}};
  resources::TerrainMesh mesh;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  AE_EXPECT_EQ(mesh.chunks, 4u, "same four chunks include floor and perimeter");
  AE_EXPECT_EQ(mesh.triangles, 4 * r.cells * r.cells + 8 * r.cells,
               "only top, bottom and outer perimeter; no internal walls");
  using Point = std::array<float, 3>;
  using Edge = std::pair<Point, Point>;
  std::map<Edge, std::pair<u32, i32>> edges;
  float minimumTop = std::numeric_limits<float>::max();
  float minimumBottom = minimumTop, maximumBottom = -minimumTop;
  double volume = 0;
  u32 floorTriangles = 0, wallTriangles = 0;
  for (const auto &part : mesh.parts) {
    AE_EXPECT_EQ(part.y, 0u, "stable heightfield chunk coordinates");
    for (usize i = 0; i < part.vertices.size(); i += 3) {
      const auto *triangle = part.vertices.data() + i;
      for (usize j = 0; j < 3; ++j) {
        const auto &v = triangle[j];
        const auto &next = triangle[(j + 1) % 3];
        Point a{v.position[0], v.position[1], v.position[2]};
        Point b{next.position[0], next.position[1], next.position[2]};
        const bool forward = a < b;
        auto &edge = edges[forward ? Edge{a, b} : Edge{b, a}];
        ++edge.first;
        edge.second += forward ? 1 : -1;
        const float length = v.normal[0] * v.normal[0] +
                             v.normal[1] * v.normal[1] +
                             v.normal[2] * v.normal[2];
        AE_EXPECT_TRUE(std::abs(length - 1) < .0001f,
                       "solid shell has finite unit normals");
        AE_EXPECT_TRUE(std::isfinite(v.uv[0]) && std::isfinite(v.uv[1]),
                       "solid shell has finite projected texture coordinates");
        if (v.normal[1] > 0)
          minimumTop = std::min(minimumTop, v.position[1]);
        else if (v.normal[1] == -1) {
          minimumBottom = std::min(minimumBottom, v.position[1]);
          maximumBottom = std::max(maximumBottom, v.position[1]);
        }
      }
      if (triangle[0].normal[1] == -1) {
        ++floorTriangles;
        AE_EXPECT_EQ(part.layer, 1u, "floor stays rock when surface is painted");
      } else if (triangle[0].normal[1] == 0) {
        ++wallTriangles;
        AE_EXPECT_EQ(part.layer, 1u, "wall stays rock when surface is painted");
        for (usize j = 0; j < 3; ++j) {
          const auto &v = triangle[j];
          const float coordinate = v.normal[0] != 0 ? v.position[0]
                                                    : v.position[2];
          const float outward = v.normal[0] != 0 ? v.normal[0] : v.normal[2];
          AE_EXPECT_EQ(coordinate, outward * r.size * .5f,
                       "walls are outward facing at the footprint edge");
        }
      }
      const auto *a = triangle[0].position, *b = triangle[1].position,
                 *c = triangle[2].position;
      volume += (double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) +
                 double(a[1]) * (double(b[2]) * c[0] - double(b[0]) * c[2]) +
                 double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0])) /
                6;
    }
  }
  for (const auto &[edge, count] : edges) {
    (void)edge;
    AE_EXPECT_EQ(count.first, 2u, "each edge is shared by exactly two faces");
    AE_EXPECT_EQ(count.second, 0, "shared edges have opposite winding");
  }
  AE_EXPECT_EQ(minimumBottom, maximumBottom, "underside is a horizontal plane");
  AE_EXPECT_TRUE(std::abs(minimumTop - minimumBottom - r.baseDepth) < .0001f,
                 "minimum thickness follows the lowest sculpted point");
  AE_EXPECT_EQ(floorTriangles, 2 * r.cells * r.cells, "complete planar floor");
  AE_EXPECT_EQ(wallTriangles, 8 * r.cells, "four closed outer walls");
  AE_EXPECT_TRUE(volume > 0, "solid triangle winding encloses positive volume");
}

AE_TEST(terrain_solid_depth_limits_budget_and_cancel_are_atomic) {
  resources::TerrainRecipe r;
  r.solidBase = true;
  resources::TerrainMesh mesh;
  mesh.triangles = 123;
  std::string error;
  for (float invalid : {0.f, -.1f, 512.1f,
                        std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    r.baseDepth = invalid;
    AE_EXPECT_TRUE(!resources::generateTerrain(r, mesh, error),
                   "invalid thickness rejected");
    AE_EXPECT_EQ(mesh.triangles, 123u, "bad thickness leaves mesh untouched");
  }
  r.baseDepth = .1f;
  r.cells = 128;
  u32 callbacks = 0;
  AE_EXPECT_TRUE(!resources::generateTerrain(r, mesh, error, [&](float) {
                   return ++callbacks < 3;
                 }),
                 "cancel stops partial multi-chunk generation");
  AE_EXPECT_EQ(mesh.triangles, 123u, "partial solid result is never published");
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  AE_EXPECT_EQ(mesh.triangles, 66560u, "maximum heightfield is within budget");
  AE_EXPECT_EQ(mesh.chunks, 64u, "maximum resolution retains chunk layout");
  r.baseDepth = 512;
  r.cells = 8;
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
}

AE_TEST(terrain_extended_brush_shape_boundary_and_serialization) {
  resources::TerrainRecipe r;
  resources::TerrainStroke brush;
  brush.aspect=1.7f;brush.verticalAspect=.6f;brush.depthAspect=2.2f;
  brush.rotation=37;brush.pitch=29;brush.roll=-23;brush.opacity=.37f;
  brush.surfaceDepth=2.4f;brush.lockHeight=true;brush.dimensionsLinked=false;
  brush.filterRadius=3;brush.application=resources::TerrainBrushApplication::Target;
  std::string error;
  for(u32 shape=0;shape<5;++shape)for(bool inverted:{false,true}) {
    brush.shape=resources::TerrainBrushShape(shape);brush.inverted=inverted;
    for(u32 ring=0;ring<3;++ring)for(u32 segment=1;segment<48;++segment) {
      // Half-space caps and the cone apex have an intentional one-sided boundary.
      if(shape==2 || shape==3)continue;
      auto p=resources::terrainBrushOutline(brush,ring,segment/48.f);
      const float d=resources::terrainBrushDistance(brush,p[0],p[1],p[2],true);
      AE_EXPECT_TRUE(std::abs(d-1)<.0001f,"rotated XYZ outline matches the operator boundary");
    }
    r.strokes={brush};resources::TerrainRecipe loaded;
    const auto encoded=resources::serializeTerrainRecipe(r);
    AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(encoded,loaded,error),error.c_str());
    AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded),encoded,"all advanced properties survive save/reopen");
  }
  brush.pitch=std::numeric_limits<float>::infinity();r.strokes={brush};
  AE_EXPECT_TRUE(!resources::validTerrainRecipe(r,error),"reject nonfinite orientation before generating");
}

AE_TEST(terrain_target_paint_caps_weight_and_preserves_other_ratios) {
  resources::TerrainRecipe r;r.cells=16;r.height=0;
  resources::TerrainStroke b;b.tool=resources::TerrainTool::Paint;b.layer=3;b.radius=200;
  b.hardness=1;b.opacity=.35f;b.application=resources::TerrainBrushApplication::Target;
  r.strokes={b};resources::TerrainMesh once,many;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,once,error),error.c_str());
  r.strokes.assign(12,b);
  AE_EXPECT_TRUE(resources::generateTerrain(r,many,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(once),terrainDigest(many),"target paint is idempotent instead of accumulating to opaque");
  for(const auto &part:many.parts)for(const auto &v:part.vertices) {
    AE_EXPECT_TRUE(std::abs(v.weights[3]-.35f)<.0001f,"selected layer reaches chosen opacity");
    float sum=0;for(float w:v.weights)sum+=w;
    AE_EXPECT_TRUE(std::abs(sum-1)<.0001f,"remaining weights stay normalized");
  }
}

AE_TEST(terrain_density_target_is_idempotent_but_accumulation_changes_volume) {
  resources::TerrainRecipe r;r.mode=resources::TerrainMode::SmoothVoxel;r.cells=r.verticalCells=16;r.size=32;r.height=0;
  resources::TerrainStroke b;b.tool=resources::TerrainTool::Dig;b.y=4;b.radius=7;b.strength=.3f;b.gesture=1;
  b.application=resources::TerrainBrushApplication::Target;
  r.strokes={b};resources::TerrainMesh once,many;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,once,error),error.c_str());
  r.strokes.assign(8,b);AE_EXPECT_TRUE(resources::generateTerrain(r,many,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(once),terrainDigest(many),"same gesture target does not overshoot with extra events");
  b.application=resources::TerrainBrushApplication::Accumulate;r.strokes={b};
  AE_EXPECT_TRUE(resources::generateTerrain(r,once,error),error.c_str());
  r.strokes.assign(8,b);AE_EXPECT_TRUE(resources::generateTerrain(r,many,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(once)!=terrainDigest(many),"accumulative volume operator adds density change");
}

AE_TEST(terrain_incremental_chunks_match_full_rebuild_including_seams) {
  for(auto mode:{resources::TerrainMode::Heightfield,resources::TerrainMode::SmoothVoxel,resources::TerrainMode::BlockVoxel}) {
    resources::TerrainRecipe r;r.mode=mode;r.cells=64;r.verticalCells=16;r.size=64;r.height=0;
    resources::TerrainBuild base,edited;resources::TerrainMesh full;std::string error;
    AE_EXPECT_TRUE(resources::updateTerrain(r,nullptr,base,error),error.c_str());
    resources::TerrainStroke stroke;stroke.tool=mode==resources::TerrainMode::Heightfield?resources::TerrainTool::Raise:resources::TerrainTool::Dig;
    stroke.x=-16;stroke.y=4;stroke.z=-16;stroke.radius=3;stroke.gesture=1;
    r.strokes={stroke};
    AE_EXPECT_TRUE(resources::updateTerrain(r,&base,edited,error),error.c_str());
    AE_EXPECT_TRUE(resources::generateTerrain(r,full,error),error.c_str());
    AE_EXPECT_EQ(terrainDigest(edited.mesh),terrainDigest(full),"localized publication matches full vertices, normals and weights");
    AE_EXPECT_TRUE(edited.mesh.reusedChunks>0 && edited.mesh.rebuiltChunks>0,"unaffected chunks reused; seam dependencies rebuilt");
    base=std::move(edited);stroke.tool=resources::TerrainTool::Paint;stroke.layer=3;stroke.opacity=.3f;stroke.gesture=2;
    r.strokes.push_back(stroke);
    AE_EXPECT_TRUE(resources::updateTerrain(r,&base,edited,error),error.c_str());
    AE_EXPECT_TRUE(resources::generateTerrain(r,full,error),error.c_str());
    AE_EXPECT_EQ(terrainDigest(edited.mesh),terrainDigest(full),"material-only edits invalidate affected chunks too");
  }
}

AE_TEST(terrain_persistent_paths_roundtrip_reedit_and_independent_channels) {
  resources::TerrainRecipe r;r.cells=64;r.size=64;r.height=0;
  resources::TerrainPath path;path.id=9;path.name="Rio / Caminho 01";
  path.points={{-20,6,-10,true},{0,8,0,false},{20,10,12,false}};
  path.width=5;path.shoulderWidth=12;path.elevation=1;path.uniformRamp=1;
  path.profile={{0,1},{.3f,.8f},{.7f,.2f},{1,0}};
  r.paths={path};std::string error;resources::TerrainRecipe restored;
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(resources::serializeTerrainRecipe(r),restored,error),error.c_str());
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(restored),resources::serializeTerrainRecipe(r),"resource points, profiles and names roundtrip");
  resources::TerrainMesh shaped,textured,original,reshaped;
  AE_EXPECT_TRUE(resources::generateTerrain(r,shaped,error),error.c_str());
  r.paths[0].shape=false;
  AE_EXPECT_TRUE(resources::generateTerrain(r,textured,error),error.c_str());
  for(const auto &part:textured.parts)for(const auto &v:part.vertices)AE_EXPECT_TRUE(std::abs(v.position[1]-r.base)<.0001f,"texture-only path does not change elevations");
  r.paths[0].texture=false;
  AE_EXPECT_TRUE(resources::generateTerrain(r,original,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(textured)!=terrainDigest(original),"independent texturing switch changes real surface weights");
  r.paths[0]=path;r.paths[0].points[1].z=20;
  AE_EXPECT_TRUE(resources::generateTerrain(r,reshaped,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(reshaped)!=terrainDigest(shaped),"moving retained point rebuilds the path");
  r.paths[0]=path;AE_EXPECT_TRUE(resources::generateTerrain(r,reshaped,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(reshaped),terrainDigest(shaped),"restoring points has no residual baked old path");
}

AE_TEST(terrain_path_edits_reuse_chunks_without_old_geometry_or_paint) {
  for(auto mode:{resources::TerrainMode::Heightfield,resources::TerrainMode::SmoothVoxel,resources::TerrainMode::BlockVoxel}) {
    resources::TerrainRecipe r;r.mode=mode;r.cells=64;r.verticalCells=16;r.size=64;r.height=0;
    resources::TerrainPath path;path.points={{-24,4,-24,true},{-16,6,-18,false},{-12,4,-24,false}};
    path.width=2;path.shoulderWidth=4;path.textureTransition=.5f;path.elevation=2;path.autoRamp=false;
    r.paths={path};resources::TerrainBuild previous,next;resources::TerrainMesh full;std::string error;
    AE_EXPECT_TRUE(resources::updateTerrain(r,nullptr,previous,error),error.c_str());
    for(u32 edit=0;edit<7;++edit) {
      switch(edit) {
      case 0:r.paths[0].points[1].z+=4;break;
      case 1:r.paths[0].surfaceLayer=1;r.paths[0].shoulderLayer=3;break;
      case 2:r.paths[0].shape=false;r.paths[0].textureOpacity=.35f;break;
      case 3:{auto crossing=r.paths[0];crossing.id=2;crossing.surfaceLayer=2;crossing.smartPaint=false;
        crossing.points[1].x-=4;r.paths[0].smartPaint=false;r.paths.push_back(crossing);break;}
      case 4:std::reverse(r.paths.begin(),r.paths.end());break;
      case 5:r.paths[0].enabled=false;break;
      case 6:r.paths.clear();break;
      }
      AE_EXPECT_TRUE(resources::updateTerrain(r,&previous,next,error),error.c_str());
      AE_EXPECT_TRUE(resources::generateTerrain(r,full,error),error.c_str());
      AE_EXPECT_EQ(terrainDigest(next.mesh),terrainDigest(full),"point/material/channel/order/delete edits match a clean rebuild");
      AE_EXPECT_TRUE(next.mesh.reusedChunks>0,"distant chunks are reused in all three mesh modes");
      previous=std::move(next);
    }
  }
}

AE_TEST(terrain_incremental_volume_filters_match_boundaries_and_restore) {
  resources::TerrainRecipe r;r.mode=resources::TerrainMode::SmoothVoxel;r.cells=32;r.verticalCells=16;r.size=32;r.height=0;
  resources::TerrainBuild previous,next;resources::TerrainMesh full;std::string error;
  AE_EXPECT_TRUE(resources::updateTerrain(r,nullptr,previous,error),error.c_str());
  for(auto tool:{resources::TerrainTool::Dig,resources::TerrainTool::Smooth,resources::TerrainTool::Sharpen,resources::TerrainTool::Restore}) {
    resources::TerrainStroke b;b.tool=tool;b.x=-16;b.z=-16;b.y=4;b.radius=6;b.filterRadius=3;b.gesture=u32(r.strokes.size()+1);
    r.strokes.push_back(b);
    AE_EXPECT_TRUE(resources::updateTerrain(r,&previous,next,error),error.c_str());
    AE_EXPECT_TRUE(resources::generateTerrain(r,full,error),error.c_str());
    AE_EXPECT_EQ(terrainDigest(next.mesh),terrainDigest(full),"finite-volume border and filter halo match ordered replay");
    previous=std::move(next);
  }
}

AE_TEST(terrain_path_curve_and_surface_sampling_are_consistent) {
  resources::TerrainPath path;path.points={{-8,4,-8,true},{0,4,8,false},{8,4,-8,false}};
  const auto curved=resources::terrainPathPolyline(path,.5f);path.points[0].curved=false;
  const auto straight=resources::terrainPathPolyline(path,.5f);
  AE_EXPECT_EQ(curved.front(),straight.front(),"curve preserves the first point");
  AE_EXPECT_EQ(curved.back(),straight.back(),"curve preserves the final point");
  AE_EXPECT_TRUE(curved!=straight,"curved segments are visible in the actual preview samples");
  resources::TerrainField field;field.width=field.depth=2;field.step=2;field.heights={0,2,4,6};
  AE_EXPECT_TRUE(std::abs(resources::terrainFieldHeight(field,0,0)-3)<.00001f,"preview height uses bilinear interpolation");
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,-5,-5),0.f,"outside sample clamps to terrain border");
}

AE_TEST(terrain_path_point_heights_ramps_and_crest_have_independent_effects) {
  resources::TerrainRecipe r;r.cells=64;r.size=64;r.height=0;
  resources::TerrainPath p;p.points={{-16,6,0,false},{0,14,0,false},{16,10,0,false}};
  p.autoRamp=false;p.width=4;p.shoulderWidth=12;p.followTerrain=false;r.paths={p};
  resources::TerrainField field;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,0,0),14.f,"intermediate point Y controls the actual road height");
  r.paths[0].uniformRamp=1;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_TRUE(std::abs(resources::terrainFieldHeight(field,0,0)-8)<.001f,"uniform ramp connects endpoint heights by horizontal length");
  r.paths[0].uniformRamp=.5f;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_TRUE(std::abs(resources::terrainFieldHeight(field,0,0)-11)<.001f,"ramp blend is continuous");
  r.paths[0].uniformRamp=0;r.paths[0].followTerrain=true;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,0,0),r.base,"follow mode uses the sculpted surface");
  r.paths[0].useDefinedHeight=true;r.paths[0].definedHeight=8;
  r.paths[0].profile={{0,1},{.5f,1.5f},{1,0}};
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,0,4),10.f,"crest profile rises above the road without changing the lane");
  r.paths[0].profile[1].y=-.5f;
  AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,0,4),2.f,"negative profile creates a shoulder ditch");
  r.paths[0].textureProfile[0].y=1.5f;
  AE_EXPECT_TRUE(!resources::validTerrainRecipe(r,error),"signed geometry range never leaks into texture opacity");
  r.paths[0].textureProfile[0].y=1;r.paths[0].followTerrain=false;
  resources::TerrainRecipe loaded;auto encoded=resources::serializeTerrainRecipe(r);
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(encoded,loaded,error),error.c_str());
  AE_EXPECT_TRUE(!loaded.paths[0].followTerrain,"point-height mode persists");
  // Project the legacy parser boundary from the non-FX body: surface reference,
  // v7 map resolution, v6 LOD fields, then v5 followTerrain. This constructed
  // boundary is separate from the real frozen historical-writer fixtures.
  encoded=resources::terrainRecipeNonFxKey(r);
  encoded.replace(0,10,"AETERRAIN4");
  for(u32 i=0;i<5;++i)encoded.resize(encoded.rfind(' '));
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(encoded,loaded,error),error.c_str());
  AE_EXPECT_TRUE(loaded.paths[0].followTerrain,"version 4 migration preserves the previous follow behavior");
}

AE_TEST(terrain_path_smart_crossings_preserve_strongest_without_order_residue) {
  resources::TerrainRecipe r;r.cells=32;r.height=0;
  resources::TerrainPath a;a.id=1;a.shape=false;a.points={{-20,4,0,false},{20,4,0,false}};
  a.width=8;a.shoulderWidth=12;a.surfaceLayer=2;a.shoulderLayer=2;a.textureOpacity=a.shoulderOpacity=.3f;
  auto b=a;b.id=2;b.surfaceLayer=b.shoulderLayer=3;b.textureOpacity=b.shoulderOpacity=.7f;
  r.paths={a,b};resources::TerrainMesh forward,reverse;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,forward,error),error.c_str());
  std::reverse(r.paths.begin(),r.paths.end());
  AE_EXPECT_TRUE(resources::generateTerrain(r,reverse,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(forward),terrainDigest(reverse),"smart crossings choose the strongest coverage before painting, independent of ordering");
  r.paths[0].textureOpacity=r.paths[0].shoulderOpacity=.3f;
  AE_EXPECT_TRUE(resources::generateTerrain(r,forward,error),error.c_str());
  std::reverse(r.paths.begin(),r.paths.end());
  AE_EXPECT_TRUE(resources::generateTerrain(r,reverse,error),error.c_str());
  AE_EXPECT_EQ(terrainDigest(forward),terrainDigest(reverse),"equal coverage resolves by stable path ID");
  for(auto &path:r.paths)path.smartPaint=false;
  AE_EXPECT_TRUE(resources::generateTerrain(r,forward,error),error.c_str());
  std::reverse(r.paths.begin(),r.paths.end());
  AE_EXPECT_TRUE(resources::generateTerrain(r,reverse,error),error.c_str());
  AE_EXPECT_TRUE(terrainDigest(forward)!=terrainDigest(reverse),"explicit ordered-overpaint mode still respects path order");
}

AE_TEST(terrain_path_caps_clip_geometry_and_paint_at_terminal_planes) {
  resources::TerrainRecipe r;r.cells=64;r.size=64;r.height=0;r.base=4;
  resources::TerrainPath path;path.points={{-16,4,0,false},{16,4,0,false}};
  path.width=8;path.shoulderWidth=12;path.elevation=6;path.autoRamp=false;
  path.surfaceLayer=path.shoulderLayer=3;
  std::string error;
  for(bool duplicateEndpoints:{false,true}) {
  if(duplicateEndpoints)path.points={{-16,4,0,false},{-16,4,0,false},{16,4,0,false},{16,4,0,false}};
  for(const auto cap:{resources::TerrainPathCap::Round,resources::TerrainPathCap::Flat,resources::TerrainPathCap::Square}) {
    path.cap=cap;r.paths={path};resources::TerrainField field;
    AE_EXPECT_TRUE(resources::generateTerrainField(r,field,error),error.c_str());
    const auto changed=[&](float x,float z){return resources::terrainFieldHeight(field,x,z)>r.base+.01f;};
    for(float sign:{-1.f,1.f}) {
      AE_EXPECT_TRUE(changed(sign*15,0),"lane interior remains shaped");
      AE_EXPECT_EQ(changed(sign*17,0),cap!=resources::TerrainPathCap::Flat,"flat cap ends at the endpoint plane");
      AE_EXPECT_EQ(changed(sign*20,5),cap==resources::TerrainPathCap::Square,"square cap retains its shoulder corner");
      AE_EXPECT_EQ(changed(sign*21,0),cap==resources::TerrainPathCap::Round,"square cap does not gain an extra rounded extension");
    }
    resources::TerrainMesh mesh;
    AE_EXPECT_TRUE(resources::meshTerrainField(r,field,mesh,error),error.c_str());
    bool sampled=false;
    for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
      if(std::abs(v.position[0]-21)<.001f && std::abs(v.position[2])<.001f) {
        sampled=true;
        AE_EXPECT_EQ(v.weights[3]>.01f,cap==resources::TerrainPathCap::Round,"paint observes the same terminal cap as shape");
      }
    }
    AE_EXPECT_TRUE(sampled,"paint probe exists in exported surface");
  }
  }
}

AE_TEST(terrain_control_map_replays_recipe_independently_of_mesh_cells) {
  using namespace resources;
  TerrainRecipe a;a.cells=8;a.controlCells=32;a.height=0;
  TerrainStroke stroke;stroke.tool=TerrainTool::Paint;stroke.layer=3;stroke.radius=7;stroke.opacity=.4f;
  a.strokes.push_back(stroke);
  TerrainPath path;path.points={{-20,0,-12},{20,0,12}};path.surfaceLayer=2;a.paths.push_back(path);
  TerrainField fa,fb;std::string error;
  AE_EXPECT_TRUE(generateTerrainField(a,fa,error),"material field");
  auto b=a;b.cells=64;
  AE_EXPECT_TRUE(generateTerrainField(b,fb,error),"different mesh field");
  AE_EXPECT_EQ(fa.controlWidth,33u,"independent map dimensions");
  AE_EXPECT_TRUE(fa.controlWeights==fb.controlWeights,"mesh cells cannot change authoritative material map");
  TerrainMesh mesh;AE_EXPECT_TRUE(meshTerrainField(a,fa,mesh,error),"map is consumed by mesh");
  for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
    const auto weights=terrainFieldWeights(fa,v.position[0],v.position[2]);
    for(u32 l=0;l<4;++l)AE_EXPECT_TRUE(std::abs(weights[l]-v.weights[l])<.00001f,"vertex consumes map exactly once");
  }
  const auto original=serializeTerrainRecipe(a);a.controlCells=8;
  TerrainField coarse;AE_EXPECT_TRUE(generateTerrainField(a,coarse,error),"coarse map");
  a.controlCells=32;TerrainField restored;
  AE_EXPECT_TRUE(generateTerrainField(a,restored,error),"replay fine map");
  AE_EXPECT_TRUE(restored.controlWeights==fa.controlWeights,"resolution change never destructively resamples paint");
  TerrainRecipe reopened;AE_EXPECT_TRUE(deserializeTerrainRecipe(original,reopened,error),"version seven reopen");
  AE_EXPECT_EQ(reopened.controlCells,32u,"map resolution persists");
  AE_EXPECT_TRUE(serializeTerrainRecipe(reopened)==original,"strokes and paths preserved");
}
AE_TEST(terrain_control_map_legacy_volume_cancellation_and_limits) {
  using namespace resources;std::string error;
  TerrainRecipe r;r.cells=8;r.height=0;
  auto legacy=terrainRecipeNonFxKey(r);legacy.replace(0,10,"AETERRAIN6");legacy.erase(legacy.find_last_of(' '));legacy.erase(legacy.find_last_of(' '));
  TerrainRecipe old;AE_EXPECT_TRUE(deserializeTerrainRecipe(legacy,old,error),"version six remains readable");
  AE_EXPECT_EQ(old.controlCells,0u,"legacy map disabled");
  TerrainMesh before,after;AE_EXPECT_TRUE(generateTerrain(r,before,error),"legacy baseline");
  AE_EXPECT_TRUE(generateTerrain(old,after,error),"legacy reopened");
  AE_EXPECT_EQ(terrainDigest(before),terrainDigest(after),"legacy vertex semantics unchanged");
  r.controlCells=512;TerrainField map;
  AE_EXPECT_TRUE(generateTerrainField(r,map,error),"maximum map fits budget");
  AE_EXPECT_EQ(map.controlWeights.size(),usize(513)*513,"map memory has bounded texels");
  TerrainField unchanged;unchanged.width=777;
  AE_EXPECT_TRUE(!generateTerrainField(r,unchanged,error,[](float){return false;}),"cancel map generation");
  AE_EXPECT_EQ(unchanged.width,777u,"cancel leaves output atomic");
  r.controlCells=9;AE_EXPECT_TRUE(!validTerrainRecipe(r,error),"reject invalid map resolution");
  r.controlCells=0;r.mode=TerrainMode::SmoothVoxel;r.verticalCells=8;
  TerrainMesh volumeA,volumeB;AE_EXPECT_TRUE(generateTerrain(r,volumeA,error),"volume baseline");
  r.controlCells=32;AE_EXPECT_TRUE(generateTerrain(r,volumeB,error),"volume ignores two dimensional map");
  AE_EXPECT_EQ(terrainDigest(volumeA),terrainDigest(volumeB),"volume wall and ceiling semantics unchanged");
}
AE_TEST(terrain_control_map_texel_halo_reuses_unaffected_chunks) {
  using namespace resources;std::string error;
  TerrainRecipe r;r.cells=64;r.controlCells=8;r.height=0;
  TerrainBuild before,after;
  AE_EXPECT_TRUE(updateTerrain(r,nullptr,before,error),"baseline map build");
  TerrainStroke paint;paint.tool=TerrainTool::Paint;paint.x=-24;paint.z=-24;
  paint.radius=1;paint.layer=3;r.strokes.push_back(paint);
  AE_EXPECT_TRUE(updateTerrain(r,&before,after,error),"regional material edit");
  AE_EXPECT_TRUE(after.mesh.reusedChunks>0,"distant chunks reused");
  TerrainMesh full;AE_EXPECT_TRUE(generateTerrain(r,full,error),"reference full rebuild");
  AE_EXPECT_EQ(terrainDigest(after.mesh),terrainDigest(full),"coarse texel influence includes neighboring mesh chunks");
  const auto fallback=std::array<float,4>{1,0,0,0};
  AE_EXPECT_TRUE(terrainFieldWeights(after.field,std::numeric_limits<float>::quiet_NaN(),0)==fallback,"NaN coordinate rejected");
  AE_EXPECT_TRUE(terrainFieldWeights(after.field,0,std::numeric_limits<float>::infinity())==fallback,"infinite coordinate rejected");
  after.field.controlStep=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(terrainFieldWeights(after.field,0,0)==fallback,"NaN step rejected");
}
AE_TEST(terrain_control_map_brush_bins_preserve_order_and_rotated_footprints) {
  using namespace resources;std::string error;
  TerrainRecipe r;r.cells=32;r.controlCells=32;r.height=0;
  for(u32 i=0;i<12;++i) {
    TerrainStroke s;s.tool=i%3==2?TerrainTool::Restore:TerrainTool::Paint;
    s.application=i%2?TerrainBrushApplication::Target:TerrainBrushApplication::Accumulate;
    s.shape=i%2?TerrainBrushShape::Box:TerrainBrushShape::RoundedBox;
    s.x=float(i%4)*12-18;s.z=float(i/4)*12-12;s.radius=5;
    s.aspect=2;s.depthAspect=.5f;s.rotation=37;s.pitch=70;s.roll=43;
    s.layer=i%4;s.opacity=.4f;r.strokes.push_back(s);
  }
  TerrainField map;AE_EXPECT_TRUE(generateTerrainField(r,map,error),"binned map");
  auto reference=r;reference.controlCells=0;TerrainMesh mesh;
  AE_EXPECT_TRUE(generateTerrain(reference,mesh,error),"legacy vertex evaluator reference");
  for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
    const auto weights=terrainFieldWeights(map,v.position[0],v.position[2]);
    for(u32 l=0;l<4;++l)AE_EXPECT_TRUE(std::abs(weights[l]-v.weights[l])<.00001f,"culling preserves exact ordered evaluator");
  }
  for(u32 i=0;i<128;++i) {
    TerrainStroke outside;outside.tool=TerrainTool::Paint;outside.x=4096;outside.z=-4096;
    outside.radius=.1f;outside.layer=3;r.strokes.push_back(outside);
  }
  TerrainField padded;AE_EXPECT_TRUE(generateTerrainField(r,padded,error),"outside-grid strokes");
  AE_EXPECT_TRUE(map.controlWeights==padded.controlWeights,"outside-grid bins cannot affect weights");
  r.strokes.back().x=std::numeric_limits<float>::infinity();
  TerrainField unchanged;unchanged.width=444;
  AE_EXPECT_TRUE(!generateTerrainField(r,unchanged,error),"infinite brush rejected before bins");
  AE_EXPECT_EQ(unchanged.width,444u,"invalid brush leaves output atomic");
}
AE_TEST(terrain_control_texture_rejects_invalid_fields_and_preserves_energy) {
  using namespace resources;std::string error;TerrainRecipe r;r.cells=8;r.controlCells=8;
  TerrainField field;AE_EXPECT_TRUE(generateTerrainField(r,field,error),error.c_str());
  for(auto &weights:field.controlWeights)weights={.25f,.25f,.25f,.25f};
  renderer::SharedAuthoringTexture texture;
  AE_EXPECT_TRUE(makeTerrainControlTexture(field,texture,error),error.c_str());
  renderer::SharedAuthoringTexture same;
  AE_EXPECT_TRUE(makeTerrainControlTexture(field,same,error),error.c_str());
  AE_EXPECT_TRUE(texture==same,"unchanged control pixels reuse immutable allocation during preview");
  for(usize i=0;i<texture->mipChain.size();i+=4)
    AE_EXPECT_EQ(u32(texture->mipChain[i])+texture->mipChain[i+1]+texture->mipChain[i+2]+texture->mipChain[i+3],255u,"RGBA8 weights sum to exactly 255");
  const auto original=texture;
  field.controlWeights[0]={0,0,0,0};
  AE_EXPECT_TRUE(!makeTerrainControlTexture(field,texture,error),"zero energy rejected");
  AE_EXPECT_TRUE(texture==original,"invalid field preserves texture output");
  field.controlWeights[0]={1,0,0,std::numeric_limits<float>::quiet_NaN()};
  AE_EXPECT_TRUE(!makeTerrainControlTexture(field,texture,error),"NaN rejected");
  field.controlWeights[0]={1,0,0,0};field.height=9;
  AE_EXPECT_TRUE(!makeTerrainControlTexture(field,texture,error),"volume field cannot masquerade as planar map");
  field.height=1;field.controlWidth=8;
  AE_EXPECT_TRUE(!makeTerrainControlTexture(field,texture,error),"insufficient control resolution rejected");
}

AE_TEST(terrain_control_map_rejects_malformed_field_before_meshing) {
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=8;
  resources::TerrainField field;
  resources::TerrainMesh unchanged;unchanged.triangles=777;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrainField(recipe,field,error),error.c_str());
  field.controlWeights.front()[0]=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!resources::meshTerrainField(recipe,field,unchanged,error),"nonfinite weight rejected");
  AE_EXPECT_EQ(unchanged.triangles,777u,"invalid map never mutates output mesh");
  field.controlWeights.front()={.5f,.5f,.5f,.5f};
  AE_EXPECT_TRUE(!resources::meshTerrainField(recipe,field,unchanged,error),"unnormalized weights rejected");
  field.controlWeights.front()={1,0,0,0};field.controlStep*=2;
  AE_EXPECT_TRUE(!resources::meshTerrainField(recipe,field,unchanged,error),"map spacing must match authoritative recipe");
}
