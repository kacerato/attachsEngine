#include "editor/editor_import_transaction.h"
#include "editor/editor_session.h"
#include "editor/terrain_material_icons.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "scene/import_link.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace ae;
namespace {
struct MaterialEditorFixture {
  editor::EditorSession session;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  std::vector<u8> vertices, fontData, iconData;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::vector<renderer::TerrainSurfaceBinding> surfaces;
  std::filesystem::path project =
      std::filesystem::temp_directory_path() /
      ("astra-material-editor-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::shared_ptr<std::atomic<unsigned>> imageReads =
      std::make_shared<std::atomic<unsigned>>(0);
  bool ready = false, ownsProject=true;
  explicit MaterialEditorFixture(const std::filesystem::path &existing={}) {
    if(!existing.empty()){project=existing;ownsProject=false;}
    std::filesystem::create_directories(project);
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices,
                                         indices, draws, materials);
    session.importMap(draws, materials, false, vertices, indices, 0);
    session.setGeometryPublisher(
        [this](std::span<const u8> v, std::span<const u32> i,
               std::span<const renderer::MapDrawRecord> d,
               std::span<const renderer::MapMaterialRecord> m,
               std::span<const renderer::SharedAuthoringTexture>,
               editor::EditorSession::PublishedGeometry &out) {
          const auto in = session.terrainSurfacePublication();
          surfaces.assign(in.begin(), in.end());
          vertices.clear();
          indices.clear();
          draws.clear();
          materials.clear();
          renderer::appendBoxAuthoringGeometry(
              renderer::MapVertexStride, vertices, indices, draws, materials);
          const auto vb = u32(vertices.size() / renderer::MapVertexStride),
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
    const auto library =
        std::filesystem::current_path() / "assets/terrain/library";
    session.setTerrainLibraryReader(
        [library, count = imageReads](std::string_view relative,
                                      std::vector<u8> &out) {
          std::filesystem::path path;
          if (relative.ends_with(".png"))
            ++*count;
          return editor::EditorImportTransaction::safePath(
                     library, std::string(relative), path) &&
                 editor::EditorImportTransaction::read(path, out, 64u << 20);
        });
    auto &f=fontData;auto &i=iconData;
    ready = editor::EditorImportTransaction::read(
                "assets/astra-visual/ui/astra-ui-font.aeuf", f) &&
            editor::EditorImportTransaction::read(
                "assets/astra-visual/ui/astra-ui-icons.aeui", i) &&
            font.load(f) && icons.load(i);
    session.initialize(&font, &icons);
    ready = ready && session.setProjectDirectory(project.string().c_str()) &&
            session.openTerrain();
  }
  ~MaterialEditorFixture() {
    session.clearTerrainPreview(false);
    std::error_code ec;
    if(ownsProject)std::filesystem::remove_all(project, ec);
  }
  const renderer::TerrainSurfaceBinding *visibleSurface(editor::EditorEntityId root) {
    const auto *entity=session.document().find(root);
    const auto *link=entity?scene::importLink(entity->components):nullptr;
    if(!link)return nullptr;
    const auto currentSource=link->source;
    std::vector<renderer::MapDrawState> visible;
    if(!session.extractMap(visible))return nullptr;
    for(const auto &draw:visible) {
      const auto *object=session.document().find(draw.objectId);
      const auto *source=object?scene::importLink(object->components):nullptr;
      if(!draw.visible || !source || source->source!=currentSource || draw.sourceDrawIndex>=draws.size())continue;
      const auto material=draws[draw.sourceDrawIndex].materialIndex;
      // The fixture prepends one package box/material to the published library.
      if(material && material-1<surfaces.size())return &surfaces[material-1];
    }
    return nullptr;
  }
  bool finishPreview() {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(15);
    do {
      session.refreshTerrainPreview();
      if (!session.terrainPreviewJob_)
        return session.screen().terrainLivePreview;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  }
};
} // namespace
AE_TEST(
    terrain_catalog_editor_selection_apply_preview_history_and_selfcontained_reopen) {
  MaterialEditorFixture f;
  AE_EXPECT_TRUE(f.ready, "material editor fixture");
  resources::TerrainRecipe legacy;
  legacy.cells = 8;
  legacy.height = 0;
  legacy.controlCells = 8;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(legacy, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.session.loadTerrainCatalog() &&
                     f.session.screen().terrainCatalog->materials.size() == 82,
                 "real packaged catalog loaded");
  const auto catalog = f.session.screen().terrainCatalog;
  for(const auto &material:catalog->materials) {
    const auto icon=editor::terrainMaterialIcon(material.id);
    AE_EXPECT_TRUE(icon!=ui::UiIcon::AssetsMaterial,"every supplied material has an actual atlas thumbnail");
    const auto rect=f.icons.rectOf(icon);const auto pixels=f.icons.pixels();
    AE_EXPECT_TRUE(rect.width==96 && rect.height==96,"material preview rect exists in loaded atlas");
    for(u32 y=u32(rect.y);y<u32(rect.bottom());++y)for(u32 x=u32(rect.x);x<u32(rect.right());++x)
      AE_EXPECT_EQ(pixels[(usize(y)*f.icons.width()+x)*4+3],u8(255),
                   "packed albedo alpha is data; UI previews remain opaque");
  }
  AE_EXPECT_TRUE(f.session.selectTerrainCatalogMaterial(0),
                 f.session.screen().terrainMessage.c_str());
  auto selected = f.session.screen().terrainDraft;
  AE_EXPECT_EQ(selected.surface.layers.size(), usize(4),
               "first replacement migrates all four legacy slots");
  AE_EXPECT_EQ(selected.surface.layers[0].sourceCatalogId,
               catalog->materials[0].id,
               "selected original catalog provenance");
  AE_EXPECT_TRUE(!selected.surface.layers[0].sourcePropertiesJson.empty(),
                 "source properties survive selection");
  AE_EXPECT_EQ(selected.surface.layers[1].name, "Rocha",
               "other original slot remains unchanged");
  const auto oldRock = selected.surface.layers[1].maps[0].texture;
  AE_EXPECT_TRUE(f.session.setTerrainNumber(64, 7.5) &&
                     f.session.setTerrainNumber(72, .65),
                 "real layer property setters");
  selected = f.session.screen().terrainDraft;
  AE_EXPECT_TRUE(f.session.applyTerrain(selected, error), error.c_str());
  AE_EXPECT_TRUE(f.session.terrainMaterialCache_ &&
                     !f.session.terrainMaterialCache_->images.empty(),
                 "full original PNGs retained immutably");
  const auto cache = f.session.terrainMaterialCache_;
  const auto reads = f.imageReads->load();
  AE_EXPECT_TRUE(f.session.setTerrainNumber(64, 9.5), "slider changes layer scale");
  auto changed=editor::prepareTerrainMaterial(f.session.screen().terrainDraft,catalog,{},cache,error);
  AE_EXPECT_TRUE(changed && changed->binding.surface.layers[0].scaleX==9.5f,
                 "new authoring value reaches binding without image reads");
  AE_EXPECT_TRUE(changed->textures.size()==cache->textures.size() && !changed->textures.empty() &&
                 changed->textures[0].get()==cache->textures[0].get(),
                 "material sliders reuse immutable decoded full-resolution pixels");
  AE_EXPECT_EQ(f.imageReads->load(), reads, "property edit does not reload source PNG");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(64, 7.5), "restore scale for history scenario");
  const auto *visible=f.visibleSurface(root);
  AE_EXPECT_TRUE(visible && visible->surface.layers.size()==4,
                 "actual visible revision resolves four PBR layers");
  AE_EXPECT_EQ(visible->surface.layers[0].scaleX, 7.5f,
               "visible runtime binding receives edited scale");
  AE_EXPECT_EQ(visible->surface.layers[1].maps[0].texture, oldRock,
               "legacy texture identity survives publication");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(3, 1),
                 "geometry-only draft change");
  f.session.queueTerrainPreview();
  AE_EXPECT_TRUE(f.finishPreview(), f.session.screen().terrainMessage.c_str());
  AE_EXPECT_TRUE(f.session.terrainMaterialCache_ == cache &&
                     f.imageReads->load() == reads,
                 "brush/geometry previews reuse immutable decoded textures "
                 "without file rereads");
  AE_EXPECT_TRUE(f.session.applyTerrain(f.session.screen().terrainDraft, error),
                 error.c_str());
  const auto source =
      scene::importLink(f.session.document().find(root)->components)->source;
  const auto record = *f.session.assets().find(source);
  std::vector<u8> glb;
  AE_EXPECT_TRUE(
      editor::EditorImportTransaction::read(f.project / record.path, glb),
      "read committed selfcontained source");
  resources::GltfImport imported;
  AE_EXPECT_TRUE(resources::importGlb(glb, {}, {}, imported),
                 imported.diagnostic.c_str());
  AE_EXPECT_TRUE(!imported.terrainSurfaceImages.empty(),
                 "original images embedded in saved GLB");
  for (const auto &image : cache->images) {
    const auto found = std::find_if(
        imported.terrainSurfaceImages.begin(),
        imported.terrainSurfaceImages.end(),
        [&](const auto &entry) { return entry.identity == image.identity; });
    AE_EXPECT_TRUE(found != imported.terrainSurfaceImages.end() &&
                       Sha256::hex(found->bytes()) ==
                           Sha256::hex(image.bytes()),
                   "every original image and GUID survives export/import");
  }
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),
                 "material terrain revision undo");
  f.session.refreshTerrainJob();
  AE_EXPECT_EQ(f.session.screen().terrainDraft.height, 0.f,
               "undo restores previous geometry and material recipe");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].scaleX, 7.5f,
               "undo retains recorded layer properties");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),
                 "redo material terrain revision");
  f.session.refreshTerrainJob();
  f.session.setTerrainLibraryReader(
      [](std::string_view, std::vector<u8> &) { return false; });
  const auto saved=f.project/"material-source.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(),0) && f.session.load(saved.string().c_str(),0),
                 "cold scene/cache reload without bundled library");
  AE_EXPECT_TRUE(f.session.openTerrain(root),
                 "reopen independent of bundled catalog");
  const auto reopened = f.session.screen().terrainDraft;
  AE_EXPECT_TRUE(f.session.applyTerrain(reopened, error), error.c_str());
  AE_EXPECT_EQ(
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
      resources::serializeTerrainRecipe(reopened),
      "selfcontained reopen preserves channels and provenance");
  const auto *coldVisible=f.visibleSurface(root);
  AE_EXPECT_TRUE(coldVisible && coldVisible->surface.layers.size()==4 &&
                 coldVisible->surface.layers[0].scaleX==7.5f &&
                 coldVisible->surface.layers[0].sourceCatalogId==catalog->materials[0].id,
                 "cold reload publishes edited PBR on the visible revision");
  MaterialEditorFixture fresh(f.project);
  AE_EXPECT_TRUE(fresh.ready,"new session initializes project");
  std::vector<u8> registry;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(f.project/".astra/assets.astra",registry) &&
                 fresh.session.loadAssets(std::string(registry.begin(),registry.end())),"production project registry restore");
  std::vector<editor::EditorSession::ReopenedSource> restored;
  for(const auto &record:fresh.session.assets().records())if(record.type==resources::AssetType::Mesh && !record.source.empty()) {
    std::vector<u8> source;
    AE_EXPECT_TRUE(editor::EditorImportTransaction::read(f.project/record.source,source),"read persisted source");
    editor::EditorSession::ReopenedSource item;item.sourceName=record.source;item.hash=Sha256::hex(source);
    const auto limits=fresh.session.importLimitsFor(fresh.session.importProfileFor(record.guid));
    AE_EXPECT_TRUE(resources::importGlb(source,limits,{},item.model),item.model.diagnostic.c_str());
    restored.push_back(std::move(item));
  }
  std::vector<editor::EditorSession::ModelImportReport> reports;
  AE_EXPECT_TRUE(!restored.empty() && fresh.session.reopenSources(restored,reports,error),error.c_str());
  fresh.session.setTerrainLibraryReader([](std::string_view,std::vector<u8>&){return false;});
  AE_EXPECT_TRUE(fresh.session.load(saved.string().c_str(),0) && fresh.session.openTerrain(root),
                 "fresh editor session reopens authored PBR with external catalog unavailable");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(fresh.session.screen().terrainDraft),
               resources::serializeTerrainRecipe(reopened),"fresh session recovers exact material recipe");
  const auto *freshVisible=fresh.visibleSurface(root);
  AE_EXPECT_TRUE(freshVisible && freshVisible->surface.layers.size()==4 &&
                 freshVisible->surface.layers[0].scaleX==7.5f,
                 "new session publishes restored PBR on visible draw");
  AE_EXPECT_TRUE(fresh.session.applyTerrain(fresh.session.screen().terrainDraft,error),error.c_str());
}
AE_TEST(terrain_catalog_failed_image_hash_preserves_applied_source_and_draft) {
  MaterialEditorFixture f;
  AE_EXPECT_TRUE(f.ready, "material editor fixture");
  resources::TerrainRecipe legacy;
  legacy.cells = 8;
  legacy.controlCells = 8;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(legacy, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto oldSource =
      scene::importLink(f.session.document().find(root)->components)->source;
  std::vector<u8> catalogBytes;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(
                     "assets/terrain/library/catalog.json", catalogBytes),
                 "catalog source");
  f.session.setTerrainLibraryReader([catalogBytes](std::string_view relative,
                                                   std::vector<u8> &out) {
    out = relative == "catalog.json" ? catalogBytes : std::vector<u8>{0, 1, 2};
    return true;
  });
  AE_EXPECT_TRUE(f.session.selectTerrainCatalogMaterial(0),
                 "draft selection itself does not publish");
  const auto draft =
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(
      !f.session.applyTerrain(f.session.screen().terrainDraft, error),
      "wrong original image checksum blocks publication");
  AE_EXPECT_EQ(
      scene::importLink(f.session.document().find(root)->components)->source,
      oldSource, "failed image validation retains original source");
  AE_EXPECT_EQ(
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft), draft,
      "failed publication preserves selected authoring draft");
}

AE_TEST(terrain_catalog_packed_material_maps_reach_publication_at_original_resolution) {
  MaterialEditorFixture f;
  AE_EXPECT_TRUE(f.ready, "material fixture");
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=8;recipe.height=0;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  AE_EXPECT_TRUE(f.session.loadTerrainCatalog(),"catalog load");
  const auto catalog=f.session.screen().terrainCatalog;
  usize selected=catalog->materials.size();
  for(usize i=0;i<catalog->materials.size();++i) {
    const auto &maps=catalog->materials[i].layer.maps;
    if(maps[0].texture.valid() && maps[1].texture.valid() && maps[2].texture.valid() &&
       maps[3].texture.valid() && maps[4].texture.valid()) {selected=i;break;}
  }
  AE_EXPECT_TRUE(selected<catalog->materials.size(),"supplied packed PBR material exists");
  AE_EXPECT_TRUE(f.session.selectTerrainCatalogMaterial(u32(selected)),"select original PBR material");
  AE_EXPECT_TRUE(f.session.applyTerrain(f.session.screen().terrainDraft,error),error.c_str());
  const auto *published=f.visibleSurface(f.session.screen().terrainTarget);
  AE_EXPECT_TRUE(published && !published->surface.layers.empty(),"actual visible PBR publication");
  const auto &binding=*published;
  const auto &layer=binding.surface.layers[0];
  for(u32 role: {0u,1u,2u,3u,4u}) {
    const auto &map=layer.maps[role];
    AE_EXPECT_TRUE(std::any_of(binding.textures.begin(),binding.textures.end(),
      [&](const auto &entry){return entry.identity==map.texture;}),"every real PBR map resolves to a texture");
  }
  const auto cache=f.session.terrainMaterialCache_;
  AE_EXPECT_TRUE(cache && !cache->textures.empty(),"material cache");
  for(const auto &entry:cache->binding.textures) {
    const auto original=std::find_if(catalog->textures.begin(),catalog->textures.end(),
      [&](const auto &image){return image.identity==entry.identity;});
    if(original==catalog->textures.end())continue;
    AE_EXPECT_TRUE(entry.textureIndex<cache->textures.size(),"decoded view index");
    const auto &texture=cache->textures[entry.textureIndex];
    AE_EXPECT_TRUE(texture && texture->width==original->width && texture->height==original->height && texture->levels>1,
                   "decoded dimensions match supplied original exactly");
  }
}

AE_TEST(terrain_material_property_undo_redo_changes_actual_visible_binding) {
  MaterialEditorFixture f;AE_EXPECT_TRUE(f.ready,"material fixture");
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.controlCells=8;recipe.height=0;
  std::string error;AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.session.selectTerrainCatalogMaterial(0) && f.session.setTerrainNumber(64,2.5),"select original material and scale");
  AE_EXPECT_TRUE(f.session.applyTerrain(f.session.screen().terrainDraft,error),error.c_str());
  AE_EXPECT_TRUE(f.session.setTerrainNumber(64,3.5) &&
                 f.session.applyTerrain(f.session.screen().terrainDraft,error),error.c_str());
  auto visible=f.visibleSurface(root);
  AE_EXPECT_TRUE(visible && visible->surface.layers.size()==4 && visible->surface.layers[0].scaleX==3.5f,
                 "applied material scale reaches actual visible binding");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo material property revision");
  f.session.refreshTerrainJob();visible=f.visibleSurface(root);
  AE_EXPECT_TRUE(visible && visible->surface.layers.size()==4 && visible->surface.layers[0].scaleX==2.5f,
                 "undo restores PBR on actual visible draw");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo material property revision");
  f.session.refreshTerrainJob();visible=f.visibleSurface(root);
  AE_EXPECT_TRUE(visible && visible->surface.layers.size()==4 && visible->surface.layers[0].scaleX==3.5f,
                 "redo restores edited PBR on actual visible draw");
}
