#pragma once
#include "resources/asset_registry.h"
#include <array>
#include <functional>

namespace ae::resources {
// A control rig addresses ordinary scene transforms. Skin joint order and
// inverse bind matrices remain owned by the imported deformation resource.
struct RigTransform {
  std::array<float,3> position{0,0,0};
  std::array<float,4> rotation{0,0,0,1};
  std::array<float,3> scale{1,1,1};
};
enum RigChannels : u8 { RigPosition=1,RigRotation=2,RigScale=4,RigAll=7 };
enum class RigControlSpace : u8 { LocalReference,RigWorld };
enum class RigLimitKind : u8 { Free,Hinge,ConeTwist };
struct RigAngularLimit {
  RigLimitKind kind=RigLimitKind::Free;
  std::array<float,3> axis{1,0,0};
  float minimum=-180,maximum=180,cone=180;
};
struct RigJoint {
  u64 id=0,parent=0;
  AssetGuid sourceNode;
  std::string name,path;
  RigTransform bind,reference;
};
struct RigControl {
  u64 id=0,joint=0;
  std::string name;
  u8 channels=RigRotation;
  RigControlSpace space=RigControlSpace::LocalReference;
  RigAngularLimit limit;
};
struct RigChain {u64 id=0;std::string name;std::vector<u64> joints;};
struct RigMirrorPair {u64 left=0,right=0;}; // same ID denotes a central control
struct RigPoseEntry {u64 joint=0;u8 channels=RigAll;RigTransform local;};
struct RigPose {
  AssetGuid guid,rig;
  u32 rigRevision=0;
  std::string name;
  // Optional project resource, never an embedded bitmap or external path.
  AssetGuid thumbnail;
  std::vector<RigPoseEntry> entries;
  bool valid(std::string *error=nullptr) const;
  std::string serialize() const;
  static bool deserialize(std::string_view,RigPose &,std::string *error=nullptr);
};
struct RigAsset {
  static constexpr usize MaximumJoints=4096,MaximumControls=4096,MaximumChains=256,MaximumBytes=8*1024*1024;
  AssetGuid guid;
  u32 revision=1;
  u64 nextId=1;
  std::string name;
  std::vector<RigJoint> joints;
  std::vector<RigControl> controls;
  std::vector<RigChain> chains;
  std::vector<RigMirrorPair> mirror;
  // Plane in rig space: dot(normal,p)=offset. The normal must be unit length.
  std::array<float,3> mirrorNormal{1,0,0};
  float mirrorOffset=0;
  const RigJoint *joint(u64) const;
  const RigControl *control(u64) const;
  bool valid(std::string *error=nullptr) const;
  bool edit(u32 expectedRevision,const std::function<bool(RigAsset &,std::string &)> &,std::string &);
  std::string serialize() const;
  static bool deserialize(std::string_view,RigAsset &,std::string *error=nullptr);
};
enum class RetargetPosition : u8 { Reference,PreserveOffset,ScaledOffset,ReconstructWorld };
enum class RetargetScale : u8 { Reference,RelativeUniform };
struct RetargetJoint {
  u64 source=0,target=0;
  RetargetPosition position=RetargetPosition::Reference;
  RetargetScale scale=RetargetScale::Reference;
  // Extra rotation of the transferred motion, not an offset to the rest pose.
  std::array<float,4> correction{0,0,0,1};
};
struct RetargetProfile {
  AssetGuid guid,source,target;
  u32 revision=1,sourceRevision=0,targetRevision=0;
  std::string name;
  float motionScale=1;
  std::array<float,4> facing{0,0,0,1};
  // Unmapped target joints retain their reference local transform. Unmapped
  // source channels are refused by bake unless explicitly allowed here.
  bool allowUnmappedSource=false;
  std::vector<RetargetJoint> mapping;
  bool valid(std::string *error=nullptr) const;
  std::string serialize() const;
  static bool deserialize(std::string_view,RetargetProfile &,std::string *error=nullptr);
};
// Uniform positive scale is an explicit first-version contract. Reflections,
// singular scale and shear cannot be represented by an orientation quaternion.
bool validRigTransform(const RigTransform &);
bool validRigLimit(const RigAngularLimit &);
bool constrainRigRotation(const RigAngularLimit &,const std::array<float,4> &reference,
                          std::array<float,4> &rotation);
RigTransform composeRigTransform(const RigTransform &parent,const RigTransform &local);
RigTransform relativeRigTransform(const RigTransform &world,const RigTransform &parent);
std::array<float,4> multiplyRigRotation(const std::array<float,4> &,const std::array<float,4> &);
std::array<float,4> inverseRigRotation(const std::array<float,4> &);
std::array<float,3> rotateRigVector(const std::array<float,4> &,const std::array<float,3> &);
} // namespace ae::resources
