#include "editor/editor_component_impact.h"
#include "editor/editor_color_picker.h"
#include <sstream>
#include "scene/script_behavior.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include <unordered_set>
#include <cctype>
#include "editor/editor_screen.h"
#include "editor/editor_number_text.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_scene_template.h"
#include "renderer/rendering_settings_file.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_properties.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_grid.h"
#include "editor/editor_collider_geometry.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_camera_handles.h"
#include "editor/editor_component_handles.h"
#include "editor/editor_import_reconcile.h"
#include "runtime/scene_environment.h"
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

  void toggle(const UiRect &area, bool on, u32 widget, bool editable=true) {
    const UiRect bounds{area.right() - kToggleWidth, area.y + (area.height - kToggleHeight) * 0.5f,
                        kToggleWidth, kToggleHeight};
    list.addRect(bounds, on ? withAlpha(theme.color.accent, 0.28f) : theme.color.line,
                 kToggleHeight * 0.5f);
    const float knob = kToggleHeight - 5.0f;
    const float knobX = on ? bounds.right() - knob - 2.5f : bounds.x + 2.5f;
    list.addRect({knobX, bounds.y + 2.5f, knob, knob}, on ? theme.color.accent : theme.color.track,
                 knob * 0.5f);
    if(editable) router.addRegion(bounds, widget, theme.touch.minimumTarget);
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

  takeLeft(content,theme.spacing.small);
  if(content.width>=52.0f) {
    const bool labelled=content.width>=180.0f;
    const auto graphics=takeLeft(content,labelled?108.0f:40.0f);
    builder.list.addRect(graphics,builder.state.qualityPanel?theme.color.accent:theme.color.raised,
                         theme.radius.control);
    const auto icon=labelled?UiRect{graphics.x+8.0f,graphics.y,24.0f,graphics.height}:graphics;
    builder.list.addImage(centred(icon,18.0f,18.0f),static_cast<UiImageId>(UiIcon::UiSliders),
                          builder.state.qualityPanel?theme.color.accentInk:theme.color.text);
    if(labelled) builder.label({graphics.x+34.0f,graphics.y,graphics.width-38.0f,graphics.height},
                               "Gráficos",builder.state.qualityPanel?theme.color.accentInk:theme.color.text,
                               theme.type.caption,UiAlign::Center);
    builder.router.addRegion(graphics,widgetId(EditorWidget::QualityOpen),theme.touch.minimumTarget);
    takeLeft(content,theme.spacing.medium);
  }
  // Ampliação temporal na barra: mostra o que o renderer EXECUTOU e alterna
  // Desligada → TAA nativo → Arm ASR → AMD FSR 2, pulando o que o aparelho
  // recusa (o motivo vai para a barra de estado).
  if(builder.state.workspace==EditorWorkspace::Scene || builder.state.workspace==EditorWorkspace::Play) {
    if(content.width>=250.0f) {
      const auto &state=builder.state;
      const char *executed=renderer::isTemporalUpscaler(state.qualityExecutedUpscaler)
          ?renderer::upscalingFilterLabel(state.qualityExecutedUpscaler)
          :state.qualityTemporalAaExecuted?"TAA nativo":"Sem temporal";
      const auto chip=takeLeft(content,112.0f);
      const bool temporalActive=renderer::isTemporalUpscaler(state.qualityExecutedUpscaler)||state.qualityTemporalAaExecuted;
      builder.list.addRect(chip,temporalActive?withAlpha(theme.color.accent,0.20f):theme.color.raised,theme.radius.control);
      builder.label(chip,executed,temporalActive?theme.color.accent:theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(chip,widgetId(EditorWidget::QualityTemporalModeQuick),theme.touch.minimumTarget);
      takeLeft(content,theme.spacing.small);
    }
  }
  if(builder.state.workspace==EditorWorkspace::Scene &&
     builder.state.qualityTemporalAvailable && builder.state.qualityTemporalDebug!=0 &&
     content.width>=136.0f) {
    constexpr const char *quickNames[]{"","Temporal: profundidade","Temporal: histórico","Temporal: rejeição",
                                       "Temporal: vetor","Temporal: reatividade","Temporal: composição"};
    const auto quick=takeLeft(content,128.0f);
    builder.list.addRect(quick,withAlpha(theme.color.accent,0.20f),theme.radius.control);
    builder.label(quick,quickNames[std::min(builder.state.qualityTemporalDebug,6u)],
                  theme.color.accent,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(quick,widgetId(EditorWidget::QualityTemporalDebugQuick),
                             theme.touch.minimumTarget);
    takeLeft(content,theme.spacing.small);
  }
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

  if(state.workspace==EditorWorkspace::Scene && state.showComponentVisuals && !state.cameraViewEntity) {
    const float aspect=state.view->frustum.tangentHalfHorizontal/state.view->frustum.tangentHalfVertical;
    const auto visuals=collectComponentVisuals(*state.document,state.selection,aspect,state.resources);
    for(const auto &v:visuals) {
      const auto color=v.enabled?theme.color.accent:theme.color.textMuted;
      for(const auto &line:v.segments) {
        UiPoint a,b;if(projectSegmentToScreen(*state.view,line.a,line.b,a,b)) builder.list.addLine(a,b,color,1.3f);
      }
      if(!v.marker) continue;
      const auto p=projectWorldToScreen(*state.view,v.origin);
      if(!p.valid || !viewport.contains(p.screen)) continue;
      const UiRect icon{p.screen.x-14+v.markerIndex*32.f,p.screen.y-14,28,28};
      if(v.markerIndex) builder.list.addLine(p.screen,{icon.x+14,icon.y+14},theme.color.textMuted,1);
      builder.list.addRect(icon,withAlpha(theme.color.surface,.9f),8);
      builder.list.addImage(deflate(icon,UiInsets::all(3)),static_cast<UiImageId>(v.icon),v.enabled?0xffffffff:theme.color.textMuted);
      if(v.entity<0x01000000u) builder.router.addRegion(icon,widgetId(EditorWidget::ComponentVisualBase)+v.entity,32);
    }
  }
  // Collider outlines now share the component visual registry. Joint anchors
  // retain their specialized connection view until that provider is migrated.
  const auto *selectedVisual=state.document->find(state.selection);
  const auto *expandedVisual=selectedVisual?selectedVisual->components.findInstance(state.expandedNative):nullptr;
  if(state.showComponentVisuals && !state.cameraViewEntity && expandedVisual && &expandedVisual->type()==&scene::Joint::descriptor) buildPhysicsOverlay(builder);
  if(state.workspace==EditorWorkspace::Scene) {
    const UiRect controls{viewport.right()-42,viewport.y+8,34,34};
    builder.iconButton(controls,UiIcon::EditorAuthorCamera,widgetId(state.cameraViewEntity?EditorWidget::CameraViewClose:EditorWidget::ComponentVisualsToggle),state.cameraViewEntity || state.showComponentVisuals);
  }
  const EditorEntity *entity = state.document->find(state.selection);
  if (entity != nullptr && state.tool != EditorGizmoMode::Select &&
      state.workspace == EditorWorkspace::Scene && !state.cameraViewEntity && !(waterRoute(*entity).count && state.waterTab==1)) {
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
  if(entity && state.showComponentVisuals && !state.cameraViewEntity &&
     state.workspace==EditorWorkspace::Scene && expandedVisual &&
     &expandedVisual->type()==&scene::Camera::descriptor) {
    const char *labels[]{"Lente","Próximo","Distante"};
    for(u32 kind=0;kind<3;++kind) {
      EditorCameraHandle handle;
      if(!cameraHandleGeometry(*state.document,entity->id,kind,handle)) continue;
      const auto p=projectWorldToScreen(*state.view,handle.point);float parameter;
      if(!p.valid || !viewport.contains(p.screen) ||
         !cameraHandleRayParameter(*state.view,handle,p.screen,parameter)) continue;
      const UiRect target{p.screen.x-18,p.screen.y-18,36,36};
      if(!viewport.contains({target.x,target.y}) || !viewport.contains({target.right(),target.bottom()})) continue;
      builder.list.addRect(target,theme.color.surface,theme.radius.control);
      builder.list.addRect({p.screen.x-6,p.screen.y-6,12,12},theme.color.accent,4);
      builder.label({p.screen.x+20,p.screen.y-12,72,24},labels[kind],theme.color.text,theme.type.caption);
      builder.router.addRegion(target,widgetId(EditorWidget::CameraHandleBase)+kind);
    }
  }
  if(entity && state.showComponentVisuals && !state.cameraViewEntity &&
     state.workspace==EditorWorkspace::Scene && expandedVisual &&
     (&expandedVisual->type()==&scene::Light::descriptor ||
      &expandedVisual->type()==&scene::Environment::descriptor)) {
    constexpr const char *labels[]{"Alcance","Cone interno","Cone externo",
        "Tamanho X","Tamanho Y","Tamanho Z","Raio","Mistura"};
    for(u32 index=0;index<static_cast<u32>(EditorComponentHandleKind::Count);++index) {
      EditorComponentHandle handle;
      const auto kind=static_cast<EditorComponentHandleKind>(index);
      if(!componentHandleGeometry(*state.document,entity->id,expandedVisual->instanceId(),kind,handle)) continue;
      const auto point=projectWorldToScreen(*state.view,handle.point);float parameter;
      if(!point.valid || !viewport.contains(point.screen) ||
         !cameraHandleRayParameter(*state.view,handle,point.screen,parameter)) continue;
      const UiRect target{point.screen.x-16,point.screen.y-16,32,32};
      if(!viewport.contains({target.x,target.y}) || !viewport.contains({target.right(),target.bottom()})) continue;
      builder.list.addRect(target,theme.color.surface,theme.radius.control);
      builder.list.addRect({point.screen.x-5,point.screen.y-5,10,10},theme.color.accent,4);
      builder.label({point.screen.x+18,point.screen.y-12,88,24},labels[index],theme.color.text,theme.type.caption);
      builder.router.addRegion(target,widgetId(EditorWidget::ComponentHandleBase)+index);
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
    builder.iconButton(takeRight(header,34),UiIcon::AssetsTexture,widgetId(EditorWidget::ImportTexture));
    builder.iconButton(takeRight(header,34),UiIcon::AssetsImport,widgetId(EditorWidget::ImportModel));
    builder.iconButton(takeRight(header,34),UiIcon::AssetsFolderOpen,widgetId(EditorWidget::ImportFolder));
    builder.iconButton(takeRight(header,34),UiIcon::LightingSky,widgetId(EditorWidget::ImportEnvironment));
    auto newFile=takeRight(header,34),newFolder=takeRight(header,34);
    builder.list.addImage(centred(newFile,19,19),static_cast<UiImageId>(UiIcon::IdeAdd),0xffffffff);
    builder.list.addImage(centred(newFolder,19,19),static_cast<UiImageId>(UiIcon::IdeFiles),0xffffffff);
    builder.router.addRegion(newFile,widgetId(EditorWidget::CodeNew));
    builder.router.addRegion(newFolder,widgetId(EditorWidget::CodeNewFolder));
    builder.list.addRect({content.x,header.bottom(),content.width,1},theme.color.lineSoft);
  } else {
    builder.router.addRegion(header,widgetId(EditorWidget::FilesCollapse));
    builder.iconButton(takeRight(header,28),UiIcon::ScriptingCode,widgetId(EditorWidget::CodeOpen));
    builder.iconButton(takeRight(header,28),UiIcon::AssetsTexture,widgetId(EditorWidget::ImportTexture));
    builder.iconButton(takeRight(header,28),UiIcon::AssetsImport,widgetId(EditorWidget::ImportModel));
    builder.iconButton(takeRight(header,28),UiIcon::AssetsFolderOpen,widgetId(EditorWidget::ImportFolder));
    builder.iconButton(takeRight(header,28),UiIcon::LightingSky,widgetId(EditorWidget::ImportEnvironment));
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
  const auto *selectedRecord=builder.state.assetRegistry?
      builder.state.assetRegistry->findByPath(builder.state.selectedFile):nullptr;
  // Fonte de modelo é o que o REGISTRO diz, não a extensão: um GLB, ou o
  // `.gltf` de uma fonte em pasta (S0). GLB ainda não registrado também entra,
  // para Reimportar registrá-lo. "Texturas" extrai imagens EMBUTIDAS (R4): só
  // existe no GLB — na pasta as imagens já são arquivos do projeto.
  const bool glb=builder.state.selectedFile.ends_with(".glb");
  if(glb || (selectedRecord&&selectedRecord->type==resources::AssetType::Mesh)) {
    auto actions=takeBottom(content,36);
    const u32 count=glb?3u:2u;
    for(const auto &item:std::array<std::pair<const char *,EditorWidget>,3>{{
        {"Instanciar",EditorWidget::AssetInstantiate},{"Reimportar",EditorWidget::AssetReimport},
        {"Texturas",EditorWidget::AssetExtractTextures}}}) {
      if(item.second==EditorWidget::AssetExtractTextures && !glb) continue;
      auto button=deflate(takeLeft(actions,content.width/static_cast<float>(count)),UiInsets::all(3));
      builder.list.addRect(button,theme.color.raised,theme.radius.control);
      builder.label(button,item.first,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(button,widgetId(item.second));
    }
  } else if(selectedRecord&&selectedRecord->type==resources::AssetType::EnvironmentMap) {
    auto actions=takeBottom(content,36);
    auto button=deflate(actions,UiInsets::all(3));
    builder.list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Reimportar HDRI",theme.color.text,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(button,widgetId(EditorWidget::AssetReimport));
  } else if(selectedRecord&&selectedRecord->type==resources::AssetType::Texture) {
    auto actions=takeBottom(content,36);
    auto button=deflate(actions,UiInsets::all(3));
    builder.list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Reimportar textura",theme.color.text,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(button,widgetId(EditorWidget::AssetReimport));
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
// Abas refletidas de um componente, na ordem em que aparecem. Desenho e toque
// usam esta mesma lista: se divergirem, o toque numa aba abre outra.
std::vector<std::string_view> componentGroups(const scene::ComponentValue &component) {
  std::vector<std::string_view> groups;
  const auto collect=[&](const auto &properties) {
    for(const auto &p:properties) if(p.presentation.isVisible(component)&&!p.presentation.group.empty() &&
        std::find(groups.begin(),groups.end(),p.presentation.group)==groups.end()) groups.push_back(p.presentation.group);
  };
  const auto &type=component.type();
  collect(type.numbers);collect(type.booleans);collect(type.enums);
  collect(type.references);collect(type.resourceBindings);collect(type.slotNumbers);
  return groups;
}

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

  // R4: os quatro bindings de textura vêm antes dos números, na mesma lista
  // paginada. Cada linha mostra a textura efetiva no alcance e de onde ela vem.
  static constexpr const char *bindingNames[scene::MaterialTextureCount]{"Mapa: cor","Mapa: normal","Mapa: metal/rug.","Mapa: emissão"};
  const u32 textureRows=scene::MaterialTextureCount;
  // R4: três linhas de superfície (alfa, corte, faces) depois das texturas.
  const u32 surfaceRows=3;
  // R4: sete linhas de oclusão, canais, normal, alfa e isolamento.
  const u32 channelRows=7;
  const u32 count=textureRows+surfaceRows+channelRows+static_cast<u32>(scene::meshRendererNumbers.size());
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 entry=page*perPage;entry<count && entry<(page+1)*perPage;++entry) {
    auto row=takeTop(content,std::min(40.0f,content.height));
    if(entry<textureRows) {
      builder.label(takeLeft(row,row.width*.42f),bindingNames[entry],theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets::symmetric(8,2));
      builder.label(takeTop(inner,inner.height*.55f),view.textureNames[entry],theme.color.text,theme.type.caption);
      builder.label(inner,view.textureOrigins[entry],theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(row,widgetId(EditorWidget::MaterialTextureBase)+entry);
      continue;
    }
    if(entry<textureRows+surfaceRows) {
      const u32 kind=entry-textureRows;
      static constexpr const char *surfaceNames[]{"Alfa","Corte alfa","Faces"};
      builder.label(takeLeft(row,row.width*.42f),surfaceNames[kind],theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets::symmetric(8,2));
      if(kind==1) {
        if(view.cutoffEditable) {
          const auto down=takeLeft(inner,32),up=takeRight(inner,32);
          builder.label(down,"-",theme.color.text,theme.type.body,UiAlign::Center);
          builder.label(up,"+",theme.color.text,theme.type.body,UiAlign::Center);
          builder.router.addRegion(down,widgetId(EditorWidget::MaterialCutoffDown));
          builder.router.addRegion(up,widgetId(EditorWidget::MaterialCutoffUp));
          char cutoff[16];std::snprintf(cutoff,sizeof(cutoff),"%.2f",static_cast<double>(view.alphaCutoff));
          builder.label(inner,cutoff,theme.color.text,theme.type.numeric,UiAlign::Center);
        } else {
          builder.label(inner,view.cutoffLabel,theme.color.textMuted,theme.type.caption);
        }
        continue;
      }
      const bool alpha=kind==0;
      builder.label(takeTop(inner,inner.height*.55f),alpha?view.alphaLabel:view.sidesLabel,theme.color.text,theme.type.caption);
      builder.label(inner,alpha?view.alphaOrigin:view.sidesOrigin,theme.color.textMuted,theme.type.caption);
      // Faces sem culling no aparelho não recebe toque: não haveria efeito visível.
      if(alpha || state.materialCulling)
        builder.router.addRegion(row,widgetId(alpha?EditorWidget::MaterialAlphaCycle:EditorWidget::MaterialSidesCycle));
      continue;
    }
    if(entry<textureRows+surfaceRows+channelRows) {
      const u32 kind=entry-textureRows-surfaceRows;
      static constexpr const char *channelNames[]{"Oclusão","Textura de oclusão","Força da oclusão","Canais","Mapa normal","Origem do alfa","Isolar na prévia"};
      builder.label(takeLeft(row,row.width*.42f),channelNames[kind],theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets::symmetric(8,2));
      if(kind==2) {
        const auto down=takeLeft(inner,32),up=takeRight(inner,32);
        builder.label(down,"-",theme.color.text,theme.type.body,UiAlign::Center);
        builder.label(up,"+",theme.color.text,theme.type.body,UiAlign::Center);
        builder.router.addRegion(down,widgetId(EditorWidget::MaterialOcclusionStrengthDown));
        builder.router.addRegion(up,widgetId(EditorWidget::MaterialOcclusionStrengthUp));
        builder.label(inner,view.occlusionStrengthLabel,theme.color.text,theme.type.numeric,UiAlign::Center);
        continue;
      }
      if(kind==3) {
        const float third=inner.width/3;
        for(u32 channel=0;channel<3;++channel) {
          const auto box=channel<2?takeLeft(inner,third):inner;
          builder.label(box,view.channelLabels[channel],theme.color.text,theme.type.caption,UiAlign::Center);
          builder.router.addRegion(box,widgetId(EditorWidget::MaterialChannelRoughness)+channel);
        }
        continue;
      }
      static constexpr EditorWidget channelWidgets[]{EditorWidget::MaterialOcclusionSourceCycle,EditorWidget::MaterialOcclusionTexture,
          EditorWidget::MaterialOcclusionStrengthDown,EditorWidget::MaterialChannelRoughness,EditorWidget::MaterialNormalFlipCycle,
          EditorWidget::MaterialAlphaSourceCycle,EditorWidget::MaterialIsolateCycle};
      std::string value,origin;
      switch(kind) {
      case 0: value=view.occlusionLabel;origin=view.occlusionOrigin;break;
      case 1: value=view.occlusionTextureLabel;origin="vale quando a oclusão é textura própria";break;
      case 4: value=view.normalFlipLabel;origin=view.normalFlipOrigin;break;
      case 5: value=view.alphaSourceLabel;origin=view.alphaSourceOrigin;break;
      default: value=view.isolateLabel;origin="só na prévia do editor; não é salvo";break;
      }
      builder.label(takeTop(inner,inner.height*.55f),value,theme.color.text,theme.type.caption);
      builder.label(inner,origin,theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(row,widgetId(channelWidgets[kind]));
      continue;
    }
    const u32 field=entry-textureRows-surfaceRows-channelRows;
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

// R4: escolha da textura de um binding, no alcance em edição (esta instância ou
// o material compartilhado do slot).
void buildTexturePicker(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  static constexpr const char *bindings[scene::MaterialTextureCount+1]{"cor base","normal","metal/rugosidade","emissão","oclusão"};
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  builder.router.addRegion(back,widgetId(EditorWidget::TexturePickerClose));
  builder.label(title,std::string("Textura de ")+bindings[std::min(state.textureBinding,scene::MaterialTextureCount)]+
                (state.materialShared?" · compartilhado":" · esta instância"),theme.color.text,theme.type.caption);
  // R4: amostragem do binding. O conjunto de UV vale para qualquer textura;
  // repetição e filtro são o sampler da textura do projeto e não recebem toque
  // quando o binding usa a textura da fonte. Vem depois de Herdar e Sem textura:
  // as escolhas principais do seletor não podem sair da tela por causa dele.
  const auto samplingControls=[&]() {
  if(content.height>=40) {
    auto sampling=takeTop(content,40);
    const float third=sampling.width/3;
    const auto button=[&](UiRect box,const std::string &text,bool enabled,u32 widget) {
      box=deflate(box,UiInsets::all(2));
      builder.list.addRect(box,theme.color.raised,theme.radius.control);
      builder.label(box,text,enabled?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(enabled) builder.router.addRegion(box,widget);
    };
    button(takeLeft(sampling,third),state.textureUvLabel,true,widgetId(EditorWidget::TextureSamplingUv));
    button(takeLeft(sampling,third),state.textureWrapLabel,state.textureSamplerEditable,widgetId(EditorWidget::TextureSamplingWrap));
    button(sampling,state.textureFilterLabel,state.textureSamplerEditable,widgetId(EditorWidget::TextureSamplingFilter));
    if(!state.textureSamplerEditable && content.height>=22)
      builder.label(takeTop(content,22),"Repetição e filtro valem só com textura do projeto",theme.color.textMuted,theme.type.caption);
  }
  // R4: transformação de UV do binding. Vale com qualquer textura: o shader a aplica.
  if(content.height>=36*3) {
    const auto stepper=[&](UiRect row,const std::string &text,u32 field) {
      const auto down=deflate(takeLeft(row,34),UiInsets::all(2)),up=deflate(takeRight(row,34),UiInsets::all(2));
      builder.list.addRect(down,theme.color.raised,theme.radius.control);
      builder.label(down,"-",theme.color.text,theme.type.body,UiAlign::Center);
      builder.router.addRegion(down,widgetId(EditorWidget::TextureUvStepBase)+field*2);
      builder.list.addRect(up,theme.color.raised,theme.radius.control);
      builder.label(up,"+",theme.color.text,theme.type.body,UiAlign::Center);
      builder.router.addRegion(up,widgetId(EditorWidget::TextureUvStepBase)+field*2+1);
      builder.label(row,text,theme.color.text,theme.type.caption,UiAlign::Center);
    };
    auto offsets=takeTop(content,36);
    stepper(takeLeft(offsets,offsets.width*.5f),state.textureUvLabels[0],0);
    stepper(offsets,state.textureUvLabels[1],1);
    auto scales=takeTop(content,36);
    stepper(takeLeft(scales,scales.width*.5f),state.textureUvLabels[2],2);
    stepper(scales,state.textureUvLabels[3],3);
    auto rotation=takeTop(content,36);
    stepper(takeLeft(rotation,rotation.width*.5f),state.textureUvLabels[4],4);
    const auto reset=deflate(rotation,UiInsets::all(2));
    builder.list.addRect(reset,theme.color.raised,theme.radius.control);
    builder.label(reset,"Zerar transformação",theme.color.text,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(reset,widgetId(EditorWidget::TextureUvReset));
  }
  };
  const auto option=[&](const std::string &label,const std::string &detail,u32 widget) {
    if(content.height<48) return;
    auto row=takeTop(content,48);const auto hit=row;
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    takeLeft(row,12);
    builder.label(takeTop(row,25),label,theme.color.text,theme.type.body);
    builder.label(row,detail,theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widget);
  };
  option("Herdar",state.materialShared?"volta à textura da fonte":"do material do projeto, senão da fonte",
         widgetId(EditorWidget::TextureUseInherited));
  option("Sem textura","o binding fica só com os fatores",widgetId(EditorWidget::TextureUseNone));
  if(state.textureBinding<scene::MaterialTextureCount) samplingControls();
  else if(content.height>=22)
    builder.label(takeTop(content,22),"A oclusão usa o conjunto de UV e a amostragem do mapa metal/rugosidade",theme.color.textMuted,theme.type.caption);
  const u32 rows=static_cast<u32>(state.projectTextureNames.size());
  if(!rows) {
    builder.label(takeTop(content,40),"Nenhuma textura no projeto. Use Importar textura em Arquivos.",
                  theme.color.textMuted,theme.type.caption);
    return;
  }
  auto footer=takeBottom(content,36),previous=takeLeft(footer,36),next=takeRight(footer,36);
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height)/48));
  const u32 pages=std::max(1u,(rows+perPage-1)/perPage),page=std::min(state.meshPage,pages-1);
  for(u32 row=page*perPage;row<rows && row<(page+1)*perPage;++row) {
    if(content.height<48) break;
    auto line=takeTop(content,48);
    const auto viewArea=takeRight(line,60);
    const auto hit=line;
    builder.list.addRect(deflate(line,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    // R4: miniatura do atlas de prévia, com a proporção da imagem.
    const auto thumb=deflate(takeLeft(line,48),UiInsets::all(5));
    if(row<state.projectTextureThumbs.size() && !state.projectTextureThumbs[row].isEmpty()) {
      const auto &texels=state.projectTextureThumbs[row];
      const float fit=std::min(thumb.width/texels.width,thumb.height/texels.height);
      const float w=texels.width*fit,h=texels.height*fit;
      builder.list.addPreviewImage({thumb.x+(thumb.width-w)*.5f,thumb.y+(thumb.height-h)*.5f,w,h},texels);
    } else {
      builder.list.addRect(thumb,theme.color.lineSoft,4);
    }
    takeLeft(line,6);
    builder.label(takeTop(line,25),state.projectTextureNames[row],theme.color.text,theme.type.body);
    builder.label(line,row<state.projectTextureDetails.size()?state.projectTextureDetails[row]:std::string(),theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::TextureChoiceBase)+row);
    const auto viewButton=deflate(viewArea,UiInsets::all(4));
    builder.list.addRect(viewButton,theme.color.raised,theme.radius.control);
    builder.label(viewButton,"Ver",theme.color.text,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(viewButton,widgetId(EditorWidget::TextureViewBase)+row);
  }
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
  builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::MeshPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::MeshNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

// R4: visualizador de textura. A imagem vem do atlas de prévia com o nível de
// mip e o canal escolhidos; os dados do arquivo ficam embaixo.
void buildTextureViewer(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  builder.router.addRegion(back,widgetId(EditorWidget::TextureViewerClose));
  builder.label(title,state.textureViewerTitle,theme.color.text,theme.type.caption);
  auto controls=takeBottom(content,std::min(content.height*.8f,346.0f));
  auto image=deflate(content,UiInsets::all(4));
  builder.list.addRect(image,theme.color.raised,theme.radius.control);
  if(!state.textureViewerImage.isEmpty() && !image.isEmpty()) {
    const auto &texels=state.textureViewerImage;
    const float fit=std::min(image.width/texels.width,image.height/texels.height);
    const float w=texels.width*fit,h=texels.height*fit;
    builder.list.addPreviewImage({image.x+(image.width-w)*.5f,image.y+(image.height-h)*.5f,w,h},texels);
  } else {
    builder.label(image,"Imagem indisponível",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  if(controls.height<38) return;
  auto channel=takeTop(controls,38);
  builder.label(takeLeft(channel,channel.width*.3f),"Canal",theme.color.textDim,theme.type.caption);
  const auto channelBox=deflate(channel,UiInsets::all(2));
  builder.list.addRect(channelBox,theme.color.raised,theme.radius.control);
  builder.label(channelBox,state.textureViewerChannelLabel,theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(channelBox,widgetId(EditorWidget::TextureViewerChannel));
  if(controls.height<38) return;
  auto mip=takeTop(controls,38);
  builder.label(takeLeft(mip,mip.width*.3f),"Mip",theme.color.textDim,theme.type.caption);
  auto mipBox=deflate(mip,UiInsets::all(2));
  builder.list.addRect(mipBox,theme.color.raised,theme.radius.control);
  if(state.textureViewerLevels>1) {
    const auto down=takeLeft(mipBox,32),up=takeRight(mipBox,32);
    builder.label(down,"-",state.textureViewerLevel?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(up,"+",state.textureViewerLevel+1<state.textureViewerLevels?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    if(state.textureViewerLevel) builder.router.addRegion(down,widgetId(EditorWidget::TextureViewerMipDown));
    if(state.textureViewerLevel+1<state.textureViewerLevels) builder.router.addRegion(up,widgetId(EditorWidget::TextureViewerMipUp));
  }
  builder.label(mipBox,state.textureViewerLevelLabel,theme.color.text,theme.type.caption,UiAlign::Center);
  if(controls.height<38) return;
  // Zoom central e fundo sob o alfa (o fundo só aparece em RGBA; nos canais a
  // imagem é opaca e o botão não recebe toque).
  auto view=takeTop(controls,38);
  auto zoomBox=deflate(takeLeft(view,view.width*.5f),UiInsets::all(2));
  const auto backgroundBox=deflate(view,UiInsets::all(2));
  builder.list.addRect(zoomBox,theme.color.raised,theme.radius.control);
  builder.label(zoomBox,"Zoom "+state.textureViewerZoomLabel,theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(zoomBox,widgetId(EditorWidget::TextureViewerZoom));
  const bool rgba=state.textureViewerChannel==0;
  builder.list.addRect(backgroundBox,theme.color.raised,theme.radius.control);
  builder.label(backgroundBox,"Fundo "+state.textureViewerBackgroundLabel,rgba?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
  if(rgba) builder.router.addRegion(backgroundBox,widgetId(EditorWidget::TextureViewerBackground));
  // O perfil tem duas páginas curtas para manter Aplicar/Reverter alcançáveis
  // também no painel estreito do aparelho.
  if(controls.height>=36+26+30*2) {
    const auto profileButton=[&](UiRect box,const std::string &text,u32 widget) {
      box=deflate(box,UiInsets::all(2));
      builder.list.addRect(box,theme.color.raised,theme.radius.control);
      builder.label(box,text,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(box,widget);
    };
    const u32 profilePage=std::min(state.textureProfilePage,2u);
    // Índices 0..9 são campos do perfil; 10 e 11 são leitura do streaming.
    constexpr u32 editableProfileFields=10;
    for(u32 line=0;line<2;++line) {
      auto row=takeTop(controls,30);
      const u32 first=profilePage*4+line*2;
      for(u32 cell=0;cell<2;++cell) {
        const u32 index=first+cell;
        auto box=cell?row:takeLeft(row,row.width*.5f);
        if(index<editableProfileFields)
          profileButton(box,state.textureProfileLabels[index],widgetId(EditorWidget::TextureProfileInterpretation)+index);
        else builder.label(deflate(box,UiInsets::all(2)),state.textureProfileLabels[index],theme.color.textDim,
                           theme.type.caption,UiAlign::Center);
      }
    }
    auto pager=takeTop(controls,26);
    const auto previous=takeLeft(pager,65),next=takeRight(pager,65);
    builder.label(previous,"‹ Perfil",profilePage?theme.color.text:theme.color.textMuted,theme.type.caption);
    builder.label(next,"Perfil ›",profilePage<2?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::End);
    builder.label(pager,std::to_string(profilePage+1)+" / 3",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(profilePage) builder.router.addRegion(previous,widgetId(EditorWidget::TextureProfilePrevious));
    if(profilePage<2) builder.router.addRegion(next,widgetId(EditorWidget::TextureProfileNext));
  }
  if(controls.height>=36) {
    auto row=takeTop(controls,36);
    const auto revert=deflate(takeLeft(row,row.width*.42f),UiInsets::all(2));
    const auto apply=deflate(row,UiInsets::all(2));
    builder.list.addRect(revert,theme.color.raised,theme.radius.control);
    builder.label(revert,"Reverter",state.textureProfileDirty?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.list.addRect(apply,state.textureProfileDirty?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(apply,"Aplicar e republicar",state.textureProfileDirty?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(state.textureProfileDirty) {
      builder.router.addRegion(revert,widgetId(EditorWidget::TextureProfileRevert));
      builder.router.addRegion(apply,widgetId(EditorWidget::TextureProfileApply));
    }
  }
  if(controls.height<20) return;
  builder.list.pushClip(controls);
  builder.label(controls,state.textureResidencyLabel+" · "+state.textureViewerInfo,theme.color.textMuted,theme.type.caption);
  builder.list.popClip();
}

// Descriptor fields are addressed by stable instance/property IDs on edit.
// Widget indices exist only for one rendered frame, never in the scene archive.
void buildComponentFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,
                          const EditorComponentEntry &entry,u32 index) {
  const auto &theme=builder.theme;
  const auto *component=entity.components.at(index);if(!component || &component->type()!=entry.type) return;
  // An attached singleton is not an error. Show actual unmet requirements,
  // rather than the Add menu's "already exists" state on its own inspector.
  for(const auto &requirement:entry.schema->requirements) if(!entity.components.find(requirement.typeId))
    builder.label(takeTop(content,28),requirement.message,theme.color.warning,theme.type.caption);
  if(entry.type==&scene::Camera::descriptor) {
    auto actions=takeTop(content,40);const float w=actions.width/4;
    const EditorWidget commands[]{EditorWidget::CameraView,EditorWidget::CameraPilot,EditorWidget::CameraAlignView,EditorWidget::CameraPreviewPin};
    const char *names[]{"Ver","Pilotar","Alinhar","Prévia"};
    for(u32 i=0;i<4;++i) {
      auto box=deflate(takeLeft(actions,w),UiInsets::all(3));
      const auto hit=box;
      builder.list.addRect(box,theme.color.raised,8);
      builder.label(box,names[i],theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(hit,widgetId(commands[i]));
    }
  }
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
  // Malha já navega entre Geometria e o editor de Material por slot acima.
  // As abas refletidas repetiriam esses nomes sem acrescentar controles.
  const auto groups=mesh?std::vector<std::string_view>{}:componentGroups(*component);
  std::string_view group=mesh?std::string_view{"Geometria"}:std::string_view{builder.state.componentGroup};
  if(!groups.empty() && std::find(groups.begin(),groups.end(),group)==groups.end()) group=groups.front();
  if(groups.size()>1) {
    // Componentes extensos (Ambiente, materiais futuros) não espremem seis
    // nomes em uma faixa ilegível. A mesma navegação vira uma grade estável de
    // até quatro abas por linha, preservando IDs e área de toque.
    constexpr u32 maximumColumns=4;
    const u32 rows=(static_cast<u32>(groups.size())+maximumColumns-1)/maximumColumns;
    auto tabs=takeTop(content,36.0f*static_cast<float>(rows));
    builder.list.addRect(tabs,theme.color.silhouette,10);
    u32 first=0;
    for(u32 row=0;row<rows;++row) {
      const u32 count=std::min(maximumColumns,static_cast<u32>(groups.size())-first);
      UiRect rowTabs{tabs.x,tabs.y+36.0f*static_cast<float>(row),tabs.width,36};
      const float width=rowTabs.width/static_cast<float>(count);
      for(u32 column=0;column<count;++column) {
        const u32 i=first+column;
        auto tab=deflate(takeLeft(rowTabs,width),UiInsets::all(3));const bool active=group==groups[i];
        if(active) {builder.list.addRect(tab,theme.color.raised,8);builder.list.addRect({tab.x+9,tab.bottom()-2,std::max(0.f,tab.width-18),2},theme.color.accent,1);}
        builder.label(tab,groups[i],active?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
        builder.router.addRegion(tab,widgetId(EditorWidget::ComponentGroupBase)+i);
      }
      first+=count;
    }
    takeTop(content,6);
  }
  const auto show=[&](const scene::PropertyPresentation &p) {return p.isVisible(*component)&&(p.group.empty()||p.group==group);};
  struct Field {u32 kind,index,slot=0;}; // 0 bool, 1 enum, 2 number, 3 action, 4 object ref, 5 triple, 6 resource, 7 collider insight, 8 volume insight, 9 slot number
  std::vector<Field> fields;
  if(entry.type==&scene::Collider::descriptor && group=="Cozimento") fields.push_back({7,0});
  if(entry.type==&scene::Environment::descriptor && group=="Volume") fields.push_back({8,0});
  if(!mesh || !builder.state.meshTab) {
    for(u32 i=0;i<entry.type->booleans.size();++i) if(show(entry.type->booleans[i].presentation)) fields.push_back({0,i});
    for(u32 i=0;i<entry.type->enums.size();++i) {
      if(!show(entry.type->enums[i].presentation)) continue;
      fields.push_back({1,i});
    }
  }
  for(u32 i=0;i<entry.type->references.size();++i) if(show(entry.type->references[i].presentation)) fields.push_back({4,i});
  // O seletor de malha atende qualquer binding refletido de malha. Material e
  // textura continuam nos editores próprios por slot, que também configuram
  // superfície, canais e amostragem no mesmo contexto.
  for(u32 i=0;i<entry.type->resourceBindings.size();++i) {
    const auto &binding=entry.type->resourceBindings[i];
    if(mesh||(binding.kind!=resources::AssetType::Mesh&&
             binding.kind!=resources::AssetType::EnvironmentProfile&&
             binding.kind!=resources::AssetType::EnvironmentMap&&
             binding.kind!=resources::AssetType::AnimationClip)||!show(binding.presentation)) continue;
    for(u32 slot=0;slot<binding.slotCount(*component);++slot) fields.push_back({6,i,slot});
  }
  if(mesh && !builder.state.meshTab) {
    fields.push_back({3,widgetId(EditorWidget::MeshChoose)});
    fields.push_back({3,widgetId(EditorWidget::ToggleCastShadow)});
  } else {
    if(mesh) fields.push_back({3,widgetId(EditorWidget::MaterialRestore)});
    // Ajustar escolhe a primitiva que contém a malha; com a forma Malha a forma
    // já É a malha, e o botão só trocaria a escolha do autor por uma caixa.
    if(entry.type==&EditorCollider::descriptor && group=="Forma" && meshAsset(entity) &&
       !scene::colliderIsMesh(*component)) fields.push_back({3,widgetId(EditorWidget::ColliderFit)});
    if(entry.type==&scene::LodGroup::descriptor && group=="Limites") fields.push_back({3,widgetId(EditorWidget::LodGroupFit)});
    if(entry.type==&scene::LodGroup::descriptor && group=="Níveis" && !builder.state.lodStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::LodGroupStatus)});
    if(entry.type==&scene::SkinnedMesh::descriptor && group=="Skin" && !builder.state.skinStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::SkinnedMeshStatus)});
    if(entry.type==&scene::Animation::descriptor && group=="Clipes" && !builder.state.animationStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::AnimationStatus)});
    for(u32 i=0;i<entry.type->numbers.size();++i) {
      if(!show(entry.type->numbers[i].presentation)) continue;
      bool grouped=false;
      for(u32 t=0;t<entry.type->triples.size();++t) {
        const auto &triple=entry.type->triples[t];
        bool complete=true;
        for(const auto channel:triple.channels) {
          bool found=false;
          for(const auto &p:entry.type->numbers) if(p.id==channel && show(p.presentation)) found=true;
          complete=complete&&found;
        }
        if(!complete) continue;
        for(u32 axis=0;axis<3;++axis) if(entry.type->numbers[i].id==triple.channels[axis]) {
          grouped=true;if(axis==0) fields.push_back({5,t});
        }
      }
      if(!grouped) fields.push_back({2,i});
    }
    for(u32 i=0;i<entry.type->slotNumbers.size();++i) {
      const auto &property=entry.type->slotNumbers[i];
      if(!show(property.presentation)) continue;
      for(u32 slot=0;slot<property.slotCount(*component) && slot<256;++slot) fields.push_back({9,i,slot});
    }
  }
  if(content.height<40) return;
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(fields.size())+perPage-1)/perPage);
  const u32 page=std::min(builder.state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 row=page*perPage;row<fields.size() && row<(page+1)*perPage;++row) {
    const auto f=fields[row];auto slot=takeTop(content,std::min(40.0f,content.height));const auto hit=slot;
    if(f.kind==8) {
      std::string summary="Influência indisponível";bool warning=true;
      std::vector<renderer::SceneEnvironmentVolume> volumes;
      if(builder.state.view && runtime::collectSceneEnvironmentVolumes(*builder.state.document,volumes))
        for(const auto &volume:volumes) if(volume.stableId==entity.id) {
          const float amount=renderer::sceneEnvironmentVolumeInfluence(volume,builder.state.view->frustum.cameraPosition);
          summary="Na vista · "+std::to_string(static_cast<u32>(std::round(amount*100)))+"% · camada "+std::to_string(volume.layer);
          warning=amount<=0;break;
        }
      const auto card=deflate(slot,UiInsets::all(2));builder.list.addRect(card,warning?withAlpha(theme.color.warning,.14f):withAlpha(theme.color.accent,.12f),8);
      builder.list.addImage(centred(takeLeft(slot,26),18,18),static_cast<UiImageId>(warning?UiIcon::UiWarning:UiIcon::UiInfo),warning?theme.color.warning:theme.color.accent);
      builder.label(slot,summary.c_str(),warning?theme.color.warning:theme.color.text,theme.type.caption);
    } else if(f.kind==7) {
      const auto &collider=static_cast<const scene::Collider &>(*component);
      std::string summary;bool warning=false;
      const auto slots=builder.state.resources?visual_detail::meshColliderSlots(collider,entity,*builder.state.resources):std::vector<u32>{};
      if(collider.convex) {
        EditorMapScene::CollisionHullPreview preview;
        if(builder.state.resources&&builder.state.resources->collisionHullPreview(slots,collider.hullTolerance,preview))
          summary="Casco Jolt · "+std::to_string(preview.vertexCount)+" vértices · "+std::to_string(preview.faceCount)+" faces";
        else {warning=true;summary="Casco indisponível";if(!preview.diagnostic.empty()) summary+=" · "+std::string(preview.diagnostic);}
      } else {
        u32 triangles=0;
        if(builder.state.resources) for(const auto assetSlot:slots)
          if(const auto *asset=builder.state.resources->asset(assetSlot-1)) triangles+=asset->indexCount/3;
        summary="Malha exata · "+std::to_string(triangles)+" tri · "+
                (collider.weldVertices?"vértices soldados":"vértices separados");
      }
      const auto card=deflate(slot,UiInsets::all(2));builder.list.addRect(card,warning?withAlpha(theme.color.warning,.16f):theme.color.raised,8);
      builder.list.addImage(centred(takeLeft(slot,26),18,18),static_cast<UiImageId>(warning?UiIcon::UiWarning:UiIcon::UiInfo),
                            warning?theme.color.warning:theme.color.accent);
      builder.label(slot,summary.c_str(),warning?theme.color.warning:theme.color.text,theme.type.caption);
    } else if(f.kind==0) {
      const auto &property=entry.type->booleans[f.index];auto toggle=takeRight(slot,44);
      builder.label(slot,property.name,theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.index<<8),property.presentation.isEditable(*component));
    } else if(f.kind==1) {
      const auto &property=entry.type->enums[f.index];
      builder.label(takeLeft(slot,slot.width*.38f),property.name,theme.color.textDim,theme.type.caption);
      const char *label="Valor inválido";
      for(const auto &option:property.options) if(option.value==property.read(*component)) label=option.name;
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(slot,label,theme.color.text,theme.type.caption,UiAlign::Center);
      if(property.presentation.isEditable(*component)) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentEnumBase)+index+(f.index<<8));
    } else if(f.kind==4) {
      const auto &property=entry.type->references[f.index];const auto target=property.read(*component);
      builder.label(takeTop(slot,17),property.name,theme.color.textMuted,theme.type.caption);
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      const auto readiness=runtime::referenceReadiness(*builder.state.document,entity.id,*component,property);
      const bool warning=readiness!=runtime::ReferenceReadiness::Ready && readiness!=runtime::ReferenceReadiness::OptionalEmpty;
      std::string referenceLabel=!target?property.nullLabel:object?object->name:"Objeto ausente";
      if(readiness==runtime::ReferenceReadiness::RequiredEmpty) referenceLabel="Obrigatório · "+referenceLabel;
      else if(readiness==runtime::ReferenceReadiness::Inactive) referenceLabel+=" · inativo";
      else if(readiness==runtime::ReferenceReadiness::Incompatible && object) referenceLabel+=" · incompatível";
      builder.label(slot,referenceLabel,warning?theme.color.warning:theme.color.text,theme.type.caption);
      builder.list.addImage(centred(takeRight(slot,24),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      if(property.presentation.isEditable(*component)) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentReferenceBase)+index+(f.index<<8));
    } else if(f.kind==5) {
      const auto &triple=entry.type->triples[f.index];
      auto title=takeLeft(slot,slot.width*.30f);
      builder.router.addRegion(title,widgetId(EditorWidget::ComponentTripleBase)+index+(f.index<<8));
      if(triple.kind==scene::ComponentTripleKind::LinearColor) {
        u32 color=0xff000000u;
        for(u32 axis=0;axis<3;++axis) for(const auto &p:entry.type->numbers) if(p.id==triple.channels[axis]) {
          const float linear=std::clamp(p.read(*component),0.f,1.f);
          const float srgb=linear<=.0031308f?12.92f*linear:1.055f*std::pow(linear,1.f/2.4f)-.055f;
          color|=static_cast<u32>(srgb*255.f+.5f)<<(16-8*axis);
        }
        const auto swatch=takeLeft(title,24);
        builder.list.addRect(deflate(swatch,UiInsets{2,9,2,9}),color,4);
        builder.router.addRegion(swatch,widgetId(EditorWidget::ComponentColorBase)+index+(f.index<<8));
      }
      std::string groupTitle=triple.name;
      std::string_view unit;bool firstUnit=true,consistentUnit=true;
      for(const auto channel:triple.channels) for(const auto &p:entry.type->numbers) if(p.id==channel) {
        if(firstUnit) {unit=p.presentation.unit;firstUnit=false;}
        else if(unit!=p.presentation.unit) consistentUnit=false;
      }
      if(consistentUnit && !unit.empty()) groupTitle+=" ("+std::string(unit)+")";
      builder.label(title,groupTitle,theme.color.textDim,theme.type.caption);
      const float cellWidth=slot.width/3;
      for(u32 axis=0;axis<3;++axis) {
        u32 channel=0;
        while(channel<entry.type->numbers.size() && entry.type->numbers[channel].id!=triple.channels[axis]) ++channel;
        if(channel==entry.type->numbers.size()) continue;
        const auto &property=entry.type->numbers[channel];
        const auto cell=takeLeft(slot,cellWidth);
        auto body=deflate(cell,UiInsets::all(2));
        builder.list.addRect(body,theme.color.silhouette,theme.radius.control);
        const char *axisLabels[3]={"X","Y","Z"};const char *colorLabels[3]={"R","G","B"};
        const UiColor colors[3]={theme.color.axisX,theme.color.axisY,theme.color.axisZ};
        builder.label(takeLeft(body,13),triple.kind==scene::ComponentTripleKind::LinearColor?colorLabels[axis]:axisLabels[axis],colors[axis],theme.type.caption,UiAlign::Center);
        char value[32];std::snprintf(value,sizeof(value),"%.4g",static_cast<double>(property.read(*component)));
        builder.label(body,value,theme.color.text,theme.type.caption,UiAlign::Center);
        if(property.presentation.isEditable(*component)) builder.router.addRegion(cell,widgetId(EditorWidget::ComponentNumberBase)+index+(channel<<8));
      }
    } else if(f.kind==2) {
      const auto &property=entry.type->numbers[f.index];
      builder.label(takeLeft(slot,slot.width*.62f),property.name,theme.color.textDim,theme.type.caption);
      char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(property.read(*component)));
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.silhouette,8);
      if(!property.presentation.unit.empty()) builder.label(takeRight(slot,24),property.presentation.unit,theme.color.textMuted,theme.type.caption,UiAlign::Center);
      builder.label(slot,value,property.presentation.isEditable(*component)?theme.color.text:theme.color.textMuted,theme.type.numeric,UiAlign::Center);
      if(property.presentation.isEditable(*component)) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentNumberBase)+index+(f.index<<8));
    } else if(f.kind==9) {
      const auto &property=entry.type->slotNumbers[f.index];
      // O endereço tem nome quando a fonte dá (alvo de blend shape); senão, a posição.
      std::string name;
      if(entry.type==&scene::SkinnedMesh::descriptor && f.slot<builder.state.blendShapeNames.size())
        name=builder.state.blendShapeNames[f.slot];
      if(name.empty()) name=std::string(property.name)+" · "+std::to_string(f.slot+1);
      builder.label(takeLeft(slot,slot.width*.62f),name.c_str(),theme.color.textDim,theme.type.caption);
      char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(property.read?property.read(*component,f.slot):0.0f));
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.silhouette,8);
      if(!property.presentation.unit.empty()) builder.label(takeRight(slot,24),property.presentation.unit,theme.color.textMuted,theme.type.caption,UiAlign::Center);
      const bool editable=property.write&&property.presentation.isEditable(*component);
      builder.label(slot,value,editable?theme.color.text:theme.color.textMuted,theme.type.numeric,UiAlign::Center);
      if(editable) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentSlotNumberBase)+index+(f.index<<8)+(f.slot<<16));
    } else if(f.kind==6) {
      const auto &binding=entry.type->resourceBindings[f.index];const auto asset=binding.at(*component,f.slot);
      std::string name=binding.name;
      if(binding.slotCount(*component)>1) name+=" · "+std::to_string(f.slot+1);
      builder.label(takeTop(slot,17),name.c_str(),theme.color.textMuted,theme.type.caption);
      std::string value;
      if(!asset.valid()) {
        value=binding.kind==resources::AssetType::EnvironmentProfile?"Sem perfil":
              binding.kind==resources::AssetType::EnvironmentMap?"Ambiente padrão":
              binding.kind==resources::AssetType::AnimationClip?"Nenhum clipe":
              binding.inheritable?"Herdar malha visual":"Sem recurso";
      } else value=asset.text().substr(0,8);
      if(binding.kind==resources::AssetType::AnimationClip && asset.valid() && builder.state.resources) {
        runtime::AnimationClipView clip;
        // Referência ausente fica gravada e é dita, nunca trocada por outra.
        value=builder.state.resources->findClip(asset,clip)?clip.name:"Clipe ausente · "+asset.text().substr(0,8);
      }
      if(const auto resolved=builder.state.resources?builder.state.resources->assetSlot(asset):0) {
        const auto name=builder.state.resources->assetName(resolved-1);
        value=name.empty()?"Malha "+std::to_string(resolved):std::string(name);
      }
      if(const auto *record=builder.state.assetRegistry?builder.state.assetRegistry->find(asset):nullptr) value=record->path;
      builder.label(slot,value.c_str(),theme.color.text,theme.type.caption);
      builder.list.addImage(centred(takeRight(slot,24),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      if(binding.presentation.isEditable(*component))
        builder.router.addRegion(hit,widgetId(EditorWidget::ComponentResourceBase)+index+(f.index<<8)+(f.slot<<16));
    } else if(f.index==widgetId(EditorWidget::MeshChoose)) {
      builder.list.addImage(centred(takeRight(slot,30),22,22),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.text);
      const auto ref=meshAsset(entity);const auto text=ref?"Malha "+std::to_string(ref):"Escolher malha";
      builder.label(slot,text.c_str(),theme.color.text,theme.type.caption);
      builder.router.addRegion(hit,f.index);
    } else if(f.index==widgetId(EditorWidget::ToggleCastShadow)) {
      auto toggle=takeRight(slot,44);builder.label(slot,"Projetar sombra",theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,entity.castShadow,f.index);
    } else if(f.index==widgetId(EditorWidget::LodGroupStatus)) {
      // Leitura, não controle: sem região de toque.
      builder.label(slot,builder.state.lodStatus.c_str(),theme.color.accent,theme.type.caption);
    } else if(f.index==widgetId(EditorWidget::SkinnedMeshStatus)||f.index==widgetId(EditorWidget::AnimationStatus)) {
      builder.label(slot,(f.index==widgetId(EditorWidget::SkinnedMeshStatus)?builder.state.skinStatus:builder.state.animationStatus).c_str(),
                    theme.color.accent,theme.type.caption);
    } else {
      builder.label(slot,f.index==widgetId(EditorWidget::MaterialRestore)?"Restaurar material da origem":
                    f.index==widgetId(EditorWidget::LodGroupFit)?"Recalcular tamanho":"Ajustar à malha",theme.color.text,theme.type.caption);
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
  const scene::ComponentValue *resourceComponent=state.resourceInstance?entity.components.findInstance(state.resourceInstance):nullptr;
  const scene::ComponentResourceBinding *resourceBinding=nullptr;
  if(resourceComponent) for(const auto &binding:resourceComponent->type().resourceBindings)
    if(binding.id==state.resourceProperty) {resourceBinding=&binding;break;}
  const auto selectedResource=resourceBinding?resourceBinding->at(*resourceComponent,state.resourceSlot):resources::AssetGuid{};
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);builder.router.addRegion(back,widgetId(EditorWidget::MeshPickerClose));
  builder.label(title,resourceBinding?resourceBinding->name:"Geometria",theme.color.text,theme.type.body);
  auto search=takeTop(content,36);builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(search,state.meshQuery.empty()?
      (resourceBinding&&resourceBinding->kind==resources::AssetType::EnvironmentProfile?"Buscar perfil":
       resourceBinding&&resourceBinding->kind==resources::AssetType::EnvironmentMap?"Buscar mapa HDRI":
       resourceBinding&&resourceBinding->kind==resources::AssetType::AnimationClip?"Buscar clipe":"Buscar malha"):
      state.meshQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::MeshSearch));
  auto footer=takeBottom(content,36),previous=takeLeft(footer,36),next=takeRight(footer,36);
  if(resourceBinding&&(resourceBinding->kind==resources::AssetType::EnvironmentProfile||
                       resourceBinding->kind==resources::AssetType::EnvironmentMap)) {
    if(resourceBinding->kind==resources::AssetType::EnvironmentProfile) {
      const auto action=deflate(takeTop(content,40),UiInsets::all(2));
      builder.list.addRect(action,theme.color.accent,theme.radius.control);
      builder.label(action,selectedResource.valid()?"Atualizar perfil com estes valores":"Criar perfil com estes valores",
                    theme.color.accentInk,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(action,widgetId(selectedResource.valid()?EditorWidget::EnvironmentProfileUpdate:
                                                 EditorWidget::EnvironmentProfileCreate));
    }
    std::vector<u32> matches;const auto query=editorSearchKey(state.meshQuery);
    if(state.assetRegistry) for(u32 i=0;i<state.assetRegistry->records().size();++i) {
      const auto &record=state.assetRegistry->records()[i];
      if(record.type!=resourceBinding->kind) continue;
      if(query.empty()||editorSearchKey(record.path).find(query)!=std::string::npos) matches.push_back(i);
    }
    const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-34)/50));
    const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage);
    const u32 page=std::min(state.meshPage,pages-1);
    auto clear=takeTop(content,34);builder.label(clear,
        resourceBinding->kind==resources::AssetType::EnvironmentProfile?"Sem perfil · conservar cópia local":"Sem mapa HDRI",
        theme.color.textDim,theme.type.caption);
    builder.router.addRegion(clear,widgetId(EditorWidget::MeshClear));
    for(u32 row=page*perPage;row<matches.size()&&row<(page+1)*perPage;++row) {
      const auto index=matches[row];const auto &record=state.assetRegistry->records()[index];
      auto slot=takeTop(content,50);const auto hit=slot;
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      if(selectedResource==record.guid) builder.list.addRect({slot.x,slot.y+4,3,slot.height-8},theme.color.accent,1);
      builder.list.addImage(centred(takeLeft(slot,40),28,28),static_cast<UiImageId>(UiIcon::LightingSun),0xffffffff);
      const auto slash=record.path.find_last_of('/');const auto name=record.path.substr(slash==std::string::npos?0:slash+1);
      builder.label(takeTop(slot,25),name.c_str(),theme.color.text,theme.type.body);
      builder.label(slot,("GUID "+record.guid.text().substr(0,8)+" · compartilhado").c_str(),theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(hit,widgetId(EditorWidget::MeshChoiceBase)+index);
    }
    if(matches.empty()) builder.label(content,resourceBinding->kind==resources::AssetType::EnvironmentProfile?
        "Nenhum perfil no projeto":"Nenhum mapa HDRI no projeto",theme.color.textMuted,theme.type.caption);
    builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::MeshPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::MeshNext));
    const auto text=std::to_string(page+1)+" / "+std::to_string(pages);
    builder.label(footer,text.c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }
  if(resourceBinding&&resourceBinding->kind==resources::AssetType::AnimationClip) {
    // Clipes de todas as fontes carregadas, com a duração e o arquivo de origem.
    std::vector<u32> matches;const auto query=editorSearchKey(state.meshQuery);
    const auto catalog=state.resources?state.resources->clipCatalog():std::vector<EditorMapScene::ClipEntry>{};
    for(u32 i=0;i<catalog.size();++i)
      if(query.empty()||editorSearchKey(catalog[i].name).find(query)!=std::string::npos) matches.push_back(i);
    const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-34)/50));
    const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage);
    const u32 page=std::min(state.meshPage,pages-1);
    auto clear=takeTop(content,34);
    builder.label(clear,"Nenhum clipe",theme.color.textDim,theme.type.caption);
    builder.router.addRegion(clear,widgetId(EditorWidget::MeshClear));
    for(u32 row=page*perPage;row<matches.size()&&row<(page+1)*perPage;++row) {
      const auto index=matches[row];const auto &entry=catalog[index];
      auto slot=takeTop(content,50);const auto hit=slot;
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      if(selectedResource==entry.clip) builder.list.addRect({slot.x,slot.y+4,3,slot.height-8},theme.color.accent,1);
      builder.list.addImage(centred(takeLeft(slot,40),28,28),static_cast<UiImageId>(UiIcon::AssetsAnimation),0xffffffff);
      builder.label(takeTop(slot,25),entry.name.c_str(),theme.color.text,theme.type.body);
      std::string origin;
      if(state.assetRegistry) if(const auto *record=state.assetRegistry->find(entry.source)) {
        const auto slash=record->path.find_last_of('/');origin=record->path.substr(slash==std::string::npos?0:slash+1);
      }
      char duration[24];std::snprintf(duration,sizeof duration,"%.2f s",entry.duration);
      builder.label(slot,(std::string(duration)+(origin.empty()?"":" · "+origin)).c_str(),theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(hit,widgetId(EditorWidget::MeshChoiceBase)+index);
    }
    if(matches.empty()) builder.label(content,"Nenhum clipe nas fontes carregadas",theme.color.textMuted,theme.type.caption);
    builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::MeshPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::MeshNext));
    const auto text=std::to_string(page+1)+" / "+std::to_string(pages);
    builder.label(footer,text.c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }
  if(resourceBinding&&resourceBinding->id=="collision_mesh") {
    auto settings=takeTop(content,38);
    const auto quality=takeLeft(settings,settings.width*.5f),error=settings;
    const auto stepped=[&](UiRect row,const char *label,const std::string &value,u32 down,u32 up) {
      builder.label(takeLeft(row,row.width*.38f),label,theme.color.textMuted,theme.type.caption);
      const auto less=takeLeft(row,28),more=takeRight(row,28);
      builder.label(less,"−",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(more,"+",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(row,value.c_str(),theme.color.text,theme.type.numeric,UiAlign::Center);
      builder.router.addRegion(less,down);builder.router.addRegion(more,up);
    };
    stepped(quality,"Triângulos",std::to_string(state.collisionTrianglePercent)+"%",
            widgetId(EditorWidget::MeshCollisionQualityDown),widgetId(EditorWidget::MeshCollisionQualityUp));
    char errorText[24];std::snprintf(errorText,sizeof errorText,"%.1f%%",state.collisionMaximumError*100.f);
    stepped(error,"Erro",errorText,widgetId(EditorWidget::MeshCollisionErrorDown),widgetId(EditorWidget::MeshCollisionErrorUp));
    const auto generate=deflate(takeTop(content,40),UiInsets::all(2));
    builder.list.addRect(generate,theme.color.accent,theme.radius.control);
    builder.label(generate,selectedResource.valid()?"Regenerar malha física":"Gerar da malha visual",
                  theme.color.accentInk,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(generate,widgetId(EditorWidget::MeshGenerateCollision));
    const auto *render=meshRenderer(entity);
    auto visual=render?render->slotAsset(0):resources::AssetGuid{};
    if(!visual.valid()&&render&&render->slotMesh(0)&&state.resources)
      visual=state.resources->assetGuid(render->slotMesh(0)-1);
    const auto visualSlot=state.resources?state.resources->assetSlot(visual):0;
    const auto physicalSlot=state.resources?state.resources->assetSlot(selectedResource):0;
    if(visualSlot) {
      const auto visualTriangles=state.resources->asset(visualSlot-1)->indexCount/3;
      std::string summary="Visual "+std::to_string(visualTriangles)+" tri";
      if(physicalSlot) {
        const auto *physical=state.resources->asset(physicalSlot-1);
        char measured[32];std::snprintf(measured,sizeof measured,"%.3g%%",physical->geometricError*100.f);
        summary+=" · física "+std::to_string(physical->indexCount/3)+" tri · erro geom. "+measured;
      }
      builder.label(takeTop(content,22),summary.c_str(),theme.color.textMuted,theme.type.caption);
    }
  }
  std::vector<u32> matches;const auto query=editorSearchKey(state.meshQuery);
  if(state.resources) for(u32 i=0;i<state.resources->assetCount();++i) {
    if(state.resources->materialFlagsForAsset(i)&renderer::MapMaterialWater) continue;
    const auto *asset=state.resources->asset(i);
    const auto resourceName=state.resources->assetName(i);
    const auto text=(resourceName.empty()?"Malha "+std::to_string(i+1):std::string(resourceName))+" material "+std::to_string(asset->materialIndex);
    if(query.empty() || editorSearchKey(text).find(query)!=std::string::npos) matches.push_back(i);
  }
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-34)/50));
  const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage),page=std::min(state.meshPage,pages-1);
  auto clear=takeTop(content,34);
  builder.label(clear,resourceBinding&&resourceBinding->inheritable?"Herdar malha visual":"Sem malha",theme.color.textDim,theme.type.caption);
  builder.router.addRegion(clear,widgetId(EditorWidget::MeshClear));
  for(u32 row=page*perPage;row<matches.size() && row<(page+1)*perPage;++row) {
    if(content.height<40) break;
    const auto i=matches[row];const auto *asset=state.resources->asset(i);auto slot=takeTop(content,50);const auto hit=slot;
    const bool selected=resourceBinding?selectedResource==state.resources->assetGuid(i):meshAsset(entity)==i+1;
    builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    if(selected) builder.list.addRect({slot.x,slot.y+4,3,slot.height-8},theme.color.accent,1);
    builder.list.addImage(centred(takeLeft(slot,40),28,28),static_cast<UiImageId>(UiIcon::EditorAuthorObject),0xffffffff);
    const auto authoredName=state.resources->assetName(i);
    const auto name=authoredName.empty()?"Malha "+std::to_string(i+1):std::string(authoredName);
    builder.label(takeTop(slot,25),name.c_str(),theme.color.text,theme.type.body);
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

void buildObjectFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity);
void buildComponents(ScreenBuilder &builder, UiRect content, const EditorEntity &entity) {
  if(builder.state.presetPanel && builder.state.presetEntity==entity.id) {
    const auto &state=builder.state;const auto &theme=builder.theme;
    const auto button=[&](UiRect rect,const char *label,EditorWidget id) {
      const auto inset=deflate(rect,UiInsets::all(3));builder.list.addRect(inset,theme.color.raised,theme.radius.control);
      builder.label(inset,label,theme.color.text,theme.type.caption,UiAlign::Center);builder.router.addRegion(rect,widgetId(id));
    };
    button(takeTop(content,36),"<  Presets do projeto",EditorWidget::PresetClose);
    auto actions=takeTop(content,40);const auto save=takeLeft(actions,actions.width*.5f);
    if(state.presetInstance) button(save,"Salvar atual",EditorWidget::PresetSave);
    if(state.presetSelected) button(actions,"Renomear",EditorWidget::PresetRename);
    // Salvar o OBJETO inteiro como receita fica junto do resto: montar "porta
    // interativa" uma vez e reusar é a razão de a receita existir.
    button(takeTop(content,38),"Salvar objeto como receita",EditorWidget::PresetSaveRecipe);
    builder.label(takeTop(content,26),"Referências de cena são preservadas ao aplicar",theme.color.textMuted,theme.type.caption);
    if(state.presetSelected) {
      auto apply=takeBottom(content,40);const auto values=takeLeft(apply,apply.width*.5f);
      if(state.presetSelectedIsRecipe) button(apply,"Aplicar receita",EditorWidget::PresetApply);
      else {
        if(state.presetInstance) button(values,"Aplicar valores",EditorWidget::PresetApply);
        button(apply,"Adicionar",EditorWidget::PresetAdd);
      }
      button(takeBottom(content,34),state.presetDeleteConfirm?"Confirmar exclusão":"Excluir preset",EditorWidget::PresetDelete);
      // O diff é a parte interessante do painel, então ele fica logo acima das
      // ações e é a primeira coisa a ganhar espaço quando o inspetor cresce.
      // Cada linha é uma escolha: o preset deixa de ser tudo-ou-nada.
      auto diff=takeBottom(content,std::min(std::max(120.f,content.height*.55f),content.height));
      builder.list.pushClip(diff);
      for(const auto &line:state.presetPreview) builder.label(takeTop(diff,22),line,theme.color.textMuted,theme.type.caption);
      if(!state.presetFields.empty()) {
        auto marks=takeTop(diff,32);
        button(takeLeft(marks,marks.width*.5f),"Marcar tudo",EditorWidget::PresetSelectAll);
        button(marks,"Desmarcar",EditorWidget::PresetSelectNone);
        auto pager=takeBottom(diff,26);
        const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,diff.height)/42));
        const u32 pages=std::max(1u,(static_cast<u32>(state.presetFields.size())+perPage-1)/perPage);
        const u32 page=std::min(state.presetFieldPage,pages-1);
        for(u32 i=page*perPage;i<state.presetFields.size()&&i<(page+1)*perPage&&diff.height>=42;++i) {
          const auto &field=state.presetFields[i];
          auto row=takeTop(diff,42);const auto hit=row;
          builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
          auto mark=takeRight(row,40);
          // Um campo não aplicável mostra o motivo no lugar da marca, em vez de
          // oferecer um interruptor que não faria nada.
          if(field.applicable) builder.toggle(deflate(mark,UiInsets::all(6)),field.selected,widgetId(EditorWidget::PresetFieldBase)+i);
          else builder.label(mark,"—",theme.color.textMuted,theme.type.caption,UiAlign::Center);
          auto text=deflate(row,UiInsets::symmetric(10,3));
          builder.label(takeTop(text,20),field.label.c_str(),
              field.applicable?theme.color.text:theme.color.textMuted,theme.type.caption);
          builder.label(text,(field.current+"  →  "+field.candidate).c_str(),theme.color.textMuted,theme.type.caption);
          if(field.applicable) builder.router.addRegion(hit,widgetId(EditorWidget::PresetFieldBase)+i);
        }
        if(pages>1) {
          button(takeLeft(pager,34),"<",EditorWidget::PresetFieldsPrevious);
          button(takeRight(pager,34),">",EditorWidget::PresetFieldsNext);
          builder.label(pager,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
        }
      }
      builder.list.popClip();
    }
    auto pager=takeBottom(content,32);button(takeLeft(pager,36),"<",EditorWidget::PresetPrevious);button(takeRight(pager,36),">",EditorWidget::PresetNext);
    const auto count=state.presetChoices.size();const u32 pages=std::max(1u,(static_cast<u32>(count)+3)/4),page=std::min(state.presetPage,pages-1);
    builder.label(pager,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.list.pushClip(content);
    if(!count) builder.label(content,"Nenhum preset deste tipo",theme.color.textMuted,theme.type.caption);
    for(u32 i=page*4;i<std::min<u32>(static_cast<u32>(count),page*4+4)&&content.height>=36;++i) {
      const auto row=takeTop(content,40);const auto &entry=state.presetChoices[i];
      builder.list.addRect(deflate(row,UiInsets::all(2)),entry.first==state.presetSelected?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(deflate(row,UiInsets::symmetric(8,2)),entry.second,entry.first==state.presetSelected?theme.color.accentInk:theme.color.text,theme.type.caption);
      builder.router.addRegion(row,widgetId(EditorWidget::PresetChoiceBase)+i);
    }
    builder.list.popClip();return;
  }
  if(builder.state.impactInstance && builder.state.impactEntity==entity.id) {
    const auto &state=builder.state;const auto &theme=builder.theme;
    const auto close=takeTop(content,36);builder.label(close,state.impactRepair?"<  Cancelar reparo":state.impactAsset.valid()?"<  Recurso":"<  Dependências",theme.color.text,theme.type.body);
    builder.router.addRegion(close,widgetId(EditorWidget::ImpactClose));
    const auto *source=entity.components.findInstance(state.impactInstance);
    const auto *sourceSchema=source?scene::findComponentSchema(source->type()):nullptr;
    builder.label(takeTop(content,28),sourceSchema?sourceSchema->name:"Componente indisponível",theme.color.textMuted,theme.type.caption);
    auto entries=state.impactAsset.valid()?resourceImpact(*state.document,state.impactAsset,state.assetRegistry,state.resources):
        componentImpact(*state.document,entity.id,state.impactInstance,state.assetRegistry,state.resources);
    const auto uses=localResourceUses(source,state.impactAsset);
    const auto sharedContext=repairMaterialContext(state.impactTrail,state.impactAsset,state.resources);
    if(state.impactRepair) {
      const auto shared=sharedTextureBindings(state.impactRepairMaterial,state.resources);
      entries=resourceRepairChoices(state.impactRepairMaterial.valid()?&shared:source,state.impactAsset,state.assetRegistry,state.resources);
      if(state.impactReplacement.valid()) {
        entries.clear();
        const auto *replacement=state.assetRegistry?state.assetRegistry->find(state.impactReplacement):nullptr;
        entries.push_back({0,0,"Novo recurso",replacement?replacement->path:state.impactReplacement.text()});
        if(state.impactRepairMaterial.valid()) {
          const auto affected=sharedTextureImpact(*state.document,state.impactRepairMaterial,state.impactAsset,state.resources);
          entries.push_back({0,0,"Alcance do material","Inclui usos em outras cenas; lista abaixo: cena aberta"});
          entries.insert(entries.end(),affected.begin(),affected.end());
        } else if(state.impactRepairScene) {
          const auto affected=sceneResourceRepairImpact(*state.document,state.impactAsset);entries.insert(entries.end(),affected.begin(),affected.end());
        } else for(const auto &use:uses) entries.push_back({0,0,"Será substituído · slot "+std::to_string(use.slot+1),use.binding});
      }
    }
    const bool blocked=scene::componentInstanceRemovalBlockedBy(state.impactInstance,entity.components) ||
        runtime::componentRemovalReferenceUse(*state.document,entity.id,state.impactInstance).object;
    builder.label(takeTop(content,28),state.impactRepair?(state.impactRepairMaterial.valid()?"Material compartilhado · com Undo":state.impactRepairScene?"Usos locais da cena · com Undo":"Somente usos locais deste componente"):state.impactAsset.valid()?"Registro e usos na cena":source?(blocked?"Remoção bloqueada pelas dependências":"Sem bloqueio de dependências para remover"):"Componente removido",blocked&&!state.impactAsset.valid()?theme.color.warning:theme.color.textMuted,theme.type.caption);
    if((!state.impactRepair&&!uses.empty())||(state.impactRepair&&state.impactReplacement.valid())) {
      const auto action=takeTop(content,40);
      builder.list.addRect(deflate(action,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(action,state.impactRepair?"Aplicar substituição":"Reparar usos locais",theme.color.accent,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(action,widgetId(state.impactRepair?EditorWidget::ImpactRepairApply:EditorWidget::ImpactRepair));
    }
    if(!state.impactRepair&&sharedContext.valid()) {
      const auto action=takeTop(content,40);
      builder.list.addRect(deflate(action,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(action,"Reparar no material",theme.color.accent,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(action,widgetId(EditorWidget::ImpactRepairShared));
    }
    if(state.impactRepair&&!state.impactRepairMaterial.valid()) {
      const auto scope=takeTop(content,36);
      builder.label(scope,state.impactRepairScene?"Alcance: cena aberta  ›":"Alcance: componente  ›",theme.color.accent,theme.type.caption);
      builder.router.addRegion(scope,widgetId(EditorWidget::ImpactRepairScope));
    }
    auto footer=takeBottom(content,32);
    const u32 count=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/58));
    const u32 pages=std::max(1u,(static_cast<u32>(entries.size())+count-1)/count),page=std::min(state.impactPage,pages-1);
    for(u32 i=page*count;i<entries.size()&&i<(page+1)*count;++i) {
      const auto &entry=entries[i];auto row=takeTop(content,58);const auto hit=row;
      builder.list.addRect(deflate(row,UiInsets{0,2,0,2}),theme.color.raised,theme.radius.control);
      row=deflate(row,UiInsets{8,2,8,2});
      const auto *target=state.document->find(entry.object);
      builder.list.pushClip(hit);
      builder.label(takeTop(row,28),entry.blocksRemoval?entry.relation+" · bloqueia remoção":entry.relation,
          entry.blocksRemoval||entry.invalid?theme.color.warning:theme.color.accent,theme.type.caption);
      builder.label(row,(target?std::string(target->name)+" · ":"")+entry.detail,theme.color.text,theme.type.caption);
      builder.list.popClip();
      if(target||entry.asset.valid()) builder.router.addRegion(hit,widgetId(EditorWidget::ImpactRowBase)+i);
    }
    if(entries.empty()) builder.label(content,state.impactRepair?"Nenhum recurso compatível disponível":"Sem relações declaradas",theme.color.textMuted,theme.type.caption);
    const auto previous=takeLeft(footer,40),next=takeRight(footer,40);
    builder.label(previous,"<",theme.color.text,theme.type.body);builder.label(next,">",theme.color.text,theme.type.body);
    if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ImpactPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ImpactNext));
    builder.label(footer,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }

  const auto &theme=builder.theme;const auto &state=builder.state;
  const bool same=state.componentSelection==entity.id,adding=same&&state.addingComponent;
  if(same && state.referenceInstance) {buildReferencePicker(builder,content,entity);return;}
  const bool resourcePicker=state.resourceInstance&&entity.components.findInstance(state.resourceInstance);
  if(same && state.meshPicker && (meshRenderer(entity)||resourcePicker)) {buildMeshPicker(builder,content,entity);return;}
  if(same && state.materialPicker && meshRenderer(entity)) {buildMaterialPicker(builder,content);return;}
  if(same && state.textureViewer && meshRenderer(entity)) {buildTextureViewer(builder,content);return;}
  if(same && state.texturePicker && meshRenderer(entity)) {buildTexturePicker(builder,content);return;}
  auto footer=takeBottom(content,42);auto button=deflate(footer,UiInsets::all(2));
  builder.list.addRect(button,theme.color.raised,theme.radius.control);
  builder.router.addRegion(button,widgetId(EditorWidget::AddComponentMenu));
  builder.list.addImage(centred(takeLeft(button,40),28,28),static_cast<UiImageId>(UiIcon::ComponentAdd),0xffffffff);
  builder.label(button,adding?"Add · fechar":"Add",theme.color.text,theme.type.body);
  struct Card {const EditorComponentEntry *native=nullptr;const scene::ComponentValue *value=nullptr;u32 index=0;bool transform=false;bool object=false;};
  std::vector<Card> cards;
  if(adding) {
    const auto presets=takeTop(content,34);builder.label(presets,"Presets do projeto  ›",theme.color.accent,theme.type.caption);
    builder.router.addRegion(presets,widgetId(EditorWidget::PresetOpen));
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
    // Transformação continua o primeiro card (é o componente de todo objeto);
    // as configurações universais do objeto vêm logo depois, antes dos componentes.
    cards.push_back({nullptr,nullptr,0,true});
    cards.push_back({nullptr,nullptr,0,false,true});
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
      if(item.object) return state.expandedComponent=="astra.object";
      if(item.transform) return state.expandedComponent=="astra.transform" || state.transformMenu;
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
    const bool open=same&&(item.object?state.expandedComponent=="astra.object":item.transform?state.expandedComponent=="astra.transform":item.native?(item.value&&state.expandedNative==item.value->instanceId()):script&&state.expandedScript==script->instanceId());
    const bool menu=same&&!item.object&&(item.transform?state.transformMenu:item.native?(item.value&&state.nativeMenu==item.value->instanceId()):script&&state.scriptMenu==script->instanceId());
    const EditorScriptType *newType=adding&&!item.native?&state.code->scriptTypes()[item.index]:nullptr;
    std::string title=item.object?"Objeto":item.transform?"Transformação":item.native?item.native->name:newType?newType->name:script?(schema?schema->name:script->scriptType):std::string(item.value->type().id);
    if(item.value && item.native && item.native->type->allowMultiple) title+=" · "+std::to_string(item.value->instanceId());
    const auto icon=item.object?UiIcon::EditorAuthorObject:item.transform?UiIcon::EditorAuthorMove:item.native?item.native->icon:UiIcon::ScriptingCode;
    const char *reason=adding&&item.native?(!item.native->type->allowMultiple&&entity.components.find(*item.native->type)?"Já adicionado":item.native->unavailable(entity)):nullptr;
    auto row=takeTop(content,adding?54.0f:44.0f);const auto hit=row;
    builder.list.addRect(row,theme.color.raised,theme.radius.control);
    if(open) builder.list.addRect({row.x,row.y+4,3,row.height-8},theme.color.accent,1);
    if(!adding) builder.label(takeLeft(row,20),open?"v":">",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.list.addImage(centred(takeLeft(row,36),28,28),static_cast<UiImageId>(icon),0xffffffff);
    auto more=takeRight(row,adding||item.object?0:32);
    builder.label(adding?takeTop(row,27):row,title.c_str(),reason?theme.color.textMuted:theme.color.text,theme.type.body);
    if(adding) {
      std::string detail=reason?reason:item.native?item.native->description:"Comportamento C#";
      if(!reason && item.native) {
        const auto plan=scene::planComponentAddition(entity.components,item.native->type->id,false,false);
        if(plan.ready && plan.addedTypes.size()>1) {
          detail="Inclui ";
          for(usize i=0;i+1<plan.addedTypes.size();++i) {
            if(i) detail+=" · ";
            detail+=scene::findComponentSchema(plan.addedTypes[i])->name;
          }
        }
      }
      builder.label(row,detail,theme.color.textMuted,theme.type.caption);
    }
    if(adding) {
      if(!reason) builder.router.addRegion(hit,widgetId(item.native?EditorWidget::ComponentAddBase:EditorWidget::ScriptAddBase)+item.index);
    } else if(item.object||item.transform||item.native||script) {
      builder.router.addRegion({hit.x,hit.y,hit.width-(item.object?0:32),hit.height},item.object?widgetId(EditorWidget::ObjectFold):item.transform?widgetId(EditorWidget::TransformFold):widgetId(item.native?EditorWidget::ComponentFoldBase:EditorWidget::ScriptFoldBase)+item.index);
      if(!item.object) {
        builder.list.addImage(centred(more,20,20),static_cast<UiImageId>(UiIcon::EditorAuthorMore),theme.color.textDim);
        builder.router.addRegion(more,item.transform?widgetId(EditorWidget::TransformMenu):widgetId(item.native?EditorWidget::ComponentMenuBase:EditorWidget::ScriptMenuBase)+item.index);
      }
      if(menu && item.transform) {
        // Unity: copiar, colar e redefinir moram no próprio card da transformação.
        const struct {const char *label;EditorWidget widget;bool enabled;} rows[]{
            {"Copiar transformação",EditorWidget::TransformCopy,true},{"Colar transformação",EditorWidget::TransformPaste,state.hasTransformClipboard},
            {"Redefinir posição",EditorWidget::TransformResetPosition,true},{"Redefinir rotação",EditorWidget::TransformResetRotation,true},
            {"Redefinir escala",EditorWidget::TransformResetScale,true},{"Redefinir tudo",EditorWidget::TransformReset,true}};
        for(const auto &action:rows) {
          if(content.height<34) break;
          auto line=takeTop(content,34);
          builder.label(line,action.label,action.enabled?theme.color.text:theme.color.textMuted,theme.type.caption);
          if(action.enabled) builder.router.addRegion(line,widgetId(action.widget));
        }
      } else if(menu) {
        if(item.native) {
          const auto impact=takeTop(content,34);builder.label(impact,"Dependências",theme.color.text,theme.type.caption);
          builder.router.addRegion(impact,widgetId(EditorWidget::ImpactOpenBase)+item.index);
          const auto presets=takeTop(content,34);builder.label(presets,"Presets",theme.color.text,theme.type.caption);
          builder.router.addRegion(presets,widgetId(EditorWidget::PresetOpen));
          const EditorWidget actions[]{EditorWidget::ComponentCopyBase,EditorWidget::ComponentPasteBase,EditorWidget::ComponentResetBase};
          const char *labels[]{"Copiar valores","Colar valores","Restaurar padrão"};
          for(u32 i=0;i<3;++i) {auto action=takeTop(content,34);builder.label(action,labels[i],theme.color.text,theme.type.caption);
            if(i!=1||(state.componentClipboard && &state.componentClipboard->type()==item.native->type)) builder.router.addRegion(action,widgetId(actions[i])+item.index);}
        }
        const auto *dependent=item.native?scene::componentInstanceRemovalBlockedBy(item.value->instanceId(),entity.components):nullptr;
        const auto use=item.native?runtime::componentRemovalReferenceUse(*state.document,entity.id,item.value->instanceId()):runtime::ComponentReferenceUse{};
        auto remove=takeTop(content,34);
        builder.label(remove,"Remover componente",dependent||use.object?theme.color.textMuted:theme.color.text,theme.type.caption);
        if(!dependent && !use.object) builder.router.addRegion(remove,widgetId(item.native?EditorWidget::ComponentRemoveBase:EditorWidget::ScriptRemoveBase)+item.index);
        else {
          const auto reason=dependent?std::string("Necessário para ")+dependent->name:std::string("Referenciado por ")+state.document->find(use.object)->name;
          builder.label(takeTop(content,28),reason,theme.color.warning,theme.type.caption);
        }
      }
      if(open) {
        auto fields=takeTop(content,std::max(0.0f,content.height-(end-card-1)*48.0f-4));
        builder.list.pushClip(fields);
        if(item.object) buildObjectFields(builder,fields,entity);
        else if(item.transform) {
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
          {ImportOverrideParent,"Reverter pai à fonte"},{ImportOverrideMesh,"Reverter malha à fonte"},
          {ImportOverrideMaterial,"Reverter material à fonte"}};
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

// Configurações universais do objeto: valem para qualquer objeto, com ou sem
// componentes. Só entra o que tem consumidor real — visibilidade (extração de
// desenhos), sombra projetada (renderer) e camada (física). "Estático" e
// "receber sombra" existem no documento mas ninguém os lê ainda; mostrá-los
// seria prometer comportamento que não existe.
void buildObjectFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  // 0 nome, 1 visível, 2 sombra projetada (só com malha: sem desenho não há
  // sombra), 3 camada, 4 informação.
  std::vector<u32> kinds{0,1};
  if(meshRenderer(entity)) kinds.push_back(2);
  kinds.push_back(3);kinds.push_back(4);
  const u32 rows=static_cast<u32>(kinds.size());
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/36));
  const u32 pages=(rows+perPage-1)/perPage,page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 index=page*perPage;index<rows && index<(page+1)*perPage;++index) {
    if(content.height<30) break;
    const u32 row=kinds[index];
    auto line=takeTop(content,36);
    if(row==0) {
      builder.label(takeLeft(line,line.width*.34f),"Nome",theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(line,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(line,entity.name,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(line,widgetId(EditorWidget::RenameSelection));
    } else if(row==1 || row==2) {
      auto toggle=takeRight(line,44);
      builder.label(line,row==1?"Visível":"Projetar sombra",theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,row==1?entity.visible:entity.castShadow,widgetId(row==1?EditorWidget::ToggleVisible:EditorWidget::ToggleCastShadow));
    } else if(row==3) {
      builder.label(takeLeft(line,line.width*.34f),"Camada",theme.color.textDim,theme.type.caption);
      auto previous=takeLeft(line,32),next=takeRight(line,32);
      builder.label(previous,"<",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(next,">",theme.color.text,theme.type.body,UiAlign::Center);
      builder.router.addRegion(previous,widgetId(EditorWidget::ObjectLayerPrevious));
      builder.router.addRegion(next,widgetId(EditorWidget::ObjectLayerNext));
      const auto layer=entity.layer%runtime::GameplayLayers::kCount;
      const auto name=state.document->layers().name(layer);
      const std::string text=name.empty()?"Camada "+std::to_string(layer):std::string(name);
      builder.label(line,text.c_str(),theme.color.text,theme.type.caption,UiAlign::Center);
    } else {
      const auto children=state.document->childrenOf(entity.id).size();
      const std::string info="ID "+std::to_string(entity.id)+" · "+std::to_string(children)+(children==1?" filho":" filhos");
      builder.label(line,info.c_str(),theme.color.textMuted,theme.type.caption);
    }
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

// Ações do objeto no ⋮ do cabeçalho do inspetor: o que se faz COM o objeto
// selecionado, no painel dele. O menu da hierarquia continua existindo.
void buildObjectActions(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  auto title=takeTop(content,36),close=takeRight(title,36);
  builder.label(title,"Ações do objeto",theme.color.text,theme.type.body);
  builder.label(close,"x",theme.color.textDim,theme.type.body,UiAlign::Center);
  builder.router.addRegion(close,widgetId(EditorWidget::InspectorMenu));
  const bool root=entity.id==state.document->root();
  const struct {const char *label;EditorWidget widget;bool enabled;} actions[]{
      {"Renomear",EditorWidget::RenameSelection,true},
      {"Duplicar",EditorWidget::DuplicateSelection,!root},
      {"Excluir",EditorWidget::DeleteSelection,!root},
      {"Enquadrar na vista",EditorWidget::FrameSelection,true},
      {"Criar filho vazio",EditorWidget::CreateChildGroup,true},
      {"Mover acima",EditorWidget::MoveEarlier,!root},
      {"Mover abaixo",EditorWidget::MoveLater,!root},
      {"Mudar pai",EditorWidget::ReparentSelection,!root},
      {"Mover para a raiz",EditorWidget::MoveToRoot,!root && entity.parent!=state.document->root()},
      {"Copiar transformação",EditorWidget::TransformCopy,!root},
      {"Colar transformação",EditorWidget::TransformPaste,!root && state.hasTransformClipboard},
      {"Redefinir transformação",EditorWidget::TransformReset,!root}};
  const u32 count=static_cast<u32>(std::size(actions));
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/38));
  const u32 pages=(count+perPage-1)/perPage,page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 i=page*perPage;i<count && i<(page+1)*perPage;++i) {
    if(content.height<34) break;
    auto row=takeTop(content,38);
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    takeLeft(row,12);
    builder.label(row,actions[i].label,actions[i].enabled?theme.color.text:theme.color.textMuted,theme.type.caption);
    if(actions[i].enabled) builder.router.addRegion(row,widgetId(actions[i].widget));
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

namespace {

// Quebra por largura medida, respeitando parágrafos e sequências UTF-8.
std::vector<std::string> wrapText(const UiDrawList &list,std::string_view text,float width,const UiTypeStyle &style) {
  std::vector<std::string> wrapped;
  usize start=0;
  while(start<=text.size()) {
    const auto end=std::min(text.find('\n',start),text.size());
    std::string line(text.substr(start,end-start));
    while(measureTextWidth(line,list.fontMetrics(),style)>width && line.size()>1) {
      usize fit=0;
      for(usize i=1;i<=line.size();++i) {
        if(i<line.size() && (static_cast<unsigned char>(line[i])&0xc0)==0x80) continue;
        if(fit && measureTextWidth(std::string_view(line).substr(0,i),list.fontMetrics(),style)>width) break;
        fit=i;
      }
      usize split=line.rfind(' ',fit);
      if(split==std::string::npos || split==0) split=fit;
      wrapped.push_back(line.substr(0,split));line.erase(0,split);
      if(!line.empty() && line.front()==' ') line.erase(0,1);
    }
    wrapped.push_back(std::move(line));
    if(end==text.size()) break;
    start=end+1;
  }
  return wrapped;
}

// R3: importador em Propriedades (W01). Substitui a janela modal que cobria o
// editor: a prévia agora é um contexto do painel, com abas de saídas reais e o
// perfil, e o resto do editor continua disponível enquanto ela está aberta.
void buildImportDock(ScreenBuilder &builder,UiRect content) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
  using Tab=EditorScreenState::ImportTab;

  auto header=takeTop(content,kPanelHeaderHeight);
  list.addImage(centred(takeLeft(header,26.0f),18.0f,18.0f),static_cast<UiImageId>(UiIcon::AssetsImport),theme.color.text);
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},state.importTexture?"Importar textura":"Importar recurso",theme.color.text,theme.type.cardName);
  builder.label({header.x,header.y+half,header.width,half},state.importPath.empty()?"Escolhendo arquivo…":state.importPath,
                theme.color.textDim,theme.type.caption); // caption: o caminho mantém a caixa do nome do arquivo
  builder.label(takeTop(content,24),state.importStatus,theme.color.accent,theme.type.caption);

  if(state.importTexture) {
    auto actions=takeBottom(content,40);
    const bool recipeReady=resources::sameTexturePreparation(state.textureImportSettings,state.textureImportPreparedSettings);
    const auto cancel=deflate(takeLeft(actions,state.importReady?actions.width*.36f:actions.width),UiInsets::all(2));
    list.addRect(cancel,theme.color.raised,theme.radius.control);
    builder.label(cancel,state.importError?"Fechar":"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(cancel,widgetId(EditorWidget::ImportCancel));
    if(state.importReady) {
      const auto accept=deflate(actions,UiInsets::all(2));
      list.addRect(accept,recipeReady?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(accept,"Importar textura",recipeReady?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(recipeReady) router.addRegion(accept,widgetId(EditorWidget::ImportAccept));
    }
    auto preview=takeTop(content,std::clamp(content.height*.28f,64.0f,150.0f));
    list.addRect(preview,theme.color.raised,theme.radius.control);
    if(!state.textureImportImage.isEmpty()) {
      const auto &texels=state.textureImportImage;
      const float fit=std::min(preview.width/texels.width,preview.height/texels.height);
      const float w=texels.width*fit,h=texels.height*fit;
      list.addPreviewImage({preview.x+(preview.width-w)*.5f,preview.y+(preview.height-h)*.5f,w,h},texels);
    } else builder.label(preview,"Prévia indisponível",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    takeTop(content,5);
    const auto &profile=state.textureImportSettings;
    static constexpr const char *types[]{"Pelo uso","Cor (sRGB)","Dados (linear)","Mapa normal"};
    const std::array<std::pair<std::string,EditorWidget>,10> settings{{
      {std::string("Interpretação · ")+types[std::min<u32>(profile.interpretation,3u)],EditorWidget::TextureProfileInterpretation},
      {profile.maximumDimension?"Tamanho máx. · "+std::to_string(profile.maximumDimension)+" px":std::string("Tamanho máx. · projeto"),EditorWidget::TextureProfileDimension},
      {profile.mipmaps?"Mipmaps · gerar":"Mipmaps · desligados",EditorWidget::TextureProfileMipmaps},
      {profile.dilateEdges?"Bordas alfa · dilatar":"Bordas alfa · preservar",EditorWidget::TextureProfileEdges},
      {profile.anisotropy?"Anisotropia · permitir":"Anisotropia · desligar",EditorWidget::TextureProfileAnisotropy},
      {profile.invertNormalGreen?"Normal Y · inverter (DX)":"Normal Y · manter (GL)",EditorWidget::TextureProfileNormalGreen},
      {profile.preserveAlphaCoverage?"Cobertura alfa · preservar":"Cobertura alfa · desligada",EditorWidget::TextureProfileCoverage},
      {"Corte cobertura · "+std::to_string(static_cast<u32>(std::lround(profile.alphaCoverageCutoff*100.0f)))+"%",EditorWidget::TextureProfileCoverageCutoff},
      {profile.streamingMipmaps?"Streaming de mips · sim":"Streaming de mips · não",EditorWidget::TextureProfileStreaming},
      {"Prioridade de streaming · "+std::to_string(profile.streamingPriority),EditorWidget::TextureProfileStreamingPriority}}};
    auto pager=takeBottom(content,26);
    const u32 settingsPage=std::min(state.importPage,3u);
    const u32 firstSetting=settingsPage*3,lastSetting=std::min<u32>(firstSetting+3,settings.size());
    for(u32 index=firstSetting;index<lastSetting;++index) {
      const auto &[label,widget]=settings[index];
      auto row=deflate(takeTop(content,30),UiInsets::all(2));
      list.addRect(row,theme.color.raised,theme.radius.control);
      builder.label(deflate(row,UiInsets::symmetric(7,0)),label,
                    widget==EditorWidget::TextureProfileNormalGreen&&profile.interpretation!=resources::TextureInterpretationNormal?
                      theme.color.textMuted:theme.color.text,theme.type.caption);
      if(widget!=EditorWidget::TextureProfileNormalGreen||profile.interpretation==resources::TextureInterpretationNormal)
        router.addRegion(row,widgetId(widget));
    }
    const auto previous=takeLeft(pager,56),next=takeRight(pager,56);
    builder.label(previous,"‹ Opções",settingsPage?theme.color.text:theme.color.textMuted,theme.type.caption);
    builder.label(next,"Opções ›",settingsPage<3?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::End);
    builder.label(pager,std::to_string(settingsPage+1)+" / 4",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(settingsPage) router.addRegion(previous,widgetId(EditorWidget::ImportPreviousPage));
    if(settingsPage<3) router.addRegion(next,widgetId(EditorWidget::ImportNextPage));
    takeTop(content,4);
    const auto alpha=state.textureImportSourceHasAlpha?"alfa":"opaca";
    builder.label(takeTop(content,22),std::to_string(state.textureImportSourceWidth)+"×"+
        std::to_string(state.textureImportSourceHeight)+" · "+alpha+" · "+
        std::to_string(state.textureImportDroppedMips)+" mip(s) descartado(s)",theme.color.textDim,theme.type.caption);
    builder.label(takeTop(content,20),"Wrap e filtro pertencem a cada uso no Material.",theme.color.textMuted,theme.type.caption);
    for(const auto &line:wrapText(list,state.importSummary,content.width,theme.type.caption)) {
      if(content.height<20) break;
      builder.label(takeTop(content,20),line,theme.color.textMuted,theme.type.caption);
    }
    return;
  }

  if(state.importEnvironment) {
    auto actions=takeBottom(content,40);
    const auto cancel=deflate(takeLeft(actions,state.importReady?actions.width*.36f:actions.width),UiInsets::all(2));
    list.addRect(cancel,theme.color.raised,theme.radius.control);
    builder.label(cancel,state.importError?"Fechar":"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(cancel,widgetId(EditorWidget::ImportCancel));
    if(state.importReady) {
      const auto accept=deflate(actions,UiInsets::all(2));
      list.addRect(accept,theme.color.accent,theme.radius.control);
      builder.label(accept,"Importar HDRI",theme.color.accentInk,theme.type.caption,UiAlign::Center);
      router.addRegion(accept,widgetId(EditorWidget::ImportAccept));
    }
    list.addRect(takeTop(content,3),theme.color.accent,0);
    takeTop(content,5);
    const float settingHeight=std::clamp((content.height-44.0f)/5.0f,25.0f,32.0f);
    const auto setting=[&](const char *label,u32 value,EditorWidget down,EditorWidget up) {
      auto row=takeTop(content,settingHeight);builder.label(takeLeft(row,row.width*.52f),label,theme.color.textDim,theme.type.caption);
      const auto less=takeLeft(row,30),more=takeRight(row,30);
      builder.label(less,"−",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(more,"+",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(row,std::to_string(value).c_str(),theme.color.text,theme.type.numeric,UiAlign::Center);
      router.addRegion(less,widgetId(down));router.addRegion(more,widgetId(up));
    };
    setting("Panorama",state.environmentImportSettings.panoramaWidth,
            EditorWidget::EnvironmentPanoramaDown,EditorWidget::EnvironmentPanoramaUp);
    setting("Reflexão",state.environmentImportSettings.specularSize,
            EditorWidget::EnvironmentSpecularDown,EditorWidget::EnvironmentSpecularUp);
    setting("BRDF LUT",state.environmentImportSettings.brdfSize,
            EditorWidget::EnvironmentBrdfDown,EditorWidget::EnvironmentBrdfUp);
    setting("Amostras GGX",state.environmentImportSettings.specularSamples,
            EditorWidget::EnvironmentSpecularSamplesDown,EditorWidget::EnvironmentSpecularSamplesUp);
    setting("Amostras BRDF",state.environmentImportSettings.brdfSamples,
            EditorWidget::EnvironmentBrdfSamplesDown,EditorWidget::EnvironmentBrdfSamplesUp);
    takeTop(content,5);
    for(const auto &line:wrapText(list,state.importSummary,content.width,theme.type.caption))
      builder.label(takeTop(content,22),line,theme.color.textDim,theme.type.caption);
    return;
  }

  const struct {const char *label;Tab tab;EditorWidget widget;} tabs[]{
      {"Resumo",Tab::Summary,EditorWidget::ImportTabSummary},{"Estrutura",Tab::Structure,EditorWidget::ImportTabStructure},
      {"Malhas",Tab::Meshes,EditorWidget::ImportTabMeshes},
      {"Texturas",Tab::Textures,EditorWidget::ImportTabTextures},{"Perfil",Tab::Profile,EditorWidget::ImportTabProfile}};
  auto tabRow=takeTop(content,32);
  const float tabWidth=tabRow.width/static_cast<float>(std::size(tabs));
  for(const auto &tab:tabs) {
    const auto cell=deflate(takeLeft(tabRow,tabWidth),UiInsets::all(2));
    const bool on=state.importTab==tab.tab;
    list.addRect(cell,on?withAlpha(theme.color.accent,.18f):theme.color.raised,theme.radius.control);
    builder.label(cell,tab.label,on?theme.color.accent:theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(cell,widgetId(tab.widget));
  }
  takeTop(content,6);

  const auto nearlyEqual=[](float a,float b) {return std::fabs(a-b)<=std::max(std::fabs(a),std::fabs(b))*1e-5f;};
  // Tem de ser a mesma pergunta que a sessão faz (`sameImportPreparation`):
  // campo do perfil fora desta conta deixava publicar a prévia antiga com o
  // perfil novo na tela — o autor pedia normais calculadas e recebia as do arquivo.
  const bool profileApplied=nearlyEqual(state.importScale,state.importPreparedScale) &&
                            state.importTextureDimension==state.importPreparedTextureDimension &&
                            state.importNormals==state.importPreparedNormals &&
                            state.importNormalWeighting==state.importPreparedNormalWeighting &&
                            state.importSmoothingAngle==state.importPreparedSmoothingAngle &&
                            state.importTangents==state.importPreparedTangents &&
                            state.importCameras==state.importPreparedCameras &&
                            state.importLights==state.importPreparedLights &&
                            state.importTextureCompression==state.importPreparedTextureCompression;

  // Rodapé: cancelar sempre; publicar só com prévia pronta, perfil aplicado e
  // ambiguidades decididas. Apagado e sem toque até lá.
  auto actions=takeBottom(content,40);
  const auto cancel=deflate(takeLeft(actions,state.importReady?actions.width*.3f:actions.width),UiInsets::all(2));
  list.addRect(cancel,theme.color.raised,theme.radius.control);
  builder.label(cancel,state.importError?"Fechar":"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);
  router.addRegion(cancel,widgetId(EditorWidget::ImportCancel));
  if(state.importReady) {
    const bool decided=(!state.importAmbiguities || state.importAmbiguityChoice) && profileApplied;
    const auto resource=deflate(takeLeft(actions,actions.width*.5f),UiInsets::all(2));
    const auto scene=deflate(actions,UiInsets::all(2));
    list.addRect(resource,theme.color.raised,theme.radius.control);
    builder.label(resource,"Só recurso",decided?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    list.addRect(scene,decided?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(scene,"Na cena",decided?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(decided) {
      router.addRegion(resource,widgetId(EditorWidget::ImportAccept));
      router.addRegion(scene,widgetId(EditorWidget::ImportIntoScene));
    }
    if(state.importAmbiguities) {
      auto choice=takeBottom(content,36);
      const auto order=deflate(takeLeft(choice,choice.width*.5f),UiInsets::all(2));
      const auto fresh=deflate(choice,UiInsets::all(2));
      const auto pill=[&](UiRect rect,const char *label,bool on,EditorWidget widget) {
        list.addRect(rect,on?theme.color.accent:theme.color.raised,theme.radius.control);
        builder.label(rect,label,on?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
        router.addRegion(rect,widgetId(widget));
      };
      pill(order,"Pela ordem",state.importAmbiguityChoice==1,EditorWidget::ImportMatchInOrder);
      pill(fresh,"Como novos",state.importAmbiguityChoice==2,EditorWidget::ImportTreatAsNew);
    }
    if(!profileApplied)
      builder.label(takeBottom(content,22),"Perfil alterado: prepare de novo para publicar",theme.color.textDim,theme.type.caption);
  }

  // Paginação comum às abas de lista. Devolve o intervalo visível.
  const auto paginate=[&](usize count,float rowHeight) {
    u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height)/rowHeight));
    if(count>perPage) perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-28)/rowHeight));
    const u32 pages=std::max(1u,static_cast<u32>((count+perPage-1)/perPage));
    const u32 page=std::min(state.importPage,pages-1);
    if(pages>1) {
      auto bar=takeBottom(content,28);
      const auto previous=takeLeft(bar,70),next=takeRight(bar,70);
      builder.label(bar,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(page) {builder.label(previous,"Anterior",theme.color.text,theme.type.caption);router.addRegion(previous,widgetId(EditorWidget::ImportPreviousPage));}
      if(page+1<pages) {builder.label(next,"Próxima",theme.color.text,theme.type.caption,UiAlign::End);router.addRegion(next,widgetId(EditorWidget::ImportNextPage));}
    }
    return std::pair<usize,usize>{static_cast<usize>(page)*perPage,std::min(count,static_cast<usize>(page+1)*perPage)};
  };

  switch(state.importTab) {
  case Tab::Summary: {
    // O impacto na cena vem depois do resumo da fonte e é refeito a cada nó
    // marcado ou desmarcado na Estrutura; o resumo não muda com isso.
    const auto text=state.importImpact.empty()?state.importSummary:state.importSummary+"\n"+state.importImpact;
    const auto lines=wrapText(list,text,content.width,theme.type.caption);
    const auto [first,last]=paginate(lines.size(),22);
    list.pushClip(content);
    for(usize i=first;i<last;++i) builder.label(takeTop(content,22),lines[i],theme.color.textDim,theme.type.caption);
    list.popClip();
    break;
  }
  case Tab::Structure: {
    if(state.importNodes.empty()) {builder.label(takeTop(content,24),"Sem prévia de estrutura ainda",theme.color.textMuted,theme.type.caption);break;}
    builder.label(takeTop(content,24),std::to_string(state.importNodes.size())+" nó(s) no arquivo",theme.color.textMuted,theme.type.caption);
    const auto [first,last]=paginate(state.importNodes.size(),24);
    list.pushClip(content);
    for(usize i=first;i<last;++i) {
      const auto &node=state.importNodes[i];
      auto row=takeTop(content,24);
      const auto hit=row;
      // O interruptor diz "este nó vem para a cena". Filho de nó excluído mostra
      // o estado herdado, apagado e sem toque: desmarcá-lo sozinho não teria
      // efeito, e um botão sem efeito é pior do que nenhum. Sem identidade (a
      // correspondência ainda está ambígua), não há o que escolher.
      const bool inherited=node.excluded && i>0 && [&] {
        for(usize p=i;p-->0;) if(state.importNodes[p].depth<node.depth) return state.importNodes[p].excluded;
        return false;
      }();
      if(node.node.valid()) {
        // Coluna do tamanho do interruptor (kToggleWidth) com folga: mais estreita,
        // o interruptor transbordava por cima da contagem de malhas.
        auto mark=takeRight(row,kToggleWidth+10);
        if(inherited) builder.label(mark,"—",theme.color.textMuted,theme.type.caption,UiAlign::Center);
        else {
          builder.toggle(deflate(mark,UiInsets::all(3)),!node.excluded,widgetId(EditorWidget::ImportNodeToggleBase)+static_cast<u32>(i));
          router.addRegion(hit,widgetId(EditorWidget::ImportNodeToggleBase)+static_cast<u32>(i));
        }
      }
      takeLeft(row,static_cast<float>(std::min(node.depth,8u))*10.0f);
      if(node.draws) builder.label(takeRight(row,64),std::to_string(node.draws)+(node.draws==1?" malha":" malhas"),theme.color.textMuted,theme.type.caption,UiAlign::End);
      builder.label(row,node.name,node.excluded?theme.color.textMuted:node.draws?theme.color.text:theme.color.textDim,theme.type.caption);
    }
    list.popClip();
    break;
  }
  case Tab::Meshes: {
    // Aba Malhas (G6-A): a fonte medida, malha a malha. A linha responde o que
    // o autor pergunta antes de montar a cena de referência — tem UV? tem
    // tangente? quantos texels por metro? — e o cartão de detalhe explica o que
    // fazer com cada apontamento.
    if(state.importMeshes.empty()) {builder.label(takeTop(content,24),"Sem prévia de malhas ainda",theme.color.textMuted,theme.type.caption);break;}
    const auto severityColor=[&](u8 level) {
      return level==2?theme.color.danger:level==1?theme.color.warning:theme.color.accent;
    };
    if(state.importMeshDetail && state.importMeshSelected<state.importMeshes.size()) {
      const auto &mesh=state.importMeshes[state.importMeshSelected];
      auto header=takeTop(content,28);
      const auto back=takeLeft(header,84);
      builder.label(back,"< Malhas",theme.color.accent,theme.type.caption);
      router.addRegion(back,widgetId(EditorWidget::ImportMeshClose));
      builder.label(header,mesh.name,theme.color.text,theme.type.cardName);
      // Faixa de gravidade: a cor diz de longe se esta malha entra na cena de
      // referência como está.
      list.addRect(takeTop(content,3),severityColor(mesh.level),0);
      takeTop(content,6);
      const auto line=[&](const char *label,const std::string &value,bool dim=false) {
        if(value.empty()) return;
        auto row=takeTop(content,34);
        builder.label(takeLeft(row,row.width*.38f),label,theme.color.textMuted,theme.type.caption);
        for(const auto &part:wrapText(list,value,row.width,theme.type.caption)) {
          builder.label({row.x,row.y+(row.height-18)*.5f,row.width,18},part,dim?theme.color.textDim:theme.color.text,theme.type.caption);
          break;
        }
      };
      line("Contagem",mesh.counts);
      line("Canais",mesh.channels);
      line("Material",mesh.material,true);
      line("Tamanho na cena",mesh.size);
      line("Densidade",mesh.density);
      line("Uniformidade",mesh.stretch);
      list.pushClip(content);
      for(const auto &issue:mesh.issues) {
        const auto lines=wrapText(list,"• "+issue,content.width,theme.type.caption);
        for(const auto &text:lines) builder.label(takeTop(content,20),text,severityColor(mesh.level),theme.type.caption);
        takeTop(content,4);
      }
      list.popClip();
      break;
    }
    // O resumo do arquivo é longo (nós, malhas, escala, densidade): numa linha só
    // ele saía cortado no aparelho, justamente na parte da densidade.
    for(const auto &line:wrapText(list,state.importMeshSummary,content.width,theme.type.caption))
      builder.label(takeTop(content,20),line,theme.color.textMuted,theme.type.caption);
    takeTop(content,4);
    const auto [first,last]=paginate(state.importMeshes.size(),56);
    list.pushClip(content);
    for(usize i=first;i<last;++i) {
      const auto &mesh=state.importMeshes[i];
      auto row=takeTop(content,56);
      router.addRegion(row,widgetId(EditorWidget::ImportMeshRowBase)+static_cast<u32>(i));
      auto title=takeTop(row,20);
      if(mesh.level) {
        const auto chip=deflate(takeRight(title,84),UiInsets::symmetric(2,2));
        list.addRect(chip,withAlpha(severityColor(mesh.level),.18f),theme.radius.control);
        builder.label(chip,mesh.level==2?"erro":"atenção",severityColor(mesh.level),theme.type.caption,UiAlign::Center);
      }
      builder.label(title,mesh.name,theme.color.text,theme.type.caption);
      builder.label(takeTop(row,18),mesh.counts+" · "+mesh.channels,theme.color.textDim,theme.type.caption);
      builder.label(takeTop(row,18),mesh.size+(mesh.density.empty()?std::string{}:" · "+mesh.density),
                    theme.color.textMuted,theme.type.caption);
    }
    list.popClip();
    break;
  }
  case Tab::Textures: {
    if(state.importTextures.empty()) {builder.label(takeTop(content,24),"Nenhuma textura aplicada",theme.color.textMuted,theme.type.caption);break;}
    u64 total=0;for(const auto &texture:state.importTextures) total+=texture.bytes;
    builder.label(takeTop(content,24),std::to_string(state.importTextures.size())+" textura(s) · "+decimalText(static_cast<double>(total)/1048576.0,1)+" MB com mips",
                  theme.color.textMuted,theme.type.caption);
    const auto [first,last]=paginate(state.importTextures.size(),42);
    list.pushClip(content);
    for(usize i=first;i<last;++i) {
      const auto &texture=state.importTextures[i];
      auto row=takeTop(content,42);
      builder.label(takeTop(row,20),"#"+std::to_string(i+1)+" · "+std::to_string(texture.width)+"×"+std::to_string(texture.height)+
                    " · "+(texture.format==renderer::AuthoringTextureAstc4x4?"ASTC 4×4":
                           texture.format==renderer::AuthoringTextureAstc6x6?"ASTC 6×6":
                           texture.format==renderer::AuthoringTextureAstc8x8?"ASTC 8×8":"RGBA8"),theme.color.text,theme.type.caption);
      builder.label(takeTop(row,20),std::string(texture.srgb?"cor (sRGB)":"dados (linear)")+" · "+
                    decimalText(static_cast<double>(texture.bytes)/1048576.0,1)+" MB · "+std::to_string(texture.uses)+
                    (texture.uses==1?" uso":" usos"),theme.color.textDim,theme.type.caption);
    }
    list.popClip();
    break;
  }
  case Tab::Profile: {
    const auto scaleText=[](float scale) {
      return "×"+decimalText(scale,scale>=1?0:scale>=.1f?1:scale>=.01f?2:3);
    };
    // As ações ficam no rodapé, como Revert/Apply no Import Settings da Unity:
    // cada campo novo do perfil empurrava "Preparar" para fora de uma tela
    // baixa, e um perfil pendente sem o botão de aplicar vira beco sem saída.
    // Os campos ocupam o que sobra acima.
    const bool canApply=state.importReady && !profileApplied;
    const auto save=deflate(takeBottom(content,38),UiInsets::all(2));
    const auto apply=deflate(takeBottom(content,38),UiInsets::all(2));
    list.addRect(apply,canApply?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(apply,"Preparar com este perfil",canApply?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(canApply) router.addRegion(apply,widgetId(EditorWidget::ImportApplyProfile));
    list.addRect(save,theme.color.raised,theme.radius.control);
    builder.label(save,"Salvar como padrão do projeto",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(save,widgetId(EditorWidget::ImportSaveDefaultProfile));
    takeBottom(content,6);
    // Oito linhas de controle, paginadas como as outras abas: numa tela baixa
    // (um celular deitado tem ~400 de altura útil) a lista corrida cortava as
    // linhas de baixo sem aviso nem como alcançá-las.
    enum ProfileRow : usize {Scale,Size,TextureLabel,TextureSteps,Compression,Streaming,StreamingPriority,Normals,Weighting,Smoothing,
                             Tangents,Cameras,Lights,ProfileRowCount};
    const auto [firstRow,lastRow]=paginate(ProfileRowCount,40);
    const auto cycle=[&](UiRect row,const char *label,const char *value,EditorWidget widget) {
      builder.label(takeLeft(row,row.width*.45f),label,theme.color.text,theme.type.caption);
      const auto cell=deflate(row,UiInsets::all(2));
      list.addRect(cell,theme.color.raised,theme.radius.control);
      builder.label(cell,value,theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widgetId(widget));
    };
    for(usize index=firstRow;index<lastRow;++index) {
      auto row=takeTop(content,40);
      switch(static_cast<ProfileRow>(index)) {
      case Scale: {
        builder.label(takeLeft(row,row.width*.4f),"Escala",theme.color.text,theme.type.caption);
        const auto down=deflate(takeLeft(row,36),UiInsets::all(2)),up=deflate(takeRight(row,36),UiInsets::all(2));
        for(const auto &[rect,label,widget]:{std::tuple{down,"-",EditorWidget::ImportScaleDown},std::tuple{up,"+",EditorWidget::ImportScaleUp}}) {
          list.addRect(rect,theme.color.raised,theme.radius.control);
          builder.label(rect,label,theme.color.text,theme.type.body,UiAlign::Center);
          router.addRegion(rect,widgetId(widget));
        }
        builder.label(row,scaleText(state.importScale),theme.color.text,theme.type.body,UiAlign::Center);
        break;
      }
      case Size: {
        // Tamanho que o modelo terá com a escala do rascunho (aproximado pelas
        // esferas dos desenhos: é uma prévia, não uma medida).
        const float ratio=state.importPreparedScale>0?state.importScale/state.importPreparedScale:1;
        builder.label(row,state.importHasExtent?
                      "Tamanho aprox.: "+decimalText(state.importExtent[0]*ratio,2)+" × "+decimalText(state.importExtent[1]*ratio,2)+" × "+
                      decimalText(state.importExtent[2]*ratio,2):std::string("Tamanho: sem geometria preparada"),
                      theme.color.textDim,theme.type.caption);
        break;
      }
      case TextureLabel:builder.label(row,"Textura máxima (px)",theme.color.text,theme.type.caption);break;
      case TextureSteps: {
        const struct {u32 value;EditorWidget widget;} steps[]{{256,EditorWidget::ImportTextureDimension256},{512,EditorWidget::ImportTextureDimension512},
                                                             {1024,EditorWidget::ImportTextureDimension1024},{2048,EditorWidget::ImportTextureDimension2048}};
        const float stepWidth=row.width/static_cast<float>(std::size(steps));
        for(const auto &step:steps) {
          const auto cell=deflate(takeLeft(row,stepWidth),UiInsets::all(2));
          const bool on=state.importTextureDimension==step.value;
          list.addRect(cell,on?theme.color.accent:theme.color.raised,theme.radius.control);
          builder.label(cell,std::to_string(step.value),on?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
          router.addRegion(cell,widgetId(step.widget));
        }
        break;
      }
      // Format/Compression do Texture Importer da Unity (S1). No aparelho sem
      // ASTC a escolha continua salva, mas o rótulo diz que aqui vira RGBA8.
      case Compression: {
        const u8 value=state.importTextureCompression;
        std::string text=value==4?"ASTC 4×4":value==6?"ASTC 6×6":value==8?"ASTC 8×8":"Sem compressão";
        if(value && !state.importAstcSupported) text+=" (RGBA8 aqui)";
        cycle(row,"Compressão das texturas",text.c_str(),EditorWidget::ImportTextureCompressionCycle);
        break;
      }
      // Stream Mipmap Levels e Priority do Texture Importer (S2), para todas as
      // texturas da fonte. Valem ao publicar; não repreparam nada.
      case Streaming:cycle(row,"Streaming de mips",state.importTextureStreaming?"Sim":"Não",
                           EditorWidget::ImportTextureStreamingToggle);break;
      case StreamingPriority:cycle(row,"Prioridade de streaming",std::to_string(state.importTextureStreamingPriority).c_str(),
                                   EditorWidget::ImportTextureStreamingPriorityCycle);break;
      // Geometria derivada (G2). Os rótulos seguem o Model Import Settings da
      // Unity — Normals / Normals Mode / Tangents — porque é o vocabulário que o
      // autor já traz de fora.
      case Normals:cycle(row,"Normais",state.importNormals==resources::GltfNormalsCalculate?"Calcular":"Importar",EditorWidget::ImportNormalsCycle);break;
      case Weighting:cycle(row,"Modo das normais",state.importNormalWeighting==resources::GltfNormalWeightAngle?"Por ângulo":"Por área",
                           EditorWidget::ImportNormalWeightingCycle);break;
      case Smoothing: {
        // Smoothing Angle: vale para toda normal gerada (Calcular, ou arquivo
        // sem NORMAL). Arestas mais agudas que o ângulo ficam duras.
        builder.label(takeLeft(row,row.width*.45f),"Ângulo de suavização",theme.color.text,theme.type.caption);
        const auto down=deflate(takeLeft(row,36),UiInsets::all(2)),up=deflate(takeRight(row,36),UiInsets::all(2));
        for(const auto &[rect,label,widget]:{std::tuple{down,"-",EditorWidget::ImportSmoothingDown},std::tuple{up,"+",EditorWidget::ImportSmoothingUp}}) {
          list.addRect(rect,theme.color.raised,theme.radius.control);
          builder.label(rect,label,theme.color.text,theme.type.body,UiAlign::Center);
          router.addRegion(rect,widgetId(widget));
        }
        builder.label(row,std::to_string(state.importSmoothingAngle)+"°",theme.color.text,theme.type.body,UiAlign::Center);
        break;
      }
      case Tangents:cycle(row,"Tangentes",state.importTangents==resources::GltfTangentsCalculate?"Calcular":"Importar",EditorWidget::ImportTangentsCycle);break;
      case Cameras:cycle(row,"Importar câmeras",state.importCameras?"Sim":"Não",EditorWidget::ImportCamerasToggle);break;
      case Lights:cycle(row,"Importar luzes",state.importLights?"Sim":"Não",EditorWidget::ImportLightsToggle);break;
      case ProfileRowCount:break;
      }
    }
    // As notas só aparecem no espaço que sobrar: são explicação, não controle.
    takeTop(content,8);
    list.pushClip(content);
    for(const auto &line:wrapText(list,"Importar usa o que vem no arquivo e gera só o que falta. Uma malha sem normal não é "
                                       "desenhável aqui, então nunca fica sem. Guardado com a fonte ao publicar; reimportar e "
                                       "reabrir o projeto usam o mesmo perfil.",content.width,theme.type.caption)) {
      if(content.height<20) break;
      builder.label(takeTop(content,20),line,theme.color.textMuted,theme.type.caption);
    }
    list.popClip();
    break;
  }
  }
}
// R4: textura escolhida em Arquivos, em Propriedades: o visualizador (imagem,
// canal, mip, zoom, fundo, perfil e residência) e quem usa a textura.
void buildTextureInspector(ScreenBuilder &builder,UiRect content) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  const float wanted=34.0f+static_cast<float>(std::max<usize>(1,state.textureUserLabels.size()))*28.0f;
  auto users=takeBottom(content,std::min(content.height*.28f,wanted));
  buildTextureViewer(builder,content);
  builder.list.addRect({users.x,users.y,users.width,1},theme.color.lineSoft);
  builder.label(takeTop(users,30),"Usuários ("+std::to_string(state.textureUserLabels.size())+")",theme.color.textDim,theme.type.caption);
  if(state.textureUserLabels.empty()) {
    if(users.height>=24) builder.label(takeTop(users,24),"Nenhum objeto ou material do projeto usa esta textura",theme.color.textMuted,theme.type.caption);
    return;
  }
  builder.list.pushClip(users);
  for(u32 i=0;i<state.textureUserLabels.size() && users.height>=26;++i) {
    auto row=takeTop(users,28);
    const bool entity=i<state.textureUserEntities.size() && state.textureUserEntities[i]!=kInvalidEntity;
    builder.label(deflate(row,UiInsets::symmetric(6,0)),state.textureUserLabels[i],entity?theme.color.text:theme.color.textMuted,theme.type.caption);
    if(entity) builder.router.addRegion(row,widgetId(EditorWidget::TextureUserBase)+i);
  }
  builder.list.popClip();
}

// R4: gerenciador de texturas: grade com miniaturas, busca por nome ou pasta e
// filtros de uso, arquivo, alfa e teto.
void buildTextureManager(ScreenBuilder &builder,UiRect content) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto title=takeTop(content,36),back=takeLeft(title,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  builder.router.addRegion(back,widgetId(EditorWidget::TextureManagerClose));
  auto import=takeRight(title,82);
  builder.list.addRect(deflate(import,UiInsets::all(2)),theme.color.raised,theme.radius.control);
  builder.label(import,"+ Importar",theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(import,widgetId(EditorWidget::ImportTexture));
  builder.label(title,(state.textureFolder.empty()?std::string("Texturas do projeto"):state.textureFolder)+" · "+
                std::to_string(state.textureManagerRows.size()),theme.color.text,theme.type.caption);
  auto search=deflate(takeTop(content,36),UiInsets::all(2));
  builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(deflate(search,UiInsets::symmetric(8,0)),
                state.textureQuery.empty()?std::string("Buscar por nome ou pasta"):"Busca: "+state.textureQuery,
                state.textureQuery.empty()?theme.color.textMuted:theme.color.text,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::TextureSearch));
  static constexpr const char *filters[]{"Todas","Não usadas","Ausentes","Alteradas","Sem alfa","Acima do teto"};
  for(u32 line=0;line<2;++line) {
    auto row=takeTop(content,32);
    const float third=row.width/3;
    for(u32 column=0;column<3;++column) {
      const u32 index=line*3+column;
      const auto box=deflate(column<2?takeLeft(row,third):row,UiInsets::all(2));
      const bool on=state.textureFilter==index;
      builder.list.addRect(box,on?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(box,filters[index],on?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(box,widgetId(EditorWidget::TextureFilterBase)+index);
    }
  }
  const u32 rows=static_cast<u32>(state.textureManagerRows.size());
  if(!rows) {
    builder.label(takeTop(content,40),"Nenhuma textura neste filtro",theme.color.textMuted,theme.type.caption);
    return;
  }
  auto footer=takeBottom(content,32);
  const float cell=104.0f,caption=22.0f;
  const u32 columns=std::max(1u,static_cast<u32>(content.width/cell));
  const u32 lines=std::max(1u,static_cast<u32>(content.height/(cell+caption)));
  const u32 perPage=columns*lines;
  const u32 pages=(rows+perPage-1)/perPage,page=std::min(state.textureManagerPage,pages-1);
  const float width=content.width/static_cast<float>(columns);
  for(u32 slot=0;slot<perPage && page*perPage+slot<rows;++slot) {
    const u32 index=state.textureManagerRows[page*perPage+slot];
    const UiRect box{content.x+width*static_cast<float>(slot%columns),content.y+(cell+caption)*static_cast<float>(slot/columns),width,cell+caption};
    const auto inner=deflate(box,UiInsets::all(3));
    builder.list.addRect(inner,theme.color.raised,theme.radius.control);
    const auto thumb=deflate(UiRect{inner.x,inner.y,inner.width,inner.height-caption},UiInsets::all(4));
    if(index<state.projectTextureThumbs.size() && !state.projectTextureThumbs[index].isEmpty()) {
      const auto &texels=state.projectTextureThumbs[index];
      const float fit=std::min(thumb.width/texels.width,thumb.height/texels.height);
      builder.list.addPreviewImage({thumb.x+(thumb.width-texels.width*fit)*.5f,thumb.y+(thumb.height-texels.height*fit)*.5f,
                                    texels.width*fit,texels.height*fit},texels);
    } else {
      builder.list.addRect(thumb,theme.color.lineSoft,4);
    }
    const UiRect name{inner.x+4,inner.bottom()-caption,inner.width-8,caption-2};
    builder.list.pushClip(name);
    builder.label(name,index<state.projectTextureNames.size()?state.projectTextureNames[index]:std::string(),theme.color.text,theme.type.caption);
    builder.list.popClip();
    builder.router.addRegion(inner,widgetId(EditorWidget::TextureManagerRowBase)+index);
  }
  if(pages>1) {
    const auto previous=takeLeft(footer,36),next=takeRight(footer,36);
    builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(footer,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(page>0) builder.router.addRegion(previous,widgetId(EditorWidget::TextureManagerPrevious));
    if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::TextureManagerNext));
  }
}
} // namespace

void buildInspector(ScreenBuilder &builder, const UiRect &panel) {
  if (builder.state.importPanel) {
    builder.list.addRect(panel, builder.theme.color.surface);
    builder.router.addBlocker(panel);
    buildImportDock(builder, deflate(panel, UiInsets::all(builder.theme.spacing.small)));
    return;
  }
  // R4: textura escolhida em Arquivos e gerenciador da pasta Texturas.
  if (builder.state.textureManager || builder.state.textureInspector) {
    builder.list.addRect(panel, builder.theme.color.surface);
    builder.router.addBlocker(panel);
    const auto inner = deflate(panel, UiInsets::all(builder.theme.spacing.small));
    if (builder.state.textureManager) buildTextureManager(builder, inner);
    else buildTextureInspector(builder, inner);
    return;
  }
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

  if(builder.state.inspectorMenu && builder.state.workspace==EditorWorkspace::Scene) {
    buildObjectActions(builder,content,*entity);
    return;
  }
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
         state.renamingResource || state.goingToLine || state.creatingCodeFolder || state.searchingConsole || state.searchingTextures || state.presetNaming || state.viewNaming;
}

const char *platformFieldTitle(const EditorScreenState &state) {
  if (state.presetNaming) return "Nome do preset";
  if (state.viewNaming) return state.viewRenaming ? "Novo nome da vista" : "Nome da vista";
  if (state.numericField != 0) return "Valor";
  if (state.renamingResource) return "Arquivo";
  if (state.editingScriptInstance != 0) return "Campo";
  if (state.creatingScript) return state.scriptTemplate==EditorCodeWorkspace::HelperTemplate?"Auxiliar C#":"Componente C#";
  if (state.searchingTextures) return "Buscar textura";
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

// Painel Qualidade. Cada linha é um controle da política de renderização do
// projeto, com o nome que a Unity usa no nível de qualidade e no URP Asset;
// o rodapé diz o que o renderer está fazendo de verdade AGORA — resolução
// interna e custo de GPU —, que é o número que decide se a escolha cabe.
void buildQualityPanel(ScreenBuilder &builder, const UiRect &viewport) {
  const auto &state = builder.state;
  const auto &theme = builder.theme;
  auto &list = builder.list;
  auto &router = builder.router;
  const auto &draft = state.qualityDraft;
  const float width = std::min(520.0f, std::max(280.0f, viewport.width - 24.0f));
  const float height = std::min(viewport.height - 16.0f, 610.0f);
  const UiRect panel{viewport.x + std::max(8.0f, std::min(16.0f, viewport.width - width - 8.0f)),
                     viewport.y + 8.0f, width, height};
  list.addRect(panel, theme.color.surface, 6);
  router.addBlocker(panel);
  auto content = deflate(panel, UiInsets::all(10));
  auto header = takeTop(content, 26);
  const auto close = takeRight(header, 28);
  builder.label(close, "X", theme.color.textDim, theme.type.caption, UiAlign::Center);
  router.addRegion(close, widgetId(EditorWidget::QualityClose));
  builder.label(header, "Gráficos do projeto", theme.color.text, theme.type.cardName);
  auto tabs=takeTop(content,34);
  const std::array<std::tuple<const char *,EditorWidget>,5> tabItems{{
    {"Geral",EditorWidget::QualityTabGeneral},{"Sombras",EditorWidget::QualityTabShadows},
    {"Luz e pós",EditorWidget::QualityTabLighting},{"Desempenho",EditorWidget::QualityTabPerformance},
    {"Texturas",EditorWidget::QualityTabTextures}}};
  for(u32 index=0;index<tabItems.size();++index) {
    auto cell=deflate(takeLeft(tabs,tabs.width/static_cast<float>(tabItems.size()-index)),UiInsets::all(2));
    list.addRect(cell,index==state.qualityTab?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(cell,std::get<0>(tabItems[index]),index==state.qualityTab?theme.color.accentInk:theme.color.text,
                  theme.type.caption,UiAlign::Center);
    router.addRegion(cell,widgetId(std::get<1>(tabItems[index])));
  }
  if(state.qualityTab==2) {
    builder.label(takeTop(content,28),"Padrões globais; volumes de Ambiente podem sobrescrever.",
                  theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  // Rodapé primeiro: Aplicar e a linha do que o renderer faz agora nunca podem
  // sair do painel. Numa tela baixa as linhas é que encolhem.
  const auto apply = deflate(takeBottom(content, 40), UiInsets::all(2));
  const auto stats = takeBottom(content, 20);
  const u32 rowCount=state.qualityTab==2?10u:state.qualityTab==0?9u:state.qualityTab==1?8u:state.qualityTab==4?8u:6u;
  const u32 rowsPerPage=std::max(1u,static_cast<u32>(std::floor(content.height/25.0f)));
  const u32 pageCount=(rowCount+rowsPerPage-1u)/rowsPerPage;
  const u32 page=std::min(state.qualityPage,pageCount-1u);
  const u32 firstRow=page*rowsPerPage;
  const u32 rowsOnPage=std::min(rowsPerPage,rowCount-firstRow);
  const float rowHeight = std::clamp(content.height / static_cast<float>(rowsOnPage), 25.0f, 38.0f);
  if(pageCount>1) {
    auto pager=takeRight(header,std::min(116.0f,header.width*.48f));
    list.addRect(pager,theme.color.surface,theme.radius.control);
    const auto previous=takeLeft(pager,30.0f),next=takeRight(pager,30.0f);
    builder.label(previous,"‹",page?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(next,"›",page+1u<pageCount?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(pager,std::to_string(page+1u)+" / "+std::to_string(pageCount),theme.color.textDim,
                  theme.type.caption,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::QualityPagePrevious));
    if(page+1u<pageCount) router.addRegion(next,widgetId(EditorWidget::QualityPageNext));
  }
  u32 rowCursor=0;
  const auto row = [&](const char *label, const std::string &value, EditorWidget widget,
                       bool enabled=true) {
    const u32 index=rowCursor++;
    if(index<firstRow||index>=firstRow+rowsOnPage) return;
    auto line = takeTop(content, rowHeight);
    builder.label(takeLeft(line, line.width * .46f), label, theme.color.textDim, theme.type.caption);
    const auto cell = deflate(line, UiInsets::all(2));
    list.addRect(cell, theme.color.raised, theme.radius.control);
    builder.label(cell, value, enabled?theme.color.text:theme.color.textMuted,
                  theme.type.caption, UiAlign::Center);
    if(enabled) router.addRegion(cell, widgetId(widget));
  };
  const auto stepper = [&](const char *label, const std::string &value, EditorWidget down, EditorWidget up) {
    const u32 index=rowCursor++;
    if(index<firstRow||index>=firstRow+rowsOnPage) return;
    auto line = takeTop(content, rowHeight);
    builder.label(takeLeft(line, line.width * .46f), label, theme.color.textDim, theme.type.caption);
    const auto minus = deflate(takeLeft(line, 36), UiInsets::all(2));
    const auto plus = deflate(takeRight(line, 36), UiInsets::all(2));
    for (const auto &[rect, glyph, widget] : {std::tuple{minus, "-", down}, std::tuple{plus, "+", up}}) {
      list.addRect(rect, theme.color.raised, theme.radius.control);
      builder.label(rect, glyph, theme.color.text, theme.type.body, UiAlign::Center);
      router.addRegion(rect, widgetId(widget));
    }
    builder.label(line, value, theme.color.text, theme.type.caption, UiAlign::Center);
  };
  const auto percent=[](float value,float inherited) {return value==inherited?std::string("Do nível"):
    std::to_string(static_cast<int>(value*100.0f+.5f))+"%";};
  const auto measure=[](float value,float inherited,const char *unit,int digits=1) {return value==inherited?std::string("Do nível"):
    decimalText(value,digits)+unit;};
  if(state.qualityTab==0) {
    std::string level=renderer::qualityLevelLabel(draft.preset);
    if(draft.preset==renderer::QualityPreset::Auto&&!state.qualityDetected.empty()) level+=" ("+state.qualityDetected+")";
    row("Nível",level,EditorWidget::QualityLevel);
    stepper("Escala de renderização",percent(draft.resolutionScale,0.0f),EditorWidget::QualityScaleDown,EditorWidget::QualityScaleUp);
    // Ampliação temporal mostra um de quatro estados: indisponível com o
    // motivo do aparelho, pendente de Aplicar, ativa (o que o renderer
    // executou no último quadro) ou apenas disponível.
    using Mode=renderer::TemporalReconstruction;
    const auto mode=renderer::temporalReconstruction(draft);
    const auto upscaler=renderer::isTemporalUpscaler(draft.upscalingFilter);
    std::string temporal=renderer::temporalReconstructionLabel(mode);
    const auto availability=mode==Mode::ArmAsr?state.qualityArmAsr:mode==Mode::Fsr2?state.qualityFsr2:
                            renderer::TemporalUpscalerAvailability::Available;
    const bool pending=mode!=renderer::temporalReconstruction(state.qualityApplied);
    if(availability!=renderer::TemporalUpscalerAvailability::Available)
      temporal+=std::string(" · indisponível: ")+renderer::temporalUpscalerAvailabilityLabel(availability);
    else if(pending) temporal+=" · aplicar";
    else if(upscaler) temporal+=state.qualityExecutedUpscaler==draft.upscalingFilter?" · ativo":
        std::string(" · falhou: ")+renderer::temporalUpscalerAvailabilityLabel(state.qualityExecutedStatus);
    else if(mode==Mode::NativeTaa) temporal+=state.qualityTemporalAaExecuted?" · ativo":" · sem histórico nesta vista";
    row("Ampliação temporal",temporal,EditorWidget::QualityTemporalMode);
    row("Qualidade temporal",mode==Mode::ArmAsr?std::string(renderer::temporalUpscalerQualityLabel(draft.temporalUpscalerQuality)):
        mode==Mode::Fsr2?std::string("Não se aplica ao FSR 2"):std::string("Só com Arm ASR"),
        EditorWidget::QualityTemporalQuality,mode==Mode::ArmAsr);
    row("Anti-aliasing",upscaler?std::string("Pelo ")+renderer::upscalingFilterLabel(draft.upscalingFilter):
        draft.antiAliasing==renderer::AntiAliasingMode::Temporal && state.qualityMotionAvailable
        ?std::string("TAA (câmera + objetos)"):std::string(renderer::antiAliasingLabel(draft.antiAliasing)),
        EditorWidget::QualityAntiAliasing,!upscaler);
    row("Filtro espacial",upscaler?std::string("Substituído pelo ")+renderer::upscalingFilterLabel(draft.upscalingFilter):
        std::string(renderer::upscalingFilterLabel(draft.upscalingFilter)),EditorWidget::QualityUpscaling,!upscaler);
    row("Taxa alvo",draft.maximumRenderHz?std::to_string(draft.maximumRenderHz)+" Hz":std::string("Do nível"),EditorWidget::QualityRate);
    row("Texturas / anisotropia",renderer::textureQualityLabel(draft.textures),EditorWidget::QualityTextures);
    row("Escala dinâmica",renderer::featureOverrideLabel(draft.dynamicResolution),EditorWidget::QualityDynamic);
  } else if(state.qualityTab==1) {
    row("Sombras",renderer::shadowQualityLabel(draft.shadows),EditorWidget::QualityShadows);
    row("Cascatas",draft.shadowCascadeCount?std::to_string(draft.shadowCascadeCount):std::string("Do nível"),EditorWidget::QualityShadowCascades);
    row("Resolução por cascata",draft.shadowCascadeResolution?std::to_string(draft.shadowCascadeResolution)+" px":std::string("Do nível"),EditorWidget::QualityShadowResolution);
    stepper("Distância máxima",measure(draft.shadowMaximumDistance,0.0f," m",0),EditorWidget::QualityShadowDistanceDown,EditorWidget::QualityShadowDistanceUp);
    stepper("Bias constante",measure(draft.shadowDepthBiasConstant,-1.0f,"",1),EditorWidget::QualityShadowBiasDown,EditorWidget::QualityShadowBiasUp);
    stepper("Bias de inclinação",measure(draft.shadowDepthBiasSlope,-1.0f,"",1),EditorWidget::QualityShadowSlopeDown,EditorWidget::QualityShadowSlopeUp);
    stepper("Offset normal",measure(draft.shadowNormalOffsetTexels,-1.0f," texel",1),EditorWidget::QualityShadowNormalDown,EditorWidget::QualityShadowNormalUp);
    row("Cache estático",renderer::featureOverrideLabel(draft.staticShadowCache),EditorWidget::QualityShadowCache);
  } else if(state.qualityTab==2) {
    row("Ambiente",renderer::ambientQualityLabel(draft.ambient),EditorWidget::QualityAmbient);
    row("BRDF especular",renderer::featureOverrideLabel(draft.environmentSplitSumBrdf),EditorWidget::QualityEnvironmentBrdf);
    row("Pós-processamento",renderer::postQualityLabel(draft.post),EditorWidget::QualityPost);
    stepper("Limiar do bloom",measure(draft.bloomThreshold,-1.0f,"",1),EditorWidget::QualityBloomThresholdDown,EditorWidget::QualityBloomThresholdUp);
    stepper("Intensidade do bloom",percent(draft.bloomIntensity,-1.0f),EditorWidget::QualityBloomIntensityDown,EditorWidget::QualityBloomIntensityUp);
    row("Anti-aliasing",renderer::isTemporalUpscaler(draft.upscalingFilter)
        ?std::string("Pelo ")+renderer::upscalingFilterLabel(draft.upscalingFilter):
        draft.antiAliasing==renderer::AntiAliasingMode::Temporal && state.qualityMotionAvailable
        ?std::string("TAA (câmera + objetos)"):std::string(renderer::antiAliasingLabel(draft.antiAliasing)),
        EditorWidget::QualityAntiAliasing,!renderer::isTemporalUpscaler(draft.upscalingFilter));
    stepper("Peso temporal",percent(draft.temporalHistoryWeight,-1.0f),EditorWidget::QualityTemporalWeightDown,EditorWidget::QualityTemporalWeightUp);
    constexpr const char *temporalViews[]{"Imagem final","Profundidade","Histórico usado","Profundidade rejeitada",
                                          "Vetor de movimento","Reatividade","Composição"};
    row("Diagnóstico temporal",state.qualityTemporalAvailable
        ?temporalViews[std::min(state.qualityTemporalDebug,6u)]:"Sem entradas temporais nesta vista",
        EditorWidget::QualityTemporalDebug,state.qualityTemporalAvailable);
    stepper("Nitidez",percent(draft.postSharpen,-1.0f),EditorWidget::QualitySharpenDown,EditorWidget::QualitySharpenUp);
    row("Vinheta",renderer::featureOverrideLabel(draft.postVignette),EditorWidget::QualityVignette);
  } else if(state.qualityTab==4) {
    // Seção Textures do Quality da Unity: limite global de mip, anisotropia e
    // Mipmap Streaming (Memory Budget, Max Level Reduction). As duas últimas
    // linhas são leitura: o que o renderer tem na GPU agora.
    const auto megabytes=[](u64 bytes) {return decimalText(static_cast<float>(bytes)/1048576.0f,bytes<(10ull<<20)?1:0)+" MB";};
    const auto &streaming=state.qualityTextureStreaming;
    row("Qualidade / anisotropia",renderer::textureQualityLabel(draft.textures),EditorWidget::QualityTextures);
    row("Streaming de mipmaps",draft.textureStreaming==renderer::FeatureOverride::Inherit?std::string("Do nível · desligado"):
        std::string(renderer::featureOverrideLabel(draft.textureStreaming)),EditorWidget::QualityTextureStreaming);
    stepper("Orçamento de memória",draft.textureStreamingBudgetMegabytes?std::to_string(draft.textureStreamingBudgetMegabytes)+" MB":
            std::string("Do nível"),EditorWidget::QualityStreamingBudgetDown,EditorWidget::QualityStreamingBudgetUp);
    stepper("Redução máxima",draft.textureStreamingMaxLevelReduction?std::to_string(draft.textureStreamingMaxLevelReduction)+" nível(is)":
            std::string("Do nível · 2"),EditorWidget::QualityStreamingReductionDown,EditorWidget::QualityStreamingReductionUp);
    stepper("Envio por quadro",draft.textureStreamingUploadKilobytesPerFrame?
            megabytes(static_cast<u64>(draft.textureStreamingUploadKilobytesPerFrame)<<10):std::string("Do nível · 4 MB"),
            EditorWidget::QualityStreamingUploadDown,EditorWidget::QualityStreamingUploadUp);
    row("Na GPU agora",!streaming.active?std::string("Streaming desligado · ")+megabytes(streaming.currentBytes):
        megabytes(streaming.currentBytes)+" de "+megabytes(streaming.budgetBytes)+(streaming.overBudget?" · acima do orçamento":""),
        EditorWidget::QualityTextureStreaming,false);
    row("Trocas",!streaming.active?std::string("—"):std::to_string(streaming.streamingTextures)+" texturas · "+
        std::to_string(streaming.pendingLoads)+" pendentes · "+std::to_string(streaming.budgetReducedTextures)+" reduzidas pelo orçamento",
        EditorWidget::QualityTextureStreaming,false);
    row("Vista de depuração",state.qualityTextureStreamingDebug
        ?std::string("Verde no nível · vermelho abaixo · azul acima · cinza fora"):std::string("Desligada"),
        EditorWidget::QualityStreamingDebugView);
  } else {
    row("Escala dinâmica",renderer::featureOverrideLabel(draft.dynamicResolution),EditorWidget::QualityDynamic);
    stepper("Escala dinâmica mínima",percent(draft.dynamicResolutionMinimumScale,0.0f),EditorWidget::QualityDynamicMinimumDown,EditorWidget::QualityDynamicMinimumUp);
    row("Seleção de LOD",renderer::featureOverrideLabel(draft.lodSelection),EditorWidget::QualityLodSelection);
    stepper("Erro de LOD",measure(draft.lodPixelErrorBudget,0.0f," px",2),EditorWidget::QualityLodErrorDown,EditorWidget::QualityLodErrorUp);
    stepper("Histerese de LOD",percent(draft.lodHysteresisBandRatio,0.0f),EditorWidget::QualityLodHysteresisDown,EditorWidget::QualityLodHysteresisUp);
    row("Variantes de material",renderer::featureOverrideLabel(draft.materialShaderVariants),EditorWidget::QualityMaterialVariants);
  }
  // Aplicar reconstrói o renderer: é explícito, como o Apply do Import
  // Settings, porque troca alvos de renderização e leva um instante.
  list.addRect(apply, state.qualityDirty ? theme.color.accent : theme.color.raised, theme.radius.control);
  builder.label(apply, state.qualityDirty ? "Aplicar" : "Aplicado",
                state.qualityDirty ? theme.color.accentInk : theme.color.textMuted, theme.type.caption, UiAlign::Center);
  if (state.qualityDirty) router.addRegion(apply, widgetId(EditorWidget::QualityApply));
  builder.label(stats, state.qualityStats, theme.color.textMuted, theme.type.caption);
}

// Escolha do modelo de cena (G6-A). Cada linha traz o nome e o que o cenário
// entrega, porque montar um modelo mexe na cena aberta e o autor precisa saber
// o que vai aparecer antes de tocar.
void buildSceneTemplatePanel(ScreenBuilder &builder, const UiRect &viewport) {
  const auto &theme = builder.theme;
  auto &list = builder.list;
  auto &router = builder.router;
  const auto models = sceneTemplates();
  const float width = std::min(360.0f, std::max(220.0f, viewport.width - 32.0f));
  const UiRect panel{viewport.x + (viewport.width - width) * .5f, viewport.y + 48.0f, width,
                     56.0f + static_cast<float>(models.size()) * 78.0f};
  list.addRect(panel, theme.color.surface, 6);
  router.addBlocker(panel);
  auto content = deflate(panel, UiInsets::all(12));
  auto header = takeTop(content, 26);
  const auto close = takeRight(header, 28);
  builder.label(close, "X", theme.color.textDim, theme.type.caption, UiAlign::Center);
  router.addRegion(close, widgetId(EditorWidget::SceneTemplateClose));
  builder.label(header, "Modelo de cena", theme.color.text, theme.type.cardName);
  for (u32 index = 0; index < models.size(); ++index) {
    auto row = takeTop(content, 78);
    const auto card = deflate(row, UiInsets::symmetric(0, 3));
    list.addRect(card, theme.color.raised, theme.radius.control);
    auto inner = deflate(card, UiInsets::all(8));
    builder.label(takeTop(inner, 20), models[index].name, theme.color.text, theme.type.caption);
    for (const auto &line : wrapText(list, models[index].summary, inner.width, theme.type.caption)) {
      builder.label(takeTop(inner, 20), line, theme.color.textMuted, theme.type.caption);
      if (inner.height <= 0) break;
    }
    router.addRegion(card, widgetId(EditorWidget::SceneTemplateRowBase) + index);
  }
}

// Painel das vistas salvas da cena (G6-A). Uma linha por enquadramento, com a
// escolhida em destaque; tocar a linha LEVA a câmera até ela. O rodapé age
// sobre a escolhida — atualizar com a vista atual, renomear, excluir — e
// "Salvar vista atual" cria uma nova. A lista é curta de propósito: ela existe
// para repetir um enquadramento, não para organizar um acervo.
void buildSceneViewsPanel(ScreenBuilder &builder, const UiRect &viewport) {
  const auto &state = builder.state;
  const auto &theme = builder.theme;
  auto &list = builder.list;
  auto &router = builder.router;
  const auto &views = state.document->views();
  const float width = std::min(300.0f, std::max(180.0f, viewport.width - 32.0f));
  const float rows = static_cast<float>(std::min<u32>(views.count(), 6));
  const UiRect panel{viewport.x + std::max(8.0f, std::min(228.0f, viewport.width - width - 8.0f)), viewport.y + 56.0f,
                     width, 124.0f + rows * 34.0f + (views.empty() ? 34.0f : 0.0f) + (views.count() > 6 ? 20.0f : 0.0f)};
  // Cabeçalho, linhas, rodapé de ações e "Salvar vista atual" — a conta antiga
  // esquecia o último botão, e ele saía cortado no aparelho.
  list.addRect(panel, theme.color.surface, 6);
  router.addBlocker(panel);
  auto content = deflate(panel, UiInsets::all(10));
  auto header = takeTop(content, 26);
  const auto close = takeRight(header, 28);
  builder.label(close, "X", theme.color.textDim, theme.type.caption, UiAlign::Center);
  router.addRegion(close, widgetId(EditorWidget::ViewsClose));
  builder.label(header, "Vistas", theme.color.text, theme.type.cardName);
  if (views.empty())
    builder.label(takeTop(content, 34), "Nenhuma vista salva ainda", theme.color.textMuted, theme.type.caption);
  for (u32 index = 0; index < views.count() && index < 6; ++index) {
    const auto *view = views.at(index);
    auto row = takeTop(content, 34);
    const bool chosen = state.viewSelected == index;
    if (chosen) list.addRect(row, theme.color.raised, theme.radius.control);
    builder.label(deflate(row, UiInsets::symmetric(8, 0)), view->name.c_str(),
                  chosen ? theme.color.accent : theme.color.text, theme.type.caption);
    router.addRegion(row, widgetId(EditorWidget::SceneViewRowBase) + index);
  }
  if (views.count() > 6)
    builder.label(takeTop(content, 20), (std::to_string(views.count() - 6) + " além destas").c_str(),
                  theme.color.textMuted, theme.type.caption);
  takeTop(content, 4);
  auto actions = takeTop(content, 34);
  const auto button = [&](UiRect rect, const char *label, EditorWidget widget, bool enabled) {
    rect = deflate(rect, UiInsets::all(2));
    list.addRect(rect, enabled ? theme.color.raised : theme.color.silhouette, theme.radius.control);
    builder.label(rect, label, enabled ? theme.color.text : theme.color.textMuted, theme.type.caption, UiAlign::Center);
    if (enabled) router.addRegion(rect, widgetId(widget));
  };
  const bool hasChosen = state.viewSelected < views.count();
  const float third = actions.width / 3.0f;
  button(takeLeft(actions, third), "Atualizar", EditorWidget::ViewUpdate, hasChosen);
  button(takeLeft(actions, third), "Renomear", EditorWidget::ViewRename, hasChosen);
  button(actions, "Excluir", EditorWidget::ViewDelete, hasChosen);
  button(takeTop(content, 34), "Salvar vista atual", EditorWidget::ViewSave, views.count() < runtime::SceneViews::kMaximum);
}

static void buildProjectDialogs(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
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
    const bool hasSecondary=!state.playSecondaryActionLabel.empty();
    const bool showJump=hasSecondary?state.playHasCharacter:
        state.playHasScripts || (controlled&&characterComponent(*controlled)&&characterComponent(*controlled)->jumpSpeed>0);
    if(showJump) {
      const UiRect jump{layout.viewport.x+layout.viewport.width-(hasSecondary?208.0f:108.0f),layout.viewport.y+layout.viewport.height-76,92,56};
      list.addRect(jump,theme.color.raised,theme.radius.control);
      builder.label(jump,hasSecondary?"Saltar":state.playHasScripts?"Ação":"Saltar",state.playPaused?theme.color.textFaint:theme.color.text,theme.type.body,UiAlign::Center);
      if(!state.playPaused) router.addRegion(jump,widgetId(EditorWidget::JumpCharacter),theme.touch.minimumTarget);
    }
    if(hasSecondary) {
      const UiRect action{layout.viewport.x+layout.viewport.width-108,layout.viewport.y+layout.viewport.height-76,92,56};
      list.addRect(action,theme.color.raised,theme.radius.control);
      builder.label(action,state.playSecondaryActionLabel,state.playPaused?theme.color.textFaint:theme.color.text,theme.type.body,UiAlign::Center);
      if(!state.playPaused) router.addRegion(action,widgetId(EditorWidget::PlaySecondaryAction),theme.touch.minimumTarget);
    }
    if(state.playFirstPerson) {
      const float cx=layout.viewport.x+layout.viewport.width*.5f,cy=layout.viewport.y+layout.viewport.height*.5f;
      list.addRect({cx-1,cy-9,2,18},theme.color.accent);
      list.addRect({cx-9,cy-1,18,2},theme.color.accent);
    }
    builder.label({layout.viewport.x+12,layout.viewport.y+8,layout.viewport.width-24,24},
                  state.playFirstPerson?"Esquerda: mover · direita: olhar · botões: saltar e agir":
                  state.playHasScripts?"Arraste à esquerda para mover · Ação ativa a habilidade":
                  controlled&&characterComponent(*controlled)?"Arraste à esquerda para mover o personagem":"Simulação - use o quadrado no topo para parar", theme.color.textDim,theme.type.caption);
    if(!state.playHudMessage.empty()) {
      const float width=std::max(120.0f,std::min(510.0f,layout.viewport.width-(hasSecondary?222.0f:122.0f)));
      const UiRect status{layout.viewport.x+8,layout.viewport.y+layout.viewport.height-62,width,40};
      list.addRect(status,withAlpha(theme.color.surface,.92f),theme.radius.control);
      builder.label(deflate(status,UiInsets::symmetric(8,4)),state.playHudMessage,theme.color.text,theme.type.caption);
    }
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
  // Vistas salvas: o enquadramento é o que torna duas medições comparáveis, e
  // por isso fica ao lado dos controles de câmera, não escondido num menu.
  // Fica DEPOIS de Lighting/Effects (+104..+220): no mesmo lugar, o botão de
  // Lighting cobria este e ele não aparecia no aparelho.
  builder.iconButton({layout.viewport.x + 228.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::SceneLayers, widgetId(EditorWidget::ViewsOpen), state.viewsPanel);
  if(state.viewsPanel && state.document) buildSceneViewsPanel(builder, layout.viewport);
  if(state.templatePanel) buildSceneTemplatePanel(builder, layout.viewport);
  if(compact) {
    const UiRect button{layout.viewport.x+104,layout.viewport.y+56,100,40};
    list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Painéis",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(button,widgetId(EditorWidget::CompactPanelMenu));
  }
  if(state.workspace==EditorWorkspace::Scene) {
    // Unity 6 agrupa Lighting e Effects no topo do Scene View. Aqui a mesma
    // organização controla o renderer editorial real; a seta abre as partes
    // que a Astra já consome (céu, neblina e pós).
    const float optionsX=layout.viewport.x+104.0f;
    const UiRect lighting{optionsX,layout.viewport.y+8.0f,40.0f,40.0f};
    const UiRect effects{optionsX+48.0f,layout.viewport.y+8.0f,40.0f,40.0f};
    const UiRect effectsMenu{optionsX+88.0f,layout.viewport.y+8.0f,28.0f,40.0f};
    builder.iconButton(lighting,UiIcon::LightingSceneLighting,
                       widgetId(EditorWidget::SceneLightingToggle),state.sceneLighting);
    builder.iconButton(effects,UiIcon::LightingSceneEffects,
                       widgetId(EditorWidget::SceneEffectsToggle),state.sceneEffects);
    builder.iconButton(effectsMenu,state.sceneEffectsMenu?UiIcon::UiChevronUp:UiIcon::UiChevronDown,
                       widgetId(EditorWidget::SceneEffectsMenu),state.sceneEffectsMenu);
    if(state.sceneEffectsMenu) {
      const UiRect panel{effects.x, effects.bottom()+6.0f, 218.0f, 126.0f};
      list.addRect(panel,withAlpha(theme.color.surface,.97f),theme.radius.control);
      // Construção explícita para preservar o retângulo completo como alvo.
      const auto row=[&](float y,UiIcon glyph,const char *label,EditorWidget widget,bool active) {
        UiRect bounds{panel.x+5.0f,y,panel.width-10.0f,36.0f};
        list.addRect(bounds,active?theme.color.raised:theme.color.surface,theme.radius.control);
        UiRect content=bounds;
        list.addImage(centred(takeLeft(content,34.0f),18.0f,18.0f),static_cast<UiImageId>(glyph),
                      active?theme.color.accent:theme.color.textDim);
        builder.label(content,label,active?theme.color.text:theme.color.textDim,theme.type.caption);
        router.addRegion(bounds,widgetId(widget),theme.touch.minimumTarget);
      };
      row(panel.y+6.0f,UiIcon::LightingSky,"Sky",EditorWidget::SceneSkyToggle,state.sceneSky);
      row(panel.y+45.0f,UiIcon::LightingFog,"Fog",EditorWidget::SceneFogToggle,state.sceneFog);
      row(panel.y+84.0f,UiIcon::LightingPostProcessing,"Post Processing",EditorWidget::ScenePostToggle,state.scenePost);
    }
    if(state.cameraViewEntity) {
      const auto *cameraEntity=state.document->find(state.cameraViewEntity);
      const auto *lens=cameraEntity?cameraComponent(*cameraEntity):nullptr;
      if(lens && layout.viewport.width>=240 && layout.viewport.height>=240) {
        const float panelWidth=std::min(390.f,layout.viewport.width-16.f);
        const UiRect panel{layout.viewport.x+(layout.viewport.width-panelWidth)*.5f,
          layout.viewport.y+(compact?146.f:50.f),panelWidth,48.f};
        list.addRect(panel,theme.color.surface,theme.radius.control);
        const bool ortho=lens->projection==scene::CameraProjection::Orthographic;
        const char *labels[]{ortho?"Meia altura":"FOV", "Próximo", "Distante"};
        const float values[]{ortho?lens->orthographicHalfHeight:lens->verticalFov,lens->nearPlane,lens->farPlane};
        for(u32 i=0;i<3;++i) {
          const UiRect cell{panel.x+i*panel.width/3+3,panel.y+3,panel.width/3-6,panel.height-6};
          list.addRect(cell,theme.color.raised,theme.radius.control);
          char value[48];std::snprintf(value,sizeof(value),"%.5g %s",static_cast<double>(values[i]),i==0&&!ortho?"°":"m");
          list.pushClip(cell);
          builder.label({cell.x,cell.y,cell.width,17},labels[i],theme.color.textMuted,theme.type.caption,UiAlign::Center);
          builder.label({cell.x,cell.y+17,cell.width,25},value,theme.color.text,theme.type.caption,UiAlign::Center);
          list.popClip();
          router.addRegion(cell,widgetId(EditorWidget::CameraLens)+i);
        }
      }
      // Reserve the framing controls and the compact tools rail before placing
      // the camera identity. It must never sit underneath a toolbar button.
      const UiRect badge{layout.viewport.x+(compact?8.f:104.f),layout.viewport.y+(compact?104.f:8.f),
        std::max(0.f,std::min(250.f,layout.viewport.width-(compact?16.f:154.f))),34};
      if(badge.width>0) {
        list.addRect(badge,theme.color.surface,theme.radius.control);
        list.pushClip(badge);
        builder.label(deflate(badge,UiInsets{8,0,8,0}),cameraEntity?
          std::string(state.cameraPiloting?"Pilotando · ":"Vista · ")+cameraEntity->name:"Câmera indisponível",
          theme.color.text,theme.type.caption);
        list.popClip();
      }
    }
    if(state.cameraPreviewEntity && layout.viewport.width>=280 && layout.viewport.height>=300) {
      const float width=std::min(280.f,layout.viewport.width*.48f),height=width*9.f/16.f;
      const UiRect panel{layout.viewport.right()-width-10,layout.viewport.bottom()-height-160,width,height+94};
      list.addRect(panel,theme.color.surface,theme.radius.control);router.addBlocker(panel);
      const auto *camera=state.document->find(state.cameraPreviewEntity);
      builder.label({panel.x+8,panel.y,panel.width-44,30},camera?camera->name:"Câmera",theme.color.text,theme.type.caption);
      const UiRect close{panel.right()-34,panel.y,34,30};
      builder.label(close,"×",theme.color.text,theme.type.title,UiAlign::Center);router.addRegion(close,widgetId(EditorWidget::CameraPreviewClose));
      const UiRect image{panel.x+4,panel.y+30,panel.width-8,height-4};
      if(state.cameraPreviewReady) list.addImage(image,kUiCameraPreviewImage,0xffffffff,6);
      else {
        builder.label(image,state.cameraPreviewFailed?
            (state.cameraPreviewDiagnostic.empty()?"Falha · tocar para tentar":state.cameraPreviewDiagnostic):
            "Preparando prévia",theme.color.textMuted,theme.type.caption,UiAlign::Center);
        if(state.cameraPreviewFailed) router.addRegion(image,widgetId(EditorWidget::CameraPreviewRetry));
      }
      const float controlWidth=(panel.width-20)/2;
      const UiRect resolution{panel.x+8,panel.y+height+58,controlWidth,28};
      const UiRect frequency{resolution.right()+4,resolution.y,controlWidth,28};
      list.addRect(resolution,theme.color.raised,theme.radius.control);
      list.addRect(frequency,theme.color.raised,theme.radius.control);
      builder.label(resolution,std::to_string(state.cameraPreviewWidth)+"×"+std::to_string(state.cameraPreviewHeight),theme.color.text,theme.type.caption,UiAlign::Center);
      builder.label(frequency,std::to_string(static_cast<int>(state.cameraPreviewFrequency))+" Hz máx.",theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(resolution,widgetId(EditorWidget::CameraPreviewResolution));
      router.addRegion(frequency,widgetId(EditorWidget::CameraPreviewFrequency));
      builder.label({panel.x+8,panel.y+height+30,panel.width-16,28},"Ambiente e pós da câmera",theme.color.textMuted,theme.type.caption);
    }
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
  // Painel global de gráficos fica acima de todas as ferramentas do viewport.
  // O roteador resolve da última região para a primeira, então a mesma ordem
  // também impede Lighting/Effects e a navegação inferior de roubarem toques.
  // Menus contextuais e modais abaixo continuam acima dele deliberadamente.
  if(state.qualityPanel) buildQualityPanel(builder,layout.viewport);
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
  if (!state.platformTextInput && (state.renameEntity != kInvalidEntity || state.editingHierarchySearch || state.editingCreationSearch || state.editingComponentSearch || state.editingMeshSearch || state.editingReferenceSearch || state.presetNaming || state.viewNaming)) {
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
    if((state.numericField&0xff000000u)==widgetId(EditorWidget::ComponentTripleBase)) {
      const auto separator=takeTop(content,28);
      builder.label(separator,"Separar canais · espaço",theme.color.accent,theme.type.caption,UiAlign::Center);
      router.addRegion(separator,widgetId(EditorWidget::NumericKeyBase)+12);
    }
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
  if(state.colorField) {
    router.addBlocker(state.surface);
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
    const auto panel=centred(state.surface,std::min(360.f,state.surface.width-16),std::min(360.f,state.surface.height-16));
    list.addRect(panel,theme.color.surface,theme.radius.control);
    auto content=deflate(panel,UiInsets::all(12));
    auto title=takeTop(content,32);list.addRect(takeRight(title,48),pickerColor(state.colorHue,state.colorSaturation,state.colorValue),6);
    builder.label(title,"Cor · RGB linear",theme.color.text,theme.type.title);
    auto footer=takeBottom(content,38);const auto cancel=takeLeft(footer,footer.width/2);
    builder.label(cancel,"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);router.addRegion(cancel,widgetId(EditorWidget::ColorCancel));
    builder.label(footer,"Aplicar",theme.color.accent,theme.type.caption,UiAlign::Center);router.addRegion(footer,widgetId(EditorWidget::ColorApply));
    const auto outline=[&](UiRect r) {
      list.addRect({r.x,r.y,r.width,2},0xffffffff,0);
      list.addRect({r.x,r.bottom()-2,r.width,2},0xff000000,0);
      list.addRect({r.x,r.y,2,r.height},0xffffffff,0);
      list.addRect({r.right()-2,r.y,2,r.height},0xff000000,0);
    };
    const auto hue=takeBottom(content,30);
    for(u32 h=0;h<24;++h) {
      const UiRect cell{hue.x+h*hue.width/24,hue.y,hue.width/24,26};
      list.addRect(cell,pickerColor(h/24.f,1,1),0);router.addRegion(cell,widgetId(EditorWidget::ColorHueBase)+h);
      if(h==static_cast<u32>(std::round(state.colorHue*24))%24) outline(cell);
    }
    for(u32 y=0;y<11;++y) for(u32 x=0;x<11;++x) {
      const UiRect cell{content.x+x*content.width/11,content.y+y*content.height/11,content.width/11,content.height/11};
      list.addRect(cell,pickerColor(state.colorHue,x/10.f,1-y/10.f),0);
      router.addRegion(cell,widgetId(EditorWidget::ColorSvBase)+y*11+x);
      if(x==static_cast<u32>(std::round(state.colorSaturation*10)) && y==static_cast<u32>(std::round((1-state.colorValue)*10))) outline(cell);
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

  if(widget==widgetId(EditorWidget::ComponentVisualsToggle)) {state.showComponentVisuals=!state.showComponentVisuals;return outcome;}
  if(widget==widgetId(EditorWidget::CameraViewClose)) {state.cameraViewEntity=0;return outcome;}
  if(widget==widgetId(EditorWidget::SceneLightingToggle)) {state.sceneLighting=!state.sceneLighting;return outcome;}
  if(widget==widgetId(EditorWidget::SceneEffectsToggle)) {state.sceneEffects=!state.sceneEffects;return outcome;}
  if(widget==widgetId(EditorWidget::SceneEffectsMenu)) {state.sceneEffectsMenu=!state.sceneEffectsMenu;return outcome;}
  if(widget==widgetId(EditorWidget::SceneSkyToggle)) {state.sceneSky=!state.sceneSky;return outcome;}
  if(widget==widgetId(EditorWidget::SceneFogToggle)) {state.sceneFog=!state.sceneFog;return outcome;}
  if(widget==widgetId(EditorWidget::ScenePostToggle)) {state.scenePost=!state.scenePost;return outcome;}
  if(widget>=widgetId(EditorWidget::ComponentVisualBase)&&widget<widgetId(EditorWidget::ComponentVisualBase)+0x01000000u) {
    const auto id=widget-widgetId(EditorWidget::ComponentVisualBase);
    if(document.find(id)) {state.selection=id;state.textureInspector=false;state.textureViewer=false;state.propertyPage=0;}
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::ComponentGroupBase)&&widget<widgetId(EditorWidget::ComponentGroupBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);const auto *value=entity?entity->components.findInstance(state.expandedNative):nullptr;
    if(!value) return outcome;
    const auto groups=componentGroups(*value);
    const auto i=widget-widgetId(EditorWidget::ComponentGroupBase);
    if(i<groups.size()) {state.componentGroup=groups[i];state.propertyPage=0;}
    return outcome;
  }

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
        auto plan=scene::planComponentAddition(value.components,entry.type->id);
        if(!plan.ready) {state.status=plan.error;return outcome;}
        value.components=std::move(plan.candidate);
        state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=~u32{0};state.addingComponent=false;state.nativeMenu=0;
      } else if(operation==widgetId(EditorWidget::ComponentRemoveBase)) {
        if(const auto *dependent=scene::componentInstanceRemovalBlockedBy(instance,value.components)) {
          state.status=std::string("Usado por ")+dependent->name;return outcome;
        }
        if(const auto use=runtime::componentRemovalReferenceUse(document,entity->id,instance);use.object) {
          state.status=std::string("Referenciado por ")+document.find(use.object)->name;return outcome;
        }
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
      state.reparentEntity=state.selection;state.entityMenu=false;state.inspectorMenu=false;state.status="Toque no novo pai na hierarquia";break;
    case EditorWidget::MoveToRoot:
      outcome.documentChanged=history.reparentKeepingWorld(document,state.selection,document.root());
      state.entityMenu=false;state.inspectorMenu=false;state.status=outcome.documentChanged?"Movido para raiz":"Transformacao incompativel";break;
    case EditorWidget::MoveEarlier:
    case EditorWidget::MoveLater: {
      u32 index=0;const auto *selected=document.find(state.selection);
      if(selected && document.childIndexOf(selected->id,index)) {
        const auto count=document.childrenOf(selected->parent).size();
        const u32 next=widget==widgetId(EditorWidget::MoveEarlier)?(index?index-1:0):std::min(index+1,static_cast<u32>(count-1));
        if(next!=index) outcome.documentChanged=history.reparent(document,selected->id,selected->parent,next);
      }
      state.entityMenu=false;state.inspectorMenu=false;break;
    }
    case EditorWidget::AssetsPrevious:
    case EditorWidget::AssetsNext: {
      const u32 page=std::max(1u,static_cast<u32>(std::max(0.0f,layout.viewport.height-108)/44));
      if(widget==widgetId(EditorWidget::AssetsNext)) state.assetScroll=std::min(state.assetScroll+page,state.assetCount?state.assetCount-1:0);
      else state.assetScroll=state.assetScroll>page?state.assetScroll-page:0;
      break;
    }
    case EditorWidget::InspectorMenu: state.inspectorMenu=!state.inspectorMenu;state.transformMenu=false;state.propertyPage=0;break;
    case EditorWidget::RenameSelection:
      if(const auto *entity=document.find(state.selection)) {
        state.renameEntity=entity->id;
        std::snprintf(state.renameText,sizeof(state.renameText),"%s",entity->name);
      }
      state.entityMenu=false;state.inspectorMenu=false;break;
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
      state.entityMenu=false;state.inspectorMenu=false;
      break;
    }
    case EditorWidget::CreateGroup: {
      const auto copy=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Objeto vazio");
      if(copy) {state.selection=copy;outcome.documentChanged=true;}
      state.entityMenu=false;state.inspectorMenu=false;
      break;
    }
    case EditorWidget::DeleteSelection:
      outcome.documentChanged=history.destroyEntity(document,state.selection);
      if(outcome.documentChanged) state.selection=kInvalidEntity;
      state.entityMenu=false;state.inspectorMenu=false;
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
    case EditorWidget::CreateChildGroup: {
      const auto parent=document.exists(state.selection)?state.selection:document.root();
      const auto child=history.createEntity(document,parent,EditorEntityKind::Folder,"Objeto vazio");
      if(child) {state.selection=child;outcome.documentChanged=true;}
      state.inspectorMenu=false;break;
    }
    case EditorWidget::TransformMenu: state.transformMenu=!state.transformMenu;state.inspectorMenu=false;break;
    case EditorWidget::TransformCopy:
      if(const auto *entity=document.find(state.selection)) {state.transformClipboard=entity->transform;state.hasTransformClipboard=true;state.status="Transformação copiada";}
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::TransformPaste:
      if(state.hasTransformClipboard && document.exists(state.selection) && state.selection!=document.root())
        outcome.documentChanged=history.setTransform(document,state.selection,state.transformClipboard);
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::TransformReset:
    case EditorWidget::TransformResetPosition:
    case EditorWidget::TransformResetRotation:
    case EditorWidget::TransformResetScale:
      if(const auto *entity=document.find(state.selection); entity && state.selection!=document.root()) {
        auto transform=entity->transform;const EditorTransform identity;
        const bool all=widget==widgetId(EditorWidget::TransformReset);
        if(all || widget==widgetId(EditorWidget::TransformResetPosition)) std::copy(identity.position,identity.position+3,transform.position);
        if(all || widget==widgetId(EditorWidget::TransformResetRotation)) std::copy(identity.rotationDegrees,identity.rotationDegrees+3,transform.rotationDegrees);
        if(all || widget==widgetId(EditorWidget::TransformResetScale)) std::copy(identity.scale,identity.scale+3,transform.scale);
        outcome.documentChanged=history.setTransform(document,state.selection,transform);
      }
      state.transformMenu=false;state.inspectorMenu=false;break;
    case EditorWidget::ObjectLayerPrevious:
    case EditorWidget::ObjectLayerNext:
      if(const auto *entity=document.find(state.selection)) {
        // Só camadas NOMEADAS: uma camada sem nome é uma camada que o projeto
        // não declarou, e escolher uma delas às cegas não diz nada à física.
        const auto &layers=document.layers();const u32 count=runtime::GameplayLayers::kCount;
        const bool forward=widget==widgetId(EditorWidget::ObjectLayerNext);
        u32 layer=entity->layer%count;
        for(u32 step=1;step<count;++step) {
          const u32 candidate=forward?(layer+step)%count:(layer+count-step)%count;
          if(layers.named(candidate)) {layer=candidate;break;}
        }
        if(layer!=entity->layer) {auto values=*entity;values.layer=layer;outcome.documentChanged=history.applyValues(document,state.selection,values);}
        else state.status="Nenhuma outra camada nomeada no projeto";
      }
      break;
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
