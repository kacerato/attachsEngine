#include "resources/animation_clip_asset.h"
#include "resources/animation_clip_edit_helpers.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>

namespace ae::resources {
namespace {
using Mode=AnimationTangentMode;
bool grouped(const AnimationClipTrack &track) {return track.path==AnimationPath::Rotation&&track.rotationMode!=AnimationRotationMode::Euler;}
bool extendHeld(AnimationCurve &curve,float time,u64 &nextId,u64 &result) {
  const bool before=time<curve.keys.front().time;
  if(!before&&time<=curve.keys.back().time)return false;
  auto &boundary=before?curve.keys.front():curve.keys.back();
  const usize index=before?0:curve.keys.size()-1;
  auto &innerMode=before?boundary.outgoing:boundary.incoming;
  if(innerMode==Mode::Auto||innerMode==Mode::ClampedAuto) {
    (before?boundary.outSlope:boundary.inSlope)=static_cast<float>(animationCurveSlope(curve,index,!before));
    innerMode=Mode::Free;
  }
  AnimationCurveKey key;key.time=time;key.value=boundary.value;
  key.incoming=key.outgoing=Mode::Free;key.broken=true;
  // Freeze only the newly used side. The old segment's handles and IDs stay
  // intact, including weighted/automatic tangents on its opposite side.
  boundary.broken=true;
  (before?boundary.incoming:boundary.outgoing)=Mode::Free;
  (before?boundary.inSlope:boundary.outSlope)=0;
  (before?boundary.weightedIn:boundary.weightedOut)=false;
  return putAnimationCurveKey(curve,key,nextId,result);
}
void linearPose(AnimationCurveKey &key) {
  key.incoming=key.outgoing=Mode::Linear;key.broken=false;key.weightedIn=key.weightedOut=false;key.inSlope=key.outSlope=0;
}
bool rebuildProgress(AnimationClipTrack &track,std::string &error) {
  auto &progress=track.curves[4];const auto original=progress;
  if(!validAnimationCurve(original)||original.keys.size()!=track.curves[0].keys.size()) {error="Topologia da progressão inválida";return false;}
  double cumulative=0;float previous[4]{};
  for(usize i=0;i<progress.keys.size();++i) {
    float pose[4];for(u32 c=0;c<4;++c)pose[c]=track.curves[c].keys[i].value;
    if(i)cumulative+=rotationDistanceDegrees(previous,pose);
    if(!std::isfinite(cumulative)) {error="Pose de rotação inválida";return false;}
    progress.keys[i].value=static_cast<float>(cumulative);
    if(i&&cumulative>double(progress.keys[i-1].value)&&progress.keys[i].value==progress.keys[i-1].value) {error="Progressão excede a precisão do clipe";return false;}
    std::copy(pose,pose+4,previous);
  }
  // Changing a pose rescales the speed profile of each affected segment,
  // preserving its normalized handles. Freeze only when values changed;
  // ordinary tangent edits retain Auto / ClampedAuto as authored modes.
  bool changed=false;for(usize i=0;i<progress.keys.size();++i)changed|=progress.keys[i].value!=original.keys[i].value;
  if(changed)for(usize i=0;i<progress.keys.size();++i) {
    auto &key=progress.keys[i];key.broken=true;
    for(bool incoming:{true,false}) {
      const usize a=incoming?(i?i-1:i):i,b=incoming?i:std::min(i+1,progress.keys.size()-1);
      const double oldSpan=double(original.keys[b].value)-original.keys[a].value,newSpan=double(progress.keys[b].value)-progress.keys[a].value;
      auto &mode=incoming?key.incoming:key.outgoing;auto &slope=incoming?key.inSlope:key.outSlope;
      if(oldSpan==0) {mode=Mode::Linear;slope=0;continue;}
      const double value=animationCurveSlope(original,i,incoming)*newSpan/oldSpan;
      if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()) {error="Handle de progressão excede a precisão";return false;}
      slope=static_cast<float>(value);if(mode!=Mode::Constant&&mode!=Mode::NextConstant)mode=Mode::Free;
    }
  }
  for(u32 c=0;c<4;++c)for(auto &key:track.curves[c].keys)linearPose(key);
  return validAnimationCurve(progress);
}
bool cropProgressive(const AnimationClipTrack &source,AnimationClipTrack &track,float start,float end,u64 &nextId,std::string &error) {
  AnimationChannel channel;channel.path=AnimationPath::Rotation;channel.rotationMode=source.rotationMode;channel.curves=source.curves;
  auto &progress=track.curves[4];
  if(!cropAnimationCurve(progress,start,end,nextId)) {error="Corte não preserva os handles da progressão";return false;}
  // New boundaries may lie on an overshoot, wrap, or reversal. Split the exact
  // scalar Bezier until every endpoint pair can express the retained arc.
  // This adds real pose keys, not a sampled approximation of the curve.
  for(usize i=0;i+1<progress.keys.size();) {
    const auto a=progress.keys[i],b=progress.keys[i+1];float qa[4],qb[4];
    if(!sampleAnimationChannel(channel,start+a.time,qa)||!sampleAnimationChannel(channel,start+b.time,qb))return false;
    const double delta=double(b.value)-a.value,angle=rotationDistanceDegrees(qa,qb);
    double variation=0;float split=a.time+(b.time-a.time)*.5f;
    for(float fraction:{.25f,.5f,.75f}) {
      const float t=a.time+(b.time-a.time)*fraction;AnimationCurveSample sample;
      if(!sampleValidatedAnimationCurve(progress,t,sample))return false;
      if(std::abs(sample.value-a.value)>variation) {variation=std::abs(sample.value-a.value);if(std::abs(delta)<1e-6)split=t;}
    }
    const bool flat=angle<1e-6&&std::abs(delta)<1e-6&&variation<1e-6;
    // Flat source poses remain flat even if unused speed handles were authored.
    const auto &sourceKeys=source.curves[4].keys;const float middle=start+split;
    const auto upper=std::upper_bound(sourceKeys.begin(),sourceKeys.end(),middle,[](float t,const auto &key){return t<key.time;});
    const bool held=upper==sourceKeys.begin()||upper==sourceKeys.end()||upper->value==(upper-1)->value;
    if(flat||held||(std::abs(delta)<179.99&&std::abs(angle-std::abs(delta))<.002)) {++i;continue;}
    if(progress.keys.size()>AnimationClipAsset::MaximumKeys/5||split<=a.time||split>=b.time) {error="Corte de rotação excede a precisão ou o limite de chaves";return false;}
    u64 id=0;if(!splitAnimationCurve(progress,split,nextId,id)) {error="Progressão não pode ser subdividida sem perda";return false;}
  }
  for(u32 c=0;c<4;++c)track.curves[c].keys.clear();
  for(const auto &p:progress.keys) {
    const auto &originalKeys=source.curves[0].keys;
    const auto matching=std::find_if(originalKeys.begin(),originalKeys.end(),[&](const auto &key){return key.time-start==p.time;});
    const float originalTime=matching==originalKeys.end()?start+p.time:matching->time;
    float pose[4];if(!sampleAnimationChannel(channel,originalTime,pose)) {error="Pose de corte inválida";return false;}
    for(u32 c=0;c<4;++c) {
      const auto &keys=source.curves[c].keys;
      const auto original=std::find_if(keys.begin(),keys.end(),[&](const auto &key){return key.time==originalTime;});
      AnimationCurveKey key;key.time=p.time;key.value=pose[c];
      key.id=original==keys.end()?nextId++:original->id;linearPose(key);track.curves[c].keys.push_back(key);
    }
  }
  return rebuildProgress(track,error);
}
}
bool rebuildProgressiveAnimationTrack(AnimationClipTrack &track,std::string &diagnostic) {
  if(track.path!=AnimationPath::Rotation||track.rotationMode!=AnimationRotationMode::ProgressiveQuaternion||track.curves.size()!=5) {diagnostic="Canal não possui rotação progressiva";return false;}
  for(const auto &curve:track.curves)if(!validAnimationCurve(curve)||curve.keys.size()!=track.curves[0].keys.size()) {diagnostic="Topologia da pose progressiva inválida";return false;}
  for(u32 c=1;c<5;++c)for(usize i=0;i<track.curves[0].keys.size();++i)
    if(track.curves[c].keys[i].time!=track.curves[0].keys[i].time) {diagnostic="Tempos da pose progressiva dessincronizados";return false;}
  return rebuildProgress(track,diagnostic);
}
bool AnimationClipAsset::addTrack(AnimationClipBinding binding,AnimationPath path,AnimationRotationMode mode,
                                 std::span<const float> values,u64 &trackId,std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;
  if(static_cast<u8>(path)>3||static_cast<u8>(mode)>2||(path!=AnimationPath::Rotation&&mode!=AnimationRotationMode::Quaternion)||
     values.empty()||values.size()>MaximumMorphTargets) {diagnostic="Propriedade ou modo de autoria inválido";return false;}
  const usize expected=path==AnimationPath::Weights?values.size():path==AnimationPath::Rotation&&mode!=AnimationRotationMode::Euler?4:3;
  if(values.size()!=expected) {diagnostic="Dimensão da pose incompatível com a propriedade";return false;}
  for(const auto value:values)if(!std::isfinite(value)) {diagnostic="Pose não finita";return false;}
  auto candidate=*this;
  const auto found=std::find_if(candidate.bindings.begin(),candidate.bindings.end(),[&](const auto &b){return b.path==binding.path;});
  u64 bindingId=0;
  if(found!=candidate.bindings.end()) {
    if(found->sourceNode!=binding.sourceNode) {diagnostic="Outro nó ocupa este binding";return false;}
    bindingId=found->id;
  } else {binding.id=candidate.nextId++;bindingId=binding.id;candidate.bindings.push_back(std::move(binding));}
  for(const auto &track:candidate.tracks)if(!track.layer&&track.binding==bindingId&&track.path==path) {diagnostic="O objeto já possui esta propriedade na base";return false;}
  AnimationClipTrack track;track.id=candidate.nextId++;track.binding=bindingId;track.path=path;track.rotationMode=mode;
  track.weightCount=path==AnimationPath::Weights?static_cast<u32>(values.size()):0;track.sourceOverride=true;track.curves.resize(track.components());
  for(u32 c=0;c<track.components();++c) {
    AnimationCurveKey key;linearPose(key);key.id=candidate.nextId++;key.value=c<values.size()?values[c]:0;
    track.curves[c].keys.push_back(key);
    if(duration>0) {key.id=candidate.nextId++;key.time=duration;track.curves[c].keys.push_back(key);}
  }
  const auto id=track.id;candidate.tracks.push_back(std::move(track));
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);trackId=id;return true;
}
bool AnimationClipAsset::removeTrack(u64 trackId,std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;auto candidate=*this;
  const auto found=std::find_if(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto &track){return track.id==trackId;});
  if(found==candidate.tracks.end()||candidate.tracks.size()==1) {diagnostic="Preserve ao menos uma propriedade no clipe";return false;}
  const auto binding=found->binding;candidate.tracks.erase(found);
  if(std::none_of(candidate.tracks.begin(),candidate.tracks.end(),[&](const auto &track){return track.binding==binding;}))
    std::erase_if(candidate.bindings,[&](const auto &value){return value.id==binding;});
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::putPose(u64 trackId,float time,std::span<const float> values,std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;auto candidate=*this;auto *track=candidate.track(trackId);
  const usize expected=track?track->path==AnimationPath::Rotation&&track->rotationMode!=AnimationRotationMode::Euler?4:track->components():0;
  if(!track||values.size()!=expected||!std::isfinite(time)||time<0||time>duration) {diagnostic="Pose, propriedade ou tempo incompatível";return false;}
  for(const float value:values)if(!std::isfinite(value)) {diagnostic="Pose não finita";return false;}
  // Insert at the runtime pose first, then replace all components atomically.
  // This avoids invalid intermediate quaternions while editing one full pose.
  for(u32 c=0;c<track->components();++c) {
    auto &curve=track->curves[c];auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.time==time;});
    if(found==curve.keys.end()) {
      u64 id=0;if(!candidate.splitKey(trackId,c,time,id,diagnostic))return false;
      track=candidate.track(trackId);
    }
  }
  for(u32 c=0;c<values.size();++c) {
    auto &curve=track->curves[c];auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.time==time;});
    if(found==curve.keys.end()) {diagnostic="Pose perdeu a chave solicitada";return false;}found->value=values[c];
  }
  if(track->rotationMode==AnimationRotationMode::ProgressiveQuaternion&&!rebuildProgress(*track,diagnostic))return false;
  track->sourceOverride=true;if(!candidate.valid(&diagnostic))return false;*this=std::move(candidate);return true;
}
bool AnimationClipAsset::changeRotationMode(u64 trackId,AnimationRotationMode mode,std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;auto candidate=*this;auto *track=candidate.track(trackId);
  if(!track||track->path!=AnimationPath::Rotation||static_cast<u8>(mode)>2) {diagnostic="Canal de rotação ausente ou modo inválido";return false;}
  if(track->rotationMode==mode)return true;
  if(mode==AnimationRotationMode::Euler||track->rotationMode==AnimationRotationMode::Euler) {diagnostic="Trocar Euler exige bake explícito para preservar voltas";return false;}
  if(mode==AnimationRotationMode::ProgressiveQuaternion) {
    const auto &keys=track->curves[0].keys;
    for(usize i=0;i+1<keys.size();++i)for(u32 c=0;c<4;++c)
      if(track->curves[c].keys[i].outgoing!=Mode::Linear||track->curves[c].keys[i+1].incoming!=Mode::Linear) {
        diagnostic="Componentes curvas exigem bake explícito para conversão";return false;
      }
    AnimationCurve progress;double total=0;float previous[4]{};
    for(usize i=0;i<keys.size();++i) {
      float pose[4];for(u32 c=0;c<4;++c)pose[c]=track->curves[c].keys[i].value;
      if(i)total+=rotationDistanceDegrees(previous,pose);
      AnimationCurveKey key;key.id=candidate.nextId++;key.time=keys[i].time;key.value=static_cast<float>(total);linearPose(key);progress.keys.push_back(key);
      std::copy(pose,pose+4,previous);
    }
    track->curves.push_back(std::move(progress));for(u32 c=0;c<4;++c)for(auto &key:track->curves[c].keys)linearPose(key);
  } else {
    const auto &keys=track->curves[4].keys;
    for(usize i=0;i+1<keys.size();++i)if(keys[i].outgoing!=Mode::Linear||keys[i+1].incoming!=Mode::Linear) {
      diagnostic="A curva de velocidade exige bake explícito para conversão";return false;
    }
    track->curves.resize(4);
  }
  track->rotationMode=mode;track->sourceOverride=true;
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::putKey(u64 trackId,u32 component,AnimationCurveKey key,u64 &resultId,std::string &diagnostic) {
  diagnostic.clear();auto candidate=*this;auto *t=candidate.track(trackId);
  if(!t||component>=t->curves.size()) {diagnostic="Canal ausente";return false;}
  auto &curve=t->curves[component];
  const bool progressive=t->rotationMode==AnimationRotationMode::ProgressiveQuaternion;
  if(progressive&&component<4&&(key.incoming!=Mode::Linear||key.outgoing!=Mode::Linear||key.broken||key.weightedIn||key.weightedOut||key.inSlope!=0||key.outSlope!=0)) {
    diagnostic="A pose progressiva não tem handles; edite a curva de progresso";return false;
  }
  if(progressive&&component==4) {
    const auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return key.id?k.id==key.id:k.time==key.time;});
    if((key.id&&found==curve.keys.end())||(found!=curve.keys.end()&&found->value!=key.value)) {diagnostic="A progressão deriva das poses; edite o tempo ou os handles";return false;}
  }
  if(grouped(*t)) {
    const auto old=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return k.id==key.id;});
    if(key.id&&old==curve.keys.end()) {diagnostic="Chave ausente";return false;}
    const usize index=static_cast<usize>(old-curve.keys.begin());
    if(key.id) {
      for(u32 c=0;c<t->components();++c)if(c!=component) {
        auto linked=t->curves[c].keys[index];linked.time=key.time;u64 ignored=0;
        if(!putAnimationCurveKey(t->curves[c],linked,candidate.nextId,ignored)) {diagnostic="Tempos de rotação colidem";return false;}
      }
    } else if(std::none_of(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return k.time==key.time;})) {
      AnimationChannel channel;channel.path=AnimationPath::Rotation;channel.rotationMode=t->rotationMode;channel.curves=t->curves;float pose[4];
      if(!sampleAnimationChannel(channel,key.time,pose)) {diagnostic="Rotação não produz pose válida";return false;}
      if(progressive) {
        u64 ignored=0;auto &progress=t->curves[4];
        if(key.time>=progress.keys.front().time&&key.time<=progress.keys.back().time) {
          if(!splitAnimationCurve(progress,key.time,candidate.nextId,ignored)) {diagnostic="Handle de progressão não pode ser subdividido";return false;}
        } else {
          AnimationCurveSample sample;if(!sampleAnimationCurve(progress,key.time,sample))return false;
          AnimationCurveKey boundary;boundary.time=key.time;boundary.value=static_cast<float>(sample.value);
          boundary.incoming=boundary.outgoing=Mode::Linear;
          if(!putAnimationCurveKey(progress,boundary,candidate.nextId,ignored))return false;
        }
        if(component==4) {const auto found=std::find_if(progress.keys.begin(),progress.keys.end(),[&](const auto &k){return k.time==key.time;});key.id=found->id;key.value=found->value;}
      }
      for(u32 c=0;c<4;++c)if(c!=component) {
        AnimationCurveKey linked;linked.time=key.time;linked.value=pose[c];
        linked.incoming=linked.outgoing=progressive?Mode::Linear:key.incoming;linked.broken=progressive?false:key.broken;
        u64 ignored=0;if(!putAnimationCurveKey(t->curves[c],linked,candidate.nextId,ignored)) {diagnostic="Inserção de rotação recusada";return false;}
      }
    }
  }
  u64 result=0;
  if(progressive&&component<4)linearPose(key);
  if(!putAnimationCurveKey(curve,key,candidate.nextId,result)||(progressive&&!rebuildProgress(*t,diagnostic))||!candidate.valid(&diagnostic)) {
    if(diagnostic.empty())diagnostic="Chave inválida ou tempo ocupado";
    return false;
  }
  t->sourceOverride=true;*this=std::move(candidate);resultId=result;return true;
}
bool AnimationClipAsset::splitKey(u64 trackId,u32 component,float time,u64 &resultId,std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;auto candidate=*this;auto *track=candidate.track(trackId);
  if(!track||component>=track->curves.size()||!std::isfinite(time)||time<0||time>duration) {diagnostic="Canal ou tempo de inserção inválidos";return false;}
  const auto *source=this->track(trackId);const auto &selected=source->curves[component].keys;
  const auto same=std::find_if(selected.begin(),selected.end(),[&](const auto &key){return key.time==time;});
  if(same!=selected.end()) {resultId=same->id;return true;}
  u64 result=0;
  if(time<selected.front().time||time>selected.back().time) {
    if(grouped(*track))for(auto &curve:track->curves) {
      if(!extendHeld(curve,time,candidate.nextId,result)) {diagnostic="Extensão da pose recusada";return false;}
    } else if(!extendHeld(track->curves[component],time,candidate.nextId,result)) {diagnostic="Extensão da curva recusada";return false;}
    if(track->rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
      for(u32 c=0;c<4;++c)for(auto &key:track->curves[c].keys)linearPose(key);
      if(!rebuildProgress(*track,diagnostic))return false;
    }
  } else if(track->rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
    if(!splitAnimationCurve(track->curves[4],time,candidate.nextId,result)||!cropProgressive(*source,*track,0,duration,candidate.nextId,diagnostic)) {
      if(diagnostic.empty())diagnostic="Progressão não pode ser subdividida neste tempo";
      return false;
    }
  } else if(grouped(*track)) {
    const auto &keys=source->curves[0].keys;
    if(time<keys.front().time||time>keys.back().time) {diagnostic="Insira dentro do intervalo de chaves da rotação";return false;}
    const auto upper=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const auto &key){return t<key.time;});
    const usize previous=static_cast<usize>(upper-keys.begin())-1;bool linear=true;
    for(const auto &curve:source->curves)linear&=curve.keys[previous].outgoing==Mode::Linear&&curve.keys[previous+1].incoming==Mode::Linear;
    if(linear) {
      AnimationChannel channel;channel.path=source->path;channel.curves=source->curves;float pose[4];
      if(!sampleAnimationChannel(channel,time,pose))return false;
      for(u32 c=0;c<4;++c) {
        AnimationCurveKey key;key.time=time;key.value=pose[c];linearPose(key);u64 id=0;
        if(!putAnimationCurveKey(track->curves[c],key,candidate.nextId,id))return false;
      }
    } else for(auto &curve:track->curves)if(!splitAnimationCurve(curve,time,candidate.nextId,result)) {diagnostic="Handles de rotação não podem ser subdivididos sem perda";return false;}
  } else if(!splitAnimationCurve(track->curves[component],time,candidate.nextId,result)) {diagnostic="Insira dentro do intervalo da curva; o handle deve ter derivada finita";return false;}
  if(!candidate.valid(&diagnostic))return false;
  const auto found=std::find_if(track->curves[component].keys.begin(),track->curves[component].keys.end(),[&](const auto &key){return key.time==time;});
  if(found==track->curves[component].keys.end()) {diagnostic="Subdivisão perdeu o tempo solicitado";return false;}
  result=found->id;track->sourceOverride=true;*this=std::move(candidate);resultId=result;return true;
}
bool AnimationClipAsset::eraseKey(u64 trackId,u32 component,u64 keyId,std::string &diagnostic) {
  diagnostic.clear();auto candidate=*this;auto *t=candidate.track(trackId);
  if(!t||component>=t->curves.size()) {diagnostic="Canal ausente";return false;}
  const auto &curve=t->curves[component];
  const auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return k.id==keyId;});
  if(found==curve.keys.end()||curve.keys.size()==1) {diagnostic="Preserve ao menos uma chave por canal";return false;}
  const usize index=static_cast<usize>(found-curve.keys.begin());
  if(grouped(*t)) {
    for(auto &c:t->curves) {const auto id=c.keys[index].id;if(!eraseAnimationCurveKeys(c,{&id,1}))return false;}
  } else if(!eraseAnimationCurveKeys(t->curves[component],{&keyId,1}))return false;
  if((t->rotationMode==AnimationRotationMode::ProgressiveQuaternion&&!rebuildProgress(*t,diagnostic))||!candidate.valid(&diagnostic))return false;
  t->sourceOverride=true;*this=std::move(candidate);return true;
}
bool AnimationClipAsset::retime(double scale,std::string &diagnostic) {
  diagnostic.clear();
  if(!valid(&diagnostic)||!std::isfinite(scale)||scale<=0||duration*scale>86400) {diagnostic="Escala de tempo inválida";return false;}
  auto candidate=*this;candidate.duration=static_cast<float>(duration*scale);
  for(auto &cue:candidate.cues)cue.time=static_cast<float>(cue.time*scale);
  for(auto &layer:candidate.layers)if(layer.referenceTime>=0)layer.referenceTime=static_cast<float>(layer.referenceTime*scale);
  for(auto &track:candidate.tracks) {
    for(auto &curve:track.curves) {
      if(!retimeAnimationCurve(curve,scale,0)) {diagnostic="Retime colide chaves ou excede a precisão";return false;}
    }
    track.sourceOverride=true;
  }
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::reverse(std::string &diagnostic) {
  diagnostic.clear();if(!valid(&diagnostic))return false;auto candidate=*this;
  for(auto &cue:candidate.cues) {cue.time=duration-cue.time;std::swap(cue.forward,cue.reverse);}
  std::sort(candidate.cues.begin(),candidate.cues.end(),[](const auto &a,const auto &b){return a.time!=b.time?a.time<b.time:a.id<b.id;});
  for(auto &layer:candidate.layers)if(layer.referenceTime>=0)layer.referenceTime=duration-layer.referenceTime;
  for(auto &track:candidate.tracks) {
    for(auto &curve:track.curves) {
      if(!reverseAnimationCurve(curve,0,duration)) {diagnostic="Inversão excede a precisão de tempo";return false;}
    }
    if(track.rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
      const float total=track.curves[4].keys.front().value;
      for(auto &key:track.curves[4].keys) {key.value=total-key.value;key.inSlope=-key.inSlope;key.outSlope=-key.outSlope;}
    }
    track.sourceOverride=true;
  }
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::crop(float start,float end,std::string &diagnostic) {
  diagnostic.clear();
  if(!valid(&diagnostic)||!std::isfinite(start)||!std::isfinite(end)||start<0||end<start||end>duration) {diagnostic="Intervalo de corte inválido";return false;}
  auto candidate=*this;candidate.duration=end-start;
  std::erase_if(candidate.cues,[&](const auto &c){return c.time<start||c.time>end;});
  for(auto &cue:candidate.cues)cue.time-=start;
  for(auto &layer:candidate.layers)if(layer.referenceTime>=0) {
    if(layer.referenceTime<start||layer.referenceTime>end) {diagnostic="O corte removeria a pose de referência de uma camada; escolha outra referência antes de cortar";return false;}
    layer.referenceTime-=start;
  }
  for(auto &track:candidate.tracks) {
    const auto *source=this->track(track.id);
    if(track.rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
      if(!cropProgressive(*source,track,start,end,candidate.nextId,diagnostic))return false;
      track.sourceOverride=true;continue;
    }
    const auto linearAt=[&](float time) {
      const auto &keys=source->curves[0].keys;
      if(time<=keys.front().time||time>=keys.back().time)return false;
      const auto upper=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const auto &key){return t<key.time;});
      const usize i=static_cast<usize>(upper-keys.begin())-1;
      for(const auto &curve:source->curves)if(curve.keys[i].outgoing!=AnimationTangentMode::Linear||curve.keys[i+1].incoming!=AnimationTangentMode::Linear)return false;
      return true;
    };
    for(auto &curve:track.curves)if(!cropAnimationCurve(curve,start,end,candidate.nextId)) {diagnostic="Corte não preserva os handles desta curva";return false;}
    if(track.path==AnimationPath::Rotation&&track.rotationMode==AnimationRotationMode::Quaternion) {
      AnimationChannel channel;channel.path=AnimationPath::Rotation;channel.curves=source->curves;
      // Scalar Hermite subdivision must not replace quaternion SLERP with
      // normalized scalar lerp. Preserve each geodesic segment and evaluate
      // new boundaries with the actual runtime sampler.
      for(const float boundary:{start,end}) {
        const auto &original=source->curves[0].keys;
        const bool existed=std::any_of(original.begin(),original.end(),[&](const auto &key){return key.time==boundary;});
        if(!existed&&linearAt(boundary)) {
          float q[4];if(!sampleAnimationChannel(channel,boundary,q)) {diagnostic="Limite de rotação inválido";return false;}
          for(u32 c=0;c<4;++c)for(auto &key:track.curves[c].keys)if(key.time==boundary-start)key.value=q[c];
        }
      }
      const auto &keys=track.curves[0].keys;
      for(usize i=0;i+1<keys.size();++i)if(linearAt(start+(keys[i].time+keys[i+1].time)*.5f))
        for(auto &curve:track.curves) {
          curve.keys[i].outgoing=AnimationTangentMode::Linear;curve.keys[i].broken=true;
          curve.keys[i+1].incoming=AnimationTangentMode::Linear;curve.keys[i+1].broken=true;
        }
    }
    track.sourceOverride=true;
  }
  if(!candidate.valid(&diagnostic))return false;
  *this=std::move(candidate);return true;
}
} // namespace ae::resources
