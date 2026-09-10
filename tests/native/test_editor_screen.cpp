#include "editor/editor_history.h"
#include "editor/editor_screen.h"
#include "harness.h"
#include "ui_software_raster.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;
using namespace ae::ui;

namespace {

bool loadAsset(const char *relative, std::vector<u8> &out) {
  const std::string path = std::string(AETHER_REPOSITORY_ROOT) + "/" + relative;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  bool ok = false;
  if (size > 0) {
    out.resize(static_cast<usize>(size));
    ok = std::fread(out.data(), 1, out.size(), file) == out.size();
  }
  std::fclose(file);
  return ok;
}

const UiFont &font() {
  static std::vector<u8> bytes;
  static UiFont value = [] {
    UiFont result;
    if (loadAsset("assets/astra-visual/ui/astra-ui-font.aeuf", bytes)) result.load(bytes);
    return result;
  }();
  return value;
}

const UiIconAtlas &icons() {
  static std::vector<u8> bytes;
  static UiIconAtlas value = [] {
    UiIconAtlas result;
    if (loadAsset("assets/astra-visual/ui/astra-ui-icons.aeui", bytes)) result.load(bytes);
    return result;
  }();
  return value;
}

// A cena dos mockups. Ela é o caso real: uma hierarquia com pastas, tipos
// diferentes e uma seleção — não três retângulos de teste.
struct WaterLab final {
  EditorDocument document;
  EditorHistory history;
  EditorEntityId block = kInvalidEntity;
  EditorEntityId environment = kInvalidEntity;

  WaterLab() {
    environment =
        history.createEntity(document, document.root(), EditorEntityKind::Folder, "Environment");
    history.createEntity(document, environment, EditorEntityKind::Light, "Sky");
    const EditorEntityId architecture =
        history.createEntity(document, document.root(), EditorEntityKind::Folder, "Architecture");
    history.createEntity(document, architecture, EditorEntityKind::Mesh, "Glass Wall");
    block = history.createEntity(document, architecture, EditorEntityKind::Mesh, "Concrete Block");
  }
};

struct Frame final {
  UiDrawList list;
  UiInputRouter router;
  EditorScreenLayout layout{};
  std::vector<UiInstance> instances;
};

// Monta um frame inteiro do editor, como o loop de execução faria.
void composeFrame(Frame &frame, const EditorScreenState &state) {
  frame.list.begin(state.surface, font().metrics(UiFontWeight::Regular));
  frame.router.beginFrame();
  frame.layout = buildEditorScreen(state, defaultTheme(), frame.list, frame.router);
  frame.instances.clear();
  buildUiInstances(frame.list, font(), icons(), 16384, frame.instances);
}

EditorScreenState waterLabState(const WaterLab &lab) {
  EditorScreenState state{};
  state.surface = {0.0f, 0.0f, 1600.0f, 900.0f};
  state.document = &lab.document;
  state.selection = lab.block;
  state.projectName = "Water Lab";
  return state;
}

// Distância entre uma cor amostrada e um token, para dizer "isto é lima".
float colourDistance(const float rgba[4], UiColor token) {
  const float red = static_cast<float>((token >> 16) & 0xffu) / 255.0f;
  const float green = static_cast<float>((token >> 8) & 0xffu) / 255.0f;
  const float blue = static_cast<float>(token & 0xffu) / 255.0f;
  const float dr = rgba[0] - red;
  const float dg = rgba[1] - green;
  const float db = rgba[2] - blue;
  return std::sqrt(dr * dr + dg * dg + db * db);
}

} // namespace

AE_TEST(screen_lays_out_panel_viewport_panel) {
  WaterLab lab;
  Frame frame;
  const EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.width > 0.0f, "");
  AE_EXPECT_TRUE(frame.layout.inspectorPanel.right() <= state.surface.right(), "");
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.right() < frame.layout.inspectorPanel.x,
                 "os dois paineis nao se sobrepoem");
  AE_EXPECT_TRUE(frame.layout.topBar.height > 0.0f, "");
}

AE_TEST(screen_panels_absorb_touches_that_would_orbit_the_camera) {
  // O espaço vazio de um painel não pode cair na cena por trás: a câmera giraria
  // por baixo da interface, que é o defeito clássico de editor mobile.
  WaterLab lab;
  Frame frame;
  composeFrame(frame, waterLabState(lab));

  const UiRect panel = frame.layout.hierarchyPanel;
  const UiPoint empty{panel.x + panel.width * 0.5f, panel.bottom() - 12.0f};
  const UiPointerRouting routing =
      frame.router.route({1, UiPointerPhase::Down, empty, 0.0});
  AE_EXPECT_TRUE(routing.target != UiPointerTarget::Viewport,
                 "o painel absorve o toque");

  const UiRect view = frame.layout.viewport;
  const UiPointerRouting scene = frame.router.route(
      {2, UiPointerPhase::Down,
       {view.x + view.width * 0.7f, view.y + view.height * 0.35f}, 0.0});
  AE_EXPECT_TRUE(scene.target == UiPointerTarget::Viewport,
                 "e o meio da cena, longe da trilha, continua sendo da cena");
}

AE_TEST(screen_hierarchy_rows_and_their_eyes_are_separate_targets) {
  // O olho fica dentro da linha. Tocar nele tem de alternar a visibilidade, e
  // nao selecionar a linha -- e e a ordem de registro que decide isso.
  WaterLab lab;
  Frame frame;
  composeFrame(frame, waterLabState(lab));

  // A primeira linha e encontrada por sondagem, e nao por uma copia das
  // constantes de layout: elas sao privadas do .cpp e mudam junto com o desenho.
  const UiRect panel = frame.layout.hierarchyPanel;
  const u32 wanted = hierarchyRowWidget(lab.environment);
  float rowY = 0.0f;
  for (float y = panel.y; y < panel.bottom(); y += 2.0f) {
    const UiPointerRouting probe =
        frame.router.route({99, UiPointerPhase::Down, {panel.x + panel.width * 0.5f, y}, 0.0});
    frame.router.route({99, UiPointerPhase::Up, {panel.x + panel.width * 0.5f, y}, 0.0});
    if (probe.widgetId == wanted) {
      rowY = y;
      break;
    }
  }
  AE_EXPECT_TRUE(rowY > 0.0f, "a primeira linha da hierarquia responde ao toque");

  const UiPointerRouting eye =
      frame.router.route({2, UiPointerPhase::Down, {panel.right() - 22.0f, rowY}, 0.0});
  AE_EXPECT_EQ(eye.widgetId, hierarchyEyeWidget(lab.environment),
               "o olho esta por cima da linha e ganha o toque");
}

namespace { bool findWidget(Frame &frame,u32 widget,const UiRect &area,UiPoint &out); }
AE_TEST(screen_folded_components_open_into_available_inspector_space) {
  WaterLab lab;
  Frame folded;
  EditorScreenState state = waterLabState(lab);
  state.surface = {0.0f, 0.0f, 853.0f, 394.0f};
  composeFrame(folded,state);UiPoint point{};
  AE_EXPECT_TRUE(findWidget(folded,widgetId(EditorWidget::TransformFold),state.surface,point),"transform header available");
  AE_EXPECT_TRUE(!findWidget(folded,transformFieldWidget(0,0),state.surface,point),"fields start closed");
  state.componentSelection=state.selection;state.expandedComponent="astra.transform";
  Frame expanded;composeFrame(expanded,state);
  AE_EXPECT_TRUE(findWidget(expanded,transformFieldWidget(0,0),state.surface,point),"focused component exposes position");
  AE_EXPECT_TRUE(findWidget(expanded,widgetId(EditorWidget::AddComponentMenu),state.surface,point),"Add stays reachable while editing");
}

namespace {

// Toca e solta num ponto, devolvendo o que a tela fez com isso.
EditorPointerOutcome tap(Frame &frame, EditorScreenState &state, WaterLab &lab, UiPoint at) {
  frame.router.route({1, UiPointerPhase::Down, at, 0.0});
  const UiPointerRouting up = frame.router.route({1, UiPointerPhase::Up, at, 0.0});
  return applyEditorPointer(state, frame.layout, up, lab.document, lab.history);
}

// Centro de um widget conhecido, procurado por sondagem no retangulo dado. Os
// testes nao copiam as constantes de layout do .cpp: elas sao privadas e mudam
// junto com o desenho.
bool findWidget(Frame &frame, u32 widget, const UiRect &area, UiPoint &out) {
  for (float y = area.y + 1.0f; y < area.bottom(); y += 2.0f) {
    for (float x = area.x + 1.0f; x < area.right(); x += 2.0f) {
      const UiPoint point{x, y};
      const UiPointerRouting probe = frame.router.route({98, UiPointerPhase::Down, point, 0.0});
      frame.router.route({98, UiPointerPhase::Up, point, 0.0});
      if (probe.widgetId == widget && probe.target == UiPointerTarget::Widget) {
        out = point;
        return true;
      }
    }
  }
  return false;
}

} // namespace

AE_TEST(screen_scene_menu_separates_authoring_context_from_play) {
  // A doca inferior virou aba superior. Numa tela em paisagem a borda de baixo e
  // a mais cara: e onde o polegar cobre o conteudo e onde a barra de gestos do
  // sistema disputa o toque.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);

  UiPoint at{};
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::TabLighting),frame.layout.topBar,at),"environment is not a top-level tab");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::TabPlay),frame.layout.topBar,at),"no duplicate play tab");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::OpenProject),frame.layout.topBar,at),"no disconnected open command");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ProjectMenu),frame.layout.topBar,at),"scene menu");
  tap(frame,state,lab,at);composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::TabLighting),state.surface,at),"scene environment remains reachable");
  const EditorPointerOutcome outcome = tap(frame, state, lab, at);
  AE_EXPECT_TRUE(outcome.consumed, "");
  AE_EXPECT_TRUE(state.workspace == EditorWorkspace::Lighting, "e trocar de aba troca o contexto");
}

AE_TEST(screen_tapping_a_row_selects_it) {
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  state.selection = kInvalidEntity;
  composeFrame(frame, state);

  UiPoint at{};
  AE_EXPECT_TRUE(findWidget(frame, hierarchyRowWidget(lab.environment),
                            frame.layout.hierarchyPanel, at),
                 "");
  tap(frame, state, lab, at);
  AE_EXPECT_EQ(state.selection, lab.environment, "tocar na linha seleciona a entidade");
}

AE_TEST(screen_tapping_the_eye_toggles_visibility_and_is_undoable) {
  // Tudo que o dedo faz passa pelo historico. Um campo que a interface mudasse
  // direto seria um campo que o Ctrl+Z nao desfaz, sem nada na tela dizendo qual.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);

  const bool before = lab.document.find(lab.environment)->visible;
  UiPoint at{};
  AE_EXPECT_TRUE(findWidget(frame, hierarchyEyeWidget(lab.environment),
                            frame.layout.hierarchyPanel, at),
                 "");
  const EditorPointerOutcome outcome = tap(frame, state, lab, at);
  AE_EXPECT_TRUE(outcome.documentChanged, "");
  AE_EXPECT_TRUE(lab.document.find(lab.environment)->visible != before, "");
  AE_EXPECT_TRUE(lab.history.undo(lab.document), "");
  AE_EXPECT_TRUE(lab.document.find(lab.environment)->visible == before,
                 "e desfazer devolve o estado");
}

AE_TEST(screen_splitter_drag_resizes_the_panel_continuously) {
  // O divisor responde ao ARRASTE, e nao ao soltar: esperar o dedo levantar
  // tornaria impossivel encontrar a largura certa.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);
  const float before = frame.layout.hierarchyPanel.width;

  UiPoint at{};
  const UiRect strip{frame.layout.hierarchyPanel.right(), frame.layout.hierarchyPanel.y, 12.0f,
                     frame.layout.hierarchyPanel.height};
  AE_EXPECT_TRUE(findWidget(frame, widgetId(EditorWidget::SplitterLeft), strip, at), "");

  frame.router.route({1, UiPointerPhase::Down, at, 0.0});
  const UiPointerRouting drag =
      frame.router.route({1, UiPointerPhase::Move, {at.x + 60.0f, at.y}, 0.0});
  applyEditorPointer(state, frame.layout, drag, lab.document, lab.history);
  AE_EXPECT_TRUE(state.hierarchyWidth > before, "arrastar para a direita alarga a hierarquia");

  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.width > before, "e o proximo frame ja mostra isso");
}

AE_TEST(screen_compact_panels_switch_without_mutating_authoring_or_wide_widths) {
  WaterLab lab;Frame frame;auto state=waterLabState(lab);
  EditorFileSystem files;state.files=&files;
  state.hierarchyWidth=260;state.inspectorWidth=280;
  const auto selection=state.selection;const auto revision=lab.document.revision();
  const auto undoDepth=lab.history.undoDepth();
  state.surface={0,0,600,394};composeFrame(frame,state);
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.isEmpty() && frame.layout.inspectorPanel.isEmpty(),"compact starts with viewport");
  AE_EXPECT_EQ(frame.layout.viewport.y,frame.layout.topBar.bottom(),"no permanent panel strip");
  const auto openPanels=[&] {
    UiPoint button{};
    AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::CompactPanelMenu),state.surface,button),"panel menu reachable");
    tap(frame,state,lab,button);composeFrame(frame,state);
  };
  UiPoint at{};
  openPanels();
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::HierarchyToggle),state.surface,at),"hierarchy remains reachable");
  tap(frame,state,lab,at);composeFrame(frame,state);
  AE_EXPECT_EQ(frame.layout.hierarchyPanel.width,240.0f,"readable hierarchy width");
  AE_EXPECT_TRUE(frame.layout.inspectorPanel.isEmpty() && frame.layout.viewport.width>=180,"one side panel only");
  openPanels();
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::InspectorToggle),state.surface,at),"inspector remains reachable");
  tap(frame,state,lab,at);composeFrame(frame,state);
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.isEmpty() && frame.layout.inspectorPanel.width==240,"switch to inspector");
  openPanels();
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::CompactFiles),state.surface,at),"files reachable even with short body");
  tap(frame,state,lab,at);state.surface.height=263;composeFrame(frame,state);
  AE_EXPECT_TRUE(!frame.layout.filesPanel.isEmpty() && frame.layout.hierarchyPanel.isEmpty(),"files use entire side panel");
  for(auto widget:{EditorWidget::ToolSelect,EditorWidget::ToolMove,EditorWidget::ToolRotate,EditorWidget::ToolScale})
    AE_EXPECT_TRUE(findWidget(frame,widgetId(widget),frame.layout.viewport,at),"all compact tools remain touchable inside viewport");
  openPanels();
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::CompactViewport),state.surface,at),"viewport return is always reachable");
  tap(frame,state,lab,at);composeFrame(frame,state);
  AE_EXPECT_EQ(frame.layout.viewport.width,600.0f,"return releases all body width");
  state.surface={0,0,1100,600};composeFrame(frame,state);
  AE_EXPECT_EQ(frame.layout.hierarchyPanel.width,260.0f,"wide hierarchy restored");
  AE_EXPECT_EQ(frame.layout.inspectorPanel.width,280.0f,"wide inspector restored");
  AE_EXPECT_EQ(state.selection,selection,"selection preserved");
  AE_EXPECT_EQ(lab.document.revision(),revision,"layout does not edit document");
  AE_EXPECT_EQ(lab.history.undoDepth(),undoDepth,"layout does not enter history");
}

AE_TEST(screen_the_viewport_never_disappears_between_the_panels) {
  // Arrastar os dois divisores ate o meio nao pode deixar o editor sem cena e
  // sem como voltar atras.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  state.surface = {0.0f, 0.0f, 600.0f, 394.0f};
  state.hierarchyWidth = 5000.0f;
  state.inspectorWidth = 5000.0f;
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.viewport.width >= 150.0f,
                 "sobra viewport mesmo com os dois paineis pedindo tudo");
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.right() <= frame.layout.viewport.x, "");
}

AE_TEST(screen_panels_shrink_the_viewport_instead_of_covering_it) {
  // Com os paineis flutuando, metade do enquadramento vivia atras deles. Agora
  // a area visivel e a area utilizavel.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.viewport.width < state.surface.width,
                 "o viewport nao ocupa a tela inteira");
  AE_EXPECT_TRUE(frame.layout.viewport.x >= frame.layout.hierarchyPanel.right(), "");
  AE_EXPECT_TRUE(frame.layout.viewport.right() <= frame.layout.inspectorPanel.x, "");
}

AE_TEST(screen_selected_row_is_painted_with_the_accent) {
  // A prova de que o realce chega ao pixel, e nao so a lista de comandos.
  WaterLab lab;
  Frame frame;
  const EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);

  test::UiSoftwareTarget target;
  target.resize(1600, 900, 0.16f, 0.18f, 0.20f);
  test::rasterizeUi(frame.instances, font(), icons(), target);

  // Procura a faixa lima dentro do painel, sem depender de qual linha da arvore
  // e a selecionada nem da altura de linha do momento.
  const UiRect panel = frame.layout.hierarchyPanel;
  bool foundAccent = false;
  for (float y = panel.y; y < panel.bottom() && !foundAccent; y += 1.0f) {
    float sampled[4];
    target.sample(static_cast<u32>(panel.x + panel.width * 0.5f), static_cast<u32>(y), sampled);
    if (colourDistance(sampled, defaultTheme().color.accent) < 0.12f) foundAccent = true;
  }
  AE_EXPECT_TRUE(foundAccent, "a linha selecionada e lima ate o pixel");
}

AE_TEST(screen_play_button_is_painted_with_the_accent) {
  WaterLab lab;
  Frame frame;
  composeFrame(frame, waterLabState(lab));
  test::UiSoftwareTarget target;
  target.resize(1600, 900, 0.16f, 0.18f, 0.20f);
  test::rasterizeUi(frame.instances, font(), icons(), target);

  float sampled[4];
  // Canto superior direito, dentro da pastilha de Play e fora do triangulo.
  target.sample(static_cast<u32>(frame.layout.topBar.right() - 24.0f),
                static_cast<u32>(frame.layout.topBar.y + 18.0f), sampled);
  AE_EXPECT_TRUE(colourDistance(sampled, defaultTheme().color.accent) < 0.12f, "");
}

AE_TEST(screen_draws_glyphs_for_every_visible_label) {
  // Um painel sem texto e o sintoma de atlas ausente ou de metrica errada, e ele
  // e invisivel em qualquer asercao que so olhe retangulos.
  WaterLab lab;
  Frame frame;
  composeFrame(frame, waterLabState(lab));
  u32 glyphs = 0;
  for (const UiInstance &instance : frame.instances)
    if (static_cast<u32>(instance.params[2] + 0.5f) == static_cast<u32>(UiInstanceKind::Glyph))
      ++glyphs;
  AE_EXPECT_TRUE(glyphs > 100, "a tela cheia do editor tem centenas de glifos");
}

AE_TEST(screen_without_a_selection_says_so_instead_of_drawing_an_empty_panel) {
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  state.selection = kInvalidEntity;
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.inspectorPanel.width > 0.0f, "o painel continua la");
  u32 glyphs = 0;
  for (const UiInstance &instance : frame.instances)
    if (static_cast<u32>(instance.params[2] + 0.5f) == static_cast<u32>(UiInstanceKind::Glyph))
      ++glyphs;
  AE_EXPECT_TRUE(glyphs > 20, "e diz o que esta acontecendo");
}

AE_TEST(screen_survives_a_surface_too_small_for_its_panels) {
  // Um aparelho estreito, ou a janela do modo dividido. Nada pode desenhar com
  // retangulo invertido nem entrar em laco.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  state.surface = {0.0f, 0.0f, 320.0f, 200.0f};
  composeFrame(frame, state);
  for (const UiInstance &instance : frame.instances) {
    AE_EXPECT_TRUE(instance.bounds[2] >= 0.0f && instance.bounds[3] >= 0.0f,
                   "nenhuma instancia com tamanho negativo");
    AE_EXPECT_TRUE(instance.clip[2] >= 0.0f && instance.clip[3] >= 0.0f, "");
  }
}

AE_TEST(screen_without_a_document_produces_nothing) {
  Frame frame;
  EditorScreenState state{};
  state.surface = {0.0f, 0.0f, 1600.0f, 900.0f};
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.instances.empty(), "");
  AE_EXPECT_TRUE(frame.layout.viewport.isEmpty(),
                 "sem documento nao ha nem viewport: quem chamou tem de notar");
}

AE_TEST(screen_hides_the_panels_and_keeps_the_tool_rail) {
  // O mockup do modo Add troca o painel pela trilha de ferramentas.
  WaterLab lab;
  Frame frame;
  EditorScreenState state = waterLabState(lab);
  state.hierarchyVisible = false;
  state.tool = EditorGizmoMode::Rotate;
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.isEmpty(), "");
  AE_EXPECT_EQ(frame.layout.hierarchyRowCount, 0u, "");

  test::UiSoftwareTarget target;
  target.resize(1600, 900, 0.16f, 0.18f, 0.20f);
  test::rasterizeUi(frame.instances, font(), icons(), target);
  // A ferramenta ativa e a unica pastilha lima da trilha; ela fica na segunda
  // posicao porque a ordem e mover, girar, escalar, selecionar.
  // A trilha vive na borda esquerda do viewport, cuja posicao depende de haver
  // paineis. Varrer a faixa inteira evita repetir a aritmetica de layout aqui.
  const UiRect view = frame.layout.viewport;
  bool foundAccent = false;
  for (u32 y = static_cast<u32>(view.y); y < static_cast<u32>(view.bottom()) && !foundAccent; ++y)
    for (u32 x = static_cast<u32>(view.x); x < static_cast<u32>(view.x + 70.0f); ++x) {
      float sampled[4];
      target.sample(x, y, sampled);
      if (colourDistance(sampled, defaultTheme().color.accent) < 0.12f) {
        foundAccent = true;
        break;
      }
    }
  AE_EXPECT_TRUE(foundAccent, "a ferramenta ativa aparece destacada na trilha");
}
