#pragma once
#include "editor/editor_document.h"
#include "physics/jolt_bridge.h"
#include "physics/character_motor.h"
#include <unordered_map>
namespace ae::editor {
// Authoring adapter to one Jolt scene world. Owns all body handles on one thread.
class EditorScenePhysics final {
public:
  ~EditorScenePhysics(){stop();}
  EditorScenePhysics()=default;
  EditorScenePhysics(const EditorScenePhysics&)=delete;
  EditorScenePhysics &operator=(const EditorScenePhysics&)=delete;
  bool start(const EditorDocument &document);
  bool advance(double elapsed,EditorDocument &document,bool (*beforeStep)(void *,float)=nullptr,void *context=nullptr, bool (*trigger)(void *,EditorEntityId,EditorEntityId,u32)=nullptr);
  bool applyBodyForce(EditorEntityId id,const float *value,u32 kind);
  bool getBodyVelocity(EditorEntityId id,float *out) const;
  bool setBodyVelocity(EditorEntityId id,const float *velocity);
  bool moveKinematic(EditorEntityId id,const float *pose);
  void stop();
  bool setCharacterMove(EditorEntityId id,float right,float forward,float yaw);
  bool jumpCharacter(EditorEntityId id);
  const std::string &error() const {return error_;}
  u32 bodyCount() const {return static_cast<u32>(bindings_.size());}
  u32 jointCount() const {return jointCount_;}
private:
  bool synchronizePoses(EditorDocument &document);
  struct Binding {EditorEntityId id;AetherBodyHandle body;float scale[3];bool moving;};
  AetherPhysicsWorld *world_=nullptr;
  std::vector<Binding> bindings_;
  std::unordered_map<AetherBodyHandle,EditorEntityId> objects_;
  std::vector<AetherTriggerEvent> events_;
  struct CharacterBinding {
    EditorEntityId id;
    std::unique_ptr<physics::CharacterMotor> motor;
    float world[16];
    float eyeHeight;
    float jumpSpeed=0;
    float right=0,forward=0,yaw=0;
  };
  std::vector<CharacterBinding> characters_;
  u32 jointCount_=0;
  double accumulated_=0;
  std::string error_;
};
}
