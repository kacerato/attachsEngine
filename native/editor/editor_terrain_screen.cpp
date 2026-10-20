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
  if(view==120) return s.terrainDraft.surface.geo.mode==resources::TerrainGeoMode::Off?"Geo · desligado":"Estratos · Geo";
  if(view==121) return "Geo · Fontes e mistura";
  if(view==123) return "Geo · Influência da camada";
  if(view==124) return "Geo · Faixa de alturas";
  if(view==130) return s.terrainDraft.surface.geo.curveEnabled?"Geo · Chaves da curva":"Curva · desligada";
  if(view==131) return s.terrainDraft.surface.geo.curveEnabled?"Geo · Tangentes e pesos":"Tangentes · curva off";
  if(view==132) return "Geo · Eixo da curva";
  if(view==140 || view==150) return "Geo · Fonte de cor";
  if(view==141 || view==151) return "Geo · Fonte normal";
  if(view==159) return "Mapas globais · Terreno";
  if(view>=160 && view<=183 && terrainModuleView(view)) {
    const char *names[]{"Tint","Normal","SAOM","Emissão"};
    return std::string("Global · ")+names[terrainModuleGlobal(view)];
  }
  if(view==1) return "Pintura · Acervo";
  if(view==2) return "Pintura · Camada";
  if(view==3) return "Pintura · Mapas PBR";
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
bool compactTerrainModule(const EditorScreenState &s,const UiRect &area) {
  return compact(area) && s.terrainTab==2 && s.terrainMaterialView!=0 && !s.terrainBusy && s.terrainBrush.layer<s.terrainDraft.surface.layers.size();
}
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
    const bool deep=compactTerrainModule(s,terrainPanelRect(s));
    if(s.terrainMaterialView==1) {
      const auto count=s.terrainCatalog ? std::min<usize>(6,s.terrainCatalog->materials.size()-
        std::min<usize>(s.terrainCatalog->materials.size(),usize(s.terrainCatalogPage)*6)):0;
      return 146.f+(s.terrainCatalog?122.f+112.f*float((count+1)/2):58.f)+padding+16-(deep?210.f:0.f);
    }
    if(terrainModuleView(s.terrainMaterialView)) {
      const bool small=compactTerrainModule(s,terrainPanelRect(s));
      const float prefix=small?162.f:0.f,geoNav=small?44.f:0.f;
      if(s.terrainMaterialView==130) return (s.terrainDraft.surface.geo.curve.keys.empty()?662.f:758.f)-prefix-geoNav-(small?80.f:0.f)+padding;
      if(s.terrainMaterialView==131) return (s.terrainDraft.surface.geo.curve.keys.empty()?578.f:974.f)-prefix-geoNav-(small?80.f:0.f)+padding;
      if(s.terrainMaterialView==132) return 490.f-prefix-geoNav+padding;
      if(terrainModulePicker(s.terrainMaterialView)) return 564.f-prefix+padding;
      if(terrainModuleMapView(s.terrainMaterialView)) return (s.terrainMaterialView==172?598.f:908.f)-prefix+padding;
      if(s.terrainMaterialView>=160 && s.terrainMaterialView<=163) return (s.terrainMaterialView<162?834.f:822.f)-prefix-(small?56.f:44.f)+padding;
      if(s.terrainMaterialView==120) return 574.f-prefix-geoNav+padding;
      if(s.terrainMaterialView==121) return 478.f-prefix-geoNav+padding;
      if(s.terrainMaterialView==124) return 554.f-prefix-geoNav+padding;
      if(s.terrainMaterialView==123) return 516.f-prefix-geoNav+padding;
      return 492.f-prefix+padding;
    }
    if(s.terrainMaterialView==60) return deep?470.f+44.f*float(s.terrainDraft.surface.layers[s.terrainBrush.layer].cluster.variants.size())+padding:914.f+padding+16;
    if(terrainClusterVariantView(s.terrainMaterialView)) {
      if(deep) return (s.terrainMaterialView>=100?(s.terrainMapRole>=2 && s.terrainMapRole<=5?316.f:714.f):s.terrainMaterialView>=80?346.f:372.f)+padding;
      if(s.terrainMaterialView>=100) return (s.terrainMapRole>=2 && s.terrainMapRole<=5?702.f:1100.f)+padding+16;
      if(s.terrainMaterialView>=80) return 700.f+padding+16;
      return 650.f+padding+16;
    }
    if(s.terrainMaterialView==30) return deep?382.f+padding:800.f+padding+16;
    if(s.terrainMaterialView==50) return deep?352.f+padding:712.f+padding+16;
    if(terrainNoiseAvailable(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView)) {
      if(deep) {
        if(terrainNoisePicker(s.terrainMaterialView)) return 346.f+padding;
        const auto selection=terrainNoiseSelection(s.terrainMaterialView);
        const auto &noise=terrainSelectedNoise(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView);
        const auto *entry=selection>=10 && selection<=12?terrainBoundAntiTile(s.terrainDraft.surface,s.terrainBrush.layer):nullptr;
        const bool last=entry && noise.enabled && unsigned(entry->normal.enabled)+unsigned(entry->detail.enabled)+unsigned(entry->distance.enabled)==1;
        const bool required=selection==13 && s.terrainDraft.surface.layers[s.terrainBrush.layer].resampleBlend==resources::TerrainDistanceBlend::Noise;
        return (selection<10?64.f:0.f)+192.f+(selection<=12?64.f:0.f)+304.f+
          (terrainNoiseNormal(s.terrainMaterialView) || noise.map.channel==resources::TerrainSurfaceChannel::RGB?462.f:64.f)+
          (last || required?32.f:0.f)+(u32(noise.map.channel)>4 && !terrainNoiseNormal(s.terrainMaterialView)?32.f:0.f)+padding;
      }
      if(terrainNoisePicker(s.terrainMaterialView)) return 838.f+padding+16;
      const auto &n=terrainSelectedNoise(s.terrainDraft.surface,s.terrainBrush.layer,s.terrainMaterialView);
      return (n.map.channel==resources::TerrainSurfaceChannel::RGB || terrainNoiseNormal(s.terrainMaterialView)?1414.f:1016.f)+
        (terrainNoiseSelection(s.terrainMaterialView)>=10 && terrainNoiseSelection(s.terrainMaterialView)<=12?32.f:0.f)+(u32(n.map.channel)>4 && !terrainNoiseNormal(s.terrainMaterialView)?32.f:0.f)+padding+16;
    }
    if(terrainNoiseView(s.terrainMaterialView)) return 76.f+padding;
    if(s.terrainMaterialView==2 && s.terrainBrush.layer<s.terrainDraft.surface.layers.size())return 1164.f+(s.terrainDraft.surface.layers[s.terrainBrush.layer].cluster.enabled?32.f:0.f)+padding+16-(deep?162.f:0.f);
    if(s.terrainMaterialView==3 && s.terrainBrush.layer<s.terrainDraft.surface.layers.size())
      return (s.terrainMapRole>=2 && s.terrainMapRole<=5?514.f:912.f)+padding+16-(deep?210.f:0.f);
    return 662.f+padding+16;
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
  take(body, compactTerrainModule(s,area)?64.f:(compact(area) ? 36.f : 48.f));
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
  const bool moduleCompact=compactTerrainModule(s,area);
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
    const bool moduleTab=moduleCompact && ((widgetId(id)>=terrainModuleActionBase+1 && widgetId(id)<=terrainModuleActionBase+3) ||
      (widgetId(id)>=terrainModuleActionBase+14 && widgetId(id)<=terrainModuleActionBase+17) ||
      id==terrainModuleAction(49) || id==terrainModuleAction(60) || id==terrainModuleAction(5) || id==terrainModuleAction(7) || id==terrainModuleAction(12));
    text(label, value, ink, compactNoiseIcon || noiseRole || moduleTab || (moduleCompact && rect.height<=32), UiAlign::Center);
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
  if(moduleCompact) {
    button({name.x,name.y,32,name.height},"<",terrainModuleAction(61),false,!s.terrainBusy);
    name.x+=36;name.width-=36;
  }
  if (compact(area))
    text(name, materialContext.empty()?(s.terrainTarget ? "Editar terreno" : "Novo terreno"):materialContext,
         theme.color.text,moduleCompact);
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
  if(moduleCompact) {
    auto selected=take(outer,32);
    button({selected.x,selected.y,32,32},"<",terrainModuleAction(62));
    button({selected.right()-32,selected.y,32,32},">",terrainModuleAction(63));
    const auto current=std::min(s.terrainBrush.layer,3u);
    const auto &layer=s.terrainDraft.surface.layers[current];
    const UiIcon fallback[]{UiIcon::TerrainSampleGrass,UiIcon::TerrainSampleRock,UiIcon::TerrainSampleSoil,UiIcon::TerrainSamplePath};
    const auto icon=layer.sourceCatalogId.empty()?fallback[current]:terrainMaterialIcon(layer.sourceCatalogId);
    list.addImage({selected.x+38,selected.y+4,24,24},static_cast<UiImageId>(icon));
    text({selected.x+66,selected.y,selected.width-98,32},std::to_string(current+1)+" · "+materialDisplayName(layer.name),theme.color.text,true);
    auto contexts=take(outer,32);
    const auto view=s.terrainMaterialView;
    const char *pbrRoles[]{"Cor base","Normal","Altura","AO","Rugosidade","Metálico","Emissão"};
    if(terrainModuleView(view)) {
      if(view<159) {
        const char *names[]{"Geo","Fontes","Faixa","Curva","Camada"};
        const u32 actions[]{49,60,5,7,12};
        const u32 active=view==120?0:(view==121 || view==140 || view==141 || view==150 || view==151)?1:view==124?2:(view>=130 && view<=132)?3:4;
        for(u32 i=0;i<5;++i) button({contexts.x+contexts.width*i/5,contexts.y,contexts.width/5,32},names[i],terrainModuleAction(actions[i]),active==i);
      } else {
        const char *names[]{"Tint","Normal","SAOM","Emissão"};
        for(u32 i=0;i<4;++i) button({contexts.x+contexts.width*i/4,contexts.y,contexts.width/4,32},names[i],terrainModuleAction(14+i),view!=159 && terrainModuleGlobal(view)==i);
      }
    } else if(view==1) {
      button({contexts.x,contexts.y,32,32},"<",EditorWidget::TerrainCatalogPrevious,false,s.terrainCatalogPage>0);
      button({contexts.right()-32,contexts.y,32,32},">",EditorWidget::TerrainCatalogNext,false,s.terrainCatalog && (s.terrainCatalogPage+1)*6<s.terrainCatalog->materials.size());
      text({contexts.x+32,contexts.y,contexts.width-64,32},"Acervo · página "+std::to_string(s.terrainCatalogPage+1),theme.color.text,true,UiAlign::Center);
    } else if(view==3 || (view>=100 && view<=102)) {
      button({contexts.x,contexts.y,32,32},"<",view==3?EditorWidget::TerrainMapPrevious:terrainClusterAction(11));
      button({contexts.right()-32,contexts.y,32,32},">",view==3?EditorWidget::TerrainMapNext:terrainClusterAction(12));
      text({contexts.x+32,contexts.y,contexts.width-64,32},pbrRoles[std::min(s.terrainMapRole,6u)],theme.color.text,true,UiAlign::Center);
    } else if(terrainNoisePicker(view)) {
      button({contexts.x,contexts.y,32,32},"<",terrainNoiseAction(12),false,s.terrainCatalogPage>0);
      button({contexts.right()-32,contexts.y,32,32},">",terrainNoiseAction(13),false,s.terrainCatalog && (s.terrainCatalogPage+1)*6<s.terrainCatalog->textures.size());
      button({contexts.x+32,contexts.y,contexts.width-64,32},"Voltar · página "+std::to_string(s.terrainCatalogPage+1),terrainNoiseAction(11));
    } else if(terrainNoiseView(view)) {
      if(terrainNoiseAvailable(s.terrainDraft.surface,s.terrainBrush.layer,view)) {
        const auto selection=terrainNoiseSelection(view);
        const auto &noise=terrainSelectedNoise(s.terrainDraft.surface,s.terrainBrush.layer,view);
        const auto *entry=selection>=10 && selection<=12?terrainBoundAntiTile(s.terrainDraft.surface,s.terrainBrush.layer):nullptr;
        const bool last=entry && noise.enabled && unsigned(entry->normal.enabled)+unsigned(entry->detail.enabled)+unsigned(entry->distance.enabled)==1;
        const bool required=selection==13 && layer.resampleBlend==resources::TerrainDistanceBlend::Noise;
        float used=0;
        if(selection<10) {
          button({contexts.x,contexts.y,60,32},"Global",terrainNoiseAction(44),!terrainNoiseLocal(view));
          button({contexts.x+60,contexts.y,60,32},"Local",terrainNoiseAction(45),terrainNoiseLocal(view));used=120;
        } else {
          const auto back=selection<=12?terrainNoiseAction(26):selection==13?terrainNoiseAction(35):terrainClusterAction(0);
          button({contexts.x,contexts.y,100,32},selection<=12?"Anti-tile":selection==13?"Distante":"Variantes",back);used=100;
        }
        button({contexts.x+used,contexts.y,80,32},"Fonte",terrainNoiseAction(10));used+=80;
        button({contexts.x+used,contexts.y,contexts.width-used,32},!noise.map.texture.valid()?"Sem fonte":noise.enabled?"Ligado":"Desligado",terrainNoiseAction(5),noise.enabled,noise.map.texture.valid() && !last && !required);
      } else button(contexts,"Selecionar entrada anti-tile",terrainNoiseAction(26));
    } else if(view==30) {
      const auto &array=s.terrainDraft.surface.antiTileArray;
      button({contexts.x,contexts.y,contexts.width/3,32},"Ruído",terrainNoiseAction(1));
      button({contexts.x+contexts.width/3,contexts.y,contexts.width/3,32},"Criar",terrainNoiseAction(28),false,array.entries.size()<32);
      button({contexts.x+contexts.width*2/3,contexts.y,contexts.width/3,32},array.enabled?"Ligado":"Desligado",terrainNoiseAction(27),array.enabled,!array.entries.empty());
    } else if(view==50) {
      const char *names[]{"Ruído","Constant","Noise","Fade"};
      const u32 actions[]{1,39,40,41};
      for(u32 i=0;i<4;++i)button({contexts.x+contexts.width*i/4,contexts.y,contexts.width/4,32},names[i],terrainNoiseAction(actions[i]),i && u32(layer.resampleBlend)==i-1);
    } else if(terrainClusterVariantView(view) && view>=80 && view<=92) {
      const bool material=view<=82;
      const auto count=s.terrainCatalog?(material?s.terrainCatalog->materials.size():s.terrainCatalog->textures.size()):0;
      button({contexts.x,contexts.y,32,32},"<",terrainClusterAction(16),false,s.terrainCatalogPage>0);
      button({contexts.right()-32,contexts.y,32,32},">",terrainClusterAction(17),false,(s.terrainCatalogPage+1)*6<count);
      button({contexts.x+32,contexts.y,contexts.width-64,32},"Voltar · página "+std::to_string(s.terrainCatalogPage+1),material?terrainClusterAction(0):terrainClusterAction(7));
    } else if(terrainClusterVariantView(view)) {
      const char *names[]{"Variantes","Mapas","Material"};const u32 actions[]{0,7,8};
      for(u32 i=0;i<3;++i)button({contexts.x+contexts.width*i/3,contexts.y,contexts.width/3,32},names[i],terrainClusterAction(actions[i]));
    } else if(view==60) {
      button({contexts.x,contexts.y,contexts.width/3,32},"Ruído",terrainNoiseAction(0));
      button({contexts.x+contexts.width/3,contexts.y,contexts.width/3,32},"Geo",terrainModuleAction(0));
      button({contexts.x+contexts.width*2/3,contexts.y,contexts.width/3,32},"Globais",terrainModuleAction(13));
    } else {
      const char *names[]{"Acervo","Camada","Mapas","Ruído"};
      const EditorWidget actions[]{EditorWidget::TerrainMaterialLibrary,EditorWidget::TerrainMaterialProperties,EditorWidget::TerrainMaterialMaps,terrainNoiseAction(0)};
      for(u32 i=0;i<4;++i)button({contexts.x+contexts.width*i/4,contexts.y,contexts.width/4,32},names[i],actions[i],view==i+1);
    }
  } else {
  auto tabrow = take(outer, compact(area) ? 36 : 48);
  for (u32 col = 0; col < 5; ++col) {
    const u32 i = tabOrder[col];
    button({tabrow.x + tabrow.width * col / 5, tabrow.y, tabrow.width / 5,
            tabrow.height},
           tabs[i], EditorWidget(widgetId(EditorWidget::TerrainTabBase) + i),
           s.terrainTab == i, !s.terrainBusy, icons[i]);
  }
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
  const auto field = [&](std::string_view n, double v, u32 id, bool en = true) {
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
      if(!moduleCompact) {
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
        sections[i],actions[i],i==4 && (terrainNoiseView(s.terrainMaterialView) || s.terrainMaterialView==30 || s.terrainMaterialView==50 || s.terrainMaterialView==60 || terrainClusterVariantView(s.terrainMaterialView)),true,i==4?UiIcon::TerrainNoise:UiIcon::None);
    auto modules=row(40);
    button({modules.x,modules.y,modules.width/2,40},"Estratos",terrainModuleAction(0),s.terrainMaterialView>=120 && s.terrainMaterialView<=151,true,UiIcon::ZzTerrainModuleStrata);
    button({modules.x+modules.width/2,modules.y,modules.width/2,40},"Mapas globais",terrainModuleAction(13),s.terrainMaterialView>=159 && terrainModuleView(s.terrainMaterialView),true,UiIcon::ZzTerrainModuleGlobalMaps);
    }
    if(terrainModuleView(s.terrainMaterialView) && b.layer<r.surface.layers.size()) {
      const auto view=s.terrainMaterialView;
      const auto &geo=r.surface.geo;
      const auto &layer=r.surface.layers[b.layer];
      const auto sourceName=[&](const resources::TerrainSurfaceMap &map) {
        if(s.terrainCatalog) for(const auto &source:s.terrainCatalog->textures)
          if(source.identity==map.texture) return terrainNoiseSourceName(source);
        return map.texture.valid()?std::string("Textura externa vinculada"):std::string("Selecionar textura original");
      };
      const auto geoNavigation=[&]() {
        if(moduleCompact) return;
        auto nav=row(44);
        const char *names[]{"Geo","Fontes","Faixa","Curva","Camada"};
        const u32 actions[]{49,60,5,7,12};
        const u32 views[]{120,121,124,130,123};
        for(u32 i=0;i<5;++i) button({nav.x+nav.width*i/5,nav.y,nav.width/5,44},names[i],terrainModuleAction(actions[i]),view==views[i] || (i==3 && (view==131 || view==132)));
      };
      if(view==120) {
        geoNavigation();
        field("Participação da mistura",geo.blendStrength,191,geo.mode!=resources::TerrainGeoMode::Off);
        field("Força do normal",geo.normalStrength,192,geo.mode==resources::TerrainGeoMode::AlbedoNormal);
        field("Tamanho vertical (m/repetição)",geo.scaleMeters,193,geo.mode!=resources::TerrainGeoMode::Off);
        field("Deslocamento vertical (m)",geo.offsetMeters,194,geo.mode!=resources::TerrainGeoMode::Off);
        field("Contraste do filtro por altura",geo.heightContrast,202,geo.mode!=resources::TerrainGeoMode::Off);
        hint("Cor e normal originais 1 × 256, quando selecionados.","O alfa da cor Geo mascara a mistura.");
      } else if(view==121) {
        geoNavigation();
        button(row(48),"Usar estratos originais do plugin",terrainModuleAction(9),false,true,UiIcon::ZzTerrainModuleStrata);
        auto modes=row(44);
        const char *names[]{"Desligado","Cor","Cor + normal"};
        for(u32 i=0;i<3;++i)button({modes.x+modes.width*i/3,modes.y,modes.width/3,44},names[i],terrainModuleAction(1+i),u32(geo.mode)==i,
          i==0 || (geo.albedo.texture.valid() && (i!=2 || geo.normal.texture.valid())));
        button(row(44),"Cor · "+sourceName(geo.albedo),terrainModuleAction(10));
        button(row(44),"Normal · "+sourceName(geo.normal),terrainModuleAction(11));
        button(row(44),geo.blend==resources::TerrainColorBlend::Multiply2x?"Mistura: Multiply2x":"Mistura: LighterColor",terrainModuleAction(4),false,geo.mode!=resources::TerrainGeoMode::Off);
        hint("Fontes e interpretação originais preservadas.","Escolha o modo depois de vincular as fontes reais.");
      } else if(view==124) {
        geoNavigation();
        button(row(44),geo.rangeEnabled?"Faixa de alturas ligada":"Faixa de alturas desligada",terrainModuleAction(6),geo.rangeEnabled);
        field("Entrada · início (altura mundial m)",geo.range[0],195,geo.rangeEnabled);
        field("Entrada · fim (altura mundial m)",geo.range[1],196,geo.rangeEnabled);
        field("Saída · início (altura mundial m)",geo.range[2],197,geo.rangeEnabled);
        field("Saída · fim (altura mundial m)",geo.range[3],198,geo.rangeEnabled);
        hint("Alturas permanecem em ordem; coincidência é válida.","Alturas coincidentes criam um limite imediato.");
      } else if(view==123) {
        geoNavigation();label("CAMADA · "+layer.name);
        field("Participação Geo da camada",layer.geoParticipation,203);
        button(row(44),layer.geoHeightFilterEnabled?"Filtro de altura ligado":"Filtro de altura desligado",terrainModuleAction(55),layer.geoHeightFilterEnabled);
        field("Limiar assinado (-1 a 1)",layer.geoHeightThreshold,204,layer.geoHeightFilterEnabled);
        field("Força do filtro",layer.geoHeightStrength,205,layer.geoHeightFilterEnabled);
        hint("O sinal inverte o filtro da altura material.","Força zero preserva a influência sem redução.");
      } else if(view==132) {
        geoNavigation();
        button(row(44),"Voltar às chaves da curva",terrainModuleAction(59));
        field("Período horizontal (m)",geo.curveScaleMeters,199);
        field("Deslocamento horizontal (m)",geo.curveOffsetMeters,200);
        field("Rotação do eixo (°)",geo.curveRotationDegrees,201);
        hint("O eixo rotacionado percorre um período horizontal.","A saída da curva desloca diretamente o UV Geo.");
      } else if(view==130 || view==131) {
        geoNavigation();
        const auto &curve=geo.curve;
        const auto selected=curve.keys.empty()?usize(0):std::min<usize>(s.terrainProfilePoint,curve.keys.size()-1);
        const auto *key=curve.keys.empty()?nullptr:&curve.keys[selected];
        const auto domain=terrainGeoCurveDomain(curve);
        const auto curveInk=geo.curveEnabled?theme.color.accent:theme.color.textMuted;
        auto graph=deflate(row(moduleCompact?80.f:144.f),UiInsets{10,8,10,8});
        list.addRect(graph,theme.color.raised,6);list.addBorder(graph,theme.color.lineSoft,1,6);
        float low=-.05f,high=.05f;
        for(const auto &point:curve.keys) {low=std::min(low,point.value);high=std::max(high,point.value);}
        for(u32 i=0;i<=96;++i) {resources::AnimationCurveSample sample;const auto time=std::lerp(domain[0],domain[1],float(i)/96);if(resources::sampleValidatedAnimationCurve(curve,time,sample)) {low=std::min(low,float(sample.value));high=std::max(high,float(sample.value));}}
        const float margin=std::max(.05f,(high-low)*.1f);low-=margin;high+=margin;
        const auto pointAt=[&](double time,double value) {return UiPoint{
          graph.x+float((time-domain[0])/(double(domain[1])-domain[0]))*graph.width,
          graph.bottom()-float(std::clamp((value-low)/(double(high)-low),-2.0,3.0))*graph.height};};
        list.pushClip(graph);
        const auto zero=pointAt(0,0),one=pointAt(1,0);
        list.addRect({zero.x,graph.y,one.x-zero.x,graph.height},geo.curveEnabled?theme.color.accentWash:theme.color.lineSoft);
        list.addLine({zero.x,graph.y},{zero.x,graph.bottom()},theme.color.lineSoft,1);
        list.addLine({one.x,graph.y},{one.x,graph.bottom()},theme.color.lineSoft,1);
        UiPoint previous{};
        for(u32 i=0;i<=96;++i) {
          resources::AnimationCurveSample sample;
          const auto time=std::lerp(domain[0],domain[1],float(i)/96);
          const auto valid=resources::sampleValidatedAnimationCurve(curve,time,sample);
          const auto point=pointAt(time,valid?sample.value:0);
          if(i)list.addLine(previous,point,curveInk,2);previous=point;
        }
        if(key) {
          const auto center=pointAt(key->time,key->value);
          if(selected) {const double span=(key->weightedIn?key->inWeight:1.f/3)*(double(key->time)-curve.keys[selected-1].time);const auto handle=pointAt(key->time-span,key->value-resources::animationCurveSlope(curve,selected,true)*span);list.addLine(center,handle,theme.color.textMuted,1);list.addRect({handle.x-3,handle.y-3,6,6},theme.color.textMuted,3);}
          if(selected+1<curve.keys.size()) {const double span=(key->weightedOut?key->outWeight:1.f/3)*(double(curve.keys[selected+1].time)-key->time);const auto handle=pointAt(key->time+span,key->value+resources::animationCurveSlope(curve,selected,false)*span);list.addLine(center,handle,theme.color.textMuted,1);list.addRect({handle.x-3,handle.y-3,6,6},theme.color.textMuted,3);}
          // Every key contributes to a visible 2 px occupancy cell; dense imports
          // do not exhaust UiDrawList capacity or truncate the authored curve.
          const auto columns=std::max(1u,u32(std::ceil(graph.width/2))),rows=std::max(1u,u32(std::ceil(graph.height/2)));
          std::vector<u8> occupied(usize(columns)*rows);
          for(const auto &authored:curve.keys) {
            const auto point=pointAt(authored.time,authored.value);
            const auto x=std::min(columns-1,u32(std::max(0.f,point.x-graph.x)/2));
            const auto y=std::min(rows-1,u32(std::max(0.f,point.y-graph.y)/2));
            auto &cell=occupied[usize(y)*columns+x];
            if(!cell) {cell=1;list.addRect({point.x-1.5f,point.y-1.5f,3,3},theme.color.textMuted,1.5f);}
          }
          list.addRect({center.x-5,center.y-5,10,10},theme.color.text,5);
        }
        list.popClip();if(key)router.addRegion(graph,widgetId(terrainModuleAction(54)));
        const float keyHeight=moduleCompact?28.f:44.f;
        auto keys=row(keyHeight);
        button({keys.x,keys.y,48,keyHeight},"<",terrainModuleAction(40),false,key && selected>0);
        button({keys.right()-48,keys.y,48,keyHeight},">",terrainModuleAction(41),false,key && selected+1<curve.keys.size());
        if(key) button({keys.x+48,keys.y,keys.width-96,keyHeight},"Chave "+std::to_string(selected+1)+" / "+std::to_string(curve.keys.size()),EditorWidget(widgetId(EditorWidget::TerrainNumberBase)+255));
        else button({keys.x+48,keys.y,keys.width-96,keyHeight},"Adicionar primeira chave",terrainModuleAction(42));
        button(row(44),geo.curveEnabled?"Curva de deslocamento ligada":"Curva de deslocamento desligada",terrainModuleAction(8),geo.curveEnabled,true,UiIcon::AnimationCurve);
        if(view==130) {
          auto edits=row();
          button({edits.x,edits.y,edits.width/2,edits.height},"Inserir chave",terrainModuleAction(42),false,curve.keys.size()<resources::AnimationCurve::MaximumKeys);
          button({edits.x+edits.width/2,edits.y,edits.width/2,edits.height},"Remover chave",terrainModuleAction(43),false,key!=nullptr);
          if(key) {
            field("Posição da chave (assinada)",key->time,206);
            field("Saída da chave (UV estrato)",key->value,207);
          } else hint("Insira uma chave para iniciar a autoria.");
          button(row(44),"Tangentes e pesos desta chave",terrainModuleAction(57),false,key!=nullptr);
          button(row(44),"Eixo e transformação da curva",terrainModuleAction(58));
        } else {
          button(row(44),"Voltar às chaves da curva",terrainModuleAction(59));
          if(key) {
            button(row(44),key->broken?"Tangentes independentes":"Tangentes vinculadas",terrainModuleAction(44),key->broken);
            const char *modes[]{"Livre","Linear","Constante","Auto","Auto limitado","Plana","Constante seguinte"};
            button(row(44),std::string("Entrada: ")+modes[unsigned(key->incoming)],terrainModuleAction(45));
            button(row(44),std::string("Saída: ")+modes[unsigned(key->outgoing)],terrainModuleAction(46));
            field("Inclinação de entrada (UV/posição)",key->inSlope,208,key->incoming==resources::AnimationTangentMode::Free);
            field("Inclinação de saída (UV/posição)",key->outSlope,209,key->outgoing==resources::AnimationTangentMode::Free);
            pair(key->weightedIn?"Entrada ponderada":"Ponderar entrada",terrainModuleAction(47),key->weightedOut?"Saída ponderada":"Ponderar saída",terrainModuleAction(48));
            field("Peso do handle de entrada",key->inWeight,210,key->weightedIn);
            field("Peso do handle de saída",key->outWeight,211,key->weightedOut);
          } else hint("Curva vazia: saída constante zero.","Volte às chaves para inserir a primeira.");
        }
        hint("Área sombreada: janela LUT 0–1.","Chaves externas são preservadas; toque seleciona.");
      } else if(view==159) {
        label("MAPAS DO DOMÍNIO COMPLETO DO TERRENO");
        const char *names[]{"Tint · cor global","Normal · relevo global","SAOM · smoothness / AO / metálico","Emissão · luz do material"};
        for(u32 i=0;i<4;++i) button(row(64),names[i],terrainModuleAction(14+i),r.surface.globalMaps[i].enabled,true,UiIcon::ZzTerrainModuleGlobalMaps);
        hint("Domínio real: "+num(r.size)+" m × "+num(r.size)+" m", "Origem X/Z: "+num(-r.size*.5f)+" m; independente dos tiles.");
      } else if(view>=160 && view<=163) {
        const auto role=terrainModuleGlobal(view);
        const auto &global=r.surface.globalMaps[role];
        const bool source=role==2?(global.smoothness.texture.valid() || global.ao.texture.valid() || global.metallic.texture.valid()):global.map.texture.valid();
        auto stateRow=row(moduleCompact?32.f:44.f);
        button({stateRow.x,stateRow.y,stateRow.width/2,stateRow.height},!source?"Sem fonte":global.enabled?"Ligado":"Desligado",terrainModuleAction(18),global.enabled,source);
        button({stateRow.x+stateRow.width/2,stateRow.y,stateRow.width/2,stateRow.height},source?(role==2?"Fontes SAOM":"Fonte e canais"):"Escolher fonte",terrainModuleAction(21));
        field("Intensidade do mapa global",global.intensity,212);
        field("Participação · "+layer.name,layer.globalMapParticipation[role],221);
        fields("Fade início (m)",global.fadeStart,213,"Fade fim (m)",global.fadeEnd,214);
        field("Força perto",global.nearStrength,215);
        field("Força longe",global.farStrength,216);
        fields("Escala UV X",global.scaleX,217,"Escala UV Y",global.scaleY,218);
        fields("Offset UV X",global.offsetX,219,"Offset UV Y",global.offsetY,220);

        button(row(44),"Voltar aos quatro mapas",terrainModuleAction(13));

        if(role==0) {const char *blends[]{"Multiply2x","Overlay","LighterColor","Crossfade"};button(row(44),std::string("Mistura: ")+blends[u32(global.blend)],terrainModuleAction(19));}
        if(role==1) button(row(44),global.normalBlend==resources::TerrainNormalBlend::Reoriented?"Normal: RNM":"Normal: Crossfade",terrainModuleAction(20));
        if(role>=2) hint("Mistura real: Crossfade");
        hint("UV do domínio: "+num(r.size)+" m × "+num(r.size)+" m", "Primeiro offset UV, depois escala; origem real do terreno.");
      } else if(terrainModulePicker(view)) {
        button(row(44),"Voltar aos parâmetros do mapa",terrainModuleAction(24));
        label("FONTE · TEXTURAS ORIGINAIS TRANSPORTADAS");
        const auto catalog=s.terrainCatalog;
        if(catalog && !catalog->textures.empty()) {
          auto pager=row(44);
          button({pager.x,pager.y,48,44},"<",terrainModuleAction(30),false,s.terrainCatalogPage>0);
          button({pager.right()-48,pager.y,48,44},">",terrainModuleAction(31),false,(s.terrainCatalogPage+1)*6<catalog->textures.size());
          text({pager.x+48,pager.y,pager.width-96,44},std::to_string(s.terrainCatalogPage+1)+" / "+std::to_string((catalog->textures.size()+5)/6),theme.color.text,true,UiAlign::Center);
          for(u32 i=0;i<6 && s.terrainCatalogPage*6+i<catalog->textures.size();++i) {const auto &source=catalog->textures[s.terrainCatalogPage*6+i];button(row(48),terrainNoiseSourceName(source),terrainModuleAction(32+i),terrainModuleMap(r.surface,view,s.terrainMapRole).texture==source.identity);}
        } else hint("Nenhuma textura disponível no catálogo.");
      } else if(terrainModuleMapView(view)) {
        const auto &map=terrainModuleMap(r.surface,view,s.terrainMapRole);
        const bool normal=terrainModuleNormal(view);
        const bool saom=view==172;
        button(row(44),"Voltar ao módulo",terrainModuleAction(22));
        if(saom) {
          const char *roles[]{"Smoothness → rugosidade = 1 − S","Oclusão ambiente (AO)","Metálico","Máscara de mistura"};
          auto selector=row(44);
          button({selector.x,selector.y,44,44},"<",terrainModuleAction(28));
          button({selector.right()-44,selector.y,44,44},">",terrainModuleAction(29));
          text({selector.x+44,selector.y,selector.width-88,44},roles[s.terrainMapRole%4],theme.color.text,true,UiAlign::Center);
          button(row(44),"Usar fonte de S para AO / metálico / máscara",terrainModuleAction(56),false,r.surface.globalMaps[2].smoothness.texture.valid());
        }
        button(row(44),sourceName(map),terrainModuleAction(23));
        const char *channels[]{"R","G","B","A","RGB","Normal RGB","Normal RG"};
        button(row(44),std::string("Canal: ")+channels[u32(map.channel)],terrainModuleAction(25),false,map.texture.valid());
        auto options=row();
        button({options.x,options.y,options.width/2,options.height},normal?"Normal · linear":map.linear?"Espaço: linear":"Espaço: sRGB",terrainModuleAction(27),false,!normal && map.texture.valid());
        button({options.x+options.width/2,options.y,options.width/2,options.height},map.invert?"Inversão ligada":"Inversão desligada",terrainModuleAction(26));
        field(normal?"Fator do normal":"Fator do mapa",map.factor,222);
        if(saom) fields("Remap mínimo",map.remapMin,223,"Remap máximo",map.remapMax,224);
        else {
          label(normal?"NORMAL · FATORES XYZ":view==173?"FATORES RGB":"FATORES RGBA");
          fields("Vermelho",map.colorFactor[0],225,"Verde",map.colorFactor[1],226);
          if(normal) field("Azul",map.colorFactor[2],227,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
          else if(view==173) field("Azul",map.colorFactor[2],227);
          else fields("Azul",map.colorFactor[2],227,"Alfa",map.colorFactor[3],228);
          label("REMAP MÍNIMO");fields("Vermelho",map.colorRemapMin[0],229,"Verde",map.colorRemapMin[1],230);
          if(normal) field("Azul",map.colorRemapMin[2],231,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
          else if(view==173) field("Azul",map.colorRemapMin[2],231);
          else fields("Azul",map.colorRemapMin[2],231,"Alfa",map.colorRemapMin[3],232);
          label("REMAP MÁXIMO");fields("Vermelho",map.colorRemapMax[0],233,"Verde",map.colorRemapMax[1],234);
          if(normal) field("Azul",map.colorRemapMax[2],235,map.channel!=resources::TerrainSurfaceChannel::NormalRG);
          else if(view==173) field("Azul",map.colorRemapMax[2],235);
          else fields("Azul",map.colorRemapMax[2],235,"Alfa",map.colorRemapMax[3],236);
        }
        hint(saom?"Padrão SAOM: R smoothness · G AO · B metálico · A máscara":"Fonte, interpretação e ajustes preservados ao salvar.");
      }
    } else if((s.terrainMaterialView==60 || terrainClusterVariantView(s.terrainMaterialView)) && b.layer<r.surface.layers.size()) {
      const auto &layer=r.surface.layers[b.layer];
      const auto &cluster=layer.cluster;
      const auto index=terrainClusterVariant(s.terrainMaterialView);
      if(!moduleCompact) label("VARIANTES E ESTOCÁSTICO · "+layer.name);
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
        if(!moduleCompact) button(row(44),material?"Voltar às variantes":"Voltar aos mapas",material?terrainClusterAction(0):terrainClusterAction(7));
        label(material?"MATERIAL ORIGINAL · SETE MAPAS":"TEXTURA ORIGINAL · MAPA SELECIONADO");
        const auto catalog=s.terrainCatalog;
        const auto count=catalog?(material?catalog->materials.size():catalog->textures.size()):0;
        if(count) {
          if(!moduleCompact) {
          auto pager=row(44);
          button({pager.x,pager.y,48,44},"<",terrainClusterAction(16),false,s.terrainCatalogPage>0);
          button({pager.right()-48,pager.y,48,44},">",terrainClusterAction(17),false,(s.terrainCatalogPage+1)*6<count);
          text({pager.x+48,pager.y,pager.width-96,44},std::to_string(s.terrainCatalogPage+1)+" / "+std::to_string((count+5)/6),theme.color.text,true,UiAlign::Center);
          }
          for(u32 i=0;i<6 && s.terrainCatalogPage*6+i<count;++i) {
            const auto source=s.terrainCatalogPage*6+i;
            if(material) button(row(48),materialDisplayName(catalog->materials[source].layer.name),terrainClusterAction(20+i),false,true,terrainMaterialIcon(catalog->materials[source].id));
            else button(row(48),terrainNoiseSourceName(catalog->textures[source]),terrainClusterAction(32+i));
          }
        } else hint("Carregue o catálogo para selecionar uma fonte real.");
        hint(material?"Identidades e sete mapas do material preservados.":"Canal, remaps e fatores permanecem independentes.");
      } else if(index<cluster.variants.size()) {
        const auto &variant=cluster.variants[index];
        if(!moduleCompact) button(row(44),"Voltar às variantes",terrainClusterAction(0));
        std::string variantName="VARIANTE "+std::to_string(index+1);
        if(s.terrainCatalog) for(const auto &material:s.terrainCatalog->materials)
          if(material.layer.identity==variant.identity) {variantName=materialDisplayName(material.layer.name);break;}
        if(!moduleCompact) label(variantName);
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
          if(!moduleCompact) {
          auto selector=row(44);
          button({selector.x,selector.y,48,44},"<",terrainClusterAction(11));
          button({selector.right()-48,selector.y,48,44},">",terrainClusterAction(12));
          text({selector.x+48,selector.y,selector.width-96,44},roles[role],theme.color.text,false,UiAlign::Center);
          }
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
      if(!moduleCompact) {label("REAMOSTRAGEM · "+layer.name);
      button(row(44),"Voltar aos ruídos",terrainNoiseAction(1));}
      button(row(44),layer.resampleAlbedo?"Cor distante: ligada":"Cor distante: desligada",terrainNoiseAction(36),layer.resampleAlbedo);
      button(row(44),layer.resampleNormal?"Normal distante: ligado":"Normal distante: desligado",terrainNoiseAction(37),layer.resampleNormal);
      button(row(44),layer.resampleHeightBlend?"Mistura de altura: ligada":"Mistura de altura: desligada",terrainNoiseAction(38),layer.resampleHeightBlend);
      if(!moduleCompact) {
      auto modes=row(44);
      const char *names[]{"Constant","Noise","Crossfade"};
      for(u32 i=0;i<3;++i)button({modes.x+modes.width*i/3,modes.y,modes.width/3,44},names[i],terrainNoiseAction(39+i),u32(layer.resampleBlend)==i);
      }
      field("Escala distante (×)",layer.resampleScale,152);
      const bool distance=layer.resampleBlend!=resources::TerrainDistanceBlend::Constant;
      fields("Início (m)",layer.resampleStart,153,"Fim (m)",layer.resampleEnd,154,distance);
      button(row(44),"Editar fonte e parâmetros do Noise",terrainNoiseAction(42),false,layer.resampleBlend==resources::TerrainDistanceBlend::Noise);
      hint(distance?"Faixa de distância medida pela câmera.":"Constant usa sempre a amostragem distante.","Altura combina amostras próximas e distantes.");
    } else if(s.terrainMaterialView==30 && b.layer<r.surface.layers.size()) {
      const auto &layer=r.surface.layers[b.layer];
      const auto &array=r.surface.antiTileArray;
      if(!moduleCompact) {
      label("ANTI-TILE · CONJUNTO COMPARTILHADO");
      button(row(44),"Voltar aos ruídos",terrainNoiseAction(1));
      button(row(44),array.enabled?"Conjunto ligado":"Conjunto desligado",terrainNoiseAction(27),array.enabled,!array.entries.empty());
      button(row(44),"Criar entrada com normal do material",terrainNoiseAction(28),false,array.entries.size()<32);
      }
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
      if(!moduleCompact) {
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
      }
      if(terrainNoisePicker(s.terrainMaterialView)) {
        if(!moduleCompact) button(row(44),"Voltar aos parâmetros",terrainNoiseAction(11));
        label("FONTE · TEXTURAS ORIGINAIS DO CATÁLOGO");
        const auto catalog=s.terrainCatalog;
        if(catalog && !catalog->textures.empty()) {
          if(!moduleCompact) {
          auto pager=row(44);
          button({pager.x,pager.y,48,44},"<",terrainNoiseAction(12),false,s.terrainCatalogPage>0);
          button({pager.right()-48,pager.y,48,44},">",terrainNoiseAction(13),false,(s.terrainCatalogPage+1)*6<catalog->textures.size());
          text({pager.x+48,pager.y,pager.width-96,44},std::to_string(s.terrainCatalogPage+1)+" / "+std::to_string((catalog->textures.size()+5)/6),theme.color.text,true,UiAlign::Center);
          }
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
        if(!moduleCompact) button(row(44),noise.enabled?"Ruído ligado":"Ruído desligado",terrainNoiseAction(5),noise.enabled,noise.map.texture.valid() && !lastSource && !requiredNoise);
        if(requiredNoise) hint("Modo Noise exige esta fonte ativa.");
        if(lastSource) hint("Mantenha uma fonte ativa por entrada anti-tile.");
        std::string sourceName="Selecionar textura original";
        if(s.terrainCatalog) for(const auto &source:s.terrainCatalog->textures)
          if(source.identity==noise.map.texture) { sourceName=terrainNoiseSourceName(source); break; }
        if(!moduleCompact) button(row(44),sourceName,terrainNoiseAction(10));
        const char *projection[]{"UV","Triplanar local","Triplanar mundial"};
        if(!moduleCompact) button(row(44),std::string("Projeção: ")+projection[u32(noise.projection)],terrainNoiseAction(6));
        if(moduleCompact && selection<10) {
          auto roles=row(32);const char *names[]{"Detalhe","Distância","N1","N2","N3"};
          const u32 actions[]{local?3u:1u,local?4u:2u,23u,24u,25u};
          const auto active=selection<4?selection%2:2+(selection-4)%3;
          for(u32 i=0;i<5;++i)button({roles.x+roles.width*i/5,roles.y,roles.width/5,32},names[i],terrainNoiseAction(actions[i]),active==i);
        }
        field("Frequência (rep/m)",noise.scale,128);
        if(moduleCompact && selection<10) {
          auto links=row(32);button({links.x,links.y,links.width/3,32},"Anti-tile",terrainNoiseAction(26));
          button({links.x+links.width/3,links.y,links.width/3,32},"Distante",terrainNoiseAction(35));
          button({links.x+links.width*2/3,links.y,links.width/3,32},"Variantes",terrainClusterAction(0));
        }
        field("Força",noise.strength,129);
        fields("Fade início (m)",noise.fadeStart,130,"Fade fim (m)",noise.fadeEnd,131);
        if(selection<4) field(selection%2==0?"Participação global · detalhe":"Participação global · distância",
          selection%2==0?layer.globalDetailParticipation:layer.globalDistanceParticipation,132);
        else if(selection<10) field("Participação global · normal ",layer.globalNormalParticipation[(selection-4)%3],148);
        else if(selection<=12) field("Participação desta camada",layer.antiTileParticipation[selection==10?0:selection==11?1:2],selection==10?149:selection==11?150:151);
        hint((selection==1 || selection==3 || selection==12)?"Distância aparece ao afastar a câmera.":"Detalhe e normal desaparecem ao afastar a câmera.",
          "Participação pertence à camada selecionada.");
        if(moduleCompact) button(row(44),std::string("Projeção: ")+projection[u32(noise.projection)],terrainNoiseAction(6));
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
    } else if(terrainNoiseView(s.terrainMaterialView)) {
      hint("Esta camada não possui entrada anti-tile vinculada.");
      button(row(44),"Selecionar ou criar entrada real",terrainNoiseAction(26));
    } else if (s.terrainMaterialView == 1) {
      const auto catalog = s.terrainCatalog;
      label("BIBLIOTECA · MATERIAIS ORIGINAIS");
      if (catalog) {
        if(!moduleCompact) {
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
        }
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
      if(!moduleCompact) {
      auto selector = row(48);
      button({selector.x, selector.y, 48, 48}, "<",
             EditorWidget::TerrainMapPrevious);
      button({selector.right() - 48, selector.y, 48, 48}, ">",
             EditorWidget::TerrainMapNext);
      text({selector.x + 48, selector.y, selector.width - 96, 48},
           roles[std::min(s.terrainMapRole, 6u)], theme.color.text, false,
           UiAlign::Center);
      }
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
