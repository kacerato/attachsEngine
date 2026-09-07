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
  state.projectSubtitle = "Scene";
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

AE_TEST(screen_places_the_viewport_behind_the_floating_panels) {
  // A decisão de layout mais visível dos masters: a cena aparece atrás e ao
  // redor dos painéis. Se o viewport encolhesse, a projeção do gizmo passaria a
  // usar outro retângulo e a seleção erraria o alvo com os painéis abertos.
  WaterLab lab;
  Frame frame;
  const EditorScreenState state = waterLabState(lab);
  composeFrame(frame, state);
  AE_EXPECT_TRUE(frame.layout.viewport.width == state.surface.width, "");
  AE_EXPECT_TRUE(frame.layout.viewport.height == state.surface.height, "");
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.width > 0.0f, "");
  AE_EXPECT_TRUE(frame.layout.inspectorPanel.right() <= state.surface.right(), "");
  AE_EXPECT_TRUE(frame.layout.hierarchyPanel.right() < frame.layout.inspectorPanel.x,
                 "os dois paineis nao se sobrepoem");
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

  const UiPointerRouting scene = frame.router.route(
      {2, UiPointerPhase::Down, {frame.layout.viewport.width * 0.5f, 400.0f}, 0.0});
  AE_EXPECT_TRUE(scene.target == UiPointerTarget::Viewport,
                 "e o vazio entre os paineis continua sendo da cena");
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

AE_TEST(screen_tabs_divide_the_inspector_content) {
  // As abas nao decoram: elas trocam o que o painel mostra. Um telefone nao tem
  // altura para transform e interruptores ao mesmo tempo.
  WaterLab lab;
  Frame transform;
  EditorScreenState state = waterLabState(lab);
  state.surface = {0.0f, 0.0f, 853.0f, 394.0f};
  state.tab = EditorInspectorTab::Transform;
  composeFrame(transform, state);

  Frame properties;
  state.tab = EditorInspectorTab::Properties;
  composeFrame(properties, state);

  const UiRect panel = properties.layout.inspectorPanel;
  const UiPoint probe{panel.right() - 30.0f, panel.y + panel.height * 0.5f};
  const UiPointerRouting onProperties =
      properties.router.route({1, UiPointerPhase::Down, probe, 0.0});
  const UiPointerRouting onTransform =
      transform.router.route({2, UiPointerPhase::Down, probe, 0.0});
  AE_EXPECT_TRUE(onProperties.widgetId != onTransform.widgetId,
                 "o mesmo ponto do painel controla coisas diferentes em cada aba");
}

AE_TEST(screen_dock_and_tabs_are_reachable) {
  WaterLab lab;
  Frame frame;
  composeFrame(frame, waterLabState(lab));

  const UiRect dock = frame.layout.dock;
  const UiPointerRouting play =
      frame.router.route({1, UiPointerPhase::Down, {dock.x + dock.width * 0.5f,
                                                    dock.y + dock.height * 0.5f}, 0.0});
  AE_EXPECT_TRUE(play.target == UiPointerTarget::Widget, "o centro da dock e um item");
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

AE_TEST(screen_hides_the_hierarchy_and_shows_the_tool_rail) {
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
  bool foundAccent = false;
  for (u32 y = 300; y < 600 && !foundAccent; ++y) {
    float sampled[4];
    target.sample(60, y, sampled);
    if (colourDistance(sampled, defaultTheme().color.accent) < 0.12f) foundAccent = true;
  }
  AE_EXPECT_TRUE(foundAccent, "a ferramenta ativa aparece destacada na trilha");
}
