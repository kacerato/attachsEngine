#include "editor/editor_creation_catalog.h"
#include "editor/editor_scene_camera.h"
#include "editor/editor_session.h"
#include "harness.h"
#include "renderer/water_authoring_geometry.h"
#include "editor/editor_water_play.h"
#include "editor/editor_properties.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace ae;
using namespace ae::editor;
using namespace ae::ui;

AE_TEST(editor_filesystem_browses_real_project_and_rejects_escape) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("aether-files-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  AE_EXPECT_TRUE(fs::create_directory(root),"pasta exclusiva do teste");
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};
  fs::create_directory(root/"Cenas");
  std::ofstream(root/"projeto.engine")<<"projeto";
  std::ofstream(root/"Cenas"/"teste.aescene")<<"cena";
  EditorFileSystem files;
  AE_EXPECT_TRUE(files.setRoot(root.string().c_str()),"abre projeto real");
  AE_EXPECT_EQ(files.entries().size(),2u,"arquivos existentes apenas");
  AE_EXPECT_TRUE(files.entries()[0].directory && files.entries()[0].name=="Cenas","pastas primeiro");
  AE_EXPECT_TRUE(files.open("Cenas"),"navega");
  AE_EXPECT_EQ(files.entries().size(),1u,"lista diretório atual");
  AE_EXPECT_TRUE(!files.resolveFile("Cenas/teste.aescene").empty(),"resolve cena real");
  AE_EXPECT_TRUE(!files.open(".."),"não sai do projeto");
  AE_EXPECT_TRUE(files.current()=="Cenas" && files.entries().size()==1,"falha preserva navegação");
  AE_EXPECT_TRUE(files.resolveFile("../fora.aescene").empty(),"não resolve arquivo externo");
  AE_EXPECT_TRUE(files.up() && files.current().empty(),"volta à raiz");
  AE_EXPECT_TRUE(files.up() && files.current().empty(),"raiz permanece raiz");
  fs::remove(root/"projeto.engine");
  AE_EXPECT_TRUE(files.refresh() && files.entries().size()==1,"atualização reflete disco");
  UiFont font;UiIconAtlas icons;
  EditorSession session;session.initialize(&font,&icons);
  AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"conecta sessão");
  session.setSurface({0,0,1100,600},{});session.update();
  AE_EXPECT_TRUE(!session.layout().filesPanel.isEmpty(),"painel real visível");
  AE_EXPECT_TRUE(session.layout().filesPanel.y>=session.layout().hierarchyPanel.bottom(),"arquivos abaixo da cena");
  AE_EXPECT_TRUE(session.layout().viewport.x>=session.layout().filesPanel.right(),"arquivos não cobrem viewport");
  const auto panel=session.layout().filesPanel;
  const UiPoint row{panel.x+40,panel.y+68};
  session.handlePointer({1,UiPointerPhase::Down,row,1});
  session.handlePointer({1,UiPointerPhase::Up,row,1.1});session.update();
  AE_EXPECT_TRUE(session.screen().files->tree().size()==3 && session.screen().files->tree()[1].expanded,"toque expande a pasta sem perder a raiz");
  const UiPoint fileRow{row.x,row.y+24};
  session.handlePointer({1,UiPointerPhase::Down,fileRow,2});
  session.handlePointer({1,UiPointerPhase::Up,fileRow,2.1});
  AE_EXPECT_TRUE(!session.requestedScenePath().empty(),"toque solicita abertura da cena real");
  session.clearSceneOpenRequest();
  const UiPoint collapse{panel.right()-19,panel.y+20};
  session.handlePointer({1,UiPointerPhase::Down,collapse,3});
  session.handlePointer({1,UiPointerPhase::Up,collapse,3.1});session.update();
  AE_EXPECT_TRUE(session.screen().filesCollapsed && session.layout().filesPanel.height==40,"recolhe sem perder acesso");
}

AE_TEST(editor_external_commands_share_history_and_reject_stale_scene) {
  EditorSession session;
  const auto id=session.history().createEntity(session.document(),session.document().root(),EditorEntityKind::Mesh,"Objeto");
  EditorActionRequest request;
  request.version=session.sceneVersion();request.entity=id;
  request.action=EditorAction::NumericProperty;request.property=0;request.number=3;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Applied,"edita o documento real");
  AE_EXPECT_TRUE(session.document().find(id)->transform.position[0]==3,"posição publicada");
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::StaleScene,"não aplica painel desatualizado");
  request.version=session.sceneVersion();request.action=EditorAction::Undo;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Applied,"mesmo histórico");
  AE_EXPECT_TRUE(session.document().find(id)->transform.position[0]==0,"desfaz propriedade");
  request.version=session.sceneVersion();request.action=EditorAction::Redo;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Applied,"refaz");
  AE_EXPECT_TRUE(session.document().find(id)->transform.position[0]==3,"restaura propriedade");
}

AE_TEST(editor_external_commands_reject_invalid_values_and_scene_epoch) {
  EditorSession session;
  const auto id=session.history().createEntity(session.document(),session.document().root(),EditorEntityKind::Folder,"Grupo");
  EditorActionRequest request;
  request.version=session.sceneVersion();request.entity=id;
  request.action=EditorAction::NumericProperty;request.property=6;request.number=-1;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::InvalidValue,"escala inválida");
  AE_EXPECT_TRUE(session.document().find(id)->transform.scale[0]==1,"não altera dados");
  session.history().begin("Arraste");
  request.action=EditorAction::Remove;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Busy,"não interrompe gesto");
  session.history().end();
  const auto previous=session.sceneVersion();
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"troca documento");
  request.version=previous;request.action=EditorAction::Select;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::StaleScene,"IDs antigos não selecionam outra cena");
}

AE_TEST(editor_external_commands_do_not_cross_sessions_with_reused_ids) {
  EditorSession previous,current;
  EditorActionRequest request;
  request.version=previous.sceneVersion();request.entity=current.document().root();
  AE_EXPECT_TRUE(current.dispatch(request).status==EditorActionStatus::StaleScene,"painel da sessão anterior não controla outra sessão");
}

AE_TEST(water_route_geometry_queries_and_end_caps_agree) {
  renderer::WaterRoute route;route.count=2;
  route.points[0].position[2]=-10;route.points[1].position[2]=10;route.points[1].position[1]=2;
  route.points[0].width=4;route.points[1].width=8;route.points[1].speed=3;
  renderer::WaterRouteSample sample;
  AE_EXPECT_TRUE(renderer::sampleWaterRoute(route,{0,0},sample),"center is wet");
  AE_EXPECT_TRUE(std::abs(sample.center.y-1)<.01f && std::abs(sample.width-6)<.01f,"height and width interpolate");
  AE_EXPECT_TRUE(!renderer::sampleWaterRoute(route,{0,-10.1f},sample),"no invisible water beyond flat start cap");
  AE_EXPECT_TRUE(!renderer::sampleWaterRoute(route,{0,10.1f},sample),"no invisible water beyond end cap");
  AE_EXPECT_TRUE(!renderer::sampleWaterRoute(route,{3.1f,0},sample),"bank is dry");
  std::vector<renderer::WaterRouteVertex> vertices;std::vector<u32> indices;
  AE_EXPECT_TRUE(renderer::buildWaterRouteMesh(route,vertices,indices),"tessellate real route");
  for(usize i=0;i<indices.size();i+=3) {
    const auto &a=vertices[indices[i]],&b=vertices[indices[i+1]],&c=vertices[indices[i+2]];
    const float y=(b.position[2]-a.position[2])*(c.position[0]-a.position[0])-(b.position[0]-a.position[0])*(c.position[2]-a.position[2]);
    AE_EXPECT_TRUE(y>0,"all river triangles face upward");
  }
  route.points[1]=route.points[0];
  AE_EXPECT_TRUE(!renderer::buildWaterRouteMesh(route,vertices,indices),"degenerate route rejected transactionally");
}

AE_TEST(editor_authored_river_roundtrip_current_and_real_jolt_buoyancy) {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"resources");
  EditorSession session;AE_EXPECT_TRUE(session.importMap(draws,materials,false),"empty document");
  const float origin[]{0,0,0},above[]{0,3,0};
  const auto river=session.instantiateAsset(2,session.document().root(),origin);
  auto value=*session.document().find(river);value.route.count=2;
  value.route.points[0].position[0]=value.route.points[1].position[0]=0;
  value.route.points[0].position[2]=-50;value.route.points[1].position[2]=50;
  value.route.points[0].speed=value.route.points[1].speed=2;
  value.waterBody[3]=0;value.route.points[0].width=value.route.points[1].width=12;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),river,value),"edit river parameters");
  const auto body=session.instantiateAsset(3,session.document().root(),above);
  AE_EXPECT_TRUE(body && session.document().find(body)->rigidBodyEnabled,"explicit physical primitive");
  EditorDocument restored;const auto archive=serializeEditorDocument(session.document(),91);
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,91,restored),"v6 scene reload");
  AE_EXPECT_EQ(restored.find(river)->route.count,2u,"route point count survives");
  AE_EXPECT_EQ(restored.find(river)->route.points[1].speed,2.0f,"per point flow survives");
  AE_EXPECT_TRUE(restored.find(body)->rigidBodyEnabled,"physics component survives");
  renderer::WaterWorld world;renderer::WaterFieldSetup setup;
  AE_EXPECT_TRUE(extractEditorWaterWorld(restored,setup,world),"extract generic authored volumes");
  const renderer::WaterVec2 positions[]{{0,0},{20,0}};renderer::WaterVolumeQuery queries[2];
  AE_EXPECT_TRUE(world.sample(positions,0,~0u,queries),"query authored river");
  AE_EXPECT_EQ(queries[0].volume,river,"stable water identity");
  AE_EXPECT_EQ(queries[0].surface.flow.y,2.0f,"river drives current");
  AE_EXPECT_EQ(queries[1].volume,renderer::InvalidWaterVolume,"outside bank remains dry");
  std::vector<renderer::MapDrawState> output;AE_EXPECT_TRUE(session.extractMap(output),"render projection");
  EditorWaterPlay play;
  AE_EXPECT_TRUE(play.start(restored,output,setup,{},renderer::WaterSpectralControls{},1000),"real Jolt world");
  for(u32 frame=0;frame<=600;++frame) AE_EXPECT_TRUE(play.update(frame/60.0,output),"fixed steps and ripple propagation");
  const auto found=std::find_if(output.begin(),output.end(),[&](const auto &draw){return draw.objectId==body;});
  AE_EXPECT_TRUE(found!=output.end(),"body drawable exists");
  AE_EXPECT_TRUE(found->pose.draw.model[13]>-.5f && found->pose.draw.model[13]<1,"body floats instead of sinking");
  AE_EXPECT_TRUE(found->pose.draw.model[14]>.5f,"current transports body");
  AE_EXPECT_EQ(restored.find(body)->transform.position[1],3.0f,"Play preserves edit transform");
  play.stop();AE_EXPECT_TRUE(!play.active(),"world lifecycle closed");
}

AE_TEST(editor_river_physics_preserves_nonuniform_width_and_rotated_flow) {
  EditorDocument document;const auto river=document.createEntity(document.root(),EditorEntityKind::Water,"River");
  auto value=*document.find(river);value.route.count=2;value.route.points[0].position[2]=-10;value.route.points[1].position[2]=10;
  value.transform.position[0]=10;value.transform.position[1]=4;value.transform.position[2]=20;
  value.transform.rotationDegrees[1]=90;value.transform.scale[0]=.5f;value.transform.scale[2]=3;
  AE_EXPECT_TRUE(document.applyEntityValues(river,value),"author transform");
  renderer::WaterWorld world;AE_EXPECT_TRUE(extractEditorWaterWorld(document,{},world),"extract scaled volume");
  const renderer::WaterVec2 points[]{{10,20},{10,21.6f}};renderer::WaterVolumeQuery samples[2];
  AE_EXPECT_TRUE(world.sample(points,0,~0u,samples),"query rotated ribbon");
  AE_EXPECT_EQ(samples[0].volume,river,"center is wet");
  AE_EXPECT_TRUE(std::abs(samples[0].surface.height-4)<.0001f,"height follows transform");
  AE_EXPECT_TRUE(samples[0].surface.flow.x>.99f,"current rotates while preserving metres per second");
  AE_EXPECT_EQ(samples[1].volume,renderer::InvalidWaterVolume,"narrow axis stays narrow in physics");
}

AE_TEST(water_generated_geometry_is_reusable_and_not_automatically_instantiated) {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"generate actual buffers");
  AE_EXPECT_EQ(draws.size(),4u,"finite, ocean, river and box resources");
  AE_EXPECT_EQ(vertices.size(),(2u*33u*33u+24u)*48u,"packed vertices");
  for(const auto &draw:draws) {
    for(u32 i=0;i<draw.indexCount;++i)
      AE_EXPECT_TRUE(indices[draw.firstIndex+i]<33u*33u,"local index stays in geometry");
  }
  float corner[3];std::memcpy(corner,vertices.data(),sizeof(corner));
  AE_EXPECT_EQ(corner[0],-10.0f,"finite extent");AE_EXPECT_EQ(corner[2],-10.0f,"finite extent Z");
  EditorSession session;
  AE_EXPECT_TRUE(session.importMap(draws,materials,true),"import resources");
  AE_EXPECT_EQ(session.document().entityCount(),1u,"library creates no ghost water");
  const auto finite=session.createWaterSurface(false);
  AE_EXPECT_TRUE(finite!=0,"create finite surface without demo mesh");
  AE_EXPECT_TRUE(session.document().find(finite)->kind==EditorEntityKind::Water,"water entity");
  const auto ocean=session.createWaterSurface(true);
  AE_EXPECT_TRUE(ocean!=0,"create camera grid");
  std::vector<renderer::MapDrawState> output;
  AE_EXPECT_TRUE(session.extractMap(output),"extract drawable geometry");
  AE_EXPECT_TRUE(output[0].visible && output[1].visible,"both authored surfaces rendered");
  const auto archive=serializeEditorDocument(session.document(),81);EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,81,restored),"save/reload water references and transforms");
  AE_EXPECT_EQ(restored.find(ocean)->assetId,2u,"stable resource selection");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo generation");
  AE_EXPECT_TRUE(session.extractMap(output),"extract undo");
  AE_EXPECT_TRUE(output[0].visible && !output[1].visible,"undo removes only ocean instance");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo generation");
  const auto size=vertices.size();
  AE_EXPECT_TRUE(!renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,7,vertices,indices,draws,materials),"invalid density rejected");
  AE_EXPECT_EQ(vertices.size(),size,"failed generation preserves buffers");
}

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

struct Fixture final {
  EditorSession session;
  EditorEntityId cube = kInvalidEntity;

  Fixture() {
    session.initialize(&font(), &icons());
    session.setSurface({0.0f, 0.0f, 853.0f, 394.0f}, {});
    renderer::MapDrawRecord mesh{};
    mesh.model[0]=mesh.model[5]=mesh.model[10]=mesh.model[15]=1;
    mesh.boundsRadius=.75f;mesh.indexCount=36;
    session.importMap({&mesh,1},{},false);
    EditorDocument &document = session.document();
    EditorHistory &history = session.history();
    const EditorEntityId folder =
        history.createEntity(document, document.root(), EditorEntityKind::Folder, "Scene");
    cube = history.createEntity(document, folder, EditorEntityKind::Mesh, "Cube");
    auto value=*document.find(cube);value.assetId=1;document.applyEntityValues(cube,value);
    session.update();
  }

  void down(u32 id, UiPoint at) { session.handlePointer({id, UiPointerPhase::Down, at, 0.0}); }
  void move(u32 id, UiPoint at) { session.handlePointer({id, UiPointerPhase::Move, at, 0.0}); }
  void up(u32 id, UiPoint at) { session.handlePointer({id, UiPointerPhase::Up, at, 0.0}); }

  UiPoint viewportCentre() const {
    const UiRect view = session.layout().viewport;
    return {view.x + view.width * 0.5f, view.y + view.height * 0.5f};
  }
};

} // namespace

AE_TEST(session_starts_empty_and_library_loading_does_not_create_objects) {
  EditorSession session;session.initialize(&font(),&icons());
  AE_EXPECT_EQ(session.document().entityCount(),1u,"only the scene root");
  renderer::MapDrawRecord draw{};draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;
  AE_EXPECT_TRUE(session.importMap({&draw,1},{},false),"load library without instantiation");
  AE_EXPECT_EQ(session.document().entityCount(),1u,"no automatic example objects");
  std::vector<renderer::MapDrawState> result;AE_EXPECT_TRUE(session.extractMap(result),"extract empty scene");
  AE_EXPECT_TRUE(!result[0].visible,"library does not render by itself");
}


AE_TEST(session_produces_instances_for_a_full_frame) {
  Fixture fixture;
  AE_EXPECT_TRUE(!fixture.session.instances().empty(), "");
  AE_EXPECT_TRUE(fixture.session.layout().viewport.width > 0.0f, "");
  AE_EXPECT_TRUE(isViewportValid(fixture.session.view()),
                 "a camera do editor produz uma vista utilizavel ja no primeiro frame");
}

AE_TEST(session_one_finger_on_the_scene_orbits_the_camera) {
  Fixture fixture;
  const float yawBefore = fixture.session.camera().yaw;
  const UiPoint centre = fixture.viewportCentre();
  fixture.down(1, centre);
  fixture.move(1, {centre.x + 80.0f, centre.y});
  AE_EXPECT_TRUE(fixture.session.camera().yaw != yawBefore, "arrastar um dedo gira a camera");
  fixture.up(1, {centre.x + 80.0f, centre.y});
}

AE_TEST(session_two_fingers_pan_and_zoom) {
  Fixture fixture;
  const float distanceBefore = fixture.session.camera().distance;
  const UiRect view = fixture.session.layout().viewport;
  const UiPoint left{view.x + view.width * 0.35f, view.y + view.height * 0.5f};
  const UiPoint right{view.x + view.width * 0.65f, view.y + view.height * 0.5f};
  fixture.down(1, left);
  fixture.down(2, right);
  // Afastar os dedos aproxima a camera.
  fixture.move(1, {left.x - 40.0f, left.y});
  fixture.move(2, {right.x + 40.0f, right.y});
  AE_EXPECT_TRUE(fixture.session.camera().distance < distanceBefore,
                 "a pinca de abrir aproxima");
  fixture.up(1, left);
  fixture.up(2, right);
}

AE_TEST(session_a_tap_on_empty_space_clears_the_selection) {
  // O gesto que todo editor tem. Sem ele nao ha como desmarcar sem selecionar
  // outra coisa.
  Fixture fixture;
  EditorDocument &document = fixture.session.document();
  EditorHistory &history = fixture.session.history();
  history.setTransform(document, fixture.cube, EditorTransform{});
  fixture.session.update();

  const UiRect view = fixture.session.layout().viewport;
  const UiPoint corner{view.x + 6.0f, view.bottom() - 6.0f};
  fixture.down(9, corner);
  fixture.up(9, corner);
  AE_EXPECT_EQ(fixture.session.selection(), kInvalidEntity, "");
}

AE_TEST(session_a_tap_on_an_object_selects_it) {
  Fixture fixture;
  EditorDocument &document = fixture.session.document();
  EditorHistory &history = fixture.session.history();
  // Coloca o cubo exatamente no alvo da orbita, que projeta no centro da vista.
  EditorTransform transform{};
  transform.position[1] = 0.5f;
  history.setTransform(document, fixture.cube, transform);
  fixture.session.update();

  const UiPoint centre = fixture.viewportCentre();
  fixture.down(1, centre);
  fixture.up(1, centre);
  AE_EXPECT_EQ(fixture.session.selection(), fixture.cube,
               "o raio do toque acerta o objeto sob o dedo");
}

AE_TEST(session_dragging_the_gizmo_moves_the_object_in_one_undo_step) {
  // O caso que justifica transacoes no historico: centenas de eventos de
  // movimento nao podem virar centenas de Ctrl+Z.
  Fixture fixture;
  EditorDocument &document = fixture.session.document();
  EditorHistory &history = fixture.session.history();
  EditorTransform start{};
  start.position[1] = 0.5f;
  history.setTransform(document, fixture.cube, start);

  // Seleciona pela hierarquia, para nao depender da projecao.
  fixture.session.setSelection(fixture.cube);
  AE_EXPECT_EQ(fixture.session.selection(), fixture.cube, "");
  fixture.session.update();

  // Procura a alca de um eixo do gizmo dentro da vista.
  // A sondagem NAO pode soltar o dedo no viewport: um toque curto no vazio
  // limpa a selecao -- comportamento correto -- e o gizmo sumiria junto. Cada
  // tentativa e cancelada em vez de solta.
  const UiRect view = fixture.session.layout().viewport;
  UiPoint handle{};
  bool found = false;
  for (float y = view.y; y < view.bottom() && !found; y += 3.0f)
    for (float x = view.x; x < view.right(); x += 3.0f) {
      const UiPoint at{x, y};
      fixture.down(7, at);
      const bool grabbed = fixture.session.screen().activeGizmoAxis != EditorGizmoHandle::None;
      fixture.session.cancelPointers();
      if (grabbed) {
        handle = at;
        found = true;
        break;
      }
    }
  AE_EXPECT_TRUE(found, "o gizmo tem alcas alcancaveis no viewport");

  // O arraste tem de ser AO LONGO do eixo pego. Arrastar na horizontal um eixo
  // que aponta para cima na tela projeta zero -- comportamento certo do gizmo, e
  // que faria este teste falhar por medir a coisa errada. O objeto esta no alvo
  // da orbita, entao a origem do gizmo cai no centro da vista, e a direcao do
  // eixo e simplesmente a alca menos esse centro.
  const UiPoint centre = fixture.viewportCentre();
  const float axisX = handle.x - centre.x;
  const float axisY = handle.y - centre.y;
  const float axisLength = std::sqrt(axisX * axisX + axisY * axisY);
  AE_EXPECT_TRUE(axisLength > 1.0f, "a alca fica longe da origem do gizmo");
  const UiPoint direction{axisX / axisLength, axisY / axisLength};

  const u32 depthBefore = history.undoDepth();
  const EditorTransform before = document.find(fixture.cube)->transform;
  fixture.down(8, handle);
  for (u32 step = 1; step <= 40; ++step) {
    const float travel = static_cast<float>(step);
    fixture.move(8, {handle.x + direction.x * travel, handle.y + direction.y * travel});
  }
  fixture.up(8, {handle.x + direction.x * 40.0f, handle.y + direction.y * 40.0f});

  const EditorTransform after = document.find(fixture.cube)->transform;
  float moved = 0.0f;
  for (u32 axis = 0; axis < 3; ++axis)
    moved += std::fabs(after.position[axis] - before.position[axis]);
  AE_EXPECT_TRUE(moved > 0.0001f, "arrastar a alca moveu o objeto");
  AE_EXPECT_EQ(history.undoDepth(), depthBefore + 1,
               "e o arraste inteiro e um unico passo de desfazer");
  AE_EXPECT_TRUE(history.undo(document), "");
  float restored = 0.0f;
  for (u32 axis = 0; axis < 3; ++axis)
    restored += std::fabs(document.find(fixture.cube)->transform.position[axis] -
                          before.position[axis]);
  AE_EXPECT_TRUE(restored < 0.0001f, "desfazer devolve a posicao original");
}

AE_TEST(session_folders_are_not_selectable_in_the_viewport) {
  // Uma pasta nao tem corpo. Deixar o toque acerta-la selecionaria um grupo
  // quando o usuario mirou uma peca.
  Fixture fixture;
  EditorDocument &document = fixture.session.document();
  EditorHistory &history = fixture.session.history();
  // Move o cubo para longe e deixa so a pasta perto do alvo.
  EditorTransform far{};
  far.position[0] = 500.0f;
  history.setTransform(document, fixture.cube, far);
  fixture.session.update();

  const UiPoint centre = fixture.viewportCentre();
  fixture.down(1, centre);
  fixture.up(1, centre);
  AE_EXPECT_EQ(fixture.session.selection(), kInvalidEntity,
               "a pasta na origem nao foi selecionada");
}

AE_TEST(session_cancelling_pointers_closes_an_open_gizmo_transaction) {
  // A tela girou ou o app perdeu o foco no meio de um arraste. Uma transacao
  // aberta atravessando isso deixaria todo comando seguinte preso nela.
  Fixture fixture;
  fixture.session.cancelPointers();
  AE_EXPECT_TRUE(!fixture.session.history().isOpen(), "");
  AE_EXPECT_TRUE(fixture.session.screen().activeGizmoAxis == EditorGizmoHandle::None, "");
}

AE_TEST(session_reports_when_play_was_requested) {
  Fixture fixture;
  AE_EXPECT_TRUE(!fixture.session.playRequested(), "");
  const UiRect bar = fixture.session.layout().topBar;
  for (float x = bar.right() - 2.0f; x > bar.x; x -= 2.0f) {
    fixture.down(1, {x, bar.y + bar.height * 0.5f});
    fixture.up(1, {x, bar.y + bar.height * 0.5f});
    if (fixture.session.playRequested()) break;
  }
  AE_EXPECT_TRUE(fixture.session.playRequested(), "o botao de Play pede a execucao");
  fixture.session.clearPlayRequest();
  AE_EXPECT_TRUE(!fixture.session.playRequested(), "");
}

AE_TEST(session_frames_the_selection) {
  Fixture fixture;
  EditorDocument &document = fixture.session.document();
  EditorHistory &history = fixture.session.history();
  EditorTransform far{};
  far.position[0] = 40.0f;
  far.position[2] = -25.0f;
  history.setTransform(document, fixture.cube, far);
  fixture.session.update();

  // Seleciona pela hierarquia: enquadrar age sobre a SELECAO, e sem ela nao ha
  // o que enquadrar.
  fixture.session.setSelection(fixture.cube);
  AE_EXPECT_EQ(fixture.session.selection(), fixture.cube, "");

  // Sem enquadrar, um objeto longe do alvo da orbita fica inalcancavel.
  fixture.session.frameSelection();
  AE_EXPECT_TRUE(std::fabs(fixture.session.camera().target[0] - 40.0f) < 0.01f,
                 "o alvo da orbita passa a ser o objeto");
}

AE_TEST(session_scene_clock_is_frozen_until_play) {
  // Editar e ver a cena PARADA. Sem isto o que se ve nao e um editor, e um
  // video com paineis por cima -- a agua ondula e o casco anda enquanto alguem
  // tenta posicionar um objeto.
  Fixture fixture;
  AE_EXPECT_TRUE(!fixture.session.isPlaying(), "o editor comeca parado");
  fixture.session.advanceClock(0.0f);
  fixture.session.advanceClock(0.016f);
  fixture.session.advanceClock(0.032f);
  AE_EXPECT_TRUE(fixture.session.sceneTime() == 0.0f, "o relogio da cena nao andou");

  // Aperta Play pela aba do topo.
  EditorScreenState &mutableState = const_cast<EditorScreenState &>(fixture.session.screen());
  mutableState.workspace = EditorWorkspace::Play;
  fixture.session.advanceClock(0.048f);
  AE_EXPECT_TRUE(fixture.session.sceneTime() > 0.0f, "em Play a cena anda");
}

AE_TEST(session_scene_clock_ignores_a_wall_clock_jump) {
  // O relogio de parede salta quando o app e retomado. Integrar esse salto
  // teleportaria a simulacao para um futuro que ninguem viu acontecer.
  Fixture fixture;
  EditorScreenState &mutableState = const_cast<EditorScreenState &>(fixture.session.screen());
  mutableState.workspace = EditorWorkspace::Play;
  fixture.session.advanceClock(10.0f);
  fixture.session.advanceClock(10.016f);
  const float afterStep = fixture.session.sceneTime();
  AE_EXPECT_TRUE(afterStep > 0.0f, "");
  fixture.session.advanceClock(400.0f);
  AE_EXPECT_TRUE(fixture.session.sceneTime() == afterStep, "o salto foi descartado");
  fixture.session.advanceClock(400.016f);
  AE_EXPECT_TRUE(fixture.session.sceneTime() > afterStep,
                 "e o passo seguinte volta a contar normalmente");
}

AE_TEST(session_camera_stays_valid_under_extreme_gestures) {
  Fixture fixture;
  const UiPoint centre = fixture.viewportCentre();
  fixture.down(1, centre);
  for (u32 step = 0; step < 200; ++step)
    fixture.move(1, {centre.x + static_cast<float>(step) * 37.0f,
                     centre.y + static_cast<float>(step) * 53.0f});
  fixture.up(1, centre);
  AE_EXPECT_TRUE(isEditorCameraValid(fixture.session.camera()), "");
  AE_EXPECT_TRUE(std::fabs(fixture.session.camera().pitch) < 1.5f,
                 "o pitch fica longe dos polos, onde a orbita perderia a referencia");
  fixture.session.update();
  AE_EXPECT_TRUE(isViewportValid(fixture.session.view()), "");
}

namespace {
UiPoint locateWidget(EditorSession &session,u32 widget) {
  session.update();
  UiInputRouter router;UiDrawList list;
  list.begin(session.screen().surface,font().metrics(UiFontWeight::Regular));
  buildEditorScreen(session.screen(),defaultTheme(),list,router);
  for(float y=2;y<session.screen().surface.height;y+=4)
    for(float x=2;x<session.screen().surface.width;x+=4) {
      const auto routed=router.route({99,UiPointerPhase::Down,{x,y},0});
      if(routed.target==UiPointerTarget::Widget && routed.widgetId==widget) return {x,y};
    }
  return {-1,-1};
}
void tapWidget(Fixture &fixture,u32 widget) {
  if(fixture.session.screen().creationMenu) for(u32 i=0;i<editorCreationCatalog.size();++i) if(widget==widgetId(editorCreationCatalog[i].action)) {
    tapWidget(fixture,widgetId(EditorWidget::CreationCategoryBase)+editorCreationCatalog[i].category);
    tapWidget(fixture,widgetId(EditorWidget::CreationRowBase)+i);break;
  }
  const auto at=locateWidget(fixture.session,widget);
  fixture.down(77,at);fixture.up(77,at);fixture.session.update();
}
}
AE_TEST(session_viewport_down_is_consumed_and_duplicate_down_does_not_leave_a_ghost_finger) {
  Fixture f;const auto at=f.viewportCentre();
  AE_EXPECT_TRUE(f.session.handlePointer({1,UiPointerPhase::Down,at,0}),"editor owns down");
  f.down(1,at);f.up(1,at);
  const auto before=f.session.camera();
  f.down(2,at);f.move(2,{at.x+25,at.y});f.up(2,{at.x+25,at.y});
  AE_EXPECT_TRUE(f.session.camera().yaw!=before.yaw,"next gesture is an orbit, not a stuck pinch");
  AE_EXPECT_EQ(f.session.camera().distance,before.distance,"no phantom zoom");
}
AE_TEST(session_tap_jitter_does_not_orbit) {
  Fixture f;const auto at=f.viewportCentre();const float yaw=f.session.camera().yaw;
  f.down(1,at);f.move(1,{at.x+1,at.y+1});f.up(1,{at.x+1,at.y+1});
  AE_EXPECT_EQ(f.session.camera().yaw,yaw,"selection jitter must not move camera");
}
AE_TEST(session_numeric_entry_changes_exact_axis_and_undo_restores_it) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,transformFieldWidget(0,0));
  AE_EXPECT_EQ(f.session.screen().numericField,transformFieldWidget(0,0),"X field opens numeric input");
  const u32 base=widgetId(EditorWidget::NumericKeyBase);
  tapWidget(f,base);tapWidget(f,base+1);tapWidget(f,base+9);tapWidget(f,base+4);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  const auto *entity=f.session.document().find(f.cube);
  AE_EXPECT_EQ(entity->transform.position[0],12.5f,"exact typed value");
  AE_EXPECT_EQ(entity->transform.position[1],0.0f,"Y unaffected");
  AE_EXPECT_EQ(entity->transform.position[2],0.0f,"Z unaffected");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one undo per confirmed edit");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo");
  AE_EXPECT_EQ(f.session.document().find(f.cube)->transform.position[0],0.0f,"restored");
}
AE_TEST(session_numeric_cancel_keeps_document_and_history_unchanged) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  const auto revision=f.session.document().revision();
  tapWidget(f,transformFieldWidget(0,2));tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+5);
  tapWidget(f,widgetId(EditorWidget::NumericCancel));
  AE_EXPECT_EQ(f.session.document().revision(),revision,"cancel does not mutate");
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"cancel creates no undo");
}
AE_TEST(session_transform_scrub_is_a_single_transaction) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  const auto at=locateWidget(f.session,transformFieldWidget(1,1));
  AE_EXPECT_TRUE(at.x>=0,"field reachable");
  f.down(7,at);f.move(7,{at.x+20,at.y});f.move(7,{at.x+40,at.y});f.up(7,{at.x+40,at.y});
  AE_EXPECT_EQ(f.session.document().find(f.cube)->transform.rotationDegrees[1],40.0f,"rotation scrub");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one gesture one undo");
}

AE_TEST(session_asset_browser_recreates_geometry_in_an_empty_document) {
  Fixture f;renderer::MapDrawRecord draw{};
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;draw.indexCount=36;
  AE_EXPECT_TRUE(f.session.importMap({&draw,1}),"import resource library");
  f.session.document().reset();f.session.update();
  tapWidget(f,widgetId(EditorWidget::TabAssets));
  tapWidget(f,widgetId(EditorWidget::AssetRowBase));
  AE_EXPECT_EQ(f.session.document().entityCount(),2u,"one authored instance plus root");
  std::vector<renderer::MapDrawState> extracted;
  AE_EXPECT_TRUE(f.session.extractMap(extracted),"extract instance");
  AE_EXPECT_TRUE(extracted[0].visible,"instantiated geometry is submitted");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo instantiate");
  AE_EXPECT_TRUE(f.session.extractMap(extracted),"extract undo");
  AE_EXPECT_TRUE(!extracted[0].visible,"undo removes actual geometry");
}

AE_TEST(session_material_and_environment_numeric_controls_use_the_same_history_path) {
  Fixture f;auto entity=*f.session.document().find(f.cube);entity.assetId=1;
  f.session.document().applyEntityValues(f.cube,entity);f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::InspectorTabMaterial));
  tapWidget(f,widgetId(EditorWidget::TransformFieldBase)+12);
  AE_EXPECT_EQ(f.session.screen().numericField,widgetId(EditorWidget::TransformFieldBase)+12,"roughness reachable");
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+10);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+9);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);tapWidget(f,widgetId(EditorWidget::NumericApply));
  AE_EXPECT_EQ(f.session.document().find(f.cube)->material.roughness,.2f,"exact roughness");
  AE_EXPECT_EQ(f.session.document().find(f.cube)->transform.position[0],0.0f,"transform untouched");
  tapWidget(f,widgetId(EditorWidget::TabLighting));
  tapWidget(f,widgetId(EditorWidget::TransformFieldBase)+20);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+2);tapWidget(f,widgetId(EditorWidget::NumericApply));
  AE_EXPECT_EQ(f.session.document().find(f.session.document().root())->environment[0],3.0f,"sun belongs to scene root");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo environment");
  AE_EXPECT_EQ(f.session.document().find(f.session.document().root())->environment[0],1.0f,"lighting restored");
}

AE_TEST(session_frame_group_uses_descendants_instead_of_empty_group_origin) {
  Fixture f;auto &doc=f.session.document();const auto parent=doc.find(f.cube)->parent;
  auto value=*doc.find(f.cube);value.transform.position[0]=150;doc.applyEntityValues(f.cube,value);
  f.session.setSelection(parent);f.session.frameSelection();
  AE_EXPECT_TRUE(std::abs(f.session.camera().target[0]-150)<.001f,"focus actual group contents");
}

AE_TEST(session_rename_uses_touch_keyboard_and_one_undo) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));
  tapWidget(f,widgetId(EditorWidget::RenameSelection));
  AE_EXPECT_EQ(f.session.screen().renameEntity,f.cube,"rename opens");
  tapWidget(f,widgetId(EditorWidget::NameClear));
  tapWidget(f,widgetId(EditorWidget::NameShift));
  tapWidget(f,widgetId(EditorWidget::NameKeyBase)+1);
  tapWidget(f,widgetId(EditorWidget::NameShift));
  tapWidget(f,widgetId(EditorWidget::NameKeyBase));
  tapWidget(f,widgetId(EditorWidget::NameApply));
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Ba","typed name");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one command");
  f.session.history().undo(f.session.document());
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Cube","undo name");
  f.session.history().redo(f.session.document());
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Ba","redo name");
}
AE_TEST(session_rename_empty_rejected_and_cancel_preserves_document) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));tapWidget(f,widgetId(EditorWidget::RenameSelection));
  tapWidget(f,widgetId(EditorWidget::NameClear));tapWidget(f,widgetId(EditorWidget::NameApply));
  AE_EXPECT_EQ(f.session.screen().renameEntity,f.cube,"empty name stays open");
  tapWidget(f,widgetId(EditorWidget::NameCancel));
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"cancel creates no edit");
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Cube","name preserved");
}
AE_TEST(session_hierarchy_collapse_hides_descendants_and_can_expand_again) {
  Fixture f;const auto parent=f.session.document().find(f.cube)->parent;
  const u32 toggle=widgetId(EditorWidget::HierarchyCollapseBase)+parent;
  tapWidget(f,toggle);
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x<0,"child hidden");
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(parent)).x>=0,"parent stays visible");
  tapWidget(f,toggle);
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x>=0,"child restored");
}

AE_TEST(session_play_releases_editor_panels_and_gizmos_then_restores_them) {
  Fixture f;f.session.setSelection(f.cube);
  tapWidget(f,widgetId(EditorWidget::TabPlay));
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x<0,"no hierarchy over runtime joystick");
  AE_EXPECT_TRUE(locateWidget(f.session,transformFieldWidget(0,0)).x<0,"no inspector during play");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::FrameSelection)).x<0,"no scene overlay during play");
  tapWidget(f,widgetId(EditorWidget::TabScene));
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x>=0,"hierarchy restored");
  AE_EXPECT_TRUE(locateWidget(f.session,transformFieldWidget(0,0)).x>=0,"inspector restored");
}

AE_TEST(session_single_finger_pan_and_zoom_are_explicit_and_do_not_rotate) {
  Fixture f;
  tapWidget(f,widgetId(EditorWidget::NavigationPan));
  const auto before=f.session.camera();const auto at=f.viewportCentre();
  f.down(1,at);f.move(1,{at.x+30,at.y+15});f.up(1,{at.x+30,at.y+15});f.session.update();
  AE_EXPECT_EQ(f.session.camera().yaw,before.yaw,"pan preserves yaw");
  AE_EXPECT_TRUE(f.session.camera().target[0]!=before.target[0],"pan moves working pivot");
  tapWidget(f,widgetId(EditorWidget::NavigationZoom));
  const auto distance=f.session.camera().distance;
  f.down(2,at);f.move(2,{at.x,at.y+30});f.up(2,{at.x,at.y+30});f.session.update();
  AE_EXPECT_TRUE(f.session.camera().distance>distance,"downward zoom backs away");
  AE_EXPECT_EQ(f.session.camera().yaw,before.yaw,"zoom preserves yaw");
  tapWidget(f,widgetId(EditorWidget::NavigationOrbit));
  f.down(3,at);f.move(3,{at.x+30,at.y});f.up(3,{at.x+30,at.y});
  AE_EXPECT_TRUE(f.session.camera().yaw!=before.yaw,"orbit restored");
}

AE_TEST(session_selection_reveals_collapsed_parent_and_scrolls_to_distant_object) {
  Fixture f;auto &doc=f.session.document();
  const auto parent=doc.find(f.cube)->parent;
  tapWidget(f,widgetId(EditorWidget::HierarchyCollapseBase)+parent);
  f.session.setSelection(f.cube);f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x>=0,"selected child revealed");
  EditorEntityId last=0;
  for(u32 i=0;i<80;++i) last=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Object");
  f.session.update();f.session.setSelection(last);f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(last)).x>=0,"distant selection visible");
}

AE_TEST(session_rotation_ring_applies_angle_and_undo) {
  Fixture f;f.session.setSelection(f.cube);
  const float camera[3]{0,0,-10};f.session.setCameraPose(camera,0,0);f.session.update();
  tapWidget(f,widgetId(EditorWidget::ToolRotate));f.session.history().clear();
  EditorGizmoSettings settings;settings.screenLengthPixels=72;
  const float origin[3]{0,0,0};const auto frame=buildGizmoFrame(f.session.view(),origin,settings);
  float a[3],b[3];gizmoRingPoint(origin,2,frame.axisWorldLength,.7f,a);
  gizmoRingPoint(origin,2,frame.axisWorldLength,1.2f,b);
  const auto start=projectWorldToScreen(f.session.view(),a).screen;
  const auto end=projectWorldToScreen(f.session.view(),b).screen;
  f.down(5,start);f.move(5,end);f.up(5,end);f.session.update();
  AE_EXPECT_TRUE(std::abs(f.session.document().find(f.cube)->transform.rotationDegrees[2]-28.64789f)<.1f,"half radian ring drag");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"single ring transaction");
  f.session.history().undo(f.session.document());
  AE_EXPECT_EQ(f.session.document().find(f.cube)->transform.rotationDegrees[2],0.0f,"undo restores angle");
}

AE_TEST(session_water_properties_persist_and_undo) {
  Fixture f;f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::TabSettings));
  tapWidget(f,widgetId(EditorWidget::TransformFieldBase)+24);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  const auto &doc=f.session.document();const auto *root=doc.find(doc.root());
  AE_EXPECT_TRUE(root->waterEnabled,"water authored in scene");
  AE_EXPECT_EQ(root->water[0],2.0f,"wave height edited");
  const auto archive=serializeEditorDocument(doc,123);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,123,restored),"archive v3 roundtrip");
  AE_EXPECT_TRUE(restored.find(restored.root())->waterEnabled,"enabled saved");
  AE_EXPECT_EQ(restored.find(restored.root())->water[0],2.0f,"value saved");
  f.session.history().undo(f.session.document());
  AE_EXPECT_TRUE(!f.session.document().find(doc.root())->waterEnabled,"undo restores legacy water source");
}

AE_TEST(session_missing_mesh_does_not_create_an_invisible_pick_target) {
  Fixture f;auto &doc=f.session.document();
  auto values=*doc.find(f.cube);values.assetId=0;doc.applyEntityValues(f.cube,values);
  f.session.update();const auto center=f.viewportCentre();f.down(71,center);f.up(71,center);
  AE_EXPECT_EQ(f.session.selection(),kInvalidEntity,"no phantom sphere for missing mesh");
  AE_EXPECT_TRUE(doc.exists(f.cube),"authored row preserved for resource repair");
}

AE_TEST(session_asset_drag_to_viewport_commits_on_release_and_can_undo) {
  Fixture f;renderer::MapDrawRecord draw{};
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;draw.indexCount=36;
  AE_EXPECT_TRUE(f.session.importMap({&draw,1},{},false),"resource library only");f.session.update();
  tapWidget(f,widgetId(EditorWidget::TabAssets));
  const auto start=locateWidget(f.session,widgetId(EditorWidget::AssetRowBase));
  f.down(91,start);f.move(91,{start.x+35,start.y+25});f.session.update();
  AE_EXPECT_TRUE(f.session.screen().draggingAsset,"visible drag feedback");
  AE_EXPECT_EQ(f.session.document().entityCount(),1u,"no object until accepted drop");
  const auto end=f.viewportCentre();f.move(91,end);f.up(91,end);f.session.update();
  AE_EXPECT_EQ(f.session.document().entityCount(),2u,"one real instance");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one transaction");
  std::vector<renderer::MapDrawState> out;AE_EXPECT_TRUE(f.session.extractMap(out),"extract");
  AE_EXPECT_TRUE(out[0].visible,"renderer receives placed geometry");
  const auto archive=serializeEditorDocument(f.session.document(),7);EditorDocument reopened;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,7,reopened),"save/reopen");
  AE_EXPECT_EQ(reopened.entityCount(),2u,"instance survives reopening");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo");
  AE_EXPECT_EQ(f.session.document().entityCount(),1u,"undo removes instance");
}

AE_TEST(session_asset_drop_on_transformed_parent_preserves_world_placement) {
  Fixture f;renderer::MapDrawRecord draw{};
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;
  AE_EXPECT_TRUE(f.session.importMap({&draw,1},{},false),"library");
  auto &doc=f.session.document();const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Parent");
  EditorTransform transform;transform.position[0]=10;transform.rotationDegrees[1]=45;transform.scale[0]=2;
  AE_EXPECT_TRUE(doc.setTransform(parent,transform),"parent transform");f.session.update();
  tapWidget(f,widgetId(EditorWidget::TabAssets));
  const auto start=locateWidget(f.session,widgetId(EditorWidget::AssetRowBase));
  f.down(92,start);f.move(92,{start.x+35,start.y+25});f.session.update();
  const auto end=locateWidget(f.session,hierarchyRowWidget(parent));f.move(92,end);f.up(92,end);
  // Nonuniform rotated parent would require shear for world-aligned identity.
  // The operation must reject atomically rather than create an unplaced mesh.
  AE_EXPECT_EQ(doc.entityCount(),2u,"unrepresentable TRS does not create an orphan");
  transform.scale[0]=1;doc.setTransform(parent,transform);f.session.update();
  const float point[]{10,0,0};
  const auto child=f.session.instantiateAsset(0,parent,point);
  AE_EXPECT_TRUE(child!=0,"representable parent accepted");
  float world[16];AE_EXPECT_TRUE(editorWorldMatrix(doc,child,world),"world matrix");
  AE_EXPECT_TRUE(std::abs(world[12]-10)<.0001f,"world placement preserved");
  AE_EXPECT_EQ(doc.find(child)->parent,parent,"actual scene graph parent");
}

AE_TEST(session_asset_drag_cancel_does_not_create_or_change_history) {
  Fixture f;renderer::MapDrawRecord draw{};
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;draw.boundsRadius=1;
  AE_EXPECT_TRUE(f.session.importMap({&draw,1},{},false),"library");f.session.update();
  tapWidget(f,widgetId(EditorWidget::TabAssets));
  const auto at=locateWidget(f.session,widgetId(EditorWidget::AssetRowBase));
  f.down(93,at);f.move(93,{at.x+35,at.y+25});f.session.update();
  f.session.cancelPointers();f.up(93,f.viewportCentre());
  AE_EXPECT_EQ(f.session.document().entityCount(),1u,"lifecycle cancel cannot instantiate");
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"cancel creates no history");
}

AE_TEST(session_hierarchy_horizontal_drag_reparents_with_world_transform_and_undo) {
  Fixture f;auto &doc=f.session.document();
  const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Destination");
  EditorTransform pose;pose.position[0]=17;doc.setTransform(parent,pose);f.session.history().clear();f.session.update();
  const auto start=locateWidget(f.session,hierarchyRowWidget(f.cube));
  float before[16];editorWorldMatrix(doc,f.cube,before);
  f.down(94,start);f.move(94,{start.x+35,start.y});f.session.update();
  AE_EXPECT_EQ(f.session.screen().draggingEntity,f.cube,"horizontal drag captures entity");
  const auto end=locateWidget(f.session,hierarchyRowWidget(parent));f.move(94,end);f.up(94,end);
  AE_EXPECT_EQ(doc.find(f.cube)->parent,parent,"real parenting");
  float after[16];editorWorldMatrix(doc,f.cube,after);
  for(u32 i=0;i<16;++i) AE_EXPECT_TRUE(std::abs(before[i]-after[i])<.0001f,"world preserved");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one reparent transaction");
  AE_EXPECT_TRUE(f.session.history().undo(doc),"undo reparent");
  AE_EXPECT_TRUE(doc.find(f.cube)->parent!=parent,"original parent restored");
}

AE_TEST(session_spectrum_property_is_reachable_through_touch_pages) {
  Fixture f;f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::TabSettings));
  const u32 windWidget=widgetId(EditorWidget::TransformFieldBase)+37;
  for(u32 page=0;page<34 && locateWidget(f.session,windWidget).x<0;++page)
    tapWidget(f,widgetId(EditorWidget::PropertyNext));
  AE_EXPECT_TRUE(locateWidget(f.session,windWidget).x>=0,"wind accessible through Inspector pages");
  tapWidget(f,windWidget);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  const auto &doc=f.session.document();const auto *root=doc.find(doc.root());
  AE_EXPECT_TRUE(root->waterSpectrumEnabled,"touch activates spectral authoring");
  AE_EXPECT_EQ(root->water[13],2.0f,"wind changed using real input routing");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo touch edit");
  AE_EXPECT_TRUE(!doc.find(doc.root())->waterSpectrumEnabled,"undo restores inheritance");
}

AE_TEST(session_river_popup_points_and_all_point_properties_work_through_touch) {
  Fixture f;std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"real library");
  AE_EXPECT_TRUE(f.session.importMap(draws,materials,false),"empty scene");
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  AE_EXPECT_TRUE(f.session.screen().creationMenu,"creation popup opens");
  tapWidget(f,widgetId(EditorWidget::CreateRiverWater));
  const auto river=f.session.selection();
  AE_EXPECT_EQ(f.session.document().entityCount(),2u,"only requested river exists");
  AE_EXPECT_EQ(f.session.document().find(river)->route.count,3u,"three editable points");
  const auto point=f.session.document().find(river)->route.points[0];
  tapWidget(f,widgetId(EditorWidget::RoutePointBase));
  AE_EXPECT_EQ(f.session.document().find(river)->route.points[0].position[0],point.position[0],"tap does not move point");
  tapWidget(f,widgetId(EditorWidget::RoutePointAdd));
  AE_EXPECT_EQ(f.session.document().find(river)->route.count,4u,"insert point");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo insert");
  AE_EXPECT_EQ(f.session.document().find(river)->route.count,3u,"topology restored");
  f.session.update();
  const auto property=widgetId(EditorWidget::TransformFieldBase)+RoutePropertyBase+f.session.screen().routePoint*8+7;
  for(u32 page=0;page<8 && locateWidget(f.session,property).x<0;++page) tapWidget(f,widgetId(EditorWidget::PropertyNext));
  AE_EXPECT_TRUE(locateWidget(f.session,property).x>=0,"last per-point property is reachable");
  tapWidget(f,property);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+9);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+4);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  AE_EXPECT_EQ(f.session.document().find(river)->route.points[f.session.screen().routePoint].tension,.5f,"numeric tension committed");
}

AE_TEST(session_plane_drag_respects_rotated_scaled_parent_and_undo) {
  Fixture f;auto &doc=f.session.document();
  auto parent=doc.find(doc.find(f.cube)->parent)->transform;
  parent.rotationDegrees[2]=90;parent.scale[0]=2;parent.scale[1]=3;
  f.session.history().setTransform(doc,doc.find(f.cube)->parent,parent);
  f.session.setSelection(f.cube);
  const float camera[3]{0,0,-10};f.session.setCameraPose(camera,0,0);f.session.update();
  f.session.history().clear();
  EditorGizmoSettings settings;settings.screenLengthPixels=72;
  const float origin[3]{};const auto frame=buildGizmoFrame(f.session.view(),origin,settings);
  const float startWorld[3]{frame.axisWorldLength*.47f,frame.axisWorldLength*.47f,0};
  const float endWorld[3]{startWorld[0]+.7f,startWorld[1]+.4f,0};
  const auto start=projectWorldToScreen(f.session.view(),startWorld).screen;
  const auto end=projectWorldToScreen(f.session.view(),endWorld).screen;
  f.down(11,start);
  AE_EXPECT_TRUE(f.session.screen().activeGizmoAxis==EditorGizmoHandle::PlaneXY,"plane captures touch");
  f.move(11,end);f.session.update();f.move(11,end);f.up(11,end);
  float world[16];AE_EXPECT_TRUE(editorWorldMatrix(doc,f.cube,world),"world transform");
  AE_EXPECT_TRUE(std::abs(world[12]-.7f)<.0001f && std::abs(world[13]-.4f)<.0001f,"world XY delta despite rotated scaled parent");
  AE_EXPECT_TRUE(std::abs(world[14])<.0001f,"locked Z");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one command despite frame rebuild");
  f.session.history().undo(doc);
  AE_EXPECT_EQ(doc.find(f.cube)->transform.position[0],0.0f,"undo local X");
  AE_EXPECT_EQ(doc.find(f.cube)->transform.position[1],0.0f,"undo local Y");
}

AE_TEST(session_axis_shaft_captures_drag_and_settings_has_no_gizmo) {
  Fixture f;f.session.setSelection(f.cube);
  const float camera[3]{0,0,-10};f.session.setCameraPose(camera,0,0);f.session.update();
  const float origin[3]{};EditorGizmoSettings settings;settings.screenLengthPixels=72;
  const auto frame=buildGizmoFrame(f.session.view(),origin,settings);
  const UiPoint start{(frame.originScreen.x+frame.axisEndScreen[0].x)*.5f,frame.originScreen.y};
  f.down(12,start);
  AE_EXPECT_TRUE(f.session.screen().activeGizmoAxis==EditorGizmoHandle::AxisX,"middle of shaft captures X");
  f.move(12,{start.x+20,start.y});f.up(12,{start.x+20,start.y});
  AE_EXPECT_TRUE(f.session.document().find(f.cube)->transform.position[0]>0,"shaft moves entity");
  tapWidget(f,widgetId(EditorWidget::TabSettings));
  AE_EXPECT_TRUE(locateWidget(f.session,gizmoAxisWidget(0)).x<0,"settings does not capture hidden gizmos");
}

AE_TEST(session_water_creation_is_reachable_by_touch_from_empty_scene) {
  Fixture fixture;
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"geometry library");
  AE_EXPECT_TRUE(fixture.session.importMap(draws,materials,false),"empty scene");
  tapWidget(fixture,widgetId(EditorWidget::TabAssets));
  tapWidget(fixture,widgetId(EditorWidget::CreateFiniteWater));
  AE_EXPECT_EQ(fixture.session.document().entityCount(),2u,"touch creates actual surface");
  AE_EXPECT_EQ(fixture.session.history().undoDepth(),1u,"one creation command");
  std::vector<renderer::MapDrawState> output;
  AE_EXPECT_TRUE(fixture.session.extractMap(output),"render extraction");
  AE_EXPECT_TRUE(output[0].visible && !output[1].visible,"only requested surface visible");
}

AE_TEST(session_stop_button_returns_to_edit_and_preserves_document) {
  Fixture f;const auto revision=f.session.document().revision();
  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
  AE_EXPECT_TRUE(f.session.isPlaying(),"inicia a simulação");
  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
  AE_EXPECT_TRUE(!f.session.isPlaying(),"o mesmo botão para a simulação");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"simular não altera a cena");
}
AE_TEST(session_explicit_camera_creation_is_undoable) {
  Fixture f;AE_EXPECT_EQ(resolveSceneCamera(f.session.document()).entity,0u,"sem câmera automática");
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  tapWidget(f,widgetId(EditorWidget::CreateCamera));
  const auto pose=resolveSceneCamera(f.session.document());
  AE_EXPECT_TRUE(pose.entity!=0,"câmera criada na cena");
  float position[3];editorCameraPosition(f.session.camera(),position);
  for(u32 i=0;i<3;++i) AE_EXPECT_TRUE(std::abs(pose.position[i]-position[i])<.001f,"mesma vista de trabalho");
  tapWidget(f,widgetId(EditorWidget::Undo));
  AE_EXPECT_EQ(resolveSceneCamera(f.session.document()).entity,0u,"desfaz a criação inteira");
}
AE_TEST(session_hierarchy_filter_keeps_matching_ancestors) {
  Fixture f;
  auto &state=const_cast<EditorScreenState&>(f.session.screen());
  std::strcpy(state.hierarchySearch,"inexistente");f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x<0,"filtra sem remover objetos");
  tapWidget(f,widgetId(EditorWidget::HierarchySearch));
  tapWidget(f,widgetId(EditorWidget::NameClear));
  tapWidget(f,widgetId(EditorWidget::NameApply));
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x>=0,"limpar restaura a árvore");
}
