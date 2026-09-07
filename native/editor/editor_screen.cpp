#include "editor/editor_screen.h"

#include "ui/ui_icon_id.h"

#include <algorithm>
#include <cstdio>

namespace ae::editor {
namespace {

using namespace ae::ui;

// Medidas em pixels lógicos, tiradas dos masters. Elas vivem aqui, e não em
// ui_theme.h, porque são a forma DESTA tela — o tema carrega o que é comum a
// todas as telas (cor, raio, escala de espaçamento), e misturar as duas coisas
// faria qualquer ajuste de layout parecer uma mudança de sistema visual.
// Medidas em DP, e a distinção importa. Os masters foram desenhados numa tela de
// 1672×941; um telefone real em paisagem tem cerca de 853×394 dp, porque a
// densidade é 3× e não 1,66×. Copiar os números do master daria uma barra
// superior ocupando 16% da altura em vez de 7%, e painéis somando 94% da
// largura. As PROPORÇÕES dos masters são o alvo; os números, não.
constexpr float kTopBarHeight = 56.0f;
constexpr float kDockHeight = 62.0f;
constexpr float kPanelHeaderHeight = 44.0f;
constexpr float kRowHeight = 28.0f;
constexpr float kActionButton = 40.0f;
constexpr float kCornerButton = 44.0f;
constexpr float kToolButton = 48.0f;
constexpr float kIconSize = 20.0f;
constexpr float kFieldHeight = 32.0f;
constexpr float kToggleWidth = 40.0f;
constexpr float kToggleHeight = 22.0f;

// Largura dos painéis como FRAÇÃO da tela, com um piso em dp. A fração mantém a
// proporção dos masters em qualquer aparelho; o piso impede que numa tela
// estreita o painel fique menor do que o próprio conteúdo.
constexpr float kHierarchyFraction = 0.26f;
constexpr float kInspectorFraction = 0.30f;
constexpr float kHierarchyMinimum = 200.0f;
constexpr float kInspectorMinimum = 240.0f;

UiRect inset(const UiRect &rect, float amount) {
  return deflate(rect, UiInsets::all(amount));
}

// Fatia da esquerda, devolvendo o resto por referência. Escrever "pega 48 px e
// anda" é mais legível do que aritmética de x acumulada, e erra menos.
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

struct ScreenBuilder final {
  const EditorScreenState &state;
  const UiTheme &theme;
  UiDrawList &list;
  UiInputRouter &router;

  bool isPressed(u32 widget) const { return state.pressedWidget == widget && widget != 0; }

  // Botão quadrado com um ícone. Devolve o retângulo para quem precisa encadear.
  void iconButton(const UiRect &bounds, UiIcon icon, u32 widget, bool active = false,
                  UiColor tint = 0xFFFFFFFF) {
    const UiColor background = active ? theme.color.accent
                                      : (isPressed(widget) ? theme.color.line : theme.color.raised);
    list.addRect(bounds, background, theme.radius.control);
    const UiColor iconTint = active ? theme.color.accentInk : tint;
    list.addImage(centred(bounds, kIconSize, kIconSize), static_cast<UiImageId>(icon), iconTint);
    router.addRegion(bounds, widget, theme.touch.minimumTarget);
  }

  // Interruptor do Inspector. A pastilha lima ligada é a marca mais repetida
  // dos masters, e a trilha cinza desligada é o que a torna legível.
  void toggle(const UiRect &area, bool on, u32 widget) {
    const UiRect bounds{area.right() - kToggleWidth, area.y + (area.height - kToggleHeight) * 0.5f,
                        kToggleWidth, kToggleHeight};
    const float radius = kToggleHeight * 0.5f;
    list.addRect(bounds, on ? withAlpha(theme.color.accent, 0.28f) : theme.color.line, radius);
    const float knob = kToggleHeight - 6.0f;
    const float knobX = on ? bounds.right() - knob - 3.0f : bounds.x + 3.0f;
    list.addRect({knobX, bounds.y + 3.0f, knob, knob}, on ? theme.color.accent : theme.color.track,
                 knob * 0.5f);
    router.addRegion(bounds, widget, theme.touch.minimumTarget);
  }

  void label(const UiRect &bounds, std::string_view text, UiColor colour,
             const UiTypeStyle &style, UiAlign horizontal = UiAlign::Start) {
    list.addText(bounds, text, colour, style, horizontal, UiAlign::Center);
  }
};

// Ícone que representa o tipo da entidade na hierarquia.
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

void formatValue(char (&buffer)[16], float value, u32 decimals) {
  std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(decimals),
                static_cast<double>(value));
}

void buildTopBar(ScreenBuilder &builder, const UiRect &bar) {
  const UiTheme &theme = builder.theme;
  builder.list.addRect(bar, theme.color.voidBlack);
  UiRect content = deflate(bar, {theme.spacing.large, theme.spacing.small, theme.spacing.large,
                                 theme.spacing.small});

  // Marca: quadrado lima com o glifo, mais o logotipo. O logotipo é imagem, não
  // texto — a fonte da interface não é a da marca.
  const UiRect mark = takeLeft(content, 36.0f);
  builder.list.addRect(mark, theme.color.accent, theme.radius.control);
  builder.list.addImage(centred(mark, 22.0f, 22.0f),
                        static_cast<UiImageId>(UiIcon::SceneObject), theme.color.accentInk);
  takeLeft(content, theme.spacing.small);
  builder.label(takeLeft(content, 96.0f), "ASTRA", theme.color.text, theme.type.cardName);

  takeLeft(content, theme.spacing.large);
  const UiRect divider = takeLeft(content, 1.0f);
  builder.list.addRect({divider.x, divider.y + 10.0f, 1.0f, divider.height - 20.0f},
                       theme.color.line);
  takeLeft(content, theme.spacing.large);

  // Ações à direita, montadas de trás para frente para ficarem ancoradas.
  const UiRect play = takeRight(content, 58.0f);
  builder.list.addRect(play, theme.color.accent, theme.radius.control);
  builder.list.addImage(centred(play, 20.0f, 20.0f),
                        static_cast<UiImageId>(UiIcon::RuntimePlay), theme.color.accentInk);
  builder.router.addRegion(play, widgetId(EditorWidget::PlayFromTopBar), theme.touch.minimumTarget);
  takeRight(content, theme.spacing.small);

  const auto action = [&](UiIcon icon, EditorWidget widget, bool enabled) {
    const UiRect bounds = takeRight(content, kActionButton);
    builder.iconButton(bounds, icon, widgetId(widget), false,
                       enabled ? theme.color.text : theme.color.textFaint);
    takeRight(content, theme.spacing.tiny);
  };
  action(UiIcon::UiMoreVertical, EditorWidget::ProjectMenu, true);
  action(UiIcon::AssetsFolder, EditorWidget::OpenProject, true);
  action(UiIcon::EditorRedo, EditorWidget::Redo, builder.state.canRedo);
  action(UiIcon::EditorUndo, EditorWidget::Undo, builder.state.canUndo);

  // O que sobra é a pastilha da cena, com nome e subtítulo empilhados.
  const UiRect chip = takeLeft(content, std::min(content.width, 220.0f));
  UiRect chipContent = chip;
  const UiRect chipIcon = takeLeft(chipContent, 40.0f);
  builder.list.addImage(centred(chipIcon, kIconSize, kIconSize),
                        static_cast<UiImageId>(UiIcon::SceneObject), theme.color.text);
  const UiRect chevron = takeRight(chipContent, 28.0f);
  builder.list.addImage(centred(chevron, 16.0f, 16.0f),
                        static_cast<UiImageId>(UiIcon::UiChevronDown), theme.color.textDim);
  const float half = chipContent.height * 0.5f;
  builder.label({chipContent.x, chipContent.y + 2.0f, chipContent.width, half},
                builder.state.projectName, theme.color.text, theme.type.cardName);
  builder.label({chipContent.x, chipContent.y + half - 2.0f, chipContent.width, half},
                builder.state.projectSubtitle, theme.color.textDim, theme.type.caption);
  builder.router.addRegion(chip, widgetId(EditorWidget::SceneChip));
}

void buildDock(ScreenBuilder &builder, const UiRect &dock) {
  const UiTheme &theme = builder.theme;
  struct Item final {
    const char *label;
    UiIcon icon;
    EditorWidget widget;
    EditorDock dockValue;
  };
  const Item items[] = {
      {"Add", UiIcon::SceneObjectAdd, EditorWidget::DockAdd, EditorDock::Add},
      {"Assets", UiIcon::SceneObject, EditorWidget::DockAssets, EditorDock::Assets},
      {"Lighting", UiIcon::LightingSun, EditorWidget::DockLighting, EditorDock::Lighting},
      {"Play", UiIcon::RuntimePlay, EditorWidget::DockPlay, EditorDock::Play},
      {"Settings", UiIcon::UiSettings, EditorWidget::DockSettings, EditorDock::Settings},
  };

  constexpr float kItemWidth = 96.0f;
  const float totalWidth = kItemWidth * 5.0f + theme.spacing.small * 2.0f;
  const UiRect bar = centred(dock, totalWidth, kDockHeight);
  builder.list.addRect(bar, theme.color.silhouette, theme.radius.card);
  builder.router.addBlocker(bar);

  UiRect content = deflate(bar, UiInsets::symmetric(theme.spacing.small, theme.spacing.tiny));
  for (const Item &item : items) {
    const UiRect slot = takeLeft(content, kItemWidth);
    const bool active = builder.state.dock == item.dockValue;
    if (active) builder.list.addRect(inset(slot, 2.0f), theme.color.accent, theme.radius.control);
    const UiColor tint = active ? theme.color.accentInk : theme.color.text;
    builder.list.addImage(
        {slot.x + (slot.width - 22.0f) * 0.5f, slot.y + 6.0f, 22.0f, 22.0f},
        static_cast<UiImageId>(item.icon), tint);
    builder.label({slot.x, slot.y + 32.0f, slot.width, 18.0f}, item.label,
                  active ? theme.color.accentInk : theme.color.textDim, theme.type.caption,
                  UiAlign::Center);
    builder.router.addRegion(slot, widgetId(item.widget), theme.touch.minimumTarget);
  }
}

u32 buildHierarchy(ScreenBuilder &builder, const UiRect &panel) {
  const UiTheme &theme = builder.theme;
  const EditorDocument &document = *builder.state.document;
  builder.list.addRect(panel, withAlpha(theme.color.surface, 0.96f), theme.radius.card);
  builder.router.addBlocker(panel);

  UiRect content = deflate(panel, UiInsets::all(theme.spacing.medium));
  UiRect header = takeTop(content, kPanelHeaderHeight);
  const UiRect headerIcon = takeLeft(header, 34.0f);
  builder.list.addImage(centred(headerIcon, kIconSize, kIconSize),
                        static_cast<UiImageId>(UiIcon::SceneObject), theme.color.text);
  const UiRect menu = takeRight(header, 34.0f);
  builder.iconButton(menu, UiIcon::UiMoreVertical, widgetId(EditorWidget::HierarchyMenu));
  takeRight(header, theme.spacing.tiny);
  const UiRect add = takeRight(header, 34.0f);
  builder.iconButton(add, UiIcon::UiAdd, widgetId(EditorWidget::HierarchyAdd));
  builder.label(header, "Hierarchy", theme.color.text, theme.type.cardName);

  // A árvore é percorrida em pré-ordem, pulando a raiz: ela é o documento, não
  // uma linha que o usuário selecione.
  builder.list.pushClip(content);
  u32 rows = 0;
  struct Frame final {
    EditorEntityId entity;
    u32 depth;
  };
  std::vector<Frame> stack;
  const auto children = document.childrenOf(document.root());
  for (usize index = children.size(); index > 0; --index)
    stack.push_back({children[index - 1], 0});

  while (!stack.empty() && content.height >= kRowHeight) {
    const Frame frame = stack.back();
    stack.pop_back();
    const EditorEntity *entity = document.find(frame.entity);
    if (entity == nullptr) continue;

    const UiRect row = takeTop(content, kRowHeight);
    const bool selected = builder.state.selection == frame.entity;
    if (selected) builder.list.addRect(row, theme.color.accent, theme.radius.thumb);
    else if (builder.isPressed(hierarchyRowWidget(frame.entity)))
      builder.list.addRect(row, theme.color.raised, theme.radius.thumb);

    UiRect rowContent = deflate(row, UiInsets::symmetric(theme.spacing.small, 0.0f));
    takeLeft(rowContent, static_cast<float>(frame.depth) * 18.0f);
    const UiColor ink = selected ? theme.color.accentInk : theme.color.text;

    const UiRect twisty = takeLeft(rowContent, 20.0f);
    if (!document.childrenOf(frame.entity).empty())
      builder.list.addImage(centred(twisty, 14.0f, 14.0f),
                            static_cast<UiImageId>(UiIcon::UiChevronDown),
                            selected ? theme.color.accentInk : theme.color.textDim);
    const UiRect kindIcon = takeLeft(rowContent, 28.0f);
    builder.list.addImage(centred(kindIcon, 18.0f, 18.0f),
                          static_cast<UiImageId>(iconForKind(entity->kind)),
                          selected ? theme.color.accentInk : theme.color.accent);

    const UiRect eye = takeRight(rowContent, 30.0f);
    builder.list.addImage(
        centred(eye, 18.0f, 18.0f),
        static_cast<UiImageId>(entity->visible ? UiIcon::SceneVisibility
                                               : UiIcon::SceneVisibilityOff),
        selected ? theme.color.accentInk : theme.color.textDim);
    builder.label(rowContent, entity->name, ink, theme.type.body);

    // A linha inteira PRIMEIRO, o olho DEPOIS. O roteador testa da última região
    // registrada para a primeira, então quem entra depois fica por cima — e o
    // olho precisa ganhar do fundo da linha, senão tocar nele seleciona em vez
    // de alternar a visibilidade.
    builder.router.addRegion(row, hierarchyRowWidget(frame.entity));
    builder.router.addRegion(eye, hierarchyEyeWidget(frame.entity), theme.touch.minimumTarget);
    ++rows;

    const auto entityChildren = document.childrenOf(frame.entity);
    for (usize index = entityChildren.size(); index > 0; --index)
      stack.push_back({entityChildren[index - 1], frame.depth + 1});
  }
  builder.list.popClip();
  return rows;
}

void buildTransformRow(ScreenBuilder &builder, UiRect &content, const char *name,
                       const float values[3], u32 row, u32 decimals) {
  const UiTheme &theme = builder.theme;
  const UiRect line = takeTop(content, kFieldHeight + theme.spacing.small);
  UiRect fields = deflate(line, {0.0f, 0.0f, 0.0f, theme.spacing.small});
  builder.label(takeLeft(fields, 58.0f), name, theme.color.textDim, theme.type.body);

  const UiColor axisColours[3] = {theme.color.axisX, theme.color.axisY, theme.color.axisZ};
  const char *axisNames[3] = {"X", "Y", "Z"};
  const float width = (fields.width - theme.spacing.small * 2.0f) / 3.0f;
  for (u32 axis = 0; axis < 3; ++axis) {
    const UiRect field = takeLeft(fields, width);
    if (axis < 2) takeLeft(fields, theme.spacing.small);
    const u32 widget = transformFieldWidget(row, axis);
    builder.list.addRect(field, builder.isPressed(widget) ? theme.color.line : theme.color.raised,
                         theme.radius.thumb);
    UiRect inner = deflate(field, UiInsets::symmetric(theme.spacing.small, 0.0f));
    builder.label(takeLeft(inner, 10.0f), axisNames[axis], axisColours[axis], theme.type.label);
    char buffer[16];
    formatValue(buffer, values[axis], decimals);
    builder.label(inner, buffer, theme.color.text, theme.type.numeric);
    builder.router.addRegion(field, widget, theme.touch.minimumTarget);
  }
}

void buildInspector(ScreenBuilder &builder, const UiRect &panel) {
  const UiTheme &theme = builder.theme;
  const EditorEntity *entity = builder.state.document->find(builder.state.selection);
  builder.list.addRect(panel, withAlpha(theme.color.surface, 0.96f), theme.radius.card);
  builder.router.addBlocker(panel);
  UiRect content = deflate(panel, UiInsets::all(theme.spacing.medium));

  if (entity == nullptr) {
    builder.label(content, "Nothing selected", theme.color.textMuted, theme.type.body,
                  UiAlign::Center);
    return;
  }

  UiRect header = takeTop(content, kPanelHeaderHeight);
  const UiRect headerIcon = takeLeft(header, 30.0f);
  builder.list.addImage(centred(headerIcon, 20.0f, 20.0f),
                        static_cast<UiImageId>(iconForKind(entity->kind)), theme.color.text);
  const UiRect menu = takeRight(header, 34.0f);
  builder.iconButton(menu, UiIcon::UiMoreVertical, widgetId(EditorWidget::InspectorMenu));
  takeRight(header, theme.spacing.small);
  // A palavra "Active" só entra quando sobra largura para ela E para o nome do
  // objeto. Num painel estreito o interruptor sozinho continua legível; o nome
  // cortado ao meio, não.
  const float nameWidth = builder.list.measure(entity->name, theme.type.cardName);
  const bool roomForWord = header.width - nameWidth > kToggleWidth + 60.0f;
  const UiRect activeArea = takeRight(header, roomForWord ? 78.0f : kToggleWidth + 8.0f);
  builder.toggle(activeArea, entity->active, widgetId(EditorWidget::InspectorActive));
  if (roomForWord)
    builder.label({activeArea.x, activeArea.y,
                   activeArea.width - kToggleWidth - theme.spacing.small, activeArea.height},
                  "Active", theme.color.textDim, theme.type.caption, UiAlign::End);
  const float half = header.height * 0.5f;
  builder.label({header.x, header.y + 2.0f, header.width, half}, entity->name, theme.color.text,
                theme.type.cardName);
  builder.label({header.x, header.y + half - 2.0f, header.width, half}, kindLabel(entity->kind),
                theme.color.textDim, theme.type.label);

  takeTop(content, theme.spacing.small);
  UiRect tabs = takeTop(content, 44.0f);
  struct Tab final {
    const char *label;
    UiIcon icon;
    EditorWidget widget;
    EditorInspectorTab value;
  };
  const Tab tabItems[] = {
      {"Transform", UiIcon::EditorMove, EditorWidget::TabTransform, EditorInspectorTab::Transform},
      {"Material", UiIcon::LightingExposure, EditorWidget::TabMaterial,
       EditorInspectorTab::Material},
      {"Properties", UiIcon::SceneLayers, EditorWidget::TabProperties,
       EditorInspectorTab::Properties},
  };
  const float tabWidth = (tabs.width - theme.spacing.small * 2.0f) / 3.0f;
  for (const Tab &tab : tabItems) {
    const UiRect slot = takeLeft(tabs, tabWidth);
    takeLeft(tabs, theme.spacing.small);
    const bool active = builder.state.tab == tab.value;
    builder.list.addRect(slot, active ? theme.color.accent : theme.color.raised,
                         theme.radius.control);
    const UiColor ink = active ? theme.color.accentInk : theme.color.textDim;
    UiRect inner = deflate(slot, UiInsets::symmetric(theme.spacing.tiny, 0.0f));
    // O ícone só entra quando sobra largura para o rótulo inteiro. Numa tela
    // estreita, "Properties" cortado no meio é pior do que uma aba sem ícone.
    const float labelWidth = builder.list.measure(tab.label, theme.type.caption);
    if (inner.width > labelWidth + 26.0f)
      builder.list.addImage(centred(takeLeft(inner, 20.0f), 15.0f, 15.0f),
                            static_cast<UiImageId>(tab.icon), ink);
    builder.label(inner, tab.label, ink, theme.type.caption, UiAlign::Center);
    builder.router.addRegion(slot, widgetId(tab.widget), theme.touch.minimumTarget);
  }

  takeTop(content, theme.spacing.medium);

  // As abas DIVIDEM o conteúdo, e não decoram o cabeçalho. É a razão de elas
  // existirem nos masters: um telefone em paisagem tem cerca de 240 dp de altura
  // útil no painel, e transform mais interruptores mais camada não cabem juntos.
  // Empilhar tudo e cortar no fim seria esconder controles sem dizer onde estão.
  switch (builder.state.tab) {
    case EditorInspectorTab::Transform: {
      buildTransformRow(builder, content, "Position", entity->transform.position, 0, 3);
      buildTransformRow(builder, content, "Rotation", entity->transform.rotationDegrees, 1, 1);
      buildTransformRow(builder, content, "Scale", entity->transform.scale, 2, 1);
      break;
    }
    case EditorInspectorTab::Material: {
      builder.label(takeTop(content, kRowHeight), "Material", theme.color.textDim,
                    theme.type.label);
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
        builder.list.addImage(centred(takeLeft(row, 26.0f), 16.0f, 16.0f),
                              static_cast<UiImageId>(item.icon), theme.color.textDim);
        builder.toggle(row, item.value, widgetId(item.widget));
        builder.label(row, item.label, theme.color.text, theme.type.body);
        takeTop(content, theme.spacing.tiny);
      }
      break;
    }
  }
}

void buildToolRail(ScreenBuilder &builder, const UiRect &area) {
  const UiTheme &theme = builder.theme;
  struct Tool final {
    UiIcon icon;
    EditorWidget widget;
    EditorGizmoMode mode;
  };
  const Tool tools[] = {
      {UiIcon::EditorMove, EditorWidget::ToolMove, EditorGizmoMode::Translate},
      {UiIcon::EditorRotate, EditorWidget::ToolRotate, EditorGizmoMode::Rotate},
      {UiIcon::EditorScale, EditorWidget::ToolScale, EditorGizmoMode::Scale},
      {UiIcon::EditorSelect, EditorWidget::ToolSelect, EditorGizmoMode::Select},
  };
  const float height = kToolButton * 4.0f + theme.spacing.small * 5.0f;
  const UiRect rail{area.x, area.y + (area.height - height) * 0.5f,
                    kToolButton + theme.spacing.small * 2.0f, height};
  builder.list.addRect(rail, withAlpha(theme.color.silhouette, 0.94f), theme.radius.card);
  builder.router.addBlocker(rail);
  UiRect content = deflate(rail, UiInsets::all(theme.spacing.small));
  for (const Tool &tool : tools) {
    const UiRect slot = takeTop(content, kToolButton);
    takeTop(content, theme.spacing.small);
    builder.iconButton(slot, tool.icon, widgetId(tool.widget), builder.state.tool == tool.mode);
  }
}

} // namespace

EditorScreenLayout buildEditorScreen(const EditorScreenState &state, const UiTheme &theme,
                                     UiDrawList &list, UiInputRouter &router) {
  EditorScreenLayout layout{};
  if (state.document == nullptr || state.surface.isEmpty()) return layout;

  ScreenBuilder builder{state, theme, list, router};
  const UiRect safe = deflate(state.surface, state.safeArea);

  // O viewport é a superfície inteira: os painéis flutuam por cima dele. É o que
  // faz a cena continuar visível em volta da hierarquia, como nos masters.
  layout.viewport = state.surface;

  UiRect remaining = safe;
  layout.topBar = takeTop(remaining, kTopBarHeight);
  buildTopBar(builder, layout.topBar);
  // A barra é opaca e vai até a borda: registrar o bloqueio DEPOIS dos controles
  // dela o deixa por baixo deles na ordem de profundidade, absorvendo apenas o
  // espaço vazio.
  router.addBlocker(layout.topBar);

  UiRect body = deflate(remaining, UiInsets::all(theme.spacing.medium));
  layout.dock = {body.x, body.bottom() - kDockHeight, body.width, kDockHeight};

  UiRect panels = {body.x, body.y, body.width, body.height - kDockHeight - theme.spacing.medium};
  const float hierarchyWidth =
      std::max(kHierarchyMinimum, panels.width * kHierarchyFraction);
  const float inspectorWidth =
      std::max(kInspectorMinimum, panels.width * kInspectorFraction);
  if (state.hierarchyVisible) {
    layout.hierarchyPanel = {panels.x, panels.y, hierarchyWidth, panels.height};
    layout.hierarchyRowCount = buildHierarchy(builder, layout.hierarchyPanel);
  } else {
    buildToolRail(builder, {panels.x, panels.y, kToolButton + theme.spacing.medium, panels.height});
  }
  if (state.inspectorVisible) {
    layout.inspectorPanel = {panels.right() - inspectorWidth, panels.y, inspectorWidth,
                             panels.height};
    buildInspector(builder, layout.inspectorPanel);
  }

  buildDock(builder, layout.dock);

  // Botões de canto: modo de visualização à esquerda, tela cheia à direita.
  const UiRect leftCorner{body.x, layout.dock.y + (kDockHeight - kCornerButton) * 0.5f,
                          kCornerButton, kCornerButton};
  builder.iconButton(leftCorner, UiIcon::AssetsTexture, widgetId(EditorWidget::ViewModeImage));
  builder.iconButton({leftCorner.x + kCornerButton + theme.spacing.small, leftCorner.y,
                      kCornerButton, kCornerButton},
                     UiIcon::SceneObject, widgetId(EditorWidget::ViewModeSolid));
  builder.iconButton({body.right() - kCornerButton, leftCorner.y, kCornerButton, kCornerButton},
                     UiIcon::ViewExpand, widgetId(EditorWidget::Fullscreen));
  return layout;
}

} // namespace ae::editor
