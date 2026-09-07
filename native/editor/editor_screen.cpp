#include "editor/editor_screen.h"

#include "ui/ui_icon_id.h"

#include <algorithm>
#include <cstdio>
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
constexpr float kGridExtent = 20.0f;

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
    case EditorEntityKind::Folder: return UiIcon::AssetsFolder;
    case EditorEntityKind::Mesh: return UiIcon::SceneObject;
    case EditorEntityKind::Light: return UiIcon::LightingSun;
    case EditorEntityKind::Camera: return UiIcon::RuntimeCamera;
    case EditorEntityKind::Water: return UiIcon::NatureWater;
    case EditorEntityKind::Effect: return UiIcon::VfxParticles;
  }
  return UiIcon::SceneObject;
}

const char *kindLabel(EditorEntityKind kind) {
  switch (kind) {
    case EditorEntityKind::Folder: return "GROUP";
    case EditorEntityKind::Mesh: return "MESH";
    case EditorEntityKind::Light: return "LIGHT";
    case EditorEntityKind::Camera: return "CAMERA";
    case EditorEntityKind::Water: return "WATER";
    case EditorEntityKind::Effect: return "EFFECT";
  }
  return "NODE";
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
    list.addText(bounds, text, colour, style, horizontal, UiAlign::Center);
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
  builder.list.addImage(centred(play, 18.0f, 18.0f), static_cast<UiImageId>(UiIcon::RuntimePlay),
                        theme.color.accentInk);
  builder.router.addRegion(play, widgetId(EditorWidget::PlayFromTopBar), theme.touch.minimumTarget);
  takeRight(content, theme.spacing.small);
  const auto action = [&](UiIcon icon, EditorWidget widget, bool enabled) {
    builder.iconButton(takeRight(content, kActionButton), icon, widgetId(widget), false,
                       enabled ? theme.color.text : theme.color.textFaint);
    takeRight(content, theme.spacing.tiny);
  };
  action(UiIcon::UiMoreVertical, EditorWidget::ProjectMenu, true);
  action(UiIcon::AssetsFolder, EditorWidget::OpenProject, true);
  action(UiIcon::EditorRedo, EditorWidget::Redo, builder.state.canRedo);
  action(UiIcon::EditorUndo, EditorWidget::Undo, builder.state.canUndo);

  // Nome do projeto, com o que sobrar entre a marca e as abas.
  const UiRect chip = takeLeft(content, std::min(content.width * 0.32f, 150.0f));
  UiRect chipContent = chip;
  builder.list.addImage(centred(takeLeft(chipContent, 24.0f), 16.0f, 16.0f),
                        static_cast<UiImageId>(UiIcon::SceneObject), theme.color.text);
  builder.list.addImage(centred(takeRight(chipContent, 18.0f), 12.0f, 12.0f),
                        static_cast<UiImageId>(UiIcon::UiChevronDown), theme.color.textDim);
  builder.label(chipContent, builder.state.projectName, theme.color.text, theme.type.cardName);
  builder.router.addRegion(chip, widgetId(EditorWidget::SceneChip));

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
      {"Scene", UiIcon::SceneObject, EditorWidget::TabScene, EditorWorkspace::Scene},
      {"Assets", UiIcon::AssetsFolder, EditorWidget::TabAssets, EditorWorkspace::Assets},
      {"Lighting", UiIcon::LightingSun, EditorWidget::TabLighting, EditorWorkspace::Lighting},
      {"Play", UiIcon::RuntimePlay, EditorWidget::TabPlay, EditorWorkspace::Play},
      {"Settings", UiIcon::UiSettings, EditorWidget::TabSettings, EditorWorkspace::Settings},
  };
  const float tabWidth = std::max(38.0f, content.width / 5.0f);
  // A decisão de mostrar rótulo é UMA, para todas as abas, e é tomada pela mais
  // larga. Decidir por aba deixaria "Play" com nome e "Settings" sem, o que
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
    const UiColor minor = withAlpha(theme.color.text, 0.10f);
    const UiColor major = withAlpha(theme.color.text, 0.22f);
    const i32 extent = static_cast<i32>(kGridExtent);
    UiPoint a{};
    UiPoint b{};
    for (i32 step = -extent; step <= extent; ++step) {
      const float offset = static_cast<float>(step);
      const bool isMajor = step % 5 == 0;
      const UiColor colour = isMajor ? major : minor;
      const float width = isMajor ? 1.5f : 1.0f;
      const float alongZ[2][3] = {{offset, 0.0f, -kGridExtent}, {offset, 0.0f, kGridExtent}};
      if (projectSegmentToScreen(*state.view, alongZ[0], alongZ[1], a, b))
        builder.list.addLine(a, b, colour, width);
      const float alongX[2][3] = {{-kGridExtent, 0.0f, offset}, {kGridExtent, 0.0f, offset}};
      if (projectSegmentToScreen(*state.view, alongX[0], alongX[1], a, b))
        builder.list.addLine(a, b, colour, width);
    }
    // Os dois eixos do mundo por cima da grade, nas cores da convenção.
    const float axisX[2][3] = {{-kGridExtent, 0.0f, 0.0f}, {kGridExtent, 0.0f, 0.0f}};
    if (projectSegmentToScreen(*state.view, axisX[0], axisX[1], a, b))
      builder.list.addLine(a, b, withAlpha(theme.color.axisX, 0.55f), 1.5f);
    const float axisZ[2][3] = {{0.0f, 0.0f, -kGridExtent}, {0.0f, 0.0f, kGridExtent}};
    if (projectSegmentToScreen(*state.view, axisZ[0], axisZ[1], a, b))
      builder.list.addLine(a, b, withAlpha(theme.color.axisZ, 0.55f), 1.5f);
  }

  const EditorEntity *entity = state.document->find(state.selection);
  if (entity != nullptr && state.tool != EditorGizmoMode::Select) {
    EditorGizmoSettings settings{};
    settings.screenLengthPixels = 72.0f;
    const EditorGizmoFrame frame =
        buildGizmoFrame(*state.view, entity->transform.position, settings);
    if (frame.valid) {
      const UiColor colours[3] = {theme.color.axisX, theme.color.axisY, theme.color.axisZ};
      for (u32 axis = 0; axis < 3; ++axis) {
        if (!frame.axisUsable[axis]) continue;
        const bool active = static_cast<u32>(state.activeGizmoAxis) == axis + 1;
        builder.list.addLine(frame.originScreen, frame.axisEndScreen[axis],
                             active ? theme.color.accent : colours[axis], active ? 5.0f : 3.0f);
        // A ponta é um alvo próprio: arrastar um eixo pela ponta é o gesto que
        // um dedo grosso acerta, e a linha inteira continua valendo como alça.
        const UiRect handle{frame.axisEndScreen[axis].x - 7.0f, frame.axisEndScreen[axis].y - 7.0f,
                            14.0f, 14.0f};
        builder.list.addRect(handle, active ? theme.color.accent : colours[axis], 7.0f);
        builder.router.addRegion(handle, gizmoAxisWidget(axis), theme.touch.minimumTarget);
      }
      builder.list.addRect({frame.originScreen.x - 4.0f, frame.originScreen.y - 4.0f, 8.0f, 8.0f},
                           theme.color.text, 4.0f);
    }
  }
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
                        static_cast<UiImageId>(UiIcon::SceneObject), theme.color.text);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::UiMoreVertical,
                     widgetId(EditorWidget::HierarchyMenu));
  takeRight(header, theme.spacing.tiny);
  builder.iconButton(takeRight(header, 28.0f), UiIcon::UiAdd, widgetId(EditorWidget::HierarchyAdd));
  builder.label(header, "Hierarchy", theme.color.text, theme.type.cardName);

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
    if (entity == nullptr) continue;
    const auto children = document.childrenOf(frame.entity);
    for (usize index = children.size(); index > 0; --index)
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
    takeLeft(rowContent, static_cast<float>(frame.depth) * 14.0f);
    const UiColor ink = selected ? theme.color.accentInk : theme.color.text;
    const UiRect twisty = takeLeft(rowContent, 16.0f);
    if (!children.empty())
      builder.list.addImage(centred(twisty, 12.0f, 12.0f),
                            static_cast<UiImageId>(UiIcon::UiChevronDown),
                            selected ? theme.color.accentInk : theme.color.textDim);
    builder.list.addImage(centred(takeLeft(rowContent, 22.0f), 15.0f, 15.0f),
                          static_cast<UiImageId>(iconForKind(entity->kind)),
                          selected ? theme.color.accentInk : theme.color.accent);
    const UiRect eye = takeRight(rowContent, 24.0f);
    builder.list.addImage(
        centred(eye, 15.0f, 15.0f),
        static_cast<UiImageId>(entity->visible ? UiIcon::SceneVisibility
                                               : UiIcon::SceneVisibilityOff),
        selected ? theme.color.accentInk : theme.color.textDim);
    builder.label(rowContent, entity->name, ink, theme.type.body);

    // A linha PRIMEIRO, o olho DEPOIS: o roteador testa da última região para a
    // primeira, então quem entra depois fica por cima. O olho precisa ganhar do
    // fundo da linha, senão tocar nele seleciona em vez de alternar.
    builder.router.addRegion(row, hierarchyRowWidget(frame.entity));
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
    builder.router.addRegion(field, widget, theme.touch.minimumTarget);
  }
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
  builder.iconButton(takeRight(header, 28.0f), UiIcon::UiMoreVertical,
                     widgetId(EditorWidget::InspectorMenu));
  takeRight(header, theme.spacing.tiny);
  builder.toggle(takeRight(header, kToggleWidth + 4.0f), entity->active,
                 widgetId(EditorWidget::InspectorActive));
  const float half = header.height * 0.5f;
  builder.label({header.x, header.y, header.width, half}, entity->name, theme.color.text,
                theme.type.cardName);
  builder.label({header.x, header.y + half, header.width, half}, kindLabel(entity->kind),
                theme.color.textDim, theme.type.label);

  UiRect tabs = takeTop(content, 30.0f);
  takeTop(content, theme.spacing.small);
  struct Tab final {
    const char *label;
    EditorWidget widget;
    EditorInspectorTab value;
  };
  const Tab tabItems[] = {
      {"Transform", EditorWidget::InspectorTabTransform, EditorInspectorTab::Transform},
      {"Material", EditorWidget::InspectorTabMaterial, EditorInspectorTab::Material},
      {"Props", EditorWidget::InspectorTabProperties, EditorInspectorTab::Properties},
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
      buildTransformRow(builder, content, "Position", entity->transform.position, 0, 3);
      buildTransformRow(builder, content, "Rotation", entity->transform.rotationDegrees, 1, 1);
      buildTransformRow(builder, content, "Scale", entity->transform.scale, 2, 1);
      break;
    }
    case EditorInspectorTab::Material: {
      builder.label(content, "Sem material atribuido", theme.color.textMuted, theme.type.body);
      break;
    }
    case EditorInspectorTab::Properties: {
      struct Switch final {
        const char *label;
        UiIcon icon;
        bool value;
        EditorWidget widget;
      };
      const Switch switches[] = {
          {"Visible", UiIcon::SceneVisibility, entity->visible, EditorWidget::ToggleVisible},
          {"Cast Shadow", UiIcon::SceneObject, entity->castShadow, EditorWidget::ToggleCastShadow},
          {"Receive Shadow", UiIcon::AssetsChecker, entity->receiveShadow,
           EditorWidget::ToggleReceiveShadow},
          {"Static", UiIcon::SceneLayers, entity->isStatic, EditorWidget::ToggleStatic},
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
      {UiIcon::EditorSelect, EditorWidget::ToolSelect, EditorGizmoMode::Select},
      {UiIcon::EditorMove, EditorWidget::ToolMove, EditorGizmoMode::Translate},
      {UiIcon::EditorRotate, EditorWidget::ToolRotate, EditorGizmoMode::Rotate},
      {UiIcon::EditorScale, EditorWidget::ToolScale, EditorGizmoMode::Scale},
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
  float hierarchyWidth = state.hierarchyVisible
      ? (state.hierarchyWidth > 0.0f ? state.hierarchyWidth : available * 0.24f)
      : 0.0f;
  float inspectorWidth = state.inspectorVisible
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
  buildViewportOverlay(builder, layout.viewport);
  if (state.workspace == EditorWorkspace::Scene) buildToolRail(builder, layout.viewport);
  if (!layout.hierarchyPanel.isEmpty())
    layout.hierarchyRowCount =
        buildHierarchy(builder, layout.hierarchyPanel, layout.hierarchyVisibleRows);
  if (!layout.inspectorPanel.isEmpty()) buildInspector(builder, layout.inspectorPanel);

  // Botões flutuantes nos cantos do viewport, fora dos painéis.
  const UiRect corner{layout.viewport.right() - kCornerButton - theme.spacing.small,
                      layout.viewport.bottom() - kCornerButton - theme.spacing.small,
                      kCornerButton, kCornerButton};
  builder.iconButton(corner, UiIcon::ViewExpand, widgetId(EditorWidget::Fullscreen));
  builder.iconButton({layout.viewport.right() - kCornerButton * 2.0f - theme.spacing.small * 2.0f,
                      corner.y, kCornerButton, kCornerButton},
                     UiIcon::ViewGrid, widgetId(EditorWidget::ViewModeSolid), state.showGrid);
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

  // O resto age no toque completo, e não no `Down`: um dedo que desce num botão
  // e desliza para fora desistiu dele.
  if (!routing.tapped) return outcome;

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
    state.selection = widget - widgetId(EditorWidget::HierarchyRowBase);
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
    case EditorWidget::Undo: outcome.documentChanged = history.undo(document); break;
    case EditorWidget::Redo: outcome.documentChanged = history.redo(document); break;
    case EditorWidget::PlayFromTopBar:
    case EditorWidget::TabPlay:
      state.workspace = EditorWorkspace::Play;
      outcome.requestPlay = true;
      break;
    case EditorWidget::TabScene: state.workspace = EditorWorkspace::Scene; break;
    case EditorWidget::TabAssets: state.workspace = EditorWorkspace::Assets; break;
    case EditorWidget::TabLighting: state.workspace = EditorWorkspace::Lighting; break;
    case EditorWidget::TabSettings: state.workspace = EditorWorkspace::Settings; break;
    case EditorWidget::InspectorTabTransform: state.tab = EditorInspectorTab::Transform; break;
    case EditorWidget::InspectorTabMaterial: state.tab = EditorInspectorTab::Material; break;
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
    case EditorWidget::HierarchyAdd: {
      const EditorEntity *selected = document.find(state.selection);
      const EditorEntityId parent = selected != nullptr ? selected->parent : document.root();
      const EditorEntityId created =
          history.createEntity(document, parent, EditorEntityKind::Mesh, "Object");
      if (created != kInvalidEntity) {
        state.selection = created;
        outcome.documentChanged = true;
      }
      break;
    }
    default: break;
  }
  return outcome;
}

} // namespace ae::editor
