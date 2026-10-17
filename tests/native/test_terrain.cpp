#include "terrain_test_helpers.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "renderer/authoring_texture.h"
#include "resources/gltf_import.h"
#include "resources/terrain_asset.h"
#include "resources/terrain_vertex_index.h"
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
using ae::test::terrainDigest;
namespace {

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
    for (u32 tab = 0; tab < 5; ++tab)
    for (u32 section=0;section<(tab==4 || tab==1?3u:1u);++section) {
      const u32 height = width == 1280 ? 720 : 300;
      f.session.setSurface({0, 0, float(width), float(height)}, {});
      f.session.update();
      auto state = f.session.screen();
      state.terrainOpen = true;
      state.terrainTab = tab;
      state.terrainSection = section;
      state.terrainPage = 0;
      if(tab==4){resources::TerrainPath path;path.points={{-12,4,-10,false},{0,8,0,true},{12,12,10,false}};state.terrainDraft.paths={path};}
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
                             "-" + std::to_string(tab) + (section?"-s"+std::to_string(section):"") + ".ppm",
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
  AE_EXPECT_EQ(terrainDigest(solid), terrainDigest(regenerated), "solid reopen is deterministic");
  const auto before = resources::serializeTerrainRecipe(loaded);
  AE_EXPECT_TRUE(!resources::deserializeTerrainRecipe(
                     "AETERRAIN2 0 1337 32 24 4 64 16 4 3 4 0 1 1", loaded,
                     error),
                 "truncated new fields are rejected");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(loaded), before,
               "invalid recipe leaves prior state untouched");
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

AE_TEST(terrain_indexed_export_preserves_triangles_and_supports_locked_border_lods) {
  resources::TerrainRecipe r;r.cells=32;r.height=0;r.lodLevels=3;
  resources::TerrainMesh mesh;std::string error;std::vector<u8> bytes;
  AE_EXPECT_TRUE(resources::generateTerrain(r,mesh,error),error.c_str());
  AE_EXPECT_TRUE(resources::writeTerrainGlb(r,mesh,{},bytes,error),error.c_str());
  // No caller flags: a cold import must recover the authored policy itself.
  resources::GltfImportLimits limits;
  resources::GltfImport model;AE_EXPECT_TRUE(resources::importGlb(bytes,limits,{},model),model.diagnostic.c_str());
  AE_EXPECT_TRUE(model.vertices.size()/renderer::MapVertexStride<mesh.triangles*3/2,"lossless sharing reduces resident vertex data by more than half");
  AE_EXPECT_TRUE(!model.meshLods.empty(),"indexed topology supports actual border-locked LOD reduction");
  for(usize part=0;part<mesh.parts.size();++part) {
    const auto &draw=model.draws[part];
    AE_EXPECT_EQ(draw.indexCount,mesh.parts[part].vertices.size(),"all source triangles retained in level zero");
    for(u32 i=0;i<draw.indexCount;++i) {
      const auto index=draw.vertexOffset+model.indices[draw.firstIndex+i];
      float position[3];std::memcpy(position,model.vertices.data()+index*renderer::MapVertexStride,12);
      for(u32 axis=0;axis<3;++axis)AE_EXPECT_EQ(position[axis],mesh.parts[part].vertices[i].position[axis],"indexed export preserves exact triangle positions");
    }
  }
}
