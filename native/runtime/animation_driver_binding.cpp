#include "runtime/animation_driver_binding.h"
#include "runtime/animation_property_channels.h"
#include "scene/animator.h"
#include "resources/animation_clip_asset.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ae::runtime {
namespace {
bool fail(std::string &error,const char *text){error=text;return false;}
std::string key(ObjectId id,const resources::AnimationPropertyAddress &address){return std::to_string(id)+":"+resources::animationPropertyIdentity(address);}
}
bool evaluateAnimationDriverRequests(const SceneGraph &graph,std::span<const AnimationDriverRequest> requests,
    const AnimationGuiResolver &gui,const AnimationDriverParameterResolver &parameters,
    std::vector<AnimationPropertyWrite> &writes,const AnimationPropertyMixer::Baselines &baselines,
    std::vector<AnimationPropertyContribution> &captured,std::string &error) {
  using namespace resources;error.clear();std::vector<AnimationPropertyContribution> results;
  const auto read=[&](ObjectId object,const AnimationPropertyAddress &address,AnimationPropertyValue &value) {
    // Every graph observes the same composed-clip snapshot. A graph cannot
    // accidentally depend on iteration order or a peer driver's output.
    for(const auto &write:writes)if(write.object==object&&write.address==address){value=write.value;return true;}
    const auto *entity=graph.find(object);const auto *component=entity?entity->components.findInstance(address.component):nullptr;
    if(component)for(const auto &write:writes)if(write.object==object) {
      const auto channel=animationPropertyChannel(*component,write.address,address);auto grouped=write.value;float scalar=0;
      if(animationPropertyChannelValue(grouped,channel,scalar)){value=scalar;return true;}
    }
    return readAnimationPropertyTarget(graph,object,address,gui,value,error);
  };
  for(const auto &request:requests) {
    if(!request.binding||request.weight<=0)continue;
    const auto &binding=*request.binding;std::vector<double> sources(binding.inputs.size()),scratch(binding.graph.scratchSize()),outputs(binding.targets.size());
    for(usize i=0;i<binding.inputs.size();++i) {
      const auto &input=binding.inputs[i];
      if(input.kind==AnimationDriverInputKind::ClipTime){sources[i]=request.time;continue;}
      if(input.node>=request.targets.size()||!graph.exists(request.targets[input.node]))return fail(error,"Binding de entrada de driver ausente");
      const auto objectId=request.targets[input.node];const auto *object=graph.find(objectId);const auto *component=object->components.findInstance(input.address.component);
      if(!component||component->type().id!=input.address.type||component->type().version!=input.address.version)return fail(error,"Componente de entrada de driver removido ou substituído");
      if(input.kind==AnimationDriverInputKind::AnimatorParameter) {
        if(&component->type()!=&scene::Animator::descriptor)return fail(error,"Entrada de velocidade exige Animator");
        if(parameters) {if(!parameters(objectId,input.address.component,input.parameter,input.controller,sources[i],error))return false;}
        else {
          const auto &animator=static_cast<const scene::Animator&>(*component);const auto *parameter=animator.parameter(input.parameter);
          if(animator.controller!=input.controller||animator.controller.valid()||!parameter||parameter->type!=scene::AnimatorParameterType::Float)
            return fail(error,"Parâmetro de driver ausente ou controlador sem resolver no preview");
          sources[i]=parameter->value;
        }
      } else {
        AnimationPropertyValue value;if(!read(objectId,input.address,value))return false;
        const auto *number=std::get_if<float>(&value);if(!number)return fail(error,"Entrada de driver não é escalar numérico");sources[i]=*number;
      }
    }
    if(!binding.graph.evaluate(sources,scratch,outputs,error))return false;
    for(usize i=0;i<outputs.size();++i) {
      const auto &target=binding.targets[i];if(target.node>=request.targets.size()||!graph.exists(request.targets[target.node]))return fail(error,"Binding de saída de driver ausente");
      const auto objectId=request.targets[target.node];
      if(request.mask&&objectId!=request.mask&&!graph.isDescendantOf(objectId,request.mask))continue;
      if(std::abs(outputs[i])>std::numeric_limits<float>::max())return fail(error,"Resultado de driver excede propriedade float");
      AnimationPropertyContribution result;result.object=objectId;result.address=target.address;result.value=static_cast<float>(outputs[i]);
      result.weight=request.weight;result.layer=request.layer;result.coverage=request.coverage;result.sourceOwner=request.sourceOwner;result.sourceInstance=request.sourceInstance;
      results.push_back(std::move(result));
    }
  }
  std::map<std::string,std::vector<const AnimationPropertyContribution*>> groups;
  for(const auto &result:results)groups[key(result.object,result.address)].push_back(&result);
  if(groups.size()>resources::AnimationClipAsset::MaximumTracks)return fail(error,"Saídas de drivers excedem orçamento");
  // Resolve every graph before modifying the candidate publication list.
  auto candidate=writes;
  for(const auto &[identity,group]:groups) {
    const auto &first=*group.front();const auto original=std::find_if(writes.begin(),writes.end(),[&](const auto &write){return write.object==first.object&&write.address==first.address;});
    AnimationPropertyValue composed;
    if(!read(first.object,first.address,composed))return false;
    const AnimationPropertyValue *baseline=original!=writes.end()?&original->value:baselines.contains(identity)?&baselines.at(identity):nullptr;
    // The baseline is the composed clip, including a scalar contained in a
    // vector/color track. Rest is used only when no clip owns that field.
    const auto *entity=graph.find(first.object);const auto *component=entity?entity->components.findInstance(first.address.component):nullptr;
    if(component)for(const auto &write:writes)if(write.object==first.object&&animationPropertyChannel(*component,write.address,first.address)>=0){baseline=&composed;break;}
    const auto *rest=baseline?std::get_if<float>(baseline):nullptr;if(!rest)return fail(error,"Repouso numérico do driver ausente");
    std::set<u32,std::greater<>> layers;for(const auto *sample:group)layers.insert(sample->layer);
    double value=0,remaining=1;
    for(const auto layer:layers) {
      double sum=0,mixed=0;for(const auto *sample:group)if(sample->layer==layer){sum+=sample->weight;mixed+=std::get<float>(sample->value)*sample->weight;}
      const double take=std::min(sum,1.0)*remaining;if(sum>0)value+=mixed/sum*take;remaining-=take;
    }
    value+=*rest*remaining;
    if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail(error,"Composição de driver excede domínio numérico");
    AnimationPropertyWrite write{first.object,first.address,static_cast<float>(value)};
    const auto found=std::find_if(candidate.begin(),candidate.end(),[&](const auto &entry){return entry.object==write.object&&entry.address==write.address;});
    if(found!=candidate.end())*found=std::move(write);
    else {
      bool merged=false;float scalar=static_cast<float>(value);
      if(component)for(auto &entry:candidate)if(entry.object==write.object) {
        const int channel=animationPropertyChannel(*component,entry.address,write.address);
        if(animationPropertyChannelValue(entry.value,channel,scalar,true)){merged=true;break;}
      }
      if(!merged)candidate.push_back(std::move(write));
    }
  }
  writes=std::move(candidate);captured=std::move(results);return true;
}
}
