#include "editor/editor_screen.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
#include <cstdio>

namespace ae::editor {
using namespace ui;
namespace {
UiRect take(UiRect &r, float h) {
  h = std::min(h, r.height);
  UiRect out{r.x, r.y, r.width, h};
  r.y += h;
  r.height -= h;
  return out;
}
std::string num(double n) {
  char text[40];
  std::snprintf(text, sizeof(text), "%.5g", n);
  return text;
}
} // namespace
void buildTerrainSurface(const EditorScreenState &s, const UiTheme &theme,
                         UiDrawList &list, UiInputRouter &router,
                         const UiRect &area) {
  list.addRect(area, theme.color.surface);
  router.addBlocker(area);
  auto body = deflate(area, UiInsets::all(6));
  const auto label = [&](UiRect r, std::string_view text, UiColor color) {
    if (r.width <= 0 || r.height <= 0)
      return;
    list.pushClip(r);
    list.addText(deflate(r, UiInsets{7, 0, 7, 0}), text, color,
                 theme.type.caption);
    list.popClip();
  };
  const auto button = [&](UiRect r, std::string_view text, EditorWidget code,
                          bool active = false, bool enabled = true) {
    if (r.width <= 0 || r.height <= 0)
      return;
    auto box = deflate(r, UiInsets::all(2));
    const bool primary = code == EditorWidget::TerrainGenerate && enabled;
    list.addRect(box,
                 primary  ? theme.color.accent
                 : active ? withAlpha(theme.color.accent, .22f)
                          : theme.color.raised,
                 3);
    label(box, text,
          primary   ? theme.color.surface
          : enabled ? theme.color.text
                    : theme.color.textFaint);
    if (enabled)
      router.addRegion(r, widgetId(code));
  };
  const auto pair = [&](UiRect row, std::string_view a, EditorWidget ac,
                        std::string_view b, EditorWidget bc) {
    UiRect left = row;
    left.width *= .5f;
    row.x += left.width;
    row.width -= left.width;
    button(left, a, ac, false, !s.terrainBusy);
    button(row, b, bc, false, !s.terrainBusy);
  };
  auto title = take(body, 32);
  auto icon = UiRect{title.x + 4, title.y + 6, 24, 24};
  const UiIcon tabIcons[]{
      s.terrainDraft.mode == resources::TerrainMode::Heightfield
          ? UiIcon::TerrainTerrain
          : UiIcon::TerrainVoxel,
      UiIcon::TerrainSculpt, UiIcon::TerrainLayers, UiIcon::TerrainTerrain};
  list.addImage(icon,
                static_cast<UiImageId>(tabIcons[std::min(s.terrainTab, 3u)]));
  title.x += 34;
  title.width -= 74;
  label(title, "TERRENO / VOXEL", theme.color.accent);
  auto close = UiRect{area.right() - 42, area.y + 6, 36, 34};
  button(close, "×", EditorWidget::TerrainClose);
  auto tabs = take(body, 32);
  const char *names[]{"Gerar", "Esculpir", "Camadas", "Gerir"};
  for (u32 i = 0; i < 4; ++i)
    button({tabs.x + tabs.width * float(i) / 4, tabs.y, tabs.width / 4,
            tabs.height},
           names[i], EditorWidget(widgetId(EditorWidget::TerrainTabBase) + i),
           s.terrainTab == i, !s.terrainBusy);
  auto footer = UiRect{body.x, std::max(body.y, body.bottom() - 78), body.width,
                       std::min(78.f, body.height)};
  body.height = std::max(0.f, body.height - footer.height);
  const u32 totals[]{12, 8, 6, 4};
  const u32 rows = std::max(1u, u32(std::max(0.f, body.height) / 32));
  const u32 pages = (totals[std::min(s.terrainTab, 3u)] + rows - 1) / rows;
  const u32 current = s.terrainPage % pages;
  u32 cursor = 0;
  const auto row = [&]() -> UiRect {
    const u32 index = cursor++;
    if (index / rows != current)
      return {};
    return take(body, 32);
  };
  const auto fieldAt = [&](UiRect line, const char *name, double value,
                           u32 index) {
    button(line, std::string(name) + "  " + num(value),
           EditorWidget(widgetId(EditorWidget::TerrainNumberBase) + index),
           false, !s.terrainBusy);
  };
  const auto field = [&](const char *name, double value, u32 index) {
    fieldAt(row(), name, value, index);
  };
  const auto &r = s.terrainDraft;
  const auto &b = s.terrainBrush;
  if (s.terrainBusy) {
    label(take(body, 38),
          "Gerando · " + std::to_string(int(s.terrainProgress * 100)) + "%",
          theme.color.text);
    auto bar = take(body, 6);
    list.addRect(bar, theme.color.raised);
    bar.width *= s.terrainProgress;
    list.addRect(bar, theme.color.accent);
    button(take(body, 40), "Cancelar geração", EditorWidget::TerrainCancel);
  } else if (s.terrainTab == 0) {
    const char *modes[]{"Altura / relevo", "Voxel suave / cavernas",
                        "Voxel em blocos"};
    button(row(), modes[u32(r.mode)], EditorWidget::TerrainMode, true);
    field("Seed", r.seed, 0);
    field("Lado (m)", r.size, 1);
    field("Células (8–128, passo 8)", r.cells, 2);
    field("Amplitude (m)", r.height, 3);
    field("Base (m)", r.base, 4);
    field("Frequência", r.frequency, 5);
    field("Oitavas", r.octaves, 6);
    field("Células verticais", r.verticalCells, 7);
    field("Textura a cada (m)", r.textureScale, 8);
    button(row(), r.island ? "Ilha: sim" : "Ilha: não",
           EditorWidget::TerrainIsland, r.island);
    button(row(), r.collision ? "Colisão: sim" : "Colisão: não",
           EditorWidget::TerrainCollision, r.collision);
  } else if (s.terrainTab == 1) {
    const char *tools[]{"Elevar",  "Baixar",    "Nivelar", "Suavizar",
                        "Escavar", "Adicionar", "Pintar",  "Caminho"};
    for (u32 j = 0; j < 4; ++j) {
      auto line = row();
      for (u32 col = 0; col < 2; ++col) {
        u32 i = j * 2 + col;
        bool enabled =
            r.mode != resources::TerrainMode::Heightfield || (i != 4 && i != 5);
        button({line.x + line.width * float(col) * .5f, line.y,
                line.width * .5f, line.height},
               tools[i],
               EditorWidget(widgetId(EditorWidget::TerrainToolBase) + i),
               u32(b.tool) == i, enabled);
      }
    }
    auto params = row();
    fieldAt({params.x, params.y, params.width / 2, params.height}, "Raio",
            b.radius, 9);
    fieldAt({params.x + params.width / 2, params.y, params.width / 2,
             params.height},
            "Força", b.strength, 10);
    button(row(),
           s.terrainBrushArmed ? "Toque no terreno · ativo"
                               : "Pincel no viewport",
           EditorWidget::TerrainBrushArm, s.terrainBrushArmed,
           s.terrainTarget != 0);
    auto xyz = row();
    fieldAt({xyz.x, xyz.y, xyz.width / 3, xyz.height}, "X", b.x, 11);
    fieldAt({xyz.x + xyz.width / 3, xyz.y, xyz.width / 3, xyz.height}, "Y", b.y,
            12);
    fieldAt({xyz.x + xyz.width * 2 / 3, xyz.y, xyz.width / 3, xyz.height}, "Z",
            b.z, 13);
    pair(row(), "Aplicar pincel", EditorWidget::TerrainStamp, "Remover última",
         EditorWidget::TerrainRemoveStroke);
  } else if (s.terrainTab == 2) {
    const char *layers[]{"Grama", "Rocha", "Terra", "Caminho"};
    for (u32 i = 0; i < 4; ++i)
      button(row(), layers[i],
             EditorWidget(widgetId(EditorWidget::TerrainLayerBase) + i),
             b.layer == i);
    field("Textura a cada (m)", r.textureScale, 8);
    button(row(), "Pincel no viewport", EditorWidget::TerrainBrushArm,
           s.terrainBrushArmed, s.terrainTarget != 0);
  } else {
    pair(row(), "Novo", EditorWidget::TerrainNew, "Abrir seleção",
         EditorWidget::TerrainLoad);
    pair(row(), "Enquadrar", EditorWidget::TerrainFrame, "Desfazer",
         EditorWidget::Undo);
    button(row(),
           r.collision ? "Colisão triangular ativa" : "Colisão desligada",
           EditorWidget::TerrainCollision, r.collision);
    label(row(), std::to_string(r.strokes.size()) + " / 256 operações",
          theme.color.textMuted);
  }
  auto page = take(footer, 28);
  auto prev = page;
  prev.width = 32;
  button(prev, "‹", EditorWidget::TerrainPrevious, false,
         !s.terrainBusy && s.terrainPage > 0);
  auto next = page;
  next.x = page.right() - 32;
  next.width = 32;
  button(next, "›", EditorWidget::TerrainNext, false,
         !s.terrainBusy && pages > 1);
  page.x += 34;
  page.width -= 68;
  label(page,
        "Ajustes " + std::to_string(current + 1) + " / " +
            std::to_string(pages),
        theme.color.textMuted);
  button(take(footer, 32), s.terrainBusy ? "Preparando…" : "Gerar e aplicar",
         EditorWidget::TerrainGenerate, true, !s.terrainBusy);
  label(footer, s.terrainMessage, theme.color.textMuted);
}
} // namespace ae::editor
