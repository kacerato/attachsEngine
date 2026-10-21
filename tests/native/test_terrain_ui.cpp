#include "editor/editor_terrain_fx_cursor.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_theme.h"
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_terrain_modules_controls.h"
#include "editor/editor_terrain_catalog_search.h"
#include "editor/editor_terrain_snow_controls.h"
#include "editor/terrain_texture_icons.h"
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
    ui::UiPoint from{window.x + window.width * .5f,
                     window.y + window.height * .5f};
    // Scroll through an actual panel background/label, not an armed paint canvas.
    bool background=false;
    for(float y=window.y+2;y<window.bottom() && !background;y+=4)
      for(float x=window.x+2;x<window.right();x+=4)
        if(router.hitTest({x,y}).widgetId==id(Widget::TerrainScroll)){
          from={x,y};background=true;break;
        }
    if(!background && router.hitTest(from).widgetId==id(editor::terrainFxAction(90))) {
      for(float y=window.y+2;y<window.bottom() && !background;y+=4)
        for(float x=window.x+2;x<window.right();x+=4){
          const auto hit=router.hitTest({x,y});
          const bool slider=(hit.widgetId>=id(Widget::TerrainSliderBase) && hit.widgetId<id(Widget::TerrainSliderBase)+0x300) ||
            (hit.widgetId>=editor::terrainSnowSliderBase && hit.widgetId<editor::terrainSnowSliderBase+0x300);
          if(hit.target==ui::UiPointerTarget::Widget && hit.widgetId!=id(editor::terrainFxAction(90)) && !slider){from={x,y};background=true;break;}
        }
    }
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
    const auto widget=field>=256?editor::terrainSnowSliderBase+field-256+column*0x100:id(Widget::TerrainSliderBase,field+column*0x100);
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
  bool textureThumbnail(std::string_view catalogId) {
    render();std::string sourceId(catalogId);
    if(session.screen().terrainCatalog)for(const auto &source:session.screen().terrainCatalog->textures)
      if(source.identity.text()==catalogId){sourceId=source.id;break;}
    const auto expected=static_cast<ui::UiImageId>(editor::terrainTextureIcon(sourceId));
    for(const auto &command:list.commands())if(command.kind==ui::UiPrimitive::Image && command.image==expected &&
      command.bounds.width==32 && command.bounds.height==32 && command.clip.height>0 && command.clip.width>0)return true;
    return false;
  }
  bool snowMode(u32 action) {
    return tap(id(editor::terrainSnowAction(21))) && tap(id(editor::terrainSnowAction(action))) && tap(id(editor::terrainSnowAction(20)));
  }
  bool catalogQuery(std::string_view query) {
    return tap(id(editor::terrainCatalogAction(0))) &&
      session.completeTextEdit(session.pendingTextEdit(),query,true);
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

AE_TEST(terrain_sculpt_continuous_moves_publish_verified_prefix_before_release_and_cancel_atomically) {
  TerrainUiFixture f(1280,720);resources::TerrainRecipe recipe;recipe.cells=32;recipe.height=0;recipe.base=0;
  std::string error;AE_EXPECT_TRUE(f.ready && f.session.applyTerrain(recipe,error),error.c_str());
  AE_EXPECT_TRUE(f.visit(1) && f.tap(id(Widget::TerrainToolBase,u32(resources::TerrainTool::Raise))),"real Raise tool arms the viewport");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(9,4) && f.session.setTerrainNumber(10,.1),"soft brush dimensions authored");
  f.render();const float a[]{-3,0,0},b[]{3,0,0};
  const auto pa=editor::projectWorldToScreen(f.session.view(),a),pb=editor::projectWorldToScreen(f.session.view(),b);
  AE_EXPECT_TRUE(pa.valid && pb.valid && f.router.hitTest(pa.screen).target==ui::UiPointerTarget::Viewport && f.router.hitTest(pb.screen).target==ui::UiPointerTarget::Viewport,"both continuous samples reach the real surface");
  const auto before=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  const auto history=f.session.history().undoDepth(),publications=f.session.screen().terrainPreviewUpdates;
  f.time+=1;f.session.handlePointer({76,ui::UiPointerPhase::Down,pa.screen,f.time});
  ui::UiPoint point=pa.screen;
  for(u32 i=0;i<100 && f.session.screen().terrainPreviewUpdates==publications;++i){
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    point=i%2?pa.screen:pb.screen;
    f.session.handlePointer({76,ui::UiPointerPhase::Move,point,f.time+.01*(i+1)});
    f.render();
  }
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewUpdates>publications && f.session.screen().terrainLivePreview,"a completed prefix publishes while samples continue, before Up or a stationary frame");
  const auto preview=f.session.screen().terrainPreviewBuild;
  AE_EXPECT_TRUE(preview && std::any_of(preview->field.heights.begin(),preview->field.heights.end(),[](float y){return y>.001f;}),"published preview contains actual elevation");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),before,"visible gesture has not committed authored strokes");
  AE_EXPECT_EQ(f.session.history().undoDepth(),history,"preview does not add scene history");
  f.session.handlePointer({76,ui::UiPointerPhase::Cancel,point,f.time+2});
  for(u32 i=0;i<20;++i){f.render();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  AE_EXPECT_TRUE(!f.session.screen().terrainLivePreview,"cancel restores committed geometry and rejects late completions");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),before,"cancel preserves full authored recipe");
  AE_EXPECT_EQ(f.session.history().undoDepth(),history,"cancel preserves history");
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


AE_TEST(terrain_catalog_search_routes_material_identity_and_isolates_other_queries) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"open material catalog");
  const auto catalog=f.session.screen().terrainCatalog;
  AE_EXPECT_TRUE(catalog && catalog->materials.size()>6,"real paginated source catalog");
  const auto target=catalog->materials.back().id;
  const auto library=f.session.screen().libraryQuery,texture=f.session.screen().textureQuery,
    reference=f.session.screen().referenceQuery,global=f.session.screen().globalQuery;
  const auto recipeJson=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogNext)),"visit another original page before search");
  AE_EXPECT_TRUE(f.catalogQuery(target),"search uses real platform text edit purpose");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogPage,0u,"query resets filtered pagination");
  AE_EXPECT_TRUE(resources::serializeTerrainRecipe(f.session.screen().terrainDraft)==recipeJson,"query never mutates authored terrain");
  AE_EXPECT_EQ(f.session.screen().libraryQuery,library,"Asset Browser query isolated");
  AE_EXPECT_EQ(f.session.screen().textureQuery,texture,"texture inspector query isolated");
  AE_EXPECT_EQ(f.session.screen().referenceQuery,reference,"reference picker query isolated");
  AE_EXPECT_EQ(f.session.screen().globalQuery,global,"global search query isolated");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogItemBase)),"tap first filtered cell");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].sourceCatalogId,target,"filtered identity selects original catalog row, not page slot zero");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61))) && f.tap(id(Widget::TerrainMaterialLibrary)),"reopen saved material search context");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogQuery,target,"context retains its query");
  AE_EXPECT_TRUE(f.catalogQuery("no_such_terrain_source_903714"),"query empty result");
  AE_EXPECT_TRUE(f.text("Sem resultados"),"empty results explained");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(!f.point(id(Widget::TerrainCatalogItemBase),point),"no fake selectable cell");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(1))),"clear actual query");
  AE_EXPECT_TRUE(f.session.screen().terrainCatalogQuery.empty(),"clear resets only terrain query");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogItemBase)),"original results restored");
}

AE_TEST(terrain_catalog_search_filtered_texture_selection_reaches_pbr_noise_geo_and_global) {
  TerrainUiFixture f;
  const auto module=[](u32 a){return id(editor::terrainModuleAction(a));};
  const auto noise=[](u32 a){return id(editor::terrainNoiseAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"load actual sources");
  const auto catalog=f.session.screen().terrainCatalog;
  AE_EXPECT_TRUE(catalog && !catalog->textures.empty(),"transported source textures exist");
  const auto target=catalog->textures.back().identity;
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(id(Widget::TerrainMaterialMaps)) && f.tap(id(editor::terrainCatalogAction(4))),"PBR opens real texture picker");
  AE_EXPECT_TRUE(f.catalogQuery(target.text()) && f.textureThumbnail(catalog->textures.back().id) && f.tap(id(editor::terrainCatalogAction(8))),"choose filtered PBR source with actual32px thumbnail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[0].texture==target,"PBR resolves filtered GUID");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(noise(0)) && f.tap(noise(10)),"open independent noise source context");
  AE_EXPECT_TRUE(f.session.screen().terrainCatalogQuery.empty(),"new picker does not inherit PBR query");
  AE_EXPECT_TRUE(f.catalogQuery(target.text()) && f.textureThumbnail(catalog->textures.back().id) && f.tap(noise(16)),"choose filtered noise source with actual thumbnail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.detailNoise.map.texture==target,"noise resolves filtered GUID");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(module(0)) && f.tap(module(60)) && f.tap(module(10)) && f.tap(module(23)),"open Geo albedo source");
  AE_EXPECT_TRUE(f.catalogQuery(target.text()) && f.textureThumbnail(catalog->textures.back().id) && f.tap(module(32)),"choose filtered Geo source with actual thumbnail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.geo.albedo.texture==target,"Geo resolves filtered GUID");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(module(13)) && f.tap(module(21)) && f.tap(module(23)),"open global tint source");
  AE_EXPECT_TRUE(f.catalogQuery(target.text()) && f.textureThumbnail(catalog->textures.back().id) && f.tap(module(32)),"choose filtered global source with actual thumbnail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.globalMaps[0].map.texture==target,"Global resolves filtered GUID");
}

AE_TEST(terrain_catalog_search_filters_and_stale_identity_guard_cover_every_picker_scope) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"load real catalog");
  editor::EditorScreenState state;state.terrainCatalog=f.session.screen().terrainCatalog;
  for(u32 view:{1u,8u,9u,10u,11u,20u,21u,22u,23u,24u,25u,39u,40u,41u,59u,69u,80u,81u,82u,90u,91u,92u,150u,151u,180u,181u,182u,183u,190u}) {
    state.terrainMaterialView=view;state.terrainCatalogPage=0;state.terrainCatalogQuery.clear();
    AE_EXPECT_TRUE(editor::terrainCatalogSearchView(view),"all actual catalog picker routes share search helper");
    editor::terrainCatalogRecordVisible(state);
    const bool material=editor::terrainCatalogMaterialScope(view);
    AE_EXPECT_EQ(editor::terrainCatalogResolve(state,0,material),0u,"unfiltered first result retains original index");
    state.terrainCatalogQuery="definitely_missing_source";
    AE_EXPECT_EQ(editor::terrainCatalogResolve(state,0,material),~0u,"stale drawn result rejected after query change");
  }
  state.terrainCatalogQuery.clear();state.terrainCatalogRoleFilter=2;
  const auto normals=editor::terrainCatalogIndices(state,false);
  AE_EXPECT_TRUE(!normals.empty(),"role filter finds actual authored normal references");
  for(auto index:normals) {
    bool referenced=(state.terrainCatalog->textures[index].roleMask&(1u<<1))!=0;const auto guid=state.terrainCatalog->textures[index].identity;
    for(const auto &m:state.terrainCatalog->materials) {
      referenced=referenced || m.layer.maps[1].texture==guid;
      for(const auto &v:m.layer.cluster.variants)referenced=referenced || v.maps[1].texture==guid;
    }
    AE_EXPECT_TRUE(referenced,"normal role derives from authored map role rather than filename");
  }
  state.terrainCatalogRoleFilter=0;
  const auto packages=editor::terrainCatalogPackages(state);
  AE_EXPECT_TRUE(!packages.empty(),"real package provenance available");
  state.terrainCatalogPackageFilter=packages.front();
  for(auto index:editor::terrainCatalogIndices(state,true))AE_EXPECT_EQ(state.terrainCatalog->materials[index].package,packages.front(),"package filter uses real provenance");
}


AE_TEST(terrain_catalog_search_cluster_material_and_texture_pickers_preserve_full_variant) {
  TerrainUiFixture f;
  const auto cluster=[](u32 a){return id(editor::terrainClusterAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(editor::terrainNoiseAction(0))) && f.tap(cluster(0)) && f.tap(cluster(5)),"open actual new variant material picker");
  const auto catalog=f.session.screen().terrainCatalog;
  const auto &material=catalog->materials.back();
  AE_EXPECT_TRUE(f.catalogQuery(material.id) && f.tap(cluster(20)),"filtered material creates real complete variant");
  const auto &variant=f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0];
  AE_EXPECT_TRUE(variant.identity==material.layer.identity,"stable material identity selected through filtered list");
  for(u32 role=0;role<7;++role)AE_EXPECT_TRUE(variant.maps[role].texture==material.layer.maps[role].texture,"all seven map sources copied");
  const auto target=catalog->textures.back().identity;
  AE_EXPECT_TRUE(f.tap(cluster(7)) && f.tap(cluster(9)),"open variant texture picker");
  AE_EXPECT_TRUE(f.catalogQuery(target.text()) && f.textureThumbnail(catalog->textures.back().id) && f.tap(cluster(32)),"filtered texture thumbnail targets variant map");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0].maps[0].texture==target,"variant texture selects filtered original GUID");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0].identity==material.layer.identity,"map replacement preserves variant identity");
}


AE_TEST(terrain_catalog_search_filter_buttons_reset_pages_and_keep_zero_scroll_entry) {
  TerrainUiFixture f;
  const auto search=[](u32 a){return id(editor::terrainCatalogAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"open compact catalog");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(search(0),point) && f.point(search(2),point) && f.point(search(3),point),"search and real filters visible immediately");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogNext)) && f.tap(search(2)),"change authored role after paging");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogPage,0u,"role filter resets page");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,1u,"actual color source filter");
  AE_EXPECT_TRUE(f.tap(search(3)),"choose first actual source package");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogPage,0u,"package filter resets page");
  const auto expected=editor::terrainCatalogIndices(f.session.screen(),true);
  AE_EXPECT_TRUE(!expected.empty(),"real package and color role yield selectable materials");
  const auto material=f.session.screen().terrainCatalog->materials[expected[0]].id;
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainCatalogItemBase)),"select filtered package result through touch");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].sourceCatalogId,material,"filter controls and selection share the same index vector");
}


AE_TEST(terrain_snow_modes_real_sources_exact_amount_and_explicit_measured_height_range) {
  TerrainUiFixture f;
  const auto snow=[](u32 a){return id(editor::terrainSnowAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(snow(0)),"Snow opens through real root navigation");
  f.render();ui::UiPoint point;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"compact Snow opens at zero scroll");
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase,point),"first effectful amount slider visible immediately");
  AE_EXPECT_TRUE(f.text("Neve · desligada"),"Off state is explicit in sticky context");
  AE_EXPECT_TRUE(f.tap(snow(21)) && f.tap(snow(32)) && f.tap(snow(20)),"explicit preset button transports original sources without activating Snow");
  AE_EXPECT_TRUE(f.snowMode(2),"Simple seeds real original mandatory source views");
  auto settings=f.session.screen().terrainDraft.surface.snow;
  AE_EXPECT_TRUE(settings.mode==resources::TerrainSnowMode::Simple,"Simple selects actual runtime mode");
  AE_EXPECT_TRUE(settings.diffuse.texture.valid() && settings.diffuse.texture==settings.height.texture,"diffuse RGB and height A share original GUID, separate authored maps");
  AE_EXPECT_TRUE(settings.normal.texture==settings.smoothness.texture && settings.normal.texture==settings.ao.texture,"packed normal and scalar views share exact original");
  AE_EXPECT_TRUE(settings.normal.channel==resources::TerrainSurfaceChannel::NormalGA,"Snow packing explicitly GA");
  for(const auto guid:{settings.diffuse.texture,settings.normal.texture}) {
    const auto &sources=f.session.screen().terrainCatalog->textures;
    const auto source=std::find_if(sources.begin(),sources.end(),[&](const auto &entry){return entry.identity==guid;});
    AE_EXPECT_TRUE(source!=sources.end() && source->width==1024 && source->height==1024,"preset resolves actual full1024 catalog originals, never thumbnails");
  }

  AE_EXPECT_EQ(settings.heightAngleRange[0],50.f,"source height start preserved during mode selection");
  AE_EXPECT_EQ(settings.heightAngleRange[1],140.f,"source height end preserved during mode selection");
  AE_EXPECT_TRUE(f.text("Acima do plano"),"default range outside known plane is explained");
  f.render();AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"mode returns to amount and range at zero scroll");
  AE_EXPECT_TRUE(f.point(snow(72),point),"actual range state/action is visible immediately below amount");
  AE_EXPECT_TRUE(f.tap(snow(72),false),"zero-scroll authoring action fits actual flat draft with documented one metre margin");
  settings=f.session.screen().terrainDraft.surface.snow;
  AE_EXPECT_EQ(settings.heightAngleRange[0],f.session.screen().terrainDraft.base-1,"measured lower bound with explicit margin");
  AE_EXPECT_EQ(settings.heightAngleRange[1],f.session.screen().terrainDraft.base+1,"measured upper bound with explicit margin");
  AE_EXPECT_TRUE(f.tap(snow(20)) && f.tap(editor::terrainSnowNumberBase),"exact amount entry from actual numeric chip");
  AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),"0.75",true),"Snow numeric expression routes to separate namespace");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.snow.amount,.75f,"precise amount persists");
  AE_EXPECT_TRUE(f.snowMode(3),"switch to Rich without dropping prepared values");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.snow.amount,.75f,"Rich preserves amount");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61))) && f.tap(snow(100)) && f.tap(snow(109)),"return to root and choose explicit512 authoring ceiling before publication");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.textureResidency.mode==resources::TerrainTextureResidencyMode::Cap && f.session.screen().terrainDraft.surface.textureResidency.maxSide==512,"actual terrain authoring policy is512 before publication");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"actual Apply publishes Snow original views with chosen mip policy");
  AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe,expected,"published recipe includes exact Snow and residency fields");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root!=0 && f.session.openTerrain(root),"reopen actual published terrain");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"GLB reopen retains GA, source GUIDs, height range and quality choice");
  const auto scenePath=f.path/"snow-ui.aescene";
  AE_EXPECT_TRUE(f.session.save(scenePath.string().c_str(),0) && f.session.load(scenePath.string().c_str(),0) && f.session.openTerrain(root),"scene round trip restores imported Snow terrain");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"saved scene preserves complete authored Snow state");

}

AE_TEST(terrain_snow_signed_mapping_ordered_fades_and_nonzero_direction_use_real_sliders) {
  TerrainUiFixture f;const auto snow=[](u32 a){return id(editor::terrainSnowAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(snow(0)) && f.snowMode(2) && f.tap(snow(26)),"open Snow height and direction");
  AE_EXPECT_TRUE(f.slide(265,1) && f.slide(266,0),"height edges stay ordered while dragging both boundaries");
  const auto &first=f.session.screen().terrainDraft.surface.snow;
  AE_EXPECT_TRUE(first.heightAngleRange[1]>first.heightAngleRange[0],"height range never crosses");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(261,0),"all-zero direction rejected atomically");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.snow.upVector[1],1.f,"rejected direction does not modify draft");
  AE_EXPECT_TRUE(f.tap(snow(20)) && f.tap(snow(25)),"open actual mapping");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(263,-32) && f.session.setTerrainNumber(264,0),"signed and zero frequency preserved");
  AE_EXPECT_TRUE(f.tap(snow(63)) && f.tap(snow(65)) && f.tap(snow(64)),"WorldSpace, triplanar and stochastic are independent flags");
  AE_EXPECT_TRUE(f.slide(279,.5f) && f.slide(280,.5f),"stochastic source controls are sliders");
  AE_EXPECT_TRUE(f.tap(snow(22)) && f.tap(snow(28)) && f.tap(snow(61)),"distance resample real enable path");
  AE_EXPECT_TRUE(f.slide(277,1) && f.slide(278,0),"distance endpoints remain usable at boundary");
  const auto &settings=f.session.screen().terrainDraft.surface.snow;
  AE_EXPECT_TRUE(settings.distanceResample[3]>settings.distanceResample[2],"distance fade cannot cross");
  AE_EXPECT_EQ(settings.uvScales[0],-32.f,"mirroring retains signed authoring value");
  AE_EXPECT_EQ(settings.uvScales[1],0.f,"fixed coordinate retains zero");
}

AE_TEST(terrain_snow_map_ga_mask_and_m12_lighting_have_precise_effectful_fields) {
  TerrainUiFixture f;const auto snow=[](u32 a){return id(editor::terrainSnowAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(snow(0)) && f.snowMode(3) && f.tap(snow(21)) && f.tap(snow(42)),"open original GA normal editor");
  const auto original=f.session.screen().terrainDraft.surface.snow.normal.texture;
  AE_EXPECT_TRUE(f.tap(snow(52)),"Snow normal cycles GA to RGB");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.channel==resources::TerrainSurfaceChannel::NormalRGB,"RGB is authored explicitly");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(309,.6),"RGB exposes consumed Z factor");
  AE_EXPECT_TRUE(f.tap(snow(52)),"Snow normal cycles RGB to RG");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.channel==resources::TerrainSurfaceChannel::NormalRG,"RG reconstructs Z");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(309,.2),"RG hides reconstructed Z");
  AE_EXPECT_TRUE(f.tap(snow(52)),"Snow normal cycles RG to GA");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.channel==resources::TerrainSurfaceChannel::NormalGA,"GA retains distinct A/G packing");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.texture==original,"packing cycle never replaces actual source");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.snow.normal.colorFactor[2],.6f,"hidden Z preparation survives packed modes");
  for(u32 field:{305u,306u,309u,310u,313u,314u,317u,318u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"hidden scalar/Z/alpha controls cannot mutate GA map");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(307,.25) && f.session.setTerrainNumber(308,.75),"logical GA X/Y factors editable");
  AE_EXPECT_TRUE(f.tap(snow(48)),"open real Snow source picker");
  AE_EXPECT_TRUE(f.catalogQuery("cc22f06eaf78d43f782ee617776c1640") && f.textureThumbnail("cc22f06eaf78d43f782ee617776c1640") && f.tap(snow(80)),"search selects exact packed original GUID with its real32px thumbnail");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.channel==resources::TerrainSurfaceChannel::NormalGA,"source replacement retains explicit GA packing");
  AE_EXPECT_TRUE(f.tap(snow(21)) && f.tap(snow(46)) && f.tap(snow(48)),"open real mask source");
  AE_EXPECT_TRUE(f.catalogQuery("b822881b9ef3441f29c88bb6e0ef60cd") && f.tap(snow(80)),"mask gets explicitly chosen actual source");
  for(u32 field:{309u,310u,313u,314u,317u,318u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"mask exposes only consumed R/G remaps");
  AE_EXPECT_TRUE(f.tap(snow(22)) && f.tap(snow(62)),"source-backed R/G mask can activate");
  AE_EXPECT_TRUE(f.tap(snow(23)) && f.tap(snow(67)) && f.tap(snow(30)),"real SSS lighting path");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(286,.8),"SSS alpha is actual thickness multiplier");
  AE_EXPECT_TRUE(f.tap(snow(23)) && f.tap(snow(68)) && f.tap(snow(31)) && f.slide(290,.5f),"rim light power slider");
  AE_EXPECT_TRUE(f.tap(snow(23)) && f.tap(snow(29)) && f.tap(snow(69)),"sparkle has transported Perlin source");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(296,4) && f.session.setTerrainNumber(297,6) && f.session.setTerrainNumber(298,2) && f.session.setTerrainNumber(299,12) && f.session.setTerrainNumber(300,.6),"all distinct sparkle parameters authorable");
  AE_EXPECT_TRUE(f.tap(snow(24)) && f.tap(snow(66)) && f.slide(281,.25f) && f.slide(282,.75f),"Rich per-layer coverage and porosity");
  const auto &surface=f.session.screen().terrainDraft.surface;
  AE_EXPECT_TRUE(surface.snow.sssEnabled && surface.snow.rimEnabled && surface.snow.sparkleEnabled && surface.snow.maskEnabled,"all authored effects reach real flags");
  AE_EXPECT_EQ(surface.layers[0].snowParticipation,.25f,"participation belongs to selected layer");
  AE_EXPECT_EQ(surface.layers[1].snowParticipation,1.f,"another layer remains independent");
}

AE_TEST(terrain_texture_residency_terrain_caps_preserve_map_original_override_and_geometry) {
  TerrainUiFixture f;const auto snow=[](u32 a){return id(editor::terrainSnowAction(a));};
  const auto back=id(editor::terrainModuleAction(61));
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialMaps)),"open authored PBR map");
  const auto texture=f.session.screen().terrainDraft.surface.layers[0].maps[0].texture;
  const auto size=f.session.screen().terrainDraft.size;
  AE_EXPECT_TRUE(f.tap(snow(104)),"explicit Original override on authored map");
  AE_EXPECT_TRUE(f.tap(snow(100)) && f.tap(snow(108)),"choose terrain1024 ceiling explicitly");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.textureResidency.maxSide,1024u,"terrain ceiling saved");
  AE_EXPECT_TRUE(f.tap(back) && f.tap(id(Widget::TerrainMaterialMaps)),"return to same authored map");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[0].textureResidency.mode==resources::TerrainTextureResidencyMode::Original,"map override survives terrain policy change");
  AE_EXPECT_TRUE(f.tap(snow(103)),"map can explicitly inherit terrain again");
  AE_EXPECT_TRUE(f.tap(snow(105)),"map can choose its own cap");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].maps[0].textureResidency.maxSide,4096u,"first explicit map cap4096");
  AE_EXPECT_TRUE(f.tap(snow(105)),"next supported authored cap");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].maps[0].textureResidency.maxSide,2048u,"map cap independent of terrain cap");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[0].texture==texture,"policy never swaps original texture identity");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.size,size,"residency never changes world units/domain");
}


AE_TEST(terrain_generic_normal_pages_offer_rgb_rg_ga_and_reject_hidden_ga_components) {
  TerrainUiFixture f;const auto module=[](u32 a){return id(editor::terrainModuleAction(a));};
  const auto noise=[](u32 a){return id(editor::terrainNoiseAction(a));};
  const auto cluster=[](u32 a){return id(editor::terrainClusterAction(a));};
  const std::string packed="cc22f06eaf78d43f782ee617776c1640";
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialMaps)) && f.tap(id(Widget::TerrainMapNext)),"open PBR normal role");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(4))) && f.catalogQuery(packed) && f.tap(id(editor::terrainCatalogAction(8))),"choose actual GA source");
  for(u32 i=0;i<3 && f.session.screen().terrainDraft.surface.layers[0].maps[1].channel!=resources::TerrainSurfaceChannel::NormalGA;++i)AE_EXPECT_TRUE(f.tap(id(Widget::TerrainMapChannel)),"cycle real normal packing");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].maps[1].channel==resources::TerrainSurfaceChannel::NormalGA,"PBR exposes distinct GA");
  for(u32 field:{97u,98u,101u,102u,105u,106u,109u,110u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"PBR GA hides scalar/Z/alpha");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(noise(0)) && f.tap(noise(23)) && f.tap(noise(10)) && f.catalogQuery(packed) && f.tap(noise(16)),"normal-noise actual GA source");
  for(u32 i=0;i<3 && f.session.screen().terrainDraft.surface.normalNoise[0].map.channel!=resources::TerrainSurfaceChannel::NormalGA;++i)AE_EXPECT_TRUE(f.tap(noise(7)),"noise cycles RGB/RG/GA");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.normalNoise[0].map.channel==resources::TerrainSurfaceChannel::NormalGA,"noise GA preserved");
  for(u32 field:{134u,135u,138u,139u,142u,143u,146u,147u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"noise GA hides scalar/Z/alpha");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(module(0)) && f.tap(module(60)) && f.tap(module(11)),"open Geo normal");
  AE_EXPECT_TRUE(f.tap(module(23)) && f.catalogQuery(packed) && f.tap(module(32)),"Geo selects actual GA source");
  for(u32 i=0;i<3 && f.session.screen().terrainDraft.surface.geo.normal.channel!=resources::TerrainSurfaceChannel::NormalGA;++i)AE_EXPECT_TRUE(f.tap(module(25)),"Geo cycles RGB/RG/GA");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.geo.normal.channel==resources::TerrainSurfaceChannel::NormalGA,"Geo GA accepted by model");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(227,.5),"Geo GA Z exact gate");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(module(13)) && f.tap(module(15)) && f.tap(module(21)) && f.tap(module(23)) && f.catalogQuery(packed) && f.tap(module(32)),"Global normal chooses GA source");
  for(u32 i=0;i<3 && f.session.screen().terrainDraft.surface.globalMaps[1].map.channel!=resources::TerrainSurfaceChannel::NormalGA;++i)AE_EXPECT_TRUE(f.tap(module(25)),"Global cycles RGB/RG/GA");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.globalMaps[1].map.channel==resources::TerrainSurfaceChannel::NormalGA,"Global GA persisted");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(227,.5),"Global GA Z exact gate");
  AE_EXPECT_TRUE(f.tap(module(61)) && f.tap(noise(0)) && f.tap(cluster(0)) && f.tap(cluster(5)) && f.tap(cluster(20)) && f.tap(cluster(7)),"create real variant for normal map");
  for(u32 i=0;i<7 && f.session.screen().terrainMapRole!=1;++i)AE_EXPECT_TRUE(f.tap(cluster(12)),"select variant normal role");
  AE_EXPECT_TRUE(f.tap(cluster(9)) && f.catalogQuery(packed) && f.tap(cluster(32)),"variant chooses packed original source");
  for(u32 i=0;i<3 && f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0].maps[1].channel!=resources::TerrainSurfaceChannel::NormalGA;++i)AE_EXPECT_TRUE(f.tap(cluster(13)),"variant cycles RGB/RG/GA");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.layers[0].cluster.variants[0].maps[1].channel==resources::TerrainSurfaceChannel::NormalGA,"variant GA persisted");
  for(u32 field:{177u,178u,181u,182u,185u,186u,189u,190u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"variant GA hidden-component exact gate");
}


AE_TEST(terrain_catalog_texture_filters_use_explicit_snow_geo_roles_and_texture_package_origin) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"load actual152-texture catalog");
  editor::EditorScreenState state;state.terrainCatalog=f.session.screen().terrainCatalog;
  state.terrainMaterialView=190;state.terrainCatalogRoleFilter=2;
  const auto materialsBefore=editor::terrainCatalogIndices(state,true);
  for(const char *guidText:{"cc22f06eaf78d43f782ee617776c1640","192aaea3237044f608d663c9884e0c70"}){
    const auto &textures=state.terrainCatalog->textures;
    const auto found=std::find_if(textures.begin(),textures.end(),[&](const auto &texture){return texture.identity.text()==guidText;});
    AE_EXPECT_TRUE(found!=textures.end(),"real standalone SnowGA/Geo normal source exists");
    AE_EXPECT_TRUE((found->roleMask&(1u<<1))!=0,"explicit JSON source_roles supplies Normal role");
    AE_EXPECT_TRUE(!found->package.empty(),"actual texture package is retained");
    const auto packages=editor::terrainCatalogPackages(state);
    AE_EXPECT_TRUE(std::find(packages.begin(),packages.end(),found->package)!=packages.end(),"texture provenance is a discoverable package");
    state.terrainCatalogPackageFilter=found->package;state.terrainCatalogQuery=guidText;
    const auto filtered=editor::terrainCatalogIndices(state,false);
    AE_EXPECT_EQ(filtered.size(),usize(1),"standalone source survives Normal plus its real package filter");
    AE_EXPECT_TRUE(textures[filtered[0]].identity==found->identity,"filtered row retains exact original GUID");
    if(std::string_view(guidText)=="cc22f06eaf78d43f782ee617776c1640")AE_EXPECT_TRUE((found->roleMask&(1u<<3)) && (found->roleMask&(1u<<4)),"packed Snow metadata also exposes AO and smoothness in roughness role");
    state.terrainCatalogPackageFilter="different_texture_origin";
    AE_EXPECT_TRUE(editor::terrainCatalogIndices(state,false).empty(),"wrong origin cannot be inferred from materials or filenames");
    state.terrainCatalogPackageFilter.clear();state.terrainCatalogQuery.clear();
  }
  AE_EXPECT_TRUE(editor::terrainCatalogIndices(state,true)==materialsBefore,"texture metadata does not alter material-query behavior");
}


AE_TEST(terrain_new_normal_pickers_default_to_real_normal_role_but_saved_all_wins) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialMaps)) && f.tap(id(Widget::TerrainMapNext)) && f.tap(id(editor::terrainCatalogAction(4))),"open new PBR normal source context");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,2u,"new unambiguous normal picker starts Normal");
  AE_EXPECT_TRUE(f.text("Papel: Normal"),"default purpose is visible");
  for(u32 i=0;i<6;++i)AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(2))),"author can cycle to All");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"All remains a real explicit choice");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(5))) && f.tap(id(editor::terrainCatalogAction(4))),"back and reopen same normal picker");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"saved All is not reset to purpose default");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(5))) && f.tap(id(Widget::TerrainMapNext)) && f.tap(id(editor::terrainCatalogAction(4))),"open different Height source context");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"other roles retain existing All default");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(5))) && f.tap(id(Widget::TerrainMapPrevious)) && f.tap(id(editor::terrainCatalogAction(4))),"return to Normal source context");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"role-scoped saved override survives purpose switching");
}

AE_TEST(terrain_normal_purpose_defaults_cover_every_normal_consumer_and_keep_snow_ga_selectable) {
  TerrainUiFixture f;
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(id(Widget::TerrainMaterialLibrary)),"load catalog with standalone source roles");
  editor::EditorScreenState state;state.terrainCatalog=f.session.screen().terrainCatalog;
  for(const auto route:std::array<std::array<u32,2>,17>{{{190,1},{90,1},{91,1},{92,1},{151,0},{181,0},{222,0},{225,0},{20,0},{21,0},{22,0},{23,0},{24,0},{25,0},{39,0},{190,0},{90,0}}}){
    state.terrainMaterialView=0;state.terrainMapRole=route[1];state.terrainCatalogSearchContexts.clear();state.terrainCatalogRoleFilter=0;
    {editor::TerrainCatalogContextGuard context(state);state.terrainMaterialView=route[0];}
    const bool normal=!(route[0]==190 && route[1]==0) && !(route[0]==90 && route[1]==0);
    AE_EXPECT_EQ(state.terrainCatalogRoleFilter,normal?2u:0u,"new context uses only unequivocal normal purpose");
    state.terrainCatalogRoleFilter=0;
    {editor::TerrainCatalogContextGuard context(state);state.terrainMaterialView=0;}
    {editor::TerrainCatalogContextGuard context(state);state.terrainMaterialView=route[0];}
    AE_EXPECT_EQ(state.terrainCatalogRoleFilter,0u,"all context overrides survive leaving and reopening");
  }
  const auto snow=[](u32 action){return id(editor::terrainSnowAction(action));};
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61))) && f.tap(snow(0)) && f.tap(snow(21)) && f.tap(snow(42)) && f.tap(snow(48)),"open new SnowGA normal picker through real editor");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,2u,"SnowGA source picker begins Normal");
  AE_EXPECT_TRUE(f.catalogQuery("cc22f06eaf78d43f782ee617776c1640") && f.textureThumbnail("cc22f06eaf78d43f782ee617776c1640") && f.tap(snow(80)),"explicit source_roles keeps actual SnowGA original selectable under Normal");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.snow.normal.channel==resources::TerrainSurfaceChannel::NormalGA,"GA packing preserved");
}

AE_TEST(terrain_snow_normal_rgb_rg_source_replacement_apply_and_reopen_preserve_packing) {
  for(const auto packing:{resources::TerrainSurfaceChannel::NormalRGB,resources::TerrainSurfaceChannel::NormalRG}) {
    TerrainUiFixture f;const auto snow=[](u32 a){return id(editor::terrainSnowAction(a));};
    AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(snow(0)) && f.snowMode(2) && f.tap(snow(72)),"prepare source-backed Snow with measured range");
    AE_EXPECT_TRUE(f.tap(snow(21)) && f.tap(snow(42)) && f.tap(snow(52)),"open normal and choose RGB");
    if(packing==resources::TerrainSurfaceChannel::NormalRG)AE_EXPECT_TRUE(f.tap(snow(52)),"choose RG explicitly");
    AE_EXPECT_TRUE(f.session.setTerrainNumber(307,.35) && f.session.setTerrainNumber(308,.65),"prepare logical XY factors");
    const auto replacement=f.session.screen().terrainCatalog->materials[0].layer.maps[1].texture;
    AE_EXPECT_TRUE(replacement.valid() && replacement!=f.session.screen().terrainDraft.surface.snow.normal.texture,"replacement is a different actual normal source");
    AE_EXPECT_TRUE(f.tap(snow(48)) && f.catalogQuery(replacement.text()) && f.tap(snow(80)),"replace source through actual filtered picker");
    const auto &normal=f.session.screen().terrainDraft.surface.snow.normal;
    AE_EXPECT_TRUE(normal.channel==packing && normal.linear,"source selection preserves authored packing and normal linear space");
    AE_EXPECT_EQ(normal.colorFactor[0],.35f,"source selection preserves logical X");
    AE_EXPECT_EQ(normal.colorFactor[1],.65f,"source selection preserves logical Y");
    for(u32 field:{305u,306u,310u,314u,318u})AE_EXPECT_TRUE(!f.session.setTerrainNumber(field,.5),"scalar and alpha controls remain unavailable");
    AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61))) && f.tap(snow(100)) && f.tap(snow(109)),"choose explicit512 publication budget");
    const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
    AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply publishes explicitly authored normal packing");
    AE_EXPECT_EQ(f.session.screen().terrainAppliedRecipe,expected,"publication retains exact Snow recipe");
    const auto target=f.session.screen().terrainTarget;
    AE_EXPECT_TRUE(target!=0 && f.session.openTerrain(target),"reopen actual terrain");
    AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"reopen retains RGB/RG source and logical factors");
  }
}

AE_TEST(terrain_fx_ui_explicit_budget_resolution_and_exclusive_modes) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)),"open independent FX authoring");
  f.render();ui::UiPoint first;
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"FX opens at zero scroll");
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+64+0x100,first),"first target slider visible at655x300 without scrolling");
  const auto window=editor::terrainContentRect(f.session.screen(),editor::terrainPanelRect(f.session.screen()));
  const auto canvas=f.session.screen().terrainFx.canvas;
  AE_EXPECT_TRUE(canvas.y>=window.y && canvas.bottom()<=window.bottom(),"entire80px bitmap/cursor fits before footer atzero scroll");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(326,.2),"hidden filter field cannot mutate from Map page");
  AE_EXPECT_TRUE(f.slide(320,.65f,1) && f.slide(321,.25f,2),"target/radius are real first-row sliders");
  AE_EXPECT_TRUE(f.tap(editor::terrainSnowNumberBase+64) && f.session.completeTextEdit(f.session.pendingTextEdit(),"0.375",true),"FX target exact entry routes through real keyboard namespace");
  AE_EXPECT_EQ(f.session.screen().terrainFx.brush.target,.375,"exact target retains author precision");
  AE_EXPECT_TRUE(f.tap(fx(23)) && f.tap(fx(43)) && f.tap(fx(10)),"request2048 explicitly at default96MiB");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.fx.enabled,"refused allocation never creates/resizes asset");
  AE_EXPECT_EQ(f.session.screen().terrainFx.resolution,2048u,"refusal keeps requested resolution explicit");
  AE_EXPECT_TRUE(f.tap(fx(41)) && f.tap(fx(10)),"explicit512 creation fits unchanged96MiB budget");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.fx.width,512u,"recipe owns actual chosen resolution");
  AE_EXPECT_TRUE(f.tap(fx(24)) && f.tap(fx(2)),"enter FX paint exclusively");
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed && f.session.screen().terrainFx.mode==1,"PBR brush stays disarmed");
  AE_EXPECT_TRUE(f.tap(fx(24)) && f.tap(fx(3)) && f.session.screen().terrainFx.mode==2,"Transformar exits paint mode");
}
AE_TEST(terrain_fx_ui_canvas_gesture_cancel_undo_redo_preserve_independent_recipe) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(24)) && f.tap(fx(2)),"create bitmap and enter real paint");
  const auto pbr=f.session.screen().terrainDraft.strokes.size();
  const auto gesture=[&](ui::UiPointerPhase end){
    f.scroll(-10000);ui::UiPoint point;for(u32 i=0;i<40;++i){f.render();if(f.point(fx(90),point))break;f.scroll(20);}
    if(!f.point(fx(90),point))return false;
    f.time+=1;f.session.handlePointer({77,ui::UiPointerPhase::Down,point,f.time});
    point.x+=3;f.session.handlePointer({77,ui::UiPointerPhase::Move,point,f.time+.1});
    f.session.handlePointer({77,end,point,f.time+.2});return true;
  };
  AE_EXPECT_TRUE(gesture(ui::UiPointerPhase::Cancel),"pointer cancellation routed");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.fx.strokes.empty(),"cancel does not publish captured samples");
  AE_EXPECT_TRUE(gesture(ui::UiPointerPhase::Up),"complete actual bitmap gesture");
  const auto authored=f.session.screen().terrainDraft.fx.strokes.size();
  AE_EXPECT_TRUE(authored>0 && f.session.screen().terrainDraft.fx.bakedStrokeCount==authored,"pointer release commits full checkpoint");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.strokes.size(),pbr,"FX never appends PBR or sculpt strokes");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo resource snapshot");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.fx.strokes.empty(),"undo restores entire pre-gesture log and seed");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo resource snapshot");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.fx.strokes.size(),authored,"redo restores authored checkpoint");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply publishes actual Recipe9 FX checkpoint via terrain pipeline");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root!=0 && f.session.openTerrain(root),"reopen published FX terrain");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"recipe/source roundtrip retains independent FX authoring");
}

AE_TEST(terrain_static_material_ui_requires_real_mask_and_preserves_reserved_properties) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(100)),"open real static Wetness page");
  f.render();ui::UiPoint first;
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+80+0x100,first),"Wetness first slider visible atzero scroll");
  AE_EXPECT_TRUE(!f.tap(fx(110)),"missing real bitmap disables enable control");
  AE_EXPECT_TRUE(f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(100)) && f.tap(fx(110)),"explicit mask creation unlocks static Wetness");
  AE_EXPECT_TRUE(f.slide(336,.25f,1) && f.slide(337,.75f,2),"ordered min/max use effectful bars");
  AE_EXPECT_TRUE(f.tap(editor::terrainSnowNumberBase+80) && f.session.completeTextEdit(f.session.pendingTextEdit(),"0.2",true),"Wetness exact keyboard entry");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(346,5),"hidden Puddle field cannot mutate from Wetness");
  AE_EXPECT_TRUE(f.tap(fx(115)),"actual static wetness can mask Snow");
  AE_EXPECT_TRUE(f.tap(fx(102)) && f.tap(fx(112)),"height Wetness actual modifier");
  for(const auto pair:{std::pair{338u,-2.},std::pair{339u,1.5},std::pair{340u,.2},std::pair{341u,.4}})AE_EXPECT_TRUE(f.session.setTerrainNumber(pair.first,pair.second),"every height formula parameter authorable");
  AE_EXPECT_TRUE(f.tap(fx(103)) && f.tap(fx(113)),"distance Wetness actual modifier");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(342,25) && f.session.setTerrainNumber(343,100),"UI end converts to source length");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.flow.distanceWetnessParams[2],75.f,"stored value is length, not mislabeled end");
  AE_EXPECT_TRUE(f.slide(342,1,1) && f.slide(343,0,2),"dragging endpoints never crosses");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.flow.distanceWetnessParams[2]>0,"positive source length retained");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(343,200) && f.session.setTerrainNumber(342,50) && f.session.setTerrainNumber(344,.75) && f.session.setTerrainNumber(345,.25),"prepare exact distance response");
  AE_EXPECT_TRUE(f.tap(fx(104)) && f.tap(fx(114)) && f.session.setTerrainNumber(349,.3),"per-material porosity uses actual canonical field");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].snowPorosity,.3f,"Snow and Flow share one porosity authority");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(348,.8),"inactive global override cannot mutate through exact input");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(63))) && f.session.setTerrainNumber(349,.6),"select another real layer");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].snowPorosity,.3f,"previous layer remains independent");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[1].snowPorosity,.6f,"selected layer receives porosity");
  AE_EXPECT_TRUE(f.tap(fx(114)) && f.session.setTerrainNumber(348,.45),"explicit global porosity mode");
  AE_EXPECT_TRUE(f.tap(fx(101)) && f.tap(fx(111)) && f.slide(346,.5f,1) && f.session.setTerrainNumber(347,.8),"Puddle contrast/max actual material controls");
  const auto &flow=f.session.screen().terrainDraft.surface.flow;
  AE_EXPECT_TRUE(flow.wetness && flow.puddles && flow.heightWetness && flow.distanceWetness && flow.wetnessMaskSnow,"only supported static flags enabled");
  AE_EXPECT_TRUE(!flow.streams && !flow.lava && !flow.globalWetness && !flow.rainDrops,"unsupported consumers never enabled by navigation");
  AE_EXPECT_EQ(flow.wetnessParams[2],0.f,"reserved Wetness component preserved");
  AE_EXPECT_EQ(flow.puddleParams[2],0.f,"reserved Puddle component preserved");
  AE_EXPECT_EQ(flow.puddleParams[3],0.f,"reserved Puddle component preserved");
}
AE_TEST(terrain_static_material_ui_rg_mask_apply_reopen_transports_real_material_consumer) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(24)) && f.tap(fx(2)),"create real FX bitmap");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(321,16),"actual metre brush radius");
  for(u32 channel:{4u,5u}){
    AE_EXPECT_TRUE(f.tap(fx(25)) && f.tap(fx(channel)),"choose independent R/G channel");
    f.scroll(-10000);f.render();ui::UiPoint point;
    for(u32 attempt=0;attempt<40 && !f.point(fx(90),point);++attempt){f.scroll(16);f.render();}
    AE_EXPECT_TRUE(f.point(fx(90),point),"actual bitmap region reached");
    f.time+=1;f.session.handlePointer({88,ui::UiPointerPhase::Down,point,f.time});
    f.session.handlePointer({88,ui::UiPointerPhase::Move,point,f.time+.1});
    f.session.handlePointer({88,ui::UiPointerPhase::Up,point,f.time+.2});
  }
  AE_EXPECT_TRUE(f.tap(fx(100)) && f.tap(fx(110)) && f.tap(fx(101)) && f.tap(fx(111)),"enable real static Wetness/Puddle consumers over painted channels");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply uses actual texture bridge/material publisher");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root!=0 && f.session.openTerrain(root),"reopen published material");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,0u,"explicit reopen restores painting root instead of stale Puddle subcontext");
  AE_EXPECT_EQ(f.session.screen().terrainFx.mode,0u,"explicit reopen starts in Edit rather than retaining another gesture mode");
  AE_EXPECT_TRUE(!f.session.screen().terrainFx.bitmap && !f.session.screen().terrainFx.cursorVisible,"reopen releases transient bitmap and cursor from prior authoring context");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"V9 FX plus V4 material fields survive actual GLB source roundtrip");
  AE_EXPECT_TRUE(f.visit(2) && f.tap(fx(0)) && f.tap(fx(24)) && f.tap(fx(2)),"paint on reopened actual terrain");
  f.scroll(-10000);f.render();ui::UiPoint point;AE_EXPECT_TRUE(f.point(fx(90),point),"complete bitmap immediately available");
  AE_EXPECT_TRUE(f.text("FX: "+std::to_string(f.session.screen().terrainDraft.fx.strokes.size())+" amostras"),"FX status counts authored samples without adding the baked prefix again");
  const auto publications=f.session.screen().terrainPreviewUpdates;
  const auto revision=f.session.document().revision();
  f.time+=1;f.session.handlePointer({89,ui::UiPointerPhase::Down,point,f.time});
  point.x+=4;f.session.handlePointer({89,ui::UiPointerPhase::Move,point,f.time+.1});
  for(u32 i=0;i<3000 && f.session.screen().terrainPreviewUpdates==publications;++i){f.render();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewUpdates>publications && f.session.screen().terrainLivePreview,"real renderer publication occurs while pointer is still down");
  f.render();
  AE_EXPECT_TRUE(f.text("Prévia"),"transient FX renderer preview has a real preview badge instead of Applied");
  AE_EXPECT_TRUE(!f.session.screen().terrainFx.recipePending,"preview badge does not mark unchanged authoring pending");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"Move never commits authoring recipe");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"live texture does not mutate scene document");
  const auto geometry=f.session.screen().terrainPreviewBuild;
  point.x+=4;f.session.handlePointer({89,ui::UiPointerPhase::Move,point,f.time+.2});
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild==geometry,"further samples retain immutable terrain build");
  f.session.handlePointer({89,ui::UiPointerPhase::Cancel,point,f.time+.3});
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"cancel restores authoring without publishing strokes");
  AE_EXPECT_TRUE(!f.session.screen().terrainLivePreview,"cancel explicitly clears transient surface texture before restore job");
  AE_EXPECT_TRUE(f.text("Gesto FX cancelado"),"cancel diagnostic is preserved instead of replaced by sample statistics");
  f.render();ui::UiPoint footerPoint;
  AE_EXPECT_TRUE(!f.point(id(Widget::TerrainSculpt),footerPoint) && f.point(fx(2),footerPoint),"applied FX footer exposes its own paint action without geometric brush");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNavigate),false),"applied footer navigation exits FX painting");
  AE_EXPECT_TRUE(f.session.screen().terrainFx.mode==0 && !f.session.screen().terrainBrushArmed,"navigation leaves both authoring brushes disarmed");
  AE_EXPECT_TRUE(f.tap(fx(2),false),"applied footer reenters actual FX painting");
  AE_EXPECT_TRUE(f.session.screen().terrainFx.mode==1 && !f.session.screen().terrainBrushArmed,"FX footer never arms geometric/PBR painting");
  f.render();AE_EXPECT_TRUE(f.point(fx(90),point),"actual bitmap available for footer gesture test");
  f.time+=1;f.session.handlePointer({86,ui::UiPointerPhase::Down,point,f.time});point.x+=3;
  f.session.handlePointer({86,ui::UiPointerPhase::Move,point,f.time+.1});
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainNavigate),false),"footer navigation cancels an in-flight FX gesture");
  AE_EXPECT_TRUE(f.session.screen().terrainFx.mode==0 && !f.session.screen().terrainFx.cursorVisible && !f.session.screen().terrainBrushArmed,"footer cancellation clears FX cursor and preserves mode exclusion");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"footer cancellation retains applied FX and geometric authoring exactly");
}

AE_TEST(terrain_preview_idle_2048_seed_skips_snapshot_encoding_and_job_for_real_frames) {
  TerrainUiFixture f;AE_EXPECT_TRUE(f.ready,"fixture");
  resources::TerrainRecipe recipe;recipe.cells=8;recipe.height=0;
  recipe.fx.enabled=true;recipe.fx.width=recipe.fx.height=2048;
  recipe.fx.seedPixels.assign(usize(2048)*2048*4,17);
  std::string error;AE_EXPECT_TRUE(f.session.applyTerrain(recipe,error),error.c_str());
  // Existing authored data may load at full resolution under the core20B/pixel
  // budget; UI editing still independently requires explicit256MiB preflight.
  for(u32 i=0;i<4;++i){f.render();f.session.queueTerrainPreview();}
  const auto before=f.session.screen().terrainFx;
  const auto *seed=f.session.screen().terrainDraft.fx.seedPixels.data();
  const auto geometry=f.session.screen().terrainPreviewBuild;
  for(u32 i=0;i<64;++i){f.session.queueTerrainPreview();f.render();}
  const auto &after=f.session.screen().terrainFx;
  AE_EXPECT_EQ(after.previewFullEncodes,before.previewFullEncodes,"actual update+queue+UI footer idle frames do not encode16MiB seed");
  AE_EXPECT_EQ(after.previewSnapshotCopies,before.previewSnapshotCopies,"idle queue performs no recipe snapshot copy");
  AE_EXPECT_EQ(after.previewJobsQueued,before.previewJobsQueued,"idle frames enqueue no duplicate work");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.fx.seedPixels.data()==seed,"authoring seed retains exact storage identity");
  AE_EXPECT_TRUE(f.session.screen().terrainPreviewBuild==geometry,"idle retains immutable built geometry");
  AE_EXPECT_TRUE(!after.recipePending,"cached footer state reflects actual applied recipe");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(3,1),"real geometric edit changes nonFX key");
  f.session.queueTerrainPreview();
  AE_EXPECT_TRUE(f.session.screen().terrainFx.previewFullEncodes>before.previewFullEncodes &&
    f.session.screen().terrainFx.previewSnapshotCopies>before.previewSnapshotCopies &&
    f.session.screen().terrainFx.previewJobsQueued>before.previewJobsQueued,"dirty positive control reaches real encoder/snapshot/job producers");
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainClose)),"closing cancels pending dirty preview");
}

AE_TEST(terrain_fx_transform_modes_expose_real_tools_gizmo_down_and_navigation_cancels_paint) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  resources::TerrainRecipe recipe;recipe.cells=16;recipe.height=0;std::string error;
  AE_EXPECT_TRUE(f.ready && f.session.applyTerrain(recipe,error),error.c_str());
  AE_EXPECT_TRUE(f.visit(2) && f.tap(fx(0)),"open real FX context");
  const auto find=[&](u32 widget,ui::UiPoint &point){for(float y=2;y<f.session.screen().surface.height;y+=2)for(float x=2;x<f.session.screen().surface.width;x+=2)if(f.router.hitTest({x,y}).widgetId==widget){point={x,y};return true;}return false;};
  const auto click=[&](u32 widget){f.render();ui::UiPoint point;if(!find(widget,point))return false;f.time+=1;f.session.handlePointer({60,ui::UiPointerPhase::Down,point,f.time});f.session.handlePointer({60,ui::UiPointerPhase::Up,point,f.time+.1});return true;};
  ui::UiPoint point;f.render();
  AE_EXPECT_TRUE(!find(id(Widget::ToolMove),point),"Edit mode hides transform rail");
  AE_EXPECT_TRUE(f.tap(fx(24)) && f.tap(fx(3)),"explicit Transformar mode");
  for(const auto tool:{Widget::ToolMove,Widget::ToolRotate,Widget::ToolScale}){
    AE_EXPECT_TRUE(click(id(tool)),"real transform tool visible and touchable");f.render();bool hit=false;
    for(u32 axis=0;axis<6 && !hit;++axis)hit=find(id(Widget::GizmoAxisBase,axis),point);
    AE_EXPECT_TRUE(hit,"selected transform exposes actual gizmo region while terrain stays open");
    f.time+=1;f.session.handlePointer({61,ui::UiPointerPhase::Down,point,f.time});
    AE_EXPECT_TRUE(f.session.screen().activeGizmoAxis!=editor::EditorGizmoHandle::None,"routed Down activates real transform handle");
    f.session.handlePointer({61,ui::UiPointerPhase::Cancel,point,f.time+.1});
  }
  AE_EXPECT_TRUE(f.tap(fx(24)) && f.tap(fx(1)),"return Edit exclusively");f.render();
  AE_EXPECT_TRUE(!find(id(Widget::ToolMove),point),"Edit hides rail again");
  AE_EXPECT_TRUE(f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(24)) && f.tap(fx(2)),"create actual mask and enter Paint");f.render();
  AE_EXPECT_TRUE(!find(id(Widget::ToolMove),point),"Paint hides transform rail");
  AE_EXPECT_TRUE(f.point(fx(90),point),"actual bitmap pointer target");
  f.time+=1;f.session.handlePointer({62,ui::UiPointerPhase::Down,point,f.time});
  f.session.handlePointer({62,ui::UiPointerPhase::Move,point,f.time+.1});f.render();
  AE_EXPECT_TRUE(find(id(Widget::NavigationOrbit),point),"navigation control remains discoverable");
  f.session.handlePointer({63,ui::UiPointerPhase::Down,point,f.time+.2});
  AE_EXPECT_EQ(f.session.screen().terrainFx.mode,0u,"navigation Down exits Paint before camera handling");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.fx.strokes.empty(),"navigation cancels captured paint atomically");
  f.session.handlePointer({63,ui::UiPointerPhase::Up,point,f.time+.3});
  AE_EXPECT_TRUE(!f.session.screen().terrainBrushArmed,"navigation never rearms PBR painting");
}

AE_TEST(terrain_fx_auxiliary_canvas_vertical_exit_scrolls_without_committing_authoring) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(24)) && f.tap(fx(2)),"create bitmap and enter paint");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(fx(90),point),"auxiliary canvas is visible");
  const auto before=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  const auto canvas=f.session.screen().terrainFx.canvas;
  f.time+=1;
  f.session.handlePointer({79,ui::UiPointerPhase::Down,point,f.time});
  point.y=canvas.y-40;
  f.session.handlePointer({79,ui::UiPointerPhase::Move,point,f.time+.1});
  f.session.handlePointer({79,ui::UiPointerPhase::Up,point,f.time+.2});
  AE_EXPECT_TRUE(f.session.screen().terrainScroll>0,"vertical swipe leaving bitmap scrolls panel");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),before,"panel scrolling cannot publish captured brush samples");
  AE_EXPECT_TRUE(!f.session.screen().terrainFx.cursorVisible,"cancelled canvas gesture clears cursor");
}

AE_TEST(terrain_fx_auxiliary_canvas_vertical_paint_preserves_all_brush_directions) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(24)) && f.tap(fx(2)),"prepare FX paint");
  f.render();const auto canvas=f.session.screen().terrainFx.canvas;
  ui::UiPoint point{canvas.x+canvas.width/2,canvas.y+canvas.height/2};
  AE_EXPECT_EQ(f.router.hitTest(point).widgetId,fx(90),"actual bitmap starts paint");
  f.time+=1;f.session.handlePointer({78,ui::UiPointerPhase::Down,point,f.time});point.y-=20;
  AE_EXPECT_TRUE(canvas.contains(point),"vertical stroke stays inside bitmap");
  f.session.handlePointer({78,ui::UiPointerPhase::Move,point,f.time+.1});f.session.handlePointer({78,ui::UiPointerPhase::Up,point,f.time+.2});
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.fx.strokes.empty(),"vertical stroke commits real FX authoring");
  AE_EXPECT_EQ(f.session.screen().terrainScroll,0.f,"painting does not scroll panel");
  AE_EXPECT_TRUE(f.tap(fx(100)) && f.tap(fx(110)) && f.tap(fx(101)) && f.tap(fx(111)),"fixture uses actual non-paint scroll route to both material pages");
}

AE_TEST(terrain_fx_cursor_matches_rotated_local_brush_world_surface_and_mode) {
  editor::TerrainFxUiState ui;ui.mode=1;ui.cursorVisible=true;ui.cursorU=.5f;ui.cursorV=.5f;
  ui.brush.radiusMeters=2;ui.brush.aspect=2;ui.brush.rotationDegrees=90;ui.brush.shape=resources::TerrainFxBrushShape::Box;
  resources::TerrainFxAsset fx;fx.enabled=true;fx.domain={-8,-8,16,16};
  float world[16]{2,0,0,0,0,3,0,0,0,0,4,0,10,20,30,1};
  const auto lines=editor::terrainFxCursorSegments(ui,fx,world,[](float x,float z){return x+z;});
  AE_EXPECT_EQ(lines.size(),usize(64),"all box edges follow sixteen terrain samples");
  AE_EXPECT_TRUE(std::abs(lines[0][0]-14)<.001f && std::abs(lines[0][2]-14)<.001f,"rotation aspect and world transform preserve actual metre footprint");
  AE_EXPECT_TRUE(std::abs(lines[0][1]-14.15f)<.001f,"cursor follows terrain height then root transform");
  const auto raised=editor::terrainFxCursorSegments(ui,fx,world,[](float x,float z){return x+z+10*std::max(0.f,1-std::abs(z)/4);});
  AE_EXPECT_TRUE(std::abs(raised[8][1]-56.15f)<.001f,"mid-edge terrain elevation deforms the box contour between its corners");
  ui.mode=0;AE_EXPECT_TRUE(editor::terrainFxCursorSegments(ui,fx,world,[](float,float){return 0.f;}).empty(),"Edit hides paint cursor");
  ui.mode=2;AE_EXPECT_TRUE(editor::terrainFxCursorSegments(ui,fx,world,[](float,float){return 0.f;}).empty(),"Transform hides paint cursor");
  ui.mode=1;ui.cursorVisible=false;AE_EXPECT_TRUE(editor::terrainFxCursorSegments(ui,fx,world,[](float,float){return 0.f;}).empty(),"Cancel hides cursor");
}

AE_TEST(terrain_fx_desktop_context_keeps_first_controls_and_root_back_without_pbr_navigation) {
  TerrainUiFixture f(1280,720);const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)),"open actual FX context");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+64+0x100,point),"target slider is visible before scrolling on desktop");
  AE_EXPECT_TRUE(!f.point(id(Widget::TerrainMaterialLibrary),point) && !f.point(id(Widget::TerrainLayerBase),point),"unrelated PBR rail stays on root painting page");
  AE_EXPECT_TRUE(f.point(fx(4),point) && f.point(fx(1),point),"desktop keeps direct real channel and mode choices");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(61)),false),"root painting breadcrumb remains directly reachable");
  AE_EXPECT_EQ(f.session.screen().terrainMaterialView,0u,"Back returns to root painting capabilities");
  f.render();
  AE_EXPECT_TRUE(f.point(id(Widget::TerrainMaterialLibrary),point) && f.point(id(Widget::TerrainLayerBase),point),"all PBR controls remain reachable through root");
  AE_EXPECT_TRUE(f.tap(fx(0)) && f.tap(fx(100)) && f.tap(fx(104)) && f.tap(fx(114)),"open actual per-material porosity authority");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(63))) && f.session.setTerrainNumber(349,.62),"contextual porosity retains explicit layer selection");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[1].snowPorosity,.62f,"selected material receives the shared authoring value");
}

AE_TEST(terrain_stream_ui_real_source_packed_controls_all_groups_and_independent_layers) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(120)),"open real Streams material context");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+112+0x100,point),"Stream maximum is visible at zero scroll on compact screen");
  AE_EXPECT_TRUE(!f.tap(fx(137)),"missing mask/source cannot enable consumer");
  AE_EXPECT_TRUE(f.tap(fx(133)) && f.tap(fx(143)),"explicit original Stream preset resolves actual catalog identity");
  const auto *source=editor::terrainFlowPreset(*f.session.screen().terrainCatalog,false,0);
  AE_EXPECT_TRUE(source && f.session.screen().terrainDraft.surface.flow.streamNormal.texture==source->identity,"source identity comes from catalog metadata");
  AE_EXPECT_TRUE(f.tap(fx(135)) && f.textureThumbnail(source->id) && f.tap(fx(136)),"actual source thumbnail is shown in real picker");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(416,.8) && f.session.setTerrainNumber(417,.7) && f.session.setTerrainNumber(419,.6) && f.session.setTerrainNumber(423,.1) && f.session.setTerrainNumber(427,.9),"logical raw RG and foam B transforms are authorable");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainSnowAction(105))),"map residency override remains available on real packed source");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.flow.streamNormal.textureResidency.mode,resources::TerrainTextureResidencyMode::Cap,"packed source uses existing author residency policy");
  AE_EXPECT_TRUE(!f.session.setTerrainNumber(420,.4) && !f.session.setTerrainNumber(424,.4) && !f.session.setTerrainNumber(428,.4),"reserved Stream alpha controls cannot mutate through exact entry");
  AE_EXPECT_TRUE(f.tap(fx(145)) && f.tap(fx(20)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(25)) && f.tap(fx(6)) && f.tap(fx(24)) && f.tap(fx(2)),"explicit FX creation and independent B painting");
  f.render();ui::UiPoint paint;AE_EXPECT_TRUE(f.point(fx(90),paint),"actual bitmap reached for Stream B gesture");
  f.time+=1;f.session.handlePointer({83,ui::UiPointerPhase::Down,paint,f.time});paint.x+=4;
  f.session.handlePointer({83,ui::UiPointerPhase::Move,paint,f.time+.1});f.session.handlePointer({83,ui::UiPointerPhase::Up,paint,f.time+.2});
  const auto painted=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()) && f.session.history().redo(f.session.document()),"actual independent B gesture supports editor history");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),painted,"history restores full B checkpoint authoring");
  AE_EXPECT_TRUE(f.tap(fx(120)) && f.tap(fx(137)),"actual painted mask unlocks Stream material consumer");
  AE_EXPECT_TRUE(f.slide(368,.6f,1) && f.tap(editor::terrainSnowNumberBase+113) && f.session.completeTextEdit(f.session.pendingTextEdit(),"3.5",true),"Stream maximum bar and precise contrast use real input");
  AE_EXPECT_TRUE(f.tap(fx(123)),"open phase/refraction group");
  for(const auto field:{370u,371u,372u,373u})AE_EXPECT_TRUE(f.session.setTerrainNumber(field,.2),"all four Stream flow parameters have real authoring gates");
  AE_EXPECT_TRUE(f.tap(fx(124)) && f.slide(374,.4f) && f.session.setTerrainNumber(375,9) && f.tap(fx(141)) && f.session.setTerrainNumber(385,.6),"foam and normal response authoring");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainModuleAction(63))) && f.session.setTerrainNumber(385,1.3),"per-material foam retains selected layer routing");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[0].flow.foam,.6f,"previous material foam is independent");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.layers[1].flow.foam,1.3f,"selected material foam changes");
  AE_EXPECT_TRUE(f.tap(fx(125)) && f.tap(fx(139)),"height window consumer enabled");
  for(u32 i=0;i<4;++i)AE_EXPECT_TRUE(f.session.setTerrainNumber(381+i,std::array<double,4>{-10,0,100,200}[i]),"all four world height values are authorable");
  AE_EXPECT_TRUE(f.slide(381,1) && f.slide(382,0),"height bars keep ordered neighbors without rejected dead motion");
  AE_EXPECT_TRUE(f.tap(fx(126)) && f.session.setTerrainNumber(376,-32) && f.session.setTerrainNumber(377,0),"signed and zero UV repeats are retained");
  const auto &flow=f.session.screen().terrainDraft.surface.flow;
  AE_EXPECT_TRUE(flow.streams && !flow.lava && !flow.globalStreams && !flow.rainDrops,"only actual selected consumers enabled");
  AE_EXPECT_EQ(flow.streamNormalFoam[2],0.f,"reserved Stream vector component preserved");
  AE_EXPECT_EQ(flow.streamUvScales[2],70.f,"reserved source UV remains unchanged");
  AE_EXPECT_EQ(flow.streamNormal.channel,resources::TerrainSurfaceChannel::NormalRG,"Stream fixed RG/B role preserves required NormalRG provenance metadata");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply publishes original packed Stream plus real painted B field");
  const auto root=f.session.screen().terrainTarget;
  if(!root || !f.session.openTerrain(root)){std::fprintf(stderr,"Stream reopen: root=%u view=%u message=%s status=%s\n",root,f.session.screen().terrainMaterialView,f.session.screen().terrainMessage.c_str(),f.session.screen().status.c_str());AE_EXPECT_TRUE(false,"reopen actual Stream material artifact");}
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"Stream source, all authored properties and B checkpoint survive actual publication");
}
AE_TEST(terrain_flow_packed_picker_metadata_filters_queries_and_identity_selection) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(120)) && f.tap(fx(133)) && f.session.setTerrainNumber(416,.7) && f.tap(fx(135)),"open actual Stream packed source picker");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,8u,"new Stream source purpose comes from explicit typed metadata");
  const auto *stream=editor::terrainFlowPreset(*f.session.screen().terrainCatalog,false,0);
  AE_EXPECT_TRUE(stream && (stream->roleMask&(1u<<7)) && f.textureThumbnail(stream->id),"actual raw-packed source is discoverable with real thumbnail");
  AE_EXPECT_TRUE(f.catalogQuery(stream->id) && f.tap(fx(150)),"filtered first slot selects the stable catalog identity");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.flow.streamNormal.texture,stream->identity,"query selection uses filtered identity rather than unfiltered index");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.flow.streamNormal.factor,.7f,"source replacement preserves authored map transforms");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(2))) && f.tap(id(editor::terrainCatalogAction(2))),"explicit Todos remains selectable after Lava purpose");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"user override All is preserved");
  AE_EXPECT_TRUE(f.tap(fx(136)) && f.tap(fx(135)),"reopen same source context");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"saved Todos override wins over default purpose");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogQuery,stream->id,"independent source query survives Back and reentry");
  AE_EXPECT_TRUE(f.tap(fx(136)) && f.tap(fx(145)) && f.tap(fx(121)) && f.tap(fx(134)) && f.tap(fx(135)),"open independent Lava purpose");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,9u,"new Lava picker selects actual RG/B/A role");
  AE_EXPECT_TRUE(f.session.screen().terrainCatalogQuery.empty(),"Stream search does not leak into Lava context");
  const auto *lava=editor::terrainFlowPreset(*f.session.screen().terrainCatalog,true,1);
  AE_EXPECT_TRUE(lava && (lava->roleMask&(1u<<8)) && f.catalogQuery(lava->id) && f.tap(fx(150)),"actual second Lava original is selectable by filtered identity");
  const auto &map=f.session.screen().terrainDraft.surface.flow.lavaDiffuse;
  AE_EXPECT_TRUE(map.texture==lava->identity && map.linear && map.channel==resources::TerrainSurfaceChannel::RGB,"new source is raw linear with explicit immutable packing purpose");
}
AE_TEST(terrain_lava_ui_all_consumed_properties_raw_alpha_and_actual_apply_reopen) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(25)) && f.tap(fx(7)) && f.tap(fx(24)) && f.tap(fx(2)),"create real FX bitmap and select independent Lava A painting");
  f.render();ui::UiPoint painted;
  AE_EXPECT_TRUE(f.point(fx(90),painted),"actual bitmap region is available for the A stroke");
  f.time+=1;f.session.handlePointer({84,ui::UiPointerPhase::Down,painted,f.time});painted.x+=4;
  f.session.handlePointer({84,ui::UiPointerPhase::Move,painted,f.time+.1});f.session.handlePointer({84,ui::UiPointerPhase::Up,painted,f.time+.2});
  bool alpha=false;const auto bitmap=f.session.screen().terrainFx.bitmap;
  if(bitmap)for(usize i=3;i<bitmap->pixels.size();i+=4)alpha=alpha || bitmap->pixels[i]>0;
  AE_EXPECT_TRUE(alpha,"authored static amount A contains actual painted nonzero bytes");
  AE_EXPECT_TRUE(f.tap(fx(121)) && f.tap(fx(143)) && f.tap(fx(138)),"real painted mask and original source enable Lava");
  AE_EXPECT_TRUE(f.slide(390,.2f,1) && f.slide(391,.8f,2) && f.session.setTerrainNumber(398,3.5),"material contrast, maximum and emission controls");
  AE_EXPECT_TRUE(f.tap(fx(128)) && f.session.setTerrainNumber(392,.4) && f.session.setTerrainNumber(393,.2) && f.tap(fx(132)),"Lava flow and nested distortion group");
  for(u32 field=394;field<=397;++field)AE_EXPECT_TRUE(f.session.setTerrainNumber(field,.2),"all source distortion/crust parameters authorable");
  AE_EXPECT_TRUE(f.tap(fx(129)),"three source color groups");
  for(u32 field=403;field<=411;++field)AE_EXPECT_TRUE(f.session.setTerrainNumber(field,1.2),"every consumed RGB is editable");
  AE_EXPECT_TRUE(f.tap(fx(130)) && f.tap(fx(140)),"actual world height filter");
  for(u32 i=0;i<4;++i)AE_EXPECT_TRUE(f.session.setTerrainNumber(412+i,std::array<double,4>{-10,0,100,200}[i]),"all four Lava world heights editable");
  AE_EXPECT_TRUE(f.tap(fx(131)) && f.session.setTerrainNumber(399,-10) && f.session.setTerrainNumber(400,0) && f.tap(fx(142)) && f.session.setTerrainNumber(401,.8) && f.session.setTerrainNumber(402,2),"UV and real stochastic consumer properties");
  AE_EXPECT_TRUE(f.tap(fx(127)) && f.tap(fx(134)),"return from UV to General and open actual Lava packed source");
  if(!f.tap(fx(144))){std::fprintf(stderr,"Lava source2: view=%u scroll=%g message=%s status=%s\n",f.session.screen().terrainMaterialView,double(f.session.screen().terrainScroll),f.session.screen().terrainMessage.c_str(),f.session.screen().status.c_str());AE_EXPECT_TRUE(false,"second actual Lava original is reachable");}
  if(!f.tap(fx(146))){std::fprintf(stderr,"Lava invert: view=%u scroll=%g message=%s status=%s\n",f.session.screen().terrainMaterialView,double(f.session.screen().terrainScroll),f.session.screen().terrainMessage.c_str(),f.session.screen().status.c_str());AE_EXPECT_TRUE(false,"Lava inversion is reachable after second source selection");}
  AE_EXPECT_TRUE(f.session.setTerrainNumber(420,.7),"consumed Lava alpha factor is authorable after inversion");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(424,.1),"consumed Lava alpha minimum is authorable after inversion");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(428,.9),"consumed Lava alpha maximum is authorable after inversion");
  const auto &flow=f.session.screen().terrainDraft.surface.flow;
  AE_EXPECT_EQ(flow.lavaDisplacementScale,1.f,"unsupported geometry displacement value is preserved without fake control");
  AE_EXPECT_EQ(flow.lavaEdgeColor[3],1.f,"reserved color alpha preserved");
  AE_EXPECT_TRUE(!flow.globalStreams && !flow.rainDrops && !flow.specularFade,"unsupported groups remain off");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply uses real V4 material and static FX bridge");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root && f.session.openTerrain(root),"reopen actual source artifact");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"all consumed and reserved Flow values survive actual publication and reimport");
}

AE_TEST(terrain_specular_height_ui_requires_no_fx_or_texture_and_roundtrips_actual_source) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(161)),"Spec-only is discoverable directly from Paint root");
  f.render();ui::UiPoint point;
  AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+175,point),"first world-height slider is visible before scrolling on compact layout");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.fx.enabled && f.tap(fx(163)),"actual Spec-only consumer enables without creating a mask");
  AE_EXPECT_TRUE(f.session.setTerrainNumber(431,-5) && f.tap(editor::terrainSnowNumberBase+176) && f.session.completeTextEdit(f.session.pendingTextEdit(),"25",true),"signed world-height precise authoring");
  AE_EXPECT_TRUE(f.slide(431,1) && f.slide(432,0),"ordered endpoints permit the real coincident interval");
  AE_EXPECT_EQ(f.session.screen().terrainDraft.surface.flow.specularFades[0],f.session.screen().terrainDraft.surface.flow.specularFades[1],"coincident endpoints are not falsely forbidden");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply publishes actual Spec-only material with no FX descriptor");
  f.render();AE_EXPECT_TRUE(!f.point(fx(2),point) && !f.point(id(Widget::TerrainSculpt),point),"missing FX authoring cannot expose a pretend active paint tool");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root && f.session.openTerrain(root),"reopen actual Spec-only artifact");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"Spec-only source roundtrip retains values without inventing an FX map");
  AE_EXPECT_TRUE(!f.session.screen().terrainDraft.fx.enabled,"Spec-only still has no authored FX bitmap after reopen");
}
AE_TEST(terrain_rain_ui_real_packed_source_requires_puddles_preserves_all_roles_and_roundtrips) {
  TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
  AE_EXPECT_TRUE(f.ready && f.visit(2) && f.tap(fx(0)) && f.tap(fx(160)),"Rain is a real material subcontext");
  f.render();ui::UiPoint point;AE_EXPECT_TRUE(f.point(editor::terrainSnowSliderBase+173,point),"Rain intensity visible at zero scroll");
  AE_EXPECT_TRUE(!f.tap(fx(162)),"missing puddle/source disables Rain enable");
  AE_EXPECT_TRUE(f.tap(fx(164)) && f.tap(fx(166)) && f.tap(fx(165)),"explicit original Rain source and actual picker");
  const auto *rain=editor::terrainRainPreset(*f.session.screen().terrainCatalog);
  AE_EXPECT_TRUE(rain && (rain->roleMask&(1u<<9)) && f.textureThumbnail(rain->id),"real Rain metadata purpose and thumbnail are present");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,10u,"Rain purpose is raw RGBA, not filtered Normal");
  AE_EXPECT_TRUE(f.catalogQuery(rain->id) && f.tap(fx(170)),"actual filtered texture identity selected");
  AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(2))),"explicit All override remains accessible");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"All remains a user choice");
  AE_EXPECT_TRUE(f.tap(fx(167)) && f.tap(fx(165)),"leave and reenter Rain picker");
  AE_EXPECT_EQ(f.session.screen().terrainCatalogRoleFilter,0u,"saved All choice survives source context");
  AE_EXPECT_TRUE(f.tap(fx(167)) && f.tap(fx(168)),"raw source inversion is effectful");
  for(u32 channel=0;channel<4;++channel){
    AE_EXPECT_TRUE(f.session.setTerrainNumber(417+channel,.8) && f.session.setTerrainNumber(421+channel,.05) && f.session.setTerrainNumber(425+channel,.95),"R shape, GB normal direction and A phase transforms are all editable");
  }
  AE_EXPECT_TRUE(f.tap(fx(167)) && !f.tap(fx(162)),"source alone cannot bypass missing real puddles");
  AE_EXPECT_TRUE(f.tap(fx(101)) && f.tap(fx(23)) && f.tap(fx(10)) && f.tap(fx(25)) && f.tap(fx(5)) && f.tap(fx(24)) && f.tap(fx(2)),"Rain to actual Puddles create-source route prepares real G mask");
  f.render();AE_EXPECT_TRUE(f.point(fx(90),point),"actual G bitmap reached");
  f.time+=1;f.session.handlePointer({82,ui::UiPointerPhase::Down,point,f.time});point.x+=4;
  f.session.handlePointer({82,ui::UiPointerPhase::Move,point,f.time+.1});f.session.handlePointer({82,ui::UiPointerPhase::Up,point,f.time+.2});
  AE_EXPECT_TRUE(f.tap(fx(101)) && f.tap(fx(111)) && f.tap(fx(20)) && f.tap(fx(160)) && f.tap(fx(162)),"real painted puddle consumer unlocks Rain");
  AE_EXPECT_TRUE(f.slide(429,.7f) && f.session.setTerrainNumber(430,-64) && f.session.setTerrainNumber(430,0),"intensity and signed/zero repeat frequency remain source-faithful");
  AE_EXPECT_TRUE(f.tap(fx(101)) && f.tap(fx(111)),"attempting to remove required puddles receives explicit editor handling");
  AE_EXPECT_TRUE(f.session.screen().terrainDraft.surface.flow.puddles && f.text("Desligue as ondulações"),"dependency refusal is explained without silently disabling Rain");
  const auto &flow=f.session.screen().terrainDraft.surface.flow;
  AE_EXPECT_EQ(flow.sourceRainMode,resources::TerrainFlowRainMode::None,"unconsumed source enum is preserved without fake UI");
  AE_EXPECT_EQ(flow.rainIntensityScale[2],0.f,"reserved Rain vector Z preserved");
  AE_EXPECT_EQ(flow.rainIntensityScale[3],0.f,"reserved Rain vector W preserved");
  const auto expected=resources::serializeTerrainRecipe(f.session.screen().terrainDraft);
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainGenerate),false) && f.finish(),"Apply publishes actual Rain packed source over G mask");
  const auto root=f.session.screen().terrainTarget;
  AE_EXPECT_TRUE(root && f.session.openTerrain(root),"reopen actual Rain material artifact");
  AE_EXPECT_EQ(resources::serializeTerrainRecipe(f.session.screen().terrainDraft),expected,"Rain source/transforms/intensity and FX checkpoint survive actual source reimport");
}

AE_TEST(terrain_catalog_search_accepts_complete_source_sha_and_custom_keyboard_without_name_capacity_changes) {
 TerrainUiFixture f;const auto fx=[](u32 a){return id(editor::terrainFxAction(a));};
 AE_EXPECT_TRUE(f.ready&&f.visit(2)&&f.tap(fx(0))&&f.tap(fx(120))&&f.tap(fx(133))&&f.tap(fx(135)),"open actual packed source picker");
 const auto*source=editor::terrainFlowPreset(*f.session.screen().terrainCatalog,false,0);AE_EXPECT_TRUE(source&&source->id.size()==64,"actual source identifier is complete SHA64");
 AE_EXPECT_TRUE(f.catalogQuery(source->id),"native text commit accepts complete source SHA");
 AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(0))),"reopen source SHA search");
 AE_EXPECT_EQ(f.session.pendingTextEdit().text,source->id,"reopen preserves all64 bytes without rename buffer truncation");
 const auto modalTap=[&](Widget widget){f.render();const auto surface=f.session.screen().surface;for(float y=surface.y+2;y<surface.bottom();y+=4)for(float x=surface.x+2;x<surface.right();x+=4)if(f.router.hitTest({x,y}).widgetId==id(widget)){f.time+=1;f.session.handlePointer({95,ui::UiPointerPhase::Down,{x,y},f.time});f.session.handlePointer({95,ui::UiPointerPhase::Up,{x,y},f.time+.1});return true;}return false;};
 AE_EXPECT_TRUE(modalTap(Widget::NameBackspace),"custom keyboard backspace applies to dedicated search draft");
 const auto shortened=f.session.pendingTextEdit().text;AE_EXPECT_EQ(shortened,source->id.substr(0,63),"custom keyboard updates full source draft");
 AE_EXPECT_TRUE(modalTap(Widget::NameCancel),"cancel custom search edit");AE_EXPECT_EQ(f.session.screen().terrainCatalogQuery,source->id,"cancel preserves authored query");
 AE_EXPECT_EQ(f.session.pendingTextEdit().purpose,editor::EditorTextPurpose::None,"Cancel closes the actual foreground search modal");
 AE_EXPECT_TRUE(f.tap(id(editor::terrainCatalogAction(0))),"open for explicit search budget");
 AE_EXPECT_TRUE(!f.session.completeTextEdit(f.session.pendingTextEdit(),std::string(129,'a'),true),"129-byte query is refused rather than truncated");
 AE_EXPECT_TRUE(f.session.completeTextEdit(f.session.pendingTextEdit(),std::string(128,'a'),true),"full128-byte search budget is supported");
}

#include "renderer/rendering_settings_file.h"
AE_TEST(viewport_quality_controls_persist_without_mutating_game_settings) {
  TerrainUiFixture f(1024,420);AE_EXPECT_TRUE(f.ready,"actual editor fixture");
  renderer::ProjectRenderingSettings game;game.preset=renderer::QualityPreset::S;
  game.upscalingFilter=renderer::UpscalingFilter::ArmAsr;
  f.session.setRenderingSettings(game);
  const auto original=renderer::writeRenderingSettings(f.session.renderingSettings());
  AE_EXPECT_TRUE(f.tap(id(Widget::TerrainClose)),"return to actual Scene viewport");
  const auto tapScreen=[&](Widget widget) {
    f.render();const auto surface=f.session.screen().surface;
    for(float y=surface.y+2;y<surface.bottom();y+=2) for(float x=surface.x+2;x<surface.right();x+=2)
      if(f.router.hitTest({x,y}).widgetId==id(widget)) {
        f.time+=1;f.session.handlePointer({97,ui::UiPointerPhase::Down,{x,y},f.time});
        f.session.handlePointer({97,ui::UiPointerPhase::Up,{x,y},f.time+.1});return true;
      }
    return false;
  };
  AE_EXPECT_TRUE(tapScreen(Widget::SceneEffectsMenu),"real Scene options menu");
  AE_EXPECT_TRUE(tapScreen(Widget::ViewportLow),"low touch target");
  AE_EXPECT_TRUE(f.session.screen().viewportSettings.quality==renderer::EditorViewportQuality::Low,"low consumed by model");
  AE_EXPECT_TRUE(tapScreen(Widget::ViewportHigh),"high touch target");
  AE_EXPECT_TRUE(tapScreen(Widget::ViewportAnimateMaterials),"animated material preview touch target");
  f.render();
  AE_EXPECT_EQ(renderer::writeRenderingSettings(f.session.renderingSettings()),original,"authoring controls preserve game graphics");
  renderer::ProjectRenderingSettings requested;
  AE_EXPECT_TRUE(!f.session.takeRenderingSettingsRequest(requested),"no project quality apply request from authoring");
  editor::EditorSession reopened;reopened.initialize(&f.font,&f.icons);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(f.path.string().c_str()),"reopen actual saved editor preferences");
  AE_EXPECT_TRUE(reopened.screen().viewportSettings.quality==renderer::EditorViewportQuality::High &&
      reopened.screen().viewportSettings.animatedMaterials,"quality and animation survive reopening");
}

#include "scene/ui_canvas.h"
AE_TEST(play_and_player_update_keep_runtime_gui_input_and_authored_source_immutable) {
  for(bool player:{false,true}) {
    TerrainUiFixture f(1024,420);AE_EXPECT_TRUE(f.ready&&f.tap(id(Widget::TerrainClose)),"actual session leaves terrain authoring");
    ui::GuiDocument source;std::string error;
    const auto button=source.create(ui::GuiKind::Toggle);auto node=*source.find(button);
    node.offsets={40,40,160,100};node.interaction.clickable=true;
    AE_EXPECT_TRUE(source.update(node,error),"real runtime toggle source");
    f.session.gui().document()=source;AE_EXPECT_TRUE(f.session.gui().save(),"registered persistent GUI resource");
    resources::AssetRegistry registry;
    AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(f.session.serializeAssets(),registry),"read actual registry");
    const auto *record=registry.findByPath("UI/main.aeui");AE_EXPECT_TRUE(record,"registered main GUI");
    if(!record)return;
    auto &document=f.session.document();const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Game HUD");
    auto object=*document.find(owner);auto *canvas=static_cast<scene::UiCanvas*>(object.components.add(scene::UiCanvas::descriptor));
    canvas->document=record->guid;canvas->mode=0;
    AE_EXPECT_TRUE(document.applyEntityValues(owner,object),"gameplay Canvas owns its presentation");
    f.session.setStandalonePlayer(player);
    AE_EXPECT_TRUE(f.session.startPlay(),"real Play request accepted");
    std::vector<renderer::MapDrawState> frame;
    AE_EXPECT_TRUE(f.session.extractPlayMap(frame),"backend starts the real execution world");f.session.update();
    auto &play=const_cast<editor::EditorPlayScene &>(f.session.playScene());
    const auto lease=play.sceneGui().instanceFor(play.world(),owner);
    const auto *live=play.sceneGui().find(play.world(),lease);AE_EXPECT_TRUE(live,"actual world GUI instance");
    if(!live)return;
    const auto *placement=live->runtime.placement(button);AE_EXPECT_TRUE(placement,"runtime GUI received fresh layout");
    if(!placement)return;
    const ui::UiPoint point{placement->bounds.x+placement->bounds.width*.5f,placement->bounds.y+placement->bounds.height*.5f};
    f.session.handlePointer({98,ui::UiPointerPhase::Down,point,20});
    f.session.handlePointer({98,ui::UiPointerPhase::Up,point,20.1});f.session.update();
    AE_EXPECT_EQ(live->runtime.document().find(button)->value,1.f,"actual routed touch changes only live GUI state");
    AE_EXPECT_EQ(f.session.gui().document().find(button)->value,0.f,"authored GUI source remains immutable");
    AE_EXPECT_TRUE(f.session.layout().inspectorPanel.isEmpty(),"gameplay has no authoring Inspector");
    AE_EXPECT_TRUE(player?f.session.layout().topBar.isEmpty():!f.session.layout().topBar.isEmpty(),"Player omits editor chrome; Play preview keeps Stop controls");
    f.session.setStandalonePlayer(false);f.render();bool stopped=false;
    for(float y=0;y<f.session.screen().surface.height&&!stopped;y+=2)
      for(float x=0;x<f.session.screen().surface.width&&!stopped;x+=2)
        if(f.router.hitTest({x,y}).widgetId==id(Widget::PlayFromTopBar)) {
          f.session.handlePointer({99,ui::UiPointerPhase::Down,{x,y},21});
          f.session.handlePointer({99,ui::UiPointerPhase::Up,{x,y},21.1});stopped=true;
        }
    AE_EXPECT_TRUE(stopped,"real Stop control is reachable when editor chrome is restored");f.session.update();
    AE_EXPECT_TRUE(!f.session.isPlaying(),"Stop ends the world without keeping a second gameplay simulation");
  }
}

AE_TEST(viewport_options_effects_remain_above_compact_navigation_touch_targets) {
 for(const auto size:{ui::UiPoint{655,300},ui::UiPoint{1280,720}}) {
  TerrainUiFixture f(size.x,size.y);AE_EXPECT_TRUE(f.ready&&f.tap(id(Widget::TerrainClose)),"actual Scene view");
  f.render();bool opened=false;
  for(float y=0;y<size.y&&!opened;y+=2)for(float x=0;x<size.x&&!opened;x+=2)
   if(f.router.hitTest({x,y}).widgetId==id(Widget::SceneEffectsMenu)) {
    f.session.handlePointer({101,ui::UiPointerPhase::Down,{x,y},30});f.session.handlePointer({101,ui::UiPointerPhase::Up,{x,y},30.1});opened=true;
   }
  AE_EXPECT_TRUE(opened,"open real viewport options");f.render();ui::UiRect title{};
  for(const auto &c:f.list.commands())if(c.kind==ui::UiPrimitive::Text&&f.list.textOf(c)=="Viewport do editor · 30 FPS")title=c.bounds;
  AE_EXPECT_TRUE(!title.isEmpty(),"production popup heading");
  const Widget effects[]{Widget::SceneSkyToggle,Widget::SceneFogToggle,Widget::ScenePostToggle};
  for(u32 i=0;i<3;++i)AE_EXPECT_EQ(f.router.hitTest({title.x+34.f+i*80.f,title.y+140.f}).widgetId,id(effects[i]),"effect center stays above viewport navigation");
 }
}
