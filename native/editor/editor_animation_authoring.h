#pragma once
#include "editor/editor_animation_property_wire.h"
#include "resources/animation_clip_asset.h"
#include <memory>

namespace ae::editor {
class EditorSession;
// Versioned editor ABI, independent of ScriptSceneAccess and Play. All handles
// belong to one synchronous editor-command invocation; no retained UI pointers.
struct AnimationAuthorKey {
  u64 id=0;
  float time=0,value=0,inSlope=0,outSlope=0,inWeight=1.f/3,outWeight=1.f/3;
  u32 incoming=4,outgoing=4,flags=0; // broken=1, weighted in=2, weighted out=4
};
static_assert(sizeof(AnimationAuthorKey)==48);
enum class AnimationAuthorOperation : u32 {
  PutKey,SplitKey,EraseKey,PutPose,Retime,Reverse,Crop,RotationMode,
  RemoveTrack,Name,DisplayRate,LayerAdd,LayerConfigure,LayerMove,LayerDuplicate,LayerRemove,LayerAddTrack,LayerCopyBase,PutCue,RemoveCue,
  InsertTime,RemoveTime,DuplicateRange,MoveRange,CloseRange,RepeatRange,PutRegion,RemoveRegion
};
struct AnimationAuthorCommand {
  u32 operation=0,component=0;
  u64 track=0;
  AnimationAuthorKey key;
  double first=0,second=0;
  u32 mode=0,reserved=0;
};
static_assert(sizeof(AnimationAuthorCommand)==88);
struct AnimationAuthorAddress {u64 track=0;u32 component=0,reserved=0;u64 key=0;};
static_assert(sizeof(AnimationAuthorAddress)==24);
struct AnimationAuthorBakeSettings {
  u32 sampleRate=60,verificationSteps=4,maximumFrames=16384,rotation=3,flags=1,reserved=0;
  double tolerance=.01;
};
struct AnimationAuthorBakeReport {u32 inputKeys=0,outputKeys=0,sampledFrames=0,verifiedSamples=0;double maximumError=0;};
static_assert(sizeof(AnimationAuthorBakeSettings)==32&&sizeof(AnimationAuthorBakeReport)==24);
struct AnimationAuthorBakeRequest {
  AnimationAuthorBakeSettings settings;
  u32 eulerPolicy=0,reserved=0;float eulerReference[3]{};u32 reserved2=0;
};
static_assert(sizeof(AnimationAuthorBakeRequest)==56);
struct AnimationAuthorAccess {
  u32 version=12,size=sizeof(AnimationAuthorAccess);
  void *context=nullptr;
  u64 (*selected)(void *)=nullptr;
  int (*count)(void *,u32 imported)=nullptr;
  int (*at)(void *,u32 imported,u32 index,resources::AssetGuid *)=nullptr;
  int (*create)(void *,u64 owner,float duration,u32 mode,resources::AssetGuid *)=nullptr;
  int (*extract)(void *,resources::AssetGuid sourceClip,u64 owner,resources::AssetGuid *)=nullptr;
  u64 (*begin)(void *,resources::AssetGuid,u32 expectedRevision)=nullptr;
  // AECLIP versioned snapshot; length query followed by a whole-buffer copy.
  int (*snapshot)(void *,u64 draft,u8 *buffer,int capacity)=nullptr;
  int (*apply)(void *,u64 draft,const AnimationAuthorCommand *,const float *values,int count,
               const u8 *text,int textLength,u64 *result)=nullptr;
  int (*addTrack)(void *,u64 draft,resources::AssetGuid sourceNode,const u8 *path,int pathLength,
                  const u8 *name,int nameLength,u32 property,u32 mode,const float *values,int count,u64 *result)=nullptr;
  int (*commit)(void *,u64 draft)=nullptr;
  int (*cancel)(void *,u64 draft)=nullptr;
  u64 (*copy)(void *,u64 draft,const AnimationAuthorAddress *,int count)=nullptr;
  int (*paste)(void *,u64 draft,u64 clipboard,float time,u32 mode)=nullptr;
  int (*transformSelection)(void *,u64 draft,const AnimationAuthorAddress *,int count,double pivot,double scale,double offset)=nullptr;
  int (*eraseSelection)(void *,u64 draft,const AnimationAuthorAddress *,int count)=nullptr;
  int (*sample)(void *,u64 draft,u64 track,float time,float *values,int capacity)=nullptr;
  int (*sampleCurve)(void *,u64 draft,u64 track,u32 component,double time,double *value,double *derivative)=nullptr;
  int (*diagnostic)(void *,u8 *buffer,int capacity)=nullptr;
  int (*releaseClipboard)(void *,u64 clipboard)=nullptr;
  // ABI 2 extension; the ABI 1 prefix remains byte-for-byte compatible.
  int (*bake)(void *,u64 draft,u64 track,const AnimationAuthorBakeSettings *,AnimationAuthorBakeReport *)=nullptr;
  // ABI 3: final composition for the property identified by any layer track.
  int (*sampleComposed)(void *,u64 draft,u64 track,float time,float *values,int capacity)=nullptr;
  // ABI 4 leaves ABI 1/2/3 prefixes and bake request intact.
  int (*bakeAdvanced)(void *,u64 draft,u64 track,const AnimationAuthorBakeRequest *,AnimationAuthorBakeReport *)=nullptr;
  int (*consolidate)(void *,u64 draft,const u8 *name,int nameLength,const AnimationAuthorBakeRequest *,resources::AssetGuid *,AnimationAuthorBakeReport *)=nullptr;
  // ABI 5: isolated authoring preview, never source-scene mutations. Staging
  // uses native track units and publishes only through recordPose.
  int (*previewClip)(void *,resources::AssetGuid,u64 owner,float time)=nullptr;
  int (*seekPreview)(void *,float time)=nullptr;
  int (*stagePose)(void *,u64 track,const float *values,int count)=nullptr;
  int (*recordPose)(void *)=nullptr;
  int (*cancelPose)(void *)=nullptr;
  int (*closePreview)(void *)=nullptr;
  // ABI 7 retains the ABI 1-6 prefix. Cue clipboards belong to this scope.
  u64 (*copyCues)(void *,u64 draft,const u64 *ids,int count)=nullptr;
  int (*pasteCues)(void *,u64 draft,u64 clipboard,float time)=nullptr;
  int (*transformCues)(void *,u64 draft,const u64 *ids,int count,double pivot,double scale,double offset)=nullptr;
  int (*eraseCues)(void *,u64 draft,const u64 *ids,int count)=nullptr;
  // ABI 8: same isolated pose tools as touch; flags create-channels=1, armed=2, auto-key=4.
  int (*configurePoseRecording)(void *,u32 flags,float start,float end)=nullptr;
  int (*configurePoseLocks)(void *,u32 path,u64 mask)=nullptr;
  // 0 identity, 1 scene reference, 2 copy local channel, 3 copy world TRS, 4 paste.
  int (*poseTool)(void *,u32 operation)=nullptr;
  // ABI 9: transient object selection + persistent clip-relative named sets.
  // 0 set, 1 toggle, 2 parents, 3 children, 4 siblings, 5 all, 6 clear,
  // 7 focus, 8 isolate, 9 leave isolation. Set/toggle alone accept targets.
  int (*poseSelection)(void *,u32 operation,const u64 *targets,int count,u64 primary)=nullptr;
  int (*poseTargets)(void *,u64 *targets,int capacity)=nullptr;
  // 0 save, 1 recall, 2 remove. UTF-8 name is copied synchronously.
  int (*poseSet)(void *,u32 operation,const u8 *name,int length)=nullptr;
  // index=-1 queries count; other indices query/copy UTF-8 byte length.
  int (*poseSetName)(void *,int index,u8 *name,int capacity)=nullptr;
  // ABI 10: arrangement shares the atomic resource operation used by touch.
  int (*arrangeSelection)(void *,u64 draft,const AnimationAuthorAddress *keys,int keyCount,
      const u64 *cues,int cueCount,u32 mode,float first,float last)=nullptr;
  // ABI 11: section clock and final composition share the runtime sampler.
  int (*sampleRegion)(void *,u64 draft,u64 region,u64 track,double elapsed,u32 wrap,float *values,int capacity)=nullptr;
  // ABI 12: same catalog, guarded bindings and atomic authoring as touch.
  int (*propertyCatalog)(void *,u64,int,AnimationAuthorProperty *,u8 *,int)=nullptr;
  int (*addProperty)(void *,u64,u64,u64,const AnimationAuthorProperty *,u64 *)=nullptr;
  int (*putProperty)(void *,u64,u64,u64,float,const AnimationAuthorPropertyValue *)=nullptr;
  int (*sampleProperty)(void *,u64,u64,float,AnimationAuthorPropertyValue *)=nullptr;
  int (*remapProperty)(void *,u64,u64,u64,u64,const AnimationAuthorProperty *)=nullptr;
  int (*configureDrivers)(void *,u64,u64,const u8 *,int)=nullptr;
  int (*bindPropertyObject)(void *,u64,u64,u64,u64 *)=nullptr;
  int (*driverParameters)(void *,u64,int,AnimationAuthorDriverParameter *,u8 *,int)=nullptr;
};
class AnimationAuthoringScope final {
public:
  explicit AnimationAuthoringScope(EditorSession &session);
  ~AnimationAuthoringScope();
  AnimationAuthoringScope(const AnimationAuthoringScope &)=delete;
  AnimationAuthoringScope &operator=(const AnimationAuthoringScope &)=delete;
  const AnimationAuthorAccess &access() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
