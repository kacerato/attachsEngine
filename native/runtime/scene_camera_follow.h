#pragma once
#include "runtime/game_world.h"
#include "scene/camera_follow.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ae::runtime {
class SceneCameraFollow final {
public:
  void reset() {cameras_.clear();knownRevision_=0;}
  bool advance(GameWorld &world,double delta) {
    if(!world.running()||!std::isfinite(delta)||delta<0||delta>.25) return false;
    if(knownRevision_!=world.structuralRevision()) {
      cameras_.clear();std::vector<ObjectId> objects;
      world.graph().collectSubtree(world.graph().root(),objects);
      for(const auto id:objects) {
        const auto *object=world.graph().find(id);
        if(object && object->components.find(scene::CameraFollow::descriptor)) cameras_.push_back(id);
      }
      knownRevision_=world.structuralRevision();
    }
    for(const auto id:cameras_) {
      const auto camera=world.handle(id);
      if(!world.activeInHierarchy(camera) || world.authorityOf(camera)!=TransformAuthority::Free) continue;
      const auto *component=world.readComponent(world.findComponent(camera,"astra.camera.follow"));
      if(!component) continue;
      const auto follow=*static_cast<const scene::CameraFollow *>(component);
      if(!follow.enabled||!follow.target||follow.target==id) continue;
      // A path follower diagnoses this competing authoring configuration.
      // Neither driver may overwrite the other's pose in the same frame.
      if(const auto *object=world.graph().find(id)) {
        bool pathWriter=false;
        for(usize k=0;k<object->components.size();++k) {
          const auto *value=object->components.at(k);
          if(value->type().id!="astra.path.follow") continue;
          bool enabled=true;
          for(const auto &property:value->type().booleans) if(property.id=="enabled") enabled=property.read(*value);
          pathWriter|=enabled;
        }
        if(pathWriter) continue;
      }
      if(world.graph().isDescendantOf(static_cast<ObjectId>(follow.target),id)) return false;
      const auto target=world.handle(static_cast<ObjectId>(follow.target));
      if(!world.activeInHierarchy(target)) continue;
      Transform cameraPose{},targetPose{};
      if(world.worldTransform(camera,cameraPose)!=WorldStatus::Ok ||
         world.worldTransform(target,targetPose)!=WorldStatus::Ok) continue;
      const float alpha=follow.dampingSeconds==0?1.0f:
        static_cast<float>(1.0-std::exp(-delta/static_cast<double>(follow.dampingSeconds)));
      for(u32 axis=0;axis<3;++axis) {
        const float desired=targetPose.position[axis]+follow.offset[axis];
        cameraPose.position[axis]+=(desired-cameraPose.position[axis])*alpha;
      }
      if(world.setWorldTransform(camera,cameraPose)!=WorldStatus::Ok) return false;
    }
    return true;
  }
private:
  std::vector<ObjectId> cameras_;
  u64 knownRevision_=0;
};
}
