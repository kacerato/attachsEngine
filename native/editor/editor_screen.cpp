#include "editor/editor_creation_catalog.h"
#include <unordered_set>
#include <cctype>
#include "editor/editor_screen.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_properties.h"
#include "editor/editor_grid.h"

#include "ui/ui_icon_id.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

namespace ae::editor {
namespace {

using namespace ae::ui;

// Medidas em DP, e a distinção importa. Os masters foram desenhados numa tela de
// 1672×941; um telefone real em paisagem tem cerca de 853×394 dp, porque a
// densidade é 3× e não 1,66×. Copiar os números do master daria uma barra
// superior ocupando 16% da altura em vez de 7%. As PROPORÇÕES dos masters são o
// alvo; os números, não.
constexpr float kTopBarHeight = 52.0f;
constexpr float kPanelHeaderHeight = 38.0f;
constexpr float kRowHeight = 28.0f;
constexpr float kActionButton = 36.0f;
constexpr float kCornerButton = 40.0f;
constexpr float kToolButton = 40.0f;
constexpr float kIconSize = 18.0f;
constexpr float kFieldHeight = 30.0f;
constexpr float kToggleWidth = 38.0f;
constexpr float kToggleHeight = 20.0f;
// Largura da alça do divisor. Ela é fina no desenho e larga no toque: a área é
// expandida pelo mínimo de acessibilidade no roteador, não aqui.
constexpr float kSplitterWidth = 6.0f;
constexpr float kPanelMinimum = 150.0f;
// A cena precisa sobrar. Sem este piso, arrastar os dois divisores até o meio
// deixaria o editor sem viewport e sem como voltar atrás.
constexpr float kViewportMinimum = 180.0f;


UiRect takeBottom(UiRect &rect,float height) {
  const float taken=std::min(rect.height,std::max(0.0f,height));
  const UiRect result{rect.x,rect.bottom()-taken,rect.width,taken};
  rect.height-=taken;return result;
}

UiRect takeLeft(UiRect &rect, float width) {
  const UiRect taken{rect.x, rect.y, std::min(width, rect.width), rect.height};
  rect.x += taken.width;
  rect.width -= taken.width;
  return taken;
}

UiRect takeRight(UiRect &rect, float width) {
  const float used = std::min(width, rect.width);
  rect.width -= used;
  return {rect.x + rect.width, rect.y, used, rect.height};
}

UiRect takeTop(UiRect &rect, float height) {
  const UiRect taken{rect.x, rect.y, rect.width, std::min(height, rect.height)};
  rect.y += taken.height;
  rect.height -= taken.height;
  return taken;
}

UiRect centred(const UiRect &area, float width, float height) {
  return {area.x + (area.width - width) * 0.5f, area.y + (area.height - height) * 0.5f, width,
          height};
}

UiIcon iconForKind(EditorEntityKind kind) {
  switch (kind) {
    case EditorEntityKind::Folder: return UiIcon::EditorAuthorFolder;
    case EditorEntityKind::Mesh: return UiIcon::EditorAuthorObject;
    case EditorEntityKind::Light: return UiIcon::EditorAuthorSun;
    case EditorEntityKind::Camera: return UiIcon::EditorAuthorCamera;
    case EditorEntityKind::Water: return UiIcon::WaterAuthorSurface;
    case EditorEntityKind::Effect: return UiIcon::VfxParticles;
  }
  return UiIcon::EditorAuthorObject;
}

const char *kindLabel(EditorEntityKind kind) {
  switch (kind) {
    case EditorEntityKind::Folder: return "GRUPO";
    case EditorEntityKind::Mesh: return "MALHA";
    case EditorEntityKind::Light: return "LUZ";
    case EditorEntityKind::Camera: return "CÂMERA";
    case EditorEntityKind::Water: return "ÁGUA";
    case EditorEntityKind::Effect: return "EFEITO";
  }
  return "NÓ";
}

struct ScreenBuilder final {
  const EditorScreenState &state;
  const UiTheme &theme;
  UiDrawList &list;
  UiInputRouter &router;

  bool isPressed(u32 widget) const { return state.pressedWidget == widget && widget != 0; }

  void iconButton(const UiRect &bounds, UiIcon icon, u32 widget, bool active = false,
                  UiColor tint = 0xFFFFFFFF) {
    const UiColor background = active ? theme.color.accent
                                      : (isPressed(widget) ? theme.color.line : theme.color.raised);
    list.addRect(bounds, background, theme.radius.control);
    list.addImage(centred(bounds, kIconSize, kIconSize), static_cast<UiImageId>(icon),
                  active ? theme.color.accentInk : tint);
    router.addRegion(bounds, widget, theme.touch.minimumTarget);
  }

  void toggle(const UiRect &area, bool on, u32 widget) {
    const UiRect bounds{area.right() - kToggleWidth, area.y + (area.height - kToggleHeight) * 0.5f,
                        kToggleWidth, kToggleHeight};
    list.addRect(bounds, on ? withAlpha(theme.color.accent, 0.28f) : theme.color.line,
                 kToggleHeight * 0.5f);
    const float knob = kToggleHeight - 5.0f;
    const float knobX = on ? bounds.right() - knob - 2.5f : bounds.x + 2.5f;
    list.addRect({knobX, bounds.y + 2.5f, knob, knob}, on ? theme.color.accent : theme.color.track,
                 knob * 0.5f);
    router.addRegion(bounds, widget, theme.touch.minimumTarget);
  }

  void label(const UiRect &bounds, std::string_view text, UiColor colour,
             const UiTypeStyle &style, UiAlign horizontal = UiAlign::Start) {
    list.pushClip(bounds);
    list.addText(bounds, text, colour, style, horizontal, UiAlign::Center);
    list.popClip();
  }
};

void buildTopBar(ScreenBuilder &builder, const UiRect &bar) {
  const UiTheme &theme = builder.theme;
  builder.list.addRect(bar, theme.color.voidBlack);
  UiRect content = deflate(bar, {theme.spacing.medium, theme.spacing.small, theme.spacing.medium,
                                 theme.spacing.small});

  // A marca de verdade, do atlas. Ela é imagem e não texto: a fonte da interface
  // não é a da marca, e desenhar "ASTRA" com Inter seria outra marca.
  const UiRect mark = takeLeft(content, 30.0f);
  builder.list.addImage(centred(mark, 26.0f, 26.0f), static_cast<UiImageId>(UiIcon::BrandMark));
  takeLeft(content, theme.spacing.tiny);
  const UiRect wordmark = takeLeft(content, 72.0f);
  builder.list.addImage(centred(wordmark, 68.0f, 16.0f),
                        static_cast<UiImageId>(UiIcon::BrandWordmark));

  takeLeft(content, theme.spacing.medium);
  const UiRect divider = takeLeft(content, 1.0f);
  builder.list.addRect({divider.x, divider.y + 8.0f, 1.0f, divider.height - 16.0f},
                       theme.color.line);
  takeLeft(content, theme.spacing.medium);

  // Ações à direita, de trás para frente para ficarem ancoradas na borda.
  const UiRect play = takeRight(content, 52.0f);
  builder.list.addRect(play, theme.color.accent, theme.radius.control);
  builder.list.addImage(centred(play, 18.0f, 18.0f), static_cast<UiImageId>(builder.state.workspace==EditorWorkspace::Play?UiIcon::EditorAuthorStop:UiIcon::EditorAuthorPlay),
                        theme.color.accentInk);
  builder.router.addRegion(play, widgetId(EditorWidget::PlayFromTopBar), theme.touch.minimumTarget);
  takeRight(content, theme.spacing.small);
  const auto action = [&](UiIcon icon, EditorWidget widget, bool enabled) {
    builder.iconButton(takeRight(content, kActionButton), icon, widgetId(widget), false,
                       enabled ? theme.color.text : theme.color.textFaint);
    takeRight(content, theme.spacing.tiny);
  };
  action(UiIcon::EditorAuthorMore, EditorWidget::ProjectMenu, true);
  action(UiIcon::EditorAuthorFolder, EditorWidget::OpenProject, true);
  action(UiIcon::EditorAuthorRedo, EditorWidget::Redo, builder.state.canRedo);
  action(UiIcon::EditorAuthorUndo, EditorWidget::Undo, builder.state.canUndo);

  builder.iconButton(takeLeft(content, 40.0f), UiIcon::EditorAuthorMore,
                     widgetId(EditorWidget::SceneChip));

  // As abas de modo. Elas trocam o CONTEXTO do viewport; por isso ficam no topo,
  // ao lado do nome do projeto, e não numa doca de ações no rodapé — numa tela
  // em paisagem a borda inferior é onde o polegar cobre o conteúdo e onde a
  // barra de gestos do sistema disputa o toque.
  takeLeft(content, theme.spacing.small);
  struct Tab final {
    const char *label;
    UiIcon icon;
    EditorWidget widget;
    EditorWorkspace workspace;
  };
  const Tab tabs[] = {
      {"Cena", UiIcon::EditorAuthorObject, EditorWidget::TabScene, EditorWorkspace::Scene},
      {"Recursos", UiIcon::EditorAuthorFolder, EditorWidget::TabAssets, EditorWorkspace::Assets},
      {"Iluminação", UiIcon::EditorAuthorSun, EditorWidget::TabLighting, EditorWorkspace::Lighting},
      {"Executar", UiIcon::EditorAuthorPlay, EditorWidget::TabPlay, EditorWorkspace::Play},
      {"Configurações", UiIcon::EditorAuthorSettings, EditorWidget::TabSettings, EditorWorkspace::Settings},
  };
  const float tabWidth = std::max(38.0f, content.width / 5.0f);
  // A decisão de mostrar rótulo é UMA, para todas as abas, e é tomada pela mais
  // larga. Decidir por aba deixaria "Executar" com nome e "Configurações" sem, o que
  // parece defeito de renderização e não uma adaptação de espaço.
  float widestLabel = 0.0f;
  for (const Tab &tab : tabs)
    widestLabel = std::max(widestLabel, builder.list.measure(tab.label, theme.type.caption));
  const bool showLabels = tabWidth - theme.spacing.small * 2.0f >= widestLabel + 22.0f;

  for (const Tab &tab : tabs) {
    if (content.width <= 0.0f) break;
    const UiRect slot = takeLeft(content, tabWidth);
    const bool active = builder.state.workspace == tab.workspace;
    if (active)
      builder.list.addRect(deflate(slot, UiInsets::symmetric(2.0f, 6.0f)), theme.color.accent,
                           theme.radius.control);
    const UiColor ink = active ? theme.color.accentInk : theme.color.textDim;
    if (showLabels) {
      UiRect inner = deflate(slot, UiInsets::symmetric(theme.spacing.small, 0.0f));
      builder.list.addImage(centred(takeLeft(inner, 20.0f), 15.0f, 15.0f),
                            static_cast<UiImageId>(tab.icon), ink);
      builder.label(inner, tab.label, ink, theme.type.caption);
    } else {
      builder.list.addImage(centred(slot, 18.0f, 18.0f), static_cast<UiImageId>(tab.icon), ink);
    }
    builder.router.addRegion(slot, widgetId(tab.widget), theme.touch.minimumTarget);
  }
}

// A grade do chão e o gizmo. É o que separa "a cena está rodando" de "estou
// editando a cena": sem referência espacial e sem alça, o viewport é um vídeo.
void buildViewportOverlay(ScreenBuilder &builder, const UiRect &viewport) {
  const EditorScreenState &state = builder.state;
  if (state.view == nullptr || !isViewportValid(*state.view)) return;
  const UiTheme &theme = builder.theme;
  builder.list.pushClip(viewport);

  if (state.showGrid) {
    const auto grid = buildEditorGrid(*state.view);
    for (u32 i=0; i<grid.count; ++i) {
      const auto &line=grid.lines[i];
      if(line.opacity<=0) continue;
      const auto base = line.axis==1 ? theme.color.axisX : line.axis==2 ? theme.color.axisZ : theme.color.textMuted;
      const float alpha = line.axis ? .35f : line.major ? .18f : .08f;
      // Atenuar quando células ficam menores que poucos pixels evita a faixa
      // branca de linhas acumuladas no horizonte. O custo permanece limitado.
      constexpr u32 segments=8;
      for(u32 segment=0;segment<segments;++segment) {
        float from[3],to[3],middle[3],neighbor[3];
        for(u32 axis=0;axis<3;++axis) {
          from[axis]=line.from[axis]+(line.to[axis]-line.from[axis])*(float(segment)/segments);
          to[axis]=line.from[axis]+(line.to[axis]-line.from[axis])*(float(segment+1)/segments);
          middle[axis]=neighbor[axis]=(from[axis]+to[axis])*.5f;
        }
        const u32 fixed=line.from[0]==line.to[0]?0:2;
        neighbor[fixed]+=grid.spacing*(line.major?10:1);
        UiPoint a{},b{};if(!projectSegmentToScreen(*state.view,from,to,a,b)) continue;
        float fade=1;
        if(!line.axis) {
          const auto center=projectWorldToScreen(*state.view,middle),offset=projectWorldToScreen(*state.view,neighbor);
          if(!center.valid||!offset.valid) continue;
          const float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
          if(length<.01f) continue;
          const float gap=std::abs((offset.screen.x-center.screen.x)*dy-(offset.screen.y-center.screen.y)*dx)/length;
          fade=std::clamp((gap-2)/6,0.0f,1.0f);
        }
        if(fade>0) builder.list.addLine(a,b,withAlpha(base,alpha*line.opacity*fade),1.0f);
      }
    }
  }

  const EditorEntity *entity = state.document->find(state.selection);
  if (entity != nullptr && state.tool != EditorGizmoMode::Select &&
      state.workspace == EditorWorkspace::Scene && !(entity->route.count && state.waterTab==1)) {
    EditorGizmoSettings settings{};
    settings.screenLengthPixels = 72.0f;
    float world[16];
    const EditorGizmoFrame frame = editorWorldMatrix(*state.document,entity->id,world)
        ? buildGizmoFrame(*state.view,world+12,settings) : EditorGizmoFrame{};
    if (frame.valid) {
      const UiColor colours[3] = {theme.color.axisX, theme.color.axisY, theme.color.axisZ};
      for (u32 axis = 0; axis < 3; ++axis) {
        if(state.tool==EditorGizmoMode::Rotate) {
          const bool active=static_cast<u32>(state.activeGizmoAxis)==axis+1;
          for(u32 segment=0;segment<64;++segment) {
            float from[3],to[3];
            gizmoRingPoint(frame.origin,axis,frame.axisWorldLength,segment*6.28318530718f/64,from);
            gizmoRingPoint(frame.origin,axis,frame.axisWorldLength,(segment+1)*6.28318530718f/64,to);
            UiPoint a,b;
            if(!projectSegmentToScreen(*state.view,from,to,a,b)) continue;
            builder.list.addLine(a,b,active?theme.color.accent:colours[axis],active?4.0f:2.0f);
            const UiPoint mid{(a.x+b.x)*.5f,(a.y+b.y)*.5f};float angle;
            if(viewport.contains(mid) && gizmoRingAngle(*state.view,frame.origin,axis,mid,angle))
              builder.router.addRegion({mid.x-7,mid.y-7,14,14},gizmoAxisWidget(axis));
          }
          continue;
        }
        if (!frame.axisUsable[axis]) continue;
        const bool active = static_cast<u32>(state.activeGizmoAxis) == axis + 1;
        builder.list.addLine(frame.originScreen, frame.axisEndScreen[axis],
                             active ? theme.color.accent : colours[axis], active ? 5.0f : 3.0f);
        // A ponta é um alvo próprio: arrastar um eixo pela ponta é o gesto que
        // um dedo grosso acerta, e a linha inteira continua valendo como alça.
        const UiRect handle{frame.axisEndScreen[axis].x - 7.0f, frame.axisEndScreen[axis].y - 7.0f,
                            14.0f, 14.0f};
        builder.list.addRect(handle, active ? theme.color.accent : colours[axis], 7.0f);
        // Sample the visible shaft, keeping every target inside the viewport.
        // The centre is reserved so overlapping axes do not trap camera gestures.
        for(u32 step=3;step<=12;++step) {
          const float t=step/12.0f;
          const UiPoint at{frame.originScreen.x+t*(frame.axisEndScreen[axis].x-frame.originScreen.x),
                           frame.originScreen.y+t*(frame.axisEndScreen[axis].y-frame.originScreen.y)};
          const float side=step==12?theme.touch.minimumTarget:16.0f;
          const float left=std::max(viewport.x,at.x-side*.5f), top=std::max(viewport.y,at.y-side*.5f);
          const float right=std::min(viewport.x+viewport.width,at.x+side*.5f);
          const float bottom=std::min(viewport.y+viewport.height,at.y+side*.5f);
          if(right>left && bottom>top) builder.router.addRegion({left,top,right-left,bottom-top},gizmoAxisWidget(axis));
        }
      }
      if(state.tool==EditorGizmoMode::Translate) for(u32 normal=0;normal<3;++normal) {
        const u32 u=(normal+1)%3,v=(normal+2)%3;
        UiPoint corners[4];bool valid=true;
        constexpr float offsets[4][2]{{.32f,.32f},{.62f,.32f},{.62f,.62f},{.32f,.62f}};
        for(u32 i=0;i<4;++i) {
          float point[3]{frame.origin[0],frame.origin[1],frame.origin[2]};
          point[u]+=offsets[i][0]*frame.axisWorldLength;
          point[v]+=offsets[i][1]*frame.axisWorldLength;
          const auto projected=projectWorldToScreen(*state.view,point);
          corners[i]=projected.screen;valid=valid && projected.valid && viewport.contains(projected.screen);
        }
        if(!valid) continue;
        const float area=std::abs((corners[1].x-corners[0].x)*(corners[3].y-corners[0].y)-
                                  (corners[1].y-corners[0].y)*(corners[3].x-corners[0].x));
        if(area<80.0f) continue; // Edge-on planes must not become invisible touch traps.
        const bool active=static_cast<u32>(state.activeGizmoAxis)==normal+4;
        for(u32 i=0;i<4;++i) builder.list.addLine(corners[i],corners[(i+1)%4],
            active?theme.color.accent:colours[normal],active?3.0f:2.0f);
        UiPoint center{};
        for(const auto &p:corners) {center.x+=p.x*.25f;center.y+=p.y*.25f;}
        builder.list.addRect({center.x-3,center.y-3,6,6},active?theme.color.accent:colours[normal],2);
        builder.router.addRegion({center.x-9,center.y-9,18,18},gizmoAxisWidget(normal+3));
      }
      builder.list.addRect({frame.originScreen.x - 4.0f, frame.originScreen.y - 4.0f, 8.0f, 8.0f},
                           theme.color.text, 4.0f);
    }
  }
  builder.list.popClip();
}

void buildFiles(ScreenBuilder &builder,const UiRect &panel) {
  const auto &theme=builder.theme;
  const auto &files=*builder.state.files;
  builder.list.addRect(panel,theme.color.surface);builder.router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(6));
  auto header=takeTop(content,28);
  builder.label(header,"Arquivos",theme.color.text,theme.type.body);
  builder.router.addRegion(header,widgetId(EditorWidget::FilesCollapse));
  if(builder.state.filesCollapsed) return;
  if(!files.error().empty()) {
    builder.list.pushClip(content);builder.label(content,files.error().c_str(),theme.color.textDim,theme.type.caption);builder.list.popClip();return;
  }
  builder.list.pushClip(content);
  const auto &entries=files.tree();
  for(u32 i=builder.state.fileScroll;i<entries.size() && content.height>=24;++i) {
    const auto &entry=entries[i];auto row=takeTop(content,24);const auto hit=row;
    takeLeft(row,static_cast<float>(entry.depth)*14);
    auto icon=takeLeft(row,22);
    builder.list.addImage(centred(icon,15,15),static_cast<UiImageId>(entry.directory?UiIcon::EditorAuthorFolder:UiIcon::AssetsFile),theme.color.textDim);
    builder.label(row,entry.name.c_str(),theme.color.text,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::FileRowBase)+i);
  }
  if(entries.empty()) builder.label(content,"Pasta vazia",theme.color.textMuted,theme.type.caption);
  builder.list.popClip();
}

u32 buildHierarchy(ScreenBuilder &builder, const UiRect &panel, u32 &outVisibleRows) {
  const UiTheme &theme = builder.theme;
  const EditorDocument &document = *builder.state.document;
  builder.list.addRect(panel, theme.color.surface);
  builder.router.addBlocker(panel);

  UiRect content = deflate(panel, UiInsets::all(theme.spacing.small));
  UiRect header = takeTop(content, kPanelHeaderHeight);
  builder.list.addImage(centred(takeLeft(header, 24.0f), 16.0f, 16.0f),
                        static_cast<UiImageId>(UiIcon::EditorAuthorObject), theme.color.text);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::EditorAuthorMore,
                     widgetId(EditorWidget::HierarchyMenu));
  takeRight(header, theme.spacing.tiny);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::EditorAuthorAdd, widgetId(EditorWidget::HierarchyAdd));
  builder.label(header, "Hierarquia", theme.color.text, theme.type.cardName);

  auto search=takeTop(content,36);
  builder.label(search,builder.state.hierarchySearch[0]?builder.state.hierarchySearch:"Pesquisar objetos...",theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::HierarchySearch));
  std::unordered_set<EditorEntityId> matches;
  const bool filtering=builder.state.hierarchySearch[0]!=0;
  if(filtering) {
    std::vector<EditorEntityId> ids;document.collectSubtree(document.root(),ids);
    const auto query=editorSearchKey(builder.state.hierarchySearch);
    for(auto id:ids) {
      const auto *entity=document.find(id);const auto name=editorSearchKey(entity->name);
      if(name.find(query)!=std::string::npos)
        for(auto parent=entity;parent;parent=document.find(parent->parent)) matches.insert(parent->id);
    }
  }
  builder.list.pushClip(content);
  outVisibleRows = static_cast<u32>(std::max(0.0f, content.height) / kRowHeight);

  struct Frame final {
    EditorEntityId entity;
    u32 depth;
  };
  std::vector<Frame> stack;
  const auto roots = document.childrenOf(document.root());
  for (usize index = roots.size(); index > 0; --index) stack.push_back({roots[index - 1], 0});

  u32 rows = 0;
  u32 skipped = 0;
  while (!stack.empty()) {
    const Frame frame = stack.back();
    stack.pop_back();
    const EditorEntity *entity = document.find(frame.entity);
    if (entity == nullptr || (filtering && !matches.contains(frame.entity))) continue;
    const auto children = document.childrenOf(frame.entity);
    const bool collapsed = std::find(builder.state.collapsedEntities.begin(), builder.state.collapsedEntities.end(), frame.entity) != builder.state.collapsedEntities.end();
    if (!collapsed || filtering) for (usize index = children.size(); index > 0; --index)
      stack.push_back({children[index - 1], frame.depth + 1});

    ++rows;
    // A rolagem descarta as primeiras linhas em vez de deslocar o desenho: a
    // lista é reconstruída todo frame, então não há nada para deslocar.
    if (skipped < builder.state.hierarchyScroll) {
      ++skipped;
      continue;
    }
    if (content.height < kRowHeight) continue;

    const UiRect row = takeTop(content, kRowHeight);
    const bool selected = builder.state.selection == frame.entity;
    if (selected) builder.list.addRect(row, theme.color.accent, theme.radius.thumb);
    else if (builder.isPressed(hierarchyRowWidget(frame.entity)))
      builder.list.addRect(row, theme.color.raised, theme.radius.thumb);

    UiRect rowContent = deflate(row, UiInsets::symmetric(theme.spacing.tiny, 0.0f));
    takeLeft(rowContent, std::min(static_cast<float>(frame.depth) * 14.0f, std::max(0.0f,rowContent.width-110.0f)));
    const UiColor ink = selected ? theme.color.accentInk : theme.color.text;
    const UiRect twisty = takeLeft(rowContent, 28.0f);
    if (!children.empty())
      builder.list.addImage(centred(twisty, 12.0f, 12.0f),
                            static_cast<UiImageId>(collapsed ? UiIcon::EditorAuthorAdd : UiIcon::EditorAuthorChevron),
                            selected ? theme.color.accentInk : theme.color.textDim);
    builder.list.addImage(centred(takeLeft(rowContent, 22.0f), 15.0f, 15.0f),
                          static_cast<UiImageId>(iconForKind(entity->kind)),
                          selected ? theme.color.accentInk : theme.color.accent);
    const UiRect eye = takeRight(rowContent, 24.0f);
    builder.list.addImage(
        centred(eye, 15.0f, 15.0f),
        static_cast<UiImageId>(entity->visible ? UiIcon::EditorAuthorEye
                                               : UiIcon::EditorAuthorEyeOff),
        selected ? theme.color.accentInk : theme.color.textDim);
    builder.label(rowContent, entity->name, ink, theme.type.body);

    // A linha PRIMEIRO, o olho DEPOIS: o roteador testa da última região para a
    // primeira, então quem entra depois fica por cima. O olho precisa ganhar do
    // fundo da linha, senão tocar nele seleciona em vez de alternar.
    builder.router.addRegion(row, hierarchyRowWidget(frame.entity));
    if (!children.empty()) builder.router.addRegion(twisty,widgetId(EditorWidget::HierarchyCollapseBase)+frame.entity);
    builder.router.addRegion(eye, hierarchyEyeWidget(frame.entity), theme.touch.minimumTarget);
  }
  builder.list.popClip();
  return rows;
}

void buildTransformRow(ScreenBuilder &builder, UiRect &content, const char *name,
                       const float values[3], u32 row, u32 decimals) {
  const UiTheme &theme = builder.theme;
  UiRect fields = takeTop(content, kFieldHeight);
  takeTop(content, theme.spacing.tiny);
  builder.label(takeLeft(fields, 52.0f), name, theme.color.textDim, theme.type.body);

  const UiColor axisColours[3] = {theme.color.axisX, theme.color.axisY, theme.color.axisZ};
  const char *axisNames[3] = {"X", "Y", "Z"};
  const float width = (fields.width - theme.spacing.tiny * 2.0f) / 3.0f;
  for (u32 axis = 0; axis < 3; ++axis) {
    const UiRect field = takeLeft(fields, width);
    if (axis < 2) takeLeft(fields, theme.spacing.tiny);
    const u32 widget = transformFieldWidget(row, axis);
    builder.list.addRect(field, builder.isPressed(widget) ? theme.color.line : theme.color.raised,
                         theme.radius.thumb);
    UiRect inner = deflate(field, UiInsets::symmetric(theme.spacing.tiny, 0.0f));
    builder.label(takeLeft(inner, 9.0f), axisNames[axis], axisColours[axis], theme.type.label);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(decimals),
                  static_cast<double>(values[axis]));
    builder.label(inner, buffer, theme.color.text, theme.type.numeric);
    builder.router.addRegion(field, widget);
  }
}

void buildPropertyPage(ScreenBuilder &builder, UiRect content, const EditorEntity &entity,
                       EditorPropertyGroup group) {
  const auto &theme=builder.theme;
  std::vector<u32> properties;
  for(u32 i=9;i<editorNumericProperties.size();++i)
    if(editorNumericProperties[i].group==group &&
       (group!=EditorPropertyGroup::WaterBody || (builder.state.waterTab==2?i<70:i>=70)) &&
       (group!=EditorPropertyGroup::Route || (i>=RoutePropertyBase+builder.state.routePoint*8 && i<RoutePropertyBase+(builder.state.routePoint+1)*8))) properties.push_back(i);
  const u32 pageSize=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-42.0f)/40.0f));
  const u32 pages=(static_cast<u32>(properties.size())+pageSize-1)/pageSize;
  const u32 page=std::min(builder.state.propertyPage,pages?pages-1:0);
  for(u32 row=0;row<pageSize && page*pageSize+row<properties.size();++row) {
    const u32 index=properties[page*pageSize+row];
    UiRect slot=takeTop(content,40);
    builder.label(takeLeft(slot,slot.width*.60f),editorNumericProperties[index].name,theme.color.textDim,theme.type.caption);
    slot=deflate(slot,UiInsets::all(2));
    char number[32];std::snprintf(number,sizeof(number),"%.5g",static_cast<double>(editorPropertyValue(entity,index)));
    builder.list.addRect(slot,theme.color.raised,theme.radius.control);
    builder.label(slot,number,theme.color.text,theme.type.numeric,UiAlign::Center);
    builder.router.addRegion(slot,widgetId(EditorWidget::TransformFieldBase)+index);
  }
  UiRect footer=takeTop(content,40);
  const float buttonWidth=footer.width*.3f;
  const auto previous=takeLeft(footer,buttonWidth),next=takeRight(footer,buttonWidth);
  builder.label(previous,"<",page?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
  builder.label(next,">",page+1<pages?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
  char pageLabel[32];std::snprintf(pageLabel,sizeof(pageLabel),"%u / %u",page+1,std::max(pages,1u));
  builder.label(footer,pageLabel,theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::PropertyNext));
}

void buildInspector(ScreenBuilder &builder, const UiRect &panel) {
  const UiTheme &theme = builder.theme;
  const EditorEntity *entity = builder.state.document->find(builder.state.selection);
  builder.list.addRect(panel, theme.color.surface);
  builder.router.addBlocker(panel);
  UiRect content = deflate(panel, UiInsets::all(theme.spacing.small));

  if (entity == nullptr) {
    builder.label(content, "Nada selecionado", theme.color.textMuted, theme.type.body,
                  UiAlign::Center);
    return;
  }

  UiRect header = takeTop(content, kPanelHeaderHeight);
  builder.list.addImage(centred(takeLeft(header, 26.0f), 18.0f, 18.0f),
                        static_cast<UiImageId>(iconForKind(entity->kind)), theme.color.text);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::EditorAuthorMore,
                     widgetId(EditorWidget::InspectorMenu));
  takeRight(header, theme.spacing.tiny);
  builder.toggle(takeRight(header, kToggleWidth + 4.0f), entity->active,
                 widgetId(EditorWidget::InspectorActive));
  const float half = header.height * 0.5f;
  builder.label({header.x, header.y, header.width, half}, entity->name, theme.color.text,
                theme.type.cardName);
  builder.label({header.x, header.y + half, header.width, half}, kindLabel(entity->kind),
                theme.color.textDim, theme.type.label);

  if(builder.state.workspace==EditorWorkspace::Settings) {
    builder.label(takeTop(content,38),"Água",theme.color.text,theme.type.body);
    buildPropertyPage(builder,content,*builder.state.document->find(builder.state.document->root()),EditorPropertyGroup::Water);
    return;
  }
  if(builder.state.workspace==EditorWorkspace::Lighting) {
    builder.label(takeTop(content,38),"Ambiente",theme.color.text,theme.type.body);
    buildPropertyPage(builder,content,*builder.state.document->find(builder.state.document->root()),EditorPropertyGroup::Environment);
    return;
  }
  if(entity->kind==EditorEntityKind::Water) {
    UiRect tabs=takeTop(content,40);
    const char *labels[]{"Superfície","Traçado","Física","Efeitos"};
    const UiIcon icons[]{UiIcon::WaterAuthorSurface,UiIcon::WaterAuthorRoute,UiIcon::WaterAuthorPhysics,UiIcon::WaterAuthorLayers};
    const EditorWidget ids[]{EditorWidget::WaterTabSurface,EditorWidget::WaterTabRoute,EditorWidget::WaterTabPhysics,EditorWidget::WaterTabEffects};
    const float width=tabs.width/4;
    for(u32 i=0;i<4;++i) {
      auto tab=takeLeft(tabs,width);const bool active=builder.state.waterTab==i;
      builder.list.addRect(deflate(tab,UiInsets::all(2)),active?theme.color.accent:theme.color.raised,4);
      builder.list.addImage({tab.x+tab.width*.5f-8,tab.y+3,16,16},static_cast<UiImageId>(icons[i]),active?theme.color.accentInk:theme.color.text);
      builder.label({tab.x,tab.y+20,tab.width,18},labels[i],active?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(tab,widgetId(ids[i]));
    }
    takeTop(content,8);
    if(builder.state.waterTab==0) {
      buildTransformRow(builder,content,"Posição",entity->transform.position,0,3);
      buildTransformRow(builder,content,"Rotação",entity->transform.rotationDegrees,1,1);
      buildTransformRow(builder,content,"Escala",entity->transform.scale,2,2);
    } else if(builder.state.waterTab==1) {
      if(!entity->route.count) {builder.label(content,"Crie um rio para editar o traçado",theme.color.textDim,theme.type.body);return;}
      UiRect controls=takeTop(content,40);
      const EditorWidget actions[]{EditorWidget::RoutePointPrevious,EditorWidget::RoutePointNext,EditorWidget::RoutePointAdd,EditorWidget::RoutePointRemove};
      const char *names[]{"<",">","+ ponto","Remover"};const float buttonWidth=controls.width/4;
      for(u32 i=0;i<4;++i) {auto button=takeLeft(controls,buttonWidth);builder.label(button,names[i],theme.color.text,theme.type.caption,UiAlign::Center);builder.router.addRegion(button,widgetId(actions[i]));}
      char label[40];std::snprintf(label,sizeof(label),"PONTO %u / %u",builder.state.routePoint+1,entity->route.count);
      builder.label(takeTop(content,24),label,theme.color.accent,theme.type.caption);
      buildPropertyPage(builder,content,*entity,EditorPropertyGroup::Route);
    } else {
      if(builder.state.waterTab==2) {
        auto row=takeTop(content,36);builder.label(row,"Volume de água / flutuação",theme.color.text,theme.type.caption);
        builder.toggle(takeRight(row,44),entity->waterPhysicsEnabled,widgetId(EditorWidget::ToggleWaterPhysics));
      }
      buildPropertyPage(builder,content,*entity,EditorPropertyGroup::WaterBody);
    }
    return;
  }
  UiRect tabs = takeTop(content, 30.0f);
  takeTop(content, theme.spacing.small);
  struct Tab final {
    const char *label;
    EditorWidget widget;
    EditorInspectorTab value;
  };
  const Tab tabItems[] = {
      {"Transformação", EditorWidget::InspectorTabTransform, EditorInspectorTab::Transform},
      {"Material", EditorWidget::InspectorTabMaterial, EditorInspectorTab::Material},
      {"Propriedades", EditorWidget::InspectorTabProperties, EditorInspectorTab::Properties},
  };
  const float tabWidth = (tabs.width - theme.spacing.tiny * 2.0f) / 3.0f;
  for (const Tab &tab : tabItems) {
    const UiRect slot = takeLeft(tabs, tabWidth);
    takeLeft(tabs, theme.spacing.tiny);
    const bool active = builder.state.tab == tab.value;
    builder.list.addRect(slot, active ? theme.color.accent : theme.color.raised,
                         theme.radius.control);
    builder.label(slot, tab.label, active ? theme.color.accentInk : theme.color.textDim,
                  theme.type.caption, UiAlign::Center);
    builder.router.addRegion(slot, widgetId(tab.widget), theme.touch.minimumTarget);
  }

  // As abas DIVIDEM o conteúdo, e não decoram o cabeçalho. É a razão de elas
  // existirem: um telefone em paisagem tem cerca de 240 dp de altura útil no
  // painel, e transform mais interruptores não cabem juntos. Empilhar tudo e
  // cortar no fim seria esconder controles sem dizer onde estão.
  switch (builder.state.tab) {
    case EditorInspectorTab::Transform: {
      buildTransformRow(builder, content, "Posição", entity->transform.position, 0, 3);
      buildTransformRow(builder, content, "Rotação", entity->transform.rotationDegrees, 1, 1);
      buildTransformRow(builder, content, "Escala", entity->transform.scale, 2, 1);
      break;
    }
    case EditorInspectorTab::Material: {
      if(entity->assetId) buildPropertyPage(builder,content,*entity,EditorPropertyGroup::Material);
      else builder.label(content,"Selecione uma malha",theme.color.textMuted,theme.type.body);
      break;
    }
    case EditorInspectorTab::Properties: {
      auto physicsRow=takeTop(content,38);
      builder.label(physicsRow,"Corpo rígido / flutuação",theme.color.text,theme.type.caption);
      builder.toggle(takeRight(physicsRow,44),entity->rigidBodyEnabled,widgetId(EditorWidget::ToggleRigidBody));
      if(entity->rigidBodyEnabled) {buildPropertyPage(builder,content,*entity,EditorPropertyGroup::Physics);break;}
      struct Switch final {
        const char *label;
        UiIcon icon;
        bool value;
        EditorWidget widget;
      };
      const Switch switches[] = {
          {"Visível", UiIcon::EditorAuthorEye, entity->visible, EditorWidget::ToggleVisible},
          {"Projetar sombra", UiIcon::EditorAuthorObject, entity->castShadow, EditorWidget::ToggleCastShadow},
          {"Receber sombra", UiIcon::AssetsChecker, entity->receiveShadow,
           EditorWidget::ToggleReceiveShadow},
          {"Estático", UiIcon::SceneLayers, entity->isStatic, EditorWidget::ToggleStatic},
      };
      for (const Switch &item : switches) {
        if (content.height < kRowHeight) break;
        UiRect row = takeTop(content, kRowHeight);
        builder.list.addImage(centred(takeLeft(row, 22.0f), 14.0f, 14.0f),
                              static_cast<UiImageId>(item.icon), theme.color.textDim);
        builder.toggle(row, item.value, widgetId(item.widget));
        builder.label(row, item.label, theme.color.text, theme.type.body);
      }
      break;
    }
  }
}

void buildSplitter(ScreenBuilder &builder, const UiRect &bounds, EditorWidget widget) {
  const UiTheme &theme = builder.theme;
  builder.list.addRect(bounds, theme.color.canvas);
  // Um traço curto no meio: sem ele o divisor é uma fresta preta que ninguém
  // adivinha que é arrastável.
  const float gripHeight = std::min(28.0f, bounds.height * 0.2f);
  builder.list.addRect(centred(bounds, 2.0f, gripHeight),
                       builder.isPressed(widgetId(widget)) ? theme.color.accent
                                                           : theme.color.track,
                       1.0f);
  builder.router.addRegion(bounds, widgetId(widget), theme.touch.minimumTarget);
}

void buildToolRail(ScreenBuilder &builder, const UiRect &viewport) {
  const UiTheme &theme = builder.theme;
  struct Tool final {
    UiIcon icon;
    EditorWidget widget;
    EditorGizmoMode mode;
  };
  const Tool tools[] = {
      {UiIcon::EditorAuthorSelect, EditorWidget::ToolSelect, EditorGizmoMode::Select},
      {UiIcon::EditorAuthorMove, EditorWidget::ToolMove, EditorGizmoMode::Translate},
      {UiIcon::EditorAuthorRotate, EditorWidget::ToolRotate, EditorGizmoMode::Rotate},
      {UiIcon::EditorAuthorScale, EditorWidget::ToolScale, EditorGizmoMode::Scale},
  };
  const float height = kToolButton * 4.0f + theme.spacing.tiny * 5.0f;
  const UiRect rail{viewport.x + theme.spacing.small,
                    viewport.y + (viewport.height - height) * 0.5f,
                    kToolButton + theme.spacing.tiny * 2.0f, height};
  builder.list.addRect(rail, withAlpha(theme.color.silhouette, 0.92f), theme.radius.control);
  builder.router.addBlocker(rail);
  UiRect content = deflate(rail, UiInsets::all(theme.spacing.tiny));
  for (const Tool &tool : tools) {
    const UiRect slot = takeTop(content, kToolButton);
    takeTop(content, theme.spacing.tiny);
    builder.iconButton(slot, tool.icon, widgetId(tool.widget), builder.state.tool == tool.mode);
  }
}

} // namespace

EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const UiTheme &theme,
                                     UiDrawList &list, UiInputRouter &router) {
  EditorScreenLayout layout{};
  if (state.document == nullptr || state.surface.isEmpty()) return layout;

  ScreenBuilder builder{state, theme, list, router};
  UiRect remaining = deflate(state.surface, state.safeArea);
  layout.topBar = takeTop(remaining, kTopBarHeight);

  // Larguras resolvidas ANTES de desenhar, porque o viewport é o que sobra e
  // precisa existir mesmo quando o usuário arrasta os dois divisores ao limite.
  const float available = remaining.width;
  float hierarchyWidth = state.hierarchyVisible && state.workspace != EditorWorkspace::Play
      ? (state.hierarchyWidth > 0.0f ? state.hierarchyWidth : available * 0.24f)
      : 0.0f;
  float inspectorWidth = state.inspectorVisible && state.workspace != EditorWorkspace::Play
      ? (state.inspectorWidth > 0.0f ? state.inspectorWidth : available * 0.27f)
      : 0.0f;
  if (hierarchyWidth > 0.0f) hierarchyWidth = std::max(hierarchyWidth, kPanelMinimum);
  if (inspectorWidth > 0.0f) inspectorWidth = std::max(inspectorWidth, kPanelMinimum);
  const float splitters = (hierarchyWidth > 0.0f ? kSplitterWidth : 0.0f) +
                          (inspectorWidth > 0.0f ? kSplitterWidth : 0.0f);
  const float overflow = hierarchyWidth + inspectorWidth + splitters + kViewportMinimum - available;
  if (overflow > 0.0f) {
    // Encolhe os dois proporcionalmente. Encolher só um deles faria o divisor
    // arrastado mexer no painel do outro lado, que é desconcertante de usar.
    const float total = hierarchyWidth + inspectorWidth;
    if (total > 0.0f) {
      hierarchyWidth = std::max(0.0f, hierarchyWidth - overflow * hierarchyWidth / total);
      inspectorWidth = std::max(0.0f, inspectorWidth - overflow * inspectorWidth / total);
    }
  }

  UiRect body = remaining;
  if (hierarchyWidth > 0.0f) {
    layout.hierarchyPanel = takeLeft(body, hierarchyWidth);
    buildSplitter(builder, takeLeft(body, kSplitterWidth), EditorWidget::SplitterLeft);
    if(state.files && layout.hierarchyPanel.height>210) {
      const float height=layout.hierarchyPanel.height;
      layout.filesPanel=takeBottom(layout.hierarchyPanel,state.filesCollapsed?40:height*std::clamp(state.filePanelRatio,.28f,.58f));
      const auto divider=takeBottom(layout.hierarchyPanel,kSplitterWidth);
      builder.list.addRect(divider,theme.color.track);

    }
  }
  if (inspectorWidth > 0.0f) {
    layout.inspectorPanel = takeRight(body, inspectorWidth);
    buildSplitter(builder, takeRight(body, kSplitterWidth), EditorWidget::SplitterRight);
  }
  layout.viewport = body;

  // O bloqueio ANTES dos controles da barra. O roteador testa da última região
  // para a primeira, então registrar o bloqueio depois o deixaria por cima de
  // todos os botões dela — a barra inteira absorveria o toque e nada nela
  // responderia. É o mesmo erro de ordem da linha e do olho da hierarquia.
  router.addBlocker(layout.topBar);
  buildTopBar(builder, layout.topBar);
  // Play owns the entire body; editor gizmos must not intercept runtime input.
  if (state.workspace == EditorWorkspace::Play) {
    list.addRect({layout.viewport.x+8,layout.viewport.y+4,std::min(390.0f,layout.viewport.width-16),32},withAlpha(theme.color.surface,.90f),3);
    builder.label({layout.viewport.x+12,layout.viewport.y+8,layout.viewport.width-24,24},
                  "Simulação - use o quadrado no topo para parar", theme.color.textDim,theme.type.caption);
    return layout;
  }
  buildViewportOverlay(builder, layout.viewport);
  if (state.workspace == EditorWorkspace::Scene) buildToolRail(builder, layout.viewport);
  if (!layout.hierarchyPanel.isEmpty())
    layout.hierarchyRowCount =
        buildHierarchy(builder, layout.hierarchyPanel, layout.hierarchyVisibleRows);
  if(!layout.filesPanel.isEmpty()) buildFiles(builder,layout.filesPanel);
  if (!layout.inspectorPanel.isEmpty()) buildInspector(builder, layout.inspectorPanel);
  if(!layout.filesPanel.isEmpty()) {
    // A área de toque ultrapassa a linha visual, acima dos bloqueadores dos painéis.
    const UiRect divider{layout.filesPanel.x,layout.hierarchyPanel.bottom()-4,
                         layout.filesPanel.width,kSplitterWidth+8};
    router.addRegion(divider,widgetId(EditorWidget::FilesSplitter));
  }


  // Botões flutuantes nos cantos do viewport, fora dos painéis.
  const UiRect corner{layout.viewport.right() - kCornerButton - theme.spacing.small,
                      layout.viewport.bottom() - kCornerButton - theme.spacing.small,
                      kCornerButton, kCornerButton};
  const UiRect save{layout.viewport.right()-48.0f,layout.viewport.y+8.0f,40.0f,40.0f};
  builder.list.addRect(save,theme.color.raised,theme.radius.control);
  builder.label(save,"Salvar",theme.color.text,theme.type.caption,UiAlign::Center);
  router.addRegion(save,widgetId(EditorWidget::SaveDocument));
  if (state.status && *state.status)
    builder.label({layout.viewport.x+8,layout.viewport.bottom()-26,layout.viewport.width-160,24},
                  state.status,theme.color.text,theme.type.caption);
  builder.iconButton(corner, UiIcon::EditorAuthorFrame, widgetId(EditorWidget::Fullscreen));
  builder.iconButton({layout.viewport.right() - kCornerButton * 2.0f - theme.spacing.small * 2.0f,
                      corner.y, kCornerButton, kCornerButton},
                     UiIcon::EditorAuthorGrid, widgetId(EditorWidget::ViewModeSolid), state.showGrid);
  builder.iconButton({layout.viewport.x + 8.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::EditorAuthorCamera, widgetId(EditorWidget::FrameSelection));
  builder.iconButton({layout.viewport.x + 56.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::EditorAuthorFrame, widgetId(EditorWidget::FrameAll));
  if(state.workspace==EditorWorkspace::Scene) {
    const UiIcon icons[]={UiIcon::EditorAuthorOrbit,UiIcon::EditorAuthorPan,UiIcon::EditorAuthorZoom};
    const EditorWidget actions[]={EditorWidget::NavigationOrbit,EditorWidget::NavigationPan,EditorWidget::NavigationZoom};
    const float width=std::min(64.0f,(layout.viewport.width-16)/3);
    for(u32 i=0;i<3;++i) {
      const UiRect cell{layout.viewport.x+8+i*width,layout.viewport.bottom()-76,width-2,36};
      const bool active=static_cast<u32>(state.navigation)==i;
      list.addRect(cell,active?theme.color.accent:theme.color.raised,theme.radius.control);
      list.addImage(centred(cell,20.0f,20.0f),static_cast<UiImageId>(icons[i]),active?theme.color.accentInk:theme.color.text);
      router.addRegion(cell,widgetId(actions[i]));
    }
  }
  if(state.workspace==EditorWorkspace::Assets) {
    list.addRect(layout.viewport,theme.color.surface);router.addBlocker(layout.viewport);
    UiRect content=deflate(layout.viewport,UiInsets::all(8));
    UiRect title=takeTop(content,44);
    builder.label(title,"Malhas do pacote",theme.color.text,theme.type.body);
    UiRect waterRow=takeTop(content,44);
    const auto finite=takeLeft(waterRow,waterRow.width*.5f);
    builder.label(finite,"Criar agua finita",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(waterRow,"Criar oceano",theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(finite,widgetId(EditorWidget::CreateFiniteWater));
    router.addRegion(waterRow,widgetId(EditorWidget::CreateOceanWater));
    const u32 pageRows=static_cast<u32>(std::max(0.0f,content.height-48)/44);
    for(u32 row=0;row<pageRows && state.assetScroll+row<state.assetCount;++row) {
      const u32 index=state.assetScroll+row;
      const auto slot=takeTop(content,44);
      char label[64];std::snprintf(label,sizeof(label),"Adicionar malha %u",index);
      builder.label(slot,label,theme.color.text,theme.type.body);
      router.addRegion(slot,widgetId(EditorWidget::AssetRowBase)+index);
    }
    UiRect footer{layout.viewport.x+8,layout.viewport.bottom()-48,layout.viewport.width-16,40};
    const auto previous=takeLeft(footer,footer.width*0.5f);
    builder.label(previous,"Anterior",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(footer,"Proxima",theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(previous,widgetId(EditorWidget::AssetsPrevious));router.addRegion(footer,widgetId(EditorWidget::AssetsNext));
  }
  if (state.entityMenu) {
    const UiRect menu{layout.viewport.x, layout.viewport.y+56.0f, std::min(180.0f,layout.viewport.width), std::min(280.0f,layout.viewport.height-56.0f)};
    builder.list.addRect(menu, theme.color.raised, theme.radius.control);
    UiRect rows=menu;
    const char *names[]={"Duplicar selecionado","Criar grupo","Excluir selecionado","Mover acima","Mover abaixo","Mudar pai","Mover para raiz","Renomear"};
    const EditorWidget actions[]={EditorWidget::DuplicateSelection,EditorWidget::CreateGroup,EditorWidget::DeleteSelection,EditorWidget::MoveEarlier,EditorWidget::MoveLater,EditorWidget::ReparentSelection,EditorWidget::MoveToRoot,EditorWidget::RenameSelection};
    for(u32 i=0;i<8;++i) {
      const auto row=takeTop(rows,menu.height/8.0f);
      builder.label(row,names[i],theme.color.text,theme.type.body,UiAlign::Center);
      router.addRegion(row,widgetId(actions[i]));
    }
  }
  if (state.renameEntity != kInvalidEntity || state.editingHierarchySearch || state.editingCreationSearch) {
    router.addBlocker(state.surface);
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,0.8f));
    const auto modal=centred(state.surface,std::min(560.0f,state.surface.width-16),std::min(320.0f,state.surface.height-16));
    list.addRect(modal,theme.color.surface,theme.radius.control);
    auto content=deflate(modal,UiInsets::all(8));
    builder.label(takeTop(content,40),state.renameText,theme.color.text,theme.type.body,UiAlign::Center);
    const char *keys=state.renameUppercase?"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ":"abcdefghijklmnopqrstuvwxyz0123456789_-. ";
    const float cw=content.width/10, ch=content.height/5;
    for(u32 i=0;i<40;++i) {
      const UiRect cell{content.x+(i%10)*cw+1,content.y+(i/10)*ch+1,cw-2,ch-2};
      char label[2]{keys[i],0};
      list.addRect(cell,theme.color.raised,theme.radius.control);
      builder.label(cell,keys[i]==' '?"Esp":label,theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widgetId(EditorWidget::NameKeyBase)+i);
    }
    const char *labels[]={"Aa","Apagar","Limpar","Cancelar","Aplicar"};
    const EditorWidget actions[]={EditorWidget::NameShift,EditorWidget::NameBackspace,EditorWidget::NameClear,EditorWidget::NameCancel,EditorWidget::NameApply};
    for(u32 i=0;i<5;++i) {
      const UiRect cell{content.x+i*content.width/5,content.y+4*ch,content.width/5,ch};
      builder.label(cell,labels[i],theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widgetId(actions[i]));
    }
  }
  if(state.draggingAsset || state.draggingEntity) {
    UiRect badge{std::clamp(state.assetDragPosition.x+12,state.surface.x,std::max(state.surface.x,state.surface.right()-160)),
                 std::clamp(state.assetDragPosition.y-44,state.surface.y,std::max(state.surface.y,state.surface.bottom()-36)),160,36};
    list.addRect(badge,theme.color.raised,theme.radius.control);
    builder.label(badge,state.draggingAsset?"Soltar malha":"Mudar pai",theme.color.accent,theme.type.body,UiAlign::Center);
  }
  if(state.document && state.view && state.workspace==EditorWorkspace::Scene) {
    const auto *water=state.document->find(state.selection);
    if(water && water->route.count && state.waterTab==1) {
      float world[16];
      if(editorWorldMatrix(*state.document,water->id,world)) {
        list.pushClip(layout.viewport);
        UiPoint previous{};bool hasPrevious=false;
        for(u32 segment=0;segment+1<water->route.count;++segment) for(u32 step=0;step<=renderer::WaterRouteSteps;++step) {
          const auto sample=renderer::evaluateWaterRoute(water->route,segment,float(step)/renderer::WaterRouteSteps);
          const float local[]{sample.center.x,sample.center.y,sample.center.z};float position[3];
          for(u32 a=0;a<3;++a) position[a]=world[12+a]+world[a]*local[0]+world[4+a]*local[1]+world[8+a]*local[2];
          const auto projected=projectWorldToScreen(*state.view,position);
          if(projected.valid) {if(hasPrevious) list.addLine(previous,projected.screen,theme.color.accent,2);previous=projected.screen;hasPrevious=true;}
          else hasPrevious=false;
        }
        for(u32 i=0;i<water->route.count;++i) {
          const auto &p=water->route.points[i];float position[3];
          for(u32 a=0;a<3;++a) position[a]=world[12+a]+world[a]*p.position[0]+world[4+a]*p.position[1]+world[8+a]*p.position[2];
          const auto projected=projectWorldToScreen(*state.view,position);if(!projected.valid) continue;
          const auto point=projected.screen;
          if(!layout.viewport.contains(point)) continue;
          list.addRect({point.x-7,point.y-7,14,14},i==state.routePoint?theme.color.accent:theme.color.text,4);
          router.addRegion({point.x-18,point.y-18,36,36},widgetId(EditorWidget::RoutePointBase)+i);
        }
        list.popClip();
      }
    }
  }
  if(state.workspaceMenu) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.65f));router.addBlocker(state.surface);
    const UiRect modal=centred(state.surface, std::min(340.0f,state.surface.width-24), 288);
    list.addRect(modal,theme.color.surface,8);auto content=deflate(modal,UiInsets::all(12));
    const char *names[]{"Cena","Recursos","Iluminação","Configurações","Fechar"};
    const EditorWidget actions[]{EditorWidget::TabScene,EditorWidget::TabAssets,EditorWidget::TabLighting,EditorWidget::TabSettings,EditorWidget::WorkspaceMenuClose};
    builder.label(takeTop(content,32),"Áreas de trabalho",theme.color.text,theme.type.cardName);
    for(u32 i=0;i<5;++i) {auto row=takeTop(content,44);builder.label(row,names[i],theme.color.text,theme.type.body);router.addRegion(row,widgetId(actions[i]));}
  }
  if(state.creationMenu && !state.editingCreationSearch) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.60f));router.addBlocker(state.surface);
    const UiRect modal=centred(state.surface,std::min(520.0f,state.surface.width-24),std::min(310.0f,state.surface.height-24));
    list.addRect(modal,theme.color.surface,5);auto content=deflate(modal,UiInsets::all(12));
    auto title=takeTop(content,30);
    builder.label(title,"Adicionar objeto",theme.color.text,theme.type.cardName);
    auto close=takeRight(title,36);builder.label(close,"X",theme.color.textDim,theme.type.body,UiAlign::Center);router.addRegion(close,widgetId(EditorWidget::CreateMenuClose));
    auto search=takeTop(content,36);list.addRect(search,theme.color.silhouette,3);
    auto clear=takeRight(search,36);builder.label(clear,"X",theme.color.textDim,theme.type.caption,UiAlign::Center);router.addRegion(clear,widgetId(EditorWidget::CreationClearSearch));
    builder.label(deflate(search,UiInsets::all(8)),state.creationSearch[0]?state.creationSearch:"Pesquisar tipo...",theme.color.textDim,theme.type.caption);
    router.addRegion(search,widgetId(EditorWidget::CreationSearch));takeTop(content,8);
    UiRect footer{content.x,content.bottom()-38,content.width,38};
    UiRect description{content.x,footer.y-28,content.width,28};content.height=std::max(0.0f,description.y-content.y);
    auto categories=takeLeft(content,112);takeLeft(content,12);
    for(u32 i=0;i<4;++i) {
      auto row=takeTop(categories,36);
      if(state.creationCategory==i&&!state.creationSearch[0]) list.addRect(row,theme.color.raised,2);
      builder.label(deflate(row,UiInsets::symmetric(8,0)),creationCategories[i],state.creationCategory==i?theme.color.accent:theme.color.textDim,theme.type.body);
      router.addRegion(row,widgetId(EditorWidget::CreationCategoryBase)+i);
    }
    const auto query=editorSearchKey(state.creationSearch);
    u32 count=0;bool selectedVisible=false;
    for(u32 i=0;i<editorCreationCatalog.size();++i) {
      const auto &entry=editorCreationCatalog[i];const auto name=editorSearchKey(entry.name);
      if(query.empty()?entry.category!=state.creationCategory:name.find(query)==std::string::npos) continue;
      const auto ordinal=count++;if(ordinal/3!=state.creationPage) continue;
      auto row=takeTop(content,40);const bool selected=state.creationSelection==i;
      if(selected) {list.addRect(row,theme.color.raised,2);list.addRect({row.x,row.y,2,row.height},theme.color.accent);selectedVisible=true;}
      auto label=deflate(row,UiInsets::symmetric(8,0));list.addImage(centred(takeLeft(label,26),16,16),static_cast<UiImageId>(entry.icon),selected?theme.color.accent:theme.color.textDim);
      builder.label(label,entry.name,theme.color.text,theme.type.body);
      router.addRegion(row,widgetId(EditorWidget::CreationRowBase)+i);
    }
    if(!count) builder.label(content,"Nenhum tipo encontrado",theme.color.textDim,theme.type.caption);
    if(selectedVisible) {
      const auto &selected=editorCreationCatalog[state.creationSelection];
      builder.label(description,selected.description,theme.color.textDim,theme.type.caption);
      auto create=takeRight(footer,96);list.addRect(create,theme.color.accent,3);builder.label(create,"Criar",theme.color.accentInk,theme.type.body,UiAlign::Center);router.addRegion(create,widgetId(selected.action));
    }
    auto cancel=takeRight(footer,90);builder.label(cancel,"Cancelar",theme.color.textDim,theme.type.body,UiAlign::Center);router.addRegion(cancel,widgetId(EditorWidget::CreateMenuClose));
    if(count>3) {
      auto previous=takeLeft(footer,36);builder.label(previous,"<",theme.color.text,theme.type.body,UiAlign::Center);router.addRegion(previous,widgetId(EditorWidget::CreationPrevious));
      auto next=takeLeft(footer,36);builder.label(next,">",theme.color.text,theme.type.body,UiAlign::Center);router.addRegion(next,widgetId(EditorWidget::CreationNext));
    }
  }
  if (state.numericField != 0) {
    router.addBlocker(state.surface);
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,0.8f));
    const float width=std::min(320.0f,state.surface.width-16.0f);
    const float height=std::min(320.0f,state.surface.height-16.0f);
    const UiRect modal=centred(state.surface,width,height);
    list.addRect(modal,theme.color.surface,theme.radius.control);
    UiRect content=deflate(modal,UiInsets::all(8.0f));
    builder.label(takeTop(content,36.0f),state.numericError ? "Valor invalido" : state.numericText,
                  theme.color.text,theme.type.numeric,UiAlign::Center);
    const float cellWidth=content.width/4.0f,cellHeight=content.height/4.0f;
    const char *labels[16]={"1","2","3","Apagar","4","5","6","Limpar","7","8","9","Cancelar",".","0","-","Aplicar"};
    const u32 keys[16]={0,1,2,12,3,4,5,13,6,7,8,14,9,10,11,15};
    for(u32 i=0;i<16;++i) {
      const UiRect cell{content.x+(i%4)*cellWidth+2,content.y+(i/4)*cellHeight+2,cellWidth-4,cellHeight-4};
      const u32 key=keys[i];
      const u32 widget=key<12 ? widgetId(EditorWidget::NumericKeyBase)+key :
        key==12?widgetId(EditorWidget::NumericBackspace):key==13?widgetId(EditorWidget::NumericClear):
        key==14?widgetId(EditorWidget::NumericCancel):widgetId(EditorWidget::NumericApply);
      list.addRect(cell,theme.color.raised,theme.radius.control);
      builder.label(cell,labels[i],theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widget);
    }
  }
  return layout;
}

EditorPointerOutcome applyEditorPointer(EditorScreenState &state,
                                        const EditorScreenLayout &layout,
                                        const ui::UiPointerRouting &routing,
                                        EditorDocument &document, EditorHistory &history) {
  EditorPointerOutcome outcome{};
  if (routing.target == ui::UiPointerTarget::Viewport) {
    outcome.viewport = true;
    return outcome;
  }
  if (routing.target != ui::UiPointerTarget::Widget) {
    outcome.consumed = true;
    return outcome;
  }
  outcome.consumed = true;
  const u32 widget = routing.widgetId;

  // Os divisores respondem ao ARRASTE, e continuamente. Esperar o dedo levantar
  // para reposicionar o painel tornaria impossível encontrar a largura certa.
  if (widget == widgetId(EditorWidget::SplitterLeft) && routing.dragging) {
    const float base =
        layout.hierarchyPanel.width > 0.0f ? layout.hierarchyPanel.width : state.hierarchyWidth;
    state.hierarchyWidth = std::max(kPanelMinimum, base + routing.stepDelta.x);
    return outcome;
  }
  if (widget == widgetId(EditorWidget::SplitterRight) && routing.dragging) {
    const float base =
        layout.inspectorPanel.width > 0.0f ? layout.inspectorPanel.width : state.inspectorWidth;
    state.inspectorWidth = std::max(kPanelMinimum, base - routing.stepDelta.x);
    return outcome;
  }

  if (((widget >= widgetId(EditorWidget::HierarchyRowBase) && widget < widgetId(EditorWidget::TransformFieldBase)) ||
       widget >= widgetId(EditorWidget::HierarchyCollapseBase)) && routing.dragging) {
    state.hierarchyScrollRemainder -= routing.stepDelta.y;
    const int lines=static_cast<int>(state.hierarchyScrollRemainder/kRowHeight);
    state.hierarchyScrollRemainder-=static_cast<float>(lines)*kRowHeight;
    const int maximum=std::max(0,static_cast<int>(layout.hierarchyRowCount)-static_cast<int>(layout.hierarchyVisibleRows));
    state.hierarchyScroll=static_cast<u32>(std::clamp(static_cast<int>(state.hierarchyScroll)+lines,0,maximum));
    return outcome;
  }

  // O resto age no toque completo, e não no `Down`: um dedo que desce num botão
  // e desliza para fora desistiu dele.
  if (!routing.tapped) return outcome;

  if (widget >= widgetId(EditorWidget::HierarchyCollapseBase)) {
    const auto entity=widget-widgetId(EditorWidget::HierarchyCollapseBase);
    if(document.find(entity)) {
      auto &ids=state.collapsedEntities;
      auto it=std::find(ids.begin(),ids.end(),entity);
      if(it==ids.end()) ids.push_back(entity); else ids.erase(it);
      state.hierarchyScroll=0;
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyEyeBase) &&
      widget < widgetId(EditorWidget::TransformFieldBase)) {
    const EditorEntityId entity = widget - widgetId(EditorWidget::HierarchyEyeBase);
    const EditorEntity *found = document.find(entity);
    if (found != nullptr) {
      EditorEntity values = *found;
      values.visible = !values.visible;
      outcome.documentChanged = history.applyValues(document, entity, values);
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyRowBase) &&
      widget < widgetId(EditorWidget::HierarchyEyeBase)) {
    const auto target=widget-widgetId(EditorWidget::HierarchyRowBase);
    if(state.reparentEntity) {
      outcome.documentChanged=history.reparentKeepingWorld(document,state.reparentEntity,target);
      state.status=outcome.documentChanged?"Pai alterado":"Pai invalido ou transformacao incompativel";
      state.reparentEntity=kInvalidEntity;
    } else {state.selection=target;state.propertyPage=0;state.routePoint=0;}
    return outcome;
  }

  const auto toggleField = [&](bool EditorEntity::*field) {
    const EditorEntity *found = document.find(state.selection);
    if (found == nullptr) return;
    EditorEntity values = *found;
    values.*field = !(values.*field);
    outcome.documentChanged = history.applyValues(document, state.selection, values);
  };

  switch (static_cast<EditorWidget>(widget)) {
    case EditorWidget::PropertyPrevious: if(state.propertyPage) --state.propertyPage; break;
    case EditorWidget::PropertyNext: {
      float available=layout.inspectorPanel.height-16.0f-kPanelHeaderHeight-38.0f;
      auto group=state.workspace==EditorWorkspace::Settings?EditorPropertyGroup::Water:state.workspace==EditorWorkspace::Lighting?EditorPropertyGroup::Environment:EditorPropertyGroup::Material;
      const auto *entity=document.find(state.selection);
      if(state.workspace==EditorWorkspace::Scene && entity) {
        if(entity->kind==EditorEntityKind::Water) {
          available-=10;
          group=state.waterTab==1?EditorPropertyGroup::Route:EditorPropertyGroup::WaterBody;
          available-=state.waterTab==1?64:state.waterTab==2?36:0;
        } else if(state.tab==EditorInspectorTab::Properties && entity->rigidBodyEnabled) {group=EditorPropertyGroup::Physics;available-=38;}
      }
      const u32 pageSize=std::max(1u,static_cast<u32>(std::max(0.0f,available-42.0f)/40.0f));
      const u32 count=group==EditorPropertyGroup::Route?8:group==EditorPropertyGroup::WaterBody?(state.waterTab==2?3:4):static_cast<u32>(std::count_if(editorNumericProperties.begin(),editorNumericProperties.end(),[&](const auto &p){return p.group==group;}));
      const u32 pages=(count+pageSize-1)/pageSize;
      state.propertyPage=std::min(state.propertyPage+1,pages?pages-1:0);break;
    }
    case EditorWidget::ReparentSelection:
      state.reparentEntity=state.selection;state.entityMenu=false;state.status="Toque no novo pai na hierarquia";break;
    case EditorWidget::MoveToRoot:
      outcome.documentChanged=history.reparentKeepingWorld(document,state.selection,document.root());
      state.entityMenu=false;state.status=outcome.documentChanged?"Movido para raiz":"Transformacao incompativel";break;
    case EditorWidget::MoveEarlier:
    case EditorWidget::MoveLater: {
      u32 index=0;const auto *selected=document.find(state.selection);
      if(selected && document.childIndexOf(selected->id,index)) {
        const auto count=document.childrenOf(selected->parent).size();
        const u32 next=widget==widgetId(EditorWidget::MoveEarlier)?(index?index-1:0):std::min(index+1,static_cast<u32>(count-1));
        if(next!=index) outcome.documentChanged=history.reparent(document,selected->id,selected->parent,next);
      }
      state.entityMenu=false;break;
    }
    case EditorWidget::AssetsPrevious:
    case EditorWidget::AssetsNext: {
      const u32 page=std::max(1u,static_cast<u32>(std::max(0.0f,layout.viewport.height-108)/44));
      if(widget==widgetId(EditorWidget::AssetsNext)) state.assetScroll=std::min(state.assetScroll+page,state.assetCount?state.assetCount-1:0);
      else state.assetScroll=state.assetScroll>page?state.assetScroll-page:0;
      break;
    }
    case EditorWidget::InspectorMenu:
    case EditorWidget::RenameSelection:
      if(const auto *entity=document.find(state.selection)) {
        state.renameEntity=entity->id;
        std::snprintf(state.renameText,sizeof(state.renameText),"%s",entity->name);
      }
      state.entityMenu=false;break;
    case EditorWidget::NavigationOrbit: state.navigation=EditorNavigationMode::Orbit;break;
    case EditorWidget::NavigationPan: state.navigation=EditorNavigationMode::Pan;break;
    case EditorWidget::NavigationZoom: state.navigation=EditorNavigationMode::Zoom;break;
    case EditorWidget::CreationSearch: state.editingCreationSearch=true;std::copy(std::begin(state.creationSearch),std::end(state.creationSearch),state.renameText);break;
    case EditorWidget::CreationClearSearch: state.creationSearch[0]=0;state.creationPage=0;break;
    case EditorWidget::CreationPrevious: if(state.creationPage) --state.creationPage;break;
    case EditorWidget::CreationNext: {
      const auto query=editorSearchKey(state.creationSearch);u32 count=0;
      for(const auto &entry:editorCreationCatalog) if(query.empty()?entry.category==state.creationCategory:editorSearchKey(entry.name).find(query)!=std::string::npos) ++count;
      state.creationPage=(state.creationPage+1)%std::max(1u,(count+2)/3);break;
    }
    case EditorWidget::HierarchySearch:
      state.editingHierarchySearch=true;std::copy(std::begin(state.hierarchySearch),std::end(state.hierarchySearch),state.renameText);break;
    case EditorWidget::HierarchyClearSearch: state.hierarchySearch[0]=0;state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyExpandAll: state.collapsedEntities.clear();state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyCollapseAll:
      document.collectSubtree(document.root(),state.collapsedEntities);state.hierarchyScroll=0;break;
    case EditorWidget::HierarchyMenu: state.entityMenu = !state.entityMenu; break;
    case EditorWidget::DuplicateSelection: {
      const auto copy=history.duplicateEntity(document,state.selection);
      if(copy) {state.selection=copy;outcome.documentChanged=true;}
      state.entityMenu=false;
      break;
    }
    case EditorWidget::CreateGroup: {
      const auto copy=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Objeto vazio");
      if(copy) {state.selection=copy;outcome.documentChanged=true;}
      state.entityMenu=false;
      break;
    }
    case EditorWidget::DeleteSelection:
      outcome.documentChanged=history.destroyEntity(document,state.selection);
      if(outcome.documentChanged) state.selection=kInvalidEntity;
      state.entityMenu=false;
      break;
    case EditorWidget::SceneChip:
    case EditorWidget::ProjectMenu: state.workspaceMenu=!state.workspaceMenu;break;
    case EditorWidget::WorkspaceMenuClose: state.workspaceMenu=false;break;
    case EditorWidget::SaveDocument: state.saveRequested=true; break;
    case EditorWidget::Undo: outcome.documentChanged = history.undo(document); break;
    case EditorWidget::Redo: outcome.documentChanged = history.redo(document); break;
    case EditorWidget::PlayFromTopBar:
    case EditorWidget::TabPlay:
      state.workspace = state.workspace == EditorWorkspace::Play ? EditorWorkspace::Scene : EditorWorkspace::Play;
      outcome.requestPlay = state.workspace == EditorWorkspace::Play;
      break;
    case EditorWidget::TabScene: state.workspaceMenu=false; state.workspace = EditorWorkspace::Scene; break;
    case EditorWidget::TabAssets: state.workspaceMenu=false; state.workspace = EditorWorkspace::Assets; break;
    case EditorWidget::TabLighting: state.workspaceMenu=false; state.workspace = EditorWorkspace::Lighting; state.selection=document.root(); state.propertyPage=0; break;
    case EditorWidget::TabSettings: state.workspaceMenu=false; state.workspace = EditorWorkspace::Settings; state.selection=document.root();state.propertyPage=0;break;
    case EditorWidget::InspectorTabTransform: state.tab = EditorInspectorTab::Transform; break;
    case EditorWidget::InspectorTabMaterial: state.tab = EditorInspectorTab::Material; state.propertyPage=0; break;
    case EditorWidget::InspectorTabProperties: state.tab = EditorInspectorTab::Properties; break;
    case EditorWidget::ToolSelect: state.tool = EditorGizmoMode::Select; break;
    case EditorWidget::ToolMove: state.tool = EditorGizmoMode::Translate; break;
    case EditorWidget::ToolRotate: state.tool = EditorGizmoMode::Rotate; break;
    case EditorWidget::ToolScale: state.tool = EditorGizmoMode::Scale; break;
    case EditorWidget::ViewModeSolid: state.showGrid = !state.showGrid; break;
    case EditorWidget::Fullscreen:
      // Tela cheia é esconder os dois painéis, e não um modo separado: o estado
      // continua o mesmo e voltar devolve as larguras que o usuário tinha.
      state.hierarchyVisible = !state.hierarchyVisible;
      state.inspectorVisible = state.hierarchyVisible;
      break;
    case EditorWidget::InspectorActive: toggleField(&EditorEntity::active); break;
    case EditorWidget::ToggleVisible: toggleField(&EditorEntity::visible); break;
    case EditorWidget::ToggleCastShadow: toggleField(&EditorEntity::castShadow); break;
    case EditorWidget::ToggleReceiveShadow: toggleField(&EditorEntity::receiveShadow); break;
    case EditorWidget::ToggleStatic: toggleField(&EditorEntity::isStatic); break;
    case EditorWidget::HierarchyAdd: state.creationMenu=true; break;
    case EditorWidget::CreateMenuClose: state.creationMenu=false;break;
    case EditorWidget::ToggleWaterPhysics: toggleField(&EditorEntity::waterPhysicsEnabled);break;
    case EditorWidget::ToggleRigidBody: toggleField(&EditorEntity::rigidBodyEnabled);state.propertyPage=0;break;
    case EditorWidget::WaterTabSurface:state.waterTab=0;state.propertyPage=0;break;
    case EditorWidget::WaterTabRoute:state.waterTab=1;state.propertyPage=0;break;
    case EditorWidget::WaterTabPhysics:state.waterTab=2;state.propertyPage=0;break;
    case EditorWidget::WaterTabEffects:state.waterTab=3;state.propertyPage=0;break;
    default: break;
  }
  return outcome;
}

} // namespace ae::editor
