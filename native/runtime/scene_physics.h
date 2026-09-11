// A ponte entre o mundo de execução e o Jolt.
//
// Era `editor/editor_scene_physics.h` e lia o documento do editor. Agora lê o
// `GameWorld`: o mesmo adaptador serve a um consumidor sem editor, e é ele que
// declara ao mundo quem publica a pose de cada objeto (`TransformAuthority`),
// de modo que um script que tente mover um corpo receba uma recusa explicada em
// vez de uma posição que volta no passo seguinte.
//
// Uma thread só: todos os handles de corpo pertencem a ela.
#pragma once
#include "runtime/game_world.h"
#include "runtime/scene_components.h"
#include "physics/jolt_bridge.h"
#include "physics/character_motor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ae::runtime {

class ScenePhysics final {
public:
  ~ScenePhysics() { stop(); }
  ScenePhysics() = default;
  ScenePhysics(const ScenePhysics &) = delete;
  ScenePhysics &operator=(const ScenePhysics &) = delete;

  // Monta corpos, juntas e personagens a partir do grafo do mundo e registra as
  // autoridades de pose. Falha deixa o mundo sem autoridades e o erro em error().
  bool start(GameWorld &gameWorld);
  bool advance(double elapsed, GameWorld &world, bool (*beforeStep)(void *, float) = nullptr,
               void *context = nullptr, bool (*trigger)(void *, ObjectId, ObjectId, u32) = nullptr);
  bool applyBodyForce(ObjectId id, const float *value, u32 kind);
  bool getBodyVelocity(ObjectId id, float *out) const;
  bool setBodyVelocity(ObjectId id, const float *velocity);
  bool moveKinematic(ObjectId id, const float *pose);
  void stop();
  bool setCharacterMove(ObjectId id, float right, float forward, float yaw);
  bool jumpCharacter(ObjectId id);
  // Solta corpo, personagem e mapeamento de um objeto removido no ponto seguro.
  // Juntas ligadas a ele deixam de existir junto com o corpo no Jolt.
  void releaseObject(ObjectId id);
  const std::string &error() const { return error_; }
  u32 bodyCount() const { return static_cast<u32>(bindings_.size()); }
  u32 jointCount() const { return jointCount_; }
  AetherPhysicsWorld *world() const noexcept { return world_; }
  // Objeto dono de um corpo nativo, para mapear resultados de consulta de volta
  // à identidade do mundo. Zero quando o corpo não pertence a esta cena.
  ObjectId objectForBody(AetherBodyHandle body) const;

private:
  bool synchronizePoses(GameWorld &world);
  struct Binding { ObjectId id; AetherBodyHandle body; float scale[3]; bool moving; };
  AetherPhysicsWorld *world_ = nullptr;
  std::vector<Binding> bindings_;
  std::unordered_map<AetherBodyHandle, ObjectId> objects_;
  std::vector<AetherTriggerEvent> events_;
  struct CharacterBinding {
    ObjectId id;
    std::unique_ptr<physics::CharacterMotor> motor;
    float world[16];
    float eyeHeight;
    float jumpSpeed = 0;
    float right = 0, forward = 0, yaw = 0;
  };
  std::vector<CharacterBinding> characters_;
  u32 jointCount_ = 0;
  double accumulated_ = 0;
  std::string error_;
};

} // namespace ae::runtime
