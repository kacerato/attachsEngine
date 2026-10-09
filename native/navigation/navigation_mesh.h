// Navegação (bloco J, F067–F070): bake, recurso e mundo de navegação.
//
// O bake (Recast) transforma triângulos já coletados da cena em camadas de
// tile do DetourTileCache. O recurso .navmesh guarda essas camadas, não o
// polígono final: em execução o TileCache monta a malha do Detour a partir
// delas, e é o mesmo caminho que recorta os obstáculos e acrescenta os links.
// Nada aqui conhece cena, editor ou componente — quem coleta a geometria e
// quem move objetos são runtime/scene_navigation e editor/editor_navigation.
//
// Referências: Unity AI Navigation 2.0 (NavMeshSurface, NavMeshAgent,
// NavMeshObstacle, NavMeshLink, NavMeshModifier)
// https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/index.html
// Godot 4.5 NavigationRegion3D/NavigationAgent3D
// https://docs.godotengine.org/en/4.5/tutorials/navigation/navigation_introduction_3d.html
// Recast/Detour 1.6.0 https://github.com/recastnavigation/recastnavigation/tree/v1.6.0
#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::navigation {

// Áreas do Detour (0..63). 63 é a área caminhável do Recast; as outras são as
// áreas embutidas da Astra. Valores gravados no recurso: nunca renumerar.
enum Area : u8 {AreaNotWalkable=0,AreaJump=2,AreaDifficult=3,AreaWalkable=63};
// Bandeiras de polígono derivadas da área: o filtro do agente as inclui/exclui.
enum PolyFlags : u16 {FlagWalk=1,FlagJump=2,FlagDifficult=4,FlagAll=0xffff};
inline u16 areaFlags(u8 area) {return area==AreaJump?FlagJump:area==AreaDifficult?FlagDifficult:area==AreaWalkable?FlagWalk:0;}

struct BakeSettings {
  float agentRadius=.5f,agentHeight=2,agentMaxClimb=.4f,agentMaxSlope=45;
  float cellSize=.2f,cellHeight=.1f;
  float minRegionArea=2;          // m² — ilhas menores somem
  float edgeMaxLength=12,edgeMaxError=1.3f;
  float detailSampleDistance=6,detailSampleMaxError=1;
  u32 tileSize=64;                // células por lado
  bool valid() const;
};

// Triângulos em espaço de mundo, com a área por triângulo. AreaNotWalkable
// força o triângulo a bloquear sem ser chão (Modificador "Não caminhável").
struct BakeGeometry {
  std::vector<float> vertices;    // xyz
  std::vector<u32> indices;       // 3 por triângulo
  std::vector<u8> areas;          // 1 por triângulo
  bool hasVolume=false;           // recorte opcional (Coletar: Volume)
  float volumeMin[3]{},volumeMax[3]{};
  u64 hash() const;
  usize triangleCount() const {return indices.size()/3;}
};

struct BakeProgress {
  std::atomic<u32> done{0},total{0};
  std::atomic<bool> cancel{false};
};

enum class BakeStatus : u32 {Ok=0,Cancelled=1,Empty=2,Failed=3};

struct NavMeshStats {u32 tiles=0,layers=0,polygons=0,vertices=0;float area=0;};

// Recurso .navmesh. Versão do formato separada da versão do componente.
struct NavMeshData {
  static constexpr u32 FormatVersion=1;
  static constexpr usize MaximumBytes=256u*1024u*1024u;
  resources::AssetGuid guid{};
  std::string name;
  BakeSettings settings{};
  float boundsMin[3]{},boundsMax[3]{};
  u32 tilesX=0,tilesZ=0;
  u64 sourceHash=0;               // hash da geometria e das configurações do bake
  float bakeSeconds=0;
  NavMeshStats stats{};
  std::vector<std::vector<u8>> layers;
  std::string serialize() const;
  static bool deserialize(std::string_view bytes,NavMeshData &out,std::string *error=nullptr);
  bool valid() const;
};
u64 bakeHash(const BakeGeometry &geometry,const BakeSettings &settings);

// Pode rodar fora da thread principal: lê só `geometry` e `settings`.
BakeStatus bake(const BakeGeometry &geometry,const BakeSettings &settings,BakeProgress &progress,
                NavMeshData &out,std::string &error);

struct OffMeshLink {
  float start[3]{},end[3]{};
  float radius=.5f;
  bool bidirectional=true;
  u8 area=AreaWalkable;
  u32 id=0;
};

// Custos e bandeiras de um agente. Áreas fora da máscara não são percorridas.
struct AgentFilter {
  u16 include=FlagWalk|FlagJump|FlagDifficult;
  float jumpCost=1,difficultCost=4;
  bool operator==(const AgentFilter &) const=default;
};

struct AgentSettings {
  float radius=.5f,height=2,speed=3.5f,acceleration=8;
  u32 avoidance=3;                // 0 sem desvio, 1..4 qualidade
  float separation=2;
  AgentFilter filter{};
};

enum class PathStatus : u32 {None=0,Complete=1,Partial=2,Invalid=3};

struct AgentState {
  bool valid=false,onLink=false,hasPath=false,pending=false;
  PathStatus status=PathStatus::None;
  float position[3]{},velocity[3]{},desiredVelocity[3]{},target[3]{};
  u32 corners=0;
  float cornerPoints[4*3]{};
};

// Malha de navegação em execução: Detour + TileCache + consulta + multidão.
class NavWorld final {
public:
  NavWorld();
  ~NavWorld();
  NavWorld(const NavWorld &)=delete;
  NavWorld &operator=(const NavWorld &)=delete;

  bool load(const NavMeshData &data,std::span<const OffMeshLink> links,u32 maxAgents,std::string &error);
  void clear();
  bool loaded() const;
  const NavMeshData *data() const {return data_.get();}
  // Links mudaram: reconstrói só os tiles do início de cada link antigo e novo.
  bool setLinks(std::span<const OffMeshLink> links);

  // Obstáculos recortados no TileCache. Devolvem 0 quando a fila está cheia
  // (tente de novo no próximo quadro).
  u32 addCylinderObstacle(const float base[3],float radius,float height);
  u32 addBoxObstacle(const float center[3],const float halfExtents[3],float yawRadians);
  bool removeObstacle(u32 obstacle);
  // Processa recortes pendentes; `upToDate` diz se a malha já reflete tudo.
  bool updateObstacles(bool &upToDate);

  bool nearest(const float point[3],const float extents[3],float out[3]) const;
  // Caminho com cantos de visada (Detour findStraightPath). Devolve o status.
  PathStatus findPath(const float from[3],const float to[3],const AgentFilter &filter,std::vector<float> &corners) const;
  bool raycast(const float from[3],const float to[3],const AgentFilter &filter,float &fraction,float hit[3]) const;

  i32 addAgent(const float position[3],const AgentSettings &settings);
  bool updateAgent(i32 agent,const AgentSettings &settings);
  void removeAgent(i32 agent);
  bool setDestination(i32 agent,const float target[3]);
  // Para no lugar: sem destino e sem velocidade.
  bool stopAgent(i32 agent);
  // Velocidade máxima deste quadro (frenagem antes da distância de parada).
  bool setAgentSpeed(i32 agent,float speed);
  // Posição real do objeto (física): move o corredor do agente até ela.
  bool syncAgent(i32 agent,const float position[3]);
  bool agentState(i32 agent,AgentState &out) const;
  float remainingDistance(i32 agent) const;
  void advance(float seconds);

  // Polígonos para desenho: triângulos da malha detalhada em mundo.
  void triangles(std::vector<float> &vertices,std::vector<u8> &areas) const;
  void edges(std::vector<float> &segments) const;
  NavMeshStats stats() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::unique_ptr<NavMeshData> data_;
};

} // namespace ae::navigation
