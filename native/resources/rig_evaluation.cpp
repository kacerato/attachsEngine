#include "resources/rig_evaluation.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <set>

namespace ae::resources {
namespace {
bool fail(std::string &error,std::string message) {error=std::move(message);return false;}
std::array<float,3> subtract(const std::array<float,3> &a,const std::array<float,3> &b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
std::array<float,3> reflected(const std::array<float,3> &v,const std::array<float,3> &normal,float offset=0) {
  const float d=2*(v[0]*normal[0]+v[1]*normal[1]+v[2]*normal[2]-offset);
  return {v[0]-d*normal[0],v[1]-d*normal[1],v[2]-d*normal[2]};
}
std::array<float,4> reflectedRotation(const std::array<float,4> &q,const std::array<float,3> &normal) {
  const auto v=reflected({q[0],q[1],q[2]},normal);return {-v[0],-v[1],-v[2],q[3]};
}
bool ancestor(const CompiledRig &rig,u32 child,u32 parent) {
  for(i32 at=static_cast<i32>(child);at>=0;at=rig.joints[at].parent)if(static_cast<u32>(at)==parent)return true;
  return false;
}
}
bool compileRig(const RigAsset &asset,CompiledRig &out,std::string &error) {
  error.clear();if(!asset.valid(&error))return false;
  CompiledRig value;value.guid=asset.guid;value.revision=asset.revision;
  std::unordered_map<u64,usize> byId;std::vector<std::vector<usize>> children(asset.joints.size());
  for(usize i=0;i<asset.joints.size();++i)byId.emplace(asset.joints[i].id,i);
  std::deque<usize> queue;
  for(usize i=0;i<asset.joints.size();++i) {
    if(asset.joints[i].parent)children[byId.at(asset.joints[i].parent)].push_back(i);else queue.push_back(i);
  }
  value.joints.reserve(asset.joints.size());value.index.reserve(asset.joints.size());
  while(!queue.empty()) {
    const auto at=queue.front();queue.pop_front();const auto &j=asset.joints[at];
    CompiledRigJoint item;item.id=j.id;item.parent=j.parent?static_cast<i32>(value.index.at(j.parent)):-1;
    item.bind=j.bind;item.reference=j.reference;
    for(const auto &control:asset.controls)if(control.joint==j.id){item.limit=control.limit;break;}
    item.referenceWorld=item.parent<0?j.reference:composeRigTransform(value.joints[item.parent].referenceWorld,j.reference);
    if(!validRigTransform(item.referenceWorld))return fail(error,"Acúmulo de escala/posição excede limites do rig: "+j.name);
    value.index.emplace(j.id,static_cast<u32>(value.joints.size()));value.joints.push_back(item);
    for(auto child:children[at])queue.push_back(child);
  }
  out=std::move(value);return true;
}
bool rigWorldPose(const CompiledRig &rig,std::span<const RigTransform> local,std::span<RigTransform> world) {
  if(local.size()!=rig.joints.size()||world.size()!=local.size())return false;
  for(usize i=0;i<local.size();++i) {
    if(!validRigTransform(local[i]))return false;
    const auto parent=rig.joints[i].parent;
    world[i]=parent<0?local[i]:composeRigTransform(world[parent],local[i]);
    if(!validRigTransform(world[i]))return false;
  }
  return true;
}
bool blendRigPose(const CompiledRig &rig,const RigPose &pose,float weight,std::span<const RigTransform> input,
    std::span<RigTransform> output,std::string &error,bool acceptRevisedRig) {
  error.clear();if(!pose.valid(&error))return false;
  if(pose.rig!=rig.guid||(!acceptRevisedRig&&pose.rigRevision!=rig.revision))return fail(error,"Pose requer rig compatível e revisão revalidada.");
  if(input.size()!=rig.joints.size()||output.size()!=input.size()||!std::isfinite(weight)||weight<0||weight>1)return fail(error,"Peso ou buffers de pose inválidos.");
  for(const auto &t:input)if(!validRigTransform(t))return fail(error,"Pose de entrada inválida.");
  for(const auto &entry:pose.entries)if(!rig.index.contains(entry.joint))return fail(error,"Pose contém junta removida: "+std::to_string(entry.joint));
  std::vector<RigTransform> result(input.begin(),input.end());
  for(const auto &entry:pose.entries) {
    auto &t=result[rig.index.at(entry.joint)];
    if(entry.channels&RigPosition)for(u32 c=0;c<3;++c)t.position[c]+=weight*(entry.local.position[c]-t.position[c]);
    if(entry.channels&RigScale)for(u32 c=0;c<3;++c)t.scale[c]+=weight*(entry.local.scale[c]-t.scale[c]);
    if(entry.channels&RigRotation&&!interpolateRotationArc(t.rotation.data(),entry.local.rotation.data(),weight,t.rotation.data()))return fail(error,"Rotação não representável na pose.");
    if(!constrainRigRotation(rig.joints[rig.index.at(entry.joint)].limit,rig.joints[rig.index.at(entry.joint)].reference.rotation,t.rotation))return fail(error,"Limite da pose inválido.");
  }
  std::vector<RigTransform> world(result.size());
  if(!rigWorldPose(rig,result,world))return fail(error,"Pose acumulada excede limites do rig.");
  std::copy(result.begin(),result.end(),output.begin());
  return true;
}
bool applyRigControl(const RigAsset &asset,const CompiledRig &rig,u64 controlId,const RigTransform &value,
    std::span<const RigTransform> input,std::span<RigTransform> output,std::string &error) {
  error.clear();
  if(asset.guid!=rig.guid||asset.revision!=rig.revision||!asset.valid(&error))return fail(error,"Rig do controle exige revalidação.");
  const auto *control=asset.control(controlId);
  if(!control||!validRigTransform(value)||input.size()!=rig.joints.size()||output.size()!=input.size())return fail(error,"Controle, pose ou buffers inválidos.");
  std::vector<RigTransform> world(input.size());
  if(!rigWorldPose(rig,input,world))return fail(error,"Pose atual não representável pelo rig.");
  const auto index=rig.index.at(control->joint);const auto parent=rig.joints[index].parent;const RigTransform identity;
  const auto wanted=control->space==RigControlSpace::LocalReference?composeRigTransform(rig.joints[index].reference,value):relativeRigTransform(value,parent<0?identity:world[parent]);
  auto local=input[index];
  if(control->channels&RigPosition)local.position=wanted.position;
  if(control->channels&RigRotation)local.rotation=wanted.rotation;
  if(control->channels&RigScale)local.scale=wanted.scale;
  if(!constrainRigRotation(control->limit,rig.joints[index].reference.rotation,local.rotation)||!validRigTransform(local))return fail(error,"Controle excede limites representáveis.");
  // Validate descendants under the changed pose before publishing any value.
  std::vector<RigTransform> result(input.begin(),input.end());result[index]=local;
  if(!rigWorldPose(rig,result,world))return fail(error,"Controle faz a cadeia exceder limites representáveis.");
  std::copy(result.begin(),result.end(),output.begin());return true;
}
bool mirrorRigPose(const RigAsset &asset,const CompiledRig &rig,std::span<const RigTransform> input,
    std::span<RigTransform> output,u8 channels,bool referenceMode,std::string &error) {
  error.clear();if(asset.guid!=rig.guid||asset.revision!=rig.revision||!asset.valid(&error))return fail(error,"Rig de espelho precisa ser revalidado.");
  if(!channels||channels>RigAll||input.size()!=rig.joints.size()||output.size()!=input.size()||asset.mirror.empty())return fail(error,"Selecione canais e pares de espelho válidos.");
  std::vector<RigTransform> world(input.size()),desired(input.size());
  if(!rigWorldPose(rig,input,world))return fail(error,"Pose de espelho inválida.");
  desired=world;
  const auto copySide=[&](u64 sourceControl,u64 targetControl) {
    const auto *a=asset.control(sourceControl),*b=asset.control(targetControl);
    const auto src=rig.index.at(a->joint),dst=rig.index.at(b->joint);const u8 mask=channels&a->channels&b->channels;
    if(mask&RigPosition) {
      if(referenceMode) {
        const auto delta=reflected(subtract(world[src].position,rig.joints[src].referenceWorld.position),asset.mirrorNormal);
        for(u32 c=0;c<3;++c)desired[dst].position[c]=rig.joints[dst].referenceWorld.position[c]+delta[c];
      } else desired[dst].position=reflected(world[src].position,asset.mirrorNormal,asset.mirrorOffset);
    }
    if(mask&RigRotation) {
      const auto source=referenceMode?multiplyRigRotation(world[src].rotation,inverseRigRotation(rig.joints[src].referenceWorld.rotation)):world[src].rotation;
      desired[dst].rotation=reflectedRotation(source,asset.mirrorNormal);
      if(referenceMode)desired[dst].rotation=multiplyRigRotation(desired[dst].rotation,rig.joints[dst].referenceWorld.rotation);
    }
    if(mask&RigScale) {
      for(u32 c=0;c<3;++c)desired[dst].scale[c]=referenceMode?rig.joints[dst].referenceWorld.scale[c]*world[src].scale[c]/rig.joints[src].referenceWorld.scale[c]:world[src].scale[c];
    }
  };
  for(const auto &pair:asset.mirror){copySide(pair.left,pair.right);if(pair.left!=pair.right)copySide(pair.right,pair.left);}
  // Unmapped children keep local transforms, rather than counteracting their
  // mirrored parent. Only controls explicitly in the map acquire a world goal.
  std::unordered_map<u64,u8> mapped;
  for(const auto &p:asset.mirror) {
    const auto *left=asset.control(p.left),*right=asset.control(p.right);const u8 mask=channels&left->channels&right->channels;
    mapped.emplace(left->joint,mask);mapped.emplace(right->joint,mask);
  }
  std::vector<RigTransform> result(input.size());
  for(usize i=0;i<input.size();++i) {
    const auto parent=rig.joints[i].parent;const RigTransform identity;
    result[i]=input[i];
    if(const auto found=mapped.find(rig.joints[i].id);found!=mapped.end()) {
      const auto mirroredLocal=relativeRigTransform(desired[i],parent<0?identity:desired[parent]);
      if(found->second&RigPosition)result[i].position=mirroredLocal.position;
      if(found->second&RigRotation)result[i].rotation=mirroredLocal.rotation;
      if(found->second&RigScale)result[i].scale=mirroredLocal.scale;
      if(found->second&RigRotation)for(const auto &control:asset.controls)if(control.joint==rig.joints[i].id&&!constrainRigRotation(control.limit,rig.joints[i].reference.rotation,result[i].rotation))return fail(error,"Limite de espelho inválido.");
    }
    desired[i]=parent<0?result[i]:composeRigTransform(desired[parent],result[i]);
    if(!validRigTransform(result[i])||!validRigTransform(desired[i]))return fail(error,"Espelho excede limites representáveis.");
  }
  std::copy(result.begin(),result.end(),output.begin());return true;
}
void RetargetWorkspace::prepare(const CompiledRetarget &r) {sourceWorld.resize(r.source.joints.size());targetWorld.resize(r.target.joints.size());idealWorld.resize(r.target.joints.size());errors.resize(r.target.joints.size());}
bool compileRetarget(const RigAsset &source,const RigAsset &target,const RetargetProfile &profile,CompiledRetarget &out,std::string &error) {
  error.clear();if(!profile.valid(&error))return false;
  if(profile.source!=source.guid||profile.target!=target.guid||profile.sourceRevision!=source.revision||profile.targetRevision!=target.revision)
    return fail(error,"Revisão de um rig mudou; revalide correspondências sem apagar correções.");
  CompiledRetarget value;if(!compileRig(source,value.source,error)||!compileRig(target,value.target,error))return false;
  value.motionScale=profile.motionScale;value.allowUnmappedSource=profile.allowUnmappedSource;
  value.targetMapping.assign(value.target.joints.size(),-1);
  for(const auto &entry:profile.mapping) {
    const auto src=value.source.index.find(entry.source),dst=value.target.index.find(entry.target);
    if(src==value.source.index.end()||dst==value.target.index.end())return fail(error,"Correspondência referencia junta ausente.");
    value.mapping.push_back({src->second,dst->second,entry,multiplyRigRotation(profile.facing,entry.correction)});
  }
  std::sort(value.mapping.begin(),value.mapping.end(),[](const auto &a,const auto &b){return a.target<b.target;});
  for(usize i=0;i<value.mapping.size();++i)value.targetMapping[value.mapping[i].target]=static_cast<i32>(i);
  for(const auto &a:value.mapping)for(const auto &b:value.mapping)if(a.source!=b.source&&ancestor(value.source,b.source,a.source)&&!ancestor(value.target,b.target,a.target))
    return fail(error,"Mapa inverte parentesco das cadeias de origem e destino.");
  out=std::move(value);return true;
}
bool evaluateRetarget(const CompiledRetarget &r,std::span<const RigTransform> sourceLocal,std::span<RigTransform> targetLocal,RetargetWorkspace &scratch) {
  if(sourceLocal.size()!=r.source.joints.size()||targetLocal.size()!=r.target.joints.size()||scratch.sourceWorld.size()!=sourceLocal.size()||scratch.targetWorld.size()!=targetLocal.size()||scratch.idealWorld.size()!=targetLocal.size()||scratch.errors.size()!=targetLocal.size()||
      !rigWorldPose(r.source,sourceLocal,scratch.sourceWorld))return false;
  scratch.maximumPositionError=0;scratch.maximumAngularError=0;scratch.limitedJoints=0;
  for(usize i=0;i<r.target.joints.size();++i) {
    const auto &target=r.target.joints[i];auto local=target.reference;const RigTransform identity;
    const auto &targetParent=target.parent<0?identity:scratch.targetWorld[target.parent];
    const auto map=r.targetMapping[i];
    if(map>=0) {
      const auto &m=r.mapping[map];const auto &source=r.source.joints[m.source];const auto &sample=scratch.sourceWorld[m.source];
      const auto delta=multiplyRigRotation(sample.rotation,inverseRigRotation(source.referenceWorld.rotation));
      const auto transferred=multiplyRigRotation(multiplyRigRotation(m.motionFrame,delta),inverseRigRotation(m.motionFrame));
      local.rotation=multiplyRigRotation(inverseRigRotation(targetParent.rotation),multiplyRigRotation(transferred,target.referenceWorld.rotation));
      if(m.policy.position==RetargetPosition::ReconstructWorld) {
        auto movement=rotateRigVector(m.motionFrame,subtract(sample.position,source.referenceWorld.position));
        auto wanted=target.referenceWorld;
        for(u32 c=0;c<3;++c)wanted.position[c]+=movement[c]*r.motionScale;
        local.position=relativeRigTransform(wanted,targetParent).position;
      } else if(m.policy.position!=RetargetPosition::Reference) {
        const auto &sourceParent=source.parent<0?identity:r.source.joints[source.parent].referenceWorld;
        const auto &targetReferenceParent=target.parent<0?identity:r.target.joints[target.parent].referenceWorld;
        auto deltaPosition=rotateRigVector(sourceParent.rotation,subtract(sourceLocal[m.source].position,source.reference.position));
        deltaPosition=rotateRigVector(inverseRigRotation(targetReferenceParent.rotation),rotateRigVector(m.motionFrame,deltaPosition));
        const float scale=m.policy.position==RetargetPosition::ScaledOffset?r.motionScale:1.f;
        for(u32 c=0;c<3;++c)local.position[c]+=deltaPosition[c]*sourceParent.scale[c]/targetReferenceParent.scale[c]*scale;
      }
      if(m.policy.scale==RetargetScale::RelativeUniform)for(u32 c=0;c<3;++c)local.scale[c]*=sourceLocal[m.source].scale[c]/source.reference.scale[c];
    }
    if(!validRigTransform(local))return false;
    const auto unboundedWorld=composeRigTransform(targetParent,local);
    const auto &idealParent=target.parent<0?identity:scratch.idealWorld[target.parent];
    auto idealLocal=local;
    if(map>=0) {
      idealLocal.rotation=multiplyRigRotation(inverseRigRotation(idealParent.rotation),unboundedWorld.rotation);
      if(r.mapping[map].policy.position==RetargetPosition::ReconstructWorld)idealLocal.position=relativeRigTransform(unboundedWorld,idealParent).position;
    }
    scratch.idealWorld[i]=composeRigTransform(idealParent,idealLocal);
    if(!validRigTransform(scratch.idealWorld[i]))return false;
    const auto &desiredWorld=scratch.idealWorld[i];
    const auto desiredRotation=local.rotation;
    if(!constrainRigRotation(target.limit,target.reference.rotation,local.rotation))return false;
    scratch.targetWorld[i]=composeRigTransform(targetParent,local);
    if(!validRigTransform(scratch.targetWorld[i]))return false;
    auto &diagnostic=scratch.errors[i];double positionError=0;
    for(u32 c=0;c<3;++c){const double d=double(scratch.targetWorld[i].position[c])-desiredWorld.position[c];positionError+=d*d;}
    diagnostic.position=static_cast<float>(std::sqrt(positionError));
    diagnostic.angleDegrees=static_cast<float>(rotationDistanceDegrees(desiredWorld.rotation.data(),scratch.targetWorld[i].rotation.data()));
    diagnostic.limited=rotationDistanceDegrees(desiredRotation.data(),local.rotation.data())>.001;
    scratch.maximumPositionError=std::max(scratch.maximumPositionError,diagnostic.position);scratch.maximumAngularError=std::max(scratch.maximumAngularError,diagnostic.angleDegrees);scratch.limitedJoints+=diagnostic.limited;
  }
  // Reconstruct locals only after all results passed, keeping the destination
  // unchanged on a late failure and permitting source/output aliasing.
  for(usize i=0;i<targetLocal.size();++i) {
    const auto parent=r.target.joints[i].parent;const RigTransform identity;
    targetLocal[i]=relativeRigTransform(scratch.targetWorld[i],parent<0?identity:scratch.targetWorld[parent]);
  }
  return true;
}
bool compileRigAnimation(const CompiledRig &rig,const AnimationClip &clip,std::span<const u64> nodes,bool allowUnmapped,CompiledRigAnimation &out,std::string &error) {
  error.clear();if(!std::isfinite(clip.duration)||clip.duration<0||clip.duration>86400||clip.channels.empty()||!clip.properties.empty()||clip.drivers)return fail(error,"Retargeting exige clipe TRS com duração válida; propriedades e drivers possuem outro consumidor.");
  if(clip.channels.size()>RigAsset::MaximumJoints*3)return fail(error,"Clipe excede orçamento de canais TRS do rig.");
  usize retained=0;
  const auto countChannel=[&](auto &&self,const AnimationChannel &channel)->bool {
    constexpr usize maximum=64*1024*1024;
    const auto add=[&](usize count,usize stride){if(count>(maximum-retained)/stride)return false;retained+=count*stride;return true;};
    if(!add(channel.times.size(),sizeof(float))||!add(channel.values.size(),sizeof(float)))return false;
    for(const auto &curve:channel.curves)if(!add(curve.keys.size(),sizeof(AnimationCurveKey)))return false;
    for(const auto &layer:channel.layerSources)if(!self(self,layer))return false;
    return true;
  };
  for(const auto &channel:clip.channels)if(!countChannel(countChannel,channel))return fail(error,"Clipe excede 64 MiB de samplers retidos.");
  CompiledRigAnimation value;value.rig=rig.guid;value.revision=rig.revision;value.clip=clip;std::set<std::pair<u32,AnimationPath>> used;
  for(u32 i=0;i<clip.channels.size();++i) {
    const auto &channel=clip.channels[i];
    if(channel.path==AnimationPath::Weights||!validAnimationChannel(channel))return fail(error,"Canal inválido ou morph sem correspondência explícita na transferência.");
    const auto found=channel.node<nodes.size()?rig.index.find(nodes[channel.node]):rig.index.end();
    if(found==rig.index.end()){if(allowUnmapped)continue;return fail(error,"Clipe anima nó não mapeado para o rig.");}
    if(!used.emplace(found->second,channel.path).second)return fail(error,"Clipe possui dois canais sobre a mesma propriedade de junta.");
    value.channels.push_back({found->second,i});
  }
  if(value.channels.empty())return fail(error,"Clipe não anima nenhuma junta do rig.");
  out=std::move(value);return true;
}
bool sampleRigAnimation(const CompiledRig &rig,const CompiledRigAnimation &clip,float time,std::span<RigTransform> local) {
  if(local.size()!=rig.joints.size()||!std::isfinite(time)||clip.rig!=rig.guid||clip.revision!=rig.revision)return false;
  for(usize i=0;i<local.size();++i)local[i]=rig.joints[i].reference;
  for(const auto &binding:clip.channels) {
    if(binding.rigJoint>=local.size()||binding.clipChannel>=clip.clip.channels.size())return false;
    const auto &channel=clip.clip.channels[binding.clipChannel];float value[4]{};
    if(!sampleValidatedAnimationChannel(channel,std::clamp(time,0.f,clip.clip.duration),std::span<float>{value,channel.components()}))return false;
    auto &t=local[binding.rigJoint];
    if(channel.path==AnimationPath::Translation)std::copy_n(value,3,t.position.begin());
    else if(channel.path==AnimationPath::Rotation)std::copy_n(value,4,t.rotation.begin());
    else if(channel.path==AnimationPath::Scale)std::copy_n(value,3,t.scale.begin());
    else return false;
  }
  for(const auto &t:local)if(!validRigTransform(t))return false;
  return true;
}
bool bakeRetarget(const CompiledRetarget &r,const CompiledRigAnimation &source,const RetargetBakeOptions &options,
    AnimationClip &out,std::string &error,const std::function<bool()> &cancelled) {
  error.clear();if((options.rate!=30&&options.rate!=60&&options.rate!=120)||!options.maximumKeys||options.maximumKeys>262144)return fail(error,"Taxa ou orçamento do bake inválidos.");
  const double frames=std::ceil(double(source.clip.duration)*options.rate);
  if(!std::isfinite(frames)||frames>262143)return fail(error,"Bake excede orçamento temporal.");
  const usize count=static_cast<usize>(frames)+1,channelCount=r.target.joints.size()*3;
  if(!channelCount||count>options.maximumKeys/channelCount)return fail(error,"Bake excede orçamento agregado de chaves.");
  if(!validateRetargetAnimation(r,source,error))return false;
  AnimationClip result;result.name=source.clip.name+" transferido";result.duration=source.clip.duration;
  result.cues=source.clip.cues;result.regions=source.clip.regions;result.channels.resize(channelCount);
  for(u32 j=0;j<r.target.joints.size();++j)for(u32 p=0;p<3;++p) {
    auto &channel=result.channels[j*3+p];channel.node=j;channel.path=static_cast<AnimationPath>(p);
    channel.times.reserve(count);channel.values.reserve(count*channel.components());
  }
  RetargetWorkspace scratch;scratch.prepare(r);std::vector<RigTransform> input(r.source.joints.size()),target(r.target.joints.size());
  for(usize frame=0;frame<count;++frame) {
    if(cancelled&&cancelled())return fail(error,"Bake cancelado; nenhum clipe foi publicado.");
    const float time=static_cast<float>(frame==count-1?source.clip.duration:double(frame)/options.rate);
    if(!sampleRigAnimation(r.source,source,time,input)||!evaluateRetarget(r,input,target,scratch))return fail(error,"Pose não representável no bake em "+std::to_string(time)+"s.");
    for(u32 j=0;j<target.size();++j)for(u32 p=0;p<3;++p) {
      auto &channel=result.channels[j*3+p];channel.times.push_back(time);
      if(p==0)channel.values.insert(channel.values.end(),target[j].position.begin(),target[j].position.end());
      if(p==1)channel.values.insert(channel.values.end(),target[j].rotation.begin(),target[j].rotation.end());
      if(p==2)channel.values.insert(channel.values.end(),target[j].scale.begin(),target[j].scale.end());
    }
  }
  for(const auto &channel:result.channels)if(!validAnimationChannel(channel))return fail(error,"Bake gerou sampler fora do contrato.");
  out=std::move(result);return true;
}
bool validateRetargetAnimation(const CompiledRetarget &r,const CompiledRigAnimation &source,std::string &error) {
  error.clear();
  if(source.rig!=r.source.guid||source.revision!=r.source.revision)return fail(error,"Clipe foi preparado para outra revisão de rig.");
  if(!r.allowUnmappedSource)for(const auto &channel:source.channels)if(std::none_of(r.mapping.begin(),r.mapping.end(),[&](const auto &m){return m.source==channel.rigJoint;}))return fail(error,"Perfil não transfere uma junta animada; mapeie ou autorize descarte explicitamente.");
  return true;
}
} // namespace ae::resources
