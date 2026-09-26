#include "editor/editor_component_impact.h"
#include "scene/component_properties.h"
#include "renderer/authoring_geometry.h"
#include "renderer/rendering_settings_file.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_scene_camera.h"
#include "editor/editor_session.h"
#include "editor/editor_scene_template.h"
#include "harness.h"
#include "scene/light.h"
#include "renderer/water_authoring_geometry.h"
#include "editor/editor_water_play.h"
#include "editor/editor_properties.h"
#include "scene/skinned_mesh.h"
#include "runtime/input_actions.h"

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

AE_TEST(editor_independent_document_roundtrip_without_map_resources) {
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("aether-independent-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".aescene");
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove(path,error);}} cleanup{path};
  EditorSession session;
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"empty resource source accepted");
  AE_EXPECT_TRUE(session.document().entityCount()==1,"only authoring root");
  const auto group=session.history().createEntity(session.document(),session.document().root(),EditorEntityKind::Folder,"Group");
  AE_EXPECT_TRUE(group!=0,"general hierarchy object created without a package");
  AE_EXPECT_TRUE(session.save(path.string().c_str(),0),"save independent archive");
  EditorSession reopened;
  AE_EXPECT_TRUE(reopened.load(path.string().c_str(),0),"load with no imported library");
  AE_EXPECT_TRUE(reopened.document().entityCount()==2,"hierarchy survives restart");
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(reopened.extractMap(draws) && draws.empty(),"no hidden draws");
  AE_EXPECT_TRUE(!reopened.load(path.string().c_str(),123),"different resource namespace rejected");
  AE_EXPECT_TRUE(reopened.document().entityCount()==2,"failed load preserves document");
}

AE_TEST(editor_creation_availability_follows_imported_resources) {
  EditorSession session;
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"independent source");
  // Luzes e formas de colisão independem da biblioteca visual importada.
  const auto semGeometria=creationAlwaysAvailable();
  AE_EXPECT_EQ(session.screen().creationAvailable,semGeometria,"independent objects remain available without geometry");
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"explicit resource library");
  AE_EXPECT_TRUE(session.importMap(draws,materials,false),"import resources without instances");
  AE_EXPECT_TRUE(creationAvailable(session.screen(),4),"finite water has its resource");
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"replace source with empty library");
  AE_EXPECT_EQ(session.screen().creationAvailable,semGeometria,"old capabilities do not survive source replacement");
}

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
  const UiPoint collapse{panel.x+40,panel.y+20};
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
  auto value=*session.document().find(river);editWaterRoute(value)->count=2;
  editWaterRoute(value)->points[0].position[0]=editWaterRoute(value)->points[1].position[0]=0;
  editWaterRoute(value)->points[0].position[2]=-50;editWaterRoute(value)->points[1].position[2]=50;
  editWaterRoute(value)->points[0].speed=editWaterRoute(value)->points[1].speed=2;
  editWaterBody(value)->waveGain=0;editWaterRoute(value)->points[0].width=editWaterRoute(value)->points[1].width=12;
  AE_EXPECT_TRUE(session.history().applyValues(session.document(),river,value),"edit river parameters");
  const auto body=session.instantiateAsset(3,session.document().root(),above);
  AE_EXPECT_TRUE(body && session.document().find(body)->rigidBodyEnabled,"explicit physical primitive");
  EditorDocument restored;const auto archive=serializeEditorDocument(session.document(),91);
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,91,restored),"v6 scene reload");
  AE_EXPECT_EQ(waterRoute(*restored.find(river)).count,2u,"route point count survives");
  AE_EXPECT_EQ(waterRoute(*restored.find(river)).points[1].speed,2.0f,"per point flow survives");
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
  auto value=*document.find(river);editWaterRoute(value)->count=2;editWaterRoute(value)->points[0].position[2]=-10;editWaterRoute(value)->points[1].position[2]=10;
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
  AE_EXPECT_EQ(meshAsset(*restored.find(ocean)),2u,"stable resource selection");
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
    auto value=*document.find(cube);editMeshRenderer(value)->mesh=1;document.applyEntityValues(cube,value);
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

AE_TEST(editor_lod_cross_fade_retains_the_transition_started_by_change_detection) {
  EditorSession session;session.initialize(&font(),&icons());
  AE_EXPECT_TRUE(session.importMap({}, {}, false),"cena independente");
  auto &document=session.document();
  const auto group=document.createEntity(document.root(),EditorEntityKind::Folder,"Grupo LOD");
  const auto level0=document.createEntity(group,EditorEntityKind::Folder,"LOD0");
  const auto level1=document.createEntity(group,EditorEntityKind::Folder,"LOD1");
  auto values=*document.find(group);
  auto *lod=static_cast<scene::LodGroup*>(values.components.add(scene::LodGroup::descriptor));
  AE_EXPECT_TRUE(lod!=nullptr,"componente criado");
  lod->levelCount=2;lod->transitions[1]=1;lod->size=2;
  lod->levels[0]=level0;lod->levels[1]=level1;
  lod->fadeMode=scene::LodFadeMode::CrossFade;lod->animateCrossFading=true;
  AE_EXPECT_TRUE(document.applyEntityValues(group,values),"grupo configurado");

  session.setSurface({0,0,800,600},{});
  const float nearPosition[3]{0,0,-2};
  session.setCameraPose(nearPosition,0,0);session.update();session.advanceClock(1);
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(session.extractMap(draws),"estado inicial no LOD 0");

  const float farPosition[3]{0,0,-4};
  session.setCameraPose(farPosition,0,0);session.update();session.advanceClock(10);
  AE_EXPECT_TRUE(session.lodSelectionChanged(),"cruzar o limite inicia a republicação");
  // O primeiro quadro tem fator zero e os mesmos estados visuais do anterior.
  // A segunda consulta só continua verdadeira se a primeira reteve o relógio
  // real da transição, em vez de iniciar a troca numa cópia descartável.
  AE_EXPECT_TRUE(session.lodSelectionChanged(),"a transição iniciada permanece ativa");
  session.advanceClock(10.25f);
  AE_EXPECT_TRUE(session.lodSelectionChanged(),"o meio do cross-fade ainda republica");
}

AE_TEST(component_mesh_resource_command_uses_the_reflected_binding_and_undo) {
  Fixture f;auto &session=f.session;auto &document=session.document();
  auto values=*document.find(f.cube);auto *collider=editCollider(values);
  collider->shape=scene::ColliderShape::Mesh;const auto instance=collider->instanceId();
  AE_EXPECT_TRUE(document.applyEntityValues(f.cube,values),"colisor preparado");
  const auto asset=session.screen().resources->assetGuid(0);
  EditorActionRequest request;request.version=session.sceneVersion();request.entity=f.cube;
  request.action=EditorAction::ComponentResource;request.componentInstance=instance;
  request.componentProperty="collision_mesh";request.componentResource=asset;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Applied,"binding refletido aceita a malha carregada");
  AE_EXPECT_TRUE(static_cast<const scene::Collider*>(document.find(f.cube)->components.findInstance(instance))->collisionMesh==asset,
                 "o GUID autoral foi aplicado");
  request.version=session.sceneVersion();request.action=EditorAction::Undo;
  AE_EXPECT_TRUE(session.dispatch(request).status==EditorActionStatus::Applied,"um desfazer cobre a troca");
  AE_EXPECT_TRUE(!static_cast<const scene::Collider*>(document.find(f.cube)->components.findInstance(instance))->collisionMesh.valid(),
                 "desfazer restaura a herança da malha visual");
}

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

AE_TEST(session_applies_a_multi_component_recipe_atomically_and_undoes_it_once) {
  Fixture fixture;auto &session=fixture.session;auto &document=session.document();
  const auto project=std::filesystem::temp_directory_path()/("aether-recipe-session-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(project);
  struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{project};
  AE_EXPECT_TRUE(session.setProjectDirectory(project.string().c_str()),"biblioteca de presets do projeto carregada");
  auto authored=*document.find(fixture.cube);
  auto *light=static_cast<scene::Light*>(authored.components.add(scene::Light::descriptor));
  AE_EXPECT_TRUE(light!=nullptr,"a origem tem dois componentes");
  light->intensity=4200;
  auto *render=editMeshRenderer(authored);render->enabled=false;
  AE_EXPECT_TRUE(document.applyEntityValues(fixture.cube,authored),"origem preparada");
  std::string diagnostic;
  AE_EXPECT_TRUE(session.openComponentPresets(fixture.cube,0),"painel carrega a biblioteca do projeto");
  AE_EXPECT_TRUE(session.saveComponentRecipe(fixture.cube,"Objeto iluminado",diagnostic),diagnostic.c_str());

  auto changed=*document.find(fixture.cube);
  changed.components.remove(scene::Light::descriptor);
  editMeshRenderer(changed)->enabled=true;
  AE_EXPECT_TRUE(document.applyEntityValues(fixture.cube,changed),"destino diverge da receita");
  AE_EXPECT_TRUE(session.openComponentPresets(fixture.cube,0),"seletor abre receitas de objeto");
  AE_EXPECT_EQ(session.screen().presetChoices.size(),usize{1},"receita disponível na sessão");
  const auto recipe=session.screen().presetChoices.front().first;
  const auto before=session.history().undoDepth();
  AE_EXPECT_TRUE(session.applyComponentRecipe(recipe,fixture.cube,session.sceneVersion(),diagnostic),diagnostic.c_str());
  const auto *result=document.find(fixture.cube);
  const auto *appliedLight=static_cast<const scene::Light*>(result->components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(appliedLight&&appliedLight->intensity==4200,"componente ausente é adicionado com os valores");
  AE_EXPECT_TRUE(!meshRenderer(*result)->enabled,"componente existente é atualizado");
  AE_EXPECT_EQ(session.history().undoDepth(),before+1,"a receita inteira é um comando");
  AE_EXPECT_TRUE(session.history().undo(document),"desfazer receita");
  result=document.find(fixture.cube);
  AE_EXPECT_TRUE(!result->components.find(scene::Light::descriptor)&&meshRenderer(*result)->enabled,
                 "um Undo restaura o objeto inteiro");
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
      router.route({99,UiPointerPhase::Up,{x,y},0});
      if(routed.target==UiPointerTarget::Widget && routed.widgetId==widget) return {x,y};
    }
  return {-1,-1};
}
void importWaterResources(Fixture &fixture) {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials),"explicit water library");
  AE_EXPECT_TRUE(fixture.session.importMap(draws,materials,false),"resource source");
  fixture.session.update();
}
u32 recipeWidget(std::string_view id) {
  u32 index=0;
  if(!findCreationRecipe(id,&index)) {
    std::fprintf(stderr,"receita não registrada: %.*s\n",static_cast<int>(id.size()),id.data());
    return 0;
  }
  return creationWidget(index);
}
void tapWidget(Fixture &fixture,u32 widget) {
  if((widget==widgetId(EditorWidget::TabAssets) || widget==widgetId(EditorWidget::TabProject) || widget==widgetId(EditorWidget::TabLighting)) && !fixture.session.screen().workspaceMenu)
    tapWidget(fixture,widgetId(EditorWidget::ProjectMenu));
  if(fixture.session.screen().creationMenu) for(u32 i=0;i<editorCreationCatalog.size();++i) if(widget==creationWidget(i)) {
    tapWidget(fixture,widgetId(EditorWidget::CreationCategoryBase)+editorCreationCatalog[i].category);
    // A grade rola pelo arraste: arrasta um cartão visível até o pedido aparecer.
    for(u32 attempt=0;attempt<16 && locateWidget(fixture.session,widgetId(EditorWidget::CreationRowBase)+i).x<0;++attempt) {
      UiPoint card{-1,-1};
      for(u32 j=0;j<editorCreationCatalog.size() && card.x<0;++j)
        card=locateWidget(fixture.session,widgetId(EditorWidget::CreationRowBase)+j);
      if(card.x<0) break;
      fixture.down(80,card);
      for(u32 step=1;step<=4;++step) fixture.move(80,{card.x,card.y-28.0f*static_cast<float>(step)});
      fixture.up(80,{card.x,card.y-112.0f});fixture.session.update();
    }
    tapWidget(fixture,widgetId(EditorWidget::CreationRowBase)+i);break;
  }
  const auto at=locateWidget(fixture.session,widget);
  if(at.x<0) std::fprintf(stderr,"unreachable test widget: %08x\n",widget);
  AE_EXPECT_TRUE(at.x>=0,"requested widget must exist before touching it");
  fixture.down(77,at);fixture.up(77,at);fixture.session.update();
}
// Escolhe a família no trilho do Add (0 é "Todos", 1..N as famílias).
void selectComponentFamily(Fixture &fixture,scene::ComponentFamily family) {
  const u32 wanted=static_cast<u32>(family)+1;
  // No telefone o trilho não mostra todas as famílias: arrasta como a pessoa faz.
  for(u32 attempt=0;attempt<8 && locateWidget(fixture.session,widgetId(EditorWidget::ComponentFamilyBase)+wanted).x<0;++attempt) {
    UiPoint cell{-1,-1};
    for(u32 value=0;value<=static_cast<u32>(scene::ComponentFamily::Count) && cell.x<0;++value)
      cell=locateWidget(fixture.session,widgetId(EditorWidget::ComponentFamilyBase)+value);
    if(cell.x<0) break;
    fixture.down(79,cell);
    for(u32 step=1;step<=4;++step) fixture.move(79,{cell.x,cell.y-12.0f*static_cast<float>(step)});
    fixture.up(79,{cell.x,cell.y-48.0f});fixture.session.update();
  }
  tapWidget(fixture,widgetId(EditorWidget::ComponentFamilyBase)+wanted);
  AE_EXPECT_EQ(fixture.session.screen().componentCategory,wanted,"família do Add selecionada");
}
// Rola a lista do Add pelo arraste, como a pessoa faz, até o item aparecer.
void revealAddEntry(Fixture &fixture,u32 widget) {
  for(u32 attempt=0;attempt<32 && locateWidget(fixture.session,widget).x<0;++attempt) {
    UiPoint row{-1,-1};
    for(u32 i=0;i<editorComponentCatalog.size() && row.x<0;++i)
      row=locateWidget(fixture.session,widgetId(EditorWidget::ComponentAddBase)+i);
    if(row.x<0) row=locateWidget(fixture.session,widgetId(EditorWidget::ScriptAddBase));
    if(row.x<0) return;
    const auto before=fixture.session.screen().addScroll;
    fixture.down(78,row);
    for(u32 step=1;step<=4;++step) fixture.move(78,{row.x,row.y-15.0f*static_cast<float>(step)});
    fixture.up(78,{row.x,row.y-60.0f});fixture.session.update();
    if(fixture.session.screen().addScroll==before) return;
  }
}
// Abre Configurações do projeto pelo menu Cena e escolhe a seção.
void openProjectSection(Fixture &fixture,EditorProjectSection section) {
  tapWidget(fixture,widgetId(EditorWidget::TabProject));
  tapWidget(fixture,widgetId(EditorWidget::ProjectSectionBase)+static_cast<u32>(section));
  AE_EXPECT_TRUE(fixture.session.screen().workspace==EditorWorkspace::Project,"projeto aberto");
  AE_EXPECT_TRUE(fixture.session.screen().projectSection==section,"seção escolhida");
}
void revealProperty(Fixture &fixture,u32 widget) {
  // Teto de segurança: a aba Material tem 25 linhas e a superfície de teste é baixa.
  for(u32 page=0;page<64 && locateWidget(fixture.session,widget).x<0;++page) {
    AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::PropertyNext)).x>=0,"next property page exists");
    tapWidget(fixture,widgetId(EditorWidget::PropertyNext));
  }
}
}
AE_TEST(r3_import_lives_in_properties_and_does_not_block_the_editor) {
  Fixture fixture;
  fixture.session.beginImportPreparation();
  resources::GltfImport model;
  model.nodes.emplace_back();
  model.nodes[0].name = "Raiz";
  fixture.session.showImportPreview("Fontes/modelo.glb", model, {}, fixture.session.importProfileDraft());
  fixture.session.update();
  const auto scene = locateWidget(fixture.session, widgetId(EditorWidget::ImportIntoScene));
  AE_EXPECT_TRUE(scene.x >= 0, "publicar na cena está no painel");
  const auto panel = fixture.session.layout().inspectorPanel;
  AE_EXPECT_TRUE(!panel.isEmpty() && scene.x >= panel.x && scene.x <= panel.x + panel.width, "dentro de Propriedades");

  // Sem janela modal: o viewport continua respondendo com o importador aberto.
  const auto before = fixture.session.camera();
  const auto centre = fixture.viewportCentre();
  fixture.down(3, centre);
  for (u32 step = 1; step <= 6; ++step) fixture.move(3, {centre.x + static_cast<float>(step) * 12.0f, centre.y});
  fixture.up(3, {centre.x + 72.0f, centre.y});
  AE_EXPECT_TRUE(std::fabs(fixture.session.camera().yaw - before.yaw) > 1e-4f, "órbita do viewport funciona com o importador aberto");

  // Mudar o perfil tira a publicação até preparar de novo.
  tapWidget(fixture, widgetId(EditorWidget::ImportTabProfile));
  tapWidget(fixture, widgetId(EditorWidget::ImportScaleUp));
  AE_EXPECT_EQ(fixture.session.screen().importScale, 2.f, "passo seguinte a 1");
  AE_EXPECT_TRUE(locateWidget(fixture.session, widgetId(EditorWidget::ImportIntoScene)).x < 0, "publicar some com perfil pendente");
  tapWidget(fixture, widgetId(EditorWidget::ImportApplyProfile));
  AE_EXPECT_TRUE(fixture.session.takeImportReprepare(), "o shell recebe o pedido de nova preparação");
  AE_EXPECT_TRUE(!fixture.session.screen().importReady, "a prévia antiga não publica mais");
  tapWidget(fixture, widgetId(EditorWidget::ImportCancel));
  AE_EXPECT_TRUE(!fixture.session.screen().importPanel, "cancelar fecha o painel");
}

AE_TEST(every_profile_field_blocks_publishing_until_prepared_again) {
  // A geometria derivada é perfil como a escala: mudar Normais, Modo, Tangentes
  // ou câmeras sem preparar de novo publicaria a prévia antiga. Achado no
  // aparelho: só escala e textura bloqueavam.
  const EditorWidget fields[] = {EditorWidget::ImportNormalsCycle, EditorWidget::ImportNormalWeightingCycle,
                                 EditorWidget::ImportTangentsCycle, EditorWidget::ImportCamerasToggle,
                                 EditorWidget::ImportLightsToggle, EditorWidget::ImportTextureCompressionCycle};
  for (const auto field : fields) {
    Fixture fixture;
    fixture.session.beginImportPreparation();
    resources::GltfImport model;
    model.nodes.emplace_back();
    model.nodes[0].name = "Raiz";
    fixture.session.showImportPreview("Fontes/modelo.glb", model, {}, fixture.session.importProfileDraft());
    fixture.session.update();
    tapWidget(fixture, widgetId(EditorWidget::ImportTabProfile));
    AE_EXPECT_TRUE(locateWidget(fixture.session, widgetId(EditorWidget::ImportIntoScene)).x >= 0, "publicar antes da mudança");
    // Numa tela baixa o perfil é paginado: a linha pedida tem de ser alcançável.
    for (u32 page = 0; page < 16 && locateWidget(fixture.session, widgetId(field)).x < 0 &&
                       locateWidget(fixture.session, widgetId(EditorWidget::ImportNextPage)).x >= 0; ++page)
      tapWidget(fixture, widgetId(EditorWidget::ImportNextPage));
    tapWidget(fixture, widgetId(field));
    AE_EXPECT_TRUE(locateWidget(fixture.session, widgetId(EditorWidget::ImportIntoScene)).x < 0,
                   "publicar some com o campo pendente");
    AE_EXPECT_TRUE(locateWidget(fixture.session, widgetId(EditorWidget::ImportApplyProfile)).x >= 0,
                   "e preparar de novo fica disponível");
    // Preparada de novo com o perfil da tela: o botão volta E a sessão aceita.
    // Tela e sessão divergindo num campo deixava o toque sem efeito.
    fixture.session.showImportPreview("Fontes/modelo.glb", model, {}, fixture.session.importProfileDraft());
    fixture.session.update();
    tapWidget(fixture, widgetId(EditorWidget::ImportIntoScene));
    AE_EXPECT_TRUE(fixture.session.takeImportAccept(), "a sessão aceita publicar com o perfil preparado");
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
  tapWidget(f,widgetId(EditorWidget::TransformFold));
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
  tapWidget(f,widgetId(EditorWidget::TransformFold));
  const auto revision=f.session.document().revision();
  tapWidget(f,transformFieldWidget(0,2));tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+5);
  tapWidget(f,widgetId(EditorWidget::NumericCancel));
  AE_EXPECT_EQ(f.session.document().revision(),revision,"cancel does not mutate");
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"cancel creates no undo");
}
AE_TEST(session_transform_scrub_is_a_single_transaction) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::TransformFold));
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
  Fixture f;auto entity=*f.session.document().find(f.cube);editMeshRenderer(entity)->mesh=1;
  f.session.document().applyEntityValues(f.cube,entity);f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::MeshMaterialTab));
  // A aba Material é por slot (Entrega 2); no alcance da instância, o campo do
  // slot 0 escreve o mesmo material do objeto, pelo mesmo histórico.
  const u32 roughness=widgetId(EditorWidget::MaterialNumberBase)+3u;
  revealProperty(f,roughness);tapWidget(f,roughness);
  AE_EXPECT_EQ(f.session.screen().numericField,roughness,"roughness reachable in mesh material");
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+10);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+9);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);tapWidget(f,widgetId(EditorWidget::NumericApply));
  AE_EXPECT_EQ(meshMaterial(*f.session.document().find(f.cube)).roughness,.2f,"exact roughness");
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

AE_TEST(r4_material_tab_lists_texture_bindings_and_the_picker_changes_the_instance) {
  Fixture f;auto entity=*f.session.document().find(f.cube);editMeshRenderer(entity)->mesh=1;
  f.session.document().applyEntityValues(f.cube,entity);f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::MeshMaterialTab));
  const u32 normal=widgetId(EditorWidget::MaterialTextureBase)+1u;
  revealProperty(f,normal);
  AE_EXPECT_TRUE(locateWidget(f.session,normal).x>=0,"binding de normal na aba Material");
  AE_EXPECT_TRUE(f.session.screen().materialSlotView.textureNames[1]=="Textura da fonte","sem troca, herda a da fonte");
  tapWidget(f,normal);
  AE_EXPECT_TRUE(f.session.screen().texturePicker,"seletor de textura aberto");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::TextureUseNone)).x>=0,"opção sem textura");
  tapWidget(f,widgetId(EditorWidget::TextureUseNone));
  AE_EXPECT_TRUE(!f.session.screen().texturePicker,"seletor fecha ao escolher");
  AE_EXPECT_TRUE(meshRenderer(*f.session.document().find(f.cube))->textures[1]==scene::MaterialTextureNone,"instância sem mapa normal");
  AE_EXPECT_TRUE(f.session.screen().materialSlotView.textureNames[1]=="Sem textura" &&
                 f.session.screen().materialSlotView.textureOrigins[1]=="só esta instância","linha mostra valor e alcance");
  std::vector<renderer::MapDrawState> extracted;
  AE_EXPECT_TRUE(f.session.extractMap(extracted),"extração");
  AE_EXPECT_EQ(extracted[0].material.textures[1],renderer::InvalidMapTexture,"o desenho perde o mapa normal");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"desfazer");
  AE_EXPECT_TRUE(!meshRenderer(*f.session.document().find(f.cube))->textures[1].valid(),"a troca volta pelo histórico");
}

AE_TEST(r4_material_tab_isolates_a_channel_and_flips_normal_y_per_instance) {
  Fixture f;auto entity=*f.session.document().find(f.cube);editMeshRenderer(entity)->mesh=1;
  f.session.document().applyEntityValues(f.cube,entity);f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::MeshMaterialTab));
  // As linhas vêm na ordem da lista: normal antes de isolar (o helper só avança).
  const u32 flip=widgetId(EditorWidget::MaterialNormalFlipCycle);
  revealProperty(f,flip);
  tapWidget(f,flip);
  tapWidget(f,flip);
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->channels.normalFlipY,scene::MaterialToggleOn,"Y invertido nesta instância");
  AE_EXPECT_TRUE(f.session.screen().materialSlotView.normalFlipLabel.starts_with("Y invertido"),"linha mostra a convenção");
  const auto depth=f.session.history().undoDepth();
  const u32 isolate=widgetId(EditorWidget::MaterialIsolateCycle);
  revealProperty(f,isolate);
  AE_EXPECT_TRUE(locateWidget(f.session,isolate).x>=0,"linha de isolar na aba Material");
  tapWidget(f,isolate);
  AE_EXPECT_EQ(f.session.screen().materialIsolate,scene::MaterialIsolateOcclusion,"isolando a oclusão");
  f.session.update();
  std::vector<renderer::MapDrawState> extracted;
  AE_EXPECT_TRUE(f.session.extractMap(extracted),"extração");
  AE_EXPECT_EQ(extracted[0].material.isolate,scene::MaterialIsolateOcclusion,"o desenho selecionado isola a oclusão");
  AE_EXPECT_EQ(f.session.history().undoDepth(),depth,"isolar não é edição do documento");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"desfazer");
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->channels.normalFlipY,scene::MaterialToggleOff,"volta pelo histórico");
}

AE_TEST(r4_material_tab_edits_alpha_mode_cutoff_and_sides_per_instance) {
  Fixture f;auto entity=*f.session.document().find(f.cube);editMeshRenderer(entity)->mesh=1;
  f.session.document().applyEntityValues(f.cube,entity);f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::MeshMaterialTab));
  const u32 alpha=widgetId(EditorWidget::MaterialAlphaCycle);
  revealProperty(f,alpha);
  AE_EXPECT_TRUE(f.session.screen().materialSlotView.alphaOrigin=="da fonte","sem troca, a fonte decide");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::MaterialCutoffUp)).x<0,"corte só é editável no modo recorte");
  tapWidget(f,alpha);
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->surface.alphaMode,scene::MaterialAlphaOpaque,"opaco nesta instância");
  tapWidget(f,alpha);
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->surface.alphaMode,scene::MaterialAlphaMask,"recorte nesta instância");
  // A linha do corte pode cair na página seguinte à do modo de alfa.
  revealProperty(f,widgetId(EditorWidget::MaterialCutoffUp));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::MaterialCutoffUp)).x>=0,"corte editável em recorte");
  tapWidget(f,widgetId(EditorWidget::MaterialCutoffUp));
  AE_EXPECT_TRUE(std::fabs(meshRenderer(*f.session.document().find(f.cube))->surface.alphaCutoff-.55f)<1e-5f,"corte sobe 0,05");
  std::vector<renderer::MapDrawState> extracted;
  AE_EXPECT_TRUE(f.session.extractMap(extracted),"extração");
  AE_EXPECT_TRUE(extracted[0].material.alphaMode==scene::MaterialAlphaMask,"o desenho recebe o recorte");
  const u32 sides=widgetId(EditorWidget::MaterialSidesCycle);
  revealProperty(f,sides);
  f.session.setMaterialCullingAvailable(false);
  AE_EXPECT_TRUE(locateWidget(f.session,sides).x<0,"faces sem culling no aparelho não recebe toque");
  AE_EXPECT_TRUE(f.session.screen().materialSlotView.sidesOrigin.find("sem efeito")!=std::string::npos,"e diz por quê");
  f.session.setMaterialCullingAvailable(true);
  tapWidget(f,sides);
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->surface.sides,scene::MaterialSidesSingle,"uma face nesta instância");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"desfazer");
  AE_EXPECT_EQ(meshRenderer(*f.session.document().find(f.cube))->surface.sides,scene::MaterialSidesKeep,"faces voltam pelo histórico");
}

AE_TEST(session_inspector_exposes_only_consumed_resource_controls) {
  Fixture f;
  auto mesh=*f.session.document().find(f.cube);editMeshRenderer(mesh)->mesh=1;
  f.session.document().applyEntityValues(f.cube,mesh);
  f.session.setSelection(f.cube);
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::MeshMaterialTab));
  const auto group=f.session.history().createEntity(f.session.document(),f.session.document().root(),EditorEntityKind::Folder,"Group");
  f.session.setSelection(group);f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::InspectorTabMaterial)).x<0,"group has no material tab");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::TransformFold)).x>=0,"new selection has its own folded transform");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleRigidBody)).x<0,"group has no physics consumer");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleCastShadow)).x<0,"group has no shadow draw");
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyEyeWidget(group)).x>=0,"inherited visibility remains available on the hierarchy row");
  f.session.setSelection(f.cube);f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::AddComponentMenu)).x>=0,"shared component catalog is available");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleSceneBody)).x<0,"component creation is not a toggle");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleRigidBody)).x<0,"dry authoring does not expose buoyancy");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleReceiveShadow)).x<0,"unconsumed receive shadow is absent");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ToggleStatic)).x<0,"unconsumed static flag is absent");
}

// Menu do componente como em Unity 6000.0 Manual/UsingComponents: Move Up/Down,
// Copy Component, Paste Component As New/Values, Reset, ajuda, interruptor no
// cabeçalho e toque longo como clique direito — pelos toques reais da tela.
AE_TEST(component_menu_moves_copies_pastes_as_new_resets_and_opens_reference) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  auto value=*doc.find(f.cube);
  value.components.add(scene::PhysicsBody::descriptor);
  auto *collider=static_cast<scene::Collider *>(value.components.add(scene::Collider::descriptor));
  collider->halfX=2.5f;
  auto *script=static_cast<scene::ScriptBehavior *>(value.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="project.Mover";script->source="Mover.cs";script->setProperty("speed","float","4");
  AE_EXPECT_TRUE(doc.applyEntityValues(f.cube,value),"objeto com malha, corpo, colisor e script");
  f.session.setSelection(f.cube);f.session.update();history.clear();
  const auto indexOf=[&](std::string_view type,u32 ordinal=0) {
    const auto *entity=doc.find(f.cube);
    for(u32 i=0;i<entity->components.size();++i) if(entity->components.at(i)->type().id==type && !ordinal--) return i;
    return ~0u;
  };
  const u32 colliderIndex=indexOf("astra.physics.collider");
  const u64 colliderInstance=doc.find(f.cube)->components.at(colliderIndex)->instanceId();

  // Os cartões do Inspector paginam no telefone: avança até o do colisor.
  for(u32 page=0;page<8 && locateWidget(f.session,widgetId(EditorWidget::ComponentFoldBase)+colliderIndex).x<0 &&
      locateWidget(f.session,widgetId(EditorWidget::ComponentNext)).x>=0;++page)
    tapWidget(f,widgetId(EditorWidget::ComponentNext));
  // Toque longo no cabeçalho abre o menu (clique direito da Unity).
  const auto header=locateWidget(f.session,widgetId(EditorWidget::ComponentFoldBase)+colliderIndex);
  AE_EXPECT_TRUE(header.x>=0,"cabeçalho do colisor visível");
  f.session.handlePointer({81,UiPointerPhase::Down,header,10.0});
  f.session.handlePointer({81,UiPointerPhase::Up,header,10.0+ui::kUiLongPressSeconds+.05});
  f.session.update();
  AE_EXPECT_EQ(f.session.screen().nativeMenu,colliderInstance,"toque longo abre o menu do componente");

  tapWidget(f,widgetId(EditorWidget::ComponentMoveUpBase)+colliderIndex);
  AE_EXPECT_EQ(indexOf("astra.physics.collider"),colliderIndex-1,"Mover para cima troca com o cartão anterior");
  AE_EXPECT_EQ(doc.find(f.cube)->components.at(colliderIndex-1)->instanceId(),colliderInstance,"identidade preservada");
  AE_EXPECT_EQ(history.undoDepth(),1u,"reordenar é um comando");
  AE_EXPECT_TRUE(history.undo(doc) && indexOf("astra.physics.collider")==colliderIndex,"Undo restaura a ordem");

  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+colliderIndex);
  tapWidget(f,widgetId(EditorWidget::ComponentCopyBase)+colliderIndex);
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+colliderIndex);
  tapWidget(f,widgetId(EditorWidget::ComponentPasteNewBase)+colliderIndex);
  const u32 copyIndex=indexOf("astra.physics.collider",1);
  AE_EXPECT_TRUE(copyIndex!=~0u,"Colar como novo cria outra instância");
  const auto *pasted=static_cast<const scene::Collider *>(doc.find(f.cube)->components.at(copyIndex));
  AE_EXPECT_EQ(pasted->halfX,2.5f,"a instância nova recebe os valores copiados");
  AE_EXPECT_TRUE(pasted->instanceId()!=colliderInstance,"com identidade própria");

  // Interruptor de ativo no cabeçalho.
  tapWidget(f,widgetId(EditorWidget::ComponentEnableBase)+colliderIndex);
  AE_EXPECT_TRUE(!static_cast<const scene::Collider *>(doc.find(f.cube)->components.findInstance(colliderInstance))->enabled,
                 "cabeçalho desliga o componente");

  // Referência oficial pelo menu.
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+colliderIndex);
  tapWidget(f,widgetId(EditorWidget::ComponentHelpBase)+colliderIndex);
  AE_EXPECT_TRUE(f.session.consumeExternalLink().starts_with("https://docs.unity3d.com/6000.0/"),"ajuda abre a referência versionada");

  // Comportamento C#: redefinir volta aos padrões do código.
  const u32 scriptIndex=indexOf("astra.script.behavior");
  tapWidget(f,widgetId(EditorWidget::ScriptMenuBase)+scriptIndex);
  tapWidget(f,widgetId(EditorWidget::ComponentResetBase)+scriptIndex);
  AE_EXPECT_TRUE(scene::scriptBehavior(doc.find(f.cube)->components.at(scriptIndex))->properties.empty(),
                 "Redefinir limpa os valores autorados do script");
  AE_EXPECT_TRUE(history.undo(doc) && !scene::scriptBehavior(doc.find(f.cube)->components.at(scriptIndex))->properties.empty(),
                 "Undo devolve os valores");
}

// Unity Manual/InspectorReferences: arrastar da Hierarchy para o campo. O
// arraste começa na linha do objeto e termina sobre o campo de referência do
// Inspector; a validação é a do seletor, e recusa não reparenteia nada.
AE_TEST(dragging_hierarchy_object_onto_reference_field_assigns_it) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  const auto camera=doc.createEntity(doc.root(),EditorEntityKind::Camera,"Câmera");
  auto value=*doc.find(camera);
  value.components.add(scene::Camera::descriptor);
  value.components.add(scene::CameraFollow::descriptor);
  AE_EXPECT_TRUE(doc.applyEntityValues(camera,value),"câmera com acompanhamento");
  u32 followIndex=0;
  for(u32 i=0;i<doc.find(camera)->components.size();++i)
    if(&doc.find(camera)->components.at(i)->type()==&scene::CameraFollow::descriptor) followIndex=i;
  f.session.setSelection(camera);f.session.update();history.clear();
  for(u32 page=0;page<8 && locateWidget(f.session,widgetId(EditorWidget::ComponentFoldBase)+followIndex).x<0 &&
      locateWidget(f.session,widgetId(EditorWidget::ComponentNext)).x>=0;++page)
    tapWidget(f,widgetId(EditorWidget::ComponentNext));
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase)+followIndex);
  const u32 field=widgetId(EditorWidget::ComponentReferenceBase)+followIndex;
  revealProperty(f,field);
  const auto target=locateWidget(f.session,field);
  const auto row=locateWidget(f.session,hierarchyRowWidget(f.cube));
  AE_EXPECT_TRUE(target.x>=0 && row.x>=0,"linha do cubo e campo Alvo visíveis juntos");
  const u64 parentBefore=doc.find(f.cube)->parent;
  f.down(82,row);
  f.move(82,{row.x+40,row.y});
  f.move(82,{(row.x+target.x)*.5f,(row.y+target.y)*.5f});
  f.move(82,target);
  f.up(82,target);f.session.update();
  const auto *follow=static_cast<const scene::CameraFollow *>(doc.find(camera)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_EQ(follow->target,static_cast<u64>(f.cube),"soltar sobre o campo atribui a referência");
  AE_EXPECT_EQ(doc.find(f.cube)->parent,parentBefore,"o objeto arrastado não muda de pai");
  AE_EXPECT_EQ(history.undoDepth(),1u,"atribuição é um comando");
  AE_EXPECT_TRUE(history.undo(doc),"Undo desfaz a atribuição");
  follow=static_cast<const scene::CameraFollow *>(doc.find(camera)->components.find(scene::CameraFollow::descriptor));
  AE_EXPECT_EQ(follow->target,0ull,"referência volta a vazia");
}

// Unity Manual/UsingComponents: arrastar um arquivo do Project para o campo de
// referência. Aqui o recurso vem da aba Recursos e cai no campo Malha.
AE_TEST(dragging_asset_onto_resource_field_assigns_it) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  const auto holder=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Suporte");
  auto value=*doc.find(holder);value.components.add(scene::MeshRenderer::descriptor);
  AE_EXPECT_TRUE(doc.applyEntityValues(holder,value),"malha vazia, sem referência");
  f.session.setSelection(holder);f.session.update();history.clear();
  for(u32 page=0;page<8 && locateWidget(f.session,widgetId(EditorWidget::ComponentFoldBase)).x<0 &&
      locateWidget(f.session,widgetId(EditorWidget::ComponentNext)).x>=0;++page)
    tapWidget(f,widgetId(EditorWidget::ComponentNext));
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  const u32 field=widgetId(EditorWidget::MeshChoose);
  revealProperty(f,field);
  const auto target=locateWidget(f.session,field);
  AE_EXPECT_TRUE(target.x>=0,"campo Malha visível");
  tapWidget(f,widgetId(EditorWidget::TabAssets));
  const auto row=locateWidget(f.session,widgetId(EditorWidget::AssetRowBase));
  AE_EXPECT_TRUE(row.x>=0,"recurso de malha listado");
  const auto entitiesBefore=doc.entityCount();
  f.down(83,row);
  f.move(83,{row.x+40,row.y});f.session.update();
  f.move(83,target);f.session.update();
  f.up(83,target);f.session.update();
  AE_EXPECT_EQ(meshAsset(*doc.find(holder)),1u,"soltar a malha no campo atribui o recurso");
  AE_EXPECT_EQ(doc.entityCount(),entitiesBefore,"nenhum objeto novo instanciado pelo arraste");
  AE_EXPECT_EQ(history.undoDepth(),1u,"atribuição é um comando");
}

// Unity Manual (Play mode): com o jogo rodando o Inspector continua editável,
// a mudança vale na hora e é descartada ao sair do Play. Aqui a edição passa
// pelos mesmos controles da autoria, chega ao mundo de execução e ao renderer,
// e o documento autoral nem muda de revisão.
AE_TEST(inspector_in_play_edits_the_running_world_and_stop_discards_it) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  auto value=*doc.find(f.cube);
  value.components.add(scene::Light::descriptor);
  AE_EXPECT_TRUE(doc.applyEntityValues(f.cube,value),"cubo com luz");
  u32 lightIndex=0;
  for(u32 i=0;i<doc.find(f.cube)->components.size();++i)
    if(&doc.find(f.cube)->components.at(i)->type()==&scene::Light::descriptor) lightIndex=i;
  f.session.setSelection(f.cube);f.session.update();history.clear();
  const u64 revision=doc.revision();
  AE_EXPECT_TRUE(f.session.startPlay(),"Play");
  std::vector<renderer::MapDrawState> draws;
  AE_EXPECT_TRUE(f.session.extractPlayMap(draws),"mundo de execução iniciado");
  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(f.session.extractLights(lights) && lights.size()==1u,"a luz roda no Play");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentFoldBase)+lightIndex).x<0,
                 "sem pedir, o Play não abre painéis sobre o jogo");

  tapWidget(f,widgetId(EditorWidget::PlayInspect));
  const auto *world=f.session.screen().document;
  AE_EXPECT_TRUE(world && world!=&doc,"o Inspector mostra o mundo de execução, não o documento");
  for(u32 page=0;page<8 && locateWidget(f.session,widgetId(EditorWidget::ComponentEnableBase)+lightIndex).x<0 &&
      locateWidget(f.session,widgetId(EditorWidget::ComponentNext)).x>=0;++page)
    tapWidget(f,widgetId(EditorWidget::ComponentNext));
  tapWidget(f,widgetId(EditorWidget::ComponentEnableBase)+lightIndex);
  world=f.session.screen().document;
  const auto *running=static_cast<const scene::Light *>(world->find(f.cube)->components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(running && !running->enabled,"desligar no Inspector desliga a luz em execução");
  lights.clear();
  AE_EXPECT_TRUE(f.session.extractLights(lights) && lights.empty(),"o renderer deixa de receber a luz no mesmo quadro");
  AE_EXPECT_TRUE(static_cast<const scene::Light *>(doc.find(f.cube)->components.find(scene::Light::descriptor))->enabled,
                 "o documento autoral continua com a luz ligada");

  // Texto: o nome abre sobre o espelho, com a época dele, e o commit vai ao mundo.
  f.session.usePlatformTextInput(true);
  // Tela estreita: um painel por vez, escolhido em "Painéis".
  if(locateWidget(f.session,widgetId(EditorWidget::HierarchyMenu)).x<0) {
    tapWidget(f,widgetId(EditorWidget::CompactPanelMenu));
    tapWidget(f,widgetId(EditorWidget::HierarchyToggle));
  }
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));
  tapWidget(f,widgetId(EditorWidget::RenameSelection));
  const auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.purpose!=EditorTextPurpose::None,"renomear abre o campo durante o Play");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"Em jogo",true),"nome aceito");
  world=f.session.screen().document;
  AE_EXPECT_TRUE(std::string(world->find(f.cube)->name)=="Em jogo","o objeto em execução ganhou o nome");
  AE_EXPECT_TRUE(std::string(doc.find(f.cube)->name)=="Cube","o nome autoral não mudou");
  AE_EXPECT_EQ(doc.revision(),revision,"nenhuma escrita no documento durante o Play");
  AE_EXPECT_EQ(history.undoDepth(),0u,"edição de Play não entra no Desfazer autoral");

  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
  f.session.advanceClock(1.0f);f.session.update();
  AE_EXPECT_TRUE(!f.session.isPlaying(),"Play parado");
  AE_EXPECT_TRUE(f.session.screen().document==&doc,"de volta ao documento autoral");
  AE_EXPECT_TRUE(static_cast<const scene::Light *>(doc.find(f.cube)->components.find(scene::Light::descriptor))->enabled &&
                 std::string(doc.find(f.cube)->name)=="Cube","parar descarta tudo o que foi feito em Play");
  AE_EXPECT_EQ(doc.revision(),revision,"documento idêntico ao de antes do Play");
}

AE_TEST(session_component_catalog_adds_closed_expands_removes_and_undoes) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::AddComponentMenu));
  selectComponentFamily(f,scene::ComponentFamily::Camera);
  revealAddEntry(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look"));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look")).x>=0,"look resolves its camera dependency");
  selectComponentFamily(f,scene::ComponentFamily::Physics3D);
  tapWidget(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.physics.body"));
  AE_EXPECT_TRUE(!physicsBody(*f.session.document().find(f.cube)),"prévia não adiciona o corpo");
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewConfirm));
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))!=nullptr,"body added by catalog");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"single add command");
  const u32 friction=widgetId(EditorWidget::ComponentNumberBase)+1+(1u<<8);
  AE_EXPECT_EQ(f.session.screen().expandedNative,physicsBody(*f.session.document().find(f.cube))->instanceId(),
               "new component opens in the Inspector");
  const auto revision=f.session.document().revision();
  revealProperty(f,friction);
  AE_EXPECT_TRUE(locateWidget(f.session,friction).x>=0,"new component properties are reachable");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"opening the Inspector never changes authoring");
  while(f.session.screen().propertyPage) tapWidget(f,widgetId(EditorWidget::PropertyPrevious));
  const u32 motion=widgetId(EditorWidget::ComponentEnumBase)+1;
  revealProperty(f,motion);
  tapWidget(f,motion);
  AE_EXPECT_EQ(f.session.screen().enumPicker,motion,"campo de enumeração abre a lista de opções");
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))->motion!=scene::BodyMotion::Kinematic,"abrir a lista não muda o valor");
  tapWidget(f,widgetId(EditorWidget::ComponentEnumOptionBase)+1);
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))->motion==scene::BodyMotion::Kinematic,"kinematic option");
  AE_EXPECT_EQ(f.session.screen().enumPicker,0u,"escolher fecha a lista");
  revealProperty(f,motion);
  tapWidget(f,motion);
  tapWidget(f,widgetId(EditorWidget::ComponentEnumOptionBase)+2);
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))->motion==scene::BodyMotion::Dynamic,"motion enum remains an editable value");
  tapWidget(f,widgetId(EditorWidget::AddComponentMenu));
  tapWidget(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.physics.body"));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentPreviewConfirm)).x<0,"duplicate has no confirmation");
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewBack));
  for(u32 page=0;page<12 && locateWidget(f.session,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.physics.character")).x<0;++page)
    tapWidget(f,widgetId(EditorWidget::ComponentNext));
  tapWidget(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.physics.character"));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentPreviewConfirm)).x<0,"incompatible character has no confirmation");
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewBack));
  tapWidget(f,widgetId(EditorWidget::AddComponentMenu));
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+1);
  tapWidget(f,widgetId(EditorWidget::ComponentRemoveBase)+1);
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))!=nullptr,"removal preview preserves body");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x>=0,"free component can be confirmed");
  tapWidget(f,widgetId(EditorWidget::ImpactRemoveConfirm));
  AE_EXPECT_TRUE(!physicsBody(*f.session.document().find(f.cube)),"explicit removal");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo removal");
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))->motion==scene::BodyMotion::Dynamic,"undo restores configured component");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo removal");
  AE_EXPECT_TRUE(!physicsBody(*f.session.document().find(f.cube)),"redo removes component");
}

AE_TEST(session_script_catalog_previews_addition_and_removal_with_undo) {
  Fixture f;auto &document=f.session.document();auto &history=f.session.history();
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("astra-script-preview-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root/"Scripts");
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};
  std::ofstream(root/"Scripts"/"Mover.cs")<<"public sealed class Mover : Astra.Behavior {}\n";
  AE_EXPECT_TRUE(f.session.setProjectDirectory(root.string().c_str()),"project code catalog connected");
  auto *code=const_cast<EditorCodeWorkspace*>(f.session.screen().code);
  AE_EXPECT_TRUE(code!=nullptr,"editor owns script catalog");
  const std::string report="ASTRA_CODE 1 1 0 1 \"project.Mover\" \"Mover\" \"Scripts/Mover.cs\" 2 "
      "\"speed\" \"Velocidade\" \"float\" \"target\" \"Alvo\" \"object\"";
  AE_EXPECT_TRUE(code->applyBuildReport(report,code->generation()),"script type staged");
  code->publishBuild();
  const auto id=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Actor");
  f.session.setSelection(id);f.session.update();history.clear();
  tapWidget(f,widgetId(EditorWidget::AddComponentMenu));
  selectComponentFamily(f,scene::ComponentFamily::Logic);
  revealAddEntry(f,widgetId(EditorWidget::ScriptAddBase));
  tapWidget(f,widgetId(EditorWidget::ScriptAddBase));
  AE_EXPECT_EQ(f.session.screen().scriptPreviewType,std::string("project.Mover"),"preview pins published type");
  AE_EXPECT_EQ(document.find(id)->components.size(),0u,"preview does not attach behavior");
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewBack));
  AE_EXPECT_EQ(history.undoDepth(),0u,"back creates no history");
  tapWidget(f,widgetId(EditorWidget::ScriptAddBase));
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewConfirm));
  const auto *script=scene::scriptBehavior(document.find(id)->components.at(0));
  AE_EXPECT_TRUE(script!=nullptr,"published behavior attached");
  AE_EXPECT_EQ(script->scriptType,std::string("project.Mover"),"type identity preserved");
  AE_EXPECT_EQ(script->source,std::string("Scripts/Mover.cs"),"source identity preserved");
  AE_EXPECT_EQ(f.session.screen().expandedScript,script->instanceId(),"new behavior focused");
  AE_EXPECT_EQ(history.undoDepth(),1u,"attachment is one Undo");
  const u64 instance=script->instanceId();
  tapWidget(f,widgetId(EditorWidget::ScriptMenuBase));
  tapWidget(f,widgetId(EditorWidget::ScriptRemoveBase));
  AE_EXPECT_TRUE(f.session.screen().impactRemoval,"behavior uses removal preview");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x>=0,"behavior can be removed");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_TRUE(document.find(id)->components.findInstance(instance)!=nullptr,"cancel preserves behavior");
  AE_EXPECT_EQ(history.undoDepth(),1u,"cancel creates no Undo");
  tapWidget(f,widgetId(EditorWidget::ScriptMenuBase));
  tapWidget(f,widgetId(EditorWidget::ScriptRemoveBase));
  tapWidget(f,widgetId(EditorWidget::ImpactRemoveConfirm));
  AE_EXPECT_EQ(document.find(id)->components.size(),0u,"confirmed removal detaches behavior");
  AE_EXPECT_EQ(history.undoDepth(),2u,"removal is one Undo");
  AE_EXPECT_TRUE(history.undo(document),"undo behavior removal");
  AE_EXPECT_TRUE(document.find(id)->components.findInstance(instance)!=nullptr,"undo restores same behavior instance");
}

AE_TEST(session_component_clipboard_and_reset_preserve_ownership_and_history) {
  Fixture f;
  auto source=*f.session.document().find(f.cube);
  auto *body=editPhysicsBody(source);body->mass=7;body->motion=scene::BodyMotion::Dynamic;
  AE_EXPECT_TRUE(f.session.document().applyEntityValues(f.cube,source),"configured source");
  const auto target=f.session.document().createEntity(f.session.document().root(),EditorEntityKind::Folder,"Target");
  auto value=*f.session.document().find(target);editPhysicsBody(value)->mass=2;
  AE_EXPECT_TRUE(f.session.document().applyEntityValues(target,value),"target");
  f.session.setSelection(f.cube);f.session.history().clear();
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+1);
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentPasteBase)+1).x<0,"empty clipboard cannot paste");
  const auto revision=f.session.document().revision();
  tapWidget(f,widgetId(EditorWidget::ComponentCopyBase)+1);
  AE_EXPECT_EQ(f.session.document().revision(),revision,"copy does not dirty document");
  AE_EXPECT_EQ(f.session.screen().nativeMenu,0u,"escolher uma ação fecha o menu, como na Unity");
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase)+1);
  tapWidget(f,widgetId(EditorWidget::ComponentResetBase)+1);
  AE_EXPECT_EQ(physicsBody(*f.session.document().find(f.cube))->mass,1.0f,"reset uses type defaults");
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(f.cube))->motion==scene::BodyMotion::Static,"motion reset too");
  f.session.setSelection(target);f.session.update();
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ComponentPasteBase));
  AE_EXPECT_EQ(physicsBody(*f.session.document().find(target))->mass,7.0f,"clipboard retains snapshot after source reset");
  AE_EXPECT_TRUE(physicsBody(*f.session.document().find(target))->motion==scene::BodyMotion::Dynamic,"paste includes motion");
  AE_EXPECT_EQ(physicsBody(*f.session.document().find(f.cube))->mass,1.0f,"paste never aliases source");
  AE_EXPECT_EQ(f.session.history().undoDepth(),2u,"reset and paste are separate commands");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo paste");
  AE_EXPECT_EQ(physicsBody(*f.session.document().find(target))->mass,2.0f,"target restored");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()),"redo paste");
  AE_EXPECT_EQ(physicsBody(*f.session.document().find(target))->mass,7.0f,"redo snapshot");
}

AE_TEST(session_platform_text_owns_focus_and_rename_is_one_undo) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  f.session.usePlatformTextInput(true);
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));tapWidget(f,widgetId(EditorWidget::RenameSelection));
  const auto edit=f.session.pendingTextEdit();
  const auto yaw=f.session.camera().yaw;const auto revision=f.session.document().revision();
  const auto at=f.viewportCentre();f.down(91,at);f.move(91,{at.x+80,at.y});f.up(91,at);
  AE_EXPECT_EQ(f.session.camera().yaw,yaw,"text focus blocks viewport");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"typing has not mutated scene");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"Portão",true),"UTF-8 name accepted");
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Portão","name retained");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one commit");
  f.session.history().undo(f.session.document());
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Cube","undo");
}
AE_TEST(session_play_input_focus_ignores_hidden_authoring_panels) {
  EditorSession session;
  auto &state=const_cast<EditorScreenState&>(session.screen());
  session.usePlatformTextInput(true); // Android enables the IME capability for the whole session.
  AE_EXPECT_TRUE(!session.gameplayInputFocused(),"Scene does not accept gameplay keys");
  state.searchingTextures=true;
  const auto previousEdit=session.pendingTextEdit();
  AE_EXPECT_EQ(previousEdit.purpose,EditorTextPurpose::TextureSearch,"Scene requests its active text field");
  state.workspace=EditorWorkspace::Play;
  state.texturePicker=true;
  state.qualityPanel=true;
  state.importPanel=true;
  AE_EXPECT_TRUE(session.gameplayInputFocused(),"hidden authoring panels do not block Play keys");
  state.searchingTextures=true;
  AE_EXPECT_TRUE(session.gameplayInputFocused(),"hidden authoring text draft does not block Play keys");
  AE_EXPECT_EQ(session.pendingTextEdit().purpose,EditorTextPurpose::None,"Play closes the authoring IME");
  AE_EXPECT_TRUE(!session.updateTextDraft(previousEdit,"late text",0),"late IME draft is rejected during Play");
  AE_EXPECT_TRUE(!session.completeTextEdit(previousEdit,"late commit",true),"late IME commit is rejected during Play");
  state.searchingTextures=false;
  state.playPaused=true;
  AE_EXPECT_TRUE(!session.gameplayInputFocused(),"paused Play releases gameplay keys");
}
AE_TEST(session_platform_text_cancel_and_stale_reply_preserve_authoring) {
  Fixture f;f.session.setSelection(f.cube);f.session.usePlatformTextInput(true);
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));tapWidget(f,widgetId(EditorWidget::RenameSelection));
  const auto edit=f.session.pendingTextEdit();const auto revision=f.session.document().revision();
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"ignored",false),"cancel");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"no mutation on cancel");
  AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"late",true),"closed editor rejects response");
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));tapWidget(f,widgetId(EditorWidget::RenameSelection));
  const auto stale=f.session.pendingTextEdit();f.session.document().setName(f.cube,"external");
  AE_EXPECT_TRUE(!f.session.completeTextEdit(stale,"overwrite",true),"revision guard");
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="external","external edit preserved");
}
AE_TEST(the_live_draft_draws_the_field_without_touching_the_document) {
  // O campo embutido: o teclado e do sistema, o campo e do editor. Para desenhar
  // o que esta sendo digitado o editor precisa do texto ANTES de confirmar --
  // com o AlertDialog ele so via o resultado final, e por isso a edicao tinha de
  // acontecer numa tela separada.
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();
  f.session.usePlatformTextInput(true);
  tapWidget(f,widgetId(EditorWidget::HierarchyMenu));tapWidget(f,widgetId(EditorWidget::RenameSelection));
  const auto edit=f.session.pendingTextEdit();
  const auto revision=f.session.document().revision();

  AE_EXPECT_TRUE(f.session.updateTextDraft(edit,"Port",4),"rascunho aceito");
  AE_EXPECT_TRUE(f.session.screen().platformDraft=="Port","o editor tem o texto vivo");
  AE_EXPECT_EQ(f.session.screen().platformCaret,4u,"e o cursor");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"rascunho nao toca no documento");
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"e nao cria historico");

  // Cursor alem do fim e preso ao fim: um indice de bytes que veio de outra
  // revisao do texto nao pode virar leitura fora do buffer na hora de desenhar.
  AE_EXPECT_TRUE(f.session.updateTextDraft(edit,"Por",99),"cursor fora de alcance");
  AE_EXPECT_EQ(f.session.screen().platformCaret,3u,"preso ao fim do texto");

  // Confirmar continua sendo o unico caminho que muda a cena.
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"Portao",true),"confirmar");
  AE_EXPECT_TRUE(std::string(f.session.document().find(f.cube)->name)=="Portao","nome aplicado");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"um unico comando");
  AE_EXPECT_TRUE(!f.session.updateTextDraft(edit,"tarde",5),"campo fechado recusa rascunho");
}

AE_TEST(the_live_draft_filters_a_search_while_it_is_typed) {
  // A busca e o unico efeito permitido antes de confirmar, porque nao toca em
  // nada. Com o dialogo ela so filtrava depois de aplicar, o que tornava a busca
  // inutil justamente enquanto se busca.
  Fixture f;f.session.usePlatformTextInput(true);
  tapWidget(f,widgetId(EditorWidget::HierarchySearch));
  const auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.purpose==EditorTextPurpose::HierarchySearch,"busca da hierarquia aberta");
  const auto revision=f.session.document().revision();
  AE_EXPECT_TRUE(f.session.updateTextDraft(edit,"Cub",3),"rascunho da busca");
  AE_EXPECT_TRUE(std::string(f.session.screen().renameText)=="Cub","o filtro ve o texto vivo");
  AE_EXPECT_EQ(f.session.document().revision(),revision,"buscar nao muda a cena");
}

AE_TEST(session_component_property_search_uses_ime_without_authoring_changes) {
  Fixture f;auto &document=f.session.document();auto &history=f.session.history();
  const auto target=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Óptica");
  auto value=*document.find(target);editCamera(value);
  AE_EXPECT_TRUE(document.applyEntityValues(target,value),"câmera preparada");
  f.session.setSelection(target);f.session.usePlatformTextInput(true);f.session.update();history.clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::ComponentPropertySearch));
  const auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.purpose==EditorTextPurpose::PropertySearch,"busca de propriedade abre o IME");
  const auto revision=document.revision();
  AE_EXPECT_TRUE(f.session.updateTextDraft(edit,"prioridade",10),"rascunho aceito");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentNumberBase)+(3u<<8)).x>=0,
                 "campo de outra aba aparece enquanto digita");
  AE_EXPECT_EQ(document.revision(),revision,"filtro não edita a cena");
  AE_EXPECT_EQ(history.undoDepth(),0u,"filtro não cria Undo");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"prioridade",true),"busca confirmada");
  AE_EXPECT_TRUE(f.session.screen().propertyQuery=="prioridade","consulta preservada no Inspector");
  tapWidget(f,widgetId(EditorWidget::ComponentPropertySearchClear));
  AE_EXPECT_TRUE(f.session.screen().propertyQuery.empty(),"limpar restaura a aba anterior");
  AE_EXPECT_EQ(history.undoDepth(),0u,"limpar busca não cria Undo");
}

AE_TEST(session_restores_one_reflected_property_with_undo) {
  Fixture f;auto &document=f.session.document();auto &history=f.session.history();
  const auto target=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Óptica");
  auto value=*document.find(target);auto *camera=editCamera(value);
  camera->verticalFov=43;camera->priority=7;
  AE_EXPECT_TRUE(document.applyEntityValues(target,value),"câmera preparada");
  const auto instance=cameraComponent(*document.find(target))->instanceId();
  f.session.setSelection(target);f.session.usePlatformTextInput(true);f.session.update();history.clear();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::ComponentPropertySearch));
  const auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"campo vertical",true),"busca concluída");
  const auto reset=widgetId(EditorWidget::ComponentFieldResetBase)+(2u<<8);
  tapWidget(f,reset);
  const auto *restored=cameraComponent(*document.find(target));
  AE_EXPECT_EQ(restored->verticalFov,60.f,"somente o FOV volta ao padrão");
  AE_EXPECT_EQ(restored->priority,7.f,"outra propriedade mantém valor autoral");
  AE_EXPECT_EQ(restored->instanceId(),instance,"identidade preservada");
  AE_EXPECT_EQ(history.undoDepth(),1u,"restauração gera um comando");
  AE_EXPECT_TRUE(history.undo(document),"desfazer restauração");
  AE_EXPECT_EQ(cameraComponent(*document.find(target))->verticalFov,43.f,"valor anterior recuperado");
  AE_EXPECT_TRUE(history.redo(document),"refazer restauração");
  AE_EXPECT_EQ(cameraComponent(*document.find(target))->verticalFov,60.f,"valor padrão reaplicado");
}

AE_TEST(session_platform_number_accepts_negative_decimal_and_rejects_invalid) {
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();f.session.usePlatformTextInput(true);
  tapWidget(f,widgetId(EditorWidget::TransformFold));
  tapWidget(f,transformFieldWidget(0,0));const auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"1.2.3",true),"invalid decimal rejected");
  AE_EXPECT_EQ(f.session.history().undoDepth(),0u,"invalid does not create undo");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"-2,5",true),"localized signed decimal");
  AE_EXPECT_EQ(f.session.document().find(f.cube)->transform.position[0],-2.5f,"value applied");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one command");
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
  tapWidget(f,widgetId(EditorWidget::TransformFold));
  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x<0,"no hierarchy over runtime joystick");
  AE_EXPECT_TRUE(locateWidget(f.session,transformFieldWidget(0,0)).x<0,"no inspector during play");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::FrameSelection)).x<0,"no scene overlay during play");
  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
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
  Fixture f;importWaterResources(f);f.session.history().clear();
  openProjectSection(f,EditorProjectSection::Water);
  tapWidget(f,widgetId(EditorWidget::TransformFieldBase)+24);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  const auto &doc=f.session.document();const auto *root=doc.find(doc.root());
  AE_EXPECT_TRUE(waterSettings(*root).enabled,"water authored in scene");
  AE_EXPECT_EQ(waterSettings(*root).legacyField(0+0),2.0f,"wave height edited");
  const auto archive=serializeEditorDocument(doc,123);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,123,restored),"archive v3 roundtrip");
  AE_EXPECT_TRUE(waterSettings(*restored.find(restored.root())).enabled,"enabled saved");
  AE_EXPECT_EQ(waterSettings(*restored.find(restored.root())).legacyField(0+0),2.0f,"value saved");
  f.session.history().undo(f.session.document());
  AE_EXPECT_TRUE(!waterSettings(*f.session.document().find(doc.root())).enabled,"undo restores legacy water source");
}

AE_TEST(session_empty_source_hides_legacy_resource_menus_and_clears_stale_context) {
  Fixture f;AE_EXPECT_TRUE(f.session.importMap({}, {}, false),"independent source");
  f.session.update();tapWidget(f,widgetId(EditorWidget::ProjectMenu));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::TabAssets)).x<0,"no resource browser without resources");
  tapWidget(f,widgetId(EditorWidget::TabProject));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ProjectSectionBase)+
                 static_cast<u32>(EditorProjectSection::Water)).x<0,"no water section without water resources");
  importWaterResources(f);
  tapWidget(f,widgetId(EditorWidget::ProjectSectionBase)+static_cast<u32>(EditorProjectSection::Water));
  AE_EXPECT_TRUE(f.session.screen().projectSection==EditorProjectSection::Water,"water library enables the water section");
  AE_EXPECT_TRUE(f.session.importMap({}, {}, false),"replace library");f.session.update();
  AE_EXPECT_TRUE(f.session.screen().projectSection==EditorProjectSection::Layers,"removed capability cannot leave stale water context");
}

AE_TEST(session_missing_mesh_does_not_create_an_invisible_pick_target) {
  Fixture f;auto &doc=f.session.document();
  auto values=*doc.find(f.cube);editMeshRenderer(values)->mesh=0;doc.applyEntityValues(f.cube,values);
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
  Fixture f;importWaterResources(f);f.session.history().clear();
  openProjectSection(f,EditorProjectSection::Water);
  const u32 windWidget=widgetId(EditorWidget::TransformFieldBase)+37;
  for(u32 page=0;page<34 && locateWidget(f.session,windWidget).x<0;++page)
    tapWidget(f,widgetId(EditorWidget::PropertyNext));
  AE_EXPECT_TRUE(locateWidget(f.session,windWidget).x>=0,"wind accessible through Inspector pages");
  tapWidget(f,windWidget);
  tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+1);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  const auto &doc=f.session.document();const auto *root=doc.find(doc.root());
  AE_EXPECT_TRUE(waterSettings(*root).spectrumEnabled,"touch activates spectral authoring");
  AE_EXPECT_EQ(waterSettings(*root).legacyField(0+13),2.0f,"wind changed using real input routing");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo touch edit");
  AE_EXPECT_TRUE(!waterSettings(*doc.find(doc.root())).spectrumEnabled,"undo restores inheritance");
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
  AE_EXPECT_EQ(waterRoute(*f.session.document().find(river)).count,3u,"three editable points");
  const auto point=waterRoute(*f.session.document().find(river)).points[0];
  tapWidget(f,widgetId(EditorWidget::RoutePointBase));
  AE_EXPECT_EQ(waterRoute(*f.session.document().find(river)).points[0].position[0],point.position[0],"tap does not move point");
  tapWidget(f,widgetId(EditorWidget::RoutePointAdd));
  AE_EXPECT_EQ(waterRoute(*f.session.document().find(river)).count,4u,"insert point");
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()),"undo insert");
  AE_EXPECT_EQ(waterRoute(*f.session.document().find(river)).count,3u,"topology restored");
  f.session.update();
  const auto property=widgetId(EditorWidget::TransformFieldBase)+RoutePropertyBase+f.session.screen().routePoint*8+7;
  for(u32 page=0;page<8 && locateWidget(f.session,property).x<0;++page) tapWidget(f,widgetId(EditorWidget::PropertyNext));
  AE_EXPECT_TRUE(locateWidget(f.session,property).x>=0,"last per-point property is reachable");
  tapWidget(f,property);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+9);tapWidget(f,widgetId(EditorWidget::NumericKeyBase)+4);
  tapWidget(f,widgetId(EditorWidget::NumericApply));
  AE_EXPECT_EQ(waterRoute(*f.session.document().find(river)).points[f.session.screen().routePoint].tension,.5f,"numeric tension committed");
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
  tapWidget(f,widgetId(EditorWidget::TabLighting));
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
  tapWidget(f,recipeWidget("basic.camera"));
  const auto pose=resolveSceneCamera(f.session.document());
  AE_EXPECT_TRUE(pose.entity!=0,"câmera criada na cena");
  float position[3];editorCameraPosition(f.session.camera(),position);
  for(u32 i=0;i<3;++i) AE_EXPECT_TRUE(std::abs(pose.position[i]-position[i])<.001f,"mesma vista de trabalho");
  tapWidget(f,widgetId(EditorWidget::Undo));
  AE_EXPECT_EQ(resolveSceneCamera(f.session.document()).entity,0u,"desfaz a criação inteira");
}
AE_TEST(session_creation_menu_places_objects_under_selected_parent_or_root) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  const auto parent=doc.find(f.cube)->parent;
  auto transform=doc.find(parent)->transform;
  transform.position[0]=4;transform.position[1]=2;transform.rotationDegrees[1]=35;
  AE_EXPECT_TRUE(history.setTransform(doc,parent,transform),"pai com pose própria");
  f.session.setSelection(parent);f.session.update();history.clear();
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  AE_EXPECT_TRUE(f.session.screen().creationAsChild,"seleção define filho como destino inicial");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::CreationAtRoot)).x>=0,"destino raiz visível");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::CreationAsChild)).x>=0,"destino filho visível");
  tapWidget(f,recipeWidget("basic.camera"));
  const auto camera=f.session.selection();
  AE_EXPECT_EQ(doc.find(camera)->parent,parent,"câmera anexada ao pai escolhido");
  float world[16],eye[3];AE_EXPECT_TRUE(editorWorldMatrix(doc,camera,world),"pose mundial da câmera");
  editorCameraPosition(f.session.camera(),eye);
  for(u32 i=0;i<3;++i) AE_EXPECT_TRUE(std::abs(world[12+i]-eye[i])<.001f,"câmera mantém posição da vista");
  AE_EXPECT_EQ(history.undoDepth(),1u,"criação da câmera é um comando");
  AE_EXPECT_TRUE(history.undo(doc),"desfaz câmera e vínculo");
  AE_EXPECT_TRUE(!doc.exists(camera),"câmera removida no desfazer");

  f.session.setSelection(parent);f.session.update();
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  tapWidget(f,widgetId(EditorWidget::CreationAtRoot));
  AE_EXPECT_TRUE(!f.session.screen().creationAsChild,"destino raiz escolhido explicitamente");
  tapWidget(f,widgetId(EditorWidget::CreateGroup));
  const auto group=f.session.selection();
  AE_EXPECT_EQ(doc.find(group)->parent,doc.root(),"objeto vazio criado na raiz");
  AE_EXPECT_TRUE(history.undo(doc),"desfaz objeto vazio");
  AE_EXPECT_TRUE(!doc.exists(group),"objeto vazio removido no desfazer");
}
AE_TEST(session_creation_menu_parents_resource_objects_and_undoes_each_creation) {
  Fixture f;importWaterResources(f);
  auto &doc=f.session.document();auto &history=f.session.history();
  const auto parent=history.createEntity(doc,doc.root(),EditorEntityKind::Folder,"Conjunto");
  AE_EXPECT_TRUE(parent!=kInvalidEntity,"pai autoral");
  auto pose=doc.find(parent)->transform;pose.position[0]=7;pose.scale[0]=2;
  AE_EXPECT_TRUE(history.setTransform(doc,parent,pose),"pai transformado");
  history.clear();
  const EditorWidget actions[]{EditorWidget::CreateCube,EditorWidget::CreateGround,
    EditorWidget::CreateFiniteWater,EditorWidget::CreateOceanWater,
    EditorWidget::CreateRiverWater,EditorWidget::CreateBuoyantBox};
  for(const auto action:actions) {
    f.session.setSelection(parent);f.session.update();
    tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
    AE_EXPECT_TRUE(f.session.screen().creationAsChild,"criação contextual para recurso");
    tapWidget(f,widgetId(action));
    const auto created=f.session.selection();
    AE_EXPECT_TRUE(created!=parent && doc.exists(created),"objeto criado");
    AE_EXPECT_EQ(doc.find(created)->parent,parent,"recurso é filho do pai escolhido");
    AE_EXPECT_EQ(history.undoDepth(),1u,"criação e composição em um comando");
    AE_EXPECT_TRUE(history.undo(doc),"desfaz criação do recurso");
    AE_EXPECT_TRUE(!doc.exists(created),"recurso removido no desfazer");
    history.clear();
  }
}
AE_TEST(session_builtin_light_and_physics_objects_are_complete_creations) {
  Fixture f;
  AE_EXPECT_TRUE(f.session.importMap({}, {}, false),"scene without imported geometry");
  f.session.update();
  auto &doc=f.session.document();auto &history=f.session.history();
  const std::string_view actions[]{"light.directional","light.point",
    "light.spot","physics.static_box","physics.static_sphere",
    "physics.static_capsule","physics.dynamic_box","physics.trigger_box",
    "physics.dynamic_sphere","physics.dynamic_capsule",
    "physics.trigger_sphere","physics.trigger_capsule",
    "physics.kinematic_box","physics.kinematic_sphere",
    "physics.character"};
  EditorEntityId createdIds[std::size(actions)]{};
  for(u32 index=0;index<std::size(actions);++index) {
    f.session.setSelection(doc.root());f.session.update();
    tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
    tapWidget(f,recipeWidget(actions[index]));
    const auto id=f.session.selection();const auto *object=doc.find(id);
    createdIds[index]=id;
    AE_EXPECT_TRUE(object && id!=doc.root(),"created object has identity");
    AE_EXPECT_EQ(object->parent,doc.root(),"created in root");
    AE_EXPECT_TRUE(object->name[0]!=0,"creation keeps its name");
    if(index<3) {
      const auto *light=runtime::lightComponent(*object);
      AE_EXPECT_TRUE(light!=nullptr,"light component attached");
      AE_EXPECT_EQ(static_cast<u32>(light->kind),index,"correct light modality");
    } else if(index<std::size(actions)-1) {
      const auto *body=runtime::physicsBody(*object);
      const auto *collider=runtime::colliderComponent(*object);
      AE_EXPECT_TRUE(body && collider,"physical body has a real shape");
      const auto sphere=actions[index]=="physics.static_sphere" ||
        actions[index]=="physics.dynamic_sphere" || actions[index]=="physics.trigger_sphere" ||
        actions[index]=="physics.kinematic_sphere";
      const auto capsule=actions[index]=="physics.static_capsule" ||
        actions[index]=="physics.dynamic_capsule" || actions[index]=="physics.trigger_capsule";
      AE_EXPECT_EQ(collider->shape,sphere?scene::ColliderShape::Sphere:
        capsule?scene::ColliderShape::Capsule:scene::ColliderShape::Box,"correct shape");
      const auto sensor=actions[index]=="physics.trigger_box" ||
        actions[index]=="physics.trigger_sphere" || actions[index]=="physics.trigger_capsule";
      const auto dynamic=actions[index]=="physics.dynamic_box" ||
        actions[index]=="physics.dynamic_sphere" || actions[index]=="physics.dynamic_capsule";
      const auto kinematic=actions[index]=="physics.kinematic_box" ||
        actions[index]=="physics.kinematic_sphere";
      AE_EXPECT_EQ(body->sensor,sensor,"sensor intent reaches body");
      AE_EXPECT_EQ(body->motion,dynamic?scene::BodyMotion::Dynamic:
        kinematic?scene::BodyMotion::Kinematic:scene::BodyMotion::Static,"motion intent reaches body");
    } else {
      AE_EXPECT_TRUE(runtime::characterComponent(*object)!=nullptr,"character controller attached");
    }
    AE_EXPECT_TRUE(history.undo(doc),"one undo removes composed object");
    AE_EXPECT_TRUE(!doc.exists(id),"no partial object after undo");
    AE_EXPECT_TRUE(history.redo(doc) && doc.exists(id),"redo restores component composition");
  }
  AE_EXPECT_EQ(doc.entityCount(),16u,"fifteen distinct creations survive");
  f.session.setSelection(createdIds[6]);f.session.update();
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  tapWidget(f,recipeWidget("physics.static_sphere"));
  AE_EXPECT_EQ(doc.entityCount(),16u,"invalid physics ancestry creates no partial object");
  AE_EXPECT_EQ(history.undoDepth(),15u,"rejected creation records no undo command");
  tapWidget(f,widgetId(EditorWidget::CreateMenuClose));
  namespace fs=std::filesystem;
  const auto path=fs::temp_directory_path()/("aether-builtins-"+
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".aescene");
  struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove(path,error);}} cleanup{path};
  AE_EXPECT_TRUE(f.session.save(path.string().c_str(),0),"save authored objects");
  EditorSession reopened;
  AE_EXPECT_TRUE(reopened.load(path.string().c_str(),0),"reopen authored objects");
  AE_EXPECT_EQ(reopened.document().entityCount(),16u,"all types survive archive");
  for(u32 index=3;index<std::size(actions)-1;++index) {
    const auto *original=doc.find(createdIds[index]);
    const auto *loaded=reopened.document().find(createdIds[index]);
    AE_EXPECT_TRUE(original && loaded,"physical object survives archive by identity");
    AE_EXPECT_EQ(runtime::physicsBody(*loaded)->motion,runtime::physicsBody(*original)->motion,"motion survives archive");
    AE_EXPECT_EQ(runtime::physicsBody(*loaded)->sensor,runtime::physicsBody(*original)->sensor,"sensor survives archive");
    AE_EXPECT_EQ(runtime::colliderComponent(*loaded)->shape,runtime::colliderComponent(*original)->shape,"shape survives archive");
  }
  AE_EXPECT_TRUE(reopened.startPlay(),"composed objects enter Play");
  std::vector<renderer::MapDrawState> playDraws;
  AE_EXPECT_TRUE(reopened.extractPlayMap(playDraws),"real physics and light consumers initialize in Play");
  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(reopened.extractLights(lights),"runtime extracts authored lights");
  AE_EXPECT_EQ(lights.size(),3u,"all three light modalities reach renderer");
  for(u32 index=0;index<lights.size();++index)
    AE_EXPECT_EQ(static_cast<u32>(lights[index].modality),index,"light modality survives archive and Play");
}
// Toda receita composta do catálogo é executada pelo mesmo caminho: um objeto,
// todos os tipos declarados, valores iniciais aplicados e um único Undo. Uma
// receita nova entra aqui sem teste próprio para existir.
AE_TEST(every_composed_recipe_creates_its_declared_composition_in_one_command) {
  Fixture f;
  AE_EXPECT_TRUE(f.session.importMap({}, {}, false),"cena sem recursos importados");
  auto &doc=f.session.document();auto &history=f.session.history();
  u32 composed=0;
  for(u32 index=0;index<editorCreationCatalog.size();++index) {
    const auto &recipe=editorCreationCatalog[index];
    if(!recipe.composed()) continue;
    ++composed;
    f.session.setSelection(doc.root());history.clear();
    const auto id=f.session.createRecipe(index,doc.root());
    const auto *object=doc.find(id);
    AE_EXPECT_TRUE(object!=nullptr,"receita cria objeto");
    if(!object) continue;
    AE_EXPECT_TRUE(object->kind==recipe.kind,"tipo de objeto declarado");
    for(const auto &part:recipe.components)
      AE_EXPECT_TRUE(object->components.find(part.type)!=nullptr,"componente declarado presente");
    for(const auto &part:recipe.components) for(const auto &initial:part.values) {
      const auto *component=object->components.find(initial.component);
      AE_EXPECT_TRUE(component!=nullptr,"valor inicial tem dono");
      if(!component) continue;
      if(const auto *number=std::get_if<float>(&initial.value)) {
        for(const auto &p:component->type().numbers) if(p.id==initial.property)
          AE_EXPECT_EQ(p.read(*component),*number,"número inicial aplicado");
      } else if(const auto *option=std::get_if<u32>(&initial.value)) {
        for(const auto &p:component->type().enums) if(p.id==initial.property)
          AE_EXPECT_EQ(p.read(*component),*option,"enumeração inicial aplicada");
      } else if(const auto *flag=std::get_if<bool>(&initial.value)) {
        for(const auto &p:component->type().booleans) if(p.id==initial.property)
          AE_EXPECT_EQ(p.read(*component),*flag,"booleano inicial aplicado");
      }
    }
    AE_EXPECT_EQ(history.undoDepth(),1u,"receita é um único comando");
    AE_EXPECT_TRUE(history.undo(doc) && !doc.exists(id),"Undo remove a composição inteira");
  }
  AE_EXPECT_TRUE(composed>=18u,"receitas compostas descritas como dados");
}
AE_TEST(session_timer_creation_is_one_undoable_object_with_real_component) {
  Fixture f;
  AE_EXPECT_TRUE(f.session.importMap({}, {}, false),"cena sem recursos importados");
  auto &doc=f.session.document();f.session.setSelection(doc.root());f.session.update();
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  tapWidget(f,recipeWidget("gameplay.timer"));
  const auto id=f.session.selection();const auto *object=doc.find(id);
  AE_EXPECT_TRUE(object && object->components.find(scene::Timer::descriptor),"Timer criado na hierarquia");
  AE_EXPECT_TRUE(f.session.history().undo(doc) && !doc.exists(id),"Undo remove objeto e componente");
  AE_EXPECT_TRUE(f.session.history().redo(doc) && doc.find(id)->components.find(scene::Timer::descriptor),"Redo restaura timer");
  // O Timer é editado no Inspector; não há workspace própria por tipo de componente.
  f.session.setSelection(doc.root());f.session.update();
  tapWidget(f,widgetId(EditorWidget::ProjectMenu));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::TabProject)).x>=0,"menu leva às configurações do projeto");
}
AE_TEST(physics_workspace_edits_solver_matrix_with_undo_and_archive) {
  Fixture f;auto &doc=f.session.document();
  openProjectSection(f,EditorProjectSection::Layers);
  tapWidget(f,widgetId(EditorWidget::PhysicsLayerAdd));
  AE_EXPECT_TRUE(doc.layers().named(1),"camada criada");
  tapWidget(f,widgetId(EditorWidget::PhysicsLayerRename));
  const auto nameEdit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(nameEdit.purpose==EditorTextPurpose::PhysicsLayerName,"edição de nome usa IME");
  AE_EXPECT_TRUE(f.session.completeTextEdit(nameEdit,"Jogador",true),"renomeia camada");
  AE_EXPECT_TRUE(doc.layers().name(1)=="Jogador","nome autorado");
  tapWidget(f,widgetId(EditorWidget::PhysicsInteractionBase));
  AE_EXPECT_TRUE(!doc.layers().interacts(1,0) && !doc.layers().interacts(0,1),"matriz recíproca alterada pela UI");
  AE_EXPECT_TRUE(f.session.history().undo(doc),"Undo da matriz");
  AE_EXPECT_TRUE(doc.layers().interacts(1,0),"Undo restaura colisão");
  AE_EXPECT_TRUE(f.session.history().redo(doc),"Redo da matriz");
  AE_EXPECT_TRUE(!doc.layers().interacts(1,0),"Redo restaura filtro");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,99),99,restored),"camadas salvas e recarregadas");
  AE_EXPECT_TRUE(restored.layers().name(1)=="Jogador" && !restored.layers().interacts(1,0),"arquivo preserva regra do solver");
  f.session.setSurface({0,0,400,740},{});f.session.update();
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::PhysicsInteractionBase)).x>=0,"camadas continuam editáveis em tela estreita");
}
AE_TEST(input_workspace_authors_runtime_action_with_history_and_archive) {
  Fixture f;auto &doc=f.session.document();
  openProjectSection(f,EditorProjectSection::Input);
  tapWidget(f,widgetId(EditorWidget::InputActionAdd));
  AE_EXPECT_EQ(doc.inputActions().actions().size(),4u,"ação é criada no dado autorado");
  tapWidget(f,widgetId(EditorWidget::InputActionRowBase));
  AE_EXPECT_EQ(f.session.screen().inputActionIndex,0u,"lista seleciona ação diretamente");
  tapWidget(f,widgetId(EditorWidget::InputActionRowBase)+3);
  AE_EXPECT_EQ(f.session.screen().inputActionIndex,3u,"lista retorna à ação recém-criada");
  tapWidget(f,widgetId(EditorWidget::InputActionRename));
  auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.purpose==EditorTextPurpose::InputActionName,"nome abre editor de texto");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"Interagir",true),"nome alterado");
  AE_EXPECT_TRUE(doc.inputActions().find("Interagir")!=nullptr,"id novo persistido no mapa");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::InputRoleMove)).x<0,
                 "ação Botão não oferece papel de movimento inválido");
  tapWidget(f,widgetId(EditorWidget::InputBindingRowBase));
  tapWidget(f,widgetId(EditorWidget::InputBindingSource)); // TouchButton -> Key
  tapWidget(f,widgetId(EditorWidget::InputDetailsToggle));
  tapWidget(f,widgetId(EditorWidget::InputBindingCode));
  edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.purpose==EditorTextPurpose::InputNumber,"código abre editor numérico");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"69",true),"código de tecla configurado");
  const auto *action=doc.inputActions().find("Interagir");
  AE_EXPECT_TRUE(action && action->bindings[0].source==runtime::InputSource::Key && action->bindings[0].code==69,
                 "vínculo completo no mapa");
  runtime::InputService service;service.setMap(doc.inputActions());
  runtime::InputDeviceState device;device.keys.push_back(69);service.submit(device);
  AE_EXPECT_TRUE(service.justPressed("Interagir"),"entrada autorada aciona runtime");
  AE_EXPECT_TRUE(f.session.history().undo(doc),"Undo do código");
  AE_EXPECT_TRUE(doc.inputActions().find("Interagir")->bindings[0].code!=69,"Undo altera dado real");
  AE_EXPECT_TRUE(f.session.history().redo(doc),"Redo do código");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,99),99,restored),"mapa salva e abre");
  service.setMap(restored.inputActions());service.submit(device);
  AE_EXPECT_TRUE(service.pressed("Interagir"),"entrada recarregada aciona runtime");
  f.session.setSurface({0,0,400,740},{});f.session.update();
  tapWidget(f,widgetId(EditorWidget::InputDetailsToggle));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::InputBindingSource)).x>=0,
                 "vínculos permanecem acessíveis em tela estreita");
}
AE_TEST(session_follow_camera_creation_links_selected_target) {
  Fixture f;auto &doc=f.session.document();
  f.session.setSelection(f.cube);f.session.update();
  tapWidget(f,widgetId(EditorWidget::HierarchyAdd));
  tapWidget(f,recipeWidget("basic.follow_camera"));
  const auto id=f.session.selection();const auto *camera=doc.find(id);
  AE_EXPECT_TRUE(camera && camera->parent==doc.root(),"câmera criada fora da hierarquia do alvo");
  const auto *follow=camera?static_cast<const scene::CameraFollow *>(camera->components.find(scene::CameraFollow::descriptor)):nullptr;
  AE_EXPECT_TRUE(follow && camera->components.find(scene::Camera::descriptor),"composição câmera + follow real");
  AE_EXPECT_EQ(follow->target,f.cube,"objeto selecionado vira alvo");
  AE_EXPECT_TRUE(f.session.history().undo(doc) && !doc.exists(id),"criação composta é um Undo");
  AE_EXPECT_TRUE(f.session.history().redo(doc) && doc.exists(id),"Redo restaura referência");
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

AE_TEST(independent_authoring_library_creates_dry_geometry_without_water_or_scene_objects) {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"generic resource generation");
  AE_EXPECT_EQ(vertices.size(),usize{24*renderer::MapVertexStride},"six faces");
  AE_EXPECT_EQ(indices.size(),usize{36},"twelve triangles");
  AE_EXPECT_EQ(materials[0].flags,renderer::BoxAuthoringResource,"no water flags");
  EditorSession session;
  AE_EXPECT_TRUE(session.importMap(draws,materials,false,vertices,indices),"independent resources");
  AE_EXPECT_EQ(session.document().entityCount(),1u,"resource is not a hidden object");
  AE_EXPECT_TRUE(!waterCreationAvailable(session.screen()),"water creation unavailable");
  const float position[]{2,3,-4};EditorAssetInstantiation options;options.name="Block";
  const auto id=session.instantiateAsset(0,session.document().root(),position,&options);
  AE_EXPECT_TRUE(id!=kInvalidEntity,"author geometry with public API");
  AE_EXPECT_EQ(session.document().find(id)->transform.position[0],2.0f,"authored transform");
  EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(session.document(),0),0,loaded),"independent archive");
  AE_EXPECT_EQ(meshAsset(*loaded.find(id)),1u,"stable built-in cube reference");
}

AE_TEST(inspector_object_card_and_actions_edit_the_selected_object_through_history) {
  // O objeto selecionado se edita onde estão as propriedades dele (padrão
  // Unity/Godot): configurações universais num card, cópia e redefinição no
  // card da transformação e as ações do objeto no ⋮ do cabeçalho. Tudo por toque
  // e pelo mesmo histórico.
  Fixture f;f.session.setSelection(f.cube);f.session.history().clear();f.session.update();
  auto &document=f.session.document();

  tapWidget(f,widgetId(EditorWidget::ObjectFold));
  tapWidget(f,widgetId(EditorWidget::ToggleVisible));
  AE_EXPECT_TRUE(!document.find(f.cube)->visible,"visível desligado pelo card Objeto");
  AE_EXPECT_TRUE(f.session.history().undo(document),"desfaz");
  AE_EXPECT_TRUE(document.find(f.cube)->visible,"visível de volta");

  auto layers=document.layers();
  AE_EXPECT_TRUE(layers.setName(3,"Jogador"),"camada nomeada no projeto");
  document.setLayers(layers);
  f.session.update();
  revealProperty(f,widgetId(EditorWidget::ObjectLayerNext));
  tapWidget(f,widgetId(EditorWidget::ObjectLayerNext));
  AE_EXPECT_EQ(document.find(f.cube)->layer,3u,"próxima camada nomeada, sem passar por camadas não declaradas");
  tapWidget(f,widgetId(EditorWidget::ObjectFold));

  auto moved=*document.find(f.cube);moved.transform.position[0]=4;moved.transform.scale[1]=2;
  AE_EXPECT_TRUE(document.applyEntityValues(f.cube,moved),"pose de partida");f.session.update();
  tapWidget(f,widgetId(EditorWidget::TransformMenu));
  tapWidget(f,widgetId(EditorWidget::TransformCopy));
  AE_EXPECT_TRUE(f.session.screen().hasTransformClipboard,"transformação copiada");
  tapWidget(f,widgetId(EditorWidget::TransformMenu));
  tapWidget(f,widgetId(EditorWidget::TransformResetPosition));
  AE_EXPECT_EQ(document.find(f.cube)->transform.position[0],0.0f,"posição redefinida");
  AE_EXPECT_EQ(document.find(f.cube)->transform.scale[1],2.0f,"a escala não foi tocada");
  tapWidget(f,widgetId(EditorWidget::TransformMenu));
  tapWidget(f,widgetId(EditorWidget::TransformPaste));
  AE_EXPECT_EQ(document.find(f.cube)->transform.position[0],4.0f,"transformação colada");

  const auto before=document.entityCount();
  tapWidget(f,widgetId(EditorWidget::InspectorMenu));
  AE_EXPECT_TRUE(f.session.screen().inspectorMenu,"o ⋮ do cabeçalho abre as ações do objeto");
  tapWidget(f,widgetId(EditorWidget::DuplicateSelection));
  AE_EXPECT_EQ(document.entityCount(),before+1,"duplicado pelo inspetor");
  AE_EXPECT_TRUE(!f.session.screen().inspectorMenu,"o menu fecha depois da ação");
  const auto copy=f.session.screen().selection;
  tapWidget(f,widgetId(EditorWidget::InspectorMenu));
  tapWidget(f,widgetId(EditorWidget::CreateChildGroup));
  AE_EXPECT_EQ(document.find(f.session.screen().selection)->parent,copy,"filho vazio criado sob o objeto selecionado");
}

AE_TEST(p01_composition_reuses_dependencies_and_undoes_the_whole_add) {
  Fixture f;auto &d=f.session.document();
  const auto id=f.session.history().createEntity(d,d.root(),EditorEntityKind::Folder,"Optics");
  f.session.history().clear();
  EditorActionRequest add;add.action=EditorAction::AddComponent;add.entity=id;
  add.componentType=scene::CameraLook::descriptor.id;add.version=f.session.sceneVersion();
  AE_EXPECT_TRUE(f.session.dispatch(add).status==EditorActionStatus::Applied,"add resolves camera and look");
  AE_EXPECT_EQ(d.find(id)->components.size(),2u,"both required components published");
  AE_EXPECT_EQ(f.session.history().undoDepth(),1u,"one authoring transaction");
  AE_EXPECT_TRUE(f.session.history().undo(d),"undo composition");
  AE_EXPECT_EQ(d.find(id)->components.size(),0u,"no partial dependency left");
  AE_EXPECT_TRUE(f.session.history().redo(d),"redo composition");
  const auto cameraId=cameraComponent(*d.find(id))->instanceId();
  EditorActionRequest remove;remove.action=EditorAction::RemoveComponent;remove.entity=id;
  remove.componentInstance=cameraId;remove.version=f.session.sceneVersion();
  AE_EXPECT_TRUE(f.session.dispatch(remove).status==EditorActionStatus::InvalidValue,"used camera cannot be removed");
  auto value=*d.find(id);editCamera(value)->verticalFov=43;
  value.components.removeInstance(cameraLook(value)->instanceId());d.applyEntityValues(id,value);
  add.version=f.session.sceneVersion();
  AE_EXPECT_TRUE(f.session.dispatch(add).status==EditorActionStatus::Applied,"reuse existing camera");
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->instanceId(),cameraId,"identity preserved");
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->verticalFov,43.f,"lens not reset by dependency resolution");
}

AE_TEST(session_component_add_previews_dependencies_before_one_undo) {
  Fixture f;auto &doc=f.session.document();auto &history=f.session.history();
  const auto target=history.createEntity(doc,doc.root(),EditorEntityKind::Folder,"Óptica");
  f.session.setSelection(target);f.session.update();history.clear();
  tapWidget(f,widgetId(EditorWidget::AddComponentMenu));
  selectComponentFamily(f,scene::ComponentFamily::Camera);
  revealAddEntry(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look"));
  tapWidget(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look"));
  AE_EXPECT_EQ(f.session.screen().componentPreview,editorComponentIndex("astra.camera.look")+1,"Olhar abre plano de composição");
  AE_EXPECT_EQ(doc.find(target)->components.size(),0u,"prévia não altera o documento");
  AE_EXPECT_EQ(history.undoDepth(),0u,"prévia não cria Undo");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ComponentPreviewConfirm)).x>=0,"confirmação acessível por toque");
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewBack));
  AE_EXPECT_EQ(f.session.screen().componentPreview,0u,"voltar conserva a busca");
  AE_EXPECT_EQ(doc.find(target)->components.size(),0u,"cancelamento sem efeito");
  revealAddEntry(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look"));
  tapWidget(f,widgetId(EditorWidget::ComponentAddBase)+editorComponentIndex("astra.camera.look"));
  tapWidget(f,widgetId(EditorWidget::ComponentPreviewConfirm));
  AE_EXPECT_TRUE(cameraComponent(*doc.find(target)) && cameraLook(*doc.find(target)),"Câmera e Olhar publicados juntos");
  AE_EXPECT_EQ(history.undoDepth(),1u,"composição inteira em um Undo");
  AE_EXPECT_TRUE(history.undo(doc),"desfaz composição");
  AE_EXPECT_EQ(doc.find(target)->components.size(),0u,"desfaz requisito e componente solicitado");
}

AE_TEST(p01_removal_protects_a_typed_reference_on_another_object) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto a=h.createEntity(d,d.root(),EditorEntityKind::Folder,"A");
  const auto b=h.createEntity(d,d.root(),EditorEntityKind::Folder,"B");
  auto target=*d.find(a);auto *body=target.components.add(scene::PhysicsBody::descriptor);const auto bodyId=body->instanceId();
  d.applyEntityValues(a,target);
  auto source=*d.find(b);source.components.add(scene::PhysicsBody::descriptor);
  auto *joint=static_cast<scene::Joint*>(source.components.add(scene::Joint::descriptor));joint->connectedBody=a;
  d.applyEntityValues(b,source);
  EditorActionRequest remove;remove.action=EditorAction::RemoveComponent;remove.entity=a;remove.componentInstance=bodyId;remove.version=f.session.sceneVersion();
  AE_EXPECT_TRUE(f.session.dispatch(remove).status==EditorActionStatus::InvalidValue,"cross-object joint protects target body");
  AE_EXPECT_TRUE(physicsBody(*d.find(a)),"body preserved");
  f.session.setSelection(a);f.session.update();
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ComponentRemoveBase));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x<0,"typed incoming reference blocks Inspector removal");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_TRUE(d.find(a)->components.findInstance(bodyId)!=nullptr,"cancel preserves referenced instance");
}

AE_TEST(p03_camera_inspection_pilot_undo_and_cancel_preserve_editor_orbit) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);value.components.add(scene::Camera::descriptor);
  value.transform.position[2]=-5;value.transform.rotationDegrees[2]=25;d.applyEntityValues(id,value);
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  const auto orbit=f.session.camera();const auto initial=d.find(id)->transform;
  h.clear();tapWidget(f,widgetId(EditorWidget::CameraView));
  AE_EXPECT_TRUE(std::abs(f.session.view().frustum.roll)>.4f,"authored roll reaches viewport");
  const auto at=f.viewportCentre();const UiPoint to{at.x+30,at.y+15};
  f.down(1,at);f.move(1,to);f.up(1,to);
  AE_EXPECT_EQ(h.undoDepth(),0u,"inspection does not author a pose");
  tapWidget(f,widgetId(EditorWidget::CameraPilot));
  f.down(2,at);f.move(2,to);f.move(2,{to.x+10,to.y});f.up(2,to);f.session.update();
  AE_EXPECT_EQ(h.undoDepth(),1u,"pilot drag is one transaction");
  AE_EXPECT_TRUE(std::abs(d.find(id)->transform.rotationDegrees[1]-initial.rotationDegrees[1])>1.f,"pilot changes authored camera");
  AE_EXPECT_EQ(f.session.camera().yaw,orbit.yaw,"editor orbit kept separate");
  AE_EXPECT_TRUE(h.undo(d),"undo pilot");f.session.update();
  AE_EXPECT_TRUE(std::abs(d.find(id)->transform.rotationDegrees[2]-25)<.01f,"roll restored");
  f.down(3,at);f.move(3,to);f.session.handlePointer({3,UiPointerPhase::Cancel,to,0});
  AE_EXPECT_EQ(h.undoDepth(),0u,"cancel does not create undo entry");
  AE_EXPECT_EQ(h.redoDepth(),1u,"cancel preserves redo future");
  AE_EXPECT_TRUE(std::abs(d.find(id)->transform.rotationDegrees[1]-initial.rotationDegrees[1])<.01f,"cancel restores pose");
  tapWidget(f,widgetId(EditorWidget::CameraViewClose));
  AE_EXPECT_EQ(f.session.screen().cameraViewEntity,0u,"camera view closed");
  AE_EXPECT_EQ(f.session.camera().distance,orbit.distance,"orbit zoom restored");
}

AE_TEST(p03_orthographic_camera_migration_archive_zoom_and_play_pose) {
  scene::Camera legacy;std::istringstream old("1 43 0.25 700 3");
  AE_EXPECT_TRUE(legacy.read(old,1)&&legacy.valid(),"v1 camera remains readable");
  AE_EXPECT_TRUE(legacy.projection==scene::CameraProjection::Perspective,"legacy remains perspective");
  AE_EXPECT_EQ(legacy.verticalFov,43.f,"old lens preserved");
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Ortho");
  auto value=*d.find(id);auto *lens=static_cast<scene::Camera*>(value.components.add(scene::Camera::descriptor));
  lens->projection=scene::CameraProjection::Orthographic;lens->orthographicHalfHeight=4;const auto instance=lens->instanceId();
  value.transform.position[2]=-5;value.transform.rotationDegrees[0]=31.123f;value.transform.rotationDegrees[2]=17.432f;d.applyEntityValues(id,value);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(d,0),0,restored),"v2 archive roundtrip");
  AE_EXPECT_EQ(cameraComponent(*restored.find(id))->instanceId(),instance,"instance identity preserved");
  AE_EXPECT_TRUE(resolveSceneCamera(restored).projection==scene::CameraProjection::Orthographic,"runtime resolves authored projection");
  // A populated project console used to share IDs with component enums and
  // consume their taps even while the console panel was closed.
  const_cast<EditorScreenState&>(f.session.screen()).console=&f.session.console();
  f.session.reportProblem(EditorConsoleSeverity::Info,"Project loaded");
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::ComponentEnumBase));
  tapWidget(f,widgetId(EditorWidget::ComponentEnumOptionBase)+0);
  AE_EXPECT_TRUE(cameraComponent(*d.find(id))->projection==scene::CameraProjection::Perspective,"inspector switches projection");
  tapWidget(f,widgetId(EditorWidget::ComponentEnumBase));
  tapWidget(f,widgetId(EditorWidget::ComponentEnumOptionBase)+1);
  AE_EXPECT_TRUE(cameraComponent(*d.find(id))->projection==scene::CameraProjection::Orthographic,"inspector switches back");
  tapWidget(f,widgetId(EditorWidget::CameraPilot));
  AE_EXPECT_TRUE(renderer::isOrthographic(f.session.view().frustum),"lens reaches editor view");
  auto &state=const_cast<EditorScreenState&>(f.session.screen());state.navigation=EditorNavigationMode::Zoom;
  h.clear();const auto at=f.viewportCentre();const UiPoint to{at.x,at.y-35};
  f.down(1,at);f.move(1,to);f.up(1,to);f.session.update();
  AE_EXPECT_TRUE(cameraComponent(*d.find(id))->orthographicHalfHeight<4,"zoom changes extent");
  AE_EXPECT_EQ(d.find(id)->transform.position[2],-5.f,"zoom does not dolly");
  AE_EXPECT_EQ(d.find(id)->transform.rotationDegrees[0],31.123f,"lens-only zoom does not round-trip rotation");
  AE_EXPECT_EQ(d.find(id)->transform.rotationDegrees[2],17.432f,"lens-only zoom preserves exact roll");
  AE_EXPECT_EQ(h.undoDepth(),1u,"one gesture one history step");
  AE_EXPECT_TRUE(h.undo(d),"undo lens zoom");
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->orthographicHalfHeight,4.f,"extent restored");
  AE_EXPECT_TRUE(h.redo(d),"redo lens zoom");
}

AE_TEST(p03_camera_view_lens_controls_target_viewed_camera_and_validate_clip) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);value.components.add(scene::Camera::descriptor);d.applyEntityValues(id,value);
  const auto other=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Other");
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  tapWidget(f,widgetId(EditorWidget::CameraView));f.session.setSelection(other);f.session.update();h.clear();
  tapWidget(f,widgetId(EditorWidget::CameraLens));auto edit=f.session.pendingTextEdit();
  AE_EXPECT_EQ(edit.entity,id,"lens targets viewed camera, not selection");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"75",true),"edit lens");f.session.update();
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->verticalFov,75.f,"authored FOV updated");
  AE_EXPECT_EQ(f.session.screen().selection,other,"selection preserved");
  AE_EXPECT_TRUE(h.undo(d),"lens undo");f.session.update();
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->verticalFov,60.f,"lens restored");
  tapWidget(f,widgetId(EditorWidget::CameraFar));edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"0.01",true),"far before near rejected");
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->farPlane,2000.f,"invalid clip leaves scene intact");
  f.session.completeTextEdit(edit,"",false);
  auto changed=*d.find(id);auto *lens=editCamera(changed);lens->projection=scene::CameraProjection::Orthographic;
  d.applyEntityValues(id,changed);f.session.update();
  tapWidget(f,widgetId(EditorWidget::CameraLens));edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"8",true),"edit orthographic extent");
  AE_EXPECT_EQ(cameraComponent(*d.find(id))->orthographicHalfHeight,8.f,"extent updated");
}

AE_TEST(p03_camera_world_handles_drag_undo_cancel_and_clip_limits) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);auto *lens=editCamera(value);lens->nearPlane=1;lens->farPlane=8;
  d.applyEntityValues(id,value);f.session.setSelection(id);
  const float eye[]{6,4,-10};f.session.setCameraPose(eye,-.4f,.2f);f.session.update();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));h.clear();
  for(u32 kind=0;kind<3;++kind) {
    const auto at=locateWidget(f.session,widgetId(EditorWidget::CameraHandleBase)+kind);
    AE_EXPECT_TRUE(at.x>=0,"world handle reachable");
    f.down(1,at);f.move(1,{at.x+25,at.y-20});f.up(1,{at.x+25,at.y-20});f.session.update();
    AE_EXPECT_EQ(h.undoDepth(),1u,"entire handle drag is one command");
    AE_EXPECT_TRUE(h.undo(d),"undo handle");f.session.update();
    AE_EXPECT_EQ(cameraComponent(*d.find(id))->verticalFov,60.f,"FOV restored");
    AE_EXPECT_EQ(cameraComponent(*d.find(id))->nearPlane,1.f,"near restored");
    AE_EXPECT_EQ(cameraComponent(*d.find(id))->farPlane,8.f,"far restored");
    f.down(2,at);f.move(2,{at.x+25,at.y-20});
    f.session.handlePointer({2,UiPointerPhase::Cancel,{at.x+25,at.y-20},0});f.session.update();
    AE_EXPECT_EQ(h.undoDepth(),0u,"cancel restores history");
    AE_EXPECT_EQ(h.redoDepth(),1u,"cancel preserves redo");h.clear();
  }
  for(auto mode:{scene::CameraProjection::Perspective,scene::CameraProjection::Orthographic}) {
    scene::Camera camera;camera.projection=mode;
    AE_EXPECT_TRUE(applyCameraHandleDelta(camera,0,5,2),"lens manipulation valid for both projections");
    AE_EXPECT_TRUE(mode==scene::CameraProjection::Perspective?camera.verticalFov>60:camera.orthographicHalfHeight==7,"lens changes in physical direction");
    AE_EXPECT_TRUE(applyCameraHandleDelta(camera,1,5,100000),"near clamps below far");
    AE_EXPECT_TRUE(applyCameraHandleDelta(camera,2,5,-100000),"far clamps above near");
    AE_EXPECT_TRUE(camera.valid(),"ordered finite planes");
  }
}

AE_TEST(component_volume_handles_use_effective_pose_schema_and_single_undo) {
  Fixture f;auto &document=f.session.document();auto &history=f.session.history();
  const auto lightId=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Spot");
  auto lightEntity=*document.find(lightId);
  auto *light=static_cast<scene::Light*>(lightEntity.components.add(scene::Light::descriptor));
  light->kind=scene::LightKind::Spot;light->range=4;
  const auto lightInstance=light->instanceId();
  lightEntity.transform.scale[0]=2;
  AE_EXPECT_TRUE(document.applyEntityValues(lightId,lightEntity),"spot autorado");
  EditorComponentHandle rangeHandle,innerHandle,outerHandle;
  AE_EXPECT_TRUE(componentHandleGeometry(document,lightId,lightInstance,EditorComponentHandleKind::LightRange,rangeHandle) &&
                 componentHandleGeometry(document,lightId,lightInstance,EditorComponentHandleKind::LightInnerAngle,innerHandle) &&
                 componentHandleGeometry(document,lightId,lightInstance,EditorComponentHandleKind::LightOuterAngle,outerHandle),
                 "alcance e dois cones possuem alças");
  AE_EXPECT_TRUE(std::fabs(rangeHandle.worldUnitsPerProperty-1)<1e-4f,
                 "alcance óptico ignora escala do objeto");
  auto changed=*document.find(lightId);
  AE_EXPECT_TRUE(applyComponentHandleDelta(changed,outerHandle,-100),"cone externo limitado pelo interno");
  AE_EXPECT_EQ(static_cast<const scene::Light*>(changed.components.findInstance(lightInstance))->outerAngle,20.f,
               "cone externo não cruza o interno");

  const auto volumeId=history.createEntity(document,document.root(),EditorEntityKind::Folder,"Volume");
  auto volumeEntity=*document.find(volumeId);
  auto *volume=static_cast<scene::Environment*>(volumeEntity.components.add(scene::Environment::descriptor));
  volume->shape=renderer::EnvironmentVolumeShape::Box;volume->boxSize[0]=4;volume->blendDistance=2;
  const auto volumeInstance=volume->instanceId();
  volumeEntity.transform.scale[0]=2;
  AE_EXPECT_TRUE(document.applyEntityValues(volumeId,volumeEntity),"volume autorado");
  EditorComponentHandle sizeHandle,blendHandle;
  AE_EXPECT_TRUE(componentHandleGeometry(document,volumeId,volumeInstance,EditorComponentHandleKind::BoxX,sizeHandle) &&
                 componentHandleGeometry(document,volumeId,volumeInstance,EditorComponentHandleKind::BlendDistance,blendHandle),
                 "tamanho e mistura possuem alças");
  AE_EXPECT_TRUE(std::fabs(sizeHandle.point[0]-4)<1e-4f &&
                 std::fabs(sizeHandle.worldUnitsPerProperty-1)<1e-4f &&
                 std::fabs(blendHandle.point[0]+6)<1e-4f &&
                 std::fabs(blendHandle.worldUnitsPerProperty-1)<1e-4f,
                 "tamanho local usa escala mundial; mistura permanece em metros mundiais");
  changed=*document.find(volumeId);
  AE_EXPECT_TRUE(applyComponentHandleDelta(changed,sizeHandle,2),"dimensão aumenta por arraste");
  AE_EXPECT_EQ(static_cast<const scene::Environment*>(changed.components.findInstance(volumeInstance))->boxSize[0],6.f,
               "dimensão chega ao valor autoral");
  AE_EXPECT_TRUE(applyComponentHandleDelta(changed,blendHandle,3),"mistura aumenta por arraste");
  AE_EXPECT_EQ(static_cast<const scene::Environment*>(changed.components.findInstance(volumeInstance))->blendDistance,5.f,
               "distância de mistura em metros mundiais");

  f.session.setSelection(lightId);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  history.clear();f.session.update();
  const auto at=locateWidget(f.session,widgetId(EditorWidget::ComponentHandleBase)+
      static_cast<u32>(EditorComponentHandleKind::LightRange));
  AE_EXPECT_TRUE(at.x>=0,"alça de luz alcançável na cena");
  f.down(1,at);f.move(1,{at.x+30,at.y-15});f.up(1,{at.x+30,at.y-15});f.session.update();
  AE_EXPECT_EQ(history.undoDepth(),1u,"arraste da luz é uma operação autoral");
  AE_EXPECT_TRUE(history.undo(document),"alcance desfazível");
  AE_EXPECT_EQ(static_cast<const scene::Light*>(document.find(lightId)->components.findInstance(lightInstance))->range,4.f,
               "desfazer restaura alcance");
  history.clear();f.session.setSelection(volumeId);f.session.update();
  tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));f.session.update();
  const auto boxAt=locateWidget(f.session,widgetId(EditorWidget::ComponentHandleBase)+
      static_cast<u32>(EditorComponentHandleKind::BoxX));
  AE_EXPECT_TRUE(boxAt.x>=0,"alça da caixa alcançável na cena");
  f.down(2,boxAt);f.move(2,{boxAt.x+25,boxAt.y-20});f.up(2,{boxAt.x+25,boxAt.y-20});f.session.update();
  AE_EXPECT_EQ(history.undoDepth(),1u,"arraste do volume é uma operação autoral");
  AE_EXPECT_TRUE(history.undo(document),"tamanho desfazível");
  AE_EXPECT_EQ(static_cast<const scene::Environment*>(document.find(volumeId)->components.findInstance(volumeInstance))->boxSize[0],4.f,
               "desfazer restaura tamanho");
}

AE_TEST(p03_preview_view_budget_pin_schedule_and_stale_publication) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);auto *camera=editCamera(value);
  camera->projection=scene::CameraProjection::Orthographic;camera->orthographicHalfHeight=3;
  value.transform.rotationDegrees[2]=20;d.applyEntityValues(id,value);
  const auto orbit=f.session.camera();auto &preview=f.session.cameraPreview();
  AE_EXPECT_TRUE(!preview.pin(d,f.session.sceneVersion(),0),"must pin explicit camera");
  AE_EXPECT_TRUE(preview.pin(d,f.session.sceneVersion(),id),"pin camera");
  renderer::PreviewViewBudget budget;budget.maximumWidth=320;budget.maximumHeight=320;budget.maximumPixels=320*180;budget.updatesPerSecond=10;
  AE_EXPECT_TRUE(preview.configure(1920,1080,budget),"bounded aspect preserving extent");
  renderer::RenderViewSnapshot first,next;
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),0,false,false,first),"hidden preview idle");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),0,true,false,first),"first visible frame");
  AE_EXPECT_EQ(first.width,320u,"width capped");AE_EXPECT_EQ(first.height,180u,"aspect retained");
  AE_EXPECT_TRUE(renderer::isOrthographic(first.frustum),"independent projection");
  AE_EXPECT_TRUE(std::abs(first.frustum.roll)>.3f,"full orientation retained");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),1,true,false,next),"single outstanding request");
  value=*d.find(id);value.transform.position[0]=2;d.applyEntityValues(id,value);
  AE_EXPECT_TRUE(!preview.complete(first,f.session.sceneVersion(),true),"outdated image refused");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),1,true,false,next),"new revision scheduled");
  AE_EXPECT_TRUE(!preview.complete(first,f.session.sceneVersion(),true),"old completion cannot consume new lease");
  AE_EXPECT_TRUE(preview.complete(next,f.session.sceneVersion(),true),"matching publication accepted");
  AE_EXPECT_TRUE(preview.hasCurrentImage(f.session.sceneVersion()),"published revision current");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),2,true,false,first),"static scene costs no redraw");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),2,true,true,first),"animation refresh");
  AE_EXPECT_TRUE(preview.complete(first,f.session.sceneVersion(),true),"animation published");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),2.05,true,true,next),"rate capped");
  preview.invalidateTarget();AE_EXPECT_TRUE(!preview.hasCurrentImage(f.session.sceneVersion()),"surface loss invalidates image");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),2.06,true,false,next),"surface recovery redraws");
  AE_EXPECT_TRUE(!preview.complete(next,f.session.sceneVersion(),false),"backend failure cannot publish");
  AE_EXPECT_TRUE(preview.failed(),"failure is visible to UI");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),10,true,false,first),"failed backend does not retry forever");
  preview.invalidateTarget();
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),11,true,false,next),"explicit retry acquires new lease");
  budget.maximumWidth=640;budget.maximumHeight=360;budget.maximumPixels=640*360;
  AE_EXPECT_TRUE(preview.configure(640,360,budget),"resize invalidates pending lease");
  AE_EXPECT_TRUE(!preview.complete(next,f.session.sceneVersion(),true),"old extent cannot publish");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),12,true,false,next),"resized image scheduled");
  AE_EXPECT_EQ(next.width,640u,"new target width");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),18,true,false,first),"lost completion times out");
  AE_EXPECT_TRUE(preview.failed(),"timeout is actionable");
  AE_EXPECT_TRUE(!preview.complete(next,f.session.sceneVersion(),true),"late completion cannot publish after timeout");
  AE_EXPECT_TRUE(!preview.request(d,f.session.sceneVersion(),19,false,false,first),"hidden preview suspends");
  AE_EXPECT_TRUE(!preview.failed(),"suspension resets failure for recovery");
  AE_EXPECT_TRUE(preview.request(d,f.session.sceneVersion(),20,true,false,next),"returning view reacquires target");
  preview.close();AE_EXPECT_TRUE(!preview.complete(next,f.session.sceneVersion(),true),"closed preview rejects in-flight work");
  AE_EXPECT_EQ(f.session.camera().yaw,orbit.yaw,"main camera unchanged");
  AE_EXPECT_EQ(f.session.camera().distance,orbit.distance,"main zoom unchanged");
}

AE_TEST(p03_preview_pin_ui_and_close_preserve_selection_and_orbit) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Preview camera");
  auto value=*d.find(id);editCamera(value);d.applyEntityValues(id,value);
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));
  const auto orbit=f.session.camera();tapWidget(f,widgetId(EditorWidget::CameraPreviewPin));
  AE_EXPECT_EQ(f.session.cameraPreview().camera(),id,"preview pins selected camera");
  const auto other=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Other");
  f.session.setSelection(other);f.session.update();
  AE_EXPECT_EQ(f.session.screen().cameraPreviewEntity,id,"selection does not switch preview");
  renderer::RenderViewSnapshot request;
  AE_EXPECT_TRUE(f.session.cameraPreview().request(d,f.session.sceneVersion(),10,true,false,request),"wall clock schedules independent view");
  AE_EXPECT_TRUE(f.session.cameraPreview().complete(request,f.session.sceneVersion(),true),"simulated backend completion");f.session.update();
  AE_EXPECT_TRUE(f.session.screen().cameraPreviewReady,"image only exposed after completion");
  tapWidget(f,widgetId(EditorWidget::CameraPreviewClose));
  AE_EXPECT_EQ(f.session.cameraPreview().camera(),0u,"close releases pin");
  AE_EXPECT_EQ(f.session.screen().selection,other,"selection preserved");
  AE_EXPECT_EQ(f.session.camera().yaw,orbit.yaw,"orbit preserved");
}

AE_TEST(p01_composite_properties_are_atomic_and_keep_archive_contract) {
  scene::Components values;
  auto *joint=static_cast<scene::Joint*>(values.add(scene::Joint::descriptor));
  joint->kind=scene::JointKind::Hinge;joint->limitMin=-90;joint->limitMax=90;
  const auto id=joint->instanceId();
  const float direction[3]{1,0,0},invalid[3]{0,0,0};
  AE_EXPECT_TRUE(scene::setComponentTriple(values,"astra.physics.joint","axis_a",direction,id)==scene::ComponentPropertyStatus::Applied,"whole direction accepted");
  AE_EXPECT_TRUE(scene::setComponentTriple(values,"astra.physics.joint","axis_a",invalid,id)==scene::ComponentPropertyStatus::InvalidValue,"zero direction rejected");
  const auto *result=static_cast<const scene::Joint*>(values.findInstance(id));
  AE_EXPECT_EQ(result->axisA[0],1.f,"rejection leaves previous direction intact");
  std::stringstream archive;result->write(archive);scene::Joint loaded;
  AE_EXPECT_TRUE(loaded.read(archive,1),"existing archive version reads composite edit");
  AE_EXPECT_EQ(loaded.axisA[0],1.f,"archive keeps edited value");
  values.add(scene::Joint::descriptor);
  AE_EXPECT_TRUE(scene::setComponentTriple(values,"astra.physics.joint","axis_a",direction)==scene::ComponentPropertyStatus::AmbiguousProperty,"multiple instances require identity");
  auto *light=values.add(scene::Light::descriptor);const auto lightId=light->instanceId();
  const float color[3]{.25f,.5f,.75f},badColor[3]{.8f,2.f,.4f};
  AE_EXPECT_TRUE(scene::setComponentTriple(values,"astra.render.light","color",color,lightId)==scene::ComponentPropertyStatus::Applied,"linear color accepted");
  AE_EXPECT_TRUE(scene::setComponentTriple(values,"astra.render.light","color",badColor,lightId)==scene::ComponentPropertyStatus::InvalidValue,"existing color bounds retained");
  AE_EXPECT_EQ(static_cast<const scene::Light*>(values.findInstance(lightId))->color[0],.25f,"invalid channel does not partially change color");
}

AE_TEST(p01_triple_editor_single_undo_invalid_input_and_cancel) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Light");
  auto value=*d.find(id);value.components.add(scene::Light::descriptor);d.applyEntityValues(id,value);
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));f.session.update();h.clear();
  revealProperty(f,widgetId(EditorWidget::ComponentTripleBase));
  tapWidget(f,widgetId(EditorWidget::ComponentTripleBase));auto edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(edit.propertyType=="triple","composite editor request");
  AE_EXPECT_TRUE(!f.session.completeTextEdit(edit,"0.2 2 0.4",true),"invalid channel rejects entire tuple");
  AE_EXPECT_EQ(h.undoDepth(),0u,"rejection adds no history");
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"0,2; 0,3; 0,4",true),"localized tuple accepted");
  AE_EXPECT_EQ(h.undoDepth(),1u,"one operation for all channels");
  const auto read=[&](){return static_cast<const scene::Light*>(d.find(id)->components.find(scene::Light::descriptor));};
  AE_EXPECT_EQ(read()->color[0],.2f,"red applied");AE_EXPECT_EQ(read()->color[2],.4f,"blue applied");
  AE_EXPECT_TRUE(h.undo(d),"undo composite");AE_EXPECT_EQ(read()->color[0],1.f,"red restored");AE_EXPECT_EQ(read()->color[2],1.f,"blue restored");
  f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentTripleBase));edit=f.session.pendingTextEdit();
  AE_EXPECT_TRUE(f.session.completeTextEdit(edit,"0 0 0",false),"cancel editor");AE_EXPECT_EQ(read()->color[1],1.f,"cancel preserves green");
}

AE_TEST(p01_color_picker_stages_and_commits_one_history_entry) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Light");
  auto entity=*d.find(id);entity.components.add(scene::Light::descriptor);d.applyEntityValues(id,entity);
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentFoldBase));f.session.update();h.clear();
  revealProperty(f,widgetId(EditorWidget::ComponentColorBase));
  tapWidget(f,widgetId(EditorWidget::ComponentColorBase));
  AE_EXPECT_TRUE(f.session.screen().colorField!=0,"swatch opens picker");
  tapWidget(f,widgetId(EditorWidget::ColorHueBase)+8);tapWidget(f,widgetId(EditorWidget::ColorSvBase)+10);
  const auto read=[&](){return static_cast<const scene::Light*>(d.find(id)->components.find(scene::Light::descriptor));};
  AE_EXPECT_EQ(read()->color[0],1.f,"draft leaves authored light unchanged");
  tapWidget(f,widgetId(EditorWidget::ColorApply));AE_EXPECT_EQ(h.undoDepth(),1u,"single commit");
  AE_EXPECT_EQ(read()->color[0],0.f,"green selection removes red");AE_EXPECT_EQ(read()->color[1],1.f,"green channel");
  AE_EXPECT_TRUE(h.undo(d),"undo color");AE_EXPECT_EQ(read()->color[0],1.f,"white restored");
  f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentColorBase));tapWidget(f,widgetId(EditorWidget::ColorCancel));
  AE_EXPECT_EQ(read()->color[2],1.f,"cancel preserves blue");
}

AE_TEST(p02_impact_lists_requirements_and_navigates_to_dependency) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);
  value.components.add(scene::Camera::descriptor);value.components.add(scene::CameraLook::descriptor);d.applyEntityValues(id,value);
  const auto *camera=d.find(id)->components.find(scene::Camera::descriptor);
  const auto rows=componentImpact(d,id,camera->instanceId());
  AE_EXPECT_TRUE(!rows.empty(),"camera reports look requirement");
  AE_EXPECT_TRUE(rows[0].blocksRemoval,"required camera reports removal blocker");
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ImpactOpenBase));
  AE_EXPECT_EQ(f.session.screen().impactInstance,camera->instanceId(),"panel pins component identity");
  tapWidget(f,widgetId(EditorWidget::ImpactRowBase));
  AE_EXPECT_EQ(f.session.screen().expandedNative,rows[0].instance,"navigate dependent component");
}

AE_TEST(p02_removal_preview_blocks_dependents_and_cancel_keeps_scene) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);
  value.components.add(scene::Camera::descriptor);value.components.add(scene::CameraLook::descriptor);
  AE_EXPECT_TRUE(d.applyEntityValues(id,value),"camera and look authored");
  f.session.setSelection(id);f.session.update();h.clear();
  const auto revision=d.revision();
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ComponentRemoveBase));
  AE_EXPECT_EQ(f.session.screen().impactEntity,id,"preview pins object");
  AE_EXPECT_TRUE(f.session.screen().impactRemoval,"removal mode active");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x<0,"dependent blocks confirmation");
  AE_EXPECT_EQ(d.revision(),revision,"opening preview never changes scene");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_TRUE(cameraComponent(*d.find(id))&&cameraLook(*d.find(id)),"cancel leaves both components");
  AE_EXPECT_EQ(h.undoDepth(),0u,"cancel creates no Undo entry");
}

AE_TEST(p02_removal_preview_revalidates_revision_before_commit) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera");
  auto value=*d.find(id);auto *camera=value.components.add(scene::Camera::descriptor);
  const auto instance=camera->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(id,value),"camera authored");
  f.session.setSelection(id);f.session.update();h.clear();
  tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ComponentRemoveBase));
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x>=0,"free camera is removable");
  auto changed=*d.find(id);std::snprintf(changed.name,sizeof(changed.name),"Changed");
  AE_EXPECT_TRUE(d.applyEntityValues(id,changed),"scene changed after preview");
  AE_EXPECT_TRUE(locateWidget(f.session,widgetId(EditorWidget::ImpactRemoveConfirm)).x<0,"stale preview cannot confirm");
  AE_EXPECT_TRUE(d.find(id)->components.findInstance(instance)!=nullptr,"camera remains after stale preview");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_EQ(h.undoDepth(),0u,"stale preview adds no history");
}

AE_TEST(p02_impact_typed_cross_object_reference_resolves_destination) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto body=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Body");
  auto target=*d.find(body);auto *b=target.components.add(scene::PhysicsBody::descriptor);const auto bodyInstance=b->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(body,target),"body authored");
  const auto linked=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Joint");
  auto source=*d.find(linked);source.components.add(scene::PhysicsBody::descriptor);
  auto *joint=static_cast<scene::Joint*>(source.components.add(scene::Joint::descriptor));joint->connectedBody=body;const auto jointInstance=joint->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(linked,source),"joint authored");
  const auto outgoing=componentImpact(d,linked,jointInstance);bool found=false;
  for(const auto &row:outgoing) if(row.object==body) {found=true;AE_EXPECT_EQ(row.instance,bodyInstance,"typed reference navigates to body");}
  AE_EXPECT_TRUE(found,"outgoing relation exists");
  const auto incoming=componentImpact(d,body,bodyInstance);found=false;
  for(const auto &row:incoming) if(row.object==linked&&row.instance==jointInstance) {found=true;AE_EXPECT_TRUE(row.blocksRemoval,"typed reference blocks body removal");}
  AE_EXPECT_TRUE(found,"reverse relationship retains source instance");
}

AE_TEST(p02_joint_execution_conditions_match_impact_and_activation) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto a=h.createEntity(d,d.root(),EditorEntityKind::Folder,"A");
  const auto b=h.createEntity(d,d.root(),EditorEntityKind::Folder,"B");
  auto first=*d.find(a),second=*d.find(b);
  auto *bodyA=static_cast<scene::PhysicsBody*>(first.components.add(scene::PhysicsBody::descriptor));
  auto *bodyB=static_cast<scene::PhysicsBody*>(second.components.add(scene::PhysicsBody::descriptor));
  bodyA->motion=scene::BodyMotion::Static;bodyB->motion=scene::BodyMotion::Kinematic;
  auto *joint=static_cast<scene::Joint*>(first.components.add(scene::Joint::descriptor));joint->connectedBody=b;
  const auto instance=joint->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(a,first)&&d.applyEntityValues(b,second),"draft pair authored");
  auto issues=runtime::jointRequirementIssues(d,a,*joint);
  AE_EXPECT_EQ(issues.size(),usize(1),"static kinematic pair has one authored prerequisite failure");
  AE_EXPECT_TRUE(issues[0].code=="joint.dynamic_body","stable diagnostic identity");
  bool visible=false;for(const auto &row:componentImpact(d,a,instance)) if(row.relation=="Condição de execução") {
    visible=true;AE_EXPECT_TRUE(row.invalid,"execution issue marked invalid");
    AE_EXPECT_EQ(row.instance,bodyA->instanceId(),"repair navigation targets source body");
  }
  AE_EXPECT_TRUE(visible,"impact exposes runtime prerequisite");
  bodyB->motion=scene::BodyMotion::Dynamic;AE_EXPECT_TRUE(d.applyEntityValues(b,second),"dynamic target authored");
  AE_EXPECT_TRUE(runtime::jointRequirementIssues(d,a,*joint).empty(),"one dynamic body satisfies condition");
  bodyB->motion=scene::BodyMotion::Static;d.applyEntityValues(b,second);
  joint->enabled=false;
  AE_EXPECT_TRUE(runtime::jointRequirementIssues(d,a,*joint).empty(),"disabled joint does not execute");
  joint->enabled=true;first.active=false;d.applyEntityValues(a,first);
  AE_EXPECT_TRUE(runtime::jointRequirementIssues(d,a,*joint).empty(),"inactive object does not execute");
}

AE_TEST(p02_conditional_reference_readiness_preserves_drafts) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Joint draft");
  auto authored=*d.find(id);authored.components.add(scene::PhysicsBody::descriptor);
  auto *joint=static_cast<scene::Joint*>(authored.components.add(scene::Joint::descriptor));
  const auto instance=joint->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(id,authored),"active incomplete joint remains authorable");
  AE_EXPECT_TRUE(runtime::referencesAccept(d,id,*joint),"draft validation accepts empty target");
  AE_EXPECT_TRUE(!runtime::referencesReadyForExecution(d,id,*joint),"execution requires target");
  bool missing=false;for(const auto &row:componentImpact(d,id,instance))
    if(row.relation=="Requisito de execução ausente") missing=row.invalid;
  AE_EXPECT_TRUE(missing,"impact exposes conditional missing target");
  joint->enabled=false;
  AE_EXPECT_TRUE(d.applyEntityValues(id,authored),"disabled draft accepted");
  AE_EXPECT_TRUE(runtime::referencesReadyForExecution(d,id,*joint),"disabled joint has no mandatory target");
  for(const auto &row:componentImpact(d,id,instance)) AE_EXPECT_TRUE(!row.invalid,"disabled draft has no missing reference warning");
  const auto destination=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Anchor");
  auto anchor=*d.find(destination);anchor.components.add(scene::PhysicsBody::descriptor);
  AE_EXPECT_TRUE(d.applyEntityValues(destination,anchor),"target body authored");
  joint->enabled=true;joint->connectedBody=destination;
  AE_EXPECT_TRUE(runtime::referencesReadyForExecution(d,id,*joint),"compatible active target ready");
  anchor.active=false;AE_EXPECT_TRUE(d.applyEntityValues(destination,anchor),"target deactivated");
  AE_EXPECT_EQ(runtime::referenceReadiness(d,id,*joint,scene::jointReferences[0]),runtime::ReferenceReadiness::Inactive,"inactive target blocks execution readiness");
  joint->connectedBody=id;
  AE_EXPECT_EQ(runtime::referenceReadiness(d,id,*joint,scene::jointReferences[0]),runtime::ReferenceReadiness::Incompatible,"self target stays invalid");
}

AE_TEST(p02_impact_keeps_optional_empty_references_nonblocking) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Collider");
  auto value=*d.find(id);const auto *collider=value.components.add(scene::Collider::descriptor);const auto instance=collider->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(id,value),"standalone collider authored");
  const auto rows=componentImpact(d,id,instance);
  AE_EXPECT_TRUE(!rows.empty(),"owner field is documented");
  bool missingBody=false;
  for(const auto &row:rows) {
    AE_EXPECT_TRUE(!row.blocksRemoval,"null owner is not a removal dependency");
    if(row.relation=="Referência vazia") AE_EXPECT_TRUE(!row.invalid,"null reference remains an authorable draft");
    if(row.relation=="Condição de execução") missingBody|=row.invalid;
  }
  AE_EXPECT_TRUE(missingBody,"execution still needs the implicit owner body");
  AE_EXPECT_TRUE(!scene::componentInstanceRemovalBlockedBy(instance,d.find(id)->components),"schema agrees with panel");
  AE_EXPECT_TRUE(!runtime::componentRemovalReferenceUse(d,id,instance).object,"reference resolver agrees with panel");
}

AE_TEST(p02_physics_impact_resolves_shapes_and_matches_runtime_scale_rules) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Body");
  auto authored=*d.find(id);
  auto *body=authored.components.add(scene::PhysicsBody::descriptor);
  const auto instance=body->instanceId();d.applyEntityValues(id,authored);
  auto rows=physicsComponentImpact(d,id,*body);
  AE_EXPECT_EQ(rows.size(),usize(1),"body missing all shapes reported");
  auto *collider=static_cast<scene::Collider*>(authored.components.add(scene::Collider::descriptor));
  collider->shape=scene::ColliderShape::Sphere;authored.transform.scale[0]=2;
  d.applyEntityValues(id,authored);
  rows=physicsComponentImpact(d,id,*body);bool shape=false,invalid=false;
  for(const auto &row:rows) {shape|=row.relation=="Forma vinculada";invalid|=row.invalid;}
  AE_EXPECT_TRUE(shape&&invalid,"linked sphere reports nonuniform scale before execution");
  collider->shape=scene::ColliderShape::Box;d.applyEntityValues(id,authored);
  rows=physicsComponentImpact(d,id,*body);
  for(const auto &row:rows) AE_EXPECT_TRUE(!row.invalid,"unrotated box accepts nonuniform scale");
  collider->rotationZ=45;d.applyEntityValues(id,authored);
  rows=physicsComponentImpact(d,id,*body);invalid=false;
  for(const auto &row:rows) invalid|=row.invalid;
  AE_EXPECT_TRUE(invalid,"rotated box under nonuniform scale reports shear");
  collider->enabled=false;d.applyEntityValues(id,authored);
  AE_EXPECT_TRUE(physicsComponentImpact(d,id,*collider).empty(),"disabled shape skips execution diagnostics");
  rows=physicsComponentImpact(d,id,*body);
  AE_EXPECT_TRUE(rows.size()==1&&rows[0].invalid,"disabled shape cannot satisfy body requirement");
  AE_EXPECT_EQ(d.find(id)->components.find(scene::PhysicsBody::descriptor)->instanceId(),instance,"query preserves identities");
}

AE_TEST(p02_component_presets_persist_preview_apply_and_undo_without_copying_references) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("astra-presets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);Fixture f;AE_EXPECT_TRUE(f.session.setProjectDirectory(root.string().c_str()),"project opened");
  auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Collider preset");
  auto authored=*d.find(id);authored.components.add(scene::PhysicsBody::descriptor);
  auto *collider=static_cast<scene::Collider*>(authored.components.add(scene::Collider::descriptor));
  collider->shape=scene::ColliderShape::Sphere;collider->radius=3;collider->owner=id;const auto instance=collider->instanceId();
  AE_EXPECT_TRUE(d.applyEntityValues(id,authored),"source values authored");
  f.session.setSelection(id);f.session.update();
  AE_EXPECT_TRUE(f.session.openComponentPresets(id,instance),"preset panel opened");f.session.update();
  tapWidget(f,widgetId(EditorWidget::PresetSave));const auto request=f.session.pendingTextEdit();
  AE_EXPECT_EQ(request.purpose,EditorTextPurpose::ComponentPresetName,"platform name request");
  AE_EXPECT_TRUE(f.session.completeTextEdit(request,"Esfera grande",true),"preset persisted through UI flow");
  AE_EXPECT_EQ(f.session.screen().presetChoices.size(),usize(1),"compatible choice listed");
  const auto preset=f.session.screen().presetChoices[0].first;
  EditorComponentPresets reloaded;std::string error;AE_EXPECT_TRUE(reloaded.load(root.string(),error),"library reopened");
  auto copy=reloaded.instantiate(preset,error);
  AE_EXPECT_EQ(static_cast<scene::Collider*>(copy.get())->owner,u64(0),"stored preset has no scene identity");
  collider->radius=1;collider->owner=0;d.applyEntityValues(id,authored);
  AE_EXPECT_TRUE(f.session.openComponentPresets(id,instance),"reopen refreshes preview version");f.session.update();
  const auto revision=d.revision();tapWidget(f,widgetId(EditorWidget::PresetChoiceBase));
  AE_EXPECT_EQ(d.revision(),revision,"selecting preset only previews");
  AE_EXPECT_TRUE(!f.session.screen().presetPreview.empty(),"preview generated");
  tapWidget(f,widgetId(EditorWidget::PresetApply));
  const auto *applied=static_cast<const scene::Collider*>(d.find(id)->components.findInstance(instance));
  AE_EXPECT_EQ(applied->radius,3.f,"saved radius applied");AE_EXPECT_EQ(applied->owner,u64(0),"destination reference preserved");
  AE_EXPECT_TRUE(h.undo(d),"preset has one undo");
  AE_EXPECT_EQ(static_cast<const scene::Collider*>(d.find(id)->components.findInstance(instance))->radius,1.f,"undo restores destination values");
  const auto before=d.find(id)->components.size();
  AE_EXPECT_TRUE(f.session.applyComponentPreset(preset,id,instance,f.session.sceneVersion(),true,error),"new instance applied transactionally");
  AE_EXPECT_EQ(d.find(id)->components.size(),before+1,"new instance added");
  AE_EXPECT_TRUE(h.undo(d),"new instance undone");AE_EXPECT_EQ(d.find(id)->components.size(),before,"undo removes added instance");
  auto stale=f.session.sceneVersion();h.createEntity(d,d.root(),EditorEntityKind::Folder,"Concurrent");
  AE_EXPECT_TRUE(!f.session.applyComponentPreset(preset,id,instance,stale,false,error),"stale preview rejected");
  // A preset can originate in a component menu and be added to an empty object;
  // its required components belong to the same undo operation.
  const auto cameraObject=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Camera preset");
  auto camera=*d.find(cameraObject);camera.components.add(scene::Camera::descriptor);
  auto *look=camera.components.add(scene::CameraLook::descriptor);const auto lookId=look->instanceId();
  d.applyEntityValues(cameraObject,camera);
  AE_EXPECT_TRUE(f.session.openComponentPresets(cameraObject,lookId),"camera preset source opened");
  AE_EXPECT_TRUE(f.session.saveComponentPreset(cameraObject,lookId,"Olhar reutilizável",error),"look preset saved");
  const auto empty=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Empty");
  f.session.setSelection(empty);
  AE_EXPECT_TRUE(f.session.openComponentPresets(empty,0),"all presets available on empty object");
  const auto lookPreset=f.session.screen().presetChoices.back().first;
  AE_EXPECT_TRUE(f.session.applyComponentPreset(lookPreset,empty,0,f.session.sceneVersion(),true,error),"add resolves camera dependency");
  AE_EXPECT_EQ(d.find(empty)->components.size(),usize(2),"camera and look added together");
  AE_EXPECT_TRUE(h.undo(d),"composition preset undo");AE_EXPECT_EQ(d.find(empty)->components.size(),usize(0),"both additions undone");
  AE_EXPECT_TRUE(reloaded.load(root.string(),error),"refresh independent library before external rename");
  AE_EXPECT_TRUE(reloaded.rename(preset,"Esfera renomeada",error),"rename persisted");
  AE_EXPECT_TRUE(!f.session.saveComponentPreset(id,instance,"Concurrent save",error),"external library edit cannot be overwritten");
  AE_EXPECT_TRUE(reloaded.erase(preset,error),"delete persisted");
  AE_EXPECT_TRUE(reloaded.load(root.string(),error)&&reloaded.entries.size()==1,"deletion preserves unrelated preset");
  auto meshObject=*d.find(f.cube);auto *mesh=static_cast<scene::MeshRenderer*>(meshObject.components.edit(scene::MeshRenderer::descriptor));
  const auto meshInstance=mesh->instanceId();mesh->mesh=1;mesh->asset={};d.applyEntityValues(f.cube,meshObject);
  AE_EXPECT_TRUE(f.session.openComponentPresets(f.cube,meshInstance),"mesh preset source");
  AE_EXPECT_TRUE(f.session.saveComponentPreset(f.cube,meshInstance,"Mesh slot one",error),"legacy one-based mesh resolves persistent identity");
  AE_EXPECT_TRUE(f.session.openComponentPresets(f.cube,meshInstance),"mesh choices refreshed");
  const auto meshPreset=f.session.screen().presetChoices.back().first;
  AE_EXPECT_TRUE(f.session.applyComponentPreset(meshPreset,f.cube,meshInstance,f.session.sceneVersion(),false,error),"mesh preset applied");
  const auto *resolved=meshRenderer(*d.find(f.cube));
  AE_EXPECT_EQ(resolved->mesh,1u,"resolved mesh index remains one based");
  AE_EXPECT_TRUE(resolved->asset==f.session.screen().resources->assetGuid(0),"persistent geometry identity preserved");
  std::error_code ec;fs::remove_all(root,ec);
}

AE_TEST(p02_impact_resources_use_persistent_ids_and_ignore_inheritance) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto first=h.createEntity(d,d.root(),EditorEntityKind::Folder,"First");
  auto a=*d.find(first);auto *mesh=static_cast<scene::MeshRenderer*>(a.components.add(scene::MeshRenderer::descriptor));
  const auto instance=mesh->instanceId();mesh->asset={10,20};mesh->materialAsset={30,40};mesh->textures[0]=scene::MaterialTextureNone;
  scene::MeshSubmesh sub;sub.asset={50,60};sub.textures[2]={70,80};mesh->submeshes.push_back(sub);
  AE_EXPECT_EQ(componentResources(*mesh).size(),4u,"collect mesh material submesh explicit texture only");
  AE_EXPECT_TRUE(d.applyEntityValues(first,a),"first authored");
  const auto second=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Second");
  auto b=*d.find(second);auto *other=static_cast<scene::MeshRenderer*>(b.components.add(scene::MeshRenderer::descriptor));
  const auto otherInstance=other->instanceId();other->materialAsset={30,40};
  AE_EXPECT_TRUE(d.applyEntityValues(second,b),"second authored");
  auto rows=componentImpact(d,first,instance);u32 shared=0;
  for(const auto &row:rows) if(row.object==second&&row.instance==otherInstance) {++shared;AE_EXPECT_TRUE(!row.blocksRemoval,"shared asset does not block component removal");}
  AE_EXPECT_EQ(shared,1u,"shared persistent material locates user");
  b=*d.find(second);static_cast<scene::MeshRenderer*>(b.components.editInstance(otherInstance))->materialAsset={90,100};
  AE_EXPECT_TRUE(d.applyEntityValues(second,b),"binding changed");
  rows=componentImpact(d,first,instance);shared=0;for(const auto &row:rows) if(row.object==second) ++shared;
  AE_EXPECT_EQ(shared,0u,"query refreshes after changing binding");
}

AE_TEST(p02_scene_repair_is_atomic_across_consumers_and_rejects_mixed_types) {
  Fixture f;auto &s=f.session;auto &d=s.document();auto &h=s.history();
  const resources::AssetGuid missing{700,800};const auto replacement=s.screen().resources->assetGuid(0);
  resources::AssetRegistry registry;resources::AssetRecord record;record.guid=replacement;record.type=resources::AssetType::Mesh;record.path="Meshes/target.mesh";
  AE_EXPECT_TRUE(registry.add(record)&&s.loadAssets(registry.serialize()),"mesh target available");
  auto value=*d.find(f.cube);editMeshRenderer(value)->asset=missing;AE_EXPECT_TRUE(d.applyEntityValues(f.cube,value),"first binding");
  const auto second=h.duplicateEntity(d,f.cube);value=*d.find(second);editMeshRenderer(value)->textures[0]=missing;
  AE_EXPECT_TRUE(d.applyEntityValues(second,value),"corrupt mixed-type consumer");
  std::string diagnostic;const auto depth=h.undoDepth();
  AE_EXPECT_TRUE(!s.repairSceneResource(missing,replacement,s.sceneVersion(),diagnostic),"mixed types reject whole transaction");
  AE_EXPECT_TRUE(meshRenderer(*d.find(f.cube))->asset==missing,"no partial first-object repair");
  AE_EXPECT_EQ(h.undoDepth(),depth,"failed preparation preserves undo");
  value=*d.find(second);editMeshRenderer(value)->textures[0]={};AE_EXPECT_TRUE(d.applyEntityValues(second,value),"remove corrupt fixture binding");
  AE_EXPECT_TRUE(s.repairSceneResource(missing,replacement,s.sceneVersion(),diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(meshRenderer(*d.find(f.cube))->asset==replacement&&meshRenderer(*d.find(second))->asset==replacement,"all consumers updated");
  AE_EXPECT_EQ(h.undoDepth(),depth+1,"one scene-wide transaction");
  AE_EXPECT_TRUE(h.undo(d),"one undo");
  AE_EXPECT_TRUE(meshRenderer(*d.find(f.cube))->asset==missing&&meshRenderer(*d.find(second))->asset==missing,"both consumers restored");
  AE_EXPECT_TRUE(s.loadAssets(resources::AssetRegistry{}.serialize()),"empty project registry");
  AE_EXPECT_TRUE(s.repairSceneResource(missing,replacement,s.sceneVersion(),diagnostic),"loaded package identities remain valid repair candidates");
}

AE_TEST(p02_resource_repair_previews_multiple_slots_and_undoes_atomically) {
  Fixture f;auto &session=f.session;auto &d=session.document();auto &h=session.history();
  const resources::AssetGuid missing{900,901};const auto target=session.screen().resources->assetGuid(0);
  resources::AssetRegistry registry;resources::AssetRecord record;record.guid=target;record.type=resources::AssetType::Mesh;record.path="Meshes/replacement.mesh";
  AE_EXPECT_TRUE(registry.add(record)&&session.loadAssets(registry.serialize()),"available mesh registered");
  auto value=*d.find(f.cube);auto *mesh=editMeshRenderer(value);const auto instance=mesh->instanceId();mesh->asset=missing;mesh->mesh=0;
  mesh->material.enabled=true;mesh->material.roughness=.37f;
  scene::MeshSubmesh sub;sub.asset=missing;sub.textures[0]={88,99};mesh->submeshes.push_back(sub);
  AE_EXPECT_TRUE(d.applyEntityValues(f.cube,value),"missing references authored");
  session.setSelection(f.cube);session.update();tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));tapWidget(f,widgetId(EditorWidget::ImpactOpenBase));
  tapWidget(f,widgetId(EditorWidget::ImpactRowBase));tapWidget(f,widgetId(EditorWidget::ImpactRepair));
  tapWidget(f,widgetId(EditorWidget::ImpactRowBase));
  AE_EXPECT_TRUE(session.screen().impactReplacement==target,"candidate staged");
  AE_EXPECT_TRUE(meshRenderer(*d.find(f.cube))->asset==missing,"preview never mutates");
  const auto depth=h.undoDepth();tapWidget(f,widgetId(EditorWidget::ImpactRepairApply));
  const auto *updated=meshRenderer(*d.find(f.cube));
  AE_EXPECT_TRUE(updated->slotAsset(0)==target&&updated->slotAsset(1)==target,"both explicit mesh uses replaced");
  AE_EXPECT_EQ(updated->slotMesh(1),1u,"package index resolved");
  AE_EXPECT_EQ(updated->material.roughness,.37f,"material override preserved");
  AE_EXPECT_TRUE(updated->submeshes[0].textures[0]==resources::AssetGuid({88,99}),"texture preserved");
  AE_EXPECT_EQ(h.undoDepth(),depth+1,"one undo for all slots");
  AE_EXPECT_TRUE(h.undo(d),"undo repair");
  AE_EXPECT_TRUE(meshRenderer(*d.find(f.cube))->slotAsset(1)==missing,"undo restores missing identity");
  const auto stale=session.sceneVersion();AE_EXPECT_TRUE(h.redo(d),"redo repair");std::string diagnostic;
  AE_EXPECT_TRUE(!session.repairComponentResource(f.cube,instance,missing,target,stale,diagnostic),"stale preview rejected");
}

AE_TEST(p02_resource_panel_opens_and_returns_without_mutation) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const auto id=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Resource user");
  auto value=*d.find(id);auto *mesh=static_cast<scene::MeshRenderer*>(value.components.add(scene::MeshRenderer::descriptor));
  const auto instance=mesh->instanceId();const resources::AssetGuid asset{111,222};mesh->materialAsset=asset;
  AE_EXPECT_TRUE(d.applyEntityValues(id,value),"binding authored");
  f.session.setSelection(id);f.session.update();tapWidget(f,widgetId(EditorWidget::ComponentMenuBase));
  tapWidget(f,widgetId(EditorWidget::ImpactOpenBase));
  tapWidget(f,widgetId(EditorWidget::ImpactRowBase));
  AE_EXPECT_TRUE(f.session.screen().impactAsset==asset,"missing resource also opens details");
  AE_EXPECT_EQ(f.session.screen().impactTrail.size(),1u,"origin retained");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_TRUE(!f.session.screen().impactAsset.valid(),"back returns to component relations");
  AE_EXPECT_EQ(f.session.screen().impactInstance,instance,"component identity retained");
  AE_EXPECT_TRUE(static_cast<const scene::MeshRenderer*>(d.find(id)->components.findInstance(instance))->materialAsset==asset,"query preserves binding");
  tapWidget(f,widgetId(EditorWidget::ImpactClose));
  AE_EXPECT_EQ(f.session.screen().impactInstance,0u,"second back closes panel");
}

AE_TEST(p02_resource_details_preserve_registry_edge_directions) {
  EditorDocument document;resources::AssetRegistry registry;
  const resources::AssetGuid texture{1,2},material{3,4};
  resources::AssetRecord record;record.guid=texture;record.type=resources::AssetType::Texture;record.path="Textures/base.png";
  AE_EXPECT_TRUE(registry.add(record),"texture registered");
  record.guid=material;record.type=resources::AssetType::Material;record.path="Materials/shared.material";record.dependencies={texture};
  AE_EXPECT_TRUE(registry.add(record),"material depends on texture");
  auto rows=resourceImpact(document,material,&registry,nullptr);bool direct=false;
  for(const auto &row:rows) if(row.asset==texture) direct=row.relation=="Depende de · registro";
  AE_EXPECT_TRUE(direct,"dependency navigable by GUID");
  rows=resourceImpact(document,texture,&registry,nullptr);bool reverse=false;
  for(const auto &row:rows) if(row.asset==material) reverse=row.relation=="Usado por · registro";
  AE_EXPECT_TRUE(reverse,"reverse dependency retains direction");
}

AE_TEST(p02_impact_resolves_material_inheritance_and_registry_diagnostics) {
  Fixture f;auto &d=f.session.document();auto &h=f.session.history();
  const resources::AssetGuid material{11,22},texture{33,44},wrong{55,66};
  resources::AssetRegistry registry;
  resources::AssetRecord record;record.guid=material;record.type=resources::AssetType::Material;record.path="Materiais/shared.material";
  AE_EXPECT_TRUE(registry.add(record),"material registered");
  record.guid=texture;record.type=resources::AssetType::Texture;record.path="Texturas/base.png";
  AE_EXPECT_TRUE(registry.add(record),"texture registered");
  record.guid=wrong;record.type=resources::AssetType::Mesh;record.path="Malhas/wrong.mesh";
  AE_EXPECT_TRUE(registry.add(record),"wrong typed resource registered");
  EditorMapScene library;EditorMapScene::SharedMaterial shared;shared.textures[0]=texture;
  library.setMaterialLibrary({{material,shared}});
  const auto first=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Inherited");
  auto a=*d.find(first);auto *mesh=static_cast<scene::MeshRenderer*>(a.components.add(scene::MeshRenderer::descriptor));
  const auto instance=mesh->instanceId();mesh->materialAsset=material;mesh->textures[1]=wrong;mesh->textures[3]={77,88};
  AE_EXPECT_TRUE(d.applyEntityValues(first,a),"author bindings");
  const auto second=h.createEntity(d,d.root(),EditorEntityKind::Folder,"Explicit");
  auto b=*d.find(second);auto *other=static_cast<scene::MeshRenderer*>(b.components.add(scene::MeshRenderer::descriptor));other->textures[0]=texture;
  AE_EXPECT_TRUE(d.applyEntityValues(second,b),"author explicit consumer");
  auto rows=componentImpact(d,first,instance,&registry,&library);
  bool inherited=false,badType=false,missing=false,consumer=false;
  for(const auto &row:rows) {
    if(row.detail=="Texturas/base.png") {inherited=row.relation.find("herdada")!=std::string::npos;AE_EXPECT_TRUE(!row.invalid,"registered inherited texture resolves");}
    if(row.detail=="Malhas/wrong.mesh") badType=row.invalid&&row.relation.find("tipo incompatível")!=std::string::npos;
    if(row.detail==resources::AssetGuid{77,88}.text()) missing=row.invalid&&row.relation.find("não registrado")!=std::string::npos;
    if(row.object==second) consumer=true;
  }
  AE_EXPECT_TRUE(inherited&&badType&&missing&&consumer,"inheritance status and reverse consumer resolved");
  a=*d.find(first);static_cast<scene::MeshRenderer*>(a.components.editInstance(instance))->textures[0]=scene::MaterialTextureNone;
  AE_EXPECT_TRUE(d.applyEntityValues(first,a),"explicit none overrides shared texture");
  rows=componentImpact(d,first,instance,&registry,&library);
  for(const auto &row:rows) {AE_EXPECT_TRUE(row.object!=second,"disabled texture no longer links consumer");AE_EXPECT_TRUE(row.detail!="Texturas/base.png","none suppresses inherited binding");}
  library.setMaterialLibrary({});rows=componentImpact(d,first,instance,&registry,&library);
  bool unloaded=false;for(const auto &row:rows) if(row.detail=="Materiais/shared.material") unloaded=row.invalid&&row.relation.find("não carregado")!=std::string::npos;
  AE_EXPECT_TRUE(unloaded,"registered material without loaded library is not falsely resolved");
}

AE_TEST(the_import_panel_measures_each_mesh_of_the_source_and_opens_its_card) {
  Fixture fixture;
  fixture.session.beginImportPreparation();
  // Uma fonte com duas malhas: um cubo bem mapeado e o mesmo cubo com a UV
  // colapsada, que é o caso que nenhuma resolução de textura conserta.
  resources::GltfImport model;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, model.vertices, model.indices,
                                                      draws, materials), "cubo");
  const auto flat = static_cast<u32>(model.vertices.size() / renderer::MapVertexStride);
  for (u32 vertex = 0; vertex < flat; ++vertex) {
    float zero[2]{0, 0};
    std::memcpy(model.vertices.data() + static_cast<usize>(vertex) * renderer::MapVertexStride + 28, zero, sizeof(zero));
  }
  model.vertices.insert(model.vertices.end(), model.vertices.begin(),
                        model.vertices.begin() + static_cast<long long>(flat) * renderer::MapVertexStride);
  // O primeiro bloco volta a ter UV; o segundo (a cópia) fica sem.
  {
    std::vector<u8> good;
    std::vector<u32> goodIndices;
    std::vector<renderer::MapDrawRecord> goodDraws;
    std::vector<renderer::MapMaterialRecord> goodMaterials;
    AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, good, goodIndices, goodDraws,
                                                        goodMaterials), "cubo de referência");
    std::copy(good.begin(), good.end(), model.vertices.begin());
  }
  model.materials = materials;
  model.materialNames.push_back("Concreto");
  // Material texturizado: é o que torna "sem UV" um erro, e não a observação
  // legítima de uma superfície de cor lisa.
  {
    auto texture = std::make_shared<renderer::AuthoringTexture>();
    const_cast<renderer::AuthoringTexture *>(texture.get())->width = 1024;
    const_cast<renderer::AuthoringTexture *>(texture.get())->height = 1024;
    model.textures.push_back(texture);
    model.materials[0].textureIndices[0] = 0;
  }
  model.draws.push_back(draws[0]);
  model.draws.push_back(draws[0]);
  model.draws[1].vertexOffset = flat;
  model.names.push_back("Parede");
  model.names.push_back("Piso");
  model.nodes.emplace_back();
  model.nodes[0].name = "Raiz";
  model.drawNodes.push_back(0);
  model.drawNodes.push_back(0);
  fixture.session.showImportPreview("Fontes/cenario.glb", model, {}, fixture.session.importProfileDraft());
  fixture.session.update();

  const auto &screen = fixture.session.screen();
  AE_EXPECT_EQ(screen.importMeshes.size(), usize{2}, "uma linha por malha da fonte");
  AE_EXPECT_TRUE(screen.importMeshes[0].name == "Parede", "o nome do desenho é o da linha");
  AE_EXPECT_TRUE(screen.importMeshes[0].channels.find("UV0") != std::string::npos &&
                 screen.importMeshes[0].channels.find("Tangentes") != std::string::npos,
                 "os canais presentes são listados como na prévia de malha da Unity");
  AE_EXPECT_TRUE(screen.importMeshes[0].counts.find("triângulos") != std::string::npos, "contagem na linha");
  AE_EXPECT_TRUE(screen.importMeshes[0].size.find("m") != std::string::npos, "tamanho na cena na linha");
  AE_EXPECT_TRUE(screen.importMeshes[1].level == 2 && !screen.importMeshes[1].issues.empty(),
                 "a malha sem UV é apontada como erro, com o motivo");
  AE_EXPECT_TRUE(screen.importMeshSummary.find("profundidade") != std::string::npos,
                 "o resumo traz hierarquia e escala do arquivo");

  tapWidget(fixture, widgetId(EditorWidget::ImportTabMeshes));
  AE_EXPECT_TRUE(fixture.session.screen().importTab == EditorScreenState::ImportTab::Meshes, "a aba Malhas abre");
  tapWidget(fixture, widgetId(EditorWidget::ImportMeshRowBase) + 1);
  AE_EXPECT_TRUE(fixture.session.screen().importMeshDetail && fixture.session.screen().importMeshSelected == 1,
                 "tocar a linha abre o cartão daquela malha");
  tapWidget(fixture, widgetId(EditorWidget::ImportMeshClose));
  AE_EXPECT_TRUE(!fixture.session.screen().importMeshDetail, "voltar fecha o cartão e devolve a lista");
}

AE_TEST(the_reference_scene_template_builds_interior_and_exterior_in_one_undo_step) {
  EditorSession session;
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride, vertices, indices, draws, materials),
                 "biblioteca com o cubo autoral");
  AE_EXPECT_TRUE(session.importMap(draws, materials, false, vertices, indices, 0), "biblioteca carregada");
  const auto before = session.document().entityCount();
  AE_EXPECT_TRUE(session.createSceneTemplate(0), "o modelo de referência monta");
  const auto &model = sceneTemplates()[0];
  AE_EXPECT_EQ(session.document().entityCount(), before + model.nodes.size() + model.volumes.size() + 1,
               "um objeto por nó e por volume do modelo, mais o grupo que os segura");

  // Escala humana: é ela que faz um modelo importado parecer grande ou pequeno
  // demais no ato, então precisa chegar com 1,80 m mesmo.
  bool humanFound = false, sunFound = false, ceilingLight = false, spotFound = false;
  for (EditorEntityId id = 1; id <= session.document().entityCount() + 8; ++id) {
    const auto *entity = session.document().find(id);
    if (!entity) continue;
    if (entity->name == std::string("Pessoa 1,80 m")) {
      humanFound = std::fabs(entity->transform.scale[1] - 1.8f) < 1e-4f;
      AE_EXPECT_TRUE(meshRenderer(*entity) != nullptr, "a referência humana é malha de verdade");
    }
    const auto *light = static_cast<const scene::Light *>(entity->components.find(scene::Light::descriptor));
    if (!light) continue;
    if (light->kind == scene::LightKind::Directional)
      sunFound = light->unit == scene::LightUnit::LuxCandela && light->intensity >= 50000;
    if (light->kind == scene::LightKind::Point)
      ceilingLight = light->unit == scene::LightUnit::LuxLumen && light->range > 1 && light->shadowMode == 2;
    if (light->kind == scene::LightKind::Spot) spotFound = true;
  }
  AE_EXPECT_TRUE(humanFound, "a referência de 1,80 m está na cena");
  AE_EXPECT_TRUE(sunFound, "o sol chega em lux, como luz direcional");
  AE_EXPECT_TRUE(ceilingLight && spotFound, "o interior tem luz de teto com sombra suave e foco de parede");

  // As vistas vêm com o modelo: é o enquadramento repetido que torna duas
  // medições comparáveis.
  AE_EXPECT_EQ(session.document().views().count(), static_cast<u32>(model.views.size()), "as vistas do modelo entraram");
  AE_EXPECT_TRUE(session.document().views().find("Interior") && session.document().views().find("Exterior"),
                 "interior e exterior têm enquadramento salvo");
  AE_EXPECT_TRUE(session.screen().status.find("vistas salvas") != std::string::npos, "o status diz o que foi montado");

  // Um passo de Desfazer devolve a cena inteira, vistas inclusive: montar um
  // cenário por engano não pode custar trinta toques para desfazer.
  AE_EXPECT_TRUE(session.history().undo(session.document()), "desfazer o modelo");
  AE_EXPECT_EQ(session.document().entityCount(), before, "a cena volta ao que era");
  AE_EXPECT_TRUE(session.document().views().empty(), "as vistas do modelo saem junto");
  AE_EXPECT_TRUE(session.history().redo(session.document()), "refazer devolve o modelo");
  AE_EXPECT_EQ(session.document().views().count(), static_cast<u32>(model.views.size()), "e as vistas voltam");

  // Sem o cubo autoral na biblioteca não há como montar: melhor dizer isso do
  // que montar um cenário invisível.
  EditorSession bare;
  AE_EXPECT_TRUE(bare.importMap({}, {}, false), "biblioteca vazia");
  AE_EXPECT_TRUE(!bare.createSceneTemplate(0), "modelo recusado sem geometria");
  AE_EXPECT_TRUE(bare.document().entityCount() == 1, "nada entrou na cena");
  AE_EXPECT_TRUE(!bare.createSceneTemplate(99), "índice fora do catálogo é recusado");
}

AE_TEST(the_quality_textures_tab_edits_mipmap_streaming_and_shows_what_the_gpu_holds) {
  Fixture fixture;
  fixture.session.setRenderingSettings({});
  fixture.session.update();
  tapWidget(fixture, widgetId(EditorWidget::QualityOpen));
  tapWidget(fixture, widgetId(EditorWidget::QualityTabTextures));
  AE_EXPECT_EQ(fixture.session.screen().qualityTab, 4u, "aba Texturas");
  AE_EXPECT_TRUE(locateWidget(fixture.session, widgetId(EditorWidget::QualityTextureStreaming)).x >= 0,
                 "linha do streaming na aba");
  // Herdado -> Desligado -> Ligado: o ciclo das outras linhas do painel.
  tapWidget(fixture, widgetId(EditorWidget::QualityTextureStreaming));
  tapWidget(fixture, widgetId(EditorWidget::QualityTextureStreaming));
  const auto &draft = fixture.session.screen().qualityDraft;
  AE_EXPECT_EQ(draft.textureStreaming, renderer::FeatureOverride::Enabled, "streaming ligado no rascunho");
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingBudgetUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingBudgetUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingBudgetUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingBudgetUp));
  AE_EXPECT_EQ(draft.textureStreamingBudgetMegabytes, 256u, "orçamento sobe pelos degraus do Memory Budget");
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingBudgetDown));
  AE_EXPECT_EQ(draft.textureStreamingBudgetMegabytes, 128u, "e desce");
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingReductionUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingReductionUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingReductionUp));
  AE_EXPECT_EQ(draft.textureStreamingMaxLevelReduction, 3u, "redução máxima em degraus de um nível");
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingUploadUp));
  AE_EXPECT_EQ(draft.textureStreamingUploadKilobytesPerFrame, 512u, "envio por quadro");
  tapWidget(fixture, widgetId(EditorWidget::QualityApply));
  renderer::ProjectRenderingSettings requested;
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(requested) &&
                 requested.textureStreaming == renderer::FeatureOverride::Enabled &&
                 requested.textureStreamingBudgetMegabytes == 128 && requested.textureStreamingMaxLevelReduction == 3 &&
                 requested.textureStreamingUploadKilobytesPerFrame == 512, "Aplicar leva os quatro campos");
  // O que o renderer relata aparece no painel, sem inventar números no host.
  renderer::TextureStreamingStats stats;
  stats.active = true;
  stats.currentBytes = 96ull << 20;
  stats.budgetBytes = 128ull << 20;
  stats.streamingTextures = 72;
  stats.pendingLoads = 5;
  fixture.session.setTextureStreamingStatus(stats);
  fixture.session.update();
  AE_EXPECT_TRUE(fixture.session.screen().qualityTextureStreaming.streamingTextures == 72 &&
                 fixture.session.screen().qualityTextureStreaming.pendingLoads == 5, "estado do renderer chega à tela");
  // A vista de depuração é ferramenta do editor: não suja o rascunho nem o projeto.
  const bool dirtyBefore = fixture.session.screen().qualityDirty;
  // Numa tela baixa a aba pagina; a linha fica na página seguinte.
  for (u32 page = 0; page < 4 && locateWidget(fixture.session, widgetId(EditorWidget::QualityStreamingDebugView)).x < 0; ++page)
    tapWidget(fixture, widgetId(EditorWidget::QualityPageNext));
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingDebugView));
  AE_EXPECT_TRUE(fixture.session.screen().qualityTextureStreamingDebug &&
                 fixture.session.screen().qualityDirty == dirtyBefore, "vista de depuração liga sem mudar o rascunho");
  tapWidget(fixture, widgetId(EditorWidget::QualityStreamingDebugView));
  AE_EXPECT_TRUE(!fixture.session.screen().qualityTextureStreamingDebug, "e desliga");
}

AE_TEST(light_explorer_lists_dark_lights_and_lights_them_in_one_undo) {
  Fixture fixture;
  auto &document = fixture.session.document();
  auto &history = fixture.session.history();
  bool created = true;
  const auto addLight = [&](const char *name, float intensity) -> EditorEntityId {
    const auto id = history.createEntity(document, document.root(), EditorEntityKind::Folder, name);
    auto values = *document.find(id);
    auto *light = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
    light->intensity = intensity;
    created = created && history.applyValues(document, id, values);
    return id;
  };
  // O caso do Sponza: luzes do arquivo com intensidade 0, que não iluminam.
  const auto lamp = addLight("Lampião", 0);
  const auto sun = addLight("SUN", 0);
  const auto lit = addLight("Acesa", 1000);
  AE_EXPECT_TRUE(created, "luzes criadas");
  fixture.session.update();
  tapWidget(fixture, widgetId(EditorWidget::QualityOpen));
  tapWidget(fixture, widgetId(EditorWidget::QualityTabLighting));
  for (u32 page = 0; page < 4 && locateWidget(fixture.session, widgetId(EditorWidget::LightExplorerOpen)).x < 0; ++page)
    tapWidget(fixture, widgetId(EditorWidget::QualityPageNext));
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerOpen));
  const auto &state = fixture.session.screen();
  AE_EXPECT_TRUE(state.lightExplorer && !state.qualityPanel, "o explorador abre no lugar do painel");
  AE_EXPECT_TRUE(state.lightExplorerTotal == 3 && state.lightExplorerDark == 2 && state.lightExplorerRows.size() == 3,
                 "três luzes, duas apagadas");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerFilter));
  AE_EXPECT_TRUE(state.lightExplorerDarkOnly && state.lightExplorerFiltered == 2 && state.lightExplorerRows.size() == 2,
                 "filtro mostra só as apagadas");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerIntensityUp));
  AE_EXPECT_EQ(state.lightExplorerIntensity, 2500.0f, "degrau acima de 1000");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerApply));
  const auto intensityOf = [&](EditorEntityId id) {
    return static_cast<const scene::Light *>(document.find(id)->components.find(scene::Light::descriptor))->intensity;
  };
  AE_EXPECT_TRUE(intensityOf(lamp) == 2500 && intensityOf(sun) == 2500 && intensityOf(lit) == 1000,
                 "o lote muda só as filtradas");
  AE_EXPECT_EQ(state.lightExplorerDark, 0u, "nenhuma apagada depois do lote");
  AE_EXPECT_TRUE(history.undo(document), "um Undo");
  AE_EXPECT_TRUE(intensityOf(lamp) == 0 && intensityOf(sun) == 0 && intensityOf(lit) == 1000, "desfaz o lote inteiro de uma vez");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerFilter));
  const auto first = state.lightExplorerRows.front();
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerToggle0));
  const auto *toggled = static_cast<const scene::Light *>(document.find(first.entity)->components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(toggled->enabled != first.enabled, "interruptor da linha troca Acesa/Apagada");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerRow0) + 2);
  AE_EXPECT_EQ(state.selection, state.lightExplorerRows[2].entity, "tocar no nome seleciona a luz");
  tapWidget(fixture, widgetId(EditorWidget::LightExplorerClose));
  AE_EXPECT_TRUE(!state.lightExplorer, "fecha");
  // A importação diz quantas luzes chegam apagadas e onde acendê-las.
  resources::GltfImport model;
  model.nodes.emplace_back();
  model.lights.resize(2);
  model.lights[0].intensity = 0;
  fixture.session.showImportPreview("Fontes/cena.glb", model, {}, fixture.session.importProfileDraft());
  fixture.session.update();
  AE_EXPECT_TRUE(state.importSummary.find("Luzes do arquivo: 2; 1 com intensidade 0") != std::string::npos &&
                 state.importSummary.find("Explorador de luzes") != std::string::npos,
                 "o resumo avisa das luzes apagadas");
}

AE_TEST(scene_statistics_overlay_toggles_from_the_performance_tab_and_shows_the_renderer_report) {
  Fixture fixture;
  fixture.session.setRenderingSettings({});
  fixture.session.update();
  tapWidget(fixture, widgetId(EditorWidget::QualityOpen));
  tapWidget(fixture, widgetId(EditorWidget::QualityTabPerformance));
  for (u32 page = 0; page < 4 && locateWidget(fixture.session, widgetId(EditorWidget::QualitySceneStatistics)).x < 0; ++page)
    tapWidget(fixture, widgetId(EditorWidget::QualityPageNext));
  const bool dirtyBefore = fixture.session.screen().qualityDirty;
  tapWidget(fixture, widgetId(EditorWidget::QualitySceneStatistics));
  AE_EXPECT_TRUE(fixture.session.screen().sceneStatisticsVisible && fixture.session.screen().qualityDirty == dirtyBefore,
                 "estatísticas ligam sem mexer no rascunho do projeto");
  renderer::SceneStatistics statistics;
  statistics.frameIntervalMs = 16.7f;
  statistics.drawCalls = 412;
  statistics.triangles = 1200000;
  statistics.lodDraws = 300;
  statistics.lodReducedDraws = 120;
  statistics.lodBaseTriangles = 3700000;
  statistics.lodSelectedTriangles = 1200000;
  fixture.session.setSceneStatistics(statistics);
  fixture.session.update();
  const auto &shown = fixture.session.screen().sceneStatistics;
  AE_EXPECT_TRUE(shown.drawCalls == 412 && shown.lodReducedDraws == 120 && shown.lodSelectedTriangles == 1200000,
                 "o relatório do renderer chega ao overlay");
  tapWidget(fixture, widgetId(EditorWidget::QualitySceneStatistics));
  AE_EXPECT_TRUE(!fixture.session.screen().sceneStatisticsVisible, "e desliga");
}

AE_TEST(the_quality_panel_edits_a_draft_and_applies_it_on_request) {
  Fixture fixture;
  renderer::ProjectRenderingSettings project;
  project.preset = renderer::QualityPreset::Auto;
  fixture.session.setRenderingSettings(project);
  fixture.session.update();
  tapWidget(fixture, widgetId(EditorWidget::QualityOpen));
  AE_EXPECT_TRUE(fixture.session.screen().qualityPanel, "o painel Qualidade abre pelo viewport");
  // Automático → Baixo → Médio → Alto, na ordem dos níveis da Unity.
  for (u32 tap = 0; tap < 3; ++tap) tapWidget(fixture, widgetId(EditorWidget::QualityLevel));
  for (u32 tap = 0; tap < 5; ++tap) tapWidget(fixture, widgetId(EditorWidget::QualityScaleDown));
  // O painel pode abrir sobre qualquer valor persistido; percorra o ciclo real
  // até TAA, como o usuário faz, em vez de depender do valor inicial.
  for (u32 tap=0;tap<4 && fixture.session.screen().qualityDraft.antiAliasing!=renderer::AntiAliasingMode::Temporal;++tap)
    tapWidget(fixture, widgetId(EditorWidget::QualityAntiAliasing));
  AE_EXPECT_TRUE(fixture.session.screen().qualityDraft.preset == renderer::QualityPreset::A, "rascunho em Alto");
  AE_EXPECT_TRUE(std::fabs(fixture.session.screen().qualityDraft.resolutionScale - .75f) < 1e-4f,
                 "escala desce em passos de 5%, a partir de 100%");
  AE_EXPECT_EQ(fixture.session.screen().qualityDraft.antiAliasing, renderer::AntiAliasingMode::Temporal,
               "anti-aliasing percorre desligado, FXAA e TAA");
  AE_EXPECT_TRUE(fixture.session.screen().qualityDirty, "rascunho diferente do aplicado");
  renderer::ProjectRenderingSettings requested;
  AE_EXPECT_TRUE(!fixture.session.takeRenderingSettingsRequest(requested), "editar não reconstrói nada sozinho");
  tapWidget(fixture, widgetId(EditorWidget::QualityApply));
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(requested), "Aplicar levanta o pedido para o shell");
  AE_EXPECT_TRUE(requested.preset == renderer::QualityPreset::A, "o pedido leva o nível escolhido");
  AE_EXPECT_TRUE(std::fabs(requested.resolutionScale - .75f) < 1e-4f &&
                 requested.antiAliasing == renderer::AntiAliasingMode::Temporal,
                 "o pedido leva escala e anti-aliasing escolhidos");
  AE_EXPECT_TRUE(!fixture.session.takeRenderingSettingsRequest(requested), "o pedido é consumido uma vez");
  fixture.session.completeRenderingSettingsRequest(true);
  AE_EXPECT_TRUE(!fixture.session.screen().qualityDirty, "aplicado");

  tapWidget(fixture, widgetId(EditorWidget::QualityClose));
  tapWidget(fixture, widgetId(EditorWidget::QualityOpen));
  AE_EXPECT_TRUE(std::fabs(fixture.session.screen().qualityDraft.resolutionScale - .75f) < 1e-4f &&
                 fixture.session.screen().qualityDraft.antiAliasing == renderer::AntiAliasingMode::Temporal,
                 "fechar e reabrir o painel preserva os valores aplicados");

  renderer::ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(renderer::readRenderingSettings(renderer::writeRenderingSettings(requested), restored),
                 "arquivo do projeto reabre");
  Fixture reopened;
  reopened.session.setRenderingSettings(restored);
  reopened.session.update();
  tapWidget(reopened, widgetId(EditorWidget::QualityOpen));
  AE_EXPECT_TRUE(reopened.session.screen().qualityDraft.preset == renderer::QualityPreset::A &&
                 std::fabs(reopened.session.screen().qualityDraft.resolutionScale - .75f) < 1e-4f &&
                 reopened.session.screen().qualityDraft.antiAliasing == renderer::AntiAliasingMode::Temporal,
                 "uma nova sessao restaura nivel, escala e TAA do codec persistido");
  tapWidget(fixture, widgetId(EditorWidget::QualityScaleUp));
  tapWidget(fixture, widgetId(EditorWidget::QualityApply));
  renderer::ProjectRenderingSettings rejected;
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(rejected), "nova aplicação entrega um snapshot");
  fixture.session.completeRenderingSettingsRequest(false);
  AE_EXPECT_TRUE(fixture.session.screen().qualityDirty &&
                 std::fabs(fixture.session.screen().qualityDraft.resolutionScale - .80f) < 1e-4f,
                 "falha de gravação preserva o rascunho e mantém Aplicar ativo");
  AE_EXPECT_TRUE(std::fabs(fixture.session.renderingSettings().resolutionScale - .75f) < 1e-4f,
                 "falha não anuncia configuração como aplicada");
  fixture.session.setRenderStats(1280, 2772, 18.5f, "Médio");
  AE_EXPECT_TRUE(fixture.session.screen().qualityStats.find("1280×2772") != std::string::npos &&
                 fixture.session.screen().qualityStats.find("18,5 ms") != std::string::npos,
                 "o rodapé mostra a resolução real e o custo de GPU");
}

AE_TEST(temporal_debug_views_are_live_editor_state_only_when_taa_is_available) {
  Fixture fixture;
  fixture.session.update();
  tapWidget(fixture,widgetId(EditorWidget::QualityOpen));
  tapWidget(fixture,widgetId(EditorWidget::QualityTabLighting));
  fixture.session.setTemporalDebugAvailable(true);
  for(u32 page=0;page<4 &&
      locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebug)).x<0;++page) {
    AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityPageNext)).x>=0,
                   "controle de diagnóstico alcançável pela paginação");
    tapWidget(fixture,widgetId(EditorWidget::QualityPageNext));
  }
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebug)).x>=0,
                 "diagnóstico aparece quando o renderer tem TAA");
  fixture.session.setTemporalDebugAvailable(false);
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebug)).x<0,
                 "diagnóstico não aceita toques sem consumidor temporal");
  fixture.session.setTemporalDebugAvailable(true);
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalDebug));
  AE_EXPECT_EQ(fixture.session.screen().qualityTemporalDebug,1u,
               "controle escolhe a visualização de profundidade");
  AE_EXPECT_TRUE(!fixture.session.screen().qualityDirty,
                 "modo de inspeção não altera a configuração autoral");
  renderer::ProjectRenderingSettings request;
  AE_EXPECT_TRUE(!fixture.session.takeRenderingSettingsRequest(request),
                 "diagnóstico não solicita reconstrução nem gravação");
  tapWidget(fixture,widgetId(EditorWidget::QualityClose));
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebugQuick)).x>=0,
                 "atalho permanece na barra com o painel fechado");
  // Profundidade, histórico, rejeição, reatividade e composição (sem vetor).
  for(u32 tap=0;tap<5;++tap) tapWidget(fixture,widgetId(EditorWidget::QualityTemporalDebugQuick));
  AE_EXPECT_EQ(fixture.session.screen().qualityTemporalDebug,0u,
               "atalho percorre as vistas até retornar à imagem final");
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebugQuick)).x<0 &&
                 !fixture.session.screen().qualityDirty,
                 "atalho desaparece na imagem final e não grava o projeto");
  fixture.session.setTemporalDebugAvailable(true,true);
  tapWidget(fixture,widgetId(EditorWidget::QualityOpen));
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalDebug)).x>=0,
                 "controle do painel permanece acessível após reabrir");
  for(u32 tap=0;tap<4;++tap) tapWidget(fixture,widgetId(EditorWidget::QualityTemporalDebug));
  AE_EXPECT_EQ(fixture.session.screen().qualityTemporalDebug,4u,
               "vista de movimento só entra no ciclo quando o passe real está disponível");
  fixture.session.setTemporalDebugAvailable(true,false);
  AE_EXPECT_EQ(fixture.session.screen().qualityTemporalDebug,0u,
               "perder o passe de vetores retira o diagnóstico de movimento");
  for(u32 tap=0;tap<4;++tap) tapWidget(fixture,widgetId(EditorWidget::QualityTemporalDebug));
  AE_EXPECT_EQ(fixture.session.screen().qualityTemporalDebug,5u,
               "reatividade permanece inspecionável sem vetor rígido");
}

AE_TEST(temporal_reconstruction_control_shows_four_states_and_refuses_unavailable_upscalers) {
  Fixture fixture;
  renderer::ProjectRenderingSettings project;
  project.antiAliasing=renderer::AntiAliasingMode::Fxaa;
  project.upscalingFilter=renderer::UpscalingFilter::CatmullRom;
  fixture.session.setRenderingSettings(project);
  fixture.session.setTemporalUpscalerStatus(renderer::TemporalUpscalerAvailability::Available,
      renderer::TemporalUpscalerAvailability::MissingStorageWriteWithoutFormat,
      renderer::UpscalingFilter::CatmullRom,renderer::TemporalUpscalerAvailability::Available,false);
  fixture.session.update();
  tapWidget(fixture,widgetId(EditorWidget::QualityOpen));
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalMode)).x>=0,
                 "ampliação temporal fica na aba Geral");
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalQuality)).x<0,
                 "preset temporal não aceita toque sem Arm ASR");
  // Desligada -> TAA nativo -> Arm ASR.
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalMode));
  AE_EXPECT_EQ(fixture.session.screen().qualityDraft.antiAliasing,renderer::AntiAliasingMode::Temporal,"TAA nativo");
  AE_EXPECT_EQ(fixture.session.screen().qualityDraft.upscalingFilter,renderer::UpscalingFilter::CatmullRom,
               "TAA preserva o filtro espacial");
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalMode));
  const auto &draft=fixture.session.screen().qualityDraft;
  AE_EXPECT_EQ(draft.upscalingFilter,renderer::UpscalingFilter::ArmAsr,"Arm ASR escolhido");
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalQuality)).x>=0 &&
                 locateWidget(fixture.session,widgetId(EditorWidget::QualityAntiAliasing)).x<0 &&
                 locateWidget(fixture.session,widgetId(EditorWidget::QualityUpscaling)).x<0,
                 "com ASR o preset fica editável; AA e filtro espacial são do ampliador");
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalQuality));
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalQuality));
  AE_EXPECT_EQ(draft.temporalUpscalerQuality,renderer::TemporalUpscalerQuality::Balanced,"preset independente da escala");
  // FSR 2 indisponível: o Aplicar recusa com o motivo e não grava.
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalMode));
  AE_EXPECT_EQ(draft.upscalingFilter,renderer::UpscalingFilter::Fsr2,"FSR 2 selecionável para ver o motivo");
  tapWidget(fixture,widgetId(EditorWidget::QualityApply));
  renderer::ProjectRenderingSettings request;
  AE_EXPECT_TRUE(!fixture.session.takeRenderingSettingsRequest(request),"FSR 2 indisponível não é aplicado");
  AE_EXPECT_TRUE(fixture.session.screen().status.find("indisponível")!=std::string::npos,"o motivo aparece");
  // Voltar a Desligada devolve o filtro espacial neutro e o AA não temporal.
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalMode));
  AE_EXPECT_TRUE(draft.upscalingFilter==renderer::UpscalingFilter::Bilinear &&
                 draft.antiAliasing==renderer::AntiAliasingMode::Fxaa,"desligar não deixa TAA escondido");
  // Controle rápido da barra: aplica na hora e pula FSR 2 com o motivo.
  tapWidget(fixture,widgetId(EditorWidget::QualityClose));
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::QualityTemporalModeQuick)).x>=0,"atalho na barra");
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalModeQuick));
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(request) &&
                 request.antiAliasing==renderer::AntiAliasingMode::Temporal,"atalho aplica TAA nativo");
  fixture.session.completeRenderingSettingsRequest(true);
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalModeQuick));
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(request) &&
                 request.upscalingFilter==renderer::UpscalingFilter::ArmAsr,"atalho aplica Arm ASR");
  fixture.session.completeRenderingSettingsRequest(true);
  tapWidget(fixture,widgetId(EditorWidget::QualityTemporalModeQuick));
  AE_EXPECT_TRUE(fixture.session.takeRenderingSettingsRequest(request) &&
                 !renderer::isTemporalUpscaler(request.upscalingFilter) &&
                 request.antiAliasing!=renderer::AntiAliasingMode::Temporal,"FSR 2 recusado: próximo é Desligada");
  AE_EXPECT_TRUE(fixture.session.screen().status.find("AMD FSR 2 indisponível")!=std::string::npos,
                 "a barra diz por que pulou");
}

// Valor por endereço no inspetor genérico: o peso de cada blend shape é um
// campo próprio, editado pelo teclado numérico, validado pela faixa do
// contrato e desfeito como qualquer propriedade autoral.
AE_TEST(session_inspector_edits_blend_shape_weights_per_slot_with_undo) {
  Fixture fixture;
  auto values=*fixture.session.document().find(fixture.cube);
  auto *skinned=static_cast<scene::SkinnedMesh *>(values.components.add(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(skinned!=nullptr,"malha deformável adicionada");
  if(!skinned) return;
  skinned->blendShapeWeights={0,0};
  AE_EXPECT_TRUE(fixture.session.document().applyEntityValues(fixture.cube,values),"dois blend shapes");
  fixture.session.setSelection(fixture.cube);fixture.session.history().clear();fixture.session.update();
  u32 type=0;
  const auto &components=fixture.session.document().find(fixture.cube)->components;
  for(u32 i=0;i<components.size();++i) if(&components.at(i)->type()==&scene::SkinnedMesh::descriptor) type=i;
  tapWidget(fixture,widgetId(EditorWidget::ComponentFoldBase)+type);
  // Abas refletidas: Skin e Blend shapes.
  tapWidget(fixture,widgetId(EditorWidget::ComponentGroupBase)+1);
  const u32 second=widgetId(EditorWidget::ComponentSlotNumberBase)+type+(0u<<8)+(1u<<16);
  revealProperty(fixture,second);
  AE_EXPECT_TRUE(locateWidget(fixture.session,widgetId(EditorWidget::ComponentSlotNumberBase)+type).x>=0,"campo do primeiro peso");
  tapWidget(fixture,second);
  AE_EXPECT_TRUE(fixture.session.screen().numericField==second,"teclado aberto no segundo endereço");
  for(const u32 key:{3u,1u,9u,4u}) tapWidget(fixture,widgetId(EditorWidget::NumericKeyBase)+key); // 42.5
  tapWidget(fixture,widgetId(EditorWidget::NumericApply));
  const auto *edited=static_cast<const scene::SkinnedMesh *>(
      fixture.session.document().find(fixture.cube)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(edited && edited->blendShapeWeights[0]==0 && edited->blendShapeWeights[1]==42.5f,"só o segundo peso muda");
  AE_EXPECT_EQ(fixture.session.history().undoDepth(),1u,"uma entrada de desfazer");
  // Fora da faixa do contrato: recusado, documento intacto.
  auto rejected=*fixture.session.document().find(fixture.cube);
  AE_EXPECT_TRUE(scene::setComponentSlotProperty(rejected.components,scene::SkinnedMesh::descriptor.id,"blend_shape_weight",1,2000.0f)==
                 scene::ComponentPropertyStatus::InvalidValue,"peso além de 1000 recusado");
  AE_EXPECT_TRUE(scene::setComponentSlotProperty(rejected.components,scene::SkinnedMesh::descriptor.id,"blend_shape_weight",2,10.0f)==
                 scene::ComponentPropertyStatus::InvalidValue,"endereço inexistente não é criado");
  AE_EXPECT_TRUE(fixture.session.history().undo(fixture.session.document()),"desfazer");
  edited=static_cast<const scene::SkinnedMesh *>(
      fixture.session.document().find(fixture.cube)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(edited && edited->blendShapeWeights[1]==0,"desfazer volta o peso");
  EditorActionRequest request;request.version=fixture.session.sceneVersion();request.entity=fixture.cube;
  request.action=EditorAction::ComponentSlotProperty;request.componentType=std::string(scene::SkinnedMesh::descriptor.id);
  request.componentProperty="blend_shape_weight";request.componentSlot=0;request.componentValue=100.0f;
  AE_EXPECT_TRUE(fixture.session.dispatch(request).status==EditorActionStatus::Applied,"ferramenta escreve o mesmo endereço");
  edited=static_cast<const scene::SkinnedMesh *>(
      fixture.session.document().find(fixture.cube)->components.find(scene::SkinnedMesh::descriptor));
  AE_EXPECT_TRUE(edited && edited->blendShapeWeights[0]==100,"peso pela ação de ferramenta");
}

// Entrada em Play sem toque (opção de lançamento da bancada): mesmo preparo do
// botão, e a volta pelo botão restaura o editor como sempre.
AE_TEST(session_start_play_matches_the_play_button_and_is_idempotent) {
  Fixture f;f.session.setSelection(f.cube);
  AE_EXPECT_TRUE(!f.session.isPlaying(),"começa editando");
  AE_EXPECT_TRUE(f.session.startPlay(),"Play aceito sem código pendente");
  AE_EXPECT_TRUE(f.session.isPlaying() && f.session.playRequested(),"mesmo estado que o botão produz");
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x<0,"painéis de autoria recolhidos");
  AE_EXPECT_TRUE(f.session.startPlay(),"pedir de novo não reinicia nem falha");
  tapWidget(f,widgetId(EditorWidget::PlayFromTopBar));
  AE_EXPECT_TRUE(!f.session.isPlaying(),"o botão para");
  AE_EXPECT_TRUE(locateWidget(f.session,hierarchyRowWidget(f.cube)).x>=0,"hierarquia volta");
}

// Cada receita tem ícone próprio: na grade da folha de criação o desenho é a
// primeira coisa que a pessoa lê, e dois cartões iguais obrigam a ler o nome.
AE_TEST(every_creation_recipe_has_its_own_icon_and_id) {
  for(u32 i=0;i<editorCreationCatalog.size();++i) {
    AE_EXPECT_TRUE(editorCreationCatalog[i].icon!=ui::UiIcon::None,"receita com ícone");
    AE_EXPECT_TRUE(!editorCreationCatalog[i].id.empty(),"receita com id persistente");
    for(u32 j=0;j<i;++j) {
      AE_EXPECT_TRUE(editorCreationCatalog[i].icon!=editorCreationCatalog[j].icon,"ícone exclusivo por receita");
      AE_EXPECT_TRUE(editorCreationCatalog[i].id!=editorCreationCatalog[j].id,"id exclusivo por receita");
    }
  }
  for(const auto name:creationCategoryIcons) AE_EXPECT_TRUE(editorIconByName(name)!=ui::UiIcon::None,"ícone de categoria existe");
}
