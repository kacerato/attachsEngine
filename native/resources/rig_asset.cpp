#include "resources/rig_asset.h"
#include "resources/animation_binding_path.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ae::resources {
namespace {
bool fail(std::string *out,std::string message) {if(out)*out=std::move(message);return false;}
bool textValid(std::string_view text,usize maximum,bool empty=false) {
  if(text.size()>maximum||(!empty&&text.empty()))return false;
  for(unsigned char c:text)if(c<32||c==127)return false;
  return true;
}
bool pathValid(std::string_view path) {
  if(!textValid(path,1024,true))return false;
  if(path.empty())return true;
  for(usize begin=0;begin<path.size();) {
    const auto end=path.find('/',begin);const auto part=path.substr(begin,end==path.npos?path.size()-begin:end-begin);
    std::string decoded;if(!animationBindingName(part,decoded)||decoded.empty())return false;
    if(end==path.npos)return true;
    begin=end+1;if(begin==path.size())return false;
  }
  return true;
}
template<usize N> bool unit(const std::array<float,N> &values) {
  double sum=0;for(float v:values){if(!std::isfinite(v))return false;sum+=double(v)*v;}
  return std::abs(sum-1)<1e-4;
}
std::string guidText(AssetGuid id) {return id.valid()?id.text():"-";}
bool readGuid(std::istream &in,AssetGuid &guid,bool optional=false) {
  std::string text;if(!(in>>text))return false;
  if(optional&&text=="-"){guid={};return true;}return AssetGuid::parse(text,guid);
}
void writeTransform(std::ostream &out,const RigTransform &t) {
  for(float v:t.position)out<<' '<<v;
  for(float v:t.rotation)out<<' '<<v;
  for(float v:t.scale)out<<' '<<v;
}
bool readTransform(std::istream &in,RigTransform &t) {
  for(float &v:t.position)if(!(in>>v))return false;
  for(float &v:t.rotation)if(!(in>>v))return false;
  for(float &v:t.scale)if(!(in>>v))return false;
  return validRigTransform(t);
}
template<class T> bool finishRead(std::istream &in,T &candidate,T &out,std::string *error) {
  in>>std::ws;if(!in.eof()||!candidate.valid(error))return fail(error,"Recurso de rig truncado, inconsistente ou com dados adicionais.");
  out=std::move(candidate);return true;
}
std::ostringstream writer() {std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);return out;}
}
const RigJoint *RigAsset::joint(u64 id) const {for(const auto &j:joints)if(j.id==id)return &j;return nullptr;}
const RigControl *RigAsset::control(u64 id) const {for(const auto &c:controls)if(c.id==id)return &c;return nullptr;}
bool RigAsset::valid(std::string *error) const {
  if(error)error->clear();
  if(!guid.valid()||!revision||!nextId||nextId==std::numeric_limits<u64>::max()||!textValid(name,256)||
      joints.empty()||joints.size()>MaximumJoints||controls.size()>MaximumControls||chains.size()>MaximumChains||mirror.size()>MaximumControls)
    return fail(error,"Identidade, nome, revisão ou orçamento do rig inválidos.");
  if(!unit(mirrorNormal)||!std::isfinite(mirrorOffset)||std::abs(mirrorOffset)>1e7f)return fail(error,"Plano de espelho inválido.");
  std::unordered_map<u64,const RigJoint *> byId;
  std::unordered_set<u64> ids;std::unordered_set<std::string> paths;
  std::unordered_set<AssetGuid,AssetGuidHash> sourceNodes;
  const auto idValid=[&](u64 id){return id&&id<nextId&&ids.insert(id).second;};
  usize roots=0;
  for(const auto &j:joints) {
    if(!idValid(j.id)||!textValid(j.name,256)||!pathValid(j.path)||!paths.insert(j.path).second||
        !validRigTransform(j.bind)||!validRigTransform(j.reference)||(j.sourceNode.valid()&&!sourceNodes.insert(j.sourceNode).second))
      return fail(error,"Junta duplicada, caminho ambíguo ou repouso/escala não representável: "+j.name);
    byId.emplace(j.id,&j);roots+=j.parent==0;
  }
  if(roots!=1)return fail(error,"Rig deve possuir exatamente uma raiz explícita.");
  for(const auto &j:joints) {
    if(!j.parent&&!j.path.empty())return fail(error,"A junta raiz deve usar caminho relativo vazio.");
    if(j.parent) {
      const auto parent=byId.find(j.parent);
      if(parent==byId.end())return fail(error,"Junta pai ausente.");
      const auto prefix=parent->second->path.empty()?std::string{}:parent->second->path+'/';
      if(!j.path.starts_with(prefix)||j.path.size()<=prefix.size()||j.path.find('/',prefix.size())!=j.path.npos)return fail(error,"Caminho da junta não corresponde ao pai declarado: "+j.name);
    }
    auto parent=j.parent;usize depth=0;
    while(parent) {
      const auto found=byId.find(parent);
      if(found==byId.end()||parent==j.id||++depth>joints.size())return fail(error,"Pai ausente ou ciclo na junta: "+j.name);
      parent=found->second->parent;
    }
  }
  std::unordered_set<u64> controlled;
  for(const auto &c:controls)if(!idValid(c.id)||!byId.contains(c.joint)||!controlled.insert(c.joint).second||!textValid(c.name,256)||
      !c.channels||c.channels>RigAll||static_cast<u8>(c.space)>1||!validRigLimit(c.limit))
    return fail(error,"Controle duplicado, limite inválido ou junta ausente: "+c.name);
  for(const auto &chain:chains) {
    if(!idValid(chain.id)||!textValid(chain.name,256)||chain.joints.size()<2||chain.joints.size()>MaximumJoints)
      return fail(error,"Cadeia inválida: "+chain.name);
    for(usize i=0;i<chain.joints.size();++i) {
      const auto found=byId.find(chain.joints[i]);
      if(found==byId.end()||(i&&found->second->parent!=chain.joints[i-1]))return fail(error,"Cadeia não contígua: "+chain.name);
      if(i) {
        const auto &p=found->second->reference.position;double length=0;for(float v:p)length+=double(v)*v;
        if(length<1e-12)return fail(error,"Segmento degenerado na cadeia: "+chain.name);
      }
    }
  }
  std::unordered_set<u64> mirrored;
  for(const auto &pair:mirror) {
    if(!control(pair.left)||!control(pair.right)||!mirrored.insert(pair.left).second||
        (pair.left!=pair.right&&!mirrored.insert(pair.right).second))return fail(error,"Mapa de espelho duplicado ou controle ausente.");
  }
  return true;
}
bool RigAsset::edit(u32 expected,const std::function<bool(RigAsset &,std::string &)> &change,std::string &error) {
  error.clear();if(expected!=revision||revision==std::numeric_limits<u32>::max())return fail(&error,"Rig foi alterado; revalide a revisão antes de editar.");
  auto candidate=*this;if(!change(candidate,error))return false;
  if(candidate.guid!=guid||candidate.nextId<nextId)return fail(&error,"Edição tentou reutilizar identidade do rig.");
  std::unordered_set<u64> previousJoints,previousControls,previousChains;
  for(const auto &j:joints)previousJoints.insert(j.id);
  for(const auto &c:controls)previousControls.insert(c.id);
  for(const auto &chain:chains)previousChains.insert(chain.id);
  const auto freshId=[&](u64 id,const auto &kind){return kind.contains(id)||id>=nextId;};
  for(const auto &j:candidate.joints)if(!freshId(j.id,previousJoints))return fail(&error,"Edição reutilizou ID de junta removida ou de outro tipo.");
  for(const auto &c:candidate.controls)if(!freshId(c.id,previousControls))return fail(&error,"Edição reutilizou ID de controle removido ou de outro tipo.");
  for(const auto &chain:candidate.chains)if(!freshId(chain.id,previousChains))return fail(&error,"Edição reutilizou ID de cadeia removida ou de outro tipo.");
  candidate.revision=revision+1;if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
std::string RigAsset::serialize() const {
  if(!valid())return {};
  auto out=writer();out<<"AERIG 1\n"<<guid.text()<<' '<<revision<<' '<<nextId<<' '<<std::quoted(name)<<'\n';
  for(float v:mirrorNormal)out<<v<<' ';
  out<<mirrorOffset<<'\n'<<joints.size()<<'\n';
  for(const auto &j:joints){out<<j.id<<' '<<j.parent<<' '<<guidText(j.sourceNode)<<' '<<std::quoted(j.name)<<' '<<std::quoted(j.path);writeTransform(out,j.bind);writeTransform(out,j.reference);out<<'\n';}
  out<<controls.size()<<'\n';
  for(const auto &c:controls) {
    out<<c.id<<' '<<c.joint<<' '<<std::quoted(c.name)<<' '<<u32(c.channels)<<' '<<u32(c.space)<<' '<<u32(c.limit.kind);
    for(float v:c.limit.axis)out<<' '<<v;
    out<<' '<<c.limit.minimum<<' '<<c.limit.maximum<<' '<<c.limit.cone<<'\n';
  }
  out<<chains.size()<<'\n';for(const auto &chain:chains){out<<chain.id<<' '<<std::quoted(chain.name)<<' '<<chain.joints.size();for(auto id:chain.joints)out<<' '<<id;out<<'\n';}
  out<<mirror.size()<<'\n';for(const auto &pair:mirror)out<<pair.left<<' '<<pair.right<<'\n';
  return out.str();
}
bool RigAsset::deserialize(std::string_view text,RigAsset &out,std::string *error) {
  if(error)error->clear();
  if(text.size()>MaximumBytes)return fail(error,"Rig excede orçamento de leitura.");
  RigAsset value;std::istringstream in{std::string(text)};in.imbue(std::locale::classic());std::string magic;u32 format=0;usize count=0;
  if(!(in>>magic>>format)||magic!="AERIG"||format!=1||!readGuid(in,value.guid)||!(in>>value.revision>>value.nextId>>std::quoted(value.name)))return fail(error,"Cabeçalho de rig inválido.");
  for(float &v:value.mirrorNormal)if(!(in>>v))return fail(error,"Plano de espelho truncado.");
  if(!(in>>value.mirrorOffset>>count)||!count||count>MaximumJoints)return fail(error,"Quantidade de juntas inválida.");
  value.joints.resize(count);
  for(auto &j:value.joints)if(!(in>>j.id>>j.parent)||!readGuid(in,j.sourceNode,true)||!(in>>std::quoted(j.name)>>std::quoted(j.path))||!readTransform(in,j.bind)||!readTransform(in,j.reference))return fail(error,"Junta truncada ou inválida.");
  if(!(in>>count)||count>MaximumControls)return fail(error,"Quantidade de controles inválida.");
  value.controls.resize(count);
  for(auto &c:value.controls) {
    u32 channels=0,space=0,kind=0;
    if(!(in>>c.id>>c.joint>>std::quoted(c.name)>>channels>>space>>kind)||channels>7||space>1||kind>2)return fail(error,"Controle truncado ou inválido.");
    c.channels=static_cast<u8>(channels);c.space=static_cast<RigControlSpace>(space);c.limit.kind=static_cast<RigLimitKind>(kind);
    for(float &v:c.limit.axis)if(!(in>>v))return fail(error,"Eixo de limite truncado.");
    if(!(in>>c.limit.minimum>>c.limit.maximum>>c.limit.cone))return fail(error,"Limite truncado.");
  }
  if(!(in>>count)||count>MaximumChains)return fail(error,"Quantidade de cadeias inválida.");
  value.chains.resize(count);
  for(auto &chain:value.chains) {
    if(!(in>>chain.id>>std::quoted(chain.name)>>count)||count>MaximumJoints)return fail(error,"Cadeia truncada.");
    chain.joints.resize(count);for(auto &id:chain.joints)if(!(in>>id))return fail(error,"Cadeia truncada.");
  }
  if(!(in>>count)||count>MaximumControls)return fail(error,"Quantidade de pares inválida.");
  value.mirror.resize(count);for(auto &pair:value.mirror)if(!(in>>pair.left>>pair.right))return fail(error,"Espelho truncado.");
  return finishRead(in,value,out,error);
}
bool RigPose::valid(std::string *error) const {
  if(error)error->clear();
  if(!guid.valid()||!revision||!rig.valid()||!rigRevision||!textValid(name,256)||entries.empty()||entries.size()>RigAsset::MaximumJoints)
    return fail(error,"Identidade, nome ou quantidade da pose inválidos.");
  std::unordered_set<u64> used;
  for(const auto &entry:entries)if(!entry.joint||!entry.channels||entry.channels>RigAll||!used.insert(entry.joint).second||!validRigTransform(entry.local))return fail(error,"Pose possui junta duplicada ou transformação inválida.");
  return true;
}
std::string RigPose::serialize() const {
  if(!valid())return {};
  auto out=writer();out<<"AERPOSE 2\n"<<guid.text()<<' '<<revision<<' '<<rig.text()<<' '<<rigRevision<<' '<<guidText(thumbnail)<<' '<<std::quoted(name)<<' '<<entries.size()<<'\n';
  for(const auto &e:entries){out<<e.joint<<' '<<u32(e.channels);writeTransform(out,e.local);out<<'\n';}return out.str();
}
bool RigPose::deserialize(std::string_view text,RigPose &out,std::string *error) {
  if(text.size()>RigAsset::MaximumBytes)return fail(error,"Pose excede orçamento.");
  RigPose value;std::istringstream in{std::string(text)};in.imbue(std::locale::classic());std::string magic;u32 format=0;usize count=0;
  if(!(in>>magic>>format)||magic!="AERPOSE"||(format!=1&&format!=2)||!readGuid(in,value.guid))return fail(error,"Cabeçalho de pose inválido.");
  if(format==2&&!(in>>value.revision))return fail(error,"Revisão da pose truncada.");
  if(!readGuid(in,value.rig)||!(in>>value.rigRevision)||!readGuid(in,value.thumbnail,true)||!(in>>std::quoted(value.name)>>count)||count>RigAsset::MaximumJoints)return fail(error,"Cabeçalho de pose inválido.");
  value.entries.resize(count);for(auto &e:value.entries){u32 channels=0;if(!(in>>e.joint>>channels)||channels>7||!readTransform(in,e.local))return fail(error,"Pose truncada.");e.channels=static_cast<u8>(channels);}
  return finishRead(in,value,out,error);
}
bool RetargetProfile::valid(std::string *error) const {
  if(error)error->clear();
  if(!guid.valid()||!source.valid()||!target.valid()||!revision||!sourceRevision||!targetRevision||!textValid(name,256)||
      !std::isfinite(motionScale)||motionScale<1e-4f||motionScale>1e4f||!unit(facing)||mapping.empty()||mapping.size()>RigAsset::MaximumJoints)
    return fail(error,"Identidade, revisões ou escala da transferência inválidos.");
  std::unordered_set<u64> sources,targets;
  for(const auto &m:mapping)if(!m.source||!m.target||!sources.insert(m.source).second||!targets.insert(m.target).second||
      static_cast<u8>(m.position)>3||static_cast<u8>(m.scale)>1||!unit(m.correction))return fail(error,"Correspondência de transferência duplicada ou inválida.");
  return true;
}
std::string RetargetProfile::serialize() const {
  if(!valid())return {};
  auto out=writer();out<<"AERTGT 1\n"<<guid.text()<<' '<<source.text()<<' '<<target.text()<<' '<<revision<<' '<<sourceRevision<<' '<<targetRevision<<' '<<std::quoted(name)<<' '<<motionScale;
  for(float v:facing)out<<' '<<v;
  out<<' '<<allowUnmappedSource<<' '<<mapping.size()<<'\n';
  for(const auto &m:mapping){out<<m.source<<' '<<m.target<<' '<<u32(m.position)<<' '<<u32(m.scale);for(float v:m.correction)out<<' '<<v;out<<'\n';}return out.str();
}
bool RetargetProfile::deserialize(std::string_view text,RetargetProfile &out,std::string *error) {
  if(text.size()>RigAsset::MaximumBytes)return fail(error,"Perfil de transferência excede orçamento.");
  RetargetProfile value;std::istringstream in{std::string(text)};in.imbue(std::locale::classic());std::string magic;u32 format=0,allow=0;usize count=0;
  if(!(in>>magic>>format)||magic!="AERTGT"||format!=1||!readGuid(in,value.guid)||!readGuid(in,value.source)||!readGuid(in,value.target)||!(in>>value.revision>>value.sourceRevision>>value.targetRevision>>std::quoted(value.name)>>value.motionScale))return fail(error,"Cabeçalho de transferência inválido.");
  for(float &v:value.facing)if(!(in>>v))return fail(error,"Orientação frontal truncada.");
  if(!(in>>allow>>count)||allow>1||count>RigAsset::MaximumJoints)return fail(error,"Quantidade de correspondências inválida.");
  value.allowUnmappedSource=allow!=0;value.mapping.resize(count);
  for(auto &m:value.mapping){u32 position=0,scale=0;if(!(in>>m.source>>m.target>>position>>scale)||position>3||scale>1)return fail(error,"Correspondência truncada.");m.position=static_cast<RetargetPosition>(position);m.scale=static_cast<RetargetScale>(scale);for(float &v:m.correction)if(!(in>>v))return fail(error,"Correção truncada.");}
  return finishRead(in,value,out,error);
}
} // namespace ae::resources
