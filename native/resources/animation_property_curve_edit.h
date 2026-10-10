#pragma once
#include "resources/animation_clip_asset.h"
namespace ae::resources {
bool putAnimationPropertyCurveKey(AnimationClipAsset &,u64 track,u32 component,AnimationCurveKey,u64 &created,std::string &error);
bool splitAnimationPropertyCurveKey(AnimationClipAsset &,u64 track,u32 component,float time,u64 &created,std::string &error);
bool eraseAnimationPropertyCurveKey(AnimationClipAsset &,u64 track,u32 component,u64 key,std::string &error);
}
