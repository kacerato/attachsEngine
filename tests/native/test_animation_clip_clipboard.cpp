#include "harness.h"
#include "resources/animation_clip_clipboard.h"
#include <algorithm>
#include <cmath>
using namespace ae;using namespace ae::resources;
AE_TEST(animation_clip_clipboard_preserves_typed_poses_handles_and_insertion_context) {
  AnimationClip source;source.name="Clipboard universal";source.duration=4;
  AnimationChannel q;q.path=AnimationPath::Rotation;q.times={0,1,2,4};q.values={0,0,0,1,0,.5f,0,.8660254f,0,.8660254f,0,.5f,0,1,0,0};
  AnimationChannel p;p.times=q.times;p.values={0,0,0,10,2,3,20,4,6,40,6,9};
  auto scale=p;scale.path=AnimationPath::Scale;scale.values={1,1,1,2,2,2,3,3,3,4,4,4};source.channels={q,p,scale};
  const AnimationClipBinding binding{0,{},"","Máquina"};AnimationClipAsset original;std::string error;
  AE_EXPECT_TRUE(authorAnimationClip(source,assetGuidFromSeed("clipboard-source"),{},{},{},{&binding,1},original,error),error.c_str());
  auto &weighted=original.tracks[1].curves[0].keys[1];weighted.incoming=weighted.outgoing=AnimationTangentMode::Free;
  weighted.inSlope=2;weighted.outSlope=4;weighted.weightedIn=weighted.weightedOut=true;weighted.inWeight=.2f;weighted.outWeight=.6f;weighted.broken=true;
  for(const auto mode:{AnimationRotationMode::Quaternion,AnimationRotationMode::ProgressiveQuaternion}) {
    auto asset=original;const auto rotation=asset.tracks[0].id,position=asset.tracks[1].id;
    AE_EXPECT_TRUE(asset.changeRotationMode(rotation,mode,error),error.c_str());
    const AnimationKeyAddress chosen[]{ {rotation,1,asset.track(rotation)->curves[1].keys[1].id},{rotation,2,asset.track(rotation)->curves[2].keys[2].id},{position,0,asset.track(position)->curves[0].keys[1].id} };
    AnimationKeyClipboard clipboard;AE_EXPECT_TRUE(clipboard.copy(asset,chosen,error),error.c_str());
    const auto copiedCount=mode==AnimationRotationMode::Quaternion?9u:11u;
    AE_EXPECT_EQ(clipboard.size(),copiedCount,"typed copy expands complete poses and retains independent scalar axes");
    const auto before=asset.serialize();const auto frontier=asset.nextId;std::vector<AnimationKeyAddress> pasted;
    AE_EXPECT_TRUE(clipboard.paste(asset,2.5f,AnimationPasteMode::Replace,pasted,error),error.c_str());
    AE_EXPECT_EQ(pasted.size(),copiedCount,"paste returns new persistent addresses");
    AE_EXPECT_TRUE(asset.nextId==frontier+copiedCount,"only new pasted keys consume IDs");
    for(const auto &address:pasted)AE_EXPECT_TRUE(address.key>=frontier,"clipboard source IDs are not reused");
    const auto &keys=asset.track(position)->curves[0].keys;
    const auto found=std::find_if(keys.begin(),keys.end(),[](const auto &key){return key.time==2.5f;});
    AE_EXPECT_TRUE(found!=keys.end()&&found->value==10&&found->inSlope==2&&found->outSlope==4&&found->weightedIn&&found->weightedOut&&found->inWeight==.2f&&found->outWeight==.6f&&found->broken,"copied weighted handles affect the destination curve");
    AnimationClip compiled;float pose[4];AE_EXPECT_TRUE(asset.compile(compiled)&&sampleAnimationChannel(compiled.channels[0],3.5f,pose)&&std::abs(pose[1]-.8660254f)<1e-5,"pasted pose reaches native runtime sampler");
    const auto insertedFrontier=asset.nextId;
    AE_EXPECT_TRUE(clipboard.paste(asset,2.5f,AnimationPasteMode::Replace,pasted,error)&&asset.nextId==insertedFrontier,"replace at existing key times retains destination identity");
    const auto unchanged=asset.serialize();const auto priorSelection=pasted;
    AE_EXPECT_TRUE(!clipboard.paste(asset,3.5f,AnimationPasteMode::Replace,pasted,error)&&asset.serialize()==unchanged&&pasted==priorSelection,"out-of-range paste is atomic including output selection");
    AnimationClipAsset clean;AE_EXPECT_TRUE(AnimationClipAsset::deserialize(before,clean),"restore source snapshot");
    clean.guid=assetGuidFromSeed("other-clipboard-target");
    AE_EXPECT_TRUE(clipboard.paste(clean,2.5f,AnimationPasteMode::Replace,pasted,error),error.c_str());
    AE_EXPECT_TRUE(clean.tracks[0].curves[0].keys.size()==6,"cross-clip paste resolves portable binding and typed property");
    auto missing=clean;missing.bindings[0].path="Outro";const auto absent=missing.serialize();
    AE_EXPECT_TRUE(!clipboard.paste(missing,0,AnimationPasteMode::Replace,pasted,error)&&missing.serialize()==absent,"missing portable binding refuses without changing the clip");
    AE_EXPECT_TRUE(AnimationClipAsset::deserialize(before,clean),"restore for insertion");
    const auto oldPositionY=clean.track(position)->curves[1].keys[1].time;
    AE_EXPECT_TRUE(clipboard.paste(clean,1,AnimationPasteMode::InsertTracks,pasted,error),error.c_str());
    AE_EXPECT_TRUE(clean.duration>5&&clean.track(position)->curves[1].keys[1].time>oldPositionY,"insert shifts complete affected properties and expands duration");
    AE_EXPECT_TRUE(clean.track(rotation)->curves[0].keys[1].time==1&&clean.track(rotation)->curves[0].keys[2].time==2&&clean.track(rotation)->curves[0].keys[3].time>2,"insert retains old boundary after pasted range plus one frame");
    AE_EXPECT_TRUE(clean.tracks[2].curves[0].keys[1].time==1,"insert affected tracks leaves unrelated channels unchanged");
    if(mode==AnimationRotationMode::ProgressiveQuaternion)AE_EXPECT_TRUE(clean.valid(&error),"inserted progressive poses retain derived cumulative angular distance");
    AE_EXPECT_TRUE(AnimationClipAsset::deserialize(before,clean)&&clipboard.paste(clean,1,AnimationPasteMode::InsertAllTracks,pasted,error),error.c_str());
    AE_EXPECT_TRUE(clean.tracks[2].curves[0].keys[1].time>2,"ripple insertion can shift every track explicitly");
  }
}

AE_TEST(animation_clip_layer_clipboard_resolves_masks_and_shifts_shared_references_safely) {
  AnimationClip clip;clip.duration=2;AnimationChannel p;p.times={0,2};p.values={0,0,0,2,0,0};auto scale=p;scale.path=AnimationPath::Scale;scale.values={1,1,1,2,2,2};clip.channels={p,scale};
  const AnimationClipBinding binding{0,{},"","Máquina"};AnimationClipAsset source;std::string error;u64 layer=0,track=0,other=0;
  AE_EXPECT_TRUE(authorAnimationClip(clip,assetGuidFromSeed("layer-clipboard"),{},{},{},{&binding,1},source,error)&&source.addLayer("Correção",AnimationAuthorBlend::Additive,layer,error)&&source.addLayerTrack(layer,source.tracks[0].id,true,track,error)&&source.addLayerTrack(layer,source.tracks[1].id,true,other,error),error.c_str());
  AE_EXPECT_TRUE(source.configureLayer(layer,"Correção",AnimationAuthorBlend::Additive,1,1,false,false,error),error.c_str());
  AnimationKeyClipboard clipboard;const AnimationKeyAddress chosen{track,0,source.track(track)->curves[0].keys.front().id};
  AE_EXPECT_TRUE(clipboard.copy(source,{&chosen,1},error),error.c_str());
  auto target=source;target.guid=assetGuidFromSeed("layer-clipboard-target");std::vector<AnimationKeyAddress> result;
  AE_EXPECT_TRUE(clipboard.paste(target,.5f,AnimationPasteMode::Replace,result,error)&&result.front().track==track&&target.tracks[0].curves[0].keys.size()==2,"cross-clip matches layer name and property, not the last property");
  u64 duplicate=0;AE_EXPECT_TRUE(target.duplicateLayer(layer,duplicate,error),error.c_str());target.layer(duplicate)->name="Correção";
  const auto ambiguous=target.serialize();AE_EXPECT_TRUE(!clipboard.paste(target,.75f,AnimationPasteMode::Replace,result,error)&&target.serialize()==ambiguous,"ambiguous layer names refuse atomically");
  const auto before=source.serialize();AE_EXPECT_TRUE(!clipboard.paste(source,.5f,AnimationPasteMode::InsertTracks,result,error)&&source.serialize()==before,"partial layer shift cannot retime a reference shared with unshifted tracks");
  AE_EXPECT_TRUE(clipboard.paste(source,.5f,AnimationPasteMode::InsertAllTracks,result,error)&&source.layer(layer)->referenceTime>1,"whole-clip insertion retimes the layer reference");
}
