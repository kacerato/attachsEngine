#include "editor/editor_rig_authoring_candidate.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::editor {
namespace {
using namespace resources;
bool fail(std::string &error,std::string message) {error=std::move(message);return false;}
bool allocate(RigAsset &asset,u64 &id,std::string &error) {
  if(asset.nextId>=std::numeric_limits<u64>::max()-1)return fail(error,"Identidades do rig esgotadas.");
  id=asset.nextId++;return true;
}
template<class Change> bool prepare(const RigAsset &base,u32 revision,RigAsset &out,
                                    std::string &error,Change change) {
  if(!base.valid(&error))return false;
  auto candidate=base;
  if(!candidate.edit(revision,change,error))return false;
  CompiledRig compiled;if(!compileRig(candidate,compiled,error))return false;
  out=std::move(candidate);return true;
}
bool maskValid(std::span<const u8> channels,usize count,std::string &error) {
  if(channels.size()!=count)return fail(error,"Máscara não corresponde às juntas do rig.");
  bool used=false;
  for(u8 channel:channels) {
    if(channel>RigAll)return fail(error,"Máscara contém canais desconhecidos.");
    used|=channel!=0;
  }
  return used||fail(error,"Selecione ao menos um canal de uma junta.");
}
}
bool prepareRigCapture(const runtime::SceneGraph &scene,runtime::ObjectId root,
                       resources::AssetGuid guid,std::string_view name,
                       resources::RigAsset &out,std::string &error) {
  resources::RigAsset candidate;
  if(!runtime::captureRig(scene,root,guid,name,candidate,error))return false;
  resources::CompiledRig compiled;if(!resources::compileRig(candidate,compiled,error))return false;
  out=std::move(candidate);return true;
}
bool prepareRigControl(const resources::RigAsset &base,u32 revision,
                       resources::RigControl value,resources::RigAsset &out,
                       u64 &assigned,std::string &error) {
  u64 id=value.id;resources::RigAsset candidate;
  if(!prepare(base,revision,candidate,error,[&](auto &rig,std::string &message) {
    if(!rig.joint(value.joint))return fail(message,"Junta do controle não existe.");
    if(!id) {
      if(!allocate(rig,id,message))return false;
      value.id=id;rig.controls.push_back(value);return true;
    }
    const auto found=std::find_if(rig.controls.begin(),rig.controls.end(),[&](const auto &entry){return entry.id==id;});
    if(found==rig.controls.end())return fail(message,"Controle removido; crie uma nova identidade.");
    if(found->joint!=value.joint)return fail(message,"Não é permitido mover a identidade de um controle para outra junta.");
    *found=value;return true;
  }))return false;
  assigned=id;out=std::move(candidate);return true;
}
bool prepareRigRemoveControl(const resources::RigAsset &base,u32 revision,u64 id,
                             resources::RigAsset &out,std::string &error) {
  return prepare(base,revision,out,error,[&](auto &rig,std::string &message) {
    const auto removed=std::erase_if(rig.controls,[&](const auto &control){return control.id==id;});
    if(!removed)return fail(message,"Controle não existe.");
    std::erase_if(rig.mirror,[&](const auto &pair){return pair.left==id||pair.right==id;});
    return true;
  });
}
bool prepareRigMirrorPair(const resources::RigAsset &base,u32 revision,u64 left,u64 right,
                          resources::RigAsset &out,std::string &error) {
  return prepare(base,revision,out,error,[&](auto &rig,std::string &message) {
    if(!rig.control(left)||!rig.control(right))return fail(message,"Escolha dois controles existentes, ou o mesmo controle central.");
    std::erase_if(rig.mirror,[&](const auto &pair){return pair.left==left||pair.right==left||pair.left==right||pair.right==right;});
    rig.mirror.push_back({left,right});return true;
  });
}
bool prepareRigMirrorPlane(const resources::RigAsset &base,u32 revision,
                           std::array<float,3> normal,float offset,
                           resources::RigAsset &out,std::string &error) {
  return prepare(base,revision,out,error,[&](auto &rig,std::string &message) {
    double length=0;for(float axis:normal){if(!std::isfinite(axis))return fail(message,"Normal do plano não é finita.");length+=double(axis)*axis;}
    if(length<1e-12||!std::isfinite(offset))return fail(message,"Plano de espelho degenerado.");
    // Preserve the geometric plane dot(n,p)=offset while normalizing its
    // representation. Scaling only n would silently move an offset plane.
    const float inverse=static_cast<float>(1/std::sqrt(length));
    for(float &axis:normal)axis*=inverse;
    rig.mirrorNormal=normal;rig.mirrorOffset=offset*inverse;return true;
  });
}
bool prepareRigChain(const resources::RigAsset &base,u32 revision,u64 id,
                     std::string_view name,std::span<const u64> joints,
                     resources::RigAsset &out,u64 &assigned,std::string &error) {
  resources::RigAsset candidate;
  if(!prepare(base,revision,candidate,error,[&](auto &rig,std::string &message) {
    if(!id) {
      if(!allocate(rig,id,message))return false;
      rig.chains.push_back({id,std::string(name),{joints.begin(),joints.end()}});return true;
    }
    const auto found=std::find_if(rig.chains.begin(),rig.chains.end(),[&](const auto &chain){return chain.id==id;});
    if(found==rig.chains.end())return fail(message,"Cadeia removida; crie uma nova identidade.");
    found->name=name;found->joints.assign(joints.begin(),joints.end());return true;
  }))return false;
  assigned=id;out=std::move(candidate);return true;
}
bool prepareRigReference(const resources::RigAsset &base,u32 revision,
                         std::span<const resources::RigTransform> local,
                         std::span<const u8> channels,resources::RigAsset &out,
                         std::string &error) {
  resources::CompiledRig compiled;
  if(!resources::compileRig(base,compiled,error)||local.size()!=compiled.joints.size())return fail(error,"Pose não corresponde ao rig de referência.");
  if(!maskValid(channels,local.size(),error))return false;
  return prepare(base,revision,out,error,[&](auto &rig,std::string &message) {
    for(auto &joint:rig.joints) {
      const auto index=compiled.index.at(joint.id);const u8 mask=channels[index];
      if(!mask)continue;
      if(!resources::validRigTransform(local[index]))return fail(message,"Transformação de referência não é representável.");
      if(mask&resources::RigPosition)joint.reference.position=local[index].position;
      if(mask&resources::RigRotation)joint.reference.rotation=local[index].rotation;
      if(mask&resources::RigScale)joint.reference.scale=local[index].scale;
    }
    return true;
  });
}
bool prepareRigPose(const resources::CompiledRig &rig,resources::AssetGuid guid,
                    std::string_view name,std::span<const resources::RigTransform> local,
                    std::span<const u8> channels,resources::RigPose &out,
                    std::string &error) {
  error.clear();
  if(!rig.guid.valid()||!rig.revision||local.size()!=rig.joints.size())return fail(error,"Pose não corresponde ao rig compilado.");
  if(!maskValid(channels,local.size(),error))return false;
  resources::RigPose candidate;candidate.guid=guid;candidate.rig=rig.guid;candidate.rigRevision=rig.revision;candidate.name=name;
  for(usize i=0;i<local.size();++i)if(channels[i])candidate.entries.push_back({rig.joints[i].id,channels[i],local[i]});
  if(!candidate.valid(&error))return false;
  out=std::move(candidate);return true;
}
bool prepareRigRetarget(const resources::RigAsset &source,u32 sourceRevision,
                        const resources::RigAsset &target,u32 targetRevision,
                        const resources::RetargetProfile &draft,resources::RetargetProfile &out,
                        std::string &error) {
  error.clear();
  if(source.revision!=sourceRevision||target.revision!=targetRevision)return fail(error,"Repouso de origem ou destino mudou; revalide o mapa.");
  auto candidate=draft;candidate.source=source.guid;candidate.target=target.guid;
  candidate.sourceRevision=sourceRevision;candidate.targetRevision=targetRevision;
  resources::CompiledRetarget compiled;
  if(!resources::compileRetarget(source,target,candidate,compiled,error))return false;
  out=std::move(candidate);return true;
}
bool prepareRetargetEdit(const resources::RetargetProfile &base,u32 profileRevision,
                         const resources::RigAsset &source,u32 sourceRevision,
                         const resources::RigAsset &target,u32 targetRevision,
                         const resources::RetargetProfile &draft,
                         resources::RetargetProfile &out,std::string &error) {
  error.clear();
  if(!base.valid(&error))return false;
  if(base.revision!=profileRevision || profileRevision==std::numeric_limits<u32>::max())
    return fail(error,"Perfil foi alterado; revalide a revisão antes de editar.");
  if(draft.guid!=base.guid)return fail(error,"Edição não pode trocar a identidade do perfil.");
  auto candidate=draft;candidate.revision=profileRevision+1;
  return prepareRigRetarget(source,sourceRevision,target,targetRevision,candidate,out,error);
}
} // namespace ae::editor
