#include "editor/editor_map_scene.h"
#include "editor/terrain_material_icons.h"
#include "editor/editor_screen.h"
#include "editor/terrain_controls.h"
#include "editor/editor_terrain_noise_controls.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::editor {
using namespace ui;
namespace {
UiRect take(UiRect &r, float h) {
  const UiRect out{r.x, r.y, r.width, h};
  r.y += h;
  r.height = std::max(0.f, r.height - h);
  return out;
}
std::string num(double n) {
  char t[40];
  std::snprintf(t, sizeof(t), "%.5g", n);
  return t;
}
std::string materialDisplayName(std::string name) {
  if(name.starts_with("layer_"))name.erase(0,6);
  std::replace(name.begin(),name.end(),'_',' ');
  return name;
}
std::string terrainNoiseSourceName(const resources::TerrainCatalogTexture &source) {
  auto name=source.originalAssetPath.empty()?source.sourcePath:source.originalAssetPath;
  const auto slash=name.find_last_of("/\\");
  if(slash!=std::string::npos) name.erase(0,slash+1);
  const auto dot=name.find_last_of('.');
  if(dot!=std::string::npos) name.resize(dot);
  return name.empty()?source.id:materialDisplayName(name);
}
std::string terrainMaterialContext(const EditorScreenState &s) {
  if(s.terrainTab!=2) return {};
  const auto view=s.terrainMaterialView;
  if(view==60) return "Variantes · Estocástico";
  if(view==62 || view==69) return "Variantes · Seleção";
  if(terrainClusterVariantView(view)) {
    const auto variant="Variante "+std::to_string(terrainClusterVariant(view)+1);
    return variant+(view>=100?" · Mapas":view>=90?" · Textura":view>=80?" · Material":" · Transformação");
  }
  if(view==50) return "Distância · Reamostragem";
  if(view==51 || view==59) return "Distância · Noise";
  if(view==30) return "Anti-tile · Conjunto";
  if(!terrainNoiseView(view)) return {};
  const auto selection=terrainNoiseSelection(view);
  if(selection>=10 && selection<=12) return selection==10?"Anti-tile · Normal":selection==11?"Anti-tile · Detalhe":"Anti-tile · Distância";
  const auto role=selection<4?(selection%2==0?std::string("Detalhe"):std::string("Distância")):"Normal "+std::to_string((selection-4)%3+1);
  return "Ruído · "+role+(terrainNoiseLocal(view)?" local":" global");
}
bool compact(const UiRect &a) { return a.height < 400; }
float contentHeight(const EditorScreenState &s) {
  constexpr float padding = 12;
  const bool hf = s.terrainDraft.mode == resources::TerrainMode::Heightfield;
  if (s.terrainBusy)
    return 182 + padding;
  if (s.terrainTab == 0) {
    if (s.terrainSection == 0)
      return (hf ? 348.f : 298.f) + padding;
    if (s.terrainSection == 1)
      return 410 + padding;
    return (hf ? 550.f : 502.f) + padding;
  }
  if (s.terrainTab == 4) {
    if (s.terrainDraft.paths.empty())
      return 104 + padding;
    return (s.terrainSection == 0   ? 612.f
            : s.terrainSection == 1 ? 874.f
                                    : 602.f) +
           (s.terrainSection && compact(terrainPanelRect(s)) ? -50.f : 0.f) +
           padding;
  }
  if (s.terrainTab == 1) {
    const auto &brush = s.terrainBrush;
    if (s.terrainSection == 0) {
      float extent = 650;
      if (brush.tool == resources::TerrainTool::Flatten ||
          brush.tool == resources::TerrainTool::Path)
        extent += 64;
      if (brush.tool == resources::TerrainTool::Dig ||
          brush.tool == resources::TerrainTool::Add ||
          brush.tool == resources::TerrainTool::Paint)
        extent += 56;
      if (brush.tool == resources::TerrainTool::Restore)
        extent += 112;
      return extent + padding;
    }
    if (s.terrainSection == 1)
      return 746 + padding;
    float extent = 508;
    if (brush.shape == resources::TerrainBrushShape::RoundedBox)
      extent += 96;
    if (brush.shape == resources::TerrainBrushShape::Hemisphere ||
        brush.shape == resources::TerrainBrushShape::Cone)
      extent += 56;
    return extent + padding;
  }
  if (s.terrainTab == 2) {
    if(s.terrainMaterialView==1) {
      const auto count=s.terrainCatalog ? std::min<usize>(6,s.terrainCatalog->materials.size()-
        std::min<usize>(s.terrainCatalog->materials.size(),usize(s.terrainCatalogPage)*6)):0;
      return 146.f+(s.terrainCatalog?122.f+112.f*float((count+1)/2):58.f)+padding-24;
    }
    if(s.terrainMaterialView==60) return 914.f+padding-24;
    if(terrainClusterVariantView(s.terrainMaterialView)) {
      if(s.terrainMaterialView>=100) return (s.terrainMapRole>=2 && s.terrainMapRole<=5?702.f:1100.f)+padding-24;
      if(s.terrainMaterialView>=80) return 700.f+padding-24;
      return 650.f+padding-24;
    }
    if(s.terrainMaterialView==30) return 800.f+padding-24;
    if(s.terrainMaterialView==50) return 712.f+padding-24;
    if(terrainNoiseAvailable(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView)) {
      if(terrainNoisePicker(s.terrainMaterialView)) return 838.f+padding-24;
      const auto &n=terrainSelectedNoise(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView);
      return (n.map.channel==resources::TerrainSurfaceChannel::RGB || terrainNoiseNormal(s.terrainMaterialView)?1414.f:1016.f)+
        (terrainNoiseSelection(s.terrainMaterialView)>=10 && terrainNoiseSelection(s.terrainMaterialView)<=12?32.f:0.f)+(u32(n.map.channel)>4 && !terrainNoiseNormal(s.terrainMaterialView)?32.f:0.f)+padding-24;
    }
    if(s.terrainMaterialView==2 && s.terrainBrush.layer<s.terrainDraft.surface.layers.size())return 1164.f+(s.terrainDraft.surface.layers[s.terrainBrush.layer].cluster.enabled?32.f:0.f)+padding-24;
    if(s.terrainMaterialView==3 && s.terrainBrush.layer<s.terrainDraft.surface.layers.size())
      return (s.terrainMapRole>=2 && s.terrainMapRole<=5?514.f:912.f)+padding-24;
    return 662.f+padding-24;
  }
  return 450.f + padding;
}
} // namespace
UiRect terrainPanelRect(const EditorScreenState &s) {
  const auto safe = deflate(s.surface, s.safeArea);
  const float w =
      std::min(safe.width, std::max(308.f, std::min(392.f, safe.width * .30f)));
  const float top =
      safe.height < 400.f ? 8.f : std::min(54.f, safe.height * .16f);
  return {safe.right() - w, safe.y + top, w,
          std::max(0.f, safe.height - top - 8)};
}
UiRect terrainContentRect(const EditorScreenState &s, const UiRect &area) {
  auto body = deflate(area, UiInsets::all(8));
  take(body, compact(area) ? 32 : 56);
  take(body, compact(area) ? 36 : 48);
  if (s.terrainTab < 2 || s.terrainTab == 4)
    take(body, compact(area) ? 28 : 40);
  body.height = std::max(0.f, body.height - (compact(area) ? 64 : 88));
  return body;
}
float terrainScrollLimit(const EditorScreenState &s, const UiRect &area) {
  return std::max(0.f, contentHeight(s) - terrainContentRect(s, area).height);
}
void buildTerrainViewport(const EditorScreenState &s, const UiTheme &theme,
                          UiDrawList &list, UiInputRouter &router,
                          const UiRect &area) {
  if (!s.view || area.isEmpty())
    return;
  list.pushClip(area);
  if (s.terrainBrushArmed && s.terrainCursorVisible) {
    for (usize i = 0; i < s.terrainCursorLines.size(); ++i) {
      const auto &line = s.terrainCursorLines[i];
      UiPoint a, b;
      if (projectSegmentToScreen(*s.view, line.data(), line.data() + 3, a, b)) {
        list.addLine(a, b, 0xb0000000, 4.f);
        list.addLine(a, b, i < 48 ? theme.color.accent : theme.color.text,
                     1.7f);
      }
    }
    const auto p = projectWorldToScreen(*s.view, s.terrainCursor);
    if (p.valid) {
      list.addLine({p.screen.x - 7, p.screen.y}, {p.screen.x + 7, p.screen.y},
                   theme.color.text, 2);
      list.addLine({p.screen.x, p.screen.y - 7}, {p.screen.x, p.screen.y + 7},
                   theme.color.text, 2);
      if (s.terrainTouchOffset)
        list.addLine(s.terrainPointer, p.screen, theme.color.textMuted, 1);
    }
  }
  if (s.terrainTab == 4 && s.document &&
      s.terrainPath < s.terrainDraft.paths.size()) {
    const auto &path = s.terrainDraft.paths[s.terrainPath];
    float world[16];
    if (s.terrainPreviewBuild &&
        editorWorldMatrix(*s.document, s.terrainTarget, world)) {
      const auto center = resources::terrainPathPolyline(
          path, s.terrainDraft.size / s.terrainDraft.cells * .5f);
      const auto localToWorld = [&](std::array<float, 3> p) {
        if (s.terrainPreviewBuild)
          p[1] = resources::terrainFieldHeight(s.terrainPreviewBuild->field,
                                               p[0], p[2]) +
                 .05f;
        std::array<float, 3> result{};
        for (u32 k = 0; k < 3; ++k)
          result[k] = world[12 + k] + world[k] * p[0] + world[4 + k] * p[1] +
                      world[8 + k] * p[2];
        return result;
      };
      const auto segment = [&](std::array<float, 3> a, std::array<float, 3> b,
                               UiColor color, float width) {
        const auto aw = localToWorld(a), bw = localToWorld(b);
        UiPoint from, to;
        if (projectSegmentToScreen(*s.view, aw.data(), bw.data(), from, to))
          list.addLine(from, to, color, width);
      };
      for (usize i = 1; i < center.size(); ++i)
        segment(center[i - 1], center[i], theme.color.accent, 1.4f);
      // Lane and shoulder outlines are expressed in terrain units, projected
      // onto the current preview, and use the actual retained curve samples.
      for (float width : {path.width, path.shoulderWidth}) {
        std::array<float, 3> oldLeft{}, oldRight{};
        const auto color =
            width == path.width ? theme.color.accent : theme.color.textMuted;
        for (usize i = 0; i < center.size(); ++i) {
          const auto a = center[i ? i - 1 : 0],
                     b = center[std::min(i + 1, center.size() - 1)];
          const float dx = b[0] - a[0], dz = b[2] - a[2],
                      length = std::max(.0001f, std::sqrt(dx * dx + dz * dz));
          auto left = center[i], right = center[i];
          left[0] -= dz / length * width * .5f;
          left[2] += dx / length * width * .5f;
          right[0] += dz / length * width * .5f;
          right[2] -= dx / length * width * .5f;
          if (i) {
            segment(oldLeft, left, color, 1.5f);
            segment(oldRight, right, color, 1.5f);
          }
          if (i == 0 || i + 1 == center.size()) {
            if (path.cap == resources::TerrainPathCap::Round) {
              auto previous = left;
              const float direction = i == 0 ? -1.f : 1.f;
              for (u32 j = 1; j <= 24; ++j) {
                const float angle = j * 3.14159265f / 24;
                auto next = center[i];
                next[0] +=
                    (-dz * std::cos(angle) + direction * dx * std::sin(angle)) /
                    length * width * .5f;
                next[2] +=
                    (dx * std::cos(angle) + direction * dz * std::sin(angle)) /
                    length * width * .5f;
                segment(previous, next, color, 1.5f);
                previous = next;
              }
            } else if (path.cap == resources::TerrainPathCap::Square) {
              const float extension = (i == 0 ? -1.f : 1.f) * path.width * .5f;
              auto capLeft = left, capRight = right;
              capLeft[0] += dx / length * extension;
              capLeft[2] += dz / length * extension;
              capRight[0] += dx / length * extension;
              capRight[2] += dz / length * extension;
              segment(left, capLeft, color, 1.5f);
              segment(capLeft, capRight, color, 1.5f);
              segment(capRight, right, color, 1.5f);
            } else
              segment(left, right, color, 1.5f);
          }
          oldLeft = left;
          oldRight = right;
        }
      }
      for (u32 i = 0; i < path.points.size(); ++i) {
        const auto &point = path.points[i];
        const auto at = localToWorld({point.x, point.y, point.z});
        const auto p = projectWorldToScreen(*s.view, at.data());
        if (!p.valid)
          continue;
        const bool selected = i == s.terrainPathPoint;
        const float radius = selected ? 9 : 6;
        list.addRect({p.screen.x - radius - 2, p.screen.y - radius - 2,
                      2 * radius + 4, 2 * radius + 4},
                     theme.color.text, radius + 2);
        list.addRect(
            {p.screen.x - radius, p.screen.y - radius, 2 * radius, 2 * radius},
            selected ? theme.color.accent : theme.color.surface, radius);
        auto style = theme.type.caption;
        style.size = 12;
        list.addText({p.screen.x + 12, p.screen.y - 20, 56, 20},
                     std::to_string(i + 1), theme.color.text, style);
        if (!s.terrainBusy)
          router.addRegion({p.screen.x - 22, p.screen.y - 22, 44, 44},
                           widgetId(EditorWidget::TerrainPathNew) + 0x100 + i);
      }
    }
  }
  const char *tools[]{"Elevar",    "Baixar", "Nivelar", "Suavizar", "Escavar",
                      "Adicionar", "Pintar", "Caminho", "Ruído",    "Terraços",
                      "Restaurar", "Aguçar", "Buraco"};
  const std::string status =
      s.terrainBrushArmed
          ? std::string(tools[std::min(12u, u32(s.terrainBrush.tool))]) +
                (s.terrainBusy          ? " · confirmando"
                 : s.terrainLivePreview ? " · prévia ao vivo"
                                        : " · toque e arraste")
      : s.terrainTab == 4 ? "Traçado · arraste os pontos"
                          : "Navegar · terreno protegido";
  UiRect chip{area.x + 12, area.y + 52, std::min(285.f, area.width - 24), 32};
  list.addRect(chip, theme.color.surface, 6);
  auto type = theme.type.caption;
  type.size = 12;
  list.addText(deflate(chip, UiInsets::all(8)), status, theme.color.text, type);
  (void)router; // Undo/Redo stay in the main bar; framing is in the dock title.
  list.popClip();
}
void buildTerrainSurface(const EditorScreenState &s, const UiTheme &theme,
                         UiDrawList &list, UiInputRouter &router,
                         const UiRect &area) {
  list.addRect(area, theme.color.surface);
  list.addBorder(area, theme.color.lineSoft, 1);
  router.addBlocker(area);
  const auto text = [&](UiRect box, std::string_view value, UiColor color,
                        bool small = false, UiAlign align = UiAlign::Start) {
    list.pushClip(box);
    auto style = small ? theme.type.caption : theme.type.body;
    style.size = small ? 12.f : 14.f;
    list.addText(deflate(box, UiInsets{8, 0, 8, 0}), value, color, style,
                 align);
    list.popClip();
  };
  const auto button = [&](UiRect rect, std::string_view value, EditorWidget id,
                          bool selected = false, bool enabled = true,
                          UiIcon icon = UiIcon::None) {
    if(id==EditorWidget::TerrainMaterialLibrary)selected=s.terrainMaterialView==1;
    else if(id==EditorWidget::TerrainMaterialProperties)selected=s.terrainMaterialView==2;
    else if(id==EditorWidget::TerrainMaterialMaps)selected=s.terrainMaterialView==3;
    else if(id==EditorWidget::TerrainMaterialPaint)selected=s.terrainMaterialView==0;
    auto box = deflate(rect, UiInsets::all(3));
    bool primary = id == EditorWidget::TerrainGenerate && enabled;
    auto ink = !enabled  ? theme.color.textFaint
               : primary ? theme.color.accentInk
                         : theme.color.text;
    list.addRect(box,
                 primary    ? theme.color.accent
                 : selected ? theme.color.accentWash
                            : theme.color.raised,
                 6);
    if (selected && !primary)
      list.addBorder(box, theme.color.accent, 1.5f, 6);
    auto label = box;
    const bool compactNoiseIcon=id==terrainNoiseAction(0) && icon!=UiIcon::None && box.width<=98;
    if(compactNoiseIcon) {
      list.addImage({box.x+(box.width-18)/2,box.y+3,18,18},static_cast<UiImageId>(icon),ink);
      label.y+=20;label.height-=20;
    }
    if (icon != UiIcon::None && box.width > 98) {
      list.addImage({box.x + 9, box.y + (box.height - 22) * .5f, 22, 22},
                    static_cast<UiImageId>(icon), ink);
      label.x += 33;
      label.width -= 33;
    }
    const bool noiseRole=(widgetId(id)>=terrainNoiseActionBase+1 && widgetId(id)<=terrainNoiseActionBase+4) ||
      (widgetId(id)>=terrainNoiseActionBase+23 && widgetId(id)<=terrainNoiseActionBase+25);
    text(label, value, ink, compactNoiseIcon || noiseRole, UiAlign::Center);
    if (enabled)
      router.addRegion(rect, widgetId(id));
  };
  auto outer = deflate(area, UiInsets::all(8));
  auto title = take(outer, compact(area) ? 32 : 56);
  button({title.right() - 44, title.y, 44, title.height}, "×",
         EditorWidget::TerrainClose);
  button({title.right() - 94, title.y, 48, title.height}, "Ver",
         EditorWidget::TerrainFrame, false, s.terrainTarget != 0);
  auto name = title;
  name.width -= 100;
  const auto materialContext=terrainMaterialContext(s);
  if (compact(area))
    text(name, materialContext.empty()?(s.terrainTarget ? "Editar terreno" : "Novo terreno"):materialContext,
         theme.color.text);
  else {
    text(take(name, 28), "Terreno e voxel", theme.color.text);
    const auto *e = s.document ? s.document->find(s.terrainTarget) : nullptr;
    text(name, materialContext.empty()?(e ? e->name : "Novo terreno · configure e crie"):materialContext,
         theme.color.textMuted, true);
  }
  const char *tabs[]{"Forma", "Pincel", "Pintar", "Gerir", "Traçado"};
  const u32 tabOrder[]{0, 1, 2, 4, 3};
  const UiIcon icons[]{UiIcon::TerrainTerrain, UiIcon::TerrainSculpt,
                       UiIcon::TerrainLayers, UiIcon::TerrainManage,
                       UiIcon::TerrainPath};
  auto tabrow = take(outer, compact(area) ? 36 : 48);
  for (u32 col = 0; col < 5; ++col) {
    const u32 i = tabOrder[col];
    button({tabrow.x + tabrow.width * col / 5, tabrow.y, tabrow.width / 5,
            tabrow.height},
           tabs[i], EditorWidget(widgetId(EditorWidget::TerrainTabBase) + i),
           s.terrainTab == i, !s.terrainBusy, icons[i]);
  }
  if (s.terrainTab < 2 || s.terrainTab == 4) {
    auto sections = take(outer, compact(area) ? 28 : 40);
    const char *form[]{"Estrutura", "Relevo", "Detalhes"};
    const char *brush[]{"Ferramentas", "Precisão", "Forma 3D"};
    const char *paths[]{"Traçado", "Perfil", "Materiais"};
    u32 count = 3;
    for (u32 i = 0; i < count; ++i)
      button({sections.x + sections.width * i / count, sections.y,
              sections.width / count, sections.height},
             s.terrainTab == 0   ? form[i]
             : s.terrainTab == 4 ? paths[i]
                                 : brush[i],
             EditorWidget(widgetId(EditorWidget::TerrainSectionBase) + i),
             s.terrainSection == i, !s.terrainBusy);
  }
  auto window = terrainContentRect(s, area), content = window;
  float scroll = s.terrainBusy ? 0
                               : std::clamp(s.terrainScroll, 0.f,
                                            terrainScrollLimit(s, area));
  content.y -= scroll;
  content.height = contentHeight(s);
  list.pushClip(window);
  router.pushClip(window);
  router.addRegion(window, widgetId(EditorWidget::TerrainScroll));
  const auto row = [&](float h = 56.f) { return take(content, h); };
  const auto label = [&](std::string_view v) {
    text(row(26), v, theme.color.textMuted, true);
  };
  const auto hint = [&](std::string_view a,
                        std::string_view b = std::string_view{}) {
    auto box = row(b.empty() ? 32 : 48);
    text(take(box, b.empty() ? 32 : 24), a, theme.color.textMuted, true);
    if (!b.empty())
      text(box, b, theme.color.textMuted, true);
  };
  const auto fieldAt = [&](UiRect rect, std::string_view titleText,
                           double value, u32 index, bool enabled = true) {
    const auto range = terrainControlRange(s, index);
    if (range.enabled || index == 29) {
      const bool active = enabled && range.enabled && !s.terrainBusy;
      const float chipWidth = 64;
      text({rect.x, rect.y, rect.width - chipWidth, 24}, titleText,
           theme.color.textDim, true);
      button({rect.right() - chipWidth, rect.y, chipWidth, 28}, num(value),
             EditorWidget(widgetId(EditorWidget::TerrainNumberBase) + index),
             false, active);
      const UiRect bar{rect.x + 10, rect.y + 43, rect.width - 20, 6};
      const float t = range.position(float(value));
      list.addRect(bar, theme.color.raised, 3);
      list.addRect({bar.x, bar.y, bar.width * t, bar.height},
                   active ? theme.color.accent : theme.color.textMuted, 3);
      list.addRect({bar.x + bar.width * t - 8, bar.y - 5, 16, 16},
                   active ? theme.color.text : theme.color.textMuted, 8);
      // Encode the column so a drag keeps its original scale across the
      // midpoint.
      const u32 column =
          rect.width < window.width * .75f
              ? (rect.x < window.x + window.width * .25f ? 1u : 2u)
              : 0u;
      if (active)
        router.addRegion({rect.x, rect.y + 28, rect.width, 36},
                         widgetId(EditorWidget::TerrainSliderBase) +
                             column * 0x100 + index);
      return;
    }
    text(take(rect, 20), titleText, theme.color.textDim, true);
    const float step = 40;
    auto n = rect;
    n.width -= step * 2;
    button(n, num(value),
           EditorWidget(widgetId(EditorWidget::TerrainNumberBase) + index),
           false, enabled && !s.terrainBusy);
    button({n.right(), rect.y, step, rect.height}, "-",
           EditorWidget(widgetId(EditorWidget::TerrainMinusBase) + index),
           false, enabled && !s.terrainBusy);
    button({n.right() + step, rect.y, step, rect.height}, "+",
           EditorWidget(widgetId(EditorWidget::TerrainPlusBase) + index), false,
           enabled && !s.terrainBusy);
  };
  const auto field = [&](const char *n, double v, u32 id, bool en = true) {
    fieldAt(row(64), n, v, id, en);
  };
  const auto fields = [&](const char *a, double av, u32 ai, const char *b,
                          double bv, u32 bi, bool enabled=true) {
    auto line = row(64);
    fieldAt({line.x, line.y, line.width / 2, line.height}, a, av, ai, enabled);
    fieldAt({line.x + line.width / 2, line.y, line.width / 2, line.height}, b,
            bv, bi, enabled);
  };
  const auto pair = [&](const char *a, EditorWidget ai, const char *b,
                        EditorWidget bi, bool en = true) {
    auto line = row();
    button({line.x, line.y, line.width / 2, line.height}, a, ai, false,
           en && !s.terrainBusy);
    button({line.x + line.width / 2, line.y, line.width / 2, line.height}, b,
           bi, false, en && !s.terrainBusy);
  };
  const auto slider = [&](const char *name, float value, u32 index, float,
                          float) { fieldAt(row(64), name, value, index); };
  const auto &r = s.terrainDraft;
  const auto &b = s.terrainBrush;
  bool hf = r.mode == resources::TerrainMode::Heightfield;
  if (s.terrainBusy) {
    label("Gerando malha e materiais");
    text(row(), std::to_string(int(s.terrainProgress * 100)) + "% · aguarde",
         theme.color.text);
    auto bar = deflate(row(12), UiInsets::all(3));
    list.addRect(bar, theme.color.raised, 3);
    bar.width *= std::clamp(s.terrainProgress, 0.f, 1.f);
    list.addRect(bar, theme.color.accent, 3);
    button(row(), "Cancelar geração", EditorWidget::TerrainCancel);
    hint("A cena anterior é mantida até concluir.");
  } else if (s.terrainTab == 0 && s.terrainSection == 0) {
    label("TIPO DE GEOMETRIA");
    auto modes = row(64);
    const char *names[]{"Relevo", "Voxel suave", "Blocos"};
    for (u32 i = 0; i < 3; ++i)
      button({modes.x + modes.width * i / 3, modes.y, modes.width / 3,
              modes.height},
             names[i],
             EditorWidget(widgetId(EditorWidget::TerrainModeBase) + i),
             u32(r.mode) == i, r.strokes.empty() || u32(r.mode) == i);
    if (hf) {
      label("ESTRUTURA");
      auto structure = row();
      button({structure.x, structure.y, structure.width / 2, structure.height},
             "Superfície", EditorWidget::TerrainSurface, !r.solidBase, true,
             UiIcon::TerrainSurface);
      button({structure.x + structure.width / 2, structure.y,
              structure.width / 2, structure.height},
             "Base sólida", EditorWidget::TerrainSolidBase, r.solidBase, true,
             UiIcon::TerrainSolidBase);
      field("Espessura mínima da base (m)", r.baseDepth, 14, r.solidBase);
    } else {
      hint("Volume editável · escave ou adicione massa.");
      field("Resolução vertical (8–64)", r.verticalCells, 7);
    }
    fields("Lado (m)", r.size, 1, "Células por lado", r.cells, 2);
    hint("Célula: " + num(r.size / r.cells) + " m · chunks de 16 células",
         hf ? "A base fecha as laterais e o fundo."
            : "Use Precisão para editar dentro do volume.");
  } else if (s.terrainTab == 0 && s.terrainSection == 1) {
    label("PONTO DE PARTIDA DO RELEVO");
    const char *presets[]{"Plano", "Colinas", "Montanhas", "Ilha"};
    for (u32 j = 0; j < 2; ++j) {
      auto line = row(52);
      for (u32 col = 0; col < 2; ++col) {
        u32 i = j * 2 + col;
        button({line.x + line.width * col / 2, line.y, line.width / 2,
                line.height},
               presets[i],
               EditorWidget(widgetId(EditorWidget::TerrainPresetBase) + i));
      }
    }
    fields("Amplitude (m)", r.height, 3, "Elevação base (m)", r.base, 4);
    fields("Frequência", r.frequency, 5, "Oitavas", r.octaves, 6);
    field("Seed · mesma seed reproduz o relevo", r.seed, 0);
    button(row(),
           r.island ? "Bordas em ilha: ligada" : "Bordas em ilha: desligada",
           EditorWidget::TerrainIsland, r.island);
    hint("Predefinições preservam seus traços.");
  } else if (s.terrainTab == 0) {
    label("MATERIAIS E FÍSICA");
      field("Repetição da textura (m)", r.textureScale, 8);
      if (hf) {
        field("Células do mapa de pintura", r.controlCells, 60);
        hint(r.controlCells ? "Mapa autoral independente da malha."
                            : "0: pintura legada · arraste para criar mapa.",
             r.controlCells ? "Pincel por texel · sem aumentar triângulos."
                            : "Mapa independente disponível ao arrastar.");
      }
      if (!hf)
        field("Resolução vertical (8–64)", r.verticalCells, 7);
      button(row(),
             r.collision ? "Colisão triangular: ligada"
                         : "Colisão triangular: desligada",
             EditorWidget::TerrainCollision, r.collision);
      label("DETALHE À DISTÂNCIA");
      const auto lodRow = row(64);
      fieldAt({lodRow.x, lodRow.y, lodRow.width * .5f, lodRow.height},
              "Níveis · 1–4", r.lodLevels, 28);
      fieldAt({lodRow.x + lodRow.width * .5f, lodRow.y, lodRow.width * .5f,
               lodRow.height},
              "Erro máx. (%)", r.lodRelativeError * 100, 29, r.lodLevels > 1);
      hint("Bordas preservadas · colisão no original.",
           "Distância segue o erro em pixels de Qualidade.");
      const u64 samples =
          u64(r.cells + 1) * (r.cells + 1) * (hf ? 1 : r.verticalCells + 1);
      hint("Geometria: " + std::to_string(samples) + " / 600.000",
           "Limite: 200.000 triângulos por terreno.");
      if (hf && r.controlCells && r.blendedSurface)
        hint("Mapa de pintura: " +
             std::to_string(u64(r.controlCells + 1) * (r.controlCells + 1)) +
             " amostras");
      else
        hint("Aplique para atualizar geometria e colisão.");
    }
    else if (s.terrainTab == 1 && s.terrainSection == 0) {
      slider("Raio (m)", b.radius, 9, .1f, std::max(8.f, r.size * .25f));
      slider(b.tool == resources::TerrainTool::Terrace ? "Altura do degrau (m)"
             : (b.tool == resources::TerrainTool::Raise ||
                b.tool == resources::TerrainTool::Lower)
                 ? "Altura por amostra (m)"
                 : "Força",
             b.strength, 10, .05f, 8.f);
      const char *tools[]{"Elevar",  "Baixar",    "Nivelar",   "Suavizar",
                          "Escavar", "Adicionar", "Pintar",    "Caminho",
                          "Ruído",   "Terraços",  "Restaurar", "Aguçar",
                          "Buraco"};
      const u32 order[]{0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12};
      const UiIcon marks[]{UiIcon::TerrainRaise,   UiIcon::TerrainLower,
                           UiIcon::TerrainFlatten, UiIcon::TerrainSmooth,
                           UiIcon::TerrainDig,     UiIcon::TerrainVoxel,
                           UiIcon::TerrainPaint,   UiIcon::TerrainPath,
                           UiIcon::TerrainNoise,   UiIcon::TerrainTerrace,
                           UiIcon::TerrainRestore, UiIcon::TerrainSharpen,
                           UiIcon::TerrainHole};
      for (u32 j = 0; j < 4; ++j) {
        auto line = row(52);
        for (u32 col = 0; col < 3; ++col) {
          const u32 i = order[j * 3 + col];
          button({line.x + line.width * col / 3, line.y, line.width / 3,
                  line.height},
                 tools[i],
                 EditorWidget(widgetId(EditorWidget::TerrainToolBase) + i),
                 u32(b.tool) == i, hf ? (i != 4 && i != 5) : i != 12, marks[i]);
        }
      }
      label("FORMA E INFLUÊNCIA");
      const bool volumeBrush =
          !hf && (b.tool == resources::TerrainTool::Dig ||
                  b.tool == resources::TerrainTool::Add ||
                  b.tool == resources::TerrainTool::Smooth ||
                  b.tool == resources::TerrainTool::Sharpen ||
                  b.tool == resources::TerrainTool::Restore ||
                  b.tool == resources::TerrainTool::Paint);
      auto shapes = row(48);
      const char *shape[]{"Elipse", "Caixa", "Meia", "Ponta", "Arred."};
      for (u32 i = 0; i < 5; ++i)
        button({shapes.x + shapes.width * i / 5, shapes.y, shapes.width / 5,
                shapes.height},
               shape[i],
               EditorWidget(widgetId(EditorWidget::TerrainShapeBase) + i),
               u32(b.shape) == i, volumeBrush || i < 2 || i == 4);
      slider("Núcleo rígido · borda suave", b.hardness, 15, 0, 1);
      if (b.tool == resources::TerrainTool::Flatten ||
          b.tool == resources::TerrainTool::Path)
        field("Altura desejada Y (m)", b.y, 12);
      fields("Rotação (°)", b.rotation, 16, "Proporção X/Z", b.aspect, 17);
      field("Opacidade (0–1)", b.opacity, 25);
      if (b.tool == resources::TerrainTool::Dig ||
          b.tool == resources::TerrainTool::Add ||
          b.tool == resources::TerrainTool::Paint)
        button(row(),
               b.application == resources::TerrainBrushApplication::Target
                   ? "Aplicação: valor alvo"
                   : "Aplicação: acumulativa",
               EditorWidget::TerrainOpacityTarget,
               b.application == resources::TerrainBrushApplication::Target);
      if (b.tool == resources::TerrainTool::Restore) {
        button(row(),
               b.restoreGeometry ? "Restaurar geometria: sim"
                                 : "Restaurar geometria: não",
               EditorWidget::TerrainRestoreGeometry, b.restoreGeometry);
        button(row(),
               b.restoreWeights ? "Restaurar materiais: sim"
                                : "Restaurar materiais: não",
               EditorWidget::TerrainRestoreWeights, b.restoreWeights);
      }
      hint("Selecione a ferramenta e arraste no terreno.",
           "Cada gesto confirmado tem um único Desfazer.");
    }
    else if (s.terrainTab == 1 && s.terrainSection == 1) {
      label("POSIÇÃO LOCAL DO PINCEL");
      fields("X (m)", b.x, 11, "Z (m)", b.z, 13);
      field("Altura Y / nível desejado (m)", b.y, 12);
      fields("Raio (m)", b.radius, 9, "Força", b.strength, 10);
      button(row(), "Aplicar na posição XYZ", EditorWidget::TerrainStamp, false,
             s.terrainTarget != 0);
      button(row(), "Remover última amostra", EditorWidget::TerrainRemoveStroke,
             false, !r.strokes.empty());
      fields("Rotação (°)", b.rotation, 16, "Proporção X/Z", b.aspect, 17);
      field("Espaçamento · fração do raio", s.terrainSpacing, 18);
      button(row(),
             s.terrainTouchOffset ? "Alvo acima do dedo: ligado"
                                  : "Alvo acima do dedo: desligado",
             EditorWidget::TerrainTouchOffset, s.terrainTouchOffset);
      field("Profundidade a partir da superfície (m)", b.surfaceDepth, 27);
      button(row(),
             b.lockHeight ? "Manter altura do gesto: sim"
                          : "Acompanhar altura da superfície",
             EditorWidget::TerrainHeightLock, b.lockHeight);
      field("Vizinhança do filtro (1–4 células)", b.filterRadius, 26);
      hint("XYZ permite alcançar o interior do voxel.",
           "Profundidade positiva entra na superfície.");
    }
    else if (s.terrainTab == 1) {
      label("DIMENSÕES LOCAIS DO PINCEL");
      field("Raio de referência (m)", b.radius, 9);
      button(row(),
             b.dimensionsLinked ? "Vincular dimensões: sim"
                                : "Dimensões independentes",
             EditorWidget::TerrainDimensionsLinked, b.dimensionsLinked, true,
             UiIcon::TerrainBrushAxes);
      field("Proporção X", b.aspect, 17);
      fields("Proporção Y", b.verticalAspect, 20, "Proporção Z", b.depthAspect,
             21);
      hint("Tamanho: " + num(2 * b.radius * b.aspect) + " × " +
           num(2 * b.radius * b.verticalAspect) + " × " +
           num(2 * b.radius * b.depthAspect) + " m");
      label("ORIENTAÇÃO");
      field("Rotação Y (°)", b.rotation, 16);
      fields("Inclinação X (°)", b.pitch, 22, "Inclinação Z (°)", b.roll, 23);
      if (b.shape == resources::TerrainBrushShape::RoundedBox) {
        auto badge = row(32);
        list.addImage({badge.x + 8, badge.y + 4, 24, 24},
                      static_cast<UiImageId>(UiIcon::TerrainRoundedBrush),
                      theme.color.text);
        text({badge.x + 36, badge.y, badge.width - 36, badge.height},
             "Cubo arredondado", theme.color.text);
        field("Arredondamento dos cantos (0–1)", b.roundness, 24);
      }
      if (b.shape == resources::TerrainBrushShape::Hemisphere ||
          b.shape == resources::TerrainBrushShape::Cone)
        button(row(), b.inverted ? "Direção: para baixo" : "Direção: para cima",
               EditorWidget::TerrainBrushInvert, b.inverted);
      hint("O contorno usa a mesma forma da edição.",
           hf ? "Inclinações X/Z são usadas no volume."
              : "Rotação aplicada nos eixos locais do terreno.");
    }
    else if (s.terrainTab == 2) {
      const char *layers[]{"Grama", "Rocha", "Terra", "Caminho"};
      const UiIcon samples[]{UiIcon::TerrainSampleGrass,UiIcon::TerrainSampleRock,
                            UiIcon::TerrainSampleSoil,UiIcon::TerrainSamplePath};
      const auto current=std::min(b.layer,3u);
      label("CAMADA "+std::to_string(current+1)+" · "+materialDisplayName(
        current<r.surface.layers.size()?r.surface.layers[current].name:layers[current]));
      auto slots=row(48);
      for(u32 i=0;i<4;++i) {
        UiRect tile{slots.x+slots.width*i/4,slots.y,slots.width/4,slots.height};
        button(tile,"",EditorWidget(widgetId(EditorWidget::TerrainLayerBase)+i),b.layer==i);
        const auto icon=i<r.surface.layers.size() && !r.surface.layers[i].sourceCatalogId.empty()
          ? terrainMaterialIcon(r.surface.layers[i].sourceCatalogId):samples[i];
        list.addImage({tile.x+(tile.width-28)/2,tile.y+2,28,28},static_cast<UiImageId>(icon));
        text({tile.x,tile.y+31,tile.width,14},std::to_string(i+1),theme.color.text,true,UiAlign::Center);
      }
      auto navigation=row(48);
      const char *sections[]{"Acervo","Camada","Mapas","Pincel","Ruído"};
      const EditorWidget actions[]{EditorWidget::TerrainMaterialLibrary,EditorWidget::TerrainMaterialProperties,
                                  EditorWidget::TerrainMaterialMaps,EditorWidget::TerrainMaterialPaint,terrainNoiseAction(0)};
      for(u32 i=0;i<5;++i)button({navigation.x+navigation.width*i/5,navigation.y,navigation.width/5,48},
        sections[i],actions[i],i==4 && s.terrainMaterialView>=4,true,i==4?UiIcon::TerrainNoise:UiIcon::None);
    if((s.terrainMaterialView==60 || terrainClusterVariantView(s.terrainMaterialView)) && b.layer<r.surface.layers.size()) {
      const auto &layer=r.surface.layers[b.layer];
      const auto &cluster=layer.cluster;
      const auto index=terrainClusterVariant(s.terrainMaterialView);
      label("VARIANTES E ESTOCÁSTICO · "+layer.name);
      if(s.terrainMaterialView==60) {
        button(row(44),cluster.enabled?"Clusters ligados":"Clusters desligados",terrainClusterAction(1),cluster.enabled,cluster.variants.size()>=2);
        button(row(44),cluster.stochastic?"Estocástico ligado":"Estocástico desligado",terrainClusterAction(2),cluster.stochastic);
        const char *quality[]{"Baixa · 1 variante","Média · 2 vizinhas","Alta · 3 triangulares"};
        button(row(44),quality[u32(cluster.quality)],terrainClusterAction(3));
        field("Contraste da seleção",cluster.contrast,164);
        field("Reforço do ruído",cluster.noiseBoost,165);
        button(row(44),"Editar máscara de seleção",terrainClusterAction(4));
        label(std::to_string(cluster.variants.size())+" / 3 VARIANTES · ALBEDOS DISTINTOS");
        for(u32 i=0;i<cluster.variants.size();++i) {
          std::string name="Variante "+std::to_string(i+1);
          auto icon=UiIcon::AssetsMaterial;
          if(s.terrainCatalog) for(const auto &material:s.terrainCatalog->materials)
            if(material.layer.identity==cluster.variants[i].identity) {
              name=materialDisplayName(material.layer.name);
              icon=terrainMaterialIcon(material.id);break;
            }
          button(row(44),name,terrainClusterAction(40+i),false,true,icon);
        }
        button(row(44),"Adicionar material original",terrainClusterAction(5),false,cluster.variants.size()<3);
        hint("Clusters exigem pelo menos dois albedos distintos.","Estocástico também funciona no material base.");
        hint("Baixa reduz diversidade; seleção pode marcar bordas.","Média e Alta misturam mais amostras.");
      } else if(s.terrainMaterialView>=80 && s.terrainMaterialView<=92) {
        const bool material=s.terrainMaterialView<=82;
        button(row(44),material?"Voltar às variantes":"Voltar aos mapas",material?terrainClusterAction(0):terrainClusterAction(7));
        label(material?"MATERIAL ORIGINAL · SETE MAPAS":"TEXTURA ORIGINAL · MAPA SELECIONADO");
        const auto catalog=s.terrainCatalog;
        const auto count=catalog?(material?catalog->materials.size():catalog->textures.size()):0;
        if(count) {
          auto pager=row(44);
          button({pager.x,pager.y,48,44},"<",terrainClusterAction(16),false,s.terrainCatalogPage>0);
          button({pager.right()-48,pager.y,48,44},">",terrainClusterAction(17),false,(s.terrainCatalogPage+1)*6<count);
          text({pager.x+48,pager.y,pager.width-96,44},std::to_string(s.terrainCatalogPage+1)+" / "+std::to_string((count+5)/6),theme.color.text,true,UiAlign::Center);
          for(u32 i=0;i<6 && s.terrainCatalogPage*6+i<count;++i) {
            const auto source=s.terrainCatalogPage*6+i;
            if(material) button(row(48),materialDisplayName(catalog->materials[source].layer.name),terrainClusterAction(20+i),false,true,terrainMaterialIcon(catalog->materials[source].id));
            else button(row(48),terrainNoiseSourceName(catalog->textures[source]),terrainClusterAction(32+i));
          }
        } else hint("Carregue o catálogo para selecionar uma fonte real.");
        hint(material?"Identidades e sete mapas do material preservados.":"Canal, remaps e fatores permanecem independentes.");
      } else if(index<cluster.variants.size()) {
        const auto &variant=cluster.variants[index];
        button(row(44),"Voltar às variantes",terrainClusterAction(0));
        std::string variantName="VARIANTE "+std::to_string(index+1);
        if(s.terrainCatalog) for(const auto &material:s.terrainCatalog->materials)
          if(material.layer.identity==variant.identity) {variantName=materialDisplayName(material.layer.name);break;}
        label(variantName);
        if(s.terrainMaterialView<80) {
          field("Tamanho do tile (m)",variant.scale,160);
          fields("Deslocamento X (m)",variant.offsetX,161,"Deslocamento Y (m)",variant.offsetY,162);
          field("Rotação (°)",variant.rotation,163);
          button(row(44),"Editar os sete mapas",terrainClusterAction(7));
          button(row(44),"Substituir por material original",terrainClusterAction(8));
          button(row(44),"Remover esta variante",terrainClusterAction(6));
          hint("Tamanho absoluto em metros por tile.","Offsets e rotação da camada são somados.");
        } else {
          const auto role=std::min(s.terrainMapRole,6u);
          const auto &map=variant.maps[role];
          const char *roles[]{"Cor base","Normal","Altura","Oclusão ambiente","Rugosidade","Metálico","Emissão"};
          auto selector=row(44);
          button({selector.x,selector.y,48,44},"<",terrainClusterAction(11));
          button({selector.right()-48,selector.y,48,44},">",terrainClusterAction(12));
          text({selector.x+48,selector.y,selector.width-96,44},roles[role],theme.color.text,false,UiAlign::Center);
          std::string sourceName="Selecionar textura original";
          if(s.terrainCatalog) for(const auto &source:s.terrainCatalog->textures)
            if(source.identity==map.texture) {sourceName=terrainNoiseSourceName(source);break;}
          button(row(44),sourceName,terrainClusterAction(9));
          const char *channels[]{"R","G","B","A","RGB","Normal RGB","Normal RG"};
          button(row(44),std::string("Canal: ")+channels[u32(map.channel)],terrainClusterAction(13),false,map.texture.valid());
          auto options=row();
          button({options.x,options.y,options.width/2,options.height},role==1?"Normal · linear":map.linear?"Espaço: linear":"Espaço: sRGB",terrainClusterAction(15),false,role!=1 && map.texture.valid());
          button({options.x+options.width/2,options.y,options.width/2,options.height},map.invert?"Inversão ligada":"Inversão desligada",terrainClusterAction(14));
          field(role==1?"Força do normal":"Fator",map.factor,176);
          if(role>=2 && role<=5) fields("Remap mínimo",map.remapMin,177,"Remap máximo",map.remapMax,178);
          else {
            label(role==1?"NORMAL · FATORES XYZ":"FATORES RGBA");
            fields("Vermelho",map.colorFactor[0],179,"Verde",map.colorFactor[1],180);
            if(role==1) field("Azul",map.colorFactor[2],181,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
            else fields("Azul",map.colorFactor[2],181,"Alfa",map.colorFactor[3],182);
            label("REMAP MÍNIMO");
            fields("Vermelho",map.colorRemapMin[0],183,"Verde",map.colorRemapMin[1],184);
            if(role==1) field("Azul",map.colorRemapMin[2],185,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
            else fields("Azul",map.colorRemapMin[2],185,"Alfa",map.colorRemapMin[3],186);
            label("REMAP MÁXIMO");
            fields("Vermelho",map.colorRemapMax[0],187,"Verde",map.colorRemapMax[1],188);
            if(role==1) field("Azul",map.colorRemapMax[2],189,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
            else fields("Azul",map.colorRemapMax[2],189,"Alfa",map.colorRemapMax[3],190);
          }
          button(row(44),"Voltar à transformação",terrainClusterAction(10));
        }
      } else hint("Selecione uma variante existente nesta camada.");
    } else if(s.terrainMaterialView==50 && b.layer<r.surface.layers.size()) {
      const auto &layer=r.surface.layers[b.layer];
      label("REAMOSTRAGEM · "+layer.name);
      button(row(44),"Voltar aos ruídos",terrainNoiseAction(1));
      button(row(44),layer.resampleAlbedo?"Cor distante: ligada":"Cor distante: desligada",terrainNoiseAction(36),layer.resampleAlbedo);
      button(row(44),layer.resampleNormal?"Normal distante: ligado":"Normal distante: desligado",terrainNoiseAction(37),layer.resampleNormal);
      button(row(44),layer.resampleHeightBlend?"Mistura de altura: ligada":"Mistura de altura: desligada",terrainNoiseAction(38),layer.resampleHeightBlend);
      auto modes=row(44);
      const char *names[]{"Constant","Noise","Crossfade"};
      for(u32 i=0;i<3;++i)button({modes.x+modes.width*i/3,modes.y,modes.width/3,44},names[i],terrainNoiseAction(39+i),u32(layer.resampleBlend)==i);
      field("Escala distante (×)",layer.resampleScale,152);
      const bool distance=layer.resampleBlend!=resources::TerrainDistanceBlend::Constant;
      fields("Início (m)",layer.resampleStart,153,"Fim (m)",layer.resampleEnd,154,distance);
      button(row(44),"Editar fonte e parâmetros do Noise",terrainNoiseAction(42),false,layer.resampleBlend==resources::TerrainDistanceBlend::Noise);
      hint(distance?"Faixa de distância medida pela câmera.":"Constant usa sempre a amostragem distante.","Altura combina amostras próximas e distantes.");
    } else if(s.terrainMaterialView==30 && b.layer<r.surface.layers.size()) {
      const auto &layer=r.surface.layers[b.layer];
      const auto &array=r.surface.antiTileArray;
      label("ANTI-TILE · CONJUNTO COMPARTILHADO");
      button(row(44),"Voltar aos ruídos",terrainNoiseAction(1));
      button(row(44),array.enabled?"Conjunto ligado":"Conjunto desligado",terrainNoiseAction(27),array.enabled,!array.entries.empty());
      button(row(44),"Criar entrada com normal do material",terrainNoiseAction(28),false,array.entries.size()<32);
      hint(std::to_string(array.entries.size())+" entradas · até 32", "A criação vincula uma fonte normal real do catálogo.");
      const auto *entry=terrainBoundAntiTile(r.surface,b.layer);
      label("VÍNCULO · "+layer.name);
      button(row(44),entry?"Entrada: "+entry->identity.text().substr(0,12):"Selecionar entrada existente",terrainNoiseAction(29),entry!=nullptr,!array.entries.empty());
      button(row(44),"Desvincular desta camada",terrainNoiseAction(30),false,entry!=nullptr);
      auto roles=row(44);
      const char *names[]{"Normal","Detalhe","Distância"};
      for(u32 i=0;i<3;++i)button({roles.x+roles.width*i/3,roles.y,roles.width/3,44},names[i],terrainNoiseAction(31+i),false,entry!=nullptr);
      field("Participação · normal",layer.antiTileParticipation[0],149,entry!=nullptr);
      fields("Participação · detalhe",layer.antiTileParticipation[1],150,"Distância",layer.antiTileParticipation[2],151,entry!=nullptr);
      hint("A identidade permanece estável ao reordenar.","Editar uma entrada afeta suas camadas vinculadas.");
    } else if(terrainNoiseAvailable(r.surface,b.layer,s.terrainMaterialView)) {
      const auto &layer=r.surface.layers[b.layer];
      const auto selection=terrainNoiseSelection(s.terrainMaterialView);
      const auto &noise=terrainSelectedNoise(r.surface,b.layer,s.terrainMaterialView);
      const bool normal=terrainNoiseNormal(s.terrainMaterialView);
      const bool local=terrainNoiseLocal(s.terrainMaterialView);
      auto scope=row(44);
      button({scope.x,scope.y,scope.width/2,44},"Global",terrainNoiseAction(44),!local);
      button({scope.x+scope.width/2,scope.y,scope.width/2,44},"Local",terrainNoiseAction(45),local);
      auto roles=row(44);
      const char *roleNames[]{"Detalhe","Distância","N1","N2","N3"};
      const u32 roleActions[]{local?3u:1u,local?4u:2u,23u,24u,25u};
      const u32 roleSelected=selection<4?selection%2:selection<10?2+(selection-4)%3:5;
      for(u32 i=0;i<5;++i) button({roles.x+roles.width*i/5,roles.y,roles.width/5,44},roleNames[i],terrainNoiseAction(roleActions[i]),roleSelected==i);
      auto links=row(44);
      button({links.x,links.y,links.width/3,44},"Anti-tile",terrainNoiseAction(26));
      button({links.x+links.width/3,links.y,links.width/3,44},"Distante",terrainNoiseAction(35));
      button({links.x+links.width*2/3,links.y,links.width/3,44},"Variantes",terrainClusterAction(0));
      if(selection==14) button(row(44),"Voltar às variantes",terrainClusterAction(0));
      label(selection==14?"VARIANTES · MÁSCARA DE SELEÇÃO":selection==13?"REAMOSTRAGEM · FONTE DO NOISE":selection>=10?"ANTI-TILE · ENTRADA COMPARTILHADA":terrainNoiseLocal(s.terrainMaterialView)?"LOCAL · "+layer.name:"GLOBAL · TODAS AS CAMADAS");
      if(terrainNoisePicker(s.terrainMaterialView)) {
        button(row(44),"Voltar aos parâmetros",terrainNoiseAction(11));
        label("FONTE · TEXTURAS ORIGINAIS DO CATÁLOGO");
        const auto catalog=s.terrainCatalog;
        if(catalog && !catalog->textures.empty()) {
          auto pager=row(44);
          button({pager.x,pager.y,48,44},"<",terrainNoiseAction(12),false,s.terrainCatalogPage>0);
          button({pager.right()-48,pager.y,48,44},">",terrainNoiseAction(13),false,(s.terrainCatalogPage+1)*6<catalog->textures.size());
          text({pager.x+48,pager.y,pager.width-96,44},std::to_string(s.terrainCatalogPage+1)+" / "+std::to_string((catalog->textures.size()+5)/6),theme.color.text,true,UiAlign::Center);
          for(u32 i=0;i<6;++i) {
            const auto index=s.terrainCatalogPage*6+i;
            if(index>=catalog->textures.size()) break;
            const auto &source=catalog->textures[index];
            button(row(48),terrainNoiseSourceName(source),terrainNoiseAction(16+i),noise.map.texture==source.identity);
          }
          hint("Selecione a fonte; canal e ajustes são preservados.");
        } else hint("Nenhuma textura disponível no catálogo.");
      } else {
        const auto *bound=selection>=10 && selection<=12?terrainBoundAntiTile(r.surface,b.layer):nullptr;
        const bool lastSource=bound && noise.enabled &&
          unsigned(bound->normal.enabled)+unsigned(bound->detail.enabled)+unsigned(bound->distance.enabled)==1;
        const bool requiredNoise=selection==13 && layer.resampleBlend==resources::TerrainDistanceBlend::Noise;
        button(row(44),noise.enabled?"Ruído ligado":"Ruído desligado",terrainNoiseAction(5),noise.enabled,noise.map.texture.valid() && !lastSource && !requiredNoise);
        if(requiredNoise) hint("Modo Noise exige esta fonte ativa.");
        if(lastSource) hint("Mantenha uma fonte ativa por entrada anti-tile.");
        std::string sourceName="Selecionar textura original";
        if(s.terrainCatalog) for(const auto &source:s.terrainCatalog->textures)
          if(source.identity==noise.map.texture) { sourceName=terrainNoiseSourceName(source); break; }
        button(row(44),sourceName,terrainNoiseAction(10));
        const char *projection[]{"UV","Triplanar local","Triplanar mundial"};
        button(row(44),std::string("Projeção: ")+projection[u32(noise.projection)],terrainNoiseAction(6));
        field("Frequência (rep/m)",noise.scale,128);
        field("Força",noise.strength,129);
        fields("Fade início (m)",noise.fadeStart,130,"Fade fim (m)",noise.fadeEnd,131);
        if(selection<4) field(selection%2==0?"Participação global · detalhe":"Participação global · distância",
          selection%2==0?layer.globalDetailParticipation:layer.globalDistanceParticipation,132);
        else if(selection<10) field("Participação global · normal ",layer.globalNormalParticipation[(selection-4)%3],148);
        else if(selection<=12) field("Participação desta camada",layer.antiTileParticipation[selection==10?0:selection==11?1:2],selection==10?149:selection==11?150:151);
        hint((selection==1 || selection==3 || selection==12)?"Distância aparece ao afastar a câmera.":"Detalhe e normal desaparecem ao afastar a câmera.",
          "Participação pertence à camada selecionada.");
        const char *channels[]{"R","G","B","A","RGB","Normal RGB","Normal RG"};
        if(u32(noise.map.channel)>4 && !normal)
          hint("Canal normal importado · selecione R, G, B, A ou RGB.");
        button(row(44),std::string("Canal: ")+channels[u32(noise.map.channel)],terrainNoiseAction(7));
        if(normal) {
          auto options=row();
          button({options.x,options.y,options.width/2,options.height},"Normal · linear",terrainNoiseAction(8),false,false);
          button({options.x+options.width/2,options.y,options.width/2,options.height},noise.map.invert?"Inversão ligada":"Inversão desligada",terrainNoiseAction(9));
        } else pair(noise.map.linear?"Espaço: linear":"Espaço: sRGB",terrainNoiseAction(8),noise.map.invert?"Inversão ligada":"Inversão desligada",terrainNoiseAction(9));
        field("Fator do mapa",noise.map.factor,133);
        if(noise.map.channel==resources::TerrainSurfaceChannel::RGB || normal) {
          label("RGB · FATORES");
          fields("Vermelho",noise.map.colorFactor[0],136,"Verde",noise.map.colorFactor[1],137);
          field("Azul",noise.map.colorFactor[2],138,noise.map.channel!=resources::TerrainSurfaceChannel::NormalRG);
          label("RGB · REMAP MÍNIMO");
          fields("Vermelho",noise.map.colorRemapMin[0],140,"Verde",noise.map.colorRemapMin[1],141);
          field("Azul",noise.map.colorRemapMin[2],142,noise.map.channel!=resources::TerrainSurfaceChannel::NormalRG);
          label("RGB · REMAP MÁXIMO");
          fields("Vermelho",noise.map.colorRemapMax[0],144,"Verde",noise.map.colorRemapMax[1],145);
          field("Azul",noise.map.colorRemapMax[2],146,noise.map.channel!=resources::TerrainSurfaceChannel::NormalRG);
        } else fields("Remap mínimo",noise.map.remapMin,134,"Remap máximo",noise.map.remapMax,135);
        hint(normal?"Força zero preserva o normal base sem amostragem.":"0,5 é neutro · força zero evita amostragem.","Toque no valor para digitar com precisão.");
      }
    } else if (s.terrainMaterialView == 1) {
      const auto catalog = s.terrainCatalog;
      label("BIBLIOTECA · MATERIAIS ORIGINAIS");
      if (catalog) {
        auto pager = row(48);
        button({pager.x, pager.y, 48, 48}, "<",
               EditorWidget::TerrainCatalogPrevious, false,
               s.terrainCatalogPage > 0);
        button({pager.right() - 48, pager.y, 48, 48}, ">",
               EditorWidget::TerrainCatalogNext, false,
               (s.terrainCatalogPage + 1) * 6 < catalog->materials.size());
        text({pager.x + 48, pager.y, pager.width - 96, 48},
             std::to_string(s.terrainCatalogPage + 1) + " / " +
                 std::to_string((catalog->materials.size() + 5) / 6),
             theme.color.text, true, UiAlign::Center);
        const auto count=std::min<usize>(6,catalog->materials.size()-
          std::min<usize>(catalog->materials.size(),usize(s.terrainCatalogPage)*6));
        for(u32 i=0;i<count;i+=2) {
          const auto grid=row(112);
          for(u32 column=0;column<2 && i+column<count;++column) {
            const auto index=i+column;
            const auto &material=catalog->materials[s.terrainCatalogPage*6+index];
            const bool selected=b.layer<r.surface.layers.size() &&
              r.surface.layers[b.layer].sourceCatalogId==material.id;
            UiRect tile{grid.x+grid.width*column/2,grid.y,grid.width/2,grid.height};
            button(tile,"",EditorWidget(widgetId(EditorWidget::TerrainCatalogItemBase)+index),selected);
            list.addImage({tile.x+(tile.width-66)/2,tile.y+7,66,66},
                          static_cast<UiImageId>(terrainMaterialIcon(material.id)));
            text({tile.x+3,tile.y+77,tile.width-6,28},materialDisplayName(material.layer.name),
                 theme.color.text,true,UiAlign::Center);
          }
        }
        hint(std::to_string(catalog->materials.size()) +
                 " materiais · quatro canais de pintura",
             "A seleção troca somente a camada escolhida.");
      } else
        hint("Abra a biblioteca para carregar o catálogo.");
    } else if (s.terrainMaterialView == 2 &&
               b.layer < r.surface.layers.size()) {
      const auto &layer = r.surface.layers[b.layer];
      label(layer.name);
      button(row(), layer.active ? "Camada ativa" : "Camada desativada",
             EditorWidget::TerrainLayerActive, layer.active);
      fields("Escala X (m)", layer.scaleX, 64, "Escala Y (m)", layer.scaleY,
             65,!layer.cluster.enabled);
      if(layer.cluster.enabled) hint("Tamanho definido em Variantes");
      field("Deslocamento X (m)", layer.offsetX, 66);
      field("Deslocamento Y (m)", layer.offsetY, 67);
      field("Rotação (°)", layer.rotation, 68);
      fields("Vermelho", layer.tint[0], 69, "Verde", layer.tint[1],
             70);
      fields("Azul", layer.tint[2], 71, "Saturação", layer.saturation,
             72);
      const char *projection[] = {"UV", "Triplanar local", "Triplanar mundial"};
      button(row(), projection[u32(layer.projection)],
             EditorWidget::TerrainLayerProjection);
      field("Contraste dos eixos", layer.axisContrast, 73, layer.projection != resources::TerrainProjection::UV);
      button(row(), layer.faceNormals ? "Normais das faces" : "Normais suaves",
             EditorWidget::TerrainLayerFaceNormals, layer.faceNormals);
      button(row(),
             layer.heightBlend ? "Mistura por altura: ligada"
                               : "Mistura por altura: desligada",
             EditorWidget::TerrainLayerHeightBlend, layer.heightBlend);
      fields("Contraste da altura", layer.heightContrast, 74,
             "Largura da mistura", layer.heightWidth, 75, layer.heightBlend);
      field("Influência da altura", layer.heightInfluence, 76, layer.heightBlend);
      const char *quality[] = {"Baixa", "Média", "Alta"};
      button(row(),
             std::string("Qualidade da altura: ") +
                 quality[u32(layer.heightQuality)],
             EditorWidget::TerrainLayerHeightQuality, false,
             layer.heightBlend && layer.maps[2].texture.valid());
      button(row(44),"Reamostragem · perto e longe",terrainNoiseAction(35));
      button(row(44),"Variantes e estocástico",terrainClusterAction(0),false,true,UiIcon::TerrainLayers);
      hint("Texturas originais · sem redução automática",
           layer.sourceCatalogId.empty()
               ? "Camada legada preservada."
               : "Propriedades e origem do catálogo preservadas.");
    } else if (s.terrainMaterialView == 3 &&
               b.layer < r.surface.layers.size()) {
      const auto &layer = r.surface.layers[b.layer];
      const auto &map = layer.maps[std::min(s.terrainMapRole, 6u)];
      const char *roles[] = {"Cor base",         "Normal",     "Altura",
                             "Oclusão ambiente", "Rugosidade", "Metálico",
                             "Emissão"};
      auto selector = row(48);
      button({selector.x, selector.y, 48, 48}, "<",
             EditorWidget::TerrainMapPrevious);
      button({selector.right() - 48, selector.y, 48, 48}, ">",
             EditorWidget::TerrainMapNext);
      text({selector.x + 48, selector.y, selector.width - 96, 48},
           roles[std::min(s.terrainMapRole, 6u)], theme.color.text, false,
           UiAlign::Center);
      hint(map.texture.valid() ? "Textura original vinculada"
                               : "Valor constante · sem textura",
           layer.name);
      const char *channels[] = {"R",   "G",          "B",        "A",
                                "RGB", "Normal RGB", "Normal RG"};
      button(row(), std::string("Canal: ") + channels[u32(map.channel)],
             EditorWidget::TerrainMapChannel, false, map.texture.valid());
      auto mapOptions=row();
      button({mapOptions.x,mapOptions.y,mapOptions.width/2,mapOptions.height},
             s.terrainMapRole==1 ? "Normal · linear" : map.linear ? "Espaço: linear" : "Espaço: sRGB",
             EditorWidget::TerrainMapColorSpace,false,s.terrainMapRole!=1 && map.texture.valid());
      button({mapOptions.x+mapOptions.width/2,mapOptions.y,mapOptions.width/2,mapOptions.height},
             map.invert ? "Inversão: ligada" : "Inversão: desligada",
             EditorWidget::TerrainMapInvert,false,s.terrainMapRole!=1 || map.texture.valid());
      field(s.terrainMapRole == 1 ? "Força do normal" : "Fator", map.factor, 96,
            s.terrainMapRole != 1 || map.texture.valid());
      if (s.terrainMapRole >= 2 && s.terrainMapRole <= 5)
        fields("Remap mínimo", map.remapMin, 97, "Remap máximo", map.remapMax,
               98);
      if (s.terrainMapRole == 0 || s.terrainMapRole == 1 ||
          s.terrainMapRole == 6) {
        label(s.terrainMapRole==1 ? "FATOR DO NORMAL XYZ" : "FATOR DE COR RGBA");
        fields("Vermelho", map.colorFactor[0], 99, "Verde", map.colorFactor[1],
               100, s.terrainMapRole!=1 || map.texture.valid());
        if (s.terrainMapRole == 1)
          field("Azul", map.colorFactor[2], 101, map.texture.valid() && map.channel!=resources::TerrainSurfaceChannel::NormalRG);
        else
          fields("Azul", map.colorFactor[2], 101, "Alfa", map.colorFactor[3],
                 102);
        label(s.terrainMapRole==1 ? "NORMAL · REMAP MÍNIMO XYZ" : "REMAP DE COR · MÍNIMO RGBA");
        fields("Vermelho", map.colorRemapMin[0], 103, "Verde",
               map.colorRemapMin[1], 104, s.terrainMapRole!=1 || map.texture.valid());
        if (s.terrainMapRole == 1)
          field("Azul", map.colorRemapMin[2], 105, map.texture.valid() && map.channel!=resources::TerrainSurfaceChannel::NormalRG);
        else
          fields("Azul", map.colorRemapMin[2], 105, "Alfa",
                 map.colorRemapMin[3], 106);
        label(s.terrainMapRole==1 ? "NORMAL · REMAP MÁXIMO XYZ" : "REMAP DE COR · MÁXIMO RGBA");
        fields("Vermelho", map.colorRemapMax[0], 107, "Verde",
               map.colorRemapMax[1], 108, s.terrainMapRole!=1 || map.texture.valid());
        if (s.terrainMapRole == 1)
          field("Azul", map.colorRemapMax[2], 109, map.texture.valid() && map.channel!=resources::TerrainSurfaceChannel::NormalRG);
        else
          fields("Azul", map.colorRemapMax[2], 109, "Alfa",
                 map.colorRemapMax[3], 110);
      }
      hint("Canais e metadados preservados ao salvar.");
    } else {
      field("Repetição da textura (m)", r.textureScale, 8);
      fields("Raio (m)", b.radius, 9, "Força", b.strength, 10);
      field("Opacidade da camada (0–1)", b.opacity, 25);
      button(row(),
             b.application == resources::TerrainBrushApplication::Target
                 ? "Pintura: opacidade alvo"
                 : "Pintura: acumulativa",
             EditorWidget::TerrainOpacityTarget,
             b.application == resources::TerrainBrushApplication::Target);
      button(row(),
             r.blendedSurface ? "Mistura e triplanar: ligados"
                              : "Materiais por região · legado",
             EditorWidget::TerrainBlend, r.blendedSurface);
      slider(r.surface.layers.empty() ? "Variação de escala e cor"
                                      : "Variação de cor",
             r.macroVariation, 19, 0, 1);
      hint("Pesos suaves · projeção em três eixos.",
           "Aplicar atualiza a configuração do material.");
    }
  } else if (s.terrainTab == 4) {
    if (r.paths.empty()) {
      button(row(), "Novo caminho", EditorWidget::TerrainPathNew);
      hint("Crie um caminho para editar pontos e perfis.",
           "A prévia aparece antes de aplicar.");
    } else {
      const auto &path =
          r.paths[std::min(s.terrainPath, u32(r.paths.size() - 1))];
      auto selector = row(44);
      button({selector.x, selector.y, 36, 44}, "<",
             EditorWidget::TerrainPathPrevious, false, r.paths.size() > 1);
      button({selector.right() - 80, selector.y, 36, 44}, ">",
             EditorWidget::TerrainPathNext, false, r.paths.size() > 1);
      button({selector.right() - 44, selector.y, 44, 44}, "+",
             EditorWidget::TerrainPathNew, false, r.paths.size() < 64);
      text({selector.x + 36, selector.y, selector.width - 116, 44},
           path.name + " · " + std::to_string(s.terrainPathPoint + 1) + "/" +
               std::to_string(path.points.size()),
           theme.color.text, true);
      if (s.terrainSection == 0) {
        if (s.terrainPathPoint < path.points.size()) {
          const auto &point = path.points[s.terrainPathPoint];
          fields("X (m)", point.x, 44, "Z (m)", point.z, 46);
          button(row(),
                 path.followTerrain ? "Altura: seguir superfície"
                                    : "Altura: pontos XYZ",
                 EditorWidget::TerrainPathFollowTerrain, path.followTerrain,
                 !path.useDefinedHeight);
          field("Altura Y do ponto (m)", point.y, 45,
                !path.useDefinedHeight &&
                    (!path.followTerrain ||
                     (path.uniformRamp > 0 &&
                      (s.terrainPathPoint == 0 ||
                       s.terrainPathPoint + 1 == path.points.size()))));
          button(row(),
                 point.curved ? "Próximo trecho: curvo"
                              : "Próximo trecho: reto",
                 EditorWidget::TerrainPointCurve, point.curved, true,
                 UiIcon::TerrainPathPoints);
        }
        pair("Ponto anterior", EditorWidget::TerrainPointPrevious,
             "Próximo ponto", EditorWidget::TerrainPointNext);
        pair("Inserir ponto", EditorWidget::TerrainPointInsert, "Remover ponto",
             EditorWidget::TerrainPointRemove);
        const char *caps[]{"Extremidades: redondas", "Extremidades: retas",
                           "Extremidades: quadradas"};
        button(row(), caps[u32(path.cap)], EditorWidget::TerrainPathCap);
        button(row(), path.enabled ? "Caminho ativo" : "Caminho desativado",
               EditorWidget::TerrainPathEnabled, path.enabled);
        pair("Duplicar caminho", EditorWidget::TerrainPathDuplicate,
             "Excluir caminho", EditorWidget::TerrainPathDelete);
        hint("Editar mantém o traçado original recuperável.",
             "Aplicar confirma; Reverter cancela o rascunho.");
      } else {
        const auto &curve =
            s.terrainSection == 2 ? path.textureProfile : path.profile;
        const u32 selected =
            std::min(s.terrainProfilePoint, u32(curve.size() - 1));
        if (!compact(area))
          label(s.terrainSection == 2 ? "CURVA DE PINTURA"
                                      : "CURVA DO ACOSTAMENTO");
        auto graph = deflate(row(compact(area) ? 64 : 88), UiInsets::all(8));
        list.addRect(graph, theme.color.raised, 5);
        const auto normalizedY = [&](float y) {
          return s.terrainSection == 2 ? y : (y + 1) / 3;
        };
        if (s.terrainSection == 1)
          for (float level : {0.f, 1.f}) {
            const float y =
                graph.bottom() - 8 - normalizedY(level) * (graph.height - 16);
            list.addLine({graph.x + 8, y}, {graph.right() - 8, y},
                         theme.color.textMuted, 1);
          }
        for (usize i = 0; i < curve.size(); ++i) {
          UiPoint at{graph.x + 8 + curve[i].x * (graph.width - 16),
                     graph.bottom() - 8 -
                         normalizedY(curve[i].y) * (graph.height - 16)};
          if (i) {
            UiPoint before{graph.x + 8 + curve[i - 1].x * (graph.width - 16),
                           graph.bottom() - 8 -
                               normalizedY(curve[i - 1].y) *
                                   (graph.height - 16)};
            list.addLine(before, at, theme.color.accent, 2);
          }
          list.addRect({at.x - 5, at.y - 5, 10, 10},
                       i == selected ? theme.color.accent : theme.color.text,
                       5);
          router.addRegion({at.x - 16, at.y - 16, 32, 32},
                           widgetId(EditorWidget::TerrainPathNew) + 0x500 +
                               u32(i));
        }
        auto curveActions = row(44);
        const EditorWidget actions[]{EditorWidget::TerrainProfilePrevious,
                                     EditorWidget::TerrainProfileNext,
                                     EditorWidget::TerrainProfileInsert,
                                     EditorWidget::TerrainProfileRemove};
        const char *labels[]{"< nó", "nó >", "+ nó", "- nó"};
        for (u32 i = 0; i < 4; ++i)
          button({curveActions.x + i * curveActions.width / 4, curveActions.y,
                  curveActions.width / 4, 44},
                 labels[i], actions[i], false,
                 i == 2   ? curve.size() < 32
                 : i == 3 ? selected > 0 && selected + 1 < curve.size()
                          : true);
        auto curveValues = row(64);
        fieldAt({curveValues.x, curveValues.y, curveValues.width * .5f,
                 curveValues.height},
                "Distância normalizada", curve[selected].x, 50,
                selected > 0 && selected + 1 < curve.size());
        fieldAt({curveValues.x + curveValues.width * .5f, curveValues.y,
                 curveValues.width * .5f, curveValues.height},
                s.terrainSection == 2 ? "Opacidade (0–1)" : "Talude / crista",
                curve[selected].y, 51);
        if (s.terrainSection == 1) {
          button(row(),
                 path.shape ? "Modelagem do terreno: ligada"
                            : "Modelagem do terreno: desligada",
                 EditorWidget::TerrainPathShape, path.shape, true,
                 UiIcon::TerrainPathProfile);
          fields("Largura da pista (m)", path.width, 32, "Largura total (m)",
                 path.shoulderWidth, 33);
          button(row(),
                 path.keepWidthRatio ? "Manter proporção: sim"
                                     : "Larguras independentes",
                 EditorWidget::TerrainPathRatio, path.keepWidthRatio);

          // Cross-section is drawn from the same piecewise-linear profile data.
          auto chart = deflate(row(72), UiInsets::all(8));
          list.addRect(chart, theme.color.raised, 5);
          const float lane = path.width / std::max(path.shoulderWidth, .01f);
          auto profileY = [&](float t) {
            for (usize i = 1; i < path.profile.size(); ++i)
              if (t <= path.profile[i].x) {
                const auto &a = path.profile[i - 1], &b = path.profile[i];
                return std::lerp(a.y, b.y, (t - a.x) / (b.x - a.x));
              }
            return path.profile.back().y;
          };
          UiPoint previous{};
          for (u32 i = 0; i <= 80; ++i) {
            const float x = i / 80.f, d = std::abs(x * 2 - 1);
            const float y =
                d < lane ? 1 : profileY((d - lane) / std::max(.001f, 1 - lane));
            UiPoint next{chart.x + x * chart.width,
                         chart.bottom() - 8 -
                             normalizedY(y) * (chart.height - 16)};
            if (i)
              list.addLine(previous, next, theme.color.accent, 2);
            previous = next;
          }
          fields("Elevação relativa (m)", path.elevation, 34,
                 "Inclinação máxima (°)", path.slopeLimit, 35);
          field(path.followTerrain ? "Seguir terreno 0 ↔ rampa uniforme 1"
                                   : "Alturas dos pontos 0 ↔ rampa uniforme 1",
                path.uniformRamp, 36);
          button(row(),
                 path.autoRamp ? "Auto Ramp: ligado" : "Auto Ramp: desligado",
                 EditorWidget::TerrainPathAutoRamp, path.autoRamp);
          button(row(),
                 path.useDefinedHeight ? "Usar altura definida: sim"
                                       : "Alturas dos pontos / terreno",
                 EditorWidget::TerrainPathDefinedHeight, path.useDefinedHeight);
          field("Altura definida (m)", path.definedHeight, 37,
                path.useDefinedHeight);
          button(row(),
                 path.reverseRamp ? "Direção da rampa: invertida"
                                  : "Direção da rampa: normal",
                 EditorWidget::TerrainPathReverse, path.reverseRamp,
                 path.uniformRamp > 0 && !path.useDefinedHeight);
        } else {
          button(row(),
                 path.texture ? "Pintura do caminho: ligada"
                              : "Pintura do caminho: desligada",
                 EditorWidget::TerrainPathTexture, path.texture);
          fields("Camada da pista", path.surfaceLayer, 42, "Camada acostamento",
                 path.shoulderLayer, 43);
          fields("Opacidade pista", path.textureOpacity, 38,
                 "Opacidade acostamento", path.shoulderOpacity, 39);
          field("Transição da textura (m)", path.textureTransition, 40);

          button(row(),
                 path.smartPaint ? "Cruzamentos: preservar pista mais forte"
                                 : "Cruzamentos: ordem dos caminhos",
                 EditorWidget::TerrainPathSmart, path.smartPaint);
          hint("Forma e textura têm transições independentes.");
        }
      }
    }
  } else {
    label(s.terrainTarget ? "TERRENO EM EDIÇÃO" : "CRIAR OU RETOMAR");
    pair("Novo terreno", EditorWidget::TerrainNew, "Abrir seleção",
         EditorWidget::TerrainLoad);
    pair("Enquadrar", EditorWidget::TerrainFrame, "Salvar cena",
         EditorWidget::TerrainSave, s.terrainTarget != 0);
    pair("Desfazer", EditorWidget::Undo, "Refazer", EditorWidget::Redo,
         s.terrainTarget != 0);
    button(row(), "Reverter alterações não aplicadas",
           EditorWidget::TerrainRevert, false, s.terrainTarget != 0);
    button(row(),
           r.collision ? "Colisão triangular: ligada"
                       : "Colisão triangular: desligada",
           EditorWidget::TerrainCollision, r.collision);
    hint(std::to_string(r.strokes.size()) + " / 4096 amostras de pincel",
         "Revisões anteriores são mantidas para Undo.");
    const auto retained =
        s.terrainRetainedBytesKnown
            ? num(double(s.terrainRetainedBytes) / (1024 * 1024)) + " / " +
                  num(double(EditorScreenState::TerrainRetainedByteLimit) /
                      (1024 * 1024)) +
                  " MiB"
            : "tamanho não confirmado";
    hint("Biblioteca total: " + retained,
         "Partes retidas: " + std::to_string(s.terrainRetainedDraws) + " / " +
             std::to_string(EditorScreenState::TerrainRetainedDrawLimit));
    hint("Não mede a RAM · sem limpeza automática.",
         "Aplique primeiro; depois salve a cena.");
  }
  router.popClip();
  list.popClip();
  float limit = terrainScrollLimit(s, area);
  if (limit > 0 && window.height > 0) {
    float thumb =
        std::max(16.f, window.height * window.height / contentHeight(s));
    list.addRect({area.right() - 5, window.y, 2, window.height},
                 theme.color.lineSoft, 1);
    list.addRect({area.right() - 5,
                  window.y + (window.height - thumb) * scroll / limit, 2,
                  thumb},
                 theme.color.textMuted, 1);
  }
  auto footer = UiRect{outer.x, window.bottom(), outer.width,
                       compact(area) ? 64.f : 88.f};
  bool pending = !s.terrainTarget ||
                 resources::serializeTerrainRecipe(r) != s.terrainAppliedRecipe;
  auto statusRow = take(footer, compact(area) ? 20 : 24);
  if (compact(area) && !s.terrainBusy) {
    auto badge = statusRow;
    badge.width = 82;
    list.addRect(deflate(badge, UiInsets{1, 1, 1, 1}),
                 pending ? theme.color.accentWash : theme.color.raised, 4);
    text(badge, pending ? "Por aplicar" : "Aplicado",
         pending ? theme.color.warning : theme.color.textMuted, true);
    statusRow.x += badge.width;
    statusRow.width -= badge.width;
    text(statusRow, s.terrainMessage, theme.color.textMuted, true);
  } else {
    const std::string status =
        s.terrainBusy
            ? "Gerando · " + std::to_string(int(s.terrainProgress * 100)) + "%"
        : pending ? "Alterações por aplicar"
                  : "Aplicado · salve a cena para manter";
    text(statusRow, status,
         pending ? theme.color.warning : theme.color.textMuted, true);
  }
  auto action = take(footer, compact(area) ? 44 : 48);
  if (!s.terrainBusy && s.terrainTarget && !pending &&
      (s.terrainTab == 1 || s.terrainTab == 2)) {
    button({action.x, action.y, action.width * .5f, action.height},
           s.terrainTab == 2 ? "Pintar" : "Esculpir",
           EditorWidget::TerrainSculpt, s.terrainBrushArmed);
    button({action.x + action.width * .5f, action.y, action.width * .5f,
            action.height},
           "Navegar", EditorWidget::TerrainNavigate, !s.terrainBrushArmed);
  } else if (!s.terrainBusy && !pending)
    button(action, "Salvar cena", EditorWidget::TerrainSave);
  else
    button(action,
           s.terrainBusy     ? "Cancelar"
           : s.terrainTarget ? "Aplicar configuração"
                             : "Criar terreno",
           s.terrainBusy ? EditorWidget::TerrainCancel
                         : EditorWidget::TerrainGenerate,
           true, s.terrainBusy || pending);
  if (!compact(area))
    text(footer, s.terrainMessage, theme.color.textMuted, true);
}
} // namespace ae::editor
