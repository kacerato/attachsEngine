#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "renderer/authoring_texture.h"
#include "resources/gltf_import.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_textures.generated.h"
#include "scene/collider.h"
#include "scene/import_link.h"
#include "scene/mesh_renderer.h"
#include "ui_software_raster.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <thread>

using namespace ae;
namespace {
std::string digest(const resources::TerrainMesh &mesh) {
  std::vector<u8> bytes;
  for (const auto &p : mesh.parts) {
    auto *b = reinterpret_cast<const u8 *>(p.vertices.data());
    bytes.insert(bytes.end(), b,
                 b + p.vertices.size() * sizeof(resources::TerrainVertex));
  }
  return Sha256::hex(bytes);
}
struct TerrainFixture {
  editor::EditorSession session;
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::filesystem::path path;
  bool refuse = false, ready = false;
  TerrainFixture() {
    path = std::filesystem::temp_directory_path() /
           ("astra-terrain-test-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices,
                                         indices, draws, materials);
    session.importMap(draws, materials, false, vertices, indices, 0);
    session.setGeometryPublisher(
        [this](std::span<const u8> v, std::span<const u32> i,
               std::span<const renderer::MapDrawRecord> d,
               std::span<const renderer::MapMaterialRecord> m,
               std::span<const renderer::SharedAuthoringTexture>,
               editor::EditorSession::PublishedGeometry &out) {
          if (refuse)
            return false;
          vertices.clear();
          indices.clear();
          draws.clear();
          materials.clear();
          renderer::appendBoxAuthoringGeometry(
              renderer::MapVertexStride, vertices, indices, draws, materials);
          u32 vb = u32(vertices.size() / renderer::MapVertexStride),
              ib = u32(indices.size()), mb = u32(materials.size());
          vertices.insert(vertices.end(), v.begin(), v.end());
          indices.insert(indices.end(), i.begin(), i.end());
          materials.insert(materials.end(), m.begin(), m.end());
          for (auto draw : d) {
            draw.vertexOffset += vb;
            draw.firstIndex += ib;
            draw.materialIndex += mb;
            draw.lodGroupId = u32(draws.size());
            draws.push_back(draw);
          }
          out = {draws, materials, vertices, indices};
          return true;
        });
    ready = session.setProjectDirectory(path.string().c_str()) &&
            session.openTerrain();
  }
  ~TerrainFixture() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
} // namespace
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
  AE_EXPECT_EQ(digest(a), digest(b), "deterministic reopen");
  ++loaded.seed;
  AE_EXPECT_TRUE(resources::generateTerrain(loaded, b, error), error.c_str());
  AE_EXPECT_TRUE(digest(a) != digest(b), "seed changes terrain");
}
AE_TEST(terrain_all_modes_export_and_import_real_parser) {
  for (u32 mode = 0; mode < 3; ++mode) {
    resources::TerrainRecipe r;
    r.mode = resources::TerrainMode(mode);
    r.cells = 16;
    resources::TerrainMesh mesh;
    std::string error;
    AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
    std::vector<u8> bytes;
    AE_EXPECT_TRUE(resources::writeTerrainGlb(r, mesh, {}, bytes, error),
                   error.c_str());
    resources::GltfImport model;
    AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, model),
                   model.diagnostic.c_str());
    AE_EXPECT_EQ(model.draws.size(), mesh.parts.size(),
                 "all chunk layers imported");
    resources::TerrainRecipe loaded;
    AE_EXPECT_TRUE(resources::terrainRecipeFromGlb(bytes, loaded, error),
                   error.c_str());
    AE_EXPECT_EQ(resources::serializeTerrainRecipe(r),
                 resources::serializeTerrainRecipe(loaded), "recipe in GLB");
    for (const auto &part : mesh.parts)
      for (const auto &v : part.vertices) {
        float n = 0;
        for (float value : v.normal)
          n += value * value;
        AE_EXPECT_TRUE(std::isfinite(n) && std::abs(n - 1) < .01f,
                       "finite unit normals");
      }
  }
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
    AE_EXPECT_TRUE(digest(original)!=digest(changed),"new sculpt operation changes actual mesh");
    r.strokes.push_back({resources::TerrainTool::Restore,0,0,0,2048,128,0});
    AE_EXPECT_TRUE(resources::generateTerrain(r,restored,error),error.c_str());
    AE_EXPECT_EQ(digest(original),digest(restored),"restore returns procedural geometry and natural weights");
  }
}

AE_TEST(terrain_revision3_hole_closes_solid_rim_and_restores) {
  resources::TerrainRecipe r;r.cells=16;r.height=0;r.solidBase=true;
  resources::TerrainMesh initial,hole,restored;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,initial,error),error.c_str());
  r.strokes={{resources::TerrainTool::Hole,0,0,0,14,1,0}};
  AE_EXPECT_TRUE(resources::generateTerrain(r,hole,error),error.c_str());
  AE_EXPECT_TRUE(digest(hole)!=digest(initial),"hole removes roof and floor cells");
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
  AE_EXPECT_EQ(digest(restored),digest(initial),"restoring fills hole and removes its interior walls");
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
    auto hash=digest(mesh);
    AE_EXPECT_TRUE(std::find(signatures.begin(),signatures.end(),hash)==signatures.end(),"each volume shape changes the actual cavity");
    signatures.push_back(hash);
  }
  r.strokes[0].shape=resources::TerrainBrushShape::Circle;
  r.strokes[0].hardness=1;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_TRUE(digest(mesh)!=signatures[0],"hardness changes voxel influence rather than only its cursor");
  r.strokes={{resources::TerrainTool::Raise,0,0,0,20,3,0}};
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  auto original=digest(mesh);
  r.strokes.push_back({resources::TerrainTool::Restore,0,-100,0,1,1,0});
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_EQ(digest(mesh),original,"restore outside volume does not reset the entire height column");
}

AE_TEST(terrain_revision3_blended_weights_and_material_bindings_survive_import) {
  resources::TerrainRecipe r;r.cells=16;r.height=0;
  r.strokes={{resources::TerrainTool::Paint,0,4,0,20,.5f,3}};
  resources::TerrainMesh mesh;std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  bool blended=false,unpainted=false;
  for(const auto &part:mesh.parts)for(const auto &v:part.vertices) {
    float sum=0;for(float w:v.weights){AE_EXPECT_TRUE(w>=0&&w<=1,"finite normalized channel");sum+=w;}
    AE_EXPECT_TRUE(std::abs(sum-1)<1e-5f,"splat weights preserve energy");
    blended|=v.weights[3]>0&&v.weights[3]<.51f;unpainted|=v.weights[3]==0;
  }
  AE_EXPECT_TRUE(blended&&unpainted,"soft painting has mixed border and untouched exterior");
  const std::array<std::vector<u8>,4> textures{{
    {std::begin(resources::terrainTexture0),std::end(resources::terrainTexture0)},
    {std::begin(resources::terrainTexture1),std::end(resources::terrainTexture1)},
    {std::begin(resources::terrainTexture2),std::end(resources::terrainTexture2)},
    {std::begin(resources::terrainTexture3),std::end(resources::terrainTexture3)}}};
  std::vector<u8> bytes;resources::GltfImport imported;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,textures,bytes,error),error.c_str());
  AE_EXPECT_TRUE(resources::importGlb(bytes,{},{},imported),imported.diagnostic.c_str());
  for(const auto &draw:imported.draws) {
    const auto &material=imported.materials[draw.materialIndex];
    AE_EXPECT_TRUE(material.flags&renderer::MapMaterialTerrain,"terrain shader contract survives GLB loader");
    AE_EXPECT_TRUE(!(material.flags&renderer::MapMaterialNormalMap),"rock albedo is not interpreted as a normal map");
  }
  AE_EXPECT_TRUE(imported.textures.size()>=4,"all four layers decoded");
  for(const auto &texture:imported.textures)AE_EXPECT_TRUE(texture&&texture->srgb,"all terrain layer bindings decode as sRGB albedo");
}
AE_TEST(terrain_sculpt_paint_and_dig_change_geometry) {
  resources::TerrainRecipe r;
  r.cells = 16;
  resources::TerrainMesh a, b;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrain(r, a, error), error.c_str());
  r.strokes.push_back({resources::TerrainTool::Raise, 0, 0, 0, 20, 5, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  AE_EXPECT_TRUE(digest(a) != digest(b), "raise changes geometry");
  r.strokes.push_back({resources::TerrainTool::Paint, 0, 0, 0, 100, 1, 3});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  for (const auto &p : b.parts)
    AE_EXPECT_EQ(p.layer, 3u, "painting applied to material");
  r.strokes.clear();
  r.mode = resources::TerrainMode::SmoothVoxel;
  AE_EXPECT_TRUE(resources::generateTerrain(r, a, error), error.c_str());
  r.strokes.push_back({resources::TerrainTool::Dig, 0, 8, 0, 8, 1, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, b, error), error.c_str());
  AE_EXPECT_TRUE(digest(a) != digest(b), "dig changes volume");
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
AE_TEST(terrain_editor_revision_undo_redo_reopen_and_collision) {
  TerrainFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  resources::TerrainRecipe r;
  r.cells = 16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(r, error), error.c_str());
  auto root = f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root != 0, "terrain created");
  const auto count = f.session.document().entityCount();
  const auto source =
      scene::importLink(f.session.document().find(root)->components)->source;
  auto children = f.session.document().childrenOf(root);
  AE_EXPECT_TRUE(!children.empty(), "chunks exist");
  for (auto id : children) {
    const auto *c = static_cast<const scene::Collider *>(
        f.session.document().find(id)->components.find(
            scene::Collider::descriptor));
    AE_EXPECT_TRUE(c && c->shape == scene::ColliderShape::Mesh && !c->convex,
                   "real mesh collider");
  }
  const auto persistentChunk = children.front();
  auto customized = *f.session.document().find(persistentChunk);
  auto *renderer = static_cast<scene::MeshRenderer *>(
      customized.components.edit(scene::MeshRenderer::descriptor));
  renderer->materialAsset = {123, 456};
  f.session.history().begin("Customize chunk");
  AE_EXPECT_TRUE(f.session.history().applyValues(f.session.document(),
                                                 persistentChunk, customized),
                 "customization");
  f.session.history().end();
  r.strokes.push_back({resources::TerrainTool::Raise, 0, 0, 0, 10, 4, 0});
  AE_EXPECT_TRUE(f.session.applyTerrain(r, error), error.c_str());
  AE_EXPECT_EQ(root, f.session.screen().terrainTarget, "stable root identity");
  const auto *preserved = f.session.document().find(persistentChunk);
  AE_EXPECT_TRUE(preserved != nullptr, "existing chunk identity preserved");
  AE_EXPECT_EQ(static_cast<const scene::MeshRenderer *>(
                   preserved->components.find(scene::MeshRenderer::descriptor))
                   ->materialAsset,
               (resources::AssetGuid{123, 456}), "material override preserved");
  const auto second =
      scene::importLink(f.session.document().find(root)->components)->source;
  AE_EXPECT_TRUE(second != source, "immutable revision");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),
                 "undo revision");
  AE_EXPECT_EQ(
      scene::importLink(f.session.document().find(root)->components)->source,
      source, "old source restored");
  AE_EXPECT_EQ(f.session.document().entityCount(), count,
               "old children restored");
  AE_EXPECT_TRUE(f.session.openTerrain(root), "recipe after undo");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.strokes.empty(),
                 "old recipe restored");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),
                 "redo revision");
  AE_EXPECT_TRUE(f.session.openTerrain(root), "recipe after redo");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(), usize(1),
               "redo recipe restored");
  auto saved = f.path / "scene.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(), 0), "save scene");
  AE_EXPECT_TRUE(f.session.load(saved.string().c_str(), 0), "reopen scene");
  AE_EXPECT_TRUE(f.session.openTerrain(root), "reopen terrain");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(), usize(1),
               "reopened recipe");
  editor::EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(f.session.document(), f.session.mapScene()),
                 "start runtime with terrain");
  AE_EXPECT_TRUE(play.physics().bodyCount() > 0, "physics bodies created");
  runtime::QueryHit hit;
  const float origin[]{0, 100, 0}, direction[]{0, -200, 0};
  AE_EXPECT_TRUE(play.physics().rayCast(origin, direction, {}, hit),
                 "Jolt ray hits terrain");
  play.stop();
  TerrainFixture cold;
  AE_EXPECT_TRUE(cold.session.setProjectDirectory(f.path.string().c_str()),
                 "fresh session opens project");
  std::vector<u8> registry;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(
                     f.path / ".astra/assets.astra", registry),
                 "registry persisted");
  AE_EXPECT_TRUE(
      cold.session.loadAssets(
          {reinterpret_cast<const char *>(registry.data()), registry.size()}),
      "shell loads registry");
  std::vector<editor::EditorSession::ReopenedSource> sources;
  for (const auto &entry :
       std::filesystem::directory_iterator(f.path / "Terrenos")) {
    if (entry.path().extension() != ".glb")
      continue;
    std::vector<u8> bytes;
    resources::GltfImport model;
    AE_EXPECT_TRUE(editor::EditorImportTransaction::read(entry.path(), bytes) &&
                       resources::importGlb(bytes, {}, {}, model),
                   "read persisted model");
    sources.push_back(
        {std::move(model), Sha256::hex(bytes),
         entry.path().lexically_relative(f.path).generic_string()});
  }
  std::vector<editor::EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(cold.session.reopenSources(sources, reports, error),
                 error.c_str());
  AE_EXPECT_TRUE(cold.session.load(saved.string().c_str(), 0),
                 "fresh session loads scene");
  AE_EXPECT_TRUE(cold.session.openTerrain(root),
                 "fresh session recovers terrain recipe");
  AE_EXPECT_EQ(cold.session.screen().terrainDraft.strokes.size(), usize(1),
               "edits persist across sessions");
  AE_EXPECT_TRUE(play.start(cold.session.document(), cold.session.mapScene()),
                 "cold runtime");
  AE_EXPECT_TRUE(play.physics().rayCast(origin, direction, {}, hit),
                 "cold terrain collision");
  play.stop();
}
AE_TEST(terrain_editor_publication_failure_preserves_scene) {
  TerrainFixture f;
  std::string error;
  resources::TerrainRecipe r;
  r.cells = 16;
  AE_EXPECT_TRUE(f.session.applyTerrain(r, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto source =
      scene::importLink(f.session.document().find(root)->components)->source;
  const auto count = f.session.document().entityCount();
  f.refuse = true;
  ++r.seed;
  AE_EXPECT_TRUE(!f.session.applyTerrain(r, error), "GPU rejection propagated");
  AE_EXPECT_EQ(f.session.document().entityCount(), count, "scene preserved");
  AE_EXPECT_EQ(
      scene::importLink(f.session.document().find(root)->components)->source,
      source, "old resource preserved");
}

AE_TEST(terrain_native_ui_controls_and_capture) {
  TerrainFixture f;
  AE_EXPECT_TRUE(f.ready, "project");
  std::vector<u8> fontBytes, iconBytes;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(
                     "assets/astra-visual/ui/astra-ui-font.aeuf", fontBytes),
                 "font");
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(
                     "assets/astra-visual/ui/astra-ui-icons.aeui", iconBytes),
                 "icons");
  ui::UiFont font;
  ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(font.load(fontBytes) && icons.load(iconBytes),
                 "assets decode");
  f.session.initialize(&font, &icons);
  std::filesystem::create_directories("build/terrain-evidence");
  for (u32 width : {1280u, 655u})
    for (u32 tab = 0; tab < 4; ++tab) {
      const u32 height = width == 1280 ? 720 : 300;
      f.session.setSurface({0, 0, float(width), float(height)}, {});
      f.session.update();
      auto state = f.session.screen();
      state.terrainOpen = true;
      state.terrainTab = tab;
      state.terrainPage = 0;
      ui::UiDrawList list;
      list.begin(state.surface, font.metrics(ui::UiFontWeight::Regular));
      ui::UiInputRouter router;
      router.beginFrame();
      editor::buildEditorScreen(state, editor::editorTheme(), list, router);
      std::vector<ui::UiInstance> instances;
      const auto built =
          ui::buildUiInstances(list, font, icons, 16384, instances);
      AE_EXPECT_EQ(built.dropped, 0u, "no draw loss");
      AE_EXPECT_EQ(built.missingGlyphRuns, 0u, "all glyphs present");
      bool foundGenerate = false;
      for (float y = 0; y < float(height); y += 4)
        for (float x = 0; x < float(width); x += 4)
          if (router.hitTest({x, y}).widgetId ==
              editor::widgetId(editor::EditorWidget::TerrainGenerate))
            foundGenerate = true;
      AE_EXPECT_TRUE(foundGenerate, "generate remains touch reachable");
      test::UiSoftwareTarget target;
      target.resize(width, height, .16f, .18f, .2f);
      test::rasterizeUi(instances, font, icons, target);
      std::ofstream file("build/terrain-evidence/ui-" + std::to_string(width) +
                             "-" + std::to_string(tab) + ".ppm",
                         std::ios::binary);
      file << "P6\n" << width << ' ' << height << "\n255\n";
      for (usize i = 0; i < target.pixels.size(); i += 4)
        for (u32 c = 0; c < 3; ++c) {
          const char v =
              char(u8(std::clamp(target.pixels[i + c], 0.f, 1.f) * 255));
          file.write(&v, 1);
        }
      AE_EXPECT_TRUE(bool(file), "capture saved");
    }
  AE_EXPECT_TRUE(f.session.setTerrainNumber(0, 777), "seed editable");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.seed, 777u, "seed applied");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(2, 17),
                 "invalid resolution rejected");
  f.session.update();
  ui::UiDrawList interactive;
  interactive.begin(f.session.screen().surface,
                    font.metrics(ui::UiFontWeight::Regular));
  ui::UiInputRouter routes;
  routes.beginFrame();
  editor::buildEditorScreen(f.session.screen(), editor::editorTheme(),
                            interactive, routes);
  bool touched = false;
  for (float y = 2; y < 300 && !touched; y += 4)
    for (float x = 2; x < 655 && !touched; x += 4)
      if (routes.hitTest({x, y}).widgetId ==
          editor::widgetId(editor::EditorWidget::TerrainGenerate)) {
        f.session.handlePointer({9, ui::UiPointerPhase::Down, {x, y}, 1});
        f.session.handlePointer({9, ui::UiPointerPhase::Up, {x, y}, 1.1});
        touched = true;
      }
  AE_EXPECT_TRUE(touched && f.session.screen().terrainBusy,
                 "touch dispatch starts real generation");
  for (u32 i = 0; i < 5000 && f.session.screen().terrainBusy; ++i) {
    f.session.refreshTerrainJob();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy &&
                     f.session.screen().terrainTarget != 0,
                 "touch generated terrain in scene");
}

AE_TEST(terrain_async_cancel_and_stale_scene_do_not_publish) {
  TerrainFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture");
  const auto count = f.session.document().entityCount();
  f.session.startTerrainJob();
  ui::UiPointerRouting click;
  click.tapped = true;
  click.widgetId = editor::widgetId(editor::EditorWidget::TerrainCancel);
  f.session.handleTerrainInput({}, click);
  for (u32 i = 0; i < 5000 && f.session.screen().terrainBusy; ++i) {
    f.session.refreshTerrainJob();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy, "cancel completes");
  AE_EXPECT_EQ(f.session.document().entityCount(), count,
               "cancel leaves scene intact");
  f.session.startTerrainJob();
  f.session.document().createEntity(f.session.document().root(),
                                    editor::EditorEntityKind::Folder,
                                    "Concurrent edit");
  for (u32 i = 0; i < 5000 && f.session.screen().terrainBusy; ++i) {
    f.session.refreshTerrainJob();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy, "job completes");
  AE_EXPECT_EQ(f.session.document().entityCount(), count + 1,
               "stale result discarded");
  AE_EXPECT_EQ(f.session.screen().terrainTarget, 0u, "no terrain applied");
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
  auto before = digest(mesh);
  r.strokes.push_back({resources::TerrainTool::Dig, 0, 4, 0, 8, 0, 0});
  AE_EXPECT_TRUE(resources::generateTerrain(r, mesh, error), error.c_str());
  AE_EXPECT_EQ(digest(mesh), before, "zero opacity dig is a no-op");
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

AE_TEST(terrain_solid_recipe_migrates_legacy_and_roundtrips_glb) {
  resources::TerrainRecipe r;
  r.solidBase = true;
  r.baseDepth = 19;
  std::string error;
  const std::string legacy =
      "AETERRAIN1 0 1337 32 24 4 64 16 4 3 4 0 1 1 0 0 0 0 4 1 0";
  AE_EXPECT_TRUE(resources::deserializeTerrainRecipe(legacy, r, error),
                 error.c_str());
  AE_EXPECT_TRUE(!r.solidBase, "legacy geometry stays an open surface");
  AE_EXPECT_EQ(r.baseDepth, 8.f, "legacy recipe receives unused depth default");
  AE_EXPECT_EQ(r.strokes.size(), usize(1), "legacy edits are preserved");
  resources::TerrainMesh surface, solid;
  AE_EXPECT_TRUE(resources::generateTerrain(r, surface, error), error.c_str());
  AE_EXPECT_EQ(surface.triangles, 2 * r.cells * r.cells,
               "legacy remesh does not acquire a base");
  r.solidBase = true;
  r.baseDepth = 12.5f;
  AE_EXPECT_TRUE(resources::generateTerrain(r, solid, error), error.c_str());
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r, solid, {}, bytes, error),
                 error.c_str());
  resources::GltfImport imported;
  AE_EXPECT_TRUE(resources::importGlb(bytes, {}, {}, imported),
                 imported.diagnostic.c_str());
  resources::TerrainRecipe loaded;
  AE_EXPECT_TRUE(resources::terrainRecipeFromGlb(bytes, loaded, error),
                 error.c_str());
  AE_EXPECT_TRUE(loaded.solidBase, "GLB carries the solid shape option");
  AE_EXPECT_EQ(loaded.baseDepth, 12.5f, "GLB carries the thickness");
  resources::TerrainMesh regenerated;
  AE_EXPECT_TRUE(resources::generateTerrain(loaded, regenerated, error),
                 error.c_str());
  AE_EXPECT_EQ(digest(solid), digest(regenerated), "solid reopen is deterministic");
  const auto before = resources::serializeTerrainRecipe(loaded);
  AE_EXPECT_TRUE(!resources::deserializeTerrainRecipe(
                     "AETERRAIN2 0 1337 32 24 4 64 16 4 3 4 0 1 1", loaded,
                     error),
                 "truncated new fields are rejected");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded), before,
               "invalid recipe leaves prior state untouched");
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

AE_TEST(terrain_solid_editor_reopen_and_side_bottom_collision) {
  TerrainFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  resources::TerrainRecipe r;
  r.cells = 32;
  r.height = 0;
  r.base = 4;
  r.solidBase = true;
  r.baseDepth = 8;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(r, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto saved = f.path / "solid.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(), 0), "save solid scene");
  AE_EXPECT_TRUE(f.session.load(saved.string().c_str(), 0), "reload solid scene");
  AE_EXPECT_TRUE(f.session.openTerrain(root), "reopen solid properties");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.solidBase,
                 "solid setting survives save and load");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.baseDepth, 8.f,
               "thickness survives save and load");
  editor::EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(f.session.document(), f.session.mapScene()),
                 "runtime accepts solid mesh");
  runtime::QueryHit hit;
  const float sideOrigin[]{100, 0, 10}, sideDirection[]{-200, 0, 0};
  AE_EXPECT_TRUE(play.physics().rayCast(sideOrigin, sideDirection, {}, hit),
                 "Jolt collides with the outside wall");
  const float bottomOrigin[]{10, -100, 10}, bottomDirection[]{0, 200, 0};
  AE_EXPECT_TRUE(play.physics().rayCast(bottomOrigin, bottomDirection, {}, hit),
                 "Jolt collides with the underside");
  play.stop();
}
