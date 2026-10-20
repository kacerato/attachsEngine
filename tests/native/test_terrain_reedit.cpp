#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_recovery.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "renderer/material_override.h"
#include "scene/import_link.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

using namespace ae;
namespace {
struct ReeditFixture {
  editor::EditorSession session;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::vector<renderer::SharedAuthoringTexture> textures;
  std::vector<renderer::MeshLodLevel> publishedLods;
  std::vector<renderer::TerrainControlBinding> controls;
  std::filesystem::path path;
  bool ready = false;
  ReeditFixture() {
    path = std::filesystem::temp_directory_path() /
           ("astra-terrain-reedit-" + std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials);
    session.importMap(draws, materials, false, vertices, indices, 0);
    session.setGeometryPublisher(
        [this](std::span<const u8> v, std::span<const u32> i,
               std::span<const renderer::MapDrawRecord> d,
               std::span<const renderer::MapMaterialRecord> m,
               std::span<const renderer::SharedAuthoringTexture> t,
               editor::EditorSession::PublishedGeometry &out) {
          const auto lods=session.skinningPublication().meshLods;
          publishedLods.assign(lods.begin(),lods.end());
          const auto controlsIn=session.terrainControlPublication();
          controls.assign(controlsIn.begin(),controlsIn.end());
          textures.assign(t.begin(),t.end());
          vertices.clear(); indices.clear(); draws.clear(); materials.clear();
          renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials);
          const auto vb = u32(vertices.size() / renderer::MapVertexStride);
          const auto ib = u32(indices.size()), mb = u32(materials.size());
          vertices.insert(vertices.end(), v.begin(), v.end());
          indices.insert(indices.end(), i.begin(), i.end());
          materials.insert(materials.end(), m.begin(), m.end());
          for (auto draw : d) {
            draw.vertexOffset += vb; draw.firstIndex += ib; draw.materialIndex += mb;
            draw.lodGroupId = u32(draws.size()); draws.push_back(draw);
          }
          out = {draws, materials, vertices, indices};
          return true;
        });
    std::vector<u8> fontBytes, iconBytes;
    ready = editor::EditorImportTransaction::read("assets/astra-visual/ui/astra-ui-font.aeuf", fontBytes) &&
            editor::EditorImportTransaction::read("assets/astra-visual/ui/astra-ui-icons.aeui", iconBytes) &&
            font.load(fontBytes) && icons.load(iconBytes);
    session.initialize(&font, &icons);
    ready = ready && session.setProjectDirectory(path.string().c_str()) && session.openTerrain();
  }
  ~ReeditFixture() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  void action(editor::EditorWidget widget) {
    ui::UiPointerRouting routing;
    routing.tapped = true;
    routing.widgetId = editor::widgetId(widget);
    session.handleTerrainInput({}, routing);
  }
  bool tapInspectorEdit(float width, float height) {
    session.setSurface({0, 0, width, height}, {});
    session.update();
    if (session.layout().inspectorPanel.isEmpty()) {
      if (!tapWidget(editor::widgetId(editor::EditorWidget::CompactPanelMenu)) ||
          !tapWidget(editor::widgetId(editor::EditorWidget::InspectorToggle))) return false;
      session.update();
    }
    ui::UiDrawList list;
    list.begin(session.screen().surface, font.metrics(ui::UiFontWeight::Regular));
    ui::UiInputRouter router;
    router.beginFrame();
    editor::buildEditorScreen(session.screen(), editor::editorTheme(), list, router);
    for (float y = 2; y < height; y += 4)
      for (float x = 2; x < width; x += 4)
        if (router.hitTest({x,y}).widgetId == editor::widgetId(editor::EditorWidget::TerrainEditSelection)) {
          session.handlePointer({11, ui::UiPointerPhase::Down, {x,y}, 1});
          session.handlePointer({11, ui::UiPointerPhase::Up, {x,y}, 1.1});
          return session.screen().terrainOpen;
        }
    return false;
  }
  bool brushPoints(ui::UiPoint &from, ui::UiPoint &to) {
    session.setSurface({0, 0, 1280, 720}, {});
    session.update();
    ui::UiDrawList list;
    list.begin(session.screen().surface, font.metrics(ui::UiFontWeight::Regular));
    ui::UiInputRouter router;
    router.beginFrame();
    editor::buildEditorScreen(session.screen(), editor::editorTheme(), list, router);
    const auto candidates = session.pickCandidates();
    const auto root = session.screen().terrainTarget;
    const auto source = scene::importLink(session.document().find(root)->components)->source;
    bool found = false;
    for (float y = 4; y < 720; y += 12)
      for (float x = 4; x < 1280; x += 12) {
        if (router.hitTest({x,y}).target != ui::UiPointerTarget::Viewport) continue;
        const auto hit = editor::pickNearest(candidates, editor::screenPointToRay(session.view(), {x,y}), 0, true);
        const auto *object = hit.hit ? session.document().find(hit.id) : nullptr;
        const auto *link = object ? scene::importLink(object->components) : nullptr;
        if (!link || link->source != source) continue;
        if (!found) {from = {x,y}; found = true;}
        if ((from.x-x)*(from.x-x)+(from.y-y)*(from.y-y) >= 80*80) {to = {x,y}; return true;}
      }
    return false;
  }
  bool finishGeneration() {
    for (u32 i = 0; i < 5000 && session.screen().terrainBusy; ++i) {
      session.refreshTerrainJob();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !session.screen().terrainBusy;
  }
  bool widgetPoint(u32 widget, ui::UiPoint &point, bool terrainOnly = false) {
    session.update();
    ui::UiDrawList list;
    list.begin(session.screen().surface, font.metrics(ui::UiFontWeight::Regular));
    ui::UiInputRouter router;
    router.beginFrame();
    editor::buildEditorScreen(session.screen(), editor::editorTheme(), list, router);
    const auto terrainPanel = editor::terrainPanelRect(session.screen());
    for (float y = 2; y < session.screen().surface.height; y += 4)
      for (float x = 2; x < session.screen().surface.width; x += 4)
        if ((!terrainOnly || terrainPanel.contains({x,y})) &&
            router.hitTest({x,y}).widgetId == widget) {
          point = {x,y};
          return true;
        }
    return false;
  }
  bool tapWidget(u32 widget, bool terrainOnly = false) {
    ui::UiPoint point;
    if (!widgetPoint(widget, point, terrainOnly)) return false;
    session.handlePointer({12, ui::UiPointerPhase::Down, point, 1});
    session.handlePointer({12, ui::UiPointerPhase::Up, point, 1.1});
    return true;
  }
  bool scrollTerrain(float distance) {
    ui::UiPoint point;
    if (!widgetPoint(editor::widgetId(editor::EditorWidget::TerrainScroll), point, true)) return false;
    const ui::UiPoint end{point.x, point.y-distance};
    session.handlePointer({13, ui::UiPointerPhase::Down, point, 2});
    session.handlePointer({13, ui::UiPointerPhase::Move, end, 2.2});
    session.handlePointer({13, ui::UiPointerPhase::Up, end, 2.3});
    session.update();
    return session.screen().terrainScroll > 0;
  }
};
} // namespace

AE_TEST(terrain_checkpoint_delta_recovers_corrupt_source_and_rebuilds_immutable_revision) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.height=0;recipe.controlCells=16;
  for(u32 i=0;i<20;++i)recipe.strokes.push_back({resources::TerrainTool::Raise,float(i%5)-2,0,float(i/5)-2,4,.1f,0});
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  recipe.strokes.push_back({resources::TerrainTool::Paint,0,0,0,8,.4f,3});
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto source=scene::importLink(f.session.document().find(root)->components)->source;
  const auto record=*f.session.assets().find(source);
  std::filesystem::path objects,reference;
  AE_EXPECT_TRUE(editor::terrainRecoveryPaths(f.path.string(),record.contentHash,objects,reference),"safe recovery paths");
  std::vector<u8> pointer;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(reference,pointer,64),"published revision points to recipe history");
  std::vector<u8> envelope;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(objects/(std::string(pointer.begin(),pointer.end())+".aethist"),envelope),"read committed history object");
  AE_EXPECT_TRUE(envelope.size()>5 && envelope[4]=='D',"similar recipe stored as an exact delta");
  const auto sourcePath=f.path/record.path;
  const std::vector<u8> truncated{0,1,2};
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(sourcePath,truncated),"simulate truncated GLB");
  AE_EXPECT_TRUE(f.session.openTerrain(root),"corrupt source can reopen through validated recipe chain");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),resources::serializeTerrainRecipe(recipe),"recovery preserves every authored value");
  AE_EXPECT_TRUE(f.session.screen().terrainAppliedRecipe.empty(),"recovered terrain offers reconstruction");
  f.session.startTerrainJob();
  AE_EXPECT_TRUE(f.finishGeneration(),"reconstruction completes through production job");
  AE_EXPECT_TRUE(!f.session.screen().terrainAppliedRecipe.empty(),f.session.screen().terrainMessage.c_str());
  std::vector<u8> repaired;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(sourcePath,repaired) && Sha256::hex(repaired)==record.contentHash,"known immutable source file is rebuilt exactly");
  AE_EXPECT_TRUE(f.session.openTerrain(root),"repaired GLB reopens normally");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("recuperada")==std::string::npos,"normal open no longer reports fallback");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.controlCells,16u,"material map resolution recovered with recipe");
}

AE_TEST(terrain_new_canvas_flat_and_soft_sculpt_preserves_saved_relief) {
  const editor::EditorScreenState freshEditor;
  AE_EXPECT_EQ(freshEditor.terrainBrush.radius,8.f,"fresh app uses gentle brush even when reopening an existing terrain");
  AE_EXPECT_EQ(freshEditor.terrainBrush.strength,.25f,"fresh app never restores aggressive legacy strength");
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  const auto blank=f.session.screen().terrainDraft;
  auto brush=f.session.screen().terrainBrush;
  resources::TerrainField field;
  std::string error;
  AE_EXPECT_TRUE(resources::generateTerrainField(blank,field,error),error.c_str());
  for(float height:field.heights)AE_EXPECT_EQ(height,blank.base,"new terrain is exactly flat");
  AE_EXPECT_TRUE(brush.radius/(blank.size/blank.cells)>=8,"soft footprint spans sufficient grid samples");
  auto sculpted=blank;
  brush.x=brush.z=0;
  sculpted.strokes.push_back(brush);
  AE_EXPECT_TRUE(resources::generateTerrainField(sculpted,field,error),error.c_str());
  const float center=resources::terrainFieldHeight(field,0,0)-blank.base;
  const float shoulder=resources::terrainFieldHeight(field,brush.radius*.5f,0)-blank.base;
  AE_EXPECT_TRUE(center>0 && center<=.3f,"default dab makes a gentle measurable elevation");
  AE_EXPECT_TRUE(shoulder>center*.35f && shoulder<center*.65f,"rounded shoulder instead of an isolated spike");
  AE_EXPECT_EQ(resources::terrainFieldHeight(field,brush.radius+2,0),blank.base,"outside remains untouched");
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainPresetBase)+2));
  const auto mountains=f.session.screen().terrainDraft;
  AE_EXPECT_TRUE(mountains.height>0,"procedural mountain preset remains available");
  AE_EXPECT_TRUE(f.session.applyTerrain(mountains,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  f.action(editor::EditorWidget::TerrainClose);
  AE_EXPECT_TRUE(f.session.openTerrain(root),"saved relief can reopen");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),resources::serializeTerrainRecipe(mountains),"new defaults never flatten saved terrain");
}

AE_TEST(terrain_reedit_inspector_touch_for_root_and_chunks_all_modes) {
  for (u32 mode = 0; mode < 3; ++mode) {
    ReeditFixture f;
    AE_EXPECT_TRUE(f.ready, "reedit fixture initialized");
    resources::TerrainRecipe recipe;
    recipe.mode = resources::TerrainMode(mode);
    recipe.cells = recipe.verticalCells = 16;
    recipe.seed = 340 + mode;
    recipe.strokes.push_back({resources::TerrainTool::Raise, 0, 0, 0, 12, 2, 0});
    std::string error;
    AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
    const auto root = f.session.screen().terrainTarget;
    const auto chunks = f.session.document().childrenOf(root);
    AE_EXPECT_TRUE(!chunks.empty(), "terrain has selectable chunks");
    const auto objects = f.session.document().entityCount();
    for (const auto selected : {root, chunks.front()}) {
      f.action(editor::EditorWidget::TerrainClose);
      f.session.setSelection(selected);
      f.session.refreshTerrainJob();
      AE_EXPECT_EQ(f.session.screen().terrainInspectorRoot, root, "inspector resolves terrain owner");
      AE_EXPECT_TRUE(f.tapInspectorEdit(655, 300), "real touch reopens editor from phone inspector");
      AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "context edits existing root");
      AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
                   resources::serializeTerrainRecipe(recipe), "exact original recipe loaded");
      AE_EXPECT_EQ(f.session.document().entityCount(), objects, "opening never duplicates terrain");
    }
    auto edited = f.session.screen().terrainDraft;
    edited.strokes.push_back({resources::TerrainTool::Lower, 4, 0, 4, 6, 1, 0});
    AE_EXPECT_TRUE(f.session.applyTerrain(edited, error), error.c_str());
    AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "apply after reedit preserves root");
    AE_EXPECT_TRUE(f.session.history().undo(f.session.document()), "reedit remains undoable");
    f.session.refreshTerrainJob();
    AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(), recipe.strokes.size(), "undo refreshes recipe");
    AE_EXPECT_TRUE(f.session.history().redo(f.session.document()), "reedit remains redoable");
    f.session.refreshTerrainJob();
    AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(), edited.strokes.size(), "redo refreshes recipe");
  }
}

AE_TEST(terrain_manage_touch_undo_redo_keeps_controls_and_brush_context) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe original;
  original.cells = 16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(original, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  auto edited = original;
  edited.seed += 1;
  edited.strokes.push_back({resources::TerrainTool::Raise, 0, 0, 0, 8, 2, 0});
  AE_EXPECT_TRUE(f.session.applyTerrain(edited, error), error.c_str());
  AE_EXPECT_TRUE(f.session.setTerrainNumber(9, 7) && f.session.setTerrainNumber(10, 2.25) &&
                 f.session.setTerrainNumber(11, 5) && f.session.setTerrainNumber(12, 9) &&
                 f.session.setTerrainNumber(13, -4), "configure precision brush");
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainLayerBase)+3));
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+
                               u32(resources::TerrainTool::Flatten)));
  f.action(editor::EditorWidget::TerrainSculpt);
  const auto brush = f.session.screen().terrainBrush;
  f.session.setSurface({0, 0, 655, 300}, {});
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainTabBase)+3, true),
                 "phone touch opens Manage");
  AE_EXPECT_TRUE(f.scrollTerrain(28), "phone touch scrolls Manage content");
  const auto scroll = f.session.screen().terrainScroll;
  ui::UiPoint savePoint;
  AE_EXPECT_TRUE(f.widgetPoint(editor::widgetId(editor::EditorWidget::TerrainSave), savePoint, true),
                 "save is visible before Undo");
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::Undo), true),
                 "touch Undo inside Manage, not toolbar");
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainTab, 3u, "Undo leaves Manage selected");
  AE_EXPECT_EQ(f.session.screen().terrainScroll, scroll, "Undo keeps the scrolled control positions");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
               resources::serializeTerrainRecipe(original), "Undo reloads the original recipe");
  AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe, resources::serializeTerrainRecipe(original),
               "Undo updates the applied recipe used by Save");
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed, "Undo disarms painting on a replaced source");
  ui::UiPoint saveAfterUndo;
  AE_EXPECT_TRUE(f.widgetPoint(editor::widgetId(editor::EditorWidget::TerrainSave), saveAfterUndo, true),
                 "Save remains visible after Undo");
  AE_EXPECT_TRUE(savePoint.x == saveAfterUndo.x && savePoint.y == saveAfterUndo.y,
                 "the next finger position still addresses Save");
  f.session.handlePointer({41, ui::UiPointerPhase::Down, savePoint, 3});
  f.session.handlePointer({41, ui::UiPointerPhase::Up, savePoint, 3.1});
  AE_EXPECT_TRUE(f.session.screen().saveRequested, "same-position touch requests scene save");
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::Redo), true),
                 "touch Redo inside the same Manage panel");
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainTab, 3u, "Redo leaves Manage selected");
  AE_EXPECT_EQ(f.session.screen().terrainScroll, scroll, "Redo keeps scroll");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
               resources::serializeTerrainRecipe(edited), "Redo reloads the edited recipe");
  const auto &restored = f.session.screen().terrainBrush;
  AE_EXPECT_TRUE(restored.tool == brush.tool && restored.x == brush.x && restored.y == brush.y &&
                 restored.z == brush.z && restored.radius == brush.radius &&
                 restored.strength == brush.strength && restored.layer == brush.layer,
                 "Undo/Redo keep the selected tool and every configured brush value");

  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainTabBase)+1, true) &&
                 f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainSectionBase)+1, true),
                 "touch opens brush Precision subsection");
  AE_EXPECT_TRUE(f.scrollTerrain(24), "scroll precision controls");
  const auto precisionScroll = f.session.screen().terrainScroll;
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()), "Undo while precision is open");
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainTab, 1u, "Undo retains brush tab");
  AE_EXPECT_EQ(f.session.screen().terrainSection, 1u, "Undo retains precision subsection");
  AE_EXPECT_EQ(f.session.screen().terrainScroll, precisionScroll, "Undo retains precision scroll");
  f.action(editor::EditorWidget::TerrainClose);
  f.session.setSelection(root);
  AE_EXPECT_TRUE(f.tapInspectorEdit(655, 300), "explicit Inspector reopening remains available");
  AE_EXPECT_EQ(f.session.screen().terrainTab, 1u, "explicit Inspector opening selects brush tab");
  AE_EXPECT_EQ(f.session.screen().terrainSection, 0u, "explicit Inspector opening starts in tools");
}

AE_TEST(terrain_failed_history_reload_is_cached_and_explicit_reopen_recovers) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe original;
  original.cells = 16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(original, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto originalSource = scene::importLink(f.session.document().find(root)->components)->source;
  const auto originalPath = f.path / f.session.assets().find(originalSource)->path;
  AE_EXPECT_TRUE(f.session.openTerrain(root), "cache the valid original source");
  auto edited = original;
  edited.seed += 1;
  AE_EXPECT_TRUE(f.session.applyTerrain(edited, error), error.c_str());
  AE_EXPECT_TRUE(f.session.setTerrainNumber(3, 27), "keep an unapplied draft to protect on read failure");
  const auto draft = resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  const auto applied = f.session.screen().terrainAppliedRecipe;
  std::vector<u8> sourceBytes;
  AE_EXPECT_TRUE(editor::EditorImportTransaction::read(originalPath, sourceBytes), "save valid source bytes");
  const std::vector<u8> corrupt{0, 1, 2, 3};
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(originalPath, corrupt), "simulate corrupt old revision");
  const auto reference=f.path/".astra/terrain-history/revisions"/
      (f.session.assets().find(originalSource)->contentHash+".ref");
  AE_EXPECT_TRUE(std::filesystem::remove(reference),"also remove recovery reference to test total data loss");
  f.action(editor::EditorWidget::TerrainSculpt);
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()), "Undo selects the corrupt revision");
  f.session.refreshTerrainJob();
  AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "failed reload keeps target");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), draft,
               "failed reload preserves unapplied draft");
  AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe, applied, "failure does not pretend draft is loaded");
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed, "failed reload disarms brush");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("indisponível") != std::string::npos,
                 "failure explains explicit recovery");
  for (const auto control : {editor::EditorWidget::TerrainSculpt, editor::EditorWidget::TerrainStamp,
                             editor::EditorWidget::TerrainGenerate}) f.action(control);
  f.session.startTerrainJob();
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed && !f.session.screen().terrainBusy,
                 "stale recipe cannot paint or start generation");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), draft,
               "refused stamp does not append to the preserved draft");
  AE_EXPECT_TRUE(editor::EditorImportTransaction::write(originalPath, sourceBytes), "restore file without registry change");
  for (u32 frame = 0; frame < 12; ++frame) f.session.refreshTerrainJob();
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), draft,
               "restored file is not retried on every frame after failure");
  f.session.setSurface({0, 0, 1280, 720}, {});
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainTabBase)+3, true) &&
                 f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainLoad), true),
                 "explicit Manage reopening retries the recovered source through UI");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
               resources::serializeTerrainRecipe(original), "explicit retry loads the restored original recipe");
  AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "recovery edits the existing root");
  f.action(editor::EditorWidget::TerrainSculpt);
  AE_EXPECT_TRUE(f.session.screen().terrainBrushArmed, "successful recovery permits painting again");
}

AE_TEST(terrain_reedit_renamed_source_scene_reload_and_failed_open_are_safe) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells = 16;
  recipe.seed = 872;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto source = scene::importLink(f.session.document().find(root)->components)->source;
  auto registry = f.session.assets();
  const auto oldPath = registry.find(source)->path;
  std::filesystem::create_directory(f.path / "Paisagem");
  const std::string newPath = "Paisagem/Montanhas.glb";
  std::filesystem::rename(f.path / oldPath, f.path / newPath);
  AE_EXPECT_TRUE(registry.setPath(source, newPath), "rename registered asset");
  AE_EXPECT_TRUE(f.session.loadAssets(registry.serialize()), "publish renamed registry");
  AE_EXPECT_EQ(f.session.terrainRoot(root), root, "authoring discovery survives source rename");
  f.session.history().begin("Attachment");
  const auto child = f.session.history().createEntity(f.session.document(), root, editor::EditorEntityKind::Folder, "Anexo");
  f.session.history().end();
  AE_EXPECT_TRUE(child != 0, "attached object created");
  const auto saved = f.path / "scene.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(), 0) && f.session.load(saved.string().c_str(), 0), "scene save and reopen");
  f.action(editor::EditorWidget::TerrainClose);
  f.session.setSelection(child);
  f.action(editor::EditorWidget::TerrainOpen);
  AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "main entry resumes selected terrain through attachment");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.seed, recipe.seed, "reopened persisted recipe");
  const auto before = resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  std::filesystem::rename(f.path / newPath, f.path / "temporary.glb");
  AE_EXPECT_TRUE(f.session.openTerrain(root), "missing source recovers authoring from checkpoint");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("recuperada")!=std::string::npos,"recovery is explained in the editor");
  AE_EXPECT_TRUE(f.session.screen().terrainAppliedRecipe.empty(),"recovered recipe requires reconstruction before save");
  const auto reference=f.path/".astra/terrain-history/revisions"/(registry.find(source)->contentHash+".ref");
  AE_EXPECT_TRUE(std::filesystem::remove(reference),"remove backup reference to test unavailable authoring");
  AE_EXPECT_TRUE(!f.session.openTerrain(root), "missing source and unavailable checkpoint refuse edit");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), before, "failure preserves draft");
  AE_EXPECT_EQ(f.session.screen().terrainTarget, root, "failure preserves target");
  std::filesystem::rename(f.path / "temporary.glb", f.path / newPath);
  AE_EXPECT_TRUE(f.session.openTerrain(child), "restored source reopens through attachment");
  f.action(editor::EditorWidget::TerrainNew);
  AE_EXPECT_EQ(f.session.screen().terrainTarget, 0u, "explicit New starts independent draft");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.solidBase, "new terrain defaults to a solid base");
}

AE_TEST(terrain_brush_touch_drag_is_one_edit_and_cancel_or_limit_preserves_recipe) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells = 16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  AE_EXPECT_TRUE(f.session.setTerrainNumber(9, 1), "small brush for spaced samples");
  f.action(editor::EditorWidget::TerrainSculpt);
  ui::UiPoint from, to;
  AE_EXPECT_TRUE(f.brushPoints(from, to), "visible terrain exposes two real touch points");
  const auto undoBefore = f.session.history().undoDepth();
  const auto saved = resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  f.session.handlePointer({21, ui::UiPointerPhase::Down, from, 2});
  f.session.handlePointer({21, ui::UiPointerPhase::Move, to, 2.2});
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy, "drag does not rebuild per move");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), saved, "draft unchanged before release");
  f.session.handlePointer({21, ui::UiPointerPhase::Cancel, to, 2.3});
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy, "cancel does not launch generation");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), saved, "cancel discards all pending samples");
  f.session.handlePointer({22, ui::UiPointerPhase::Down, from, 3});
  f.session.handlePointer({22, ui::UiPointerPhase::Move, to, 3.2});
  f.session.handlePointer({22, ui::UiPointerPhase::Up, to, 3.3});
  AE_EXPECT_TRUE(f.session.screen().terrainBusy, "viewport release launches generation without widget tapped");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.strokes.size() > 1, "drag emits spaced brush samples");
  AE_EXPECT_TRUE(f.finishGeneration(), "stroke generation completes");
  AE_EXPECT_EQ(f.session.history().undoDepth(), undoBefore+1, "whole gesture is one undo step");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()), "undo whole stroke");
  f.session.refreshTerrainJob();
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), saved, "undo restores pre-drag recipe");

  recipe.strokes.resize(4096);
  for (auto &stroke : recipe.strokes) stroke.strength = 0;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  f.action(editor::EditorWidget::TerrainSculpt);
  AE_EXPECT_TRUE(f.brushPoints(from, to), "terrain remains touchable at recipe limit");
  const auto full = resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  f.session.handlePointer({23, ui::UiPointerPhase::Down, from, 4});
  f.session.handlePointer({23, ui::UiPointerPhase::Up, from, 4.1});
  AE_EXPECT_TRUE(!f.session.screen().terrainBusy, "full recipe refuses generation");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft), full, "over-budget gesture is atomic");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("4096") != std::string::npos, "limit has explicit feedback");
}

AE_TEST(terrain_apply_reuses_completed_build_and_rebuilds_only_changed_chunks) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells=64;recipe.size=64;recipe.height=0;recipe.base=4;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto original=f.session.screen().terrainPreviewBuild;
  f.action(editor::EditorWidget::TerrainGenerate);
  AE_EXPECT_TRUE(f.session.screen().terrainBusy,"apply starts asynchronous generation");
  AE_EXPECT_TRUE(f.finishGeneration(),"async apply completes");
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild==original,"unchanged apply shares immutable completed geometry");
  resources::TerrainStroke stroke;
  stroke.x=-20;stroke.z=-20;stroke.radius=3;stroke.strength=2;
  recipe.strokes.push_back(stroke);
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto updated=f.session.screen().terrainPreviewBuild;
  AE_EXPECT_TRUE(updated!=original,"edit creates a new immutable build");
  AE_EXPECT_TRUE(updated->mesh.rebuiltChunks>0 && updated->mesh.reusedChunks>0,"local apply retains unaffected chunks");
  resources::TerrainBuild full;
  AE_EXPECT_TRUE(resources::updateTerrain(recipe,nullptr,full,error),error.c_str());
  AE_EXPECT_EQ(updated->mesh.parts.size(),full.mesh.parts.size(),"regional commit has full-build part count");
  for(usize i=0;i<full.mesh.parts.size();++i) {
    const auto &a=updated->mesh.parts[i].vertices;
    const auto &b=full.mesh.parts[i].vertices;
    AE_EXPECT_EQ(a.size(),b.size(),"regional commit preserves triangle count");
    AE_EXPECT_TRUE(std::memcmp(a.data(),b.data(),a.size()*sizeof(resources::TerrainVertex))==0,"regional commit preserves geometry normals UVs and weights");
  }
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild==updated,"synchronous apply shares completed geometry");
}

AE_TEST(terrain_release_commits_the_completed_live_preview_without_remeshing) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells=16;recipe.height=0;recipe.base=4;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(resources::TerrainTool::Raise)));
  ui::UiPoint from,to;
  AE_EXPECT_TRUE(f.brushPoints(from,to),"pickable terrain");
  f.session.handlePointer({25,ui::UiPointerPhase::Down,from,5});
  for(u32 i=0;i<5000 && !f.session.screen().terrainLivePreview;++i) {
    f.session.refreshTerrainJob();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // A stationary dab has no residual endpoint to flush on release. A dragged
  // stroke can legitimately add its final sample and must take the update path.
  AE_EXPECT_TRUE(f.session.screen().terrainLivePreview,"real preview published before release");
  const auto preview=f.session.screen().terrainPreviewBuild;
  AE_EXPECT_TRUE(preview && !preview->recipe.strokes.empty(),"preview contains sculpting");
  f.session.handlePointer({25,ui::UiPointerPhase::Up,from,5.2});
  AE_EXPECT_TRUE(f.finishGeneration(),"release completes");
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild==preview,"release commits exact preview snapshot without copying or remeshing");
  AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe,resources::serializeTerrainRecipe(preview->recipe),"persisted recipe agrees with visible preview");
}

AE_TEST(terrain_live_preview_is_visible_before_release_and_cancel_is_atomic) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells=16;recipe.height=0;recipe.base=4;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  const auto source=scene::importLink(f.session.document().find(root)->components)->source;
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(resources::TerrainTool::Raise)));
  AE_EXPECT_TRUE(f.session.setTerrainNumber(9,16) && f.session.setTerrainNumber(10,3),"configure brush");
  ui::UiPoint from,to;
  AE_EXPECT_TRUE(f.brushPoints(from,to),"pickable terrain");
  const auto revision=f.session.document().revision();
  const auto undo=f.session.history().undoDepth();
  const auto registry=f.session.assets().serialize();
  const auto serialized=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  const auto files=[&] {
    usize count=0;
    for(const auto &entry:std::filesystem::recursive_directory_iterator(f.path)) {
      if(entry.is_regular_file())++count;
    }
    return count;
  };
  const auto filesBefore=files();
  const auto publishedDraws=f.draws.size();
  const auto publishedTextures=f.textures;
  const auto publishedMaterials=f.materials;
  f.session.handlePointer({25,ui::UiPointerPhase::Down,from,5});
  f.session.handlePointer({25,ui::UiPointerPhase::Move,to,5.1});
  for(u32 i=0;i<5000 && !f.session.screen().terrainLivePreview;++i) {
    f.session.refreshTerrainJob();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(f.session.screen().terrainLivePreview,f.session.screen().terrainMessage.c_str());
  const auto firstPreview=f.session.screen().terrainPreviewUpdates;
  // Down and the first Move may correctly coalesce before the worker finishes.
  // A subsequent move must publish NEW geometry, without requiring redundant
  // publication of an unchanged draft just to satisfy a timing-dependent count.
  f.session.handlePointer({25,ui::UiPointerPhase::Move,from,5.2});
  for(u32 i=0;i<5000 && f.session.screen().terrainPreviewUpdates==firstPreview;++i) {
    f.session.refreshTerrainJob();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewUpdates>firstPreview,"a later drag sample publishes new geometry while gesture remains captured");
  AE_EXPECT_TRUE(f.session.screen().terrainCursorVisible && f.session.screen().terrainCursorLines.size()>=96,
                 "surface cursor includes outer boundary and falloff ring");
  AE_EXPECT_TRUE(f.draws.size()>publishedDraws,"live preview publishes additional real mesh draws");
  AE_EXPECT_TRUE(f.textures==publishedTextures,"preview shares exact resident images without allocating additional texture slots");
  const auto &previewBuild=*f.session.screen().terrainPreviewBuild;
  usize previewVertices=0,previewCorners=0;
  for(usize part=0;part<previewBuild.mesh.parts.size();++part) {
    const auto &mesh=f.draws[publishedDraws+part];
    const auto &material=f.materials[mesh.materialIndex];
    const auto &original=publishedMaterials[1+previewBuild.mesh.parts[part].layer];
    for(u32 slot=0;slot<4;++slot)
      AE_EXPECT_EQ(material.textureIndices[slot],original.textureIndices[slot],"preview preserves original layer bindings");
    const auto &sourceVertices=previewBuild.mesh.parts[part].vertices;
    AE_EXPECT_EQ(mesh.indexCount,sourceVertices.size(),"preview preserves every triangle corner");
    u32 maximum=0;
    for(u32 corner=0;corner<mesh.indexCount;++corner) {
      const auto index=f.indices[mesh.firstIndex+corner];maximum=std::max(maximum,index);
      const auto *packed=f.vertices.data()+(mesh.vertexOffset+index)*renderer::MapVertexStride;
      const auto &expected=sourceVertices[corner];
      AE_EXPECT_TRUE(std::memcmp(packed,expected.position,12)==0,"preview indexing preserves positions");
      AE_EXPECT_TRUE(std::memcmp(packed+28,expected.uv,8)==0 && std::memcmp(packed+36,expected.uv1,8)==0,
                     "preview indexing preserves both texture coordinates");
      for(u32 axis=0;axis<3;++axis) {
        i16 normal;std::memcpy(&normal,packed+12+axis*2,2);
        AE_EXPECT_EQ(normal,i16(std::clamp(expected.normal[axis],-1.f,1.f)*32767),"preview preserves packed normals");
      }
      for(u32 layer=0;layer<4;++layer)
        AE_EXPECT_EQ(packed[44+layer],u8(std::lround(std::clamp(expected.weights[layer],0.f,1.f)*255)),
                     "preview preserves all paint weights");
    }
    previewVertices+=maximum+1;previewCorners+=mesh.indexCount;
  }
  AE_EXPECT_TRUE(previewVertices<previewCorners,"live preview shares vertices without decimation");
  std::vector<renderer::MapDrawState> visible;
  AE_EXPECT_TRUE(f.session.extractMap(visible),"extract live scene");
  bool previewVisible=false,raised=false;
  for(const auto &draw:visible)if(draw.visible && draw.objectId==root && draw.sourceDrawIndex>=publishedDraws) {
    previewVisible=true;
    const auto &mesh=f.draws[draw.sourceDrawIndex];
    for(u32 i=0;i<mesh.indexCount;++i) {
      float p[3];std::memcpy(p,f.vertices.data()+(mesh.vertexOffset+f.indices[mesh.firstIndex+i])*renderer::MapVertexStride,sizeof(p));
      raised|=p[1]>recipe.base+.05f;
    }
  }
  AE_EXPECT_TRUE(previewVisible && raised,"render extraction exposes raised geometry while pointer is down");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"preview cannot mutate document revision");
  AE_EXPECT_EQ(f.session.history().undoDepth(),undo,"preview cannot add Undo steps");
  AE_EXPECT_EQ(f.session.assets().serialize(),registry,"preview cannot register resources");
  AE_EXPECT_EQ(files(),filesBefore,"preview cannot create files");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),serialized,"preview keeps draft immutable");
  AE_EXPECT_EQ(scene::importLink(f.session.document().find(root)->components)->source,source,"preview keeps source link");
  f.session.handlePointer({25,ui::UiPointerPhase::Cancel,from,5.3});
  f.session.refreshTerrainJob();
  AE_EXPECT_TRUE(!f.session.screen().terrainLivePreview && !f.session.screen().terrainCursorVisible,"cancel clears preview and cursor");
  AE_EXPECT_EQ(f.draws.size(),publishedDraws,"cancel restores original render package immediately");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"cancel preserves document");
  AE_EXPECT_EQ(f.session.assets().serialize(),registry,"cancel preserves registry");
}

AE_TEST(terrain_live_preview_does_not_feed_displacement_into_stationary_brush) {
  for (const auto mode : {resources::TerrainMode::Heightfield, resources::TerrainMode::SmoothVoxel}) {
    ReeditFixture f;
    AE_EXPECT_TRUE(f.ready,"fixture initialized");
    resources::TerrainRecipe recipe;
    recipe.mode=mode;recipe.cells=recipe.verticalCells=16;recipe.height=0;recipe.base=4;
    std::string error;
    AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
    const auto tool=mode==resources::TerrainMode::Heightfield?resources::TerrainTool::Raise:resources::TerrainTool::Dig;
    f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(tool)));
    AE_EXPECT_TRUE(f.session.setTerrainNumber(9,16) && f.session.setTerrainNumber(10,3),"configure brush");
    ui::UiPoint from,to;
    AE_EXPECT_TRUE(f.brushPoints(from,to),"pickable surface");
    const ui::UiPoint point{std::lerp(from.x,to.x,.5f),std::lerp(from.y,to.y,.5f)};
    f.session.handlePointer({27,ui::UiPointerPhase::Down,point,6});
    for(u32 i=0;i<5000 && !f.session.screen().terrainLivePreview;++i) {
      f.session.refreshTerrainJob();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    AE_EXPECT_TRUE(f.session.screen().terrainLivePreview,"deformed mesh published before subsequent pointer events");
    for(u32 i=0;i<20;++i) {
      f.session.handlePointer({27,ui::UiPointerPhase::Move,point,6.1+i*.01});
      f.session.refreshTerrainJob();
    }
    f.session.handlePointer({27,ui::UiPointerPhase::Up,point,7});
    AE_EXPECT_TRUE(f.finishGeneration(),"stationary gesture completes");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(),usize(1),
                 "preview displacement and stationary move events cannot add authoring samples");
  }
}

AE_TEST(terrain_open_from_geometry_context_dismisses_creation_overlay) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  f.action(editor::EditorWidget::TerrainClose);
  f.session.setSelection(f.session.document().root());
  f.session.setSurface({0, 0, 1280, 720}, {});
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::HierarchyAdd)), "open real Create context menu");
  AE_EXPECT_TRUE(f.session.screen().contextMenu, "context is visible");
  AE_EXPECT_TRUE(f.tapWidget(editor::context_menu_widget::id(editor::context_menu_widget::Category+1)), "open Geometry category");
  bool opened = false;
  for (u32 page = 0; page < 12 && !opened; ++page) {
    opened = f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainOpen));
    if (!opened) {
      const auto window = f.session.layout().contextMenuWindow;
      const ui::UiPoint down{window.x+window.width*.5f,window.bottom()-12};
      const ui::UiPoint up{down.x,window.y+12};
      f.session.handlePointer({13, ui::UiPointerPhase::Down, down, 2});
      f.session.handlePointer({13, ui::UiPointerPhase::Move, up, 2.2});
      f.session.handlePointer({13, ui::UiPointerPhase::Up, up, 2.3});
    }
  }
  AE_EXPECT_TRUE(opened && f.session.screen().terrainOpen, "context action opens real terrain editor");
  AE_EXPECT_TRUE(!f.session.screen().contextMenu && !f.session.screen().contextMenuCreating && !f.session.screen().creationMenu,
                 "all creation overlays close before terrain authoring");
}

AE_TEST(terrain_flatten_and_path_viewport_keep_configured_level_and_change_surface) {
  for (const auto tool : {resources::TerrainTool::Flatten, resources::TerrainTool::Path}) {
    ReeditFixture f;
    AE_EXPECT_TRUE(f.ready, "fixture initialized");
    resources::TerrainRecipe recipe;
    recipe.cells = 16;
    recipe.height = 0;
    recipe.base = 2;
    std::string error;
    AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
    f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(tool)));
    constexpr float targetLevel = 18;
    AE_EXPECT_TRUE(f.session.setTerrainNumber(9, 24) && f.session.setTerrainNumber(10, 128) &&
                   f.session.setTerrainNumber(12, targetLevel), "configure radius, strength and precise level");
    f.action(editor::EditorWidget::TerrainSculpt);
    ui::UiPoint from, to;
    AE_EXPECT_TRUE(f.brushPoints(from, to), "flat starting terrain has visible brush targets");
    f.session.handlePointer({31, ui::UiPointerPhase::Down, from, 2});
    f.session.handlePointer({31, ui::UiPointerPhase::Move, to, 2.2});
    f.session.handlePointer({31, ui::UiPointerPhase::Up, to, 2.3});
    AE_EXPECT_TRUE(f.session.screen().terrainBusy, "level brush commits from the viewport");
    AE_EXPECT_TRUE(f.finishGeneration(), "level brush generation completes");
    const auto &edited = f.session.screen().terrainDraft;
    AE_EXPECT_TRUE(!edited.strokes.empty(), "level gesture produced authoring samples");
    AE_EXPECT_EQ(f.session.screen().terrainBrush.y, targetLevel, "precision control keeps configured level");
    for (const auto &stroke : edited.strokes)
      AE_EXPECT_EQ(stroke.y, targetLevel, "sample uses configured level instead of picked surface height");

    // Query the published picking geometry, not a separately generated copy.
    // Move slightly inside the perimeter so an edge tap cannot cause a border
    // precision miss. The wide, strong brush flattens these vertices completely.
    const auto &stroke = edited.strokes.front();
    editor::EditorRay ray;
    ray.origin[0] = std::clamp(stroke.x, -28.f, 28.f);
    ray.origin[1] = 100;
    ray.origin[2] = std::clamp(stroke.z, -28.f, 28.f);
    ray.direction[1] = -1;
    ray.valid = true;
    ray.maximumDistance = 200;
    const auto hit = editor::pickNearest(f.session.pickCandidates(), ray, 0, true);
    AE_EXPECT_TRUE(hit.hit, "published terrain remains pickable after level edit");
    AE_EXPECT_TRUE(std::abs((100-hit.distance)-targetLevel) < .01f,
                   "published surface reaches configured target level");
  }
}

AE_TEST(terrain_open_heightfield_normalizes_volume_only_brushes) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells = recipe.verticalCells = 16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto heightfield = f.session.screen().terrainTarget;
  f.action(editor::EditorWidget::TerrainNew);
  recipe.mode = resources::TerrainMode::SmoothVoxel;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto voxel = f.session.screen().terrainTarget;
  for (const auto tool : {resources::TerrainTool::Dig, resources::TerrainTool::Add}) {
    AE_EXPECT_TRUE(f.session.openTerrain(voxel), "open editable voxel");
    f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(tool)));
    AE_EXPECT_EQ(f.session.screen().terrainBrush.tool, tool, "voxel accepts volume brush");
    AE_EXPECT_TRUE(f.session.openTerrain(heightfield), "open existing heightfield");
    AE_EXPECT_EQ(f.session.screen().terrainBrush.tool, resources::TerrainTool::Raise,
                 "heightfield replaces incompatible brush with Raise");
  }
  AE_EXPECT_TRUE(f.session.openTerrain(voxel), "reopen voxel before New");
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(resources::TerrainTool::Dig)));
  f.action(editor::EditorWidget::TerrainNew);
  AE_EXPECT_EQ(f.session.screen().terrainBrush.tool, resources::TerrainTool::Raise,
               "new heightfield also resets incompatible brush");
}

AE_TEST(terrain_new_revision_budget_refuses_growth_and_reuses_existing_hash) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells = 8;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto source = scene::importLink(f.session.document().find(root)->components)->source;

  resources::TerrainMesh mesh;
  std::vector<u8> bytes;
  resources::GltfImport library;
  AE_EXPECT_TRUE(resources::generateTerrain(recipe, mesh, error) &&
                 resources::writeTerrainGlb(recipe, mesh, {}, bytes, error) &&
                 resources::importGlb(bytes, {}, {}, library), "prepare real lightweight geometry library");
  const auto draw = library.draws.front();
  const auto node = library.nodes.front();
  library.draws.assign(4096, draw);
  library.nodes.assign(4096, node);
  library.names.resize(4096);
  library.drawNodes.resize(4096);
  library.drawSkins.assign(4096, -1);
  library.drawMorphs.assign(4096, -1);
  library.keys.clear();
  for (u32 i = 0; i < 4096; ++i) {
    library.drawNodes[i] = i;
    library.nodes[i].name = library.names[i] = "Budget mesh " + std::to_string(i);
    library.nodes[i].parent = -1;
  }
  editor::EditorSession::ModelImportReport report;
  AE_EXPECT_TRUE(f.session.publishModel(library, std::string(64, 'b'), "Budget/library.glb", report), report.diagnostic.c_str());
  f.session.refreshTerrainBudget();
  AE_EXPECT_TRUE(f.session.screen().terrainRetainedDraws > editor::EditorScreenState::TerrainRetainedDrawLimit,
                 "real loaded library reaches aggregate draw budget");
  AE_EXPECT_TRUE(f.session.screen().terrainRetainedBytesKnown && f.session.screen().terrainRetainedBytes > 0 &&
                 f.session.screen().terrainRetainedBytes < editor::EditorScreenState::TerrainRetainedByteLimit,
                 "retained CPU payload is measured independently of draw count");
  const auto registry = f.session.assets().serialize();
  const auto objects = f.session.document().entityCount();
  const auto undo = f.session.history().undoDepth();
  auto changed = recipe;
  ++changed.seed;
  AE_EXPECT_TRUE(!f.session.applyTerrain(changed, error), "new terrain revision refuses aggregate over-budget growth");
  AE_EXPECT_TRUE(error.find("128 MiB") != std::string::npos && error.find("4096") != std::string::npos,
                 "refusal explains both retention limits");
  AE_EXPECT_EQ(f.session.assets().serialize(), registry, "refusal does not register a new source");
  AE_EXPECT_EQ(f.session.document().entityCount(), objects, "refusal preserves scene objects");
  AE_EXPECT_EQ(f.session.history().undoDepth(), undo, "refusal preserves Undo history");
  AE_EXPECT_EQ(scene::importLink(f.session.document().find(root)->components)->source, source,
               "refusal preserves current terrain geometry revision");
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), "existing immutable hash remains usable above budget");
  AE_EXPECT_EQ(f.session.assets().serialize(), registry, "existing hash adds no retained source");
}

AE_TEST(terrain_straight_stroke_is_independent_of_touch_event_frequency) {
  for(const auto mode:{resources::TerrainMode::Heightfield,resources::TerrainMode::SmoothVoxel}) {
    std::vector<resources::TerrainStroke> reference;
    resources::TerrainField referenceField;
    for(u32 eventCount:{1u,8u,32u}) {
      ReeditFixture f;
      AE_EXPECT_TRUE(f.ready,"fixture initialized");
      resources::TerrainRecipe recipe;recipe.mode=mode;recipe.cells=recipe.verticalCells=16;
      recipe.height=0;recipe.base=4;
      std::string error;
      AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
      const auto tool=mode==resources::TerrainMode::Heightfield?resources::TerrainTool::Raise:resources::TerrainTool::Dig;
      f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(tool)));
      f.action(editor::EditorWidget::TerrainOpacityTarget);
      f.action(editor::EditorWidget::TerrainOpacityTarget);
      AE_EXPECT_TRUE(f.session.setTerrainNumber(9,4) && f.session.setTerrainNumber(10,2),"accumulative brush configured");
      ui::UiPoint from,to;
      AE_EXPECT_TRUE(f.brushPoints(from,to),"straight visible stroke available");
      f.session.handlePointer({41,ui::UiPointerPhase::Down,from,10});
      for(u32 i=1;i<=eventCount;++i) {
        const float t=float(i)/eventCount;
        f.session.handlePointer({41,ui::UiPointerPhase::Move,{std::lerp(from.x,to.x,t),std::lerp(from.y,to.y,t)},10+t});
      }
      f.session.handlePointer({41,ui::UiPointerPhase::Up,to,11.1});
      AE_EXPECT_TRUE(f.finishGeneration(),"stroke completes");
      const auto &authored=f.session.screen().terrainDraft;
      AE_EXPECT_TRUE(authored.strokes.size()>1,"pointer routing actually authored a multi-stamp stroke");
      resources::TerrainField field;
      AE_EXPECT_TRUE(resources::generateTerrainField(authored,field,error),error.c_str());
      if(eventCount==1) {reference=authored.strokes;referenceField=field;}
      else {
        AE_EXPECT_EQ(authored.strokes.size(),reference.size(),"event density does not add stamps");
        for(usize i=0;i<reference.size();++i) {
          const auto &a=authored.strokes[i],&b=reference[i];
          AE_EXPECT_TRUE(std::abs(a.x-b.x)<.001f && std::abs(a.y-b.y)<.001f && std::abs(a.z-b.z)<.001f,
                         "same world-space sampling under sparse and dense events");
        }
        for(usize i=0;i<field.heights.size();++i)
          AE_EXPECT_TRUE(std::abs(field.heights[i]-referenceField.heights[i])<.002f,"height effect independent of event frequency");
        for(usize i=0;i<field.density.size();++i)
          AE_EXPECT_TRUE(std::abs(field.density[i]-referenceField.density[i])<.002f,"volume effect independent of event frequency");
      }
    }
  }
}

AE_TEST(terrain_authored_lod_survives_editor_apply_history_and_scene_reload) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe recipe;
  recipe.cells=16;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  const auto originalSource=scene::importLink(f.session.document().find(root)->components)->source;
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(28,2.5) && !f.session.setTerrainNumber(28,5),
                 "LOD level count rejects fractional and excessive values");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,1u,"invalid edits preserve the draft");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(28,3) && f.session.setTerrainNumber(29,2),
                 "editor accepts authored LOD policy");
  const auto authored=f.session.screen().terrainDraft;
  AE_EXPECT_TRUE(f.session.applyTerrain(authored,error),error.c_str());
  AE_EXPECT_TRUE(!f.publishedLods.empty(),"authored LOD reaches the actual geometry publisher");
  for(const auto &lod:f.publishedLods) {
    AE_EXPECT_TRUE(lod.level>0 && lod.level<authored.lodLevels,"published levels obey authored count");
    AE_EXPECT_TRUE(lod.indexCount>0 && u64(lod.firstIndex)+lod.indexCount<=f.indices.size(),
                   "published LOD has an accessible index range");
  }
  const auto lodSource=scene::importLink(f.session.document().find(root)->components)->source;
  AE_EXPECT_TRUE(lodSource!=originalSource,"LOD settings create a distinct persisted revision");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo LOD revision");
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,1u,"Undo restores disabled LOD");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo LOD revision");
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,3u,"Redo restores authored levels");
  const auto saved=f.path/"lod-scene.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(),0) && f.session.load(saved.string().c_str(),0),
                 "save and reload actual scene");
  f.action(editor::EditorWidget::TerrainClose);
  AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen terrain after scene reload");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
               resources::serializeTerrainRecipe(authored),"all authored properties survive reload");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(28,1),"disable authored LOD through editor");
  const auto disabled=f.session.screen().terrainDraft;
  AE_EXPECT_TRUE(f.session.applyTerrain(disabled,error),error.c_str());
  f.action(editor::EditorWidget::TerrainClose);
  AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen after disabling LOD");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,1u,"disabled policy also persists");
}

AE_TEST(terrain_slider_touch_columns_precision_and_scroll) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  f.session.setSurface({0,0,1280,720},{});
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainTabBase)+1));
  const auto slider=editor::widgetId(editor::EditorWidget::TerrainSliderBase);
  ui::UiPoint point;
  AE_EXPECT_TRUE(f.widgetPoint(slider+9,point),"radius is visible before scrolling");
  AE_EXPECT_TRUE(f.widgetPoint(slider+10,point),"strength is visible before scrolling");
  const float initial=f.session.screen().terrainBrush.strength;
  f.session.handlePointer({70,ui::UiPointerPhase::Down,point,1});
  f.session.handlePointer({70,ui::UiPointerPhase::Move,{point.x,point.y-60},1.2});
  f.session.handlePointer({70,ui::UiPointerPhase::Up,{point.x,point.y-60},1.3});
  AE_EXPECT_EQ(f.session.screen().terrainBrush.strength,initial,"vertical scrolling never edits strength");
  AE_EXPECT_TRUE(f.session.screen().terrainScroll>0,"vertical drag scrolls the panel");
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainTabBase)+0));
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainSectionBase)+2));
  AE_EXPECT_TRUE(f.widgetPoint(slider+0x100+28,point),"left-column LOD slider routed");
  const auto area=editor::terrainContentRect(f.session.screen(),editor::terrainPanelRect(f.session.screen()));
  const ui::UiPoint end{area.x+area.width*.5f-10,point.y};
  f.session.handlePointer({71,ui::UiPointerPhase::Down,point,2});
  f.session.handlePointer({71,ui::UiPointerPhase::Move,end,2.2});
  f.session.handlePointer({71,ui::UiPointerPhase::Up,end,2.3});
  AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,4u,"left column reaches its discrete maximum");
  AE_EXPECT_TRUE(f.widgetPoint(slider+0x200+29,point),"dependent right-column slider becomes enabled");
  const ui::UiPoint right{area.right()-10,point.y};
  f.session.handlePointer({72,ui::UiPointerPhase::Down,point,3});
  f.session.handlePointer({72,ui::UiPointerPhase::Move,right,3.2});
  f.session.handlePointer({72,ui::UiPointerPhase::Up,right,3.3});
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.lodRelativeError-.1f)<.00001f,"right column uses its own scale");
  AE_EXPECT_TRUE(f.tapWidget(editor::widgetId(editor::EditorWidget::TerrainNumberBase)+29),"value chip opens precise entry");
  AE_EXPECT_EQ(f.session.screen().numericField,editor::widgetId(editor::EditorWidget::TerrainNumberBase)+29,"precise input retains the selected property");
}

AE_TEST(terrain_revisions_share_exact_texture_pixels_without_losing_history) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  resources::TerrainRecipe r;r.cells=8;r.height=0;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  const auto original=f.textures;
  AE_EXPECT_TRUE(original.size()>=4,"decoded terrain textures published");
  const auto before=f.session.screen().terrainRetainedBytes;
  r.strokes.push_back({resources::TerrainTool::Raise,0,0,0,8,.2f,0});
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  AE_EXPECT_EQ(f.textures.size(),original.size(),"source revisions share GPU slots instead of consuming duplicate descriptors");
  for(usize i=0;i<original.size();++i)
    AE_EXPECT_TRUE(f.textures[i]==original[i],"identical mips share immutable storage across revisions");
  for(const auto &material:f.materials)for(const auto slot:material.textureIndices)
    AE_EXPECT_TRUE(slot==renderer::InvalidMapTexture || slot<f.textures.size(),"every published material binding remains valid");
  AE_EXPECT_TRUE(f.session.screen().terrainRetainedBytes<before*2,"retained budget counts shared pixels once");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo retains first revision");f.session.update();
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.strokes.empty(),"original geometry recipe restored");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo retains second revision");f.session.update();
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(),usize(1),"edited geometry recipe restored");
  const auto saved=f.path/"shared-textures.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(),0) && f.session.load(saved.string().c_str(),0),"real scene reload");
  AE_EXPECT_TRUE(f.session.openTerrain(root),"terrain editable after reload");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(),usize(1),"revision persists with shared pixels");
  AE_EXPECT_EQ(f.textures.size(),original.size(),"cached project reload also deduplicates GPU texture slots");
  const auto source=scene::importLink(f.session.document().find(root)->components)->source;
  auto profile=f.session.importProfileFor(source);profile.textureStreamingPriority=3;
  AE_EXPECT_TRUE(f.session.saveImportProfile(source,profile),"distinct authoring policy saved");
  r.strokes.push_back({resources::TerrainTool::Raise,4,0,4,8,.2f,0});
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  AE_EXPECT_EQ(f.textures.size(),original.size()*2,"different streaming priorities retain independent GPU slots");
  bool defaultPriority=false,highPriority=false;
  for(const auto &policy:f.session.textureStreamingParameters()) {defaultPriority|=policy.priority==0;highPriority|=policy.priority==3;}
  AE_EXPECT_TRUE(defaultPriority && highPriority,"deduplication preserves both effective streaming policies");
}

AE_TEST(terrain_control_maps_publish_preview_cancel_history_and_reload) {
  ReeditFixture f;
  AE_EXPECT_TRUE(f.ready,"fixture initialized");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.controlCells,u32(128),"new editor terrain starts with independent paint map");
  resources::TerrainRecipe r;r.cells=8;r.controlCells=128;r.height=0;r.base=4;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  const auto original=f.textures;
  const auto originalControls=f.controls;
  AE_EXPECT_TRUE(f.controls.size()+1==f.materials.size(),"publication controls aligned imported materials before built-in prefix");
  for(const auto &control:f.controls) {
    if(control.textureIndex==renderer::InvalidMapTexture)continue;
    AE_EXPECT_TRUE(control.textureIndex<f.textures.size(),"control resolves deduplicated texture slot");
    const auto &texture=*f.textures[control.textureIndex];
    AE_EXPECT_TRUE(!texture.srgb && texture.levels==1 && texture.width==129,"data texture retains full authoring map");
  }
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainToolBase)+u32(resources::TerrainTool::Paint)));
  f.action(editor::EditorWidget(editor::widgetId(editor::EditorWidget::TerrainLayerBase)+3));
  AE_EXPECT_TRUE(f.session.setTerrainNumber(9,3) && f.session.setTerrainNumber(10,1),"configure fine paint brush");
  ui::UiPoint from,to;AE_EXPECT_TRUE(f.brushPoints(from,to),"terrain visible for brush");
  const auto revision=f.session.document().revision();
  f.session.handlePointer({91,ui::UiPointerPhase::Down,from,10});
  f.session.handlePointer({91,ui::UiPointerPhase::Move,to,10.1});
  for(u32 i=0;i<5000 && !f.session.screen().terrainLivePreview;++i) {
    f.session.refreshTerrainJob();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  AE_EXPECT_TRUE(f.session.screen().terrainLivePreview,f.session.screen().terrainMessage.c_str());
  AE_EXPECT_EQ(f.session.document().revision(),revision,"live paint does not mutate document");
  AE_EXPECT_TRUE(f.controls.size()>originalControls.size(),"preview carries its own material bindings");
  const auto previewBinding=std::find_if(f.controls.rbegin(),f.controls.rend(),[](const auto &binding){return binding.textureIndex!=renderer::InvalidMapTexture;});
  AE_EXPECT_TRUE(previewBinding!=f.controls.rend(),"preview has active control binding");
  const auto &previewControl=*previewBinding;
  renderer::SharedAuthoringTexture expected;
  AE_EXPECT_TRUE(resources::makeTerrainControlTexture(f.session.screen().terrainPreviewBuild->field,expected,error),error.c_str());
  AE_EXPECT_TRUE(expected->mipChain!=original[originalControls.front().textureIndex]->mipChain,"actual paint changes control pixels before release");
  AE_EXPECT_TRUE(previewControl.textureIndex<f.textures.size(),"preview control slot remapped with texture interning");
  AE_EXPECT_TRUE(f.textures[previewControl.textureIndex]->mipChain==expected->mipChain,"live paint publishes exact new control pixels");
  for(const auto &texture:original)
    AE_EXPECT_TRUE(std::find(f.textures.begin(),f.textures.end(),texture)!=f.textures.end(),"preview preserves shared committed texture allocations");
  f.session.handlePointer({91,ui::UiPointerPhase::Cancel,from,10.2});
  f.session.refreshTerrainJob();
  AE_EXPECT_TRUE(f.textures==original && f.controls.size()==originalControls.size(),"cancel restores committed map transaction");
  r.strokes.push_back({resources::TerrainTool::Paint,1,4,1,3,1,3});
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo paint");f.session.update();
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.strokes.empty(),"undo restores original paint recipe");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo paint");f.session.update();
  const auto saved=f.path/"control-map.aescene";
  AE_EXPECT_TRUE(f.session.save(saved.string().c_str(),0) && f.session.load(saved.string().c_str(),0),"cold scene/cache reload");
  AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen editable terrain");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.controlCells,u32(128),"control resolution persists");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(),usize(1),"paint persists through undo redo cache reload");
  AE_EXPECT_TRUE(f.session.terrainControlPublication().empty(),"transaction metadata never dangles outside publisher");
  for(const auto &control:f.controls)AE_EXPECT_TRUE(control.textureIndex==renderer::InvalidMapTexture || control.textureIndex<f.textures.size(),"reloaded control slot remains valid");
}

AE_TEST(terrain_material_extension_carries_control_without_overwriting_lightmap) {
  renderer::MaterialOverride material;
  renderer::TerrainControlBinding control;control.textureIndex=3;
  control.uvScale[0]=.12f;control.uvScale[1]=.23f;control.uvOffset[0]=.5f;control.uvOffset[1]=.4f;
  float rows[renderer::MaterialExtensionFloats]{};
  renderer::materialExtensionEntry(material,renderer::MapMaterialTerrain,renderer::MaterialExtensionNoTexture,rows,9,&control,17);
  AE_EXPECT_EQ(rows[44],1.f,"lightmap remains enabled");AE_EXPECT_EQ(rows[45],9.f,"lightmap slot independent");
  AE_EXPECT_EQ(rows[52],1.f,"terrain control enabled");AE_EXPECT_EQ(rows[53],17.f,"bindless control slot");
  AE_EXPECT_EQ(rows[54],.12f,"scale X");AE_EXPECT_EQ(rows[55],.23f,"scale Z");
  AE_EXPECT_EQ(rows[56],.5f,"offset X");AE_EXPECT_EQ(rows[57],.4f,"offset Z");
  renderer::materialExtensionEntry(material,0,renderer::MaterialExtensionNoTexture,rows,9,&control,17);
  for(usize i=52;i<renderer::MaterialExtensionFloats;++i)AE_EXPECT_EQ(rows[i],0.f,"non-terrain cannot inherit stale control data");
}
