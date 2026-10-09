#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_session.h"
#include "editor/editor_archive.h"
#include "runtime/scene_navigation.h"
#include "scene/navigation.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <filesystem>

using namespace ae;
using namespace ae::editor;

namespace {
// Verificação dentro de funções que devolvem valor (AE_EXPECT_* encerra com `return;`).
inline bool check(bool condition,const std::string &message) {
  if(!condition) {std::fprintf(stderr,"  FALHA: %s\n",message.c_str());::ae::test::currentTestFailed()=true;}
  return condition;
}
// Chão estático 20×20 e, opcionalmente, uma parede no meio (x=0, z −6..6).
struct NavScene {
  EditorDocument doc;EditorMapScene resources;
  EditorEntityId floor=0,wall=0,surface=0;
  explicit NavScene(bool withWall=true) {
    floor=staticBox("Chão",0,-.5f,0,10,.5f,10);
    if(withWall) wall=staticBox("Parede",0,1,0,.5f,1,6);
    surface=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Superfície");
    auto value=*doc.find(surface);value.components.add(scene::NavSurface::descriptor);
    doc.applyEntityValues(surface,value);
  }
  EditorEntityId staticBox(const char *name,float x,float y,float z,float hx,float hy,float hz) {
    const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);
    auto value=*doc.find(id);value.transform.position[0]=x;value.transform.position[1]=y;value.transform.position[2]=z;
    editPhysicsBody(value);auto *c=editCollider(value);c->halfX=hx;c->halfY=hy;c->halfZ=hz;
    doc.applyEntityValues(id,value);return id;
  }
  scene::NavSurface &surfaceValue(EditorEntity &e) {return scene::navSurface(*e.components.edit(scene::NavSurface::descriptor));}
  navigation::NavMeshData bake() {
    navigation::BakeGeometry geometry;std::string error;
    check(EditorPlayScene::collectNavigationGeometry(doc,resources,surface,geometry,error),error);
    navigation::BakeProgress progress;navigation::NavMeshData data;
    const auto *component=doc.find(surface)->components.find(scene::NavSurface::descriptor);
    check(navigation::bake(geometry,runtime::navigationBakeSettings(scene::navSurface(*component)),progress,data,error)==navigation::BakeStatus::Ok,"bake: "+error);
    data.guid=resources::assetGuidFromSeed("navmesh-test");
    auto value=*doc.find(surface);surfaceValue(value).data=data.guid;doc.applyEntityValues(surface,value);
    return data;
  }
  EditorEntityId agent(const char *name,float x,float z,bool character) {
    const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,name);
    auto value=*doc.find(id);value.transform.position[0]=x;value.transform.position[2]=z;
    if(character) {value.transform.position[1]=.05f;editCharacter(value)->speed=6;}
    value.components.add(scene::NavAgent::descriptor);
    doc.applyEntityValues(id,value);return id;
  }
};
template<class Graph> u64 instanceOf(const Graph &doc,EditorEntityId id,const scene::ComponentType &type) {
  const auto *c=doc.find(id)->components.find(type);return c?c->instanceId():0;
}
scene::ComponentOperationValue call(EditorPlayScene &play,EditorEntityId id,const scene::ComponentType &type,std::string_view method,
                                    std::span<const scene::ComponentOperationValue> args={}) {
  scene::ComponentOperationValue out;
  const auto status=play.invokeMethod(static_cast<runtime::ObjectId>(id),instanceOf(play.document(),id,type),method,args,out);
  check(status==runtime::WorldStatus::Ok,"método "+std::string(method));
  return out;
}
float distanceXZ(const float *a,float x,float z) {return std::hypot(a[0]-x,a[2]-z);}
} // namespace

AE_TEST(navigation_scene_collects_static_colliders_with_modifiers_and_collect_modes) {
  NavScene s;
  navigation::BakeGeometry all;std::string error;
  AE_EXPECT_TRUE(EditorPlayScene::collectNavigationGeometry(s.doc,s.resources,s.surface,all,error),error.c_str());
  AE_EXPECT_EQ(all.triangleCount(),usize{24},"chão e parede, 12 triângulos cada");
  auto wall=*s.doc.find(s.wall);auto *modifier=static_cast<scene::NavModifier *>(wall.components.add(scene::NavModifier::descriptor));
  modifier->mode=scene::NavModifierMode::Ignore;s.doc.applyEntityValues(s.wall,wall);
  navigation::BakeGeometry ignored;
  AE_EXPECT_TRUE(EditorPlayScene::collectNavigationGeometry(s.doc,s.resources,s.surface,ignored,error)&&ignored.triangleCount()==12,"Ignorar tira a parede");
  modifier=static_cast<scene::NavModifier *>(wall.components.edit(scene::NavModifier::descriptor));
  modifier->mode=scene::NavModifierMode::Area;modifier->area=scene::NavArea::Difficult;s.doc.applyEntityValues(s.wall,wall);
  navigation::BakeGeometry marked;
  AE_EXPECT_TRUE(EditorPlayScene::collectNavigationGeometry(s.doc,s.resources,s.surface,marked,error),error.c_str());
  usize difficult=0;for(const auto a:marked.areas) difficult+=a==navigation::AreaDifficult;
  AE_EXPECT_EQ(difficult,usize{12},"área difícil da parede");
  auto surface=*s.doc.find(s.surface);s.surfaceValue(surface).collect=scene::NavCollect::Children;s.doc.applyEntityValues(s.surface,surface);
  navigation::BakeGeometry children;
  AE_EXPECT_TRUE(EditorPlayScene::collectNavigationGeometry(s.doc,s.resources,s.surface,children,error)&&children.triangleCount()==0,"só filhos: nada");
  // Agentes e obstáculos não entram como chão assado.
  s.surfaceValue(surface).collect=scene::NavCollect::All;s.doc.applyEntityValues(s.surface,surface);
  wall.components.add(scene::NavObstacle::descriptor);s.doc.applyEntityValues(s.wall,wall);
  navigation::BakeGeometry withoutObstacle;
  AE_EXPECT_TRUE(EditorPlayScene::collectNavigationGeometry(s.doc,s.resources,s.surface,withoutObstacle,error)&&withoutObstacle.triangleCount()==12,"obstáculo fora do bake");
}

AE_TEST(navigation_scene_character_agent_walks_around_wall_to_destination) {
  NavScene s;const auto data=s.bake();
  const auto actor=s.agent("Agente",-5,0,true);
  EditorPlayScene play;play.navigation().setMeshes(std::span(&data,1));
  AE_EXPECT_TRUE(play.start(s.doc,s.resources),"Play");
  for(int i=0;i<10;++i) AE_EXPECT_TRUE(play.advance(1.0/60.0),"assentar");
  AE_EXPECT_TRUE(call(play,s.surface,scene::NavSurface::descriptor,"is_ready").boolean==1,"superfície carregada");
  AE_EXPECT_TRUE(call(play,s.surface,scene::NavSurface::descriptor,"polygon_count").integer>0,"polígonos");
  const scene::ComponentOperationValue goal[]{scene::ComponentOperationValue::makeVector(5,0,0)};
  AE_EXPECT_TRUE(call(play,actor,scene::NavAgent::descriptor,"set_destination",goal).boolean==1,"destino aceito");
  bool reached=false;float maxZ=0;
  for(int i=0;i<60*10&&!reached;++i) {
    AE_EXPECT_TRUE(play.advance(1.0/60.0),"quadro");
    const auto *p=play.document().find(actor)->transform.position;
    maxZ=std::max(maxZ,std::abs(p[2]));
    reached=distanceXZ(p,5,0)<.35f;
  }
  AE_EXPECT_TRUE(reached,"Personagem chega ao destino");
  AE_EXPECT_TRUE(maxZ>5.5f,"contorna a parede de 12 m");
  for(int i=0;i<30;++i) play.advance(1.0/60.0);
  AE_EXPECT_TRUE(call(play,actor,scene::NavAgent::descriptor,"has_path").boolean==0,"para ao chegar");
  const auto *p=play.document().find(actor)->transform.position;
  AE_EXPECT_TRUE(distanceXZ(p,5,0)<.4f,"fica no destino");
  play.stop();
}

AE_TEST(navigation_scene_pose_agent_chases_target_stops_resumes_and_warps) {
  NavScene s(false);const auto data=s.bake();
  const auto target=s.doc.createEntity(s.doc.root(),EditorEntityKind::Folder,"Alvo");
  auto t=*s.doc.find(target);t.transform.position[0]=6;t.transform.position[2]=6;s.doc.applyEntityValues(target,t);
  const auto actor=s.agent("Seguidor",-6,-6,false);
  auto value=*s.doc.find(actor);
  auto &agent=scene::navAgent(*value.components.edit(scene::NavAgent::descriptor));agent.target=target;agent.stoppingDistance=1;agent.speed=5;
  s.doc.applyEntityValues(actor,value);
  EditorPlayScene play;play.navigation().setMeshes(std::span(&data,1));
  AE_EXPECT_TRUE(play.start(s.doc,s.resources),"Play");
  for(int i=0;i<60*6;++i) play.advance(1.0/60.0);
  const auto *p=play.document().find(actor)->transform.position;
  check(distanceXZ(p,6,6)<1.3f&&distanceXZ(p,6,6)>.6f,"para na distância de parada do alvo: d="+std::to_string(distanceXZ(p,6,6))+" x="+std::to_string(p[0])+" y="+std::to_string(p[1])+" z="+std::to_string(p[2]));
  AE_EXPECT_TRUE(std::abs(p[1])<.2f,"anda sobre o chão");
  AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.rotationDegrees[1]-45)<15,"gira para o movimento");
  // Script assume: Parar segura, Retomar volta ao destino guardado.
  const scene::ComponentOperationValue corner[]{scene::ComponentOperationValue::makeVector(-6,0,6)};
  AE_EXPECT_TRUE(call(play,actor,scene::NavAgent::descriptor,"set_destination",corner).boolean==1,"script substitui o alvo");
  for(int i=0;i<20;++i) play.advance(1.0/60.0);
  call(play,actor,scene::NavAgent::descriptor,"stop");
  const float x=play.document().find(actor)->transform.position[0];
  for(int i=0;i<30;++i) play.advance(1.0/60.0);
  AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.position[0]-x)<.15f,"parado");
  call(play,actor,scene::NavAgent::descriptor,"resume");
  for(int i=0;i<60*5;++i) play.advance(1.0/60.0);
  AE_EXPECT_TRUE(distanceXZ(play.document().find(actor)->transform.position,-6,6)<1.3f,"retoma o destino (distância de parada 1 m)");
  const scene::ComponentOperationValue jump[]{scene::ComponentOperationValue::makeVector(0,0,-8)};
  AE_EXPECT_TRUE(call(play,actor,scene::NavAgent::descriptor,"warp",jump).boolean==1,"teleporta");
  AE_EXPECT_TRUE(distanceXZ(play.document().find(actor)->transform.position,0,-8)<.3f,"no ponto da malha");
  const scene::ComponentOperationValue far[]{scene::ComponentOperationValue::makeVector(0,40,0)};
  AE_EXPECT_TRUE(call(play,actor,scene::NavAgent::descriptor,"set_destination",far).boolean==0,"destino fora da malha recusado");
  AE_EXPECT_EQ(call(play,actor,scene::NavAgent::descriptor,"path_status").integer,i64{3},"estado inválido");
  play.stop();
}

AE_TEST(navigation_scene_obstacle_carves_and_link_crosses_gap) {
  // Corredor 20×4: um obstáculo no meio deixa só uma passagem lateral.
  NavScene s(false);
  auto floor=*s.doc.find(s.floor);auto *c=editCollider(floor);c->halfZ=2;s.doc.applyEntityValues(s.floor,floor);
  const auto data=s.bake();
  const auto obstacle=s.doc.createEntity(s.doc.root(),EditorEntityKind::Folder,"Porta");
  auto o=*s.doc.find(obstacle);o.transform.position[1]=1;
  auto &settings=scene::navObstacle(*o.components.add(scene::NavObstacle::descriptor));settings.size[0]=1;settings.size[1]=2;settings.size[2]=4;
  s.doc.applyEntityValues(obstacle,o);
  EditorPlayScene play;play.navigation().setMeshes(std::span(&data,1));
  AE_EXPECT_TRUE(play.start(s.doc,s.resources),"Play");
  for(int i=0;i<5;++i) play.advance(1.0/60.0);
  AE_EXPECT_TRUE(call(play,obstacle,scene::NavObstacle::descriptor,"is_carving").boolean==1,"recortando");
  const auto *world=play.navigation().surfaceWorld(static_cast<runtime::ObjectId>(s.surface));
  AE_EXPECT_TRUE(world!=nullptr,"malha da superfície");
  std::vector<float> corners;const float a[3]{-8,0,0},b[3]{8,0,0};
  AE_EXPECT_TRUE(world->findPath(a,b,{},corners)!=navigation::PathStatus::Complete,"porta fecha o corredor");
  // Desligar o obstáculo devolve a passagem.
  scene::ComponentOperationValue unused;(void)unused;
  auto &live=play.world();
  const auto handle=live.handle(static_cast<runtime::ObjectId>(obstacle));
  AE_EXPECT_TRUE(live.setActive(handle,false)==runtime::WorldStatus::Ok,"desativa a porta");
  for(int i=0;i<5;++i) play.advance(1.0/60.0);
  AE_EXPECT_TRUE(world->findPath(a,b,{},corners)==navigation::PathStatus::Complete,"corredor aberto");
  play.stop();

  // Duas ilhas e um Link de salto entre elas: o agente atravessa.
  NavScene islands(false);
  auto left=*islands.doc.find(islands.floor);left.transform.position[0]=-5.5f;editCollider(left)->halfX=4.5f;editCollider(left)->halfZ=3;
  islands.doc.applyEntityValues(islands.floor,left);
  islands.staticBox("Direita",6.5f,-.5f,0,3.5f,.5f,3);
  const auto apart=islands.bake();
  const auto link=islands.doc.createEntity(islands.doc.root(),EditorEntityKind::Folder,"Ponte");
  auto l=*islands.doc.find(link);
  auto &bridge=scene::navLink(*l.components.add(scene::NavLink::descriptor));
  bridge.start[0]=-1.8f;bridge.start[2]=0;bridge.end[0]=3.8f;bridge.end[2]=0;bridge.area=scene::NavArea::Jump;
  islands.doc.applyEntityValues(link,l);
  const auto actor=islands.agent("Viajante",-8,0,false);
  EditorPlayScene crossing;crossing.navigation().setMeshes(std::span(&apart,1));
  AE_EXPECT_TRUE(crossing.start(islands.doc,islands.resources),"Play ilhas");
  crossing.advance(1.0/60.0);
  const scene::ComponentOperationValue goal[]{scene::ComponentOperationValue::makeVector(8,0,0)};
  AE_EXPECT_TRUE(call(crossing,actor,scene::NavAgent::descriptor,"set_destination",goal).boolean==1,"destino na outra ilha");
  bool linked=false,reached=false;
  for(int i=0;i<60*12&&!reached;++i) {
    crossing.advance(1.0/60.0);
    linked|=call(crossing,actor,scene::NavAgent::descriptor,"is_on_link").boolean!=0;
    reached=distanceXZ(crossing.document().find(actor)->transform.position,8,0)<.4f;
  }
  AE_EXPECT_TRUE(linked,"atravessou o link");
  AE_EXPECT_TRUE(reached,"chegou do outro lado");
  crossing.stop();
}

AE_TEST(navigation_session_bakes_to_project_asset_undoes_and_reloads) {
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("astra-navmesh-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{root};
  EditorSession session;
  AE_EXPECT_TRUE(session.setProjectDirectory(root.string().c_str()),"projeto");
  auto &doc=session.document();
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Chão");
  auto value=*doc.find(floor);value.transform.position[1]=-.5f;editPhysicsBody(value);auto *c=editCollider(value);c->halfX=c->halfZ=8;c->halfY=.5f;
  doc.applyEntityValues(floor,value);
  const auto surface=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Pátio");
  value=*doc.find(surface);const u64 instance=value.components.add(scene::NavSurface::descriptor)->instanceId();
  doc.applyEntityValues(surface,value);
  session.history().clear();
  AE_EXPECT_TRUE(session.bakeNavigation(surface,instance),session.screen().status.c_str());
  session.waitNavigationBake();
  const auto guid=scene::navSurface(*doc.find(surface)->components.findInstance(instance)).data;
  AE_EXPECT_TRUE(guid.valid(),"recurso atribuído");
  AE_EXPECT_EQ(session.history().undoDepth(),1u,"atribuição é um passo");
  AE_EXPECT_TRUE(fs::exists(root/"Navegação"/"Pátio.navmesh"),"arquivo no projeto");
  AE_EXPECT_EQ(session.navMeshes().size(),usize{1},"malha carregada");
  // Assar de novo mantém o recurso e a identidade.
  AE_EXPECT_TRUE(session.bakeNavigation(surface,instance),"segundo bake");
  session.waitNavigationBake();
  AE_EXPECT_TRUE(scene::navSurface(*doc.find(surface)->components.findInstance(instance)).data==guid,"mesma identidade");
  AE_EXPECT_EQ(session.navMeshes().size(),usize{1},"sem duplicar");
  AE_EXPECT_TRUE(session.history().undo(doc),"desfaz a atribuição");
  AE_EXPECT_TRUE(!scene::navSurface(*doc.find(surface)->components.findInstance(instance)).data.valid(),"sem recurso após desfazer");
  AE_EXPECT_TRUE(session.history().redo(doc),"refaz");
  // Reabrir o registro carrega o .navmesh do disco.
  const auto registry=session.serializeAssets();
  EditorSession reopened;
  AE_EXPECT_TRUE(reopened.setProjectDirectory(root.string().c_str())&&reopened.loadAssets(registry),"reabre");
  AE_EXPECT_EQ(reopened.navMeshes().size(),usize{1},"recarregada do disco");
  AE_EXPECT_TRUE(reopened.navMeshes()[0].guid==guid&&reopened.navMeshes()[0].stats.polygons>0,"mesma malha");
}
