#include "editor/editor_screen.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
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
bool compact(const UiRect &a) { return a.height < 400; }
float contentHeight(const EditorScreenState &s) {
  bool hf = s.terrainDraft.mode == resources::TerrainMode::Heightfield;
  if (s.terrainTab == 0) {
    if (s.terrainSection == 0)
      return hf ? 354.f : 298.f;
    if (s.terrainSection == 1)
      return 410.f;
    return hf ? 244.f : 308.f;
  }
  if (s.terrainTab == 1)
    return s.terrainSection == 0 ? 680.f : 590.f;
  return s.terrainTab == 2 ? 480.f : 488.f;
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
  if (s.terrainTab < 2)
    take(body, compact(area) ? 28 : 40);
  body.height = std::max(0.f, body.height - (compact(area) ? 64 : 88));
  return body;
}
float terrainScrollLimit(const EditorScreenState &s, const UiRect &area) {
  return std::max(0.f, contentHeight(s) - terrainContentRect(s, area).height);
}
void buildTerrainViewport(const EditorScreenState &s,const UiTheme &theme,UiDrawList &list,UiInputRouter &router,const UiRect &area) {
  if(!s.view || area.isEmpty())return;
  list.pushClip(area);
  if(s.terrainBrushArmed && s.terrainCursorVisible) {
    for(usize i=0;i<s.terrainCursorLines.size();++i) {
      const auto &line=s.terrainCursorLines[i];UiPoint a,b;
      if(projectSegmentToScreen(*s.view,line.data(),line.data()+3,a,b)) {
        list.addLine(a,b,0xb0000000,4.f);
        list.addLine(a,b,i<48?theme.color.accent:theme.color.text,1.7f);
      }
    }
    const auto p=projectWorldToScreen(*s.view,s.terrainCursor);
    if(p.valid) {
      list.addLine({p.screen.x-7,p.screen.y},{p.screen.x+7,p.screen.y},theme.color.text,2);
      list.addLine({p.screen.x,p.screen.y-7},{p.screen.x,p.screen.y+7},theme.color.text,2);
      if(s.terrainTouchOffset)list.addLine(s.terrainPointer,p.screen,theme.color.textMuted,1);
    }
  }
  const char *tools[]{"Elevar","Baixar","Nivelar","Suavizar","Escavar","Adicionar","Pintar","Caminho","Ruído","Terraços","Restaurar","Aguçar","Buraco"};
  const std::string status=s.terrainBrushArmed?std::string(tools[std::min(12u,u32(s.terrainBrush.tool))])+
    (s.terrainBusy?" · confirmando":s.terrainLivePreview?" · prévia ao vivo":" · toque e arraste"):"Navegar · terreno protegido";
  UiRect chip{area.x+12,area.y+52,std::min(285.f,area.width-24),32};
  list.addRect(chip,theme.color.surface,6);
  auto type=theme.type.caption;type.size=12;
  list.addText(deflate(chip,UiInsets::all(8)),status,theme.color.text,type);
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
    style.size = small ? 11.f : 13.f;
    list.addText(deflate(box, UiInsets{8, 0, 8, 0}), value, color, style,
                 align);
    list.popClip();
  };
  const auto button = [&](UiRect rect, std::string_view value, EditorWidget id,
                          bool selected = false, bool enabled = true,
                          UiIcon icon = UiIcon::None) {
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
    if (icon != UiIcon::None && box.width > 98) {
      list.addImage({box.x + 9, box.y + (box.height - 22) * .5f, 22, 22},
                    static_cast<UiImageId>(icon), ink);
      label.x += 33;
      label.width -= 33;
    }
    text(label, value, ink, false, UiAlign::Center);
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
  if (compact(area))
    text(name, s.terrainTarget ? "Editar terreno" : "Novo terreno",
         theme.color.text);
  else {
    text(take(name, 28), "Terreno e voxel", theme.color.text);
    const auto *e = s.document ? s.document->find(s.terrainTarget) : nullptr;
    text(name, e ? e->name : "Novo terreno · configure e crie",
         theme.color.textMuted, true);
  }
  const char *tabs[]{"Forma", "Esculpir", "Pintar", "Gerir"};
  const UiIcon icons[]{UiIcon::TerrainTerrain, UiIcon::TerrainSculpt,
                       UiIcon::TerrainLayers, UiIcon::TerrainManage};
  auto tabrow = take(outer, compact(area) ? 36 : 48);
  for (u32 i = 0; i < 4; ++i)
    button({tabrow.x + tabrow.width * i / 4, tabrow.y, tabrow.width / 4,
            tabrow.height},
           tabs[i], EditorWidget(widgetId(EditorWidget::TerrainTabBase) + i),
           s.terrainTab == i, !s.terrainBusy, icons[i]);
  if (s.terrainTab < 2) {
    auto sections = take(outer, compact(area) ? 28 : 40);
    const char *form[]{"Estrutura", "Relevo", "Detalhes"};
    const char *brush[]{"Ferramentas", "Precisão"};
    u32 count = s.terrainTab == 0 ? 3 : 2;
    for (u32 i = 0; i < count; ++i)
      button({sections.x + sections.width * i / count, sections.y,
              sections.width / count, sections.height},
             s.terrainTab == 0 ? form[i] : brush[i],
             EditorWidget(widgetId(EditorWidget::TerrainSectionBase) + i),
             s.terrainSection == i, !s.terrainBusy);
  }
  auto window = terrainContentRect(s, area), content = window;
  float scroll = s.terrainBusy?0:std::clamp(s.terrainScroll, 0.f, terrainScrollLimit(s, area));
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
                          double bv, u32 bi) {
    auto line = row(64);
    fieldAt({line.x, line.y, line.width / 2, line.height}, a, av, ai);
    fieldAt({line.x + line.width / 2, line.y, line.width / 2, line.height}, b,
            bv, bi);
  };
  const auto pair = [&](const char *a, EditorWidget ai, const char *b,
                        EditorWidget bi, bool en = true) {
    auto line = row();
    button({line.x, line.y, line.width / 2, line.height}, a, ai, false,
           en && !s.terrainBusy);
    button({line.x + line.width / 2, line.y, line.width / 2, line.height}, b,
           bi, false, en && !s.terrainBusy);
  };
  const auto slider = [&](const char *name,float value,u32 index,float low,float high) {
    auto line=row(64);text(take(line,24),name,theme.color.textMuted,true);
    auto valueBox=UiRect{line.right()-76,line.y,76,36};
    button(valueBox,num(value),EditorWidget(widgetId(EditorWidget::TerrainNumberBase)+index));
    UiRect bar{line.x+8,line.y+15,line.width-96,6};
    list.addRect(bar,theme.color.raised,3);
    float t=std::clamp((value-low)/(high-low),0.f,1.f);
    list.addRect({bar.x,bar.y,bar.width*t,bar.height},theme.color.accent,3);
    list.addRect({bar.x+bar.width*t-7,bar.y-5,14,16},theme.color.text,7);
    router.addRegion({bar.x-4,line.y,bar.width+8,36},widgetId(EditorWidget::TerrainSliderBase)+index);
  };
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
    if (!hf)
      field("Resolução vertical (8–64)", r.verticalCells, 7);
    button(row(),
           r.collision ? "Colisão triangular: ligada"
                       : "Colisão triangular: desligada",
           EditorWidget::TerrainCollision, r.collision);
    const u64 samples =
        u64(r.cells + 1) * (r.cells + 1) * (hf ? 1 : r.verticalCells + 1);
    hint(std::to_string(samples) + " / 600.000 amostras",
         "Limite: 200.000 triângulos por terreno.");
    hint("Aplique para atualizar geometria e colisão.");
  } else if (s.terrainTab == 1 && s.terrainSection == 0) {
    const char *tools[]{"Elevar","Baixar","Nivelar","Suavizar","Escavar","Adicionar","Pintar","Caminho","Ruído","Terraços","Restaurar","Aguçar","Buraco"};
    const u32 order[]{0,1,2,3,4,5,7,8,9,10,11,12};
    const UiIcon marks[]{UiIcon::TerrainRaise,UiIcon::TerrainLower,UiIcon::TerrainFlatten,UiIcon::TerrainSmooth,
      UiIcon::TerrainDig,UiIcon::TerrainVoxel,UiIcon::TerrainPaint,UiIcon::TerrainPath,UiIcon::TerrainNoise,
      UiIcon::TerrainTerrace,UiIcon::TerrainRestore,UiIcon::TerrainSharpen,UiIcon::TerrainHole};
    for(u32 j=0;j<4;++j) {
      auto line=row(52);
      for(u32 col=0;col<3;++col) {
        const u32 i=order[j*3+col];
        button({line.x+line.width*col/3,line.y,line.width/3,line.height},tools[i],
          EditorWidget(widgetId(EditorWidget::TerrainToolBase)+i),u32(b.tool)==i,
          hf?(i!=4&&i!=5):i!=12,marks[i]);
      }
    }
    label("FORMA E INFLUÊNCIA");
    const bool volumeBrush=!hf && (b.tool==resources::TerrainTool::Dig || b.tool==resources::TerrainTool::Add ||
      b.tool==resources::TerrainTool::Smooth || b.tool==resources::TerrainTool::Sharpen ||
      b.tool==resources::TerrainTool::Restore || b.tool==resources::TerrainTool::Paint);
    auto shapes=row(44);const char *shape[]{"Circular","Caixa","Meia","Ponta"};
    for(u32 i=0;i<4;++i)button({shapes.x+shapes.width*i/4,shapes.y,shapes.width/4,shapes.height},
      shape[i],EditorWidget(widgetId(EditorWidget::TerrainShapeBase)+i),u32(b.shape)==i,volumeBrush||i<2);
    slider("Raio (m)",b.radius,9,.1f,std::max(8.f,r.size*.25f));
    slider(b.tool==resources::TerrainTool::Terrace?"Altura do degrau (m)":"Força",b.strength,10,.05f,8.f);
    slider("Núcleo rígido · borda suave",b.hardness,15,0,1);
    if(b.tool==resources::TerrainTool::Flatten || b.tool==resources::TerrainTool::Path)
      field("Altura desejada Y (m)",b.y,12);
    fields("Rotação (°)",b.rotation,16,"Proporção X/Z",b.aspect,17);
    hint("Selecione a ferramenta e arraste no terreno.","Cada gesto confirmado tem um único Desfazer.");
  } else if (s.terrainTab == 1) {
    label("POSIÇÃO LOCAL DO PINCEL");
    fields("X (m)", b.x, 11, "Z (m)", b.z, 13);
    field("Altura Y / nível desejado (m)", b.y, 12);
    fields("Raio (m)", b.radius, 9, "Força", b.strength, 10);
    button(row(), "Aplicar na posição XYZ", EditorWidget::TerrainStamp, false,
           s.terrainTarget != 0);
    button(row(), "Remover última amostra", EditorWidget::TerrainRemoveStroke,
           false, !r.strokes.empty());
    fields("Rotação (°)",b.rotation,16,"Proporção X/Z",b.aspect,17);
    field("Espaçamento · fração do raio",s.terrainSpacing,18);
    button(row(),s.terrainTouchOffset?"Alvo acima do dedo: ligado":"Alvo acima do dedo: desligado",EditorWidget::TerrainTouchOffset,s.terrainTouchOffset);
    hint("XYZ permite alcançar o interior do voxel.");
  } else if (s.terrainTab == 2) {
    label("CAMADA DO PINCEL");
    const char *layers[]{"Grama", "Rocha", "Terra", "Caminho"};
    const UiIcon samples[]{UiIcon::TerrainSampleGrass,UiIcon::TerrainSampleRock,UiIcon::TerrainSampleSoil,UiIcon::TerrainSamplePath};
    for (u32 j = 0; j < 2; ++j) {
      auto line = row(72);
      for (u32 col = 0; col < 2; ++col) {
        u32 i = j * 2 + col;
        UiRect tile{line.x + line.width * col / 2, line.y, line.width / 2,
                    line.height};
        button(tile, "",
               EditorWidget(widgetId(EditorWidget::TerrainLayerBase) + i),
               b.layer == i);
        list.addImage({tile.x+8,tile.y+12,42,42},static_cast<UiImageId>(samples[i]));
        text({tile.x+52,tile.y,tile.width-56,tile.height},layers[i],theme.color.text);
      }
    }
    field("Repetição da textura (m)", r.textureScale, 8);
    fields("Raio (m)", b.radius, 9, "Força", b.strength, 10);
    button(row(),r.blendedSurface?"Mistura e triplanar: ligados":"Materiais por região · legado",EditorWidget::TerrainBlend,r.blendedSurface);
    slider("Variação de escala e cor",r.macroVariation,19,0,1);
    hint("Pesos suaves · projeção em três eixos.","Aplicar atualiza a configuração do material.");
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
  auto action=take(footer,compact(area)?44:48);
  if(!s.terrainBusy && s.terrainTarget && !pending && (s.terrainTab==1 || s.terrainTab==2)) {
    button({action.x,action.y,action.width*.5f,action.height},s.terrainTab==2?"Pintar":"Esculpir",EditorWidget::TerrainSculpt,s.terrainBrushArmed);
    button({action.x+action.width*.5f,action.y,action.width*.5f,action.height},"Navegar",EditorWidget::TerrainNavigate,!s.terrainBrushArmed);
  } else if(!s.terrainBusy && !pending) button(action,"Salvar cena",EditorWidget::TerrainSave);
  else button(action,s.terrainBusy?"Cancelar":s.terrainTarget?"Aplicar configuração":"Criar terreno",
    s.terrainBusy?EditorWidget::TerrainCancel:EditorWidget::TerrainGenerate,true,s.terrainBusy||pending);
  if (!compact(area))
    text(footer, s.terrainMessage, theme.color.textMuted, true);
}
} // namespace ae::editor
