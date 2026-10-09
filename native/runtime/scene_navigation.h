#pragma once
// Navegação no mundo de Play (bloco J).
//
// Ordem no quadro: depois de Update (scripts pedem destinos) e antes do passo
// de física. Cada Superfície ativa carrega o recurso .navmesh do projeto num
// NavWorld; Links e Obstáculos entram como conexões e recortes; cada Agente
// vira um agente da multidão do Detour, que devolve a velocidade desejada.
// A velocidade vai para quem move o objeto: Personagem e Motor dinâmico pela
// posse de controle (fonte IA, a de menor prioridade), corpo móvel pela
// velocidade linear e, sem física, a própria pose. O corredor do agente segue
// a posição real do objeto a cada quadro.
#include "navigation/navigation_mesh.h"
#include "runtime/component_operations.h"
#include "runtime/game_world.h"
#include "scene/navigation.h"

#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ae::runtime {
class ScenePhysics;

// Geometria do bake a partir de um mundo com física montada: colisores
// estáticos coletados pela Superfície, com Modificadores aplicados.
bool collectNavigationGeometry(const GameWorld &world,const ScenePhysics &physics,ObjectId surface,
                               navigation::BakeGeometry &out,std::string &error);
navigation::BakeSettings navigationBakeSettings(const scene::NavSurface &surface);

class SceneNavigation final {
public:
  enum class Drive : u32 {None=0,Character=1,DynamicMotor=2,Body=3,Pose=4};
  struct AgentView {
    ObjectId id=kInvalidObject;
    Drive drive=Drive::None;
    bool hasPath=false,onLink=false;
    navigation::PathStatus status=navigation::PathStatus::None;
    float position[3]{},target[3]{};
    std::vector<float> corners;
  };
  using Key=std::pair<ObjectId,u64>;

  SceneNavigation();
  ~SceneNavigation();
  void setMeshes(std::span<const navigation::NavMeshData> meshes);
  void setEvents(ComponentEventQueue *events) noexcept {events_=events;}
  void setPhysics(ScenePhysics *physics) noexcept {physics_=physics;}
  void reset();

  bool advance(GameWorld &world,float seconds);
  WorldStatus command(GameWorld &world,ComponentHandle handle,std::string_view method,
                      std::span<const scene::ComponentOperationValue> arguments,scene::ComponentOperationValue &out);

  // Inspeção (overlay do editor e testes).
  const navigation::NavWorld *surfaceWorld(ObjectId surface) const;
  std::vector<AgentView> agents() const;
  const std::string &diagnostic() const noexcept {return diagnostic_;}

private:
  struct Surface {
    Key key{};
    resources::AssetGuid guid{};
    navigation::NavWorld world;
    std::vector<navigation::OffMeshLink> links;
    u32 polygons=0;
  };
  struct Agent {
    Key surface{};
    i32 crowd=-1;
    Drive drive=Drive::None;
    navigation::AgentSettings settings{};
    bool hasDestination=false,stopped=false,scriptOverride=false,reached=false,failed=false,onLink=false;
    float destination[3]{},chasedAt[3]{};
    bool chasing=false;
  };
  struct Obstacle {
    std::vector<std::pair<Key,u32>> refs;   // superfície, recorte
    float pose[4]{};                         // centro x y z + guinada
    float still=0;
    bool placed=false,moving=false;
    u64 shape=0;                             // assinatura de forma e tamanho
  };
  void sync(GameWorld &w);
  Surface *surfaceFor(GameWorld &w,const scene::NavAgent &settings,const float position[3]);
  Surface *surfaceOf(ObjectId id);
  void unbind(Agent &agent);
  void driveAgent(GameWorld &w,const Key &key,Agent &agent,const scene::NavAgent &settings,float seconds);
  void updateLinks(GameWorld &w);
  void updateObstacles(GameWorld &w,float seconds);
  void emit(GameWorld &w,const Key &key,std::string_view event);

  std::vector<std::shared_ptr<const navigation::NavMeshData>> meshes_;
  std::map<Key,std::unique_ptr<Surface>> surfaces_;   // NavWorld não é móvel
  std::map<Key,Agent> agents_;
  std::map<Key,Obstacle> obstacles_;
  std::vector<ObjectId> ids_;
  ComponentEventQueue *events_=nullptr;
  ScenePhysics *physics_=nullptr;
  u64 revision_=~u64{0},world_=0;
  std::string diagnostic_;
};

} // namespace ae::runtime
