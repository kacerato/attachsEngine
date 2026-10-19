#include "editor/editor_screen.h"
#include "editor/editor_map_scene.h"
#include <cmath>
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
  constexpr float padding=12;
  const bool hf=s.terrainDraft.mode==resources::TerrainMode::Heightfield;
  if(s.terrainBusy)return 182+padding;
  if(s.terrainTab==0) {
    if(s.terrainSection==0)return (hf?348.f:298.f)+padding;
    if(s.terrainSection==1)return 410+padding;
    return (hf?364.f:428.f)+padding;
  }
  if(s.terrainTab==4) {
    if(s.terrainDraft.paths.empty())return 104+padding;
    return (s.terrainSection==0?612.f:s.terrainSection==1?874.f:602.f)+(s.terrainSection && compact(terrainPanelRect(s))?-50.f:0.f)+padding;
  }
  if(s.terrainTab==1) {
    const auto &brush=s.terrainBrush;
    if(s.terrainSection==0) {
      float extent=650;
      if(brush.tool==resources::TerrainTool::Flatten || brush.tool==resources::TerrainTool::Path)extent+=64;
      if(brush.tool==resources::TerrainTool::Dig || brush.tool==resources::TerrainTool::Add || brush.tool==resources::TerrainTool::Paint)extent+=56;
      if(brush.tool==resources::TerrainTool::Restore)extent+=112;
      return extent+padding;
    }
    if(s.terrainSection==1)return 746+padding;
    float extent=508;
    if(brush.shape==resources::TerrainBrushShape::RoundedBox)extent+=96;
    if(brush.shape==resources::TerrainBrushShape::Hemisphere || brush.shape==resources::TerrainBrushShape::Cone)extent+=56;
    return extent+padding;
  }
  return (s.terrainTab==2?586.f:450.f)+padding;
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
  if (s.terrainTab < 2 || s.terrainTab==4)
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
  if(s.terrainTab==4 && s.document && s.terrainPath<s.terrainDraft.paths.size()) {
    const auto &path=s.terrainDraft.paths[s.terrainPath];float world[16];
    if(s.terrainPreviewBuild && editorWorldMatrix(*s.document,s.terrainTarget,world)) {
      const auto center=resources::terrainPathPolyline(path,s.terrainDraft.size/s.terrainDraft.cells*.5f);
      const auto localToWorld=[&](std::array<float,3> p){
        if(s.terrainPreviewBuild)p[1]=resources::terrainFieldHeight(s.terrainPreviewBuild->field,p[0],p[2])+.05f;
        std::array<float,3> result{};
        for(u32 k=0;k<3;++k)result[k]=world[12+k]+world[k]*p[0]+world[4+k]*p[1]+world[8+k]*p[2];
        return result;
      };
      const auto segment=[&](std::array<float,3> a,std::array<float,3> b,UiColor color,float width){
        const auto aw=localToWorld(a),bw=localToWorld(b);UiPoint from,to;
        if(projectSegmentToScreen(*s.view,aw.data(),bw.data(),from,to))list.addLine(from,to,color,width);
      };
      for(usize i=1;i<center.size();++i)segment(center[i-1],center[i],theme.color.accent,1.4f);
      // Lane and shoulder outlines are expressed in terrain units, projected
      // onto the current preview, and use the actual retained curve samples.
      for(float width:{path.width,path.shoulderWidth}) {
        std::array<float,3> oldLeft{},oldRight{};
        const auto color=width==path.width?theme.color.accent:theme.color.textMuted;
        for(usize i=0;i<center.size();++i) {
          const auto a=center[i?i-1:0],b=center[std::min(i+1,center.size()-1)];
          const float dx=b[0]-a[0],dz=b[2]-a[2],length=std::max(.0001f,std::sqrt(dx*dx+dz*dz));
          auto left=center[i],right=center[i];
          left[0]-=dz/length*width*.5f;left[2]+=dx/length*width*.5f;
          right[0]+=dz/length*width*.5f;right[2]-=dx/length*width*.5f;
          if(i){segment(oldLeft,left,color,1.5f);segment(oldRight,right,color,1.5f);}
          if(i==0 || i+1==center.size()) {
            if(path.cap==resources::TerrainPathCap::Round) {
              auto previous=left;const float direction=i==0?-1.f:1.f;
              for(u32 j=1;j<=24;++j) {
                const float angle=j*3.14159265f/24;auto next=center[i];
                next[0]+=(-dz*std::cos(angle)+direction*dx*std::sin(angle))/length*width*.5f;
                next[2]+=(dx*std::cos(angle)+direction*dz*std::sin(angle))/length*width*.5f;
                segment(previous,next,color,1.5f);previous=next;
              }
            } else if(path.cap==resources::TerrainPathCap::Square) {
              const float extension=(i==0?-1.f:1.f)*path.width*.5f;
              auto capLeft=left,capRight=right;
              capLeft[0]+=dx/length*extension;capLeft[2]+=dz/length*extension;
              capRight[0]+=dx/length*extension;capRight[2]+=dz/length*extension;
              segment(left,capLeft,color,1.5f);segment(capLeft,capRight,color,1.5f);segment(capRight,right,color,1.5f);
            } else segment(left,right,color,1.5f);
          }
          oldLeft=left;oldRight=right;
        }
      }
      for(u32 i=0;i<path.points.size();++i) {
        const auto &point=path.points[i];
        const auto at=localToWorld({point.x,point.y,point.z});
        const auto p=projectWorldToScreen(*s.view,at.data());if(!p.valid)continue;
        const bool selected=i==s.terrainPathPoint;const float radius=selected?9:6;
        list.addRect({p.screen.x-radius-2,p.screen.y-radius-2,2*radius+4,2*radius+4},theme.color.text,radius+2);
        list.addRect({p.screen.x-radius,p.screen.y-radius,2*radius,2*radius},selected?theme.color.accent:theme.color.surface,radius);
        auto style=theme.type.caption;style.size=12;
        list.addText({p.screen.x+12,p.screen.y-20,56,20},std::to_string(i+1),theme.color.text,style);
        if(!s.terrainBusy)router.addRegion({p.screen.x-22,p.screen.y-22,44,44},widgetId(EditorWidget::TerrainPathNew)+0x100+i);
      }
    }
  }
  const char *tools[]{"Elevar","Baixar","Nivelar","Suavizar","Escavar","Adicionar","Pintar","Caminho","Ruído","Terraços","Restaurar","Aguçar","Buraco"};
  const std::string status=s.terrainBrushArmed?std::string(tools[std::min(12u,u32(s.terrainBrush.tool))])+
    (s.terrainBusy?" · confirmando":s.terrainLivePreview?" · prévia ao vivo":" · toque e arraste"):s.terrainTab==4?"Traçado · arraste os pontos":"Navegar · terreno protegido";
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
  const char *tabs[]{"Forma", "Pincel", "Pintar", "Gerir", "Traçado"};
  const u32 tabOrder[]{0,1,2,4,3};
  const UiIcon icons[]{UiIcon::TerrainTerrain, UiIcon::TerrainSculpt,
                       UiIcon::TerrainLayers, UiIcon::TerrainManage, UiIcon::TerrainPath};
  auto tabrow = take(outer, compact(area) ? 36 : 48);
  for(u32 col=0;col<5;++col) {
    const u32 i=tabOrder[col];
    button({tabrow.x+tabrow.width*col/5,tabrow.y,tabrow.width/5,tabrow.height},tabs[i],
      EditorWidget(widgetId(EditorWidget::TerrainTabBase)+i),s.terrainTab==i,!s.terrainBusy,icons[i]);
  }
  if (s.terrainTab < 2 || s.terrainTab==4) {
    auto sections = take(outer, compact(area) ? 28 : 40);
    const char *form[]{"Estrutura", "Relevo", "Detalhes"};
    const char *brush[]{"Ferramentas", "Precisão", "Forma 3D"};
    const char *paths[]{"Traçado", "Perfil", "Materiais"};
    u32 count = 3;
    for (u32 i = 0; i < count; ++i)
      button({sections.x + sections.width * i / count, sections.y,
              sections.width / count, sections.height},
             s.terrainTab == 0 ? form[i] : s.terrainTab==4?paths[i]:brush[i],
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
    label("DETALHE À DISTÂNCIA");
    const auto lodRow=row(64);
    fieldAt({lodRow.x,lodRow.y,lodRow.width*.5f,lodRow.height},"Níveis · 1–4",r.lodLevels,28);
    fieldAt({lodRow.x+lodRow.width*.5f,lodRow.y,lodRow.width*.5f,lodRow.height},
            "Erro máx. (%)",r.lodRelativeError*100,29,r.lodLevels>1);
    hint("Bordas preservadas · colisão no original.",
         "Distância segue o erro em pixels de Qualidade.");
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
    auto shapes=row(48);const char *shape[]{"Elipse","Caixa","Meia","Ponta","Arred."};
    for(u32 i=0;i<5;++i)button({shapes.x+shapes.width*i/5,shapes.y,shapes.width/5,shapes.height},
      shape[i],EditorWidget(widgetId(EditorWidget::TerrainShapeBase)+i),u32(b.shape)==i,volumeBrush||i<2||i==4);
    slider("Raio (m)",b.radius,9,.1f,std::max(8.f,r.size*.25f));
    slider(b.tool==resources::TerrainTool::Terrace?"Altura do degrau (m)":"Força",b.strength,10,.05f,8.f);
    slider("Núcleo rígido · borda suave",b.hardness,15,0,1);
    if(b.tool==resources::TerrainTool::Flatten || b.tool==resources::TerrainTool::Path)
      field("Altura desejada Y (m)",b.y,12);
    fields("Rotação (°)",b.rotation,16,"Proporção X/Z",b.aspect,17);
    field("Opacidade (0–1)",b.opacity,25);
    if(b.tool==resources::TerrainTool::Dig || b.tool==resources::TerrainTool::Add || b.tool==resources::TerrainTool::Paint)button(row(),b.application==resources::TerrainBrushApplication::Target?"Aplicação: valor alvo":"Aplicação: acumulativa",EditorWidget::TerrainOpacityTarget,b.application==resources::TerrainBrushApplication::Target);
    if(b.tool==resources::TerrainTool::Restore) {
      button(row(),b.restoreGeometry?"Restaurar geometria: sim":"Restaurar geometria: não",EditorWidget::TerrainRestoreGeometry,b.restoreGeometry);
      button(row(),b.restoreWeights?"Restaurar materiais: sim":"Restaurar materiais: não",EditorWidget::TerrainRestoreWeights,b.restoreWeights);
    }
    hint("Selecione a ferramenta e arraste no terreno.","Cada gesto confirmado tem um único Desfazer.");
  } else if (s.terrainTab == 1 && s.terrainSection == 1) {
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
    field("Profundidade a partir da superfície (m)",b.surfaceDepth,27);
    button(row(),b.lockHeight?"Manter altura do gesto: sim":"Acompanhar altura da superfície",EditorWidget::TerrainHeightLock,b.lockHeight);
    field("Vizinhança do filtro (1–4 células)",b.filterRadius,26);
    hint("XYZ permite alcançar o interior do voxel.","Profundidade positiva entra na superfície.");
  } else if(s.terrainTab==1) {
    label("DIMENSÕES LOCAIS DO PINCEL");
    field("Raio de referência (m)",b.radius,9);
    button(row(),b.dimensionsLinked?"Vincular dimensões: sim":"Dimensões independentes",EditorWidget::TerrainDimensionsLinked,b.dimensionsLinked,true,UiIcon::TerrainBrushAxes);
    field("Proporção X",b.aspect,17);
    fields("Proporção Y",b.verticalAspect,20,"Proporção Z",b.depthAspect,21);
    hint("Tamanho: "+num(2*b.radius*b.aspect)+" × "+num(2*b.radius*b.verticalAspect)+" × "+num(2*b.radius*b.depthAspect)+" m");
    label("ORIENTAÇÃO");
    field("Rotação Y (°)",b.rotation,16);
    fields("Inclinação X (°)",b.pitch,22,"Inclinação Z (°)",b.roll,23);
    if(b.shape==resources::TerrainBrushShape::RoundedBox){auto badge=row(32);list.addImage({badge.x+8,badge.y+4,24,24},static_cast<UiImageId>(UiIcon::TerrainRoundedBrush),theme.color.text);text({badge.x+36,badge.y,badge.width-36,badge.height},"Cubo arredondado",theme.color.text);field("Arredondamento dos cantos (0–1)",b.roundness,24);}
    if(b.shape==resources::TerrainBrushShape::Hemisphere || b.shape==resources::TerrainBrushShape::Cone)
      button(row(),b.inverted?"Direção: para baixo":"Direção: para cima",EditorWidget::TerrainBrushInvert,b.inverted);
    hint("O contorno usa a mesma forma da edição.",hf?"Inclinações X/Z são usadas no volume.":"Rotação aplicada nos eixos locais do terreno.");
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
    field("Opacidade da camada (0–1)",b.opacity,25);
    button(row(),b.application==resources::TerrainBrushApplication::Target?"Pintura: opacidade alvo":"Pintura: acumulativa",EditorWidget::TerrainOpacityTarget,b.application==resources::TerrainBrushApplication::Target);
    button(row(),r.blendedSurface?"Mistura e triplanar: ligados":"Materiais por região · legado",EditorWidget::TerrainBlend,r.blendedSurface);
    slider("Variação de escala e cor",r.macroVariation,19,0,1);
    hint("Pesos suaves · projeção em três eixos.","Aplicar atualiza a configuração do material.");
  } else if(s.terrainTab==4) {
    if(r.paths.empty()) {
      button(row(),"Novo caminho",EditorWidget::TerrainPathNew);
      hint("Crie um caminho para editar pontos e perfis.","A prévia aparece antes de aplicar.");
    } else {
      const auto &path=r.paths[std::min(s.terrainPath,u32(r.paths.size()-1))];
      auto selector=row(44);
      button({selector.x,selector.y,36,44},"<",EditorWidget::TerrainPathPrevious,false,r.paths.size()>1);
      button({selector.right()-80,selector.y,36,44},">",EditorWidget::TerrainPathNext,false,r.paths.size()>1);
      button({selector.right()-44,selector.y,44,44},"+",EditorWidget::TerrainPathNew,false,r.paths.size()<64);
      text({selector.x+36,selector.y,selector.width-116,44},path.name+" · "+std::to_string(s.terrainPathPoint+1)+"/"+std::to_string(path.points.size()),theme.color.text,true);
      if(s.terrainSection==0) {
        if(s.terrainPathPoint<path.points.size()) {
          const auto &point=path.points[s.terrainPathPoint];
          fields("X (m)",point.x,44,"Z (m)",point.z,46);
          button(row(),path.followTerrain?"Altura: seguir superfície":"Altura: pontos XYZ",EditorWidget::TerrainPathFollowTerrain,path.followTerrain,!path.useDefinedHeight);
          field("Altura Y do ponto (m)",point.y,45,!path.useDefinedHeight && (!path.followTerrain || (path.uniformRamp>0 && (s.terrainPathPoint==0 || s.terrainPathPoint+1==path.points.size()))));
          button(row(),point.curved?"Próximo trecho: curvo":"Próximo trecho: reto",EditorWidget::TerrainPointCurve,point.curved,true,UiIcon::TerrainPathPoints);
        }
        pair("Ponto anterior",EditorWidget::TerrainPointPrevious,"Próximo ponto",EditorWidget::TerrainPointNext);
        pair("Inserir ponto",EditorWidget::TerrainPointInsert,"Remover ponto",EditorWidget::TerrainPointRemove);
        const char *caps[]{"Extremidades: redondas","Extremidades: retas","Extremidades: quadradas"};
        button(row(),caps[u32(path.cap)],EditorWidget::TerrainPathCap);
        button(row(),path.enabled?"Caminho ativo":"Caminho desativado",EditorWidget::TerrainPathEnabled,path.enabled);
        pair("Duplicar caminho",EditorWidget::TerrainPathDuplicate,"Excluir caminho",EditorWidget::TerrainPathDelete);
        hint("Editar mantém o traçado original recuperável.","Aplicar confirma; Reverter cancela o rascunho.");
      } else {
        const auto &curve=s.terrainSection==2?path.textureProfile:path.profile;
        const u32 selected=std::min(s.terrainProfilePoint,u32(curve.size()-1));
        if(!compact(area))label(s.terrainSection==2?"CURVA DE PINTURA":"CURVA DO ACOSTAMENTO");
        auto graph=deflate(row(compact(area)?64:88),UiInsets::all(8));list.addRect(graph,theme.color.raised,5);
        const auto normalizedY=[&](float y){return s.terrainSection==2?y:(y+1)/3;};
        if(s.terrainSection==1)for(float level:{0.f,1.f}){
          const float y=graph.bottom()-8-normalizedY(level)*(graph.height-16);
          list.addLine({graph.x+8,y},{graph.right()-8,y},theme.color.textMuted,1);
        }
        for(usize i=0;i<curve.size();++i) {
          UiPoint at{graph.x+8+curve[i].x*(graph.width-16),graph.bottom()-8-normalizedY(curve[i].y)*(graph.height-16)};
          if(i){UiPoint before{graph.x+8+curve[i-1].x*(graph.width-16),graph.bottom()-8-normalizedY(curve[i-1].y)*(graph.height-16)};list.addLine(before,at,theme.color.accent,2);}
          list.addRect({at.x-5,at.y-5,10,10},i==selected?theme.color.accent:theme.color.text,5);
          router.addRegion({at.x-16,at.y-16,32,32},widgetId(EditorWidget::TerrainPathNew)+0x500+u32(i));
        }
        auto curveActions=row(44);
        const EditorWidget actions[]{EditorWidget::TerrainProfilePrevious,EditorWidget::TerrainProfileNext,EditorWidget::TerrainProfileInsert,EditorWidget::TerrainProfileRemove};
        const char *labels[]{"< nó","nó >","+ nó","- nó"};
        for(u32 i=0;i<4;++i)button({curveActions.x+i*curveActions.width/4,curveActions.y,curveActions.width/4,44},labels[i],actions[i],false,i==2?curve.size()<32:i==3?selected>0 && selected+1<curve.size():true);
        auto curveValues=row(64);
        fieldAt({curveValues.x,curveValues.y,curveValues.width*.5f,curveValues.height},"Distância normalizada",curve[selected].x,50,selected>0 && selected+1<curve.size());
        fieldAt({curveValues.x+curveValues.width*.5f,curveValues.y,curveValues.width*.5f,curveValues.height},s.terrainSection==2?"Opacidade (0–1)":"Talude / crista (−1–2)",curve[selected].y,51);
        if(s.terrainSection==1) {
        button(row(),path.shape?"Modelagem do terreno: ligada":"Modelagem do terreno: desligada",EditorWidget::TerrainPathShape,path.shape,true,UiIcon::TerrainPathProfile);
        fields("Largura da pista (m)",path.width,32,"Acostamento total (m)",path.shoulderWidth,33);
        button(row(),path.keepWidthRatio?"Manter proporção: sim":"Larguras independentes",EditorWidget::TerrainPathRatio,path.keepWidthRatio);

        // Cross-section is drawn from the same piecewise-linear profile data.
        auto chart=deflate(row(72),UiInsets::all(8));list.addRect(chart,theme.color.raised,5);
        const float lane=path.width/std::max(path.shoulderWidth,.01f);
        auto profileY=[&](float t){for(usize i=1;i<path.profile.size();++i)if(t<=path.profile[i].x){const auto &a=path.profile[i-1],&b=path.profile[i];return std::lerp(a.y,b.y,(t-a.x)/(b.x-a.x));}return path.profile.back().y;};
        UiPoint previous{};
        for(u32 i=0;i<=80;++i){const float x=i/80.f,d=std::abs(x*2-1);const float y=d<lane?1:profileY((d-lane)/std::max(.001f,1-lane));UiPoint next{chart.x+x*chart.width,chart.bottom()-8-normalizedY(y)*(chart.height-16)};if(i)list.addLine(previous,next,theme.color.accent,2);previous=next;}
        fields("Elevação relativa (m)",path.elevation,34,"Inclinação máxima (°)",path.slopeLimit,35);
        field(path.followTerrain?"Seguir terreno 0 ↔ rampa uniforme 1":"Alturas dos pontos 0 ↔ rampa uniforme 1",path.uniformRamp,36);
        button(row(),path.autoRamp?"Auto Ramp: ligado":"Auto Ramp: desligado",EditorWidget::TerrainPathAutoRamp,path.autoRamp);
        button(row(),path.useDefinedHeight?"Usar altura definida: sim":"Alturas dos pontos / terreno",EditorWidget::TerrainPathDefinedHeight,path.useDefinedHeight);
        field("Altura definida (m)",path.definedHeight,37,path.useDefinedHeight);
        button(row(),path.reverseRamp?"Direção da rampa: invertida":"Direção da rampa: normal",EditorWidget::TerrainPathReverse,path.reverseRamp,path.uniformRamp>0 && !path.useDefinedHeight);
      } else {
        button(row(),path.texture?"Pintura do caminho: ligada":"Pintura do caminho: desligada",EditorWidget::TerrainPathTexture,path.texture);
        fields("Camada da pista",path.surfaceLayer,42,"Camada acostamento",path.shoulderLayer,43);
        fields("Opacidade pista",path.textureOpacity,38,"Opacidade acostamento",path.shoulderOpacity,39);
        field("Transição da textura (m)",path.textureTransition,40);

        button(row(),path.smartPaint?"Cruzamentos: preservar pista mais forte":"Cruzamentos: ordem dos caminhos",EditorWidget::TerrainPathSmart,path.smartPaint);
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
