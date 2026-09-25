#include "editor/editor_history.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_screen.h"
#include "harness.h"
#include "renderer/authoring_geometry.h"
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

AE_TEST(graphics_panel_paginates_before_overlapping_apply_on_short_landscape) {
  WaterLab lab;
  Frame frame;
  auto state=waterLabState(lab);
  state.surface={0.0f,0.0f,900.0f,360.0f};
  state.qualityPanel=true;
  state.qualityTab=2;
  composeFrame(frame,state);
  UiRect apply{},lastSetting{};
  bool foundApply=false,foundPager=false;
  for(const auto &command:frame.list.commands()) {
    if(command.kind!=UiPrimitive::Text) continue;
    const auto text=frame.list.textOf(command);
    if(text=="Aplicar"||text=="Aplicado") {apply=command.bounds;foundApply=true;}
    if(text=="Ambiente"||text=="BRDF especular"||text=="Pós-processamento")
      if(command.bounds.bottom()>lastSetting.bottom()) lastSetting=command.bounds;
    if(text.starts_with("1 / ")) foundPager=true;
  }
  AE_EXPECT_TRUE(foundApply&&foundPager,"painel baixo mostra ação fixa e paginação");
  AE_EXPECT_TRUE(lastSetting.bottom()<=apply.y,"controles paginados não invadem Aplicar");
}

AE_TEST(graphics_entry_lives_in_the_editor_top_bar) {
  WaterLab lab;
  Frame frame;
  composeFrame(frame,waterLabState(lab));
  UiRect graphics{};
  bool found=false;
  for(const auto &command:frame.list.commands()) {
    if(command.kind==UiPrimitive::Text&&frame.list.textOf(command)=="Gráficos") {
      graphics=command.bounds;found=true;break;
    }
  }
  AE_EXPECT_TRUE(found,"barra superior oferece Gráficos");
  AE_EXPECT_TRUE(frame.layout.topBar.contains({graphics.x+1.0f,graphics.y+1.0f})&&
                 frame.layout.topBar.contains({graphics.right()-1.0f,graphics.bottom()-1.0f}),
                 "entrada fica na barra do editor, fora do viewport");
}

AE_TEST(graphics_panel_draws_after_viewport_toolbars) {
  WaterLab lab;
  Frame frame;
  auto state=waterLabState(lab);
  state.qualityPanel=true;
  composeFrame(frame,state);
  usize lighting=0,title=0;
  const auto commands=frame.list.commands();
  for(usize index=0;index<commands.size();++index) {
    const auto &command=commands[index];
    if(command.kind==UiPrimitive::Image&&command.image==static_cast<UiImageId>(UiIcon::LightingSceneLighting))
      lighting=index;
    if(command.kind==UiPrimitive::Text&&frame.list.textOf(command)=="Gráficos do projeto") title=index;
  }
  AE_EXPECT_TRUE(lighting>0&&title>lighting,"painel de gráficos cobre a toolbar do viewport em vez de ficar sob ela");
}

AE_TEST(screen_mesh_cooking_group_reports_the_jolt_hull_before_play) {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"cubo");
  EditorDocument document;EditorMapScene resources;
  AE_EXPECT_TRUE(resources.import(document,draws,materials,false,vertices,indices,91),"biblioteca CPU");
  const auto id=document.createEntity(document.root(),EditorEntityKind::Mesh,"Casco de teste");
  auto value=*document.find(id);editMeshRenderer(value)->mesh=1;
  auto *collider=editCollider(value);collider->shape=scene::ColliderShape::Mesh;collider->convex=true;
  const auto instance=collider->instanceId();AE_EXPECT_TRUE(document.applyEntityValues(id,value),"colisor convexo");
  EditorScreenState state{};state.surface={0,0,1600,900};state.document=&document;state.resources=&resources;
  state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Cozimento";
  Frame frame;composeFrame(frame,state);bool summary=false;
  for(const auto &command:frame.list.commands())
    if(frame.list.textOf(command).find("Casco Jolt · 8 vértices · 6 faces")!=std::string_view::npos) summary=true;
  AE_EXPECT_TRUE(summary,"o Inspector explica o resultado efetivo do cooking antes do Play");
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

AE_TEST(component_search_finds_other_categories) {
  WaterLab lab;
  Frame frame;
  auto state=waterLabState(lab);
  state.componentSelection=lab.block;
  state.addingComponent=true;
  state.componentCategory=static_cast<u32>(scene::ComponentCategory::Physics);
  state.componentQuery="camera";
  composeFrame(frame,state);
  UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentAddBase)+5,
                            frame.layout.inspectorPanel,point),
                 "busca encontra Camera mesmo com filtro Física selecionado");
  state.componentQuery="RigidBody3D";
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentAddBase),
                            frame.layout.inspectorPanel,point),
                 "termo de Godot encontra o corpo físico Astra");
  state.componentQuery="Camera3D";
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentAddBase)+5,
                            frame.layout.inspectorPanel,point),
                 "termo de Godot encontra a câmera Astra");
}

AE_TEST(component_preview_shows_schema_defaults_without_editing_document) {
  WaterLab lab;
  Frame frame;
  auto state=waterLabState(lab);
  state.componentSelection=lab.block;
  state.addingComponent=true;
  state.componentPreview=6; // Câmera
  composeFrame(frame,state);
  UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentPreviewValues),
                            frame.layout.inspectorPanel,point),"aba de valores acessível por toque");
  const auto before=lab.history.undoDepth();
  tap(frame,state,lab,point);
  AE_EXPECT_TRUE(state.componentPreviewValues,"toque abre os defaults");
  composeFrame(frame,state);
  bool field=false,value=false;
  for(const auto &command:frame.list.commands()) {
    if(command.kind!=UiPrimitive::Text) continue;
    const auto label=frame.list.textOf(command);
    field|=label=="Campo vertical · conforme modo";
    value|=label=="60 °";
  }
  AE_EXPECT_TRUE(field&&value,"a prévia mostra o FOV inicial declarado pelo componente");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentPreviewComposition),
                            frame.layout.inspectorPanel,point),"composição continua acessível");
  tap(frame,state,lab,point);
  AE_EXPECT_TRUE(!state.componentPreviewValues,"retorno à composição não altera o plano");
  AE_EXPECT_EQ(lab.history.undoDepth(),before,"prévia não cria Undo");
  AE_EXPECT_EQ(lab.document.find(lab.block)->components.size(),0u,"prévia não anexa componentes");

  state.surface={0,0,900,360};
  state.componentPreviewValues=true;
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentPreviewNext),
                            frame.layout.inspectorPanel,point),"valores longos paginam em tela baixa");
  tap(frame,state,lab,point);
  AE_EXPECT_EQ(state.componentPreviewPage,1u,"próxima página responde ao toque");
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentPreviewPrevious),
                            frame.layout.inspectorPanel,point),"página anterior permanece acessível");
}

AE_TEST(component_property_search_crosses_groups_and_keeps_conditional_fields) {
  WaterLab lab;
  auto object=*lab.document.find(lab.block);
  auto *camera=object.components.add(scene::Camera::descriptor);
  AE_EXPECT_TRUE(camera!=nullptr,"câmera anexada ao objeto do teste");
  const auto instance=camera->instanceId();
  AE_EXPECT_TRUE(lab.document.applyEntityValues(lab.block,object),"objeto preparado");
  Frame frame;
  auto state=waterLabState(lab);
  state.componentSelection=lab.block;
  state.expandedNative=instance;
  state.componentGroup="Lente";
  state.propertyQuery="prioridade";
  composeFrame(frame,state);
  UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentNumberBase)+(3u<<8),
                            frame.layout.inspectorPanel,point),"busca encontra campo de outra aba sem trocar a aba salva");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::ComponentNumberBase),
                             frame.layout.inspectorPanel,point),"campo não correspondente fica oculto");
  AE_EXPECT_TRUE(state.componentGroup=="Lente","busca não altera o grupo escolhido");
  state.propertyQuery="vertical_fov";
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentNumberBase),
                            frame.layout.inspectorPanel,point),"id persistente também localiza o campo");
  object=*lab.document.find(lab.block);
  static_cast<scene::Camera *>(object.components.editInstance(instance))->projection=scene::CameraProjection::Orthographic;
  AE_EXPECT_TRUE(lab.document.applyEntityValues(lab.block,object),"projeção ortográfica preparada");
  composeFrame(frame,state);
  bool empty=false;
  for(const auto &command:frame.list.commands())
    empty|=frame.list.textOf(command)=="Nenhuma propriedade encontrada";
  AE_EXPECT_TRUE(empty,"busca respeita visibilidade condicional do descritor");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::ComponentNumberBase),
                             frame.layout.inspectorPanel,point),"FOV oculto não recebe toque");
}

AE_TEST(material_slot_search_keeps_slot_controls_in_the_material_scope) {
  WaterLab lab;Frame frame;auto state=waterLabState(lab);
  auto object=*lab.document.find(lab.block);
  auto *renderer=object.components.add(scene::MeshRenderer::descriptor);
  AE_EXPECT_TRUE(renderer!=nullptr,"renderer preparado");
  const auto instance=renderer->instanceId();
  AE_EXPECT_TRUE(lab.document.applyEntityValues(lab.block,object),"renderer anexado");
  state.componentSelection=lab.block;state.expandedNative=instance;state.meshTab=1;
  state.materialSlotView.slots=1;state.propertyQuery="normal";
  composeFrame(frame,state);
  UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::MaterialTextureBase)+1,
                            frame.layout.inspectorPanel,point),"busca mantém seletor de textura por slot");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::MaterialNormalFlipCycle),
                            frame.layout.inspectorPanel,point),"busca mantém controle de mapa normal");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::MaterialNumberBase)+5,
                            frame.layout.inspectorPanel,point),"busca mantém intensidade no alcance do material");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::MaterialTextureBase),
                             frame.layout.inspectorPanel,point),"binding alheio ao filtro fica oculto");
}

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

AE_TEST(diagnostic_dock_uses_session_console_without_replacing_component_add) {
  WaterLab lab;
  EditorScreenState state=waterLabState(lab);
  EditorConsole console;
  EditorConsoleEntry problem;problem.origin=EditorConsoleOrigin::Compiler;
  problem.severity=EditorConsoleSeverity::Error;problem.message="CS1001: erro de compilação";
  problem.file="Scripts/Player.cs";problem.line=12;console.add(std::move(problem));
  EditorConsoleEntry log;log.origin=EditorConsoleOrigin::Script;
  log.message="Personagem iniciou";console.add(std::move(log));
  state.console=&console;state.consoleProblems=true;state.tab=EditorInspectorTab::Properties;
  const auto historyBefore=lab.history.undoDepth();
  Frame frame;composeFrame(frame,state);UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::DiagnosticDockToggle),frame.layout.topBar,point),
                 "diagnósticos acessíveis pela barra Astra");
  tap(frame,state,lab,point);composeFrame(frame,state);
  AE_EXPECT_TRUE(!frame.layout.diagnosticDock.isEmpty() &&
                 frame.layout.viewport.bottom()<=frame.layout.diagnosticDock.y,
                 "doca larga reserva espaço próprio para o viewport");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleProblems),frame.layout.diagnosticDock,point),
                 "aba Problemas visível na Cena");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleRowBase),frame.layout.diagnosticDock,point),
                 "erro real do console listado");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::AddComponentMenu),frame.layout.inspectorPanel,point),
                 "Add continua no Inspector");
  state.consoleSelected=console.at(0)->eventId;composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleOpenSource),state.surface,point),
                 "detalhe usa a origem do evento");
  AE_EXPECT_EQ(lab.history.undoDepth(),historyBefore,"abrir diagnóstico não edita a cena");
  state.consoleSelected=0;state.consoleProblems=false;composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleRowBase)+1,frame.layout.diagnosticDock,point),
                 "aba Registros mostra saída de script");
}

AE_TEST(diagnostic_dock_compact_sheet_blocks_scene_without_shrinking_viewport) {
  WaterLab lab;
  EditorScreenState state=waterLabState(lab);
  state.surface={0,0,600,360};state.diagnosticDockOpen=true;
  Frame frame;composeFrame(frame,state);
  AE_EXPECT_TRUE(frame.layout.diagnosticDock.width==600 &&
                 frame.layout.viewport.height==308,"sheet compacto ocupa largura da tela");
  const UiPoint under{frame.layout.diagnosticDock.x+4,frame.layout.diagnosticDock.bottom()-4};
  const auto route=frame.router.route({2,UiPointerPhase::Down,under,0.0});
  AE_EXPECT_TRUE(route.target!=UiPointerTarget::Viewport,"toque na sheet não orbita a câmera");
}

AE_TEST(diagnostic_dock_pages_bounded_events_without_copying_their_identity) {
  WaterLab lab;
  EditorScreenState state=waterLabState(lab);
  EditorConsole console;
  for(u32 i=0;i<8;++i) {EditorConsoleEntry event;event.message="Evento "+std::to_string(i);console.add(std::move(event));}
  state.console=&console;state.diagnosticDockOpen=true;
  Frame frame;composeFrame(frame,state);UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleRowBase)+7,
                            frame.layout.diagnosticDock,point),"evento mais recente visível");
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::DiagnosticOlder),
                            frame.layout.diagnosticDock,point),"eventos antigos alcançáveis");
  tap(frame,state,lab,point);composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ConsoleRowBase)+4,
                            frame.layout.diagnosticDock,point),"paginação alcança IDs originais");
  AE_EXPECT_TRUE(!findWidget(frame,widgetId(EditorWidget::ConsoleRowBase)+7,
                             frame.layout.diagnosticDock,point),"página antiga não inclui evento recente");
}

AE_TEST(animation_clip_entries_reorder_and_remove_in_inspector_with_undo) {
  WaterLab lab;
  auto value=*lab.document.find(lab.block);
  auto *animation=static_cast<scene::Animation *>(value.components.add(scene::Animation::descriptor));
  AE_EXPECT_TRUE(animation!=nullptr,"componente Animação criado");
  if(!animation) return;
  const resources::AssetGuid first{11,12},second{21,22};
  const u64 firstId=animation->appendClip(first),secondId=animation->appendClip(second);
  const u64 instance=animation->instanceId();
  AE_EXPECT_TRUE(lab.history.applyValues(lab.document,lab.block,value),"lista autoral aplicada");
  const auto *stored=lab.document.find(lab.block)->components.findInstance(instance);
  u32 componentIndex=0;
  while(componentIndex<lab.document.find(lab.block)->components.size() &&
        lab.document.find(lab.block)->components.at(componentIndex)!=stored) ++componentIndex;
  EditorScreenState state=waterLabState(lab);
  state.componentSelection=lab.block;state.expandedNative=instance;state.componentGroup="Clipes";
  Frame frame;composeFrame(frame,state);UiPoint point{};
  const auto rowId=componentIndex+(1u<<8);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentClipMoveDownBase)+rowId,
                            frame.layout.inspectorPanel,point),"mover clipe está no Inspector");
  tap(frame,state,lab,point);
  const auto *moved=static_cast<const scene::Animation *>(lab.document.find(lab.block)->components.findInstance(instance));
  AE_EXPECT_TRUE(moved && moved->clips[0].id==secondId && moved->clips[1].id==firstId &&
                 moved->clips[1].asset==first,"mover preserva ID e recurso");
  AE_EXPECT_TRUE(lab.history.undo(lab.document),"desfazer reordenação");
  const auto *undone=static_cast<const scene::Animation *>(lab.document.find(lab.block)->components.findInstance(instance));
  AE_EXPECT_TRUE(undone && undone->clips[0].id==firstId && undone->clips[1].id==secondId,"Undo restaura ordem");
  AE_EXPECT_TRUE(lab.history.redo(lab.document),"refazer reordenação");
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentClipRemoveBase)+rowId,
                            frame.layout.inspectorPanel,point),"remover clipe está no Inspector");
  tap(frame,state,lab,point);
  const auto *removed=static_cast<const scene::Animation *>(lab.document.find(lab.block)->components.findInstance(instance));
  AE_EXPECT_TRUE(removed && removed->clips.size()==1 && removed->clips[0].id==firstId,"remoção atinge a entrada visível");
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::ComponentClipAdd),frame.layout.inspectorPanel,point),
                 "adicionar clipe permanece no Inspector");
  tap(frame,state,lab,point);
  const auto *added=static_cast<const scene::Animation *>(lab.document.find(lab.block)->components.findInstance(instance));
  AE_EXPECT_TRUE(added && added->clips.size()==2 && added->clips[1].id>secondId,
                 "nova entrada não reutiliza identidade removida");
}

AE_TEST(scene_view_options_are_real_editor_state_and_expose_effect_parts) {
  WaterLab lab;
  EditorScreenState state=waterLabState(lab);
  const auto historyBefore=lab.history.undoDepth();
  Frame frame;composeFrame(frame,state);UiPoint point{};
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::SceneLightingToggle),frame.layout.viewport,point),
                 "Lighting fica na barra superior do viewport");
  tap(frame,state,lab,point);
  AE_EXPECT_TRUE(!state.sceneLighting,"Lighting altera o estado editorial");

  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::SceneEffectsMenu),frame.layout.viewport,point),
                 "Effects possui menu como a Scene View");
  tap(frame,state,lab,point);
  AE_EXPECT_TRUE(state.sceneEffectsMenu,"menu de efeitos abre");
  composeFrame(frame,state);
  AE_EXPECT_TRUE(findWidget(frame,widgetId(EditorWidget::SceneSkyToggle),frame.layout.viewport,point),
                 "Sky é uma opção funcional");
  tap(frame,state,lab,point);
  AE_EXPECT_TRUE(!state.sceneSky,"Sky alterna sem editar o documento");
  AE_EXPECT_EQ(lab.history.undoDepth(),historyBefore,"opções da vista não criam Undo autoral");
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
