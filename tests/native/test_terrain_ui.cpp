#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_terrain_modules_controls.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "scene/import_link.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <set>
#include <thread>

using namespace ae;
namespace {
using Widget = editor::EditorWidget;
u32 id(Widget widget, u32 offset = 0) {
  return editor::widgetId(widget) + offset;
}

struct TerrainUiFixture {
  editor::EditorSession session;
  ui::UiFont font;
  ui::UiIconAtlas icons;
  ui::UiDrawList list;
  ui::UiFontMetrics metrics;
  ui::UiInputRouter router;
  std::vector<u8> vertices, fontBytes, iconBytes;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  std::filesystem::path path;
  bool ready = false;
  double time = 10;
  TerrainUiFixture(float width = 655, float height = 300) {
    path = std::filesystem::temp_directory_path() /
           ("astra-terrain-ui-" +
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
          vertices.clear();
          indices.clear();
          draws.clear();
          materials.clear();
          renderer::appendBoxAuthoringGeometry(
              renderer::MapVertexStride, vertices, indices, draws, materials);
          const auto vb = u32(vertices.size() / renderer::MapVertexStride);
          const auto ib = u32(indices.size()), mb = u32(materials.size());
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
    ready = editor::EditorImportTransaction::read(
                "assets/astra-visual/ui/astra-ui-font.aeuf", fontBytes) &&
            editor::EditorImportTransaction::read(
                "assets/astra-visual/ui/astra-ui-icons.aeui", iconBytes) &&
            font.load(fontBytes) && icons.load(iconBytes);
    session.initialize(&font, &icons);
    session.setSurface({0, 0, width, height}, {});
    ready = ready && session.setProjectDirectory(path.string().c_str()) &&
            session.openTerrain();
  }
  ~TerrainUiFixture() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  void render() {
    session.update();
    metrics=font.metrics(ui::UiFontWeight::Regular);
    list.begin(session.screen().surface,metrics);
    router.beginFrame();
    editor::buildEditorScreen(session.screen(), editor::editorTheme(), list,
                              router);
  }
  bool point(u32 widget, ui::UiPoint &out) const {
    const auto panel = editor::terrainPanelRect(session.screen());
    for (float y = panel.y + 2; y < panel.bottom(); y += 4)
      for (float x = panel.x + 2; x < panel.right(); x += 4)
        if (router.hitTest({x, y}).widgetId == widget) {
          out = {x, y};
          return true;
        }
    return false;
  }
  void scroll(float amount) {
    render();
    const auto window = editor::terrainContentRect(
        session.screen(), editor::terrainPanelRect(session.screen()));
    const ui::UiPoint from{window.x + window.width * .5f,
                           window.y + window.height * .5f};
    const ui::UiPoint to{from.x, from.y - amount};
    time += 1;
    session.handlePointer({91, ui::UiPointerPhase::Down, from, time});
    session.handlePointer({91, ui::UiPointerPhase::Move, to, time + .1});
    session.handlePointer({91, ui::UiPointerPhase::Up, to, time + .2});
  }
  bool tap(u32 widget, bool seek = true) {
    render();
    ui::UiPoint p;
    if (!point(widget, p) && seek) {
      scroll(-10000);
      for (u32 attempt = 0; attempt < 96; ++attempt) {
        render();
        if (point(widget, p))
          break;
        const float before = session.screen().terrainScroll;
        scroll(24);
        if (session.screen().terrainScroll == before)
          break;
      }
    }
    render();
    if (!point(widget, p))
      return false;
    time += 1;
    session.handlePointer({92, ui::UiPointerPhase::Down, p, time});
    session.handlePointer({92, ui::UiPointerPhase::Up, p, time + .1});
    return true;
  }
  bool slide(u32 field,float fraction,u32 column=0) {
    const auto widget=id(Widget::TerrainSliderBase,field+column*0x100);
    scroll(-10000);
    ui::UiPoint p;
    for(u32 attempt=0;attempt<96;++attempt) {
      render();if(point(widget,p))break;
      const auto before=session.screen().terrainScroll;scroll(24);
      if(before==session.screen().terrainScroll)return false;
    }
    render();if(!point(widget,p))return false;
    auto area=editor::terrainContentRect(session.screen(),editor::terrainPanelRect(session.screen()));
    if(column){area.width*=.5f;if(column==2)area.x+=area.width;}
    p.x=area.x+10+(area.width-20)*fraction;
    time+=1;
    session.handlePointer({93,ui::UiPointerPhase::Down,p,time});
    session.handlePointer({93,ui::UiPointerPhase::Up,p,time+.1});
    return true;
  }
  bool text(std::string_view part) {
    render();
    for (const auto &command : list.commands())
      if (command.kind == ui::UiPrimitive::Text &&
          list.textOf(command).find(part) != std::string_view::npos)
        return true;
    return false;
  }
  bool finish() {
    for (u32 i = 0; i < 5000 && session.screen().terrainBusy; ++i) {
      session.refreshTerrainJob();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !session.screen().terrainBusy;
  }
  bool visit(u32 tab, u32 section = 0) {
    return tap(id(Widget::TerrainTabBase, tab), false) &&
           ((tab==2 || tab==3) || tap(id(Widget::TerrainSectionBase, section), false));
  }
};

void fields(std::set<u32> &ids, std::initializer_list<u32> indexes) {
  for(auto index:indexes) {
    ids.insert(id(Widget::TerrainNumberBase,index));
    if(index==0 || (index>=11 && index<=13) || index==27) {
      ids.insert(id(Widget::TerrainMinusBase,index));
      ids.insert(id(Widget::TerrainPlusBase,index));
    } else ids.insert(id(Widget::TerrainSliderBase,index));
  }
}
std::set<u32> contentControls(u32 mode, u32 tab, u32 section) {
  std::set<u32> result;
  if (tab == 0 && section == 0) {
    result.insert(id(Widget::TerrainModeBase, mode));
    fields(result, {1, 2});
    if (mode == 0) {
      result.insert(id(Widget::TerrainSurface));
      result.insert(id(Widget::TerrainSolidBase));
      fields(result, {14});
    } else
      fields(result, {7});
  } else if (tab == 0 && section == 1) {
    for (u32 i = 0; i < 4; ++i)
      result.insert(id(Widget::TerrainPresetBase, i));
    fields(result, {0, 3, 4, 5, 6});
    result.insert(id(Widget::TerrainIsland));
  } else if (tab == 0) {
    fields(result, {8});
    if(mode==0)fields(result,{60});
    if (mode != 0)
      fields(result, {7});
    result.insert(id(Widget::TerrainCollision));
  } else if (tab == 1 && section == 0) {
    for (u32 i = 0; i < 13; ++i)
      if (i!=6 && (mode==0?(i!=4&&i!=5):i!=12))
        result.insert(id(Widget::TerrainToolBase, i));
    for(u32 i:{9u,10u,15u}) {
      result.insert(id(Widget::TerrainNumberBase,i));
      result.insert(id(Widget::TerrainSliderBase,i));
    }
    fields(result,{16,17,25});
    // Fixture opens Raise: volume-only shapes become enabled with Dig/Add.
    for(u32 i=0;i<2;++i)result.insert(id(Widget::TerrainShapeBase,i));
  } else if (tab == 1 && section==1) {
    fields(result, {9, 10, 11, 12, 13,16,17,18,26,27});
    result.insert(id(Widget::TerrainHeightLock));
    result.insert(id(Widget::TerrainStamp));
    result.insert(id(Widget::TerrainRemoveStroke));
    result.insert(id(Widget::TerrainTouchOffset));
  } else if(tab==1) {
    fields(result,{9,17,20,21,16,22,23});
    result.insert(id(Widget::TerrainDimensionsLinked));
  } else if (tab == 2) {
    for (u32 i = 0; i < 4; ++i)
      result.insert(id(Widget::TerrainLayerBase, i));
    fields(result, {8, 9, 10,25});
    result.insert(id(Widget::TerrainOpacityTarget));
    result.insert(id(Widget::TerrainBlend));
    result.insert(id(Widget::TerrainNumberBase,19));
    result.insert(id(Widget::TerrainSliderBase,19));
  } else {
    for (auto widget :
         {Widget::TerrainNew, Widget::TerrainLoad, Widget::TerrainSave,
          Widget::TerrainRevert, Widget::TerrainCollision})
      result.insert(id(widget));
  }
  return result;
}
} // namespace

AE_TEST(
    terrain_ui_all_advanced_controls_scroll_into_view_without_covering_footer) {
  for (const auto size : {ui::UiPoint{1280, 720}, ui::UiPoint{655, 300}}) {
    TerrainUiFixture f(size.x, size.y);
    AE_EXPECT_TRUE(f.ready, "terrain UI fixture initialized");
    for (u32 mode = 0; mode < 3; ++mode) {
      resources::TerrainRecipe recipe;
      recipe.mode = resources::TerrainMode(mode);
      recipe.cells = recipe.verticalCells = 8;
      recipe.solidBase = true;
      recipe.strokes.push_back(
          {resources::TerrainTool::Raise, 0, 0, 0, 4, 1, 0});
      std::string error;
      AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
      ui::UiPointerRouting chooseRaise;
      chooseRaise.tapped=true;chooseRaise.widgetId=id(Widget::TerrainToolBase);
      f.session.handleTerrainInput({},chooseRaise);
      for (u32 tab = 0; tab < 4; ++tab) {
        const u32 sections = tab < 2 ? 3 : 1;
        for (u32 section = 0; section < sections; ++section) {
          AE_EXPECT_TRUE(f.visit(tab, section),
                         "tabs and subsections respond to actual touch");
          AE_EXPECT_EQ(f.session.screen().terrainTab, tab,
                       "selected tab shown");
          AE_EXPECT_EQ(f.session.screen().terrainSection, section,
                       "selected subsection shown");
          AE_EXPECT_EQ(f.session.screen().terrainScroll, 0.f,
                       "new section starts at top");
          const auto expected = contentControls(mode, tab, section);
          std::set<u32> seen;
          f.render();
          ui::UiPoint footer;
          const auto footerId=id(tab==1||tab==2?Widget::TerrainSculpt:Widget::TerrainSave);
          const auto initialPanel=editor::terrainPanelRect(f.session.screen());
          const auto initialWindow=editor::terrainContentRect(f.session.screen(),initialPanel);
          bool foundFooter=false;
          for(float y=initialWindow.bottom();y<initialPanel.bottom();y+=4)
            for(float x=initialPanel.x;x<initialPanel.right();x+=4)
              if(f.router.hitTest({x,y}).widgetId==footerId){footer={x,y};foundFooter=true;}
          AE_EXPECT_TRUE(foundFooter,"current action visible before scrolling");
          for (u32 step = 0; step < 32; ++step) {
            f.render();
            const auto panel = editor::terrainPanelRect(f.session.screen());
            const auto window =
                editor::terrainContentRect(f.session.screen(), panel);
            if (size.y == 300)
              AE_EXPECT_TRUE(
                  window.height >= 90,
                  "phone leaves useful space for scrolling controls");
            AE_EXPECT_TRUE(footer.y >= window.bottom() &&
                               footer.y < panel.bottom(),
                           "footer stays outside scrolling content");
            AE_EXPECT_EQ(f.router.hitTest(footer).widgetId,
                         footerId,
                         "scrolling never covers current action");
            for (float y = panel.y + 2; y < panel.bottom(); y += 4)
              for (float x = panel.x + 2; x < panel.right(); x += 4) {
                auto widget = f.router.hitTest({x, y}).widgetId;
                if(widget>=id(Widget::TerrainSliderBase) && widget<id(Widget::TerrainSliderBase)+0x300)
                  widget=id(Widget::TerrainSliderBase)+(widget-id(Widget::TerrainSliderBase))%0x100;
                if (!expected.contains(widget) || (widget==footerId && !window.contains({x,y})))
                  continue;
                AE_EXPECT_TRUE(window.contains({x, y}),
                               "offscreen field cannot intercept header or "
                               "footer touches");
                seen.insert(widget);
              }
            const float before = f.session.screen().terrainScroll;
            f.scroll(24);
            if (f.session.screen().terrainScroll == before)
              break;
          }
          AE_EXPECT_EQ(seen, expected,
                       "every field, stepper and advanced action is reachable "
                       "by touch scrolling");
          if (tab == 3) {
            AE_EXPECT_TRUE(f.text("Biblioteca total:"),
                           "management exposes retained library budget");
            AE_EXPECT_TRUE(f.text("Partes retidas:"),
                           "management exposes retained geometry budget");
          }
          f.scroll(-10000);
          AE_EXPECT_EQ(f.session.screen().terrainScroll, 0.f,
                       "reverse drag returns to top and clamps");
        }
      }
    }
  }
}

AE_TEST(terrain_ui_structure_slider_and_explicit_mode_selection) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.solidBase,
                 "new editor terrain starts with closed base");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainSurface)),
                 "surface button reachable on phone");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.solidBase,
                 "surface button opens base");
  AE_EXPECT_TRUE(!f.slide(14,.5f),
                 "surface disables base thickness editing");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainSolidBase)),
                 "solid base button reachable after scrolling");
  const auto originalDepth = f.session.screen().terrainDraft.baseDepth;
  AE_EXPECT_TRUE(f.slide(14,.5f),
                 "increase thickness via touch");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.baseDepth > originalDepth,
                 "plus affects serialized base depth");
  AE_EXPECT_TRUE(f.slide(14,0.f),
                 "decrease thickness via touch");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.baseDepth<originalDepth,
               "slider reaches minimum thickness");
  for (u32 mode : {2u, 0u, 1u}) {
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainModeBase, mode)),
                   "requested geometry mode has its own touch target");
    AE_EXPECT_EQ(u32(f.session.screen().terrainDraft.mode), mode,
                 "mode selection selects explicit mode rather than cycling");
  }
}

AE_TEST(terrain_ui_presets_preserve_sculpting_and_mode_lock) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  resources::TerrainRecipe recipe;
  recipe.cells = 8;
  recipe.seed = 837;
  recipe.solidBase = true;
  recipe.strokes = {{resources::TerrainTool::Raise, 3, 2, -4, 6, 2, 0},
                    {resources::TerrainTool::Paint, -2, 1, 5, 3, .5f, 2}};
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto applied = f.session.screen().terrainAppliedRecipe;
  AE_EXPECT_TRUE(f.visit(0, 1), "open relief subsection");
  float hillsHeight = 0;
  for (u32 preset = 0; preset < 4; ++preset) {
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPresetBase, preset)),
                   "preset can be selected through scroll");
    const auto &draft = f.session.screen().terrainDraft;
    auto normalized = draft;
    normalized.height = recipe.height;
    normalized.frequency = recipe.frequency;
    normalized.base = recipe.base;
    normalized.octaves = recipe.octaves;
    normalized.island = recipe.island;
    AE_EXPECT_EQ(
        resources::serializeTerrainRecipe(normalized),
        resources::serializeTerrainRecipe(recipe),
        "preset preserves strokes, seed, size, materials and structure");
    AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe, applied,
                 "preset remains a draft until applied");
    if (preset == 0)
      AE_EXPECT_EQ(draft.height, 0.f,
                   "flat preset removes procedural relief amplitude");
    if (preset == 1) {
      hillsHeight = draft.height;
      AE_EXPECT_TRUE(hillsHeight > 0, "hills have relief");
    }
    if (preset == 2)
      AE_EXPECT_TRUE(draft.height > hillsHeight,
                     "mountains are higher than hills");
    if (preset == 3)
      AE_EXPECT_TRUE(draft.island, "island preset enables island edges");
  }
  AE_EXPECT_TRUE(f.visit(0, 0), "return to structure");
  AE_EXPECT_TRUE(!f.tap(id(Widget::TerrainModeBase, 1)),
                 "sculpted terrain does not expose destructive mode change");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.mode,
               resources::TerrainMode::Heightfield, "mode remains unchanged");
}

AE_TEST(terrain_ui_pending_apply_revert_and_save_follow_real_scene_revision) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  resources::TerrainRecipe recipe;
  recipe.cells = 8;
  recipe.solidBase = true;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto source =
      scene::importLink(f.session.document().find(root)->components)->source;
  const auto applied = f.session.screen().terrainAppliedRecipe;
  AE_EXPECT_TRUE(f.slide(14,.5f),
                 "edit thickness in real UI");
  AE_EXPECT_TRUE(f.text("Por aplicar"), "footer exposes pending revision");
  AE_EXPECT_EQ(
      scene::importLink(f.session.document().find(root)->components)->source,
      source, "draft does not mutate published geometry");
  AE_EXPECT_TRUE(f.visit(3), "open management tab");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainSave)), "save action reachable");
  AE_EXPECT_TRUE(!f.session.saveRequested(),
                 "save refuses uncommitted terrain settings");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("Aplique") !=
                     std::string::npos,
                 "refused save explains next action");
  AE_EXPECT_TRUE(f.text("Aplique ou reverta"),
                 "compact footer keeps the refusal message visible");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainRevert)), "revert action reachable");
  AE_EXPECT_EQ(
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
      applied, "revert restores persisted source recipe");
  AE_EXPECT_TRUE(f.text("Aplicado"),
                 "footer clears pending state after revert");
  AE_EXPECT_TRUE(f.visit(0), "return to structure");
  AE_EXPECT_TRUE(f.slide(14,.5f),
                 "edit thickness again");
  const auto expected =
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate), false),
                 "fixed footer launches apply");
  AE_EXPECT_TRUE(f.session.screen().terrainBusy,
                 "apply enters generation state");
  AE_EXPECT_TRUE(f.finish(), "generation finishes");
  AE_EXPECT_EQ(f.session.screen().terrainTarget, root,
               "apply preserves edited root");
  AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe, expected,
               "generation publishes exact pending recipe");
  AE_EXPECT_TRUE(
      scene::importLink(f.session.document().find(root)->components)->source !=
          source,
      "applied geometry has a new immutable source");
  AE_EXPECT_TRUE(f.visit(3) && f.tap(id(Widget::TerrainSave)),
                 "save available after apply");
  AE_EXPECT_TRUE(f.session.saveRequested(),
                 "applied terrain requests actual scene save");
  const auto scenePath = f.path / "ui-scene.aescene";
  AE_EXPECT_TRUE(f.session.save(scenePath.string().c_str(), 0),
                 "consumer saves requested scene");
  AE_EXPECT_TRUE(f.session.load(scenePath.string().c_str(), 0) &&
                     f.session.openTerrain(root),
                 "saved scene reopens for editing");
  AE_EXPECT_EQ(
      resources::serializeTerrainRecipe(f.session.screen().terrainDraft),
      expected, "saved terrain keeps applied thickness and recipe");
}

AE_TEST(terrain_ui_armed_brush_replaces_gizmo_hit_without_moving_root) {
  TerrainUiFixture f(1280, 720);
  AE_EXPECT_TRUE(f.ready, "fixture ready");
  resources::TerrainRecipe recipe;
  recipe.cells = 16;
  recipe.height = 0;
  recipe.solidBase = true;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe, error), error.c_str());
  const auto root = f.session.screen().terrainTarget;
  const auto source =
      scene::importLink(f.session.document().find(root)->components)->source;
  float rootBefore[16];
  AE_EXPECT_TRUE(
      editor::editorWorldMatrix(f.session.document(), root, rootBefore),
      "terrain has a world transform");
  AE_EXPECT_TRUE(f.visit(2) && f.tap(id(Widget::TerrainLayerBase, 2)),
                 "select terrain painting through the real panel");
  f.render();
  const auto isGizmo = [](u32 widget) {
    return widget >= id(Widget::GizmoAxisBase) &&
           widget < id(Widget::GizmoAxisBase, 6);
  };
  const auto candidates = f.session.pickCandidates();
  const auto viewport = f.session.view().rect;
  ui::UiPoint covered{};
  bool found = false;
  for (float y = viewport.y + 2; y < viewport.bottom() && !found; y += 2)
    for (float x = viewport.x + 2; x < viewport.right(); x += 2) {
      const ui::UiPoint point{x, y};
      if (f.router.hitTest(point).target != ui::UiPointerTarget::Viewport)
        continue;
      const auto hit = editor::pickNearest(
          candidates, editor::screenPointToRay(f.session.view(), point), 0,
          true);
      const auto *entity =
          hit.hit ? f.session.document().find(hit.id) : nullptr;
      const auto *link =
          entity ? scene::importLink(entity->components) : nullptr;
      if (link && !link->root && link->source == source) {
        covered = point;
        found = true;
        break;
      }
    }
  AE_EXPECT_TRUE(found, "visible terrain has a real pickable viewport point");
  AE_EXPECT_TRUE(f.session.screen().terrainBrushArmed,
                 "selecting a layer arms painting immediately");
  f.render();
  AE_EXPECT_EQ(f.router.hitTest(covered).target, ui::UiPointerTarget::Viewport,
               "armed brush exposes the terrain at the former gizmo hit");
  for (float y = viewport.y + 2; y < viewport.bottom(); y += 4)
    for (float x = viewport.x + 2; x < viewport.right(); x += 4)
      AE_EXPECT_TRUE(!isGizmo(f.router.hitTest({x, y}).widgetId),
                     "armed terrain brush leaves no transform gizmo targets");

  f.time += 1;
  f.session.handlePointer({93, ui::UiPointerPhase::Down, covered, f.time});
  f.session.handlePointer({93, ui::UiPointerPhase::Up, covered, f.time + .15});
  AE_EXPECT_TRUE(f.session.screen().terrainBusy,
                 "touch formerly intercepted by gizmo applies a brush gesture");
  AE_EXPECT_TRUE(f.finish(), "painting revision finishes");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(), 1u,
               "stationary touch adds one painting sample");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.front().tool,
               resources::TerrainTool::Paint, "sample uses the selected brush");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.front().layer, 2u,
               "sample paints the selected terrain layer");
  float rootAfter[16];
  AE_EXPECT_TRUE(
      editor::editorWorldMatrix(f.session.document(), root, rootAfter),
      "painted terrain keeps a valid world transform");
  AE_EXPECT_TRUE(std::equal(rootBefore, rootBefore + 16, rootAfter),
                 "painting does not translate, rotate or scale the root");
  AE_EXPECT_TRUE(
      scene::importLink(f.session.document().find(root)->components)->source !=
          source,
      "painting publishes a new terrain source");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNavigate)),
                 "switch to navigation through touch");
  f.render();
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed,"navigation disarms brush");
  for (float y=viewport.y+2;y<viewport.bottom();y+=4)
    for(float x=viewport.x+2;x<viewport.right();x+=4)
      AE_EXPECT_TRUE(!isGizmo(f.router.hitTest({x,y}).widgetId),"navigation keeps terrain transform protected");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainClose)),"close dedicated terrain workspace");
  f.render();
  bool gizmoReturned=false;
  for(float y=0;y<720;y+=4)for(float x=0;x<1280;x+=4)
    gizmoReturned|=isGizmo(f.router.hitTest({x,y}).widgetId);
  AE_EXPECT_TRUE(gizmoReturned,"normal object transform controls return outside terrain editing");
}

AE_TEST(terrain_path_ui_create_edit_preview_apply_and_reopen) {
  TerrainUiFixture f(1280,720);AE_EXPECT_TRUE(f.ready,"UI fixture ready");
  resources::TerrainRecipe r;r.cells=16;r.height=0;std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.visit(4),"Caminhos is reachable by touch");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPathNew)),"create persistent path");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths.size(),usize(1),"path exists as a separate resource");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPointInsert)),"insert retained point");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].points.size(),usize(3),"inserted point retained");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(46,8),"move selected point");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPointCurve)),"curve the outgoing segment");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPathFollowTerrain)),"select authored point heights through UI");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.paths[0].followTerrain,"height mode changes real recipe");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(45,12),"intermediate point height editable");
  AE_EXPECT_TRUE(f.visit(4,1),"profile section opens");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(32,6),"path width edits independently of brush");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].shoulderWidth,12.f,"linked shoulder follows width ratio");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainPathRatio)),"unlink widths");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(33,16),"set shoulder width");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].width,6.f,"unlinked shoulder preserves road width");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(34,2),"raise path relative to terrain");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainProfileInsert)),"insert a retained curve node");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(50,.3),"edit the selected node position");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(51,.8),"edit the selected node value");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainProfileInsert)),"insert another independent curve node");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].profile.size(),usize(4),"editing keeps all curve nodes");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(51,.2),"edit the fourth-node profile");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].profile[1].y,.8f,"unselected node is not overwritten");
  f.render();ui::UiPoint curvePoint;
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainPathNew)+0x500+1,curvePoint),"curve node has a direct touch target");
  const auto profileBefore=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  f.time+=1;
  f.session.handlePointer({72,ui::UiPointerPhase::Down,curvePoint,f.time});
  const ui::UiPoint moved{curvePoint.x,curvePoint.y+12};
  f.session.handlePointer({72,ui::UiPointerPhase::Move,moved,f.time+.1});
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.paths[0].profile[1].y<.8f,"dragging a curve node changes its actual influence");
  f.session.handlePointer({72,ui::UiPointerPhase::Cancel,moved,f.time+.2});
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),profileBefore,"cancel restores the complete curve draft");
  AE_EXPECT_TRUE(f.visit(4,2),"open independent material curve");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths[0].textureProfile.size(),usize(2),"geometry curve edits do not modify texture curve");

  for(u32 i=0;i<5000 && !f.session.screen().terrainLivePreview;++i){f.session.update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  AE_EXPECT_TRUE(f.session.screen().terrainLivePreview,"configuration preview is visible before Apply");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate)),"Apply remains reachable");
  AE_EXPECT_TRUE(f.finish(),"path generation completes");
  const auto serialized=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen terrain editor");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),serialized,"path and properties survive GLB reediting");
}

AE_TEST(terrain_lod_controls_touch_enable_edit_and_disable_in_compact_and_wide_layouts) {
  for(const float width:{655.f,1280.f}) {
    TerrainUiFixture f(width,width<1000?300:720);
    AE_EXPECT_TRUE(f.ready && f.visit(0,2),"open terrain details through actual UI");
    AE_EXPECT_TRUE(f.slide(28,1.f/3,1),"LOD plus is reachable by scrolling and touch");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,2u,"touch enables a reduced level");
    const auto before=f.session.screen().terrainDraft.lodRelativeError;
    AE_EXPECT_TRUE(f.slide(29,1,2),"error control is enabled for multiple levels");
    AE_EXPECT_TRUE(f.session.screen().terrainDraft.lodRelativeError>before,"error button changes authored policy");
    AE_EXPECT_TRUE(f.slide(28,0,1),"LOD minus remains touch reachable");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.lodLevels,1u,"touch disables additional levels");
    f.render();ui::UiPoint point;
    AE_EXPECT_TRUE(!f.point(id(Widget::TerrainSliderBase,0x200+29),point) &&
                   !f.point(id(Widget::TerrainNumberBase,29),point),
                   "disabled error field and increment do not receive touch input");
  }
}

AE_TEST(terrain_compact_path_profiles_fit_before_scrolling) {
  TerrainUiFixture f;AE_EXPECT_TRUE(f.ready,"compact UI fixture");
  AE_EXPECT_TRUE(f.visit(4) && f.tap(id(Widget::TerrainPathNew)),"new path");
  for(u32 section:{1u,2u}) {
    AE_EXPECT_TRUE(f.visit(4,section),"profile opens without scroll");f.render();
    for(u32 index:{0u,1u}) {ui::UiPoint at;
      AE_EXPECT_TRUE(f.point(id(Widget::TerrainPathNew)+0x500+index,at),"both ends of the complete curve are touch reachable on compact screen");
    }
  }
}

AE_TEST(terrain_path_handles_follow_applied_surface_and_reopen_without_gpu_preview) {
  TerrainUiFixture f(1280,720);AE_EXPECT_TRUE(f.ready,"fixture");
  resources::TerrainRecipe r;r.cells=32;r.height=0;
  resources::TerrainPath path;path.elevation=8;path.points={{-12,0,0,false},{0,0,0,false},{12,0,0,false}};r.paths={path};
  std::string error;AE_EXPECT_TRUE(f.session.applyTerrain(r,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.visit(4),"open applied path");f.render();
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild!=nullptr,"committed field retained for handles");
  const float at[]{0,12.05f,0};const auto projected=editor::projectWorldToScreen(f.session.view(),at);
  AE_EXPECT_TRUE(projected.valid,"raised path projects into the viewport");
  AE_EXPECT_EQ(f.router.hitTest(projected.screen).widgetId,id(Widget::TerrainPathNew)+0x101,"handle follows applied height instead of raw authored Y");
  const auto before=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  const auto historyBefore=f.session.history().undoDepth();
  f.time+=1;f.session.handlePointer({74,ui::UiPointerPhase::Down,projected.screen,f.time});
  const ui::UiPoint moved{projected.screen.x+24,projected.screen.y};
  f.session.handlePointer({74,ui::UiPointerPhase::Move,moved,f.time+.1});
  const auto &point=f.session.screen().terrainDraft.paths[0].points[1];
  AE_EXPECT_TRUE(point.x!=0 || point.z!=0,"viewport drag moves actual path point");
  AE_EXPECT_EQ(point.y,0.f,"surface-plane drag preserves authored Y");
  f.session.handlePointer({74,ui::UiPointerPhase::Cancel,moved,f.time+.2});
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),before,"cancel restores XYZ and recipe");
  AE_EXPECT_EQ(f.session.history().undoDepth(),historyBefore,"draft drag does not create committed history");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainClose)),"close authoring");f.session.update();
  AE_EXPECT_TRUE(!f.session.screen().terrainPreviewBuild,"closing releases the authoring field");
  AE_EXPECT_TRUE(f.session.openTerrain(root) && f.visit(4),"reopen applied path");
  const auto publications=f.session.screen().terrainPreviewUpdates;
  for(u32 i=0;i<5000 && !f.session.screen().terrainPreviewBuild;++i){f.session.update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  f.render();
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild!=nullptr,"reopen reconstructs authoring field asynchronously");
  AE_EXPECT_TRUE(!f.session.screen().terrainLivePreview && f.session.screen().terrainPreviewUpdates==publications,"reconstructing handles does not publish duplicate geometry");
  const auto reopened=editor::projectWorldToScreen(f.session.view(),at);
  AE_EXPECT_EQ(f.router.hitTest(reopened.screen).widgetId,id(Widget::TerrainPathNew)+0x101,"reopened handle retains surface alignment");
}

AE_TEST(terrain_volume_resolution_sliders_keep_the_combined_sampling_budget_valid) {
  TerrainUiFixture f(1280,720);
  AE_EXPECT_TRUE(f.ready,"fixture ready");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainModeBase,1)),"select smooth voxel through touch");
  AE_EXPECT_TRUE(f.slide(7,1),"vertical resolution slider reaches its maximum");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.verticalCells,64u,"vertical steps stay integral");
  AE_EXPECT_TRUE(f.slide(2,1,2),"horizontal resolution slider respects current vertical density");
  const auto &draft=f.session.screen().terrainDraft;
  AE_EXPECT_EQ(draft.cells,88u,"largest valid eight-cell step selected instead of an invalid 128");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(draft,error),error.c_str());
  AE_EXPECT_TRUE(f.slide(7,0),"reduce vertical density through touch");
  AE_EXPECT_TRUE(f.slide(2,1,2),"horizontal range expands when budget permits");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.cells,128u,"maximum detail remains available");
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}

AE_TEST(terrain_path_curve_sliders_preserve_node_order_and_profile_domains) {
  TerrainUiFixture f(1280,720);
  AE_EXPECT_TRUE(f.ready && f.visit(4) && f.tap(id(Widget::TerrainPathNew)),"create path through UI");
  AE_EXPECT_TRUE(f.visit(4,1) && f.tap(id(Widget::TerrainProfileInsert)),"insert an editable shoulder node");
  AE_EXPECT_TRUE(f.slide(50,1,1),"node position slider reaches its right boundary");
  const auto selected=f.session.screen().terrainProfilePoint;
  const auto &profile=f.session.screen().terrainDraft.paths.front().profile;
  AE_EXPECT_TRUE(selected>0 && selected+1<profile.size(),"interior node selected");
  AE_EXPECT_TRUE(profile[selected].x>profile[selected-1].x && profile[selected].x<profile[selected+1].x,"slider cannot cross adjacent nodes");
  AE_EXPECT_TRUE(f.slide(51,1,2),"geometry profile supports crest values");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.paths.front().profile[selected].y,2.f,"geometry keeps the complete -1 to 2 range");
  AE_EXPECT_TRUE(f.visit(4,2) && f.tap(id(Widget::TerrainProfileInsert)),"insert paint profile node");
  AE_EXPECT_TRUE(f.slide(51,1,2),"paint curve has its own normalized scale");
  const auto &draft=f.session.screen().terrainDraft;
  AE_EXPECT_EQ(draft.paths.front().textureProfile[f.session.screen().terrainProfilePoint].y,1.f,"paint opacity remains normalized");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(draft,error),error.c_str());
}

AE_TEST(terrain_material_map_resolution_slider_and_precise_entry_survive_reopen) {
  TerrainUiFixture f(655,300);
  AE_EXPECT_TRUE(f.ready && f.visit(0,2),"open detail settings on compact layout");
  const auto cells=f.session.screen().terrainDraft.cells;
  AE_EXPECT_TRUE(f.slide(60,120.f/504),"independent material map slider is reachable");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.controlCells,128u,"material resolution follows valid eight-cell steps");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.cells,cells,"material adjustment never changes mesh cells");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,60)),"precise map value remains accessible");
  AE_EXPECT_EQ(f.session.screen().numericCurrent,128.,"numeric editor opens the actual map value");
  AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),"",false),"cancel precise input");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(60,0),"zero preserves legacy evaluation explicitly");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(60,15) && !f.session.setTerrainNumber(60,513),"invalid map steps rejected");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(60,64),"precise entry selects a map resolution");
  auto recipe=f.session.screen().terrainDraft;recipe.cells=8;
  std::string error;
  AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen committed material map");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.controlCells,64u,"independent map setting persists through GLB");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.cells,8u,"low mesh resolution remains independent");
}

AE_TEST(terrain_catalog_touch_selection_sliders_and_vertical_scroll_are_distinct) {
  TerrainUiFixture f(655,300);
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),
                 "open actual material library by touch");
  AE_EXPECT_TRUE(f.session.screen().terrainCatalog && f.session.screen().terrainCatalog->materials.size()==82,
                 "touch opens real supplied catalog");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogItemBase)),"tap first real material thumbnail");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,2u,"selection opens effectful layer properties");
  AE_EXPECT_TRUE(f.slide(64,.1f,1),"actual compact scale slider touch");
  const auto scale=f.session.screen().terrainDraft.surface.layers[0].scaleX;
  AE_EXPECT_TRUE(scale>20 && scale<21,"scale slider changes authored metre value");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainSliderBase,64+0x100),point),"slider remains touch reachable");
  const auto before=f.session.screen().terrainScroll;f.time+=1;
  const ui::UiPoint to{point.x,point.y-70};
  f.session.handlePointer({94,ui::UiPointerPhase::Down,point,f.time});
  f.session.handlePointer({94,ui::UiPointerPhase::Move,to,f.time+.1});
  f.session.handlePointer({94,ui::UiPointerPhase::Up,to,f.time+.2});
  AE_EXPECT_TRUE(f.session.screen().terrainScroll>before,"vertical drag over bar scrolls properties");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].scaleX,scale,
               "vertical scroll never edits the slider value");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainMaterialMaps)) && f.slide(96,.25f),
                 "map factor is reachable through actual navigation and touch");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.layers[0].maps[0].factor-2.f)<.001f,
                 "map slider updates actual PBR factor");
}


AE_TEST(terrain_m05_noise_touch_sources_exact_input_and_independent_settings) {
  TerrainUiFixture f(655,300);
  const auto noiseId=[](u32 action){return id(editor::terrainNoiseAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(noiseId(0)),"open M05 noise through routed compact navigation");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,4u,"opens global detail");
  const auto catalog=f.session.screen().terrainCatalog;
  AE_EXPECT_TRUE(catalog && catalog->textures.size()>6,"source picker uses actual transported catalog");
  AE_EXPECT_TRUE(f.tap(noiseId(10)) && f.tap(noiseId(13)),"open source picker and advance a real page");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogPage,1u,"texture pagination advances");
  AE_EXPECT_TRUE(f.tap(noiseId(16)),"select first texture from page two through touch");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,4u,"selection returns to originating noise");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.detailNoise.map.texture==catalog->textures[6].identity,"source GUID is the selected catalog texture");
  AE_EXPECT_TRUE(f.tap(noiseId(5)),"enable source-backed detail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.detailNoise.enabled,"enable changes actual model");
  AE_EXPECT_TRUE(f.slide(129,.25f),"global strength slider routes to model");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.detailNoise.strength-2.f)<.001f,"global strength becomes two");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,128)),"frequency chip opens precise editor");
  AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),"32",true),"numeric text commit accepts frequency outside practical slider");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.detailNoise.scale,32.f,"exact value reaches global noise");
  AE_EXPECT_TRUE(f.slide(132,.25f),"participation is reachable by compact touch");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.layers[0].globalDetailParticipation-.25f)<.001f,"participation belongs to selected layer");
  AE_EXPECT_TRUE(f.tap(noiseId(45)) && f.tap(noiseId(3)) && f.slide(129,.5f),"local detail receives independent slider input");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,6u,"local detail selected");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.layers[0].detailNoise.strength-4.f)<.001f,"local detail changed");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.detailNoise.strength,2.f,"local adjustment preserves global strength");
  AE_EXPECT_TRUE(f.tap(noiseId(44)) && f.tap(noiseId(2)) && f.slide(129,.75f),"global distance has independent routed control");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.distanceNoise.strength-6.f)<.001f,"distance strength changed");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.detailNoise.strength,2.f,"distance preserves detail");
  AE_EXPECT_TRUE(f.tap(noiseId(45)) && f.tap(noiseId(4)) && f.slide(129,.125f),"local distance has its own routed controls");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.layers[0].distanceNoise.strength-1.f)<.001f,"local distance adjusted");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.distanceNoise.strength,6.f,"local distance preserves global distance");
  AE_EXPECT_TRUE(f.tap(noiseId(44)) && f.tap(noiseId(2)),"return to global distance");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(63)),false) && f.slide(132,.5f),"participation follows selected layer");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].globalDistanceParticipation,1.f,"first layer remains unchanged");
  AE_EXPECT_TRUE(std::abs(f.session.screen().terrainDraft.surface.layers[1].globalDistanceParticipation-.5f)<.001f,"second layer receives distance participation");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61)),false) && f.tap(id(Widget::TerrainMaterialLibrary)),"material library remains accessible after texture pagination");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogPage,0u,"material page is reset independently of prior texture page");
}

AE_TEST(terrain_m05_noise_fade_sliders_preserve_valid_order_at_both_edges) {
  TerrainUiFixture f(1280,720);
  const auto noiseId=[](u32 action){return id(editor::terrainNoiseAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(noiseId(0)),"open noise settings");
  AE_EXPECT_TRUE(f.slide(130,1,1),"fade start reaches its valid upper edge");
  auto noise=f.session.screen().terrainDraft.surface.detailNoise;
  AE_EXPECT_TRUE(noise.fadeStart>99 && noise.fadeStart<noise.fadeEnd,"start changes without crossing end");
  AE_EXPECT_TRUE(f.slide(131,0,2),"fade end reaches its valid lower edge");
  noise=f.session.screen().terrainDraft.surface.detailNoise;
  AE_EXPECT_TRUE(noise.fadeEnd>noise.fadeStart,"end remains strictly above start");
  AE_EXPECT_TRUE(f.slide(131,1,2),"end can expand again after touching near-equal boundary");
  noise=f.session.screen().terrainDraft.surface.detailNoise;
  AE_EXPECT_TRUE(noise.fadeEnd>2047 && noise.fadeEnd>noise.fadeStart,"end slider remains responsive");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,130)),"precise fade start remains reachable");
  AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),"12",true),"exact fade entry commits through numeric route");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.detailNoise.fadeStart,12.f,"precise start value persists");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}


AE_TEST(terrain_m06_normal_roles_and_antitile_sources_follow_routed_touch) {
  TerrainUiFixture f(1280,720);
  const auto noiseId=[](u32 action){return id(editor::terrainNoiseAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(noiseId(0)),"open noise UI");
  for(u32 role=0;role<3;++role) {
    AE_EXPECT_TRUE(f.tap(noiseId(23+role)),"select global normal role");
    AE_EXPECT_EQ(f.session.screen().terrainMaterialView,12u+role,"normal view identity");
    AE_EXPECT_TRUE(f.tap(noiseId(10)) && f.tap(noiseId(16)) && f.tap(noiseId(5)),"bind actual source and enable normal");
    AE_EXPECT_TRUE(f.slide(129,float(role+1)/8),"global normal strength slider");
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,148)),"precise participation chip");
    AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),"0.25",true),"exact normal participation route");
    AE_EXPECT_TRUE(f.tap(noiseId(7)),"toggle normal RGB to RG");
    AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.normalNoise[role].map.channel==resources::TerrainSurfaceChannel::NormalRG,"normal RG metadata applied");
    AE_EXPECT_TRUE(f.tap(noiseId(editor::terrainNoiseLocal(f.session.screen().terrainMaterialView)?44:45)) && f.slide(129,.75f),"switch scope and adjust local normal");
    const auto &surface=f.session.screen().terrainDraft.surface;
    AE_EXPECT_EQ(surface.normalNoise[role].strength,float(role+1),"global strength independent");
    AE_EXPECT_EQ(surface.layers[0].normalNoise[role].strength,6.f,"local normal strength independent");
    AE_EXPECT_EQ(surface.layers[0].globalNormalParticipation[role],.25f,"participation indexed by role");
    AE_EXPECT_TRUE(f.tap(noiseId(editor::terrainNoiseLocal(f.session.screen().terrainMaterialView)?44:45)),"return to global scope");
  }
  AE_EXPECT_TRUE(f.tap(noiseId(26)) && f.tap(noiseId(28)),"create anti-tile entry from actual normal");
  auto surface=f.session.screen().terrainDraft.surface;
  AE_EXPECT_EQ(surface.antiTileArray.entries.size(),usize(1),"entry created");
  const auto first=surface.antiTileArray.entries[0].identity;
  AE_EXPECT_TRUE(surface.layers[0].antiTileEntry==first && surface.antiTileArray.entries[0].normal.enabled && surface.antiTileArray.entries[0].normal.map.texture.valid(),"stable identity binds real enabled normal source");
  AE_EXPECT_TRUE(f.tap(noiseId(27)) && f.tap(noiseId(31)),"enable set and edit normal");
  AE_EXPECT_TRUE(!f.tap(noiseId(5)),"last active source cannot be disabled");
  AE_EXPECT_TRUE(f.tap(noiseId(26)) && f.tap(noiseId(32)),"open bound detail source");
  AE_EXPECT_TRUE(f.tap(noiseId(10)) && f.tap(noiseId(16)) && f.tap(noiseId(5)),"assign actual detail source");
  AE_EXPECT_TRUE(f.slide(129,.25f),"entry detail strength slider");
  AE_EXPECT_TRUE(f.tap(noiseId(26)) && f.slide(149,.5f),"layer anti-normal participation");
  AE_EXPECT_TRUE(f.slide(150,.25f,1) && f.slide(151,.75f,2),"independent detail and distance participation");
  AE_EXPECT_TRUE(f.tap(noiseId(28)) && f.tap(noiseId(29)),"create second identity and cycle back to first");
  surface=f.session.screen().terrainDraft.surface;
  AE_EXPECT_EQ(surface.antiTileArray.entries.size(),usize(2),"two real entries");
  AE_EXPECT_TRUE(surface.antiTileArray.entries[1].identity!=first && surface.layers[0].antiTileEntry==first,"binding selects actual stable GUID");
  AE_EXPECT_EQ(surface.antiTileArray.entries[0].detail.strength,2.f,"first entry edits retained");
  AE_EXPECT_EQ(surface.layers[0].antiTileParticipation[0],.5f,"normal participation retained");
  AE_EXPECT_EQ(surface.layers[0].antiTileParticipation[1],.25f,"detail participation retained");
  AE_EXPECT_EQ(surface.layers[0].antiTileParticipation[2],.75f,"distance participation retained");
  AE_EXPECT_TRUE(f.tap(noiseId(30)),"unbind layer without removing shared entries");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.surface.layers[0].antiTileEntry.valid(),"binding cleared");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}


AE_TEST(terrain_m07_resampling_modes_sources_ranges_and_exact_values_follow_touch) {
  TerrainUiFixture f(1280,720);
  const auto noiseId=[](u32 action){return id(editor::terrainNoiseAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(noiseId(0)) && f.tap(noiseId(35)),"open resampling through material noise navigation");
  AE_EXPECT_TRUE(f.tap(noiseId(36)) && f.tap(noiseId(37)) && f.tap(noiseId(38)),"independent albedo normal and height switches");
  AE_EXPECT_TRUE(f.tap(noiseId(39)),"select Constant mode");
  auto layer=f.session.screen().terrainDraft.surface.layers[0];
  AE_EXPECT_TRUE(layer.resampleAlbedo && layer.resampleNormal && layer.resampleHeightBlend,"all authored roles enabled");
  AE_EXPECT_TRUE(layer.resampleBlend==resources::TerrainDistanceBlend::Constant,"Constant selected");
  AE_EXPECT_TRUE(!f.tap(id(Widget::TerrainNumberBase,153)),"Constant does not expose unused distance entry");
  AE_EXPECT_TRUE(f.tap(noiseId(40)),"Noise mode seeds an actual usable source");
  layer=f.session.screen().terrainDraft.surface.layers[0];
  AE_EXPECT_TRUE(layer.resampleBlend==resources::TerrainDistanceBlend::Noise && layer.resampleNoise.enabled && layer.resampleNoise.map.texture.valid(),"Noise mode has effectful source immediately");
  AE_EXPECT_TRUE(f.slide(153,1,1) && f.slide(154,0,2),"resampling distance edges remain valid");
  layer=f.session.screen().terrainDraft.surface.layers[0];
  AE_EXPECT_TRUE(layer.resampleStart>99 && layer.resampleEnd>layer.resampleStart,"start and end cannot cross");
  AE_EXPECT_TRUE(f.slide(154,1,2),"distance end expands after touching lower boundary");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,153)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"12",true),"exact start distance routes to model");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,152)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"32",true),"precise frequency multiplier can exceed practical slider");
  AE_EXPECT_TRUE(f.tap(noiseId(42)),"edit reused Noise source settings");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,51u,"resampling noise editor view");
  AE_EXPECT_TRUE(f.tap(noiseId(10)) && f.tap(noiseId(16)),"actual transported texture picker works for resampling source");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,51u,"picker returns to resampling editor");
  AE_EXPECT_TRUE(f.slide(129,.25f),"Noise strength slider has its own model consumer");
  AE_EXPECT_TRUE(f.tap(noiseId(35)) && f.tap(noiseId(41)),"return and switch to Crossfade");
  layer=f.session.screen().terrainDraft.surface.layers[0];
  AE_EXPECT_TRUE(layer.resampleBlend==resources::TerrainDistanceBlend::Crossfade,"Crossfade selected explicitly");
  AE_EXPECT_EQ(layer.resampleScale,32.f,"mode switch preserves frequency multiplier");
  AE_EXPECT_EQ(layer.resampleStart,12.f,"precise start retained");
  AE_EXPECT_EQ(layer.resampleNoise.strength,2.f,"noise settings preserved independently of blend mode");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}


AE_TEST(terrain_m08_clusters_use_actual_distinct_materials_seven_maps_and_precise_transforms) {
  TerrainUiFixture f(1280,720);
  const auto clusterId=[](u32 action){return id(editor::terrainClusterAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(editor::terrainNoiseAction(0))) && f.tap(clusterId(0)),"open cluster authoring through actual navigation");
  AE_EXPECT_TRUE(f.tap(clusterId(2)),"stochastic can run on base material before variants exist");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].cluster.stochastic,"base stochastic flag retained");
  AE_EXPECT_TRUE(!f.tap(clusterId(1)),"cluster enable unavailable until two distinct variants exist");
  const auto catalog=f.session.screen().terrainCatalog;
  AE_EXPECT_TRUE(catalog && !catalog->materials.empty(),"real original material catalog loaded");
  u32 second=0;
  while(second<catalog->materials.size() && catalog->materials[second].layer.maps[0].texture==catalog->materials[0].layer.maps[0].texture) ++second;
  AE_EXPECT_TRUE(second<catalog->materials.size(),"catalog has another distinct albedo");
  const auto pick=[&](u32 source) {
    for(u32 page=0;page<source/6;++page) if(!f.tap(clusterId(17))) return false;
    return f.tap(clusterId(20+source%6));
  };
  AE_EXPECT_TRUE(f.tap(clusterId(5)) && pick(0),"first variant imports a real material");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,70u,"first variant transformation view");
  AE_EXPECT_TRUE(f.tap(clusterId(0)) && f.tap(clusterId(5)) && pick(0),"attempt duplicate material selection");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].cluster.variants.size(),usize(1),"duplicate material is not appended");
  AE_EXPECT_TRUE(f.session.screen().terrainMessage.find("duplicada")!=std::string::npos,"duplicate produces explicit explanation");
  AE_EXPECT_TRUE(pick(second),"choose distinct real albedo");
  AE_EXPECT_TRUE(f.tap(clusterId(0)) && f.tap(clusterId(1)),"enable complete two-variant cluster");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].cluster.enabled,"cluster enabled reaches model");
  AE_EXPECT_TRUE(f.tap(clusterId(3)),"quality has actual selectable levels");
  AE_EXPECT_TRUE(f.slide(164,.5f) && f.slide(165,.25f),"contrast and noise boost sliders route to cluster");
  AE_EXPECT_TRUE(f.tap(clusterId(40)),"edit first stable variant");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,160)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"32",true),"exact absolute metres per tile entry");
  AE_EXPECT_TRUE(f.slide(161,.75f,1) && f.slide(162,.25f,2) && f.slide(163,.75f),"variant offsets and degrees have real sliders");
  const auto identity=f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0].identity;
  AE_EXPECT_TRUE(f.tap(clusterId(7)),"open all variant map roles");
  for(u32 role=0;role<7;++role) {
    AE_EXPECT_EQ(f.session.screen().terrainMapRole,role,"map role remains in zero to six domain");
    AE_EXPECT_TRUE(f.slide(176,float(role+1)/8),"each PBR map has an effectful factor slider");
    if(role<6) AE_EXPECT_TRUE(f.tap(clusterId(12)),"advance actual PBR role");
  }
  AE_EXPECT_TRUE(f.tap(clusterId(9)) && f.tap(clusterId(32)),"variant map source picker selects actual transported texture");
  auto cluster=f.session.screen().terrainDraft.surface.layers[0].cluster;
  AE_EXPECT_TRUE(cluster.variants[0].identity==identity,"map editing preserves stable variant identity");
  AE_EXPECT_EQ(cluster.variants[0].scale,32.f,"absolute tile size retained");
  AE_EXPECT_EQ(cluster.variants[0].offsetX,1024.f,"offset X is metres");
  AE_EXPECT_EQ(cluster.variants[0].offsetY,-1024.f,"offset Y is metres");
  AE_EXPECT_EQ(cluster.variants[0].rotation,90.f,"rotation is degrees");
  for(u32 role=0;role<7;++role) AE_EXPECT_EQ(cluster.variants[0].maps[role].factor,float(role+1),"all seven independent map factors retained");
  AE_EXPECT_TRUE(cluster.variants[1].identity==catalog->materials[second].layer.identity,"catalog variant identity remains stable");
  for(u32 role=0;role<7;++role) AE_EXPECT_TRUE(cluster.variants[1].maps[role].texture==catalog->materials[second].layer.maps[role].texture,"original complete map set preserved");
  AE_EXPECT_TRUE(f.tap(clusterId(0)) && f.tap(clusterId(41)) && f.tap(clusterId(6)),"remove selected variant through UI");
  cluster=f.session.screen().terrainDraft.surface.layers[0].cluster;
  AE_EXPECT_EQ(cluster.variants.size(),usize(1),"removal preserves remaining authored variant");
  AE_EXPECT_TRUE(!cluster.enabled && cluster.stochastic,"incomplete cluster disables while base stochastic remains authored");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}


AE_TEST(terrain_m09_geo_original_sources_ranges_layer_filter_and_weighted_curve_authoring) {
  TerrainUiFixture f(655,300);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(moduleId(0)),"compact UI discovers real Geo module");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.heightContrast,10.f,"actual source default contrast is ten");
  AE_EXPECT_TRUE(f.tap(moduleId(60)) && f.tap(moduleId(9)) && f.tap(moduleId(49)),"explicit original plugin pair authoring");
  auto geo=f.session.screen().terrainDraft.surface.geo;
  resources::AssetGuid originalAlbedo,originalNormal;
  resources::AssetGuid::parse("863f19b183b2443859f959a96f7dce32",originalAlbedo);
  resources::AssetGuid::parse("192aaea3237044f608d663c9884e0c70",originalNormal);
  AE_EXPECT_TRUE(geo.mode==resources::TerrainGeoMode::AlbedoNormal && geo.albedo.texture==originalAlbedo && geo.normal.texture==originalNormal,"real Geo GUIDs reach model");
  AE_EXPECT_TRUE(f.slide(191,.5f) && f.slide(192,.25f),"Geo color and normal strengths are independent routed sliders");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,193)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"250",true),"exact vertical metres per repetition");
  AE_EXPECT_TRUE(f.tap(moduleId(60)) && f.tap(moduleId(11)) && f.tap(moduleId(25)),"Geo normal channel selector switches RGB/RG");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.geo.normal.channel==resources::TerrainSurfaceChannel::NormalRG,"Geo normal RG persisted in map");
  for(u32 field:{223u,224u,227u,228u,231u,232u,235u,236u}) AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"Geo normal RG rejects hidden scalar/blue/alpha fields");
  AE_EXPECT_TRUE(f.tap(moduleId(22)) && f.tap(moduleId(5)) && f.tap(moduleId(6)),"open and enable four-height range");
  AE_EXPECT_TRUE(f.slide(195,1) && f.slide(196,1) && f.slide(197,0),"range edges retain nondecreasing order including coincidence");
  geo=f.session.screen().terrainDraft.surface.geo;
  AE_EXPECT_TRUE(geo.range[0]<=geo.range[1] && geo.range[1]<=geo.range[2] && geo.range[2]<=geo.range[3],"all four world heights remain ordered");
  AE_EXPECT_TRUE(f.tap(moduleId(12)) && f.tap(moduleId(55)),"selected layer owns Geo height filter");
  AE_EXPECT_TRUE(f.slide(203,.25f) && f.slide(204,.25f) && f.slide(205,.75f),"layer influence and signed threshold and filter strength authoring");
  const auto &layer=f.session.screen().terrainDraft.surface.layers[0];
  AE_EXPECT_EQ(layer.geoParticipation,.25f,"Geo participation is per layer");
  AE_EXPECT_EQ(layer.geoHeightThreshold,-.5f,"negative threshold is preserved as inverse filter");
  AE_EXPECT_EQ(layer.geoHeightStrength,.75f,"filter strength is independent");
  AE_EXPECT_TRUE(f.tap(moduleId(7)) && f.tap(moduleId(8)) && f.tap(moduleId(42)),"enable real AnimationCurve and split exact segment");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.curve.keys.size(),usize(3),"key insertion preserves endpoint keys");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,207)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"1.25",true),"precise curve output uses stratum UV units");
  AE_EXPECT_TRUE(f.tap(moduleId(57)),"dedicated selected-key tangent authoring is reachable by compact touch");
  auto current=[&]() -> const resources::AnimationCurveKey & {return f.session.screen().terrainDraft.surface.geo.curve.keys[f.session.screen().terrainProfilePoint];};
  if(!current().broken) AE_EXPECT_TRUE(f.tap(moduleId(44)),"break tangent coupling");
  for(u32 attempt=0;attempt<7 && current().incoming!=resources::AnimationTangentMode::Free;++attempt) AE_EXPECT_TRUE(f.tap(moduleId(45)),"choose incoming Free mode");
  for(u32 attempt=0;attempt<7 && current().outgoing!=resources::AnimationTangentMode::Free;++attempt) AE_EXPECT_TRUE(f.tap(moduleId(46)),"choose outgoing Free mode");
  if(!current().weightedIn) AE_EXPECT_TRUE(f.tap(moduleId(47)),"enable weighted incoming handle");
  if(!current().weightedOut) AE_EXPECT_TRUE(f.tap(moduleId(48)),"enable weighted outgoing handle");
  AE_EXPECT_TRUE(f.slide(208,.75f) && f.slide(209,.25f) && f.slide(210,.7f) && f.slide(211,.6f),"independent slopes and Bezier handle weights are real sliders");
  AE_EXPECT_EQ(current().inSlope,8.f,"incoming slope changed");
  AE_EXPECT_EQ(current().outSlope,-8.f,"outgoing slope changed independently");
  AE_EXPECT_TRUE(std::abs(current().inWeight-.7f)<.001f && std::abs(current().outWeight-.6f)<.001f,"weighted handle fractions persisted");
  std::set<unsigned> modes;
  for(u32 attempt=0;attempt<7;++attempt) {modes.insert(unsigned(current().incoming));AE_EXPECT_TRUE(f.tap(moduleId(45)),"all incoming tangent modes cycle");}
  AE_EXPECT_EQ(modes.size(),usize(7),"all seven existing AnimationCurve tangent modes are available");
  AE_EXPECT_TRUE(f.tap(moduleId(59)) && f.tap(moduleId(54)),"curve graph hit testing selects a real authored key");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,255)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"2",true),"exact key selector accesses persisted key identity without truncation");
  AE_EXPECT_EQ(f.session.screen().terrainProfilePoint,1u,"key index selects middle key");
  AE_EXPECT_TRUE(f.tap(moduleId(58)),"curve axis has a compact separate authoring page");
  AE_EXPECT_TRUE(f.slide(201,.75f),"curve axis rotation is authored in degrees");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.curveRotationDegrees,90.f,"rotation degrees persist");
  std::string encoded,error;resources::TerrainSurfaceAsset restored;
  AE_EXPECT_TRUE(resources::serializeTerrainSurface(f.session.screen().terrainDraft.surface,encoded,error) && resources::deserializeTerrainSurface(encoded,restored,error),error.c_str());
  AE_EXPECT_EQ(restored.geo.curve.keys.size(),usize(3),"all curve keys persist through new format");
  AE_EXPECT_EQ(restored.geo.curve.keys[1].value,1.25f,"curve UV output persists cold");
  AE_EXPECT_TRUE(restored.geo.curve.keys[1].weightedIn && restored.geo.curve.keys[1].weightedOut,"weighted tangent flags persist cold");
}

AE_TEST(terrain_m10_four_global_maps_use_real_sources_saom_channels_domain_and_influences) {
  TerrainUiFixture f(1280,720);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(0,0) && f.tap(id(Widget::TerrainNumberBase,1)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"128",true),"terrain domain derives actual authored recipe size");
  AE_EXPECT_TRUE(f.visit(2) && f.tap(moduleId(13)),"open global map hub");
  f.scroll(500);
  AE_EXPECT_TRUE(f.text("128 m"),"hub displays actual terrain domain instead of a fixed UV extent");
  for(u32 role=0;role<4;++role) {
    AE_EXPECT_TRUE(f.tap(moduleId(14+role)) && f.tap(moduleId(21)),"open each independent global source editor");
    AE_EXPECT_TRUE(f.tap(moduleId(23)) && f.tap(moduleId(32)),"select transported real texture for global role");
    if(role==0) for(u32 field:{223u,224u}) AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"Tint rejects hidden scalar remaps");
    if(role==2) {
      for(u32 field=225;field<=236;++field) AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"SAOM rejects hidden color factor/remap properties");
      AE_EXPECT_TRUE(f.tap(moduleId(56)),"explicit packed-source reuse retains independent SAOM metadata");
      AE_EXPECT_TRUE(f.slide(223,.25f,1) && f.slide(224,.75f,2),"SAOM scalar remaps are authored");
      AE_EXPECT_TRUE(f.tap(moduleId(26)),"smoothness inversion is independently authored");
      for(u32 channel=0;channel<4;++channel) {
        const auto &g=f.session.screen().terrainDraft.surface.globalMaps[2];
        const auto &map=channel==0?g.smoothness:channel==1?g.ao:channel==2?g.metallic:g.mask;
        AE_EXPECT_EQ(unsigned(map.channel),channel,"SAOM default channels explicitly R/G/B/A");
        if(channel<3) AE_EXPECT_TRUE(f.tap(moduleId(29)),"select next independent packed role");
      }
    }
    if(role==3) {
      const auto before=f.session.screen().terrainDraft.surface.globalMaps[3].map;
      for(u32 field:{228u,232u,236u}) {
        AE_EXPECT_TRUE(!f.tap(id(Widget::TerrainNumberBase,field)),"Emission alpha fields have no executable UI control");
        AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"exact API rejects hidden Emission alpha edits");
      }
      const auto &after=f.session.screen().terrainDraft.surface.globalMaps[3].map;
      AE_EXPECT_TRUE(before.colorFactor[3]==after.colorFactor[3] && before.colorRemapMin[3]==after.colorRemapMin[3] && before.colorRemapMax[3]==after.colorRemapMax[3],"inactive alpha properties remain preserved");
    }
    if(role==1) {
      AE_EXPECT_TRUE(f.tap(moduleId(25)),"normal map has RGB/RG packing control");
      AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.globalMaps[1].map.channel==resources::TerrainSurfaceChannel::NormalRG,"global normal RG is a real enum");
      for(u32 field:{223u,224u,227u,228u,231u,232u,235u,236u}) AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"global normal RG rejects hidden scalar/blue/alpha controls");
    }
    AE_EXPECT_TRUE(f.tap(moduleId(22)) && f.tap(moduleId(18)),"enable source-backed global role");
    AE_EXPECT_TRUE(f.slide(212,.25f) && f.slide(215,.125f) && f.slide(216,.375f),"independent intensity and near/far strengths");
    AE_EXPECT_TRUE(f.slide(213,1,1) && f.slide(214,0,2) && f.slide(214,1,2),"global fade edges remain strictly ordered and responsive");
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,217)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"16",true),"exact global UV scale outside practical slider");
    AE_EXPECT_TRUE(f.slide(218,0,2),"global UV scale slider supports mirroring");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.globalMaps[role].scaleY,-8.f,"negative UV scale reaches actual map");
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,218)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"0",true),"exact zero UV scale supports constant coordinate");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.globalMaps[role].scaleY,0.f,"zero UV scale is preserved");
    AE_EXPECT_TRUE(f.slide(221,.75f),"current layer global influence is routed");
    const auto &surface=f.session.screen().terrainDraft.surface;
    AE_EXPECT_TRUE(surface.globalMaps[role].enabled,"all four role enable switches retained");
    AE_EXPECT_EQ(surface.globalMaps[role].scaleX,16.f,"UV scale per map retained");
    AE_EXPECT_EQ(surface.layers[0].globalMapParticipation[role],role==0?.75f:1.5f,"source-specific layer participation ranges preserved");
    if(role==0) {std::set<unsigned> blends;for(u32 i=0;i<4;++i) {blends.insert(unsigned(f.session.screen().terrainDraft.surface.globalMaps[0].blend));AE_EXPECT_TRUE(f.tap(moduleId(19)),"Tint blend cycles actual modes");}AE_EXPECT_EQ(blends.size(),usize(4),"all four Tint blends supported");}
    if(role==1) AE_EXPECT_TRUE(f.tap(moduleId(20)),"normal RNM/Crossfade is independent from color blend enum");
    AE_EXPECT_TRUE(f.tap(moduleId(13)),"return to four-map hub");
  }
  const auto &g=f.session.screen().terrainDraft.surface.globalMaps[2];
  AE_EXPECT_TRUE(g.smoothness.texture==g.ao.texture && g.smoothness.texture==g.metallic.texture && g.smoothness.texture==g.mask.texture,"compatible SAOM roles share actual source GUID");
  AE_EXPECT_TRUE(g.smoothness.invert && !g.ao.invert,"SAOM remap/inversion metadata remain independent despite shared source");
  std::string error;
  AE_EXPECT_TRUE(resources::validTerrainRecipe(f.session.screen().terrainDraft,error),error.c_str());
}


AE_TEST(terrain_m09_m10_direct_module_entry_initializes_fresh_and_legacy_drafts_only) {
  for(u32 action:{0u,13u}) for(bool applied:{false,true}) {
    TerrainUiFixture f(1280,720);
    AE_EXPECT_TRUE(f.ready,"direct-entry fixture");
    if(applied) {
      resources::TerrainRecipe legacy;legacy.cells=8;legacy.controlCells=0;legacy.surface.layers.clear();
      std::string error;AE_EXPECT_TRUE(f.session.applyTerrain(legacy,error),error.c_str());
      const auto root=f.session.screen().terrainTarget;
      AE_EXPECT_TRUE(f.session.openTerrain(root),"reopen applied legacy recipe");
    }
    const auto history=f.session.history().undoDepth();
    const auto target=f.session.screen().terrainTarget;
    AE_EXPECT_TRUE(f.visit(2) && f.tap(id(editor::terrainModuleAction(action))),"direct Geo/Global link opens without visiting old material/noise initialization");
    AE_EXPECT_EQ(f.session.screen().terrainMaterialView,action==0?120u:159u,"requested module opened");
    AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers.size(),usize(4),"four legacy layer definitions initialized in draft");
    AE_EXPECT_TRUE(f.session.screen().terrainDraft.blendedSurface,"advanced surface consumer selected in draft");
    AE_EXPECT_EQ(f.session.history().undoDepth(),history,"opening authoring does not commit scene history");
    AE_EXPECT_EQ(f.session.screen().terrainTarget,target,"opening authoring preserves applied entity identity");
  }
}


AE_TEST(terrain_geo_signed_empty_single_curve_authoring_preserves_all_keys_and_weights) {
  TerrainUiFixture f(655,300);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(moduleId(0)) && f.tap(moduleId(60)) && f.tap(moduleId(9)) && f.tap(moduleId(49)) && f.tap(moduleId(7)) && f.tap(moduleId(8)),"open enabled Geo curve through real compact UI");
  AE_EXPECT_TRUE(f.tap(moduleId(43)),"delete first key without endpoint lock");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.curve.keys.size(),usize(1),"one key remains as a constant curve");
  AE_EXPECT_TRUE(f.tap(moduleId(43)),"delete the final authored key");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.geo.curve.keys.empty(),"empty curve is preserved as zero output");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(206,-1) && !f.session.setTerrainNumber(207,1) && !f.session.setTerrainNumber(255,1),"empty numeric entry rejects without invalid indexing");
  AE_EXPECT_TRUE(!f.tap(moduleId(54)),"empty flat graph does not expose fake key selection");
  f.scroll(-10000);f.render();ui::UiPoint addAt;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"empty curve can return to zero scroll");
  AE_EXPECT_TRUE(f.point(moduleId(42),addAt),"Add first key is visible alongside empty graph at zero scroll");
  AE_EXPECT_TRUE(f.tap(moduleId(42),false),"Add key works from empty curve");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.curve.keys.size(),usize(1),"first key created");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,206)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"-2.5",true),"single key accepts exact signed time");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,207)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"1.75",true),"single key value authored");
  resources::AnimationCurveSample sample;
  AE_EXPECT_TRUE(resources::sampleValidatedAnimationCurve(f.session.screen().terrainDraft.surface.geo.curve,0,sample),"single key uses existing constant evaluator");
  AE_EXPECT_EQ(sample.value,1.75,"single key is constant throughout LUT domain");
  const auto firstId=f.session.screen().terrainDraft.surface.geo.curve.keys[0].id;
  AE_EXPECT_TRUE(f.tap(moduleId(42)),"Add key extends single-key curve");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.geo.curve.keys.size(),usize(2),"second key created without dropping original");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,206)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"3",true),"last key accepts exact time beyond LUT one");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,207)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"-0.5",true),"outside key affects real curve data");
  const auto lastId=f.session.screen().terrainDraft.surface.geo.curve.keys.back().id;
  AE_EXPECT_TRUE(f.tap(moduleId(40)),"select first signed key");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNumberBase,206)) && f.session.completeTextEdit(f.session.pendingTextEdit(),"-3",true),"first time remains editable without normalized endpoint lock");
  AE_EXPECT_TRUE(f.tap(moduleId(42)),"insert exact weighted curve subdivision between signed keys");
  const auto &curve=f.session.screen().terrainDraft.surface.geo.curve;
  AE_EXPECT_EQ(curve.keys.size(),usize(3),"interior key added without truncation");
  AE_EXPECT_TRUE(curve.keys.front().id==firstId && curve.keys.back().id==lastId,"existing signed key identities preserved");
  const auto domain=editor::terrainGeoCurveDomain(curve);
  AE_EXPECT_EQ(domain[0],-3.f,"graph includes all negative key times");
  AE_EXPECT_EQ(domain[1],3.f,"graph includes all key times beyond LUT one");
  AE_EXPECT_TRUE(f.tap(moduleId(57)),"weighted tangent page works on signed domain");
  const auto current=[&]() -> const resources::AnimationCurveKey & {return f.session.screen().terrainDraft.surface.geo.curve.keys[f.session.screen().terrainProfilePoint];};
  if(!current().broken) AE_EXPECT_TRUE(f.tap(moduleId(44)),"enable independent tangents");
  for(u32 i=0;i<7 && current().incoming!=resources::AnimationTangentMode::Free;++i) AE_EXPECT_TRUE(f.tap(moduleId(45)),"choose signed incoming Free mode");
  for(u32 i=0;i<7 && current().outgoing!=resources::AnimationTangentMode::Free;++i) AE_EXPECT_TRUE(f.tap(moduleId(46)),"choose signed outgoing Free mode");
  if(!current().weightedIn) AE_EXPECT_TRUE(f.tap(moduleId(47)),"weighted incoming handle");
  if(!current().weightedOut) AE_EXPECT_TRUE(f.tap(moduleId(48)),"weighted outgoing handle");
  AE_EXPECT_TRUE(f.slide(208,.75f) && f.slide(209,.25f) && f.slide(210,.7f) && f.slide(211,.6f),"signed curve preserves full weighted authoring");
  std::string encoded,error;resources::TerrainSurfaceAsset restored;
  AE_EXPECT_TRUE(resources::serializeTerrainSurface(f.session.screen().terrainDraft.surface,encoded,error) && resources::deserializeTerrainSurface(encoded,restored,error),error.c_str());
  AE_EXPECT_EQ(restored.geo.curve.keys.size(),usize(3),"cold read keeps all signed/outside keys");
  AE_EXPECT_EQ(restored.geo.curve.keys.front().time,-3.f,"negative time survives serialization");
  AE_EXPECT_EQ(restored.geo.curve.keys.back().time,3.f,"outside time survives serialization");
  AE_EXPECT_TRUE(restored.geo.curve.keys[1].weightedIn && restored.geo.curve.keys[1].weightedOut,"full weighted flags survive serialization");
  AE_EXPECT_TRUE(std::abs(restored.geo.curve.keys[1].inWeight-.7f)<.001f && std::abs(restored.geo.curve.keys[1].outWeight-.6f)<.001f,"weights survive serialization");
}


AE_TEST(terrain_compact_module_context_shows_live_controls_graph_sources_and_layer_navigation_at_zero_scroll) {
  TerrainUiFixture f(655,300);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(moduleId(0)),"compact Geo opens from painting");
  AE_EXPECT_TRUE(f.tap(moduleId(60)) && f.tap(moduleId(9)),"Fontes offers actual original Geo pair");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,121u,"dedicated real Fontes page");
  for(u32 action:{1u,2u,3u,4u}) AE_EXPECT_TRUE(f.tap(moduleId(action)),"all source-backed Geo modes and blend are reachable");
  AE_EXPECT_TRUE(f.tap(moduleId(10)) && f.tap(moduleId(22)),"albedo map editor returns to Fontes");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,121u,"source return preserves authoring context");
  AE_EXPECT_TRUE(f.tap(moduleId(11)) && f.tap(moduleId(22)),"normal map editor remains reachable");
  AE_EXPECT_TRUE(f.tap(moduleId(49)),"numeric Geo defaults replace stacked navigation");
  f.render();ui::UiPoint point;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"new context opens at zero scroll");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainNumberBase,191),point),"first real editable Geo value is visible without scrolling");
  AE_EXPECT_TRUE(!f.point(id(Widget::TerrainTabBase,2),point),"root tab row does not consume compact module viewport");
  const auto before=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  for(u32 layer=1;layer<=4;++layer) {
    AE_EXPECT_TRUE(f.tap(moduleId(63),false),"pinned next-layer selector is always reachable");
    AE_EXPECT_EQ(f.session.screen().terrainBrush.layer,layer%4,"all four real layer channels cycle");
  }
  AE_EXPECT_TRUE(f.tap(moduleId(62),false),"pinned previous layer selector is reachable");
  AE_EXPECT_EQ(f.session.screen().terrainBrush.layer,3u,"previous wraps actual layer channel");
  AE_EXPECT_TRUE(f.tap(moduleId(7)),"open real curve authoring");
  f.render();AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"curve begins at zero scroll");
  AE_EXPECT_TRUE(f.point(moduleId(54),point),"real curve graph is visible immediately on 655 by300");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainNumberBase,255),point),"selected curve key is visible alongside graph at zero scroll");
  AE_EXPECT_TRUE(f.tap(moduleId(61),false),"Back Pintura restores root authoring");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,0u,"breadcrumb returns to painting");
  f.render();AE_EXPECT_TRUE(f.point(id(Widget::TerrainTabBase,2),point),"normal root tabs are restored");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),before,"navigation and layer cycling preserve every authored property");
  AE_EXPECT_TRUE(f.tap(moduleId(13)),"global entry opens a real role directly on compact viewport");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,160u,"Tint role is compact global entry");
  f.render();AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"global role begins at zero scroll");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainNumberBase,212),point),"global intensity is visible at zero scroll");
  for(u32 role=0;role<4;++role) {
    AE_EXPECT_TRUE(f.tap(moduleId(14+role),false),"all four global roles remain in pinned navigation");
    AE_EXPECT_EQ(f.session.screen().terrainMaterialView,160u+role,"global role selection is actual model context");
  }
}


AE_TEST(terrain_compact_empty_global_role_explains_missing_source_and_enables_only_real_texture) {
  TerrainUiFixture f(655,300);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(moduleId(13)),"compact global entry on a fresh draft");
  f.render();ui::UiPoint point;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"empty role opens at zero scroll");
  AE_EXPECT_TRUE(f.text("Sem fonte"),"missing source state is explicit before numeric authoring");
  AE_EXPECT_TRUE(!f.point(moduleId(18),point),"missing source cannot activate a fake map");
  AE_EXPECT_TRUE(f.point(moduleId(21),point),"real source CTA visible at zero scroll");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainNumberBase,212),point),"intensity stays visible beside source state");
  AE_EXPECT_TRUE(f.tap(moduleId(21),false) && f.tap(moduleId(23)) && f.tap(moduleId(32)),"CTA chooses a transported original texture");
  AE_EXPECT_TRUE(f.tap(moduleId(22)),"return to prepared global role");
  f.render();AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"source return resets scroll");
  AE_EXPECT_TRUE(f.point(moduleId(18),point),"source-backed actual enable control becomes available");
  AE_EXPECT_TRUE(f.tap(moduleId(18),false),"toggle activates actual global map");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.globalMaps[0].enabled && f.session.screen().terrainDraft.surface.globalMaps[0].map.texture.valid(),"enabled state and real reference reach consumer model");
  AE_EXPECT_TRUE(f.slide(212,.25f),"authored intensity changes after activation");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.globalMaps[0].intensity,2.f,"actual intensity value persists");
}


AE_TEST(terrain_compact_legacy_noise_and_pbr_maps_keep_real_controls_visible_at_zero_scroll) {
  TerrainUiFixture f(655,300);
  const auto moduleId=[](u32 action){return id(editor::terrainModuleAction(action));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(editor::terrainNoiseAction(0))),"open existing Noise through real painting navigation");
  f.render();ui::UiPoint point;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"Noise starts at zero scroll");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainSliderBase,128),point),"existing frequency slider is visible and draggable immediately");
  AE_EXPECT_TRUE(f.point(id(editor::terrainNoiseAction(10)),point),"existing source picker remains in pinned Noise context");
  const auto before=f.session.screen().terrainDraft.surface.detailNoise.scale;
  AE_EXPECT_TRUE(f.slide(128,.5f),"actual Noise frequency slider input");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.detailNoise.scale!=before,"existing Noise parameter still changes");
  AE_EXPECT_TRUE(f.tap(moduleId(61),false) && f.tap(id(Widget::TerrainMaterialMaps)),"Back root opens existing PBR map editor");
  f.render();AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"PBR map page starts at zero scroll");
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainMapChannel),point),"actual linked-texture channel control is visible immediately");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[0].texture.valid(),"existing map control refers to real transported legacy texture");
  const auto channel=f.session.screen().terrainDraft.surface.layers[0].maps[0].channel;
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainMapChannel),false),"existing source channel receives actual touch");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[0].channel!=channel,"map source interpretation changes in real model");
  AE_EXPECT_TRUE(f.tap(moduleId(61),false),"legacy map context returns to painting root");
  f.render();AE_EXPECT_TRUE(f.point(id(Widget::TerrainTabBase,2),point),"unchanged root tabs are restored");
}
