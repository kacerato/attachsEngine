#pragma once
#include "resources/animation_curve.h"
#include "resources/asset_registry.h"
#include <memory>
#include <iosfwd>
#include <variant>

namespace ae::resources {
struct AnimationChannel;
enum class AnimationPropertyKind : u8 { Number,Vector2,Vector3,LinearColor,Quaternion,Boolean,Enumeration,Object,Resource };
enum class AnimationPropertyDomain : u8 { Component,MaterialSlot,GuiNode };
// Object identity is the enclosing clip binding. Component identity is local to
// that object and survives prefab cloning; indices and display labels are not IDs.
struct AnimationPropertyAddress {
  AnimationPropertyDomain domain=AnimationPropertyDomain::Component;
  AnimationPropertyKind kind=AnimationPropertyKind::Number;
  u64 component=0;
  u32 version=0;
  std::string type,property;
  u32 slot=0,guiNode=0;
  // Guard the material primitive / UI document. Replacing the referenced
  // resource must produce a broken binding rather than silently reuse an index.
  AssetGuid resource{};
  bool operator==(const AnimationPropertyAddress &) const=default;
};
bool validAnimationPropertyAddress(const AnimationPropertyAddress &);
u32 animationPropertyComponents(AnimationPropertyKind);

struct AnimationPropertyVector2 {float x=0,y=0;};
struct AnimationPropertyVector3 {float x=0,y=0,z=0;};
struct AnimationPropertyColor {float r=0,g=0,b=0;};
struct AnimationPropertyQuaternion {float x=0,y=0,z=0,w=1;};
struct AnimationPropertyObject {u64 id=0;};
// Closed semantic types. Neither object IDs nor AssetGuid bits pass through float.
using AnimationPropertyValue=std::variant<float,AnimationPropertyVector2,AnimationPropertyVector3,
  AnimationPropertyColor,AnimationPropertyQuaternion,bool,u32,AnimationPropertyObject,AssetGuid>;
struct AnimationPropertyDiscreteKey {
  u64 id=0;float time=0;
  AnimationPropertyValue value=false;
};
struct AnimationPropertyData {
  AnimationPropertyKind kind=AnimationPropertyKind::Number;
  std::vector<AnimationCurve> curves;
  std::vector<AnimationPropertyDiscreteKey> keys;
  // Reverse preserves discontinuities exactly: the key at a boundary belongs
  // to the following interval normally, and to the preceding one after reverse.
  bool leftContinuous=false;
};
bool validAnimationPropertyData(const AnimationPropertyData &,float duration,std::string &error);
class CompiledAnimationProperty final {
public:
  AnimationPropertyKind kind() const {return kind_;}
  // Immutable data; allocation occurs at compilation, never during sampling.
  // Errors preserve the caller's previous output.
  bool sample(float time,AnimationPropertyValue &out) const;
private:
  friend bool compileAnimationProperty(const AnimationPropertyData &,float,CompiledAnimationProperty &,std::string &);
  AnimationPropertyKind kind_=AnimationPropertyKind::Number;
  std::vector<AnimationCurve> curves_;
  std::vector<AnimationPropertyDiscreteKey> keys_;
  std::shared_ptr<const AnimationChannel> quaternion_;
  bool leftContinuous_=false;
};
bool compileAnimationProperty(const AnimationPropertyData &,float duration,CompiledAnimationProperty &,std::string &error);
// Shared by catalog authoring and native clients: construct a real first key,
// preserving semantic types rather than coercing discrete identities to floats.
bool seedAnimationProperty(AnimationPropertyKind,const AnimationPropertyValue &,float time,
                           AnimationPropertyData &,std::string &error);
struct AnimationPropertyTrack {
  u64 id=0,binding=0;
  AnimationPropertyAddress address;
  AnimationPropertyData data;
};
struct CompiledAnimationPropertyTrack {
  u32 node=0;
  AnimationPropertyAddress address;
  CompiledAnimationProperty data;
};
std::string animationPropertyIdentity(const AnimationPropertyAddress &);
void writeAnimationPropertyTrack(std::ostream &,const AnimationPropertyTrack &);
bool readAnimationPropertyTrack(std::istream &,AnimationPropertyTrack &,usize &keyBudget,std::string &error);
// Timeline operations share these functions with authoring, SDK and drivers.
bool retimeAnimationProperty(AnimationPropertyData &,double scale,double offset,std::string &error);
bool reverseAnimationProperty(AnimationPropertyData &,float duration,std::string &error);
bool cropAnimationProperty(AnimationPropertyData &,float start,float end,u64 &nextId,std::string &error);
}
