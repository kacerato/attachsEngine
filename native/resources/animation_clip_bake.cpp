#include "resources/animation_clip_bake.h"
#include "resources/animation_clip_edit_helpers.h"
#include "core/rotation_math.h"
#include <array>
#include <set>
#include <limits>
#include <sstream>

namespace ae::resources {
namespace {
using Mode=AnimationTangentMode;
struct Frame {float time=0;std::array<float,MaximumMorphTargets> value{};};
AnimationChannel channel(const AnimationClipTrack &t) {
  AnimationChannel c;c.path=t.path;c.rotationMode=t.rotationMode;c.weightCount=t.weightCount;c.curves=t.curves;return c;
}
bool step(const AnimationCurveKey &a,const AnimationCurveKey &b) {
  return a.outgoing==Mode::Constant||a.outgoing==Mode::NextConstant||b.incoming==Mode::Constant||b.incoming==Mode::NextConstant;
}
usize keys(const AnimationClipTrack &track) {usize n=0;for(const auto &c:track.curves)n+=c.keys.size();return n;}
}
static bool bakeEuler(AnimationClipAsset &,u64,const AnimationBakeSettings &,AnimationBakeReport &,std::string &,
    const std::function<bool()> &,const AnimationChannel *);
static bool bakeTrack(AnimationClipAsset &asset,u64 id,const AnimationBakeSettings &settings,
    AnimationBakeReport &report,std::string &error,const std::function<bool()> &cancelled,const AnimationChannel *composed=nullptr) {
  error.clear();report={};
  auto reject=[&](const char *why){error=why;return false;};
  if(!asset.valid(&error))return false;
  if(!settings.sampleRate||settings.sampleRate>240||settings.verificationSteps<2||settings.verificationSteps>16||
     settings.maximumFrames<2||settings.maximumFrames>65536||!std::isfinite(settings.tolerance)||settings.tolerance<=0||settings.rotation>3)
    return reject("Configuração de bake inválida");
  const auto *original=asset.track(id);if(!original)return reject("Canal de bake inexistente");
  const bool rotation=original->path==AnimationPath::Rotation;
  const auto mode=settings.rotation==3?original->rotationMode:static_cast<AnimationRotationMode>(settings.rotation);
  if(!rotation&&settings.rotation!=3)return reject("Modo de rotação exige um canal de rotação");
  if(rotation&&mode==AnimationRotationMode::Euler&&(original->rotationMode!=mode||composed))
    return bakeEuler(asset,id,settings,report,error,cancelled,composed);
  const bool euler=rotation&&mode==AnimationRotationMode::Euler;
  const u32 components=rotation?(euler?3u:4u):original->components();
  const u32 outputComponents=rotation?rotationCurveComponents(mode):components;
  const usize otherKeys=[&]{usize n=0;for(const auto &t:asset.tracks)if(t.id!=id)n+=keys(t);return n;}();
  const usize limit=std::min<usize>(settings.maximumFrames,(AnimationClipAsset::MaximumKeys-otherKeys)/outputComponents);
  if(limit<2)return reject("Orçamento do clipe não permite bake");
  const double regularCount=std::ceil(double(asset.duration)*settings.sampleRate);
  if(regularCount>double(limit-1))return reject("Duração e taxa excedem o orçamento de bake");
  auto source=composed?*composed:channel(*original);
  std::vector<const AnimationChannel *> sources;
  if(source.layerSources.empty())sources.push_back(&source);
  else for(const auto &layer:source.layerSources)sources.push_back(&layer);
  std::vector<const AnimationCurve *> curves;
  for(const auto *s:sources)for(const auto &curve:s->curves)curves.push_back(&curve);
  std::set<float> times{0,asset.duration},locked{0,asset.duration};
  for(const auto *entry:curves) {const auto &curve=*entry;
    for(usize i=0;i<curve.keys.size();++i) {
      const auto t=curve.keys[i].time;times.insert(t);
      // Steps have distinct values on either side, even when reversed. Retain
      // the adjacent representable runtime times; do not smooth across them.
      if((i&&step(curve.keys[i-1],curve.keys[i]))||(i+1<curve.keys.size()&&step(curve.keys[i],curve.keys[i+1]))) {
        locked.insert(t);
        for(float adjacent:{std::nextafter(t,-std::numeric_limits<float>::infinity()),std::nextafter(t,std::numeric_limits<float>::infinity())})
          if(adjacent>=0&&adjacent<=asset.duration) {times.insert(adjacent);locked.insert(adjacent);}
      }
    }
    std::vector<float> extrema;if(!animationCurveExtremaTimes(curve,extrema))return reject("Extremos da curva inválidos");
    times.insert(extrema.begin(),extrema.end());
    if(times.size()>limit)return reject("Chaves e descontinuidades excedem o orçamento de bake");
  }
  for(u32 i=1;i<static_cast<u32>(regularCount);++i)times.insert(static_cast<float>(double(i)/settings.sampleRate));
  if(times.size()>limit)return reject("Grade de bake excede o orçamento");
  u32 evaluations=0;
  auto sample=[&](float t,Frame &frame) {
    if(++evaluations>2000000) {error="Bake excede o orçamento de avaliação";return false;}
    if((evaluations==1||!(evaluations%256))&&cancelled&&cancelled()) {error="Bake cancelado";return false;}
    frame.time=t;
    if(euler)for(u32 c=0;c<3;++c) {
      AnimationCurveSample value;
      if(!sampleValidatedAnimationCurve(source.curves[c],t,value)||!std::isfinite(value.value)||std::abs(value.value)>std::numeric_limits<float>::max()) {
        error="Origem Euler não produz valor finito no runtime";return false;
      }
      frame.value[c]=static_cast<float>(value.value);
    }
    else if(!sampleValidatedAnimationChannel(source,t,{frame.value.data(),components})) {error="Origem não produz pose válida";return false;}
    return true;
  };
  auto interpolate=[&](const Frame &a,const Frame &b,float t,Frame &out) {
    const double alpha=(double(t)-a.time)/(double(b.time)-a.time);out.time=t;
    if(rotation&&!euler)return interpolateRotationArc(a.value.data(),b.value.data(),alpha,out.value.data());
    for(u32 c=0;c<components;++c)out.value[c]=static_cast<float>(double(a.value[c])+(double(b.value[c])-a.value[c])*alpha);
    return true;
  };
  auto distance=[&](const Frame &a,const Frame &b) {
    double result=0;
    if(rotation) {
      if(!euler)return rotationDistanceDegrees(a.value.data(),b.value.data());
      float qa[4],qb[4];rotationQuaternionXYZ(a.value.data(),qa);rotationQuaternionXYZ(b.value.data(),qb);
      result=rotationDistanceDegrees(qa,qb);
    }
    for(u32 c=0;c<components;++c)result=std::max(result,std::abs(double(a.value[c])-b.value[c]));
    return result;
  };
  auto safeTravel=[&](const Frame &a,const Frame &b) {
    if(!rotation)return true;
    // Pose-only tests can alias complete Euler turns. Extrema are already
    // boundaries; limiting raw travel on their monotonic pieces avoids that.
    double travel=0;
    for(const auto *s:sources) {
    if(s->rotationMode==AnimationRotationMode::Quaternion)continue;
    const u32 start=s->rotationMode==AnimationRotationMode::Euler?0:4;
    for(u32 c=start;c<s->curves.size();++c) {
      AnimationCurveSample x,y;
      if(!sampleValidatedAnimationCurve(s->curves[c],a.time,x)||!sampleValidatedAnimationCurve(s->curves[c],b.time,y))return false;
      travel+=std::abs(y.value-x.value);
    }
    }
    return travel<=90;
  };
  std::vector<Frame> frames;frames.reserve(times.size());Frame first;
  if(!sample(*times.begin(),first))return false;
  frames.push_back(first);
  struct Span {Frame a,b;u32 depth;};
  auto previous=times.begin();
  for(auto it=std::next(previous);it!=times.end();++it) {
    Frame end;if(!sample(*it,end))return false;
    std::vector<Span> work{{frames.back(),end,0}};
    while(!work.empty()) {
      auto span=work.back();work.pop_back();bool split=!safeTravel(span.a,span.b);
      for(u32 j=1;j<settings.verificationSteps;++j) {
        const auto t=static_cast<float>(double(span.a.time)+(double(span.b.time)-span.a.time)*j/settings.verificationSteps);
        if(t<=span.a.time||t>=span.b.time)continue;
        Frame actual,linear;if(!sample(t,actual))return false;
        if(!interpolate(span.a,span.b,t,linear))return reject("Interpolação de bake inválida");
        const auto err=distance(actual,linear);if(!std::isfinite(err))return reject("Erro de bake não finito");
        split|=err>settings.tolerance*.45;
      }
      if(split) {
        const float t=static_cast<float>((double(span.a.time)+span.b.time)*.5);Frame middle;
        if(span.depth>=192||t<=span.a.time||t>=span.b.time) {
          std::ostringstream message;message<<"Tolerância de bake excede a precisão dos tempos · canal "<<id<<" · "<<span.a.time<<".."<<span.b.time<<" · profundidade "<<span.depth;
          error=message.str();return false;
        }
        if(frames.size()+work.size()+2>=limit)return reject("Refinamento excede o orçamento de bake");
        if(!sample(t,middle))return false;
        work.push_back({middle,span.b,span.depth+1});work.push_back({span.a,middle,span.depth+1});
      } else {frames.push_back(span.b);if(frames.size()>limit)return reject("Bake excede o orçamento de chaves");}
    }
  }
  std::vector<bool> retained(frames.size(),!settings.reduce);
  retained.front()=retained.back()=true;
  for(usize i=0;i<frames.size();++i)if(locked.contains(frames[i].time))retained[i]=true;
  if(settings.reduce) {
    std::vector<std::pair<usize,usize>> work;usize start=0;
    for(usize i=1;i<frames.size();++i)if(retained[i]) {work.push_back({start,i});start=i;}
    u32 comparisons=0;
    while(!work.empty()) {
      const auto [a,b]=work.back();work.pop_back();double worst=settings.tolerance*.45;usize chosen=0;
      for(usize i=a+1;i<b;++i) {
        if(++comparisons>2000000)return reject("Redução excede o orçamento de comparação");
        if(!(comparisons%256)&&cancelled&&cancelled())return reject("Redução cancelada");
        Frame linear;if(!interpolate(frames[a],frames[b],frames[i].time,linear))return reject("Redução não produz pose válida");
        const double err=distance(frames[i],linear);
        if(!std::isfinite(err))return reject("Erro da redução não finito");
        if(err>worst) {worst=err;chosen=i;}
      }
      if(chosen) {retained[chosen]=true;work.push_back({a,chosen});work.push_back({chosen,b});}
    }
  }
  auto candidate=asset;auto *target=candidate.track(id);target->rotationMode=mode;target->curves.assign(outputComponents,{});target->sourceOverride=true;
  auto add=[&](u32 c,const Frame &frame) {
    AnimationCurveKey key;key.time=frame.time;key.value=frame.value[c];key.incoming=key.outgoing=Mode::Linear;
    if(c<original->curves.size()) {
      const auto &old=original->curves[c].keys;const auto it=std::lower_bound(old.begin(),old.end(),key.time,[](const auto &k,float t){return k.time<t;});
      if(it!=old.end()&&it->time==key.time)key.id=it->id;
    }
    if(!key.id) {if(candidate.nextId==std::numeric_limits<u64>::max())return false;key.id=candidate.nextId++;}
    target->curves[c].keys.push_back(key);return true;
  };
  for(usize i=0;i<frames.size();++i)if(retained[i])for(u32 c=0;c<outputComponents;++c)if(!add(c,frames[i]))return reject("Identidades de chave esgotadas");
  if(rotation&&mode==AnimationRotationMode::ProgressiveQuaternion&&!rebuildProgressiveAnimationTrack(*target,error))return false;
  if(!candidate.valid(&error))return false;
  auto baked=channel(*target);AnimationBakeReport result;
  result.inputKeys=static_cast<u32>(keys(*original));result.outputKeys=static_cast<u32>(keys(*target));result.sampledFrames=static_cast<u32>(frames.size());
  auto verify=[&](float t) {
    Frame actual,test;if(!sample(t,actual))return false;
    if(euler)for(u32 c=0;c<3;++c) {
      AnimationCurveSample value;
      if(!sampleValidatedAnimationCurve(baked.curves[c],t,value)||!std::isfinite(value.value)||std::abs(value.value)>std::numeric_limits<float>::max()) {
        error="Resultado Euler não produz valor finito no runtime";return false;
      }
      test.value[c]=static_cast<float>(value.value);
    } else if(!sampleValidatedAnimationChannel(baked,t,{test.value.data(),components})) {
      error="Resultado de bake não produz pose válida";return false;
    }
    const auto err=distance(actual,test);if(!std::isfinite(err)||err>settings.tolerance) {error="Bake não atende à tolerância na verificação";return false;}
    result.maximumError=std::max(result.maximumError,err);++result.verifiedSamples;return true;
  };
  for(usize i=0;i<frames.size();++i) {
    if(!verify(frames[i].time))return false;
    if(i)for(u32 j=1;j<settings.verificationSteps;++j) {
      const auto t=static_cast<float>(double(frames[i-1].time)+(double(frames[i].time)-frames[i-1].time)*j/settings.verificationSteps);
      if(t>frames[i-1].time&&t<frames[i].time&&!verify(t))return false;
    }
  }
  if(cancelled&&cancelled())return reject("Bake cancelado");
  asset=std::move(candidate);report=result;return true;
}
static bool bakeEuler(AnimationClipAsset &asset,u64 id,const AnimationBakeSettings &settings,
    AnimationBakeReport &report,std::string &error,const std::function<bool()> &cancelled,const AnimationChannel *composed) {
  if(!settings.eulerReferenceExplicit) {error="Conversão Euler exige uma referência XYZ explícita";return false;}
  for(float value:settings.eulerReference)if(!std::isfinite(value)||std::abs(value)>1e7f) {error="Referência Euler inválida";return false;}
  const auto *original=asset.track(id);const auto source=composed?*composed:channel(*original);
  auto candidate=asset;auto dense=settings;dense.rotation=0;dense.reduce=false;dense.tolerance*=.2;
  AnimationBakeReport initial;
  if(!bakeTrack(candidate,id,dense,initial,error,cancelled,composed))return false;
  const auto *quaternion=candidate.track(id);
  std::set<float> times;for(const auto &key:quaternion->curves[0].keys)times.insert(key.time);
  const usize otherKeys=[&]{usize n=0;for(const auto &t:candidate.tracks)if(t.id!=id)n+=keys(t);return n;}();
  const usize limit=std::min<usize>(settings.maximumFrames,(AnimationClipAsset::MaximumKeys-otherKeys)/3);
  std::vector<Frame> frames;u32 evaluations=0;
  const auto pose=[&](float t,float q[4]) {
    if(++evaluations>2000000) {error="Conversão Euler excede o orçamento";return false;}
    if((evaluations==1||!(evaluations%256))&&cancelled&&cancelled()) {error="Conversão Euler cancelada";return false;}
    if(!sampleValidatedAnimationChannel(source,t,{q,4})) {error="Conversão Euler não produz orientação válida";return false;}
    return true;
  };
  // Rebuild in chronological order after each refinement. Branch selection is
  // deterministic and never depends on the order in which probes are queried.
  for(u32 round=0;;++round) {
    frames.clear();float reference[3];std::copy_n(settings.eulerReference,3,reference);
    for(float t:times) {
      float q[4];Frame f;f.time=t;
      if(!pose(t,q)||!rotationEulerXYZNear(q,reference,f.value.data())) {if(error.empty())error="Não foi possível levantar o ramo Euler";return false;}
      std::copy_n(f.value.data(),3,reference);frames.push_back(f);
    }
    std::vector<float> additions;
    for(usize i=1;i<frames.size();++i) {
      const auto &a=frames[i-1],&b=frames[i];bool split=false;
      for(u32 c=0;c<3;++c)split|=std::abs(double(b.value[c])-a.value[c])>90;
      for(u32 j=1;j<settings.verificationSteps;++j) {
        const double alpha=double(j)/settings.verificationSteps;
        const float t=static_cast<float>(double(a.time)+(double(b.time)-a.time)*alpha);
        if(t<=a.time||t>=b.time)continue;
        float q[4],angles[3],test[4];if(!pose(t,q))return false;
        for(u32 c=0;c<3;++c)angles[c]=static_cast<float>(double(a.value[c])+(double(b.value[c])-a.value[c])*alpha);
        rotationQuaternionXYZ(angles,test);const double distance=rotationDistanceDegrees(q,test);
        if(!std::isfinite(distance)) {error="Erro Euler não finito";return false;}
        split|=distance>settings.tolerance*.2;
      }
      const float middle=static_cast<float>((double(a.time)+b.time)*.5);
      // An adjacent pair of representable runtime times may be a step.
      if(split&&middle>a.time&&middle<b.time)additions.push_back(middle);
    }
    if(additions.empty())break;
    if(round>=24||times.size()+additions.size()>limit) {error="Continuidade Euler excede a precisão ou orçamento";return false;}
    times.insert(additions.begin(),additions.end());
  }
  auto *target=candidate.track(id);target->rotationMode=AnimationRotationMode::Euler;target->curves.assign(3,{});target->sourceOverride=true;
  for(const auto &frame:frames)for(u32 c=0;c<3;++c) {
    if(candidate.nextId==std::numeric_limits<u64>::max()) {error="Identidades de chave esgotadas";return false;}
    AnimationCurveKey key;key.id=candidate.nextId++;key.time=frame.time;key.value=frame.value[c];key.incoming=key.outgoing=Mode::Linear;
    target->curves[c].keys.push_back(key);
  }
  auto reduction=settings;reduction.rotation=3;reduction.tolerance*=.2;
  AnimationBakeReport reduced;if(!bakeTrack(candidate,id,reduction,reduced,error,cancelled))return false;
  const auto baked=channel(*candidate.track(id));AnimationBakeReport result=reduced;
  result.inputKeys=static_cast<u32>(keys(*original));result.sampledFrames=static_cast<u32>(frames.size());result.verifiedSamples=0;result.maximumError=0;
  const auto verify=[&](float t) {
    float q[4],test[4];if(!pose(t,q)||!sampleValidatedAnimationChannel(baked,t,{test,4}))return false;
    const double distance=rotationDistanceDegrees(q,test);
    if(!std::isfinite(distance)||distance>settings.tolerance) {error="Euler não atende à tolerância de orientação";return false;}
    result.maximumError=std::max(result.maximumError,distance);++result.verifiedSamples;return true;
  };
  for(usize i=0;i<frames.size();++i) {
    if(!verify(frames[i].time))return false;
    if(i)for(u32 j=1;j<settings.verificationSteps;++j) {
      const float t=static_cast<float>(double(frames[i-1].time)+(double(frames[i].time)-frames[i-1].time)*j/settings.verificationSteps);
      if(t>frames[i-1].time&&t<frames[i].time&&!verify(t))return false;
    }
  }
  if(cancelled&&cancelled()) {error="Conversão Euler cancelada";return false;}
  asset=std::move(candidate);report=result;return true;
}
bool bakeAnimationClipTrack(AnimationClipAsset &asset,u64 id,const AnimationBakeSettings &settings,
    AnimationBakeReport &report,std::string &error,const std::function<bool()> &cancelled) {
  return bakeTrack(asset,id,settings,report,error,cancelled);
}
bool consolidateAnimationClip(const AnimationClipAsset &source,AssetGuid guid,std::string_view name,
    const AnimationBakeSettings &settings,AnimationClipAsset &out,AnimationBakeReport &report,
    std::string &error,const std::function<bool()> &cancelled) {
  error.clear();report={};
  if(!guid.valid()||guid==source.guid) {error="Consolidação exige identidade nova";return false;}
  AnimationClip compiled;if(!source.compile(compiled,&error))return false;
  auto candidate=source;candidate.guid=guid;candidate.name=name;candidate.revision=1;
  // Original resource remains the provenance; source import GUID/hash and
  // bindings retain their existing meaning and dependency contract.
  std::erase_if(candidate.tracks,[](const auto &t){return t.layer!=0;});
  candidate.layers={AnimationClipLayer{}};
  AnimationBakeReport result;for(const auto &t:source.tracks)result.inputKeys+=static_cast<u32>(keys(t));
  for(const auto &c:compiled.channels) {
    const u64 binding=source.bindings[c.node].id;u64 id=0;
    for(const auto &t:candidate.tracks)if(t.binding==binding&&t.path==c.path) {id=t.id;break;}
    if(!id) {error="Composição perdeu o binding da Base";return false;}
    auto settingsForTrack=settings;
    if(c.path!=AnimationPath::Rotation)settingsForTrack.rotation=3;
    else if(settingsForTrack.rotation==3)settingsForTrack.rotation=0;
    AnimationBakeReport channelReport;
    if(!bakeTrack(candidate,id,settingsForTrack,channelReport,error,cancelled,&c))return false;
    result.outputKeys+=channelReport.outputKeys;result.sampledFrames+=channelReport.sampledFrames;
    result.verifiedSamples+=channelReport.verifiedSamples;result.maximumError=std::max(result.maximumError,channelReport.maximumError);
  }
  if(!candidate.valid(&error))return false;
  out=std::move(candidate);report=result;return true;
}
}
