#include "harness.h"
#include "navigation/navigation_mesh.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

using namespace ae;
using namespace ae::navigation;

namespace {
// Verificação dentro de funções que devolvem valor (AE_EXPECT_* encerra com `return;`).
inline bool check(bool condition,const std::string &message) {
  if(!condition) {std::fprintf(stderr,"  FALHA: %s\n",message.c_str());::ae::test::currentTestFailed()=true;}
  return condition;
}
// Caixa sólida (12 triângulos) na geometria do bake.
void box(BakeGeometry &g,float minX,float minY,float minZ,float maxX,float maxY,float maxZ,u8 area=AreaWalkable) {
  const u32 base=static_cast<u32>(g.vertices.size()/3);
  for(u32 i=0;i<8;++i) g.vertices.insert(g.vertices.end(),{i&1?maxX:minX,i&2?maxY:minY,i&4?maxZ:minZ});
  constexpr u32 faces[12][3]{{0,2,1},{1,2,3},{4,5,6},{5,7,6},{0,1,4},{1,5,4},{2,6,3},{3,6,7},{0,4,2},{2,4,6},{1,3,5},{3,7,5}};
  for(const auto &f:faces) {g.indices.insert(g.indices.end(),{base+f[0],base+f[1],base+f[2]});g.areas.push_back(area);}
}
float pathLength(const float from[3],const std::vector<float> &corners) {
  float total=0;const float *p=from;
  for(usize i=0;i<corners.size();i+=3) {
    const float dx=corners[i]-p[0],dy=corners[i+1]-p[1],dz=corners[i+2]-p[2];
    total+=std::sqrt(dx*dx+dy*dy+dz*dz);p=&corners[i];
  }
  return total;
}
NavMeshData bakeOrFail(const BakeGeometry &g,BakeSettings s={}) {
  BakeProgress progress;NavMeshData data;std::string error;
  const auto status=bake(g,s,progress,data,error);
  check(status==BakeStatus::Ok,"bake: "+error);
  return data;
}
} // namespace

AE_TEST(navigation_bake_floor_with_wall_routes_around_and_round_trips) {
  BakeGeometry g;box(g,-10,-1,-10,10,0,10);box(g,-.5f,0,-6,.5f,2,6);
  BakeSettings s;s.tileSize=32;
  const auto data=bakeOrFail(g,s);
  AE_EXPECT_TRUE(data.tilesX>1&&data.tilesZ>1,"vários tiles");
  AE_EXPECT_TRUE(data.stats.polygons>0&&data.stats.area>200&&data.stats.area<400,"área caminhável do chão menos a parede");
  AE_EXPECT_EQ(data.sourceHash,bakeHash(g,s),"hash da fonte");
  NavWorld world;std::string error;
  AE_EXPECT_TRUE(world.load(data,{},8,error),error.c_str());
  const float from[3]{-5,0,0},to[3]{5,0,0};std::vector<float> corners;
  AE_EXPECT_TRUE(world.findPath(from,to,{},corners)==PathStatus::Complete,"caminho completo");
  AE_EXPECT_TRUE(pathLength(from,corners)>12.5f,"contorna a parede de 12 m");
  // Recurso: ida e volta exata, corrupção e versão recusadas.
  auto bytes=data.serialize();NavMeshData back;
  AE_EXPECT_TRUE(NavMeshData::deserialize(bytes,back,&error),error.c_str());
  AE_EXPECT_EQ(back.serialize(),bytes,"ida e volta byte a byte");
  auto broken=bytes;broken[bytes.size()/2]^=0x5a;
  AE_EXPECT_TRUE(!NavMeshData::deserialize(broken,back,&error)&&error.find("corrompida")!=std::string::npos,"soma recusa corrupção");
  AE_EXPECT_TRUE(!NavMeshData::deserialize(bytes.substr(0,bytes.size()-9),back,&error),"truncado recusado");
}

AE_TEST(navigation_bake_cancels_reports_empty_and_marks_areas) {
  BakeGeometry g;box(g,-40,-1,-40,40,0,40);
  BakeProgress progress;progress.cancel=true;NavMeshData data;std::string error;
  AE_EXPECT_TRUE(bake(g,{},progress,data,error)==BakeStatus::Cancelled,"cancelado antes do primeiro tile");
  BakeGeometry empty;progress.cancel=false;
  AE_EXPECT_TRUE(bake(empty,{},progress,data,error)==BakeStatus::Empty,"sem geometria");
  // Faixa "não caminhável" no meio separa as metades; faixa difícil encarece.
  BakeGeometry split;box(split,-10,-1,-10,10,0,10);box(split,-1,0,-10,1,.05f,10,AreaNotWalkable);
  const auto cut=bakeOrFail(split);
  NavWorld world;AE_EXPECT_TRUE(world.load(cut,{},4,error),error.c_str());
  const float a[3]{-5,0,0},b[3]{5,0,0};std::vector<float> corners;
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)!=PathStatus::Complete,"faixa não caminhável separa");
  BakeGeometry mud;box(mud,-10,-1,-10,10,0,10);box(mud,-2,0,-3,2,.05f,3,AreaDifficult);
  const auto costly=bakeOrFail(mud);
  AE_EXPECT_TRUE(world.load(costly,{},4,error),error.c_str());
  const float c[3]{-6,0,0},d[3]{6,0,0};
  AgentFilter cheap;cheap.difficultCost=1;AgentFilter expensive;expensive.difficultCost=20;
  AE_EXPECT_TRUE(world.findPath(c,d,cheap,corners)==PathStatus::Complete,"atravessa a lama");
  const float direct=pathLength(c,corners);
  AE_EXPECT_TRUE(world.findPath(c,d,expensive,corners)==PathStatus::Complete&&pathLength(c,corners)>direct+.5f,"custo alto contorna a lama");
  AgentFilter dry;dry.include=FlagWalk;
  AE_EXPECT_TRUE(world.findPath(c,d,dry,corners)==PathStatus::Complete&&pathLength(c,corners)>direct+.5f,"máscara sem Difícil desvia");
}

AE_TEST(navigation_obstacle_carves_and_link_bridges_a_gap) {
  BakeGeometry g;box(g,-10,-1,-3,10,0,3);
  const auto data=bakeOrFail(g);
  NavWorld world;std::string error;AE_EXPECT_TRUE(world.load(data,{},4,error),error.c_str());
  const float a[3]{-8,0,0},b[3]{8,0,0};std::vector<float> corners;
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)==PathStatus::Complete,"corredor livre");
  const float freeLength=pathLength(a,corners);
  const float center[3]{0,1,0},half[3]{.5f,1,2.2f};
  const auto obstacle=world.addBoxObstacle(center,half,0);
  AE_EXPECT_TRUE(obstacle!=0,"obstáculo aceito");
  bool upToDate=false;for(int i=0;i<16&&!upToDate;++i) AE_EXPECT_TRUE(world.updateObstacles(upToDate),"recorte");
  AE_EXPECT_TRUE(upToDate,"recorte concluído");
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)==PathStatus::Complete&&pathLength(a,corners)>freeLength+.3f,"desvia do recorte");
  AE_EXPECT_TRUE(world.removeObstacle(obstacle),"remove");
  upToDate=false;for(int i=0;i<16&&!upToDate;++i) world.updateObstacles(upToDate);
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)==PathStatus::Complete&&std::abs(pathLength(a,corners)-freeLength)<.05f,"recorte desfeito");

  BakeGeometry islands;box(islands,-10,-1,-3,-1,0,3);box(islands,3,-1,-3,10,0,3);
  const auto apart=bakeOrFail(islands);
  AE_EXPECT_TRUE(world.load(apart,{},4,error),error.c_str());
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)!=PathStatus::Complete,"ilhas separadas");
  OffMeshLink link;const float s[3]{-1.8f,0,0},e[3]{3.8f,0,0};
  for(u32 k=0;k<3;++k) {link.start[k]=s[k];link.end[k]=e[k];}
  link.radius=.6f;link.area=AreaJump;link.id=7;
  const OffMeshLink links[]{link};
  AE_EXPECT_TRUE(world.setLinks(links),"link reconstrói tiles");
  AE_EXPECT_TRUE(world.findPath(a,b,{},corners)==PathStatus::Complete,"link liga as ilhas");
  AgentFilter noJump;noJump.include=FlagWalk;
  AE_EXPECT_TRUE(world.findPath(a,b,noJump,corners)!=PathStatus::Complete,"máscara sem Salto não usa o link");
  AE_EXPECT_TRUE(world.setLinks({})&&world.findPath(a,b,{},corners)!=PathStatus::Complete,"remover link separa de novo");
}

AE_TEST(navigation_crowd_agent_reaches_destination_and_follows_sync) {
  BakeGeometry g;box(g,-10,-1,-10,10,0,10);box(g,-.5f,0,-6,.5f,2,6);
  const auto data=bakeOrFail(g);
  NavWorld world;std::string error;AE_EXPECT_TRUE(world.load(data,{},4,error),error.c_str());
  AgentSettings settings;settings.speed=4;
  const float start[3]{-5,0,0},goal[3]{5,0,0};
  const auto agent=world.addAgent(start,settings);
  AE_EXPECT_TRUE(agent>=0,"agente na multidão");
  AE_EXPECT_TRUE(world.setDestination(agent,goal),"destino aceito");
  AgentState state;
  for(int i=0;i<60*12;++i) world.advance(1.f/60);
  AE_EXPECT_TRUE(world.agentState(agent,state)&&state.hasPath&&state.status==PathStatus::Complete,"caminho completo");
  AE_EXPECT_TRUE(world.remainingDistance(agent)<.3f,"chegou");
  AE_EXPECT_TRUE(std::abs(state.position[0]-5)<.4f&&std::abs(state.position[2])<.4f,"posição no destino");
  // A física moveu o objeto: o corredor acompanha; teletransporte recoloca.
  const float pushed[3]{4,0,3};
  AE_EXPECT_TRUE(world.syncAgent(agent,pushed)&&world.agentState(agent,state)&&std::abs(state.position[2]-3)<.3f,"corredor segue");
  AE_EXPECT_TRUE(world.stopAgent(agent)&&world.agentState(agent,state)&&!state.hasPath,"parar limpa o caminho");
  const float unreachable[3]{0,50,0};
  AE_EXPECT_TRUE(!world.setDestination(agent,unreachable),"destino fora da malha recusado");
}
