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
      float offset[3]{follow.offset[0],follow.offset[1],follow.offset[2]};
      if(follow.orbit) {
        // Transform a direction, never a point: the camera's current translation
        // must not feed back into its next orbit position. Remove world scale.
        Transform orientation=cameraPose;
        for(u32 axis=0;axis<3;++axis){orientation.position[axis]=0;orientation.scale[axis]=1;}
        float matrix[16];transformMatrix(orientation,matrix);
        for(u32 axis=0;axis<3;++axis)offset[axis]=matrix[axis]*follow.offset[0]+matrix[4+axis]*follow.offset[1]+matrix[8+axis]*follow.offset[2];
      }
      for(u32 axis=0;axis<3;++axis) {
        const float desired=targetPose.position[axis]+offset[axis]+(follow.orbit&&axis==1?follow.pivotHeight:0);
        cameraPose.position[axis]+=(desired-cameraPose.position[axis])*alpha;
      }
      // Own translation only. Publishing a full world TRS decomposes yaw into
      // the canonical [-90,90] Euler branch (pitch/roll flip at the poles).
      // CameraLook must retain its authored local angles through a full orbit.
      float desired[16],parent[16];Transform translated;
      transformMatrix(cameraPose,desired);
      if(!parentWorldMatrix(world.poseGraph(),id,parent)||!localTransformForWorld(desired,parent,translated))return false;
      auto local=world.find(camera)->transform;
      std::copy(translated.position,translated.position+3,local.position);
      if(world.setLocalTransform(camera,local)!=WorldStatus::Ok) return false;
    }
    return true;
  }
private:
  std::vector<ObjectId> cameras_;
  u64 knownRevision_=0;
};
}
