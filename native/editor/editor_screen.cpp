#include "runtime/scene_physics.h"
#include "runtime/scene_timers.h"
#include "runtime/scene_tweens.h"
#include "editor/editor_component_impact.h"
#include "editor/editor_color_picker.h"
#include "editor/editor_numeric_expression.h"
#include "editor/editor_value_library.h"
#include "editor/editor_curve_view.h"
#include <sstream>
#include <iomanip>
#include "scene/event_connection.h"
#include "scene/tween_sequence.h"
#include "runtime/scene_tween_sequences.h"
#include "scene/script_behavior.h"
#include "scene/prefab_link.h"
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
#include "scene/component_reflection.h"
#include "editor/editor_grid.h"
#include "editor/editor_collider_geometry.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_camera_handles.h"
#include "editor/editor_component_handles.h"
#include "editor/editor_collider_handles.h"
#include "editor/editor_collider_topology.h"
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
// Barra de status: uma linha de texto de legenda com folga para o toque.
constexpr float kStatusBarHeight = 24.0f;
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

UiIcon iconForEntity(const EditorEntity &entity) {
  if(const auto *link=scene::prefabLink(entity.components);link && link->instanceRoot==entity.id) return UiIcon::ScenePrefab;
  if(cameraComponent(entity)) return UiIcon::EditorAuthorCamera;
  if(meshRenderer(entity)) return UiIcon::EditorAuthorObject;
  if(runtime::lightComponent(entity)) return UiIcon::EditorAuthorSun;
  if(runtime::characterComponent(entity)) return UiIcon::ComponentCharacter;
  if(runtime::physicsBody(entity)) return UiIcon::ComponentPhysics;
  if(entity.components.find(scene::PhysicsEventConnection2D::descriptor))return UiIcon::EventPhysicsConnection2d;
  if(entity.components.find(scene::PhysicsEventConnection3D::descriptor))return UiIcon::EventPhysicsConnection;
  if(entity.components.find(scene::EventConnection::descriptor))return UiIcon::ComponentEventConnection;
  if(const auto *timer=static_cast<const scene::Timer*>(entity.components.find(scene::Timer::descriptor)))
    return timer->elapsedAction!=0 ? UiIcon::EventTimeoutConnection : UiIcon::ComponentTimer;
  if(entity.components.find(scene::TweenSequence::descriptor)) return UiIcon::ComponentTweenSequence;
  if(const auto *tween=static_cast<const scene::TransformTween*>(entity.components.find(scene::TransformTween::descriptor)))
    return tween->finishedAction!=0?UiIcon::EventTweenCompletion:UiIcon::ComponentTweenTransform;
  return iconForKind(entity.kind);
}

struct ScreenBuilder final {
  const EditorScreenState &state;
  const UiTheme &theme;
  UiDrawList &list;
  UiInputRouter &router;
  u32 componentPage=0;
  // Bloco F: o gerenciador anota as linhas de textura das fontes que desenhou.
  std::vector<u32> *visibleSourceTextureRows=nullptr;
  // Medidas que a sessão usa para rolar o Inspector de textura.
  EditorScreenLayout *layout=nullptr;
  // Inspector focado num componente: a instância cujo cartão aparece sozinho.
  u64 onlyComponent=0;
  // Dentro da janela focada: o cabeçalho não tem cadeado nem ⋮ (a aba já é
  // presa ao alvo e a janela tem o seu próprio ⋮).
  bool focusedWindow=false;
  // Inspector da multisseleção (o da seleção, fora de janela focada).
  bool multiEdit=false;
  bool mixed(std::string_view key) const {return multiEdit && state.multi.isMixed(key);}
  // Interruptor com valores diferentes (Unity: traço no lugar do check):
  // tocar liga em todos.
  void mixedToggle(const UiRect &area, u32 widget, bool editable=true) {
    const UiRect bounds{area.right() - kToggleWidth, area.y + (area.height - kToggleHeight) * 0.5f, kToggleWidth, kToggleHeight};
    list.addRect(bounds, withAlpha(theme.color.accent, 0.14f), kToggleHeight * 0.5f);
    list.addRect({bounds.x + bounds.width * .3f, bounds.y + bounds.height * .5f - 1.5f, bounds.width * .4f, 3}, theme.color.accent, 1.5f);
    if(editable) router.addRegion(bounds, widget, theme.touch.minimumTarget);
  }

  bool isPressed(u32 widget) const { return state.pressedWidget == widget && widget != 0; }

  void iconButton(const UiRect &bounds, UiIcon icon, u32 widget, bool active = false,
                  UiColor tint = 0xFFFFFFFF, bool editable = true) {
    const UiColor background = active ? theme.color.accent
                                      : (isPressed(widget) ? theme.color.line : theme.color.raised);
    list.addRect(bounds, background, theme.radius.control);
    list.addImage(centred(bounds, kIconSize, kIconSize), static_cast<UiImageId>(icon),
                  active ? theme.color.accentInk : tint);
    if(editable) router.addRegion(bounds, widget, theme.touch.minimumTarget);
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

  void checkbox(const UiRect &area,bool on,u32 widget,bool editable=true,bool mixedValue=false) {
    const auto box=centred(area,18,18);
    list.addRect(box,on&&editable?theme.color.accent:theme.color.canvas,2);
    list.addBorder(box,editable?theme.color.textDim:theme.color.textFaint,1,2);
    if(mixedValue) list.addRect({box.x+4,box.y+8,10,2},theme.color.text,1);
    else if(on) list.addImage(deflate(box,UiInsets::all(3)),static_cast<UiImageId>(UiIcon::UiCheck),editable?theme.color.accentInk:theme.color.textFaint);
    if(editable) router.addRegion(area,widget,theme.touch.minimumTarget);
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
    const auto control=[&](const char *label,EditorWidget id,bool enabled,bool active=false) {
      const auto rect=takeRight(content,active||id==EditorWidget::PlayInspect?92:76);
      builder.list.addRect(rect,active?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(rect,label,active?theme.color.accentInk:enabled?theme.color.text:theme.color.textFaint,
                    theme.type.caption,UiAlign::Center);
      if(enabled) builder.router.addRegion(rect,widgetId(id),theme.touch.minimumTarget);
      takeRight(content,theme.spacing.small);
    };
    if(builder.state.playPaused) control("Passo",EditorWidget::StepPlay,true);
    else {
      char speed[32];std::snprintf(speed,sizeof(speed),"Tempo %.2gx",double(builder.state.playTimeScale));
      control(speed,EditorWidget::PlayTimeScale,true,builder.state.playTimeScale!=1);
    }
    control(builder.state.playPaused?"Retomar":"Pausar",EditorWidget::PausePlay,true);
    control("Inspecionar",EditorWidget::PlayInspect,true,builder.state.playInspect);
  } else {
    action(UiIcon::EditorAuthorRedo, EditorWidget::Redo, builder.state.canRedo);
    action(UiIcon::EditorAuthorUndo, EditorWidget::Undo, builder.state.canUndo);
    // Unity 6000.0: a Busca global fica na barra, ao lado do histórico.
    builder.iconButton(takeRight(content, kActionButton), UiIcon::IdeSearch, widgetId(EditorWidget::GlobalSearchOpen),
                       builder.state.globalSearch);
    takeRight(content, theme.spacing.tiny);
  }

  const auto sceneMenu=takeLeft(content,76.0f);
  builder.list.addRect(sceneMenu,theme.color.raised,theme.radius.control);
  builder.label(sceneMenu,builder.state.workspace==EditorWorkspace::Gui?"Interface":"Cena",theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(sceneMenu,widgetId(EditorWidget::ProjectMenu));
  takeLeft(content,theme.spacing.small);
  const auto save=takeLeft(content,64.0f);
  builder.list.addRect(save,theme.color.raised,theme.radius.control);
  builder.label(save,"Salvar",theme.color.text,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(save,widgetId(EditorWidget::SaveDocument));

  takeLeft(content,theme.spacing.small);
  if(builder.state.workspace==EditorWorkspace::Scene && content.width>=44.0f) {
    builder.iconButton(takeLeft(content,40.0f),UiIcon::UiDiagnostics,
                       widgetId(EditorWidget::DiagnosticDockToggle),builder.state.diagnosticDockOpen);
    takeLeft(content,theme.spacing.small);
  }
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
    // Cabe o chip: o rótulo de contexto à direita é opcional e some antes. Uma
    // vista de diagnóstico ligada tem prioridade: é estado vivo que precisa de
    // saída à vista, então o chip de modo cede o espaço a ela.
    const bool debugQuick=builder.state.workspace==EditorWorkspace::Scene &&
        builder.state.qualityTemporalAvailable && builder.state.qualityTemporalDebug!=0;
    if(content.width>=(debugQuick?264.0f:120.0f)) {
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
      builder.state.workspace==EditorWorkspace::Project ? "Configurações do projeto" :
      builder.state.workspace==EditorWorkspace::Assets ? "Recursos importados" : "Edição";
  if(content.width>80) builder.label(content,context,theme.color.textDim,theme.type.caption);

}

// Janela de cor (Unity 6000.0 Manual/InspectorColorPicker), para triples de
// cor dos componentes e campos Color dos scripts. À esquerda o quadrado de
// saturação × valor e a faixa de matiz, contínuos; à direita a cor original
// (tocar volta a ela) ao lado da nova, as barras do modo escolhido, alfa e
// intensidade HDR quando o campo as tem, o hexadecimal e, embaixo, as amostras
// da biblioteca ativa. Nada grava antes de Aplicar.
void buildColorWindow(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
  const UiRect panel=centred(state.surface,std::min(640.f,state.surface.width-16),std::min(380.f,state.surface.height-16));
  list.addRect(panel,theme.color.surface,theme.radius.control);
  auto content=deflate(panel,UiInsets::all(10));
  float srgb[3];pickerRgb(state.colorHue,state.colorSaturation,state.colorValue,srgb);
  const UiColor current=pickerColor(state.colorHue,state.colorSaturation,state.colorValue);
  // Cabeçalho: título, abas de modo e ações.
  auto header=takeTop(content,34);
  builder.label(takeLeft(header,110),state.colorHdr?"Cor HDR":"Cor",theme.color.text,theme.type.title);
  const auto apply=takeRight(header,92);takeRight(header,6);const auto cancel=takeRight(header,92);
  list.addRect(apply,theme.color.accent,theme.radius.control);
  builder.label(apply,"Aplicar",theme.color.accentInk,theme.type.caption,UiAlign::Center);router.addRegion(apply,widgetId(EditorWidget::ColorApply));
  list.addRect(cancel,theme.color.raised,theme.radius.control);
  builder.label(cancel,"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);router.addRegion(cancel,widgetId(EditorWidget::ColorCancel));
  takeRight(header,10);
  const char *modes[]{"RGB 0-255","RGB 0-1","HSV"};
  const float tab=std::min(84.f,header.width/3);
  for(u32 i=0;i<3;++i) {
    const UiRect cell{header.x+i*tab,header.y+3,tab-4,header.height-6};
    const bool on=state.colorMode==i;
    list.addRect(cell,on?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    builder.label(cell,modes[i],on?theme.color.accent:theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(cell,widgetId(EditorWidget::ColorModeBase)+i);
  }
  takeTop(content,8);
  // Faixa de amostras embaixo.
  auto swatches=takeBottom(content,40);takeBottom(content,6);
  // Esquerda: quadrado SV e matiz.
  const float side=std::min(content.height,content.width*.42f);
  auto left=takeLeft(content,side+36);takeLeft(content,12);
  const UiRect square{left.x,left.y,side,side};
  const UiRect hue{square.right()+10,left.y,24,side};
  constexpr u32 steps=24;
  for(u32 y=0;y<steps;++y) for(u32 x=0;x<steps;++x) {
    const UiRect cell{square.x+x*side/steps,square.y+y*side/steps,side/steps+1,side/steps+1};
    list.addRect(cell,pickerColor(state.colorHue,(x+.5f)/steps,1-(y+.5f)/steps),0);
  }
  const UiPoint mark{square.x+state.colorSaturation*side,square.y+(1-state.colorValue)*side};
  list.addRect({mark.x-7,mark.y-7,14,14},0xffffffff,7);
  list.addRect({mark.x-5,mark.y-5,10,10},current,5);
  for(u32 i=0;i<36;++i) list.addRect({hue.x,hue.y+i*side/36,hue.width,side/36+1},pickerColor(i/36.f,1,1),0);
  const float hueY=hue.y+state.colorHue*side;
  list.addRect({hue.x-3,hueY-2,hue.width+6,4},0xffffffff,2);
  router.addRegion(square,widgetId(EditorWidget::ColorSquare));
  router.addRegion(hue,widgetId(EditorWidget::ColorHueStrip));
  layout.colorSquare=square;layout.colorHue=hue;
  // Direita: conta-gotas, original × nova, barras, hexadecimal.
  auto preview=takeTop(content,40);
  builder.iconButton(takeLeft(preview,40),UiIcon::EditorEyedropper,widgetId(EditorWidget::ColorEyedropper));
  takeLeft(preview,6);
  const auto original=takeLeft(preview,preview.width*.5f);
  float originalSrgb[3],originalBase[3],originalIntensity=0;
  splitHdr({state.colorOriginal[0],state.colorOriginal[1],state.colorOriginal[2]},originalBase,originalIntensity);
  for(u32 i=0;i<3;++i) originalSrgb[i]=colorToSrgb(originalBase[i]);
  float h=0,sat=0,val=0;srgbToHsv(originalSrgb,h,sat,val);
  list.addRect(original,pickerColor(h,sat,val),theme.radius.control);
  builder.label(deflate(original,UiInsets{6,0,6,0}),"Original",val>.6f?0xff101010u:0xffffffffu,theme.type.caption);
  router.addRegion(original,widgetId(EditorWidget::ColorOriginal));
  list.addRect(preview,current,theme.radius.control);
  builder.label(deflate(preview,UiInsets{6,0,6,0}),"Nova",state.colorValue>.6f?0xff101010u:0xffffffffu,theme.type.caption,UiAlign::End);
  takeTop(content,6);
  struct Slider {const char *label;float value;float minimum,maximum;int kind;};
  std::vector<Slider> sliders;
  if(state.colorMode==2) {
    sliders.push_back({"H",state.colorHue,0,1,3});sliders.push_back({"S",state.colorSaturation,0,1,4});
    sliders.push_back({"V",state.colorValue,0,1,5});
  } else {
    sliders.push_back({"R",srgb[0],0,1,0});sliders.push_back({"G",srgb[1],0,1,1});sliders.push_back({"B",srgb[2],0,1,2});
  }
  if(state.colorHasAlpha) sliders.push_back({"A",state.colorAlpha,0,1,6});
  if(state.colorHdr) sliders.push_back({"Int.",state.colorIntensity,-10,10,7});
  const float rowHeight=std::min(30.f,(content.height-34)/std::max<usize>(1,sliders.size()));
  for(usize i=0;i<sliders.size() && i<5;++i) {
    auto row=takeTop(content,rowHeight);
    const auto &slider=sliders[i];
    builder.label(takeLeft(row,34),slider.label,theme.color.textDim,theme.type.caption);
    const auto valueBox=takeRight(row,58);
    auto bar=deflate(row,UiInsets{4,rowHeight*.25f,6,rowHeight*.25f});
    // O fundo da barra mostra o que cada posição daria.
    constexpr u32 segments=24;
    for(u32 k=0;k<segments;++k) {
      const float t=(k+.5f)/segments;float c[3]{srgb[0],srgb[1],srgb[2]};UiColor color=0;
      switch(slider.kind) {
        case 0:case 1:case 2: c[slider.kind]=t;color=0xff000000u|static_cast<u32>(c[0]*255+.5f)<<16|static_cast<u32>(c[1]*255+.5f)<<8|static_cast<u32>(c[2]*255+.5f);break;
        case 3: color=pickerColor(t,1,1);break;
        case 4: color=pickerColor(state.colorHue,t,state.colorValue);break;
        case 5: color=pickerColor(state.colorHue,state.colorSaturation,t);break;
        case 6: color=pickerColor(state.colorHue,state.colorSaturation,state.colorValue,t);break;
        default: {const float g=std::clamp(.5f+(t-.5f)*.8f,0.f,1.f);color=pickerColor(state.colorHue,state.colorSaturation*.5f,g);}
      }
      list.addRect({bar.x+k*bar.width/segments,bar.y,bar.width/segments+1,bar.height},color,0);
    }
    const float t=(slider.value-slider.minimum)/(slider.maximum-slider.minimum);
    list.addRect({bar.x+t*bar.width-2,bar.y-3,4,bar.height+6},0xffffffff,2);
    char text[24];
    if(slider.kind==3) std::snprintf(text,sizeof(text),"%.0f°",slider.value*360);
    else if(slider.kind==4||slider.kind==5) std::snprintf(text,sizeof(text),"%.0f%%",slider.value*100);
    else if(slider.kind==7) std::snprintf(text,sizeof(text),"%+.2f",slider.value);
    else if(state.colorMode==0) std::snprintf(text,sizeof(text),"%.0f",slider.value*255);
    else std::snprintf(text,sizeof(text),"%.3f",slider.value);
    builder.label(valueBox,text,theme.color.text,theme.type.caption,UiAlign::End);
    const UiRect hit{bar.x,row.y,bar.width,row.height};
    router.addRegion(hit,widgetId(EditorWidget::ColorSliderBase)+static_cast<u32>(i));
    layout.colorSliders[i]=bar;
  }
  auto hex=takeTop(content,30);
  builder.label(takeLeft(hex,70),"Hexadecimal",theme.color.textDim,theme.type.caption);
  list.addRect(hex,theme.color.raised,theme.radius.control);
  const auto hexText="#"+formatColorHex(srgb,state.colorAlpha,state.colorHasAlpha);
  builder.label(deflate(hex,UiInsets{8,0,8,0}),hexText,theme.color.text,theme.type.caption);
  router.addRegion(hex,widgetId(EditorWidget::ColorHex));
  // Amostras: biblioteca ativa, cores salvas e "+" para guardar a atual.
  const auto *libraries=state.colorLibraries;
  const auto *library=libraries?libraries->currentOrNull():nullptr;
  const auto chip=takeLeft(swatches,120);takeLeft(swatches,6);
  list.addRect(chip,state.colorLibraryMenu?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
  builder.label(deflate(chip,UiInsets{8,0,8,0}),library?library->name:std::string("Padrão"),theme.color.text,theme.type.caption);
  router.addRegion(chip,widgetId(EditorWidget::ColorLibraryToggle));
  const auto add=takeRight(swatches,40);
  list.addRect(add,withAlpha(theme.color.accent,.16f),theme.radius.control);
  builder.label(add,"+",theme.color.accent,theme.type.title,UiAlign::Center);
  router.addRegion(add,widgetId(EditorWidget::ColorSwatchAdd));
  takeRight(swatches,6);
  const u32 capacity=static_cast<u32>(std::max(0.f,swatches.width)/36);
  if(library) for(u32 i=0;i<library->entries.size() && i<capacity;++i) {
    const UiRect cell{swatches.x+i*36.f,swatches.y+4,32,32};
    float rgba[4]{1,1,1,1};scene::parseScriptColor(library->entries[i].value,rgba);
    float base[3],intensity=0;splitHdr({rgba[0],rgba[1],rgba[2]},base,intensity);
    float c[3];for(u32 k=0;k<3;++k) c[k]=colorToSrgb(base[k]);
    float sh=0,ss=0,sv=0;srgbToHsv(c,sh,ss,sv);
    if(state.colorSwatchMenu==i+1) list.addRect(deflate(cell,UiInsets::all(-3)),theme.color.accent,theme.radius.control);
    list.addRect(cell,pickerColor(sh,ss,sv),theme.radius.control);
    router.addRegion(cell,widgetId(EditorWidget::ColorSwatchBase)+i);
  }
  if(!library || library->entries.empty())
    builder.label(swatches,"Sem amostras: + guarda a cor atual",theme.color.textMuted,theme.type.caption);
  // Menus sobre a faixa: bibliotecas ou ações da amostra (toque longo).
  if(state.colorLibraryMenu && libraries) {
    const float height=36.f*(libraries->libraries.size()+1)+8;
    const UiRect menu{chip.x,std::max(panel.y+8,chip.y-height-4),200,height};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    for(u32 i=0;i<libraries->libraries.size();++i) {
      const auto row=takeTop(rows,36);
      if(i==libraries->active) list.addRect({row.x,row.y+6,3,row.height-12},theme.color.accent,1);
      builder.label(deflate(row,UiInsets{10,0,6,0}),libraries->libraries[i].name,theme.color.text,theme.type.caption);
      router.addRegion(row,widgetId(EditorWidget::ColorLibraryBase)+i);
    }
    const auto create=takeTop(rows,36);
    builder.label(deflate(create,UiInsets{10,0,6,0}),"+ Nova biblioteca",theme.color.accent,theme.type.caption);
    router.addRegion(create,widgetId(EditorWidget::ColorLibraryNew));
  }
  if(state.colorSwatchMenu && library && state.colorSwatchMenu<=library->entries.size()) {
    const u32 i=state.colorSwatchMenu-1;
    const char *actions[]{"Substituir pela atual","Mover para a esquerda","Mover para a direita","Renomear","Apagar"};
    const UiRect menu{std::min(swatches.x+i*36.f,panel.right()-220),std::max(panel.y+8,swatches.y-5*34.f-30),210,5*34.f+26};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    builder.label(takeTop(rows,22),library->entries[i].name,theme.color.textDim,theme.type.caption);
    for(u32 k=0;k<5;++k) {
      const auto row=takeTop(rows,34);
      builder.label(deflate(row,UiInsets{8,0,6,0}),actions[k],k==4?theme.color.axisX:theme.color.text,theme.type.caption);
      router.addRegion(row,widgetId(EditorWidget::ColorSwatchActionBase)+k);
    }
  }
}

// Faixa de gradiente em fatias. Em cima a cor com alfa sobre xadrez; embaixo
// a cor opaca, para o alfa baixo não esconder qual é a cor.
void drawGradientBar(ScreenBuilder &builder,UiRect bar,const scene::ScriptGradient &gradient,u32 slices=48) {
  auto &list=builder.list;
  const float top=bar.height*.62f;
  for(u32 k=0;k<slices;++k) {
    const float x=bar.x+k*bar.width/slices,w=bar.width/slices+1;
    const float checker=std::max(4.f,top*.5f);
    for(float y=0;y<top;y+=checker) {
      const bool dark=(static_cast<u32>((x-bar.x)/checker)+static_cast<u32>(y/checker))%2;
      list.addRect({x,bar.y+y,w,std::min(checker,top-y)},dark?0xff8c8c8cu:0xffcfcfcfu,0);
    }
    float rgba[4];scene::evaluateScriptGradient(gradient,(k+.5f)/slices,rgba);
    float base[3],intensity=0;splitHdr({rgba[0],rgba[1],rgba[2]},base,intensity);
    float srgb[3];for(u32 i=0;i<3;++i) srgb[i]=colorToSrgb(base[i]);
    float h=0,s=0,v=0;srgbToHsv(srgb,h,s,v);
    list.addRect({x,bar.y,w,top},pickerColor(h,s,v,rgba[3]),0);
    list.addRect({x,bar.y+top,w,bar.height-top},pickerColor(h,s,v),0);
  }
}

// Editor de gradiente (Unity 6000.0 Manual/InspectorColorPicker, "Gradient
// Editor"). Paradas de alfa em cima e de cor embaixo da faixa: tocar numa
// parada escolhe, arrastar move, arrastar para longe da faixa apaga, tocar no
// vazio de uma faixa acrescenta. A parada escolhida mostra cor (abre a janela
// de cor) ou alfa e a posição. Modos Blend, Perceptual e Fixed; presets em
// bibliotecas do projeto. Nada grava antes de Aplicar.
void buildGradientEditor(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  scene::ScriptGradient gradient;
  if(!scene::parseScriptGradient(state.gradientDraft,gradient)) return;
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
  const UiRect panel=centred(state.surface,std::min(640.f,state.surface.width-16),std::min(360.f,state.surface.height-16));
  list.addRect(panel,theme.color.surface,theme.radius.control);
  auto content=deflate(panel,UiInsets::all(10));
  auto header=takeTop(content,34);
  builder.label(takeLeft(header,130),state.gradientType=="gradient:hdr"?"Gradiente HDR":"Gradiente",theme.color.text,theme.type.title);
  const auto apply=takeRight(header,92);takeRight(header,6);const auto cancel=takeRight(header,92);
  list.addRect(apply,theme.color.accent,theme.radius.control);
  builder.label(apply,"Aplicar",theme.color.accentInk,theme.type.caption,UiAlign::Center);router.addRegion(apply,widgetId(EditorWidget::GradientApply));
  list.addRect(cancel,theme.color.raised,theme.radius.control);
  builder.label(cancel,"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);router.addRegion(cancel,widgetId(EditorWidget::GradientCancel));
  takeRight(header,10);
  const char *modes[]{"Blend","Fixed","Perceptual"};
  const float tab=std::min(92.f,header.width/3);
  for(u32 i=0;i<3;++i) {
    const UiRect cell{header.x+i*tab,header.y+3,tab-4,header.height-6};
    const bool on=static_cast<u32>(gradient.mode)==i;
    list.addRect(cell,on?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    builder.label(cell,modes[i],on?theme.color.accent:theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(cell,widgetId(EditorWidget::GradientModeBase)+i);
  }
  takeTop(content,6);
  auto presets=takeBottom(content,44);takeBottom(content,6);
  auto detail=takeBottom(content,36);takeBottom(content,10);
  // Faixa de alfa, barra (o espaço que sobrar) e faixa de cor.
  const auto alphaLane=takeTop(content,30);
  const auto colorLane=takeBottom(content,30);
  const auto bar=content;
  const UiRect track{bar.x+14,bar.y,bar.width-28,bar.height};
  list.addRect(deflate(bar,UiInsets{10,-2,10,-2}),theme.color.line,theme.radius.control);
  drawGradientBar(builder,track,gradient);
  layout.gradientBar=track;
  router.addRegion({track.x-14,alphaLane.y,track.width+28,alphaLane.height},widgetId(EditorWidget::GradientAlphaLane));
  router.addRegion({track.x-14,colorLane.y,track.width+28,colorLane.height},widgetId(EditorWidget::GradientColorLane));
  const auto marker=[&](float time,bool up,UiColor fill,bool selected,bool removing,u32 widget,float laneY,float laneH) {
    const float x=track.x+time*track.width;
    const UiRect box{x-11,laneY+(up?2.f:6.f),22,laneH-8};
    list.addRect(deflate(box,UiInsets::all(-2)),removing?theme.color.axisX:selected?theme.color.accent:theme.color.textMuted,4);
    list.addRect(box,fill,3);
    // O bico aponta para a barra.
    list.addRect({x-1.5f,up?box.bottom():laneY,3,up?laneY+laneH-box.bottom():box.y-laneY},selected?theme.color.accent:theme.color.textMuted,1);
    router.addRegion(deflate(box,UiInsets::all(-6)),widget);
  };
  for(u32 i=0;i<gradient.alphas.size();++i) {
    const float a=gradient.alphas[i].alpha;const u32 g=static_cast<u32>(a*255+.5f);
    const bool selected=state.gradientSelectedAlpha && state.gradientSelected==i+1;
    marker(gradient.alphas[i].time,true,0xff000000u|g<<16|g<<8|g,selected,selected&&state.gradientRemoving,
           widgetId(EditorWidget::GradientAlphaStopBase)+i,alphaLane.y,alphaLane.height);
  }
  for(u32 i=0;i<gradient.colors.size();++i) {
    float base[3],intensity=0;splitHdr(gradient.colors[i].rgb,base,intensity);
    float srgb[3];for(u32 k=0;k<3;++k) srgb[k]=colorToSrgb(base[k]);
    float h=0,s=0,v=0;srgbToHsv(srgb,h,s,v);
    const bool selected=!state.gradientSelectedAlpha && state.gradientSelected==i+1;
    marker(gradient.colors[i].time,false,pickerColor(h,s,v),selected,selected&&state.gradientRemoving,
           widgetId(EditorWidget::GradientColorStopBase)+i,colorLane.y,colorLane.height);
  }
  // Painel da parada escolhida.
  const bool alpha=state.gradientSelectedAlpha;
  const bool any=state.gradientSelected && state.gradientSelected<=(alpha?gradient.alphas.size():gradient.colors.size());
  if(!any) builder.label(detail,"Toque numa parada para editar; toque no vazio de uma faixa para acrescentar",theme.color.textMuted,theme.type.caption);
  else {
    const u32 i=state.gradientSelected-1;
    const float time=alpha?gradient.alphas[i].time:gradient.colors[i].time;
    auto value=takeLeft(detail,detail.width*.42f);takeLeft(detail,10);
    if(alpha) {
      builder.label(takeLeft(value,40),"Alfa",theme.color.textDim,theme.type.caption);
      const auto slider=deflate(takeLeft(value,value.width-44),UiInsets{0,12,6,12});
      list.addRect(slider,theme.color.raised,theme.radius.control);
      list.addRect({slider.x,slider.y,slider.width*gradient.alphas[i].alpha,slider.height},theme.color.accent,theme.radius.control);
      char text[16];std::snprintf(text,sizeof(text),"%.0f",gradient.alphas[i].alpha*255);
      builder.label(value,text,theme.color.text,theme.type.caption,UiAlign::End);
      router.addRegion({slider.x,value.y,slider.width,value.height},widgetId(EditorWidget::GradientAlphaValue));
      layout.gradientAlpha=slider;
    } else {
      builder.label(takeLeft(value,40),"Cor",theme.color.textDim,theme.type.caption);
      const auto swatch=deflate(value,UiInsets{0,4,0,4});
      float base[3],intensity=0;splitHdr(gradient.colors[i].rgb,base,intensity);
      float srgb[3];for(u32 k=0;k<3;++k) srgb[k]=colorToSrgb(base[k]);
      float h=0,s=0,v=0;srgbToHsv(srgb,h,s,v);
      list.addRect(swatch,pickerColor(h,s,v),theme.radius.control);
      builder.label(deflate(swatch,UiInsets{8,0,8,0}),"#"+formatColorHex(srgb,1,false),v>.6f?0xff101010u:0xffffffffu,theme.type.caption);
      router.addRegion(swatch,widgetId(EditorWidget::GradientColorSwatch));
    }
    const auto remove=takeRight(detail,86);
    const bool removable=(alpha?gradient.alphas.size():gradient.colors.size())>1;
    list.addRect(remove,theme.color.raised,theme.radius.control);
    builder.label(remove,"Apagar",removable?theme.color.axisX:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    if(removable) router.addRegion(remove,widgetId(EditorWidget::GradientDeleteStop));
    takeRight(detail,8);
    builder.label(takeLeft(detail,64),"Posição",theme.color.textDim,theme.type.caption);
    const auto valueBox=takeRight(detail,48);
    const auto slider=deflate(detail,UiInsets{0,12,6,12});
    list.addRect(slider,theme.color.raised,theme.radius.control);
    list.addRect({slider.x+time*slider.width-2,slider.y-4,4,slider.height+8},theme.color.accent,2);
    char text[16];std::snprintf(text,sizeof(text),"%.1f%%",time*100);
    builder.label(valueBox,text,theme.color.text,theme.type.caption,UiAlign::End);
    router.addRegion({slider.x,detail.y,slider.width,detail.height},widgetId(EditorWidget::GradientLocation));
    layout.gradientLocation=slider;
  }
  // Presets.
  const auto *libraries=state.gradientLibraries;
  const auto *library=libraries?libraries->currentOrNull():nullptr;
  const auto chip=takeLeft(presets,120);takeLeft(presets,6);
  list.addRect(chip,state.gradientLibraryMenu?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
  builder.label(deflate(chip,UiInsets{8,0,8,0}),library?library->name:std::string("Padrão"),theme.color.text,theme.type.caption);
  router.addRegion(chip,widgetId(EditorWidget::GradientLibraryToggle));
  const auto add=takeRight(presets,40);
  list.addRect(add,withAlpha(theme.color.accent,.16f),theme.radius.control);
  builder.label(add,"+",theme.color.accent,theme.type.title,UiAlign::Center);
  router.addRegion(add,widgetId(EditorWidget::GradientPresetAdd));
  takeRight(presets,6);
  const u32 capacity=static_cast<u32>(std::max(0.f,presets.width)/64);
  if(library) for(u32 i=0;i<library->entries.size() && i<capacity;++i) {
    const UiRect cell{presets.x+i*64.f,presets.y+6,58,32};
    scene::ScriptGradient preset;
    if(!scene::parseScriptGradient(library->entries[i].value,preset)) continue;
    if(state.gradientPresetMenu==i+1) list.addRect(deflate(cell,UiInsets::all(-3)),theme.color.accent,theme.radius.control);
    drawGradientBar(builder,cell,preset,16);
    router.addRegion(cell,widgetId(EditorWidget::GradientPresetBase)+i);
  }
  if(!library || library->entries.empty())
    builder.label(presets,"Sem presets: + guarda o gradiente atual",theme.color.textMuted,theme.type.caption);
  if(state.gradientLibraryMenu && libraries) {
    const float height=36.f*(libraries->libraries.size()+1)+8;
    const UiRect menu{chip.x,std::max(panel.y+8,chip.y-height-4),200,height};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    for(u32 i=0;i<libraries->libraries.size();++i) {
      const auto row=takeTop(rows,36);
      if(i==libraries->active) list.addRect({row.x,row.y+6,3,row.height-12},theme.color.accent,1);
      builder.label(deflate(row,UiInsets{10,0,6,0}),libraries->libraries[i].name,theme.color.text,theme.type.caption);
      router.addRegion(row,widgetId(EditorWidget::GradientLibraryBase)+i);
    }
    const auto create=takeTop(rows,36);
    builder.label(deflate(create,UiInsets{10,0,6,0}),"+ Nova biblioteca",theme.color.accent,theme.type.caption);
    router.addRegion(create,widgetId(EditorWidget::GradientLibraryNew));
  }
  if(state.gradientPresetMenu && library && state.gradientPresetMenu<=library->entries.size()) {
    const u32 i=state.gradientPresetMenu-1;
    const char *actions[]{"Substituir pelo atual","Mover para a esquerda","Mover para a direita","Renomear","Apagar"};
    const UiRect menu{std::min(presets.x+i*64.f,panel.right()-220),std::max(panel.y+8,presets.y-5*34.f-30),210,5*34.f+26};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    builder.label(takeTop(rows,22),library->entries[i].name,theme.color.textDim,theme.type.caption);
    for(u32 k=0;k<5;++k) {
      const auto row=takeTop(rows,34);
      builder.label(deflate(row,UiInsets{8,0,6,0}),actions[k],k==4?theme.color.axisX:theme.color.text,theme.type.caption);
      router.addRegion(row,widgetId(EditorWidget::GradientPresetActionBase)+k);
    }
  }
}

// Curva desenhada em linha dentro de um retângulo: miniatura de campo e de preset.
void drawCurveThumbnail(ScreenBuilder &builder,UiRect box,const scene::ScriptCurve &curve,UiColor color) {
  float view[4];frameCurve(curve,view);
  if(curve.keys.empty()) return;
  UiPoint previous{};
  for(u32 i=0;i<=24;++i) {
    const float t=view[0]+(view[2]-view[0])*i/24.f;
    const auto point=curveToScreen(box,view,t,scene::evaluateScriptCurve(curve,t));
    if(i) builder.list.addLine(previous,point,color,1.5f);
    previous=point;
  }
}

// Editor de curvas (Unity 6000.0 Manual/EditingCurves e InspectorCurves).
// Toque duplo no gráfico acrescenta chave sobre a curva; arrastar uma chave
// move tempo e valor; a chave escolhida mostra as alças de tangente
// (arrastar muda a inclinação) e o painel com tempo, valor, modo de tangente
// e Apagar. Arrastar o vazio desloca a vista; Enquadrar, + e − ajustam.
// Presets em bibliotecas do projeto, com os de fábrica. Nada grava antes de
// Aplicar.
void buildCurveEditor(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  scene::ScriptCurve curve;
  if(!scene::parseScriptCurve(state.curveDraft,curve)) return;
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
  const UiRect panel=centred(state.surface,std::min(700.f,state.surface.width-16),std::min(380.f,state.surface.height-16));
  list.addRect(panel,theme.color.surface,theme.radius.control);
  auto content=deflate(panel,UiInsets::all(10));
  auto header=takeTop(content,34);
  builder.label(takeLeft(header,90),"Curva",theme.color.text,theme.type.title);
  const auto button=[&](UiRect rect,const char *label,EditorWidget widget,bool primary=false) {
    list.addRect(rect,primary?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(rect,label,primary?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(rect,widgetId(widget));
  };
  button(takeRight(header,88),"Aplicar",EditorWidget::CurveApply,true);takeRight(header,6);
  button(takeRight(header,88),"Cancelar",EditorWidget::CurveCancel);takeRight(header,14);
  button(takeRight(header,40),"+",EditorWidget::CurveZoomIn);takeRight(header,4);
  button(takeRight(header,40),"-",EditorWidget::CurveZoomOut);takeRight(header,4);
  button(takeRight(header,92),"Enquadrar",EditorWidget::CurveFrame);
  takeTop(content,6);
  auto presets=takeBottom(content,44);takeBottom(content,6);
  const bool selected=state.curveSelected && state.curveSelected<=curve.keys.size();
  const bool broken=selected && curve.keys[state.curveSelected-1].broken;
  auto panelRows=takeBottom(content,broken?72.f:36.f);takeBottom(content,8);
  // Gráfico.
  const UiRect graph=deflate(content,UiInsets{34,4,4,16});
  list.addRect(content,theme.color.voidBlack,theme.radius.control);
  layout.curveGraph=graph;
  router.addRegion(content,widgetId(EditorWidget::CurveGraph));
  const auto &view=state.curveView;
  for(u32 i=0;i<=4;++i) {
    const float t=view[0]+(view[2]-view[0])*i/4.f,v=view[1]+(view[3]-view[1])*i/4.f;
    const auto x=curveToScreen(graph,view,t,view[1]).x,y=curveToScreen(graph,view,view[0],v).y;
    list.addRect({x,graph.y,1,graph.height},theme.color.line);
    list.addRect({graph.x,y,graph.width,1},theme.color.line);
    char text[16];
    std::snprintf(text,sizeof(text),"%.3g",t);
    builder.label({x-24,graph.bottom()+1,48,14},text,theme.color.textMuted,theme.type.caption,UiAlign::Center);
    std::snprintf(text,sizeof(text),"%.3g",v);
    builder.label({content.x+2,y-7,30,14},text,theme.color.textMuted,theme.type.caption,UiAlign::End);
  }
  list.pushClip(graph);
  if(curve.keys.empty())
    builder.label(graph,"Curva vazia · toque duplo acrescenta a primeira chave",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  else {
    // Dentro das chaves em destaque; fora, a repetição (Clamp/Loop/PingPong) apagada.
    const float first=curve.keys.front().time,last=curve.keys.back().time;
    UiPoint previous{};bool started=false;
    constexpr u32 samples=160;
    for(u32 i=0;i<=samples;++i) {
      const float t=view[0]+(view[2]-view[0])*i/samples;
      const auto point=curveToScreen(graph,view,t,scene::evaluateScriptCurve(curve,t));
      if(started) {
        const bool inside=t>=first && t<=last;
        list.addLine(previous,point,inside?theme.color.accent:withAlpha(theme.color.accent,.35f),inside?2.f:1.5f);
      }
      previous=point;started=true;
    }
    for(u32 i=0;i<curve.keys.size();++i) {
      const auto &key=curve.keys[i];
      const auto at=curveToScreen(graph,view,key.time,key.value);
      const bool chosen=state.curveSelected==i+1;
      if(chosen) {
        for(const bool incoming:{true,false}) {
          const auto mode=incoming?key.left:key.right;
          if((incoming && i==0) || (!incoming && i+1==curve.keys.size())) continue;
          if(mode==scene::CurveTangentMode::Linear || mode==scene::CurveTangentMode::Constant) continue;
          const auto handle=curveHandle(graph,view,key,incoming);
          list.addLine(at,handle,theme.color.textDim,1.5f);
          list.addRect({handle.x-6,handle.y-6,12,12},theme.color.textDim,6);
        }
        list.addRect({at.x-9,at.y-9,18,18},theme.color.accent,9);
      }
      list.addRect({at.x-6,at.y-6,12,12},chosen?0xffffffffu:theme.color.text,6);
    }
  }
  list.popClip();
  // Painel: chave escolhida ou repetição.
  auto row=takeTop(panelRows,36);
  const auto chip=[&](UiRect rect,const char *label,u32 widget,bool on) {
    list.addRect(rect,on?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    builder.label(rect,label,on?theme.color.accent:theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(rect,widget);
  };
  if(!selected) {
    const char *wraps[]{"Clamp","Loop","PingPong"};
    builder.label(takeLeft(row,52),"Antes",theme.color.textDim,theme.type.caption);
    for(u32 i=0;i<3;++i) chip(deflate(takeLeft(row,78),UiInsets{2,2,2,2}),wraps[i],widgetId(EditorWidget::CurvePreWrapBase)+i,static_cast<u32>(curve.pre)==i);
    takeLeft(row,14);
    builder.label(takeLeft(row,56),"Depois",theme.color.textDim,theme.type.caption);
    for(u32 i=0;i<3;++i) chip(deflate(takeLeft(row,78),UiInsets{2,2,2,2}),wraps[i],widgetId(EditorWidget::CurvePostWrapBase)+i,static_cast<u32>(curve.post)==i);
    takeLeft(row,10);
    builder.label(row,"Toque duplo acrescenta chave",theme.color.textMuted,theme.type.caption);
  } else {
    const auto &key=curve.keys[state.curveSelected-1];
    const auto field=[&](const char *label,float value,EditorWidget widget) {
      builder.label(takeLeft(row,44),label,theme.color.textDim,theme.type.caption);
      const auto box=deflate(takeLeft(row,72),UiInsets{0,2,4,2});
      list.addRect(box,theme.color.raised,theme.radius.control);
      char text[24];std::snprintf(text,sizeof(text),"%.4g",value);
      builder.label(deflate(box,UiInsets{6,0,6,0}),text,theme.color.text,theme.type.caption);
      router.addRegion(box,widgetId(widget));
    };
    field("Tempo",key.time,EditorWidget::CurveKeyTime);
    field("Valor",key.value,EditorWidget::CurveKeyValue);
    const auto remove=takeRight(row,78);
    list.addRect(deflate(remove,UiInsets{0,2,0,2}),theme.color.raised,theme.radius.control);
    builder.label(remove,"Apagar",theme.color.axisX,theme.type.caption,UiAlign::Center);
    router.addRegion(remove,widgetId(EditorWidget::CurveKeyDelete));
    takeRight(row,6);
    // Modos: os quatro alinhados e "Quebrada".
    const bool smooth=!key.broken && key.left==key.right;
    const struct {const char *label;bool on;} modes[]{
      {"Auto suave",smooth&&key.left==scene::CurveTangentMode::ClampedAuto},{"Auto",smooth&&key.left==scene::CurveTangentMode::Auto},
      {"Livre",smooth&&key.left==scene::CurveTangentMode::Free&&(key.in!=0||key.out!=0)},
      {"Plana",smooth&&key.left==scene::CurveTangentMode::Free&&key.in==0&&key.out==0},{"Quebrada",key.broken}};
    const float width=row.width/5;
    for(u32 i=0;i<5;++i) chip({row.x+i*width+2,row.y+2,width-4,row.height-4},modes[i].label,widgetId(EditorWidget::CurveTangentBase)+i,modes[i].on);
    if(key.broken) {
      auto sides=takeTop(panelRows,36);
      const char *sideModes[]{"Livre","Linear","Constante"};
      const auto sideIndex=[](scene::CurveTangentMode mode){return mode==scene::CurveTangentMode::Linear?1u:mode==scene::CurveTangentMode::Constant?2u:0u;};
      builder.label(takeLeft(sides,70),"Esquerda",theme.color.textDim,theme.type.caption);
      for(u32 i=0;i<3;++i) chip(deflate(takeLeft(sides,84),UiInsets{2,2,2,2}),sideModes[i],widgetId(EditorWidget::CurveLeftModeBase)+i,sideIndex(key.left)==i);
      takeLeft(sides,16);
      builder.label(takeLeft(sides,64),"Direita",theme.color.textDim,theme.type.caption);
      for(u32 i=0;i<3;++i) chip(deflate(takeLeft(sides,84),UiInsets{2,2,2,2}),sideModes[i],widgetId(EditorWidget::CurveRightModeBase)+i,sideIndex(key.right)==i);
    }
  }
  // Presets.
  const auto *libraries=state.curveLibraries;
  const auto *library=libraries?libraries->currentOrNull():nullptr;
  const auto libraryChip=takeLeft(presets,120);takeLeft(presets,6);
  list.addRect(libraryChip,state.curveLibraryMenu?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
  builder.label(deflate(libraryChip,UiInsets{8,0,8,0}),library?library->name:std::string("Padrão"),theme.color.text,theme.type.caption);
  router.addRegion(libraryChip,widgetId(EditorWidget::CurveLibraryToggle));
  const auto add=takeRight(presets,40);
  list.addRect(add,withAlpha(theme.color.accent,.16f),theme.radius.control);
  builder.label(add,"+",theme.color.accent,theme.type.title,UiAlign::Center);
  router.addRegion(add,widgetId(EditorWidget::CurvePresetAdd));
  takeRight(presets,6);
  const u32 capacity=static_cast<u32>(std::max(0.f,presets.width)/64);
  if(library) for(u32 i=0;i<library->entries.size() && i<capacity;++i) {
    const UiRect cell{presets.x+i*64.f,presets.y+4,58,36};
    scene::ScriptCurve preset;
    if(!scene::parseScriptCurve(library->entries[i].value,preset)) continue;
    list.addRect(cell,state.curvePresetMenu==i+1?withAlpha(theme.color.accent,.30f):theme.color.raised,theme.radius.control);
    drawCurveThumbnail(builder,deflate(cell,UiInsets::all(5)),preset,theme.color.accent);
    router.addRegion(cell,widgetId(EditorWidget::CurvePresetBase)+i);
  }
  if(!library || library->entries.empty())
    builder.label(presets,"Sem presets: + guarda a curva, ou use os de fábrica",theme.color.textMuted,theme.type.caption);
  if(state.curveLibraryMenu && libraries) {
    const float height=36.f*(libraries->libraries.size()+2)+8;
    const UiRect menu{libraryChip.x,std::max(panel.y+8,libraryChip.y-height-4),230,height};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    for(u32 i=0;i<libraries->libraries.size();++i) {
      const auto entry=takeTop(rows,36);
      if(i==libraries->active) list.addRect({entry.x,entry.y+6,3,entry.height-12},theme.color.accent,1);
      builder.label(deflate(entry,UiInsets{10,0,6,0}),libraries->libraries[i].name,theme.color.text,theme.type.caption);
      router.addRegion(entry,widgetId(EditorWidget::CurveLibraryBase)+i);
    }
    const auto create=takeTop(rows,36);
    builder.label(deflate(create,UiInsets{10,0,6,0}),"+ Nova biblioteca",theme.color.accent,theme.type.caption);
    router.addRegion(create,widgetId(EditorWidget::CurveLibraryNew));
    const auto factory=takeTop(rows,36);
    builder.label(deflate(factory,UiInsets{10,0,6,0}),"+ Presets de fábrica",theme.color.accent,theme.type.caption);
    router.addRegion(factory,widgetId(EditorWidget::CurveLibraryFactory));
  }
  if(state.curvePresetMenu && library && state.curvePresetMenu<=library->entries.size()) {
    const u32 i=state.curvePresetMenu-1;
    const char *actions[]{"Substituir pela atual","Mover para a esquerda","Mover para a direita","Renomear","Apagar"};
    const UiRect menu{std::min(presets.x+i*64.f,panel.right()-220),std::max(panel.y+8,presets.y-5*34.f-30),210,5*34.f+26};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    builder.label(takeTop(rows,22),library->entries[i].name,theme.color.textDim,theme.type.caption);
    for(u32 k=0;k<5;++k) {
      const auto entry=takeTop(rows,34);
      builder.label(deflate(entry,UiInsets{8,0,6,0}),actions[k],k==4?theme.color.axisX:theme.color.text,theme.type.caption);
      router.addRegion(entry,widgetId(EditorWidget::CurvePresetActionBase)+k);
    }
  }
}


void buildInspectorFor(ScreenBuilder &builder, const UiRect &panel, EditorEntityId target, u64 onlyComponent);
// Inspectors focados (Unity 6000.0 Manual/InspectorFocused) numa janela sobre
// o viewport: uma aba por Inspector, cada um preso ao seu objeto ou
// componente; o cabeçalho mostra o caminho completo (o tooltip da aba na
// Unity). ⋮ tem Ping e Fechar todas; "–" minimiza para um chip.
void buildMaterialAssetInspector(ScreenBuilder &builder,const UiRect &panel);
void buildEnvironmentAssetInspector(ScreenBuilder &builder,const UiRect &panel);
void buildProfileAssetInspector(ScreenBuilder &builder,const UiRect &panel);
void buildFocusedInspectors(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  if(state.focusedInspectors.empty() || layout.viewport.isEmpty()) return;
  const u32 count=static_cast<u32>(state.focusedInspectors.size());
  const u32 active=std::min(std::max(state.focusedActive,1u),count)-1;
  if(state.focusedCollapsed) {
    // Abaixo da barra do viewport, para não cobrir os botões dela.
    const UiRect chip{layout.viewport.right()-150,layout.viewport.y+60,142,34};
    list.addRect(chip,theme.color.raised,theme.radius.control);
    list.addImage(centred({chip.x,chip.y,30,chip.height},16,16),static_cast<UiImageId>(UiIcon::ScenePin),theme.color.accent);
    builder.label(deflate(chip,UiInsets{30,0,8,0}),("Propriedades · "+std::to_string(count)).c_str(),theme.color.text,theme.type.caption);
    router.addRegion(chip,widgetId(EditorWidget::FocusedChip));
    return;
  }
  const float width=std::min(300.f,layout.viewport.width-16);
  const UiRect window{layout.viewport.right()-width-8,layout.viewport.y+8,width,layout.viewport.height-16};
  layout.focusedWindow=window;
  router.addBlocker(window);
  list.addRect(deflate(window,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(window,theme.color.surface,theme.radius.control);
  auto content=deflate(window,UiInsets::all(4));
  auto tabs=takeTop(content,32);
  builder.iconButton(takeRight(tabs,30),UiIcon::EditorAuthorMore,widgetId(EditorWidget::FocusedMenu),state.focusedMenu);
  const auto collapse=takeRight(tabs,30);
  builder.label(collapse,"-",theme.color.textDim,theme.type.title,UiAlign::Center);
  router.addRegion(collapse,widgetId(EditorWidget::FocusedCollapse));
  // Abas: nome do objeto (e do componente, se for de componente) com ×.
  const float tabWidth=std::max(70.f,std::min(140.f,tabs.width/count));
  for(u32 i=0;i<count && tabs.width>=40;++i) {
    const auto &focused=state.focusedInspectors[i];
    const bool asset=focused.kind!=EditorScreenState::FocusedAsset::None;
    const auto *object=asset?nullptr:state.document->find(focused.entity);
    std::string name=asset?focused.name:object?object->name:"Ausente";
    if(object && focused.component)
      if(const auto *value=object->components.findInstance(focused.component)) {
        const auto *schema=scene::findComponentSchema(value->type().id);
        const auto *script=scene::scriptBehavior(value);
        name+=" · "+(script?script->scriptType:schema?std::string(schema->name):std::string(value->type().id));
      }
    auto tab=takeLeft(tabs,std::min(tabWidth,tabs.width));
    const bool on=i==active;
    list.addRect(deflate(tab,UiInsets{1,2,1,0}),on?theme.color.raised:theme.color.silhouette,theme.radius.control);
    if(on) list.addRect({tab.x+2,tab.bottom()-2,tab.width-4,2},theme.color.accent,1);
    const auto close=takeRight(tab,24);
    builder.label(close,"×",theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(close,widgetId(EditorWidget::FocusedCloseBase)+i);
    list.pushClip(tab);
    builder.label(deflate(tab,UiInsets{6,0,2,0}),name,on?theme.color.text:theme.color.textDim,theme.type.caption);
    list.popClip();
    router.addRegion(tab,widgetId(EditorWidget::FocusedTabBase)+i);
  }
  const auto &focused=state.focusedInspectors[active];
  if(focused.kind!=EditorScreenState::FocusedAsset::None) {
    // Recurso: o mesmo Inspector de recurso, desenhado com o contexto da janela
    // (o instantâneo trocado numa cópia do estado; o estado real não muda).
    EditorScreenState scoped=state;
    scoped.swapAssetInspector(scoped.focusedAsset);
    ScreenBuilder sub{scoped,theme,list,router};
    sub.layout=builder.layout;sub.focusedWindow=true;
    using Kind=EditorScreenState::FocusedAsset;
    if(focused.kind==Kind::Material && scoped.materialInspector.valid()) buildMaterialAssetInspector(sub,content);
    else if(focused.kind==Kind::EnvironmentMap && scoped.environmentInspector.valid()) buildEnvironmentAssetInspector(sub,content);
    else if(focused.kind==Kind::EnvironmentProfile && scoped.profileInspector.valid()) buildProfileAssetInspector(sub,content);
    else builder.label(content,"Carregando recurso…",theme.color.textDim,theme.type.caption,UiAlign::Center);
  } else {
    // Caminho completo do item.
    auto path=takeTop(content,20);
    std::string text;
    for(const auto *up=state.document->find(focused.entity);up && up->id!=state.document->root();up=state.document->find(up->parent))
      text=text.empty()?std::string(up->name):std::string(up->name)+" / "+text;
    builder.label(deflate(path,UiInsets{6,0,6,0}),text.empty()?"Objeto ausente":"Cena / "+text,theme.color.textMuted,theme.type.caption);
    builder.focusedWindow=true;
    buildInspectorFor(builder,content,focused.entity,focused.component);
    builder.focusedWindow=false;builder.onlyComponent=0;
  }
  if(state.focusedMenu) {
    const UiRect menu{window.right()-170,window.y+34,164,80};
    list.addRect(menu,theme.color.raised,theme.radius.control);
    auto rows=deflate(menu,UiInsets::all(4));
    auto ping=takeTop(rows,36),closeAll=takeTop(rows,36);
    router.addRegion(ping,widgetId(EditorWidget::FocusedPing));
    router.addRegion(closeAll,widgetId(EditorWidget::FocusedCloseAll));
    list.addImage(centred(takeLeft(ping,30),16,16),static_cast<UiImageId>(UiIcon::EditorFrameObject),theme.color.textDim);
    builder.label(ping,"Ping na Hierarquia",theme.color.text,theme.type.caption);
    list.addImage(centred(takeLeft(closeAll,30),16,16),static_cast<UiImageId>(UiIcon::UiClose),theme.color.axisX);
    builder.label(closeAll,"Fechar todas",theme.color.axisX,theme.type.caption);
  }
}

// Histórico de Desfazer (Unity 6000.0 Manual/UndoWindow) sob os botões de
// Desfazer/Refazer: tocar num ponto leva a cena até ele, desfazendo ou refazendo
// quantos passos forem precisos. Os desfeitos ficam apagados com um traço
// vermelho (como na Unity) até uma edição nova descartá-los.
void buildUndoHistory(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  const float width=std::min(320.f,state.surface.width-16);
  const float top=layout.topBar.bottom()+4;
  const UiRect window{state.surface.right()-width-8,top,width,std::max(120.f,state.surface.bottom()-top-8)};
  router.addBlocker(window);
  list.addRect(deflate(window,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(window,theme.color.surface,theme.radius.control);
  auto content=deflate(window,UiInsets::all(6));
  auto header=takeTop(content,40);
  builder.iconButton(takeRight(header,36),UiIcon::UiClose,widgetId(EditorWidget::UndoHistoryClose));
  takeRight(header,4);
  builder.iconButton(takeRight(header,36),state.undoNewestFirst?UiIcon::UiChevronDown:UiIcon::UiChevronUp,
                     widgetId(EditorWidget::UndoHistoryOrder));
  list.addImage(centred(takeLeft(header,30),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
  const u32 count=static_cast<u32>(state.undoEntries.size());
  const u32 undone=count-std::min(count,state.undoApplied);
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},"Histórico",theme.color.text,theme.type.cardName);
  const std::string summary=std::to_string(state.undoApplied)+(state.undoApplied==1?" passo":" passos")+
      (undone?" · "+std::to_string(undone)+(undone==1?" desfeito":" desfeitos"):std::string());
  builder.label({header.x,header.y+half,header.width,half},summary.c_str(),theme.color.textDim,theme.type.label);
  // Pontos: 0 = cena como abriu, i = depois do passo i.
  const u32 points=count+1;
  auto footer=takeBottom(content,30);
  const float rowHeight=38;
  const u32 perPage=std::max(1u,static_cast<u32>(content.height/rowHeight));
  const u32 pages=(points+perPage-1)/perPage,page=std::min(state.undoHistoryPage,pages-1);
  for(u32 slot=page*perPage;slot<points && slot<(page+1)*perPage;++slot) {
    const u32 point=state.undoNewestFirst?points-1-slot:slot;
    auto row=takeTop(content,rowHeight);row.height-=4;
    const bool current=point==state.undoApplied,redo=point>state.undoApplied;
    list.addRect(row,current?withAlpha(theme.color.accent,.18f):theme.color.silhouette,theme.radius.control);
    if(current) list.addRect({row.x,row.y,3,row.height},theme.color.accent,1);
    if(redo) list.addRect({row.x,row.y,3,row.height},theme.color.danger,1);
    auto inner=deflate(row,UiInsets{10,0,8,0});
    const auto number=takeLeft(inner,28);
    builder.label(number,point?std::to_string(point).c_str():"·",redo?theme.color.textFaint:theme.color.textDim,
                  theme.type.caption,UiAlign::Center);
    if(current) {
      const auto tag=takeRight(inner,56);
      list.addRect(centred(tag,52,20),theme.color.accent,10);
      builder.label(tag,"atual",theme.color.accentInk,theme.type.label,UiAlign::Center);
    }
    const std::string &text=point?state.undoEntries[point-1]:std::string("Cena aberta");
    list.pushClip(inner);
    builder.label(deflate(inner,UiInsets{4,0,2,0}),text,redo?theme.color.textFaint:current?theme.color.text:theme.color.textDim,
                  theme.type.caption);
    list.popClip();
    if(!current) router.addRegion(row,widgetId(EditorWidget::UndoHistoryRowBase)+point);
  }
  if(pages>1) {
    const auto previous=takeLeft(footer,44),next=takeRight(footer,44);
    builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::UndoHistoryPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::UndoHistoryNext));
    builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textDim,theme.type.caption,UiAlign::Center);
  } else builder.label(footer,count?"Toque num ponto para voltar a ele":"Nada para desfazer ainda",theme.color.textFaint,
                       theme.type.caption,UiAlign::Center);
}

// Busca global (Unity 6000.0 Manual/search-overview) em tela cheia: campo no
// topo, chips de provedor com a contagem de cada um, resultados com ícone,
// título, caminho e o provedor; tocar abre o resultado (objeto: seleciona e faz
// Ping; arquivo: revela no painel e abre; receita: abre Criar nela).
void buildGlobalSearch(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
  const float width=std::min(640.f,state.surface.width-16);
  const UiRect window{state.surface.x+(state.surface.width-width)*.5f,state.surface.y+8,width,state.surface.height-16};
  list.addRect(deflate(window,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(window,theme.color.surface,theme.radius.control);
  auto content=deflate(window,UiInsets::all(8));
  // Campo.
  auto top=takeTop(content,40);
  builder.iconButton(takeRight(top,40),UiIcon::UiClose,widgetId(EditorWidget::GlobalSearchClose));
  takeRight(top,6);
  list.addRect(top,state.editingGlobalSearch?theme.color.raised:theme.color.silhouette,theme.radius.control);
  if(state.editingGlobalSearch) list.addRect({top.x,top.bottom()-2,top.width,2},theme.color.accent,1);
  list.addImage(centred(UiRect{top.x,top.y,36,top.height},18,18),static_cast<UiImageId>(UiIcon::IdeSearch),theme.color.accent);
  const std::string shown=state.editingGlobalSearch?std::string(state.renameText):state.globalQuery;
  builder.label(deflate(top,UiInsets{38,0,8,0}),shown.empty()?"Buscar na cena, no projeto e em Criar  (t:Tipo  h:  p:  m:)":shown,
                shown.empty()?theme.color.textFaint:theme.color.text,theme.type.body);
  router.addRegion(top,widgetId(EditorWidget::GlobalSearchField));
  takeTop(content,8);
  // Chips de provedor.
  auto chips=takeTop(content,32);takeTop(content,8);
  const u32 total=state.globalCounts.scene+state.globalCounts.project+state.globalCounts.create;
  const struct {const char *name;UiIcon icon;u32 count;} providers[]{
    {"Tudo",UiIcon::AssetsSearch,total},{"Cena",UiIcon::SceneObject,state.globalCounts.scene},
    {"Projeto",UiIcon::AssetsFolder,state.globalCounts.project},{"Criar",UiIcon::SceneObjectAdd,state.globalCounts.create}};
  const float chipWidth=std::min(140.f,(chips.width-12)/4);
  for(u32 i=0;i<4;++i) {
    const UiRect chip{chips.x+i*(chipWidth+4),chips.y,chipWidth,chips.height};
    const bool on=static_cast<u32>(state.globalProvider)==i;
    list.addRect(chip,on?theme.color.accent:theme.color.raised,16);
    auto inner=deflate(chip,UiInsets{10,0,10,0});
    list.addImage(centred(takeLeft(inner,18),14,14),static_cast<UiImageId>(providers[i].icon),on?theme.color.accentInk:theme.color.textDim);
    const auto count=takeRight(inner,30);
    builder.label(count,std::to_string(providers[i].count).c_str(),on?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(deflate(inner,UiInsets{4,0,0,0}),providers[i].name,on?theme.color.accentInk:theme.color.text,theme.type.caption);
    router.addRegion(chip,widgetId(EditorWidget::GlobalSearchProviderBase)+i);
  }
  // Resultados.
  auto footer=takeBottom(content,28);
  const float rowHeight=44;
  const u32 count=static_cast<u32>(state.globalResults.size());
  const u32 perPage=std::max(1u,static_cast<u32>(content.height/rowHeight));
  const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.globalPage,pages-1);
  if(!count) {
    const bool idle=state.globalQuery.empty() && state.globalProvider==EditorSearchProvider::All;
    builder.label(content,idle?"Digite para buscar, ou escolha um provedor para ver tudo dele":"Nada encontrado",
                  theme.color.textDim,theme.type.body,UiAlign::Center);
  }
  for(u32 i=page*perPage;i<count && i<(page+1)*perPage;++i) {
    const auto &result=state.globalResults[i];
    auto row=takeTop(content,rowHeight);row.height-=4;
    list.addRect(row,theme.color.raised,theme.radius.control);
    auto inner=deflate(row,UiInsets{8,0,8,0});
    auto icon=result.icon;
    if(result.provider==EditorSearchProvider::Scene)
      if(const auto *entity=state.document->find(static_cast<EditorEntityId>(result.key))) icon=iconForEntity(*entity);
    list.addImage(centred(takeLeft(inner,30),18,18),static_cast<UiImageId>(icon),theme.color.accent);
    const auto tag=takeRight(inner,70);
    const char *tagName=result.provider==EditorSearchProvider::Scene?"Cena":result.provider==EditorSearchProvider::Project?"Projeto":"Criar";
    list.addRect(centred(tag,64,20),theme.color.silhouette,10);
    builder.label(tag,tagName,theme.color.textDim,theme.type.caption,UiAlign::Center);
    list.pushClip(inner);
    builder.label({inner.x+4,inner.y+2,inner.width-4,inner.height*.5f},result.title,theme.color.text,theme.type.caption);
    builder.label({inner.x+4,inner.y+inner.height*.5f,inner.width-4,inner.height*.5f-2},result.detail,theme.color.textDim,theme.type.caption);
    list.popClip();
    router.addRegion(row,widgetId(EditorWidget::GlobalSearchResultBase)+i);
  }
  if(pages>1) {
    const auto previous=takeLeft(footer,44),next=takeRight(footer,44);
    builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::GlobalSearchPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::GlobalSearchNext));
    builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)+" · "+std::to_string(count)+" resultados").c_str(),
                  theme.color.textDim,theme.type.caption,UiAlign::Center);
  } else if(count) builder.label(footer,(std::to_string(count)+(count==1?" resultado":" resultados")).c_str(),
                                 theme.color.textDim,theme.type.caption,UiAlign::Center);
}

// Layouts dos painéis (Unity 6000.0 Toolbar › Layout): cinco arranjos prontos
// e os salvos pelo usuário, cada um com uma miniatura do arranjo; o que
// coincide com a tela atual vem marcado. Salvar guarda o arranjo atual com um
// nome; o × apaga um salvo; Restaurar volta ao Padrão.
void drawLayoutThumbnail(ScreenBuilder &builder,const UiRect &area,const EditorLayout &layout,bool on) {
  const auto &theme=builder.theme;auto &list=builder.list;
  list.addRect(area,theme.color.voidBlack,4);
  auto inner=deflate(area,UiInsets::all(3));
  const auto ink=on?theme.color.accent:theme.color.textDim;
  const float total=builder.state.surface.width>0?builder.state.surface.width:853;
  const auto share=[&](float width,float fallback){return (width>0?width/total:fallback)*inner.width;};
  list.addRect(takeTop(inner,3),withAlpha(ink,.5f),1);takeTop(inner,1);
  if(layout.hierarchyVisible) {auto h=takeLeft(inner,std::max(4.f,share(layout.hierarchyWidth,.22f)));h.width-=1;list.addRect(h,withAlpha(ink,.55f),1);}
  if(layout.inspectorVisible) {auto i=takeRight(inner,std::max(4.f,share(layout.inspectorWidth,.26f)));i.x+=1;i.width-=1;list.addRect(i,withAlpha(ink,.55f),1);}
  if(layout.diagnosticDock) {auto d=takeBottom(inner,inner.height*.3f);d.y+=1;list.addRect(d,withAlpha(theme.color.warning,.6f),1);}
  list.addRect(inner,withAlpha(ink,.18f),1);
}

void buildLayoutsPanel(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.7f));
  const float width=std::min(560.f,state.surface.width-16);
  const UiRect window=centred(state.surface,width,std::min(380.f,state.surface.height-16));
  list.addRect(deflate(window,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(window,theme.color.surface,theme.radius.control);
  auto content=deflate(window,UiInsets::all(10));
  auto header=takeTop(content,36);
  builder.iconButton(takeRight(header,36),UiIcon::UiClose,widgetId(EditorWidget::LayoutsClose));
  list.addImage(centred(takeLeft(header,28),18,18),static_cast<UiImageId>(UiIcon::UiPanelLeft),theme.color.accent);
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},"Layout dos painéis",theme.color.text,theme.type.cardName);
  builder.label({header.x,header.y+half,header.width,half},"Arranjo da tela; fica no projeto, não na cena",theme.color.textDim,theme.type.caption);
  takeTop(content,6);
  auto actions=takeBottom(content,36);takeBottom(content,6);
  EditorLayout current;
  current.hierarchyWidth=state.hierarchyWidth;current.inspectorWidth=state.inspectorWidth;
  current.hierarchyVisible=state.hierarchyVisible;current.inspectorVisible=state.inspectorVisible;
  current.filesCollapsed=state.filesCollapsed;current.diagnosticDock=state.diagnosticDockOpen;
  // Cartões: prontos numa faixa, salvos numa lista embaixo.
  const auto builtins=editorBuiltinLayouts(state.surface.width);
  auto grid=takeTop(content,92);takeTop(content,8);
  const float cell=(grid.width-4*6)/5;
  for(u32 i=0;i<builtins.size();++i) {
    const UiRect card{grid.x+i*(cell+6),grid.y,cell,grid.height};
    const bool on=builtins[i]==current;
    list.addRect(card,on?withAlpha(theme.color.accent,.16f):theme.color.raised,theme.radius.control);
    if(on) list.addRect({card.x,card.bottom()-3,card.width,3},theme.color.accent,1);
    auto inner=deflate(card,UiInsets::all(6));
    drawLayoutThumbnail(builder,takeTop(inner,48),builtins[i],on);
    builder.label(deflate(inner,UiInsets{0,4,0,0}),builtins[i].name,on?theme.color.text:theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(card,widgetId(EditorWidget::LayoutBuiltinBase)+i);
  }
  builder.label(takeTop(content,20),state.userLayouts.empty()?"Nenhum layout salvo":"Salvos",theme.color.textMuted,theme.type.caption);
  const u32 perRow=2,rows=std::max(1u,static_cast<u32>(content.height/40));
  for(u32 i=0;i<state.userLayouts.size() && i<rows*perRow;++i) {
    const float w=(content.width-6)/2;
    const UiRect row{content.x+(i%perRow)*(w+6),content.y+(i/perRow)*40,w,36};
    const bool on=state.userLayouts[i]==current;
    list.addRect(row,on?withAlpha(theme.color.accent,.16f):theme.color.raised,theme.radius.control);
    auto inner=deflate(row,UiInsets{6,4,4,4});
    drawLayoutThumbnail(builder,takeLeft(inner,44),state.userLayouts[i],on);
    const auto remove=takeRight(inner,32);
    list.addImage(centred(remove,14,14),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
    builder.label(deflate(inner,UiInsets{8,0,0,0}),state.userLayouts[i].name,on?theme.color.text:theme.color.textDim,theme.type.caption);
    router.addRegion(row,widgetId(EditorWidget::LayoutUserBase)+i);
    router.addRegion(remove,widgetId(EditorWidget::LayoutDeleteBase)+i);
  }
  const float bw=(actions.width-6)/2;
  const UiRect save{actions.x,actions.y,bw,actions.height},reset{actions.x+bw+6,actions.y,bw,actions.height};
  const bool canSave=state.userLayouts.size()<12;
  list.addRect(save,canSave?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(save,canSave?"Salvar layout atual":"Limite de 12 layouts",canSave?theme.color.accentInk:theme.color.textFaint,theme.type.caption,UiAlign::Center);
  if(canSave) router.addRegion(save,widgetId(EditorWidget::LayoutSave));
  list.addRect(reset,theme.color.raised,theme.radius.control);
  builder.label(reset,"Restaurar o padrão",theme.color.text,theme.type.caption,UiAlign::Center);
  router.addRegion(reset,widgetId(EditorWidget::LayoutReset));
}

// Barra de status (Unity 6000.0 Manual/StatusBar): à esquerda a última
// mensagem do console com o ícone da severidade (tocar abre o console); à
// direita as contagens de avisos e erros e, quando há trabalho em segundo
// plano, a atividade girando com o nome do primeiro trabalho e a barra de
// progresso quando ele tem medida real (tocar lista todos).
void buildStatusBar(ScreenBuilder &builder,const UiRect &bar) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  if(bar.isEmpty()) return;
  list.addRect(bar,theme.color.voidBlack);
  list.addRect({bar.x,bar.y,bar.width,1},theme.color.line);
  auto content=deflate(bar,UiInsets{8,1,8,0});
  // Trabalhos à direita.
  if(!state.backgroundTasks.empty()) {
    const auto &first=state.backgroundTasks.front();
    // Coluna estreita (telefone deitado com os dois painéis): a atividade
    // encolhe e, com vários trabalhos, vira a contagem; a mensagem fica com o resto.
    const bool narrow=content.width<520;
    auto tasks=takeRight(content,narrow?140.f:std::min(280.f,content.width*.4f));
    const auto spinner=takeLeft(tasks,20);
    const UiPoint centre{spinner.x+spinner.width*.5f,spinner.y+spinner.height*.5f};
    for(u32 i=0;i<8;++i) {
      const float angle=static_cast<float>(state.uiTime*6.0)+i*0.785398f;
      const float fade=static_cast<float>((i+1))/8.0f;
      list.addRect({centre.x+std::cos(angle)*6-1.5f,centre.y+std::sin(angle)*6-1.5f,3,3},withAlpha(theme.color.accent,fade),1.5f);
    }
    const float trackWidth=narrow?36.f:56.f;
    if(first.progress>=0) {
      auto track=takeRight(tasks,trackWidth+4);track=centred(track,trackWidth,4);
      list.addRect(track,theme.color.track,2);
      list.addRect({track.x,track.y,track.width*std::clamp(first.progress,0.f,1.f),track.height},theme.color.accent,2);
      takeRight(tasks,6);
    }
    std::string label=first.label;
    if(state.backgroundTasks.size()>1)
      label=narrow?std::to_string(state.backgroundTasks.size())+" trabalhos":label+" +"+std::to_string(state.backgroundTasks.size()-1);
    list.pushClip(tasks);
    builder.label(deflate(tasks,UiInsets{4,0,0,0}),label.c_str(),theme.color.text,theme.type.caption);
    list.popClip();
    router.addRegion({tasks.x-20,bar.y,tasks.width+20+(first.progress>=0?trackWidth+10:0.f),bar.height},widgetId(EditorWidget::StatusTasks));
    takeRight(content,10);
  }
  // Contagens.
  u32 warnings=0,errors=0;
  const EditorConsoleEntry *last=nullptr;
  if(state.console) {
    warnings=state.console->count(EditorConsoleSeverity::Warning);errors=state.console->count(EditorConsoleSeverity::Error);
    if(!state.console->entries().empty()) last=&state.console->entries().back();
  }
  const auto counter=[&](UiIcon icon,u32 value,UiColor tint) {
    auto area=takeRight(content,value>99?52.f:40.f);
    list.addImage(centred(takeLeft(area,18),13,13),static_cast<UiImageId>(icon),value?tint:theme.color.textFaint);
    builder.label(area,std::to_string(value).c_str(),value?theme.color.text:theme.color.textFaint,theme.type.caption);
  };
  const float countsRight=content.right();
  counter(UiIcon::IdeError,errors,theme.color.danger);
  counter(UiIcon::IdeWarning,warnings,theme.color.warning);
  const UiRect counts{content.right(),bar.y,countsRight-content.right(),bar.height};
  // Última mensagem.
  const UiColor tint=!last?theme.color.textFaint:last->severity==EditorConsoleSeverity::Error?theme.color.danger:
      last->severity==EditorConsoleSeverity::Warning?theme.color.warning:theme.color.textDim;
  list.addImage(centred(takeLeft(content,20),13,13),static_cast<UiImageId>(
      !last?UiIcon::IdeConsole:last->severity==EditorConsoleSeverity::Error?UiIcon::IdeError:
      last->severity==EditorConsoleSeverity::Warning?UiIcon::IdeWarning:UiIcon::IdeConsole),tint);
  std::string message=last?last->message.substr(0,last->message.find('\n')):std::string("Console vazio");
  if(last && last->repeats>1) message+="  \xC3\x97"+std::to_string(last->repeats);
  list.pushClip(content);
  builder.label(deflate(content,UiInsets{4,0,0,0}),message.c_str(),last?theme.color.text:theme.color.textFaint,theme.type.caption);
  list.popClip();
  router.addRegion({bar.x,bar.y,counts.right()-bar.x,bar.height},widgetId(EditorWidget::StatusConsole));
}

// Trabalhos em segundo plano (Unity: Background Tasks), sobre a barra de
// status: um cartão por trabalho com detalhe, progresso real quando existe e
// Cancelar quando o trabalho aceita.
void buildBackgroundTasks(ScreenBuilder &builder,const EditorScreenLayout &layout) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  const float width=std::min(360.f,state.surface.width-16);
  const u32 count=static_cast<u32>(state.backgroundTasks.size());
  const float height=std::min(56.f+std::max(1u,count)*58.f,layout.statusBar.y-layout.topBar.bottom()-8);
  const UiRect window{state.surface.right()-width-8,layout.statusBar.y-height-4,width,height};
  router.addBlocker(window);
  list.addRect(deflate(window,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(window,theme.color.surface,theme.radius.control);
  auto content=deflate(window,UiInsets::all(8));
  auto header=takeTop(content,36);
  builder.iconButton(takeRight(header,32),UiIcon::UiClose,widgetId(EditorWidget::StatusTasksClose));
  builder.label(header,"Trabalhos em segundo plano",theme.color.text,theme.type.cardName);
  if(!count) builder.label(content,"Nada em andamento",theme.color.textDim,theme.type.caption,UiAlign::Center);
  for(u32 i=0;i<count && content.height>=54;++i) {
    const auto &task=state.backgroundTasks[i];
    auto card=takeTop(content,54);card.height-=4;
    list.addRect(card,theme.color.raised,theme.radius.control);
    auto inner=deflate(card,UiInsets{10,6,8,6});
    if(task.cancelable) {
      const auto cancel=takeRight(inner,76);
      list.addRect(centred(cancel,72,28),theme.color.silhouette,theme.radius.control);
      builder.label(cancel,"Cancelar",theme.color.danger,theme.type.caption,UiAlign::Center);
      router.addRegion(cancel,widgetId(EditorWidget::StatusTaskCancelBase)+i);
    }
    auto line=takeTop(inner,18);
    if(task.progress>=0) {
      const std::string percent=std::to_string(static_cast<int>(std::clamp(task.progress,0.f,1.f)*100+.5f))+"%";
      builder.label(takeRight(line,44),percent.c_str(),theme.color.accent,theme.type.caption,UiAlign::Center);
    }
    builder.label(line,task.label.c_str(),theme.color.text,theme.type.caption);
    auto detail=takeTop(inner,16);
    list.pushClip(detail);
    builder.label(detail,task.detail.empty()?"Sem etapa informada":task.detail.c_str(),theme.color.textDim,theme.type.caption);
    list.popClip();
    auto track=takeBottom(inner,4);
    list.addRect(track,theme.color.track,2);
    if(task.progress>=0) list.addRect({track.x,track.y,track.width*std::clamp(task.progress,0.f,1.f),track.height},theme.color.accent,2);
    else {
      // Sem medida real: um segmento que corre, não uma porcentagem.
      const float t=static_cast<float>(std::fmod(state.uiTime*.6,1.0));
      list.addRect({track.x+track.width*t*.75f,track.y,track.width*.25f,track.height},withAlpha(theme.color.accent,.8f),2);
    }
  }
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
// Debug.DrawLine/DrawRay do Play, na câmera que produziu o quadro. Cor no
// mesmo formato da interface (0xAARRGGBB); só vivem no mundo de execução.
void buildPlayDebugLines(ScreenBuilder &builder) {
  const auto &state=builder.state;
  if(!state.debugLines || state.workspace!=EditorWorkspace::Play) return;
  for(const auto &line:state.debugLines->lines()) {
    UiPoint a,b;
    if(projectSegmentToScreen(state.debugView,line.a,line.b,a,b)) builder.list.addLine(a,b,line.rgba,1.6f);
  }
}

namespace {std::string fitMiddle(const UiDrawList &,const std::string &,float,const UiTypeStyle &);}
#include "editor/editor_collider_authoring_ui.inl"
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

  if((state.workspace==EditorWorkspace::Scene||state.physicsDiagnosticOpen) && (state.showComponentVisuals||state.physicsDiagnosticOpen) && !state.cameraViewEntity) {
    const float aspect=state.view->frustum.tangentHalfHorizontal/state.view->frustum.tangentHalfVertical;
    const auto visuals=collectComponentVisuals(*state.document,state.selection,aspect,state.resources,state.pathInstance,state.pathPointId,state.pathEntity,state.expandedNative);
    const auto *inspectedObject=state.document->find(state.selection);
    const auto *inspected=inspectedObject?inspectedObject->components.findInstance(state.expandedNative):nullptr;
    const bool colliderInspection=inspected && (&inspected->type()==&scene::Collider::descriptor || &inspected->type()==&scene::PhysicsBody::descriptor) &&
        !state.multiSelect && state.selectionSet.size()<=1 && !state.guiSelection.valid();
    for(const auto &v:visuals) {
      if(!componentVisualVisible(*state.document,v.entity,state.hiddenLayers,state.sceneHidden))continue;
      const auto color=v.enabled?theme.color.accent:theme.color.textMuted;
      if(state.physicsDiagnosticOpen && (v.icon==UiIcon::PhysicsCharacterGround || v.icon==UiIcon::PhysicsCharacterPlatformCarry))continue;
      for(const auto &line:v.segments) {
        auto lineColor=line.emphasis==2?theme.color.text:line.emphasis==1?theme.color.axisY:color;
        float width=line.emphasis?2.f:1.3f;
        if(state.physicsDiagnosticOpen && v.icon==UiIcon::ComponentCollider && !state.physicsDiagnosticCollision)continue;
        if(state.colliderTopology && v.icon==UiIcon::ComponentCollider && v.entity==state.selection && v.instance==state.expandedNative)continue;
        if(colliderInspection && v.icon==UiIcon::ComponentCollider) {
          const bool focused=v.entity==state.selection && v.instance==state.expandedNative;
          lineColor=focused?theme.color.text:withAlpha(theme.color.textMuted,.5f);width=focused?2.f:1.f;
        }
        UiPoint a,b;if(projectSegmentToScreen(*state.view,line.a,line.b,a,b)) builder.list.addLine(a,b,lineColor,width);
      }
      if(!v.marker) continue;
      const auto p=projectWorldToScreen(*state.view,v.origin);
      if(!p.valid || !viewport.contains(p.screen)) continue;
      const UiRect icon{p.screen.x-14+v.markerIndex*32.f,p.screen.y-14,28,28};
      if(v.markerIndex) builder.list.addLine(p.screen,{icon.x+14,icon.y+14},theme.color.textMuted,1);
      builder.list.addRect(icon,withAlpha(theme.color.surface,.9f),8);
      builder.list.addImage(deflate(icon,UiInsets::all(3)),static_cast<UiImageId>(v.icon),v.enabled?0xffffffff:theme.color.textMuted);
      if(v.entity<0x01000000u&&componentVisualSelectable(*state.document,v.entity,
          state.hiddenLayers,state.unpickableLayers,state.sceneHidden,state.scenePickOff))
        builder.router.addRegion(icon,widgetId(EditorWidget::ComponentVisualBase)+v.entity,32);
    }
  }
  if(state.motorBakeReady&&state.motorSetupTarget==state.selection&&!state.cameraViewEntity) {
    float world[16];if(editorWorldMatrix(*state.document,state.selection,world)) {
      const UiColor colors[]{theme.color.accent,theme.color.axisX,theme.color.axisY,theme.color.axisZ};
      for(usize part=0;part<state.motorBakePreview.size();++part) {
        const auto color=part<state.motorBakeEnabled.size()&&state.motorBakeEnabled[part]?colors[part%4]:theme.color.textMuted;
        for(const auto &triangle:state.motorBakePreview[part].triangles)for(u32 edge=0;edge<3;++edge) {
          float a[3],b[3];const auto *p=triangle.data()+edge*3,*q=triangle.data()+((edge+1)%3)*3;
          for(u32 k=0;k<3;++k){a[k]=world[12+k]+world[k]*p[0]+world[4+k]*p[1]+world[8+k]*p[2];b[k]=world[12+k]+world[k]*q[0]+world[4+k]*q[1]+world[8+k]*q[2];}
          UiPoint from,to;if(projectSegmentToScreen(*state.view,a,b,from,to))builder.list.addLine(from,to,color,1.4f);
        }
      }
    }
  }
  // Collider outlines now share the component visual registry. Joint anchors
  // retain their specialized connection view until that provider is migrated.
  const auto *selectedVisual=state.document->find(state.selection);
  const auto *expandedVisual=selectedVisual?selectedVisual->components.findInstance(state.expandedNative):nullptr;
  if(state.workspace==EditorWorkspace::Scene && state.showComponentVisuals && !state.cameraViewEntity &&
     !state.multiSelect && state.selectionSet.size()<=1 && !state.guiSelection.valid() && expandedVisual &&
     (&expandedVisual->type()==&scene::Collider::descriptor || &expandedVisual->type()==&scene::PhysicsBody::descriptor) &&
     componentVisualSelectable(*state.document,state.selection,state.hiddenLayers,state.unpickableLayers,state.sceneHidden,state.scenePickOff)) {
    // A contextual instruction, not another permanent panel. The instance ID
    // matches the Inspector and the emphasized contour; ordinary taps still pick meshes.
    const auto bodyId=colliderInspectionBody(*state.document,state.selection,state.expandedNative);
    const auto *bodyObject=state.document->find(bodyId);
    const bool collider=&expandedVisual->type()==&scene::Collider::descriptor;
    const auto *shape=collider?static_cast<const scene::Collider*>(expandedVisual):nullptr;
    const std::string hint=state.physicsDiagnosticOpen?"Diagnóstico · COM e apoio são leituras do solver":
        shape&&shape->shape==scene::ColliderShape::Mesh&&state.tool!=EditorGizmoMode::Select&&
        (state.tool==EditorGizmoMode::Scale||!shape->meshLocalPose)?
        (state.tool==EditorGizmoMode::Scale?"Malha · dimensões pertencem ao recurso":"Malha · ative Pose local no Inspector"):
        collider&&state.tool!=EditorGizmoMode::Select?
        "Colisor "+std::to_string(expandedVisual->instanceId())+" · "+
        (state.tool==EditorGizmoMode::Translate?"Centro local":state.tool==EditorGizmoMode::Rotate?"Rotação local":"Dimensões simétricas")+" · arraste a alça":bodyObject?
        "Corpo "+std::string(bodyObject->name)+" · toque na forma para editar":
        "Colisor "+std::to_string(expandedVisual->instanceId())+" · toque na forma para trocar";
    // This draw list exposes Regular metrics. Keep measurement, middle
    // truncation and glyph rendering on the same weight: measuring Regular
    // and drawing Medium clipped the last letter on the physical device.
    auto hintStyle=theme.type.caption;hintStyle.medium=false;
    // Above all navigation variants (normal, compact and narrow), and apart
    // from the status row. Device captures caught both overlaps at -24/-50.
    // The short viewport's vertical tool strip reaches this row; leave its
    // touch targets clear rather than placing the hint underneath them.
    const float hintInset=viewport.height<500?64.f:8.f;
    const UiRect hintRect{viewport.x+hintInset,viewport.bottom()-116,
        std::min(viewport.width-160,measureTextWidth(hint,builder.list.fontMetrics(),hintStyle)+16),20};
    if(hintRect.width>0) {
      builder.list.addRect(hintRect,withAlpha(theme.color.surface,.9f),2);
      const auto text=deflate(hintRect,UiInsets{6,0,6,0});
      builder.label(text,fitMiddle(builder.list,hint,text.width,hintStyle),theme.color.textDim,hintStyle);
    }
  }
  if(state.showComponentVisuals && !state.cameraViewEntity && expandedVisual && &expandedVisual->type()==&scene::Joint::descriptor) buildPhysicsOverlay(builder);
  if(state.workspace==EditorWorkspace::Scene) {
    const UiRect controls{viewport.right()-42,viewport.y+8,34,34};
    builder.iconButton(controls,UiIcon::EditorAuthorCamera,widgetId(state.cameraViewEntity?EditorWidget::CameraViewClose:EditorWidget::ComponentVisualsToggle),state.cameraViewEntity || state.showComponentVisuals);
  }
  const EditorEntity *entity = state.document->find(state.selection);
  const bool colliderEditing=entity && (state.showComponentVisuals||state.colliderTopology) && !state.cameraViewEntity &&
      !state.multiSelect && state.selectionSet.size()<=1 && !state.guiSelection.valid() && expandedVisual &&
      &expandedVisual->type()==&scene::Collider::descriptor;
  if (entity != nullptr && !state.guiSelection.valid() && state.tool != EditorGizmoMode::Select &&
      state.workspace == EditorWorkspace::Scene && !state.cameraViewEntity && !colliderEditing && !(waterRoute(*entity).count && state.waterTab==1)) {
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
  buildColliderTopologyOverlay(builder,viewport);
  buildPhysicsDiagnosticOverlay(builder);
  if(colliderEditing && !state.colliderTopology && !state.physicsDiagnosticOpen && state.workspace==EditorWorkspace::Scene &&
     componentVisualSelectable(*state.document,entity->id,state.hiddenLayers,state.unpickableLayers,state.sceneHidden,state.scenePickOff)) {
      const UiColor colors[]{theme.color.axisX,theme.color.axisY,theme.color.axisZ};
      std::array<UiPoint,9> gripCenters{};u32 gripCount=0;
    for(u32 index=0;index<static_cast<u32>(ColliderHandleKind::Count);++index) {
      const auto kind=static_cast<ColliderHandleKind>(index);ColliderHandle handle;
      if(!colliderHandleMatchesTool(kind,state.tool)||!colliderHandleGeometry(*state.document,entity->id,
          expandedVisual->instanceId(),kind,*state.view,handle))continue;
      const u32 axis=index<6?index/2:index<9?index-6:index-9;
      const auto color=state.activeColliderHandle==index+1?theme.color.accent:colors[axis];
      const auto visible=[&](const float *world,UiPoint &pixel) {
        const auto p=projectWorldToScreen(*state.view,world);pixel=p.screen;
        const auto ray=screenPointToRay(*state.view,pixel);float depth=0;
        for(u32 i=0;i<3;++i)depth+=(world[i]-ray.origin[i])*ray.direction[i];
        return p.valid && viewport.contains(pixel) &&
            !colliderPointOccluded(state.colliderHandleOccluders,*state.view,pixel,entity->id,depth);
      };
      if(handle.rotation) {
        float previous[3];colliderRingPoint(handle,0,previous);
        for(u32 segment=1;segment<=48;++segment) {
          float point[3],middle[3];colliderRingPoint(handle,segment*6.28318530718f/48,point);
          colliderRingPoint(handle,(segment-.5f)*6.28318530718f/48,middle);
          UiPoint a,b,m;float angle;
          if(visible(middle,m)&&colliderRingAngle(*state.view,handle,m,angle)&&
             projectSegmentToScreen(*state.view,previous,point,a,b)) {
            builder.list.addLine(a,b,color,2.f);
            const UiRect target{m.x-10,m.y-10,20,20};
            if(viewport.contains({target.x,target.y})&&viewport.contains({target.right(),target.bottom()}))
              builder.router.addRegion(target,widgetId(EditorWidget::ColliderHandleBase)+index);
          }
          std::copy(point,point+3,previous);
        }
        } else {
          UiPoint point;float parameter;
          if(!visible(handle.point,point)||!cameraHandleRayParameter(*state.view,handle,point,parameter))continue;
          const auto anchor=point;
          const auto origin=projectWorldToScreen(*state.view,handle.origin);
          if(!origin.valid)continue;
          const float dx=point.x-origin.screen.x,dy=point.y-origin.screen.y;
          const float length=std::sqrt(dx*dx+dy*dy);
          if(length<1.f)continue;
          // Tiny shapes still need distinct touch targets. Keep each grip on its
          // projected axis and connect it to the real shape instead of allowing
          // overlapping rectangles to route a visible X grip to the Y channel.
          float distance=std::max(length,40.f);bool separated=false;
          for(u32 attempt=0;attempt<9;++attempt,distance+=34.f) {
            point={origin.screen.x+dx*distance/length,origin.screen.y+dy*distance/length};
            separated=true;
            for(u32 n=0;n<gripCount;++n)if(std::abs(point.x-gripCenters[n].x)<34.f&&
                std::abs(point.y-gripCenters[n].y)<34.f){separated=false;break;}
            if(separated)break;
          }
          if(!separated||!cameraHandleRayParameter(*state.view,handle,point,parameter))continue;
          const UiRect target{point.x-16,point.y-16,32,32};
          if(!viewport.contains({target.x,target.y})||!viewport.contains({target.right(),target.bottom()}))continue;
          gripCenters[gripCount++]=point;
          if(std::abs(point.x-anchor.x)>.5f||std::abs(point.y-anchor.y)>.5f)
            builder.list.addLine(anchor,point,color,1.f);
        if(index>=6) {UiPoint a,b;if(projectSegmentToScreen(*state.view,handle.origin,handle.point,a,b))builder.list.addLine(a,b,color,2);}
        // A discrete grip and generous touch area, not a card over the mesh.
        builder.list.addRect({point.x-7,point.y-7,14,14},theme.color.surface,2);
        builder.list.addRect({point.x-5,point.y-5,10,10},color,index<6?1.f:5.f);
        builder.router.addRegion(target,widgetId(EditorWidget::ColliderHandleBase)+index);
      }
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
    builder.iconButton(takeRight(header,34),UiIcon::AudioClip,widgetId(EditorWidget::ImportWave),false,
      builder.state.workspace==EditorWorkspace::Play?theme.color.textMuted:0xffffffff,builder.state.workspace!=EditorWorkspace::Play);
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
    builder.iconButton(takeRight(header,28),UiIcon::AudioClip,widgetId(EditorWidget::ImportWave),false,
      builder.state.workspace==EditorWorkspace::Play?theme.color.textMuted:0xffffffff,builder.state.workspace!=EditorWorkspace::Play);
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
  // "Selecionar vários" nasce da escolha de um (escolher um e somar outros,
  // como Ctrl+clique da Unity) e fica na mesma faixa enquanto o modo está aceso.
  const bool several=builder.state.selectedFiles.size()>1;
  if(!builder.state.selectedFile.empty() || builder.state.filesMultiSelect) {
    auto actions=takeBottom(content,26);
    auto toggle=takeLeft(actions,30);toggle.height=std::min(toggle.height,26.0f);
    builder.iconButton(deflate(toggle,UiInsets::all(2)),UiIcon::EditorSelectBox,widgetId(EditorWidget::FilesMultiToggle),builder.state.filesMultiSelect);
    auto action=[&](UiRect rect,const char *label,EditorWidget widget,UiColor color) {
      rect=deflate(rect,UiInsets::all(2));
      builder.list.addRect(rect,theme.color.raised,theme.radius.control);
      builder.label(rect,label,color,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(rect,widgetId(widget));
    };
    if(builder.state.filesMultiSelect) {
      const u32 count=static_cast<u32>(builder.state.selectedFiles.size());
      action(takeRight(actions,std::min(64.0f,actions.width*.4f)),"Nada",EditorWidget::FilesSelectNone,theme.color.text);
      builder.label(deflate(actions,UiInsets::symmetric(6,0)),
                    count?std::to_string(count)+(count==1?" selecionado":" selecionados"):std::string("Toque para somar"),
                    count>1?theme.color.accent:theme.color.textMuted,theme.type.caption);
    } else {
      const bool confirming=builder.state.pendingResourceDelete==builder.state.selectedFile;
      action(takeLeft(actions,actions.width*.5f),"Renomear",EditorWidget::FilesRename,theme.color.text);
      action(actions,confirming?"Apagar mesmo assim":"Apagar",EditorWidget::FilesDelete,
             confirming?theme.color.accent:theme.color.text);
    }
  }
  const auto *selectedRecord=builder.state.assetRegistry&&!several?
      builder.state.assetRegistry->findByPath(builder.state.selectedFile):nullptr;
  // Fonte de modelo é o que o REGISTRO diz, não a extensão: um GLB, ou o
  // `.gltf` de uma fonte em pasta (S0). GLB ainda não registrado também entra,
  // para Reimportar registrá-lo. "Texturas" extrai imagens EMBUTIDAS (R4): só
  // existe no GLB — na pasta as imagens já são arquivos do projeto.
  const bool glb=!several && builder.state.selectedFile.ends_with(".glb");
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
  } else if(selectedRecord&&selectedRecord->type==resources::AssetType::Prefab) {
    auto actions=takeBottom(content,40);
    auto button=deflate(actions,UiInsets::all(3));
    builder.list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Instanciar prefab",theme.color.text,theme.type.caption,UiAlign::Center);
    if(builder.state.workspace!=EditorWorkspace::Play) builder.router.addRegion(button,widgetId(EditorWidget::AssetInstantiate));
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
      if(code || several) builder.list.addRect({hit.x,hit.y,3,hit.height},theme.color.accent);
    } else if(several && builder.state.isFileSelected(entry.relativePath)) {
      // Os outros escolhidos: realce mais leve, como na Hierarquia.
      builder.list.addRect(hit,withAlpha(theme.color.accent,0.09f),theme.radius.control);
      builder.list.addRect({hit.x,hit.y,3,hit.height},withAlpha(theme.color.accent,0.55f));
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
        entry.name.ends_with(".cs")?(code?UiIcon::IdeCode:UiIcon::ScriptingCode):entry.name.ends_with(".prefab")?UiIcon::ScenePrefab:UiIcon::AssetsFile),code?0xffffffff:theme.color.textDim);
    builder.label(row,entry.name.c_str(),theme.color.text,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::FileRowBase)+i);
  }
  if(entries.empty()) builder.label(content,"Pasta vazia",theme.color.textMuted,theme.type.caption);
  builder.list.popClip();
}

u32 buildHierarchy(ScreenBuilder &builder, const UiRect &panel, u32 &outVisibleRows) {
  const UiTheme &theme = builder.theme;
  const auto &document = *builder.state.document;
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
  // No modo "Selecionar vários" o título vira a contagem.
  const u32 selectedCount=static_cast<u32>(builder.state.selectionSet.size());
  if(builder.state.multiSelect)
    builder.label(header,(std::to_string(selectedCount)+(selectedCount==1?" selecionado":" selecionados")).c_str(),theme.color.accent,theme.type.cardName);
  else builder.label(header, "Hierarquia", theme.color.text, theme.type.cardName);

  auto search=takeTop(content,36);
  // Unity: Ctrl/Shift+clique. No toque, um modo ao lado da busca: aceso, cada
  // toque na lista ou na vista soma ou tira da seleção.
  builder.iconButton(takeRight(search,32),UiIcon::EditorSelectBox,widgetId(EditorWidget::HierarchyMultiToggle),builder.state.multiSelect);
  takeRight(search,4);
  if(builder.state.multiSelect) {
    // Ações de conjunto em toda a largura, logo abaixo da busca.
    const UiRect row=takeTop(content,30);takeTop(content,4);
    const struct {const char *label;EditorWidget id;} actions[]{{"Tudo",EditorWidget::SelectAll},{"Filhos",EditorWidget::SelectChildren},
                                                               {"Inverter",EditorWidget::SelectInvert},{"Nada",EditorWidget::SelectNone}};
    const float cell=(row.width-9)/4;
    for(u32 i=0;i<4;++i) {
      const UiRect chip{row.x+i*(cell+3),row.y,cell,row.height};
      builder.list.addRect(chip,theme.color.raised,theme.radius.control);
      builder.label(chip,actions[i].label,theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(chip,widgetId(actions[i].id));
    }
  }
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
    for(const auto &row:builder.state.guiRows)if(editorSearchKey(row.name+" "+row.path).find(query)!=std::string::npos)
      for(auto parent=document.find(row.target.owner);parent;parent=document.find(parent->parent))matches.insert(parent->id);
  }
  builder.list.pushClip(content);
  outVisibleRows = static_cast<u32>(std::max(0.0f, content.height) / kRowHeight);

  using GuiParent=std::tuple<EditorEntityId,u64,ui::GuiId>;
  std::map<GuiParent,std::vector<usize>> guiChildrenByParent;
  std::map<EditorEntityId,std::vector<usize>> guiRoots;
  for(usize i=0;i<builder.state.guiRows.size();++i) {
    const auto &row=builder.state.guiRows[i];
    if(!row.target.node)guiRoots[row.target.owner].push_back(i);
    else guiChildrenByParent[{row.target.owner,row.target.component,row.parent}].push_back(i);
  }

  struct Frame final {
    EditorEntityId entity;
    u32 depth;
    usize gui=~usize{0};
  };
  std::vector<Frame> stack;
  const auto roots = document.childrenOf(document.root());
  for (usize index = roots.size(); index > 0; --index) stack.push_back({roots[index - 1], 0});

  u32 rows = 0;
  u32 skipped = 0;
  while (!stack.empty()) {
    const Frame frame = stack.back();
    stack.pop_back();
    if(frame.gui!=~usize{0}) {
      const auto &entry=builder.state.guiRows[frame.gui];
      const bool collapsed=std::find(builder.state.collapsedGui.begin(),builder.state.collapsedGui.end(),entry.target)!=builder.state.collapsedGui.end();
      const auto descendants=guiChildrenByParent.find({entry.target.owner,entry.target.component,entry.target.node});
      const bool children=descendants!=guiChildrenByParent.end();
      if(children&&(!collapsed||filtering))for(auto i=descendants->second.rbegin();i!=descendants->second.rend();++i)
        stack.push_back({frame.entity,frame.depth+1,*i});
      ++rows;if(skipped<builder.state.hierarchyScroll){++skipped;continue;}
      if(content.height<kRowHeight)continue;
      const auto row=takeTop(content,kRowHeight);const bool selected=builder.state.guiSelection==entry.target;
      if(selected)builder.list.addRect(row,theme.color.accent,theme.radius.thumb);
      auto inner=deflate(row,UiInsets::symmetric(theme.spacing.tiny,0));
      takeLeft(inner,std::min(float(frame.depth)*14.f,std::max(0.f,inner.width-80.f)));
      const auto fold=takeLeft(inner,28);
      if(children)builder.list.addImage(centred(fold,12,12),static_cast<UiImageId>(collapsed?UiIcon::EditorAuthorAdd:UiIcon::EditorAuthorChevron),selected?theme.color.accentInk:theme.color.textDim);
      const auto icon=!entry.target.node?UiIcon::UiInterfaceCanvas:entry.kind==ui::GuiKind::Joystick?UiIcon::UiJoystick:entry.kind==ui::GuiKind::ActionButton?UiIcon::UiActionButton:entry.kind==ui::GuiKind::LookArea?UiIcon::UiLookArea:entry.kind==ui::GuiKind::Image?UiIcon::AssetsTexture:ui::guiContainer(entry.kind)?UiIcon::UiAutoLayout:UiIcon::UiInterfaceCanvas;
      builder.list.addImage(centred(takeLeft(inner,22),15,15),static_cast<UiImageId>(icon),selected?theme.color.accentInk:theme.color.accent);
      builder.label(inner,entry.name.c_str(),selected?theme.color.accentInk:entry.error.empty()?theme.color.text:theme.color.warning,theme.type.body);
      builder.router.addRegion(row,widgetId(EditorWidget::GuiHierarchyRowBase)+entry.token);
      if(children)builder.router.addRegion(fold,widgetId(EditorWidget::GuiHierarchyCollapseBase)+entry.token);
      continue;
    }
    const EditorEntity *entity = document.find(frame.entity);
    if (entity == nullptr || (filtering && !matches.contains(frame.entity))) continue;
    const auto children = document.childrenOf(frame.entity);
    const bool collapsed = std::find(builder.state.collapsedEntities.begin(), builder.state.collapsedEntities.end(), frame.entity) != builder.state.collapsedEntities.end();
    if (!collapsed || filtering) for (usize index = children.size(); index > 0; --index)
      stack.push_back({children[index - 1], frame.depth + 1});
    const auto uiRoots=guiRoots.find(frame.entity);const bool guiChildren=uiRoots!=guiRoots.end();
    if(guiChildren&&(!collapsed||filtering))for(auto i=uiRoots->second.rbegin();i!=uiRoots->second.rend();++i)
      stack.push_back({frame.entity,frame.depth+1,*i});

    ++rows;
    // A rolagem descarta as primeiras linhas em vez de deslocar o desenho: a
    // lista é reconstruída todo frame, então não há nada para deslocar.
    if (skipped < builder.state.hierarchyScroll) {
      ++skipped;
      continue;
    }
    if (content.height < kRowHeight) continue;

    const UiRect row = takeTop(content, kRowHeight);
    const bool selected = builder.state.selection == frame.entity && !builder.state.guiSelection.valid();
    const bool secondary = !selected && builder.state.isSelected(frame.entity);
    if (selected) builder.list.addRect(row, theme.color.accent, theme.radius.thumb);
    else if (secondary) {
      // Selecionado, não ativo: faixa suave com a borda da cor de destaque.
      builder.list.addRect(row, withAlpha(theme.color.accent, .22f), theme.radius.thumb);
      builder.list.addRect({row.x,row.y+3,3,row.height-6}, theme.color.accent, 1);
    }
    else if (builder.isPressed(hierarchyRowWidget(frame.entity)))
      builder.list.addRect(row, theme.color.raised, theme.radius.thumb);
    // Ping (Unity): a linha pisca em amarelo enquanto dura.
    if (builder.state.pingEntity == frame.entity && builder.state.uiTime < builder.state.pingUntil) {
      const float phase = static_cast<float>(builder.state.pingUntil - builder.state.uiTime);
      const float alpha = .35f + .35f * std::abs(std::sin(phase * 6.f));
      builder.list.addRect(deflate(row, UiInsets::all(-1)), withAlpha(theme.color.warning, alpha), theme.radius.thumb);
    }

    UiRect rowContent = deflate(row, UiInsets::symmetric(theme.spacing.tiny, 0.0f));
    takeLeft(rowContent, std::min(static_cast<float>(frame.depth) * 14.0f, std::max(0.0f,rowContent.width-110.0f)));
    // Objeto numa camada escondida na vista: a linha fica apagada (Unity
    // SceneVisibility), mas continua selecionável pela Hierarquia.
    const bool layerHidden = (entity->layer < 32 && (builder.state.hiddenLayers & (1u << entity->layer))) ||
                             builder.state.sceneHiddenHas(frame.entity);
    const UiColor ink = selected ? theme.color.accentInk : layerHidden ? theme.color.textFaint : theme.color.text;
    const UiRect twisty = takeLeft(rowContent, 28.0f);
    if (!children.empty() || guiChildren)
      builder.list.addImage(centred(twisty, 12.0f, 12.0f),
                            static_cast<UiImageId>(collapsed ? UiIcon::EditorAuthorAdd : UiIcon::EditorAuthorChevron),
                            selected ? theme.color.accentInk : theme.color.textDim);
    builder.list.addImage(centred(takeLeft(rowContent, 22.0f), 15.0f, 15.0f),
                          static_cast<UiImageId>(iconForEntity(*entity)),
                          selected ? theme.color.accentInk : theme.color.accent);
    // Olho e mão (Unity: Scene visibility e Scene picking), só do editor. Com
    // filhos em estado diferente do pai, um ponto marca a mistura (pai visível
    // com filho oculto, pai oculto com filho visível; idem para a seleção).
    const UiRect eye = takeRight(rowContent, 26.0f);
    const UiRect hand = takeRight(rowContent, 26.0f);
    {
      const auto &s=builder.state;
      const bool hiddenSelf=s.sceneHiddenHas(frame.entity),pickOffSelf=s.scenePickOffHas(frame.entity);
      bool anyHidden=false,anyShown=false,anyPickOff=false,anyPickOn=false;
      if(!children.empty()) {
        std::vector<EditorEntityId> subtree;document.collectSubtree(frame.entity,subtree);
        for(const auto id:subtree) if(id!=frame.entity) {
          (s.sceneHiddenHas(id)?anyHidden:anyShown)=true;
          (s.scenePickOffHas(id)?anyPickOff:anyPickOn)=true;
        }
      }
      const UiColor quiet=selected?withAlpha(theme.color.accentInk,.45f):theme.color.textFaint;
      const UiColor loud=selected?theme.color.accentInk:theme.color.warning;
      builder.list.addImage(centred(eye,15.0f,15.0f),static_cast<UiImageId>(hiddenSelf?UiIcon::EditorAuthorEyeOff:UiIcon::EditorAuthorEye),
                            hiddenSelf?loud:quiet);
      if((!hiddenSelf && anyHidden) || (hiddenSelf && anyShown))
        builder.list.addRect({eye.right()-8,eye.y+eye.height*.5f-9,5,5},loud,2.5f);
      const UiRect handIcon=centred(hand,15.0f,15.0f);
      builder.list.addImage(handIcon,static_cast<UiImageId>(UiIcon::EditorAuthorPan),pickOffSelf?loud:quiet);
      if(pickOffSelf) builder.list.addLine({handIcon.x-1,handIcon.bottom()+1},{handIcon.right()+1,handIcon.y-1},loud,1.8f);
      if((!pickOffSelf && anyPickOff) || (pickOffSelf && anyPickOn))
        builder.list.addRect({hand.right()-8,hand.y+hand.height*.5f-9,5,5},loud,2.5f);
    }
    // O objeto em que o Inspector está travado leva o cadeado na linha.
    if (builder.state.inspectorLocked == frame.entity)
      builder.list.addImage(centred(takeRight(rowContent, 20.0f), 13.0f, 13.0f), static_cast<UiImageId>(UiIcon::SceneLock),
                            selected ? theme.color.accentInk : theme.color.accent);
    builder.label(rowContent, entity->name, ink, theme.type.body);

    // A linha PRIMEIRO, o olho DEPOIS: o roteador testa da última região para a
    // primeira, então quem entra depois fica por cima. O olho precisa ganhar do
    // fundo da linha, senão tocar nele seleciona em vez de alternar.
    builder.router.addRegion(row, hierarchyRowWidget(frame.entity));
    if (!children.empty() || guiChildren) builder.router.addRegion(twisty,widgetId(EditorWidget::HierarchyCollapseBase)+frame.entity);
    builder.router.addRegion(eye, hierarchyEyeWidget(frame.entity));
    builder.router.addRegion(hand, hierarchyPickWidget(frame.entity));
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
    // Multisseleção com valores diferentes neste eixo: traço (Unity).
    const bool mixed = builder.mixed("t." + std::to_string(row) + std::to_string(axis));
    builder.label(inner, mixed ? "\xE2\x80\x94" : buffer, mixed ? theme.color.textDim : theme.color.text, theme.type.numeric);
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
  if(type.id==std::string_view{"astra.physics2d.joint"}) {
    const auto rank=[](std::string_view group){return group=="Conexão"?0:(group=="Anchors"||group=="Âncoras")?1:2;};
    std::stable_sort(groups.begin(),groups.end(),[&](std::string_view a,std::string_view b){return rank(a)<rank(b);});
  }
  // Conexão de evento lê como frase: primeiro o gatilho, depois a resposta.
  if(&type==&scene::EventConnection::descriptor) {
    const auto rank=[](std::string_view group){return group=="Quando"?0:group=="Então"?1:2;};
    std::stable_sort(groups.begin(),groups.end(),[&](std::string_view a,std::string_view b){return rank(a)<rank(b);});
  }
  return groups;
}

void buildMaterialSlots(ScreenBuilder &builder,UiRect content,std::string_view query={}) {
  const auto &theme=builder.theme;const auto &state=builder.state;const auto &view=state.materialSlotView;
  if(!view.slots || content.height<40) return;
  // Material do projeto em Propriedades: sem slot nem alcance, só os campos.
  const bool assetMode=state.materialInspector.valid();
  // "—" no campo que difere entre os materiais escolhidos (só no recurso).
  const bool manyMaterials=assetMode && !builder.focusedWindow && !state.materialMixed.empty();
  const auto mixed=[&](std::string_view key) {return manyMaterials && state.materialMixedHas(key);};
  static const std::string dash="\xE2\x80\x94",differs="Valores diferentes";
  if(!assetMode) {
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
  }

  // R4: os quatro bindings de textura vêm antes dos números, na mesma lista
  // paginada. Cada linha mostra a textura efetiva no alcance e de onde ela vem.
  static constexpr const char *bindingNames[scene::MaterialTextureCount]{"Mapa: cor","Mapa: normal","Mapa: metal/rug.","Mapa: emissão"};
  static constexpr const char *surfaceNames[]{"Alfa","Corte alfa","Faces"};
  static constexpr const char *channelNames[]{"Oclusão","Textura de oclusão","Força da oclusão","Canais","Mapa normal","Origem do alfa","Isolar na prévia"};
  const u32 textureRows=scene::MaterialTextureCount;
  // R4: três linhas de superfície (alfa, corte, faces) depois das texturas.
  const u32 surfaceRows=3;
  // R4: sete linhas de oclusão, canais, normal, alfa e isolamento.
  // Isolar na prévia vale para um slot de objeto; o recurso sozinho não tem prévia.
  const u32 channelRows=assetMode?6:7;
  const u32 count=textureRows+surfaceRows+channelRows+static_cast<u32>(scene::meshRendererNumbers.size());
  const auto search=editorSearchKey(query);
  std::vector<u32> matches;
  for(u32 entry=0;entry<count;++entry) {
    std::string_view name=entry<textureRows?bindingNames[entry]:
      entry<textureRows+surfaceRows?surfaceNames[entry-textureRows]:
      entry<textureRows+surfaceRows+channelRows?channelNames[entry-textureRows-surfaceRows]:
      scene::meshRendererNumbers[entry-textureRows-surfaceRows-channelRows].name;
    if(search.empty()||editorSearchKey(name).find(search)!=std::string::npos) matches.push_back(entry);
  }
  if(!search.empty()) {
    builder.label(takeTop(content,24),std::to_string(matches.size())+(matches.size()==1?" propriedade encontrada":" propriedades encontradas"),theme.color.textMuted,theme.type.caption);
    if(matches.empty()) {builder.label(content,"Nenhuma propriedade encontrada",theme.color.textDim,theme.type.caption);return;}
  }
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage),page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 item=page*perPage;item<matches.size() && item<(page+1)*perPage;++item) {
    const u32 entry=matches[item];
    auto row=takeTop(content,std::min(40.0f,content.height));
    if(entry<textureRows) {
      builder.label(takeLeft(row,row.width*.42f),bindingNames[entry],theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets::symmetric(8,2));
      const bool different=mixed("tex"+std::to_string(entry));
      builder.label(takeTop(inner,inner.height*.55f),different?dash:view.textureNames[entry],different?theme.color.textDim:theme.color.text,theme.type.caption);
      builder.label(inner,different?differs:view.textureOrigins[entry],theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(row,widgetId(EditorWidget::MaterialTextureBase)+entry);
      continue;
    }
    if(entry<textureRows+surfaceRows) {
      const u32 kind=entry-textureRows;
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
          builder.label(inner,mixed("cutoff")?dash:std::string(cutoff),mixed("cutoff")?theme.color.textDim:theme.color.text,theme.type.numeric,UiAlign::Center);
        } else {
          builder.label(inner,view.cutoffLabel,theme.color.textMuted,theme.type.caption);
        }
        continue;
      }
      const bool alpha=kind==0,different=mixed(alpha?"alpha":"sides");
      builder.label(takeTop(inner,inner.height*.55f),different?dash:alpha?view.alphaLabel:view.sidesLabel,different?theme.color.textDim:theme.color.text,theme.type.caption);
      builder.label(inner,different?differs:alpha?view.alphaOrigin:view.sidesOrigin,theme.color.textMuted,theme.type.caption);
      // Faces sem culling no aparelho não recebe toque: não haveria efeito visível.
      if(alpha || state.materialCulling)
        builder.router.addRegion(row,widgetId(alpha?EditorWidget::MaterialAlphaCycle:EditorWidget::MaterialSidesCycle));
      continue;
    }
    if(entry<textureRows+surfaceRows+channelRows) {
      const u32 kind=entry-textureRows-surfaceRows;
      builder.label(takeLeft(row,row.width*.42f),channelNames[kind],theme.color.textDim,theme.type.caption);
      builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets::symmetric(8,2));
      if(kind==2) {
        const auto down=takeLeft(inner,32),up=takeRight(inner,32);
        builder.label(down,"-",theme.color.text,theme.type.body,UiAlign::Center);
        builder.label(up,"+",theme.color.text,theme.type.body,UiAlign::Center);
        builder.router.addRegion(down,widgetId(EditorWidget::MaterialOcclusionStrengthDown));
        builder.router.addRegion(up,widgetId(EditorWidget::MaterialOcclusionStrengthUp));
        builder.label(inner,mixed("ch2")?dash:view.occlusionStrengthLabel,mixed("ch2")?theme.color.textDim:theme.color.text,theme.type.numeric,UiAlign::Center);
        continue;
      }
      if(kind==3) {
        const float third=inner.width/3;
        for(u32 channel=0;channel<3;++channel) {
          const auto box=channel<2?takeLeft(inner,third):inner;
          const bool different=mixed("ch3."+std::to_string(channel));
          builder.label(box,different?dash:view.channelLabels[channel],different?theme.color.textDim:theme.color.text,theme.type.caption,UiAlign::Center);
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
      const bool different=mixed("ch"+std::to_string(kind));
      builder.label(takeTop(inner,inner.height*.55f),different?dash:value,different?theme.color.textDim:theme.color.text,theme.type.caption);
      builder.label(inner,different?differs:origin,theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(row,widgetId(channelWidgets[kind]));
      continue;
    }
    const u32 field=entry-textureRows-surfaceRows-channelRows;
    builder.label(takeLeft(row,row.width*.62f),scene::meshRendererNumbers[field].name,theme.color.textDim,theme.type.caption);
    char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(view.values[field]));
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    const bool different=mixed("num"+std::to_string(field));
    builder.label(row,different?dash:std::string(value),different?theme.color.textDim:theme.color.text,theme.type.numeric,UiAlign::Center);
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

// Material do projeto em Propriedades (Unity 6000.0 Material Inspector): o
// cartão do recurso (nome, caminho, revisão e usos na cena, com Ping a cada
// toque) e, embaixo, os mesmos campos do alcance Compartilhado de um slot —
// texturas e amostragem, superfície, canais e números. Toda edição muda todos
// os slots que usam o material, pelo histórico.
void buildTexturePicker(ScreenBuilder &builder,UiRect content);
void buildMaterialAssetInspector(ScreenBuilder &builder,const UiRect &panel) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  list.addRect(panel,theme.color.surface);
  router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(theme.spacing.small));
  auto header=takeTop(content,kPanelHeaderHeight);
  const auto back=takeLeft(header,32);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  router.addRegion(back,widgetId(EditorWidget::MaterialInspectorClose));
  if(!builder.focusedWindow) builder.iconButton(takeRight(header,32),UiIcon::UiPanelRight,widgetId(EditorWidget::AssetInspectorFocus));
  list.addImage(centred(takeLeft(header,26),18,18),static_cast<UiImageId>(UiIcon::AssetsMaterial),theme.color.accent);
  const float half=header.height*.5f;
  // Vários materiais escolhidos: este é o ativo e a edição vale para todos.
  const bool several=!builder.focusedWindow && state.multiAsset.kind==EditorScreenState::MultiAssetView::Kind::Materials &&
                     state.multiAsset.items.size()>1;
  builder.label({header.x,header.y,header.width,half},several?std::to_string(state.multiAsset.items.size())+" materiais":state.materialSlotView.name,theme.color.text,theme.type.cardName);
  if(several)
    builder.label({header.x,header.y+half,header.width,half},"Ativo: "+state.materialSlotView.name,theme.color.accent,theme.type.caption);
  else
    builder.label({header.x,header.y+half,header.width,half},
                  ("Material do projeto · rev. "+std::to_string(state.materialInspectorRevision)).c_str(),theme.color.textDim,theme.type.caption);
  // Cartão de usos.
  auto uses=takeTop(content,44);takeTop(content,4);
  list.addRect(uses,theme.color.raised,theme.radius.control);
  auto inner=deflate(uses,UiInsets{10,4,8,4});
  const u32 objects=static_cast<u32>(state.materialInspectorObjects.size());
  const auto ping=takeRight(inner,objects?92.f:0.f);
  if(objects) {
    list.addRect(centred(ping,88,28),theme.color.silhouette,theme.radius.control);
    builder.label(ping,"Localizar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(ping,widgetId(EditorWidget::MaterialInspectorUses));
  }
  const std::string usage=(several?std::string("Ativo · "):std::string())+(objects?std::to_string(state.materialInspectorSlots)+(state.materialInspectorSlots==1?" slot":" slots")+" em "+
      std::to_string(objects)+(objects==1?" objeto":" objetos"):std::string("Sem usos nesta cena"));
  builder.label({inner.x,inner.y,inner.width,inner.height*.5f},usage.c_str(),objects?theme.color.text:theme.color.textDim,theme.type.caption);
  list.pushClip(inner);
  builder.label({inner.x,inner.y+inner.height*.5f,inner.width,inner.height*.5f},state.materialInspectorPath.c_str(),theme.color.textMuted,theme.type.caption);
  list.popClip();
  if(state.texturePicker) {buildTexturePicker(builder,content);return;}
  buildMaterialSlots(builder,content);
}

// Mapa HDRI do projeto em Propriedades (Unity 6000.0 Texture Import Settings
// de um cubemap latitude-longitude, adaptado ao importador HDRI da Astra): a
// prévia do panorama com exposição só de prévia, os derivados, os usos e a
// receita de importação; Aplicar reimporta pela trilha do botão Reimportar.
void buildEnvironmentAssetInspector(ScreenBuilder &builder,const UiRect &panel) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  list.addRect(panel,theme.color.surface);
  router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(theme.spacing.small));
  auto header=takeTop(content,kPanelHeaderHeight);
  const auto back=takeLeft(header,32);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  router.addRegion(back,widgetId(EditorWidget::EnvironmentInspectorClose));
  if(!builder.focusedWindow) builder.iconButton(takeRight(header,32),UiIcon::UiPanelRight,widgetId(EditorWidget::AssetInspectorFocus));
  list.addImage(centred(takeLeft(header,26),18,18),static_cast<UiImageId>(UiIcon::LightingSceneLighting),theme.color.accent);
  const auto slash=state.environmentInspectorPath.rfind('/');
  const std::string name=slash==std::string::npos?state.environmentInspectorPath:state.environmentInspectorPath.substr(slash+1);
  const bool many=!builder.focusedWindow && state.multiAsset.kind==EditorScreenState::MultiAssetView::Kind::EnvironmentMaps;
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},many?state.multiAsset.title:name,theme.color.text,theme.type.cardName);
  builder.label({header.x,header.y+half,header.width,half},many?"Ativo: "+name:std::string("Mapa HDRI · céu, luz difusa e reflexos"),theme.color.textDim,theme.type.caption);
  // Prévia 2:1 com exposição.
  auto preview=takeTop(content,std::min(content.width*.5f,96.f));takeTop(content,4);
  list.addRect(preview,theme.color.voidBlack,theme.radius.control);
  if(!state.environmentPreview.isEmpty()) {
    const auto &texels=state.environmentPreview;
    const float fit=std::min(preview.width/texels.width,preview.height/texels.height);
    const float w=texels.width*fit,h=texels.height*fit;
    list.addPreviewImage({preview.x+(preview.width-w)*.5f,preview.y+(preview.height-h)*.5f,w,h},texels);
  } else builder.label(preview,"Sem derivados carregados",theme.color.textDim,theme.type.caption,UiAlign::Center);
  {
    const UiRect bar{preview.right()-124,preview.bottom()-30,120,26};
    list.addRect(bar,withAlpha(theme.color.voidBlack,.75f),13);
    auto inner=bar;
    const auto minus=takeLeft(inner,30),plus=takeRight(inner,30);
    builder.label(minus,"-",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(plus,"+",theme.color.text,theme.type.body,UiAlign::Center);
    char ev[24];std::snprintf(ev,sizeof ev,"EV %+.1f",static_cast<double>(state.environmentExposure));
    builder.label(inner,ev,theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(minus,widgetId(EditorWidget::EnvironmentExposureDown));
    router.addRegion(plus,widgetId(EditorWidget::EnvironmentExposureUp));
  }
  // Usos.
  auto uses=takeTop(content,40);takeTop(content,4);
  list.addRect(uses,theme.color.raised,theme.radius.control);
  auto inner=deflate(uses,UiInsets{10,4,8,4});
  const u32 objects=static_cast<u32>(state.environmentObjects.size());
  if(objects) {
    const auto ping=takeRight(inner,92);
    list.addRect(centred(ping,88,28),theme.color.silhouette,theme.radius.control);
    builder.label(ping,"Localizar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(ping,widgetId(EditorWidget::EnvironmentInspectorUses));
  }
  const std::string usage=std::to_string(objects)+(objects==1?" ambiente":" ambientes")+" · "+
      std::to_string(state.environmentProfiles)+(state.environmentProfiles==1?" perfil":" perfis");
  builder.label(inner,usage.c_str(),objects?theme.color.text:theme.color.textDim,theme.type.caption);
  // Rodapé: reverter e aplicar.
  const auto &draft=state.environmentDraft,&saved=state.environmentSaved;
  const bool dirty=(many && state.environmentPending) || draft.panoramaWidth!=saved.panoramaWidth || draft.specularSize!=saved.specularSize ||
      draft.specularSamples!=saved.specularSamples || draft.brdfSize!=saved.brdfSize || draft.brdfSamples!=saved.brdfSamples;
  auto footer=takeBottom(content,36);takeBottom(content,4);
  const float bw=(footer.width-6)/2;
  const UiRect revert{footer.x,footer.y,bw,footer.height},apply{footer.x+bw+6,footer.y,bw,footer.height};
  list.addRect(revert,theme.color.raised,theme.radius.control);
  builder.label(revert,"Reverter",dirty?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
  list.addRect(apply,dirty?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(apply,"Aplicar e reimportar",dirty?theme.color.accentInk:theme.color.textFaint,theme.type.caption,UiAlign::Center);
  if(dirty) {router.addRegion(revert,widgetId(EditorWidget::EnvironmentRecipeRevert));router.addRegion(apply,widgetId(EditorWidget::EnvironmentRecipeApply));}
  // Receita paginada como o resto do Inspector; os derivados ocupam o que
  // sobrar depois de pelo menos duas linhas dela.
  const float rowHeight=34;
  const u32 derivedLines=static_cast<u32>(std::min<usize>(state.environmentDerived.size(),
      static_cast<usize>(std::max(0.f,(content.height-2*rowHeight-26-4)/18))));
  for(u32 i=0;i<derivedLines;++i) {
    auto row=takeTop(content,18);
    list.pushClip(row);builder.label(row,state.environmentDerived[i].c_str(),theme.color.textMuted,theme.type.caption);list.popClip();
  }
  takeTop(content,4);
  const u32 perPage=std::max(1u,static_cast<u32>((content.height-26)/rowHeight));
  const u32 pages=(5+perPage-1)/perPage,page=std::min(state.propertyPage,pages-1);
  auto pager=pages>1?takeBottom(content,26):UiRect{};
  if(pages>1) {
    const auto previous=takeLeft(pager,36),next=takeRight(pager,36);
    builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::PropertyNext));
    builder.label(pager,("Receita "+std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  const struct {const char *name;u32 value,saved;} recipe[]{
    {"Panorama (largura)",draft.panoramaWidth,saved.panoramaWidth},{"Reflexão (tamanho)",draft.specularSize,saved.specularSize},
    {"Reflexão (amostras)",draft.specularSamples,saved.specularSamples},{"BRDF (tamanho)",draft.brdfSize,saved.brdfSize},
    {"BRDF (amostras)",draft.brdfSamples,saved.brdfSamples}};
  for(u32 i=page*perPage;i<5 && i<(page+1)*perPage && content.height>=rowHeight;++i) {
    auto row=takeTop(content,rowHeight);row.height-=2;
    builder.label(takeLeft(row,row.width*.48f),recipe[i].name,theme.color.textDim,theme.type.caption);
    list.addRect(row,theme.color.raised,theme.radius.control);
    const auto minus=takeLeft(row,30),plus=takeRight(row,30);
    builder.label(minus,"-",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(plus,"+",theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(minus,widgetId(EditorWidget::EnvironmentRecipeDownBase)+i);
    router.addRegion(plus,widgetId(EditorWidget::EnvironmentRecipeUpBase)+i);
    if(many && (state.environmentMixed&(1u<<i))) router.addRegion(row,widgetId(EditorWidget::EnvironmentRecipeUpBase)+i);
    const bool changed=recipe[i].value!=recipe[i].saved;
    builder.label(row,many && (state.environmentMixed&(1u<<i))?"—":std::to_string(recipe[i].value),changed?theme.color.accent:theme.color.text,theme.type.numeric,UiAlign::Center);
  }
}

// Perfil de ambiente do projeto em Propriedades (Unity 6000.0 Volume Profile):
// abas pelos grupos do esquema do componente Ambiente e, em cada uma, só o que
// o perfil guarda — interruptores, opções que ciclam, cores e números pelo
// teclado numérico, o mapa HDRI. Sem consumidor neste aparelho, a linha fica
// apagada e não recebe toque; toda edição muda os ambientes que usam o perfil.
void buildProfileAssetInspector(ScreenBuilder &builder,const UiRect &panel) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  list.addRect(panel,theme.color.surface);
  router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(theme.spacing.small));
  auto header=takeTop(content,kPanelHeaderHeight);
  const auto back=takeLeft(header,32);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);
  router.addRegion(back,widgetId(EditorWidget::ProfileInspectorClose));
  if(!builder.focusedWindow) builder.iconButton(takeRight(header,32),UiIcon::UiPanelRight,widgetId(EditorWidget::AssetInspectorFocus));
  list.addImage(centred(takeLeft(header,26),18,18),static_cast<UiImageId>(UiIcon::LightingSceneEffects),theme.color.accent);
  const u32 objects=static_cast<u32>(state.profileObjects.size());
  const bool many=!builder.focusedWindow && state.multiAsset.kind==EditorScreenState::MultiAssetView::Kind::Profiles;
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},many?state.multiAsset.title:state.profileInspectorName,theme.color.text,theme.type.cardName);
  const std::string sub="Perfil de ambiente · rev. "+std::to_string(state.profileInspectorRevision)+" · "+std::to_string(objects)+
      (objects==1?" ambiente":" ambientes");
  builder.label({header.x,header.y+half,header.width,half},many?"Ativo: "+state.profileInspectorName:sub,theme.color.textDim,theme.type.caption);
  // Grupo do esquema num seletor de uma linha (as abas em quebra comiam metade
  // do Inspector estreito); Localizar ao lado.
  {
    auto line=takeTop(content,34);takeTop(content,6);
    if(objects) {
      const auto ping=takeRight(line,88);
      list.addRect(deflate(ping,UiInsets{4,2,0,2}),theme.color.raised,theme.radius.control);
      builder.label(deflate(ping,UiInsets{4,0,0,0}),"Localizar",theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(ping,widgetId(EditorWidget::ProfileInspectorUses));
    }
    const u32 groups=static_cast<u32>(state.profileGroups.size());
    list.addRect(line,theme.color.accent,17);
    const auto previous=takeLeft(line,34),next=takeRight(line,34);
    builder.label(previous,"<",state.profileGroup?theme.color.accentInk:withAlpha(theme.color.accentInk,.35f),theme.type.body,UiAlign::Center);
    builder.label(next,">",state.profileGroup+1<groups?theme.color.accentInk:withAlpha(theme.color.accentInk,.35f),theme.type.body,UiAlign::Center);
    if(state.profileGroup) router.addRegion(previous,widgetId(EditorWidget::ProfileGroupBase)+state.profileGroup-1);
    if(state.profileGroup+1<groups) router.addRegion(next,widgetId(EditorWidget::ProfileGroupBase)+state.profileGroup+1);
    const std::string title=groups?state.profileGroups[state.profileGroup]+"  "+std::to_string(state.profileGroup+1)+"/"+std::to_string(groups):std::string("Sem grupos");
    builder.label(line,title.c_str(),theme.color.accentInk,theme.type.caption,UiAlign::Center);
  }
  // Linhas, paginadas como o resto do Inspector.
  const float rowHeight=40;
  const u32 count=static_cast<u32>(state.profileRows.size());
  const u32 perPage=std::max(1u,static_cast<u32>((content.height-28)/rowHeight));
  const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,26):UiRect{};
  using Kind=EditorScreenState::ProfileRow::Kind;
  for(u32 i=page*perPage;i<count && i<(page+1)*perPage;++i) {
    const auto &row=state.profileRows[i];
    auto rect=takeTop(content,rowHeight);rect.height-=4;
    const auto ink=row.editable?theme.color.text:theme.color.textFaint;
    builder.label(takeLeft(rect,rect.width*.46f),row.label.c_str(),row.editable?theme.color.textDim:theme.color.textFaint,theme.type.caption);
    if(row.kind==Kind::Boolean && !row.mixed && row.editable) {
      builder.toggle(takeRight(rect,52),row.on,widgetId(EditorWidget::ProfileRowBase)+i);
      continue;
    }
    list.addRect(rect,theme.color.raised,theme.radius.control);
    auto inner=deflate(rect,UiInsets{8,0,8,0});
    if(row.kind==Kind::Triple && row.color && !row.mixed) {
      const auto swatch=centred(takeLeft(inner,22),18,18);
      const auto channel=[&](float v){return static_cast<u32>(std::clamp(std::pow(std::max(v,0.f),1/2.2f),0.f,1.f)*255+.5f);};
      list.addRect(swatch,0xff000000u|(channel(row.rgb[2])<<16)|(channel(row.rgb[1])<<8)|channel(row.rgb[0]),4);
      takeLeft(inner,6);
    }
    if(row.kind==Kind::Enum || row.kind==Kind::EnvironmentMap) {
      const auto arrow=takeRight(inner,18);
      builder.label(arrow,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
    }
    list.pushClip(inner);
    builder.label(inner,row.mixed?"—":row.kind==Kind::Boolean?(row.on?"Ligado":"Desligado"):row.value,ink,row.kind==Kind::Number||row.kind==Kind::Triple?theme.type.numeric:theme.type.caption,
                  row.kind==Kind::Number?UiAlign::Center:UiAlign::Start);
    list.popClip();
    if(row.editable) router.addRegion(rect,widgetId(EditorWidget::ProfileRowBase)+i);
  }
  if(!count) builder.label(content,"Nada deste grupo no perfil",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(pages>1) {
    const auto previous=takeLeft(footer,36),next=takeRight(footer,36);
    builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::PropertyNext));
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
                (state.materialInspector.valid()?" · material do projeto":state.materialShared?" · compartilhado":" · esta instância"),
                theme.color.text,theme.type.caption);
  const UiRect window=content;
  const u32 rows=static_cast<u32>(state.projectTextureNames.size());
  const bool narrow=window.width<320;
  const float transformHeight=narrow?216.f:108.f;
  const float height=96+(state.textureBinding<scene::MaterialTextureCount?
      40+(state.textureSamplerEditable?0:22)+transformHeight:22)+std::max(40.f,48.f*rows);
  const float scroll=std::clamp(state.texturePickerScroll,0.f,std::max(0.f,height-window.height));
  builder.router.addRegion(window,widgetId(EditorWidget::TexturePickerScroll));
  builder.list.pushClip(window);
  const auto region=[&](const UiRect &rect,u32 widget) {
    const auto hit=intersect(rect,window);if(!hit.isEmpty()) builder.router.addRegion(hit,widget);
  };
  content={window.x,window.y-scroll,window.width-5,height};
  // R4: amostragem do binding. O conjunto de UV vale para qualquer textura;
  // repetição e filtro são o sampler da textura do projeto e não recebem toque
  // quando o binding usa a textura da fonte. Vem depois de Herdar e Sem textura:
  // as escolhas principais do seletor não podem sair da tela por causa dele.
  const auto mixed=[&](std::string_view key) {
    return !builder.focusedWindow && state.materialInspector.valid() &&
        state.materialMixedHas("uv"+std::to_string(state.textureBinding)+"."+std::string(key));
  };
  const auto samplingControls=[&]() {
  if(content.height>=40) {
    auto sampling=takeTop(content,40);
    const float third=sampling.width/3;
    const auto button=[&](UiRect box,const std::string &text,bool enabled,u32 widget) {
      box=deflate(box,UiInsets::all(2));
      builder.list.addRect(box,theme.color.raised,theme.radius.control);
      builder.label(box,text,enabled?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(enabled) region(box,widget);
    };
    button(takeLeft(sampling,third),mixed("set")?"UV: —":state.textureUvLabel,true,widgetId(EditorWidget::TextureSamplingUv));
    button(takeLeft(sampling,third),mixed("wrap")?"Rep.: —":state.textureWrapLabel,state.textureSamplerEditable,widgetId(EditorWidget::TextureSamplingWrap));
    button(sampling,mixed("filter")?"Filtro: —":state.textureFilterLabel,state.textureSamplerEditable,widgetId(EditorWidget::TextureSamplingFilter));
    if(!state.textureSamplerEditable && content.height>=22)
      builder.label(takeTop(content,22),"Repetição e filtro valem só com textura do projeto",theme.color.textMuted,theme.type.caption);
  }
  // R4: transformação de UV do binding. Vale com qualquer textura: o shader a aplica.
  if(content.height>=transformHeight) {
    const auto stepper=[&](UiRect row,const std::string &text,u32 field) {
      const auto down=deflate(takeLeft(row,34),UiInsets::all(2)),up=deflate(takeRight(row,34),UiInsets::all(2));
      builder.list.addRect(down,theme.color.raised,theme.radius.control);
      builder.label(down,"-",theme.color.text,theme.type.body,UiAlign::Center);
      region(down,widgetId(EditorWidget::TextureUvStepBase)+field*2);
      builder.list.addRect(up,theme.color.raised,theme.radius.control);
      builder.label(up,"+",theme.color.text,theme.type.body,UiAlign::Center);
      region(up,widgetId(EditorWidget::TextureUvStepBase)+field*2+1);
      static constexpr const char *keys[]{"offset0","offset1","scale0","scale1","rotation"};
      static constexpr const char *labels[]{"Desl. U —","Desl. V —","Esc. U —","Esc. V —","Rot. —"};
      builder.label(row,mixed(keys[field])?labels[field]:text,theme.color.text,theme.type.caption,UiAlign::Center);
    };
    UiRect reset;
    if(narrow) {
      for(u32 field=0;field<5;++field) stepper(takeTop(content,36),state.textureUvLabels[field],field);
      reset=deflate(takeTop(content,36),UiInsets::all(2));
    } else {
      auto offsets=takeTop(content,36);
      stepper(takeLeft(offsets,offsets.width*.5f),state.textureUvLabels[0],0);
      stepper(offsets,state.textureUvLabels[1],1);
      auto scales=takeTop(content,36);
      stepper(takeLeft(scales,scales.width*.5f),state.textureUvLabels[2],2);
      stepper(scales,state.textureUvLabels[3],3);
      auto rotation=takeTop(content,36);
      stepper(takeLeft(rotation,rotation.width*.5f),state.textureUvLabels[4],4);
      reset=deflate(rotation,UiInsets::all(2));
    }
    builder.list.addRect(reset,theme.color.raised,theme.radius.control);
    builder.label(reset,"Zerar transformação",theme.color.text,theme.type.caption,UiAlign::Center);
    region(reset,widgetId(EditorWidget::TextureUvReset));
  }
  };
  const auto option=[&](const std::string &label,const std::string &detail,u32 widget) {
    if(content.height<48) return;
    auto row=takeTop(content,48);const auto hit=row;
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    takeLeft(row,12);
    builder.label(takeTop(row,25),label,theme.color.text,theme.type.body);
    builder.label(row,detail,theme.color.textMuted,theme.type.caption);
    region(hit,widget);
  };
  option("Herdar",state.materialShared?"volta à textura da fonte":"do material do projeto, senão da fonte",
         widgetId(EditorWidget::TextureUseInherited));
  option("Sem textura","o binding fica só com os fatores",widgetId(EditorWidget::TextureUseNone));
  if(state.textureBinding<scene::MaterialTextureCount) samplingControls();
  else if(content.height>=22)
    builder.label(takeTop(content,22),"A oclusão usa o conjunto de UV e a amostragem do mapa metal/rugosidade",theme.color.textMuted,theme.type.caption);
  if(!rows) {
    builder.label(takeTop(content,40),"Nenhuma textura no projeto. Use Importar textura em Arquivos.",
                  theme.color.textMuted,theme.type.caption);
  }
  for(u32 row=0;row<rows;++row) {
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
    region(hit,widgetId(EditorWidget::TextureChoiceBase)+row);
    const auto viewButton=deflate(viewArea,UiInsets::all(4));
    builder.list.addRect(viewButton,theme.color.raised,theme.radius.control);
    builder.label(viewButton,"Ver",theme.color.text,theme.type.caption,UiAlign::Center);
    region(viewButton,widgetId(EditorWidget::TextureViewBase)+row);
  }
  builder.list.popClip();
  if(builder.layout) {
    const u32 scope=builder.focusedWindow?1u:0u;
    builder.layout->texturePickerContent[scope]=height;builder.layout->texturePickerWindow[scope]=window.height;
  }
  if(height>window.height) {
    const float thumb=std::max(24.f,window.height*window.height/height);
    const float offset=scroll/(height-window.height)*(window.height-thumb);
    builder.list.addRect({window.right()-3,window.y+offset,3,thumb},theme.color.line,1.5f);
  }
}

// R4: visualizador de textura. A imagem vem do atlas de prévia com o nível de
// mip e o canal escolhidos; os dados do arquivo ficam embaixo.
// Inspector de textura em cartões, o mesmo para texturas do projeto e das
// fontes. Referências: prévia e import settings do Inspector da Unity, doca
// Import do Godot e Detalhes do editor de textura da Unreal. Cabeçalho (nome,
// trilha de pastas, tipo) e rodapé de ações ficam fixos; os cartões rolam entre
// eles e cada um recolhe pelo título.
namespace {
constexpr float kTexCardGap=6.0f,kTexCardHeader=26.0f,kTexCardPad=8.0f,kTexField=22.0f,kTexFieldLabel=13.0f;
constexpr UiTypeStyle kTexCardTitle{11.0f,0.0f,1.2f,false,true};
constexpr UiTypeStyle kTexValue{12.0f,0.0f,1.2f,false,true};

usize utf8Start(const std::string &text,usize at) {
  while(at>0 && at<text.size() && (static_cast<u8>(text[at])&0xC0u)==0x80u) --at;
  return at;
}
usize utf8Next(const std::string &text,usize at) {
  while(at<text.size() && (static_cast<u8>(text[at])&0xC0u)==0x80u) ++at;
  return at;
}
// Nomes de textura diferem no fim (_BaseColor.png, _Normal.png): corta o meio.
// O atlas da fonte vai só até Latin-1, então a reticência é de três pontos.
std::string fitMiddle(const UiDrawList &list,const std::string &text,float width,const UiTypeStyle &style) {
  const auto &metrics=list.fontMetrics();
  if(width<=0.0f || measureTextWidth(text,metrics,style)<=width) return text;
  for(usize keep=text.size();keep>1;--keep) {
    const usize head=utf8Start(text,(keep+1)/2),tail=utf8Next(text,text.size()-keep/2);
    if(tail<head) continue;
    std::string candidate=text.substr(0,head)+"..."+text.substr(tail);
    if(measureTextWidth(candidate,metrics,style)<=width) return candidate;
  }
  return "...";
}

// Cartões desenhados numa coluna que rola dentro de `window`: o desenho é
// recortado pela janela e o toque só existe na parte visível.
struct TextureCards {
  ScreenBuilder &b;
  UiRect window;
  void region(const UiRect &rect,u32 widget) const {
    const auto hit=intersect(rect,window);
    if(!hit.isEmpty()) b.router.addRegion(hit,widget);
  }
  static u32 sub(u32 offset) {return widgetId(EditorWidget::TextureInspectorBase)+offset;}
};

// Abre um cartão: fundo, borda, ícone, título e seta; tocar no título recolhe.
// `body` fica vazio quando recolhido.
void textureCard(const TextureCards &c,UiRect &column,detail::TextureInspectorCard card,UiIcon icon,const std::string &title,
                 float bodyHeight,UiRect &body,std::string_view note={}) {
  auto &b=c.b;const auto &theme=b.theme;
  const bool open=!(b.state.textureCardsClosed&(1u<<card));
  auto frame=takeTop(column,kTexCardHeader+(open?bodyHeight+kTexCardPad:0.0f));
  takeTop(column,kTexCardGap);
  b.list.addRect(frame,theme.color.surface,theme.radius.card);
  b.list.addBorder(frame,theme.color.lineSoft,1.0f,theme.radius.card);
  const auto header=takeTop(frame,kTexCardHeader);
  auto inner=deflate(header,UiInsets::symmetric(kTexCardPad,0));
  b.list.addImage(centred(takeLeft(inner,16),14,14),static_cast<UiImageId>(icon),theme.color.accent);
  takeLeft(inner,6);
  b.list.addImage(centred(takeRight(inner,14),12,12),static_cast<UiImageId>(open?UiIcon::UiChevronDown:UiIcon::UiChevronRight),
                  theme.color.textMuted);
  if(!note.empty()) b.label(takeRight(inner,std::min(inner.width*.45f,measureTextWidth(note,b.list.fontMetrics(),theme.type.caption)+6)),
                            note,theme.color.accent,theme.type.caption,UiAlign::End);
  b.label(inner,title,theme.color.text,kTexCardTitle);
  c.region(header,TextureCards::sub(detail::TextureInspectorSection+card));
  body=open?deflate(frame,UiInsets{kTexCardPad,0,kTexCardPad,kTexCardPad}):UiRect{};
}

float textureFieldHeight(bool stacked) {return stacked?kTexFieldLabel+kTexField:kTexField;}

// Célula da grade: rótulo discreto em cima do campo (painel estreito) ou ao
// lado (painel largo). Devolve o campo, para setas dentro dele.
UiRect textureField(const TextureCards &c,UiRect cell,std::string_view label,const std::string &value,bool stacked,
                    u32 widget=0,UiIcon trailing=UiIcon::None) {
  auto &b=c.b;const auto &theme=b.theme;
  auto field=cell;
  if(stacked) b.label(takeTop(field,kTexFieldLabel),label,theme.color.textMuted,theme.type.caption);
  else b.label(takeLeft(field,field.width*.42f),label,theme.color.textMuted,theme.type.caption);
  field.height=kTexField;
  b.list.addRect(field,theme.color.canvas,theme.radius.control);
  if(widget) b.list.addBorder(field,b.isPressed(widget)?theme.color.accent:theme.color.lineSoft,1.0f,theme.radius.control);
  auto text=deflate(field,UiInsets::symmetric(6,0));
  if(trailing!=UiIcon::None)
    b.list.addImage(centred(takeRight(text,12),11,11),static_cast<UiImageId>(trailing),widget?theme.color.textDim:theme.color.textMuted);
  if(!value.empty()) b.label(text,fitMiddle(b.list,value,text.width,theme.type.caption),theme.color.text,theme.type.caption);
  if(widget) c.region(field,widget);
  return field;
}

// Botão de ação: primário cheio na cor de destaque, secundário em relevo;
// desabilitado apagado e sem toque.
void textureAction(ScreenBuilder &b,UiRect box,UiIcon icon,std::string_view text,u32 widget,bool primary,bool enabled=true) {
  const auto &theme=b.theme;
  const UiColor fill=primary&&enabled?theme.color.accent:(b.isPressed(widget)?theme.color.line:theme.color.raised);
  const UiColor ink=primary&&enabled?theme.color.accentInk:enabled?theme.color.text:theme.color.textFaint;
  b.list.addRect(box,fill,theme.radius.control);
  const float textWidth=measureTextWidth(text,b.list.fontMetrics(),theme.type.caption);
  const float total=std::min(box.width-8,14+5+textWidth);
  UiRect row{box.x+(box.width-total)*.5f,box.y,total,box.height};
  b.list.addImage(centred(takeLeft(row,14),13,13),static_cast<UiImageId>(icon),ink);
  takeLeft(row,5);
  b.label(row,text,ink,theme.type.caption);
  if(enabled) b.router.addRegion(box,widget);
}

// Chips de "Usado por": largura pelo texto, quebra de linha, e no máximo três
// linhas até o usuário pedir todos.
struct TextureChip {UiRect box;u32 index;bool more;};
std::vector<TextureChip> layoutTextureChips(const ScreenBuilder &b,float width,bool expanded,float &height) {
  const auto &state=b.state;const auto &theme=b.theme;
  constexpr float chipHeight=22,gap=4;constexpr u32 collapsedRows=3;
  std::vector<TextureChip> chips;
  float x=0,y=0;u32 row=0;
  const auto widthOf=[&](std::string_view text) {
    return std::min(width,8+11+4+measureTextWidth(text,b.list.fontMetrics(),theme.type.caption)+9);
  };
  const auto count=static_cast<u32>(state.textureUserLabels.size());
  for(u32 i=0;i<count;++i) {
    std::string_view name=state.textureUserLabels[i];
    if(name.starts_with("Objeto: ")) name.remove_prefix(8);
    const float w=widthOf(name);
    if(x>0 && x+w>width) {x=0;y+=chipHeight+gap;++row;}
    // Na última linha permitida, reserva lugar para o "+N".
    if(!expanded && row==collapsedRows-1 && i+1<count && x+w+gap+widthOf("+999")>width) {
      chips.push_back({{x,y,widthOf("+"+std::to_string(count-i)),chipHeight},count-i,true});
      height=y+chipHeight;
      return chips;
    }
    if(!expanded && row>=collapsedRows) break;
    chips.push_back({{x,y,w,chipHeight},i,false});
    x+=w+gap;
  }
  if(expanded && count>0) {
    const float w=widthOf("Menos");
    if(x>0 && x+w>width) {x=0;y+=chipHeight+gap;}
    chips.push_back({{x,y,w,chipHeight},0,true});
  }
  height=count?y+chipHeight:16;
  return chips;
}
} // namespace

void buildTextureViewer(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const bool source=state.textureViewerSource;
  // Cabeçalho: voltar, nome, trilha de pastas (cada pasta leva a ela em
  // Arquivos) e o tipo do recurso.
  {
    auto header=takeTop(content,46);
    auto top=takeTop(header,26);
    const auto back=takeLeft(top,26);
    builder.list.addRect(back,builder.isPressed(widgetId(EditorWidget::TextureViewerClose))?theme.color.line:theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(back,14,14),static_cast<UiImageId>(UiIcon::UiChevronLeft),theme.color.text);
    builder.router.addRegion(back,widgetId(EditorWidget::TextureViewerClose),theme.touch.minimumTarget*.75f);
    takeLeft(top,8);
    builder.label(top,fitMiddle(builder.list,state.textureViewerTitle,top.width,theme.type.cardName),theme.color.text,theme.type.cardName);
    auto crumbs=deflate(header,UiInsets{34,4,0,0});
    const std::string_view badge=source?"Da fonte":"Textura";
    const float badgeWidth=measureTextWidth(badge,builder.list.fontMetrics(),theme.type.label)+10+12;
    auto pill=takeRight(crumbs,badgeWidth);pill.height=16;
    builder.list.addRect(pill,theme.color.accentWash,8);
    auto pillInner=deflate(pill,UiInsets::symmetric(5,0));
    builder.list.addImage(centred(takeLeft(pillInner,10),9,9),static_cast<UiImageId>(UiIcon::AssetsTexture),theme.color.accent);
    takeLeft(pillInner,2);
    builder.label(pillInner,badge,theme.color.accent,theme.type.label);
    takeRight(crumbs,6);crumbs.height=16;
    builder.list.addImage(centred(takeLeft(crumbs,12),11,11),static_cast<UiImageId>(UiIcon::AssetsFolder),theme.color.textFaint);
    takeLeft(crumbs,4);
    // Da pasta mais funda para trás, até caber; o começo vira "...". O
    // separador é o ícone de seta (o atlas da fonte não tem "›").
    const auto &parts=state.textureBreadcrumb;
    const auto &metrics=builder.list.fontMetrics();
    constexpr float separator=14;
    const float elided=measureTextWidth("...",metrics,theme.type.caption)+separator;
    usize first=parts.size();float used=0;
    while(first>0) {
      const float w=measureTextWidth(parts[first-1],metrics,theme.type.caption)+(first<parts.size()?separator:0);
      if(used+w>crumbs.width-(first>1?elided:0)) break;
      used+=w;--first;
    }
    const auto arrow=[&]() {
      builder.list.addImage(centred(takeLeft(crumbs,separator),8,8),static_cast<UiImageId>(UiIcon::UiChevronRight),theme.color.textFaint);
    };
    if(first>0) {
      builder.label(takeLeft(crumbs,elided-separator),"...",theme.color.textFaint,theme.type.caption);
      arrow();
    }
    for(usize i=first;i<parts.size();++i) {
      if(i>first) arrow();
      const auto box=takeLeft(crumbs,std::min(crumbs.width,measureTextWidth(parts[i],metrics,theme.type.caption)));
      const u32 widget=TextureCards::sub(detail::TextureInspectorCrumb+static_cast<u32>(i));
      builder.label(box,parts[i],builder.isPressed(widget)?theme.color.accent:theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(box,widget);
    }
  }
  // Rodapé fixo: a ação principal em destaque e as de navegação abaixo.
  {
    auto footer=takeBottom(content,70);
    builder.list.addRect({footer.x,footer.y,footer.width,1},theme.color.lineSoft);
    takeTop(footer,6);
    auto primary=takeTop(footer,30);
    takeTop(footer,4);
    if(source) {
      textureAction(builder,primary,UiIcon::AssetsFolderOpen,"Abrir a fonte",widgetId(EditorWidget::TextureSourceShowOrigin),true);
    } else {
      const auto revert=takeLeft(primary,primary.width*.4f-2);takeLeft(primary,4);
      textureAction(builder,revert,UiIcon::EditorAuthorUndo,"Reverter",widgetId(EditorWidget::TextureProfileRevert),false,state.textureProfileDirty);
      textureAction(builder,primary,UiIcon::UiCheck,"Aplicar e republicar",widgetId(EditorWidget::TextureProfileApply),true,state.textureProfileDirty);
    }
    auto row=takeTop(footer,28);
    const float third=(row.width-8)/3;
    textureAction(builder,takeLeft(row,third),UiIcon::AssetsSearch,"Localizar",TextureCards::sub(detail::TextureInspectorLocate),false);
    takeLeft(row,4);
    textureAction(builder,takeLeft(row,third),UiIcon::AssetsGrid,"Todas",widgetId(EditorWidget::TextureSourceShowAll),false);
    takeLeft(row,4);
    textureAction(builder,row,UiIcon::RuntimeRestart,"Reimportar",TextureCards::sub(detail::TextureInspectorReimport),false);
  }
  takeTop(content,4);takeBottom(content,4);
  const UiRect window=content;
  // Arrastar em qualquer ponto da janela rola; os controles ficam por cima.
  builder.router.addRegion(window,TextureCards::sub(detail::TextureInspectorScroll));
  builder.list.pushClip(window);
  const TextureCards cards{builder,window};
  UiRect column{window.x,window.y-state.textureInspectorScroll,window.width-5,1.0e6f};
  const float start=column.y;
  const bool stacked=column.width-2*kTexCardPad<300.0f;
  UiRect body;
  // Pré-visualização: a imagem (toque amplia), tamanho, nível e as ferramentas de olhar.
  textureCard(cards,column,detail::TextureCardPreview,UiIcon::SceneObjectPreview,"Pré-visualização",88,body);
  if(!body.isEmpty()) {
    auto image=takeLeft(body,88);
    builder.list.addRect(image,theme.color.canvas,theme.radius.control);
    builder.list.addBorder(image,theme.color.lineSoft,1.0f,theme.radius.control);
    const auto inner=deflate(image,UiInsets::all(3));
    if(!state.textureViewerImage.isEmpty()) {
      const auto &texels=state.textureViewerImage;
      const float fit=std::min(inner.width/texels.width,inner.height/texels.height);
      const float w=texels.width*fit,h=texels.height*fit;
      builder.list.addPreviewImage({inner.x+(inner.width-w)*.5f,inner.y+(inner.height-h)*.5f,w,h},texels);
      cards.region(image,TextureCards::sub(detail::TextureInspectorExpand));
    } else builder.label(inner,"Sem prévia",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    takeLeft(body,10);
    builder.label(takeTop(body,20),state.textureDimensions,theme.color.text,kTexValue);
    builder.label(takeTop(body,14),fitMiddle(builder.list,state.textureViewerLevelLabel,body.width,theme.type.caption),
                  theme.color.textMuted,theme.type.caption);
    auto tools=takeBottom(body,26);
    const auto zoom=takeLeft(tools,42);takeLeft(tools,4);
    const u32 zoomWidget=widgetId(EditorWidget::TextureViewerZoom);
    builder.list.addRect(zoom,builder.isPressed(zoomWidget)?theme.color.line:theme.color.raised,theme.radius.control);
    auto zoomInner=deflate(zoom,UiInsets::symmetric(5,0));
    builder.list.addImage(centred(takeLeft(zoomInner,12),12,12),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
    builder.label(zoomInner,std::to_string(1u<<state.textureViewerZoom)+"×",theme.color.text,theme.type.caption,UiAlign::Center);
    cards.region(zoom,zoomWidget);
    const bool rgba=state.textureViewerChannel==0;
    const auto backgroundBox=takeLeft(tools,26);takeLeft(tools,4);
    builder.list.addRect(backgroundBox,theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(backgroundBox,13,13),static_cast<UiImageId>(UiIcon::AssetsChecker),rgba?theme.color.textDim:theme.color.textFaint);
    if(rgba) cards.region(backgroundBox,widgetId(EditorWidget::TextureViewerBackground));
    const auto expand=takeLeft(tools,26);
    builder.list.addRect(expand,theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(expand,13,13),static_cast<UiImageId>(UiIcon::ViewExpand),theme.color.textDim);
    if(!state.textureViewerImage.isEmpty()) cards.region(expand,TextureCards::sub(detail::TextureInspectorExpand));
  }
  // Propriedades: Canal e Mip são controles; o resto é leitura do que foi preparado.
  {
    const usize cells=2+state.textureProperties.size(),rows=(cells+1)/2;
    const float cell=textureFieldHeight(stacked),gap=6;
    textureCard(cards,column,detail::TextureCardProperties,UiIcon::UiSliders,"Propriedades",rows*cell+(rows-1)*gap,body);
    if(!body.isEmpty()) {
      const float half=(body.width-8)*.5f;
      const auto at=[&](usize index) {
        return UiRect{body.x+(index%2)*(half+8),body.y+static_cast<float>(index/2)*(cell+gap),half,cell};
      };
      textureField(cards,at(0),"Canal",state.textureViewerChannelLabel,stacked,widgetId(EditorWidget::TextureViewerChannel),UiIcon::UiChevronDown);
      auto mip=textureField(cards,at(1),"Mip",std::string(),stacked);
      if(state.textureViewerLevels>1) {
        const auto down=takeLeft(mip,20),up=takeRight(mip,20);
        const bool canDown=state.textureViewerLevel>0,canUp=state.textureViewerLevel+1<state.textureViewerLevels;
        builder.list.addImage(centred(down,11,11),static_cast<UiImageId>(UiIcon::UiChevronLeft),canDown?theme.color.text:theme.color.textFaint);
        builder.list.addImage(centred(up,11,11),static_cast<UiImageId>(UiIcon::UiChevronRight),canUp?theme.color.text:theme.color.textFaint);
        if(canDown) cards.region(down,widgetId(EditorWidget::TextureViewerMipDown));
        if(canUp) cards.region(up,widgetId(EditorWidget::TextureViewerMipUp));
      }
      builder.label(mip,state.textureMipValue,theme.color.text,theme.type.caption,UiAlign::Center);
      for(usize i=0;i<state.textureProperties.size();++i) {
        const auto &field=state.textureProperties[i];
        const bool gpu=field.label=="Na GPU" && !state.textureGpuNote.empty();
        textureField(cards,at(2+i),field.label,field.value,stacked,gpu?TextureCards::sub(detail::TextureInspectorGpuInfo):0u,
                     gpu?UiIcon::UiInfo:UiIcon::None);
      }
    }
  }
  // Importação (só texturas do projeto): todo o perfil de uma vez, sem páginas.
  if(!source) {
    constexpr u32 editable=10;
    const float cell=textureFieldHeight(true),gap=6;
    const u32 rows=(editable+1)/2;
    textureCard(cards,column,detail::TextureCardImport,UiIcon::UiSettings,"Importação",rows*cell+(rows-1)*gap,body,
                state.textureProfileDirty?"não aplicada":"");
    if(!body.isEmpty()) {
      const float half=(body.width-8)*.5f;
      for(u32 i=0;i<editable;++i) {
        const std::string &text=state.textureProfileLabels[i];
        const auto split=text.find(": ");
        const UiRect at{body.x+(i%2)*(half+8),body.y+static_cast<float>(i/2)*(cell+gap),half,cell};
        textureField(cards,at,split==std::string::npos?std::string_view("Perfil"):std::string_view(text).substr(0,split),
                     split==std::string::npos?text:text.substr(split+2),true,
                     widgetId(EditorWidget::TextureProfileInterpretation)+i,UiIcon::UiChevronRight);
      }
    }
  }
  // Origem: de onde a imagem vem, cada linha copiável.
  if(!state.textureOrigin.empty()) {
    const float row=22,gap=4;
    textureCard(cards,column,detail::TextureCardOrigin,UiIcon::AssetsFile,"Origem",
                state.textureOrigin.size()*row+(state.textureOrigin.size()-1)*gap,body);
    if(!body.isEmpty()) for(usize i=0;i<state.textureOrigin.size();++i) {
      const auto &entry=state.textureOrigin[i];
      auto line=takeTop(body,row);takeTop(body,gap);
      builder.label(takeLeft(line,52),entry.label,theme.color.textMuted,theme.type.caption);
      if(!entry.copy.empty()) {
        const auto copy=takeRight(line,22);
        const u32 widget=TextureCards::sub(detail::TextureInspectorCopy+static_cast<u32>(i));
        builder.list.addRect(copy,builder.isPressed(widget)?theme.color.line:theme.color.raised,theme.radius.control);
        builder.list.addImage(centred(copy,12,12),static_cast<UiImageId>(UiIcon::AssetsCopy),theme.color.textDim);
        cards.region(copy,widget);
        takeRight(line,4);
      }
      builder.list.addRect(line,theme.color.canvas,theme.radius.control);
      const auto text=deflate(line,UiInsets::symmetric(6,0));
      builder.label(text,fitMiddle(builder.list,entry.value,text.width,theme.type.caption),theme.color.text,theme.type.caption);
    }
  }
  // Usado por: um chip por objeto; tocar seleciona na cena.
  {
    float chipsHeight=0;
    const auto chips=layoutTextureChips(builder,column.width-2*kTexCardPad,state.textureUsersExpanded,chipsHeight);
    textureCard(cards,column,detail::TextureCardUsers,UiIcon::SceneObject,
                "Usado por ("+std::to_string(state.textureUserLabels.size())+")",chipsHeight,body);
    if(!body.isEmpty()) {
      if(state.textureUserLabels.empty())
        builder.label(takeTop(body,16),source?"Nenhum objeto da cena usa esta textura.":"Nenhum objeto ou material do projeto usa esta textura.",
                      theme.color.textMuted,theme.type.caption);
      for(const auto &chip:chips) {
        const UiRect box{body.x+chip.box.x,body.y+chip.box.y,chip.box.width,chip.box.height};
        const bool entity=!chip.more && chip.index<state.textureUserEntities.size() && state.textureUserEntities[chip.index]!=kInvalidEntity;
        const u32 widget=chip.more?TextureCards::sub(detail::TextureInspectorUsersMore):widgetId(EditorWidget::TextureUserBase)+chip.index;
        builder.list.addRect(box,builder.isPressed(widget)?theme.color.line:theme.color.canvas,11);
        builder.list.addBorder(box,theme.color.lineSoft,1.0f,11);
        auto inner=deflate(box,UiInsets{8,0,6,0});
        if(chip.more) {
          builder.label(inner,state.textureUsersExpanded?std::string("Menos"):"+"+std::to_string(chip.index),theme.color.accent,
                        theme.type.caption,UiAlign::Center);
          cards.region(box,widget);
          continue;
        }
        std::string_view name=state.textureUserLabels[chip.index];
        if(name.starts_with("Objeto: ")) name.remove_prefix(8);
        builder.list.addImage(centred(takeLeft(inner,11),11,11),static_cast<UiImageId>(entity?UiIcon::SceneObject:UiIcon::AssetsMaterial),
                              entity?theme.color.textDim:theme.color.textFaint);
        takeLeft(inner,4);
        builder.label(inner,name,entity?theme.color.text:theme.color.textMuted,theme.type.caption);
        if(entity) cards.region(box,widget);
      }
    }
  }
  const float height=column.y-start;
  builder.list.popClip();
  if(builder.layout) {builder.layout->textureInspectorContent=height;builder.layout->textureInspectorWindow=window.height;}
  // Barra de rolagem fina quando os cartões passam da janela.
  if(height>window.height+1) {
    const float thumb=std::max(24.0f,window.height*window.height/height);
    const float travel=window.height-thumb;
    const float offset=std::clamp(state.textureInspectorScroll/(height-window.height),0.0f,1.0f)*travel;
    builder.list.addRect({window.right()-3,window.y+offset,3,thumb},theme.color.line,1.5f);
  }
}

// Prévia ampliada sobre a cena: a imagem inteira no maior tamanho que cabe,
// com os mesmos controles de nível, canal, zoom e fundo.
void buildTextureExpanded(ScreenBuilder &builder,const UiRect &area) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  builder.list.addRect(area,withAlpha(theme.color.canvas,0.96f));
  builder.router.addBlocker(area);
  auto content=deflate(area,UiInsets::all(theme.spacing.medium));
  auto top=takeTop(content,30);
  const auto close=takeRight(top,30);
  const u32 closeWidget=TextureCards::sub(detail::TextureInspectorExpandClose);
  builder.list.addRect(close,builder.isPressed(closeWidget)?theme.color.line:theme.color.raised,theme.radius.control);
  builder.list.addImage(centred(close,14,14),static_cast<UiImageId>(UiIcon::UiClose),theme.color.text);
  builder.router.addRegion(close,closeWidget,theme.touch.minimumTarget);
  takeRight(top,8);
  const auto info=takeRight(top,std::min(top.width*.45f,220.0f));
  builder.label(info,fitMiddle(builder.list,state.textureViewerLevelLabel,info.width,theme.type.caption),theme.color.textMuted,
                theme.type.caption,UiAlign::End);
  builder.label(top,fitMiddle(builder.list,state.textureViewerTitle,top.width,theme.type.cardName),theme.color.text,theme.type.cardName);
  auto bar=takeBottom(content,30);
  takeBottom(content,8);takeTop(content,8);
  if(!state.textureViewerImage.isEmpty()) {
    const auto &texels=state.textureViewerImage;
    const float fit=std::min(content.width/texels.width,content.height/texels.height);
    const float w=texels.width*fit,h=texels.height*fit;
    const UiRect image{content.x+(content.width-w)*.5f,content.y+(content.height-h)*.5f,w,h};
    builder.list.addBorder(deflate(image,UiInsets::all(-1)),theme.color.lineSoft,1.0f);
    builder.list.addPreviewImage(image,texels);
  }
  // Controles centrados: ‹ nível ›, canal, zoom e fundo.
  const float cluster=30+70+30+8+70+8+50+8+30;
  UiRect row{bar.x+std::max(0.0f,(bar.width-cluster)*.5f),bar.y,std::min(bar.width,cluster),bar.height};
  const auto square=[&](UiIcon icon,u32 widget,bool enabled) {
    const auto box=takeLeft(row,30);
    builder.list.addRect(box,builder.isPressed(widget)?theme.color.line:theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(box,13,13),static_cast<UiImageId>(icon),enabled?theme.color.text:theme.color.textFaint);
    if(enabled) builder.router.addRegion(box,widget);
  };
  const auto pill=[&](const std::string &text,u32 widget,float width) {
    const auto box=takeLeft(row,width);
    builder.list.addRect(box,builder.isPressed(widget)?theme.color.line:theme.color.raised,theme.radius.control);
    builder.label(box,text,theme.color.text,theme.type.caption,UiAlign::Center);
    if(widget) builder.router.addRegion(box,widget);
  };
  square(UiIcon::UiChevronLeft,widgetId(EditorWidget::TextureViewerMipDown),state.textureViewerLevel>0);
  pill("Mip "+state.textureMipValue,0,70);
  square(UiIcon::UiChevronRight,widgetId(EditorWidget::TextureViewerMipUp),state.textureViewerLevel+1<state.textureViewerLevels);
  takeLeft(row,8);
  pill(state.textureViewerChannelLabel,widgetId(EditorWidget::TextureViewerChannel),70);
  takeLeft(row,8);
  pill(std::to_string(1u<<state.textureViewerZoom)+"×",widgetId(EditorWidget::TextureViewerZoom),50);
  takeLeft(row,8);
  square(UiIcon::AssetsChecker,widgetId(EditorWidget::TextureViewerBackground),state.textureViewerChannel==0);
}

// Descriptor fields are addressed by stable instance/property IDs on edit.
// Widget indices exist only for one rendered frame, never in the scene archive.
// A collection is a route, never a stack of 128 property groups. Numeric
// addresses retain the slot ABI; the session pins point identity while editing.
void buildPathPoints(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,const scene::Path &path,u32 index) {
  const auto &s=builder.state;const auto &t=builder.theme;const auto &points=path.curve.points;
  const bool authoring=s.workspace!=EditorWorkspace::Play;
  auto heading=takeTop(content,36);
  builder.iconButton(takeLeft(heading,36),UiIcon::UiArrowLeft,widgetId(EditorWidget::PathEditorClose));
  builder.label(heading,"Pontos · Path",t.color.text,t.type.body);
  const auto at=std::find_if(points.begin(),points.end(),[&](const auto&p){return p.id==s.pathPointId;});
  const bool valid=at!=points.end();const u32 current=valid?static_cast<u32>(at-points.begin()):0;
  auto nav=takeTop(content,40);
  builder.iconButton(takeLeft(nav,36),UiIcon::UiChevronLeft,widgetId(EditorWidget::PathPointPrevious),false,t.color.textDim,valid&&current>0);
  builder.iconButton(takeRight(nav,36),UiIcon::UiChevronRight,widgetId(EditorWidget::PathPointNext),false,t.color.textDim,valid&&current+1<points.size());
  const auto listHit=nav;
  builder.list.addImage(centred(takeLeft(nav,22),16,16),static_cast<UiImageId>(UiIcon::PathPoint),t.color.accent);
  builder.list.addImage(centred(takeRight(nav,18),12,12),static_cast<UiImageId>(UiIcon::UiChevronDown),t.color.textDim);
  const std::string title=valid?std::to_string(current+1)+" / "+std::to_string(points.size())+" · #"+std::to_string(at->id):"Vazio · 0 / 128";
  builder.label(nav,title,t.color.text,t.type.caption,UiAlign::Center);
  builder.router.addRegion(listHit,widgetId(EditorWidget::PathPointList));
  if(s.pathPointList) {
    auto footer=takeBottom(content,36);
    const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/40));
    const u32 pages=std::max(1u,(static_cast<u32>(points.size())+perPage-1)/perPage),page=std::min(s.pathPointPage,pages-1);
    builder.iconButton(takeLeft(footer,36),UiIcon::UiChevronLeft,widgetId(EditorWidget::PathPointListPrevious),false,t.color.textDim,page>0);
    builder.iconButton(takeRight(footer,36),UiIcon::UiChevronRight,widgetId(EditorWidget::PathPointListNext),false,t.color.textDim,page+1<pages);
    builder.label(footer,std::to_string(page+1)+" / "+std::to_string(pages),t.color.textDim,t.type.caption,UiAlign::Center);
    for(u32 n=page*perPage;n<points.size()&&n<(page+1)*perPage;++n) {
      auto row=takeTop(content,40);if(points[n].id==s.pathPointId)builder.list.addRect(row,withAlpha(t.color.accent,.12f));
      builder.list.addImage(centred(takeLeft(row,26),16,16),static_cast<UiImageId>(UiIcon::PathPoint),t.color.accent);
      builder.label(row,"Ponto "+std::to_string(n+1)+" · #"+std::to_string(points[n].id),t.color.text,t.type.caption);
      builder.router.addRegion({row.x-26,row.y,row.width+26,row.height},widgetId(EditorWidget::PathPointSelectBase)+n);
    }
    return;
  }
  if(!s.pathOrientation){
  auto actions=takeTop(content,40);const float width=actions.width/4;
  const UiIcon icons[]{UiIcon::ComponentAdd,UiIcon::UiRemove,UiIcon::UiChevronUp,UiIcon::UiChevronDown};
  const EditorWidget ids[]{EditorWidget::PathPointInsert,EditorWidget::PathPointRemove,EditorWidget::PathPointMoveUp,EditorWidget::PathPointMoveDown};
  const bool enabled[]{authoring&&points.size()<resources::Curve3D::MaximumPoints,authoring&&valid,authoring&&valid&&current>0,authoring&&valid&&current+1<points.size()};
  for(u32 a=0;a<4;++a)builder.iconButton(takeLeft(actions,width),icons[a],widgetId(ids[a]),false,t.color.textDim,enabled[a]);
  }
  auto modes=takeTop(content,40);const float modeWidth=modes.width/3;
  const char*modeLabels[]{"Posição","Tangentes","Orientação"};
  const UiIcon modeIcons[]{UiIcon::PathPoint,UiIcon::PathTangent,UiIcon::PathOrientation};
  const EditorWidget modeWidgets[]{EditorWidget::PathPointPosition,EditorWidget::PathPointTangents,EditorWidget::PathPointOrientation};
  const u32 selectedMode=s.pathOrientation?2:s.pathTangents?1:0;
  for(u32 n=0;n<3;++n){
    auto cell=takeLeft(modes,modeWidth);const auto hit=cell;const auto color=n==selectedMode?t.color.accent:t.color.textDim;
    builder.list.addImage(centred(takeTop(cell,18),14,14),static_cast<UiImageId>(modeIcons[n]),color);
    builder.label(cell,modeLabels[n],color,t.type.caption,UiAlign::Center);builder.router.addRegion(hit,widgetId(modeWidgets[n]));
  }
  if(s.pathOrientation){
    if(valid){
      auto row=takeTop(content,40);builder.label(takeLeft(row,row.width*.5f),"Roll do ponto",t.color.textDim,t.type.caption);
      const auto hit=row;char number[32];std::snprintf(number,sizeof(number),"%.4g°",static_cast<double>(at->rollDegrees));builder.label(row,number,t.color.text,t.type.caption,UiAlign::Center);
      if(authoring)builder.router.addRegion(hit,widgetId(EditorWidget::ComponentSlotNumberBase)+index+(9u<<8)+(current<<16));
    }
    builder.label(takeTop(content,20),"Up inicial da curva · local",t.color.textDim,t.type.caption);
    auto row=takeTop(content,40);const float width=row.width/3;const UiColor colors[]{t.color.axisX,t.color.axisY,t.color.axisZ};
    for(u32 axis=0;axis<3;++axis){const auto hit=takeLeft(row,width);auto cell=deflate(hit,UiInsets::all(2));builder.list.addRect(cell,t.color.silhouette,4);builder.label(takeLeft(cell,12),axis==0?"X":axis==1?"Y":"Z",colors[axis],t.type.caption,UiAlign::Center);char number[24];std::snprintf(number,sizeof(number),"%.4g",static_cast<double>(path.curve.up[axis]));builder.label(cell,number,t.color.text,t.type.caption,UiAlign::Center);if(authoring)builder.router.addRegion(hit,widgetId(EditorWidget::ComponentNumberBase)+index+(axis<<8));}
  }else if(valid) {
    const u32 start=s.pathTangents?3:0,end=s.pathTangents?9:3;
    for(u32 field=start;field<end;field+=3) {
      auto row=takeTop(content,40);builder.label(takeLeft(row,48),field==0?"Local":field==3?"Entrada":"Saída",t.color.textDim,t.type.caption);
      const float cellWidth=row.width/3;const UiColor colors[]{t.color.axisX,t.color.axisY,t.color.axisZ};
      for(u32 axis=0;axis<3;++axis) {
        const auto hit=takeLeft(row,cellWidth);auto cell=deflate(hit,UiInsets::all(2));builder.list.addRect(cell,t.color.silhouette,4);
        builder.label(takeLeft(cell,12),axis==0?"X":axis==1?"Y":"Z",colors[axis],t.type.caption,UiAlign::Center);
        char number[24];std::snprintf(number,sizeof(number),"%.4g",static_cast<double>(path.type().slotNumbers[field+axis].read(path,current)));
        builder.label(cell,number,t.color.text,t.type.caption,UiAlign::Center);
        if(authoring)builder.router.addRegion(hit,widgetId(EditorWidget::ComponentSlotNumberBase)+index+((field+axis)<<8)+(current<<16));
      }
    }
  }
  const std::string note=s.pathOrientation?"Up não nulo · roll contínuo em graus":!s.pathStatus.empty()?s.pathStatus:s.pathTangents?"Offsets locais relativos ao ponto":"Coordenadas locais · até 128 pontos";
  builder.label(takeTop(content,20),fitMiddle(builder.list,note,content.width,t.type.caption),s.pathStatusWarning?t.color.warning:t.color.textMuted,t.type.caption);
  (void)entity;
}

void buildComponentFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,
                          const EditorComponentEntry &entry,u32 index) {
  const auto &theme=builder.theme;
  const auto *component=entity.components.at(index);if(!component || &component->type()!=entry.type) return;
  const auto &state=builder.state;
  if(entry.type==&scene::Collider::descriptor||entry.type==&scene::PhysicsBody::descriptor||entry.type==&scene::Character::descriptor){
    auto tools=takeTop(content,38);
    if(entry.type==&scene::Collider::descriptor){const float half=tools.width*.5f;colliderToolButton(builder,takeLeft(tools,half),"Geometria",EditorWidget::ColliderGeometryOpen,UiIcon::EditorColliderFace,false,state.workspace!=EditorWorkspace::Play);}
    colliderToolButton(builder,tools,"Diagnóstico",EditorWidget::PhysicsDiagnosticOpen,UiIcon::PhysicsDiagnostic);
  }
  if(entry.type==&scene::Timer::descriptor && state.timerRuntime && !builder.multiEdit && content.height>=52) {
    const auto *live=state.timerRuntime->state(entity.id,component->instanceId());
    const auto &timer=static_cast<const scene::Timer&>(*component);
    const bool running=live&&live->running,paused=live&&live->paused;
    auto row=takeTop(content,44);const auto base=widgetId(EditorWidget::TimerControlBase)+index*4;
    builder.iconButton(takeRight(row,36),UiIcon::RuntimeStop,base+2);
    builder.iconButton(takeRight(row,36),paused?UiIcon::RuntimePlay:UiIcon::RuntimePause,base+1,paused);
    builder.iconButton(takeRight(row,36),running?UiIcon::RuntimeRestart:UiIcon::RuntimePlay,base);
    char remaining[48];std::snprintf(remaining,sizeof(remaining),"%.2f s",running?std::max(0.0,live->remaining):0);
    builder.label(takeTop(row,22),remaining,theme.color.text,theme.type.body);
    const char *status=!live?"Aguardando quadro":paused||state.playPaused?"Pausado":!timer.enabled?"Desabilitado":!state.document->activeInHierarchy(entity.id)?"Inativo":live->completed?"Concluído":running?"Restante":"Parado";
    builder.label(row,status,theme.color.textDim,theme.type.caption);
    auto bar=takeTop(content,4);builder.list.addRect(bar,theme.color.line,1);
    if(running){const auto fraction=std::clamp(static_cast<float>(live->remaining/timer.intervalSeconds),0.f,1.f);builder.list.addRect({bar.x,bar.y,bar.width*fraction,bar.height},paused?theme.color.textDim:theme.color.accent,1);}
    takeTop(content,4);
  }
  if(entry.type==&scene::Path::descriptor) {
    const auto &path=static_cast<const scene::Path&>(*component);
    auto row=takeTop(content,40);builder.label(row,std::to_string(path.curve.points.size())+" pontos · Bézier local",theme.color.textDim,theme.type.caption);
    row=takeTop(content,40);builder.checkbox(takeLeft(row,36),path.curve.closed,widgetId(EditorWidget::ComponentBooleanBase)+index,state.workspace!=EditorWorkspace::Play);
    builder.label(row,"Fechar caminho",theme.color.text,theme.type.caption);
    row=takeTop(content,40);builder.list.addImage(centred(takeLeft(row,32),20,20),static_cast<UiImageId>(UiIcon::PathPoint),theme.color.accent);
    builder.label(row,state.workspace==EditorWorkspace::Play?"Pontos · autoria em Edit":"Editar pontos",state.workspace==EditorWorkspace::Play?theme.color.textMuted:theme.color.text,theme.type.body);
    if(state.workspace!=EditorWorkspace::Play)builder.router.addRegion({row.x-32,row.y,row.width+32,row.height},widgetId(EditorWidget::PathEditorOpen));
    return;
  }
  if(entry.type==&scene::PathFollow::descriptor) {
    const bool commands=state.workspace==EditorWorkspace::Play&&state.componentGroup=="Execução";
    auto note=takeTop(content,commands?40.f:20.f);
    const auto searchHit=takeRight(note,24);builder.list.addImage(centred(searchHit,12,12),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
    builder.router.addRegion(searchHit,widgetId(EditorWidget::ComponentPropertySearch));
    if(commands) {
      builder.iconButton(takeRight(note,36),UiIcon::RuntimeStop,widgetId(EditorWidget::PathFollowStop));
      builder.iconButton(takeRight(note,36),UiIcon::RuntimeRestart,widgetId(EditorWidget::PathFollowRestart));
    }
    builder.label(note,fitMiddle(builder.list,state.followStatus.empty()?std::string{"Estado real no Play"}:state.followStatus,note.width,theme.type.caption),state.followStatusWarning?theme.color.warning:theme.color.textDim,theme.type.caption);
  }
  const std::string_view searchText=state.editingPropertySearch?std::string_view(state.renameText):std::string_view(state.propertyQuery);
  const auto search=editorSearchKey(searchText);
  const bool character=entry.type==&scene::Character::descriptor;
  const bool compactCharacter=character&&content.height<220;
  const bool searching=!search.empty()||(compactCharacter&&state.editingPropertySearch);
  const bool mesh=entry.type==&scene::MeshRenderer::descriptor;
  const bool constantForce=entry.type->id==std::string_view{"astra.physics.constant_force"};
  const bool tween=entry.type->id==std::string_view{"astra.tween.transform"};
  const bool audio=std::string_view(entry.type->id).starts_with("astra.audio.");
  const bool audioSource=entry.type->id==std::string_view{"astra.audio.source"};
  if((std::string_view(entry.type->id).starts_with("astra.constraint.")||std::string_view(entry.type->id).starts_with("astra.spring."))&&!state.constraintStatus.empty())
    builder.label(takeTop(content,20),fitMiddle(builder.list,state.constraintStatus,content.width,theme.type.caption),state.constraintStatusWarning?theme.color.warning:theme.color.textDim,theme.type.caption);
  if(constantForce&&!searching) {
    auto note=takeTop(content,20);const auto searchHit=takeRight(note,24);
    builder.label(note,"Requer Corpo físico dinâmico",theme.color.textMuted,theme.type.caption);
    builder.list.addImage(centred(searchHit,12,12),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
    builder.router.addRegion(searchHit,widgetId(EditorWidget::ComponentPropertySearch));
  }
  // Sequência: diz em que etapa está e, se parou, por quê; o motivo vem do avaliador.
  if(entry.type==&scene::TweenSequence::descriptor&&!searching) {
    const auto &sequence=static_cast<const scene::TweenSequence&>(*component);
    const auto *live=state.tweenSequenceRuntime?state.tweenSequenceRuntime->state(entity.id,component->instanceId()):nullptr;
    using S=runtime::SceneTweenSequences::Status;
    const u32 total=sequence.stepCount();
    std::string status;
    if(!live) status=total==0?std::string{"Aponte a etapa 1 para um objeto com Transform Tween"}:
      std::to_string(total)+(total==1?" etapa · ":" etapas · ")+(sequence.autoplay?"começa no Play":"aguarda Tocar");
    else if(live->status==S::Failed) status=live->failure;
    else {
      status=live->paused||state.playPaused?std::string{"Pausada"}:std::string{runtime::SceneTweenSequences::statusText(live->status)};
      if(live->status==S::Interval||live->status==S::Running) {
        u32 position=0;for(u32 i=0;i<=live->step&&i<scene::kTweenSequenceSteps;++i) position+=sequence.steps[i].target!=0;
        status+=" · etapa "+std::to_string(position)+" de "+std::to_string(total);
      }
    }
    const bool warning=total==0||(live&&(live->status==S::Failed||live->status==S::NoSteps));
    builder.label(takeTop(content,20),fitMiddle(builder.list,status,content.width,theme.type.caption),warning?theme.color.warning:theme.color.textDim,theme.type.caption);
  }
  if(tween&&searching) {
    const auto *live=state.tweenRuntime?state.tweenRuntime->state(entity.id,component->instanceId()):nullptr;
    const std::string status=live?(live->paused?"Pausado":runtime::SceneTweens::statusText(live->status)):"Destino local · início em Play";
    builder.label(takeTop(content,20),fitMiddle(builder.list,status,content.width,theme.type.caption),live&&live->status>=runtime::SceneTweens::Status::Authority?theme.color.warning:theme.color.textDim,theme.type.caption);
  }
  if(character&&!searching&&!state.editingPropertySearch&&state.characterRuntime&&state.characterWorld) {
    physics::CharacterMotor::RuntimeState observed;const auto status=state.characterRuntime->characterState(*state.characterWorld,entity.id,observed);
    if(status==runtime::WorldStatus::Ok) {
      const char *names[]{"No chão","Rampa íngreme","Sem apoio","No ar"};const auto ground=static_cast<u32>(observed.groundState);
      auto supportRow=takeTop(content,20);
      if(static_cast<const scene::Character&>(*component).inheritPlatformHorizontal){const auto mark=takeRight(supportRow,20);builder.list.addImage(centred(mark,14,14),static_cast<UiImageId>(UiIcon::PhysicsCharacterPlatformCarry),theme.color.accent);}
      if(compactCharacter) {
        std::ostringstream compact;compact<<(ground<4?names[ground]:"Apoio desconhecido");
        if(observed.hasMeasuredStep)compact<<std::fixed<<std::setprecision(1)<<" · v "<<observed.velocity.x<<" / "<<observed.velocity.y<<" / "<<observed.velocity.z<<" m/s";else compact<<" · sem passo";
        builder.label(supportRow,fitMiddle(builder.list,compact.str(),supportRow.width,theme.type.caption),theme.color.textDim,theme.type.caption);
      } else {
        builder.label(supportRow,ground<4?names[ground]:"Apoio desconhecido",theme.color.textDim,theme.type.caption);
      std::ostringstream line;if(observed.hasMeasuredStep)line<<std::fixed<<std::setprecision(2)<<"Vel. medida · "<<observed.velocity.x<<" / "<<observed.velocity.y<<" / "<<observed.velocity.z<<" m/s";else line<<"Aguardando passo físico";
      builder.label(takeTop(content,20),fitMiddle(builder.list,line.str(),content.width,theme.type.caption),theme.color.textMuted,theme.type.caption);
      }
    } else builder.label(takeTop(content,20),runtime::worldStatusMessage(status),theme.color.warning,theme.type.caption);
  }
  if(audio&&!searching) {
    auto note=takeTop(content,20);const auto searchHit=takeRight(note,24);
    const auto status=state.audioStatus.empty()?std::string_view{audioSource?"Pedido salvo · estado no Play":"Estado real no Play"}:std::string_view{state.audioStatus};
    builder.label(note,fitMiddle(builder.list,std::string(status),note.width,theme.type.caption),state.audioStatusWarning?theme.color.warning:theme.color.textMuted,theme.type.caption);
    builder.list.addImage(centred(searchHit,12,12),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
    builder.router.addRegion(searchHit,widgetId(EditorWidget::ComponentPropertySearch));
  }
  if((!character&&!constantForce&&!audio&&!tween&&entry.type!=&scene::PathFollow::descriptor&&(!mesh||state.meshTab!=2))||searching||state.editingPropertySearch) {
    auto searchRow=takeTop(content,34),clear=takeRight(searchRow,32);
    builder.list.addRect(searchRow,theme.color.raised,theme.radius.control);
    builder.label(searchRow,searchText.empty()?"Buscar propriedade":searchText,searchText.empty()?theme.color.textMuted:theme.color.text,theme.type.caption);
    builder.router.addRegion(searchRow,widgetId(EditorWidget::ComponentPropertySearch));
    if(!searchText.empty()) {
      builder.label(clear,"×",theme.color.textDim,theme.type.body,UiAlign::Center);
      builder.router.addRegion(clear,widgetId(EditorWidget::ComponentPropertySearchClear));
    }
  }
  // An attached singleton is not an error. Show actual unmet requirements,
  // rather than the Add menu's "already exists" state on its own inspector.
  for(const auto &requirement:entry.schema->requirements) if(!entity.components.find(requirement.typeId))
    builder.label(takeTop(content,28),requirement.message,theme.color.warning,theme.type.caption);
  if(!searching && entry.type==&scene::Camera::descriptor) {
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
  if(!searching && entry.type==&scene::Joint::descriptor) {
    const auto &j=static_cast<const scene::Joint &>(*component);
    builder.label(takeTop(content,28),j.kind==scene::JointKind::Hinge?"Limites/alvo: graus · velocidade: graus/s":j.kind==scene::JointKind::SixDOF?"Translação: u · rotação: graus":"Âncoras locais aos corpos",theme.color.textMuted,theme.type.caption);
  }
  if(mesh && !searching) {
    auto tabs=takeTop(content,36);const float width=tabs.width/3;
    for(u32 i=0;i<3;++i) {
      auto tab=deflate(takeLeft(tabs,width),UiInsets::all(2));const bool selected=builder.state.meshTab==i;
      builder.list.addRect(tab,selected?theme.color.accent:theme.color.raised,theme.radius.control);
      const char *names[]{"Geometria","Material","Lightmap"};
      const EditorWidget actions[]{EditorWidget::MeshGeometryTab,EditorWidget::MeshMaterialTab,EditorWidget::InspectorMeshLightmap};
      builder.label(tab,names[i],selected?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(tab,widgetId(actions[i]));
    }
  }
  // A aba Material é por SLOT, com alcance explícito; não é mais a lista de
  // números do componente, que só alcançava o primeiro slot.
  if(mesh && builder.state.meshTab==1) {buildMaterialSlots(builder,content,searchText);return;}
  if(mesh && builder.state.meshTab==2 && !searching) {
    const auto note=takeTop(content,34);
    auto firstLine=UiRect{note.x,note.y,note.width,17};
    const auto search=takeRight(firstLine,24);
    builder.list.addImage(centred(search,12,12),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
    builder.router.addRegion({search.x,note.y,search.width,note.height},widgetId(EditorWidget::ComponentPropertySearch));
    builder.label(firstLine,"RGB linear · indireto · UV1",theme.color.textMuted,theme.type.caption);
    const auto &render=static_cast<const scene::MeshRenderer &>(*component);
    u32 invalid=0;std::string firstDiagnostic;
    for(u32 slot=0;slot<render.slotCount();++slot) {
      const auto asset=render.slotMesh(slot);
      const bool usable=asset&&state.resources&&state.resources->lightmapUvUsable(asset-1);
      if(!usable) {
        ++invalid;
        if(firstDiagnostic.empty()) firstDiagnostic=asset&&state.resources?state.resources->lightmapUvDiagnostic(asset-1):"Geometria ausente";
      }
    }
    const std::string diagnostic=invalid?firstDiagnostic+(invalid>1?" · "+std::to_string(invalid)+" slots":"")+" · sem bake":"Bindless · sem bake / unwrap";
    builder.label({note.x,note.y+17,note.width,17},fitMiddle(builder.list,diagnostic,note.width,theme.type.caption),invalid?theme.color.warning:theme.color.textMuted,theme.type.caption);
  }
  // Malha já navega entre Geometria e o editor de Material por slot acima.
  // As abas refletidas repetiriam esses nomes sem acrescentar controles.
  const auto groups=mesh?std::vector<std::string_view>{}:constantForce?
    std::vector<std::string_view>{"Força mundo","Força local"}:componentGroups(*component);
  std::string_view group=mesh?std::string_view{builder.state.meshTab==2?"Lightmap":"Geometria"}:std::string_view{builder.state.componentGroup};
  if(constantForce) {if(group=="Torque mundo") group="Força mundo";else if(group=="Torque local") group="Força local";}
  if(!groups.empty() && std::find(groups.begin(),groups.end(),group)==groups.end()) group=groups.front();
  if(groups.size()>1 && !searching) {
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
        const auto name=audioSource?(groups[i]=="Reprodução"?std::string_view{"Tocar"}:groups[i]=="Emissão"?std::string_view{"Cone"}:groups[i]):
          entry.type->id==std::string_view{"astra.physics2d.joint"}&&groups[i]=="Anchors"?std::string_view{"Âncoras"}:groups[i];
        const auto jointTabName=entry.type==&scene::Joint::descriptor&&groups[i].starts_with("Translação ")?std::string("Mov ")+std::string(groups[i].substr(std::string_view("Translação ").size())):
          entry.type==&scene::Joint::descriptor&&groups[i].starts_with("Rotação ")?std::string("Rot ")+std::string(groups[i].substr(std::string_view("Rotação ").size())):
          entry.type==&scene::Joint::descriptor&&static_cast<const scene::Joint&>(*component).kind==scene::JointKind::Spring&&name=="Motor"?std::string("Mola"):std::string(name);
        builder.label(tab,constantForce?(i==0?std::string_view{"Mundo"}:std::string_view{"Local"}):std::string_view(jointTabName),active?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
        builder.router.addRegion(tab,widgetId(EditorWidget::ComponentGroupBase)+i);
      }
      first+=count;
    }
    takeTop(content,6);
  }
  if(tween&&!searching) {
    const auto *live=state.tweenRuntime?state.tweenRuntime->state(entity.id,component->instanceId()):nullptr;
    const bool actions=group=="Tempo"&&state.workspace==EditorWorkspace::Play&&state.tweenRuntime;
    auto note=takeTop(content,20);const auto searchHit=takeRight(note,24);
    builder.list.addImage(centred(searchHit,12,12),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
    builder.router.addRegion(searchHit,widgetId(EditorWidget::ComponentPropertySearch));
    const auto status=live?std::string{!static_cast<const scene::TransformTween*>(component)->enabled?"Desabilitado":!state.document->activeInHierarchy(entity.id)?"Inativo":live->paused||state.playPaused?"Pausado":runtime::SceneTweens::statusText(live->status)}:std::string{"Destino local · início em Play"};
    const bool warning=live&&live->status>=runtime::SceneTweens::Status::Authority;
    builder.label(note,fitMiddle(builder.list,status,note.width,theme.type.caption),warning?theme.color.warning:theme.color.textDim,theme.type.caption);
    if(group=="Conexão") {
      auto connection=takeTop(content,24);builder.list.addImage(centred(takeLeft(connection,24),16,16),static_cast<UiImageId>(UiIcon::EventTweenCompletion),theme.color.accent);
      const bool failed=live&&live->connectionInvoked&&live->connectionStatus!=runtime::WorldStatus::Ok;
      const char *text=failed?"Receptor indisponível":live&&live->connectionInvoked?"Conexão aplicada":"Após todos os ciclos finitos";
      builder.label(connection,fitMiddle(builder.list,text,connection.width,theme.type.caption),failed?theme.color.warning:theme.color.textDim,theme.type.caption);
    }
    if(actions) {
      auto row=takeTop(content,44);
      builder.iconButton(takeRight(row,36),UiIcon::RuntimeStop,widgetId(EditorWidget::TweenCancel)+index*4);
      builder.iconButton(takeRight(row,36),live&&live->paused?UiIcon::RuntimePlay:UiIcon::RuntimePause,widgetId(EditorWidget::TweenPause)+index*4,live&&live->paused);
      builder.iconButton(takeRight(row,36),UiIcon::RuntimeRestart,widgetId(EditorWidget::TweenRestart)+index*4);
      char elapsed[48];std::snprintf(elapsed,sizeof(elapsed),"%.2f s",live?live->elapsed:0);
      builder.label(takeTop(row,22),elapsed,theme.color.text,theme.type.body);
      builder.label(row,"Decorrido",theme.color.textDim,theme.type.caption);
      takeTop(content,4);
    }
  }
  const auto show=[&](const scene::PropertyPresentation &p) {
    if(!p.isVisible(*component)) return false;
    // Lightmap é uma rota própria. Propriedades sem grupo do material e da
    // geometria não podem inflar essa rota nem esconder o binding principal.
    if(mesh&&builder.state.meshTab==2&&!searching) return p.group=="Lightmap";
    if(constantForce&&!searching) return group=="Força mundo"?(p.group=="Força mundo"||p.group=="Torque mundo"):
      (p.group=="Força local"||p.group=="Torque local");
    return searching||p.group.empty()||p.group==group;
  };
  struct Field {u32 kind,index,slot=0,second=~u32{0};}; // 11: par por slot; 12: binding/habilitação; 13: canais booleanos XYZ.
  std::vector<Field> fields;
  if(!searching && entry.type==&scene::Collider::descriptor && group=="Cozimento") fields.push_back({7,0});
  if(!searching && entry.type==&scene::Environment::descriptor && group=="Volume") fields.push_back({8,0});
  if(!mesh || builder.state.meshTab!=1 || searching) {
    for(u32 i=0;i<entry.type->booleans.size();++i) if(show(entry.type->booleans[i].presentation)&&
      !((constantForce||tween||audio||entry.type==&scene::PathFollow::descriptor||std::string_view(entry.type->id).starts_with("astra.physics2d."))&&!searching&&entry.type->booleans[i].id=="enabled")) fields.push_back({0,i});
    for(u32 i=0;i<entry.type->enums.size();++i) {
      if(!show(entry.type->enums[i].presentation)) continue;
      fields.push_back({1,i});
    }
    for(u32 i=0;i<entry.type->slotEnums.size();++i) {
      const auto &property=entry.type->slotEnums[i];
      if(!show(property.presentation)) continue;
      for(u32 slot=0;slot<property.slotCount(*component)&&slot<256;++slot) fields.push_back({10,i,slot});
    }
  }
  // Barra dividida do LOD Group antes dos objetos de cada nível, como na Unity.
  if(!searching && !mesh && entry.type==&scene::LodGroup::descriptor && group=="Níveis") fields.push_back({3,widgetId(EditorWidget::LodBar)});
  for(u32 i=0;i<entry.type->references.size();++i) if(show(entry.type->references[i].presentation)) fields.push_back({4,i});
  // O seletor de malha atende qualquer binding refletido de malha. Material e
  // textura continuam nos editores próprios por slot, que também configuram
  // superfície, canais e amostragem no mesmo contexto.
  for(u32 i=0;i<entry.type->resourceBindings.size();++i) {
    const auto &binding=entry.type->resourceBindings[i];
    const bool lightmap=binding.kind==resources::AssetType::Texture&&binding.id=="texture.lightmap";
    if((mesh&&!searching&&!lightmap)||(binding.kind!=resources::AssetType::Mesh&&!lightmap&&
             binding.kind!=resources::AssetType::EnvironmentProfile&&
             binding.kind!=resources::AssetType::EnvironmentMap&&
             binding.kind!=resources::AssetType::AnimationClip&&binding.kind!=resources::AssetType::AudioClip&&binding.kind!=resources::AssetType::UiDocument&&
             binding.kind!=resources::AssetType::PhysicsMaterial)||!show(binding.presentation)) continue;
    for(u32 slot=0;slot<binding.slotCount(*component);++slot) fields.push_back({6,i,slot});
  }
  if(entry.type==&scene::UiCanvas::descriptor && !searching && group=="Canvas")fields.push_back({3,widgetId(EditorWidget::GuiCanvasEdit)});
  if(mesh && !builder.state.meshTab && !searching) {
    fields.push_back({3,widgetId(EditorWidget::MeshChoose)});
    fields.push_back({3,widgetId(EditorWidget::ToggleCastShadow)});
  } else {
    if(mesh && builder.state.meshTab==1 && !searching) fields.push_back({3,widgetId(EditorWidget::MaterialRestore)});
    // Ajustar escolhe a primitiva que contém a malha; com a forma Malha a forma
    // já É a malha, e o botão só trocaria a escolha do autor por uma caixa.
    if(!searching && entry.type==&EditorCollider::descriptor && group=="Forma" && meshAsset(entity) &&
       !scene::colliderIsMesh(*component)) fields.push_back({3,widgetId(EditorWidget::ColliderFit)});
    if(!searching && entry.type==&scene::LodGroup::descriptor && group=="Limites") fields.push_back({3,widgetId(EditorWidget::LodGroupFit)});
    if(!searching && entry.type==&scene::LodGroup::descriptor && group=="Níveis" && !builder.state.lodStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::LodGroupStatus)});
    if(!searching && entry.type==&scene::SkinnedMesh::descriptor && group=="Skin" && !builder.state.skinStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::SkinnedMeshStatus)});
    if(!searching && entry.type==&scene::Animation::descriptor && group=="Clipes" && !builder.state.animationStatus.empty())
      fields.push_back({3,widgetId(EditorWidget::AnimationStatus)});
    if(!searching && entry.type==&scene::Animation::descriptor && group=="Clipes" &&
       static_cast<const scene::Animation &>(*component).clips.size()<scene::Animation::MaximumClips)
      fields.push_back({3,widgetId(EditorWidget::ComponentClipAdd)});
    for(u32 i=0;(!mesh||builder.state.meshTab==2||searching)&&i<entry.type->numbers.size();++i) {
      if(!show(entry.type->numbers[i].presentation)) continue;
      bool grouped=false;
      for(u32 t=0;t<entry.type->triples.size();++t) {
        const auto &triple=entry.type->triples[t];
        bool complete=true;
        for(u32 axis=0;axis<triple.dimensions();++axis) {
          const auto channel=triple.channels[axis];
          bool found=false;
          for(const auto &p:entry.type->numbers) if(p.id==channel && show(p.presentation)) found=true;
          complete=complete&&found;
        }
        if(!complete) continue;
        for(u32 axis=0;axis<triple.dimensions();++axis) if(entry.type->numbers[i].id==triple.channels[axis]) {
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
  if((std::string_view(entry.type->id).starts_with("astra.constraint.")||std::string_view(entry.type->id).starts_with("astra.spring."))&&!searching) {
    u32 enabled=~u32{0},source=~u32{0};
    for(const auto &field:fields) {
      if(field.kind==0&&entry.type->booleans[field.index].id=="enabled") enabled=field.index;
      if(field.kind==4&&entry.type->references[field.index].id=="target") source=field.index;
    }
    if(enabled!=~u32{0}&&source!=~u32{0}) {
      std::erase_if(fields,[&](const Field &field){return field.kind==0&&field.index==enabled;});
      for(auto &field:fields) if(field.kind==4&&field.index==source) {field.kind=14;field.second=enabled;}
    }
    // Os eixos são uma máscara de canais, não três páginas de liga/desliga.
    // Cada caixa conserva o setter, estado misto e reset do booleano refletido.
    constexpr std::string_view axisSets[2][3]{{"axis_x","axis_y","axis_z"},{"rotation_x","rotation_y","rotation_z"}};
    for(const auto &names:axisSets) {
    u32 axes[3]{~u32{0},~u32{0},~u32{0}};
    for(const auto &field:fields) if(field.kind==0)
      for(u32 axis=0;axis<3;++axis) if(entry.type->booleans[field.index].id==names[axis]) axes[axis]=field.index;
    if(std::all_of(std::begin(axes),std::end(axes),[](u32 i){return i!=~u32{0};})) {
      std::vector<Field> compact;
      for(auto field:fields) {
        if(field.kind==0&&field.index==axes[0]) {field.kind=13;field.second=axes[1];field.slot=axes[2];}
        else if(field.kind==0&&(field.index==axes[1]||field.index==axes[2])) continue;
        compact.push_back(field);
      }
      fields=std::move(compact);
    }
    }
  }
  if(std::string_view(entry.type->id).starts_with("astra.physics2d.")&&!searching) {
    constexpr std::string_view pairs[][2]{{"velocity_x","velocity_y"},{"half_x","half_y"},{"offset_x","offset_y"},
      {"force_x","force_y"},{"relative_force_x","relative_force_y"},{"anchor_a_x","anchor_a_y"},{"anchor_b_x","anchor_b_y"}};
    for(const auto &names:pairs) {
      u32 channels[2]{~u32{0},~u32{0}};
      for(const auto &field:fields) if(field.kind==2)
        for(u32 axis=0;axis<2;++axis) if(entry.type->numbers[field.index].id==names[axis]) channels[axis]=field.index;
      if(channels[0]==~u32{0}||channels[1]==~u32{0}) continue;
      std::erase_if(fields,[&](const Field &field){return field.kind==2&&field.index==channels[1];});
      for(auto &field:fields) if(field.kind==2&&field.index==channels[0]) {field.kind=16;field.second=channels[1];}
    }
  }
  if(tween&&!searching) {
    if(group=="Tempo") {
      for(u32 pair=0;pair<2;++pair) {
        u32 number=~u32{0},companion=~u32{0};const u32 companionKind=pair?0u:1u;
        for(const auto &field:fields) {
          if(field.kind==2&&entry.type->numbers[field.index].id==(pair?"delay":"duration")) number=field.index;
          if(field.kind==companionKind&&(pair?entry.type->booleans[field.index].id=="autoplay":entry.type->enums[field.index].id=="easing")) companion=field.index;
        }
        if(number==~u32{0}||companion==~u32{0}) continue;
        std::erase_if(fields,[&](const Field &field){return field.kind==companionKind&&field.index==companion;});
        for(auto &field:fields) if(field.kind==2&&field.index==number) {field.kind=pair?19u:18u;field.second=companion;}
      }
    }
    constexpr std::string_view channels[]{"position","rotation","scale"};
    for(const auto channel:channels) {
      u32 enabled=~u32{0},triple=~u32{0};
      for(const auto &field:fields) {
        if(field.kind==0&&entry.type->booleans[field.index].id==channel) enabled=field.index;
        if(field.kind==5&&entry.type->triples[field.index].id==std::string(channel)+"_destination") triple=field.index;
      }
      if(enabled==~u32{0}||triple==~u32{0}) continue;
      std::erase_if(fields,[&](const Field &field){return field.kind==0&&field.index==enabled;});
      for(auto &field:fields) if(field.kind==5&&field.index==triple) {field.kind=17;field.second=enabled;}
    }
    const auto rank=[&](const Field &field) {
      if(field.kind==2) {const auto id=entry.type->numbers[field.index].id;if(id=="duration") return 0;if(id=="delay") return 2;}
      if(field.kind==1&&entry.type->enums[field.index].id=="easing") return 1;
      if(field.kind==0&&entry.type->booleans[field.index].id=="autoplay") return 3;
      return 4;
    };
    std::stable_sort(fields.begin(),fields.end(),[&](const Field &a,const Field &b){return rank(a)<rank(b);});
  }
  if(audio&&!searching) {
    if(entry.type->id==std::string_view{"astra.audio.bus"}) {
      u32 mute=~u32{0},solo=~u32{0};
      for(const auto &field:fields) if(field.kind==0) {
        const auto id=entry.type->booleans[field.index].id;if(id=="mute") mute=field.index;if(id=="solo") solo=field.index;
      }
      if(mute!=~u32{0}&&solo!=~u32{0}) {
        std::erase_if(fields,[&](const Field &field){return field.kind==0&&field.index==solo;});
        for(auto &field:fields) if(field.kind==0&&field.index==mute) {field.kind=15;field.second=solo;}
      }
    }
    const auto rank=[&](const Field &field) {
      if(entry.type->id==std::string_view{"astra.audio.bus"}&&field.kind==4) return 0;
      if(field.kind==6) return 0;
      if(field.kind==2&&entry.type->numbers[field.index].id=="volume") return 1;
      if(field.kind==1) return 2;
      if(field.kind==15) return 2;
      return 3;
    };
    std::stable_sort(fields.begin(),fields.end(),[&](const Field &a,const Field &b){return rank(a)<rank(b);});
  }
  if(entry.type==&scene::PathFollow::descriptor&&!searching&&group=="Percurso") {
    const auto rank=[](const Field &field){return field.kind==4?0:field.kind==1?1:2;};
    std::stable_sort(fields.begin(),fields.end(),[&](const Field&a,const Field&b){return rank(a)<rank(b);});
  }
  if(entry.type==&scene::PathFollow::descriptor&&!searching) {
    if(group=="Percurso") {
      u32 initial=~u32{0},advance=~u32{0};
      for(const auto&f:fields)if(f.kind==2){const auto id=entry.type->numbers[f.index].id;if(id=="progress_distance")initial=f.index;else if(id=="speed"||id=="duration")advance=f.index;}
      if(initial!=~u32{0}&&advance!=~u32{0}){std::erase_if(fields,[&](const Field&f){return f.kind==2&&f.index==advance;});for(auto&f:fields)if(f.kind==2&&f.index==initial){f.kind=21;f.second=advance;}}
    } else if(group=="Orientação") {
      u32 channels[3]{~u32{0},~u32{0},~u32{0}};constexpr std::string_view ids[]{"offset_x","offset_y","offset_z"};
      for(const auto&f:fields)if(f.kind==2)for(u32 a=0;a<3;++a)if(entry.type->numbers[f.index].id==ids[a])channels[a]=f.index;
      if(channels[0]!=~u32{0}&&channels[1]!=~u32{0}&&channels[2]!=~u32{0}){std::erase_if(fields,[&](const Field&f){return f.kind==2&&(f.index==channels[1]||f.index==channels[2]);});for(auto&f:fields)if(f.kind==2&&f.index==channels[0]){f.kind=20;f.second=channels[1];f.slot=channels[2];}}
    } else if(group=="Execução") {
      u32 autoplay=~u32{0},loop=~u32{0};for(const auto&f:fields)if(f.kind==0){const auto id=entry.type->booleans[f.index].id;if(id=="autoplay")autoplay=f.index;else if(id=="loop")loop=f.index;}
      if(autoplay!=~u32{0}&&loop!=~u32{0}){std::erase_if(fields,[&](const Field&f){return f.kind==0&&f.index==loop;});for(auto&f:fields)if(f.kind==0&&f.index==autoplay){f.kind=15;f.second=loop;}}
    }
  }
  // Ação e receptor primeiro: uma ativação cabe numa página do telefone; método
  // e valor só existem quando a ação chama método e ficam na sequência.
  // Material do corpo: primeiro o recurso, depois cada valor seguido da sua
  // combinação, como na ficha do PhysicsMaterial.
  if(entry.type==&scene::PhysicsBody::descriptor&&!searching&&group=="Material") {
    const auto rank=[&](const Field &field) {
      if(field.kind==6) return 0;
      const std::string_view id=field.kind==2?entry.type->numbers[field.index].id:field.kind==1?entry.type->enums[field.index].id:std::string_view{};
      return id=="friction"?1:id=="friction_combine"?2:id=="restitution"?3:id=="restitution_combine"?4:5;
    };
    std::stable_sort(fields.begin(),fields.end(),[&](const Field &a,const Field &b){return rank(a)<rank(b);});
  }
  // Sequência: uma linha por etapa, na ordem de execução (objeto, espera e
  // "junto da anterior"), em vez de três listas separadas por tipo de campo.
  if(entry.type==&scene::TweenSequence::descriptor&&!searching&&group=="Etapas") {
    fields.clear();
    for(u32 i=0;i<scene::kTweenSequenceSteps&&i<entry.type->references.size();++i) {
      if(!entry.type->references[i].presentation.isVisible(*component)) continue;
      fields.push_back({22,i,i,i>0?2+i:~u32{0}});
    }
  }
  if(entry.type==&scene::EventConnection::descriptor&&!searching&&group=="Então") {
    const auto rank=[&](const Field &field){
      if(field.kind==1) return entry.type->enums[field.index].id=="action"?0:2;
      return field.kind==4?1:3;
    };
    std::stable_sort(fields.begin(),fields.end(),[&](const Field &a,const Field &b){return rank(a)<rank(b);});
  }
  if(entry.type->id==std::string_view{"astra.physics2d.joint"}&&!searching&&group=="Conexão") {
    const auto rank=[](const Field &field){return field.kind==1?0:field.kind==4?1:2;};
    std::stable_sort(fields.begin(),fields.end(),[&](const Field &a,const Field &b){return rank(a)<rank(b);});
  }
  if(mesh&&state.meshTab==2&&!searching) {
    // Compor controles de um mesmo recurso evita sete linhas escalares. Os
    // endereços individuais continuam os do schema e dos widgets já existentes.
    std::vector<Field> compact;
    for(auto field:fields) {
      if(field.kind==10&&entry.type->slotEnums[field.index].id=="lightmap.enabled"&&entry.type->slotEnums[field.index].options.size()==2) {
        const bool paired=std::any_of(fields.begin(),fields.end(),[&](const Field &other){
          return other.kind==6&&other.slot==field.slot&&entry.type->resourceBindings[other.index].id=="texture.lightmap";
        });
        if(paired) continue;
      }
      if(field.kind==6&&entry.type->resourceBindings[field.index].id=="texture.lightmap") {
        for(const auto &other:fields) if(other.kind==10&&other.slot==field.slot&&entry.type->slotEnums[other.index].id=="lightmap.enabled"&&entry.type->slotEnums[other.index].options.size()==2) {
          field.kind=12;field.second=other.index;break;
        }
      }
      if(field.kind==9) {
        const auto id=entry.type->slotNumbers[field.index].id;
        const bool first=id=="lightmap.scale_u"||id=="lightmap.offset_u";
        const bool second=id=="lightmap.scale_v"||id=="lightmap.offset_v";
        const auto mate=id=="lightmap.scale_u"?"lightmap.scale_v":id=="lightmap.offset_u"?"lightmap.offset_v":
                        id=="lightmap.scale_v"?"lightmap.scale_u":"lightmap.offset_u";
        if(first||second) for(const auto &other:fields) if(other.kind==9&&other.slot==field.slot&&entry.type->slotNumbers[other.index].id==mate) {
          if(second) field.kind=~u32{0};else {field.kind=11;field.second=other.index;}
          break;
        }
      }
      if(field.kind!=~u32{0}) compact.push_back(field);
    }
    fields=std::move(compact);
    // Cada slot fica junto: receber/textura → escala → deslocamento → intensidade.
    std::stable_sort(fields.begin(),fields.end(),[](const Field &a,const Field &b){return a.slot<b.slot;});
  }
  if(searching) {
    const auto matches=[&](std::string_view name,std::string_view id,std::string_view propertyGroup) {
      return editorSearchKey(std::string(name)+" "+std::string(id)+" "+std::string(propertyGroup)).find(search)!=std::string::npos;
    };
    std::erase_if(fields,[&](const Field &field) {
      switch(field.kind) {
      case 0: {const auto &p=entry.type->booleans[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 10: {const auto &p=entry.type->slotEnums[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 1: {const auto &p=entry.type->enums[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 2: {const auto &p=entry.type->numbers[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 4: {const auto &p=entry.type->references[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 5: {
        const auto &triple=entry.type->triples[field.index];
        for(const auto &p:entry.type->numbers) for(const auto channel:triple.channels)
          if(p.id==channel && (matches(triple.name,triple.id,p.presentation.group)||matches(p.name,p.id,p.presentation.group))) return false;
        return true;
      }
      case 6: {const auto &p=entry.type->resourceBindings[field.index];return !matches(p.name,p.id,p.presentation.group);}
      case 9: {
        const auto &p=entry.type->slotNumbers[field.index];
        return !matches(p.name,p.id,p.presentation.group) &&
          (entry.type!=&scene::SkinnedMesh::descriptor || field.slot>=state.blendShapeNames.size() ||
           editorSearchKey(state.blendShapeNames[field.slot]).find(search)==std::string::npos);
      }
      default: return true;
      }
    });
    builder.label(takeTop(content,24),std::to_string(fields.size())+(fields.size()==1?" propriedade encontrada":" propriedades encontradas"),theme.color.textMuted,theme.type.caption);
    if(fields.empty()) {builder.label(content,"Nenhuma propriedade encontrada",theme.color.textDim,theme.type.caption);return;}
  }
  if(content.height<40) return;
  const auto defaults=entry.type->create();
  const auto changes=defaults?scene::componentDelta(*component,*defaults):std::vector<scene::FieldDelta>{};
  const auto resetFields=[&](const Field &field) {
    std::vector<scene::FieldAddress> addresses;
    const auto add=[&](std::string_view id,scene::FieldKind kind,u32 slot=0) {
      for(const auto &change:changes) if(change.address==scene::FieldAddress{id,slot,kind} && change.differs && change.applicable)
        addresses.push_back(change.address);
    };
    switch(field.kind) {
    case 0:add(entry.type->booleans[field.index].id,scene::FieldKind::Boolean);break;
    case 1:add(entry.type->enums[field.index].id,scene::FieldKind::Enum);break;
    case 2:add(entry.type->numbers[field.index].id,scene::FieldKind::Number);break;
    case 4:add(entry.type->references[field.index].id,scene::FieldKind::Reference);break;
    case 5:for(const auto channel:entry.type->triples[field.index].channels) add(channel,scene::FieldKind::Number);break;
    case 9:add(entry.type->slotNumbers[field.index].id,scene::FieldKind::SlotNumber,field.slot);break;
    case 10:add(entry.type->slotEnums[field.index].id,scene::FieldKind::SlotEnum,field.slot);break;
    default:break;
    }
    return addresses;
  };
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-30)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(fields.size())+perPage-1)/perPage);
  const u32 page=std::min(builder.state.propertyPage,pages-1);
  auto footer=pages>1?takeBottom(content,30):UiRect{};
  for(u32 row=page*perPage;row<fields.size() && row<(page+1)*perPage;++row) {
    const auto f=fields[row];auto slot=takeTop(content,std::min(40.0f,content.height));
    const auto reset=resetFields(f);
    if(!reset.empty() && index<256 && f.index<256 && f.slot<256) {
      const auto button=takeRight(slot,30);
      builder.list.addImage(centred(button,18,18),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
      builder.router.addRegion(button,widgetId(EditorWidget::ComponentFieldResetBase)+index+(f.kind<<8)+(f.index<<12)+(f.slot<<20));
    }
    const auto hit=slot;
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
    } else if(f.kind==13) {
      builder.label(takeLeft(slot,46),std::string_view(entry.type->booleans[f.index].id).starts_with("rotation_")?"Rotação":"Eixos",theme.color.textMuted,theme.type.caption);
      const float width=slot.width/3;
      const u32 fieldsByAxis[]{f.index,f.second,f.slot};const char *names[]{"X","Y","Z"};
      for(u32 axis=0;axis<3;++axis) {
        auto cell=takeLeft(slot,width);const auto field=fieldsByAxis[axis];const auto &property=entry.type->booleans[field];
        builder.label(takeLeft(cell,12),names[axis],theme.color.textDim,theme.type.caption);
        if(!resetFields({0,field}).empty()) {
          const auto resetHit=takeRight(cell,18);
          builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(field<<12));
        }
        builder.checkbox(cell,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(field<<8),
                         property.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),property.id)));
      }
    } else if(f.kind==20||f.kind==21) {
      const bool triple=f.kind==20;
      if(triple)builder.label(takeLeft(slot,36),"Frame",theme.color.textDim,theme.type.caption);
      const u32 channels[]{f.index,f.second,f.slot};const u32 count=triple?3u:2u;const float width=slot.width/count;
      for(u32 a=0;a<count;++a){auto cell=takeLeft(slot,width);const u32 field=channels[a];const auto&p=entry.type->numbers[field];
        if(!resetFields({2,field}).empty()){const auto reset=takeRight(cell,triple?16.f:20.f);builder.list.addImage(centred(reset,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);builder.router.addRegion(reset,widgetId(EditorWidget::ComponentFieldResetBase)+index+(2u<<8)+(field<<12));}
        const auto hit=cell;builder.list.addRect(deflate(cell,UiInsets::all(2)),theme.color.silhouette,theme.radius.control);
        const std::string label=triple?(a==0?"X (u)":a==1?"Y (u)":"Z (u)"):(a==0?"Inicial (u)":p.id=="speed"?"Vel. (u/s)":"Duração (s)");
        builder.label(takeTop(cell,17),label,theme.color.textDim,theme.type.caption,UiAlign::Center);
        char value[24];std::snprintf(value,sizeof(value),"%.4g",static_cast<double>(p.read(*component)));const bool mixed=builder.mixed(multiKey(component->instanceId(),p.id));
        builder.label(cell,mixed?"—":value,theme.color.text,theme.type.caption,UiAlign::Center);if(p.presentation.isEditable(*component))builder.router.addRegion(hit,widgetId(EditorWidget::ComponentNumberBase)+index+(field<<8));
      }
    } else if(f.kind==15) {
      const float width=slot.width*.5f;const u32 channels[]{f.index,f.second};
      for(const auto field:channels) {
        auto cell=takeLeft(slot,width);const auto &property=entry.type->booleans[field];
        const auto check=takeRight(cell,32);
        if(!resetFields({0,field}).empty()) {
          const auto resetHit=takeRight(cell,18);
          builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(field<<12));
        }
        const std::string name=entry.type==&scene::PathFollow::descriptor?(property.id=="autoplay"?"Auto":"Repetir"):std::string{property.name};
        builder.label(cell,fitMiddle(builder.list,name,cell.width,theme.type.caption),theme.color.textDim,theme.type.caption);
        builder.checkbox(check,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(field<<8),
                         property.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),property.id)));
      }
    } else if(f.kind==18||f.kind==19) {
      const auto &number=entry.type->numbers[f.index];
      builder.label(takeLeft(slot,42),f.kind==18?"Duração":"Atraso",theme.color.textDim,theme.type.caption);
      auto numeric=takeLeft(slot,66);
      const auto resetOne=[&](UiRect &area,u32 kind,u32 field) {
        if(resetFields({kind,field}).empty()) return;
        const auto resetHit=takeRight(area,20);
        builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
        builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(kind<<8)+(field<<12));
      };
      resetOne(numeric,2,f.index);
      builder.list.addRect(deflate(numeric,UiInsets::all(2)),theme.color.silhouette,theme.radius.control);
      char text[32];std::snprintf(text,sizeof(text),"%.3g s",static_cast<double>(number.read(*component)));
      const bool mixedNumber=builder.mixed(multiKey(component->instanceId(),number.id));
      builder.label(numeric,mixedNumber?"—":text,theme.color.text,theme.type.caption,UiAlign::Center);
      if(number.presentation.isEditable(*component)) builder.router.addRegion(numeric,widgetId(EditorWidget::ComponentNumberBase)+index+(f.index<<8));
      if(f.kind==18) {
        const auto &property=entry.type->enums[f.second];resetOne(slot,1,f.second);
        const auto enumHit=slot;auto box=deflate(slot,UiInsets::all(2));
        builder.list.addRect(box,theme.color.raised,theme.radius.control);
        builder.list.addImage(centred(takeRight(box,16),10,10),static_cast<UiImageId>(UiIcon::UiChevronDown),theme.color.textDim);
        const char *label="Inválido";for(const auto &option:property.options) if(option.value==property.read(*component)) label=option.name;
        if(builder.mixed(multiKey(component->instanceId(),property.id))) label="—";
        builder.label(box,label,theme.color.text,theme.type.caption,UiAlign::Center);
        if(property.presentation.isEditable(*component)) builder.router.addRegion(enumHit,widgetId(EditorWidget::ComponentEnumBase)+index+(f.second<<8));
      } else {
        const auto &property=entry.type->booleans[f.second];resetOne(slot,0,f.second);
        const auto check=takeLeft(slot,32);
        builder.checkbox(check,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.second<<8),
                         property.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),property.id)));
        builder.label(slot,"Auto",theme.color.textDim,theme.type.caption);
      }
    } else if(f.kind==16) {
      const auto id=entry.type->numbers[f.index].id;
      const char *title=id=="velocity_x"?"Vel. (m/s)":id=="half_x"?"Meia dim. (m)":id=="force_x"?"Mundo (N)":
        id=="relative_force_x"?"Local (N)":id=="anchor_a_x"?"Âncora A":id=="anchor_b_x"?"Âncora B":"Centro (m)";
      builder.label(takeLeft(slot,std::min(72.f,slot.width*.34f)),title,theme.color.textDim,theme.type.caption);
      const float width=slot.width*.5f;
      const u32 channels[]{f.index,f.second};const char *names[]{"X","Y"};
      for(u32 axis=0;axis<2;++axis) {
        auto cell=takeLeft(slot,width);const auto field=channels[axis];const auto &property=entry.type->numbers[field];
        builder.label(takeLeft(cell,13),names[axis],axis?theme.color.axisY:theme.color.axisX,theme.type.caption);
        if(!resetFields({2,field}).empty()) {
          const auto resetHit=takeRight(cell,20);
          builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(2u<<8)+(field<<12));
        }
        const auto hit=cell;builder.list.addRect(deflate(cell,UiInsets::all(2)),theme.color.silhouette,theme.radius.control);
        char value[32];std::snprintf(value,sizeof(value),"%.4g",static_cast<double>(property.read(*component)));
        const bool mixed=builder.mixed(multiKey(component->instanceId(),property.id));
        builder.label(cell,mixed?"—":value,mixed?theme.color.textDim:theme.color.text,theme.type.caption,UiAlign::Center);
        if(property.presentation.isEditable(*component)) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentNumberBase)+index+(field<<8));
      }
    } else if(f.kind==10) {
      const auto &property=entry.type->slotEnums[f.index];
      const auto widget=widgetId(EditorWidget::ComponentSlotEnumBase)+index+(f.index<<8)+(f.slot<<16);
      const bool editable=property.write&&!property.options.empty()&&property.presentation.isEditable(*component);
      const u32 current=property.read(*component,f.slot);
      const bool mixed=builder.mixed(multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(f.slot)));
      const std::string name=std::string(property.name)+" · "+std::to_string(f.slot+1);
      if(property.options.size()==2) {
        auto check=takeRight(slot,32);
        builder.label(slot,name,theme.color.textDim,theme.type.caption);
        builder.checkbox(check,current==property.options[1].value,widget,editable,mixed);
      } else {
        builder.label(takeLeft(slot,slot.width*.48f),name,theme.color.textDim,theme.type.caption);
        const char *value="Valor inválido";
        for(const auto &option:property.options) if(option.value==current) value=option.name;
        builder.label(slot,mixed?"—":value,editable?theme.color.text:theme.color.textMuted,theme.type.caption);
        if(editable) builder.router.addRegion(hit,widget);
      }
    } else if(f.kind==0) {
      const auto &property=entry.type->booleans[f.index];auto toggle=takeRight(slot,44);
      builder.label(slot,property.name,theme.color.textDim,theme.type.caption);
      if(builder.mixed(multiKey(component->instanceId(),property.id)))
        builder.mixedToggle(toggle,widgetId(EditorWidget::ComponentBooleanBase)+index+(f.index<<8),property.presentation.isEditable(*component));
      else builder.toggle(toggle,property.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.index<<8),property.presentation.isEditable(*component));
    } else if(f.kind==1) {
      const auto &property=entry.type->enums[f.index];
      builder.label(takeLeft(slot,slot.width*.38f),property.name,theme.color.textDim,theme.type.caption);
      const char *label="Valor inválido";
      for(const auto &option:property.options) if(option.value==property.read(*component)) label=option.name;
      if(builder.mixed(multiKey(component->instanceId(),property.id))) label="\xE2\x80\x94";
      // Aparência de lista suspensa: o toque abre as opções, não cicla às cegas.
      const bool editable=property.presentation.isEditable(*component);
      auto box=deflate(slot,UiInsets::all(2));
      builder.list.addRect(box,theme.color.raised,theme.radius.control);
      auto chevron=takeRight(box,24);
      if(editable) builder.list.addImage(centred(chevron,10,10),static_cast<UiImageId>(UiIcon::UiChevronDown),theme.color.textDim);
      builder.label(deflate(box,UiInsets{8,0,0,0}),label,editable?theme.color.text:theme.color.textMuted,theme.type.caption);
      if(editable) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentEnumBase)+index+(f.index<<8));
    } else if(f.kind==22) {
      const auto &reference=entry.type->references[f.index];const auto &interval=entry.type->numbers[f.slot];
      const auto target=reference.read(*component);const bool used=target!=0;
      const bool joined=f.second!=~u32{0}&&entry.type->booleans[f.second].read(*component);
      if(used&&f.second!=~u32{0}) {
        auto cell=takeRight(slot,44);const auto &join=entry.type->booleans[f.second];
        builder.label(takeTop(cell,14),"Junto",theme.color.textDim,theme.type.caption,UiAlign::Center);
        builder.checkbox(centred(cell,26,24),joined,widgetId(EditorWidget::ComponentBooleanBase)+index+(f.second<<8),
                         join.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),join.id)));
      }
      if(used) {
        auto cell=takeRight(slot,60);
        builder.label(takeTop(cell,14),"Espera",theme.color.textDim,theme.type.caption,UiAlign::Center);
        const auto box=deflate(cell,UiInsets::all(2));
        builder.list.addRect(box,theme.color.silhouette,theme.radius.control);
        char text[32];std::snprintf(text,sizeof(text),"%.3g s",static_cast<double>(interval.read(*component)));
        builder.label(box,builder.mixed(multiKey(component->instanceId(),interval.id))?"â":text,theme.color.text,theme.type.caption,UiAlign::Center);
        if(interval.presentation.isEditable(*component)) builder.router.addRegion(box,widgetId(EditorWidget::ComponentNumberBase)+index+(f.slot<<8));
      }
      const auto referenceHit=slot;
      auto title=takeTop(slot,17);
      const auto badge=takeLeft(title,18);
      builder.list.addRect(centred(badge,16,14),joined?withAlpha(theme.color.accent,.25f):theme.color.raised,4);
      builder.label(badge,std::to_string(f.slot+1),joined?theme.color.accent:theme.color.text,theme.type.caption,UiAlign::Center);
      builder.label(title,f.slot==0?"Primeira etapa":joined?"Junto da anterior":"Depois da anterior",theme.color.textMuted,theme.type.caption);
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      const auto readiness=runtime::referenceReadiness(*builder.state.document,entity.id,*component,reference);
      const bool warning=used&&readiness!=runtime::ReferenceReadiness::Ready&&readiness!=runtime::ReferenceReadiness::OptionalEmpty;
      std::string label=!used?std::string{"Escolher objeto com Transform Tween"}:object?object->name:std::string{"Objeto ausente"};
      if(used&&readiness==runtime::ReferenceReadiness::Incompatible&&object) label+=" · sem tween";
      builder.list.addImage(centred(takeRight(slot,22),16,16),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      builder.label(slot,fitMiddle(builder.list,label,slot.width,theme.type.caption),warning?theme.color.warning:used?theme.color.text:theme.color.textDim,theme.type.caption);
      if(reference.presentation.isEditable(*component)) builder.router.addRegion(referenceHit,widgetId(EditorWidget::ComponentReferenceBase)+index+(f.index<<8));
    } else if(f.kind==4||f.kind==14) {
      const auto &property=entry.type->references[f.index];const auto target=property.read(*component);
      if(f.kind==14) {
        const auto &enabled=entry.type->booleans[f.second];const auto check=takeLeft(slot,32);
        builder.checkbox(check,enabled.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.second<<8),
                         enabled.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),enabled.id)));
        if(!resetFields({0,f.second}).empty()) {
          const auto resetHit=takeLeft(slot,18);
          builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(f.second<<12));
        }
        if(!resetFields({4,f.index}).empty()) {
          const auto resetHit=takeRight(slot,20);
          builder.list.addImage(centred(resetHit,12,12),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(4u<<8)+(f.index<<12));
        }
      }
      const auto referenceHit=slot;
      builder.label(takeTop(slot,17),property.name,theme.color.textMuted,theme.type.caption);
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      const auto readiness=runtime::referenceReadiness(*builder.state.document,entity.id,*component,property);
      const bool warning=readiness!=runtime::ReferenceReadiness::Ready && readiness!=runtime::ReferenceReadiness::OptionalEmpty;
      std::string referenceLabel=!target?property.nullLabel:object?object->name:"Objeto ausente";
      if(builder.mixed(multiKey(component->instanceId(),property.id))) referenceLabel="\xE2\x80\x94";
      if(readiness==runtime::ReferenceReadiness::RequiredEmpty) referenceLabel="Obrigatório · "+referenceLabel;
      else if(readiness==runtime::ReferenceReadiness::Inactive) referenceLabel+=" · inativo";
      else if(readiness==runtime::ReferenceReadiness::Incompatible && object) referenceLabel+=" · incompatível";
      builder.list.addImage(centred(takeRight(slot,24),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      builder.label(slot,fitMiddle(builder.list,referenceLabel,slot.width,theme.type.caption),warning?theme.color.warning:theme.color.text,theme.type.caption);
      if(property.presentation.isEditable(*component)) builder.router.addRegion(referenceHit,widgetId(EditorWidget::ComponentReferenceBase)+index+(f.index<<8));
    } else if(f.kind==5||f.kind==17) {
      const auto &triple=entry.type->triples[f.index];
      if(f.kind==17&&!resetFields({5,f.index}).empty()) {
        const auto resetHit=takeRight(slot,30);
        builder.list.addImage(centred(resetHit,18,18),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
        builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(5u<<8)+(f.index<<12));
      }
      auto title=takeLeft(slot,f.kind==17?58.f:slot.width*(constantForce?.34f:.30f));
      if(f.kind==17) {
        const auto &enabled=entry.type->booleans[f.second];
        builder.checkbox(takeLeft(title,24),enabled.read(*component),widgetId(EditorWidget::ComponentBooleanBase)+index+(f.second<<8),
                         enabled.presentation.isEditable(*component),builder.mixed(multiKey(component->instanceId(),enabled.id)));
        if(!resetFields({0,f.second}).empty()) {
          const auto resetHit=takeRight(title,12);
          builder.list.addImage(centred(resetHit,10,10),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(f.second<<12));
        }
      }
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
      if(f.kind==17) groupTitle=triple.id==std::string_view{"position_destination"}?"Pos":triple.id==std::string_view{"rotation_destination"}?"Rot":"Esc";
      if(constantForce) groupTitle=std::string_view(triple.id).find("torque")!=std::string_view::npos?"Torque":"Força";
      std::string_view unit;bool firstUnit=true,consistentUnit=true;
      for(const auto channel:triple.channels) for(const auto &p:entry.type->numbers) if(p.id==channel) {
        if(firstUnit) {unit=p.presentation.unit;firstUnit=false;}
        else if(unit!=p.presentation.unit) consistentUnit=false;
      }
      if(f.kind!=17&&consistentUnit && !unit.empty()) groupTitle+=" ("+std::string(unit)+")";
      builder.label(title,groupTitle,theme.color.textDim,theme.type.caption);
      const float cellWidth=slot.width/triple.dimensions();
      for(u32 axis=0;axis<triple.dimensions();++axis) {
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
        const bool mixedChannel=builder.mixed(multiKey(component->instanceId(),property.id));
        builder.label(body,mixedChannel?"\xE2\x80\x94":value,mixedChannel?theme.color.textDim:theme.color.text,theme.type.caption,UiAlign::Center);
        if(property.presentation.isEditable(*component)) builder.router.addRegion(cell,widgetId(EditorWidget::ComponentNumberBase)+index+(channel<<8));
      }
    } else if(f.kind==2) {
      const auto &property=entry.type->numbers[f.index];
      builder.label(takeLeft(slot,slot.width*.62f),property.name,theme.color.textDim,theme.type.caption);
      char value[32];std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(property.read(*component)));
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.silhouette,8);
      if(!property.presentation.unit.empty()) builder.label(takeRight(slot,24),property.presentation.unit,theme.color.textMuted,theme.type.caption,UiAlign::Center);
      const bool mixedNumber=builder.mixed(multiKey(component->instanceId(),property.id));
      builder.label(slot,mixedNumber?"\xE2\x80\x94":value,!mixedNumber&&property.presentation.isEditable(*component)?theme.color.text:theme.color.textMuted,theme.type.numeric,UiAlign::Center);
      if(property.presentation.isEditable(*component)) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentNumberBase)+index+(f.index<<8));
    } else if(f.kind==11) {
      const auto id=entry.type->slotNumbers[f.index].id;
      std::string name=id=="lightmap.scale_u"?"Escala UV":"Desloc. UV";
      if(entry.type->slotNumbers[f.index].slotCount(*component)>1) name+=" · "+std::to_string(f.slot+1);
      builder.label(takeLeft(slot,std::min(74.f,slot.width*.34f)),name,theme.color.textDim,theme.type.caption);
      const float width=slot.width*.5f;
      for(u32 axis=0;axis<2;++axis) {
        const u32 field=axis?f.second:f.index;
        const auto &property=entry.type->slotNumbers[field];
        auto cell=takeLeft(slot,width);const auto valueHit=cell;
        const auto key=multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(f.slot));
        const bool editable=property.write&&property.presentation.isEditable(*component);
        builder.list.addRect(deflate(cell,UiInsets::all(2)),theme.color.silhouette,4);
        builder.label(takeLeft(cell,16),axis?"V":"U",theme.color.textMuted,theme.type.caption,UiAlign::Center);
        const auto reset=resetFields({9,field,f.slot});
        auto resetHit=reset.empty()?UiRect{}:takeRight(cell,20);
        char value[24];std::snprintf(value,sizeof(value),"%.4g",static_cast<double>(property.read(*component,f.slot)));
        builder.label(cell,builder.mixed(key)?"—":value,editable?theme.color.text:theme.color.textMuted,theme.type.numeric,UiAlign::Center);
        if(editable) builder.router.addRegion(valueHit,widgetId(EditorWidget::ComponentSlotNumberBase)+index+(field<<8)+(f.slot<<16));
        if(!resetHit.isEmpty()) {
          builder.list.addImage(centred(resetHit,14,14),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(resetHit,widgetId(EditorWidget::ComponentFieldResetBase)+index+(9u<<8)+(field<<12)+(f.slot<<20));
        }
      }
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
      const bool mixed=builder.mixed(multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(f.slot)));
      builder.label(slot,mixed?"—":value,editable?theme.color.text:theme.color.textMuted,theme.type.numeric,UiAlign::Center);
      if(editable) builder.router.addRegion(hit,widgetId(EditorWidget::ComponentSlotNumberBase)+index+(f.index<<8)+(f.slot<<16));
    } else if(f.kind==6||f.kind==12) {
      const auto &binding=entry.type->resourceBindings[f.index];const auto asset=binding.at(*component,f.slot);
      const bool clipEntry=entry.type==&scene::Animation::descriptor && binding.id=="clips";
      auto pickerHit=hit;
      UiRect enabledHit{},enabledReset{};
      if(f.kind==12) {
        enabledHit=takeLeft(slot,36);
        pickerHit.x=slot.x;pickerHit.width=slot.width;
        if(!resetFields({10,f.second,f.slot}).empty()) {
          enabledReset=takeRight(slot,24);pickerHit.width-=24;
        }
      }
      if(clipEntry) {
        const auto encoded=index+(f.index<<8)+(f.slot<<16);
        auto remove=takeRight(slot,32),down=takeRight(slot,32),up=takeRight(slot,32);
        pickerHit.width=std::max(0.0f,pickerHit.width-96.0f);
        builder.label(up,"↑",f.slot?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
        builder.label(down,"↓",f.slot+1<binding.slotCount(*component)?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
        builder.label(remove,"×",theme.color.danger,theme.type.body,UiAlign::Center);
        if(f.slot) builder.router.addRegion(up,widgetId(EditorWidget::ComponentClipMoveUpBase)+encoded);
        if(f.slot+1<binding.slotCount(*component)) builder.router.addRegion(down,widgetId(EditorWidget::ComponentClipMoveDownBase)+encoded);
        builder.router.addRegion(remove,widgetId(EditorWidget::ComponentClipRemoveBase)+encoded);
      }
      std::string name=binding.name;
      if(f.kind==12) name="Receber lightmap indireto";
      if(binding.slotCount(*component)>1) name+=" · "+std::to_string(f.slot+1);
      builder.label(takeTop(slot,17),name.c_str(),theme.color.textMuted,theme.type.caption);
      std::string value;
      if(!asset.valid()) {
        value=binding.kind==resources::AssetType::EnvironmentProfile?"Sem perfil":
              binding.kind==resources::AssetType::PhysicsMaterial?"Sem material · valores deste corpo":
              binding.kind==resources::AssetType::EnvironmentMap?"Ambiente padrão":
              binding.kind==resources::AssetType::AnimationClip||binding.kind==resources::AssetType::AudioClip?"Nenhum clipe":
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
      else if(asset.valid()&&binding.kind==resources::AssetType::PhysicsMaterial) value="Material ausente · "+asset.text().substr(0,8)+" · cópia local";
      if(builder.mixed(multiKey(component->instanceId(),std::string(binding.id)+"@"+std::to_string(f.slot)))) value="—";
      builder.list.addImage(centred(takeRight(slot,24),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.textDim);
      builder.label(slot,fitMiddle(builder.list,value,slot.width,theme.type.caption),theme.color.text,theme.type.caption);
      if(binding.presentation.isEditable(*component))
        builder.router.addRegion(pickerHit,widgetId(EditorWidget::ComponentResourceBase)+index+(f.index<<8)+(f.slot<<16));
      if(f.kind==12) {
        const auto &property=entry.type->slotEnums[f.second];
        const bool mixed=builder.mixed(multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(f.slot)));
        builder.checkbox(enabledHit,property.read(*component,f.slot)==property.options[1].value,
            widgetId(EditorWidget::ComponentSlotEnumBase)+index+(f.second<<8)+(f.slot<<16),
            property.write&&property.presentation.isEditable(*component),mixed);
        if(!enabledReset.isEmpty()) {
          builder.list.addImage(centred(enabledReset,14,14),static_cast<UiImageId>(UiIcon::EditorAuthorUndo),theme.color.accent);
          builder.router.addRegion(enabledReset,widgetId(EditorWidget::ComponentFieldResetBase)+index+(10u<<8)+(f.second<<12)+(f.slot<<20));
        }
      }
    } else if(f.index==widgetId(EditorWidget::GuiCanvasEdit)) {
      builder.label(slot,"Editar documento UI",theme.color.text,theme.type.caption);
      builder.list.addImage(centred(takeRight(slot,26),18,18),static_cast<UiImageId>(UiIcon::UiInterfaceCanvas),theme.color.accent);
      if(static_cast<const scene::UiCanvas&>(*component).document.valid())builder.router.addRegion(hit,f.index);
    } else if(f.index==widgetId(EditorWidget::MeshChoose)) {
      builder.list.addImage(centred(takeRight(slot,30),22,22),static_cast<UiImageId>(UiIcon::EditorAuthorZoom),theme.color.text);
      const auto ref=meshAsset(entity);const auto text=ref?"Malha "+std::to_string(ref):"Escolher malha";
      builder.label(slot,text.c_str(),theme.color.text,theme.type.caption);
      builder.router.addRegion(hit,f.index);
    } else if(f.index==widgetId(EditorWidget::ToggleCastShadow)) {
      auto toggle=takeRight(slot,44);builder.label(slot,"Projetar sombra",theme.color.textDim,theme.type.caption);
      builder.toggle(toggle,entity.castShadow,f.index);
    } else if(f.index==widgetId(EditorWidget::LodBar)) {
      // Unity Manual/InspectorBarSliders e LOD Group: da esquerda (100% da
      // tela) para a direita (0%), um segmento por nível e o Culled; cada um
      // mostra onde termina. Divisores arrastam, o marcador é a vista atual.
      const auto &group=static_cast<const scene::LodGroup &>(*component);
      const auto bar=deflate(hit,UiInsets{0,2,0,2});
      const u32 count=group.levelCount;
      const UiColor colors[]{0xff3f8f5au,0xff2f6f9fu,0xff6f5aa6u,0xff9a7a38u};
      const auto xAt=[&](float percent){return bar.x+(1-percent/100.f)*bar.width;};
      float start=100;
      for(u32 i=0;i<=count;++i) {
        const float end=i<count?group.transitions[i]:0;
        const UiRect segment{xAt(start),bar.y,xAt(end)-xAt(start),bar.height};
        const bool culled=i==count,chosen=builder.state.lodSelected==i+1;
        builder.list.addRect(segment,culled?0xff7a2e2eu:colors[i%4],0);
        if(chosen) builder.list.addRect({segment.x,segment.bottom()-3,segment.width,3},0xffffffffu,0);
        builder.list.pushClip(segment);
        char text[24];
        if(culled) std::snprintf(text,sizeof(text),"Culled");
        else std::snprintf(text,sizeof(text),"LOD %u · %.0f%%",i,static_cast<double>(end));
        builder.label(deflate(segment,UiInsets{5,0,3,0}),text,0xffffffffu,theme.type.caption);
        builder.list.popClip();
        builder.router.addRegion(segment,widgetId(EditorWidget::LodBarSegmentBase)+i);
        start=end;
      }
      // Divisores depois dos segmentos: ficam por cima para o dedo pegar.
      for(u32 i=0;i<count;++i) {
        const float x=xAt(group.transitions[i]);
        builder.list.addRect({x-1.5f,bar.y-2,3,bar.height+4},0xffffffffu,1);
        builder.router.addRegion({x-12,bar.y,24,bar.height},widgetId(EditorWidget::LodBarDividerBase)+i);
      }
      if(builder.state.lodViewPercent>=0) {
        const float x=xAt(std::min(builder.state.lodViewPercent,100.f));
        builder.list.addRect({x-5,bar.y-5,10,5},theme.color.accent,2);
        builder.list.addRect({x-1,bar.y,2,bar.height},withAlpha(theme.color.accent,.8f),0);
      }
      if(builder.layout) builder.layout->lodBar=bar;
      // Menu do segmento (toque longo), sobre a própria barra.
      if(builder.state.lodMenu) {
        const u32 i=builder.state.lodMenu-1;
        const bool canInsert=count<scene::LodGroupMaximumLevels,canDelete=i<count&&count>1;
        const UiRect menu{std::clamp(xAt(i<count?(i?group.transitions[i-1]:100):group.transitions[count-1])-4,bar.x,bar.right()-150),
                          bar.y,150,bar.height};
        builder.list.addRect(menu,theme.color.raised,theme.radius.control);
        const auto insert=UiRect{menu.x,menu.y,menu.width*.5f,menu.height};
        const auto remove=UiRect{menu.x+menu.width*.5f,menu.y,menu.width*.5f,menu.height};
        builder.label(insert,"Inserir antes",canInsert?theme.color.accent:theme.color.textFaint,theme.type.caption,UiAlign::Center);
        builder.label(remove,"Apagar",canDelete?theme.color.axisX:theme.color.textFaint,theme.type.caption,UiAlign::Center);
        if(canInsert) builder.router.addRegion(insert,widgetId(EditorWidget::LodBarInsert));
        if(canDelete) builder.router.addRegion(remove,widgetId(EditorWidget::LodBarDelete));
      }
    } else if(f.index==widgetId(EditorWidget::LodGroupStatus)) {
      // Leitura, não controle: sem região de toque.
      builder.label(slot,builder.state.lodStatus.c_str(),theme.color.accent,theme.type.caption);
    } else if(f.index==widgetId(EditorWidget::SkinnedMeshStatus)||f.index==widgetId(EditorWidget::AnimationStatus)) {
      builder.label(slot,(f.index==widgetId(EditorWidget::SkinnedMeshStatus)?builder.state.skinStatus:builder.state.animationStatus).c_str(),
                    theme.color.accent,theme.type.caption);
    } else if(f.index==widgetId(EditorWidget::ComponentClipAdd)) {
      builder.label(slot,"+ Adicionar clipe",theme.color.accent,theme.type.caption);
      builder.router.addRegion(hit,f.index);
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
  if(resourceBinding&&resourceBinding->kind==resources::AssetType::AudioClip) {
    const auto import=takeRight(title,68);
    builder.list.addRect(deflate(import,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    const bool editable=state.workspace!=EditorWorkspace::Play;
    builder.label(import,editable?"+ WAV":"Em Edit",editable?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(editable) builder.router.addRegion(import,widgetId(EditorWidget::ImportWave));
  }
  builder.label(title,resourceBinding?resourceBinding->name:"Geometria",theme.color.text,theme.type.body);
  auto search=takeTop(content,36);builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(search,state.meshQuery.empty()?
      (resourceBinding&&resourceBinding->kind==resources::AssetType::EnvironmentProfile?"Buscar perfil":
       resourceBinding&&resourceBinding->kind==resources::AssetType::EnvironmentMap?"Buscar mapa HDRI":
       resourceBinding&&resourceBinding->kind==resources::AssetType::Texture?"Buscar textura":
       resourceBinding&&resourceBinding->kind==resources::AssetType::AudioClip?"Buscar WAV":
       resourceBinding&&resourceBinding->kind==resources::AssetType::UiDocument?"Buscar documento UI":
       resourceBinding&&resourceBinding->kind==resources::AssetType::PhysicsMaterial?"Buscar material físico":
       resourceBinding&&resourceBinding->kind==resources::AssetType::AnimationClip?"Buscar clipe":"Buscar malha"):
      state.meshQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::MeshSearch));
  auto footer=takeBottom(content,36),previous=takeLeft(footer,36),next=takeRight(footer,36);
  if(resourceBinding&&(resourceBinding->kind==resources::AssetType::EnvironmentProfile||
                       resourceBinding->kind==resources::AssetType::EnvironmentMap||
                       resourceBinding->kind==resources::AssetType::Texture||resourceBinding->kind==resources::AssetType::AudioClip||resourceBinding->kind==resources::AssetType::UiDocument||
                       resourceBinding->kind==resources::AssetType::PhysicsMaterial)) {
    if(resourceBinding->kind==resources::AssetType::PhysicsMaterial&&state.workspace!=EditorWorkspace::Play) {
      // Criar copia os valores deste corpo para um recurso; atualizar leva os
      // valores editados aqui a todos os corpos que usam o mesmo material.
      const auto action=deflate(takeTop(content,40),UiInsets::all(2));
      builder.list.addRect(action,theme.color.accent,theme.radius.control);
      auto label=action;
      builder.list.addImage(centred(takeLeft(label,34),20,20),static_cast<UiImageId>(UiIcon::PhysicsMaterial),theme.color.accentInk);
      builder.label(label,selectedResource.valid()?"Atualizar material com os valores deste corpo":"Criar material com os valores deste corpo",
                    theme.color.accentInk,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(action,widgetId(selectedResource.valid()?EditorWidget::PhysicsMaterialUpdate:EditorWidget::PhysicsMaterialCreate));
    }
    if(resourceBinding->kind==resources::AssetType::EnvironmentProfile) {
      const auto action=deflate(takeTop(content,40),UiInsets::all(2));
      builder.list.addRect(action,theme.color.accent,theme.radius.control);
      builder.label(action,selectedResource.valid()?"Atualizar perfil com estes valores":"Criar perfil com estes valores",
                    theme.color.accentInk,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(action,widgetId(selectedResource.valid()?EditorWidget::EnvironmentProfileUpdate:
                                                 EditorWidget::EnvironmentProfileCreate));
    }
    std::vector<u32> matches;const auto query=editorSearchKey(state.meshQuery);u32 resourceCount=0;
    if(state.assetRegistry) for(u32 i=0;i<state.assetRegistry->records().size();++i) {
      const auto &record=state.assetRegistry->records()[i];
      if(record.type!=resourceBinding->kind) continue;
      ++resourceCount;
      if(query.empty()||editorSearchKey(record.path).find(query)!=std::string::npos) matches.push_back(i);
    }
    const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-34)/50));
    const u32 pages=std::max(1u,(static_cast<u32>(matches.size())+perPage-1)/perPage);
    const u32 page=std::min(state.meshPage,pages-1);
    auto clear=takeTop(content,34);builder.label(clear,
        resourceBinding->kind==resources::AssetType::EnvironmentProfile?"Sem perfil · conservar cópia local":
        resourceBinding->kind==resources::AssetType::PhysicsMaterial?"Sem material · conservar os valores do corpo":
        resourceBinding->kind==resources::AssetType::Texture?"Sem textura":resourceBinding->kind==resources::AssetType::AudioClip?"Nenhum clipe WAV":resourceBinding->kind==resources::AssetType::UiDocument?"Sem documento UI · canvas inativo":"Sem mapa HDRI",
        theme.color.textDim,theme.type.caption);
    builder.router.addRegion(clear,widgetId(EditorWidget::MeshClear));
    for(u32 row=page*perPage;row<matches.size()&&row<(page+1)*perPage;++row) {
      const auto index=matches[row];const auto &record=state.assetRegistry->records()[index];
      auto slot=takeTop(content,50);const auto hit=slot;
      builder.list.addRect(deflate(slot,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      if(selectedResource==record.guid) builder.list.addRect({slot.x,slot.y+4,3,slot.height-8},theme.color.accent,1);
      builder.list.addImage(centred(takeLeft(slot,40),28,28),static_cast<UiImageId>(resourceBinding->id=="texture.lightmap"?UiIcon::LightingLightmap:resourceBinding->kind==resources::AssetType::AudioClip?UiIcon::AudioClip:resourceBinding->kind==resources::AssetType::Texture?UiIcon::AssetsTexture:resourceBinding->kind==resources::AssetType::UiDocument?UiIcon::UiInterfaceCanvas:UiIcon::LightingSun),0xffffffff);
      const auto slash=record.path.find_last_of('/');const auto name=record.path.substr(slash==std::string::npos?0:slash+1);
      builder.label(takeTop(slot,25),name.c_str(),theme.color.text,theme.type.body);
      builder.label(slot,("GUID "+record.guid.text().substr(0,8)+" · compartilhado").c_str(),theme.color.textMuted,theme.type.caption);
      builder.router.addRegion(hit,widgetId(EditorWidget::MeshChoiceBase)+index);
    }
    if(matches.empty()) builder.label(content,resourceCount&& !query.empty()?
        (resourceBinding->kind==resources::AssetType::AudioClip?"Nenhum WAV corresponde à busca":"Nenhum recurso corresponde à busca"):
        resourceBinding->kind==resources::AssetType::EnvironmentProfile?
        "Nenhum perfil no projeto":resourceBinding->kind==resources::AssetType::PhysicsMaterial?"Nenhum material físico no projeto":resourceBinding->kind==resources::AssetType::Texture?
        "Nenhuma textura no projeto":resourceBinding->kind==resources::AssetType::AudioClip?"Nenhum WAV importado no projeto":resourceBinding->kind==resources::AssetType::UiDocument?"Salve uma interface .aeui para registrar o documento":"Nenhum mapa HDRI no projeto",theme.color.textMuted,theme.type.caption);
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
void buildScriptFields(ScreenBuilder &builder,UiRect content,const scene::ScriptBehavior &script,u32 index,EditorEntityId owner) {
  const auto &theme=builder.theme;
  // Ativo mora no interruptor do cabeçalho e "Abrir código" no menu ⋮: repetir
  // as duas aqui roubava duas das poucas linhas que o telefone tem para campos.
  const auto *schema=scriptSchema(builder.state,script.scriptType);
  if(!schema) {builder.label(content,"Tipo não resolvido; dados preservados",theme.color.textMuted,theme.type.caption);return;}
  if(builder.state.playInspect && builder.state.scriptInspectionEntity!=owner) {
    builder.label(content,"Selecione o objeto para ler os campos em Play",theme.color.textMuted,theme.type.caption);return;
  }
  if(builder.state.playInspect && !builder.state.scriptInspectionError.empty()) {
    builder.label(content,builder.state.scriptInspectionError.c_str(),theme.color.warning,theme.type.caption);return;
  }
  const auto issueFor=[&](std::string_view id)->const runtime::ScriptFieldIssue* {
    if(!builder.state.playInspect) return nullptr;
    for(const auto &issue:builder.state.scriptFieldIssues) if(issue.instance==script.instanceId() && issue.property==id) return &issue;
    return nullptr;
  };
  usize orphaned=0;
  for(const auto &value:script.properties) if(std::none_of(schema->properties.begin(),schema->properties.end(),[&](const auto &property){return property.id==value.id;})) ++orphaned;
  if(orphaned) builder.label(takeTop(content,30),(std::to_string(orphaned)+" campos preservados fora do schema").c_str(),theme.color.warning,theme.type.caption);
  // Uma linha por campo; a lista aberta desdobra Tamanho, elementos e + / −
  // logo abaixo do próprio campo (Unity Manual/InspectorArray). Campos com
  // [HideInInspector] ficam fora da tela e continuam no arquivo e no Play.
  struct Row {enum Kind {Field,Size,Element,Footer} kind;u32 field;u32 element;};
  std::vector<Row> rows;
  std::vector<std::vector<std::string>> arrays(schema->properties.size());
  const auto authored=[&](const EditorScriptProperty &property)->const scene::ScriptPropertyValue * {
    for(const auto &p:script.properties) if(p.id==property.id && p.valueType==property.valueType) return &p;
    return nullptr;
  };
  for(u32 field=0;field<schema->properties.size();++field) {
    const auto &property=schema->properties[field];
    if(property.hidden) continue;
    rows.push_back({Row::Field,field,0});
    if(issueFor(property.id) || scene::scriptArrayElementType(property.valueType).empty() || builder.state.expandedScriptArray!=property.id) continue;
    if(const auto *value=authored(property)) scene::parseScriptArray(value->value,arrays[field]);
    rows.push_back({Row::Size,field,0});
    for(u32 element=0;element<arrays[field].size();++element) rows.push_back({Row::Element,field,element});
    rows.push_back({Row::Footer,field,0});
  }
  // Valor de um campo simples ou de um elemento: referências mostram o nome,
  // componentes o ícone do tipo e a ausência em cor de aviso.
  const auto drawValue=[&](UiRect row,std::string_view type,const std::string *text,bool compact=false) {
    std::string shown=text?*text:std::string("Padrão do código");
    bool missing=false;
    const auto componentType=scene::scriptComponentTypeId(type);
    const auto *componentSchema=componentType.empty()?nullptr:scene::findComponentSchema(componentType);
    if(type=="object") {
      u64 target=0;if(text) {std::istringstream in(*text);in>>target;}
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      shown=!target?"Escolher objeto":object?object->name:"Objeto ausente";missing=target&&!object;
    } else if(!componentType.empty()) {
      const char *typeName=componentSchema?componentSchema->name:"Componente";
      u64 target=0,instance=0;if(text) scene::parseScriptComponentValue(*text,target,instance);
      const auto *object=target<=std::numeric_limits<EditorEntityId>::max()?builder.state.document->find(static_cast<EditorEntityId>(target)):nullptr;
      const bool present=object&&object->components.findInstance(instance)&&
                         object->components.findInstance(instance)->type().id==componentType;
      // No elemento de lista o tipo já está no cabeçalho e no ícone.
      shown=!target?std::string("Escolher ")+typeName:present?(compact?std::string(object->name):std::string(object->name)+" · "+typeName):
            std::string(typeName)+" ausente";
      missing=target&&!present;
    } else if(type=="string" && text && text->empty()) shown="(vazio)";
    else if(scene::scriptCurveType(type)) {
      // Curva: a própria forma, sem texto.
      scene::ScriptCurve curve;if(text) scene::parseScriptCurve(*text,curve);
      builder.list.addRect(row,theme.color.raised,theme.radius.control);
      if(curve.keys.empty()) builder.label(deflate(row,UiInsets{10,0,6,0}),text?"Curva vazia":"Padrão do código",theme.color.textMuted,theme.type.caption);
      else drawCurveThumbnail(builder,deflate(row,UiInsets{8,6,8,6}),curve,theme.color.accent);
      return;
    }
    else if(scene::scriptGradientType(type)) {
      // Gradiente: a própria faixa, sem texto.
      scene::ScriptGradient gradient;if(text) scene::parseScriptGradient(*text,gradient);
      builder.list.addRect(row,theme.color.raised,theme.radius.control);
      drawGradientBar(builder,deflate(row,UiInsets{6,8,6,8}),gradient,32);
      if(!text) builder.label(deflate(row,UiInsets{10,0,6,0}),"Padrão do código",0xff202020u,theme.type.caption);
      return;
    }
    else if(bool hdr=false,alpha=true;scene::scriptColorType(type,&hdr,&alpha)) {
      // Cor: amostra à esquerda e o hexadecimal (com a intensidade se HDR).
      float rgba[4]{1,1,1,1};if(text) scene::parseScriptColor(*text,rgba);
      float base[3],intensity=0;splitHdr({rgba[0],rgba[1],rgba[2]},base,intensity);
      float srgb[3];for(u32 i=0;i<3;++i) srgb[i]=colorToSrgb(base[i]);
      float h=0,s=0,v=0;srgbToHsv(srgb,h,s,v);
      builder.list.addRect(row,theme.color.raised,theme.radius.control);
      auto inner=deflate(row,UiInsets{4,4,6,4});
      const auto swatch=takeLeft(inner,std::min(56.f,inner.width*.4f));
      builder.list.addRect(swatch,pickerColor(h,s,v),theme.radius.control);
      if(alpha) builder.list.addRect({swatch.x,swatch.bottom()-3,swatch.width*rgba[3],3},0xffffffffu,1);
      takeLeft(inner,6);
      char note[24]{};if(hdr&&intensity>0) std::snprintf(note,sizeof(note)," · +%.1f",intensity);
      builder.label(inner,"#"+formatColorHex(srgb,rgba[3],alpha)+note,text?theme.color.text:theme.color.textMuted,theme.type.caption);
      return;
    }
    builder.list.addRect(row,theme.color.raised,theme.radius.control);
    if(componentSchema)
      builder.list.addImage(centred(takeLeft(row,24),16,16),static_cast<UiImageId>(editorIconByName(componentSchema->icon)),
                            missing?theme.color.warning:theme.color.accent);
    else takeLeft(row,6);
    builder.label(row,shown,missing?theme.color.warning:text?theme.color.text:theme.color.textMuted,theme.type.caption);
  };
  const u32 visible=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-32)/40));
  const u32 pages=std::max(1u,(static_cast<u32>(rows.size())+visible-1)/visible);
  const u32 page=std::min(builder.state.scriptPropertyPage,pages-1);
  auto footer=takeBottom(content,32);
  for(u32 at=page*visible;at<rows.size() && at<(page+1)*visible;++at) {
    const auto &item=rows[at];
    const auto &property=schema->properties[item.field];
    const auto element=scene::scriptArrayElementType(property.valueType);
    auto row=takeTop(content,40);const auto hit=deflate(row,UiInsets{0,2,0,2});
    if(item.kind==Row::Field) {
      builder.label(takeLeft(row,row.width*.43f),property.name.c_str(),theme.color.textDim,theme.type.caption);
      const auto *value=authored(property);
      bool changedType=false;
      for(const auto &p:script.properties) if(p.id==property.id && p.valueType!=property.valueType) changedType=true;
      auto box=deflate(row,UiInsets{0,2,0,2});
      if(const auto *issue=issueFor(property.id)) {
        builder.label(box,issue->isNull?"Nulo · editar":issue->description.c_str(),issue->isNull?theme.color.textMuted:theme.color.warning,theme.type.caption);
        // Null is a real value, not a zero/default. The existing field editor
        // can explicitly initialize it; failed getters remain read-only.
        if(issue->isNull) builder.router.addRegion(hit,widgetId(EditorWidget::ScriptFieldBase)+index+(item.field<<8));
        continue;
      }
      if(!element.empty()) {
        // Cabeçalho da lista: seta, contagem e o tipo dos elementos.
        std::vector<std::string> items;if(value) scene::parseScriptArray(value->value,items);
        const bool open=builder.state.expandedScriptArray==property.id;
        builder.list.addRect(box,open?theme.color.line:theme.color.raised,theme.radius.control);
        auto inner=box;
        builder.label(takeLeft(inner,20),open?"v":">",theme.color.textDim,theme.type.caption,UiAlign::Center);
        const std::string summary=changedType?"Tipo alterado":!value?"Padrão do código":
            std::to_string(items.size())+(items.size()==1?" elemento":" elementos");
        builder.label(inner,summary,value?theme.color.text:theme.color.textMuted,theme.type.caption);
      } else if(changedType) {
        builder.list.addRect(box,theme.color.raised,theme.radius.control);
        builder.label(deflate(box,UiInsets{6,0,6,0}),"Tipo alterado",theme.color.warning,theme.type.caption);
      } else if(property.valueType=="bool") {
        builder.toggle(takeRight(box,48),value&&value->value=="true",widgetId(EditorWidget::ScriptFieldBase)+index+(item.field<<8));
        builder.label(box,value?"":"Padrão do código",theme.color.textMuted,theme.type.caption);
      } else drawValue(box,property.valueType,value?&value->value:nullptr);
      builder.router.addRegion(hit,widgetId(EditorWidget::ScriptFieldBase)+index+(item.field<<8));
      continue;
    }
    // Linhas da lista aberta, recuadas sob o cabeçalho.
    takeLeft(row,14);
    builder.list.addRect({row.x-8,row.y,2,row.height},theme.color.line);
    if(item.kind==Row::Size) {
      builder.label(takeLeft(row,row.width*.40f),"Tamanho",theme.color.textDim,theme.type.caption);
      const auto box=deflate(row,UiInsets{0,2,0,2});
      builder.list.addRect(box,theme.color.raised,theme.radius.control);
      builder.label(deflate(box,UiInsets{6,0,6,0}),std::to_string(arrays[item.field].size()).c_str(),theme.color.text,theme.type.caption);
      builder.router.addRegion(box,widgetId(EditorWidget::ScriptArraySizeBase)+index);
    } else if(item.kind==Row::Element) {
      const bool selected=builder.state.scriptArraySelected==item.element+1;
      const bool lifted=builder.state.scriptArrayDrag==item.element+1;
      const bool target=builder.state.scriptArrayDrag && builder.state.scriptArrayDragTarget==item.element+1 && !lifted;
      // Alça: arrastar pela alça reordena, como o ≡ da Unity.
      const auto handle=takeLeft(row,28);
      for(u32 line=0;line<3;++line)
        builder.list.addRect({handle.x+8,handle.y+14+line*5.f,12,2},selected?theme.color.accent:theme.color.textMuted,1);
      builder.router.addRegion(handle,widgetId(EditorWidget::ScriptArrayHandleBase)+index+(item.element<<8));
      // Índice num selo curto: "Elemento 12" não cabe ao lado do valor no telefone.
      const auto badge=centred(takeLeft(row,34),28,24);
      builder.list.addRect(badge,selected?theme.color.accent:theme.color.silhouette,theme.radius.control);
      builder.label(badge,std::to_string(item.element).c_str(),selected?theme.color.accentInk:theme.color.textDim,
                    theme.type.caption,UiAlign::Center);
      takeLeft(row,4);
      const auto box=deflate(row,UiInsets{0,2,0,2});
      if(element=="bool") {
        builder.list.addRect(box,theme.color.raised,theme.radius.control);
        auto inner=deflate(box,UiInsets{0,4,4,4});
        builder.toggle(takeRight(inner,48),arrays[item.field][item.element]=="true",
                       widgetId(EditorWidget::ScriptArrayElementBase)+index+(item.element<<8));
      } else drawValue(box,element,&arrays[item.field][item.element],true);
      if(selected) builder.list.addRect({box.x,box.bottom()-2,box.width,2},theme.color.accent,1);
      if(lifted) builder.list.addRect(hit,withAlpha(theme.color.voidBlack,.45f),theme.radius.control);
      if(target) {
        const float y=builder.state.scriptArrayDragTarget<builder.state.scriptArrayDrag?hit.y-2:hit.bottom()-1;
        builder.list.addRect({hit.x,y,hit.width,3},theme.color.accent,1);
      }
      builder.router.addRegion(box,widgetId(EditorWidget::ScriptArrayElementBase)+index+(item.element<<8));
    } else {
      // + copia o último elemento (Unity); − remove o escolhido ou o último.
      const auto add=takeLeft(row,row.width*.5f-2);takeLeft(row,4);
      builder.list.addRect(deflate(add,UiInsets{0,3,0,3}),withAlpha(theme.color.accent,.16f),theme.radius.control);
      builder.label(add,"+ Adicionar",theme.color.accent,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(add,widgetId(EditorWidget::ScriptArrayAddBase)+index);
      const bool any=!arrays[item.field].empty();
      builder.list.addRect(deflate(row,UiInsets{0,3,0,3}),theme.color.raised,theme.radius.control);
      builder.label(row,builder.state.scriptArraySelected&&any?("- Remover "+std::to_string(builder.state.scriptArraySelected-1)).c_str():"- Remover último",
                    any?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
      if(any) builder.router.addRegion(row,widgetId(EditorWidget::ScriptArrayRemoveBase)+index);
    }
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
  // O avançado é uma janela sobre a tela (buildAdvancedReferencePicker); o
  // Inspector só diz que ela está aberta.
  if(state.pickerAdvanced) {builder.label(content,"Seletor de objeto aberto",theme.color.textMuted,theme.type.caption,UiAlign::Center);return;}
  const auto scriptReference=editorScriptReference(state.referenceScriptType);
  const auto *property=state.presetInputIndex?state.presetInputProperty:state.referenceScript?&scriptReference:editorReferenceProperty(entity,state.referenceInstance,state.referenceProperty);
  const auto *requiredSchema=property&&!property->requiredType.empty()?scene::findComponentSchema(property->requiredType):nullptr;
  auto header=takeTop(content,36),back=takeLeft(header,36);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);builder.router.addRegion(back,widgetId(EditorWidget::ReferenceClose));
  // Campo de componente: o cabeçalho diz qual tipo o objeto precisa ter.
  if(requiredSchema && state.referenceScript) {
    builder.list.addImage(centred(takeLeft(header,28),18,18),static_cast<UiImageId>(editorIconByName(requiredSchema->icon)),theme.color.accent);
    builder.label(header,(std::string("Componente · ")+requiredSchema->name).c_str(),theme.color.text,theme.type.body);
  } else builder.label(header,property?property->name:"Referência ausente",theme.color.text,theme.type.body);
  if(!property) return;
  auto searchRow=takeTop(content,38);
  // Unity: o seletor clássico tem filtro fixo; "Avançado" troca de modo.
  const auto mode=deflate(takeRight(searchRow,86),UiInsets::all(2));
  builder.list.addRect(mode,theme.color.raised,theme.radius.control);
  builder.label(mode,"Avançado",theme.color.accent,theme.type.caption,UiAlign::Center);
  builder.router.addRegion(mode,widgetId(EditorWidget::ReferenceModeToggle));
  auto search=deflate(searchRow,UiInsets::all(2));builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(search,state.referenceQuery.empty()?"Buscar objeto":state.referenceQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::ReferenceSearch));
  auto clear=takeTop(content,34);builder.label(clear,property->nullLabel,theme.color.textDim,theme.type.caption);builder.router.addRegion(clear,widgetId(EditorWidget::ReferenceClear));
  auto footer=takeBottom(content,32),previous=takeLeft(footer,36),next=takeRight(footer,36);
  const auto choices=state.presetInputIndex?editorRecipeReferenceChoices(*state.document,state.presetInputChoices,state.referenceQuery):editorReferenceChoices(*state.document,entity.id,*property,state.referenceQuery);
  const u32 visible=std::max(1u,static_cast<u32>(std::max(0.0f,content.height)/48));
  const u32 pages=std::max(1u,(static_cast<u32>(choices.size())+visible-1)/visible),page=std::min(state.referencePage,pages-1);
  for(u32 i=page*visible;i<choices.size()&&i<(page+1)*visible;++i) {
    auto row=takeTop(content,48);const auto hit=row;const auto *object=state.document->find(choices[i]);
    builder.list.addRect(deflate(row,UiInsets::all(2)),theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(takeLeft(row,36),24,24),static_cast<UiImageId>(UiIcon::EditorAuthorObject),0xffffffff);
    builder.label(takeTop(row,25),object->name,theme.color.text,theme.type.body);
    const auto *parent=state.document->find(object->parent);
    auto detail=std::string(parent?parent->name:"Cena")+" · "+std::to_string(object->id)+(object->active?"":" · inativo");
    // Vários do mesmo tipo: a Unity atribui o primeiro, e o seletor avisa.
    if(requiredSchema && state.referenceScript) {
      u32 count=0;for(usize c=0;c<object->components.size();++c) count+=object->components.at(c)->type().id==property->requiredType;
      if(count>1) detail+=" · "+std::to_string(count)+" deste tipo, usa o 1º";
    }
    builder.label(row,detail.c_str(),theme.color.textMuted,theme.type.caption);
    builder.router.addRegion(hit,widgetId(EditorWidget::ReferenceChoiceBase)+i);
  }
  if(choices.empty()) builder.label(content,"Nenhum objeto compatível",theme.color.textMuted,theme.type.caption);
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ReferencePrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ReferenceNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

// Seletor de objeto avançado (Unity 6000.0 Manual/search-advanced-object-
// picker), em janela própria como na Unity: consulta editável ("t:Tipo" e
// palavras), filtro de tipo do campo que se pode tirar, visões lista/grade/
// tabela à esquerda e, à direita, o painel do item destacado (caminho, estado,
// componentes) com "Escolher". Tocar destaca; o segundo toque ou "Escolher"
// atribuem. Incompatíveis aparecem apagados, com o motivo.
void buildAdvancedReferencePicker(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  const auto *entity=state.document?state.document->find(state.inspectorTarget?state.inspectorTarget:state.selection):nullptr;
  if(!entity) return;
  const auto scriptReference=editorScriptReference(state.referenceScriptType);
  const auto *propertyPointer=state.presetInputIndex?state.presetInputProperty:state.referenceScript?&scriptReference:editorReferenceProperty(*entity,state.referenceInstance,state.referenceProperty);
  if(!propertyPointer) return;
  const auto &property=*propertyPointer;
  const auto *required=property.requiredType.empty()?nullptr:scene::findComponentSchema(property.requiredType);
  router.addBlocker(state.surface);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.8f));
  const UiRect panel=centred(state.surface,std::min(700.f,state.surface.width-16),std::min(380.f,state.surface.height-16));
  list.addRect(panel,theme.color.surface,theme.radius.control);
  auto content=deflate(panel,UiInsets::all(10));
  auto header=takeTop(content,34),back=takeLeft(header,34);
  builder.label(back,"<",theme.color.text,theme.type.body,UiAlign::Center);router.addRegion(back,widgetId(EditorWidget::ReferenceClose));
  const auto mode=deflate(takeRight(header,92),UiInsets::all(2));
  list.addRect(mode,theme.color.raised,theme.radius.control);
  builder.label(mode,"Clássico",theme.color.textDim,theme.type.caption,UiAlign::Center);
  router.addRegion(mode,widgetId(EditorWidget::ReferenceModeToggle));
  const auto clear=deflate(takeRight(header,140),UiInsets::all(2));
  list.addRect(clear,theme.color.raised,theme.radius.control);
  builder.label(clear,property.nullLabel,theme.color.textDim,theme.type.caption,UiAlign::Center);
  router.addRegion(clear,widgetId(EditorWidget::ReferenceClear));
  builder.label(deflate(header,UiInsets{4,0,6,0}),required?std::string(property.name)+" · "+required->name:std::string(property.name),
                theme.color.text,theme.type.body);
  takeTop(content,6);
  // Consulta, filtro de tipo e visões.
  auto query=takeTop(content,34);
  const char *views[]{"Lista","Grade","Tabela"};
  for(u32 i=3;i-->0;) {
    const auto cell=deflate(takeRight(query,66),UiInsets::all(2));
    const bool on=state.pickerView==i;
    list.addRect(cell,on?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    builder.label(cell,views[i],on?theme.color.accent:theme.color.textDim,theme.type.caption,UiAlign::Center);
    router.addRegion(cell,widgetId(EditorWidget::ReferenceViewBase)+i);
  }
  takeRight(query,8);
  if(required) {
    const auto chip=deflate(takeLeft(query,170),UiInsets::all(2));
    const bool on=state.referenceTypeFilter;
    list.addRect(chip,on?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    builder.label(deflate(chip,UiInsets{8,0,8,0}),(on?std::string("t:")+required->name+"   ×":std::string("+ t:")+required->name).c_str(),
                  on?theme.color.accent:theme.color.textDim,theme.type.caption);
    router.addRegion(chip,widgetId(EditorWidget::ReferenceTypeFilter));
  }
  const auto search=deflate(query,UiInsets::all(2));
  list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(deflate(search,UiInsets{8,0,8,0}),state.referenceQuery.empty()?"Buscar · t:Tipo nome":state.referenceQuery.c_str(),
                state.referenceQuery.empty()?theme.color.textMuted:theme.color.text,theme.type.caption);
  router.addRegion(search,widgetId(EditorWidget::ReferenceSearch));
  takeTop(content,8);
  // Direita: painel de inspeção.
  auto inspection=takeRight(content,std::min(230.f,content.width*.38f));takeRight(content,10);
  const auto results=state.presetInputIndex?editorRecipeReferenceResults(*state.document,entity->id,state.presetInputChoices,state.referenceQuery,state.referenceTypeFilter):editorAdvancedReferenceResults(*state.document,entity->id,property,state.referenceQuery,state.referenceTypeFilter);
  const EditorReferenceResult *highlighted=nullptr;
  for(const auto &result:results) if(result.id==state.referenceHighlight) highlighted=&result;
  list.addRect(inspection,theme.color.silhouette,theme.radius.control);
  auto inner=deflate(inspection,UiInsets::all(10));
  if(!highlighted) {
    builder.label(takeTop(inner,inner.height*.5f),"Toque para ver o objeto",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.label(inner,"toque de novo para escolher",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  else {
    const auto *object=state.document->find(highlighted->id);
    const auto assign=takeBottom(inner,40);
    list.addRect(assign,highlighted->compatible?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(assign,highlighted->compatible?"Escolher":"O campo recusa",highlighted->compatible?theme.color.accentInk:theme.color.textFaint,
                  theme.type.caption,UiAlign::Center);
    if(highlighted->compatible) router.addRegion(assign,widgetId(EditorWidget::ReferenceAssign));
    takeBottom(inner,8);
    list.addImage(centred(takeTop(inner,40),32,32),static_cast<UiImageId>(UiIcon::EditorAuthorObject),0xffffffffu);
    builder.label(takeTop(inner,24),object->name,theme.color.text,theme.type.body,UiAlign::Center);
    std::string path;
    for(const auto *up=state.document->find(object->parent);up && up->id!=state.document->root();up=state.document->find(up->parent))
      path=path.empty()?std::string(up->name):std::string(up->name)+" / "+path;
    builder.label(takeTop(inner,18),path.empty()?"Na raiz da cena":path,theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.label(takeTop(inner,18),(std::string(object->active?"Ativo":"Inativo")+" · id "+std::to_string(object->id)).c_str(),
                  theme.color.textMuted,theme.type.caption,UiAlign::Center);
    takeTop(inner,6);
    // Um componente por linha, o exigido pelo campo em destaque.
    for(usize c=0;c<object->components.size() && inner.height>=22;++c) {
      const auto *value=object->components.at(c);
      const auto *schema=scene::findComponentSchema(value->type().id);
      const auto *script=scene::scriptBehavior(value);
      auto row=takeTop(inner,22);
      const bool isRequired=!property.requiredType.empty() && value->type().id==property.requiredType;
      list.addImage(centred(takeLeft(row,24),16,16),static_cast<UiImageId>(schema&&!script?editorIconByName(schema->icon):UiIcon::ScriptingCode),
                    isRequired?theme.color.accent:theme.color.textDim);
      builder.label(row,script?script->scriptType:schema?schema->name:std::string(value->type().id),
                    isRequired?theme.color.accent:theme.color.text,theme.type.caption);
    }
    if(object->components.size()==0) builder.label(takeTop(inner,22),"Sem componentes",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  // Esquerda: resultados.
  auto pager=takeBottom(content,28);
  if(state.pickerView==2) {
    auto head=takeTop(content,22);
    builder.label(takeLeft(head,head.width*.42f),"Nome",theme.color.textMuted,theme.type.caption);
    builder.label(takeLeft(head,head.width*.62f),"Pai",theme.color.textMuted,theme.type.caption);
    builder.label(head,"Componentes",theme.color.textMuted,theme.type.caption,UiAlign::End);
  }
  const float cellHeight=state.pickerView==1?72.f:state.pickerView==2?30.f:44.f;
  const u32 columns=state.pickerView==1?std::max(1u,static_cast<u32>(content.width/104)):1u;
  const u32 rows=std::max(1u,static_cast<u32>(content.height/cellHeight));
  const u32 perPage=rows*columns;
  const u32 pages=std::max(1u,(static_cast<u32>(results.size())+perPage-1)/perPage),page=std::min(state.referencePage,pages-1);
  const float cellWidth=content.width/columns;
  for(u32 i=page*perPage;i<results.size() && i<(page+1)*perPage;++i) {
    const u32 local=i-page*perPage;
    const UiRect cell{content.x+(local%columns)*cellWidth,content.y+(local/columns)*cellHeight,cellWidth,cellHeight};
    const auto *object=state.document->find(results[i].id);
    const bool chosen=results[i].id==state.referenceHighlight;
    const UiColor ink=results[i].compatible?theme.color.text:theme.color.textFaint;
    list.addRect(deflate(cell,UiInsets::all(2)),chosen?withAlpha(theme.color.accent,.22f):theme.color.raised,theme.radius.control);
    const auto *parent=state.document->find(object->parent);
    if(state.pickerView==1) {
      auto box=deflate(cell,UiInsets::all(8));
      list.addImage(centred(takeTop(box,30),26,26),static_cast<UiImageId>(UiIcon::EditorAuthorObject),results[i].compatible?0xffffffffu:0x55ffffffu);
      builder.label(box,object->name,ink,theme.type.caption,UiAlign::Center);
    } else if(state.pickerView==2) {
      auto box=deflate(cell,UiInsets{10,0,10,0});
      builder.label(takeLeft(box,box.width*.42f),object->name,ink,theme.type.caption);
      builder.label(takeLeft(box,box.width*.62f),parent&&parent->id!=state.document->root()?parent->name:"Cena",theme.color.textMuted,theme.type.caption);
      builder.label(box,std::to_string(object->components.size()).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::End);
    } else {
      auto box=deflate(cell,UiInsets{4,2,10,2});
      list.addImage(centred(takeLeft(box,36),22,22),static_cast<UiImageId>(UiIcon::EditorAuthorObject),results[i].compatible?0xffffffffu:0x55ffffffu);
      builder.label(takeTop(box,box.height*.55f),object->name,ink,theme.type.body);
      const auto detail=std::string(parent&&parent->id!=state.document->root()?parent->name:"Cena")+" · id "+std::to_string(object->id)+
          (results[i].compatible?"":" · o campo recusa");
      builder.label(box,detail.c_str(),results[i].compatible?theme.color.textMuted:theme.color.warning,theme.type.caption);
    }
    router.addRegion(cell,widgetId(EditorWidget::ReferenceResultBase)+i);
  }
  if(results.empty()) builder.label(content,"Nenhum objeto para esta consulta",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  const auto previous=takeLeft(pager,40),next=takeRight(pager,40);
  builder.label(previous,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(next,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) router.addRegion(previous,widgetId(EditorWidget::ReferencePrevious));
  if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::ReferenceNext));
  builder.label(pager,(std::to_string(results.size())+(results.size()==1?" objeto · ":" objetos · ")+std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),
                theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

void buildObjectFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity);
// Catálogo do Add: busca, trilho de famílias e lista rolável agrupada.
//
// Substitui cartões grandes paginados de cinco em cinco, que com 15 tipos já
// exigiam três páginas e com 200 exigiriam quarenta. O trilho mostra as
// famílias por ícone (a largura do Inspector no telefone é de ~230 dp); a lista
// agrupa por subfamília e rola pelo arraste, como a Hierarquia. Sem busca e em
// "Todos", abrem a lista o que o objeto sugere (tipos cujas exigências ele já
// cumpre) e o que foi adicionado por último nesta sessão.
struct CatalogRow {
  enum Kind : u8 {Header,Native,Script} kind=Header;
  std::string title;
  u32 index=0;
  // Sugeridos e Recentes repetem tipos que aparecem de novo na família.
  bool repeat=false;
};
std::vector<CatalogRow> componentCatalogRows(const EditorScreenState &state,const EditorEntity &entity,
                                             std::string_view query,u32 family) {
  std::vector<CatalogRow> rows;
  const auto native=[&](u32 index,bool repeat=false){rows.push_back({CatalogRow::Native,{},index,repeat});};
  const auto header=[&](std::string title){rows.push_back({CatalogRow::Header,std::move(title),0});};
  const auto scripts=[&](bool filtered) {
    if(!state.code) return;
    bool first=true;
    for(u32 i=0;i<state.code->scriptTypes().size();++i) {
      const auto &type=state.code->scriptTypes()[i];
      if(filtered && editorSearchKey(type.name+" "+type.id).find(query)==std::string::npos) continue;
      if(first && !filtered) header("Scripts do projeto");
      first=false;rows.push_back({CatalogRow::Script,{},i});
    }
  };
  if(!query.empty()) {
    for(u32 i=0;i<editorComponentCatalog.size();++i) {
      const auto &entry=editorComponentCatalog[i];
      if(editorSearchKey(std::string(entry.name)+" "+entry.description+" "+std::string(entry.type->id)+" "+
                         std::string(entry.searchTerms)).find(query)!=std::string::npos) native(i);
    }
    scripts(true);
    return rows;
  }
  if(!family) {
    // Sugestões vêm das exigências do schema: um tipo que precisa de algo que o
    // objeto já tem (Olhar com Câmera, Junta com Corpo) é o próximo passo natural.
    bool first=true;
    for(u32 i=0;i<editorComponentCatalog.size();++i) {
      const auto &entry=editorComponentCatalog[i];
      if(entry.schema->requirements.empty() || entry.unavailable(entity)) continue;
      bool satisfied=true;
      for(const auto &rule:entry.schema->requirements) satisfied&=entity.components.find(rule.typeId)!=nullptr;
      if(!satisfied) continue;
      if(first) header("Sugeridos para este objeto");
      first=false;native(i,true);
    }
    first=true;
    for(const auto &id:state.recentComponents) {
      const u32 index=editorComponentIndex(id);
      if(index>=editorComponentCatalog.size()) continue;
      if(first) header("Recentes");
      first=false;native(index,true);
    }
  }
  for(u32 f=0;f<static_cast<u32>(scene::ComponentFamily::Count);++f) {
    if(family && family-1!=f) continue;
    std::string_view group;bool any=false;
    for(u32 i=0;i<editorComponentCatalog.size();++i) {
      const auto &entry=editorComponentCatalog[i];
      if(static_cast<u32>(entry.family)!=f) continue;
      // Em "Todos" o grupo é a família; dentro de uma família, a subfamília.
      const std::string_view title=family?entry.schema->subfamily:
                                   std::string_view(scene::componentFamilyName(entry.family));
      if(!any || (family && title!=group)) header(std::string(title));
      group=title;any=true;native(i);
    }
    if(f==static_cast<u32>(scene::ComponentFamily::Logic)) scripts(false);
  }
  return rows;
}

void buildComponentPreview(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,bool compact);
void buildScriptPreview(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,bool compact);

// Trilho de famílias do Add e de categorias da criação usam a mesma célula:
// ícone numa pastilha, marca lima à esquerda quando escolhida.
void railCell(ScreenBuilder &builder,UiRect cell,UiIcon icon,const char *caption,bool on,u32 widget) {
  const auto &theme=builder.theme;
  cell=deflate(cell,UiInsets{0,2,0,2});
  builder.list.addRect(cell,on?theme.color.accentWash:theme.color.silhouette,theme.radius.control);
  if(on) builder.list.addRect({cell.x,cell.y+8,3,cell.height-16},theme.color.accent,1.5f);
  if(caption && cell.width>=96) {
    auto text=cell;
    builder.list.addImage(centred(takeLeft(text,40),20,20),static_cast<UiImageId>(icon),on?theme.color.accent:theme.color.textDim);
    builder.label(text,caption,on?theme.color.text:theme.color.textDim,theme.type.caption);
  } else builder.list.addImage(centred(cell,22,22),static_cast<UiImageId>(icon),on?theme.color.accent:theme.color.textDim);
  builder.router.addRegion(cell,widget);
}

// A lista rolável do Add: título com contagem, cabeçalhos de grupo e linhas.
void buildComponentList(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,const std::string &query,u32 family) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const auto rows=componentCatalogRows(state,entity,query,family);
  u32 types=0;for(const auto &row:rows) types+=row.kind!=CatalogRow::Header && !row.repeat;
  auto title=takeTop(content,28);
  const std::string scope=!query.empty()?"Resultados":family?
      scene::componentFamilyName(static_cast<scene::ComponentFamily>(family-1)):"Todos os componentes";
  builder.label(takeRight(title,40),std::to_string(types),theme.color.textMuted,theme.type.caption,UiAlign::End);
  builder.label(title,scope,theme.color.text,theme.type.cardName);
  if(rows.empty()) {
    builder.label(takeTop(content,40),"Nenhum componente encontrado",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }
  // Rolagem por linha: o topo é `addScroll`, o que sobra abaixo fica para o arraste.
  const float headerHeight=24,rowHeight=52;
  const u32 first=std::min(state.addScroll,static_cast<u32>(rows.size()-1));
  builder.list.pushClip(content);
  u32 drawn=0;
  auto list=deflate(content,UiInsets{0,0,6,0});
  for(u32 r=first;r<rows.size();++r) {
    const auto &row=rows[r];
    const float height=row.kind==CatalogRow::Header?headerHeight:rowHeight;
    if(list.height<height) break;
    auto cell=takeTop(list,height);++drawn;
    if(row.kind==CatalogRow::Header) {
      builder.label(deflate(cell,UiInsets{2,6,0,0}),row.title,theme.color.textMuted,theme.type.label);
      continue;
    }
    cell=deflate(cell,UiInsets{0,2,0,2});
    const auto hit=cell;
    const EditorComponentEntry *entry=row.kind==CatalogRow::Native?&editorComponentCatalog[row.index]:nullptr;
    const EditorScriptType *script=row.kind==CatalogRow::Script?&state.code->scriptTypes()[row.index]:nullptr;
    const bool chosen=entry?state.componentPreview==row.index+1:state.scriptPreviewType==script->id;
    const char *reason=entry?(!entry->type->allowMultiple&&entity.components.find(*entry->type)?"Já adicionado":entry->unavailable(entity)):
        entity.components.size()>=scene::Components::MaximumCount?"Limite de 64 componentes por objeto":nullptr;
    builder.list.addRect(cell,chosen?theme.color.line:theme.color.raised,theme.radius.control);
    if(chosen) builder.list.addRect({cell.x,cell.y+6,3,cell.height-12},theme.color.accent,1.5f);
    auto tile=takeLeft(cell,48);tile=centred(tile,38,38);
    builder.list.addRect(tile,theme.color.silhouette,theme.radius.control);
    builder.list.addImage(centred(tile,26,26),static_cast<UiImageId>(entry?entry->icon:UiIcon::ScriptingCode),
                          reason?withAlpha(0xffffffff,.45f):0xffffffff);
    auto chevron=takeRight(cell,22);
    builder.list.addImage(centred(chevron,12,12),static_cast<UiImageId>(UiIcon::UiChevronRight),theme.color.textFaint);
    cell=deflate(cell,UiInsets{4,6,0,6});
    builder.label(takeTop(cell,cell.height*.5f),entry?entry->name:script->name,reason?theme.color.textMuted:theme.color.text,theme.type.body);
    // Segunda linha: o motivo do bloqueio vence; depois a dependência que vem
    // junto; depois o termo de outra engine que casou a busca; senão a função.
    std::string detail;UiColor detailColour=theme.color.textMuted;
    if(reason) {detail=reason;detailColour=theme.color.warning;}
    else if(entry) {
      const auto plan=scene::planComponentAddition(entity.components,entry->type->id,false,false);
      if(plan.ready && plan.addedTypes.size()>1) {
        detail="Inclui ";
        for(usize i=0;i+1<plan.addedTypes.size();++i) {
          if(i) detail+=" · ";
          detail+=scene::findComponentSchema(plan.addedTypes[i])->name;
        }
        detailColour=theme.color.accent;
      } else if(!query.empty() &&
                editorSearchKey(std::string(entry->name)+" "+entry->description).find(query)==std::string::npos) {
        std::istringstream aliases{std::string(entry->searchTerms)};std::string alias;
        while(aliases>>alias) if(editorSearchKey(alias).find(query)!=std::string::npos) {detail="Equivale a "+alias;break;}
      }
      if(detail.empty()) detail=entry->description;
    } else detail="Comportamento C# · "+script->file;
    builder.label(cell,detail,detailColour,theme.type.caption);
    builder.router.addRegion(hit,widgetId(entry?EditorWidget::ComponentAddBase:EditorWidget::ScriptAddBase)+row.index);
  }
  builder.list.popClip();
  // Indicador de rolagem: posição e proporção do que está visível.
  if(first>0 || first+drawn<rows.size()) {
    const float track=content.height,thumb=std::max(24.f,track*static_cast<float>(drawn)/static_cast<float>(rows.size()));
    const float offset=(track-thumb)*static_cast<float>(first)/static_cast<float>(std::max<usize>(1,rows.size()-drawn));
    builder.list.addRect({content.right()-3,content.y+std::min(offset,track-thumb),3,thumb},withAlpha(theme.color.textFaint,.6f),1.5f);
  }
  if(builder.layout) {builder.layout->addRowCount=static_cast<u32>(rows.size());builder.layout->addVisibleRows=drawn;}
}

// Folha do Add: sobre o viewport, não espremida no Inspector.
//
// No telefone em paisagem (853×394 dp) o Inspector tem ~230 dp de largura e o
// catálogo dentro dele mostrava três famílias e um item por vez. A folha usa a
// largura da tela: trilho de famílias | lista agrupada | detalhe do tipo
// escolhido com composição, valores iniciais e o botão Adicionar. Em telas
// estreitas o detalhe substitui a lista, com "Voltar".
void buildComponentSheet(ScreenBuilder &builder,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const auto surface=deflate(state.surface,state.safeArea);
  builder.list.addRect(state.surface,withAlpha(theme.color.voidBlack,.55f));
  builder.router.addBlocker(state.surface);
  builder.router.addRegion(state.surface,widgetId(EditorWidget::AddComponentMenu));
  const float width=std::min(1040.f,surface.width-16),height=std::max(0.f,surface.height-kTopBarHeight-12);
  const UiRect sheet{surface.x+(surface.width-width)*.5f,surface.y+kTopBarHeight+6,width,height};
  builder.list.addRect(sheet,theme.color.surface,theme.radius.card);
  builder.list.addBorder(sheet,theme.color.line,1,theme.radius.card);
  builder.router.addBlocker(sheet);
  auto content=deflate(sheet,UiInsets::all(10));
  const auto query=editorSearchKey(state.componentQuery);
  const u32 familyCount=static_cast<u32>(scene::ComponentFamily::Count);
  const u32 family=state.componentCategory%(familyCount+1);

  // Cabeçalho: o que está sendo feito e em qual objeto, a busca e o fechar.
  auto header=takeTop(content,40);takeTop(content,8);
  auto close=takeRight(header,40);
  builder.list.addRect(deflate(close,UiInsets::all(4)),theme.color.raised,theme.radius.control);
  builder.list.addImage(centred(close,14,14),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
  builder.router.addRegion(close,widgetId(EditorWidget::AddComponentMenu),theme.touch.minimumTarget);
  takeRight(header,6);
  // Presets do projeto: aplicar uma configuração salva em vez de um tipo vazio.
  auto presets=takeRight(header,40);
  builder.list.addRect(deflate(presets,UiInsets::all(4)),theme.color.raised,theme.radius.control);
  builder.list.addImage(centred(presets,18,18),static_cast<UiImageId>(UiIcon::AssetsLibrary),theme.color.textDim);
  builder.router.addRegion(presets,widgetId(EditorWidget::PresetOpen),theme.touch.minimumTarget);
  takeRight(header,8);
  auto titleArea=takeLeft(header,std::min(260.f,header.width*.36f));
  builder.list.addImage(centred(takeLeft(titleArea,34),24,24),static_cast<UiImageId>(UiIcon::ComponentAdd),0xffffffff);
  builder.label(takeTop(titleArea,22),"Adicionar componente",theme.color.text,theme.type.cardName);
  builder.label(titleArea,std::string("em ")+entity.name,theme.color.textMuted,theme.type.caption);
  auto search=header;
  builder.list.addRect(search,theme.color.silhouette,theme.radius.control);
  builder.list.addBorder(search,state.editingComponentSearch?theme.color.accent:theme.color.line,1,theme.radius.control);
  auto field=deflate(search,UiInsets::symmetric(10,0));
  builder.list.addImage(centred(takeLeft(field,22),15,15),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
  if(!state.componentQuery.empty()) {
    auto clear=takeRight(field,30);
    builder.list.addImage(centred(clear,12,12),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
    builder.router.addRegion(clear,widgetId(EditorWidget::ComponentSearchClear),theme.touch.minimumTarget);
  }
  builder.label(deflate(field,UiInsets{6,0,0,0}),state.componentQuery.empty()?"Buscar por nome, função ou termo de outra engine":
                state.componentQuery,state.componentQuery.empty()?theme.color.textFaint:theme.color.text,theme.type.body);
  builder.router.addRegion(field,widgetId(EditorWidget::ComponentSearch));

  // Trilho: famílias com pelo menos um tipo. Com rótulo quando a folha é
  // larga; só ícone no telefone. Rola pelo arraste quando não cabe — o plano
  // leva a 18 famílias, que nenhuma altura de telefone mostra de uma vez.
  const bool wide=width>=900;
  // Mesmo no telefone landscape a folha tem largura para nomes. O trilho só
  // de ícones escondia as famílias e obrigava descobrir cada uma por tentativa.
  auto rail=takeLeft(content,wide?150.f:width>=720?128.f:48.f);takeLeft(content,10);
  if(query.empty()) {
    struct RailItem {u32 value;UiIcon icon;const char *caption;};
    std::vector<RailItem> items{{0,UiIcon::AssetsGrid,"Todos"}};
    for(u32 f=0;f<familyCount;++f) {
      bool present=false;
      for(const auto &entry:editorComponentCatalog) present|=static_cast<u32>(entry.family)==f;
      if(f==static_cast<u32>(scene::ComponentFamily::Logic) && state.code) present|=!state.code->scriptTypes().empty();
      const auto value=static_cast<scene::ComponentFamily>(f);
      if(present) items.push_back({f+1,editorIconByName(scene::componentFamilyIcon(value)),scene::componentFamilyName(value)});
    }
    constexpr float kRailCell=44;
    const u32 fits=std::max(1u,static_cast<u32>(rail.height/kRailCell));
    const u32 first=std::min(state.addRailScroll,static_cast<u32>(items.size()>fits?items.size()-fits:0));
    const auto area=rail;
    for(u32 i=first;i<items.size() && rail.height>=kRailCell;++i)
      railCell(builder,takeTop(rail,kRailCell),items[i].icon,items[i].caption,family==items[i].value,
               widgetId(EditorWidget::ComponentFamilyBase)+items[i].value);
    // Setas sobre o trilho quando há famílias acima ou abaixo do visível.
    const auto more=[&](float y,UiIcon icon) {
      const UiRect badge{area.x+area.width*.5f-10,y,20,14};
      builder.list.addRect(badge,theme.color.line,7);
      builder.list.addImage(centred(badge,10,10),static_cast<UiImageId>(icon),theme.color.text);
    };
    if(first>0) more(area.y-2,UiIcon::UiChevronUp);
    if(first+fits<items.size()) more(area.y+fits*kRailCell-10,UiIcon::UiChevronDown);
    if(builder.layout) {builder.layout->addRailCount=static_cast<u32>(items.size());builder.layout->addRailVisible=fits;}
  }

  // Detalhe ao lado da lista quando cabe; senão ele ocupa o lugar da lista.
  const bool previewing=state.componentPreview || !state.scriptPreviewType.empty();
  const bool split=content.width>=560;
  if(split || previewing) {
    UiRect detail=split?takeRight(content,std::max(260.f,content.width*.44f)):content;
    if(split) {takeRight(content,10);builder.list.addRect(detail,theme.color.silhouette,theme.radius.card);detail=deflate(detail,UiInsets::all(10));}
    if(state.componentPreview) buildComponentPreview(builder,detail,entity,!split);
    else if(!state.scriptPreviewType.empty()) buildScriptPreview(builder,detail,entity,!split);
    else {
      auto hint=centred(detail,detail.width,96);
      builder.list.addImage(centred(takeTop(hint,40),28,28),static_cast<UiImageId>(UiIcon::UiInfo),theme.color.textFaint);
      builder.label(takeTop(hint,24),"Escolha um componente",theme.color.textDim,theme.type.body,UiAlign::Center);
      builder.label(hint,"Composição e valores iniciais aparecem aqui",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    }
    if(!split) return;
  }
  buildComponentList(builder,content,entity,query,family);
}

// Folha de criação: a mesma linguagem do Add, para objetos.
//
// Antes era um diálogo de 520×350 com lista de texto paginada de três em três
// e ícones de 16 dp. Aqui: trilho de categorias, grade de cartões com o ícone
// grande (a pessoa escolhe O QUE colocar na cena, e a forma reconhece mais
// rápido que o nome) e detalhe com a composição real da receita, o destino na
// hierarquia e o botão Criar. A grade rola pelo arraste.
struct CreationGrid {u32 columns=1;float cellWidth=0,cellHeight=0;};
CreationGrid creationGrid(float width) {
  CreationGrid grid;
  grid.columns=std::max(1u,static_cast<u32>((width+8)/(118+8)));
  grid.cellWidth=(width-8.0f*static_cast<float>(grid.columns-1))/static_cast<float>(grid.columns);
  grid.cellHeight=92;
  return grid;
}
void buildCreationSheet(ScreenBuilder &builder,EditorScreenLayout &layout) {
  const auto &theme=builder.theme;const auto &state=builder.state;auto &list=builder.list;auto &router=builder.router;
  const auto surface=deflate(state.surface,state.safeArea);
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.55f));router.addBlocker(state.surface);
  router.addRegion(state.surface,widgetId(EditorWidget::CreateMenuClose));
  const float width=std::min(1040.f,surface.width-16),height=std::max(0.f,surface.height-kTopBarHeight-12);
  const UiRect sheet{surface.x+(surface.width-width)*.5f,surface.y+kTopBarHeight+6,width,height};
  list.addRect(sheet,theme.color.surface,theme.radius.card);
  list.addBorder(sheet,theme.color.line,1,theme.radius.card);
  router.addBlocker(sheet);
  auto content=deflate(sheet,UiInsets::all(10));
  const auto *parent=state.document->find(state.selection);
  const bool canCreateChild=parent && parent->id!=state.document->root();
  const bool asChild=state.creationAsChild && canCreateChild;

  // Cabeçalho: ação, destino atual, busca e fechar.
  auto header=takeTop(content,40);takeTop(content,8);
  auto close=takeRight(header,40);
  list.addRect(deflate(close,UiInsets::all(4)),theme.color.raised,theme.radius.control);
  list.addImage(centred(close,14,14),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
  router.addRegion(close,widgetId(EditorWidget::CreateMenuClose),theme.touch.minimumTarget);
  takeRight(header,8);
  auto titleArea=takeLeft(header,std::min(260.f,header.width*.36f));
  list.addImage(centred(takeLeft(titleArea,34),24,24),static_cast<UiImageId>(UiIcon::SceneObjectAdd),0xffffffff);
  builder.label(takeTop(titleArea,22),"Criar objeto",theme.color.text,theme.type.cardName);
  builder.label(titleArea,asChild?std::string("como filho de ")+parent->name:std::string("na raiz da cena"),theme.color.textMuted,theme.type.caption);
  list.addRect(header,theme.color.silhouette,theme.radius.control);
  list.addBorder(header,theme.color.line,1,theme.radius.control);
  auto field=deflate(header,UiInsets::symmetric(10,0));
  list.addImage(centred(takeLeft(field,22),15,15),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textMuted);
  if(state.creationSearch[0]) {
    auto clear=takeRight(field,30);
    list.addImage(centred(clear,12,12),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
    router.addRegion(clear,widgetId(EditorWidget::CreationClearSearch),theme.touch.minimumTarget);
  }
  builder.label(deflate(field,UiInsets{6,0,0,0}),state.creationSearch[0]?state.creationSearch:"Buscar objeto por nome ou termo de outra engine",
                state.creationSearch[0]?theme.color.text:theme.color.textFaint,theme.type.body);
  router.addRegion(field,widgetId(EditorWidget::CreationSearch));

  const auto query=editorSearchKey(state.creationSearch);
  const bool wide=width>=900;
  auto rail=takeLeft(content,wide?150.f:48.f);takeLeft(content,10);
  if(query.empty())
    for(u32 i=0;i<std::size(creationCategories) && rail.height>=44;++i) {
      bool available=false;
      for(u32 j=0;j<editorCreationCatalog.size();++j) available|=creationAvailable(state,j) && editorCreationCatalog[j].category==i;
      if(!available) continue;
      railCell(builder,takeTop(rail,44),editorIconByName(creationCategoryIcons[i]),creationCategories[i],
               state.creationCategory==i,widgetId(EditorWidget::CreationCategoryBase)+i);
    }

  std::vector<u32> entries;
  for(u32 i=0;i<editorCreationCatalog.size();++i)
    if(creationAvailable(state,i) && creationListed(editorCreationCatalog[i],query,state.creationCategory)) entries.push_back(i);
  const bool selectedListed=std::find(entries.begin(),entries.end(),state.creationSelection)!=entries.end();

  // Detalhe à direita quando cabe; no telefone estreito vira uma faixa inferior.
  const bool split=content.width>=560;
  UiRect detail=split?takeRight(content,std::max(250.f,content.width*.36f)):takeBottom(content,selectedListed?96.f:0.f);
  if(split) takeRight(content,10);
  if(selectedListed && detail.height>0) {
    const auto &selected=editorCreationCatalog[state.creationSelection];
    list.addRect(detail,theme.color.silhouette,theme.radius.card);
    auto body=deflate(detail,UiInsets::all(10));
    auto create=takeBottom(body,44);
    list.addRect(create,theme.color.accent,theme.radius.control);
    builder.label(create,"Criar",theme.color.accentInk,theme.type.body,UiAlign::Center);
    router.addRegion(create,creationWidget(state.creationSelection));
    if(split) {
      takeBottom(body,8);
      // Destino na hierarquia como controle segmentado, logo acima do Criar.
      auto destination=takeBottom(body,34);takeBottom(body,8);
      list.addRect(destination,theme.color.canvas,theme.radius.control);
      auto root=takeLeft(destination,destination.width*.42f);
      const auto segment=[&](UiRect rect,const std::string &text,bool on,bool enabled,EditorWidget widget) {
        const auto pill=deflate(rect,UiInsets::all(3));
        if(on) list.addRect(pill,theme.color.raised,theme.radius.control);
        builder.label(deflate(pill,UiInsets::symmetric(6,0)),text,on?theme.color.text:enabled?theme.color.textMuted:theme.color.textFaint,theme.type.caption,UiAlign::Center);
        if(enabled) router.addRegion(rect,widgetId(widget));
      };
      segment(root,"Na raiz",!asChild,true,EditorWidget::CreationAtRoot);
      segment(destination,canCreateChild?std::string("Filho de ")+parent->name:std::string("Sem pai selecionado"),asChild,canCreateChild,EditorWidget::CreationAsChild);
      auto title=takeTop(body,48);
      auto tile=centred(takeLeft(title,52),46,46);
      list.addRect(tile,theme.color.raised,theme.radius.control);
      list.addImage(centred(tile,32,32),static_cast<UiImageId>(selected.icon),0xffffffff);
      takeLeft(title,6);
      builder.label(takeTop(title,26),selected.name,theme.color.text,theme.type.cardName);
      builder.label(title,creationCategories[selected.category],theme.color.textMuted,theme.type.caption);
      builder.label(takeTop(body,34),selected.description,theme.color.textDim,theme.type.caption);
      // A composição é o contrato da receita: exatamente o que entra no objeto,
      // em chips que quebram linha — cabem duas ou três no telefone.
      if(!selected.components.empty()) {
        builder.label(takeTop(body,20),"COMPONENTES",theme.color.textMuted,theme.type.label);
        float x=body.x,y=body.y;
        for(const auto &part:selected.components) {
          const auto *schema=scene::findComponentSchema(part.type);
          if(!schema) continue;
          const float chip=std::min(body.width,list.measure(schema->name,theme.type.caption)+40);
          if(x+chip>body.right()) {x=body.x;y+=32;}
          if(y+28>body.bottom()) break;
          const UiRect rect{x,y,chip,28};
          list.addRect(rect,theme.color.raised,14);
          auto inner=deflate(rect,UiInsets{8,0,10,0});
          list.addImage(centred(takeLeft(inner,20),16,16),static_cast<UiImageId>(editorIconByName(schema->icon)),0xffffffff);
          builder.label(inner,schema->name,theme.color.text,theme.type.caption);
          x+=chip+6;
        }
        if(scene::validPrimitive(selected.childVisual)) {
          const auto row=takeBottom(body,42);
          list.addImage(centred({row.x,row.y,32,row.height},22,22),static_cast<UiImageId>(UiIcon::PrimitiveCylinder),0xffffffff);
          builder.label({row.x+38,row.y,row.width-38,row.height},"Cilindro · sem Body/Collider",theme.color.textDim,theme.type.caption);
        }
      }
    } else {
      auto text=deflate(takeLeft(body,body.width),UiInsets{0,0,0,6});
      builder.label(takeTop(text,20),selected.name,theme.color.text,theme.type.body);
      builder.label(text,selected.description,theme.color.textMuted,theme.type.caption);
    }
  }

  // Grade de cartões com rolagem por linha.
  auto titleRow=takeTop(content,28);
  builder.label(takeRight(titleRow,40),std::to_string(entries.size()),theme.color.textMuted,theme.type.caption,UiAlign::End);
  builder.label(titleRow,query.empty()?creationCategories[std::min<u32>(state.creationCategory,std::size(creationCategories)-1)]:"Resultados",
                theme.color.text,theme.type.cardName);
  if(entries.empty()) {builder.label(takeTop(content,40),"Nenhum objeto encontrado",theme.color.textMuted,theme.type.caption,UiAlign::Center);return;}
  const auto grid=creationGrid(content.width-6);
  const u32 rows=(static_cast<u32>(entries.size())+grid.columns-1)/grid.columns;
  const u32 fits=std::max(1u,static_cast<u32>((content.height+8)/(grid.cellHeight+8)));
  const u32 first=std::min(state.creationScroll,rows>fits?rows-fits:0u);
  list.pushClip(content);
  for(u32 r=first;r<rows && r<first+fits;++r)
    for(u32 c=0;c<grid.columns;++c) {
      const u32 slot=r*grid.columns+c;if(slot>=entries.size()) break;
      const u32 index=entries[slot];const auto &entry=editorCreationCatalog[index];
      const UiRect card{content.x+static_cast<float>(c)*(grid.cellWidth+8),content.y+static_cast<float>(r-first)*(grid.cellHeight+8),grid.cellWidth,grid.cellHeight};
      const bool selected=state.creationSelection==index;
      list.addRect(card,selected?theme.color.line:theme.color.raised,theme.radius.card);
      if(selected) list.addBorder(card,theme.color.accent,1.5f,theme.radius.card);
      auto inner=deflate(card,UiInsets::all(8));
      auto icon=takeTop(inner,48);
      list.addImage(centred(icon,36,36),static_cast<UiImageId>(entry.icon),0xffffffff);
      builder.label(inner,entry.name,theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(card,widgetId(EditorWidget::CreationRowBase)+index);
    }
  list.popClip();
  if(first>0 || first+fits<rows) {
    const float track=content.height,thumb=std::max(24.f,track*static_cast<float>(fits)/static_cast<float>(rows));
    const float offset=(track-thumb)*static_cast<float>(first)/static_cast<float>(std::max(1u,rows-fits));
    list.addRect({content.right()-3,content.y+offset,3,thumb},withAlpha(theme.color.textFaint,.6f),1.5f);
  }
  layout.creationRowCount=rows;layout.creationVisibleRows=fits;
}

// Prévia de um tipo antes de anexar: composição que entra no mesmo Undo e os
// valores iniciais declarados no contrato. Nada é escrito no documento aqui.
void buildComponentPreview(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,bool compact) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  if(!state.componentPreview || state.componentPreview>editorComponentCatalog.size()) return;
  const auto &entry=editorComponentCatalog[state.componentPreview-1];
  const auto plan=scene::planComponentAddition(entity.components,entry.type->id,false,false);

  // Rodapé: ação primária larga; "Voltar" só quando o detalhe cobre a lista.
  auto footer=takeBottom(content,44);takeBottom(content,8);
  if(compact) {
    auto back=takeLeft(footer,96);builder.list.addRect(deflate(back,UiInsets{0,0,8,0}),theme.color.raised,theme.radius.control);
    builder.label(deflate(back,UiInsets{0,0,8,0}),"‹ Voltar",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.router.addRegion(back,widgetId(EditorWidget::ComponentPreviewBack));
  }
  builder.list.addRect(footer,plan.ready?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(footer,plan.ready?"Adicionar ao objeto":"Indisponível",plan.ready?theme.color.accentInk:theme.color.textMuted,theme.type.body,UiAlign::Center);
  if(plan.ready) builder.router.addRegion(footer,widgetId(EditorWidget::ComponentPreviewConfirm));

  // Identidade do tipo: ícone, nome, família e função.
  auto title=takeTop(content,44);
  if(!compact) {
    // Ao lado da lista, o detalhe se fecha pelo próprio cabeçalho.
    auto dismiss=takeRight(title,36);
    builder.list.addImage(centred(dismiss,12,12),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textMuted);
    builder.router.addRegion(dismiss,widgetId(EditorWidget::ComponentPreviewBack),theme.touch.minimumTarget);
  }
  auto tile=centred(takeLeft(title,48),42,42);
  builder.list.addRect(tile,theme.color.raised,theme.radius.control);
  builder.list.addImage(centred(tile,28,28),static_cast<UiImageId>(entry.icon),0xffffffff);
  takeLeft(title,6);
  builder.label(takeTop(title,24),entry.name,theme.color.text,theme.type.cardName);
  builder.label(title,std::string(scene::componentFamilyName(entry.family))+" · "+std::string(entry.schema->subfamily),
                theme.color.textMuted,theme.type.caption);
  builder.label(takeTop(content,28),entry.description,theme.color.textDim,theme.type.caption);
  takeTop(content,4);

  // Controle segmentado: Composição | Valores iniciais.
  auto tabs=takeTop(content,32);takeTop(content,8);
  builder.list.addRect(tabs,theme.color.canvas,theme.radius.control);
  auto composition=takeLeft(tabs,tabs.width*.5f);
  const auto segment=[&](UiRect rect,const char *text,bool on,EditorWidget widget) {
    const auto pill=deflate(rect,UiInsets::all(3));
    if(on) builder.list.addRect(pill,theme.color.raised,theme.radius.control);
    builder.label(pill,text,on?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(rect,widgetId(widget));
  };
  segment(composition,"Composição",!state.componentPreviewValues,EditorWidget::ComponentPreviewComposition);
  segment(tabs,"Valores iniciais",state.componentPreviewValues,EditorWidget::ComponentPreviewValues);

  if(!state.componentPreviewValues) {
    // O que o objeto passa a ter, na ordem do fecho de dependências, e o que
    // ele já tinha e o tipo exige.
    const auto line=[&](const scene::ComponentSchema &schema,const char *status,UiColor statusColour) {
      if(content.height<36) return;
      auto row=deflate(takeTop(content,38),UiInsets{0,2,0,2});
      builder.list.addRect(row,theme.color.raised,theme.radius.control);
      builder.list.addImage(centred(takeLeft(row,36),20,20),static_cast<UiImageId>(editorIconByName(schema.icon)),0xffffffff);
      builder.label(takeRight(row,96),status,statusColour,theme.type.caption,UiAlign::End);
      builder.label(row,schema.name,theme.color.text,theme.type.caption);
    };
    for(const auto &rule:entry.schema->requirements)
      if(entity.components.find(rule.typeId))
        if(const auto *present=scene::findComponentSchema(rule.typeId)) line(*present,"já presente",theme.color.textMuted);
    if(plan.ready) for(const auto id:plan.addedTypes)
      if(const auto *added=scene::findComponentSchema(id)) line(*added,id==entry.type->id?"novo":"novo · exigido",theme.color.accent);
    if(!plan.ready) builder.label(takeTop(content,40),plan.error?plan.error:"Composição indisponível",theme.color.warning,theme.type.caption);
    else builder.label(takeTop(content,28),std::to_string(plan.addedTypes.size())+" componente(s) em um único Undo",theme.color.textMuted,theme.type.caption);
  } else {
    const auto contracts=scene::componentContracts(*entry.schema);
    std::vector<const scene::PropertyContract *> rows;
    for(const auto &row:contracts) if(!row.defaultValue.empty()) rows.push_back(&row);
    if(rows.empty()) builder.label(content,"Sem valores iniciais no contrato",theme.color.textDim,theme.type.caption);
    else {
      // Grade rótulo | valor, 30 dp por linha: cabe o dobro da lista anterior.
      const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height-30)/30));
      const u32 pages=(static_cast<u32>(rows.size())+perPage-1)/perPage;
      const u32 page=std::min(state.componentPreviewPage,pages-1);
      auto pager=pages>1?takeBottom(content,30):UiRect{};
      for(u32 i=page*perPage;i<rows.size()&&i<(page+1)*perPage;++i) {
        const auto &property=*rows[i];auto row=takeTop(content,30);
        if(i%2==0) builder.list.addRect(row,theme.color.canvas,theme.radius.control);
        row=deflate(row,UiInsets::symmetric(8,0));
        auto value=takeRight(row,row.width*.45f);
        builder.label(row,std::string(property.label)+(property.conditional?" · conforme modo":""),theme.color.textDim,theme.type.caption);
        builder.label(value,property.defaultValue+std::string(property.unit.empty()?"":" ")+std::string(property.unit)+(property.perSlot?" /slot":""),
                      theme.color.text,theme.type.caption,UiAlign::End);
      }
      if(pages>1) {
        auto previous=takeLeft(pager,40),next=takeRight(pager,40);
        builder.list.addImage(centred(previous,12,12),static_cast<UiImageId>(UiIcon::UiChevronLeft),page?theme.color.textDim:theme.color.textFaint);
        builder.list.addImage(centred(next,12,12),static_cast<UiImageId>(UiIcon::UiChevronRight),page+1<pages?theme.color.textDim:theme.color.textFaint);
        if(page) builder.router.addRegion(previous,widgetId(EditorWidget::ComponentPreviewPrevious));
        if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::ComponentPreviewNext));
        builder.label(pager,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
      }
    }
  }
}
void buildScriptPreview(ScreenBuilder &builder,UiRect content,const EditorEntity &entity,bool compact) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const EditorScriptType *type=nullptr;
  if(state.code) for(const auto &candidate:state.code->scriptTypes())
    if(candidate.id==state.scriptPreviewType) {type=&candidate;break;}
  scene::ScriptBehavior draft;
  if(type) {draft.scriptType=type->id;draft.source=type->file;}
  const bool published=type && state.code->publishedGeneration()==state.scriptPreviewGeneration;
  const bool ready=published && draft.valid() && entity.components.size()<scene::Components::MaximumCount;
  auto footer=takeBottom(content,44);
  if(compact) {
    auto back=takeLeft(footer,88);builder.label(back,"‹ Voltar",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.router.addRegion(back,widgetId(EditorWidget::ComponentPreviewBack));
  }
  auto add=takeRight(footer,104);
  builder.list.addRect(add,ready?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(add,"Adicionar",ready?theme.color.accentInk:theme.color.textMuted,theme.type.body,UiAlign::Center);
  if(ready) builder.router.addRegion(add,widgetId(EditorWidget::ComponentPreviewConfirm));
  auto title=takeTop(content,40);
  if(!compact) {
    auto dismiss=takeRight(title,36);
    builder.list.addImage(centred(dismiss,12,12),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textMuted);
    builder.router.addRegion(dismiss,widgetId(EditorWidget::ComponentPreviewBack),theme.touch.minimumTarget);
  }
  builder.list.addImage(centred(takeLeft(title,36),28,28),static_cast<UiImageId>(UiIcon::ScriptingCode),0xffffffff);
  builder.label(title,type?type->name:"Tipo indisponível",theme.color.text,theme.type.cardName);
  builder.label(takeTop(content,30),std::string("Em ")+entity.name,theme.color.textMuted,theme.type.caption);
  if(type) {
    builder.label(takeTop(content,30),type->file,theme.color.textDim,theme.type.caption);
    builder.label(takeTop(content,32),std::to_string(type->properties.size())+" campo(s) exposto(s)",theme.color.accent,theme.type.body);
    for(const auto &property:type->properties) {
      if(content.height<58) break;
      auto row=takeTop(content,30);
      builder.list.addRect(row,theme.color.raised,theme.radius.control);
      builder.label(deflate(row,UiInsets::symmetric(8,0)),property.name+" · "+property.valueType,theme.color.text,theme.type.caption);
    }
    if(ready) builder.label(takeTop(content,30),"Valores iniciais definidos pelo código publicado",theme.color.textDim,theme.type.caption);
  }
  if(!ready) builder.label(takeTop(content,40),!published?"Catálogo alterado; volte e escolha novamente":
      !draft.valid()?"Tipo ou fonte inválidos":"Limite de componentes atingido",theme.color.warning,theme.type.caption);
}

// Menu do componente (⋮ ou toque longo no cabeçalho): as ações da Unity em
// Manual/UsingComponents, em grade de duas colunas com ícone. Ação que não
// pode ser feita agora aparece esmaecida, sem região de toque.
void buildComponentMenu(ScreenBuilder &builder,UiRect &content,const EditorEntity &entity,u32 index,bool native) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const auto *component=entity.components.at(index);
  if(!component) return;
  const auto *script=scene::scriptBehavior(component);
  const auto visible=[&](usize i) {
    const auto *other=entity.components.at(i);
    return findEditorComponent(other->type().id)||scene::scriptBehavior(other)||other->unresolved();
  };
  bool above=false,below=false;
  for(usize i=0;i<index;++i) above|=visible(i);
  for(usize i=index+1;i<entity.components.size();++i) below|=visible(i);
  const auto &clip=state.componentClipboard;
  const bool pasteValues=clip && (script?scene::scriptBehavior(clip.get()) && scene::scriptBehavior(clip.get())->scriptType==script->scriptType:
                                          &clip->type()==&component->type());
  const bool pasteNew=clip && scene::planComponentAddition(entity.components,clip->type().id,false,false).ready;
  const auto *schema=scene::findComponentSchema(component->type().id);
  struct Action {UiIcon icon;const char *label;u32 widget;bool enabled;bool danger=false;};
  std::vector<Action> actions{
    {UiIcon::UiChevronUp,"Subir",widgetId(EditorWidget::ComponentMoveUpBase)+index,above},
    {UiIcon::UiChevronDown,"Descer",widgetId(EditorWidget::ComponentMoveDownBase)+index,below},
    {UiIcon::AssetsCopy,"Copiar",widgetId(EditorWidget::ComponentCopyBase)+index,true},
    {UiIcon::UiAdd,"Colar novo",widgetId(EditorWidget::ComponentPasteNewBase)+index,pasteNew},
    {UiIcon::AssetsImport,"Colar valores",widgetId(EditorWidget::ComponentPasteBase)+index,pasteValues},
    {UiIcon::EditorAuthorUndo,"Redefinir",widgetId(EditorWidget::ComponentResetBase)+index,true}};
  if(native) {
    actions.push_back({UiIcon::AssetsLibrary,"Presets",widgetId(EditorWidget::PresetOpen),true});
    actions.push_back({UiIcon::ScriptingNodes,"Impacto",widgetId(EditorWidget::ImpactOpenBase)+index,true});
    actions.push_back({UiIcon::UiHelp,"Referência",widgetId(EditorWidget::ComponentHelpBase)+index,
                       schema && schema->reference.starts_with("https://")});
  } else if(script) actions.push_back({UiIcon::IdeCode,"Abrir código",widgetId(EditorWidget::ScriptSourceBase)+index,true});
  // Unity: ⋮ › Properties abre um Inspector só deste componente.
  actions.push_back({UiIcon::ScenePin,"Propriedades",widgetId(EditorWidget::ComponentPropertiesBase)+index,true});
  actions.push_back({UiIcon::UiRemove,"Remover",widgetId(native?EditorWidget::ComponentRemoveBase:EditorWidget::ScriptRemoveBase)+index,true,true});
  // As linhas encolhem (até 26) para caber tudo, com Remover sempre por último.
  const float gap=4,rows=static_cast<float>((actions.size()+1)/2);
  const float cell=std::clamp(content.height/rows-gap,26.f,32.f);
  const float half=(content.width-gap)*.5f;
  for(usize i=0;i<actions.size();i+=2) {
    if(content.height<cell) break;
    auto line=takeTop(content,cell+gap);line.height=cell;
    for(usize j=i;j<std::min(actions.size(),i+2);++j) {
      const auto &action=actions[j];
      const UiRect rect{line.x+(j-i)*(half+gap),line.y,half,cell};
      builder.list.addRect(rect,action.danger&&action.enabled?withAlpha(theme.color.danger,.14f):theme.color.silhouette,theme.radius.control);
      auto inner=deflate(rect,UiInsets::symmetric(8,0));
      const auto tint=!action.enabled?theme.color.textFaint:action.danger?theme.color.danger:theme.color.textDim;
      builder.list.addImage(centred(takeLeft(inner,22),16,16),static_cast<UiImageId>(action.icon),tint);
      builder.label(deflate(inner,UiInsets{6,0,0,0}),action.label,!action.enabled?theme.color.textFaint:
                    action.danger?theme.color.danger:theme.color.text,theme.type.caption);
      if(action.enabled) builder.router.addRegion(rect,action.widget);
    }
  }
}

// Lista de opções de um campo de enumeração, aberta sobre a tela. A opção
// atual vem marcada; tocar fora fecha sem mudar nada.
void buildEnumPicker(ScreenBuilder &builder) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const u32 index=state.enumPicker&0xffu,field=(state.enumPicker>>8)&0xffffu;
  const auto *entity=state.document->find(state.inspectorTarget?state.inspectorTarget:state.selection);
  const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
  if(!component || field>=component->type().enums.size()) return;
  const auto &property=component->type().enums[field];
  const auto current=property.read(*component);
  builder.list.addRect(state.surface,withAlpha(theme.color.voidBlack,.5f));
  builder.router.addBlocker(state.surface);
  builder.router.addRegion(state.surface,widgetId(EditorWidget::ComponentEnumPickerClose));
  const float rows=static_cast<float>(property.options.size());
  const float height=std::min(state.surface.height-24,52+rows*44);
  const UiRect sheet=centred(state.surface,std::min(360.f,state.surface.width-24),height);
  builder.list.addRect(sheet,theme.color.surface,theme.radius.card);
  builder.list.addBorder(sheet,theme.color.line,1,theme.radius.card);
  builder.router.addBlocker(sheet);
  auto content=deflate(sheet,UiInsets::all(8));
  builder.label(takeTop(content,36),property.name,theme.color.text,theme.type.cardName);
  builder.list.pushClip(content);
  for(u32 i=0;i<property.options.size() && content.height>=40;++i) {
    auto row=deflate(takeTop(content,44),UiInsets{0,2,0,2});
    const bool on=property.options[i].value==current;
    builder.list.addRect(row,on?theme.color.raised:theme.color.silhouette,theme.radius.control);
    auto mark=takeLeft(row,32);
    if(on) builder.list.addImage(centred(mark,14,14),static_cast<UiImageId>(UiIcon::UiCheck),theme.color.accent);
    builder.label(row,property.options[i].name,on?theme.color.text:theme.color.textDim,theme.type.body);
    builder.router.addRegion(row,widgetId(EditorWidget::ComponentEnumOptionBase)+i);
  }
  builder.list.popClip();
}

namespace {
std::vector<std::string> wrapText(const UiDrawList &list,std::string_view text,float width,const UiTypeStyle &style);
}
void buildComponents(ScreenBuilder &builder, UiRect content, const EditorEntity &entity, bool addInHeader=false) {
  if(builder.state.presetInputIndex && builder.state.referenceInstance) {buildReferencePicker(builder,content,entity);return;}
  if(builder.state.presetPanel && builder.state.presetEntity==entity.id) {
    const auto &state=builder.state;const auto &theme=builder.theme;
    const auto button=[&](UiRect rect,const char *label,EditorWidget id,bool enabled=true) {
      const auto inset=deflate(rect,UiInsets::all(3));builder.list.addRect(inset,theme.color.raised,theme.radius.control);
      builder.label(inset,label,enabled?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);if(enabled)builder.router.addRegion(rect,widgetId(id));
    };
    if(state.presetSelectedIsRecipe) {
      const bool wide=content.width>=600;
      auto navigation=takeTop(content,36);
      if(wide) {
        button(takeRight(navigation,130),state.presetDeleteConfirm?"Confirmar exclusão":"Excluir receita",EditorWidget::PresetDelete);
        button(takeRight(navigation,110),"Renomear",EditorWidget::PresetRename);
        navigation=takeLeft(navigation,std::min(260.f,navigation.width));
      }
      button(navigation,"<  Biblioteca de receitas",EditorWidget::PresetLibrary);
      auto heading=takeTop(content,wide?32.f:28.f);
      if(wide) {
        auto destination=takeRight(heading,heading.width*.42f);
        builder.label(destination,fitMiddle(builder.list,std::string("Destino · ")+entity.name,destination.width,theme.type.caption),theme.color.textDim,theme.type.caption,UiAlign::End);
      }
      builder.label(heading,fitMiddle(builder.list,state.presetRecipeName,heading.width,theme.type.body),theme.color.text,theme.type.body);
      if(!wide)builder.label(takeTop(content,24),fitMiddle(builder.list,std::string("Destino · ")+entity.name,content.width,theme.type.caption),theme.color.textDim,theme.type.caption);
      button(takeBottom(content,46),"Aplicar composição e alvos",EditorWidget::PresetApply,state.presetRecipeReady);
      if(!wide) {
        auto actions=takeBottom(content,36);button(takeLeft(actions,actions.width*.5f),"Renomear",EditorWidget::PresetRename);
        button(actions,state.presetDeleteConfirm?"Confirmar exclusão":"Excluir receita",EditorWidget::PresetDelete);
      }
      for(const auto &line:wrapText(builder.list,state.presetRecipeStatus,content.width,theme.type.caption)) {
        if(content.height<(wide?95.f:130.f))break;
        builder.label(takeTop(content,20),line,state.presetRecipeReady?theme.color.accent:theme.color.warning,theme.type.caption);
      }
      const auto section=takeTop(content,38);
      builder.label(section,(state.presetRecipeDetails?"‹ Alvos da receita":"Composição · "+std::to_string(state.presetRecipeCount)+" componentes ›"),theme.color.accent,theme.type.caption);
      builder.router.addRegion(section,widgetId(EditorWidget::PresetRecipeDetails));
      const u32 count=state.presetRecipeDetails?static_cast<u32>(state.presetPreview.size()):static_cast<u32>(state.presetInputNames.size());
      const float rowHeight=state.presetRecipeDetails?34.f:54.f;
      auto pager=takeBottom(content,wide && count*rowHeight<=content.height?0.f:32.f);
      const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/rowHeight));
      const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.presetRecipePage,pages-1);
      if(!count)builder.label(content,"Sem alvos externos · referências próprias vão ao destino",theme.color.textMuted,theme.type.caption);
      for(u32 i=page*perPage;i<count && i<(page+1)*perPage;++i) {
        auto row=takeTop(content,rowHeight);const auto hit=row;
        if(state.presetRecipeDetails)builder.label(row,state.presetPreview[i],theme.color.textDim,theme.type.caption);
        else {
          builder.list.addImage(centred(takeLeft(row,28),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorObject),theme.color.accent);
          auto name=wide?takeLeft(row,row.width*.5f):takeTop(row,23);
          builder.label(name,fitMiddle(builder.list,state.presetInputNames[i],name.width,theme.type.caption),theme.color.textDim,theme.type.caption);
          const auto target=i<state.presetInputValues.size()?state.presetInputValues[i]:0;
          const auto *object=target && target<=std::numeric_limits<EditorEntityId>::max()?state.document->find(static_cast<EditorEntityId>(target)):nullptr;
          builder.label(row,fitMiddle(builder.list,object?object->name:"Escolher objeto ›",row.width,theme.type.body),object?theme.color.text:theme.color.accent,theme.type.body);
          builder.router.addRegion(hit,widgetId(EditorWidget::PresetInputBase)+i);
        }
      }
      if(page)button(takeLeft(pager,36),"<",EditorWidget::PresetRecipePrevious);
      if(page+1<pages)button(takeRight(pager,36),">",EditorWidget::PresetRecipeNext);
      if(!pager.isEmpty())builder.label(pager,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
      return;
    }
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
    const auto close=takeTop(content,36);builder.label(close,state.impactRepair?"<  Cancelar reparo":state.impactAsset.valid()?"<  Recurso":state.impactRemoval?"<  Cancelar remoção":"<  Dependências",theme.color.text,theme.type.body);
    builder.router.addRegion(close,widgetId(EditorWidget::ImpactClose));
    const auto *source=entity.components.findInstance(state.impactInstance);
    const auto *sourceSchema=source?scene::findComponentSchema(source->type()):nullptr;
    const auto *script=scene::scriptBehavior(source);
    const auto *scriptType=script?scriptSchema(state,script->scriptType):nullptr;
    builder.label(takeTop(content,28),script?(scriptType?scriptType->name:script->scriptType):sourceSchema?sourceSchema->name:"Componente indisponível",theme.color.textMuted,theme.type.caption);
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
    const bool stale=state.impactRemoval && state.document->revision()!=state.impactRemovalRevision;
    builder.label(takeTop(content,28),state.impactRepair?(state.impactRepairMaterial.valid()?"Material compartilhado · com Undo":state.impactRepairScene?"Usos locais da cena · com Undo":"Somente usos locais deste componente"):state.impactAsset.valid()?"Registro e usos na cena":source?(stale?"Cena alterada; reabra a remoção":blocked?"Remoção bloqueada pelas dependências":"Sem bloqueio de dependências para remover"):"Componente removido",(blocked||stale)&&!state.impactAsset.valid()?theme.color.warning:theme.color.textMuted,theme.type.caption);
    if(state.impactRemoval && !state.impactAsset.valid()) {
      const auto action=takeTop(content,44);
      const bool ready=source&&!blocked&&!stale;
      builder.list.addRect(deflate(action,UiInsets::all(2)),ready?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(action,"Remover componente · 1 Undo",ready?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(ready) builder.router.addRegion(action,widgetId(EditorWidget::ImpactRemoveConfirm));
    }
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
  const bool same=state.componentSelection==entity.id;
  if(same && state.referenceInstance) {buildReferencePicker(builder,content,entity);return;}
  const bool resourcePicker=state.resourceInstance&&entity.components.findInstance(state.resourceInstance);
  if(same && state.meshPicker && (meshRenderer(entity)||resourcePicker)) {buildMeshPicker(builder,content,entity);return;}
  if(same && state.materialPicker && meshRenderer(entity)) {buildMaterialPicker(builder,content);return;}
  if(same && state.textureViewer && meshRenderer(entity)) {buildTextureViewer(builder,content);return;}
  if(same && state.texturePicker && meshRenderer(entity)) {buildTexturePicker(builder,content);return;}
  const bool detailFocused=same && (!state.expandedComponent.empty() || state.expandedNative || state.expandedScript ||
                                    state.nativeMenu || state.scriptMenu || state.transformMenu);
  // Em landscape curto, o Add permanece na faixa das abas; o rodapé não
  // rouba a única linha editável do componente focado.
  if(!builder.onlyComponent && !addInHeader) {
    auto footer=takeBottom(content,42);auto button=deflate(footer,UiInsets::all(2));
    builder.list.addRect(button,theme.color.raised,theme.radius.control);
    builder.router.addRegion(button,widgetId(EditorWidget::AddComponentMenu));
    builder.list.addImage(centred(takeLeft(button,40),28,28),static_cast<UiImageId>(UiIcon::ComponentAdd),0xffffffff);
    builder.label(button,"Adicionar componente",theme.color.text,theme.type.body);
  }
  struct Card {const EditorComponentEntry *native=nullptr;const scene::ComponentValue *value=nullptr;u32 index=0;bool transform=false;bool object=false;};
  std::vector<Card> cards;
  {
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
  // Multisseleção: só os componentes que todos têm (Unity 6000.0 Multi-object
  // editing); os outros ficam ocultos, com o aviso de quantos tipos.
  if(builder.multiEdit) {
    std::vector<Card> common;
    for(const auto &card:cards) if(!card.value || state.multi.isCommon(card.value->instanceId())) common.push_back(card);
    cards=std::move(common);
    // Durante a edição focada, priorize os campos: o cabeçalho já identifica
    // a multisseleção e a aba Componentes mantém o diagnóstico do conjunto.
    if(state.multi.hidden && !detailFocused) {
      auto note=takeTop(content,30);takeTop(content,4);
      builder.list.addRect(note,withAlpha(theme.color.warning,.12f),theme.radius.control);
      builder.list.addImage(centred(takeLeft(note,26),14,14),static_cast<UiImageId>(UiIcon::UiInfo),theme.color.warning);
      builder.label(note,(std::to_string(state.multi.hidden)+(state.multi.hidden==1?" componente oculto":" componentes ocultos")+
                          ": nem todos têm").c_str(),theme.color.warning,theme.type.caption);
    }
  }
  // Inspector focado num componente: só o cartão dele, sempre aberto.
  if(builder.onlyComponent) {
    std::vector<Card> only;
    for(const auto &card:cards) if(card.value && card.value->instanceId()==builder.onlyComponent) only.push_back(card);
    cards=std::move(only);
    if(cards.empty()) {builder.label(content,"O componente não existe mais neste objeto",theme.color.textMuted,theme.type.caption,UiAlign::Center);return;}
  }
  if(same) {
    const auto focused=std::find_if(cards.begin(),cards.end(),[&](const Card &item) {
      if(item.object) return state.expandedComponent=="astra.object";
      if(item.transform) return state.expandedComponent=="astra.transform" || state.transformMenu;
      if(item.native) return (item.value&&state.expandedNative==item.value->instanceId()) || (item.value&&state.nativeMenu==item.value->instanceId());
      const auto *script=scene::scriptBehavior(item.value);
      return script&&(state.expandedScript==script->instanceId()||state.scriptMenu==script->instanceId());
    });
    if(focused!=cards.end()) {const auto selected=*focused;cards.assign(1,selected);}
  }
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-32)/48));
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
    const bool open=builder.onlyComponent?true:same&&(item.object?state.expandedComponent=="astra.object":item.transform?state.expandedComponent=="astra.transform":item.native?(item.value&&state.expandedNative==item.value->instanceId()):script&&state.expandedScript==script->instanceId());
    const bool menu=same&&!item.object&&(item.transform?state.transformMenu:item.native?(item.value&&state.nativeMenu==item.value->instanceId()):script&&state.scriptMenu==script->instanceId());
    std::string title=item.object?"Objeto":item.transform?"Transformação":item.native?item.native->name:script?(schema?schema->name:script->scriptType):std::string(item.value->type().id);
    if(item.value && item.native && item.native->type->allowMultiple) title+=" · "+std::to_string(item.value->instanceId());
    const auto icon=item.object?UiIcon::EditorAuthorObject:item.transform?UiIcon::EditorAuthorMove:item.native?item.native->icon:UiIcon::ScriptingCode;
    auto row=takeTop(content,44.0f);const auto hit=row;
    builder.list.addRect(row,theme.color.raised,theme.radius.control);
    if(open) builder.list.addRect({row.x,row.y+4,3,row.height-8},theme.color.accent,1);
    // Reordenando: o cartão levantado fica apagado e o destino mostra a barra
    // de inserção do lado em que o cartão vai entrar.
    if(state.componentReorder && item.value && !item.object && !item.transform) {
      if(state.componentReorder==item.index+1) builder.list.addRect(hit,withAlpha(theme.color.voidBlack,.45f),theme.radius.control);
      else if(state.componentReorderTarget==item.index+1) {
        const float y=state.componentReorderTarget<state.componentReorder?hit.y-2:hit.bottom()-1;
        builder.list.addRect({hit.x,y,hit.width,3},theme.color.accent,1);
      }
    }
    builder.label(takeLeft(row,20),open?"v":">",theme.color.textDim,theme.type.body,UiAlign::Center);
    UiRect previousComponent{},nextComponent{};
    const bool componentNavigation=open&&!builder.onlyComponent&&!builder.focusedWindow;
    if(componentNavigation) {
      previousComponent=takeLeft(row,24);nextComponent=takeLeft(row,24);
      builder.list.addImage(centred(previousComponent,12,12),static_cast<UiImageId>(UiIcon::UiChevronLeft),theme.color.textDim);
      builder.list.addImage(centred(nextComponent,12,12),static_cast<UiImageId>(UiIcon::UiChevronRight),theme.color.textDim);
    } else builder.list.addImage(centred(takeLeft(row,36),28,28),static_cast<UiImageId>(icon),0xffffffff);
    auto more=takeRight(row,item.object?0:32);
    const bool characterSearch=open&&item.native&&item.native->type==&scene::Character::descriptor;
    const auto characterSearchHit=characterSearch?takeRight(row,40):UiRect{};
    // Interruptor de ativo no cabeçalho, como a caixa de Behaviour.enabled da
    // Unity: só onde existe `enabled` com consumidor, ou no comportamento C#.
    // O interruptor é registrado DEPOIS da área de dobrar o cartão: o roteador
    // testa da última região para a primeira, e a área de dobrar cobre o
    // cabeçalho inteiro — antes, tocar no interruptor dobrava o cartão.
    bool enableToggle=false,enableOn=false;UiRect enableRect{};
    if(item.value && !item.object && !item.transform) {
      enableToggle=script!=nullptr;enableOn=script?script->enabled:false;
      if(item.native) for(const auto &p:item.native->type->booleans) if(p.id=="enabled") {enableToggle=true;enableOn=p.read(*item.value);}
      if(enableToggle) enableRect=takeRight(row,48);
    }
    builder.label(row,fitMiddle(builder.list,title,row.width,theme.type.body),theme.color.text,theme.type.body);
    if(item.object||item.transform||item.native||script) {
      builder.router.addRegion({hit.x,hit.y,hit.width-(item.object?0:32),hit.height},item.object?widgetId(EditorWidget::ObjectFold):item.transform?widgetId(EditorWidget::TransformFold):widgetId(item.native?EditorWidget::ComponentFoldBase:EditorWidget::ScriptFoldBase)+item.index);
      if(componentNavigation) {
        builder.router.addRegion(previousComponent,widgetId(EditorWidget::InspectorComponentPrevious));
        builder.router.addRegion(nextComponent,widgetId(EditorWidget::InspectorComponentNext));
      }
      if(enableToggle) builder.checkbox(enableRect,enableOn,widgetId(EditorWidget::ComponentEnableBase)+item.index,true,
                                       builder.mixed(multiKey(item.value->instanceId(),"enabled")));
      if(!item.object) {
        builder.list.addImage(centred(more,20,20),static_cast<UiImageId>(UiIcon::EditorAuthorMore),theme.color.textDim);
        builder.router.addRegion(more,item.transform?widgetId(EditorWidget::TransformMenu):widgetId(item.native?EditorWidget::ComponentMenuBase:EditorWidget::ScriptMenuBase)+item.index);
      }
      if(characterSearch){builder.list.addImage(centred(characterSearchHit,16,16),static_cast<UiImageId>(UiIcon::AssetsSearch),theme.color.textDim);builder.router.addRegion(characterSearchHit,widgetId(EditorWidget::ComponentPropertySearch));}
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
        buildComponentMenu(builder,content,entity,item.index,item.native!=nullptr);
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
        } else if(builder.multiEdit && item.value && state.multi.isUnsupported(item.value->instanceId())) {
          // Unity: "Multi-object editing not supported" — editor por objeto.
          auto note=takeTop(fields,std::min(fields.height,56.f));
          builder.list.addRect(note,withAlpha(theme.color.warning,.12f),theme.radius.control);
          builder.list.addImage(centred(takeLeft(note,30),16,16),static_cast<UiImageId>(UiIcon::UiWarning),theme.color.warning);
          builder.label({note.x,note.y+4,note.width,note.height*.5f-4},"Edição múltipla não disponível",theme.color.warning,theme.type.caption);
          builder.label({note.x,note.y+note.height*.5f,note.width,note.height*.5f-4},"Este componente é editado um objeto por vez",theme.color.textDim,theme.type.caption);
        } else if(item.native) buildComponentFields(builder,fields,entity,*item.native,item.index);
        else buildScriptFields(builder,fields,*script,item.index,entity.id);
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
void buildTags(ScreenBuilder &builder,UiRect content,bool assigning) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  const auto &names=state.document->tags().names();
  const auto *object=state.document->find(state.inspectorTarget?state.inspectorTarget:state.selection);
  auto title=takeTop(content,40);
  builder.list.addImage(centred(takeLeft(title,28),22,22),static_cast<UiImageId>(UiIcon::SceneTag),theme.color.accent);
  if(assigning) {
    auto back=takeRight(title,64);builder.label(back,"Voltar",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(back,widgetId(EditorWidget::TagClose));
  }
  builder.label(title,assigning?"Atribuir tag":"Tags do projeto",theme.color.text,theme.type.cardName);
  std::string detail=assigning?(state.multi.count>1?std::to_string(state.multi.count)+" objetos selecionados":object?object->name:""):
                               std::to_string(names.size()-1)+" tags · uma por objeto";
  builder.label(takeTop(content,26),detail.c_str(),theme.color.textDim,theme.type.caption);
  auto tools=takeTop(content,44),create=takeRight(tools,78);
  builder.list.addRect(deflate(tools,UiInsets::all(3)),theme.color.raised,theme.radius.control);
  builder.label(deflate(tools,UiInsets::all(9)),state.tagQuery.empty()?"Buscar tags...":state.tagQuery.c_str(),theme.color.textDim,theme.type.caption);
  builder.router.addRegion(tools,widgetId(EditorWidget::TagSearch));
  const bool authoring=state.workspace!=EditorWorkspace::Play && !state.playInspect;
  builder.label(create,"+ Nova",authoring?theme.color.accent:theme.color.textFaint,theme.type.caption,UiAlign::Center);
  if(authoring) builder.router.addRegion(create,widgetId(EditorWidget::TagNew));
  UiRect removal{};
  if(!assigning) removal=takeBottom(content,44);
  auto footer=takeBottom(content,36);
  std::vector<u32> counts(names.size()),shown;
  std::vector<EditorEntityId> ids;state.document->collectSubtree(state.document->root(),ids);
  for(const auto id:ids) if(id!=state.document->root()) {
    const auto &tag=state.document->find(id)->tag;
    const auto it=std::find(names.begin(),names.end(),tag);if(it!=names.end()) ++counts[it-names.begin()];
  }
  const auto query=editorSearchKey(state.tagQuery);
  for(u32 i=0;i<names.size();++i) if(query.empty() || editorSearchKey(names[i]).find(query)!=std::string::npos) shown.push_back(i);
  const u32 pageSize=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/44));
  const u32 pages=std::max(1u,static_cast<u32>((shown.size()+pageSize-1)/pageSize));
  const u32 page=std::min(state.tagPage,pages-1);
  if(shown.empty()) builder.label(takeTop(content,44),"Nenhuma tag encontrada",theme.color.textMuted,theme.type.caption);
  for(u32 slot=page*pageSize;slot<shown.size() && slot<(page+1)*pageSize && content.height>=32;++slot) {
    const auto index=shown[slot];auto row=takeTop(content,44),text=deflate(row,UiInsets::all(6));
    const bool selected=assigning?object && !builder.mixed("tag") && object->tag==names[index]:state.tagSelected==names[index];
    if(selected) builder.list.addRect({row.x,row.y+7,3,row.height-14},theme.color.accent,1);
    builder.list.addRect({row.x,row.bottom()-1,row.width,1},theme.color.lineSoft);
    auto usage=takeRight(text,82);
    builder.label(text,index==0?"Sem tag":names[index].c_str(),selected?theme.color.accent:theme.color.text,theme.type.body);
    const auto count=std::to_string(counts[index])+" na cena";
    builder.label(usage,count.c_str(),theme.color.textMuted,theme.type.caption,UiAlign::End);
    builder.router.addRegion(row,widgetId(EditorWidget::TagRowBase)+index);
  }
  auto previous=takeLeft(footer,42),next=takeRight(footer,42);
  builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.body,UiAlign::Center);
  builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.body,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::TagPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::TagNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  if(!assigning) {
    const bool removable=authoring && state.tagSelected!="Untagged" && state.document->tags().contains(state.tagSelected);
    builder.label(removal,"Excluir tag sem uso",removable?theme.color.text:theme.color.textFaint,theme.type.caption);
    if(removable) builder.router.addRegion(removal,widgetId(EditorWidget::TagDelete));
  }
}

void buildObjectFields(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  if(state.inspectorReferenceFlags) {
    auto back=takeTop(content,32);
    builder.label(back,"‹  Objeto / Static",theme.color.accent,theme.type.caption);
    builder.router.addRegion(back,widgetId(EditorWidget::InspectorReferenceFlagsClose));
    // Unity 6000.0 StaticEditorFlags. Estas caixas são inventário de referência,
    // não bits autorais: cada consumidor precisa existir antes da habilitação.
    struct Flag {const char *name;const char *reason;};
    const Flag flags[]{
      {"Contribute GI","Indisponível · bake e associação GI"},
      {"Occluder Static","Indisponível · bake de oclusão"},
      {"Batching Static","Indisponível · bake de geometria"},
      {"Navigation Static","Legado Unity · sem bake de navegação"},
      {"Occludee Static","Indisponível · dados de oclusão"},
      {"Off Mesh Link Generation","Legado Unity · sem links de navegação"},
      {"Reflection Probe Static","Indisponível · bake de reflexão"}
    };
    constexpr u32 count=static_cast<u32>(std::size(flags));
    auto pager=takeBottom(content,28);
    const u32 fits=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/48));
    const u32 pages=(count+fits-1)/fits,page=std::min(state.propertyPage,pages-1);
    for(u32 i=page*fits;i<count&&i<(page+1)*fits&&content.height>=44;++i) {
      auto row=takeTop(content,48);
      builder.checkbox(takeLeft(row,28),false,0,false);
      builder.label(takeTop(row,22),flags[i].name,theme.color.textDim,theme.type.caption);
      builder.label(row,fitMiddle(builder.list,flags[i].reason,row.width,theme.type.caption),theme.color.warning,theme.type.caption);
    }
    if(pages>1) {
      const auto previous=takeLeft(pager,32),next=takeRight(pager,32);
      builder.list.addImage(centred(previous,14,14),static_cast<UiImageId>(UiIcon::UiChevronLeft),page?theme.color.text:theme.color.textFaint);
      builder.list.addImage(centred(next,14,14),static_cast<UiImageId>(UiIcon::UiChevronRight),page+1<pages?theme.color.text:theme.color.textFaint);
      if(page) builder.router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
      if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::PropertyNext));
      builder.label(pager,std::to_string(page+1)+" / "+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    }
    return;
  }
  // 0 nome, 1 visível, 2 sombra projetada (só com malha: sem desenho não há
  // sombra), 3 camada, 4 informação.
  std::vector<u32> kinds{0,1};
  if(meshRenderer(entity)) kinds.push_back(2);
  kinds.push_back(3);if(entity.id!=state.document->root()) kinds.push_back(5);kinds.push_back(7);kinds.push_back(6);kinds.push_back(4);
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
      builder.label(line,builder.mixed("name")?"\xE2\x80\x94":entity.name,builder.mixed("name")?theme.color.textDim:theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(line,widgetId(EditorWidget::RenameSelection));
    } else if(row==1 || row==2) {
      auto toggle=takeRight(line,44);
      builder.label(line,row==1?"Visível":"Projetar sombra",theme.color.textDim,theme.type.caption);
      if(builder.mixed(row==1?"visible":"castShadow")) builder.mixedToggle(toggle,widgetId(row==1?EditorWidget::ToggleVisible:EditorWidget::ToggleCastShadow));
      else builder.toggle(toggle,row==1?entity.visible:entity.castShadow,widgetId(row==1?EditorWidget::ToggleVisible:EditorWidget::ToggleCastShadow));
    } else if(row==3) {
      builder.label(takeLeft(line,line.width*.34f),"Camada",theme.color.textDim,theme.type.caption);
      auto previous=takeLeft(line,32),next=takeRight(line,32);
      builder.label(previous,"<",theme.color.text,theme.type.body,UiAlign::Center);
      builder.label(next,">",theme.color.text,theme.type.body,UiAlign::Center);
      builder.router.addRegion(previous,widgetId(EditorWidget::ObjectLayerPrevious));
      builder.router.addRegion(next,widgetId(EditorWidget::ObjectLayerNext));
      const auto layer=entity.layer%runtime::GameplayLayers::kCount;
      const auto name=state.document->layers().name(layer);
      const std::string text=builder.mixed("layer")?std::string("\xE2\x80\x94"):name.empty()?"Camada "+std::to_string(layer):std::string(name);
      builder.label(line,text.c_str(),theme.color.text,theme.type.caption,UiAlign::Center);
    } else if(row==5) {
      builder.label(takeLeft(line,line.width*.34f),"Tag",theme.color.textDim,theme.type.caption);
      const bool known=state.document->tags().contains(entity.tag);
      const std::string text=builder.mixed("tag")?"—":!known?"Ausente: "+entity.tag:entity.tag=="Untagged"?"Sem tag":entity.tag;
      builder.list.addRect(deflate(line,UiInsets::all(2)),theme.color.raised,theme.radius.control);
      builder.label(line,text.c_str(),known?theme.color.text:theme.color.warning,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(line,widgetId(EditorWidget::ObjectTagOpen));
    } else if(row==7) {
      builder.list.addImage(centred(takeLeft(line,26),18,18),static_cast<UiImageId>(UiIcon::SceneGroups),theme.color.accent);
      builder.label(line,("Grupos · "+std::to_string(entity.groups.names().size())+"  ›").c_str(),theme.color.text,theme.type.caption);
      builder.router.addRegion(line,widgetId(EditorWidget::ObjectGroupsOpen));
    } else if(row==6) {
      builder.label(line,"Static · referência Unity  ›",theme.color.textDim,theme.type.caption);
      builder.router.addRegion(line,widgetId(EditorWidget::InspectorReferenceFlags));
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
// Modo Debug do Inspector (Unity 6000.0 Manual/InspectorOptions: ⋮ > Debug).
// Tudo o que o objeto guarda, pelo nome persistente: ids, flags, pose crua,
// cada propriedade de cada componente pelo id do arquivo, o payload
// serializado, e nos comportamentos todos os campos autorados — inclusive os
// ocultos ([HideInInspector]) e os que o código já não declara. Só leitura:
// editar continua no modo Normal, com validação e Desfazer.
void buildInspectorDebug(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  struct Row {std::string key,value;bool heading=false;};
  std::vector<Row> rows;
  const auto number=[](double v){char text[32];std::snprintf(text,sizeof(text),"%.7g",v);return std::string(text);};
  rows.push_back({"Objeto","",true});
  rows.push_back({"id",std::to_string(entity.id)});
  rows.push_back({"pai",std::to_string(entity.parent)});
  rows.push_back({"tipo",std::to_string(static_cast<u32>(entity.kind))});
  rows.push_back({"camada",std::to_string(entity.layer)});
  rows.push_back({"ativo",entity.active?"true":"false"});
  rows.push_back({"visível",entity.visible?"true":"false"});
  rows.push_back({"estático",entity.isStatic?"true":"false"});
  rows.push_back({"projeta sombra",entity.castShadow?"true":"false"});
  rows.push_back({"recebe sombra",entity.receiveShadow?"true":"false"});
  const auto &t=entity.transform;
  rows.push_back({"posição",number(t.position[0])+" "+number(t.position[1])+" "+number(t.position[2])});
  rows.push_back({"rotação (graus)",number(t.rotationDegrees[0])+" "+number(t.rotationDegrees[1])+" "+number(t.rotationDegrees[2])});
  rows.push_back({"escala",number(t.scale[0])+" "+number(t.scale[1])+" "+number(t.scale[2])});
  for(usize c=0;c<entity.components.size();++c) {
    const auto &value=*entity.components.at(c);
    const auto &type=value.type();
    rows.push_back({std::string(type.id)+" v"+std::to_string(type.version),"#"+std::to_string(value.instanceId()),true});
    if(const auto *script=scene::scriptBehavior(&value)) {
      rows.push_back({"script",script->scriptType});
      rows.push_back({"fonte",script->source});
      rows.push_back({"enabled",script->enabled?"true":"false"});
      const auto *schema=scriptSchema(state,script->scriptType);
      for(const auto &p:script->properties) {
        std::string note;
        const EditorScriptProperty *declared=nullptr;
        if(schema) for(const auto &candidate:schema->properties) if(candidate.id==p.id) declared=&candidate;
        if(!declared) note=" · fora do código";
        else if(declared->hidden) note=" · oculto";
        else if(declared->valueType!=p.valueType) note=" · tipo alterado";
        rows.push_back({p.id+" ("+p.valueType+")"+note,p.value});
      }
      continue;
    }
    for(const auto &p:type.numbers) if(p.read) rows.push_back({std::string(p.id),number(p.read(value))});
    for(const auto &p:type.booleans) if(p.read) rows.push_back({std::string(p.id),p.read(value)?"true":"false"});
    for(const auto &p:type.enums) if(p.read) rows.push_back({std::string(p.id),std::to_string(p.read(value))});
    for(const auto &p:type.references) if(p.read) rows.push_back({std::string(p.id),std::to_string(p.read(value))});
    for(const auto &p:type.slotNumbers) if(p.read)
      for(u32 slot=0;slot<p.slotCount(value);++slot) rows.push_back({std::string(p.id)+"["+std::to_string(slot)+"]",number(p.read(value,slot))});
    for(const auto &p:type.resourceBindings) if(p.read)
      for(u32 slot=0;slot<p.slotCount(value);++slot) rows.push_back({std::string(p.id)+"["+std::to_string(slot)+"]",p.read(value,slot).text()});
    std::ostringstream payload;payload.imbue(std::locale::classic());value.write(payload);
    rows.push_back({"payload",payload.str()});
  }
  auto banner=takeTop(content,26);
  builder.list.addRect(banner,withAlpha(theme.color.warning,.16f),theme.radius.control);
  builder.label(deflate(banner,UiInsets{8,0,8,0}),"Modo Debug · só leitura · menu volta ao Normal",theme.color.warning,theme.type.caption);
  takeTop(content,4);
  constexpr float rowHeight=30;
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height-30)/rowHeight));
  const u32 pages=std::max(1u,(static_cast<u32>(rows.size())+perPage-1)/perPage),page=std::min(state.propertyPage,pages-1);
  auto footer=takeBottom(content,30);
  for(u32 i=page*perPage;i<rows.size() && i<(page+1)*perPage;++i) {
    auto row=takeTop(content,rowHeight);
    if(rows[i].heading) {
      builder.list.addRect(deflate(row,UiInsets{0,2,0,2}),theme.color.silhouette,theme.radius.control);
      builder.label(deflate(row,UiInsets{8,0,8,0}),rows[i].key,theme.color.accent,theme.type.caption);
      builder.label(deflate(row,UiInsets{8,0,8,0}),rows[i].value,theme.color.textMuted,theme.type.caption,UiAlign::End);
      continue;
    }
    builder.label(takeLeft(row,row.width*.46f),rows[i].key,theme.color.textDim,theme.type.caption);
    builder.list.pushClip(row);
    builder.label(row,rows[i].value,theme.color.text,theme.type.caption);
    builder.list.popClip();
  }
  const auto back=takeLeft(footer,36),forward=takeRight(footer,36);
  builder.label(back,"<",theme.color.textDim,theme.type.caption,UiAlign::Center);
  builder.label(forward,">",theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page) builder.router.addRegion(back,widgetId(EditorWidget::PropertyPrevious));
  if(page+1<pages) builder.router.addRegion(forward,widgetId(EditorWidget::PropertyNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

namespace {
std::vector<std::string> wrapText(const UiDrawList &list,std::string_view text,float width,const UiTypeStyle &style);
}
void prefabComparisonText(ScreenBuilder &builder,UiRect rect,std::string text,UiColor color,u32 maximumLines=2) {
  if(text.size()>256) {
    usize end=256;while(end && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end;
    text.resize(end);text+="…";
  }
  const auto lines=wrapText(builder.list,text,rect.width,builder.theme.type.caption);
  for(u32 i=0;i<std::min<usize>(maximumLines,lines.size());++i) {
    auto line=lines[i];if(i+1==maximumLines && lines.size()>maximumLines) line+="…";
    builder.label(takeTop(rect,16),line,color,builder.theme.type.caption);
  }
}
void buildPrefabOverrides(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;const auto &view=state.prefabOverrides;
  auto navigation=takeTop(content,36);
  auto back=takeLeft(navigation,76),refresh=takeRight(navigation,76);
  builder.label(back,"< Objeto",theme.color.accent,theme.type.caption);
  builder.router.addRegion(back,widgetId(EditorWidget::PrefabOverridesClose));
  builder.label(refresh,"Ler fonte",theme.color.accent,theme.type.caption,UiAlign::End);
  builder.router.addRegion(refresh,widgetId(EditorWidget::PrefabOverridesRefresh));
  const auto *object=state.document->find(view.object);
  const auto *sourceAsset=state.assetRegistry?state.assetRegistry->find(view.asset):nullptr;
  const std::string title=std::string(object?object->name:"Objeto indisponível")+" → "+
                        (sourceAsset?sourceAsset->path:"Fonte de prefab indisponível");
  builder.label(takeTop(content,24),fitMiddle(builder.list,title,content.width,theme.type.body),theme.color.text,theme.type.body);
  const bool stale=state.document->revision()!=view.revision;
  if(!view.error.empty()) {prefabComparisonText(builder,content,view.error,theme.color.warning,8);return;}
  if(view.hasSourceChanges) {
    const auto hit=navigation;auto receive=navigation;
    builder.list.addImage(centred(takeLeft(receive,26),18,18),static_cast<UiImageId>(UiIcon::EditorPrefabReceive),stale?theme.color.textMuted:theme.color.accent);
    builder.label(receive,"Receber fonte",stale?theme.color.textMuted:theme.color.accent,theme.type.caption,UiAlign::Start);
    if(!stale) builder.router.addRegion(hit,widgetId(EditorWidget::PrefabReceiveSource));
  }
  if(stale) prefabComparisonText(builder,takeTop(content,32),"Comparação antiga · Ler fonte antes de aplicar/reverter",theme.color.warning);
  else if(!state.prefabOperationStatus.empty()) prefabComparisonText(builder,takeTop(content,32),state.prefabOperationStatus,theme.color.warning);
  else prefabComparisonText(builder,takeTop(content,32),"Aplicar: fonte + instâncias herdadas · Reverter: só esta instância",theme.color.textDim);
  if(view.rows.empty()) {prefabComparisonText(builder,content,"Nenhuma diferença de propriedade ou componente",theme.color.textDim);return;}
  if(view.rows.size()>1) {
    auto batch=takeTop(content,44);
    const bool enabled=!stale&&std::all_of(view.rows.begin(),view.rows.end(),[](const auto &row){return row.applicable;});
    const std::string label="Reverter "+std::to_string(view.rows.size())+" diferenças deste objeto";
    builder.label(batch,fitMiddle(builder.list,label,batch.width,theme.type.caption),enabled?theme.color.accent:theme.color.textMuted,theme.type.caption);
    if(enabled) builder.router.addRegion(batch,widgetId(EditorWidget::PrefabOverridesRevertAll));
  }
  auto footer=takeBottom(content,36);
  constexpr float rowHeight=160;
  const u32 perPage=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/rowHeight));
  const u32 count=static_cast<u32>(view.rows.size()),pages=(count+perPage-1)/perPage;
  const u32 page=std::min(state.propertyPage,pages-1);
  for(u32 i=page*perPage;i<std::min(count,(page+1)*perPage);++i) {
    if(content.height<rowHeight) break;
    auto row=takeTop(content,rowHeight);const auto rowBounds=row;const auto &difference=view.rows[i];
    const bool collection=difference.kind==PrefabOverrideKind::Component && difference.current.find("\nPróximo ID livre:")!=std::string::npos;
    auto heading=takeTop(row,collection?20.f:24.f);
    prefabComparisonText(builder,heading,difference.label,theme.color.text);
    const auto origin=difference.origin;
    const std::string status=collection?(origin==PrefabOverrideOrigin::Inherited?"Herdado · componente completo":origin==PrefabOverrideOrigin::Conflict?"Conflito · componente completo":"Local · componente completo"):
        origin==PrefabOverrideOrigin::Inherited?"Herdado · fonte alterada":
        std::string(origin==PrefabOverrideOrigin::Conflict?"Conflito · base: ":"Local · base: ")+difference.baseline;
    prefabComparisonText(builder,takeTop(row,collection?16.f:24.f),status,origin==PrefabOverrideOrigin::Conflict?theme.color.warning:theme.color.accent,1);
    auto actions=takeBottom(row,40);auto explanation=takeBottom(row,collection?18.f:24.f);
    if(origin!=PrefabOverrideOrigin::Inherited) {
      const auto apply=deflate(takeRight(actions,actions.width*.5f),UiInsets{4,2,0,2});
      const bool enabled=!stale&&difference.applyable;
      builder.list.addRect(apply,enabled?withAlpha(origin==PrefabOverrideOrigin::Conflict?theme.color.warning:theme.color.accent,.16f):theme.color.raised,theme.radius.control);
      const auto *component=object?object->components.findInstance(difference.component):nullptr;
      const bool revision=component&&&component->type()==&scene::CollisionRecipe::descriptor;
      builder.label(apply,origin==PrefabOverrideOrigin::Conflict?"Substituir fonte":revision?"Aplicar revisão":"Aplicar fonte",
                    enabled?(origin==PrefabOverrideOrigin::Conflict?theme.color.warning:theme.color.accent):theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(enabled) builder.router.addRegion(apply,widgetId(EditorWidget::PrefabOverrideApplyBase)+i);
    }
    const auto revert=deflate(actions,UiInsets{0,2,4,2});
    builder.list.addRect(revert,theme.color.raised,theme.radius.control);
    builder.label(revert,origin==PrefabOverrideOrigin::Inherited?"Receber nesta instância":origin==PrefabOverrideOrigin::Conflict?"Usar fonte aqui":"Reverter aqui",
                  !stale&&difference.applicable?theme.color.accent:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(!stale&&difference.applicable) builder.router.addRegion(revert,widgetId(EditorWidget::PrefabOverrideRevertBase)+i);
    const std::string reason=!difference.applyable&&!difference.applyReason.empty()?difference.applyReason:
      collection?"Aplicar substitui estrutura e valores do componente inteiro":
      origin==PrefabOverrideOrigin::Conflict?"Substituir publica o valor local sobre a fonte atual":
      origin==PrefabOverrideOrigin::Inherited?"Receber preserva os outros overrides locais":"Aplicar publica somente este endereço";
    prefabComparisonText(builder,explanation,reason,!difference.applyable&&origin!=PrefabOverrideOrigin::Inherited?theme.color.warning:theme.color.textMuted,1);
    if(row.width>=580) {
      const float width=row.width/3;
      const char *names[]{"Base salva","Instância atual","Fonte atual"};
      const std::string *values[]{&difference.baseline,&difference.current,&difference.source};
      for(u32 column=0;column<3;++column) {
        auto cell=takeLeft(row,width);takeRight(cell,12);
        builder.label(takeTop(cell,18),names[column],theme.color.textMuted,theme.type.caption);
        prefabComparisonText(builder,cell,*values[column],column==1?theme.color.text:theme.color.textDim,collection?3u:1u);
      }
    } else {
      prefabComparisonText(builder,takeTop(row,24),"Instância: "+difference.current,theme.color.text,1);
      prefabComparisonText(builder,takeTop(row,24),"Fonte: "+difference.source,theme.color.textDim,1);
    }
    builder.list.addRect({rowBounds.x,rowBounds.y+rowBounds.height-1,rowBounds.width,1},theme.color.raised,0);
  }
  const auto previous=takeLeft(footer,44),next=takeRight(footer,44);
  builder.label(previous,"<",theme.color.textDim,theme.type.body,UiAlign::Center);
  builder.label(next,">",theme.color.textDim,theme.type.body,UiAlign::Center);
  if(page) builder.router.addRegion(previous,widgetId(EditorWidget::PropertyPrevious));
  if(page+1<pages) builder.router.addRegion(next,widgetId(EditorWidget::PropertyNext));
  builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)+" · "+std::to_string(count)+" diferenças").c_str(),theme.color.textDim,theme.type.caption,UiAlign::Center);
}

void buildObjectActions(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;const auto &state=builder.state;
  const bool compactReview=state.motorSetupTarget==entity.id&&state.motorSetupPolicy==3&&state.motorBakeReady&&!state.motorBakeSourcesOpen&&state.surface.height<600;
  if(!compactReview) {
    auto title=takeTop(content,36),close=takeRight(title,36);
    builder.label(title,"Ações do objeto",theme.color.text,theme.type.body);
    builder.label(close,"x",theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.router.addRegion(close,widgetId(EditorWidget::InspectorMenu));
  }
  if(state.motorSetupTarget==entity.id) {
    if(state.motorSetupPolicy==3&&state.motorBakeSourcesOpen) {
      auto footer=takeBottom(content,40);builder.label(footer,"Concluir escolha das fontes",theme.color.accent,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(footer,widgetId(EditorWidget::MotorBakeSourcesDone));
      builder.label(takeTop(content,32),"Fontes para a colisão",theme.color.text,theme.type.body);
      auto actions=takeTop(content,32),own=takeLeft(actions,actions.width/2);
      builder.label(own,"Só este objeto",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.router.addRegion(own,widgetId(EditorWidget::MotorBakeSourcesObject));
      builder.label(actions,"Disponíveis",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.router.addRegion(actions,widgetId(EditorWidget::MotorBakeSourcesHierarchy));
      const auto blocked=std::count_if(state.motorBakeSourceRows.begin(),state.motorBakeSourceRows.end(),[](const auto &row){return !row.error.empty();});
      builder.label(takeTop(content,24),"Fontes: "+std::to_string(state.motorBakeSelectedSources)+" · bloqueadas: "+std::to_string(blocked),theme.color.textDim,theme.type.caption);
      if(!state.motorSetupError.empty())builder.label(takeTop(content,24),fitMiddle(builder.list,state.motorSetupError,content.width,theme.type.caption),theme.color.warning,theme.type.caption);
      const usize pageSize=state.motorBakePartsPerPage(),count=state.motorBakeSourceRows.size();
      if(!count){builder.label(takeTop(content,24),"Sem malhas nesta hierarquia",theme.color.warning,theme.type.caption);return;}
      const usize page=std::min<usize>(state.motorBakeSourcePage,(count-1)/pageSize),begin=page*pageSize;
      auto nav=count>pageSize?takeBottom(content,32):UiRect{};
      for(usize i=begin;i<std::min(begin+pageSize,count)&&content.height>=44;++i) {
        const auto &source=state.motorBakeSourceRows[i];auto row=takeTop(content,44);
        auto mark=takeLeft(row,24);builder.label(mark,source.error.empty()?(source.selected?"+":"-"):"!",source.error.empty()?(source.selected?theme.color.accent:theme.color.textDim):theme.color.warning,theme.type.body,UiAlign::Center);
        builder.label(takeTop(row,22),fitMiddle(builder.list,source.label,row.width,theme.type.caption),source.selected?theme.color.text:theme.color.textDim,theme.type.caption);
        builder.label(row,source.error.empty()?(source.selected?"Incluída · toque para remover":"Fora do bake · toque para incluir"):fitMiddle(builder.list,source.error,row.width,theme.type.caption),source.error.empty()?theme.color.textDim:theme.color.warning,theme.type.caption);
        if(source.error.empty()||source.selected)builder.router.addRegion({mark.x,mark.y,mark.width+row.width,44},widgetId(EditorWidget::MotorBakeSourceBase)+static_cast<u32>(i));
      }
      if(nav.height>0){auto previous=takeLeft(nav,nav.width/2);builder.label(previous,"Anterior",page?theme.color.textDim:theme.color.textMuted,theme.type.caption,UiAlign::Center);builder.label(nav,"Próximas fontes",begin+pageSize<count?theme.color.textDim:theme.color.textMuted,theme.type.caption,UiAlign::Center);
        if(page)builder.router.addRegion(previous,widgetId(EditorWidget::MotorBakeSourcePrevious));
        if(begin+pageSize<count)builder.router.addRegion(nav,widgetId(EditorWidget::MotorBakeSourceNext));}
      return;
    }
    auto footer=takeBottom(content,44),back=takeLeft(footer,80);
    builder.label(back,"Voltar",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(back,widgetId(EditorWidget::MotorSetupBack));
    if(state.motorSetupError.empty()&&(state.motorSetupPolicy!=3||state.motorBakeReady)) {
      builder.list.addRect(footer,theme.color.accent,theme.radius.control);
      builder.label(footer,"Aplicar · 1 Undo",theme.color.accentInk,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(footer,widgetId(EditorWidget::MotorSetupApply));
    }
    const bool compactBake=state.motorSetupPolicy==3&&state.surface.height<600;
    if(compactReview&&!state.motorSetupError.empty())builder.label(takeBottom(content,20),fitMiddle(builder.list,state.motorSetupError,content.width,theme.type.caption),theme.color.warning,theme.type.caption);
    auto heading=takeTop(content,compactBake?26:40);
    if(state.motorSetupPolicy==3&&state.motorBakeReady) {
      auto sources=takeRight(heading,70);builder.label(sources,"Fontes",theme.color.accent,theme.type.caption,UiAlign::Center);builder.router.addRegion(sources,widgetId(EditorWidget::MotorBakeSourcesOpen));
    }
    builder.list.addImage(centred(takeLeft(heading,32),24,24),static_cast<UiImageId>(state.motorSetupPolicy==3?(state.motorBakeRecipeAvailable?UiIcon::PhysicsCollisionRecipe:UiIcon::ComponentConvexParts):UiIcon::ComponentDynamicBodyMotor),0xffffffff);
    builder.label(heading,state.motorSetupPolicy==3?(compactBake?"Colisão":state.motorBakeRecipeAvailable?"Regenerar colisão":"Colisão"):"Locomoção no objeto",theme.color.text,theme.type.body);
    const auto paragraph=[&](std::string_view text,UiColor color) {
      for(const auto &line:wrapText(builder.list,text,content.width,theme.type.caption)) {
        if(content.height<22)break;
        builder.label(takeTop(content,22),line.c_str(),color,theme.type.caption);
      }
      takeTop(content,8);
    };
    if(!compactBake)paragraph(entity.name,theme.color.textDim);
    auto choices=takeTop(content,compactBake?(state.motorBakeReady?0:30):38);const float width=choices.width/4;
    const char *labels[]{"Preservar","Ajustar","Convexo","Decompor"};
    const EditorWidget widgets[]{EditorWidget::MotorSetupPreserve,EditorWidget::MotorSetupFit,EditorWidget::MotorSetupConvex,EditorWidget::MotorSetupDecompose};
    for(u32 i=0;i<4&&choices.height>0;++i) {
      auto choice=takeLeft(choices,width);
      if(i==state.motorSetupPolicy)builder.list.addRect(deflate(choice,UiInsets::all(2)),theme.color.accent,theme.radius.control);
      builder.label(choice,labels[i],i==state.motorSetupPolicy?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(choice,widgetId(widgets[i]));
    }
    if(!compactBake)takeTop(content,8);
    if(state.motorSetupPolicy==3) {
      if(!state.motorBakeReady&&!state.motorBakeRunning) {
        auto sources=takeTop(content,compactBake?28:32);
        builder.label(sources,"Fontes · "+std::to_string(state.motorBakeSelectedSources)+" selecionadas",theme.color.accent,theme.type.caption);
        builder.router.addRegion(sources,widgetId(EditorWidget::MotorBakeSourcesOpen));
      }
      auto budgets=takeTop(content,compactBake?(state.motorBakeReady?0:30):34);const float size=budgets.width/3;
      const char *names[]{"Leve · 8","Equilibrado · 16","Detalhado · 32"};
      const char *compactNames[]{"8 partes","16 partes","32 partes"};
      for(u32 i=0;i<3&&budgets.height>0;++i){auto r=takeLeft(budgets,size);if(i==state.motorBakeBudget)builder.list.addRect(deflate(r,UiInsets::all(2)),theme.color.raised,theme.radius.control);
        builder.label(r,compactBake?compactNames[i]:names[i],i==state.motorBakeBudget?theme.color.text:theme.color.textDim,theme.type.caption,UiAlign::Center);
        builder.router.addRegion(r,widgetId(EditorWidget::MotorBakeBudgetLow)+i);}
      if(!compactBake||(!state.motorBakeReady&&state.motorBakeBudget==3)) {
        std::string custom="Receita personalizada";
        if(const auto *recipe=scene::collisionRecipe(entity.components);state.motorBakeBudget==3&&recipe) {
          const auto &s=recipe->settings;char text[160];std::snprintf(text,sizeof(text),"Receita · %u partes · %u voxels · %u vértices · %.3g%% · %us",s.maximumParts,s.voxelResolution,s.maximumVertices,s.volumeErrorPercent,s.timeBudgetSeconds);custom=text;
        }
        if(compactBake) {
          const auto *recipe=scene::collisionRecipe(entity.components);
          const auto text=recipe?"Personalizado · "+std::to_string(recipe->settings.maximumParts)+" partes · "+std::to_string(recipe->settings.timeBudgetSeconds)+"s":custom;
          builder.label(takeTop(content,20),fitMiddle(builder.list,text,content.width,theme.type.caption),theme.color.textDim,theme.type.caption);
        } else paragraph(state.motorBakeBudget==3?custom:state.motorBakeBudget==0?"50 mil voxels · 32 vértices/casco · 1% volume":state.motorBakeBudget==1?"100 mil voxels · 32 vértices/casco · 1% volume":"400 mil voxels · 64 vértices/casco · 1% volume",theme.color.textDim);
      }
      if(state.motorBakeRunning) {
        auto progress=takeTop(content,4);builder.list.addRect(progress,theme.color.raised,2);progress.width*=state.motorBakeProgress;builder.list.addRect(progress,theme.color.accent,2);
        if(compactBake)builder.label(takeTop(content,20),fitMiddle(builder.list,state.motorBakeStage,content.width,theme.type.caption),theme.color.textDim,theme.type.caption);
        else paragraph(state.motorBakeStage,theme.color.textDim);
      }
      auto generate=takeTop(content,compactReview?32:compactBake?28:36);
      if(compactReview&&state.motorBakeRegenerating) {
        const auto confirm=takeRight(generate,generate.width/2);
        builder.label(confirm,state.motorBakeMappingConfirmed?"Confirmado":"Confirmar",state.motorBakeMappingConfirmed?theme.color.textDim:theme.color.accent,theme.type.caption,UiAlign::Center);
        builder.router.addRegion(confirm,widgetId(EditorWidget::MotorBakeConfirmMapping));
      }
      builder.label(generate,compactReview?"Gerar novamente":state.motorBakeRunning?"Cancelar geração":state.motorBakeReady?"Gerar novamente":state.motorBakeRecipeAvailable?"Regenerar · preservar edições":"Gerar prévia",theme.color.accent,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(generate,widgetId(state.motorBakeRunning?EditorWidget::MotorBakeCancel:state.motorBakeRecipeAvailable?EditorWidget::MotorBakeRegenerate:EditorWidget::MotorBakeStart));
      if(state.motorBakeReady) {
        if(state.motorBakeRegenerating) {
          auto confirm=takeTop(content,compactReview?0:32);
          if(confirm.height>0) {
          builder.label(confirm,state.motorBakeMappingConfirmed?"Correspondências confirmadas":"Confirmar correspondências",state.motorBakeMappingConfirmed?theme.color.textDim:theme.color.accent,theme.type.caption,UiAlign::Center);
          builder.router.addRegion(confirm,widgetId(EditorWidget::MotorBakeConfirmMapping));
          }
          if(!compactBake)paragraph(state.motorBakeMappingSummary,theme.color.textDim);
        }
        if(compactBake)builder.label(takeTop(content,20),std::to_string(state.motorBakePreview.size())+" partes · toque para alternar",theme.color.text,theme.type.caption);
        else paragraph(std::to_string(state.motorBakePreview.size())+" partes · toque para ativar/desativar",theme.color.text);
        const usize pageSize=state.motorBakePartsPerPage();
        const usize page=std::min<usize>(state.motorBakePage,(state.motorBakeEnabled.size()-1)/pageSize);
        const usize begin=page*pageSize;
        auto navigation=state.motorBakeEnabled.size()>pageSize?takeBottom(content,32):UiRect{};
        for(usize i=begin;i<std::min(begin+pageSize,state.motorBakeEnabled.size())&&content.height>=(state.motorBakeRegenerating?64:32);++i) {
          const bool removed=i<state.motorBakePartNotes.size()&&state.motorBakePartNotes[i].starts_with("Remoção local");
          auto row=takeTop(content,32);const auto label=std::string(removed?"Removida localmente":state.motorBakeEnabled[i]?"Ativa":"Desativada")+" · parte "+std::to_string(i+1);
          builder.label(row,label.c_str(),state.motorBakeEnabled[i]?theme.color.accent:theme.color.textMuted,theme.type.caption);
          if(!removed)builder.router.addRegion(row,widgetId(EditorWidget::MotorBakePartBase)+static_cast<u32>(i));
          if(state.motorBakeRegenerating&&i<state.motorBakePartMapping.size()) {
            auto mapping=takeTop(content,32);const auto previous=state.motorBakePartMapping[i];
            const auto text=previous?"Vínculo: Colisor #"+std::to_string(previous):"Vínculo: nova parte";
            builder.label(mapping,text,theme.color.text,theme.type.caption);
            builder.router.addRegion(mapping,widgetId(EditorWidget::MotorBakeMappingBase)+static_cast<u32>(i));
            if(!compactBake&&i<state.motorBakePartNotes.size()&&!state.motorBakePartNotes[i].empty())paragraph(state.motorBakePartNotes[i],theme.color.textDim);
          }
        }
        if(navigation.height>0){auto previous=takeLeft(navigation,navigation.width/2);
          builder.label(previous,"Anterior",theme.color.textDim,theme.type.caption,UiAlign::Center);builder.label(navigation,"Próximas partes",theme.color.textDim,theme.type.caption,UiAlign::Center);
          builder.router.addRegion(previous,widgetId(EditorWidget::MotorBakePrevious));builder.router.addRegion(navigation,widgetId(EditorWidget::MotorBakeNext));}
      }
    }
    if(!compactReview&&(!compactBake||!state.motorSetupError.empty()))paragraph(state.motorSetupError.empty()?state.motorSetupSummary:state.motorSetupError,state.motorSetupError.empty()?theme.color.text:theme.color.warning);
    return;
  }
  if(state.characterConversionTarget==entity.id) {
    auto footer=takeBottom(content,44);
    const auto back=takeLeft(footer,80);
    builder.label(back,"Voltar",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.router.addRegion(back,widgetId(EditorWidget::CharacterConversionBack));
    if(state.characterConversionError.empty()) {
      builder.list.addRect(footer,theme.color.accent,theme.radius.control);
      builder.label(footer,"Converter · 1 Undo",theme.color.accentInk,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(footer,widgetId(EditorWidget::CharacterConversionApply));
    }
    const auto paragraph=[&](std::string_view text,UiColor color) {
      for(const auto &line:wrapText(builder.list,text,content.width,theme.type.caption)) {
        if(content.height<22) break;
        builder.label(takeTop(content,22),line.c_str(),color,theme.type.caption);
      }
      takeTop(content,8);
    };
    builder.list.addImage(centred(takeTop(content,42),32,32),static_cast<UiImageId>(UiIcon::ComponentCharacter),0xffffffff);
    paragraph(std::string("Character > ")+entity.name,theme.color.text);
    paragraph("A raiz controla a cápsula. A malha selecionada permanece como filho visual, com material, identidade e filhos preservados.",theme.color.textDim);
    if(!state.characterConversionError.empty()) paragraph(state.characterConversionError,theme.color.warning);
    else {
      paragraph("Body/Collider serão retirados do visual. Undo restaura seus valores, a hierarquia e a ordem anteriores.",theme.color.textDim);
      paragraph("Cápsula sugerida: raio "+decimalText(state.characterConversionRadius,2)+" m; altura "+decimalText(state.characterConversionHeight,2)+" m. Ajustável depois da conversão.",theme.color.accent);
      paragraph("A cápsula é uma aproximação da geometria; não conserva a forma de colisão do Body. Scripts que buscam Body por código precisam ser adaptados.",theme.color.textMuted);
    }
    return;
  }
  const bool root=entity.id==state.document->root();
  const struct {const char *label;EditorWidget widget;bool enabled;} actions[]{
      {scene::collisionRecipe(entity.components)?"Regenerar colisão…":"Configurar locomoção…",EditorWidget::MotorSetupOpen,!root&&!builder.multiEdit&&state.workspace!=EditorWorkspace::Play},
      {"Criar raiz Character…",EditorWidget::CharacterConversionOpen,!root&&runtime::meshRenderer(entity)&&!builder.multiEdit},
      {"Renomear",EditorWidget::RenameSelection,true},
      {"Duplicar",EditorWidget::DuplicateSelection,!root},
      {"Excluir",EditorWidget::DeleteSelection,!root},
      {"Enquadrar na vista",EditorWidget::FrameSelection,true},
      {"Criar filho vazio",EditorWidget::CreateChildGroup,true},
      {"Criar prefab da seleção",EditorWidget::PrefabCreate,!root && !scene::prefabLink(entity.components) && state.workspace!=EditorWorkspace::Play},
      {"Desvincular instância de prefab",EditorWidget::PrefabUnpack,scene::prefabLink(entity.components) && state.workspace!=EditorWorkspace::Play},
      {"Comparar com a fonte do prefab",EditorWidget::PrefabOverrides,scene::prefabLink(entity.components) && state.workspace!=EditorWorkspace::Play},
      {"Mover acima",EditorWidget::MoveEarlier,!root},
      {"Mover abaixo",EditorWidget::MoveLater,!root},
      {"Mudar pai",EditorWidget::ReparentSelection,!root},
      {"Mover para a raiz",EditorWidget::MoveToRoot,!root && entity.parent!=state.document->root()},
      {"Copiar transformação",EditorWidget::TransformCopy,!root},
      {"Colar transformação",EditorWidget::TransformPaste,!root && state.hasTransformClipboard},
      {"Redefinir transformação",EditorWidget::TransformReset,!root},
      {"Propriedades (janela)",EditorWidget::InspectorOpenFocused,!root},
      {state.inspectorDebug?"Modo Normal":"Modo Debug",EditorWidget::InspectorDebugToggle,true},
      {"Ping na Hierarquia",EditorWidget::InspectorPing,!root}};
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
  builder.label({header.x,header.y,header.width,half},state.importBatch?"Aplicar receitas HDRI":state.importTexture?"Importar textura":"Importar recurso",theme.color.text,theme.type.cardName);
  builder.label({header.x,header.y+half,header.width,half},state.importPath.empty()?"Escolhendo arquivo…":state.importPath,
                theme.color.textDim,theme.type.caption); // caption: o caminho mantém a caixa do nome do arquivo
  builder.label(takeTop(content,24),state.importStatus,theme.color.accent,theme.type.caption);

  if(state.importBatch) {
    const auto cancel=deflate(takeBottom(content,40),UiInsets::all(2));
    list.addRect(cancel,theme.color.raised,theme.radius.control);
    builder.label(cancel,"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(cancel,widgetId(EditorWidget::ImportCancel));
    for(const auto &line:wrapText(list,"Preparando todos os mapas. As receitas serão publicadas juntas quando o lote estiver pronto.",content.width,theme.type.body))
      builder.label(takeTop(content,24),line,theme.color.textDim,theme.type.body);
    return;
  }
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
                            state.importTextureCompression==state.importPreparedTextureCompression &&
                            state.importGenerateLods==state.importPreparedGenerateLods &&
                            state.importLodLevels==state.importPreparedLodLevels &&
                            state.importOptimizeOrder==state.importPreparedOptimizeOrder;

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
                             Tangents,Lods,LodLevels,PolygonOrder,Cameras,Lights,ProfileRowCount};
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
      // Mesh LOD da Unity 6.2 / LOD de importação da Godot: níveis na própria
      // malha, escolhidos pelo erro em pixels do painel Qualidade.
      case Lods:cycle(row,"Níveis de detalhe (LOD)",state.importGenerateLods?"Gerar":"Não gerar",EditorWidget::ImportGenerateLodsToggle);break;
      case LodLevels:cycle(row,"Máximo de níveis",state.importGenerateLods?std::to_string(state.importLodLevels).c_str():"—",
                           EditorWidget::ImportLodLevelsCycle);break;
      case PolygonOrder:cycle(row,"Otimizar ordem dos polígonos",state.importOptimizeOrder?"Sim":"Não",
                              EditorWidget::ImportOptimizeOrderToggle);break;
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
  buildTextureViewer(builder,content);
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
  builder.label(title,(state.textureManagerSources?std::string("Texturas das fontes"):
                       state.textureFolder.empty()?std::string("Texturas do projeto"):state.textureFolder)+" · "+
                std::to_string(state.textureManagerRows.size()),theme.color.text,theme.type.caption);
  // Bloco F: as imagens dos modelos importados ficam ao lado das do projeto.
  {
    auto scope=takeTop(content,32);
    const auto project=deflate(takeLeft(scope,scope.width*.5f),UiInsets::all(2)),sources=deflate(scope,UiInsets::all(2));
    for(const auto &[box,label,on,widget]:{std::tuple{project,"Do projeto",!state.textureManagerSources,EditorWidget::TextureManagerShowProject},
                                           std::tuple{sources,"Das fontes",state.textureManagerSources,EditorWidget::TextureManagerShowSources}}) {
      builder.list.addRect(box,on?theme.color.accent:theme.color.raised,theme.radius.control);
      builder.label(box,label,on?theme.color.accentInk:theme.color.text,theme.type.caption,UiAlign::Center);
      builder.router.addRegion(box,widgetId(widget));
    }
  }
  auto search=deflate(takeTop(content,36),UiInsets::all(2));
  builder.list.addRect(search,theme.color.raised,theme.radius.control);
  builder.label(deflate(search,UiInsets::symmetric(8,0)),
                state.textureQuery.empty()?std::string("Buscar por nome ou pasta"):"Busca: "+state.textureQuery,
                state.textureQuery.empty()?theme.color.textMuted:theme.color.text,theme.type.caption);
  builder.router.addRegion(search,widgetId(EditorWidget::TextureSearch));
  static constexpr const char *filters[]{"Todas","Não usadas","Ausentes","Alteradas","Sem alfa","Acima do teto"};
  // Os filtros falam de arquivos do projeto; nas fontes a lista é a do modelo.
  for(u32 line=0;line<(state.textureManagerSources?0u:2u);++line) {
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
    builder.label(takeTop(content,40),state.textureManagerSources
                  ?"Nenhum modelo importado com texturas. Importe um GLB ou uma pasta em Arquivos."
                  :"Nenhuma textura neste filtro",theme.color.textMuted,theme.type.caption);
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
    const auto &thumbs=state.textureManagerSources?state.sourceTextureThumbs:state.projectTextureThumbs;
    if(state.textureManagerSources && builder.visibleSourceTextureRows) builder.visibleSourceTextureRows->push_back(index);
    if(index<thumbs.size() && !thumbs[index].isEmpty()) {
      const auto &texels=thumbs[index];
      const float fit=std::min(thumb.width/texels.width,thumb.height/texels.height);
      builder.list.addPreviewImage({thumb.x+(thumb.width-texels.width*fit)*.5f,thumb.y+(thumb.height-texels.height*fit)*.5f,
                                    texels.width*fit,texels.height*fit},texels);
    } else {
      builder.list.addRect(thumb,theme.color.lineSoft,4);
    }
    const UiRect name{inner.x+4,inner.bottom()-caption,inner.width-8,caption-2};
    builder.list.pushClip(name);
    const auto &names=state.textureManagerSources?state.sourceTextureNames:state.projectTextureNames;
    builder.label(name,index<names.size()?names[index]:std::string(),theme.color.text,theme.type.caption);
    builder.list.popClip();
    builder.router.addRegion(inner,widgetId(EditorWidget::TextureManagerRowBase)+
                                   (state.textureManagerSources?detail::TextureManagerSourceRowOffset:0u)+index);
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
// Vários recursos escolhidos em Arquivos (Unity 6000.0: Inspector com vários
// assets): o que é comum, "—" no que difere, estreitar por tipo e a lista.
void buildMultiAssetInspector(ScreenBuilder &builder,UiRect content) {
  const auto &theme=builder.theme;const auto &state=builder.state;const auto &view=state.multiAsset;
  using Kind=EditorScreenState::MultiAssetView::Kind;
  const bool textures=view.kind==Kind::Textures;
  // Cabeçalho: pilha do tipo com a contagem, título, o ativo e fechar.
  {
    auto header=takeTop(content,44);
    const auto close=centred(takeRight(header,28),26,26);
    const u32 closeWidget=widgetId(EditorWidget::FilesSelectNone);
    builder.list.addRect(close,builder.isPressed(closeWidget)?theme.color.line:theme.color.raised,theme.radius.control);
    builder.list.addImage(centred(close,12,12),static_cast<UiImageId>(UiIcon::UiClose),theme.color.textDim);
    builder.router.addRegion(close,closeWidget,theme.touch.minimumTarget*.75f);
    auto badge=takeLeft(header,40);
    const UiRect back{badge.x+8,badge.y+6,26,26},front{badge.x+3,badge.y+11,26,26};
    const u32 icon=view.kind==Kind::Mixed?static_cast<u32>(UiIcon::AssetsGrid):view.groups.empty()?0u:view.groups.front().icon;
    builder.list.addRect(back,theme.color.raised,theme.radius.control);
    builder.list.addBorder(back,theme.color.lineSoft,1.0f,theme.radius.control);
    builder.list.addRect(front,theme.color.accentWash,theme.radius.control);
    builder.list.addBorder(front,withAlpha(theme.color.accent,.6f),1.0f,theme.radius.control);
    if(icon) builder.list.addImage(centred(front,15,15),icon,theme.color.accent);
    takeLeft(header,6);
    auto top=takeTop(header,24);
    builder.label(top,view.title,theme.color.text,theme.type.cardName);
    std::string active;
    for(const auto &item:view.items) if(item.active) active=item.name;
    builder.label(takeTop(header,16),fitMiddle(builder.list,"Ativo: "+active,header.width,theme.type.caption),theme.color.textMuted,theme.type.caption);
  }
  // Rodapé com Reverter e Aplicar quando há o que aplicar em conjunto.
  if(textures) {
    auto footer=takeBottom(content,40);
    builder.list.addRect({footer.x,footer.y,footer.width,1},theme.color.lineSoft);
    takeTop(footer,6);
    auto row=takeTop(footer,30);
    const auto revert=takeLeft(row,row.width*.36f-2);takeLeft(row,4);
    const bool pending=view.pending>0;
    textureAction(builder,revert,UiIcon::EditorAuthorUndo,"Reverter",widgetId(EditorWidget::MultiAssetRevert),false,pending);
    textureAction(builder,row,UiIcon::UiCheck,pending?"Aplicar a "+std::to_string(view.pending)+(view.pending==1?" textura":" texturas"):std::string("Aplicar"),
                  widgetId(EditorWidget::MultiAssetApply),true,pending);
  }
  takeTop(content,2);
  const UiRect window=content;
  builder.router.addRegion(window,widgetId(EditorWidget::MultiAssetScroll));
  builder.list.pushClip(window);
  const TextureCards cards{builder,window};
  UiRect column{window.x,window.y-state.multiAssetScroll,window.width-5,1.0e6f};
  const float start=column.y;
  // Cartão simples, sem recolher: fundo, ícone, título e nota à direita.
  const auto card=[&](UiIcon icon,const std::string &title,std::string_view note,float bodyHeight,UiColor noteColor) {
    auto frame=takeTop(column,kTexCardHeader+bodyHeight+kTexCardPad);
    takeTop(column,kTexCardGap);
    builder.list.addRect(frame,theme.color.surface,theme.radius.card);
    builder.list.addBorder(frame,theme.color.lineSoft,1.0f,theme.radius.card);
    auto header=deflate(takeTop(frame,kTexCardHeader),UiInsets::symmetric(kTexCardPad,0));
    builder.list.addImage(centred(takeLeft(header,16),14,14),static_cast<UiImageId>(icon),theme.color.accent);
    takeLeft(header,6);
    if(!note.empty()) builder.label(takeRight(header,std::min(header.width*.5f,measureTextWidth(note,builder.list.fontMetrics(),theme.type.caption)+6)),
                                    note,noteColor,theme.type.caption,UiAlign::End);
    builder.label(header,title,theme.color.text,kTexCardTitle);
    return deflate(frame,UiInsets{kTexCardPad,0,kTexCardPad,kTexCardPad});
  };
  // Tipos: com mais de um, cada linha estreita a seleção àquele tipo. Com um
  // tipo só, o cabeçalho já diz qual; sem edição comum, fica o aviso.
  // Aviso quebrado em palavras na largura do cartão, sem cortar a frase.
  const auto noteText=wrapText(builder.list,view.note,column.width-2*kTexCardPad-12-19,theme.type.caption);
  const auto noteLines=[&](UiRect box,UiColor ink) {
    builder.list.addRect(box,withAlpha(ink,.10f),theme.radius.control);
    auto text=deflate(box,UiInsets{6,5,6,5});
    builder.list.addImage(centred(takeLeft(text,14),12,12),static_cast<UiImageId>(UiIcon::UiInfo),ink);
    takeLeft(text,5);
    for(const auto &line:noteText) builder.label(takeTop(text,15),line,ink,theme.type.caption);
  };
  const float noteHeight=view.note.empty()?0.0f:10.0f+15.0f*static_cast<float>(noteText.size());
  if(view.kind==Kind::Other) noteLines(card(UiIcon::AssetsFile,"Edição múltipla","",noteHeight,theme.color.textMuted),theme.color.warning);
  if(view.kind==Kind::Mixed) {
    const bool mixed=true;
    const float row=28,gap=4;
    auto body=card(UiIcon::AssetsGrid,"Tipos","",noteHeight+4+view.groups.size()*row+(view.groups.size()-1)*gap,theme.color.textMuted);
    if(!view.note.empty()) {noteLines(takeTop(body,noteHeight),theme.color.accent);takeTop(body,4);}
    for(u32 i=0;i<view.groups.size();++i) {
      const auto &group=view.groups[i];
      auto line=takeTop(body,row);takeTop(body,gap);
      const u32 widget=widgetId(EditorWidget::MultiAssetNarrowBase)+i;
      builder.list.addRect(line,mixed&&builder.isPressed(widget)?theme.color.line:theme.color.canvas,theme.radius.control);
      auto inner=deflate(line,UiInsets::symmetric(8,0));
      builder.list.addImage(centred(takeLeft(inner,16),14,14),group.icon,theme.color.textDim);
      takeLeft(inner,6);
      if(mixed) builder.list.addImage(centred(takeRight(inner,12),11,11),static_cast<UiImageId>(UiIcon::UiChevronRight),theme.color.textDim);
      const std::string count=std::to_string(group.count);
      auto pill=takeRight(inner,measureTextWidth(count,builder.list.fontMetrics(),theme.type.label)+14);
      pill=centred(pill,pill.width,16);
      builder.list.addRect(pill,theme.color.accentWash,8);
      builder.label(pill,count,theme.color.accent,theme.type.label,UiAlign::Center);
      builder.label(inner,group.label,theme.color.text,theme.type.caption);
      if(mixed) cards.region(line,widget);
    }
  }
  // Importação comum às texturas: igual mostra o valor, diferente mostra "—".
  if(textures) {
    constexpr u32 fields=10;
    const float cell=textureFieldHeight(true),gap=6;
    const u32 rows=(fields+1)/2;
    const std::string note=view.pending?std::to_string(view.pending)+" não aplicada"+(view.pending==1?"":"s"):std::string();
    const auto hints=view.mixed?wrapText(builder.list,"Toque longo num \xE2\x80\x94 copia o valor de uma delas",
                                         column.width-2*kTexCardPad,theme.type.caption):std::vector<std::string>{};
    auto body=card(UiIcon::UiSettings,"Importação",note,rows*cell+(rows-1)*gap+(hints.empty()?0.0f:6+14.0f*hints.size()),theme.color.accent);
    const float half=(body.width-8)*.5f;
    for(u32 i=0;i<fields;++i) {
      const std::string &text=view.fields[i];
      const auto split=text.find(": ");
      const UiRect at{body.x+(i%2)*(half+8),body.y+static_cast<float>(i/2)*(cell+gap),half,cell};
      const bool mixed=(view.mixed>>i)&1u;
      const auto field=textureField(cards,at,split==std::string::npos?std::string_view("Perfil"):std::string_view(text).substr(0,split),
                                    split==std::string::npos?text:text.substr(split+2),true,
                                    widgetId(EditorWidget::MultiAssetFieldBase)+i,mixed?UiIcon::EditorAuthorMore:UiIcon::UiChevronRight);
      // O "—" em destaque: é o que um toque longo resolve (copiar de uma delas).
      if(mixed) builder.list.addRect({field.x,field.bottom()-2,field.width,2},withAlpha(theme.color.accent,.7f),1.0f);
    }
    UiRect hint{body.x,body.y+rows*cell+(rows-1)*gap+6,body.width,14};
    for(const auto &line:hints) {builder.label(hint,line,theme.color.textMuted,theme.type.caption);hint.y+=14;}
  }
  // A seleção: tocar abre só aquele; o x tira da seleção.
  {
    const float row=34,gap=3;
    auto body=card(UiIcon::EditorSelectBox,"Seleção ("+std::to_string(view.items.size())+")","",
                   view.items.size()*row+(view.items.size()-1)*gap,theme.color.textMuted);
    for(u32 i=0;i<view.items.size();++i) {
      const auto &item=view.items[i];
      auto line=takeTop(body,row);takeTop(body,gap);
      const u32 widget=widgetId(EditorWidget::MultiAssetItemBase)+i,remove=widgetId(EditorWidget::MultiAssetRemoveBase)+i;
      builder.list.addRect(line,builder.isPressed(widget)?theme.color.line:item.active?withAlpha(theme.color.accent,.14f):theme.color.canvas,theme.radius.control);
      if(item.active) builder.list.addRect({line.x,line.y,3,line.height},theme.color.accent);
      auto inner=deflate(line,UiInsets{8,0,2,0});
      builder.list.addImage(centred(takeLeft(inner,18),15,15),item.icon,item.active?theme.color.accent:theme.color.textDim);
      takeLeft(inner,6);
      const auto x=takeRight(inner,28);
      builder.list.addImage(centred(x,11,11),static_cast<UiImageId>(UiIcon::UiClose),builder.isPressed(remove)?theme.color.accent:theme.color.textMuted);
      auto name=takeTop(inner,inner.height*.55f);
      builder.label(name,fitMiddle(builder.list,item.name,name.width,theme.type.caption),theme.color.text,theme.type.caption);
      builder.label(inner,fitMiddle(builder.list,item.detail,inner.width,theme.type.caption),theme.color.textFaint,theme.type.caption);
      cards.region(line,widget);
      cards.region(x,remove);
    }
  }
  const float height=column.y-start;
  builder.list.popClip();
  if(builder.layout) {builder.layout->multiAssetContent=height;builder.layout->multiAssetWindow=window.height;}
  if(height>window.height+1) {
    const float thumb=std::max(24.0f,window.height*window.height/height);
    const float travel=window.height-thumb;
    const float offset=std::clamp(state.multiAssetScroll/(height-window.height),0.0f,1.0f)*travel;
    builder.list.addRect({window.right()-3,window.y+offset,3,thumb},theme.color.line,1.5f);
  }
}
} // namespace

void buildInspectorFor(ScreenBuilder &builder, const UiRect &panel, EditorEntityId target, u64 onlyComponent);
// Hierarchy → Inspector → Components → campo / Add Component. A lista conserva
// todos os componentes do alvo enquanto a inspeção detalhada usa sua própria rota.
// Referência: Unity 6000.0 Manual/UsingComponents; identidade é a instância real.
void buildComponentOverview(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto add=takeBottom(content,44);takeBottom(content,6);
  const auto addHit=add;
  builder.list.addRect(add,theme.color.raised,theme.radius.control);
  builder.list.addImage(centred(takeLeft(add,36),22,22),static_cast<UiImageId>(UiIcon::ComponentAdd));
  builder.label(add,"Add Component",theme.color.text,theme.type.body);
  builder.router.addRegion(addHit,widgetId(EditorWidget::AddComponentMenu));
  // Os campos universais precedem os componentes na Unity. Dê a eles uma
  // entrada explícita: antes só apareciam como um cartão de outra página.
  auto object=takeTop(content,36);const auto objectHit=object;
  builder.list.addImage(centred(takeLeft(object,28),18,18),static_cast<UiImageId>(UiIcon::EditorAuthorObject),theme.color.textDim);
  builder.label(takeRight(object,20),"›",theme.color.textMuted,theme.type.body,UiAlign::Center);
  builder.label(object,"Objeto · Tag / Layer / Static",theme.color.textDim,theme.type.caption);
  builder.router.addRegion(objectHit,widgetId(EditorWidget::InspectorOverviewObject));
  if(builder.multiEdit && state.multi.hidden) {
    builder.label(takeTop(content,30),std::to_string(state.multi.hidden)+" componentes não comuns",theme.color.warning,theme.type.caption);
  }
  std::vector<u32> indices;
  for(u32 i=0;i<entity.components.size();++i) {
    const auto *value=entity.components.at(i);
    if(value->type().id=="astra.prefab.link" || value->type().id=="astra.import.link") continue;
    if(builder.multiEdit && !state.multi.isCommon(value->instanceId())) continue;
    indices.push_back(i);
  }
  constexpr float rowHeight=64;
  const float total=(indices.size()+1)*rowHeight;
  const auto window=content;
  const float offset=std::clamp(state.componentOverviewScroll,0.f,std::max(0.f,total-window.height));
  if(builder.layout) {builder.layout->componentOverviewWindow=window;builder.layout->componentOverviewContent=total;}
  builder.router.addRegion(window,widgetId(EditorWidget::InspectorOverviewScroll));
  builder.list.pushClip(window);
  const auto region=[&](UiRect area,u32 widget) {const auto hit=intersect(area,window);if(!hit.isEmpty()) builder.router.addRegion(hit,widget);};
  for(u32 row=0;row<=indices.size();++row) {
    UiRect line{window.x,window.y+row*rowHeight-offset,window.width,rowHeight};
    if(intersect(line,window).isEmpty()) continue;
    const auto hit=line;line=deflate(line,UiInsets{4,5,4,5});
    const auto *value=row?entity.components.at(indices[row-1]):nullptr;
    const auto *entry=value?findEditorComponent(value->type().id):nullptr;
    const auto *script=value?scene::scriptBehavior(value):nullptr;
    const auto *schema=script?scriptSchema(state,script->scriptType):nullptr;
    const bool inspectable=!value || (!value->unresolved() && (entry || script));
    const auto icon=row?(entry?entry->icon:UiIcon::ScriptingCode):UiIcon::EditorAuthorMove;
    builder.list.addImage(centred(takeLeft(line,34),24,24),static_cast<UiImageId>(icon));
    const auto next=takeRight(line,22);
    if(inspectable) builder.list.addImage(centred(next,14,14),static_cast<UiImageId>(UiIcon::UiChevronRight),theme.color.textMuted);
    else builder.list.addImage(centred(next,16,16),static_cast<UiImageId>(UiIcon::UiInfo),theme.color.warning);
    bool enabled=false,on=false;
    if(script) {enabled=true;on=script->enabled;}
    else if(entry) for(const auto &p:entry->type->booleans) if(p.id=="enabled") {enabled=true;on=p.read(*value);}
    const auto check=enabled?takeRight(line,34):UiRect{};
    std::string title=row?(entry?entry->name:schema?schema->name:script?script->scriptType:std::string(value->type().id)):"Transform";
    std::string detail=row?(entry?std::string(scene::componentFamilyName(entry->schema->family))+" / "+std::string(entry->schema->subfamily):script?"Script C#":"Dados preservados · indisponível"):"Position · Rotation · Scale";
    bool missing=value&&value->unresolved();
    if(entry) for(const auto &requirement:entry->schema->requirements) if(!entity.components.find(requirement.typeId)) {detail=requirement.message;missing=true;break;}
    builder.label(takeTop(line,29),title,theme.color.text,theme.type.body);
    builder.label(line,fitMiddle(builder.list,detail,line.width,theme.type.caption),missing?theme.color.warning:theme.color.textMuted,theme.type.caption);
    if(inspectable) region(hit,widgetId(row?EditorWidget::InspectorOverviewOpenBase:EditorWidget::InspectorOverviewTransform)+(row?indices[row-1]:0));
    if(enabled) {
      const auto box=centred(check,18,18);builder.list.addRect(box,on?theme.color.accent:theme.color.canvas,2);
      builder.list.addBorder(box,theme.color.textDim,1,2);
      if(builder.mixed(multiKey(value->instanceId(),"enabled"))) builder.list.addRect({box.x+4,box.y+8,10,2},theme.color.text,1);
      else if(on) builder.list.addImage(deflate(box,UiInsets::all(3)),static_cast<UiImageId>(UiIcon::UiCheck),theme.color.accentInk);
      region(check,widgetId(EditorWidget::ComponentEnableBase)+indices[row-1]);
    }
    builder.list.addRect({window.x,hit.bottom()-1,window.width,1},theme.color.lineSoft);
  }
  builder.list.popClip();
  if(total>window.height && window.height>0) {
    const float height=std::max(24.f,window.height*window.height/total);
    builder.list.addRect({window.right()-3,window.y+offset/(total-window.height)*(window.height-height),3,height},theme.color.textFaint,1);
  }
}


void buildInspector(ScreenBuilder &builder, const UiRect &panel) {
  if(builder.state.guiInspector) {
    builder.list.addRect(panel,builder.theme.color.surface);builder.router.addBlocker(panel);return;
  }
  if (builder.state.importPanel) {
    builder.list.addRect(panel, builder.theme.color.surface);
    builder.router.addBlocker(panel);
    buildImportDock(builder, deflate(panel, UiInsets::all(builder.theme.spacing.small)));
    return;
  }
  if (builder.state.multiAsset.items.size() > 1 && builder.state.multiAsset.kind != EditorScreenState::MultiAssetView::Kind::Materials &&
      builder.state.multiAsset.kind != EditorScreenState::MultiAssetView::Kind::Profiles &&
      builder.state.multiAsset.kind != EditorScreenState::MultiAssetView::Kind::EnvironmentMaps) {
    builder.list.addRect(panel, builder.theme.color.canvas);
    builder.router.addBlocker(panel);
    buildMultiAssetInspector(builder, deflate(panel, UiInsets::all(builder.theme.spacing.small)));
    return;
  }
  if (builder.state.materialInspector.valid()) {buildMaterialAssetInspector(builder, panel);return;}
  if (builder.state.environmentInspector.valid()) {buildEnvironmentAssetInspector(builder, panel);return;}
  if (builder.state.profileInspector.valid()) {buildProfileAssetInspector(builder, panel);return;}
  // R4: textura escolhida em Arquivos e gerenciador da pasta Texturas.
  if (builder.state.textureManager || builder.state.textureInspector) {
    // O Inspector de textura é fundo escuro com cartões por cima.
    builder.list.addRect(panel, builder.state.textureManager ? builder.theme.color.surface : builder.theme.color.canvas);
    builder.router.addBlocker(panel);
    const auto inner = deflate(panel, UiInsets::all(builder.theme.spacing.small));
    if (builder.state.textureManager) buildTextureManager(builder, inner);
    else buildTextureInspector(builder, inner);
    return;
  }
  // O cadeado prende o Inspector num objeto; a seleção segue livre na
  // Hierarquia e na cena (Unity Manual/InspectorOptions).
  buildInspectorFor(builder, panel, builder.state.inspectorLocked ? builder.state.inspectorLocked : builder.state.selection, 0);
}

// Inspector de um alvo: o principal (travado ou a seleção) ou um focado. Com
// `onlyComponent`, só o cartão daquela instância, aberto (Unity: Properties de
// um componente).
void buildObjectGroups(ScreenBuilder &builder,UiRect content,const EditorEntity &entity) {
  const auto &theme=builder.theme;
  auto header=takeTop(content,44);
  builder.iconButton(takeLeft(header,44),UiIcon::UiChevronLeft,widgetId(EditorWidget::GroupsClose));
  builder.label(header,"Grupos",theme.color.text,theme.type.cardName);
  takeTop(content,theme.spacing.small);
  auto context=takeTop(content,36);
  builder.list.addImage(centred(takeLeft(context,28),20,20),static_cast<UiImageId>(UiIcon::SceneGroups),theme.color.accent);
  builder.label(context,fitMiddle(builder.list,entity.name,context.width,theme.type.body),theme.color.text,theme.type.body);
  auto help=takeTop(content,36);
  builder.label(help,"Associações deste objeto",theme.color.textDim,theme.type.caption);
  auto add=takeTop(content,44);
  builder.list.addRect(add,theme.color.raised,theme.radius.control);
  const bool groupLimit=entity.groups.names().size()>=runtime::ObjectGroups::MaximumCount;
  builder.label(add,groupLimit?"Limite de 32 associações":"+ Associar grupo",groupLimit?theme.color.textMuted:theme.color.accent,theme.type.body,UiAlign::Center);
  if(entity.groups.names().size()<runtime::ObjectGroups::MaximumCount)
    builder.router.addRegion(add,widgetId(EditorWidget::GroupNew));
  takeTop(content,theme.spacing.small);
  builder.list.addRect(takeTop(content,1),theme.color.lineSoft);
  takeTop(content,theme.spacing.small);
  const auto &names=entity.groups.names();
  if(names.empty()) {
    builder.label(content,"Nenhuma associação",theme.color.textMuted,theme.type.body,UiAlign::Center);
    return;
  }
  auto footer=takeBottom(content,32);
  const u32 fits=std::max(1u,static_cast<u32>(std::max(0.f,content.height)/48));
  const u32 pages=(static_cast<u32>(names.size())+fits-1)/fits;
  const u32 page=std::min(builder.state.groupPage,pages-1);
  for(u32 i=page*fits;i<names.size() && i<(page+1)*fits && content.height>=44;++i) {
    auto row=takeTop(content,48);auto remove=takeRight(row,44);
    builder.iconButton(remove,UiIcon::UiRemove,widgetId(EditorWidget::GroupRemoveBase)+i);
    builder.label(row,fitMiddle(builder.list,names[i],row.width,theme.type.body),theme.color.text,theme.type.body);
    builder.router.addRegion(row,widgetId(EditorWidget::GroupEditBase)+i);
  }
  if(pages>1) {
    auto back=takeLeft(footer,44),next=takeRight(footer,44);
    if(page) builder.iconButton(back,UiIcon::UiChevronLeft,widgetId(EditorWidget::GroupsPrevious));
    if(page+1<pages) builder.iconButton(next,UiIcon::UiChevronRight,widgetId(EditorWidget::GroupsNext));
  }
  builder.label(footer,std::to_string(names.size())+" / 32 associações",theme.color.textMuted,theme.type.caption,UiAlign::Center);
}

void buildInspectorFor(ScreenBuilder &builder, const UiRect &panel, EditorEntityId target, u64 onlyComponent) {
  const UiTheme &theme = builder.theme;
  const EditorEntity *entity = builder.state.document->find(target);
  builder.list.addRect(panel, theme.color.surface);
  builder.router.addBlocker(panel);
  UiRect content = deflate(panel, UiInsets::all(theme.spacing.small));
  builder.onlyComponent = onlyComponent;
  bool addInHeader=false;

  if (entity == nullptr) {
    builder.label(content, "Nada selecionado", theme.color.textMuted, theme.type.body,
                  UiAlign::Center);
    return;
  }

  if(builder.state.tagPicker && !onlyComponent && (!builder.state.inspectorTarget || builder.state.inspectorTarget==target)) {
    builder.multiEdit=builder.state.multi.count>1 && target==builder.state.selection && !builder.focusedWindow;
    buildTags(builder,content,true);return;
  }
  if(builder.state.groupPicker && !onlyComponent && builder.state.groupEntity==target) {
    buildObjectGroups(builder,content,*entity);return;
  }
  UiRect header = takeTop(content, kPanelHeaderHeight);
  builder.list.addImage(centred(takeLeft(header, 26.0f), 18.0f, 18.0f),
                        static_cast<UiImageId>(iconForEntity(*entity)), theme.color.text);
  if (!builder.focusedWindow) {
    builder.iconButton(takeRight(header, 28.0f), UiIcon::EditorAuthorMore,
                       widgetId(EditorWidget::InspectorMenu));
    builder.iconButton(takeRight(header, 28.0f), UiIcon::SceneLock, widgetId(EditorWidget::InspectorLock),
                       builder.state.inspectorLocked != 0,
                       builder.state.inspectorLocked ? theme.color.accentInk : theme.color.textDim);
  }
  takeRight(header, theme.spacing.tiny);
  builder.multiEdit = builder.state.multi.count > 1 && target == builder.state.selection && !builder.focusedWindow && !onlyComponent;
  builder.checkbox(takeRight(header,28),entity->active,widgetId(EditorWidget::InspectorActive),true,builder.mixed("active"));
  const float half = header.height * 0.5f;
  if (builder.multiEdit) {
    // Unity: o cabeçalho diz quantos objetos; o ativo é o de referência.
    builder.label({header.x, header.y, header.width, half}, (std::to_string(builder.state.multi.count) + " objetos").c_str(),
                  theme.color.text, theme.type.cardName);
    builder.label({header.x, header.y + half, header.width, half}, (std::string("Ativo: ") + entity->name).c_str(),
                  theme.color.accent, theme.type.caption);
  } else
  builder.label({header.x, header.y, header.width, half}, fitMiddle(builder.list,entity->name,header.width,theme.type.cardName), theme.color.text,
                theme.type.cardName);
  // O vínculo com a fonte tem linha própria, não é card: não entra na contagem.
  // Perto do teto por objeto (a Unity não tem teto; aqui é explícito) a linha
  // passa a mostrar o limite em cor de aviso, antes de o Add recusar.
  if (!builder.multiEdit) {
    const usize cards=entity->components.size()-(scene::importLink(entity->components)?1u:0u)-(scene::prefabLink(entity->components)?1u:0u);
    const bool near=entity->components.size()+8>=scene::Components::MaximumCount;
    const std::string count=near?std::to_string(entity->components.size())+" de "+std::to_string(scene::Components::MaximumCount)+" componentes":
                                 std::to_string(cards)+(cards==1?" componente":" componentes");
    builder.label({header.x, header.y + half, header.width, half},count.c_str(),
                  near?theme.color.warning:theme.color.textDim, theme.type.label);
  }

  if(!onlyComponent && !builder.focusedWindow && builder.state.workspace!=EditorWorkspace::Lighting) {
    auto tabs=takeTop(content,42);takeTop(content,6);
    addInHeader=builder.state.inspectorSurface==EditorInspectorSurface::Inspection && content.height<300 && !builder.state.addingComponent;
    if(addInHeader)
      builder.iconButton(takeRight(tabs,36),UiIcon::ComponentAdd,widgetId(EditorWidget::AddComponentMenu));
    const float width=tabs.width*.5f;
    const char *names[]{"Inspeção","Componentes"};
    const UiIcon icons[]{UiIcon::EditorInspection,UiIcon::EditorComponents};
    const EditorWidget actions[]{EditorWidget::InspectorInspection,EditorWidget::InspectorComponents};
    for(u32 i=0;i<2;++i) {
      auto tab=takeLeft(tabs,width);const bool active=static_cast<u32>(builder.state.inspectorSurface)==i;
      const auto text=tab;
      if(tab.width>=120) builder.list.addImage(centred(takeLeft(tab,26),16,16),static_cast<UiImageId>(icons[i]),active?theme.color.accent:theme.color.textMuted);
      builder.label(tab,names[i],active?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
      if(active) builder.list.addRect({text.x,text.bottom()-3,text.width,3},theme.color.accent);
      builder.router.addRegion(text,widgetId(actions[i]));
    }
  }
  if(builder.state.inspectorDebug && !builder.state.inspectorMenu) {
    buildInspectorDebug(builder,content,*entity);
    return;
  }
  if((builder.state.colliderTopology&&builder.state.colliderTopology->object==target)||builder.state.physicsDiagnosticOpen){buildColliderAuthoringInspector(builder,content);return;}
  if(builder.state.pathEditorOpen) {
    const auto *path=entity->components.findInstance(builder.state.pathInstance);
    if(path&&&path->type()==&scene::Path::descriptor) {
      for(u32 n=0;n<entity->components.size();++n)if(entity->components.at(n)==path) {
        buildPathPoints(builder,content,*entity,static_cast<const scene::Path&>(*path),n);return;
      }
    }
  }
  if(builder.state.prefabOverridesOpen && builder.state.prefabOverrides.object==target &&
     !builder.multiEdit && builder.state.workspace==EditorWorkspace::Scene && !builder.focusedWindow) {
    buildPrefabOverrides(builder,content);return;
  }
  if(builder.state.inspectorMenu && builder.state.workspace==EditorWorkspace::Scene) {
    buildObjectActions(builder,content,*entity);
    return;
  }
  if(builder.state.workspace==EditorWorkspace::Lighting) {
    builder.label(takeTop(content,38),"Ambiente",theme.color.text,theme.type.body);
    buildPropertyPage(builder,content,*builder.state.document->find(builder.state.document->root()),EditorPropertyGroup::Environment);
    return;
  }
  if(builder.state.inspectorSurface==EditorInspectorSurface::Components && !onlyComponent && !builder.focusedWindow) {
    if(builder.state.addingComponent && builder.state.componentSelection==entity->id) {
      buildComponents(builder,content,*entity,addInHeader);return;
    }
    buildComponentOverview(builder,content,*entity);return;
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
  if(const auto *link=scene::prefabLink(entity->components)) {
    const auto *record=builder.state.assetRegistry?builder.state.assetRegistry->find(link->asset):nullptr;
    auto row=takeTop(content,44);
    if(!builder.multiEdit && builder.state.workspace==EditorWorkspace::Scene && !builder.focusedWindow)
      builder.router.addRegion(row,widgetId(EditorWidget::PrefabOverrides));
    builder.label(takeRight(row,20),">",theme.color.accent,theme.type.caption,UiAlign::Center);
    builder.list.addImage(centred(takeLeft(row,24),16,16),static_cast<UiImageId>(UiIcon::ScenePrefab),theme.color.accent);
    builder.label(row,record?record->path:"Fonte de prefab ausente",record?theme.color.textDim:theme.color.warning,theme.type.caption);
  }
  buildImportLinkCard(builder,content);
  buildComponents(builder,content,*entity,addInHeader);
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
         state.editingCreationSearch || state.editingComponentSearch || state.editingPropertySearch || state.editingMeshSearch ||
         state.editingReferenceSearch || state.editingGlobalSearch || state.namingLayout || state.numericField != 0 || (state.colorField != 0 && state.colorText != 0) ||
         (state.gradientField != 0 && state.gradientText != 0) || (state.curveField != 0 && state.curveText != 0) ||
         state.editingScriptInstance != 0 || state.creatingScript || state.searchingCode ||
         state.renamingResource || state.goingToLine || state.creatingCodeFolder || state.searchingConsole || state.searchingTextures || state.presetNaming || state.viewNaming ||
         state.editingTagName || state.editingTagSearch || state.editingPhysicsLayerName || state.editingInputActionName || state.editingInputContext || state.inputEditField;
}

const char *platformFieldTitle(const EditorScreenState &state) {
  if (state.presetNaming) return "Nome do preset";
  if (state.viewNaming) return state.viewRenaming ? "Novo nome da vista" : "Nome da vista";
  if (state.namingLayout) return "Nome do layout";
  if (state.colorField != 0 && state.colorText != 0) return state.colorText == 1 ? "Hexadecimal" : "Nome";
  if (state.numericField != 0) return "Valor";
  if (state.editingInputActionName) return "Ação";
  if (state.editingInputContext) return "Contexto";
  if (state.editingTagName) return "Nova tag";
  if (state.editingTagSearch) return "Buscar tag";
  if (state.editingPhysicsLayerName) return "Camada";
  if (state.renamingResource) return "Arquivo";
  if (state.editingScriptInstance != 0) return "Campo";
  if (state.creatingScript) return state.scriptTemplate==EditorCodeWorkspace::HelperTemplate?"Auxiliar C#":"Componente C#";
  if (state.searchingTextures) return "Buscar textura";
  if (state.searchingCode) return "Localizar";
  if (state.searchingConsole) return "Console";
  if (state.goingToLine) return "Ir para linha";
  if (state.creatingCodeFolder) return "Nova pasta";
  if (state.renameEntity != kInvalidEntity) return "Nome";
  if (state.editingPropertySearch) return "Propriedade";
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
  // Campo numérico com o teclado do sistema: o teclado numérico não tem
  // operadores, então "Expressão" troca para o de texto (e volta).
  if (state.numericField != 0 && (state.numericField&0xff000000u)!=widgetId(EditorWidget::ComponentTripleBase)) {
    const UiRect toggle = takeRight(content, 96.0f);
    builder.list.addRect(toggle, state.numericExpression ? theme.color.accent : theme.color.raised, theme.radius.control);
    builder.label(toggle, state.numericExpression ? "Números" : "Expressão",
                  state.numericExpression ? theme.color.accentInk : theme.color.text, theme.type.caption, UiAlign::Center);
    builder.router.addRegion(toggle, widgetId(EditorWidget::NumericExpressionToggle), theme.touch.minimumTarget);
    takeRight(content, 8.0f);
    double preview = 0;
    NumericExpressionContext context;
    context.current = state.numericCurrent;
    if (!state.platformDraft.empty() && evaluateNumericExpression(state.platformDraft, context, preview)) {
      char line[40];
      std::snprintf(line, sizeof(line), "= %.6g", preview);
      builder.label(takeRight(content, 110.0f), line, theme.color.accent, theme.type.caption, UiAlign::End);
    }
  }
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
  const u32 rowCount=state.qualityTab==2?11u:state.qualityTab==0?9u:state.qualityTab==1?8u:state.qualityTab==4?8u:7u;
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
    // S4: as luzes da cena num lugar só (Light Explorer da Unity).
    row("Explorador de luzes",std::to_string(state.lightExplorerTotal)+" luz(es)"+
        (state.lightExplorerDark?" · "+std::to_string(state.lightExplorerDark)+" apagada(s)":std::string()),
        EditorWidget::LightExplorerOpen);
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
    row("Estatísticas no viewport",state.sceneStatisticsVisible?"Visíveis":"Ocultas",EditorWidget::QualitySceneStatistics);
  }
  // Aplicar reconstrói o renderer: é explícito, como o Apply do Import
  // Settings, porque troca alvos de renderização e leva um instante.
  list.addRect(apply, state.qualityDirty ? theme.color.accent : theme.color.raised, theme.radius.control);
  builder.label(apply, state.qualityDirty ? "Aplicar" : "Aplicado",
                state.qualityDirty ? theme.color.accentInk : theme.color.textMuted, theme.type.caption, UiAlign::Center);
  if (state.qualityDirty) router.addRegion(apply, widgetId(EditorWidget::QualityApply));
  builder.label(stats, state.qualityStats, theme.color.textMuted, theme.type.caption);
}

// Explorador de luzes (S4) — o Light Explorer da Unity: todas as luzes da cena
// numa lista, com o que decide se iluminam (acesa, tipo, intensidade na unidade
// da própria luz, alcance). Tocar no nome seleciona a luz no Inspector; a ação
// em lote escreve uma intensidade nas luzes do filtro em um único Undo.
void buildLightExplorer(ScreenBuilder &builder, const UiRect &viewport) {
  const auto &state=builder.state;
  const auto &theme=builder.theme;
  auto &list=builder.list;
  auto &router=builder.router;
  const float width=std::min(520.0f,std::max(280.0f,viewport.width-24.0f));
  const float height=std::min(viewport.height-16.0f,520.0f);
  const UiRect panel{viewport.x+std::max(8.0f,std::min(16.0f,viewport.width-width-8.0f)),viewport.y+8.0f,width,height};
  list.addRect(panel,theme.color.surface,6);
  router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(10));
  auto header=takeTop(content,26);
  const auto close=takeRight(header,28);
  builder.label(close,"X",theme.color.textDim,theme.type.caption,UiAlign::Center);
  router.addRegion(close,widgetId(EditorWidget::LightExplorerClose));
  builder.label(header,"Luzes da cena",theme.color.text,theme.type.cardName);
  // Filtro e ação em lote ficam no rodapé: nunca saem do painel.
  auto apply=takeBottom(content,40);
  auto batch=takeBottom(content,36);
  auto filter=deflate(takeTop(content,34),UiInsets::all(2));
  list.addRect(filter,theme.color.raised,theme.radius.control);
  builder.label(filter,state.lightExplorerDarkOnly
      ?"Mostrando apagadas: "+std::to_string(state.lightExplorerDark)+" de "+std::to_string(state.lightExplorerTotal)
      :"Mostrando todas: "+std::to_string(state.lightExplorerTotal)+" · "+std::to_string(state.lightExplorerDark)+" apagada(s)",
      theme.color.text,theme.type.caption,UiAlign::Center);
  router.addRegion(filter,widgetId(EditorWidget::LightExplorerFilter));
  const u32 pages=std::max(1u,(state.lightExplorerFiltered+7u)/8u);
  if(pages>1) {
    auto pager=takeRight(header,std::min(116.0f,header.width*.48f));
    const auto previous=takeLeft(pager,30.0f),next=takeRight(pager,30.0f);
    builder.label(previous,"‹",state.lightExplorerPage?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(next,"›",state.lightExplorerPage+1u<pages?theme.color.text:theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(pager,std::to_string(state.lightExplorerPage+1u)+" / "+std::to_string(pages),theme.color.textDim,
                  theme.type.caption,UiAlign::Center);
    if(state.lightExplorerPage) router.addRegion(previous,widgetId(EditorWidget::LightExplorerPrevious));
    if(state.lightExplorerPage+1u<pages) router.addRegion(next,widgetId(EditorWidget::LightExplorerNext));
  }
  if(state.lightExplorerRows.empty())
    builder.label(takeTop(content,30),state.lightExplorerDarkOnly?"Nenhuma luz apagada.":"A cena não tem luzes.",
                  theme.color.textMuted,theme.type.caption,UiAlign::Center);
  const float rowHeight=std::clamp(content.height/8.0f,30.0f,44.0f);
  for(u32 i=0;i<state.lightExplorerRows.size() && i<8;++i) {
    const auto &row=state.lightExplorerRows[i];
    auto line=takeTop(content,rowHeight);
    const auto toggle=deflate(takeRight(line,96.0f),UiInsets::all(2));
    list.addRect(toggle,row.enabled?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(toggle,row.enabled?"Acesa":"Apagada",row.enabled?theme.color.accentInk:theme.color.text,
                  theme.type.caption,UiAlign::Center);
    router.addRegion(toggle,widgetId(EditorWidget::LightExplorerToggle0)+i);
    const auto name=deflate(line,UiInsets::all(2));
    builder.label(takeTop(line,line.height*.5f),row.name,row.dark?theme.color.textMuted:theme.color.text,theme.type.caption);
    builder.label(line,row.detail,row.dark?theme.color.accent:theme.color.textDim,theme.type.caption);
    router.addRegion(name,widgetId(EditorWidget::LightExplorerRow0)+i);
  }
  auto label=takeLeft(batch,batch.width*.46f);
  builder.label(label,"Intensidade em lote",theme.color.textDim,theme.type.caption);
  const auto minus=deflate(takeLeft(batch,36),UiInsets::all(2)),plus=deflate(takeRight(batch,36),UiInsets::all(2));
  for(const auto &[rect,glyph,widget]:{std::tuple{minus,"-",EditorWidget::LightExplorerIntensityDown},
                                       std::tuple{plus,"+",EditorWidget::LightExplorerIntensityUp}}) {
    list.addRect(rect,theme.color.raised,theme.radius.control);
    builder.label(rect,glyph,theme.color.text,theme.type.body,UiAlign::Center);
    router.addRegion(rect,widgetId(widget));
  }
  builder.label(batch,decimalText(state.lightExplorerIntensity,0)+" (unidade de cada luz)",theme.color.text,
                theme.type.caption,UiAlign::Center);
  apply=deflate(apply,UiInsets::all(2));
  const bool any=state.lightExplorerFiltered>0;
  list.addRect(apply,any?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(apply,any?"Aplicar às "+std::to_string(state.lightExplorerFiltered)+" luz(es) "+
                (state.lightExplorerDarkOnly?"apagadas":"listadas")+" e acender":std::string("Nada a aplicar"),
                any?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
  if(any) router.addRegion(apply,widgetId(EditorWidget::LightExplorerApply));
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
// Camadas na vista da cena: o olho esconde os objetos da camada só no editor
// (Unity 6000.0 SceneVisibility / View Options › Layers); a seta os tira da
// seleção por toque na vista (ScenePickingControls). A Hierarquia continua
// selecionando tudo. Tudo/Nada como no menu Layers da Unity.
void buildSceneLayersPanel(ScreenBuilder &builder, const UiRect &viewport) {
  const auto &state=builder.state;const auto &theme=builder.theme;auto &list=builder.list;auto &router=builder.router;
  const auto &layers=state.document->layers();
  u32 counts[runtime::GameplayLayers::kCount]{};
  std::vector<EditorEntityId> all;state.document->collectSubtree(state.document->root(),all);
  for(const auto id:all) if(const auto *e=state.document->find(id); e && id!=state.document->root() && e->layer<runtime::GameplayLayers::kCount) ++counts[e->layer];
  // Camadas com nome, e as sem nome que ainda têm objetos (senão sumiriam).
  std::vector<u32> shown;
  for(u32 i=0;i<runtime::GameplayLayers::kCount;++i) if(layers.named(i) || counts[i]) shown.push_back(i);
  const float width=std::min(310.f,std::max(200.f,viewport.width-16));
  // Altura pelo conteúdo (cabeçalho, ações, linhas e rodapé), até a do viewport.
  const float wanted=36+6+36+26+16+38.f*static_cast<float>(std::max<usize>(1,shown.size()));
  const UiRect panel{viewport.x+std::max(8.f,std::min(276.f,viewport.width-width-8)),viewport.y+56,width,
                     std::clamp(wanted,160.f,std::max(160.f,viewport.height-64))};
  list.addRect(deflate(panel,UiInsets::all(-1)),theme.color.line,theme.radius.control);
  list.addRect(panel,theme.color.surface,theme.radius.control);
  router.addBlocker(panel);
  auto content=deflate(panel,UiInsets::all(8));
  auto header=takeTop(content,36);
  builder.iconButton(takeRight(header,32),UiIcon::UiClose,widgetId(EditorWidget::SceneLayersClose));
  list.addImage(centred(takeLeft(header,28),18,18),static_cast<UiImageId>(UiIcon::SceneVisibility),theme.color.accent);
  const float half=header.height*.5f;
  builder.label({header.x,header.y,header.width,half},"Camadas na vista",theme.color.text,theme.type.cardName);
  builder.label({header.x,header.y+half,header.width,half},"Só no editor; o jogo não muda",theme.color.textDim,theme.type.caption);
  // Ações de conjunto.
  auto actions=takeTop(content,30);takeTop(content,6);
  const float third=(actions.width-8)/3;
  const struct {const char *label;EditorWidget id;bool on;} bulk[]{
    {"Ver todas",EditorWidget::SceneLayersShowAll,state.hiddenLayers!=0},
    {"Esconder",EditorWidget::SceneLayersHideAll,state.hiddenLayers!=0xffffffffu},
    {"Tocar todas",EditorWidget::SceneLayersPickAll,state.unpickableLayers!=0}};
  for(u32 i=0;i<3;++i) {
    const UiRect rect{actions.x+i*(third+4),actions.y,third,actions.height};
    list.addRect(rect,theme.color.raised,theme.radius.control);
    builder.label(rect,bulk[i].label,bulk[i].on?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    if(bulk[i].on) router.addRegion(rect,widgetId(bulk[i].id));
  }
  auto footer=takeBottom(content,26);
  const float rowHeight=38;
  const u32 perPage=std::max(1u,static_cast<u32>(content.height/rowHeight));
  const u32 count=static_cast<u32>(shown.size());
  const u32 pages=std::max(1u,(count+perPage-1)/perPage),page=std::min(state.sceneLayersPage,pages-1);
  for(u32 slot=page*perPage;slot<count && slot<(page+1)*perPage;++slot) {
    const u32 layer=shown[slot];
    const bool hidden=state.hiddenLayers&(1u<<layer),locked=state.unpickableLayers&(1u<<layer);
    auto row=takeTop(content,rowHeight);row.height-=4;
    list.addRect(row,hidden?theme.color.silhouette:theme.color.raised,theme.radius.control);
    const auto eye=takeLeft(row,40),pick=takeRight(row,40);
    list.addImage(centred(eye,18,18),static_cast<UiImageId>(hidden?UiIcon::SceneVisibilityOff:UiIcon::SceneVisibility),
                  hidden?theme.color.textFaint:theme.color.accent);
    list.addImage(centred(pick,18,18),static_cast<UiImageId>(locked?UiIcon::SceneLock:UiIcon::EditorAuthorSelect),
                  locked||hidden?theme.color.textFaint:theme.color.text);
    auto text=deflate(row,UiInsets{2,0,4,0});
    const auto number=takeRight(text,64);
    const std::string name=layers.named(layer)?std::string(layers.name(layer)):"Camada "+std::to_string(layer)+" (sem nome)";
    builder.label({text.x,text.y,text.width,text.height*.55f},name.c_str(),hidden?theme.color.textFaint:theme.color.text,theme.type.caption);
    const std::string detail=std::to_string(counts[layer])+(counts[layer]==1?" objeto":" objetos")+
        (hidden?" · escondida":locked?" · sem seleção":"");
    builder.label({text.x,text.y+text.height*.5f,text.width,text.height*.5f},detail.c_str(),
                  hidden?theme.color.warning:locked?theme.color.textMuted:theme.color.textDim,theme.type.caption);
    builder.label(number,("#"+std::to_string(layer)).c_str(),theme.color.textFaint,theme.type.caption,UiAlign::Center);
    router.addRegion(eye,widgetId(EditorWidget::SceneLayerVisibleBase)+layer);
    router.addRegion(pick,widgetId(EditorWidget::SceneLayerPickBase)+layer);
  }
  if(pages>1) {
    const auto previous=takeLeft(footer,40),next=takeRight(footer,40);
    builder.label(previous,"<",page?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    builder.label(next,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.title,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::SceneLayersPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::SceneLayersNext));
    builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textDim,theme.type.caption,UiAlign::Center);
  } else builder.label(footer,"Olho: mostrar · seta: tocar seleciona",theme.color.textFaint,theme.type.caption,UiAlign::Center);
}

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

// O mesmo console que recebe compilação, scripts e importação fica acessível
// na Cena. O Add do Inspector continua sendo a única entrada para componentes.
void buildDiagnosticDock(ScreenBuilder &builder,const UiRect &bounds) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  const auto *console=state.console;
  auto &list=builder.list;auto &router=builder.router;
  if(bounds.isEmpty()) return;
  router.addBlocker(bounds);
  list.addRect(bounds,theme.color.surface);
  list.addRect({bounds.x,bounds.y,bounds.width,2},theme.color.line);
  auto content=deflate(bounds,UiInsets::symmetric(8,6));
  auto header=takeTop(content,40);
  auto close=takeRight(header,40);
  builder.label(close,"×",theme.color.textDim,theme.type.body,UiAlign::Center);
  router.addRegion(close,widgetId(EditorWidget::DiagnosticDockToggle),theme.touch.minimumTarget);
  const auto tab=[&](UiRect rect,UiIcon icon,const char *name,EditorWidget widget,bool active,u32 count) {
    const auto hit=rect;
    list.addRect(rect,active?theme.color.raised:theme.color.surface,theme.radius.control);
    list.addImage(centred(takeLeft(rect,32),20,20),static_cast<UiImageId>(icon),
                  active?theme.color.accent:theme.color.textDim);
    const auto label=std::string(name)+" "+std::to_string(count);
    builder.label(rect,label.c_str(),active?theme.color.text:theme.color.textDim,theme.type.caption);
    router.addRegion(hit,widgetId(widget),theme.touch.minimumTarget);
  };
  u32 problems=0,logs=0;
  if(console) for(const auto &event:console->entries()) {
    if(event.origin==EditorConsoleOrigin::Compiler) ++problems; else ++logs;
  }
  tab(takeLeft(header,std::min(150.0f,header.width*.48f)),UiIcon::UiDiagnostics,
      "Problemas",EditorWidget::ConsoleProblems,state.consoleProblems,problems);
  tab(takeLeft(header,std::min(150.0f,header.width*.60f)),UiIcon::IdeConsole,
      "Registros",EditorWidget::ConsoleLogs,!state.consoleProblems,logs);
  if(!console) {builder.label(content,"Console indisponível",theme.color.textMuted,theme.type.caption);return;}
  const auto order=console->filtered(state.consoleProblems?1:0,-1,{});
  const u32 rows=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-32.0f)/42.0f));
  auto pager=takeBottom(content,30);
  const u32 offset=std::min(state.consoleScroll,order.size()>rows?static_cast<u32>(order.size()-rows):0u);
  const u32 end=static_cast<u32>(order.size())-offset;
  const u32 first=end>rows?end-rows:0;
  if(order.size()>rows) {
    auto older=takeLeft(pager,84),newer=takeRight(pager,84);
    builder.label(older,"‹ Antigos",theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(newer,"Recentes ›",theme.color.textDim,theme.type.caption,UiAlign::Center);
    if(first) router.addRegion(older,widgetId(EditorWidget::DiagnosticOlder));
    if(offset) router.addRegion(newer,widgetId(EditorWidget::DiagnosticNewer));
  }
  builder.label(pager,(std::to_string(order.empty()?0:first+1)+"–"+std::to_string(end)+" / "+
                       std::to_string(order.size())).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  if(order.empty()) builder.label(content,state.consoleProblems?"Nenhum problema":"Nenhum registro",
                                  theme.color.textDim,theme.type.caption);
  list.pushClip(content);
  for(u32 i=first;i<end;++i) {
    auto row=takeTop(content,42);
    if(row.height<40) break;
    const auto *event=console->at(order[i]);if(!event) continue;
    const auto hit=row;
    list.addRect(row,theme.color.raised,theme.radius.control);
    const auto severity=event->severity==EditorConsoleSeverity::Error?theme.color.danger:
                        event->severity==EditorConsoleSeverity::Warning?theme.color.warning:theme.color.accent;
    list.addRect({row.x+4,row.y+7,3,row.height-14},severity,1);
    row=deflate(row,UiInsets::symmetric(12,2));
    const auto title=event->message+(event->repeats>1?" ×"+std::to_string(event->repeats):"");
    builder.label(takeTop(row,22),title.c_str(),theme.color.text,theme.type.caption);
    const auto source=event->file.empty()?EditorConsole::originName(event->origin):event->file;
    builder.label(row,source,theme.color.textMuted,theme.type.caption);
    router.addRegion(hit,widgetId(EditorWidget::ConsoleRowBase)+order[i],theme.touch.minimumTarget);
  }
  list.popClip();
}

void buildDiagnosticDetail(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  const auto *event=state.console?state.console->find(state.consoleSelected):nullptr;
  if(!state.diagnosticDockOpen || state.workspace!=EditorWorkspace::Scene || !event) return;
  auto &list=builder.list;auto &router=builder.router;
  list.addRect(state.surface,withAlpha(theme.color.voidBlack,.62f));router.addBlocker(state.surface);
  const UiRect modal=centred(state.surface,std::min(520.0f,state.surface.width-24),
                              std::min(340.0f,state.surface.height-24));
  list.addRect(modal,theme.color.surface,theme.radius.control);
  auto content=deflate(modal,UiInsets::all(14));
  auto heading=takeTop(content,38),close=takeRight(heading,42);
  builder.label(heading,"Detalhes do evento",theme.color.text,theme.type.cardName);
  builder.label(close,"×",theme.color.textDim,theme.type.body,UiAlign::Center);
  router.addRegion(close,widgetId(EditorWidget::ConsoleDetailClose));
  auto actions=takeBottom(content,42);
  const bool source=!event->file.empty()||event->object!=0;
  auto open=takeLeft(actions,actions.width*.65f);
  list.addRect(open,source?theme.color.accent:theme.color.raised,theme.radius.control);
  builder.label(open,"Abrir origem",source?theme.color.accentInk:theme.color.textMuted,
                theme.type.caption,UiAlign::Center);
  if(source) router.addRegion(open,widgetId(EditorWidget::ConsoleOpenSource));
  builder.label(actions,"Copiar",theme.color.textDim,theme.type.caption,UiAlign::Center);
  router.addRegion(actions,widgetId(EditorWidget::ConsoleCopy));
  const auto lines=wrapText(list,EditorConsole::describe(*event),content.width,theme.type.caption);
  const u32 perPage=std::max(1u,static_cast<u32>(content.height/24.0f));
  const u32 pages=std::max(1u,(static_cast<u32>(lines.size())+perPage-1)/perPage);
  const u32 page=std::min(state.consoleDetailPage,pages-1);
  if(pages>1) {
    auto pager=takeBottom(content,28),previous=takeLeft(pager,70),next=takeRight(pager,70);
    builder.label(previous,"‹",theme.color.text,theme.type.body,UiAlign::Center);
    builder.label(next,"›",theme.color.text,theme.type.body,UiAlign::Center);
    if(page) router.addRegion(previous,widgetId(EditorWidget::ConsoleDetailPrevious));
    if(page+1<pages) router.addRegion(next,widgetId(EditorWidget::ConsoleDetailNext));
    builder.label(pager,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),
                  theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  list.pushClip(content);
  for(u32 i=page*perPage;i<std::min(static_cast<u32>(lines.size()),(page+1)*perPage);++i)
    builder.label(takeTop(content,24),lines[i].c_str(),theme.color.textDim,theme.type.caption);
  list.popClip();
}

std::vector<u32> namedPhysicsLayers(const runtime::GameplayLayers &layers) {
  std::vector<u32> ids;
  for(u32 i=0;i<runtime::GameplayLayers::kCount;++i) if(layers.named(i)) ids.push_back(i);
  return ids;
}
void buildPhysicsLayers(ScreenBuilder &builder,UiRect area) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
  const auto &layers=state.document->layers();const auto named=namedPhysicsLayers(layers);
  const u32 selected=layers.named(state.physicsLayer)?state.physicsLayer:0;
  list.addRect(area,theme.color.surface);
  auto content=deflate(area,UiInsets::all(12));
  auto heading=takeTop(content,42),add=takeRight(heading,102);
  builder.label(heading,"Camadas de colisão",theme.color.text,theme.type.cardName);
  list.addRect(add,theme.color.accent,theme.radius.control);
  builder.label(add,"+ Camada",theme.color.accentInk,theme.type.caption,UiAlign::Center);
  router.addRegion(add,widgetId(EditorWidget::PhysicsLayerAdd));
  builder.label(takeTop(content,30),"A matriz é simétrica e o solver usa esses pares no Play.",theme.color.textDim,theme.type.caption);
  auto current=takeTop(content,48),previous=takeLeft(current,42),next=takeRight(current,42),rename=takeRight(current,82);
  builder.label(previous,"<",theme.color.text,theme.type.body,UiAlign::Center);
  builder.label(next,">",theme.color.text,theme.type.body,UiAlign::Center);
  list.addRect(current,theme.color.raised,theme.radius.control);
  builder.label(current,(std::to_string(selected)+" · "+std::string(layers.name(selected))).c_str(),theme.color.accent,theme.type.body);
  builder.label(rename,"Renomear",theme.color.text,theme.type.caption,UiAlign::Center);
  router.addRegion(previous,widgetId(EditorWidget::PhysicsLayerPrevious));
  router.addRegion(next,widgetId(EditorWidget::PhysicsLayerNext));
  router.addRegion(rename,widgetId(EditorWidget::PhysicsLayerRename));
  takeTop(content,10);
  builder.label(takeTop(content,28),"INTERAGE COM",theme.color.textDim,theme.type.caption);
  const u32 pageSize=std::max(1u,static_cast<u32>(std::max(0.0f,content.height-44)/46.0f));
  const u32 pages=std::max(1u,(static_cast<u32>(named.size())+pageSize-1)/pageSize);
  const u32 page=std::min(state.physicsMatrixPage,pages-1);
  for(u32 i=page*pageSize;i<std::min(static_cast<u32>(named.size()),(page+1)*pageSize);++i) {
    auto row=takeTop(content,46);if(row.height<42) break;
    const u32 target=named[i];const bool on=layers.interacts(selected,target);
    list.addRect(row,theme.color.silhouette,2);
    auto toggle=takeRight(row,64);
    list.addRect(deflate(toggle,UiInsets::all(5)),on?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(toggle,on?"SIM":"NÃO",on?theme.color.accentInk:theme.color.textDim,theme.type.caption,UiAlign::Center);
    builder.label(deflate(row,UiInsets::symmetric(10,0)),
      (std::to_string(target)+" · "+std::string(layers.name(target))).c_str(),theme.color.text,theme.type.body);
    router.addRegion({content.x,row.y,content.width,row.height},widgetId(EditorWidget::PhysicsInteractionBase)+target,theme.touch.minimumTarget);
  }
  if(pages>1) {
    auto footer=takeBottom(content,38),previousPage=takeLeft(footer,72),nextPage=takeRight(footer,72);
    builder.label(previousPage,"<",page?theme.color.text:theme.color.textFaint,theme.type.body,UiAlign::Center);
    builder.label(nextPage,">",page+1<pages?theme.color.text:theme.color.textFaint,theme.type.body,UiAlign::Center);
    if(page) router.addRegion(previousPage,widgetId(EditorWidget::PhysicsMatrixPrevious));
    if(page+1<pages) router.addRegion(nextPage,widgetId(EditorWidget::PhysicsMatrixNext));
    builder.label(footer,(std::to_string(page+1)+" / "+std::to_string(pages)).c_str(),theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
}

std::string inputBindingCodeLabel(const runtime::InputBinding &binding) {
  constexpr const char *buttons[]={"Principal","Secundário","Meio","Voltar","Avançar"};
  constexpr const char *axes[]={"X direita","Y abaixo","Scroll horizontal","Scroll vertical"};
  if(binding.source==runtime::InputSource::MouseButton && binding.code<5)return buttons[binding.code];
  if(binding.source==runtime::InputSource::MouseAxis && binding.code<4)return axes[binding.code];
  return std::to_string(binding.code);
}

void buildInputWorkspaceFocused(ScreenBuilder &builder,UiRect area) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
  const auto &actions=state.document->inputActions().actions();
  list.addRect(area,theme.color.surface);
  UiRect content=deflate(area,UiInsets::all(8));
  const auto button=[&](UiRect rect,std::string_view title,EditorWidget widget,bool active=false,
                        bool enabled=true) {
    list.addRect(rect,active?theme.color.accent:theme.color.raised,theme.radius.control);
    builder.label(rect,title,active?theme.color.accentInk:
                  enabled?theme.color.text:theme.color.textFaint,theme.type.caption,UiAlign::Center);
    if(enabled) router.addRegion(rect,widgetId(widget),theme.touch.minimumTarget);
  };
  const auto field=[&](UiRect &column,std::string_view title,std::string_view value,EditorWidget widget) {
    const UiRect row=takeTop(column,42);
    list.addRect(row,theme.color.silhouette,2);
    UiRect labels=deflate(row,UiInsets::symmetric(10,0));
    builder.label(takeLeft(labels,labels.width*.38f),title,theme.color.textDim,theme.type.caption);
    list.pushClip(labels);
    builder.label(labels,value,theme.color.text,theme.type.caption,UiAlign::Center);
    list.popClip();
    router.addRegion(row,widgetId(widget),theme.touch.minimumTarget);
  };
  const auto captureField=[&](UiRect &column,std::string_view title,EditorWidget editWidget,EditorWidget captureWidget) {
    auto row=takeTop(column,44),edit=takeLeft(row,row.width*.5f);
    button(edit,title,editWidget);button(row,captureWidget==EditorWidget::InputBindingCaptureNegative?"Capturar -":"Capturar",captureWidget);
  };
  if(state.inputCapturing) {
    auto title=takeTop(content,48),cancel=takeRight(title,std::min(120.f,title.width*.4f));
    button(cancel,"Cancelar · Esc",EditorWidget::InputBindingCaptureCancel);
    builder.label(title,"CAPTURAR",theme.color.accent,theme.type.cardName);
    UiIcon sourceIcon=UiIcon::InputKeyboard;std::string current;
    if(state.inputActionIndex<actions.size()) {
      const auto &action=actions[state.inputActionIndex];
      builder.label(takeTop(content,36),(action.id+"  /  vínculo "+std::to_string(state.inputBindingIndex+1)).c_str(),theme.color.text,theme.type.cardName);
      if(state.inputBindingIndex<action.bindings.size()) {
        const auto &binding=action.bindings[state.inputBindingIndex];
        sourceIcon=binding.source==runtime::InputSource::Key?UiIcon::InputKeyboard:binding.source==runtime::InputSource::MouseButton?UiIcon::InputMouse:UiIcon::InputGamepad;
        current=binding.source==runtime::InputSource::Key?"Teclado":binding.source==runtime::InputSource::GamepadAxis?"Eixo gamepad":binding.source==runtime::InputSource::MouseButton?"Botão mouse":"Botão gamepad";
        current+=" · atual "+(binding.source==runtime::InputSource::MouseButton?inputBindingCodeLabel(binding):std::to_string(state.inputCaptureNegative?binding.negativeCode:binding.code));
        if(state.inputCaptureNegative)current+=" · negativo";
      }
    }
    list.addRect(takeTop(content,1),theme.color.raised);
    takeTop(content,std::min(24.f,content.height*.05f));
    auto focus=takeTop(content,std::min(94.f,content.height*.27f));
    list.addImage(centred(focus,48,48),static_cast<UiImageId>(sourceIcon),0xffffffff);
    builder.label(takeTop(content,44),state.inputCapturePrompt,theme.color.text,theme.type.title,UiAlign::Center);
    builder.label(takeTop(content,30),state.inputCaptureFeedback,theme.color.textDim,theme.type.body,UiAlign::Center);
    builder.label(takeTop(content,34),current,theme.color.textMuted,theme.type.body,UiAlign::Center);
    builder.label(takeTop(content,28),"Escape ou Voltar cancela.",theme.color.textMuted,theme.type.caption,UiAlign::Center);
    return;
  }

  const bool wide=content.width>=700;
  if(wide) {
    auto rail=takeLeft(content,std::clamp(content.width*.27f,180.f,250.f));
    list.addRect(takeLeft(content,1),theme.color.line);takeLeft(content,12);
    auto heading=takeTop(rail,44),add=takeRight(heading,86);
    builder.label(heading,"AÇÕES",theme.color.textDim,theme.type.caption);
    button(add,"+ Ação",EditorWidget::InputActionAdd,false,actions.size()<runtime::InputActionMap::kMaximumActions);
    auto footer=takeBottom(rail,40);
    const u32 pageSize=std::max(1u,std::min(8u,static_cast<u32>(std::max(0.f,rail.height)/48)));
    const u32 pages=std::max(1u,(static_cast<u32>(actions.size())+pageSize-1)/pageSize),page=std::min(state.inputActionPage,pages-1);
    for(u32 i=page*pageSize;i<std::min(static_cast<u32>(actions.size()),(page+1)*pageSize);++i) {
      auto row=takeTop(rail,48),text=deflate(row,UiInsets::all(6));
      const bool selected=i==state.inputActionIndex;
      list.addRect(row,selected?theme.color.raised:theme.color.surface);
      if(selected)list.addRect({row.x,row.y,3,row.height},theme.color.accent);
      list.addImage(centred(takeLeft(text,34),26,26),static_cast<UiImageId>(UiIcon::InputAction),0xffffffff);
      builder.label(takeTop(text,20),actions[i].id,theme.color.text,theme.type.caption);
      const char*types[]={"Botão","Eixo 1D","Eixo 2D"};
      builder.label(text,types[static_cast<u32>(actions[i].kind)],theme.color.textMuted,theme.type.caption);
      router.addRegion(row,widgetId(EditorWidget::InputActionRowBase)+i,theme.touch.minimumTarget);
    }
    if(pages>1) {
      auto previous=takeLeft(footer,42),next=takeRight(footer,42);
      button(previous,"<",EditorWidget::InputActionPagePrevious,false,page>0);
      button(next,">",EditorWidget::InputActionPageNext,false,page+1<pages);
      builder.label(footer,std::to_string(page+1)+"/"+std::to_string(pages),theme.color.textMuted,theme.type.caption,UiAlign::Center);
    } else builder.label(footer,std::to_string(actions.size())+" / 64 ações",theme.color.textMuted,theme.type.caption,UiAlign::Center);
  }
  auto title=takeTop(content,44);
  if(!wide) {
    button(takeLeft(title,40),"<",EditorWidget::InputActionPrevious,false,!actions.empty());
    button(takeRight(title,40),">",EditorWidget::InputActionNext,false,!actions.empty());
    button(takeRight(title,66),"+ Ação",EditorWidget::InputActionAdd,false,actions.size()<runtime::InputActionMap::kMaximumActions);
  } else list.addImage(centred(takeLeft(title,36),28,28),static_cast<UiImageId>(UiIcon::InputAction),0xffffffff);
  if(actions.empty()){builder.label(title,"Crie uma ação",theme.color.text,theme.type.body);return;}
  const auto &action=actions[std::min(state.inputActionIndex,static_cast<u32>(actions.size()-1))];
  builder.label(title,action.id,theme.color.text,theme.type.cardName);
  auto tabs=takeTop(content,44);const float tabWidth=tabs.width/3;
  button(takeLeft(tabs,tabWidth),"Ação",EditorWidget::InputTabAction,state.inputTab==0);
  button(takeLeft(tabs,tabWidth),"Vínculos",EditorWidget::InputTabBinding,state.inputTab==1);
  button(tabs,"Resposta",EditorWidget::InputTabResponse,state.inputTab==2);
  takeTop(content,6);
  if(state.inputTab==2) {
    auto enabled=takeTop(content,40),icon=takeRight(enabled,40);
    list.addImage(centred(icon,28,28),static_cast<UiImageId>(UiIcon::InputResponse),0xffffffff);
    field(enabled,"Habilitada",action.enabled?"Sim":"Não",EditorWidget::InputActionEnabled);
    if(action.kind==runtime::ActionKind::Button) {
      auto choice=takeTop(content,44);const float w=choice.width/3;
      button(takeLeft(choice,w),"Pressionar",EditorWidget::InputInteractionPress,action.interaction==runtime::InputInteraction::Press);
      button(takeLeft(choice,w),"Segurar",EditorWidget::InputInteractionHold,action.interaction==runtime::InputInteraction::Hold);
      button(choice,"Tocar",EditorWidget::InputInteractionTap,action.interaction==runtime::InputInteraction::Tap);
      if(action.interaction!=runtime::InputInteraction::Press) {
        char duration[40];std::snprintf(duration,sizeof(duration),"%.3g s",static_cast<double>(action.duration));
        field(content,action.interaction==runtime::InputInteraction::Hold?"Tempo mínimo":"Prazo máximo",duration,EditorWidget::InputDuration);
      }
    }
    builder.label(takeTop(content,24),"GRUPOS DE DISPOSITIVOS",theme.color.textDim,theme.type.caption);
    auto groups=takeTop(content,44);const float w=groups.width/3;
    button(takeLeft(groups,w),"Touch",EditorWidget::InputGroupTouch,(action.deviceGroups&runtime::InputTouch)!=0);
    button(takeLeft(groups,w),"Teclado / mouse",EditorWidget::InputGroupKeyboardMouse,(action.deviceGroups&runtime::InputKeyboardMouse)!=0);
    button(groups,"Gamepad",EditorWidget::InputGroupGamepad,(action.deviceGroups&runtime::InputGamepad)!=0);
    const char*note=action.deviceGroups==0?"Nenhuma fonte autorizada para esta ação":
      action.interaction==runtime::InputInteraction::Hold?"Executa após o tempo mínimo pressionado":
      action.interaction==runtime::InputInteraction::Tap?"Executa ao soltar dentro do prazo":"Responde enquanto o controle estiver ativo";
    builder.label(takeTop(content,24),note,theme.color.textMuted,theme.type.caption);
    return;
  }
  if(state.inputTab==0) {
    auto primary=content,secondary=content;const bool columns=wide;
    if(columns){primary=takeLeft(content,content.width*.52f);takeLeft(content,8);secondary=content;}
    const char*types[]={"Botão","Eixo 1D","Eixo 2D"};
    field(primary,"Tipo",types[static_cast<u32>(action.kind)],EditorWidget::InputActionKind);
    field(primary,"Nome",action.id,EditorWidget::InputActionRename);
    field(primary,"Contexto",action.context.empty()?"Sempre ativo":action.context,EditorWidget::InputActionContext);
    if(!columns)secondary=primary;
    if(action.kind!=runtime::ActionKind::Button) {
      char number[40];std::snprintf(number,sizeof(number),"%.3g",static_cast<double>(action.deadzone));
      field(secondary,"Zona morta",number,EditorWidget::InputDeadzone);
      std::snprintf(number,sizeof(number),"%.3g",static_cast<double>(action.sensitivity));
      field(secondary,"Sensibilidade",number,EditorWidget::InputSensitivity);
    }
    auto roles=takeTop(secondary,44);const float w=roles.width/3;const auto&map=state.document->inputActions();
    button(takeLeft(roles,w),"Mover",EditorWidget::InputRoleMove,map.moveAction()==action.id,action.kind==runtime::ActionKind::Axis2D);
    button(takeLeft(roles,w),"Olhar",EditorWidget::InputRoleLook,map.lookAction()==action.id,action.kind==runtime::ActionKind::Axis2D);
    button(roles,"Saltar",EditorWidget::InputRoleJump,map.jumpAction()==action.id,action.kind==runtime::ActionKind::Button);
    button(takeTop(columns?primary:secondary,44),"Remover ação",EditorWidget::InputActionRemove);
    return;
  }
  auto navigation=takeTop(content,44),previous=takeLeft(navigation,40),next=takeRight(navigation,40),add=takeRight(navigation,100);
  button(previous,"<",EditorWidget::InputBindingPrevious,false,!action.bindings.empty());
  button(next,">",EditorWidget::InputBindingNext,false,!action.bindings.empty());
  button(add,"+ Vínculo",EditorWidget::InputBindingAdd,false,action.bindings.size()<runtime::InputActionMap::kMaximumBindings);
  const u32 index=action.bindings.empty()?0:std::min(state.inputBindingIndex,static_cast<u32>(action.bindings.size()-1));
  builder.label(navigation,action.bindings.empty()?"Nenhum vínculo":std::to_string(index+1)+"/"+std::to_string(action.bindings.size()),theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(action.bindings.empty())return;
  const auto&binding=action.bindings[index];auto primary=content,secondary=content;
  if(wide){primary=takeLeft(content,content.width*.58f);takeLeft(content,8);secondary=content;}
  constexpr const char*sources[]={"Nenhuma","Manche touch","Olhar touch","Botão touch","Tecla","Eixo gamepad","Botão gamepad","Botão mouse","Eixo mouse"};
  const bool capture=binding.source==runtime::InputSource::Key||binding.source==runtime::InputSource::GamepadButton||binding.source==runtime::InputSource::GamepadAxis||binding.source==runtime::InputSource::MouseButton;
  if(capture)captureField(primary,sources[static_cast<u32>(binding.source)],EditorWidget::InputBindingSource,EditorWidget::InputBindingCapture);
  else field(primary,"Fonte",sources[static_cast<u32>(binding.source)],EditorWidget::InputBindingSource);
  if(!state.inputDetails) {
    if(binding.source==runtime::InputSource::MouseButton||binding.source==runtime::InputSource::MouseAxis)
      field(primary,binding.source==runtime::InputSource::MouseAxis?"Canal":"Botão",inputBindingCodeLabel(binding),EditorWidget::InputBindingCode);
    if(action.kind==runtime::ActionKind::Axis2D)field(primary,"Eixo",binding.axis?"Y":"X",EditorWidget::InputBindingAxis);
    if(action.kind!=runtime::ActionKind::Button)field(primary,"Inverter",binding.invert?"Sim":"Não",EditorWidget::InputBindingInvert);
  } else {
    if(binding.source==runtime::InputSource::Key||binding.source==runtime::InputSource::TouchButton||capture||binding.source==runtime::InputSource::MouseAxis)
      field(primary,binding.source==runtime::InputSource::MouseAxis?"Canal":"Código",inputBindingCodeLabel(binding),EditorWidget::InputBindingCode);
    if(binding.source==runtime::InputSource::Key&&action.kind!=runtime::ActionKind::Button)
      captureField(primary,"Negativo: "+std::to_string(binding.negativeCode),EditorWidget::InputBindingNegativeCode,EditorWidget::InputBindingCaptureNegative);
    char number[40];std::snprintf(number,sizeof(number),"%.3g",static_cast<double>(binding.scale));
    field(primary,"Escala",number,EditorWidget::InputBindingScale);
  }
  if(!wide)secondary=primary;
  button(takeTop(secondary,44),state.inputDetails?"< Básico":"Avançado >",EditorWidget::InputDetailsToggle);
  button(takeTop(secondary,44),"Remover vínculo",EditorWidget::InputBindingRemove);

}

// Configurações do projeto: uma workspace, seções à esquerda.
//
// Camadas de colisão, mapa de entrada e água da cena são dados do PROJETO, não
// de um objeto nem de um componente — como Project Settings na Unity. Antes cada
// um tinha a própria workspace no menu Cena; agora são seções da mesma tela,
// com resumo do que contêm, e novas configurações (tempo, áudio, idiomas)
// entram como seção, nunca como workspace nova.
void buildProjectSettings(ScreenBuilder &builder,UiRect area) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
  list.addRect(area,theme.color.canvas);
  auto content=deflate(area,UiInsets::all(10));
  // Rótulos no trilho só quando sobra: no telefone cada dp vai para a seção.
  const bool wide=content.width>=1000;
  auto rail=takeLeft(content,wide?210.f:52.f);takeLeft(content,10);
  struct Section {EditorProjectSection id;UiIcon icon;const char *name;std::string summary;};
  u32 layers=0;for(u32 i=0;i<runtime::GameplayLayers::kCount;++i) layers+=state.document->layers().named(i)?1u:0u;
  std::vector<Section> sections{
    {EditorProjectSection::Layers,UiIcon::SceneLayers,"Camadas e colisão",std::to_string(layers)+" camada(s)"},
    {EditorProjectSection::Input,UiIcon::InputAction,"Entrada",std::to_string(state.document->inputActions().actions().size())+" ação(ões)"},
    {EditorProjectSection::Tags,UiIcon::SceneTag,"Tags",std::to_string(state.document->tags().names().size()-1)+" no projeto"}};
  if(waterCreationAvailable(state)) sections.push_back({EditorProjectSection::Water,UiIcon::NatureWater,"Água da cena","Superfícies e ondas"});
  if(wide) builder.label(takeTop(rail,30),"PROJETO",theme.color.textMuted,theme.type.label);
  for(const auto &section:sections) {
    if(rail.height<48) break;
    const auto cell=deflate(takeTop(rail,wide?58.f:50.f),UiInsets{0,2,0,2});
    const bool on=state.projectSection==section.id;
    list.addRect(cell,on?theme.color.raised:theme.color.surface,theme.radius.control);
    if(on) list.addRect({cell.x,cell.y+8,3,cell.height-16},theme.color.accent,1.5f);
    auto text=cell;
    auto icon=takeLeft(text,wide?44.f:text.width);
    list.addImage(centred(icon,22,22),static_cast<UiImageId>(section.icon),on?theme.color.accent:theme.color.textDim);
    if(wide) {
      builder.label(takeTop(text,text.height*.52f),section.name,on?theme.color.text:theme.color.textDim,theme.type.body);
      builder.label(text,section.summary,theme.color.textMuted,theme.type.caption);
    }
    router.addRegion(cell,widgetId(EditorWidget::ProjectSectionBase)+static_cast<u32>(section.id));
  }
  switch(state.projectSection) {
    case EditorProjectSection::Tags: buildTags(builder,content,false);break;
    case EditorProjectSection::Layers: buildPhysicsLayers(builder,content);break;
    case EditorProjectSection::Input: buildInputWorkspaceFocused(builder,content);break;
    case EditorProjectSection::Water: {
      list.addRect(content,theme.color.surface,theme.radius.card);
      auto page=deflate(content,UiInsets::all(10));
      builder.label(takeTop(page,36),"Água da cena",theme.color.text,theme.type.cardName);
      buildPropertyPage(builder,page,*state.document->find(state.document->root()),EditorPropertyGroup::Water);
      break;
    }
  }
}


void buildWorkspaceMenu(ScreenBuilder &builder) {
  const auto &state=builder.state;const auto &theme=builder.theme;
  auto &list=builder.list;auto &router=builder.router;
  if(state.workspaceMenu) {
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.65f));router.addBlocker(state.surface);
    const u32 menuRows=6+(state.assetCount?1:0);
    const UiRect modal=centred(state.surface, std::min(340.0f,state.surface.width-24), 48.0f+44.0f*menuRows);
    list.addRect(modal,theme.color.surface,8);auto content=deflate(modal,UiInsets::all(12));
    const char *names[]{"Voltar à edição","Recursos importados","Ambiente da cena","Configurações do projeto","Interface (UI + ImGui)","Layout dos painéis","Fechar"};
    const EditorWidget actions[]{EditorWidget::TabScene,EditorWidget::TabAssets,EditorWidget::TabLighting,EditorWidget::TabProject,EditorWidget::TabGui,EditorWidget::LayoutsOpen,EditorWidget::WorkspaceMenuClose};
    builder.label(takeTop(content,24),"Cena",theme.color.text,theme.type.cardName);
    for(u32 i=0;i<std::size(actions);++i) {
      if(actions[i]==EditorWidget::TabAssets && !state.assetCount) continue;
      auto row=takeTop(content,44);
      if(actions[i]==EditorWidget::TabGui) {
        builder.list.addImage(centred(takeLeft(row,32),24,24),static_cast<UiImageId>(UiIcon::UiInterfaceCanvas));
      }
      builder.label(row,names[i],theme.color.text,theme.type.body);
      // The full menu line is the target, including its icon.
      router.addRegion({content.x,row.y,content.width,row.height},widgetId(actions[i]));
    }
  }
}

EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const UiTheme &theme,
                                     UiDrawList &list, UiInputRouter &router) {
  EditorScreenLayout layout{};
  if (state.document == nullptr || state.surface.isEmpty()) return layout;

  ScreenBuilder builder{state, theme, list, router};
  builder.visibleSourceTextureRows=&layout.visibleSourceTextureRows;
  builder.layout=&layout;
  UiRect remaining = deflate(state.surface, state.safeArea);
  layout.topBar = takeTop(remaining, kTopBarHeight);
  if(state.workspace==EditorWorkspace::Gui) {
    buildTopBar(builder,layout.topBar);
    layout.viewport=remaining;router.addBlocker(remaining);list.addRect(remaining,theme.color.canvas);
    buildWorkspaceMenu(builder);
    return layout;
  }
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

  // No Play os painéis só existem quando o usuário pede para inspecionar.
  const bool panelsAllowed = state.workspace != EditorWorkspace::Project &&
      (state.workspace != EditorWorkspace::Play || state.playInspect);
  const auto *inspectionObject=state.document->find(state.inspectorLocked?state.inspectorLocked:state.inspectorTarget?state.inspectorTarget:state.selection);
  const auto *inspectionValue=inspectionObject?inspectionObject->components.findInstance(state.expandedNative):nullptr;
  const bool mechanismPortrait=compact && available<600.f && state.surface.height>state.surface.width &&
      panelsAllowed && state.compactPanel==EditorScreenState::CompactPanel::Inspector && inspectionValue &&
      (&inspectionValue->type()==&scene::Joint::descriptor ||
       (&inspectionValue->type()==&scene::Collider::descriptor &&
        static_cast<const scene::Collider&>(*inspectionValue).shape==scene::ColliderShape::Cylinder));
  float hierarchyWidth = state.hierarchyVisible && panelsAllowed
      ? (state.hierarchyWidth > 0.0f ? state.hierarchyWidth : available * 0.24f)
      : 0.0f;
  float inspectorWidth = state.inspectorVisible && panelsAllowed
      ? (state.inspectorWidth > 0.0f ? state.inspectorWidth : available * 0.27f)
      : 0.0f;
  if (hierarchyWidth > 0.0f) hierarchyWidth = std::max(hierarchyWidth, kPanelMinimum);
  if (inspectorWidth > 0.0f) inspectorWidth = std::max(inspectorWidth, kPanelMinimum);
  if (compact) {
    hierarchyWidth = panelsAllowed &&
        (state.compactPanel == EditorScreenState::CompactPanel::Hierarchy || state.compactPanel == EditorScreenState::CompactPanel::Files)
        ? std::min(compactPanelWidth, std::max(0.0f, available-kViewportMinimum-kSplitterWidth)) : 0;
    inspectorWidth = panelsAllowed &&
        state.compactPanel == EditorScreenState::CompactPanel::Inspector
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
  // A barra de status fica na coluna central, sob o viewport: numa tela de
  // telefone deitado, atravessar a largura toda tiraria uma linha da Hierarquia
  // e do Inspector. Só onde o editor mostra seus painéis (Cena e Play
  // inspecionado); o jogo em Play ocupa o viewport inteiro.
  if(state.workspace==EditorWorkspace::Scene || (state.workspace==EditorWorkspace::Play && state.playInspect)) {
    layout.statusBar = takeBottom(body, kStatusBarHeight);
    buildStatusBar(builder, layout.statusBar);
  }
  if(state.workspace==EditorWorkspace::Scene && state.diagnosticDockOpen) {
    if(compact || body.height<310.0f) {
      const float height=std::min(212.0f,remaining.height*.62f);
      layout.diagnosticDock={remaining.x,remaining.bottom()-height,remaining.width,height};
    } else {
      layout.diagnosticDock=takeBottom(body,std::min(212.0f,body.height*.32f));
    }
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
    const auto *controlled=state.document?state.document->find(state.selection):nullptr;
    const bool hasSecondary=!state.playSecondaryActionLabel.empty();
    const bool showJump=hasSecondary?state.playHasCharacter:
        (controlled&&characterComponent(*controlled)&&characterComponent(*controlled)->jumpSpeed>0);
    if(showJump) {
      const UiRect jump{layout.viewport.x+layout.viewport.width-(hasSecondary?208.0f:108.0f),layout.viewport.y+layout.viewport.height-76,92,56};
      list.addRect(jump,theme.color.raised,theme.radius.control);
      builder.label(jump,"Saltar",state.playPaused?theme.color.textFaint:theme.color.text,theme.type.body,UiAlign::Center);
      if(!state.playPaused) router.addRegion(jump,widgetId(EditorWidget::JumpCharacter),theme.touch.minimumTarget);
    }
    if(hasSecondary) {
      const UiRect action{layout.viewport.x+layout.viewport.width-108,layout.viewport.y+layout.viewport.height-76,92,56};
      list.addRect(action,theme.color.raised,theme.radius.control);
      builder.label(action,state.playSecondaryActionLabel,state.playPaused?theme.color.textFaint:theme.color.text,theme.type.body,UiAlign::Center);
      if(!state.playPaused) router.addRegion(action,widgetId(EditorWidget::PlaySecondaryAction),theme.touch.minimumTarget);
    }
    buildPlayDebugLines(builder);
    if(state.playFirstPerson) {
      const float cx=layout.viewport.x+layout.viewport.width*.5f,cy=layout.viewport.y+layout.viewport.height*.5f;
      list.addRect({cx-1,cy-9,2,18},theme.color.accent);
      list.addRect({cx-9,cy-1,18,2},theme.color.accent);
    }
    if(!state.playHudMessage.empty()) {
      const float width=std::max(120.0f,std::min(510.0f,layout.viewport.width-(hasSecondary?222.0f:122.0f)));
      const UiRect status{layout.viewport.x+8,layout.viewport.y+layout.viewport.height-62,width,40};
      list.addRect(status,withAlpha(theme.color.surface,.92f),theme.radius.control);
      builder.label(deflate(status,UiInsets::symmetric(8,4)),state.playHudMessage,theme.color.text,theme.type.caption);
    }
    if(!state.playInspect) return layout;
  }
  const bool playing=state.workspace==EditorWorkspace::Play;
  if(!playing) buildViewportOverlay(builder, layout.viewport);
  else if(state.physicsDiagnosticOpen) {
    list.pushClip(layout.viewport);
    buildPhysicsDiagnosticOverlay(builder,true);
    list.popClip();
  }
  // Tinta de Play (Unity: Play Mode tint): os painéis mostram o mundo vivo, e
  // isso fica visível sem ler nada. A faixa do Inspector diz o que acontece
  // com a edição e, quando o mundo recusa, o motivo.
  if(playing) {
    if(!layout.hierarchyPanel.isEmpty())
      list.addRect({layout.hierarchyPanel.x,layout.hierarchyPanel.y,layout.hierarchyPanel.width,2},theme.color.accent);
    if(!layout.inspectorPanel.isEmpty()) {
      const UiRect banner=takeTop(layout.inspectorPanel,34);
      const UiColor tone=state.playEditRefused?theme.color.warning:theme.color.accent;
      list.addRect(banner,withAlpha(tone,.16f));
      list.addRect({banner.x,banner.y,banner.width,2},tone);
      UiRect content=deflate(banner,UiInsets{8,2,8,0});
      list.addImage(centred(takeLeft(content,20),14,14),static_cast<UiImageId>(UiIcon::EditorAuthorPlay),tone);
      takeLeft(content,6);
      builder.label(content,state.playEditNote.empty()?"Em execução · edições voltam ao parar":state.playEditNote,
                    state.playEditRefused?theme.color.warning:theme.color.text,theme.type.caption);
      router.addBlocker(banner);
    }
  }
  if (state.workspace == EditorWorkspace::Scene) buildToolRail(builder, layout.viewport, compact);
  if (!layout.hierarchyPanel.isEmpty())
    layout.hierarchyRowCount =
        buildHierarchy(builder, layout.hierarchyPanel, layout.hierarchyVisibleRows);
  if(!layout.filesPanel.isEmpty()) buildFiles(builder,layout.filesPanel);
  if (!layout.inspectorPanel.isEmpty() && !mechanismPortrait) buildInspector(builder, layout.inspectorPanel);
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
  // Inspecionando o Play: o viewport é do jogo (toques, botões de ação). Os
  // controles de câmera do editor e as vistas salvas ficam de fora; a barra de
  // estado vai para a faixa de Play no topo do Inspector.
  if(playing) {
    if(compact) {
      const UiRect button{layout.viewport.x+8,layout.viewport.y+42,100,36};
      list.addRect(button,theme.color.raised,theme.radius.control);
      builder.label(button,"Painéis",theme.color.text,theme.type.caption,UiAlign::Center);
      router.addRegion(button,widgetId(EditorWidget::CompactPanelMenu));
    }
  }
  if(!playing) {
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
  if(layout.viewport.width>=324.f) {
  builder.iconButton({layout.viewport.x + 228.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::SceneLayers, widgetId(EditorWidget::ViewsOpen), state.viewsPanel);
  // Unity 6 põe as camadas visíveis nas View Options do Scene view; o olho
  // aceso indica que alguma está escondida ou fora da seleção.
  builder.iconButton({layout.viewport.x + 276.0f, layout.viewport.y + 8.0f, 40.0f, 40.0f},
                     UiIcon::SceneVisibility,
                     widgetId(EditorWidget::SceneLayersOpen), state.sceneLayersPanel || state.hiddenLayers || state.unpickableLayers);
  }
  if(state.viewsPanel && state.document) buildSceneViewsPanel(builder, layout.viewport);
  if(state.templatePanel) buildSceneTemplatePanel(builder, layout.viewport);
  if(compact) {
    const float panelX=std::min(104.f,std::max(8.f,layout.viewport.width-48.f));
    const UiRect button{layout.viewport.x+panelX,layout.viewport.y+56,std::min(100.f,layout.viewport.width-panelX-8.f),40};
    list.addRect(button,theme.color.raised,theme.radius.control);
    builder.label(button,"Painéis",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(button,widgetId(EditorWidget::CompactPanelMenu));
  }
  }
  if(state.workspace==EditorWorkspace::Scene) {
    // Unity 6 agrupa Lighting e Effects no topo do Scene View. Aqui a mesma
    // organização controla o renderer editorial real; a seta abre as partes
    // que a Astra já consome (céu, neblina e pós).
    // Secondary scene options belong to the full viewport. An authoring panel
    // must never be painted over or receive their hit regions in portrait.
    if(layout.viewport.width>=324.f) {
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
    const bool narrowViewport=compact && layout.viewport.width<324.f;
    const float width=std::min(64.0f,(layout.viewport.width-(compact&&!narrowViewport?112:16))/3);
    for(u32 i=0;i<3;++i) {
      const UiRect cell{layout.viewport.x+8+i*width,layout.viewport.bottom()-(narrowViewport?88:compact?44:76),width-2,narrowViewport?40.f:36.f};
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
  // S5: estatísticas do quadro, como a janela Statistics do Game View da Unity.
  // Só leitura e sem região de toque: gestos da cena passam por baixo.
  if(state.sceneStatisticsVisible && state.workspace==EditorWorkspace::Scene && !layout.viewport.isEmpty()) {
    const auto &s=state.sceneStatistics;
    const auto millions=[](u64 value) {
      return value>=1000000?decimalText(static_cast<double>(value)/1e6,2)+" M":
             value>=1000?decimalText(static_cast<double>(value)/1e3,1)+" mil":std::to_string(value);
    };
    std::array<std::string,4> lines;
    lines[0]="Quadro "+(s.frameIntervalMs>0?decimalText(s.frameIntervalMs,1)+" ms · "+
                         std::to_string(static_cast<u32>(1000.0f/s.frameIntervalMs+.5f))+" fps":std::string("—"))+
             " · GPU "+(s.gpuFrameMs>0?decimalText(s.gpuFrameMs,1)+" ms":std::string("não medida"));
    lines[1]=std::to_string(s.renderWidth)+"×"+std::to_string(s.renderHeight)+" · "+std::to_string(s.drawCalls)+
             " desenhos · "+millions(s.triangles)+" triângulos";
    lines[2]=s.lodDraws?"LOD "+std::to_string(s.lodReducedDraws)+"/"+std::to_string(s.lodDraws)+" reduzidos · "+
             millions(s.lodBaseTriangles)+" → "+millions(s.lodSelectedTriangles):std::string("LOD: sem níveis na cena");
    const auto &t=state.qualityTextureStreaming;
    lines[3]=std::string("Texturas ")+decimalText(static_cast<double>(t.currentBytes)/1048576.0,0)+" MB"+
             (t.active?" de "+decimalText(static_cast<double>(t.budgetBytes)/1048576.0,0)+" MB":std::string(" · sem streaming"));
    const float width=std::min(330.0f,layout.viewport.width-16.0f);
    const UiRect panel{layout.viewport.right()-width-8.0f,layout.viewport.y+56.0f,width,8.0f+lines.size()*19.0f};
    builder.list.addRect(panel,theme.color.surface,theme.radius.control);
    UiRect text=deflate(panel,UiInsets::all(4));
    for(const auto &line:lines) builder.label(takeTop(text,19),line,theme.color.text,theme.type.caption);
  }
  // Painel global de gráficos fica acima de todas as ferramentas do viewport.
  // O roteador resolve da última região para a primeira, então a mesma ordem
  // também impede Lighting/Effects e a navegação inferior de roubarem toques.
  // Menus contextuais e modais abaixo continuam acima dele deliberadamente.
  if(state.qualityPanel) buildQualityPanel(builder,layout.viewport);
  if(state.lightExplorer) buildLightExplorer(builder,layout.viewport);
  if (state.entityMenu) {
    const auto *selected=state.document?state.document->find(state.selection):nullptr;
    const bool convert=selected&&runtime::meshRenderer(*selected)&&state.workspace==EditorWorkspace::Scene;
    const UiRect menu{layout.viewport.x, layout.viewport.y+56.0f, std::min(240.0f,layout.viewport.width), std::min(convert?370.0f:333.0f,layout.viewport.height-56.0f)};
    builder.list.addRect(menu, theme.color.raised, theme.radius.control);
    UiRect rows=menu;
    const char *names[]={"Duplicar selecionado","Criar grupo","Excluir selecionado","Mover acima","Mover abaixo","Mudar pai","Mover para raiz","Renomear","Propriedades"};
    const EditorWidget actions[]={EditorWidget::DuplicateSelection,EditorWidget::CreateGroup,EditorWidget::DeleteSelection,EditorWidget::MoveEarlier,EditorWidget::MoveLater,EditorWidget::ReparentSelection,EditorWidget::MoveToRoot,EditorWidget::RenameSelection,EditorWidget::HierarchyProperties};
    for(u32 i=0;i<9;++i) {
      const auto row=takeTop(rows,menu.height/(convert?10.f:9.f));
      builder.label(row,names[i],theme.color.text,theme.type.body,UiAlign::Center);
      router.addRegion(row,widgetId(actions[i]));
    }
    if(convert) {
      builder.label(rows,"Criar raiz Character…",theme.color.accent,theme.type.caption,UiAlign::Center);
      router.addRegion(rows,widgetId(EditorWidget::CharacterConversionOpen));
    }
  }
  // Janelas que não bloqueiam o editor (Inspectors focados, histórico, camadas
  // na vista) ficam sobre os painéis e SOB tudo que é modal: busca, teclados,
  // menus, folhas de Adicionar e Criar e as janelas de valor. Desenhadas por
  // último, cobriam o teclado numérico aberto por um campo delas e a busca.
  if(state.workspace==EditorWorkspace::Scene || (state.workspace==EditorWorkspace::Play && state.playInspect))
    buildFocusedInspectors(builder,layout);
  if(state.undoHistory) buildUndoHistory(builder,layout);
  if(state.sceneLayersPanel && state.document && state.workspace==EditorWorkspace::Scene)
    buildSceneLayersPanel(builder,layout.viewport);
  // "Definir como o valor de…" (Unity: Set to Value of): um objeto por linha,
  // com o valor que ele tem; tocar copia para todos os selecionados.
  if(!state.setValueMenu.key.empty()) {
    router.addBlocker(state.surface);
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,.55f));
    const u32 count=static_cast<u32>(state.setValueMenu.rows.size());
    const float width=std::min(380.f,state.surface.width-16);
    const float height=std::min(state.surface.height-16,56.f+std::min(count,7u)*40.f);
    const UiRect menu=centred(state.surface,width,height);
    list.addRect(menu,theme.color.surface,theme.radius.control);
    auto content=deflate(menu,UiInsets::all(8));
    auto header=takeTop(content,40);
    builder.iconButton(takeRight(header,36),UiIcon::UiClose,widgetId(EditorWidget::SetValueClose));
    builder.label({header.x,header.y,header.width,header.height*.5f},"Definir como o valor de…",theme.color.text,theme.type.cardName);
    builder.label({header.x,header.y+header.height*.5f,header.width,header.height*.5f},state.setValueMenu.label.c_str(),theme.color.accent,theme.type.caption);
    for(u32 i=0;i<count && content.height>=38;++i) {
      auto row=takeTop(content,40);row.height-=4;
      const bool active=state.setValueMenu.rows[i].first==state.selection;
      list.addRect(row,active?withAlpha(theme.color.accent,.16f):theme.color.raised,theme.radius.control);
      list.pushClip(row);
      builder.label(deflate(row,UiInsets{10,0,8,0}),state.setValueMenu.rows[i].second.c_str(),theme.color.text,theme.type.caption);
      list.popClip();
      router.addRegion(row,widgetId(EditorWidget::SetValueRowBase)+i);
    }
  }
  // A busca global fica sob o teclado interno, que edita o campo dela.
  if(state.globalSearch) buildGlobalSearch(builder);
  if (!state.platformTextInput && (state.renameEntity != kInvalidEntity || state.editingHierarchySearch || state.editingCreationSearch || state.editingComponentSearch || state.editingPropertySearch || state.editingMeshSearch || state.editingReferenceSearch || state.editingGlobalSearch || state.namingLayout || state.presetNaming || state.viewNaming || state.editingInputActionName || state.editingInputContext || state.editingTagName || state.editingTagSearch || state.editingPhysicsLayerName)) {
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
  if(state.componentReorder && state.document) {
    const auto *entity=state.document->find(state.selection);
    const auto *value=entity&&state.componentReorder<=entity->components.size()?entity->components.at(state.componentReorder-1):nullptr;
    if(value) {
      const auto *schema=scene::findComponentSchema(value->type().id);
      const auto *script=scene::scriptBehavior(value);
      const auto *scriptType=script?scriptSchema(state,script->scriptType):nullptr;
      const std::string name=scriptType?scriptType->name:script?script->scriptType:schema?schema->name:std::string(value->type().id);
      // Ao lado do dedo, do lado do viewport: por cima dos cartões ele
      // esconderia justamente o cabeçalho de destino.
      UiRect ghost{std::clamp(state.componentReorderPoint.x-244,state.surface.x,std::max(state.surface.x,state.surface.right()-220)),
                   std::clamp(state.componentReorderPoint.y-22,state.surface.y,std::max(state.surface.y,state.surface.bottom()-44)),220,44};
      list.addRect({ghost.x-1,ghost.y-1,ghost.width+2,ghost.height+2},theme.color.accent,theme.radius.control);
      list.addRect(ghost,theme.color.raised,theme.radius.control);
      list.addRect({ghost.x,ghost.y,3,ghost.height},theme.color.accent,1);
      UiRect inner=deflate(ghost,UiInsets{10,0,8,0});
      list.addImage(centred(takeLeft(inner,28),22,22),
                    static_cast<UiImageId>(schema&&!script?editorIconByName(schema->icon):UiIcon::ScriptingCode),0xffffffff);
      takeLeft(inner,6);
      builder.label(takeTop(inner,24),name,theme.color.text,theme.type.body);
      builder.label(inner,state.componentReorderTarget&&state.componentReorderTarget!=state.componentReorder?
                    "Solte para mover aqui":"Solte sobre outro cabeçalho",theme.color.textMuted,theme.type.caption);
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
  // Prévia ampliada da textura: por cima de tudo que a viewport desenha.
  if(state.textureViewerExpanded && state.textureViewer && !layout.viewport.isEmpty())
    buildTextureExpanded(builder,layout.viewport);
  if(!layout.diagnosticDock.isEmpty()) {
    buildDiagnosticDock(builder,layout.diagnosticDock);
    buildDiagnosticDetail(builder);
  }
  if(state.workspace==EditorWorkspace::Project && !layout.viewport.isEmpty()) {
    router.addBlocker(layout.viewport);
    buildProjectSettings(builder,layout.viewport);
  }
  // Reading differences needs width. This temporary route leaves the saved
  // panel sizes and scene camera untouched; returning restores the same layout.
  const auto prefabTarget=state.inspectorLocked?state.inspectorLocked:state.selection;
  if((compact || layout.inspectorPanel.width<380 || layout.inspectorPanel.height<520) &&
     state.workspace==EditorWorkspace::Scene && state.prefabOverridesOpen &&
     state.prefabOverrides.object==prefabTarget && state.multi.count<=1 && state.document->exists(prefabTarget)) {
    auto surface=remaining;
    list.addRect(surface,theme.color.surface);router.addBlocker(surface);
    auto status=takeBottom(surface,kStatusBarHeight);buildStatusBar(builder,status);
    auto page=deflate(surface,UiInsets::all(12));
    buildPrefabOverrides(builder,page);
  }
  const bool recipeExpanded=(compact || layout.inspectorPanel.height<520) &&
      state.workspace==EditorWorkspace::Scene && state.presetPanel && state.presetSelectedIsRecipe &&
      state.presetEntity==prefabTarget && state.multi.count<=1 && state.document->exists(prefabTarget);
  if(recipeExpanded) {
    // Preparation and its existing object picker temporarily share this route.
    // Saved panel geometry and scene camera remain owned by the scene workspace.
    router.addBlocker(remaining);list.addRect(remaining,theme.color.surface);
    auto page=deflate(remaining,UiInsets::all(12));
    buildComponents(builder,page,*state.document->find(prefabTarget));
  }
  buildWorkspaceMenu(builder);
  // A contextual surface, not a second Inspector implementation. Draw after
  // viewport tools and block their hit regions, but before property pickers and
  // keyboards. Keeping the underlying viewport valid avoids zero-size render
  // targets and lets the existing navigation return without losing camera state.
  if(mechanismPortrait && !recipeExpanded) {
    layout.inspectorPanel=remaining;
    router.addBlocker(remaining);list.addRect(remaining,theme.color.surface);
    auto content=remaining;auto navigation=takeTop(content,44);
    builder.label(deflate(navigation,UiInsets::symmetric(12,0)),"< Voltar ao viewport",theme.color.text,theme.type.body);
    router.addRegion(navigation,widgetId(EditorWidget::CompactViewport),theme.touch.minimumTarget);
    buildInspector(builder,content);layout.componentPage=builder.componentPage;
  }
  if(state.layoutsPanel) buildLayoutsPanel(builder);
  if(state.backgroundPanel) buildBackgroundTasks(builder,layout);
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
  // Os modais do Inspector pertencem ao alvo de quem os abriu (travado,
  // focado ou a seleção), não necessariamente à seleção atual.
  const EditorEntityId modalTarget=state.inspectorTarget?state.inspectorTarget:state.selection;
  if(state.addingComponent && state.componentSelection==modalTarget && !state.editingComponentSearch)
    if(const auto *entity=state.document->find(modalTarget)) buildComponentSheet(builder,*entity);
  if(state.creationMenu && !state.editingCreationSearch) buildCreationSheet(builder,layout);
  if(state.enumPicker) buildEnumPicker(builder);
  // O editor de curvas abre o teclado numérico para tempo e valor da chave:
  // desenhado antes, fica por baixo dele.
  if(state.curveField) buildCurveEditor(builder,layout);
  if (!state.platformTextInput && state.numericField != 0) {
    router.addBlocker(state.surface);
    list.addRect(state.surface,withAlpha(theme.color.voidBlack,0.8f));
    // Unity Manual/InspectorNumericFields: o campo aceita contas, edição
    // relativa (+=) e as distribuições L(a,b) e R(a,b). Os dígitos ficam onde
    // estavam; operadores numa coluna ao lado e as funções numa faixa acima,
    // com a prévia do resultado antes de aplicar.
    const bool triple=(state.numericField&0xff000000u)==widgetId(EditorWidget::ComponentTripleBase);
    const float width=std::min(triple?320.0f:440.0f,state.surface.width-16.0f);
    const float height=std::min(triple?320.0f:372.0f,state.surface.height-16.0f);
    const UiRect modal=centred(state.surface,width,height);
    list.addRect(modal,theme.color.surface,theme.radius.control);
    UiRect content=deflate(modal,UiInsets::all(8.0f));
    builder.label(takeTop(content,34.0f),state.numericText,theme.color.text,theme.type.numeric,UiAlign::Center);
    if(!triple) {
      double preview=0;std::string reason;
      NumericExpressionContext context;context.current=state.numericCurrent;
      const bool ok=state.numericText[0] && evaluateNumericExpression(state.numericText,context,preview,&reason);
      char line[96];
      if(ok) std::snprintf(line,sizeof(line),"= %.6g",preview);
      else std::snprintf(line,sizeof(line),"%s",state.numericError?"Valor inválido":state.numericText[0]?reason.c_str():" ");
      builder.label(takeTop(content,20.0f),line,ok?theme.color.accent:theme.color.warning,theme.type.caption,UiAlign::Center);
      takeTop(content,4.0f);
      auto chips=takeTop(content,30.0f);
      const struct {const char *label;u32 key;} functions[]{{"+=",21},{"-=",22},{"×=",23},{"÷=",24},{"L(a,b)",25},{"R(a,b)",26},{"raiz",27}};
      const float chipWidth=chips.width/std::size(functions);
      for(u32 i=0;i<std::size(functions);++i) {
        const UiRect cell{chips.x+i*chipWidth+2,chips.y,chipWidth-4,chips.height};
        list.addRect(cell,withAlpha(theme.color.accent,.16f),theme.radius.control);
        builder.label(cell,functions[i].label,theme.color.accent,theme.type.caption,UiAlign::Center);
        router.addRegion(cell,widgetId(EditorWidget::NumericKeyBase)+functions[i].key);
      }
      takeTop(content,6.0f);
    } else {
      if(state.numericError) builder.label(takeTop(content,20.0f),"Valor inválido",theme.color.warning,theme.type.caption,UiAlign::Center);
      const auto separator=takeTop(content,28);
      builder.label(separator,"Separar canais · espaço",theme.color.accent,theme.type.caption,UiAlign::Center);
      router.addRegion(separator,widgetId(EditorWidget::NumericKeyBase)+12);
    }
    const u32 columns=triple?4:6;
    const float cellWidth=content.width/columns,cellHeight=content.height/4.0f;
    // Chave: 0..11 dígitos, ponto e menos; 13.. operadores; 100+ ações.
    constexpr u32 backspace=100,clear=101,cancel=102,apply=103;
    const char *tripleLabels[16]={"1","2","3","Apagar","4","5","6","Limpar","7","8","9","Cancelar",".","0","-","Aplicar"};
    const u32 tripleKeys[16]={0,1,2,backspace,3,4,5,clear,6,7,8,cancel,9,10,11,apply};
    const char *labels[24]={"1","2","3","+","(","Apagar","4","5","6","×",")","Limpar","7","8","9","÷","^","Cancelar",".","0","-","pi",",","Aplicar"};
    const u32 keys[24]={0,1,2,13,16,backspace,3,4,5,14,17,clear,6,7,8,15,18,cancel,9,10,11,19,20,apply};
    for(u32 i=0;i<columns*4;++i) {
      const UiRect cell{content.x+(i%columns)*cellWidth+2,content.y+(i/columns)*cellHeight+2,cellWidth-4,cellHeight-4};
      const u32 key=triple?tripleKeys[i]:keys[i];
      const u32 widget=key<backspace ? widgetId(EditorWidget::NumericKeyBase)+key :
        key==backspace?widgetId(EditorWidget::NumericBackspace):key==clear?widgetId(EditorWidget::NumericClear):
        key==cancel?widgetId(EditorWidget::NumericCancel):widgetId(EditorWidget::NumericApply);
      const bool operatorKey=key>=13 && key<backspace;
      list.addRect(cell,key==apply?theme.color.accent:operatorKey?theme.color.silhouette:theme.color.raised,theme.radius.control);
      const char *label=triple?tripleLabels[i]:labels[i];
      builder.label(cell,label,key==apply?theme.color.accentInk:operatorKey?theme.color.accent:theme.color.text,
                    std::strlen(label)<=2?theme.type.title:theme.type.caption,UiAlign::Center);
      router.addRegion(cell,widget);
    }
  }
  if(state.pickerAdvanced && state.referenceInstance) buildAdvancedReferencePicker(builder);
  if(state.gradientField) buildGradientEditor(builder,layout);
  // Amostrando: a janela de cor some para a tela real ficar visível; só a faixa
  // de instrução (com Cancelar) fica por cima, no topo, longe do centro.
  if(state.colorField && state.colorPicking) {
    const float width=std::min(420.f,state.surface.width-16);
    const UiRect bar{state.surface.x+(state.surface.width-width)*.5f,state.surface.y+6,width,40};
    list.addRect(deflate(bar,UiInsets::all(-1)),theme.color.accent,theme.radius.control);
    list.addRect(bar,theme.color.surface,theme.radius.control);
    auto inner=deflate(bar,UiInsets{8,4,4,4});
    list.addImage(centred(takeLeft(inner,28),20,20),static_cast<UiImageId>(UiIcon::EditorEyedropper),theme.color.accent);
    const auto cancel=takeRight(inner,92);
    list.addRect(cancel,theme.color.raised,theme.radius.control);
    builder.label(cancel,"Cancelar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(cancel,widgetId(EditorWidget::ColorPickCancel));
    builder.label(deflate(inner,UiInsets{6,0,6,0}),state.colorSampling?"Lendo a cor da tela…":"Toque em qualquer ponto da tela",
                  theme.color.text,theme.type.caption);
  } else if(state.colorField) buildColorWindow(builder,layout);
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
       (widget >= widgetId(EditorWidget::HierarchyCollapseBase) && widget < widgetId(EditorWidget::ImpactOpenBase))) && routing.dragging) {
    state.hierarchyScrollRemainder -= routing.stepDelta.y;
    const int lines=static_cast<int>(state.hierarchyScrollRemainder/kRowHeight);
    state.hierarchyScrollRemainder-=static_cast<float>(lines)*kRowHeight;
    const int maximum=std::max(0,static_cast<int>(layout.hierarchyRowCount)-static_cast<int>(layout.hierarchyVisibleRows));
    state.hierarchyScroll=static_cast<u32>(std::clamp(static_cast<int>(state.hierarchyScroll)+lines,0,maximum));
    return outcome;
  }

  // A lista do Add rola pelo arraste sobre as linhas, como a Hierarquia; o
  // toque que não virou arraste continua abrindo a prévia do tipo.
  if(state.addingComponent && routing.dragging &&
     ((widget>=widgetId(EditorWidget::ComponentAddBase) && widget<widgetId(EditorWidget::ComponentAddBase)+detail::kRange) ||
      (widget>=widgetId(EditorWidget::ScriptAddBase) && widget<widgetId(EditorWidget::ScriptAddBase)+detail::kRange))) {
    constexpr float kCatalogRowHeight=52.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int lines=static_cast<int>(state.addScrollRemainder/kCatalogRowHeight);
    state.addScrollRemainder-=static_cast<float>(lines)*kCatalogRowHeight;
    const int maximum=std::max(0,static_cast<int>(layout.addRowCount)-static_cast<int>(layout.addVisibleRows));
    state.addScroll=static_cast<u32>(std::clamp(static_cast<int>(state.addScroll)+lines,0,maximum));
    return outcome;
  }
  if(state.creationMenu && routing.dragging && widget>=widgetId(EditorWidget::CreationRowBase) &&
     widget<widgetId(EditorWidget::CreationRowBase)+detail::kRange) {
    constexpr float kCardPitch=100.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int rows=static_cast<int>(state.addScrollRemainder/kCardPitch);
    state.addScrollRemainder-=static_cast<float>(rows)*kCardPitch;
    const int maximum=std::max(0,static_cast<int>(layout.creationRowCount)-static_cast<int>(layout.creationVisibleRows));
    state.creationScroll=static_cast<u32>(std::clamp(static_cast<int>(state.creationScroll)+rows,0,maximum));
    return outcome;
  }
  if(state.addingComponent && routing.dragging && widget>=widgetId(EditorWidget::ComponentFamilyBase) &&
     widget<widgetId(EditorWidget::ComponentFamilyBase)+detail::kRange) {
    constexpr float kRailCell=44.0f;
    state.addScrollRemainder-=routing.stepDelta.y;
    const int cells=static_cast<int>(state.addScrollRemainder/kRailCell);
    state.addScrollRemainder-=static_cast<float>(cells)*kRailCell;
    const int maximum=std::max(0,static_cast<int>(layout.addRailCount)-static_cast<int>(layout.addRailVisible));
    state.addRailScroll=static_cast<u32>(std::clamp(static_cast<int>(state.addRailScroll)+cells,0,maximum));
    return outcome;
  }

  // O resto age no toque completo, e não no `Down`: um dedo que desce num botão
  // e desliza para fora desistiu dele.
  if (!routing.tapped) return outcome;

  if(widget>=widgetId(EditorWidget::ComponentSlotEnumBase)&&widget<widgetId(EditorWidget::ComponentSlotEnumBase)+0x01000000u) {
    const auto *entity=document.find(state.selection);
    const u32 encoded=widget-widgetId(EditorWidget::ComponentSlotEnumBase);
    const u32 index=encoded&0xffu,field=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    if(!component||field>=component->type().slotEnums.size()) return outcome;
    const auto &property=component->type().slotEnums[field];
    if(!property.write||!property.read||slot>=property.slotCount(*component)||property.options.empty()||!property.presentation.isEditable(*component)) return outcome;
    usize selected=0;
    for(usize i=0;i<property.options.size();++i) if(property.options[i].value==property.read(*component,slot)) {selected=(i+1)%property.options.size();break;}
    if(property.options.size()==2 && state.multi.count>1 && state.multi.isMixed(multiKey(component->instanceId(),std::string(property.id)+"@"+std::to_string(slot)))) selected=1;
    auto candidate=*entity;
    if(scene::setComponentSlotProperty(candidate.components,component->type().id,property.id,slot,property.options[selected].value,component->instanceId())!=scene::ComponentPropertyStatus::Applied) {
      state.status="Valor por slot recusado pelo componente";return outcome;
    }
    outcome.documentChanged=history.applyValues(document,state.selection,candidate);
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::InspectorOverviewOpenBase) && widget<widgetId(EditorWidget::InspectorOverviewOpenBase)+detail::kRange) {
    const auto target=state.inspectorLocked?state.inspectorLocked:state.selection;
    const auto *entity=document.find(target);
    const auto index=widget-widgetId(EditorWidget::InspectorOverviewOpenBase);
    if(!entity || index>=entity->components.size()) return outcome;
    const auto *component=entity->components.at(index);
    if(component->unresolved() || scene::prefabLink(entity->components)==component || scene::importLink(entity->components)==component) return outcome;
    state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=target;
    state.expandedComponent.clear();state.nativeMenu=0;state.scriptMenu=0;
    state.expandedNative=scene::scriptBehavior(component)?0:component->instanceId();
    state.expandedScript=scene::scriptBehavior(component)?component->instanceId():0;
    state.propertyPage=0;state.scriptPropertyPage=0;state.propertyQuery.clear();
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::ProjectSectionBase) && widget<=widgetId(EditorWidget::ProjectSectionBase)+3) {
    state.projectSection=static_cast<EditorProjectSection>(widget-widgetId(EditorWidget::ProjectSectionBase));
    state.tagPage=0;state.tagQuery.clear();state.tagPicker=false;state.physicsMatrixPage=0;state.inputActionPage=0;state.inputBindingPage=0;state.propertyPage=0;
    // As propriedades de água moram no objeto raiz, como antes na aba avulsa.
    if(state.projectSection==EditorProjectSection::Water) state.selection=document.root();
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerPrevious) || widget==widgetId(EditorWidget::PhysicsLayerNext)) {
    const auto named=namedPhysicsLayers(document.layers());
    const auto it=std::find(named.begin(),named.end(),state.physicsLayer);
    const u32 index=it==named.end()?0u:static_cast<u32>(it-named.begin());
    if(!named.empty()) state.physicsLayer=named[widget==widgetId(EditorWidget::PhysicsLayerNext)?
      (index+1)%named.size():(index+named.size()-1)%named.size()];
    state.physicsMatrixPage=0;return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerAdd)) {
    auto layers=document.layers();
    for(u32 i=1;i<runtime::GameplayLayers::kCount;++i) if(!layers.named(i)) {
      if(layers.setName(i,"Camada "+std::to_string(i)) && history.setLayers(document,layers)) {
        state.physicsLayer=i;state.physicsMatrixPage=0;
      }
      break;
    }
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsLayerRename)) {
    if(document.layers().named(state.physicsLayer)) {
      state.editingPhysicsLayerName=true;
      const auto name=document.layers().name(state.physicsLayer);
      std::snprintf(state.renameText,sizeof(state.renameText),"%.*s",static_cast<int>(name.size()),name.data());
    }
    return outcome;
  }
  if(widget==widgetId(EditorWidget::PhysicsMatrixPrevious)) {if(state.physicsMatrixPage) --state.physicsMatrixPage;return outcome;}
  if(widget==widgetId(EditorWidget::PhysicsMatrixNext)) {++state.physicsMatrixPage;return outcome;}
  if(widget>=widgetId(EditorWidget::PhysicsInteractionBase) &&
     widget<widgetId(EditorWidget::PhysicsInteractionBase)+runtime::GameplayLayers::kCount) {
    const u32 target=widget-widgetId(EditorWidget::PhysicsInteractionBase);
    if(!document.layers().named(state.physicsLayer) || !document.layers().named(target)) return outcome;
    auto layers=document.layers();
    layers.setInteraction(state.physicsLayer,target,!layers.interacts(state.physicsLayer,target));
    history.setLayers(document,layers);
    return outcome;
  }

  if(widget>=widgetId(EditorWidget::InputActionRowBase) &&
     widget<widgetId(EditorWidget::InputActionRowBase)+runtime::InputActionMap::kMaximumActions) {
    const u32 index=widget-widgetId(EditorWidget::InputActionRowBase);
    if(index<document.inputActions().actions().size()) {
      state.inputActionIndex=index;state.inputBindingIndex=0;state.inputBindingPage=0;
      state.inputTab=0;state.inputDetails=false;
    }
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::InputBindingRowBase) &&
     widget<widgetId(EditorWidget::InputBindingRowBase)+runtime::InputActionMap::kMaximumBindings) {
    const auto &actions=document.inputActions().actions();
    if(state.inputActionIndex<actions.size() &&
       widget-widgetId(EditorWidget::InputBindingRowBase)<actions[state.inputActionIndex].bindings.size()) {
      state.inputBindingIndex=widget-widgetId(EditorWidget::InputBindingRowBase);
      state.inputTab=1;state.inputDetails=false;
    }
    return outcome;
  }
  if(widget>=widgetId(EditorWidget::InputActionPrevious) && widget<=widgetId(EditorWidget::InputDetailsToggle)) {
    if(widget==widgetId(EditorWidget::InputDetailsToggle)) {state.inputDetails=!state.inputDetails;return outcome;}
    if(widget==widgetId(EditorWidget::InputActionPagePrevious)) {if(state.inputActionPage) --state.inputActionPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputActionPageNext)) {++state.inputActionPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputBindingPagePrevious)) {if(state.inputBindingPage) --state.inputBindingPage;return outcome;}
    if(widget==widgetId(EditorWidget::InputBindingPageNext)) {++state.inputBindingPage;return outcome;}
    auto map=document.inputActions();
    const auto &actions=map.actions();
    if(widget==widgetId(EditorWidget::InputActionAdd)) {
      runtime::InputAction action;action.kind=runtime::ActionKind::Button;
      action.deadzone=0;action.bindings.push_back({runtime::InputSource::TouchButton,1,0,0,1,false});
      for(u32 i=1;i<=runtime::InputActionMap::kMaximumActions;++i) {
        action.id="Ação "+std::to_string(i);
        if(!map.find(action.id)) break;
      }
      if(map.add(action) && history.setInputActions(document,map)) {
        state.inputActionIndex=static_cast<u32>(map.actions().size()-1);state.inputBindingIndex=0;
        state.inputTab=0;state.inputActionPage=state.inputActionIndex/8;
        state.inputBindingPage=0;outcome.documentChanged=true;
      } else state.status="Limite de ações atingido";
      return outcome;
    }
    if(actions.empty()) return outcome;
    state.inputActionIndex=std::min(state.inputActionIndex,static_cast<u32>(actions.size()-1));
    const auto original=actions[state.inputActionIndex];
    if(widget==widgetId(EditorWidget::InputActionPrevious) || widget==widgetId(EditorWidget::InputActionNext)) {
      state.inputActionIndex=widget==widgetId(EditorWidget::InputActionNext)?
        (state.inputActionIndex+1)%actions.size():(state.inputActionIndex+actions.size()-1)%actions.size();
      state.inputBindingIndex=0;state.inputBindingPage=0;return outcome;
    }
    if(widget==widgetId(EditorWidget::InputTabAction) || widget==widgetId(EditorWidget::InputTabBinding) || widget==widgetId(EditorWidget::InputTabResponse)) {
      state.inputTab=widget==widgetId(EditorWidget::InputTabResponse)?2u:widget==widgetId(EditorWidget::InputTabBinding)?1u:0u;state.inputDetails=false;return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionRename)) {
      state.editingInputActionName=true;std::snprintf(state.renameText,sizeof(state.renameText),"%s",original.id.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionContext)) {
      state.editingInputContext=true;std::snprintf(state.renameText,sizeof(state.renameText),"%s",original.context.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputActionRemove)) {
      if(map.remove(original.id)) {outcome.documentChanged=history.setInputActions(document,map);
        state.inputActionIndex=std::min(state.inputActionIndex,static_cast<u32>(map.actions().empty()?0:map.actions().size()-1));
        state.inputBindingIndex=0;state.inputActionPage=0;state.inputBindingPage=0;}
      return outcome;
    }
    if(widget==widgetId(EditorWidget::InputDuration) || widget==widgetId(EditorWidget::InputDeadzone) || widget==widgetId(EditorWidget::InputSensitivity) ||
       widget==widgetId(EditorWidget::InputBindingCode) || widget==widgetId(EditorWidget::InputBindingNegativeCode) ||
       widget==widgetId(EditorWidget::InputBindingScale)) {
      state.inputEditField=widget;state.numericField=widget;state.numericReplace=true;
      std::string initial;
      if(widget==widgetId(EditorWidget::InputDuration)) initial=std::to_string(original.duration);
      else if(widget==widgetId(EditorWidget::InputDeadzone)) initial=std::to_string(original.deadzone);
      else if(widget==widgetId(EditorWidget::InputSensitivity)) initial=std::to_string(original.sensitivity);
      else if(!original.bindings.empty()) {
        const auto &binding=original.bindings[std::min(state.inputBindingIndex,static_cast<u32>(original.bindings.size()-1))];
        state.inputBindingIndex=std::min(state.inputBindingIndex,static_cast<u32>(original.bindings.size()-1));
        if(widget==widgetId(EditorWidget::InputBindingCode)) initial=std::to_string(binding.code);
        else if(widget==widgetId(EditorWidget::InputBindingNegativeCode)) initial=std::to_string(binding.negativeCode);
        else initial=std::to_string(binding.scale);
      } else {state.inputEditField=0;state.numericField=0;return outcome;}
      std::snprintf(state.numericText,sizeof(state.numericText),"%s",initial.c_str());return outcome;
    }
    if(widget==widgetId(EditorWidget::InputRoleMove) || widget==widgetId(EditorWidget::InputRoleLook) ||
       widget==widgetId(EditorWidget::InputRoleJump)) {
      const bool jump=widget==widgetId(EditorWidget::InputRoleJump);
      if(original.kind!=(jump?runtime::ActionKind::Button:runtime::ActionKind::Axis2D)) {
        state.status=jump?"Saltar exige uma ação Botão":"Mover e Olhar exigem Eixo 2D";return outcome;
      }
      const auto &current=jump?map.jumpAction():widget==widgetId(EditorWidget::InputRoleMove)?map.moveAction():map.lookAction();
      const auto role=current==original.id?std::string_view{}:std::string_view(original.id);
      if(jump?map.setJumpAction(role):widget==widgetId(EditorWidget::InputRoleMove)?map.setMoveAction(role):map.setLookAction(role))
        outcome.documentChanged=history.setInputActions(document,map);
      return outcome;
    }
    if(widget==widgetId(EditorWidget::InputBindingPrevious) || widget==widgetId(EditorWidget::InputBindingNext)) {
      if(!original.bindings.empty()) state.inputBindingIndex=widget==widgetId(EditorWidget::InputBindingNext)?
        (state.inputBindingIndex+1)%original.bindings.size():
        (state.inputBindingIndex+original.bindings.size()-1)%original.bindings.size();
      return outcome;
    }
    auto action=original;
    if(widget==widgetId(EditorWidget::InputActionEnabled))action.enabled=!action.enabled;
    else if(widget>=widgetId(EditorWidget::InputInteractionPress)&&widget<=widgetId(EditorWidget::InputInteractionTap)) {
      if(action.kind!=runtime::ActionKind::Button)return outcome;
      action.interaction=static_cast<runtime::InputInteraction>(widget-widgetId(EditorWidget::InputInteractionPress));
    } else if(widget>=widgetId(EditorWidget::InputGroupTouch)&&widget<=widgetId(EditorWidget::InputGroupGamepad)) {
      action.deviceGroups^=1u<<(widget-widgetId(EditorWidget::InputGroupTouch));
    } else if(widget==widgetId(EditorWidget::InputActionKind)) {
      action.kind=static_cast<runtime::ActionKind>((static_cast<u32>(action.kind)+1)%3);
      if(action.kind!=runtime::ActionKind::Button)action.interaction=runtime::InputInteraction::Press;
      if(action.kind!=runtime::ActionKind::Axis2D) for(auto &binding:action.bindings) binding.axis=0;
      if(action.kind!=runtime::ActionKind::Axis2D) {
        if(map.moveAction()==action.id) map.setMoveAction({});
        if(map.lookAction()==action.id) map.setLookAction({});
      }
      if(action.kind!=runtime::ActionKind::Button && map.jumpAction()==action.id) map.setJumpAction({});
    } else if(widget==widgetId(EditorWidget::InputBindingAdd)) {
      if(action.bindings.size()>=runtime::InputActionMap::kMaximumBindings) {state.status="Máximo de vínculos atingido";return outcome;}
      action.bindings.push_back({runtime::InputSource::TouchButton,1,0,0,1,false});
      state.inputBindingIndex=static_cast<u32>(action.bindings.size()-1);
      state.inputBindingPage=state.inputBindingIndex/4;
    } else {
      if(action.bindings.empty()) return outcome;
      state.inputBindingIndex=std::min(state.inputBindingIndex,static_cast<u32>(action.bindings.size()-1));
      if(widget==widgetId(EditorWidget::InputBindingRemove)) {
        action.bindings.erase(action.bindings.begin()+state.inputBindingIndex);
        state.inputBindingPage=0;
      }
      else {
        auto &binding=action.bindings[state.inputBindingIndex];
        if(widget==widgetId(EditorWidget::InputBindingSource)) {
          binding.source=static_cast<runtime::InputSource>(1+(static_cast<u32>(binding.source)%8));
          if(binding.source==runtime::InputSource::TouchButton && binding.code>=32) binding.code=0;
          if(binding.source!=runtime::InputSource::Key) binding.negativeCode=0;
          if(binding.source==runtime::InputSource::MouseButton && binding.code>=5)binding.code=0;
          if(binding.source==runtime::InputSource::MouseAxis && binding.code>=4)binding.code=0;
          if(binding.source==runtime::InputSource::TouchMove || binding.source==runtime::InputSource::TouchLook) binding.code=0;
        } else if(widget==widgetId(EditorWidget::InputBindingAxis)) {
          if(action.kind!=runtime::ActionKind::Axis2D) {state.status="Esta ação tem apenas um eixo";return outcome;}
          binding.axis=1-binding.axis;
        } else if(widget==widgetId(EditorWidget::InputBindingInvert)) binding.invert=!binding.invert;
        else return outcome;
      }
    }
    if(map.replace(original.id,action)) outcome.documentChanged=history.setInputActions(document,map);
    if(!outcome.documentChanged) state.status="Vínculo ou ação inválida";
    return outcome;
  }

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
    if(componentVisualSelectable(document,id,state.hiddenLayers,state.unpickableLayers,
        state.sceneHidden,state.scenePickOff)) {
      state.selection=id;state.textureInspector=false;state.textureViewer=false;state.propertyPage=0;
    }
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

  if(widget==widgetId(EditorWidget::ComponentClipAdd)) {
    const auto *entity=document.find(state.selection);
    const auto *component=entity?entity->components.findInstance(state.expandedNative):nullptr;
    if(!entity || state.workspace!=EditorWorkspace::Scene || history.isOpen() ||
       !component || &component->type()!=&scene::Animation::descriptor) return outcome;
    auto value=*entity;
    auto *animation=static_cast<scene::Animation *>(value.components.editInstance(component->instanceId()));
    if(animation && animation->appendClip()) outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }
  const u32 clipOperation=widget&0xff000000u;
  if(clipOperation==widgetId(EditorWidget::ComponentClipMoveUpBase) ||
     clipOperation==widgetId(EditorWidget::ComponentClipMoveDownBase) ||
     clipOperation==widgetId(EditorWidget::ComponentClipRemoveBase)) {
    const auto *entity=document.find(state.selection);
    const u32 encoded=widget&0x00ffffffu,type=encoded&0xffu,bindingIndex=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
    if(!entity || state.workspace!=EditorWorkspace::Scene || history.isOpen() || type>=entity->components.size()) return outcome;
    const auto *component=entity->components.at(type);
    if(&component->type()!=&scene::Animation::descriptor || bindingIndex>=component->type().resourceBindings.size()) return outcome;
    const auto &binding=component->type().resourceBindings[bindingIndex];
    if(binding.id!="clips" || slot>=binding.slotCount(*component)) return outcome;
    const u64 element=binding.elementAt(*component,slot);
    if(!element) return outcome;
    auto value=*entity;
    auto *animation=static_cast<scene::Animation *>(value.components.editInstance(component->instanceId()));
    bool changed=false;
    if(clipOperation==widgetId(EditorWidget::ComponentClipRemoveBase)) changed=animation->removeClip(element);
    else if(clipOperation==widgetId(EditorWidget::ComponentClipMoveUpBase) && slot>0) changed=animation->moveClip(element,slot-1);
    else if(clipOperation==widgetId(EditorWidget::ComponentClipMoveDownBase) && slot+1<animation->clips.size())
      changed=animation->moveClip(element,slot+1);
    if(changed) outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }

  if(state.enumPicker && (widget==widgetId(EditorWidget::ComponentEnumPickerClose) ||
     (widget>=widgetId(EditorWidget::ComponentEnumOptionBase) && widget<widgetId(EditorWidget::ComponentEnumOptionBase)+detail::kRange))) {
    const u32 field=state.enumPicker,index=field&0xffu,enumIndex=(field>>8)&0xffffu;
    state.enumPicker=0;
    if(widget==widgetId(EditorWidget::ComponentEnumPickerClose)) return outcome;
    const auto *entity=document.find(state.selection);
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    const u32 option=widget-widgetId(EditorWidget::ComponentEnumOptionBase);
    if(!component || enumIndex>=component->type().enums.size() || history.isOpen()) return outcome;
    const auto &property=component->type().enums[enumIndex];
    if(option>=property.options.size() || property.options[option].value==property.read(*component)) return outcome;
    auto value=*entity;
    if(scene::setComponentProperty(value.components,component->type().id,property.id,property.options[option].value,
                                   component->instanceId())!=scene::ComponentPropertyStatus::Applied) {
      state.status="Esta opção não é aceita com os valores atuais";return outcome;
    }
    outcome.documentChanged=history.applyValues(document,entity->id,value);
    return outcome;
  }
  // Menu comum a componentes nativos e comportamentos C#. O índice é a
  // posição do componente no objeto, a mesma que o Inspector usa nos cartões.
  {
    const u32 operation=widget&0xff000000u,index=widget&0x00ffffffu;
    const auto is=[&](EditorWidget base){return operation==widgetId(base);};
    const auto *entity=document.find(state.selection);
    const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
    const auto *script=scene::scriptBehavior(component);
    const bool generic=is(EditorWidget::ComponentMoveUpBase)||is(EditorWidget::ComponentMoveDownBase)||
        is(EditorWidget::ComponentPasteNewBase)||is(EditorWidget::ComponentHelpBase)||is(EditorWidget::ComponentEnableBase)||
        (script&&(is(EditorWidget::ComponentCopyBase)||is(EditorWidget::ComponentPasteBase)||is(EditorWidget::ComponentResetBase)));
    if(generic) {
      if(!component || state.workspace!=EditorWorkspace::Scene || history.isOpen()) return outcome;
      // Como o menu de contexto da Unity, escolher uma ação fecha o menu.
      state.nativeMenu=0;state.scriptMenu=0;
      auto value=*entity;
      if(is(EditorWidget::ComponentHelpBase)) {
        const auto *schema=scene::findComponentSchema(component->type().id);
        if(schema && schema->reference.starts_with("https://")) {
          state.externalLink=std::string(schema->reference);
          state.status=std::string("Referência de ")+schema->name+" aberta no navegador";
        } else state.status="Este componente não tem página de referência";
        return outcome;
      }
      if(is(EditorWidget::ComponentCopyBase)) {
        state.componentClipboard=component->clone();state.status="Componente copiado";return outcome;
      }
      if(is(EditorWidget::ComponentMoveUpBase)||is(EditorWidget::ComponentMoveDownBase)) {
        // O vizinho é o próximo cartão visível: vínculos de importação e outros
        // dados sem cartão não contam como posição.
        const bool up=is(EditorWidget::ComponentMoveUpBase);
        const auto visible=[&](usize i) {
          const auto *other=entity->components.at(i);
          return findEditorComponent(other->type().id)||scene::scriptBehavior(other)||other->unresolved();
        };
        usize target=index;bool found=false;
        if(up) {for(usize i=index;i>0;--i) if(visible(i-1)) {target=i-1;found=true;break;}}
        else for(usize i=index+1;i<entity->components.size();++i) if(visible(i)) {target=i;found=true;break;}
        if(!found || !value.components.moveInstance(component->instanceId(),target)) return outcome;
        state.status=up?"Componente movido para cima":"Componente movido para baixo";
      } else if(is(EditorWidget::ComponentPasteNewBase)) {
        const auto &clip=state.componentClipboard;
        if(!clip) {state.status="Copie um componente antes";return outcome;}
        auto plan=scene::planComponentAddition(value.components,clip->type().id);
        if(!plan.ready) {state.status=plan.error?plan.error:"Não foi possível colar como novo";return outcome;}
        if(!plan.candidate.replaceInstance(plan.requestedInstance,*clip) ||
           !editorReferencesAccept(document,entity->id,*clip)) {state.status="Valores ou referências incompatíveis";return outcome;}
        value.components=std::move(plan.candidate);state.status="Componente colado como novo";
      } else if(is(EditorWidget::ComponentPasteBase)) {
        const auto *copied=scene::scriptBehavior(state.componentClipboard.get());
        if(!copied || copied->scriptType!=script->scriptType ||
           !value.components.replaceInstance(component->instanceId(),*copied)) {state.status="Copie um comportamento do mesmo tipo";return outcome;}
        state.status="Valores do comportamento colados";
      } else if(is(EditorWidget::ComponentResetBase)) {
        // Sem valores autorados, o comportamento volta aos padrões do código.
        auto replacement=*script;replacement.properties.clear();
        if(!value.components.replaceInstance(component->instanceId(),replacement)) return outcome;
        state.status="Comportamento redefinido para os padrões do código";
      } else if(is(EditorWidget::ComponentEnableBase)) {
        if(script) {
          auto replacement=*script;replacement.enabled=!replacement.enabled;
          if(!value.components.replaceInstance(component->instanceId(),replacement)) return outcome;
        } else {
          const scene::ComponentBoolean *enabled=nullptr;
          for(const auto &p:component->type().booleans) if(p.id=="enabled") enabled=&p;
          if(!enabled || scene::setComponentProperty(value.components,component->type().id,"enabled",!enabled->read(*component),
                                                     component->instanceId())!=scene::ComponentPropertyStatus::Applied) return outcome;
        }
      }
      outcome.documentChanged=history.applyValues(document,entity->id,value);
      return outcome;
    }
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
      state.nativeMenu=state.nativeMenu==instance?0:instance;state.expandedNative=0;state.expandedComponent.clear();state.expandedScript=0;state.propertyQuery.clear();
    } else if(operation==widgetId(EditorWidget::ComponentFoldBase)) {
      state.expandedNative=state.expandedNative==instance?0:instance;state.expandedComponent.clear();state.expandedScript=0;state.nativeMenu=0;state.scriptMenu=0;state.meshPicker=false;state.propertyQuery.clear();state.propertyPage=0;
    } else if(operation==widgetId(EditorWidget::ComponentCopyBase)) {
      state.componentClipboard=component->clone();state.status="Componente copiado";state.nativeMenu=0;
    } else {
      auto value=*entity;
      if(adding) {
        if(const auto *reason=entry.unavailable(value)) {state.status=reason;return outcome;}
        auto plan=scene::planComponentAddition(value.components,entry.type->id);
        if(!plan.ready) {state.status=plan.error;return outcome;}
        value.components=std::move(plan.candidate);
        state.expandedComponent.clear();state.expandedNative=0;state.expandedScript=0;state.componentPage=~u32{0};state.addingComponent=false;state.nativeMenu=0;
      } else if(operation==widgetId(EditorWidget::ComponentBooleanBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->booleans.size()) return outcome;
        const auto &property=entry.type->booleans[field];
        if(scene::setComponentProperty(value.components,entry.type->id,property.id,!property.read(*component),instance)!=scene::ComponentPropertyStatus::Applied) return outcome;
      } else if(operation==widgetId(EditorWidget::ComponentEnumBase)) {
        const u32 field=(widget&0x00ffffffu)>>8;if(field>=entry.type->enums.size()) return outcome;
        state.enumPicker=widget;return outcome;
      } else if(operation==widgetId(EditorWidget::ComponentPasteBase)) {
        state.nativeMenu=0;
        if(!state.componentClipboard||&state.componentClipboard->type()!=entry.type||
           !editorReferencesAccept(document,entity->id,*state.componentClipboard)||
           !value.components.replaceInstance(instance,*state.componentClipboard)) {state.status="Valores ou referências incompatíveis";return outcome;}
      } else if(operation==widgetId(EditorWidget::ComponentResetBase)) {
        state.nativeMenu=0;
        const auto defaults=entry.type->create();if(!defaults||!value.components.replaceInstance(instance,*defaults)) return outcome;
      } else return outcome;
      outcome.documentChanged=history.applyValues(document,state.selection,value);
    }
    state.propertyPage=0;
    return outcome;
  }

  if (widget >= widgetId(EditorWidget::HierarchyCollapseBase) && widget < widgetId(EditorWidget::ImpactOpenBase)) {
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
    // Unity SceneVisibility: o olho esconde (ou mostra) o objeto e os
    // descendentes só na vista do editor; a cena e o jogo não mudam.
    const EditorEntityId entity = widget - widgetId(EditorWidget::HierarchyEyeBase);
    if (document.find(entity) != nullptr) {
      std::vector<EditorEntityId> subtree;document.collectSubtree(entity,subtree);
      const bool hide=!state.sceneHiddenHas(entity);
      for(const auto id:subtree) {
        auto found=std::find(state.sceneHidden.begin(),state.sceneHidden.end(),id);
        if(hide && found==state.sceneHidden.end()) state.sceneHidden.push_back(id);
        if(!hide && found!=state.sceneHidden.end()) state.sceneHidden.erase(found);
      }
      state.sceneVisibilityChanged=true;
    }
    return outcome;
  }
  if (widget >= widgetId(EditorWidget::HierarchyPickBase) && widget < widgetId(EditorWidget::HierarchyPickBase)+EditorDocument::kMaximumEntities) {
    // Unity Scene picking: a mão tira (ou devolve) o objeto e os descendentes
    // da seleção por toque na vista; a Hierarquia continua selecionando.
    const EditorEntityId entity = widget - widgetId(EditorWidget::HierarchyPickBase);
    if (document.find(entity) != nullptr) {
      std::vector<EditorEntityId> subtree;document.collectSubtree(entity,subtree);
      const bool off=!state.scenePickOffHas(entity);
      for(const auto id:subtree) {
        auto found=std::find(state.scenePickOff.begin(),state.scenePickOff.end(),id);
        if(off && found==state.scenePickOff.end()) state.scenePickOff.push_back(id);
        if(!off && found!=state.scenePickOff.end()) state.scenePickOff.erase(found);
      }
      state.sceneVisibilityChanged=true;
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
    } else {state.selection=target;state.selectionSet={target};state.propertyPage=0;state.routePoint=0;}
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
      state.addingComponent=!state.addingComponent;state.addScroll=0;state.componentPreview=0;state.componentPreviewValues=false;state.componentPreviewPage=0;state.scriptPreviewType.clear();state.meshPicker=false;state.propertyPage=0;state.componentPage=0;break;
    case EditorWidget::ComponentPreviewBack: state.componentPreview=0;state.componentPreviewValues=false;state.componentPreviewPage=0;state.scriptPreviewType.clear();break;
    case EditorWidget::ComponentPreviewComposition: state.componentPreviewValues=false;state.componentPreviewPage=0;break;
    case EditorWidget::ComponentPreviewValues: state.componentPreviewValues=true;state.componentPreviewPage=0;break;
    case EditorWidget::ComponentPreviewPrevious: if(state.componentPreviewPage) --state.componentPreviewPage;break;
    case EditorWidget::ComponentPreviewNext: ++state.componentPreviewPage;break;
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
    case EditorWidget::CreationClearSearch: state.creationSearch[0]=0;state.creationScroll=0;break;
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
      const auto parent=state.creationMenu && state.creationAsChild && document.exists(state.selection) && state.selection!=document.root()?state.selection:document.root();
      const auto copy=history.createEntity(document,parent,EditorEntityKind::Folder,"Objeto vazio");
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
    case EditorWidget::TabGui: state.workspaceMenu=false;state.workspace=EditorWorkspace::Gui;break;
    case EditorWidget::TabScene: state.workspaceMenu=false; state.workspace = EditorWorkspace::Scene; break;
    case EditorWidget::TabAssets: if(state.assetCount) {state.workspaceMenu=false; state.workspace = EditorWorkspace::Assets;} break;
    case EditorWidget::TabLighting: state.workspaceMenu=false; state.workspace = EditorWorkspace::Lighting; state.selection=document.root(); state.propertyPage=0; break;
    case EditorWidget::TabProject:
      state.workspaceMenu=false;state.workspace=EditorWorkspace::Project;state.propertyPage=0;
      state.tagPage=0;state.tagQuery.clear();state.tagPicker=false;state.physicsMatrixPage=0;state.inputActionPage=0;state.inputBindingPage=0;
      if(state.projectSection==EditorProjectSection::Water && !waterCreationAvailable(state)) state.projectSection=EditorProjectSection::Layers;
      if(state.projectSection==EditorProjectSection::Water) state.selection=document.root();
      break;
    case EditorWidget::InspectorInspection: state.inspectorSurface=EditorInspectorSurface::Inspection;break;
    case EditorWidget::InspectorComponents: state.inspectorSurface=EditorInspectorSurface::Components;state.componentOverviewScroll=0;break;
    case EditorWidget::InspectorMeshLightmap: state.meshTab=2;state.propertyPage=0;state.meshPicker=false;break;
    case EditorWidget::InspectorReferenceFlags: state.inspectorReferenceFlags=true;state.propertyPage=0;break;
    case EditorWidget::InspectorReferenceFlagsClose: state.inspectorReferenceFlags=false;state.propertyPage=0;break;
    case EditorWidget::InspectorOverviewObject:
      state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=state.inspectorLocked?state.inspectorLocked:state.selection;
      state.expandedComponent="astra.object";state.expandedNative=0;state.expandedScript=0;
      state.nativeMenu=0;state.scriptMenu=0;state.transformMenu=false;state.inspectorReferenceFlags=false;state.propertyPage=0;break;
    case EditorWidget::InspectorComponentPrevious:
    case EditorWidget::InspectorComponentNext: {
      const auto target=state.inspectorLocked?state.inspectorLocked:state.selection;
      const auto *entity=document.find(target);if(!entity) break;
      std::vector<i32> items{-2,-1}; // Objetos e Transform são dados universais.
      i32 current=state.expandedComponent=="astra.object"?-2:-1;
      for(u32 i=0;i<entity->components.size();++i) {
        const auto *component=entity->components.at(i);
        if(component->unresolved()||(!findEditorComponent(component->type().id)&&!scene::scriptBehavior(component))) continue;
        if(state.multi.count>1&&target==state.selection&&!state.multi.isCommon(component->instanceId())) continue;
        items.push_back(static_cast<i32>(i));
        if(component->instanceId()==state.expandedNative||component->instanceId()==state.expandedScript) current=static_cast<i32>(i);
      }
      const auto found=std::find(items.begin(),items.end(),current);
      const usize at=found==items.end()?0:static_cast<usize>(found-items.begin());
      const usize next=widget==widgetId(EditorWidget::InspectorComponentNext)?(at+1)%items.size():(at+items.size()-1)%items.size();
      state.componentSelection=target;state.expandedNative=0;state.expandedScript=0;
      state.nativeMenu=0;state.scriptMenu=0;state.transformMenu=false;state.propertyPage=0;state.scriptPropertyPage=0;
      state.propertyQuery.clear();state.inspectorReferenceFlags=false;
      const auto selected=items[next];
      state.expandedComponent=selected==-2?"astra.object":selected==-1?"astra.transform":"";
      if(selected>=0) {
        const auto *component=entity->components.at(static_cast<u32>(selected));
        if(scene::scriptBehavior(component)) state.expandedScript=component->instanceId();
        else state.expandedNative=component->instanceId();
      }
      break;
    }
    case EditorWidget::InspectorOverviewTransform:
      state.inspectorSurface=EditorInspectorSurface::Inspection;state.componentSelection=state.inspectorLocked?state.inspectorLocked:state.selection;
      state.expandedComponent="astra.transform";state.expandedNative=0;state.expandedScript=0;state.propertyPage=0;break;
    case EditorWidget::InspectorTabTransform: state.tab = EditorInspectorTab::Transform; break;
    case EditorWidget::InspectorTabMaterial:
      if (const auto *selected=document.find(state.selection); selected && meshAsset(*selected)) {
        state.tab = EditorInspectorTab::Material; state.propertyPage=0;
      }
      break;
    case EditorWidget::InspectorTabProperties: state.tab = EditorInspectorTab::Properties; break;
    case EditorWidget::DiagnosticDockToggle:
      state.diagnosticDockOpen=!state.diagnosticDockOpen;
      state.consoleScroll=0;state.consoleSelected=0;break;
    case EditorWidget::DiagnosticOlder:
      state.consoleScroll=std::min<u32>(EditorConsole::Capacity,state.consoleScroll+3);break;
    case EditorWidget::DiagnosticNewer:
      state.consoleScroll=state.consoleScroll>3?state.consoleScroll-3:0;break;
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
    case EditorWidget::HierarchyAdd:
      state.creationAsChild=document.exists(state.selection) && state.selection!=document.root();
      state.creationMenu=true;break;
    case EditorWidget::CreationAtRoot: state.creationAsChild=false;break;
    case EditorWidget::CreationAsChild:
      if(document.exists(state.selection) && state.selection!=document.root()) state.creationAsChild=true;
      break;
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
    case EditorWidget::PlayInspect:
      if(state.workspace==EditorWorkspace::Play) {
        state.playInspect=!state.playInspect;
        // Numa tela estreita só cabe um painel: abrir já no Inspector.
        if(state.playInspect && state.compactPanel==EditorScreenState::CompactPanel::Viewport)
          state.compactPanel=EditorScreenState::CompactPanel::Inspector;
      }
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
