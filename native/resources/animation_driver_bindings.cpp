#include "resources/animation_driver_bindings.h"
#include "resources/animation_clip_asset.h"
#include <algorithm>
#include <iomanip>
#include <set>

namespace ae::resources {
namespace {
bool fail(std::string &error,const char *text){error=text;return false;}
void writeAddress(std::ostream &out,const AnimationDriverTarget &target) {
  const auto &a=target.address;out<<target.binding<<' '<<u32(a.domain)<<' '<<a.component<<' '<<a.version<<' '
    <<std::quoted(a.type)<<' '<<std::quoted(a.property)<<' '<<a.slot<<' '<<a.guiNode<<' '<<(a.resource.valid()?a.resource.text():"-")<<' ';
}
bool readAddress(std::istream &in,AnimationDriverTarget &target) {
  auto &a=target.address;u32 domain=0;std::string resource;
  if(!(in>>target.binding>>domain>>a.component>>a.version>>std::quoted(a.type)>>std::quoted(a.property)>>a.slot>>a.guiNode>>resource)||domain>2)return false;
  a.domain=static_cast<AnimationPropertyDomain>(domain);a.kind=AnimationPropertyKind::Number;
  if(resource!="-"&&!AssetGuid::parse(resource,a.resource))return false;
  return validAnimationPropertyAddress(a);
}
}
bool compileAnimationDriverBindings(const AnimationClipDrivers &source,std::span<const AnimationClipBinding> bindings,
    CompiledAnimationDriverBindings &out,std::string &error) {
  error.clear();CompiledAnimationDriverBindings compiled;
  if(source.empty()) {
    if(!source.inputs.empty()||!source.targets.empty()||!source.graph.outputs.empty()||source.graph.sources)return fail(error,"Driver vazio contém vínculos órfãos");
    out=std::move(compiled);return true;
  }
  if(source.inputs.size()!=source.graph.sources||source.targets.size()!=source.graph.outputs.size())return fail(error,"Quantidade de vínculos difere das entradas e saídas do driver");
  if(!compileAnimationDrivers(source.graph,compiled.graph,error))return false;
  const auto node=[&](const AnimationDriverTarget &target,u32 &index) {
    const auto found=std::find_if(bindings.begin(),bindings.end(),[&](const auto &binding){return binding.id==target.binding;});
    if(found==bindings.end()||target.address.kind!=AnimationPropertyKind::Number||!validAnimationPropertyAddress(target.address))return false;
    index=static_cast<u32>(found-bindings.begin());return true;
  };
  for(const auto &input:source.inputs) {
    CompiledAnimationDriverInput result;result.kind=input.kind;result.parameter=input.parameter;result.controller=input.controller;
    if(input.kind==AnimationDriverInputKind::ClipTime) {
      if(input.binding||input.parameter||input.controller.valid()||input.address.component)return fail(error,"Entrada tempo possui identidade indevida");
    } else {
      if(!node(input,result.node))return fail(error,"Entrada de driver sem binding numérico válido");
      result.address=input.address;
      if(input.kind==AnimationDriverInputKind::AnimatorParameter) {
        if(!input.parameter||input.address.type!="astra.animation.animator"||input.address.domain!=AnimationPropertyDomain::Component)return fail(error,"Entrada Animator exige identidade de parâmetro");
      } else if(input.kind!=AnimationDriverInputKind::Property||input.parameter||input.controller.valid())return fail(error,"Tipo de entrada de driver inválido");
    }
    compiled.inputs.push_back(std::move(result));
  }
  std::set<std::string> targets;
  for(const auto &target:source.targets) {
    CompiledAnimationDriverTarget result;
    if(!node(target,result.node)||!targets.insert(std::to_string(target.binding)+":"+animationPropertyIdentity(target.address)).second)return fail(error,"Driver possui saída ausente ou duplicada");
    result.address=target.address;compiled.targets.push_back(std::move(result));
  }
  // Feedback through a property is a graph cycle too, including loops hidden
  // behind a chain of output addresses. Sources observe the composed clip,
  // never another driver's publication from the same frame.
  for(const auto &input:source.inputs)if(input.kind==AnimationDriverInputKind::Property)
    if(targets.contains(std::to_string(input.binding)+":"+animationPropertyIdentity(input.address)))return fail(error,"Ciclo entre entrada e saída de propriedade do driver");
  out=std::move(compiled);return true;
}
void writeAnimationClipDrivers(std::ostream &out,const AnimationClipDrivers &source) {
  out<<source.graph.nodes.size()<<' '<<source.inputs.size()<<' '<<source.targets.size()<<'\n';
  for(const auto &input:source.inputs) {
    out<<u32(input.kind)<<' ';
    if(input.kind!=AnimationDriverInputKind::ClipTime)writeAddress(out,input);
    out<<input.parameter<<' '<<(input.controller.valid()?input.controller.text():"-")<<'\n';
  }
  for(const auto &target:source.targets){writeAddress(out,target);out<<'\n';}
  for(const auto &n:source.graph.nodes) {
    out<<n.id<<' '<<u32(n.operation)<<' '<<n.first<<' '<<n.second<<' '<<n.source<<' '<<n.a<<' '<<n.b<<' '<<n.c<<' '<<n.d<<' '<<n.curve.keys.size()<<'\n';
    for(const auto &k:n.curve.keys)out<<k.id<<' '<<k.time<<' '<<k.value<<' '<<k.inSlope<<' '<<k.outSlope<<' '<<k.inWeight<<' '<<k.outWeight<<' '
      <<u32(k.incoming)<<' '<<u32(k.outgoing)<<' '<<k.broken<<' '<<k.weightedIn<<' '<<k.weightedOut<<'\n';
  }
  for(const auto id:source.graph.outputs)out<<id<<' ';
  out<<'\n';
}
bool readAnimationClipDrivers(std::istream &in,AnimationClipDrivers &out,std::string &error) {
  AnimationClipDrivers source;usize nodes=0,inputs=0,outputs=0;
  if(!(in>>nodes>>inputs>>outputs)||nodes>AnimationDriverGraph::MaximumNodes||inputs>AnimationDriverGraph::MaximumSources||outputs>AnimationDriverGraph::MaximumOutputs)return fail(error,"Driver excede orçamento de leitura");
  source.inputs.resize(inputs);source.targets.resize(outputs);source.graph.nodes.resize(nodes);source.graph.sources=static_cast<u32>(inputs);source.graph.outputs.resize(outputs);
  for(auto &input:source.inputs) {
    u32 kind=0;std::string controller;
    if(!(in>>kind)||kind>2)return fail(error,"Tipo de entrada incompleto");
    input.kind=static_cast<AnimationDriverInputKind>(kind);
    if(input.kind!=AnimationDriverInputKind::ClipTime&&!readAddress(in,input))return fail(error,"Endereço de entrada inválido");
    if(!(in>>input.parameter>>controller)||(controller!="-"&&!AssetGuid::parse(controller,input.controller)))return fail(error,"Identidade de Animator inválida");
  }
  for(auto &target:source.targets)if(!readAddress(in,target))return fail(error,"Endereço de saída inválido");
  usize keys=0;
  for(auto &n:source.graph.nodes) {
    u32 operation=0;usize count=0;
    if(!(in>>n.id>>operation>>n.first>>n.second>>n.source>>n.a>>n.b>>n.c>>n.d>>count)||operation>7||count>AnimationDriverGraph::MaximumCurveKeys-keys)return fail(error,"Node de driver incompleto ou acima do orçamento");
    n.operation=static_cast<AnimationDriverOp>(operation);keys+=count;n.curve.keys.resize(count);
    for(auto &k:n.curve.keys) {
      u32 incoming=0,outgoing=0,broken=0,wi=0,wo=0;
      if(!(in>>k.id>>k.time>>k.value>>k.inSlope>>k.outSlope>>k.inWeight>>k.outWeight>>incoming>>outgoing>>broken>>wi>>wo)||incoming>6||outgoing>6||broken>1||wi>1||wo>1)return fail(error,"Curva de driver incompleta");
      k.incoming=static_cast<AnimationTangentMode>(incoming);k.outgoing=static_cast<AnimationTangentMode>(outgoing);k.broken=broken;k.weightedIn=wi;k.weightedOut=wo;
    }
  }
  for(auto &id:source.graph.outputs)if(!(in>>id))return fail(error,"Saída de driver incompleta");
  out=std::move(source);return true;
}
}
