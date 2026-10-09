#include "resources/animation_clip_asset.h"
#include "resources/animation_binding_path.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <tuple>

namespace ae::resources {
namespace {
bool fail(std::string *out,const char *message) {if(out)*out=message;return false;}
bool textValid(std::string_view text,usize maximum,bool empty=true) {
  if(text.size()>maximum||(!empty&&text.empty()))return false;
  for(unsigned char c:text)if(c<32||c==127)return false;
  return true;
}
bool bindingPathValid(std::string_view path,bool legacy=false) {
  if(!textValid(path,1024)||(!path.empty()&&(path.front()=='/'||path.back()=='/'))||path.find('\\')!=path.npos)return false;
  for(usize start=0;start<path.size();) {
    const auto end=path.find('/',start);const auto part=path.substr(start,end==path.npos?path.size()-start:end-start);
    if(part.empty()||part=="."||part=="..")return false;
    if(!legacy) {std::string name;if(!animationBindingName(part,name))return false;}
    if(end==path.npos)break;
    start=end+1;
  }
  return true;
}
bool hashValid(std::string_view text) {
  if(text.size()!=64)return false;
  for(char c:text)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
std::string guidText(AssetGuid guid) {return guid.valid()?guid.text():"-";}
bool readGuid(std::istream &in,AssetGuid &guid,bool optional) {
  std::string text;if(!(in>>text))return false;
  if(optional&&text=="-") {guid={};return true;}
  return AssetGuid::parse(text,guid);
}
bool readCurve(std::istream &in,AnimationCurve &curve,usize &total) {
  usize count=0;if(!(in>>count)||!count||count>AnimationClipAsset::MaximumKeys-total)return false;
  total+=count;curve.keys.resize(count);
  for(auto &k:curve.keys) {
    u32 a=0,b=0,broken=0,wi=0,wo=0;
    if(!(in>>k.id>>k.time>>k.value>>k.inSlope>>k.outSlope>>k.inWeight>>k.outWeight>>a>>b>>broken>>wi>>wo)||
       a>6||b>6||broken>1||wi>1||wo>1)return false;
    k.incoming=static_cast<AnimationTangentMode>(a);k.outgoing=static_cast<AnimationTangentMode>(b);
    k.broken=broken!=0;k.weightedIn=wi!=0;k.weightedOut=wo!=0;
  }
  return true;
}
}
bool AnimationClipAsset::valid(std::string *diagnostic) const {
  if(diagnostic)diagnostic->clear();
  if(!guid.valid()||!revision||!nextId||nextId==std::numeric_limits<u64>::max()||!textValid(name,256,false)||
     !std::isfinite(duration)||duration<0||duration>86400||!displayRate||displayRate>1000)
    return fail(diagnostic,"Identidade, nome, duração ou taxa do clipe inválidos");
  if(source.valid()!=sourceClip.valid()||(source.valid()?!hashValid(sourceHash):!sourceHash.empty()))
    return fail(diagnostic,"Vínculo ou hash da origem inválido");
  if(bindings.empty()||bindings.size()>MaximumBindings||tracks.empty()||tracks.size()>MaximumTracks)
    return fail(diagnostic,"Quantidade de bindings ou canais inválida");
  std::unordered_set<u64> ids,bindingIds;
  std::unordered_set<std::string> paths;
  std::unordered_set<AssetGuid,AssetGuidHash> sourceNodes;
  auto idValid=[&](u64 id){return id&&id<nextId&&ids.insert(id).second;};
  if(layers.empty()||layers.size()>MaximumLayers||layers.front().id!=0)
    return fail(diagnostic,"Camada base ausente ou limite de camadas excedido");
  std::unordered_set<u64> layerIds{0};
  for(const auto &l:layers) {
    if((l.id&&(!idValid(l.id)||!layerIds.insert(l.id).second))||
       (!l.id && &l!=&layers.front())||!textValid(l.name,256,false)||static_cast<u8>(l.blend)>1||
       !std::isfinite(l.weight)||l.weight<0||l.weight>1||!std::isfinite(l.referenceTime)||
       (l.referenceTime!=-1&&(l.referenceTime<0||l.referenceTime>duration))||
       (l.blend==AnimationAuthorBlend::Override&&l.referenceTime!=-1)||
       (!l.id&&(l.blend!=AnimationAuthorBlend::Override||l.weight!=1||l.referenceTime!=-1)))
      return fail(diagnostic,"Identidade, mistura, peso ou referência de camada inválidos");
  }
  for(const auto &b:bindings) {
    if(!idValid(b.id)||!bindingPathValid(b.path)||!textValid(b.name,256)||!paths.insert(b.path).second||
       (b.sourceNode.valid()&&!sourceNodes.insert(b.sourceNode).second))
      return fail(diagnostic,"Binding duplicado ou caminho inválido");
    bindingIds.insert(b.id);
  }
  usize total=0;std::unordered_set<std::string> targets;
  for(const auto &t:tracks) {
    if(!idValid(t.id)||!bindingIds.contains(t.binding)||static_cast<u8>(t.path)>3||static_cast<u8>(t.rotationMode)>2||
       (t.path!=AnimationPath::Rotation&&t.rotationMode!=AnimationRotationMode::Quaternion)||
       (t.path==AnimationPath::Weights?(!t.weightCount||t.weightCount>MaximumMorphTargets):t.weightCount!=0)||
       !layerIds.contains(t.layer)||t.curves.size()!=t.components()||!targets.insert(std::to_string(t.layer)+":"+std::to_string(t.binding)+":"+std::to_string(static_cast<u8>(t.path))).second)
      return fail(diagnostic,"Canal duplicado, incompatível ou sem binding");
    for(const auto &c:t.curves) {
      if(c.keys.size()>MaximumKeys-total||!validAnimationCurve(c)||c.keys.back().time>duration)
        return fail(diagnostic,"Chaves inválidas ou fora da duração do clipe");
      total+=c.keys.size();for(const auto &k:c.keys)if(!idValid(k.id))return fail(diagnostic,"ID de chave duplicado ou fora do alocador");
    }
    if(t.path==AnimationPath::Rotation&&t.rotationMode!=AnimationRotationMode::Euler) {
      const auto &first=t.curves[0].keys;
      for(u32 c=1;c<4;++c) {
        if(t.curves[c].keys.size()!=first.size())return fail(diagnostic,"Quaternion exige chaves sincronizadas nos quatro canais");
        for(usize i=0;i<first.size();++i)if(t.curves[c].keys[i].time!=first[i].time)
          return fail(diagnostic,"Quaternion exige tempos sincronizados");
      }
      for(usize i=0;i<first.size();++i) {
        double norm=0;for(u32 c=0;c<4;++c)norm+=double(t.curves[c].keys[i].value)*t.curves[c].keys[i].value;
        if(norm<1e-20||!std::isfinite(norm))return fail(diagnostic,"Quaternion nulo ou inválido");
      }
      if(t.rotationMode==AnimationRotationMode::ProgressiveQuaternion) {
        AnimationChannel channel;channel.path=t.path;channel.rotationMode=t.rotationMode;channel.curves=t.curves;
        if(!validAnimationChannel(channel))return fail(diagnostic,"Progressão de rotação incompatível com as poses");
      }
    }
  }
  std::map<std::pair<u64,u8>,u32> baseDimensions;
  for(const auto &t:tracks)if(!t.layer)baseDimensions.emplace(std::pair{t.binding,static_cast<u8>(t.path)},t.weightCount);
  for(const auto &t:tracks)if(t.layer) {
    const auto base=baseDimensions.find({t.binding,static_cast<u8>(t.path)});
    if(base==baseDimensions.end()||base->second!=t.weightCount)return fail(diagnostic,"Canal de camada exige propriedade base compatível");
  }
  return true;
}
std::string AnimationClipAsset::serialize() const {
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  out<<"AECLIP "<<FormatVersion<<' '<<guidText(guid)<<' '<<revision<<' '<<nextId<<' '<<displayRate<<' '<<duration<<' '<<std::quoted(name)
     <<' '<<guidText(source)<<' '<<guidText(sourceClip)<<' '<<std::quoted(sourceHash)<<'\n'<<bindings.size()<<'\n';
  for(const auto &b:bindings)out<<b.id<<' '<<guidText(b.sourceNode)<<' '<<std::quoted(b.path)<<' '<<std::quoted(b.name)<<'\n';
  out<<layers.size()<<'\n';
  for(const auto &l:layers)out<<l.id<<' '<<std::quoted(l.name)<<' '<<static_cast<u32>(l.blend)<<' '<<l.weight<<' '<<l.referenceTime<<' '<<(l.muted?1:0)<<' '<<(l.solo?1:0)<<'\n';
  out<<tracks.size()<<'\n';
  for(const auto &t:tracks) {
    out<<t.id<<' '<<t.binding<<' '<<static_cast<u32>(t.path)<<' '<<t.weightCount<<' '<<(t.sourceOverride?1:0)<<' '<<static_cast<u32>(t.rotationMode)<<' '<<t.layer<<' '<<t.curves.size()<<'\n';
    for(const auto &c:t.curves) {
      out<<c.keys.size()<<'\n';
      for(const auto &k:c.keys)out<<k.id<<' '<<k.time<<' '<<k.value<<' '<<k.inSlope<<' '<<k.outSlope<<' '<<k.inWeight<<' '<<k.outWeight<<' '
        <<static_cast<u32>(k.incoming)<<' '<<static_cast<u32>(k.outgoing)<<' '<<(k.broken?1:0)<<' '<<(k.weightedIn?1:0)<<' '<<(k.weightedOut?1:0)<<'\n';
    }
  }
  return out.str();
}
bool AnimationClipAsset::deserialize(std::string_view text,AnimationClipAsset &out,std::string *diagnostic) {
  if(diagnostic)diagnostic->clear();
  if(text.size()>MaximumBytes)return fail(diagnostic,"Clipe excede o limite de arquivo");
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  AnimationClipAsset value;std::string magic;u32 version=0;usize count=0,total=0;
  if(!(in>>magic>>version)||magic!="AECLIP"||version<1||version>FormatVersion||!readGuid(in,value.guid,false)||
     !(in>>value.revision>>value.nextId>>value.displayRate>>value.duration>>std::quoted(value.name))||
     !readGuid(in,value.source,true)||!readGuid(in,value.sourceClip,true)||!(in>>std::quoted(value.sourceHash)>>count)||
     !count||count>MaximumBindings)return fail(diagnostic,"Cabeçalho ou versão do clipe inválidos");
  value.bindings.resize(count);
  for(auto &b:value.bindings)if(!(in>>b.id)||!readGuid(in,b.sourceNode,true)||!(in>>std::quoted(b.path)>>std::quoted(b.name)))
    return fail(diagnostic,"Binding do clipe incompleto");
  if(version==1)for(auto &binding:value.bindings) {
    if(!bindingPathValid(binding.path,true))return fail(diagnostic,"Caminho legado inválido");
    binding.path=migrateAnimationBindingPath(binding.path);
  }
  if(version>=3) {
    if(!(in>>count)||!count||count>MaximumLayers)return fail(diagnostic,"Quantidade de camadas inválida");
    value.layers.resize(count);
    for(auto &l:value.layers) {
      u32 blend=0,mute=0,solo=0;
      if(!(in>>l.id>>std::quoted(l.name)>>blend>>l.weight>>l.referenceTime>>mute>>solo)||blend>1||mute>1||solo>1)
        return fail(diagnostic,"Camada incompleta ou flags inválidas");
      l.blend=static_cast<AnimationAuthorBlend>(blend);l.muted=mute!=0;l.solo=solo!=0;
    }
  }
  if(!(in>>count)||!count||count>MaximumTracks)return fail(diagnostic,"Quantidade de canais inválida");
  value.tracks.resize(count);
  for(auto &t:value.tracks) {
    u32 path=0,override=0,rotation=0;usize components=0;
    if(!(in>>t.id>>t.binding>>path>>t.weightCount>>override)||(version>=2&&!(in>>rotation))||(version>=3&&!(in>>t.layer))||
       !(in>>components)||path>3||override>1||rotation>2||!components||components>MaximumMorphTargets)
      return fail(diagnostic,"Cabeçalho de canal inválido");
    t.path=static_cast<AnimationPath>(path);t.sourceOverride=override!=0;t.rotationMode=static_cast<AnimationRotationMode>(rotation);
    if(components!=t.components())return fail(diagnostic,"Número de componentes incompatível");
    t.curves.resize(components);for(auto &c:t.curves)if(!readCurve(in,c,total))return fail(diagnostic,"Curva do clipe incompleta");
  }
  in>>std::ws;if(!in.eof())return fail(diagnostic,"Dados extras após o clipe");
  if(!value.valid(diagnostic))return false;
  out=std::move(value);return true;
}
bool AnimationClipAsset::compile(AnimationClip &out,std::string *diagnostic) const {
  if(!valid(diagnostic))return false;
  AnimationClip value;value.name=name;value.duration=duration;value.channels.reserve(std::count_if(tracks.begin(),tracks.end(),[](const auto &t){return !t.layer;}));
  std::map<std::tuple<u64,u64,u8>,const AnimationClipTrack*> channels;
  for(const auto &t:tracks)channels.emplace(std::tuple{t.layer,t.binding,static_cast<u8>(t.path)},&t);
  const bool solo=std::any_of(layers.begin(),layers.end(),[](const auto &l){return l.solo&&!l.muted;});
  for(const auto &t:tracks) {
    if(t.layer)continue;
    const auto b=std::find_if(bindings.begin(),bindings.end(),[&](const auto &entry){return entry.id==t.binding;});
    AnimationChannel c;c.node=static_cast<u32>(b-bindings.begin());c.path=t.path;c.weightCount=t.weightCount;c.curves=t.curves;c.rotationMode=t.rotationMode;
    if(layers.size()>1||layers.front().muted||solo) {
      auto base=c;base.layerWeight=layers.front().muted||(solo&&!layers.front().solo)?0.f:1.f;
      c.curves.clear();c.rotationMode=AnimationRotationMode::Quaternion;c.layerSources.push_back(std::move(base));
      for(usize i=1;i<layers.size();++i) {
        const auto &l=layers[i];if(l.muted||(solo&&!l.solo)||l.weight==0)continue;
        const auto entry=channels.find({l.id,t.binding,static_cast<u8>(t.path)});
        if(entry==channels.end())continue;
        const auto *found=entry->second;
        AnimationChannel source;source.node=c.node;source.path=t.path;source.weightCount=t.weightCount;
        source.curves=found->curves;source.rotationMode=found->rotationMode;source.layerWeight=l.weight;
        source.layerAdditive=l.blend==AnimationAuthorBlend::Additive;source.layerReferenceTime=l.referenceTime;
        if(source.layerAdditive&&l.referenceTime>=0) {
          float reference[MaximumMorphTargets];
          if(!sampleAnimationChannel(source,l.referenceTime,{reference,source.components()}))return fail(diagnostic,"Referência da camada não produz pose válida");
          if(source.path==AnimationPath::Scale)for(u32 c=0;c<3;++c)if(std::abs(reference[c])<1e-8f)return fail(diagnostic,"Referência aditiva de escala não pode ser zero");
        }
        c.layerSources.push_back(std::move(source));
      }
    }
    if(!validAnimationChannel(c))return fail(diagnostic,"Canal não pode ser publicado ao runtime");
    value.channels.push_back(std::move(c));
  }
  out=std::move(value);return true;
}
bool AnimationClipAsset::edit(u32 expectedRevision,const std::function<bool(AnimationClipAsset &)> &change,std::string &diagnostic) {
  diagnostic.clear();
  if(expectedRevision!=revision||revision==std::numeric_limits<u32>::max())return fail(&diagnostic,"Revisão do clipe mudou");
  if(!valid(&diagnostic)||!change)return false;
  auto candidate=*this;
  if(!change(candidate)) {
    if(diagnostic.empty())diagnostic="Edição do clipe recusada";
    return false;
  }
  if(candidate.guid!=guid||candidate.revision!=revision||candidate.nextId<nextId)
    return fail(&diagnostic,"Edição alterou identidade, revisão ou alocador");
  if(!candidate.valid(&diagnostic))return false;
  std::unordered_map<u64,u32> previous;
  for(const auto &l:layers)if(l.id)previous.emplace(l.id,3);
  for(const auto &b:bindings)previous.emplace(b.id,0);
  for(const auto &t:tracks) {
    previous.emplace(t.id,1);for(const auto &c:t.curves)for(const auto &k:c.keys)previous.emplace(k.id,2);
  }
  auto unchangedRole=[&](u64 id,u32 role) {
    const auto old=previous.find(id);return old==previous.end()?id>=nextId:old->second==role;
  };
  for(const auto &b:candidate.bindings)if(!unchangedRole(b.id,0))return fail(&diagnostic,"Binding reutiliza uma identidade retirada");
  for(const auto &l:candidate.layers)if(l.id&&!unchangedRole(l.id,3))return fail(&diagnostic,"Camada reutiliza uma identidade retirada");
  for(const auto &t:candidate.tracks) {
    if(!unchangedRole(t.id,1))return fail(&diagnostic,"Canal reutiliza uma identidade retirada");
    for(const auto &c:t.curves)for(const auto &k:c.keys)if(!unchangedRole(k.id,2))return fail(&diagnostic,"Chave reutiliza uma identidade retirada");
  }
  if(candidate.serialize()==serialize())return true;
  candidate.revision=revision+1;*this=std::move(candidate);return true;
}
AnimationClipTrack *AnimationClipAsset::track(u64 id) {
  for(auto &t:tracks)if(t.id==id)return &t;
  return nullptr;
}
const AnimationClipTrack *AnimationClipAsset::track(u64 id) const {
  for(const auto &t:tracks)if(t.id==id)return &t;
  return nullptr;
}
AnimationClipLayer *AnimationClipAsset::layer(u64 id) {
  for(auto &l:layers)if(l.id==id)return &l;
  return nullptr;
}
const AnimationClipLayer *AnimationClipAsset::layer(u64 id) const {
  for(const auto &l:layers)if(l.id==id)return &l;
  return nullptr;
}
bool AnimationClipAsset::addLayer(std::string_view name,AnimationAuthorBlend blend,u64 &id,std::string &error) {
  if(!valid(&error))return false;
  auto candidate=*this;AnimationClipLayer l;l.id=candidate.nextId++;l.name=name;l.blend=blend;
  candidate.layers.push_back(l);
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);id=l.id;return true;
}
bool AnimationClipAsset::configureLayer(u64 id,std::string_view name,AnimationAuthorBlend blend,float weight,
                                        float referenceTime,bool muted,bool solo,std::string &error) {
  if(!valid(&error))return false;
  auto candidate=*this;auto *l=candidate.layer(id);
  if(!l)return fail(&error,"Camada inexistente");
  l->name=name;l->blend=blend;l->weight=weight;l->referenceTime=referenceTime;l->muted=muted;l->solo=solo;
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::moveLayer(u64 id,u32 position,std::string &error) {
  if(!valid(&error))return false;
  if(!id||!position||position>=layers.size())return fail(&error,"A base permanece em primeiro lugar");
  auto candidate=*this;const auto found=std::find_if(candidate.layers.begin(),candidate.layers.end(),[&](const auto &l){return l.id==id;});
  if(found==candidate.layers.end())return fail(&error,"Camada inexistente");
  const auto value=*found;candidate.layers.erase(found);candidate.layers.insert(candidate.layers.begin()+position,value);
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::addLayerTrack(u64 layerId,u64 sourceId,bool copyCurves,u64 &created,std::string &error,std::optional<AnimationRotationMode> rotation) {
  if(!valid(&error))return false;
  const auto *l=layer(layerId);const auto *source=track(sourceId);
  if(!l||!layerId||!source)return fail(&error,"Escolha uma camada de correção e uma propriedade existente");
  if(std::any_of(tracks.begin(),tracks.end(),[&](const auto &t){return t.layer==layerId&&t.binding==source->binding&&t.path==source->path;}))
    return fail(&error,"A camada já possui esta propriedade");
  if(rotation&&(static_cast<u8>(*rotation)>2||source->path!=AnimationPath::Rotation))return fail(&error,"Modo de rotação incompatível");
  if(copyCurves&&rotation&&*rotation!=source->rotationMode)return fail(&error,"Cópia entre formatos exige bake explícito");
  auto candidate=*this;auto value=*source;
  if(rotation)value.rotationMode=*rotation;
  value.curves.resize(value.components());value.layer=layerId;value.id=candidate.nextId++;value.sourceOverride=true;
  if(copyCurves)for(auto &curve:value.curves)for(auto &key:curve.keys)key.id=candidate.nextId++;
  else {
    AnimationChannel c;c.path=source->path;c.weightCount=source->weightCount;c.rotationMode=source->rotationMode;c.curves=source->curves;
    float initial[MaximumMorphTargets]{};
    if(l->blend==AnimationAuthorBlend::Override) {
      if(source->rotationMode==AnimationRotationMode::Euler&&value.rotationMode==AnimationRotationMode::Euler)for(u32 i=0;i<3;++i)initial[i]=source->curves[i].keys.front().value;
      else {
        if(!sampleValidatedAnimationChannel(c,0,{initial,MaximumMorphTargets}))return fail(&error,"Propriedade não produz pose inicial válida");
        if(source->path==AnimationPath::Rotation&&value.rotationMode==AnimationRotationMode::Euler) {
          float angles[3];if(!rotationEulerXYZ(initial,angles))return fail(&error,"Pose inicial Euler inválida");std::copy_n(angles,3,initial);
        }
      }
    } else {
      if(source->path==AnimationPath::Scale)std::fill_n(initial,3,1.f);
      if(source->path==AnimationPath::Rotation&&value.rotationMode!=AnimationRotationMode::Euler)initial[3]=1;
    }
    for(u32 i=0;i<value.components();++i) {
      value.curves[i].keys.clear();AnimationCurveKey key;key.id=candidate.nextId++;key.value=initial[i];
      key.incoming=key.outgoing=AnimationTangentMode::Linear;value.curves[i].keys.push_back(key);
      if(duration>0) {key.id=candidate.nextId++;key.time=duration;value.curves[i].keys.push_back(key);}
    }
  }
  const auto id=value.id;candidate.tracks.push_back(std::move(value));
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);created=id;return true;
}
bool AnimationClipAsset::duplicateLayer(u64 id,u64 &created,std::string &error) {
  if(!valid(&error))return false;
  const auto *l=layer(id);if(!l)return fail(&error,"Camada inexistente");
  auto candidate=*this;auto copy=*l;copy.id=candidate.nextId++;copy.name=l->name.substr(0,240);
  if(l->name.size()>240)while(!copy.name.empty()&&(static_cast<unsigned char>(l->name[copy.name.size()])&0xc0)==0x80)copy.name.pop_back();
  copy.name+=" · cópia";copy.solo=false;
  const auto index=static_cast<usize>(l-layers.data());candidate.layers.insert(candidate.layers.begin()+index+1,copy);
  for(const auto &t:tracks)if(t.layer==id) {
    auto track=t;track.layer=copy.id;track.id=candidate.nextId++;track.sourceOverride=true;
    for(auto &curve:track.curves)for(auto &key:curve.keys)key.id=candidate.nextId++;
    candidate.tracks.push_back(std::move(track));
  }
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);created=copy.id;return true;
}
bool AnimationClipAsset::removeLayer(u64 id,std::string &error) {
  if(!valid(&error))return false;
  if(!id||!layer(id))return fail(&error,"A camada Base não pode ser removida");
  auto candidate=*this;std::erase_if(candidate.layers,[&](const auto &l){return l.id==id;});
  std::erase_if(candidate.tracks,[&](const auto &t){return t.layer==id;});
  if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
bool AnimationClipAsset::copyBaseToTrack(u64 id,std::string &error) {
  if(!valid(&error))return false;
  const auto *target=track(id);if(!target||!target->layer)return fail(&error,"Escolha uma propriedade em uma camada de correção");
  const auto base=std::find_if(tracks.begin(),tracks.end(),[&](const auto &t){return !t.layer&&t.binding==target->binding&&t.path==target->path;});
  if(base==tracks.end())return fail(&error,"Propriedade base inexistente");
  auto candidate=*this;auto *value=candidate.track(id);value->curves=base->curves;value->rotationMode=base->rotationMode;
  for(auto &curve:value->curves)for(auto &key:curve.keys)key.id=candidate.nextId++;
  value->sourceOverride=true;if(!candidate.valid(&error))return false;
  *this=std::move(candidate);return true;
}
bool authorAnimationClip(const AnimationClip &clip,AssetGuid guid,AssetGuid source,AssetGuid sourceClip,
                         std::string_view sourceHash,std::span<const AnimationClipBinding> bindings,
                         AnimationClipAsset &out,std::string &diagnostic) {
  diagnostic.clear();AnimationClipAsset result;result.guid=guid;result.source=source;result.sourceClip=sourceClip;
  result.sourceHash=sourceHash;result.name=clip.name.empty()?"Clipe":clip.name;result.duration=clip.duration;
  if(bindings.empty()||bindings.size()>AnimationClipAsset::MaximumBindings||clip.channels.empty()||clip.channels.size()>AnimationClipAsset::MaximumTracks)
    return fail(&diagnostic,"Origem sem bindings/canais ou excedendo o limite");
  result.bindings.assign(bindings.begin(),bindings.end());for(auto &b:result.bindings)b.id=result.nextId++;
  usize total=0;
  for(const auto &c:clip.channels) {
    if(!c.layerSources.empty())return fail(&diagnostic,"Composição exige o recurso de autoria original, não uma extração de canais compilados");
    if(!validAnimationChannel(c)||c.node>=result.bindings.size())return fail(&diagnostic,"Canal da origem inválido ou sem nó");
    if(c.curves.empty()) {
      if(c.times.size()>AnimationClipAsset::MaximumKeys/c.components()||c.times.size()*c.components()>AnimationClipAsset::MaximumKeys-total)
        return fail(&diagnostic,"Origem excede o limite de chaves");
      total+=c.times.size()*c.components();
    } else for(const auto &curve:c.curves) {
      if(curve.keys.size()>AnimationClipAsset::MaximumKeys-total)return fail(&diagnostic,"Origem excede o limite de chaves");
      total+=curve.keys.size();
    }
    AnimationClipTrack t;t.id=result.nextId++;t.binding=result.bindings[c.node].id;t.path=c.path;t.weightCount=c.weightCount;t.rotationMode=c.rotationMode;
    t.curves.resize(t.components());
    const bool cubic=c.interpolation==AnimationInterpolation::CubicSpline;
    for(u32 component=0;component<t.components();++component) {
      auto &curve=t.curves[component];
      if(!c.curves.empty())curve=c.curves[component];
      else for(usize i=0;i<c.times.size();++i) {
        AnimationCurveKey k;k.time=c.times[i];k.broken=cubic;
        k.incoming=k.outgoing=cubic?AnimationTangentMode::Free:c.interpolation==AnimationInterpolation::Step?
          AnimationTangentMode::Constant:AnimationTangentMode::Linear;
        k.value=c.values[(cubic?i*3+1:i)*c.components()+component];
        if(cubic) {k.inSlope=c.values[(i*3)*c.components()+component];k.outSlope=c.values[(i*3+2)*c.components()+component];}
        // The glTF sampler resolves equal timestamps to the last key there.
        if(!curve.keys.empty()&&curve.keys.back().time==k.time)curve.keys.back()=k;else curve.keys.push_back(k);
      }
      for(auto &k:curve.keys)k.id=result.nextId++;
    }
    result.tracks.push_back(std::move(t));
  }
  if(!result.valid(&diagnostic))return false;
  out=std::move(result);return true;
}
} // namespace ae::resources
