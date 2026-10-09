#pragma once
#include "resources/skeletal_animation.h"
#include <functional>
#include <optional>
#include <string>

namespace ae::resources {
enum class AnimationAuthorBlend : u8 { Override, Additive };
struct AnimationClipLayer {
  // Zero is the permanent base. All other identities share the clip allocator.
  u64 id=0;
  std::string name="Base";
  AnimationAuthorBlend blend=AnimationAuthorBlend::Override;
  float weight=1,referenceTime=-1; // -1: neutral offset; >=0: this layer at that time
  bool muted=false,solo=false;
};
struct AnimationClipBinding {
  u64 id=0;
  AssetGuid sourceNode;
  // Canonical relative object path (animation_binding_path.h encodes names).
  // Empty means the owner. Name is display metadata,
  // never an ambiguous fallback when a path was explicitly authored.
  std::string path,name;
};
struct AnimationClipTrack {
  u64 id=0,binding=0;
  AnimationPath path=AnimationPath::Translation;
  u32 weightCount=0;
  bool sourceOverride=false;
  std::vector<AnimationCurve> curves;
  AnimationRotationMode rotationMode=AnimationRotationMode::Quaternion;
  u64 layer=0;
  u32 components() const {return path==AnimationPath::Rotation?rotationCurveComponents(rotationMode):path==AnimationPath::Weights?weightCount:3u;}
};
struct AnimationClipAsset {
  static constexpr u32 FormatVersion=3,MaximumLayers=32;
  static constexpr usize MaximumBytes=64*1024*1024,MaximumBindings=4096,MaximumTracks=16384,MaximumKeys=262144;
  AssetGuid guid,source,sourceClip;
  u64 nextId=1;
  u32 revision=1,displayRate=60;
  std::string name,sourceHash;
  float duration=0;
  std::vector<AnimationClipBinding> bindings;
  std::vector<AnimationClipTrack> tracks;
  std::vector<AnimationClipLayer> layers{AnimationClipLayer{}};
  bool valid(std::string *diagnostic=nullptr) const;
  std::string serialize() const;
  static bool deserialize(std::string_view text,AnimationClipAsset &out,std::string *diagnostic=nullptr);
  // Immutable runtime data. Track/binding order may change without altering IDs.
  bool compile(AnimationClip &out,std::string *diagnostic=nullptr) const;
  // In-memory authoring transaction; disk publication / history use the same
  // candidate and revision contract. Failed edits leave every ID and key intact.
  bool edit(u32 expectedRevision,const std::function<bool(AnimationClipAsset &)> &change,std::string &diagnostic);
  AnimationClipTrack *track(u64 id);
  const AnimationClipTrack *track(u64 id) const;
  AnimationClipLayer *layer(u64 id);
  const AnimationClipLayer *layer(u64 id) const;
  bool addLayer(std::string_view name,AnimationAuthorBlend blend,u64 &id,std::string &error);
  bool configureLayer(u64 id,std::string_view name,AnimationAuthorBlend blend,float weight,
                      float referenceTime,bool muted,bool solo,std::string &error);
  bool moveLayer(u64 id,u32 position,std::string &error);
  bool duplicateLayer(u64 id,u64 &created,std::string &error);
  bool removeLayer(u64 id,std::string &error);
  bool copyBaseToTrack(u64 track,std::string &error);
  // Sparse whole-property mask. A property is authored on the base first so
  // muted/solo layers always have a stable, scene-independent initial pose.
  bool addLayerTrack(u64 layerId,u64 baseTrack,bool copyCurves,u64 &created,std::string &error,
                     std::optional<AnimationRotationMode> rotation={});
  // A quaternion key is a synchronized group. These operations preserve the
  // other components and their IDs; callers never repair topology themselves.
  bool putKey(u64 trackId,u32 component,AnimationCurveKey key,u64 &resultId,std::string &diagnostic);
  bool splitKey(u64 trackId,u32 component,float time,u64 &resultId,std::string &diagnostic);
  // Typed authoring primitives. Initial values and pose values are runtime
  // units (quaternion XYZW; Euler degrees; morph weights in 0..1 units).
  // Progressive speed is derived from poses, never supplied as a fifth value.
  bool addTrack(AnimationClipBinding binding,AnimationPath path,AnimationRotationMode mode,
                std::span<const float> values,u64 &trackId,std::string &diagnostic);
  bool removeTrack(u64 trackId,std::string &diagnostic);
  bool putPose(u64 trackId,float time,std::span<const float> values,std::string &diagnostic);
  bool eraseKey(u64 trackId,u32 component,u64 keyId,std::string &diagnostic);
  bool retime(double scale,std::string &diagnostic);
  bool reverse(std::string &diagnostic);
  bool crop(float start,float end,std::string &diagnostic);
  // Lossless conversion of shortest-arc linear rotations. Curved quaternion
  // components and Euler turns require an explicit bake, never silent loss.
  bool changeRotationMode(u64 trackId,AnimationRotationMode mode,std::string &diagnostic);
};
// Preserves glTF Step/Linear/CubicSpline values and quaternion interpolation.
// The caller provides the already resolved, portable bindings of the source.
bool authorAnimationClip(const AnimationClip &clip,AssetGuid guid,AssetGuid source,AssetGuid sourceClip,
                         std::string_view sourceHash,std::span<const AnimationClipBinding> bindings,
                         AnimationClipAsset &out,std::string &diagnostic);
} // namespace ae::resources
