#pragma once
#include "runtime/object_activation_connection.h"
#include "scene/physics_event_connection.h"
#include "scene/physics2d_event_connection.h"
#include <string>

namespace ae::runtime {
// Typed authoring connections consume the already-dispatched 3D physics events.
// No delegate, retained object pointer, or second collision world is created.
class ScenePhysicsConnections final {
public:
  void reset() {diagnostic_.clear();deliveries_=0;}
  const std::string &diagnostic() const noexcept {return diagnostic_;}
  u64 deliveries() const noexcept {return deliveries_;}
  void trigger(GameWorld &world,ObjectId source,ObjectId other,u32 phase) {
    if(phase<=2)deliver<scene::PhysicsEventConnection3D>(world,source,other,phase);
  }
  void contact(GameWorld &world,ObjectId first,ObjectId second,u32 phase) {
    if(phase>2)return;
    deliver<scene::PhysicsEventConnection3D>(world,first,second,phase+3);deliver<scene::PhysicsEventConnection3D>(world,second,first,phase+3);
  }
  void event2D(GameWorld &world,ObjectId first,ObjectId second,u32 phase,bool sensor) {
    if(phase>2)return;
    deliver<scene::PhysicsEventConnection2D>(world,first,second,phase+(sensor?0:3));
    if(!sensor)deliver<scene::PhysicsEventConnection2D>(world,second,first,phase+3);
  }
private:
  template<class Connection> void deliver(GameWorld &world,ObjectId source,ObjectId other,u32 event) {
    const auto handle=world.handle(source);
    if(!world.activeInHierarchy(handle))return;
    const auto count=world.componentCount(handle);
    for(u32 i=0;i<count;++i) {
      if(!world.activeInHierarchy(handle))break;
      const auto component=world.componentAt(handle,i);const auto *value=world.readComponent(component);
      if(!value||&value->type()!=&Connection::descriptor)continue;
      // Copy before setActive invalidates consumers; never retain values across mutation.
      const auto connection=*static_cast<const Connection*>(value);
      if(!connection.enabled||connection.action==0||connection.event!=event||
         (connection.otherFilter&&connection.otherFilter!=other))continue;
      const auto status=applyObjectActivationConnection(world,static_cast<ObjectId>(connection.receiver),connection.action);
      if(status==WorldStatus::Ok){++deliveries_;diagnostic_.clear();}
      else diagnostic_="Conexão física "+std::to_string(connection.instanceId())+" no objeto "+std::to_string(source)+
        ": receptor "+std::to_string(connection.receiver)+" indisponível (status "+std::to_string(static_cast<u32>(status))+")";
    }
  }
  std::string diagnostic_;
  u64 deliveries_=0;
};
}
