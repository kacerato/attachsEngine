#include <sstream>
#include "scene/script_behavior.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include <unordered_set>
#include <cctype>
#include "editor/editor_screen.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_properties.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_grid.h"
#include "editor/editor_collider_geometry.h"
#include "editor/editor_import_reconcile.h"
#include <bit>

#include "ui/ui_icon_id.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>
#include <array>
#include <filesystem>

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

struct ScreenBuilder final {
  const EditorScreenState &state;
  const UiTheme &theme;
  UiDrawList &list;
  UiInputRouter &router;
  u32 componentPage=0;

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


  if(builder.state.workspace==EditorWorkspace::Play && !waterCreationAvailable(builder.state)) {
    const auto control=[&](const char *label,EditorWidget id,bool enabled) {
      const auto rect=takeRight(content,76);
      builder.list.addRect(rect,theme.color.raised,theme.radius.control);
      builder.label(rect,label,enabled?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
      if(enabled) builder.router.addRegion(rect,widgetId(id),theme.touch.minimumTarget);
      takeRight(content,theme.spacing.small);
    };
    control("Passo",EditorWidget::StepPlay,builder.state.playPaused);
    control(builder.state.playPaused?"Retomar":"Pausar",EditorWidget::PausePlay,true);
  } else {
    action(UiIcon::EditorAuthorRedo, EditorWidget::Redo, builder.state.canRedo);
    action(UiIcon::EditorAuthorUndo, EditorWidget::Undo, builder.state.canUndo);
  }

  const auto sceneMenu=takeLeft(content,76.0f);
  builder.list.addRect(sceneMenu,theme.color.raised,theme.radius.control);
  builder.label(sceneMenu,"Cena",theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(sceneMenu,widgetId(EditorWidget::ProjectMenu));
  takeLeft(content,theme.spacing.small);
  const auto save=takeLeft(content,64.0f);
  builder.list.addRect(save,theme.color.raised,theme.radius.control);
  builder.label(save,"Salvar",theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(save,widgetId(EditorWidget::SaveDocument));

  takeLeft(content,theme.spacing.medium);
  const char *context=builder.state.workspace==EditorWorkspace::Play ? (builder.state.playPaused?"Pausado":"Em execução") :
      builder.state.workspace==EditorWorkspace::Lighting ? "Ambiente da cena" :
      builder.state.workspace==EditorWorkspace::Settings ? "Água da cena" :
      builder.state.workspace==EditorWorkspace::Assets ? "Recursos importados" : "Edição";
  if(content.width>80) builder.label(content,context,theme.color.textDim,theme.type.caption);

}

void buildPhysicsOverlay(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto *entity=state.document->find(state.selection);
  if(!entity || state.workspace!=EditorWorkspace::Scene || state.componentSelection!=entity->id) return;
  const auto *component=entity->components.findInstance(state.expandedNative);if(!component) return;
  if(&component->type()==&scene::Collider::descriptor) {
    const auto &c=static_cast<const scene::Collider &>(*component);float world[16],local[16];
    if(!editorWorldMatrix(*state.document,entity->id,world)) return;
    EditorTransform t;t.position[0]=c.centerX;t.position[1]=c.centerY;t.position[2]=c.centerZ;
    t.rotationDegrees[0]=c.rotationX;t.rotationDegrees[1]=c.rotationY;t.rotationDegrees[2]=c.rotationZ;editorTransformMatrix(t,local);
    const bool validOwner=editorReferenceAccepts(*state.document,entity->id,scene::colliderReferences[0],c.owner,true);
    const auto color=!validOwner?builder.theme.color.axisX:c.enabled?builder.theme.color.accent:builder.theme.color.textMuted;
    editorColliderSegments(c,[&](const auto &a,const auto &b) {
      float points[2][3];const auto transform=[&](const auto &p,float *out) {
        float l[3];for(u32 k=0;k<3;++k) l[k]=local[k]*p[0]+local[4+k]*p[1]+local[8+k]*p[2]+local[12+k];
        for(u32 k=0;k<3;++k) out[k]=world[k]*l[0]+world[4+k]*l[1]+world[8+k]*l[2]+world[12+k];
      };
      transform(a,points[0]);transform(b,points[1]);UiPoint from,to;
      if(projectSegmentToScreen(*state.view,points[0],points[1],from,to)) builder.list.addLine(from,to,color,1.5f);
    });
  } else if(&component->type()==&scene::Joint::descriptor) {
    const auto &joint=static_cast<const scene::Joint &>(*component);
    if(!editorReferenceAccepts(*state.document,entity->id,scene::jointReferences[0],joint.connectedBody,true)) return;
    float a[16],b[16];if(!editorWorldMatrix(*state.document,entity->id,a)||!editorWorldMatrix(*state.document,static_cast<EditorEntityId>(joint.connectedBody),b)) return;
    float p[3],q[3];for(u32 k=0;k<3;++k) {p[k]=a[k]*joint.anchorA[0]+a[4+k]*joint.anchorA[1]+a[8+k]*joint.anchorA[2]+a[12+k];q[k]=b[k]*joint.anchorB[0]+b[4+k]*joint.anchorB[1]+b[8+k]*joint.anchorB[2]+b[12+k];}
    UiPoint from,to;if(projectSegmentToScreen(*state.view,p,q,from,to)) builder.list.addLine(from,to,builder.theme.color.accent,2);
    for(const float *point:{p,q}) {const auto projected=projectWorldToScreen(*state.view,point);if(projected.valid) builder.list.addRect({projected.screen.x-3,projected.screen.y-3,6,6},builder.theme.color.accent,3);}
  }
}

// A grade do chão e o gizmo. É o que separa "a cena está rodando" de "estou
// editando a cena": sem referência espacial e sem alça, o viewport é um vídeo.
void buildViewportOverlay(ScreenBuilder &builder, const UiRect &viewport) {
  const EditorScreenState &state = builder.state;
  if (state.view == nullptr || !isViewportValid(*state.view)) return;
  const UiTheme &theme = builder.theme;
  builder.list.pushClip(viewport);

  // A grade NÃO é desenhada aqui. Ela era uma lista de 258 segmentos na camada
  // de interface, depois de toda a cena e sem profundidade nenhuma: por
  // construção aparecia por cima de qualquer objeto. Agora é um passe do
  // renderer, com teste de profundidade e mistura contínua de escala — ver
  // renderer/grid_plan.h e editor/editor_grid.h. Aqui ficam só as ferramentas
  // e o HUD, que são coisas diferentes de "desenho no mundo".

  buildPhysicsOverlay(builder);
  const EditorEntity *entity = state.document->find(state.selection);
  if (entity != nullptr && state.tool != EditorGizmoMode::Select &&
      state.workspace == EditorWorkspace::Scene && !(waterRoute(*entity).count && state.waterTab==1)) {
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
  const bool code=builder.state.workspace==EditorWorkspace::Code;
  auto header=takeTop(content,code?62.0f:28.0f);
  if(code) {
    auto activity=header;const float width=activity.width/3;
    u32 index=0;
    for(const auto &item:std::array<std::pair<UiIcon,EditorWidget>,3>{{
        {UiIcon::IdeCode,EditorWidget::CodeFiles},{UiIcon::IdeFiles,EditorWidget::CodeFiles},
        {UiIcon::IdeConsole,EditorWidget::CodeConsole}}}) {
      auto hit=takeLeft(activity,width);auto label=hit;
      const auto image=takeTop(label,35);
      builder.list.addImage(centred(image,24,24),static_cast<UiImageId>(item.first),0xffffffff);
      const char *names[]{"Código","Arquivos","Console"};
      builder.label(label,names[index],index==1?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(index==1) builder.list.addRect({hit.x+8,hit.bottom()-2,hit.width-16,2},theme.color.accent,1);
      else builder.router.addRegion(hit,widgetId(item.second));
      ++index;
    }
    header=takeTop(content,44);
    builder.iconButton(takeRight(header,34),UiIcon::AssetsImport,widgetId(EditorWidget::ImportModel));
    auto newFile=takeRight(header,34),newFolder=takeRight(header,34);
    builder.list.addImage(centred(newFile,19,19),static_cast<UiImageId>(UiIcon::IdeAdd),0xffffffff);
    builder.list.addImage(centred(newFolder,19,19),static_cast<UiImageId>(UiIcon::IdeFiles),0xffffffff);
    builder.router.addRegion(newFile,widgetId(EditorWidget::CodeNew));
    builder.router.addRegion(newFolder,widgetId(EditorWidget::CodeNewFolder));
    builder.list.addRect({content.x,header.bottom(),content.width,1},theme.color.lineSoft);
  } else {
    builder.router.addRegion(header,widgetId(EditorWidget::FilesCollapse));
    builder.iconButton(takeRight(header,28),UiIcon::ScriptingCode,widgetId(EditorWidget::CodeOpen));
    builder.iconButton(takeRight(header,28),UiIcon::AssetsImport,widgetId(EditorWidget::ImportModel));
  }
  builder.label(header,"Arquivos",theme.color.text,theme.type.body);
  if(builder.state.filesCollapsed && !code) return;
  if(code) {
    auto bottom=takeBottom(content,58);
    auto folder=deflate(takeLeft(bottom,bottom.width*.48f),UiInsets::all(5));
    builder.list.addRect(folder,theme.color.raised,6);builder.list.addBorder(folder,theme.color.line,1,6);
    builder.label(folder,"Nova pasta",theme.color.text,theme.type.body,UiAlign::Center);
    builder.router.addRegion(folder,widgetId(EditorWidget::CodeNewFolder));
    auto add=deflate(bottom,UiInsets::all(5));
    builder.list.addRect(add,theme.color.accent,6);
    builder.label(add,"+  Add",theme.color.accentInk,theme.type.body,UiAlign::Center);
    builder.router.addRegion(add,widgetId(EditorWidget::CodeNew));
    auto project=takeBottom(content,48);
    builder.list.addRect({project.x,project.y,project.width,1},theme.color.lineSoft);
    const auto root=std::filesystem::path(files.rootPath()).filename().string();
    builder.label(deflate(project,UiInsets::symmetric(10,0)),("Projeto local  ·  "+root).c_str(),theme.color.textMuted,theme.type.caption);
  }
  // As acoes aparecem para o arquivo escolhido, e desaparecem com ele. Nomes em
  // texto e nao icones: apagar um recurso nao e uma acao para se adivinhar pelo
  // desenho.
  //
  // Ancoradas EMBAIXO, e nao sob o cabecalho: entre o cabecalho e a lista, elas
  // empurrariam as linhas 26 pixels para baixo no mesmo toque que escolhe uma
  // -- o proximo toque cairia numa linha diferente da que o dedo mirou.
  if(!builder.state.selectedFile.empty()) {
    auto actions=takeBottom(content,26);
    const bool confirming=builder.state.pendingResourceDelete==builder.state.selectedFile;
    auto action=[&](UiRect rect,const char *label,EditorWidget widget,UiColor color) {
      rect=deflate(rect,UiInsets::all(2));
      builder.list.addRect(rect,theme.color.raised,theme.radius.control);
      builder.label(rect,label,color,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(rect,widgetId(widget));
    };
    action(takeLeft(actions,actions.width*.5f),"Renomear",EditorWidget::FilesRename,theme.color.text);
    action(actions,confirming?"Apagar mesmo assim":"Apagar",EditorWidget::FilesDelete,
           confirming?theme.color.accent:theme.color.text);
  }
  if(builder.state.selectedFile.ends_with(".glb")) {
    auto actions=takeBottom(content,36);
    for(const auto &item:std::array<std::pair<const char *,EditorWidget>,2>{{
        {"Instanciar",EditorWidget::AssetInstantiate},{"Reimportar",EditorWidget::AssetReimport}}}) {
      auto button=deflate(takeLeft(actions,content.width*.5f),UiInsets::all(3));
      builder.list.addRect(button,theme.color.raised,theme.radius.control);
      builder.label(button,item.first,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(button,widgetId(item.second));
    }
  }
  if(!files.error().empty()) {
    builder.list.pushClip(content);builder.label(content,files.error().c_str(),theme.color.textDim,theme.type.caption);builder.list.popClip();return;
  }
  builder.list.pushClip(content);
  const auto &entries=files.tree();
  const float rowHeight=code?38.0f:24.0f;
  for(u32 i=builder.state.fileScroll;i<entries.size() && content.height>=rowHeight;++i) {
    const auto &entry=entries[i];auto row=takeTop(content,rowHeight);const auto hit=row;
    if(entry.relativePath==builder.state.selectedFile) {
      builder.list.addRect(hit,code?theme.color.accentWash:withAlpha(theme.color.accent,0.18f),theme.radius.control);
      if(code) builder.list.addRect({hit.x,hit.y,3,hit.height},theme.color.accent);
    }
    takeLeft(row,static_cast<float>(entry.depth)*14);
    if(code) {
      for(unsigned depth=0;depth<entry.depth;++depth)
        builder.list.addRect({hit.x+static_cast<float>(depth)*14+7,hit.y,1,hit.height},theme.color.lineSoft);
      const auto fold=takeLeft(row,14);
      if(entry.directory) builder.label(fold,entry.expanded?"v":">",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    }
    auto icon=takeLeft(row,22);
    builder.list.addImage(centred(icon,code?20:15,code?20:15),static_cast<UiImageId>(entry.directory?(code?UiIcon::IdeFiles:UiIcon::EditorAuthorFolder):
        entry.name.ends_with(".cs")?(code?UiIcon::IdeCode:UiIcon::ScriptingCode):UiIcon::AssetsFile),code?0xffffffff:theme.color.textDim);
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
                          static_cast<UiImageId>(cameraComponent(*entity)?UiIcon::EditorAuthorCamera:meshRenderer(*entity)?UiIcon::EditorAuthorObject:iconForKind(entity->kind)),
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
    if(editorNumericProperties[i].group==group && editorPropertyVisible(entity,i) &&
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
  if(pages<=1) return;
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

// Material por slot (Entrega 2). Uma coluna, de cima para baixo: qual slot,
// qual material ele usa, em que ALCANCE a edição vale, e os campos. O alcance
// fica sempre à vista: editar o recurso compartilhado muda todos os usos, e isso
// não pode ser uma surpresa.
void buildMaterialSlots(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;const auto &view=state.materialSlotView;
  if(!view.slots || content.height<40) return;
  // Uma linha só para slot e material: "<  Slot 2/3 · Vidro · da fonte  >". O
  // centro abre a escolha do material; as setas trocam de slot. Numa tela de
  // telefone, cada linha a menos é um campo a mais visível.
  auto use=takeTop(content,44);
  auto previous=takeLeft(use,32),next=takeRight(use,32);const auto useHit=use;
  const bool canPrevious=state.materialSlot>0,canNext=state.materialSlot+1<view.slots;
  builder.label(previous,"<",canPrevious?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
  builder.label(next,">",canNext?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
  if(canPrevious) builder.router.addRegion(previous,widgetId(EditorWidget::MaterialSlotPrevious));
  if(canNext) builder.router.addRegion(next,widgetId(EditorWidget::MaterialSlotNext));
  builder.list.addRect(deflate(use,UiInsets::all(2)),theme.color.raised,theme.radius.control);
  const std::string origin=view.missing?"ausente":view.shared?"do projeto":"da fonte";
  builder.label(takeTop(use,22),("Slot "+std::to_string(state.materialSlot+1)+"/"+std::to_string(view.slots)+" · "+view.name).c_str(),
                view.missing?theme.color.warning:theme.color.text,theme.type.caption);
  builder.label(use,("Material "+origin+(view.overridden?" · substituição local":"")).c_str(),view.overridden?theme.color.accent:theme.color.textMuted,theme.type.caption);
  builder.router.addRegion(useHit,widgetId(EditorWidget::MaterialChoose));
  if(content.height<34) return;

  auto scope=takeTop(content,34);
  auto instance=takeLeft(scope,scope.width*.5f-2);takeLeft(scope,4);
  const auto pill=[&](UiRect rect,const char *label,bool on,bool enabled,EditorWidget widget) {
    builder.list.addRect(deflate(rect,UiInsets::all(2)),on?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(rect,label,on?theme.color.accentInk:enabled?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(rect,widgetId(widget));
  };
  pill(instance,"Esta instância",!state.materialShared,true,EditorWidget::MaterialScopeInstance);
  pill(scope,"Compartilhado",state.materialShared,view.shared,EditorWidget::MaterialScopeShared);

  // A ação do alcance só ocupa linha se ainda couberem campos; senão ela
  // continua disponível na lista de escolha do material.
  if(content.height>=32+80) {
    if(!state.materialShared && view.overridden) {
      auto clear=takeTop(content,32);builder.label(clear,"Remover substituição local",theme.color.text,theme.type.caption);
      builder.router.addRegion(clear,widgetId(EditorWidget::MaterialClearOverride));
    } else if(!view.shared) {
      auto create=takeTop(content,32);builder.label(create,"Criar material do projeto a partir deste slot",theme.color.text,theme.type.caption);
      builder.router.addRegion(create,widgetId(EditorWidget::MaterialCreateShared));
    }
  }

  const u32 count=static_cast<u32>(scene::meshRendererNumbers.size());
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 field=page*perPage;field<count && field<(page+1)*perPage;++field) {
    auto row=takeTop(content,std::min(40.0f,content.height));
    builder.label(takeLeft(row,row.width*.62f),scene::meshRendererNumbers[field].name,theme.color.textDim,theme.type.caption);
    char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(view.values[field]));
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    builder.label(row,value,theme.color.text,theme.type.numeric,UiAlign::Center);
    builder.router.addRegion(row,widgetId(EditorWidget::MaterialNumberBase)+field);
  }
  if(pages>1) {
    const auto back=takeLeft(footer,36),forward=takeRight(footer,36);
    builder.label(back,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(forward,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(back,widgetId(EditorWidget::PropertyPrevious));
    if(page+1<pages) builder.router.addRegion(forward,widgetId(EditorWidget::PropertyNext));
    builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
}

// Escolha do material de um slot: o da fonte, um do projeto, ou um novo.
void buildMaterialPicker(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;const auto &view=state.materialSlotView;
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  builder.router.addRegion(back,widgetId(EditorWidget::MaterialPickerClose));
  builder.label(title,("Material do slot "+std::to_string(state.materialSlot+1)).c_str(),theme.color.text,theme.type.body);
  const auto option=[&](const std::string &label,const char *detail,bool current,u32 widget) {
    if(content.height<48) return;
    auto row=takeTop(content,48);const auto hit=row;
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    if(current) builder.list.addRect({row.x,row.y+4,3,row.height-8},theme.color.accent,1);
    // Recuo do texto: sem ele a primeira letra encostava na borda (visto no aparelho).
    takeLeft(row,12);
    builder.label(takeTop(row,25),label.c_str(),theme.color.text,theme.type.body);
    builder.label(row,detail,theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widget);
  };
  option("Material da fonte","o arquivo importado decide",!view.shared && !view.missing,widgetId(EditorWidget::MaterialUseSource));
  auto footer=takeBottom(content,36),previous=takeLeft(footer,36),next=takeRight(footer,36);
  const u32 rows=static_cast<u32>(state.projectMaterials.size())+1;
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height)/48));
  const u32 pages=std::max(1u,(rows+perPage-1)/perPage),page=std::min(state.meshPage,pages-1);
  for(u32 row=page*perPage;row<rows && row<(page+1)*perPage;++row) {
    if(row<state.projectMaterials.size())
      option(state.projectMaterials[row],"do projeto · compartilhado",view.shared && view.name==state.projectMaterials[row],
             widgetId(EditorWidget::MaterialChoiceBase)+row);
    else option("Novo material do projeto","com os valores deste slot",false,widgetId(EditorWidget::MaterialCreateShared));
  }
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
  builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::MeshPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::MeshNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

// Descriptor fields are addressed by stable instance/property IDs on edit.
// Widget indices exist only for one rendered frame, never in the scene archive.
void buildComponentFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,
                          const EditorComponentEntry &entry,u32 index) {
  const auto &theme=builder.theme;
  const auto *component=entity.components.at(index);if(!component || &component->type()!=entry.type) return;
  if(const auto *reason=entry.unavailable(entity)) builder.label(takeTop(content,30),reason,theme.color.textMuted,theme.type.caption);
  if(entry.type==&scene::Joint::descriptor) {
    const auto &j=static_cast<const scene::Joint &>(*component);
    builder.label(takeTop(content,28),j.kind==scene::JointKind::Hinge?"Limites/alvo: graus · velocidade: graus/s":"Âncoras e limites: unidades de cena",theme.color.textMuted,theme.type.caption);
  }
  const bool mesh=entry.type==&scene::MeshRenderer::descriptor;
  if(mesh) {
    auto tabs=takeTop(content,36);const float width=tabs.width*.5f;
    for(u32 i=0;i<2;++i) {
      auto tab=deflate(takeLeft(tabs,width),UiInsets::all(2));const bool selected=builder.state.meshTab==i;
      builder.list.addRect(tab,selected?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(tab,i?"Material":"Geometria",selected?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(tab,widgetId(i?EditorWidget::MeshMaterialTab:EditorWidget::MeshGeometryTab));
    }
  }
  // A aba Material é por SLOT, com alcance explícito; não é mais a lista de
  // números do componente, que só alcançava o primeiro slot.
  if(mesh && builder.state.meshTab) {buildMaterialSlots(builder,content);return;}
  struct Field {u32 kind,index;}; // 0 boolean, 1 enum, 2 number, 3 action, 4 object reference
  std::vector<Field> fields;
  if(!mesh || !builder.state.meshTab) {
    for(u32 i=0;i<entry.type->booleans.size();++i) fields.push_back({0,i});
    for(u32 i=0;i<entry.type->enums.size();++i) {
      if(entry.type==&scene::Joint::descriptor && i==1) {const auto &j=static_cast<const scene::Joint &>(*component);if(j.kind==scene::JointKind::Point||j.kind==scene::JointKind::Distance) continue;}
      fields.push_back({1,i});
    }
  }
  for(u32 i=0;i<entry.type->references.size();++i) fields.push_back({4,i});
  if(mesh && !builder.state.meshTab) {
    fields.push_back({3,widgetId(EditorWidget::MeshChoose)});
    fields.push_back({3,widgetId(EditorWidget::ToggleCastShadow)});
  } else {
    if(mesh) fields.push_back({3,widgetId(EditorWidget::MaterialRestore)});
    if(entry.type==&EditorCollider::descriptor && meshAsset(entity)) fields.push_back({3,widgetId(EditorWidget::ColliderFit)});
    for(u32 i=0;i<entry.type->numbers.size();++i) {
      if(entry.type==&EditorCollider::descriptor) {
        const auto *c=static_cast<const scene::Collider *>(component);
        if(i<5 && !(c->shape==scene::ColliderShape::Box?i<3:c->shape==scene::ColliderShape::Sphere?i==3:i>=3)) continue;
      }
      if(entry.type==&scene::Joint::descriptor) {
        const auto &j=static_cast<const scene::Joint &>(*component);
        if(i>=6&&i<12&&(j.kind==scene::JointKind::Point||j.kind==scene::JointKind::Distance)) continue;
        if(i>=12&&j.kind==scene::JointKind::Point) continue;
        if(i>=14&&(!j.motor||j.kind==scene::JointKind::Distance)) continue;
      }
      fields.push_back({2,i});
    }
  }
  if(content.height<40) return;
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(fields.size())+perPage-1)/perPage);
  const u32 page=std::min(builder.state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 row=page*perPage;row<fields.size() && row<(page+1)*perPage;++row) {
    const auto f=fields[row];auto slot=takeTop(content,std::min(40.0f,content.height));const auto hit=slot;
    if(f.kind==0) {
      const auto &property=entry.type->booleans[f.index];auto toggle=takeRight(slot,44);
      builder.label(slot,property.name,theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.index<<8));
    } else if(f.kind==1) {
      const auto &property=entry.type->enums[f.index];
      builder.label(takeLeft(slot,slot.width*.38f),property.name,theme.color.textDim,theme.type.caption);
      const char *label="Valor inválido";
      for(const auto &option:property.options) if(option.value==property.read(*component)) label=option.name;
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(slot,label,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(hit,widgetId(EditorWidget::ComponentEnumBase)+index+(f.index<<8));
    } else if(f.kind==4) {
      const auto &property=entry.type->references[f.index];const auto target=property.read(*component);
      builder.label(takeTop(slot,17),property.name,theme.color.textMuted,theme.type.caption);
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      builder.label(slot,!target?property.nullLabel:object?object->name:"Objeto ausente",theme.color.text,theme.type.caption);
      builder.list.addImage(centred(takeRight(slot,24),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      builder.router.addRegion(hit,widgetId(EditorWidget::ComponentReferenceBase)+index+(f.index<<8));
    } else if(f.kind==2) {
      const auto &property=entry.type->numbers[f.index];
      builder.label(takeLeft(slot,slot.width*.62f),property.name,theme.color.textDim,theme.type.caption);
      char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(property.read(*component)));
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(slot,value,theme.color.text,theme.type.numeric,UiAlign::Center);
      builder.router.addRegion(slot,widgetId(EditorWidget::ComponentNumberBase)+index+(f.index<<8));
    } else if(f.index==widgetId(EditorWidget::MeshChoose)) {
      builder.list.addImage(centred(takeRight(slot,30),22,22),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.text);
      const auto ref=meshAsset(entity);const auto text=ref?"Malha "+std::to_string(ref):"Escolher malha";
      builder.label(slot,text.c_str(),theme.color.text,theme.type.caption);
      builder.router.addRegion(hit,f.index);
    } else if(f.index==widgetId(EditorWidget::ToggleCastShadow)) {
      auto toggle=takeRight(slot,44);builder.label(slot,"Projetar sombra",theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,entity.castShadow,f.index);
    } else {
      builder.label(slot,f.index==widgetId(EditorWidget::MaterialRestore)?"Restaurar material da origem":"Ajustar à malha",theme.color.text,theme.type.caption);
      builder.router.addRegion(hit,f.index);
    }
  }
  if(pages>1) {
    const auto previous=takeLeft(footer,36),next=takeRight(footer,36);
    builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::PropertyNext));
    const auto label=std::to_string(page+1)+" / "+std::to_string(pages);
    builder.label(footer,label.c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
}

void buildMeshPicker(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);builder.router.addRegion(back,widgetId(EditorWidget::MeshPickerClose));
  builder.label(title,"Geometria",theme.color.text,theme.type.body);
  auto search=takeTop(content,36);builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(search,state.meshQuery.empty()?"Buscar malha ou material":state.meshQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::MeshSearch));
  auto footer=takeBottom(content,36),previous=takeLeft(footer,36),next=takeRight(footer,36);
  std::vector<u32> matches;const auto query=editorSearchKey(state.meshQuery);
  if(state.resources) for(u32 i=0;i<state.resources->assetCount();++i) {
    if(state.resources->materialFlagsForAsset(i)&renderer::MapMaterialWater) continue;
    const auto *asset=state.resources->asset(i);
    const auto text="Malha "+std::to_string(i+1)+" material "+std::to_string(asset->materialIndex);
    if(query.empty() || editorSearchKey(text).find(query)!=std::string::npos) matches.push_back(i);
  }
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-34)/50));
  const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage),page=std::min(state.meshPage,pages-1);
  auto clear=takeTop(content,34);builder.label(clear,"Sem malha",theme.color.textDim,theme.type.caption);builder.router.addRegion(clear,widgetId(EditorWidget::MeshClear));
  for(u32 row=page*perPage;row<matches.size() && row<(page+1)*perPage;++row) {
    if(content.height<40) break;
    const auto i=matches[row];const auto *asset=state.resources->asset(i);auto slot=takeTop(content,50);const auto hit=slot;
    const bool selected=meshAsset(entity)==i+1;builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    if(selected) builder.list.addRect({slot.x,slot.y+4,3,slot.height-8},theme.color.accent,1);
    builder.list.addImage(centred(takeLeft(slot,40),28,28),static_cast<UiImageId>(UiIcon::EditorAuthorObject),0xffffffff);
    const auto name="Malha "+std::to_string(i+1);builder.label(takeTop(slot,25),name.c_str(),theme.color.text,theme.type.body);
    const auto detail=std::to_string(asset->indexCount/3)+" triângulos · material "+std::to_string(asset->materialIndex);
    builder.label(slot,detail.c_str(),theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::MeshChoiceBase)+i);
  }
  if(matches.empty()) builder.label(content,"Nenhuma malha nesta biblioteca",theme.color.textMuted,theme.type.caption);
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::MeshPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::MeshNext));
  const auto text=std::to_string(page+1)+" / "+std::to_string(pages);builder.label(footer,text.c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

const EditorScriptType *scriptSchema(const EditorScreenState &state,std::string_view id) {
  if(state.code) for(const auto &type:state.code->scriptTypes()) if(type.id==id) return &type;
  return nullptr;
}
void buildScriptFields(ScreenBuilder &builder,UiRect content,const scene::ScriptBehavior &script,u32 index) {
  const auto &theme=builder.theme;
  auto enabled=takeTop(content,38);builder.label(enabled,"Ativo",theme.color.textDim,theme.type.caption);
  builder.toggle(takeRight(enabled,44),script.enabled,widgetId(EditorWidget::ScriptEnabledBase)+index);
  auto source=takeTop(content,38);builder.label(source,"Abrir código",theme.color.text,theme.type.caption);
  builder.router.addRegion(source,widgetId(EditorWidget::ScriptSourceBase)+index);
  const auto *schema=scriptSchema(builder.state,script.scriptType);
  if(!schema) {builder.label(content,"Tipo não resolvido; dados preservados",theme.color.textMuted,theme.type.caption);return;}
  usize orphaned=0;
  for(const auto &value:script.properties) if(std::none_of(schema->properties.begin(),schema->properties.end(),[&](const auto &property){return property.id==value.id;})) ++orphaned;
  if(orphaned) builder.label(takeTop(content,30),(std::to_string(orphaned)+" campos preservados fora do schema").c_str(),theme.color.warning,theme.type.caption);
  const u32 visible=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-32)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(schema->properties.size())+visible-1)/visible);
  const u32 page=std::min(builder.state.scriptPropertyPage,pages-1);
  auto footer=takeBottom(content,32);
  for(u32 field=page*visible;field<schema->properties.size() && field<(page+1)*visible;++field) {
    const auto &property=schema->properties[field];
    auto row=takeTop(content,40);const auto hit=row;
    builder.label(takeLeft(row,row.width*.43f),property.name.c_str(),theme.color.textDim,theme.type.caption);
    const char *value="Padrão do código";
    for(const auto &p:script.properties) if(p.id==property.id) value=p.valueType==property.valueType?p.value.c_str():"Tipo alterado";
    std::string referenceName;
    if(property.valueType=="object") {
      u64 target=0;for(const auto &p:script.properties) if(p.id==property.id&&p.valueType=="object") {std::istringstream in(p.value);in>>target;}
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      referenceName=!target?"Escolher objeto":object?object->name:"Objeto ausente";value=referenceName.c_str();
    }
    builder.list.addRect(row,theme.color.raised,theme.radius.control);
    builder.label(row,value,theme.color.text,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::ScriptFieldBase)+index+(field<<8));
  }
  if(pages>1) {
    auto previous=takeLeft(footer,40),next=takeRight(footer,40);
    builder.label(previous,"<",theme.color.text,theme.type.caption,UiAlign::Center);
    builder.label(next,">",theme.color.text,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ScriptFieldsPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ScriptFieldsNext));
    const auto text=std::to_string(page+1)+" / "+std::to_string(pages);
    builder.label(footer,text.c_str(),theme.color.textDim,theme.type.caption,UiAlign::Center);
  }
}
void buildReferencePicker(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  const auto *property=state.referenceScript?&editorAnyObjectReference:editorReferenceProperty(entity,state.referenceInstance,state.referenceProperty);
  auto header=takeTop(content,36),back=takeLeft(header,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);builder.router.addRegion(back,widgetId(EditorWidget::ReferenceClose));
  builder.label(header,property?property->name:"Referência ausente",theme.color.text,theme.type.body);
  if(!property) return;
  auto search=deflate(takeTop(content,38),UiInsets::all(2));builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(search,state.referenceQuery.empty()?"Buscar objeto":state.referenceQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::ReferenceSearch));
  auto clear=takeTop(content,34);builder.label(clear,property->nullLabel,theme.color.textDim,theme.type.caption);builder.router.addRegion(clear,widgetId(EditorWidget::ReferenceClear));
  auto footer=takeBottom(content,32),previous=takeLeft(footer,36),next=takeRight(footer,36);
  const auto choices=editorReferenceChoices(*state.document,entity.id,*property,state.referenceQuery);
  const u32 visible=std::max(1u,static_cast<u32>(std::max(0.0f,content.height)/48));
  const u32 pages=std::max(1u,(static_cast<u32>(choices.size())+visible-1)/visible),page=std::min(state.referencePage,pages-1);
  for(u32 i=page*visible;i<choices.size()&&i<(page+1)*visible;++i) {
    auto row=takeTop(content,48);const auto hit=row;const auto *object=state.document->find(choices[i]);
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(takeLeft(row,36),24,24),static_cast<UiImageId>(UiIcon::EditorAuthorObject),0xffffffff);
    builder.label(takeTop(row,25),object->name,theme.color.text,theme.type.body);
    const auto *parent=state.document->find(object->parent);
    const auto detail=std::string(parent?parent->name:"Cena")+" · "+std::to_string(object->id)+(object->active?"":" · inativo");
    builder.label(row,detail.c_str(),theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::ReferenceChoiceBase)+i);
  }
  if(choices.empty()) builder.label(content,"Nenhum objeto compatível",theme.color.textMuted,theme.type.caption);
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ReferencePrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ReferenceNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

void buildComponents(ScreenBuilder &builder, UiRect content, const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const bool same=state.componentSelection==entity.id,adding=same&&state.addingComponent;
  if(same && state.referenceInstance) {buildReferencePicker(builder,content,entity);return;}
  if(same && state.meshPicker && meshRenderer(entity)) {buildMeshPicker(builder,content,entity);return;}
  if(same && state.materialPicker && meshRenderer(entity)) {buildMaterialPicker(builder,content);return;}
  auto footer=takeBottom(content,42);auto button=deflate(footer,UiInsets::all(2));
  builder.list.addRect(button,theme.color.raised,theme.radius.control);
  builder.router.addRegion(button,widgetId(EditorWidget::AddComponentMenu));
  builder.list.addImage(centred(takeLeft(button,40),28,28),static_cast<UiImageId>(UiIcon::ComponentAdd),0xffffffff);
  builder.label(button,adding?"Add · fechar":"Add",theme.color.text,theme.type.body);
  struct Card {const EditorComponentEntry *native=nullptr;const scene::ComponentValue *value=nullptr;u32 index=0;bool transform=false;};
  std::vector<Card> cards;
  if(adding) {
    auto search=takeTop(content,34),clear=takeRight(search,32);builder.list.addRect(search,theme.color.raised,theme.radius.control);
    builder.label(search,state.componentQuery.empty()?"Buscar componente":state.componentQuery.c_str(),theme.color.textDim,theme.type.caption);
    builder.router.addRegion(search,widgetId(EditorWidget::ComponentSearch));
    if(!state.componentQuery.empty()) {builder.label(clear,"x",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.router.addRegion(clear,widgetId(EditorWidget::ComponentSearchClear));}
    const char *categories[]{"Todos","Câmera","Visual","Física","Código"};
    auto category=takeTop(content,32);builder.label(category,categories[state.componentCategory%5],theme.color.accent,theme.type.caption);
    builder.label(takeRight(category,24),">",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.router.addRegion(category,widgetId(EditorWidget::ComponentCategory));
    const auto query=editorSearchKey(state.componentQuery);
    for(u32 i=0;i<editorComponentCatalog.size();++i) {
      const auto &entry=editorComponentCatalog[i];const u32 category=static_cast<u32>(entry.category);
      if(state.componentCategory && state.componentCategory!=category) continue;
      if(!query.empty() && editorSearchKey(std::string(entry.name)+" "+entry.description+" "+std::string(entry.type->id)).find(query)==std::string::npos) continue;
      cards.push_back({&entry,nullptr,i});
    }
    if(state.code && (!state.componentCategory||state.componentCategory==4)) for(u32 i=0;i<state.code->scriptTypes().size();++i) {
      const auto &type=state.code->scriptTypes()[i];
      if(query.empty()||editorSearchKey(type.name+" "+type.id).find(query)!=std::string::npos) cards.push_back({nullptr,nullptr,i});
    }
  } else {
    cards.push_back({nullptr,nullptr,0,true});
    for(u32 i=0;i<entity.components.size();++i) {
      const auto *v=entity.components.at(i);bool known=false;
      for(u32 type=0;type<editorComponentCatalog.size();++type) if(&v->type()==editorComponentCatalog[type].type) {
        cards.push_back({&editorComponentCatalog[type],v,i});known=true;break;
      }
      if(!known && (scene::scriptBehavior(v)||v->unresolved())) cards.push_back({nullptr,v,i});
    }
  }
  if(!adding && same) {
    const auto focused=std::find_if(cards.begin(),cards.end(),[&](const Card &item) {
      if(item.transform) return state.expandedComponent=="astra.transform";
      if(item.native) return (item.value&&state.expandedNative==item.value->instanceId()) || (item.value&&state.nativeMenu==item.value->instanceId());
      const auto *script=scene::scriptBehavior(item.value);
      return script&&(state.expandedScript==script->instanceId()||state.scriptMenu==script->instanceId());
    });
    if(focused!=cards.end()) {const auto selected=*focused;cards.assign(1,selected);}
  }
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-32)/(adding?58:48)));
  const u32 pages=std::max(1u,(static_cast<u32>(cards.size())+perPage-1)/perPage);
  const u32 page=same?std::min(state.componentPage,pages-1):0;
  builder.componentPage=page;
  if(pages>1) {
    auto pager=takeBottom(content,32),previous=takeLeft(pager,36),next=takeRight(pager,36);
    builder.label(previous,"<",theme.color.text,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.text,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ComponentPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ComponentNext));
    const auto text=std::to_string(page+1)+" / "+std::to_string(pages);builder.label(pager,text.c_str(),theme.color.textDim,theme.type.caption,UiAlign::Center);
  }
  const u32 end=std::min(static_cast<u32>(cards.size()),(page+1)*perPage);
  for(u32 card=page*perPage;card<end;++card) {
    if(content.height<40) break;
    const auto &item=cards[card];const auto *script=scene::scriptBehavior(item.value);const auto *schema=script?scriptSchema(state,script->scriptType):nullptr;
    const bool open=same&&(item.transform?state.expandedComponent=="astra.transform":item.native?(item.value&&state.expandedNative==item.value->instanceId()):script&&state.expandedScript==script->instanceId());
    const bool menu=same&&!item.transform&&(item.native?(item.value&&state.nativeMenu==item.value->instanceId()):script&&state.scriptMenu==script->instanceId());
    const EditorScriptType *newType=adding&&!item.native?&state.code->scriptTypes()[item.index]:nullptr;
    std::string title=item.transform?"Transformação":item.native?item.native->name:newType?newType->name:script?(schema?schema->name:script->scriptType):std::string(item.value->type().id);
    if(item.value && item.native && item.native->type->allowMultiple) title+=" · "+std::to_string(item.value->instanceId());
    const auto icon=item.transform?UiIcon::EditorAuthorMove:item.native?item.native->icon:UiIcon::ScriptingCode;
    const char *reason=adding&&item.native?(!item.native->type->allowMultiple&&entity.components.find(*item.native->type)?"Já adicionado":item.native->unavailable(entity)):nullptr;
    auto row=takeTop(content,adding?54.0f:44.0f);const auto hit=row;
    builder.list.addRect(row,theme.color.raised,theme.radius.control);
    if(open) builder.list.addRect({row.x,row.y+4,3,row.height-8},theme.color.accent,1);
    if(!adding) builder.label(takeLeft(row,20),open?"v":">",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.list.addImage(centred(takeLeft(row,36),28,28),static_cast<UiImageId>(icon),0xffffffff);
    auto more=takeRight(row,adding||item.transform?0:32);
    builder.label(adding?takeTop(row,27):row,title.c_str(),reason?theme.color.textMuted:theme.color.text,theme.type.body);
    if(adding) builder.label(row,reason?reason:item.native?item.native->description:"Comportamento C#",theme.color.textMuted,theme.type.caption);
    if(adding) {
      if(!reason) builder.router.addRegion(hit,widgetId(item.native?EditorWidget::ComponentAddBase:EditorWidget::ScriptAddBase)+item.index);
    } else if(item.transform||item.native||script) {
      builder.router.addRegion({hit.x,hit.y,hit.width-(item.transform?0:32),hit.height},item.transform?widgetId(EditorWidget::TransformFold):widgetId(item.native?EditorWidget::ComponentFoldBase:EditorWidget::ScriptFoldBase)+item.index);
      if(!item.transform) {
        builder.list.addImage(centred(more,20,20),static_cast<UiImageId>(UiIcon::EditorAuthorMore),theme.color.textDim);
        builder.router.addRegion(more,widgetId(item.native?EditorWidget::ComponentMenuBase:EditorWidget::ScriptMenuBase)+item.index);
      }
      if(menu) {
        if(item.native) {
          const EditorWidget actions[]{EditorWidget::ComponentCopyBase,EditorWidget::ComponentPasteBase,EditorWidget::ComponentResetBase};
          const char *labels[]{"Copiar valores","Colar valores","Restaurar padrão"};
          for(u32 i=0;i<3;++i) {auto action=takeTop(content,34);builder.label(action,labels[i],theme.color.text,theme.type.caption);
            if(i!=1||(state.componentClipboard && &state.componentClipboard->type()==item.native->type)) builder.router.addRegion(action,widgetId(actions[i])+item.index);}
        }
        auto remove=takeTop(content,34);builder.label(remove,"Remover componente",theme.color.text,theme.type.caption);
        builder.router.addRegion(remove,widgetId(item.native?EditorWidget::ComponentRemoveBase:EditorWidget::ScriptRemoveBase)+item.index);
      }
      if(open) {
        auto fields=takeTop(content,std::max(0.0f,content.height-(end-card-1)*48.0f-4));
        builder.list.pushClip(fields);
        if(item.transform) {
          const u32 rows=std::max(1u,std::min(3u,static_cast<u32>(std::max(0.0f,fields.height-30)/34)));
          const u32 page=std::min(state.propertyPage,(3u+rows-1)/rows-1);
          auto pager=rows<3?takeBottom(fields,30):UiRect{};
          for(u32 i=page*rows;i<3 && i<(page+1)*rows;++i) {
            if(fields.height<34) break;
            buildTransformRow(builder,fields,i==0?"Posição":i==1?"Rotação":"Escala",i==0?entity.transform.position:i==1?entity.transform.rotationDegrees:entity.transform.scale,i,i==1?1:3);
          }
          if(rows<3) {
            auto prev=takeLeft(pager,36),next=takeRight(pager,36);
            builder.label(prev,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
            if(page) builder.router.addRegion(prev,widgetId(EditorWidget::PropertyPrevious));
            if((page+1)*rows<3) builder.router.addRegion(next,widgetId(EditorWidget::PropertyNext));
          }
        } else if(item.native) buildComponentFields(builder,fields,entity,*item.native,item.index);
        else buildScriptFields(builder,fields,*script,item.index);
        builder.list.popClip();
      }
    } else {
      // Missing records stay visible without claiming that they execute.
      builder.label(takeTop(content,24),"Indisponível · dados preservados",theme.color.textMuted,theme.type.caption);
    }
    takeTop(content,4);
  }
  if(cards.empty()) builder.label(content,"Nenhum componente encontrado",theme.color.textMuted,theme.type.caption);
}

// Vínculo com a fonte importada (M08.2): uma linha que diz de onde o objeto
// veio e quanto ele difere da fonte; as ações reais abrem numa lista, no mesmo
// padrão do menu de componente, sem barra permanente nova.
void buildImportLinkCard(ScreenBuilder &builder, UiRect &content) {
  const auto &state=builder.state;const auto &theme=builder.theme;const auto &view=state.importLink;
  if(!view.linked || content.height<48) return;
  auto row=takeTop(content,44);const auto hit=row;
  builder.list.addRect(row,theme.color.raised,theme.radius.control);
  if(view.orphan || view.overrides) builder.list.addRect({row.x,row.y+4,3,row.height-8},view.orphan?theme.color.warning:theme.color.accent,1);
  builder.list.addImage(centred(takeLeft(row,36),24,24),static_cast<UiImageId>(UiIcon::AssetsImport),0xffffffff);
  auto more=takeRight(row,32);
  builder.list.addImage(centred(more,20,20),static_cast<UiImageId>(UiIcon::EditorAuthorMore),theme.color.textDim);
  const std::string title=view.orphan?std::string("Removido da fonte"):view.source;
  std::string detail=view.orphan?std::string("órfão · dados locais preservados"):view.node.empty()?std::string("vinculado"):view.node;
  if(!view.orphan) {
    const auto changes=std::popcount(view.overrides);
    detail+=changes?" · "+std::to_string(changes)+(changes==1?" alteração local":" alterações locais"):std::string(" · igual à fonte");
  }
  builder.label(takeTop(row,24),title.c_str(),theme.color.text,theme.type.body);
  builder.label(row,detail.c_str(),view.orphan?theme.color.warning:view.overrides?theme.color.accent:theme.color.textMuted,theme.type.caption);
  builder.router.addRegion(hit,widgetId(EditorWidget::ImportLinkMenu));
  if(state.importLinkMenu) {
    struct Action {const char *label;u32 id;};
    std::vector<Action> actions;
    if(view.orphan) {
      actions.push_back({"Manter como objeto independente",widgetId(EditorWidget::ImportLinkKeep)});
      actions.push_back({"Apagar objeto",widgetId(EditorWidget::ImportLinkDelete)});
    } else {
      const struct {u32 bit;const char *label;} fields[]{
          {ImportOverrideName,"Reverter nome à fonte"},{ImportOverridePosition,"Reverter posição à fonte"},
          {ImportOverrideRotation,"Reverter rotação à fonte"},{ImportOverrideScale,"Reverter escala à fonte"},
          {ImportOverrideParent,"Reverter pai à fonte"},{ImportOverrideMesh,"Reverter malha à fonte"}};
      for(const auto &field:fields) if(view.overrides&field.bit) actions.push_back({field.label,widgetId(EditorWidget::ImportLinkRevertBase)+field.bit});
      if(std::popcount(view.overrides)>1) actions.push_back({"Reverter tudo à fonte",widgetId(EditorWidget::ImportLinkRevertBase)+ImportOverrideAll});
      actions.push_back({"Desvincular instância",widgetId(EditorWidget::ImportLinkUnlink)});
    }
    for(const auto &action:actions) {
      if(content.height<34) break;
      auto line=takeTop(content,34);
      builder.label(line,action.label,theme.color.text,theme.type.caption);
      builder.router.addRegion(line,action.id);
    }
  }
  takeTop(content,4);
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
                        static_cast<UiImageId>(cameraComponent(*entity)?UiIcon::EditorAuthorCamera:meshRenderer(*entity)?UiIcon::EditorAuthorObject:iconForKind(entity->kind)), theme.color.text);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::EditorAuthorMore,
                     widgetId(EditorWidget::InspectorMenu));
  takeRight(header, theme.spacing.tiny);
  builder.toggle(takeRight(header, kToggleWidth + 4.0f), entity->active,
                 widgetId(EditorWidget::InspectorActive));
  const float half = header.height * 0.5f;
  builder.label({header.x, header.y, header.width, half}, entity->name, theme.color.text,
                theme.type.cardName);
  builder.label({header.x, header.y + half, header.width, half}, // O vínculo com a fonte tem linha própria, não é card: não entra na contagem.
                (std::to_string(entity->components.size()-(scene::importLink(entity->components)?1u:0u))+" componentes").c_str(),
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
      if(!waterRoute(*entity).count) {builder.label(content,"Crie um rio para editar o traçado",theme.color.textDim,theme.type.body);return;}
      UiRect controls=takeTop(content,40);
      const EditorWidget actions[]{EditorWidget::RoutePointPrevious,EditorWidget::RoutePointNext,EditorWidget::RoutePointAdd,EditorWidget::RoutePointRemove};
      const char *names[]{"<",">","+ ponto","Remover"};const float buttonWidth=controls.width/4;
      for(u32 i=0;i<4;++i) {auto button=takeLeft(controls,buttonWidth);builder.label(button,names[i],theme.color.text,theme.type.caption,UiAlign::Center);builder.router.addRegion(button,widgetId(actions[i]));}
      char label[40];std::snprintf(label,sizeof(label),"PONTO %u / %u",builder.state.routePoint+1,waterRoute(*entity).count);
      builder.label(takeTop(content,24),label,theme.color.accent,theme.type.caption);
      buildPropertyPage(builder,content,*entity,EditorPropertyGroup::Route);
    } else {
      if(builder.state.waterTab==2) {
        auto row=takeTop(content,36);builder.label(row,"Volume de água / flutuação",theme.color.text,theme.type.caption);
        builder.toggle(takeRight(row,44),waterBody(*entity).physicsEnabled,widgetId(EditorWidget::ToggleWaterPhysics));
      }
      buildPropertyPage(builder,content,*entity,EditorPropertyGroup::WaterBody);
    }
    return;
  }
  takeTop(content,6);
  buildImportLinkCard(builder,content);
  buildComponents(builder,content,*entity);
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

void buildToolRail(ScreenBuilder &builder, const UiRect &viewport, bool compact) {
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
  const UiRect vertical{viewport.x + theme.spacing.small,
                    viewport.y + (viewport.height - height) * 0.5f,
                    kToolButton + theme.spacing.tiny * 2.0f, height};
  const UiRect rail = compact ? UiRect{viewport.x+8,viewport.y+56,height,kToolButton+theme.spacing.tiny*2} : vertical;
  builder.list.addRect(rail, withAlpha(theme.color.silhouette, 0.92f), theme.radius.control);
  builder.router.addBlocker(rail);
  UiRect content = deflate(rail, UiInsets::all(theme.spacing.tiny));
  for (const Tool &tool : tools) {
    const UiRect slot = compact ? takeLeft(content,kToolButton) : takeTop(content, kToolButton);
    if(compact) takeLeft(content,theme.spacing.tiny); else takeTop(content, theme.spacing.tiny);
    builder.iconButton(slot, tool.icon, widgetId(tool.widget), builder.state.tool == tool.mode);
  }
}


// Um campo de texto EMBUTIDO, desenhado pelo editor enquanto o teclado do
// sistema esta aberto.
//
// A divisao e essa: o teclado e do Android, o campo e do editor. A ponte JNI
// nao desenha nada -- ela carrega o IME, entrega o texto a cada tecla e diz
// quanto da tela o teclado ocupa. Antes disso a edicao inteira acontecia num
// `AlertDialog` que cobria a tela: o usuario nao via o objeto que estava
// renomeando nem o valor que estava mudando enquanto digitava, e a busca so
// filtrava depois de confirmar.
//
// O cursor e um glifo inserido no texto desenhado, e nao um retangulo medido.
// A largura do texto so e resolvida na construcao das instancias, depois deste
// passo; medir aqui exigiria uma segunda copia da metrica da fonte, que e
// exatamente a forma de defeito que ja custou caro neste editor.
bool platformFieldActive(const EditorScreenState &state) {
  if (!state.platformTextInput || state.editingCode) return false;
  return state.renameEntity != kInvalidEntity || state.editingHierarchySearch ||
         state.editingCreationSearch || state.editingComponentSearch || state.editingMeshSearch ||
         state.editingReferenceSearch || state.numericField != 0 ||
         state.editingScriptInstance != 0 || state.creatingScript || state.searchingCode ||
         state.renamingResource || state.goingToLine || state.creatingCodeFolder || state.searchingConsole;
}

const char *platformFieldTitle(const EditorScreenState &state) {
  if (state.numericField != 0) return "Valor";
  if (state.renamingResource) return "Arquivo";
  if (state.editingScriptInstance != 0) return "Campo";
  if (state.creatingScript) return state.scriptTemplate==EditorCodeWorkspace::HelperTemplate?"Auxiliar C#":"Componente C#";
  if (state.searchingCode) return "Localizar";
  if (state.searchingConsole) return "Console";
  if (state.goingToLine) return "Ir para linha";
  if (state.creatingCodeFolder) return "Nova pasta";
  if (state.renameEntity != kInvalidEntity) return "Nome";
  return "Buscar";
}

void buildPlatformTextField(ScreenBuilder &builder) {
  const auto &state = builder.state;
  const UiTheme &theme = builder.theme;
  if (!platformFieldActive(state)) return;
  // A borda de cima do teclado. Sem ela o campo nasce embaixo da tela, que e
  // meio caminho de volta para o dialogo.
  const float keyboard = state.surface.height * state.platformImeFraction;
  const float height = 56.0f;
  const float margin = 8.0f;
  float top = state.surface.bottom() - keyboard - height - margin;
  top = std::max(top, state.surface.y + margin);
  const UiRect bar{state.surface.x + margin, top, state.surface.width - margin * 2.0f, height};
  // Bloqueia o toque na barra, e SO nela: o resto da tela continua visivel e
  // rolavel, que e a diferenca entre editar embutido e editar num modal.
  builder.router.addBlocker(bar);
  builder.list.addRect(bar, theme.color.surface, theme.radius.control);
  builder.list.addRect({bar.x, bar.bottom() - 2.0f, bar.width, 2.0f}, theme.color.accent);
  UiRect content = deflate(bar, UiInsets::all(8.0f));
  const UiRect title = takeLeft(content, 92.0f);
  builder.label(title, platformFieldTitle(state), theme.color.textFaint, theme.type.caption,
                UiAlign::Start);
  const auto caret = std::min<usize>(state.platformCaret, state.platformDraft.size());
  std::string shown = state.platformDraft.substr(0, caret);
  shown += "|";
  shown += state.platformDraft.substr(caret);
  builder.label(content, shown, theme.color.text,
                state.numericField != 0 ? theme.type.numeric : theme.type.body, UiAlign::Start);
}

} // namespace


// A lista que abre no icone de menu da barra do IDE.
//
// O que esta aqui e o que se usa de vez em quando: escrito por extenso, porque
// um nome legivel vale mais do que um decimo icone que ninguem decifra. E fica
// a UM toque -- nao atras de um gesto que so quem ja sabe descobre.
void buildCodeMenu(ScreenBuilder &builder, const UiRect &anchor, EditorScreenLayout &layout) {
  if (!builder.state.codeMenu) return;
  const auto &theme = builder.theme;
  const auto *workspace = builder.state.code;
  const auto *buffer = workspace ? workspace->active() : nullptr;
  struct Item { const char *label; EditorWidget action; bool enabled; };
  const Item items[] = {
    {"Novo componente C#", EditorWidget::CodeNew, true},
    {"Novo auxiliar C#", EditorWidget::CodeNewHelper, true},
    {"Modelos de código…", EditorWidget::CodeTemplates, true},
    {builder.state.codeBuildBusy ? "Compilando…" : "Recompilar projeto", EditorWidget::CodeApply,
     builder.state.codeCompilerAvailable && !builder.state.codeBuildBusy && !builder.state.codeComposing},
    {"Salvar tudo", EditorWidget::CodeSaveAll, workspace != nullptr},
    {"Ir para linha…", EditorWidget::CodeGoLine, buffer != nullptr},
    {"Desfazer", EditorWidget::CodeUndo, buffer && !buffer->undo.empty()},
    {"Refazer", EditorWidget::CodeRedo, buffer && !buffer->redo.empty()},
    {"Fechar arquivo", EditorWidget::CodeClose, buffer != nullptr},
  };
  const float row = std::min(42.0f,std::max(20.0f,(builder.state.surface.bottom()-anchor.bottom()-12)/static_cast<float>(std::size(items))));
  const float width = 220.0f;
  UiRect panel{anchor.right() - width - 8.0f, anchor.bottom() + 2.0f, width,
               row * static_cast<float>(std::size(items)) + 8.0f};
  panel.x = std::min(panel.x, builder.state.surface.right() - width - 8.0f);
  layout.codeMenu = panel;
  // Bloqueia a TELA inteira, e nao so o painel: um menu aberto que deixa o
  // toque passar para o editor atras dele fecha e edita no mesmo gesto.
  builder.router.addBlocker(builder.state.surface);
  builder.router.addRegion(builder.state.surface, widgetId(EditorWidget::CodeMenu));
  builder.list.addRect(panel, theme.color.raised, theme.radius.card);
  UiRect content = deflate(panel, UiInsets::all(4.0f));
  for (const auto &item : items) {
    auto line = takeTop(content, row);
    builder.label(deflate(line, UiInsets::all(6.0f)), item.label,
                  item.enabled ? theme.color.text : theme.color.textFaint, theme.type.body);
    if (item.enabled) builder.router.addRegion(line, widgetId(item.action));
  }
}

// O console editorial: compilador, scripts e editor no mesmo lugar.
//
// Antes disso o compilador falava numa lista apertada aqui dentro e os scripts
// falavam no `logcat`, que nao existe para quem esta com o aparelho na mao. Um
// erro de execucao simplesmente nao tinha onde aparecer.
//
// A lista e VIRTUALIZADA: so as linhas que cabem sao desenhadas. Quinhentas
// linhas viram quinhentos retangulos e quinhentos textos por quadro, e o
// console e justamente o painel que enche quando a coisa esta indo mal.
// Console rows are a viewport into bounded events. Selection is an event ID,
// so eviction or a new compiler batch cannot redirect a detail action.
void buildConsole(ScreenBuilder &builder, UiRect panel, EditorScreenLayout &layout) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const auto *console=state.console;if(!console) return;
  builder.list.addRect(panel,theme.color.canvas,10);
  builder.list.addBorder(panel,theme.color.line,1,10);
  if(!state.consoleCollapsed) {
    builder.list.addRect({panel.x+panel.width*.5f-10,panel.y+3,20,2},theme.color.textMuted,1);
  }
  builder.router.addBlocker(panel);
  if(!state.consoleCollapsed) builder.router.addRegion({panel.x+panel.width*.5f-32,panel.y,64,14},widgetId(EditorWidget::ConsoleResize));
  auto content=deflate(panel,UiInsets::all(8));
  auto button=[&](UiRect rect,const std::string &label,EditorWidget widget,bool active=false,bool enabled=true) {
    rect=deflate(rect,UiInsets::all(2));
    builder.list.addRect(rect,active?theme.color.raised:theme.color.surface,5);
    builder.list.addBorder(rect,active?theme.color.line:theme.color.lineSoft,1,5);
    builder.label(rect,label.c_str(),enabled?(active?theme.color.accent:theme.color.textDim):theme.color.textFaint,
                  theme.type.caption,UiAlign::Center);
    if(enabled) builder.router.addRegion(rect,widgetId(widget));
  };
  auto header=takeTop(content,30);
  builder.list.addImage(centred(takeLeft(header,32),22,22),static_cast<UiImageId>(UiIcon::IdeConsole),0xffffffff);
  button(takeRight(header,34),state.consoleCollapsed?"+":"-",EditorWidget::ConsoleCollapse);
  if(!state.consoleCollapsed) {
    const auto expand=takeRight(header,34);
    builder.list.addImage(centred(expand,18,18),static_cast<UiImageId>(state.consoleExpanded?UiIcon::ViewCollapse:UiIcon::ViewExpand),theme.color.textDim);
    builder.router.addRegion(expand,widgetId(EditorWidget::ConsoleExpand));
  }
  builder.label(takeLeft(header,std::min(65.0f,header.width)),"Console",theme.color.text,theme.type.caption);
  if(header.width>65) {
    const auto errors=console->count(EditorConsoleSeverity::Error);
    const char *summary=state.codeBuildBusy?"Compilando":errors?"Com erros":
        state.code&&state.code->publishedGeneration()?"Publicado":"Sem erros";
    builder.label(header,summary,errors?theme.color.danger:theme.color.accent,theme.type.caption);
  }
  if(state.consoleCollapsed) return;
  auto tabs=takeTop(content,34);
  const auto problems=std::count_if(console->entries().begin(),console->entries().end(),[](const auto &e) {
    return e.origin==EditorConsoleOrigin::Compiler;
  });
  const auto problemsTab=takeLeft(tabs,tabs.width*.5f);
  button(problemsTab,"Problemas  "+std::to_string(problems),EditorWidget::ConsoleProblems,state.consoleProblems);
  button(tabs,"Registros  "+std::to_string(console->entries().size()-problems),EditorWidget::ConsoleLogs,!state.consoleProblems);
  const auto activeTab=state.consoleProblems?problemsTab:tabs;
  builder.list.addRect({activeTab.x+3,activeTab.bottom()-2,activeTab.width-6,2},theme.color.accent,1);
  auto search=takeTop(content,34);
  const auto searchHit=deflate(search,UiInsets::all(2));
  builder.list.addRect(searchHit,theme.color.surface,5);
  builder.list.addBorder(searchHit,theme.color.line,1,5);
  builder.list.addImage(centred(takeLeft(search,30),18,18),static_cast<UiImageId>(UiIcon::IdeSearch),0xffffffff);
  builder.label(search,state.consoleQuery.empty()?"Buscar mensagem ou arquivo…":state.consoleQuery.c_str(),theme.color.textMuted,theme.type.caption);
  builder.router.addRegion(searchHit,widgetId(EditorWidget::ConsoleSearch));
  const std::string origin=state.consoleOrigin<0?"Todas origens":EditorConsole::originName(static_cast<EditorConsoleOrigin>(state.consoleOrigin));
  auto filters=takeTop(content,32);
  const auto source=takeRight(filters,108);
  button(source,origin,EditorWidget::ConsoleSource,state.consoleOrigin>=0);
  for(const auto severity:{EditorConsoleSeverity::Error,EditorConsoleSeverity::Warning,EditorConsoleSeverity::Info}) {
    const auto id=severity==EditorConsoleSeverity::Error?EditorWidget::ConsoleError:severity==EditorConsoleSeverity::Warning?EditorWidget::ConsoleWarning:EditorWidget::ConsoleInfo;
    const char *name=severity==EditorConsoleSeverity::Error?"Erros":severity==EditorConsoleSeverity::Warning?"Avisos":"Info";
    const auto chip=deflate(takeLeft(filters,filters.width/(severity==EditorConsoleSeverity::Error?3:severity==EditorConsoleSeverity::Warning?2:1)),UiInsets::all(2));
    const auto color=severity==EditorConsoleSeverity::Error?theme.color.danger:severity==EditorConsoleSeverity::Warning?theme.color.warning:0xff68b9f2;
    const bool active=console->visible(severity);
    builder.list.addRect(chip,theme.color.surface,5);
    builder.list.addBorder(chip,active?color:theme.color.lineSoft,1,5);
    builder.label(chip,(std::string(name)+" "+std::to_string(console->count(severity))).c_str(),active?color:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(chip,widgetId(id));
  }
  auto tools=takeBottom(content,36);
  button(takeLeft(tools,tools.width/3),"Exportar",EditorWidget::ConsoleExport);
  button(takeRight(tools,tools.width*.5f),"Limpar",EditorWidget::ConsoleClear,false,!state.consoleProblems);
  const auto followHit=tools;
  const auto toggle=centred(takeLeft(tools,36),30,16);
  builder.list.addRect(toggle,state.consoleFollow?theme.color.accentWash:theme.color.raised,8);
  builder.list.addRect({toggle.x+(state.consoleFollow?15.0f:1.0f),toggle.y+1,14,14},state.consoleFollow?theme.color.accent:theme.color.textMuted,7);
  builder.label(tools,"Seguir saída",theme.color.textDim,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(followHit,widgetId(EditorWidget::ConsoleFollow));
  auto footer=takeBottom(content,22);
  const auto order=console->filtered(state.consoleProblems?1:0,state.consoleOrigin,state.consoleQuery);
  builder.label(footer,(std::to_string(order.size())+" eventos · repetições agrupadas"+
      (console->dropped()?" · "+std::to_string(console->dropped())+" descartados":"")).c_str(),theme.color.textMuted,theme.type.caption);
  const auto *selected=console->find(state.consoleSelected);
  if(selected && content.height>150) {
    auto detail=deflate(takeBottom(content,std::min(264.0f,content.height-62.0f)),UiInsets::symmetric(2,4));
    builder.list.addRect(detail,theme.color.surface,8);
    builder.list.addBorder(detail,theme.color.line,1,8);
    auto inner=deflate(detail,UiInsets::all(8));
    auto actions=takeTop(inner,30);
    button(takeRight(actions,28),"×",EditorWidget::ConsoleDetailClose);
    builder.label(actions,"Detalhes do evento",theme.color.text,theme.type.caption);
    auto detailActions=takeBottom(inner,34);
    auto openSource=deflate(takeLeft(detailActions,detailActions.width*.58f),UiInsets::all(2));
    const bool hasSource=!selected->file.empty()||selected->object!=0;
    builder.list.addRect(openSource,hasSource?theme.color.accent:theme.color.raised,5);
    builder.label(openSource,"Abrir fonte",hasSource?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(hasSource) builder.router.addRegion(openSource,widgetId(EditorWidget::ConsoleOpenSource));
    button(detailActions,"Copiar",EditorWidget::ConsoleCopy);
    // Immutable excerpt returned by the compiler, even when the editor buffer
    // has changed since that build. Never substitute today's text for the error.
    if(!selected->sourceExcerpt.empty() && selected->excerptLine && inner.height>110) {
      const auto &source=selected->sourceExcerpt;
      {
        auto snippet=takeBottom(inner,76);
        builder.list.addRect(snippet,theme.color.canvas,4);
        builder.list.pushClip(snippet);
        u32 lineNumber=selected->excerptLine;usize at=0;
        for(int shown=0;shown<3 && at<source.size();++shown,++lineNumber) {
          auto row=takeTop(snippet,24);
          if(lineNumber==selected->line) builder.list.addRect(row,0xff302127,3);
          auto number=takeLeft(row,32);
          builder.label(number,std::to_string(lineNumber).c_str(),lineNumber==selected->line?theme.color.danger:theme.color.textMuted,theme.type.caption,UiAlign::Center);
          const auto end=source.find('\n',at);
          builder.label(row,source.substr(at,end==std::string::npos?end:end-at).c_str(),theme.color.textDim,theme.type.caption);
          if(end==std::string::npos) break;
          at=end+1;
        }
        builder.list.popClip();
      }
    }
    // Wrap by UTF-8 code points, never split a byte sequence. Pages expose the
    // complete retained message/stack; copying always uses the complete text.
    const auto text=EditorConsole::describe(*selected);
    const auto columns=static_cast<u32>(std::max(12.0f,inner.width/(theme.type.caption.size*.62f)));
    std::vector<std::string> lines;std::string line;u32 column=0;
    for(usize i=0;i<text.size();) {
      if(text[i]=='\n') {lines.push_back(line);line.clear();column=0;++i;continue;}
      usize end=i+1;while(end<text.size()&&(static_cast<unsigned char>(text[end])&0xc0)==0x80) ++end;
      line.append(text,i,end-i);i=end;
      if(++column>=columns) {lines.push_back(line);line.clear();column=0;}
    }
    if(!line.empty()) lines.push_back(line);
    const bool needsPages=lines.size()>static_cast<u32>(std::max(0.0f,inner.height)/19);
    auto pager=needsPages?takeBottom(inner,24):UiRect{};
    const u32 fits=std::max(1u,static_cast<u32>(std::max(0.0f,inner.height)/19));
    const u32 pages=std::max(1u,(static_cast<u32>(lines.size())+fits-1)/fits);
    const u32 page=std::min(state.consoleDetailPage,pages-1);
    if(needsPages) {
      button(takeRight(pager,38),">",EditorWidget::ConsoleDetailNext,false,page+1<pages);
      button(takeRight(pager,38),"<",EditorWidget::ConsoleDetailPrevious,false,page>0);
      builder.label(pager,("Página "+std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption);
    }
    builder.list.pushClip(inner);
    for(u32 i=page*fits;i<lines.size() && i<(page+1)*fits;++i)
      builder.label(takeTop(inner,19),lines[i].c_str(),theme.color.text,theme.type.caption);
    builder.list.popClip();
  }
  constexpr float rowHeight=56;
  const u32 fits=static_cast<u32>(std::max(0.0f,content.height)/rowHeight);
  layout.consoleVisibleRows=fits;layout.consoleRowCount=static_cast<u32>(order.size());
  if(order.empty()) {
    builder.label(content,state.consoleQuery.empty()?(state.consoleProblems?"Nenhum problema de compilação":"Nenhum registro nesta sessão"):
        "Nenhuma mensagem corresponde aos filtros",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }
  const u32 hidden=order.size()>fits?static_cast<u32>(order.size())-fits:0;
  u32 first=hidden>state.consoleScroll?hidden-state.consoleScroll:0;
  if(!state.consoleFollow && state.consoleAnchor) {
    const auto anchor=std::find_if(order.begin(),order.end(),[&](u32 i) {return console->at(i)->eventId>=state.consoleAnchor;});
    first=std::min(hidden,static_cast<u32>(anchor-order.begin()));
  }
  builder.list.pushClip(content);
  for(u32 offset=0;offset<fits && first+offset<order.size();++offset) {
    const u32 index=order[first+offset];const auto *entry=console->at(index);
    auto row=takeTop(content,rowHeight);
    const auto card=deflate(row,UiInsets::symmetric(2,3));
    const auto semantic=entry->severity==EditorConsoleSeverity::Error?theme.color.danger:
        entry->severity==EditorConsoleSeverity::Warning?theme.color.warning:0xff68b9f2;
    builder.list.addRect(card,entry->eventId==state.consoleSelected?
        (entry->severity==EditorConsoleSeverity::Error?0xff302127:0xff1b2933):theme.color.surface,6);
    builder.list.addBorder(card,entry->eventId==state.consoleSelected?semantic:theme.color.lineSoft,1,6);
    builder.router.addRegion(row,widgetId(EditorWidget::ConsoleRowBase)+index);
    const auto icon=entry->severity==EditorConsoleSeverity::Error?UiIcon::IdeError:
        entry->severity==EditorConsoleSeverity::Warning?UiIcon::IdeWarning:UiIcon::IdeConsole;
    row=deflate(row,UiInsets::symmetric(6,3));
    builder.list.addImage(centred(takeLeft(row,30),20,20),static_cast<UiImageId>(icon),0xffffffff);
    auto message=takeTop(row,25);
    if(entry->repeats>1) builder.label(takeRight(message,50),("×"+std::to_string(entry->repeats)).c_str(),theme.color.accent,theme.type.caption,UiAlign::End);
    builder.label(message,entry->message.c_str(),theme.color.text,theme.type.caption);
    std::string where=EditorConsole::originName(entry->origin);
    if(!entry->file.empty()) {where+=" · "+entry->file;if(entry->line) where+=":"+std::to_string(entry->line);}
    else if(entry->object) where+=" · objeto "+std::to_string(entry->object);
    where+=" · +"+std::to_string(entry->elapsedMs/1000)+"s";
    builder.label(row,where.c_str(),theme.color.textMuted,theme.type.caption);
  }
  builder.list.popClip();
}

void buildCodeWorkspace(ScreenBuilder &builder,UiRect body,UiRect toolbar,EditorScreenLayout &layout) {
  const auto &theme=builder.theme;const auto *workspace=builder.state.code;
  // O teclado do sistema come a parte de baixo da janela. Sem descontar isso, a
  // linha que esta sendo digitada fica atras do teclado e a rolagem acha que ela
  // esta visivel -- que e meio caminho de volta para editar numa caixa separada.
  if(builder.state.platformTextInput && builder.state.platformImeFraction>0) {
    const float keyboard=builder.state.surface.height*builder.state.platformImeFraction;
    body.height=std::max(0.0f,body.height-keyboard);
  }
  builder.list.addRect(toolbar,theme.color.canvas);
  const UiRect toolbarOrigin=toolbar;
  builder.router.addBlocker(toolbar);builder.router.addBlocker(body);
  const auto *buffer=workspace?workspace->active():nullptr;
  auto icon=[&](UiRect rect,UiIcon glyph,EditorWidget action,bool enabled=true,bool active=false) {
    rect=deflate(rect,UiInsets::all(4));
    if(active) builder.list.addRect(rect,theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(rect,25,25),static_cast<UiImageId>(glyph),enabled?0xffffffff:0x55ffffff);
    if(enabled) builder.router.addRegion(rect,widgetId(action));
  };
  const auto brand=takeLeft(toolbar,42);
  builder.list.addImage(centred(brand,28,28),static_cast<UiImageId>(UiIcon::BrandMark),0xffffffff);
  builder.router.addRegion(brand,widgetId(EditorWidget::CodeFiles));
  icon(takeRight(toolbar,42),UiIcon::EditorAuthorMore,EditorWidget::CodeMenu,true,builder.state.codeMenu);
  const auto play=deflate(takeRight(toolbar,52),UiInsets::symmetric(3,5));
  builder.list.addRect(play,theme.color.accent,6);
  builder.list.addImage(centred(play,22,22),static_cast<UiImageId>(UiIcon::EditorAuthorPlay),theme.color.accentInk);
  builder.router.addRegion(play,widgetId(EditorWidget::PlayFromTopBar));
  icon(takeRight(toolbar,42),UiIcon::IdeSearch,EditorWidget::CodeSearch,buffer!=nullptr);
  builder.list.pushClip(toolbar);
  builder.list.addImage(centred(takeLeft(toolbar,108),96,23),static_cast<UiImageId>(UiIcon::BrandWordmark),0xffffffff);
  builder.list.popClip();
  layout.viewport=body;
  builder.list.addRect(body,theme.color.surface);
  auto breadcrumb=takeTop(body,32);
  builder.list.addRect(breadcrumb,theme.color.canvas);
  auto back=takeLeft(breadcrumb,66);
  builder.label(back,"< Cena",theme.color.textDim,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(back,widgetId(EditorWidget::CodeScene));
  builder.list.addImage(centred(takeLeft(breadcrumb,26),18,18),static_cast<UiImageId>(UiIcon::IdeFiles),0xffffffff);
  builder.label(deflate(breadcrumb,UiInsets::symmetric(8,0)),buffer?buffer->path.c_str():"Projeto / Código",theme.color.textDim,theme.type.caption);
  auto overlays=[&] {
    if(builder.state.codeFiles && builder.state.files) {
      builder.router.addBlocker(body);
      builder.router.addRegion(body,widgetId(EditorWidget::CodeFiles));
      builder.list.addRect(body,withAlpha(theme.color.voidBlack,.45f));
      layout.filesPanel={body.x,body.y,std::min(440.0f,body.width*.90f),body.height};
      buildFiles(builder,layout.filesPanel);
    }
    buildCodeMenu(builder,toolbarOrigin,layout);
  };
  if(builder.state.choosingTemplate) {
    // Escolher de onde partir vem ANTES de nomear a classe: o nome que o
    // usuário digita já entra no arquivo certo, sem um segundo passo de
    // "agora cole este código aqui".
    auto panel=deflate(body,UiInsets::all(24));
    builder.list.addRect(panel,theme.color.canvas,theme.radius.card);
    auto inner=deflate(panel,UiInsets::all(14));
    builder.label(takeTop(inner,26),"Novo script: escolha o ponto de partida",theme.color.text,theme.type.body);
    auto footer=takeBottom(inner,36);
    auto row=takeTop(inner,38);
    builder.list.addRect(row,theme.color.raised,2);
    builder.label(deflate(row,UiInsets::symmetric(10,0)),"Arquivo vazio",theme.color.text,theme.type.body);
    builder.router.addRegion(row,widgetId(EditorWidget::CodeTemplateBase));
    for(u32 i=0;i<editorScriptTemplates.size();++i) {
      const auto &model=editorScriptTemplates[i];
      auto entry=takeTop(inner,38);
      if(entry.height<38) break;
      builder.list.addRect(entry,theme.color.raised,2);
      auto label=deflate(entry,UiInsets::symmetric(10,0));
      builder.label(takeLeft(label,150),std::string(model.name).c_str(),theme.color.text,theme.type.body);
      builder.label(label,std::string(model.description).c_str(),theme.color.textDim,theme.type.caption);
      builder.router.addRegion(entry,widgetId(EditorWidget::CodeTemplateBase)+i+1);
    }
    auto cancel=takeRight(footer,90);
    builder.label(cancel,"Cancelar",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.router.addRegion(cancel,widgetId(EditorWidget::CodeTemplateClose));
    return;
  }
  auto content=body;
  auto tabs=takeTop(content,40);layout.codeTabs=tabs;
  builder.list.addRect(tabs,theme.color.canvas);
  icon(takeRight(tabs,40),UiIcon::EditorAuthorAdd,EditorWidget::CodeNew);
  builder.list.addRect({tabs.x,tabs.bottom()-1,tabs.width,1},theme.color.lineSoft);
  if(workspace && !workspace->buffers().empty()) {
    const u32 first=std::min(builder.state.codeFirstTab,static_cast<u32>(workspace->buffers().size()-1));
    const bool overflow=workspace->buffers().size()*156.0f>tabs.width;
    if(overflow) {
      auto previous=takeRight(tabs,30),next=takeRight(tabs,30);
      builder.label(previous,"<",theme.color.textDim,theme.type.body,UiAlign::Center);
      builder.label(next,">",theme.color.textDim,theme.type.body,UiAlign::Center);
      if(first) builder.router.addRegion(previous,widgetId(EditorWidget::CodeTabsPrevious));
      if(first+1<workspace->buffers().size()) builder.router.addRegion(next,widgetId(EditorWidget::CodeTabsNext));
    }
    for(u32 i=first;i<workspace->buffers().size() && tabs.width>=48;++i) {
      const auto &item=workspace->buffers()[i];
      const auto tab=deflate(takeLeft(tabs,std::min(156.0f,tabs.width)),UiInsets::all(2));
      const bool selected=buffer&&item.id==buffer->id;
      builder.list.addRect(tab,selected?theme.color.raised:theme.color.canvas,theme.radius.control);
      builder.list.addBorder(tab,selected?theme.color.line:theme.color.lineSoft,1,theme.radius.control);
      if(selected) builder.list.addRect({tab.x,tab.bottom()-2,tab.width,2},theme.color.accent);
      auto label=deflate(tab,UiInsets::symmetric(8,0));
      const auto close=selected?takeRight(label,24):UiRect{};
      builder.list.addImage(centred(takeLeft(label,24),19,19),static_cast<UiImageId>(UiIcon::IdeCode),0xffffffff);
      const auto slash=item.path.find_last_of('/');
      const std::string name=(slash==std::string::npos?item.path:item.path.substr(slash+1))+(item.dirty()?" *":"");
      builder.label(label,name.c_str(),selected?theme.color.text:theme.color.textDim,theme.type.caption);
      builder.router.addRegion(tab,widgetId(EditorWidget::CodeTabBase)+i);
      if(selected) {
        builder.label(close,"×",theme.color.textDim,theme.type.body,UiAlign::Center);
        builder.router.addRegion(close,widgetId(EditorWidget::CodeClose));
      }
    }
  }
  if(!builder.state.codeQuery.empty() && buffer) {
    auto find=takeTop(content,30);
    builder.label(takeLeft(find,std::max(0.0f,find.width-100)),("Buscar: "+builder.state.codeQuery).c_str(),theme.color.textDim,theme.type.caption);
    auto previous=takeLeft(find,48),next=takeLeft(find,48);
    builder.label(previous,"<",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(next,">",theme.color.text,theme.type.body,UiAlign::Center);
    builder.router.addRegion(previous,widgetId(EditorWidget::CodeFindPrevious));
    builder.router.addRegion(next,widgetId(EditorWidget::CodeFindNext));
  }
  // O console e a voz do PROJETO, nao do arquivo aberto. Ele e reservado antes
  // do caminho que sai cedo quando nao ha buffer: um erro de compilacao que so
  // aparece quando ha um arquivo aberto e um erro que se esconde justamente de
  // quem acabou de fechar o arquivo por causa dele.
  if(builder.state.platformCodeView) {
    layout.codeAccessory=takeBottom(content,44);
    builder.list.addRect(layout.codeAccessory,theme.color.canvas);
    builder.list.addRect({layout.codeAccessory.x,layout.codeAccessory.y,layout.codeAccessory.width,1},theme.color.lineSoft);
  }
  if(builder.state.console) {
    const float wanted=builder.state.consoleCollapsed?40.0f:content.height*(builder.state.consoleExpanded?.82f:builder.state.consoleFraction);
    layout.consolePanel=takeBottom(content,wanted);
  }
  auto status=takeBottom(content,28);
  builder.list.addRect(status,theme.color.silhouette);
  builder.list.addRect({status.x,status.y,status.width,1},theme.color.lineSoft);
  status=deflate(status,UiInsets::symmetric(12,0));
  std::string statusText;
  if(buffer) {
    u32 line=1,column=1;const auto caret=std::min<usize>(buffer->selectionEnd,buffer->text.size());
    for(usize i=0;i<caret;++i) {
      const auto c=static_cast<unsigned char>(buffer->text[i]);
      if(c=='\n') {++line;column=1;} else if((c&0xc0)!=0x80) ++column;
    }
    statusText="Ln "+std::to_string(line)+", Col "+std::to_string(column)+"   ·   "+
        (buffer->path.ends_with(".cs")?"C#":"Texto");
  }
  if(builder.state.codeComposing) statusText+="   Compondo";
  else if(builder.state.codeBuildBusy) statusText+="   Compilando";
  auto saved=takeRight(status,80);
  builder.label(saved,buffer&&buffer->dirty()?"Salvar *":"Salvo",buffer&&buffer->dirty()?theme.color.accent:theme.color.textMuted,theme.type.caption,UiAlign::End);
  if(buffer) builder.router.addRegion(saved,widgetId(EditorWidget::CodeSave));
  builder.list.addRect(centred(takeLeft(status,20),8,8),theme.color.accent,4);
  builder.label(status,statusText.c_str(),theme.color.textMuted,theme.type.caption);
  if(!layout.consolePanel.isEmpty()) buildConsole(builder,layout.consolePanel,layout);
  if(!buffer) {
    builder.label(takeTop(content,48),"Abra um arquivo ou crie um script C#",theme.color.text,theme.type.body);
    builder.label(takeTop(content,32),"Os arquivos pertencem ao projeto e têm histórico próprio.",theme.color.textDim,theme.type.caption);
    auto open=takeTop(content,42);
    builder.label(open,"Abrir arquivos",theme.color.accent,theme.type.body);
    builder.router.addRegion(open,widgetId(EditorWidget::CodeFiles));
    auto create=takeTop(content,42);
    builder.label(create,"Criar script C#",theme.color.text,theme.type.body);
    builder.router.addRegion(create,widgetId(EditorWidget::CodeNew));
    overlays();
    return;
  }
  builder.router.addRegion(content,widgetId(EditorWidget::CodeBody));
  layout.codeBody=content;
  layout.codeLineHeight=24.0f;
  layout.codeVisibleLines=static_cast<u32>(std::max(0.0f,content.height)/24.0f);
  if(builder.state.platformCodeView && !builder.state.codeFiles && !builder.state.codeMenu &&
      !platformFieldActive(builder.state)) {overlays();return;}
  builder.list.pushClip(content);
  // O cursor, quando o editor esta com o teclado aberto. Ele e um glifo inserido
  // no texto da linha, e nao um retangulo medido: a largura do texto so e
  // resolvida na construcao das instancias, depois deste passo, e medir aqui
  // exigiria uma segunda copia da metrica da fonte.
  const bool caretVisible=builder.state.editingCode && builder.state.platformTextInput;
  const usize caret=std::min<usize>(builder.state.platformCaret,buffer->text.size());
  usize start=0;u32 line=0;
  while(start<buffer->text.size() && line<buffer->firstLine) {
    const auto end=buffer->text.find('\n',start);if(end==std::string::npos) {start=buffer->text.size();break;}
    start=end+1;++line;
  }
  while(content.height>=24 && start<=buffer->text.size()) {
    const auto end=buffer->text.find('\n',start);
    auto row=takeTop(content,24);const auto gutter=takeLeft(row,48);
    const std::string number=std::to_string(++line);
    const auto length=end==std::string::npos?buffer->text.size()-start:end-start;
    const bool onThisLine=caretVisible && caret>=start && caret<=start+length;
    builder.label(gutter,number.c_str(),onThisLine?theme.color.accent:theme.color.textMuted,
                  theme.type.numeric,UiAlign::Center);
    if(onThisLine) builder.list.addRect(row,withAlpha(theme.color.accent,0.10f));
    std::string text=buffer->text.substr(start,std::min<usize>(length,512));
    if(onThisLine) text.insert(std::min<usize>(caret-start,text.size()),"|");
    std::replace(text.begin(),text.end(),'\t',' ');
    builder.label(row,text.c_str(),theme.color.text,theme.type.numeric);
    if(end==std::string::npos) break;
    start=end+1;
  }
  builder.list.popClip();
  overlays();
}

static void buildProjectDialogs(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
    if(state.importPanel) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.7f));router.addBlocker(state.surface);
    const auto panel=centred(state.surface,std::min(600.0f,state.surface.width-24),std::min(430.0f,state.surface.height-24));
    list.addRect(panel,theme.color.surface,12);list.addBorder(panel,theme.color.line,1,12);
    auto content=deflate(panel,UiInsets::all(16));
    auto title=takeTop(content,36);
    list.addImage(centred(takeLeft(title,32),24,24),static_cast<UiImageId>(UiIcon::AssetsImport),0xffffffff);
    builder.label(title,"Importar recurso",theme.color.text,theme.type.cardName);
    builder.label(takeTop(content,28),state.importPath.c_str(),theme.color.textMuted,theme.type.caption);
    builder.label(takeTop(content,30),state.importStatus.c_str(),theme.color.accent,theme.type.body);
    auto actions=takeBottom(content,42);
    auto cancel=takeLeft(actions,state.importReady?actions.width*.22f:actions.width);takeLeft(actions,8);
    list.addRect(cancel,theme.color.raised,8);builder.label(cancel,state.importError?"Fechar":"Cancelar",theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(cancel,widgetId(EditorWidget::ImportCancel));
    if(state.importReady) {
      // Ambiguidade na correspondência de nós: publicar só depois de o usuário
      // dizer como associar. Os botões aparecem apagados e sem toque até lá.
      const bool decided=!state.importAmbiguities || state.importAmbiguityChoice;
      auto resource=takeLeft(actions,actions.width*.4f);takeLeft(actions,8);
      list.addRect(resource,theme.color.raised,8);builder.label(resource,"Só recurso",decided?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
      if(decided) router.addRegion(resource,widgetId(EditorWidget::ImportAccept));
      list.addRect(actions,decided?theme.color.accent:theme.color.raised,8);
      builder.label(actions,"Importar na cena",decided?theme.color.accentInk:theme.color.textMuted,theme.type.body,UiAlign::Center);
      if(decided) router.addRegion(actions,widgetId(EditorWidget::ImportIntoScene));
      if(state.importAmbiguities) {
        takeBottom(content,6);
        auto choice=takeBottom(content,40);
        auto order=takeLeft(choice,choice.width*.5f-4);takeLeft(choice,8);
        const auto pill=[&](UiRect rect,const char *label,bool on,EditorWidget widget) {
          list.addRect(rect,on?theme.color.accent:theme.color.raised,8);
          builder.label(rect,label,on?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
          router.addRegion(rect,widgetId(widget));
        };
        pill(order,"Associar pela ordem",state.importAmbiguityChoice==1,EditorWidget::ImportMatchInOrder);
        pill(choice,"Tratar como novos",state.importAmbiguityChoice==2,EditorWidget::ImportTreatAsNew);
      }
    }
    auto pages=takeBottom(content,30);
    std::vector<std::string> wrapped;
    std::istringstream paragraphs(state.importSummary);std::string line;
    while(std::getline(paragraphs,line)) {
      while(measureTextWidth(line,list.fontMetrics(),theme.type.caption)>content.width && line.size()>1) {
        usize width=0;
        for(usize i=1;i<=line.size();++i) {
          if(i<line.size() && (static_cast<unsigned char>(line[i])&0xc0)==0x80) continue;
          if(width && measureTextWidth(std::string_view(line).substr(0,i),list.fontMetrics(),theme.type.caption)>content.width) break;
          width=i;
        }
        usize split=line.rfind(' ',width);
        if(split==std::string::npos || split==0) split=width;
        wrapped.push_back(line.substr(0,split));line.erase(0,split);if(!line.empty() && line.front()==' ') line.erase(0,1);
      }
      wrapped.push_back(line);
    }
    const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/22));
    const u32 count=std::max(1u,(static_cast<u32>(wrapped.size())+perPage-1)/perPage),page=std::min(state.importPage,count-1);
    if(count>1) {
      auto previous=takeLeft(pages,70),next=takeRight(pages,70);
      builder.label(pages,std::to_string(page+1)+" / "+std::to_string(count),theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(page) {builder.label(previous,"Anterior",theme.color.text,theme.type.caption);router.addRegion(previous,widgetId(EditorWidget::ImportPreviousPage));}
      if(page+1<count) {builder.label(next,"Próxima",theme.color.text,theme.type.caption);router.addRegion(next,widgetId(EditorWidget::ImportNextPage));}
    }
    list.pushClip(content);
    for(usize i=page*perPage;i<wrapped.size() && i<(page+1)*perPage;++i)
      builder.label(takeTop(content,22),wrapped[i],theme.color.textDim,theme.type.caption);
    list.popClip();
  }
  if(state.codeRecoveryPending) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.7f));router.addBlocker(state.surface);
    const auto panel=centred(state.surface,std::min(440.0f,state.surface.width-24),190);
    list.addRect(panel,theme.color.surface,12);list.addBorder(panel,theme.color.line,1,12);
    auto content=deflate(panel,UiInsets::all(16));
    builder.label(takeTop(content,36),"Recuperar código",theme.color.text,theme.type.cardName);
    builder.label(takeTop(content,28),"Há rascunhos de uma sessão interrompida.",theme.color.textDim,theme.type.caption);
    builder.label(takeTop(content,28),"Os arquivos salvos permanecem preservados.",theme.color.textDim,theme.type.caption);
    auto buttons=takeBottom(content,42);auto discard=takeLeft(buttons,buttons.width*.43f);takeLeft(buttons,8);
    list.addRect(discard,theme.color.raised,8);builder.label(discard,"Descartar",theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(discard,widgetId(EditorWidget::CodeDiscardRecovery));
    list.addRect(buttons,theme.color.accent,8);builder.label(buttons,"Recuperar",theme.color.accentInk,theme.type.body,UiAlign::Center);
    router.addRegion(buttons,widgetId(EditorWidget::CodeRecover));
  }
}

EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const UiTheme &theme,
                                     UiDrawList &list, UiInputRouter &router) {
  EditorScreenLayout layout{};
  if (state.document == nullptr || state.surface.isEmpty()) return layout;

  ScreenBuilder builder{state, theme, list, router};
  UiRect remaining = deflate(state.surface, state.safeArea);
  layout.topBar = takeTop(remaining, kTopBarHeight);
  if(state.workspace==EditorWorkspace::Code) {
    buildCodeWorkspace(builder,remaining,layout.topBar,layout);
    // O campo embutido tambem vale aqui. Sem esta chamada, criar um script ou
    // buscar no codigo abria o teclado com o campo invisivel: o usuario digitava
    // as cegas, que e pior do que o dialogo que isto substituiu.
    buildProjectDialogs(builder);
  buildPlatformTextField(builder);
    return layout;
  }

  // Larguras resolvidas ANTES de desenhar, porque o viewport é o que sobra e
  // precisa existir mesmo quando o usuário arrasta os dois divisores ao limite.
  const float available = remaining.width;
  // Keep a useful property row instead of compressing both side panels.
  constexpr float compactPanelWidth = 240.0f;
  const bool compact = available < compactPanelWidth * 2 + kViewportMinimum + kSplitterWidth * 2;

  float hierarchyWidth = state.hierarchyVisible && state.workspace != EditorWorkspace::Play
      ? (state.hierarchyWidth > 0.0f ? state.hierarchyWidth : available * 0.24f)
      : 0.0f;
  float inspectorWidth = state.inspectorVisible && state.workspace != EditorWorkspace::Play
      ? (state.inspectorWidth > 0.0f ? state.inspectorWidth : available * 0.27f)
      : 0.0f;
  if (hierarchyWidth > 0.0f) hierarchyWidth = std::max(hierarchyWidth, kPanelMinimum);
  if (inspectorWidth > 0.0f) inspectorWidth = std::max(inspectorWidth, kPanelMinimum);
  if (compact) {
    hierarchyWidth = state.workspace != EditorWorkspace::Play && (state.compactPanel == EditorScreenState::CompactPanel::Hierarchy || state.compactPanel == EditorScreenState::CompactPanel::Files)
        ? std::min(compactPanelWidth, std::max(0.0f, available-kViewportMinimum-kSplitterWidth)) : 0;
    inspectorWidth = state.workspace != EditorWorkspace::Play && state.compactPanel == EditorScreenState::CompactPanel::Inspector
        ? std::min(compactPanelWidth, std::max(0.0f, available-kViewportMinimum-kSplitterWidth)) : 0;
  }
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
    const auto splitter=takeLeft(body, kSplitterWidth);
    if (!compact) buildSplitter(builder, splitter, EditorWidget::SplitterLeft);
    if(compact && state.files && state.compactPanel == EditorScreenState::CompactPanel::Files) {
      layout.filesPanel=layout.hierarchyPanel;layout.hierarchyPanel={};
    } else if(state.files && layout.hierarchyPanel.height>210) {
      const float height=layout.hierarchyPanel.height;
      layout.filesPanel=takeBottom(layout.hierarchyPanel,state.filesCollapsed?40:height*std::clamp(state.filePanelRatio,.28f,.58f));
      const auto divider=takeBottom(layout.hierarchyPanel,kSplitterWidth);
      builder.list.addRect(divider,theme.color.track);

    }
  }
  if (inspectorWidth > 0.0f) {
    layout.inspectorPanel = takeRight(body, inspectorWidth);
    const auto splitter=takeRight(body, kSplitterWidth);
    if (!compact) buildSplitter(builder, splitter, EditorWidget::SplitterRight);
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
    const auto *controlled=state.document?state.document->find(state.selection):nullptr;
    if(controlled&&characterComponent(*controlled)&&characterComponent(*controlled)->jumpSpeed>0) {
      const UiRect jump{layout.viewport.x+layout.viewport.width-108,layout.viewport.y+layout.viewport.height-76,92,56};
      list.addRect(jump,theme.color.raised,theme.radius.control);
      builder.label(jump,"Saltar",state.playPaused?theme.color.textFaint:theme.color.text,theme.type.body,UiAlign::Center);
      if(!state.playPaused) router.addRegion(jump,widgetId(EditorWidget::JumpCharacter),theme.touch.minimumTarget);
    }
    builder.label({layout.viewport.x+12,layout.viewport.y+8,layout.viewport.width-24,24},
                  controlled&&characterComponent(*controlled)?"Arraste à esquerda para mover o personagem":"Simulação - use o quadrado no topo para parar", theme.color.textDim,theme.type.caption);
    return layout;
  }
  buildViewportOverlay(builder, layout.viewport);
  if (state.workspace == EditorWorkspace::Scene) buildToolRail(builder, layout.viewport, compact);
  if (!layout.hierarchyPanel.isEmpty())
    layout.hierarchyRowCount =
        buildHierarchy(builder, layout.hierarchyPanel, layout.hierarchyVisibleRows);
  if(!layout.filesPanel.isEmpty()) buildFiles(builder,layout.filesPanel);
  if (!layout.inspectorPanel.isEmpty()) buildInspector(builder, layout.inspectorPanel);
  layout.componentPage=builder.componentPage;
  if(!compact && !layout.filesPanel.isEmpty()) {
    // A área de toque ultrapassa a linha visual, acima dos bloqueadores dos painéis.
    const UiRect divider{layout.filesPanel.x,layout.hierarchyPanel.bottom()-4,
                         layout.filesPanel.width,kSplitterWidth+8};
    router.addRegion(divider,widgetId(EditorWidget::FilesSplitter));
  }


  // Botões flutuantes nos cantos do viewport, fora dos painéis.
  const UiRect corner{layout.viewport.right() - kCornerButton - theme.spacing.small,
                      layout.viewport.bottom() - kCornerButton - theme.spacing.small,
                      kCornerButton, kCornerButton};
  if (!compact && !state.status.empty())
    builder.label({layout.viewport.x+8,layout.viewport.bottom()-26,layout.viewport.width-160,24},
                  state.status.c_str(),theme.color.text,theme.type.caption);
  builder.iconButton(corner, UiIcon::EditorAuthorFrame, widgetId(EditorWidget::Fullscreen));
  builder.iconButton({layout.viewport.right() - kCornerButton * 2.0f - theme.spacing.small * 2.0f,
                      corner.y, kCornerButton, kCornerButton},
                     UiIcon::EditorAuthorGrid, widgetId(EditorWidget::ViewModeSolid), state.showGrid);
  builder.iconButton({layout.viewport.x + 8.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::EditorAuthorCamera, widgetId(EditorWidget::FrameSelection));
  builder.iconButton({layout.viewport.x + 56.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::EditorAuthorFrame, widgetId(EditorWidget::FrameAll));
  if(compact) {
    const UiRect button{layout.viewport.x+104,layout.viewport.y+8,100,40};
    list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Painéis",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(button,widgetId(EditorWidget::CompactPanelMenu));
  }
  if(state.workspace==EditorWorkspace::Scene) {
    const UiIcon icons[]={UiIcon::EditorAuthorOrbit,UiIcon::EditorAuthorPan,UiIcon::EditorAuthorZoom};
    const EditorWidget actions[]={EditorWidget::NavigationOrbit,EditorWidget::NavigationPan,EditorWidget::NavigationZoom};
    const float width=std::min(64.0f,(layout.viewport.width-(compact?112:16))/3);
    for(u32 i=0;i<3;++i) {
      const UiRect cell{layout.viewport.x+8+i*width,layout.viewport.bottom()-(compact?44:76),width-2,36};
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
    builder.label(title,"Malhas disponíveis",theme.color.text,theme.type.body);
    if(creationAvailable(state,4) || creationAvailable(state,5)) {
      UiRect waterRow=takeTop(content,44);
      if(creationAvailable(state,4)) {
        const auto finite=takeLeft(waterRow,creationAvailable(state,5)?waterRow.width*.5f:waterRow.width);
        builder.label(finite,"Criar agua finita",theme.color.text,theme.type.body,UiAlign::Center);
        router.addRegion(finite,widgetId(EditorWidget::CreateFiniteWater));
      }
      if(creationAvailable(state,5)) {
        builder.label(waterRow,"Criar oceano",theme.color.text,theme.type.body,UiAlign::Center);
        router.addRegion(waterRow,widgetId(EditorWidget::CreateOceanWater));
      }
    }
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
  if (!state.platformTextInput && (state.renameEntity != kInvalidEntity || state.editingHierarchySearch || state.editingCreationSearch || state.editingComponentSearch || state.editingMeshSearch || state.editingReferenceSearch)) {
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
    if(water && waterRoute(*water).count && state.waterTab==1) {
      float world[16];
      if(editorWorldMatrix(*state.document,water->id,world)) {
        list.pushClip(layout.viewport);
        UiPoint previous{};bool hasPrevious=false;
        for(u32 segment=0;segment+1<waterRoute(*water).count;++segment) for(u32 step=0;step<=renderer::WaterRouteSteps;++step) {
          const auto sample=renderer::evaluateWaterRoute(waterRoute(*water),segment,float(step)/renderer::WaterRouteSteps);
          const float local[]{sample.center.x,sample.center.y,sample.center.z};float position[3];
          for(u32 a=0;a<3;++a) position[a]=world[12+a]+world[a]*local[0]+world[4+a]*local[1]+world[8+a]*local[2];
          const auto projected=projectWorldToScreen(*state.view,position);
          if(projected.valid) {if(hasPrevious) list.addLine(previous,projected.screen,theme.color.accent,2);previous=projected.screen;hasPrevious=true;}
          else hasPrevious=false;
        }
        for(u32 i=0;i<waterRoute(*water).count;++i) {
          const auto &p=waterRoute(*water).points[i];float position[3];
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
    const u32 menuRows=3+(state.assetCount?1:0)+(waterCreationAvailable(state)?1:0);
    const UiRect modal=centred(state.surface, std::min(340.0f,state.surface.width-24), 48.0f+44.0f*menuRows);
    list.addRect(modal,theme.color.surface,8);auto content=deflate(modal,UiInsets::all(12));
    const char *names[]{"Voltar à edição","Recursos importados","Ambiente da cena","Água da cena","Fechar"};
    const EditorWidget actions[]{EditorWidget::TabScene,EditorWidget::TabAssets,EditorWidget::TabLighting,EditorWidget::TabSettings,EditorWidget::WorkspaceMenuClose};
    builder.label(takeTop(content,24),"Cena",theme.color.text,theme.type.cardName);
    for(u32 i=0;i<5;++i) {
      if(actions[i]==EditorWidget::TabAssets && !state.assetCount) continue;
      if(actions[i]==EditorWidget::TabSettings && !waterCreationAvailable(state)) continue;
      auto row=takeTop(content,44);builder.label(row,names[i],theme.color.text,theme.type.body);router.addRegion(row,widgetId(actions[i]));
    }
  }
  if(compact && state.compactPanelMenu) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.6f));
    router.addBlocker(state.surface);
    const UiRect modal=centred(state.surface,std::min(300.0f,state.surface.width-16),148);
    list.addRect(modal,theme.color.surface,theme.radius.control);
    auto content=deflate(modal,UiInsets::all(8));
    auto header=takeTop(content,40);auto close=takeRight(header,60);
    builder.label(header,"Painéis",theme.color.text,theme.type.body);
    builder.label(close,"Fechar",theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(close,widgetId(EditorWidget::CompactPanelMenu));
    const char *labels[]{"Viewport","Hierarquia","Inspector","Arquivos"};
    const EditorWidget actions[]{EditorWidget::CompactViewport,EditorWidget::HierarchyToggle,EditorWidget::InspectorToggle,EditorWidget::CompactFiles};
    const u32 count=state.files?4:3;
    for(u32 i=0;i<count;++i) {
      const UiRect cell{content.x+(i%2)*content.width/2,content.y+(i/2)*44,content.width/2-4,40};
      const bool selected=static_cast<u32>(state.compactPanel)==i;
      list.addRect(cell,selected?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(cell,labels[i],selected?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widgetId(actions[i]));
    }
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
      bool available=false;
      for(u32 j=0;j<editorCreationCatalog.size();++j)
        available |= creationAvailable(state,j) && editorCreationCatalog[j].category==i;
      if(!available) continue;
      auto row=takeTop(categories,36);
      if(state.creationCategory==i&&!state.creationSearch[0]) list.addRect(row,theme.color.raised,2);
      builder.label(deflate(row,UiInsets::symmetric(8,0)),creationCategories[i],state.creationCategory==i?theme.color.accent:theme.color.textDim,theme.type.body);
      router.addRegion(row,widgetId(EditorWidget::CreationCategoryBase)+i);
    }
    const auto query=editorSearchKey(state.creationSearch);
    u32 count=0;bool selectedVisible=false;
    for(u32 i=0;i<editorCreationCatalog.size();++i) {
      if(!creationAvailable(state,i)) continue;
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
  if (!state.platformTextInput && state.numericField != 0) {
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
  buildProjectDialogs(builder);
  buildPlatformTextField(builder);
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

  if(widget>=widgetId(EditorWidget::ComponentAddBase) && widget<widgetId(EditorWidget::ComponentEnumBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);
    if(!entity || state.workspace!=EditorWorkspace::Scene) return outcome;
    if(state.componentSelection!=state.selection) {
      state.componentSelection=state.selection;state.expandedComponent.clear();state.expandedNative=0;state.nativeMenu=0;state.addingComponent=false;state.componentPage=0;state.expandedScript=0;state.scriptMenu=0;
    }
    const u32 operation=widget&0xff000000u;
    const u32 index=widget&((operation==widgetId(EditorWidget::ComponentBooleanBase)||operation==widgetId(EditorWidget::ComponentEnumBase))?0xffu:0x00ffffffu);
    const bool adding=operation==widgetId(EditorWidget::ComponentAddBase);
    const auto *component=adding?nullptr:entity->components.at(index);
    const auto *metadata=adding?(index<editorComponentCatalog.size()?&editorComponentCatalog[index]:nullptr):component?findEditorComponent(component->type().id):nullptr;
    if(!metadata) return outcome;
    const auto &entry=*metadata;
    const u64 instance=component?component->instanceId():0;
    if(operation==widgetId(EditorWidget::ComponentMenuBase)) {
      state.nativeMenu=state.nativeMenu==instance?0:instance;state.expandedNative=0;state.expandedComponent.clear();state.expandedScript=0;
    } else if(operation==widgetId(EditorWidget::ComponentFoldBase)) {
      state.expandedNative=state.expandedNative==instance?0:instance;state.expandedComponent.clear();state.expandedScript=0;state.nativeMenu=0;state.scriptMenu=0;state.meshPicker=false;
    } else if(operation==widgetId(EditorWidget::ComponentCopyBase)) {
      state.componentClipboard=component->clone();state.status="Valores do componente copiados";
    } else {
      auto value=*entity;
      if(adding) {
        if(const auto *reason=entry.unavailable(value)) {state.status=reason;return outcome;}
        if(!value.components.add(*entry.type)) return outcome;
        state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=~u32{0};state.addingComponent=false;state.nativeMenu=0;
      } else if(operation==widgetId(EditorWidget::ComponentRemoveBase)) {
        if(!value.components.removeInstance(instance)) return outcome;
        if(state.expandedNative==instance) state.expandedNative=0;
        state.nativeMenu=0;
      } else if(operation==widgetId(EditorWidget::ComponentBooleanBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->booleans.size()) return outcome;
        const auto &property=entry.type->booleans[field];
        if(scene::setComponentProperty(value.components,entry.type->id,property.id,!property.read(*component),instance)!=scene::ComponentPropertyStatus::Applied) return outcome;
      } else if(operation==widgetId(EditorWidget::ComponentEnumBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->enums.size()) return outcome;
        const auto &property=entry.type->enums[field];const auto current=property.read(*component);
        bool applied=false;
        for(u32 i=0;i<property.options.size();++i) if(property.options[i].value==current) {
          const auto next=property.options[(i+1)%property.options.size()].value;
          applied=scene::setComponentProperty(value.components,entry.type->id,property.id,next,instance)==scene::ComponentPropertyStatus::Applied;break;
        }
        if(!applied) {state.status="Ajuste os limites ou o motor antes de trocar o tipo";return outcome;}
      } else if(operation==widgetId(EditorWidget::ComponentPasteBase)) {
        if(!state.componentClipboard||&state.componentClipboard->type()!=entry.type||
           !editorReferencesAccept(document,entity->id,*state.componentClipboard)||
           !value.components.replaceInstance(instance,*state.componentClipboard)) {state.status="Valores ou referências incompatíveis";return outcome;}
      } else if(operation==widgetId(EditorWidget::ComponentResetBase)) {
        const auto defaults=entry.type->create();if(!defaults||!value.components.replaceInstance(instance,*defaults)) return outcome;
      } else return outcome;
      outcome.documentChanged=history.applyValues(document,state.selection,value);
    }
    state.propertyPage=0;
    return outcome;
  }

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
    case EditorWidget::AddComponentMenu:
      if(state.componentSelection!=state.selection) {state.componentSelection=state.selection;state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=0;state.addingComponent=false;}
      state.addingComponent=!state.addingComponent;state.meshPicker=false;state.propertyPage=0;state.componentPage=0;break;
    case EditorWidget::PropertyPrevious: if(state.propertyPage) --state.propertyPage; break;
    // The rendered pager already knows its exact field rectangle and only
    // registers Next when another page exists. Do not duplicate layout math.
    case EditorWidget::PropertyNext: ++state.propertyPage;break;
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
      for(u32 i=0;i<editorCreationCatalog.size();++i) {
        const auto &entry=editorCreationCatalog[i];
        if(creationAvailable(state,i) && (query.empty()?entry.category==state.creationCategory:editorSearchKey(entry.name).find(query)!=std::string::npos)) ++count;
      }
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
      if(state.workspace!=EditorWorkspace::Play) {
        if(state.codeBuildBusy) {state.status="Aguarde a compilação antes de Play";break;}
        if(state.code && (state.code->catalogState()==EditorCodeCatalogState::Failed ||
            state.code->catalogState()==EditorCodeCatalogState::Stale || state.code->dirty())) {
          state.status="Play bloqueado: a fonte atual ainda não foi publicada";
          state.consoleCollapsed=false;state.consoleProblems=true;break;
        }
      }
      state.workspace = state.workspace == EditorWorkspace::Play ? EditorWorkspace::Scene : EditorWorkspace::Play;
      state.playPaused=false;state.playStepRequested=false;
      outcome.requestPlay = state.workspace == EditorWorkspace::Play;
      break;
    case EditorWidget::TabScene: state.workspaceMenu=false; state.workspace = EditorWorkspace::Scene; break;
    case EditorWidget::TabAssets: if(state.assetCount) {state.workspaceMenu=false; state.workspace = EditorWorkspace::Assets;} break;
    case EditorWidget::TabLighting: state.workspaceMenu=false; state.workspace = EditorWorkspace::Lighting; state.selection=document.root(); state.propertyPage=0; break;
    case EditorWidget::TabSettings: if(waterCreationAvailable(state)) {state.workspaceMenu=false; state.workspace = EditorWorkspace::Settings; state.selection=document.root();state.propertyPage=0;} break;
    case EditorWidget::InspectorTabTransform: state.tab = EditorInspectorTab::Transform; break;
    case EditorWidget::InspectorTabMaterial:
      if (const auto *selected=document.find(state.selection); selected && meshAsset(*selected)) {
        state.tab = EditorInspectorTab::Material; state.propertyPage=0;
      }
      break;
    case EditorWidget::InspectorTabProperties: state.tab = EditorInspectorTab::Properties; break;
    case EditorWidget::ToolSelect: state.tool = EditorGizmoMode::Select; break;
    case EditorWidget::ToolMove: state.tool = EditorGizmoMode::Translate; break;
    case EditorWidget::ToolRotate: state.tool = EditorGizmoMode::Rotate; break;
    case EditorWidget::ToolScale: state.tool = EditorGizmoMode::Scale; break;
    case EditorWidget::ViewModeSolid: state.showGrid = !state.showGrid; break;
    case EditorWidget::CompactPanelMenu: state.compactPanelMenu=!state.compactPanelMenu;break;
    case EditorWidget::CompactFiles: state.compactPanel=EditorScreenState::CompactPanel::Files;state.compactPanelMenu=false;break;
    case EditorWidget::CompactViewport: state.compactPanel=EditorScreenState::CompactPanel::Viewport;state.compactPanelMenu=false;break;
    case EditorWidget::HierarchyToggle: state.compactPanel=EditorScreenState::CompactPanel::Hierarchy;state.compactPanelMenu=false;break;
    case EditorWidget::InspectorToggle: state.compactPanel=EditorScreenState::CompactPanel::Inspector;state.compactPanelMenu=false;break;
    case EditorWidget::Fullscreen:
      state.compactPanel=EditorScreenState::CompactPanel::Viewport;
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
    case EditorWidget::ToggleWaterPhysics:
      if(const auto *entity=document.find(state.selection)) {
        auto value=*entity;
        if(setWaterBodyFlags(value,!waterBody(value).physicsEnabled,waterBody(value).infinite))
          outcome.documentChanged=history.applyValues(document,state.selection,value);
      }
      break;
    case EditorWidget::ToggleSceneBody:
    case EditorWidget::ToggleDynamicBody: {
      if(const auto *current=document.find(state.selection)) {
        auto value=*current;
        if(static_cast<EditorWidget>(widget)==EditorWidget::ToggleSceneBody) {
          if(physicsBody(value)) value.components.remove(EditorPhysicsBody::descriptor);
          else editPhysicsBody(value);
        } else if(physicsBody(value)) {auto *body=editPhysicsBody(value);if(body) body->motion=body->motion==scene::BodyMotion::Dynamic?scene::BodyMotion::Static:scene::BodyMotion::Dynamic;}
        outcome.documentChanged=history.applyValues(document,value.id,value);
        state.propertyPage=0;
      }
      break;
    case EditorWidget::PausePlay:
      if(state.workspace==EditorWorkspace::Play && !waterCreationAvailable(state)) {
        state.playPaused=!state.playPaused;state.playStepRequested=false;
      }
      break;
    case EditorWidget::StepPlay:
      if(state.workspace==EditorWorkspace::Play && state.playPaused && !waterCreationAvailable(state)) state.playStepRequested=true;
      break;
    }
    case EditorWidget::ToggleCharacter: {
      if(const auto *current=document.find(state.selection);current && !physicsBody(*current)) {
        auto value=*current;
        if(characterComponent(value)) value.components.remove(EditorCharacter::descriptor);
        else editCharacter(value);
        outcome.documentChanged=history.applyValues(document,value.id,value);state.propertyPage=0;
      }
      break;
    }
    case EditorWidget::ToggleCameraLook: {
      if(const auto *current=document.find(state.selection);current&&cameraComponent(*current)) {
        auto value=*current;
        if(cameraLook(value)) value.components.remove(EditorCameraLook::descriptor);else editCameraLook(value);
        outcome.documentChanged=history.applyValues(document,value.id,value);state.propertyPage=0;
      }
      break;
    }
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
