#include "editor/editor_session.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {namespace w=clip_widget;using namespace resources;}
bool EditorSession::handleAnimationPropertyCurveWidget(u32 code) {
  const auto *asset=animationClipAsset(state_.clipGuid);
  const auto *track=asset?asset->propertyTrack(state_.clipPropertyTrack):nullptr;
  if(!track||track->data.curves.empty())return false;
  const u32 component=std::min<u32>(state_.clipPropertyComponent,u32(track->data.curves.size()-1));
  const auto &curve=track->data.curves[component];std::string error;
  const auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.id==state_.clipPropertyKey;});
  if(code==w::PropertyCurve)state_.clipPropertyCurves=!state_.clipPropertyCurves;
  else if(code==w::PropertyComponent) {
    state_.clipPropertyComponent=(component+1)%u32(track->data.curves.size());state_.clipPropertyKey=0;clipPropertyResourceRevision_=0;
  } else if(code==w::PropertyCurveFields)state_.clipPropertyCurveMode=(state_.clipPropertyCurveMode+1)%3;
  else if(code>=w::PropertyCurveKey&&code<w::PropertyCurveKey+64) {
    const usize index=code-w::PropertyCurveKey;
    if(index<curve.keys.size()){state_.clipPropertyKey=curve.keys[index].id;seekAnimationClip(curve.keys[index].time,error);}
  } else if((code==w::PropertyTangent||code==w::PropertyWeighted)&&found!=curve.keys.end()) {
    auto key=*found;
    if(code==w::PropertyTangent)key.incoming=key.outgoing=AnimationTangentMode((u32(key.outgoing)+1)%7);
    else key.weightedIn=key.weightedOut=!(key.weightedIn&&key.weightedOut);
    editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){u64 id=0;return candidate.putKey(track->id,component,key,id,error)&&validateAnimationPropertyClip(candidate,state_.clipOwner,key.time,error);},error);
  } else if(code>=w::PropertyCurveNumber&&code<w::PropertyCurveNumber+6&&found!=curve.keys.end()) {
    const double values[]={found->time,found->value,found->inSlope,found->outSlope,found->inWeight,found->outWeight};
    beginAnimationClipNumber(code,values[code-w::PropertyCurveNumber]);
  } else return false;
  state_.clipDiagnostic=error;refreshAnimationClip();refreshAnimationPropertySurface();appearanceChanged_=true;return true;
}
bool EditorSession::applyAnimationPropertyCurveNumber(u32 code,double value) {
  const auto *asset=animationClipAsset(clipNumberGuid_);
  if(!asset||asset->revision!=clipNumberRevision_||state_.clipPropertyTrack!=clipNumberTrack_||
     state_.clipPropertyComponent!=clipNumberComponent_||document_.revision()!=clipNumberSceneRevision_) {
    state_.clipDiagnostic="Curva ou cena mudou; reabra o campo";return false;
  }
  const auto *track=asset->propertyTrack(clipNumberTrack_);
  if(!track||clipNumberComponent_>=track->data.curves.size())return false;
  const auto &curve=track->data.curves[clipNumberComponent_];
  const auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.id==clipNumberKey_;});
  if(found==curve.keys.end())return false;
  auto key=*found;const auto index=code-w::PropertyCurveNumber;
  if(index>5||!std::isfinite(value))return false;
  if(index==0)key.time=float(value);else if(index==1)key.value=value;else if(index==2)key.inSlope=value;
  else if(index==3)key.outSlope=value;else if(index==4)key.inWeight=value;else key.outWeight=value;
  std::string error;
  const bool ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){u64 id=0;return candidate.putKey(track->id,clipNumberComponent_,key,id,error)&&validateAnimationPropertyClip(candidate,state_.clipOwner,key.time,error);},error);
  state_.clipDiagnostic=error;if(ok){seekAnimationClip(key.time,error);refreshAnimationClip();refreshAnimationPropertySurface();}return ok;
}
}
