#include "resources/unity_animation_import.h"
#include "resources/animation_binding_path.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace ae::resources {
namespace {
using Node=UnityYamlNode;
bool fail(std::string &error,const std::string &reason){error=reason;return false;}
const Node *field(const Node &node,std::string_view key){return node.get(key);}
bool scalar(const Node &node,std::string_view key,std::string &out) {
  const auto *value=field(node,key);if(!value||value->kind!=Node::Kind::Scalar)return false;out=value->scalar;return true;
}
bool numeric(const Node &node,std::string_view key,double &out) {const auto *value=field(node,key);return value&&value->number(out);}
bool integer(const Node &node,std::string_view key,i64 &out) {const auto *value=field(node,key);return value&&value->integer(out);}
bool enabled(const Node &node,std::string_view key) {const auto *value=field(node,key);bool result=false;return value&&value->boolean(result)&&result;}
bool nonempty(const Node &node,std::string_view key){const auto *value=field(node,key);return value&&!value->empty();}
bool component(const Node &node,std::string_view key,u32 index,u32 dimensions,double &out,bool infinity=false) {
  const auto *value=field(node,key);if(!value)return false;
  if(dimensions!=1){constexpr const char *names[]{"x","y","z","w"};value=value->get(names[index]);if(!value)return false;}
  return value->number(out,infinity);
}
bool curves(const Node &curve,u32 dimensions,std::vector<AnimationCurve> &out,usize &total,
            std::string &error) {
  const auto *keys=curve.get("m_Curve");
  if(!keys||keys->kind!=Node::Kind::Sequence||keys->elements.empty()||
      dimensions*keys->elements.size()>AnimationClipAsset::MaximumKeys-total)return fail(error,"Curva Unity vazia ou acima do orçamento de chaves.");
  for(const char *name:{"m_PreInfinity","m_PostInfinity"}) {
    i64 mode=2;const auto *value=curve.get(name);
    if(value&&(!value->integer(mode)||mode!=2))return fail(error,"Wrap por curva Unity exige Clamp; looping é configuração de reprodução, não da curva nativa.");
  }
  out.resize(dimensions);float previous=-1;
  for(const auto &source:keys->elements) {
    double time=0;if(!numeric(source,"time",time)||time<0||time>86400)return fail(error,"Tempo de chave Unity inválido.");
    const float seconds=static_cast<float>(time);
    if(seconds<=previous)return fail(error,"Tempos Unity não estritamente crescentes após conversão float.");
    previous=seconds;i64 weighted=0;
    if(const auto *mode=source.get("weightedMode");mode&&(!mode->integer(weighted)||weighted<0||weighted>3))return fail(error,"WeightedMode Unity inválido.");
    for(u32 c=0;c<dimensions;++c) {
      double value=0,in=0,away=0,wi=1.0/3,wo=1.0/3;
      if(!component(source,"value",c,dimensions,value)||!component(source,"inSlope",c,dimensions,in,true)||
         !component(source,"outSlope",c,dimensions,away,true))return fail(error,"Valor ou tangente Unity inválido.");
      // Curvas de vetor (serializedVersion 3) guardam um peso por componente.
      if(source.get("inWeight")&&!component(source,"inWeight",c,dimensions,wi))return fail(error,"Peso de tangente Unity inválido.");
      if(source.get("outWeight")&&!component(source,"outWeight",c,dimensions,wo))return fail(error,"Peso de tangente Unity inválido.");
      AnimationCurveKey key;key.time=seconds;key.value=static_cast<float>(value);key.broken=true;
      key.incoming=std::isinf(in)?AnimationTangentMode::Constant:AnimationTangentMode::Free;
      key.outgoing=std::isinf(away)?AnimationTangentMode::Constant:AnimationTangentMode::Free;
      key.inSlope=std::isinf(in)?0:static_cast<float>(in);key.outSlope=std::isinf(away)?0:static_cast<float>(away);
      key.inWeight=static_cast<float>(wi);key.outWeight=static_cast<float>(wo);
      key.weightedIn=(weighted&1)!=0;key.weightedOut=(weighted&2)!=0;
      if(!std::isfinite(key.value)||!std::isfinite(key.inSlope)||!std::isfinite(key.outSlope))return fail(error,"Chave Unity excede a precisão float nativa.");
      out[c].keys.push_back(key);
    }
  }
  total+=dimensions*keys->elements.size();
  // Temporary IDs are assigned only after every source property has a native
  // representation. The authoring allocator owns the final identities.
  for(auto &c:out){u64 id=1;for(auto &key:c.keys)key.id=id++;if(!validAnimationCurve(c))return fail(error,"Curva Unity não representável pelo avaliador nativo.");}
  return true;
}
void multiply(AnimationCurve &curve,float factor) {
  for(auto &key:curve.keys){key.value*=factor;key.inSlope*=factor;key.outSlope*=factor;}
}
struct Track {AnimationPath path;std::string binding;std::vector<AnimationCurve> curves;};
}
bool convertUnityAnimation(const UnityYamlDocument &document,AssetGuid nativeGuid,AssetGuid unityGuid,
    std::string_view sourceHash,std::span<const UnityAnimationMorphBinding> morphs,
    const UnityHumanoidTarget *humanoid,UnityAnimationImport &out,std::string &diagnostic,UnityHandedness handedness) {
  const UnityYamlObject *source=nullptr;
  if(document.objects.size()!=1)return fail(diagnostic,"AnimationClip exige um único objeto completo; objetos adicionais não serão descartados.");
  for(const auto &object:document.objects)if(object.classId==74) {if(source)return fail(diagnostic,"Mais de um AnimationClip no documento; escolha o fileID explicitamente.");source=&object;}
  if(!source||source->stripped||source->type!="AnimationClip")return fail(diagnostic,"AnimationClip Unity completo não encontrado.");
  const auto &node=source->value;
  for(const char *name:{"m_Compressed","m_HasMotionFloatCurves"}) {
    bool value=false;if(const auto *entry=node.get(name);entry&&!entry->boolean(value))return fail(diagnostic,"Flag de AnimationClip Unity inválida: "+std::string(name));
  }
  // m_WrapMode só governa clipes legados (componente Animation); num clipe
  // Mecanim a Unity o ignora e o laço vem de m_LoopTime.
  i64 wrap=0;const bool legacy=enabled(node,"m_Legacy");
  if(const auto *entry=node.get("m_WrapMode");entry&&(!entry->integer(wrap)||(legacy&&wrap!=0)))
    return fail(diagnostic,"WrapMode legado Unity exige configuração explícita; use LoopTime no clipe de origem.");
  if(enabled(node,"m_Compressed")||nonempty(node,"m_CompressedRotationCurves"))return fail(diagnostic,"AnimationClip Unity comprimido exige decodificação específica; fonte preservada.");
  if(nonempty(node,"m_PPtrCurves"))return fail(diagnostic,"Curvas de referência Unity não possuem mapa de recurso nativo nesta conversão.");
  if(nonempty(node,"m_Events"))return fail(diagnostic,"AnimationEvents Unity executam callbacks arbitrários; configure cues nativos explicitamente.");
  if(nonempty(node,"m_EulerCurves"))return fail(diagnostic,"Curvas Euler Unity usam ordem ZXY; conversão exata exige bake declarado, ainda indisponível neste importador.");
  if(enabled(node,"m_HasMotionFloatCurves"))return fail(diagnostic,"Curvas de root motion Unity exigem conversão do contrato de movimento, não apenas canais de pose.");
  std::vector<Track> tracks;usize total=0;
  for(const auto &[name,path,dimensions]:std::array<std::tuple<const char*,AnimationPath,u32>,3>{{
      {"m_PositionCurves",AnimationPath::Translation,3},{"m_RotationCurves",AnimationPath::Rotation,4},{"m_ScaleCurves",AnimationPath::Scale,3}}}) {
    const auto *entries=node.get(name);if(!entries||entries->empty())continue;
    if(entries->kind!=Node::Kind::Sequence)return fail(diagnostic,"Coleção de curvas Unity inválida.");
    for(const auto &entry:entries->elements) {
      Track track;track.path=path;const auto *curve=entry.get("curve");
      if(!scalar(entry,"path",track.binding)||!curve||!curves(*curve,dimensions,track.curves,total,diagnostic))return false;
      track.binding=migrateAnimationBindingPath(track.binding);
      // Unity left-handed to native right-handed coordinates: H=diag(1,1,-1)
      // for converted scenes/prefabs, H=diag(-1,1,1) for FBX-imported models.
      if(handedness==UnityHandedness::MirrorZ) {
        if(path==AnimationPath::Translation)multiply(track.curves[2],-1);
        if(path==AnimationPath::Rotation){multiply(track.curves[0],-1);multiply(track.curves[1],-1);}
      } else {
        if(path==AnimationPath::Translation)multiply(track.curves[0],-1);
        if(path==AnimationPath::Rotation){multiply(track.curves[1],-1);multiply(track.curves[2],-1);}
      }
      tracks.push_back(std::move(track));
    }
  }
  const auto *floats=node.get("m_FloatCurves");std::map<std::string,usize> morphTracks;
  std::set<std::pair<std::string,usize>> morphChannels;
  // Humanoid (classID 95 do Animator): músculos e corpo, convertidos no fim
  // para o esqueleto do personagem alvo (unity_humanoid.cpp).
  UnityHumanoidCurves human;std::set<std::string> humanIgnored;
  if(floats&&!floats->empty()) {
    if(floats->kind!=Node::Kind::Sequence)return fail(diagnostic,"Coleção de curvas escalares Unity inválida.");
    for(const auto &entry:floats->elements) {
      i64 classId=0;std::string attribute,path;
      if(!integer(entry,"classID",classId)||!scalar(entry,"attribute",attribute)||!scalar(entry,"path",path))return fail(diagnostic,"Binding de curva Unity inválido.");
      if(classId==95) {
        const auto address=unityHumanoidAttribute(attribute);
        if(address.kind==UnityHumanoidAttribute::Kind::None)return fail(diagnostic,"Curva Humanoid sem correspondência: '"+attribute+"'.");
        if(address.kind==UnityHumanoidAttribute::Kind::Ignored){humanIgnored.insert(attribute);continue;}
        const auto *curve=entry.get("curve");std::vector<AnimationCurve> converted;
        if(!curve||!curves(*curve,1,converted,total,diagnostic))return false;
        if(address.kind==UnityHumanoidAttribute::Kind::Muscle) {
          if(human.present[address.bone][address.axis])return fail(diagnostic,"Músculo Humanoid duplicado: "+attribute);
          human.muscles[address.bone][address.axis]=std::move(converted.front());human.present[address.bone][address.axis]=true;++human.muscleCurves;
        } else if(address.kind==UnityHumanoidAttribute::Kind::RootT) {human.rootT[address.axis]=std::move(converted.front());human.hasRootT=true;}
        else {human.rootQ[address.axis]=std::move(converted.front());human.hasRootQ=true;}
        continue;
      }
      if(classId!=137||!attribute.starts_with("blendShape."))return fail(diagnostic,"Propriedade Unity sem correspondência: classID "+std::to_string(classId)+" · "+attribute);
      const auto *script=entry.get("script");UnityObjectReference reference;
      if(script&&(!unityObjectReference(*script,reference)||reference.fileId))return fail(diagnostic,"Curva de script Unity não é um blendShape nativo.");
      path=migrateAnimationBindingPath(path);const UnityAnimationMorphBinding *binding=nullptr;
      for(const auto &candidate:morphs)if(candidate.path==path){if(binding)return fail(diagnostic,"Mais de uma malha corresponde ao caminho morph Unity.");binding=&candidate;}
      if(!binding||binding->names.empty()||binding->names.size()>MaximumMorphTargets||binding->defaults.size()!=binding->names.size())
        return fail(diagnostic,"Selecione um rig com malha/morphs correspondentes ao caminho '"+path+"'.");
      const auto name=attribute.substr(11);usize selected=binding->names.size();
      for(usize c=0;c<binding->names.size();++c)if(binding->names[c]==name){if(selected!=binding->names.size())return fail(diagnostic,"Nome morph duplicado na malha.");selected=c;}
      if(selected==binding->names.size())return fail(diagnostic,"Morph Unity ausente no alvo: "+name);
      if(!morphChannels.emplace(path,selected).second)return fail(diagnostic,"Canal morph Unity duplicado.");
      auto at=morphTracks.find(path);
      if(at==morphTracks.end()) {
        if(binding->names.size()>AnimationClipAsset::MaximumKeys-total)return fail(diagnostic,"Defaults morph excedem orçamento de chaves.");
        Track track;track.path=AnimationPath::Weights;track.binding=path;track.curves.resize(binding->names.size());
        for(usize c=0;c<track.curves.size();++c){AnimationCurveKey key;key.id=1;key.value=binding->defaults[c];key.incoming=key.outgoing=AnimationTangentMode::Free;
          if(!std::isfinite(key.value))return fail(diagnostic,"Default morph inválido.");
          track.curves[c].keys.push_back(key);}
        total+=track.curves.size();at=morphTracks.emplace(path,tracks.size()).first;tracks.push_back(std::move(track));
      }
      const auto *curve=entry.get("curve");std::vector<AnimationCurve> converted;
      if(!curve||!curves(*curve,1,converted,total,diagnostic))return false;
      multiply(converted.front(),.01f);tracks[at->second].curves[selected]=std::move(converted.front());
    }
  }
  const bool humanoidClip=human.muscleCurves||human.hasRootT||human.hasRootQ;
  if(humanoidClip&&!humanoid)
    return fail(diagnostic,"Clipe Humanoid (músculos): escolha o personagem com Avatar Humanoid antes de aplicar; a conversão usa o mapeamento e a pose em T dele.");
  if(tracks.empty()&&!humanoidClip)return fail(diagnostic,"AnimationClip sem canais nativos; fonte permanece disponível, sem clipe vazio artificial.");
  UnityAnimationImport candidate;candidate.fileId=source->fileId;candidate.unityGuid=unityGuid;candidate.sourceHash=sourceHash;
  // A Unity file GUID is not an Astra registered FBX source. Store provenance
  // in the result/registry recipe; do not invent a runtime source dependency.
  auto &clip=candidate.clip;clip.guid=nativeGuid;
  if(!scalar(node,"m_Name",clip.name)||clip.name.empty())return fail(diagnostic,"AnimationClip sem nome válido.");
  double rate=60;if(const auto *value=node.get("m_SampleRate");value&&!value->number(rate))return fail(diagnostic,"SampleRate Unity inválido.");
  if(rate<1||rate>1000)return fail(diagnostic,"SampleRate Unity fora do intervalo nativo.");
  clip.displayRate=static_cast<u32>(std::lround(rate));
  std::map<std::string,u64> bindings;
  for(auto &sourceTrack:tracks) {
    auto found=bindings.find(sourceTrack.binding);
    if(found==bindings.end()) {
      AnimationClipBinding binding;binding.id=clip.nextId++;binding.path=sourceTrack.binding;
      const auto slash=binding.path.rfind('/');binding.name=binding.path.substr(slash==binding.path.npos?0:slash+1);
      // Native source-node GUIDs belong to imported FBX nodes, not to a Unity
      // path hash. The explicit canonical path remains the binding authority.
      found=bindings.emplace(binding.path,binding.id).first;clip.bindings.push_back(std::move(binding));
    }
    AnimationClipTrack track;track.id=clip.nextId++;track.binding=found->second;track.path=sourceTrack.path;
    track.weightCount=track.path==AnimationPath::Weights?static_cast<u32>(sourceTrack.curves.size()):0;
    track.curves=std::move(sourceTrack.curves);
    for(auto &curve:track.curves)for(auto &key:curve.keys){key.id=clip.nextId++;clip.duration=std::max(clip.duration,key.time);}
    clip.tracks.push_back(std::move(track));
  }
  if(const auto *settings=node.get("m_AnimationClipSettings")) {
    for(const auto &[name,value]:settings->members)if(name=="m_LoopTime"||name=="m_LoopBlend"||name=="m_LoopBlendOrientation"||name=="m_LoopBlendPositionY"||name=="m_LoopBlendPositionXZ"||name=="m_Mirror"||name=="m_HasAdditiveReferencePose") {
      bool flag=false;if(!value.boolean(flag))return fail(diagnostic,"Flag de configuração Unity inválida: "+name);
    }
    double start=0,stop=clip.duration;
    if(const auto *value=settings->get("m_StartTime");value&&!value->number(start))return fail(diagnostic,"StartTime Unity inválido.");
    if(const auto *value=settings->get("m_StopTime");value&&!value->number(stop))return fail(diagnostic,"StopTime Unity inválido.");
    if(start!=0||stop<clip.duration||stop>86400)return fail(diagnostic,"Recorte Unity não coincide com os canais; converta o intervalo explicitamente.");
    for(const char *flag:{"m_HasAdditiveReferencePose","m_Mirror","m_LoopBlend","m_LoopBlendOrientation","m_LoopBlendPositionY","m_LoopBlendPositionXZ"})
      if(enabled(*settings,flag))return fail(diagnostic,std::string("Configuração Unity sem equivalente nesta conversão: ")+flag);
    for(const char *offset:{"m_OrientationOffsetY","m_Level","m_CycleOffset"}) {
      double value=0;if(const auto *entry=settings->get(offset);entry&&(!entry->number(value)||value!=0))return fail(diagnostic,std::string("Offset Unity exige transformação explícita: ")+offset);
    }
    candidate.loop=enabled(*settings,"m_LoopTime");clip.duration=static_cast<float>(stop);
  }
  UnityHumanoidReport humanReport;
  if(humanoidClip) {
    // Duração: o recorte da Unity ou a última chave das curvas Humanoid.
    float duration=clip.duration;
    const auto last=[&](const AnimationCurve &c){if(!c.keys.empty())duration=std::max(duration,c.keys.back().time);};
    for(const auto &bone:human.muscles)for(const auto &c:bone)last(c);
    for(const auto &c:human.rootT)last(c);
    for(const auto &c:human.rootQ)last(c);
    if(!appendUnityHumanoidTracks(human,*humanoid,duration,clip.displayRate,clip,humanReport,diagnostic))return false;
  }
  if(!clip.valid(&diagnostic))return false;
  candidate.transformations={handedness==UnityHandedness::MirrorZ?
      "Unity LH para Astra RH: posição Z e quaternion X/Y invertidos; metros preservados.":
      "Unity LH para o modelo FBX (X espelhado): posição X e quaternion Y/Z invertidos; metros preservados.",
      "Curvas Bezier: tempos, valores, tangentes e pesos preservados; modos editoriais convertidos em tangentes Free/Constant.",
      "Curvas editoriais duplicadas não executadas; somente m_Position/Rotation/Scale/FloatCurves alimentam o clipe nativo."};
  if(!morphTracks.empty())candidate.transformations.push_back("blendShape por nome exato da malha selecionada; percentual Unity convertido para peso 0..1; defaults não animados preservados.");
  for(auto &note:humanReport.notes)candidate.transformations.push_back(std::move(note));
  if(!humanIgnored.empty())candidate.transformations.push_back("Humanoid: "+std::to_string(humanIgnored.size())+
      " curvas de IK/Motion/TDOF fora da pose (o passo de IK da Unity é opcional por estado).");
  out=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::resources
